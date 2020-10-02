#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>

static struct i2c_adapter *adapter;
static struct i2c_board_info oled_info;
static struct i2c_client *oled_client;

/*
  0.96 OLED使用的是SSD1306控制器
  根据数据手册以及硬件电路，可以知道地址是0x3c
*/
static const unsigned short addr_list[] = {0x3c, I2C_CLIENT_END};

static int oled_dev_init(void)
{
    struct i2c_adapter *adapter;

    memset(&oled_info, 0, sizeof(struct i2c_board_info));
    strlcpy(oled_info.type, "0.96-oled", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(0);
    /*
        法一：i2c_new_probed_device函数是要检测目标设备是否在总线上
        法二：i2c_new_device函数默认目标设备已经在总线上
    */
    oled_client = i2c_new_probed_device(adapter, &oled_info, addr_list, NULL);
    i2c_put_adapter(adapter);

    if (oled_client)
    {
        return 0;
    }
    else
    {
        return -ENODEV;
    }
}

static void oled_dev_exit(void)
{
    i2c_unregister_device(oled_client);
}

module_init(oled_dev_init);
module_exit(oled_dev_exit);
//https: //www.cnblogs.com/Recca/p/12760575.html