#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "addsub.h"
MODULE_LICENSE("Dual BSD/GPL");

static long a = 2;
static long b = 1;
static long AddorSub = 1;
static int addsub_init(void)
{
    long ret = 0;
    printk(KERN_ALERT "Hello, world\n");
    if (AddorSub == 1)
    {
        ret = addInteger(a, b);
    }
    if (AddorSub == 2)
    {
        ret = subInteger(a, b);
    }
    printk("ret is %d\n", ret);

    return 0;
}

static void addsub_exit(void)
{
    printk(KERN_ALERT "Goodbye, world\n");
}

module_init(addsub_init);
module_exit(addsub_exit);

module_param(AddorSub, long, S_IRUGO);
module_param(a, long, S_IRUGO);
module_param(b, long, S_IRUGO);
//sudo insmod helloaddsub.ko a=1 b=2 AddorSub=2