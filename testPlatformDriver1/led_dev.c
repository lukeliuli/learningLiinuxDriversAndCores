
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
    
static struct resource led_resource[] = {  
};  
    
static void led_device_release(struct device * pdev)  
{  
    
}  
    
static struct platform_device led_device = {  
    .name               = "myled",  
    .id                 = -1,  
    .num_resources      = 0,  
    .resource           = led_resource,  
    .dev = {  
            .release = led_device_release,  
    },  
};  
    
static int __init led_dev_init(void)  
{  
    printk("led_dev_init\n");
    platform_device_register(&led_device);  
        
    return 0;  
}  
    
    
static void led_dev_exit(void)  
{  
    platform_device_unregister(&led_device);  
}  
    
    
module_init(led_dev_init);  
module_exit(led_dev_exit);  
    
MODULE_LICENSE("GPL");  
