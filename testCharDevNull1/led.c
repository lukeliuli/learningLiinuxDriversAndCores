
#include <linux/types.h>   

#include <linux/ioctl.h>  
#include <linux/delay.h>  
#include <linux/uaccess.h>
#include <linux/init.h>  
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
#include <linux/mm.h>
#include <asm/io.h>

#define DEVICE_NAME "pi_Led"
#define DRIVER_NAME "pi_Led"

//代码删除所有LED硬件相关操作，只是一个字符设备空壳

//class声明内核模块驱动信息,是UDEV能够自动生成/dev下相应文件
static dev_t pi_led_devno; //设备号
static struct class *pi_led_class;
static struct cdev pi_led_class_dev;




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
  
      printk("LED OFF!\n");
      break;
   case 1:
      
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
      open_flag = 0;
      printk("close file success!\n");
      return 0;
   }
   else
   {
      printk("The file has closed!\n");
      return -1;
   }
}

static ssize_t pi_led_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{

   //unsigned long copy_to_user(void __user *to, const void *from, unsigned long count);
   char echo[] = "led_read";
   unsigned int count = strlen(echo)+1;
   copy_to_user(buf, (void *)(echo), count);
   return count;
}

//file_operations使系统的open,ioctl等函数指针指向我们所写的led_open等函数,
//这样系统才能够调用
static struct file_operations pi_led_dev_fops = {
    .owner = THIS_MODULE,
    .open = pi_led_open,
    .unlocked_ioctl = pi_led_ioctl,
    .release = pi_led_release,
    .read = pi_led_read,
};

//内核加载后的初始化函数.
static int __init pi_led_init(void)
{
   struct device *dev;
   int major; //自动分配主设备号
   int rvl;
   int ma;
   int mi;
   rvl = alloc_chrdev_region(&pi_led_devno, 0, 1, DRIVER_NAME);
   //register_chrdev 注册字符设备使系统知道有LED这个模块在.

   ma = MAJOR(pi_led_devno);    //主设备号
   mi = MINOR(pi_led_devno);    //次设备号
    printk("%d,%d\n", ma,mi);

   //mknod /dev/pi_Led c 236 0
   //rm -rf /dev/pi_Led
   
   cdev_init(&pi_led_class_dev, &pi_led_dev_fops);
   major = cdev_add(&pi_led_class_dev, pi_led_devno, 1);
   //注册class
   //echo /sbin/mdev > /proc/sys/kernel/hotplug
   pi_led_class = class_create(THIS_MODULE, DRIVER_NAME);

    if(IS_ERR(pi_led_class)) 
     {
          printk("Err: failed in creating class.\n");
          return -1; 
      }
   dev = device_create(pi_led_class, NULL, pi_led_devno, NULL, DRIVER_NAME);



   printk("pi led init ok!\n");
   printk("%d,%d\n", ma,mi);
   return 0;
}
//内核卸载后的销毁函数.
void pi_led_exit(void)
{
  
   device_destroy(pi_led_class, pi_led_devno);
   class_destroy(pi_led_class);
   cdev_del(&pi_led_class_dev);
   unregister_chrdev_region(pi_led_devno, 1);
   printk("pi led exit ok!\n");
}

module_init(pi_led_init);
module_exit(pi_led_exit);
MODULE_LICENSE("GPL");
