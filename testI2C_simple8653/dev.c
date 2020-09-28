#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>

static const unsigned short addr_list[] = {0x50, 0x68, I2C_CLIENT_END};
static struct i2c_board_info LL_info = {
    .type = "LLI2C",
};
static struct i2c_client *LL_client;
//https://blog.csdn.net/tangkegagalikaiwu/article/details/8563392
static int dev_init(void)
{
    struct i2c_adapter *i2c_adap;
    i2c_adap = i2c_get_adapter(0);
    LL_client = i2c_new_probed_device(i2c_adap, &LL_info, addr_list, NULL);
    i2c_put_adapter(i2c_adap);

    if (LL_client)
        return 0;
    else
        return -ENODEV;
}
static void dev_exit(void)
{
    i2c_unregister_device(LL_client);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");