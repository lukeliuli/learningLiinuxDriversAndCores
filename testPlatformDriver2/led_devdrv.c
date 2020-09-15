
//https://www.cnblogs.com/lifexy/p/7569371.html
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

#define DevDrvName "mypiled"
//////////////////////////////////////////////////////////
//platform driver 部分
static struct class *cls;
static  int led_remove(struct platform_device *led_dev);
static  int led_probe(struct platform_device *led_dev);
static  int dev_major = 0;
static  int dev_minor =0;

struct platform_driver led_drv = 
{
       .probe           = led_probe,        //当与设备匹配,则调用该函数
       .remove          = led_remove,             //删除设备

       .driver            = {
              .name     = DevDrvName,           //与设备名称一样
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


static struct  file_operations led_fops= 
{
    .owner  =   THIS_MODULE,     //被使用时阻止模块被卸载
    .open   =   led_open,     
    .write   =  led_write,   
};

static int led_probe(struct platform_device *pdev)
{
   
       dev_major= register_chrdev(0, DevDrvName, &led_fops);   //赋入file_operations结构体,dynamic alloc
       cls = class_create(THIS_MODULE, DevDrvName);
       device_create(cls, NULL, MKDEV(dev_major, dev_minor), NULL, DevDrvName); /* /dev/mypiled */
       
       return 0;
}
static int led_remove(struct platform_device *pdev)
{
       /* 卸载字符设备驱动程序 */
       device_destroy(cls, MKDEV(dev_major, 0));
       class_destroy(cls);
       unregister_chrdev(dev_major, DevDrvName);
       return 0;
}

////////////////////////////////////////////////////////
//platform device 部分
static struct resource led_resource[] =
{  
};  
    
static void led_device_release(struct device * pdev)  
{  
    
}  


static struct platform_device led_device = {  
    .name               = DevDrvName,  
    .id                 = -1,  
    .num_resources      = 0,  
    .resource           = led_resource,  
    .dev = {  
            .release = led_device_release,  
    },  
};  
 
 /////////////////////////////////////////////////////////////
 ///入口   
static int __init led_dd_init(void)  
{  
    printk("led_dd_init\n");
    platform_device_register(&led_device); 
    platform_driver_register(&led_drv); 
        
    return 0;  
}  
    
    
static void led_dd_exit(void)  
{  
    platform_device_unregister(&led_device);  
    platform_driver_unregister(&led_drv);
    printk("led_dd_exit\n");
}  
    
    
module_init(led_dd_init);  
module_exit(led_dd_exit);  
    
MODULE_LICENSE("GPL");  
