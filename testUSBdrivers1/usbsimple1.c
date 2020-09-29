#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
 
 
 
static int usb_simple_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
 
    printk(KERN_INFO"usb_simple_probe\n");
 
    return 0;
}
 
 
static void usb_simple_disconnect(struct usb_interface *intf)
{
    printk(KERN_INFO"usb_mouse_disconnect\n");
}
 
static const struct usb_device_id usb_simple_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
        USB_INTERFACE_PROTOCOL_MOUSE) },
    {} /* Terminating entry */
};
 
static struct usb_driver usb_simple_driver = {
    .name       = "usb_simple",
    .probe      = usb_simple_probe,
    .disconnect = usb_simple_disconnect,
    .id_table   = usb_simple_id_table,
};
 
 
module_usb_driver(usb_simple_driver);
MODULE_LICENSE("GPL");




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * struct usb_driver - identifies USB interface driver to usbcore
 * @name: The driver name should be unique among USB drivers,
 *	and should normally be the same as the module name.
 * @probe: Called to see if the driver is willing to manage a particular
 *	interface on a device.  If it is, probe returns zero and uses
 *	usb_set_intfdata() to associate driver-specific data with the
 *	interface.  It may also use usb_set_interface() to specify the
 *	appropriate altsetting.  If unwilling to manage the interface,
 *	return -ENODEV, if genuine IO errors occurred, an appropriate
 *	negative errno value.
 * @disconnect: Called when the interface is no longer accessible, usually
 *	because its device has been (or is being) disconnected or the
 *	driver module is being unloaded.
 * @unlocked_ioctl: Used for drivers that want to talk to userspace through
 *	the "usbfs" filesystem.  This lets devices provide ways to
 *	expose information to user space regardless of where they
 *	do (or don't) show up otherwise in the filesystem.
 * @suspend: Called when the device is going to be suspended by the
 *	system either from system sleep or runtime suspend context. The
 *	return value will be ignored in system sleep context, so do NOT
 *	try to continue using the device if suspend fails in this case.
 *	Instead, let the resume or reset-resume routine recover from
 *	the failure.
 * @resume: Called when the device is being resumed by the system.
 * @reset_resume: Called when the suspended device has been reset instead
 *	of being resumed.
 * @pre_reset: Called by usb_reset_device() when the device is about to be
 *	reset.  This routine must not return until the driver has no active
 *	URBs for the device, and no more URBs may be submitted until the
 *	post_reset method is called.
 * @post_reset: Called by usb_reset_device() after the device
 *	has been reset
 * @id_table: USB drivers use ID table to support hotplugging.
 *	Export this with MODULE_DEVICE_TABLE(usb,...).  This must be set
 *	or your driver's probe function will never get called.
 * @dynids: used internally to hold the list of dynamically added device
 *	ids for this driver.
 * @drvwrap: Driver-model core structure wrapper.
 * @no_dynamic_id: if set to 1, the USB core will not allow dynamic ids to be
 *	added to this driver by preventing the sysfs file from being created.
 * @supports_autosuspend: if set to 0, the USB core will not allow autosuspend
 *	for interfaces bound to this driver.
 * @soft_unbind: if set to 1, the USB core will not kill URBs and disable
 *	endpoints before calling the driver's disconnect method.
 * @disable_hub_initiated_lpm: if set to 1, the USB core will not allow hubs
 *	to initiate lower power link state transitions when an idle timeout
 *	occurs.  Device-initiated USB 3.0 link PM will still be allowed.
 *
 * USB interface drivers must provide a name, probe() and disconnect()
 * methods, and an id_table.  Other driver fields are optional.
 *
 * The id_table is used in hotplugging.  It holds a set of descriptors,
 * and specialized data may be associated with each entry.  That table
 * is used by both user and kernel mode hotplugging support.
 *
 * The probe() and disconnect() methods are called in a context where
 * they can sleep, but they should avoid abusing the privilege.  Most
 * work to connect to a device should be done when the device is opened,
 * and undone at the last close.  The disconnect code needs to address
 * concurrency issues with respect to open() and close() methods, as
 * well as forcing all pending I/O requests to complete (by unlinking
 * them as necessary, and blocking until the unlinks complete).
 */

//name：方便用户查看设备的具体名称，逻辑上不重要。

//probe：设备和驱动匹配后执行的初始化函数。

//disconnect：设备移除后执行的函数（usb是支持热插拔的）。

//usb_device_id ：和platform中原理一样都是用来驱动和设备做匹配的，但usb和platform不一样的是platform匹配的是name，而usb中支持匹配的项就比较对了，下面一一列出。 
struct usb_driver
{
    const char *name;

    int (*probe)(struct usb_interface *intf,
                 const struct usb_device_id *id);

    void (*disconnect)(struct usb_interface *intf);

    int (*unlocked_ioctl)(struct usb_interface *intf, unsigned int code,
                          void *buf);

    int (*suspend)(struct usb_interface *intf, pm_message_t message);
    int (*resume)(struct usb_interface *intf);
    int (*reset_resume)(struct usb_interface *intf);

    int (*pre_reset)(struct usb_interface *intf);
    int (*post_reset)(struct usb_interface *intf);

    const struct usb_device_id *id_table;

    struct usb_dynids dynids;
    struct usbdrv_wrap drvwrap;
    unsigned int no_dynamic_id : 1;
    unsigned int supports_autosuspend : 1;
    unsigned int disable_hub_initiated_lpm : 1;
    unsigned int soft_unbind : 1;
};

/**
 * struct usb_device_id - identifies USB devices for probing and hotplugging
 * @match_flags: Bit mask controlling which of the other fields are used to
 *	match against new devices. Any field except for driver_info may be
 *	used, although some only make sense in conjunction with other fields.
 *	This is usually set by a USB_DEVICE_*() macro, which sets all
 *	other fields in this structure except for driver_info.
 * @idVendor: USB vendor ID for a device; numbers are assigned
 *	by the USB forum to its members.
 * @idProduct: Vendor-assigned product ID.
 * @bcdDevice_lo: Low end of range of vendor-assigned product version numbers.
 *	This is also used to identify individual product versions, for
 *	a range consisting of a single device.
 * @bcdDevice_hi: High end of version number range.  The range of product
 *	versions is inclusive.
 * @bDeviceClass: Class of device; numbers are assigned
 *	by the USB forum.  Products may choose to implement classes,
 *	or be vendor-specific.  Device classes specify behavior of all
 *	the interfaces on a device.
 * @bDeviceSubClass: Subclass of device; associated with bDeviceClass.
 * @bDeviceProtocol: Protocol of device; associated with bDeviceClass.
 * @bInterfaceClass: Class of interface; numbers are assigned
 *	by the USB forum.  Products may choose to implement classes,
 *	or be vendor-specific.  Interface classes specify behavior only
 *	of a given interface; other interfaces may support other classes.
 * @bInterfaceSubClass: Subclass of interface; associated with bInterfaceClass.
 * @bInterfaceProtocol: Protocol of interface; associated with bInterfaceClass.
 * @bInterfaceNumber: Number of interface; composite devices may use
 *	fixed interface numbers to differentiate between vendor-specific
 *	interfaces.
 * @driver_info: Holds information used by the driver.  Usually it holds
 *	a pointer to a descriptor understood by the driver, or perhaps
 *	device flags.
 *
 * In most cases, drivers will create a table of device IDs by using
 * USB_DEVICE(), or similar macros designed for that purpose.
 * They will then export it to userspace using MODULE_DEVICE_TABLE(),
 * and provide it to the USB core through their usb_driver structure.
 *
 * See the usb_match_id() function for information about how matches are
 * performed.  Briefly, you will normally use one of several macros to help
 * construct these entries.  Each entry you provide will either identify
 * one or more specific products, or will identify a class of products
 * which have agreed to behave the same.  You should put the more specific
 * matches towards the beginning of your table, so that driver_info can
 * record quirks of specific products.
 */


struct usb_device_id
{
    /* which fields to match against? */
    __u16 match_flags;

    /* Used for product specific matches; range is inclusive */
    __u16 idVendor;
    __u16 idProduct;
    __u16 bcdDevice_lo;
    __u16 bcdDevice_hi;

    /* Used for device class matches */
    __u8 bDeviceClass;
    __u8 bDeviceSubClass;
    __u8 bDeviceProtocol;

    /* Used for interface class matches */
    __u8 bInterfaceClass;
    __u8 bInterfaceSubClass;
    __u8 bInterfaceProtocol;

    /* Used for vendor-specific interface matches */
    __u8 bInterfaceNumber;

    /* not matched against */
    kernel_ulong_t driver_info
        __attribute__((aligned(sizeof(kernel_ulong_t))));
};
/*
truct usb_device_id - 识别用于探测和热插拔的USB设备
    @match_flags：位掩码控制哪些其他字段用于匹配新设备。可以使用除driver_info之外的任何字段，但有些字段仅与其他字段一起使用。这通常由USB_DEVICE _ *（）宏设置，该宏设置此结构中除driver_info之外的所有其他字段。
    @idVendor：设备的USB供应商ID;
USB论坛将号码分配给其成员。
    @idProduct：供应商分配的产品ID。
    @bcdDevice_lo：供应商分配的产品版本号范围的低端。此选项还用于标识单个产品版本，包括单个设备。
    @bcdDevice_hi：版本号范围的高端。产品版本范围包括在内。
    @bDeviceClass：设备类;号码由USB论坛分配。产品可以选择实现类，或者是特定于供应商的。设备类指定设备上所有接口的行为。
    @bDeviceSubClass：设备的子类;与bDeviceClass相关联。
    @bDeviceProtocol：设备协议;与bDeviceClass相关联。
    @bInterfaceClass：接口类;号码由USB论坛分配。产品可以选择实现类，或者是特定于供应商的。接口类仅指定给定接口的行为;其他接口可能支持其他类。
    @bInterfaceSubClass：接口的子类;与bInterfaceClass相关联。
    @bInterfaceProtocol：接口协议;与bInterfaceClass相关联。
    @bInterfaceNumber：接口数量;复合设备可以使用固定的接口号来区分特定于供应商的接口。

    @driver_info：保存驱动程序使用的信息。 通常它包含一个指向驱动程序所理解的描述符的指针，或者可能是设备标志。 在大多数情况下，驱动程序将使用USB_DEVICE（）或为此目的设计的类似宏来创建设备ID表。然后，他们将使用MODULE_DEVICE_TABLE（）将其导出到用户空间，并通过其usb_driver结构将其提供给USB内核。 有关如何执行匹配的信息，请参阅usb_match_id（）函数。 简而言之，您通常会使用多个宏中的一个来帮助构造这些条目。 您提供的每个条目将标识一个或多个特定产品，或者将标识已同意行为相同的一类产品。 您应该将更具体的匹配放在表的开头，以便driver_info可以记录特定产品的怪癖。
    */

   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* returns 0 if no match, 1 if match */
int usb_match_one_id_intf(struct usb_device *dev,
                          struct usb_host_interface *intf,
                          const struct usb_device_id *id)
{
    /* The interface class, subclass, protocol and number should never be
	 * checked for a match if the device class is Vendor Specific,
	 * unless the match record specifies the Vendor ID. */
    if (dev->descriptor.bDeviceClass == USB_CLASS_VENDOR_SPEC &&
        !(id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
        (id->match_flags & (USB_DEVICE_ID_MATCH_INT_CLASS |
                            USB_DEVICE_ID_MATCH_INT_SUBCLASS |
                            USB_DEVICE_ID_MATCH_INT_PROTOCOL |
                            USB_DEVICE_ID_MATCH_INT_NUMBER)))
        return 0;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
        (id->bInterfaceClass != intf->desc.bInterfaceClass))
        return 0;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
        (id->bInterfaceSubClass != intf->desc.bInterfaceSubClass))
        return 0;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
        (id->bInterfaceProtocol != intf->desc.bInterfaceProtocol))
        return 0;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_NUMBER) &&
        (id->bInterfaceNumber != intf->desc.bInterfaceNumber))
        return 0;

    return 1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * module_usb_driver() - Helper macro for registering a USB driver
 * @__usb_driver: usb_driver struct
 *
 * Helper macro for USB drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_usb_driver(__usb_driver)       \
    module_driver(__usb_driver, usb_register, \
                  usb_deregister)

