

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include "mygpio.h"

#define DEVICE_NAME "pi_Led"
#define DRIVER_NAME "pi_Led"

//class声明内核模块驱动信息,是UDEV能够自动生成/dev下相应文件
static dev_t pi_led_devno; //设备号
static struct class *pi_led_class;
static struct cdev pi_led_class_dev;


//////////////////////////////////////////////////////
//GPIO related
#define BCM2835_GPSET0 0x001c
#define BCM2835_GPFSEL0 0x0000
#define BCM2835_GPCLR0 0x0028
#define BCM2835_GPIO_FSEL_OUTP 1
#define BCM2835_GPIO_BASE 0x3f200000

int bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
    //初始化GPIOB功能选择寄存器的物理地址
    volatile uint32_t *bcm2835_gpio = (volatile uint32_t *)ioremap(BCM2835_GPIO_BASE, 16);
    volatile uint32_t *bcm2835_gpio_fsel = bcm2835_gpio + BCM2835_GPFSEL0 / 4 + (pin / 10);
    uint8_t shift = (pin % 10) * 3;
    uint32_t value = mode << shift;
    *bcm2835_gpio_fsel = *bcm2835_gpio_fsel | value;

    printk("fsel address: 0x%lx : %x\n", (long unsigned int)bcm2835_gpio_fsel, *bcm2835_gpio_fsel);

    return 0;
}

int bcm2835_gpio_set(uint8_t pin)
{
    //GPIO输出功能物理地址
    volatile uint32_t *bcm2835_gpio = (volatile uint32_t *)ioremap(BCM2835_GPIO_BASE, 16);
    volatile uint32_t *bcm2835_gpio_set = bcm2835_gpio + BCM2835_GPSET0 / 4 + pin / 32;
    uint8_t shift = pin % 32;
    uint32_t value = 1 << shift;
    *bcm2835_gpio_set = *bcm2835_gpio_set | value;

    printk("set address:  0x%lx : %x\n", (long unsigned int)bcm2835_gpio_set, *bcm2835_gpio_set);

    return 0;
}

int bcm2835_gpio_clr(uint8_t pin)
{
    //GPIO清除功能物理地址
    volatile uint32_t *bcm2835_gpio = (volatile uint32_t *)ioremap(BCM2835_GPIO_BASE, 16);
    volatile uint32_t *bcm2835_gpio_clr = bcm2835_gpio + BCM2835_GPCLR0 / 4 + pin / 32;
    uint8_t shift = pin % 32;
    uint32_t value = 1 << shift;
    *bcm2835_gpio_clr = *bcm2835_gpio_clr | value;

    printk("clr address:  0x%lx : %x\n", (long unsigned int)bcm2835_gpio_clr, *bcm2835_gpio_clr);

    return 0;
}



#define PIN 26 //GPIO26

//这部分函数为内核调用后open的设备IO操作,和裸板程序一样
int open_flag = 0;
static int pi_led_open(struct inode *inode, struct file *filp)
{
   printk("Open led ing!\n");
   if (open_flag == 0)
   {
      open_flag = 1;
      printk("Open led success!\n");
      return 0;
   }
   else
   {
      printk("Led has opened!\n");
   }
   return 0;
}
//这部分函数为内核调用后ioctl的设备IO操作,和裸板程序一样
static long pi_led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   switch (cmd)
   {
   case 0:
      bcm2835_gpio_clr(PIN);
      printk("LED OFF!\n");
      break;
   case 1:
      bcm2835_gpio_set(PIN);
      printk("LED ON!\n");
      break;

   default:
      return -EINVAL;
   }

   return 0;
}

static int pi_led_release(struct inode *inode, struct file *file)
{

   if (open_flag == 1)
   {
      open_flag= 0;
      printk("close file success!\n");
      return 0;
   }
   else
   {
      printk("The file has closed!\n");
      return -1;
   }
}

//file_operations使系统的open,ioctl等函数指针指向我们所写的led_open等函数,
//这样系统才能够调用
static struct file_operations pi_led_dev_fops = {
    .owner = THIS_MODULE,
    .open = pi_led_open,
    .unlocked_ioctl = pi_led_ioctl,
    .release = pi_led_release,
};

//内核加载后的初始化函数.
static int __init pi_led_init(void)
{
   struct device *dev;
   int major; //自动分配主设备号
   major = alloc_chrdev_region(&pi_led_devno, 0, 1, DRIVER_NAME);
   //register_chrdev 注册字符设备使系统知道有LED这个模块在.

   cdev_init(&pi_led_class_dev, &pi_led_dev_fops);
   major = cdev_add(&pi_led_class_dev, pi_led_devno, 1);
   //注册class
   pi_led_class = class_create(THIS_MODULE, DRIVER_NAME);

   dev = device_create(pi_led_class, NULL, pi_led_devno, NULL, DRIVER_NAME);

   //配置功能选择寄存器为输出
   bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);

   //设置输出电平为高电平，LED亮
   bcm2835_gpio_set(PIN);

   printk("pi led init ok!\n");
   return 0;
}
//内核卸载后的销毁函数.
void pi_led_exit(void)
{
   bcm2835_gpio_clr(PIN);
   device_destroy(pi_led_class, pi_led_devno);
   class_destroy(pi_led_class);
   cdev_del(&pi_led_class_dev);
   unregister_chrdev_region(pi_led_devno, 1);
   printk("pi led exit ok!\n");
}

module_init(pi_led_init);
module_exit(pi_led_exit);
MODULE_LICENSE("GPL");
