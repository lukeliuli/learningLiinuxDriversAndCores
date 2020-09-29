// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 *
 *  USB HIDBP Mouse support
 */

/*
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

/* for apple IDs */
#ifdef CONFIG_USB_HID_MODULE
#include "../hid-ids.h"
#endif

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.6"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@ucw.cz>"
#define DRIVER_DESC "USB HID Boot Protocol mouse driver"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

struct usb_mouse
{
    char name[128]; //  名称，一般存储制造商名称
    char phys[64];
    struct usb_device *usbdev; //  usb 设备模型
    struct input_dev *dev;     //  输入设备
    struct urb *irq;           //   用于usb 设备通信的urb 模块

    signed char *data; //  USB  鼠标事件的buffer，存储鼠标的左键，右键，滑轮，坐标事件
    dma_addr_t data_dma;
};

/*
鼠标的协议
鼠标发送给PC的数据每次4个字节
BYTE1 BYTE2 BYTE3 BYTE4
定义分别是：
BYTE1 –
|–bit7: 1 表示 Y 坐标的变化量超出－256 ~ 255的范围,0表示没有溢出
|–bit6: 1 表示 X 坐标的变化量超出－256 ~ 255的范围，0表示没有溢出
|–bit5: Y 坐标变化的符号位，1表示负数，即鼠标向下移动
|–bit4: X 坐标变化的符号位，1表示负数，即鼠标向左移动
|–bit3: 恒为1
|–bit2: 1表示中键按下
|–bit1: 1表示右键按下
|–bit0: 1表示左键按下
BYTE2 – X坐标变化量，与byte的bit4组成9位符号数,负数表示向左移，正数表右移。用补码表示变化量
BYTE3 – Y坐标变化量，与byte的bit5组成9位符号数，负数表示向下移，正数表上移。用补码表示变化量
BYTE4 – 滚轮变化。

*/

//回调里面解析完 usb mouse 事件之后，通过input_report_key丢给linux 的input 系统来处理。至此usb mouse 完成了事件的获取没解析和转发。另外input子系统还有以下input event 转发接口，在其它输入设备中可以用到，例如usb keyboard。
static void usb_mouse_irq(struct urb *urb)
{
    struct usb_mouse *mouse = urb->context; //  mouse 为上下文
    signed char *data = mouse->data;        // buffer 中存储的事件信息
    struct input_dev *dev = mouse->dev;     //
    int status;
    //  判断urb 通信是否成功
    switch (urb->status)
    {
    case 0: /* success */
        break;
    case -ECONNRESET: /* unlink */
    case -ENOENT:
    case -ESHUTDOWN:
        return;
    /* -EPIPE:  should clear the halt */
    default:           /* error */
        goto resubmit; // 产生错误，重新提交urb
    }

    // data 第一个字节代表左右按键，中间按键，都会触发
    input_report_key(dev, BTN_LEFT, data[0] & 0x01);
    input_report_key(dev, BTN_RIGHT, data[0] & 0x02);
    input_report_key(dev, BTN_MIDDLE, data[0] & 0x04);
    input_report_key(dev, BTN_SIDE, data[0] & 0x08);
    input_report_key(dev, BTN_EXTRA, data[0] & 0x10);
    // 第二个字节代表X的坐标
    input_report_rel(dev, REL_X, data[1]);
    //  第三个字节代码Y的坐标
    input_report_rel(dev, REL_Y, data[2]);
    //  第四个字节代表滑轮的当前值
    input_report_rel(dev, REL_WHEEL, data[3]);

    input_sync(dev);
resubmit:
    status = usb_submit_urb(urb, GFP_ATOMIC);
    if (status)
        dev_err(&mouse->usbdev->dev,
                "can't resubmit intr, %s-%s/input0, status %d\n",
                mouse->usbdev->bus->bus_name,
                mouse->usbdev->devpath, status);
}

static int usb_mouse_open(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);
    // 设置urb->dev ，urb submit 时需要使用该usb 设备
    mouse->irq->dev = mouse->usbdev;
    // urb 提交，之后urb 的通信通道完成启动
    if (usb_submit_urb(mouse->irq, GFP_KERNEL))
        return -EIO;
    return 0;
}

static void usb_mouse_close(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);
    //  断开urb 通信
    usb_kill_urb(mouse->irq);
}

