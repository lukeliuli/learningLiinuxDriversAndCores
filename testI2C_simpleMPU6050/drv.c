

#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/fs.h>


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
#include <linux/i2c.h>

//先看用python和smbus读取mpu6050 i2c的例子.testSmbusIIC.py
//然后看代码 参考https : //blog.csdn.net/coolliugang123/article/details/71181411

#define MPU6050_REG_PWR_MGMT_1 0x6b
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_REG_GYRO_X 0x43
#define MPU6050_REG_GYRO_Y 0x45
#define MPU6050_REG_GYRO_Z 0x47

#define MPU6050_REG_ACC_X 0x3b
#define MPU6050_REG_ACC_Y 0x3d
#define MPU6050_REG_ACC_Z 0x3f
static struct i2c_client *g_client; //全局指针

struct MPU6050_data
{
    int gyroX; 
    int gyroY; 
    int gyroZ;

    int accX;
    int accY;
    int accZ;
};

static ssize_t mpu6050_write(struct file * file, const char __user *buf, size_t, loff_t *offset);
static ssize_t mpu6050_read(struct file *file, char __user *buf, size_t size, loff_t *offset);
static int mpu6050drv_probe(struct i2c_client *i2c_client, const struct i2c_device_id *i2c_device_id);
static int mpu6050drv_remove(struct i2c_client *i2c_client);

static struct i2c_device_id mpu6050dev2drv_id =
    {
        .name = "mpu6050",
        .driver_data = 0x68,

};

static struct i2c_driver mpu6050drv =
    {
        .driver = {
            .name = "mpu6050",
            .owner = THIS_MODULE,
        },
        .probe = mpu6050drv_probe,
        .remove = mpu6050drv_remove,
        .id_table = &mpu6050dev2drv_id,

};

/* 字符设备相关 */
// static struct class *cls;
// int major = 0;
// static struct cdev mpu6050_cdev;
static struct file_operations mpu6050ops =
    {
        .owner = THIS_MODULE,
        .read = mpu6050_read,
        .write = mpu6050_write,
};


static ssize_t mpu6050_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    //为了好看 容易理解.我没有增加任何自定义函数。完全拆开了写。
    int high;
    int low;
    int gyroTmp;
    int accTmp;

    struct i2c_client *i2c_client = g_client; //全局指针
    struct MPU6050_data mpudata;              //分配内核缓冲区

    ///////读X
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_X);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_X+1);
    gyroTmp = (high << 8) + low;

    if (gyroTmp >= 0x8000)
        gyroTmp = -((65535 - gyroTmp) + 1);
    mpudata.gyroX = gyroTmp / 131;

    ///////读Y
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_Y);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_Y + 1);
    gyroTmp = (high << 8) + low;

    if (gyroTmp >= 0x8000)
        gyroTmp = -((65535 - gyroTmp) + 1);
    mpudata.gyroY = gyroTmp / 131;

    ///////读Z
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_Z);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_GYRO_Z + 1);
    gyroTmp = (high << 8) + low;

    if (gyroTmp >= 0x8000)
        gyroTmp = -((65535 - gyroTmp) + 1);
    mpudata.gyroY = gyroTmp / 131;

    //读ACC_X
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_X);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_X + 1);
    accTmp = (high << 8) + low;
    mpudata.accX = accTmp / 16384.0;

    //读ACC_Y
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_Y);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_Y + 1);
    accTmp = (high << 8) + low;
    mpudata.accY = accTmp / 16384.0;

    //读ACC_Z
    high = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_Z);
    low = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_ACC_Z + 1);
    accTmp = (high << 8) + low;
    mpudata.accZ = accTmp / 16384.0;

    copy_to_user((struct MPU6050_data *)buf, &mpudata, sizeof(mpudata));
}

static ssize_t mpu6050_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    printK("mpu6050_write and IOCTRL,Please DIY ");
    return 0;
}

//这个mpu6050drv_probe，注册为字符设备，作为备用。
// static int mpu6050drv_probe(struct i2c_client *i2c_client, const struct i2c_device_id *i2c_device_id)
// {
//     dev_t dev = 0;
//     printk("mpu6050drv_probe\r\n");
// #if 0
//     /* 把mpu6050当做字符设备注册到内核 */
//     major = register_chrdev ( 0, "mpu6050", &mpu6050ops );
// #else
//     alloc_chrdev_region(&dev, 0, 2, "mpu6050_region"); // 占用2个次设备号
//     cdev_init(&mpu6050_cdev, &mpu6050ops);
//     mpu6050_cdev.owner = THIS_MODULE;
//     cdev_add(&mpu6050_cdev, dev, 2); // 占用2个次设备号

// #endif

//     cls = class_create(THIS_MODULE, "mpu6050cls");
//     major = MAJOR(dev);
//     device_create(cls, NULL, MKDEV(major, 0), NULL, "mpu6050");
//     //    device_create ( cls, NULL, MKDEV ( major, 1 ), NULL, "mpu6050_2" );

//     return 0;
// }
// static int mpu6050drv_remove(struct i2c_client *i2c_client)
// {
//     printk("mpu6050drv_remove\r\n");
//     unregister_chrdev_region(0, 1);
//     cdev_del(&mpu6050_cdev);
//     device_destroy(cls, MKDEV(major, 0));
//     //    device_destroy ( cls, MKDEV ( major, 1 ) );
//     class_destroy(cls);
//     return 0;
// }

//这个mpu6050drv_probe，注册为混杂设备，简单就好.
//无论混杂还是字符设备都是面对用户用于操控设备的接口。
//也就是让用户看到在/dev/下看到mpu6050的设备，并用open,close，read,write进行操作
#define DevDrvName "mpu6050"
static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DevDrvName,
    .fops = &mpu6050ops,
};



static int mpu6050drv_probe(struct i2c_client *i2c_client, const struct i2c_device_id *i2c_device_id)
{
    dev_t dev = 0;
    printk("mpu6050drv_probe\r\n");
    misc_register(&misc); //注册主设备号为10的特殊字符设备  
    printk("mpu6050_misc_init\n");

    u16 version;
    i2c_smbus_write_byte_data(i2c_client, MPU6050_REG_PWR_MGMT_1, 0);
    version = i2c_smbus_read_byte_data(i2c_client, MPU6050_REG_WHO_AM_I);
    printk(" mpu6050_hw_init,%d", version);
    g_client = i2c_client; //全局指针
    return 0;
}
static int mpu6050drv_remove(struct i2c_client *i2c_client)
{
    misc_deregister(&misc);
    printk("mpu6050_misc_exit\n");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////
static int mpu6050_init(void)
{
    i2c_add_driver(&mpu6050drv);
    return 0;
}

static void mpu6050_exit(void)
{
    i2c_del_driver(&mpu6050drv);
}

module_init(mpu6050_init);
module_exit(mpu6050_exit);
MODULE_LICENSE("GPL");