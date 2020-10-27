
//https://blog.csdn.net/SLASH_24/article/details/54952767
#include <linux/kernel.h>  

#include <linux/module.h>

#include <linux/device.h>



#include <linux/types.h>  

#include <linux/fs.h>   

#include <linux/ioctl.h>  


#include <linux/delay.h>  

#include <linux/uaccess.h>

#include <linux/init.h>  

#include <linux/miscdevice.h>

#define DevDrvName "mymiscled"



static int led_open(struct inode *inode, struct file  *file)
 {
         
       return 0;
 } 

static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
       
       return 0 ;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{

       //unsigned long copy_to_user(void __user *to, const void *from, unsigned long count);
       char*echo= "led_read";
       unsigned int count = strlen(echo)+1;
       copy_to_user(buf, (void *)(echo), count);
       return count;
}

////////////////////////////////////////////////////////
//misc device 部分
static struct file_operations led_fops =
    {
        .owner = THIS_MODULE, //被使用时阻止模块被卸载
        .open = led_open,
        .write = led_write,
        .read = led_read,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DevDrvName,
    .fops = &led_fops,
};

/////////////////////////////////////////////////////////////
///入口   
static int __init led_misc_init(void)  
{
       misc_register(&misc); //注册主设备号为10的特殊字符设备  
       printk("led_misc_init\n");

       return 0;  
}  
    
    
static void led_misc_exit(void)  
{
       misc_deregister(&misc);
       printk("led_misc_exit\n");
}

module_init(led_misc_init);
module_exit(led_misc_exit);

MODULE_LICENSE("GPL");  