/**
 * module_driver() - Helper macro for drivers that don't do anything
 * special in module init/exit. This eliminates a lot of boilerplate.
 * Each module may only use this macro once, and calling it replaces
 * module_init() and module_exit().
 *
 * @__driver: driver name
 * @__register: register function for this driver type
 * @__unregister: unregister function for this driver type
 * @...: Additional arguments to be passed to __register and __unregister.
 *
 * Use this macro to construct bus specific macros for registering
 * drivers, and do not use it on its own.
 */
#define module_driver(__driver, __register, __unregister, ...) \
    static int __init __driver##_init(void)                    \
    {                                                          \
        return __register(&(__driver), ##__VA_ARGS__);         \
    }                                                          \
    module_init(__driver##_init);                              \
    static void __exit __driver##_exit(void)                   \
    {                                                          \
        __unregister(&(__driver), ##__VA_ARGS__);              \
    }                                                          \
    module_exit(__driver##_exit);

static struct usb_driver usb_simple_driver = {

    ......

};

static int __init usb_simple_driver _init(void)
{
    return usb_register(&(usb_simple_driver));
}
module_init(usb_simple_driver _init);
static void __exit usb_simple_driver _exit(void)
{
    usb_deregister(&(usb_simple_driver));
}
module_exit(usb_simple_driver _exit);



/////////////////////////////////////////////////////////////////////////////
/* use a define to avoid include chaining to get THIS_MODULE & friends */
#define usb_register(driver) \
    usb_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

/**
 * usb_register_driver - register a USB interface driver
 * @new_driver: USB operations for the interface driver
 * @owner: module owner of this driver.
 * @mod_name: module name string
 *
 * Registers a USB interface driver with the USB core.  The list of
 * unattached interfaces will be rescanned whenever a new driver is
 * added, allowing the new driver to attach to any recognized interfaces.
 *
 * Return: A negative error code on failure and 0 on success.
 *
 * NOTE: if you want your driver to use the USB major number, you must call
 * usb_register_dev() to enable that functionality.  This function no longer
 * takes care of that.
 */
int usb_register_driver(struct usb_driver *new_driver, struct module *owner,
                        const char *mod_name)
{
    int retval = 0;

    if (usb_disabled())
        return -ENODEV;

    new_driver->drvwrap.for_devices = 0;
    new_driver->drvwrap.driver.name = new_driver->name;
    new_driver->drvwrap.driver.bus = &usb_bus_type;
    new_driver->drvwrap.driver.probe = usb_probe_interface;
    new_driver->drvwrap.driver.remove = usb_unbind_interface;
    new_driver->drvwrap.driver.owner = owner;
    new_driver->drvwrap.driver.mod_name = mod_name;
    spin_lock_init(&new_driver->dynids.lock);
    INIT_LIST_HEAD(&new_driver->dynids.list);

    retval = driver_register(&new_driver->drvwrap.driver);
    if (retval)
        goto out;

    retval = usb_create_newid_files(new_driver);
    if (retval)
        goto out_newid;

    pr_info("%s: registered new interface driver %s\n",
            usbcore_name, new_driver->name);

out:
    return retval;

out_newid:
    driver_unregister(&new_driver->drvwrap.driver);

    printk(KERN_ERR "%s: error %d registering interface "
                    "	driver %s\n",
           usbcore_name, retval, new_driver->name);
    goto out;
}

/**
 * usb_deregister - unregister a USB interface driver
 * @driver: USB operations of the interface driver to unregister
 * Context: must be able to sleep
 *
 * Unlinks the specified driver from the internal USB driver list.
 *
 * NOTE: If you called usb_register_dev(), you still need to call
 * usb_deregister_dev() to clean up your driver's allocated minor numbers,
 * this * call will no longer do it for you.
 */
void usb_deregister(struct usb_driver *driver)
{
    pr_info("%s: deregistering interface driver %s\n",
            usbcore_name, driver->name);

    usb_remove_newid_files(driver);
    driver_unregister(&driver->drvwrap.driver);
    usb_free_dynids(driver);
}

//////
//测试看
https: //blog.csdn.net/qq_16777851/article/details/88373035