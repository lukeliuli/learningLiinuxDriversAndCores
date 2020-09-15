//https://www.cnblogs.com/lifexy/p/7569371.html

//latform device 仅仅是为了char/block设备注册做前提条件.至少nand flash 是这样的.
//platform device是用来匹配相应的platform driver并触发driver的probe接口，而字符设备或块设备就是在那里注册的。
//platform只是用来传送变量的一套框架具体是什么设备类型 看他内部函数里\代码里 他注册和开辟的是blk还是普通的char
//http://blog.chinaunix.net/uid-25844538-id-4900206.html 
#include <linux/kernel.h>  

#include <linux/module.h>

#include <linux/device.h>

#include <linux/platform_device.h>

#include <linux/types.h>  

#include <linux/fs.h>   

#include <linux/ioctl.h>  

#include <linux/cdev.h>  

#include <linux/delay.h>  

#include <linux/uaccess.h>

#include <linux/init.h> 

static struct class *cls;                                      //类,用来注册,和注销
/*函数声明*/
static  int  led_remove(struct platform_device *led_dev);
static  int led_probe(struct platform_device *led_dev);
static  int dev_major = 0;
static  int dev_minor =0;

struct platform_driver led_drv = {
       .probe           = led_probe,        //当与设备匹配,则调用该函数
       .remove         = led_remove,             //删除设备

       .driver            = {
              .name     = "myled",           //与设备名称一样
       }
};

static int led_open(struct inode *inode, struct file  *file)
 {
         
       return 0;
 } 

static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
       
       return 0 ;
}


static struct  file_operations led_fops= {
    .owner  =   THIS_MODULE,     //被使用时阻止模块被卸载
    .open   =   led_open,     
    .write   =   led_write,   
  };

 
static int led_probe(struct platform_device *pdev)
{
   
       dev_major= register_chrdev(0, "myled", &led_fops);   //赋入file_operations结构体,dynamic alloc
       cls = class_create(THIS_MODULE, "myled");
       device_create(cls, NULL, MKDEV(dev_major, dev_minor), NULL, "myled"); /* /dev/myled */
       
       return 0;
}
static int led_remove(struct platform_device *pdev)
{
       /* 卸载字符设备驱动程序 */
       device_destroy(cls, MKDEV(dev_major, 0));
       class_destroy(cls);
       unregister_chrdev(dev_major, "myled");
       
       return 0;
}

static int led_drv_init(void)           //入口函数,注册驱动
{
       platform_driver_register(&led_drv);
       return 0;
} 
static void led_drv_exit(void)       //出口函数,卸载驱动
{
       platform_driver_unregister(&led_drv);
}

module_init(led_drv_init);     
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");