static int usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    //  USB 接口描述符被当成参数传入(USB 的一个接口表示一个功能)
    // 获取USB 设备描述
    struct usb_device *dev = interface_to_usbdev(intf);
    struct usb_host_interface *interface;
    struct usb_endpoint_descriptor *endpoint;
    struct usb_mouse *mouse;
    struct input_dev *input_dev;
    int pipe, maxp;
    int error = -ENOMEM;

    //  获取当前接口描述
    interface = intf->cur_altsetting;
    // 判断接口是否合法，根据HID规范，鼠标只有一个端点（且不包含端点0）
    if (interface->desc.bNumEndpoints != 1)
        return -ENODEV;
    //  获取端点0的描述符
    endpoint = &interface->endpoint[0].desc;
    //  判断端点类型是否合法，HID 规范，鼠标唯一的端点为中断端点，因为鼠标是中断控制
    if (!usb_endpoint_is_int_in(endpoint))
        return -ENODEV;
    //  创建中断管道(in)，鼠标属于中断控制
    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
    /* 返回该端点能够传输的最大的包长度，鼠标的返回的最大数据包为4个字节。*/
    //初始化URB的时候会用到这个长度，缓冲区的长度要依照maxp来决定，最大不能超过8
    maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

    // 为mouse 申请内存
    //mouse结构的主要作用是赋值给usb_interface中的一个属性
    //以便于触发其它函数的时候通过usb_interface中的这个属性就可以知道相关信息
    //usb_interface中的这个属性是专门为了储存用户需要的数据的
    mouse = kzalloc(sizeof(struct usb_mouse), GFP_KERNEL);
    // 创建input设备
    input_dev = input_allocate_device();
    if (!mouse || !input_dev)
        goto fail1;
    //  为urb 传输申请内存，data 指向该地址空间，初始化urb 缓存区，第四个参数为dma相关
    mouse->data = usb_alloc_coherent(dev, 8, GFP_ATOMIC, &mouse->data_dma);
    if (!mouse->data)
        goto fail1;
    //  申请urb
    mouse->irq = usb_alloc_urb(0, GFP_KERNEL);
    if (!mouse->irq)
        goto fail2;

    // 为mouse 的usbdev，dev 赋值
    mouse->usbdev = dev;
    mouse->dev = input_dev;
    //  为mouse 那么赋值制造商名称
    if (dev->manufacturer)
        strlcpy(mouse->name, dev->manufacturer, sizeof(mouse->name));

    if (dev->product)
    {
        if (dev->manufacturer)
            strlcat(mouse->name, " ", sizeof(mouse->name));
        strlcat(mouse->name, dev->product, sizeof(mouse->name));
    }

    if (!strlen(mouse->name))
        snprintf(mouse->name, sizeof(mouse->name),
                 "USB HIDBP Mouse %04x:%04x",
                 le16_to_cpu(dev->descriptor.idVendor),
                 le16_to_cpu(dev->descriptor.idProduct));

    usb_make_path(dev, mouse->phys, sizeof(mouse->phys));
    strlcat(mouse->phys, "/input0", sizeof(mouse->phys));

    input_dev->name = mouse->name;
    input_dev->phys = mouse->phys;
    // 从dev 设备中获取总线类型，设备id，厂商id，版本号，设置父设备
    usb_to_input_id(dev, &input_dev->id);
    input_dev->dev.parent = &intf->dev;
    // 设置输入设备所支持的事件信息
    // 支持相对坐标和事件
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
    //  记录支持的按键值
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
                                             BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
    // 支持的相对坐标为鼠标移动坐标和滑轮坐标
    input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] |= BIT_MASK(BTN_SIDE) |
                                              BIT_MASK(BTN_EXTRA);
    input_dev->relbit[0] |= BIT_MASK(REL_WHEEL);
    // 将mouse 传入input_dev，方便通过input_dev 获取全部的mouse 信息
    input_set_drvdata(input_dev, mouse);
    // 设置输入设备的open，close函数
    input_dev->open = usb_mouse_open;
    input_dev->close = usb_mouse_close;
    //  填充urb 模块，mouse 作为上下文被设置下去，另外usb_mouse_irq函数为回调
    // 当usb mouse 有事件产生时，回调被调用
    usb_fill_int_urb(mouse->irq, dev, pipe, mouse->data,
                     (maxp > 8 ? 8 : maxp),
                     usb_mouse_irq, mouse, endpoint->bInterval);
    //  mouse->irq 就是urb，如下设置DMA传输相关，当flag为URB_NO_TRANSFER_DMA_MAP时
    // 表示优先使用transfer_dma，而不是transfer buffer
    mouse->irq->transfer_dma = mouse->data_dma;
    mouse->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    //  注册input 设备
    error = input_register_device(mouse->dev);
    if (error)
        goto fail3;
    // 设置mouse 到 usb interface 中
    usb_set_intfdata(intf, mouse);
    return 0;

fail3:
    usb_free_urb(mouse->irq);
fail2:
    usb_free_coherent(dev, 8, mouse->data, mouse->data_dma);
fail1:
    input_free_device(input_dev);
    kfree(mouse);
    return error;
}

// set_intfdata() ，他向内核注册一个data，这个data结构可以是任意的，在这段程序用向内核注册了一个usb_skel结构，就是我们刚刚看到的被初始化的那个，这个data可以在以后用usb_get_intfdata来得到。
static void usb_mouse_disconnect(struct usb_interface *intf)
{
    struct usb_mouse *mouse = usb_get_intfdata(intf);
    //  断开mouse 和usb interface
    usb_set_intfdata(intf, NULL);
    if (mouse)
    {
        usb_kill_urb(mouse->irq);            // kill urb 模块
        input_unregister_device(mouse->dev); //  卸载input设备
        usb_free_urb(mouse->irq);            //  释放urb 模块
        // 释放申请的buffer
        usb_free_coherent(interface_to_usbdev(intf), 8, mouse->data, mouse->data_dma);
        //  释放mouse
        kfree(mouse);
    }
}



static const struct usb_device_id usb_mouse_id_table[] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};

//MODULE_DEVICE_TABLE一般用在热插拔的设备驱动中。
//作用是：将xx_driver_ids结构输出到用户空间，这样模块加载系统在加载模块时，就知道了什么模块对应什么硬件设备。
//用法是：MODULE_DEVICE_TABLE（设备类型，设备表）

MODULE_DEVICE_TABLE(usb, usb_mouse_id_table);

static struct usb_driver usb_mouse_driver = {
	.name		= "usbmouse",
	.probe		= usb_mouse_probe,
	.disconnect	= usb_mouse_disconnect,
	.id_table	= usb_mouse_id_table,
};

module_usb_driver(usb_mouse_driver);
