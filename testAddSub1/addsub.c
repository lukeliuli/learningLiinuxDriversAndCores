#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "addsub.h"
long addInteger(long a, long b)
{
    return a + b;
}
long subInteger(long a, long b)
{
    return a - b;
}
EXPORT_SYMBOL(addInteger);
EXPORT_SYMBOL(subInteger);
MODULE_LICENSE("Dual BSD/GPL");


