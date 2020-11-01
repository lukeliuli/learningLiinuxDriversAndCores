// SPDX-License-Identifier: GPL-2.0
/*
 * Silicon Laboratories CP210x USB to RS232 serial adaptor driver
 *
 * Copyright (C) 2005 Craig Shelley (craig@microtron.org.uk)
 *
 * Support to set flow control line levels using TIOCMGET and TIOCMSET
 * thanks to Karl Hiramoto karl@hiramoto.org. RTSCTS hardware flow
 * control thanks to Munir Nassar nassarmu@real-time.com
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
#include <linux/usb/serial.h>
#include <linux/gpio/driver.h>
#include <linux/bitops.h>
#include <linux/mutex.h>

#define DRIVER_DESC "sust-cekong usbserial(cp210x,0x10C4, 0xEA88) driver;Silicon Labs CP210x RS232 serial adaptor driver"

/*
 * Function Prototypes
 */
static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *);
static void cp210x_close(struct usb_serial_port *);
static void cp210x_get_termios(struct tty_struct *, struct usb_serial_port *);
static void cp210x_get_termios_port(struct usb_serial_port *port,
	tcflag_t *cflagp, unsigned int *baudp);
static void cp210x_change_speed(struct tty_struct *, struct usb_serial_port *,
							struct ktermios *);
static void cp210x_set_termios(struct tty_struct *, struct usb_serial_port *,
							struct ktermios*);
static bool cp210x_tx_empty(struct usb_serial_port *port);
static int cp210x_tiocmget(struct tty_struct *);
static int cp210x_tiocmset(struct tty_struct *, unsigned int, unsigned int);
static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int, unsigned int);
static void cp210x_break_ctl(struct tty_struct *, int);
static int cp210x_attach(struct usb_serial *);
static void cp210x_disconnect(struct usb_serial *);
static void cp210x_release(struct usb_serial *);
static int cp210x_port_probe(struct usb_serial_port *);
static int cp210x_port_remove(struct usb_serial_port *);
static void cp210x_dtr_rts(struct usb_serial_port *p, int on);

static const struct usb_device_id id_table[] = {
	
	{ USB_DEVICE(0x10C4, 0xEA88) }, /* Silicon Labs factory default */
	
	{ } /* Terminating Entry */
};

MODULE_DEVICE_TABLE(usb, id_table);

struct cp210x_serial_private {
#ifdef CONFIG_GPIOLIB
	struct gpio_chip	gc;
	bool			gpio_registered;
	u8			gpio_pushpull;
	u8			gpio_altfunc;
	u8			gpio_input;
#endif
	u8			partnum;
	speed_t			min_speed;
	speed_t			max_speed;
	bool			use_actual_rate;
};

struct cp210x_port_private {
	__u8			bInterfaceNumber;
	bool			has_swapped_line_ctl;
};

static struct usb_serial_driver cp210x_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"csust-cekong-cp210x",
	},
	.id_table		= id_table,
	.num_ports		= 1,
	.bulk_in_size		= 256,
	.bulk_out_size		= 256,
	.open			= cp210x_open,
	.close			= cp210x_close,
	.break_ctl		= cp210x_break_ctl,
	.set_termios		= cp210x_set_termios,
	.tx_empty		= cp210x_tx_empty,
	.throttle		= usb_serial_generic_throttle,
	.unthrottle		= usb_serial_generic_unthrottle,
	.tiocmget		= cp210x_tiocmget,
	.tiocmset		= cp210x_tiocmset,
	.attach			= cp210x_attach,
	.disconnect		= cp210x_disconnect,
	.release		= cp210x_release,
	.port_probe		= cp210x_port_probe,
	.port_remove		= cp210x_port_remove,
	.dtr_rts		= cp210x_dtr_rts
};

static struct usb_serial_driver * const serial_drivers[] = {
	&cp210x_device, NULL
};

/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
#define REQTYPE_INTERFACE_TO_HOST	0xc1
#define REQTYPE_HOST_TO_DEVICE	0x40
#define REQTYPE_DEVICE_TO_HOST	0xc0

/* Config request codes */
#define CP210X_IFC_ENABLE	0x00
#define CP210X_SET_BAUDDIV	0x01
#define CP210X_GET_BAUDDIV	0x02
#define CP210X_SET_LINE_CTL	0x03
#define CP210X_GET_LINE_CTL	0x04
#define CP210X_SET_BREAK	0x05
#define CP210X_IMM_CHAR		0x06
#define CP210X_SET_MHS		0x07
#define CP210X_GET_MDMSTS	0x08
#define CP210X_SET_XON		0x09
#define CP210X_SET_XOFF		0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR		0x0D
#define CP210X_GET_CHARS	0x0E
#define CP210X_GET_PROPS	0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET		0x11
#define CP210X_PURGE		0x12
#define CP210X_SET_FLOW		0x13
#define CP210X_GET_FLOW		0x14
#define CP210X_EMBED_EVENTS	0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS	0x19
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_SET_BAUDRATE	0x1E
#define CP210X_VENDOR_SPECIFIC	0xFF

/* CP210X_IFC_ENABLE */
#define UART_ENABLE		0x0001
#define UART_DISABLE		0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ	0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK		0X0f00
#define BITS_DATA_5		0X0500
#define BITS_DATA_6		0X0600
#define BITS_DATA_7		0X0700
#define BITS_DATA_8		0X0800
#define BITS_DATA_9		0X0900

#define BITS_PARITY_MASK	0x00f0
#define BITS_PARITY_NONE	0x0000
#define BITS_PARITY_ODD		0x0010
#define BITS_PARITY_EVEN	0x0020
#define BITS_PARITY_MARK	0x0030
#define BITS_PARITY_SPACE	0x0040

#define BITS_STOP_MASK		0x000f
#define BITS_STOP_1		0x0000
#define BITS_STOP_1_5		0x0001
#define BITS_STOP_2		0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON		0x0001
#define BREAK_OFF		0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR		0x0001
#define CONTROL_RTS		0x0002
#define CONTROL_CTS		0x0010
#define CONTROL_DSR		0x0020
#define CONTROL_RING		0x0040
#define CONTROL_DCD		0x0080
#define CONTROL_WRITE_DTR	0x0100
#define CONTROL_WRITE_RTS	0x0200

/* CP210X_VENDOR_SPECIFIC values */
#define CP210X_READ_2NCONFIG	0x000E
#define CP210X_READ_LATCH	0x00C2
#define CP210X_GET_PARTNUM	0x370B
#define CP210X_GET_PORTCONFIG	0x370C
#define CP210X_GET_DEVICEMODE	0x3711
#define CP210X_WRITE_LATCH	0x37E1

/* Part number definitions */
#define CP210X_PARTNUM_CP2101	0x01
#define CP210X_PARTNUM_CP2102	0x02
#define CP210X_PARTNUM_CP2103	0x03
#define CP210X_PARTNUM_CP2104	0x04
#define CP210X_PARTNUM_CP2105	0x05
#define CP210X_PARTNUM_CP2108	0x08
#define CP210X_PARTNUM_CP2102N_QFN28	0x20
#define CP210X_PARTNUM_CP2102N_QFN24	0x21
#define CP210X_PARTNUM_CP2102N_QFN20	0x22
#define CP210X_PARTNUM_UNKNOWN	0xFF

/* CP210X_GET_COMM_STATUS returns these 0x13 bytes */
struct cp210x_comm_status {
	__le32   ulErrors;
	__le32   ulHoldReasons;
	__le32   ulAmountInInQueue;
	__le32   ulAmountInOutQueue;
	u8       bEofReceived;
	u8       bWaitForImmediate;
	u8       bReserved;
} __packed;

/*
 * CP210X_PURGE - 16 bits passed in wValue of USB request.
 * SiLabs app note AN571 gives a strange description of the 4 bits:
 * bit 0 or bit 2 clears the transmit queue and 1 or 3 receive.
 * writing 1 to all, however, purges cp2108 well enough to avoid the hang.
 */
#define PURGE_ALL		0x000f

/* CP210X_GET_FLOW/CP210X_SET_FLOW read/write these 0x10 bytes */
struct cp210x_flow_ctl {
	__le32	ulControlHandshake;
	__le32	ulFlowReplace;
	__le32	ulXonLimit;
	__le32	ulXoffLimit;
} __packed;

/* cp210x_flow_ctl::ulControlHandshake */
#define CP210X_SERIAL_DTR_MASK		GENMASK(1, 0)
#define CP210X_SERIAL_DTR_SHIFT(_mode)	(_mode)
#define CP210X_SERIAL_CTS_HANDSHAKE	BIT(3)
#define CP210X_SERIAL_DSR_HANDSHAKE	BIT(4)
#define CP210X_SERIAL_DCD_HANDSHAKE	BIT(5)
#define CP210X_SERIAL_DSR_SENSITIVITY	BIT(6)

/* values for cp210x_flow_ctl::ulControlHandshake::CP210X_SERIAL_DTR_MASK */
#define CP210X_SERIAL_DTR_INACTIVE	0
#define CP210X_SERIAL_DTR_ACTIVE	1
#define CP210X_SERIAL_DTR_FLOW_CTL	2

/* cp210x_flow_ctl::ulFlowReplace */
#define CP210X_SERIAL_AUTO_TRANSMIT	BIT(0)
#define CP210X_SERIAL_AUTO_RECEIVE	BIT(1)
#define CP210X_SERIAL_ERROR_CHAR	BIT(2)
#define CP210X_SERIAL_NULL_STRIPPING	BIT(3)
#define CP210X_SERIAL_BREAK_CHAR	BIT(4)
#define CP210X_SERIAL_RTS_MASK		GENMASK(7, 6)
#define CP210X_SERIAL_RTS_SHIFT(_mode)	(_mode << 6)
#define CP210X_SERIAL_XOFF_CONTINUE	BIT(31)

/* values for cp210x_flow_ctl::ulFlowReplace::CP210X_SERIAL_RTS_MASK */
#define CP210X_SERIAL_RTS_INACTIVE	0
#define CP210X_SERIAL_RTS_ACTIVE	1
#define CP210X_SERIAL_RTS_FLOW_CTL	2

/* CP210X_VENDOR_SPECIFIC, CP210X_GET_DEVICEMODE call reads these 0x2 bytes. */
struct cp210x_pin_mode {
	u8	eci;
	u8	sci;
} __packed;

#define CP210X_PIN_MODE_MODEM		0
#define CP210X_PIN_MODE_GPIO		BIT(0)

/*
 * CP210X_VENDOR_SPECIFIC, CP210X_GET_PORTCONFIG call reads these 0xf bytes
 * on a CP2105 chip. Structure needs padding due to unused/unspecified bytes.
 */
struct cp210x_dual_port_config {
	__le16	gpio_mode;
	u8	__pad0[2];
	__le16	reset_state;
	u8	__pad1[4];
	__le16	suspend_state;
	u8	sci_cfg;
	u8	eci_cfg;
	u8	device_cfg;
} __packed;

/*
 * CP210X_VENDOR_SPECIFIC, CP210X_GET_PORTCONFIG call reads these 0xd bytes
 * on a CP2104 chip. Structure needs padding due to unused/unspecified bytes.
 */
struct cp210x_single_port_config {
	__le16	gpio_mode;
	u8	__pad0[2];
	__le16	reset_state;
	u8	__pad1[4];
	__le16	suspend_state;
	u8	device_cfg;
} __packed;

/* GPIO modes */
#define CP210X_SCI_GPIO_MODE_OFFSET	9
#define CP210X_SCI_GPIO_MODE_MASK	GENMASK(11, 9)

#define CP210X_ECI_GPIO_MODE_OFFSET	2
#define CP210X_ECI_GPIO_MODE_MASK	GENMASK(3, 2)

#define CP210X_GPIO_MODE_OFFSET		8
#define CP210X_GPIO_MODE_MASK		GENMASK(11, 8)

/* CP2105 port configuration values */
#define CP2105_GPIO0_TXLED_MODE		BIT(0)
#define CP2105_GPIO1_RXLED_MODE		BIT(1)
#define CP2105_GPIO1_RS485_MODE		BIT(2)

/* CP2104 port configuration values */
#define CP2104_GPIO0_TXLED_MODE		BIT(0)
#define CP2104_GPIO1_RXLED_MODE		BIT(1)
#define CP2104_GPIO2_RS485_MODE		BIT(2)

/* CP2102N configuration array indices */
#define CP210X_2NCONFIG_CONFIG_VERSION_IDX	2
#define CP210X_2NCONFIG_GPIO_MODE_IDX		581
#define CP210X_2NCONFIG_GPIO_RSTLATCH_IDX	587
#define CP210X_2NCONFIG_GPIO_CONTROL_IDX	600

/* CP210X_VENDOR_SPECIFIC, CP210X_WRITE_LATCH call writes these 0x2 bytes. */
struct cp210x_gpio_write {
	u8	mask;
	u8	state;
} __packed;

/*
 * Helper to get interface number when we only have struct usb_serial.
 */
static u8 cp210x_interface_num(struct usb_serial *serial)
{
	struct usb_host_interface *cur_altsetting;

	cur_altsetting = serial->interface->cur_altsetting;

	return cur_altsetting->desc.bInterfaceNumber;
}

/*
 * Reads a variable-sized block of CP210X_ registers, identified by req.
 * Returns data into buf in native USB byte order.
 */
static int cp210x_read_reg_block(struct usb_serial_port *port, u8 req,
		void *buf, int bufsize)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	void *dmabuf;
	int result;

	dmabuf = kmalloc(bufsize, GFP_KERNEL);
	if (!dmabuf) {
		/*
		 * FIXME Some callers don't bother to check for error,
		 * at least give them consistent junk until they are fixed
		 */
		memset(buf, 0, bufsize);
		return -ENOMEM;
	}

	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			req, REQTYPE_INTERFACE_TO_HOST, 0,
			port_priv->bInterfaceNumber, dmabuf, bufsize,
			USB_CTRL_SET_TIMEOUT);
	if (result == bufsize) {
		memcpy(buf, dmabuf, bufsize);
		result = 0;
	} else {
		dev_err(&port->dev, "failed get req 0x%x size %d status: %d\n",
				req, bufsize, result);
		if (result >= 0)
			result = -EIO;

		/*
		 * FIXME Some callers don't bother to check for error,
		 * at least give them consistent junk until they are fixed
		 */
		memset(buf, 0, bufsize);
	}

	kfree(dmabuf);

	return result;
}

/*
 * Reads any 32-bit CP210X_ register identified by req.
 */
static int cp210x_read_u32_reg(struct usb_serial_port *port, u8 req, u32 *val)
{
	__le32 le32_val;
	int err;

	err = cp210x_read_reg_block(port, req, &le32_val, sizeof(le32_val));
	if (err) {
		/*
		 * FIXME Some callers don't bother to check for error,
		 * at least give them consistent junk until they are fixed
		 */
		*val = 0;
		return err;
	}

	*val = le32_to_cpu(le32_val);

	return 0;
}

/*
 * Reads any 16-bit CP210X_ register identified by req.
 */
static int cp210x_read_u16_reg(struct usb_serial_port *port, u8 req, u16 *val)
{
	__le16 le16_val;
	int err;

	err = cp210x_read_reg_block(port, req, &le16_val, sizeof(le16_val));
	if (err)
		return err;

	*val = le16_to_cpu(le16_val);

	return 0;
}

/*
 * Reads any 8-bit CP210X_ register identified by req.
 */
static int cp210x_read_u8_reg(struct usb_serial_port *port, u8 req, u8 *val)
{
	return cp210x_read_reg_block(port, req, val, sizeof(*val));
}

/*
 * Reads a variable-sized vendor block of CP210X_ registers, identified by val.
 * Returns data into buf in native USB byte order.
 */
static int cp210x_read_vendor_block(struct usb_serial *serial, u8 type, u16 val,
				    void *buf, int bufsize)
{
	void *dmabuf;
	int result;

	dmabuf = kmalloc(bufsize, GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
				 CP210X_VENDOR_SPECIFIC, type, val,
				 cp210x_interface_num(serial), dmabuf, bufsize,
				 USB_CTRL_GET_TIMEOUT);
	if (result == bufsize) {
		memcpy(buf, dmabuf, bufsize);
		result = 0;
	} else {
		dev_err(&serial->interface->dev,
			"failed to get vendor val 0x%04x size %d: %d\n", val,
			bufsize, result);
		if (result >= 0)
			result = -EIO;
	}

	kfree(dmabuf);

	return result;
}

/*
 * Writes any 16-bit CP210X_ register (req) whose value is passed
 * entirely in the wValue field of the USB request.
 */
static int cp210x_write_u16_reg(struct usb_serial_port *port, u8 req, u16 val)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	int result;

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			req, REQTYPE_HOST_TO_INTERFACE, val,
			port_priv->bInterfaceNumber, NULL, 0,
			USB_CTRL_SET_TIMEOUT);
	if (result < 0) {
		dev_err(&port->dev, "failed set request 0x%x status: %d\n",
				req, result);
	}

	return result;
}

/*
 * Writes a variable-sized block of CP210X_ registers, identified by req.
 * Data in buf must be in native USB byte order.
 */
static int cp210x_write_reg_block(struct usb_serial_port *port, u8 req,
		void *buf, int bufsize)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	void *dmabuf;
	int result;

	dmabuf = kmemdup(buf, bufsize, GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			req, REQTYPE_HOST_TO_INTERFACE, 0,
			port_priv->bInterfaceNumber, dmabuf, bufsize,
			USB_CTRL_SET_TIMEOUT);

	kfree(dmabuf);

	if (result == bufsize) {
		result = 0;
	} else {
		dev_err(&port->dev, "failed set req 0x%x size %d status: %d\n",
				req, bufsize, result);
		if (result >= 0)
			result = -EIO;
	}

	return result;
}

/*
 * Writes any 32-bit CP210X_ register identified by req.
 */
static int cp210x_write_u32_reg(struct usb_serial_port *port, u8 req, u32 val)
{
	__le32 le32_val;

	le32_val = cpu_to_le32(val);

	return cp210x_write_reg_block(port, req, &le32_val, sizeof(le32_val));
}

#ifdef CONFIG_GPIOLIB
/*
 * Writes a variable-sized vendor block of CP210X_ registers, identified by val.
 * Data in buf must be in native USB byte order.
 */
static int cp210x_write_vendor_block(struct usb_serial *serial, u8 type,
				     u16 val, void *buf, int bufsize)
{
	void *dmabuf;
	int result;

	dmabuf = kmemdup(buf, bufsize, GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				 CP210X_VENDOR_SPECIFIC, type, val,
				 cp210x_interface_num(serial), dmabuf, bufsize,
				 USB_CTRL_SET_TIMEOUT);

	kfree(dmabuf);

	if (result == bufsize) {
		result = 0;
	} else {
		dev_err(&serial->interface->dev,
			"failed to set vendor val 0x%04x size %d: %d\n", val,
			bufsize, result);
		if (result >= 0)
			result = -EIO;
	}

	return result;
}
#endif

/*
 * Detect CP2108 GET_LINE_CTL bug and activate workaround.
 * Write a known good value 0x800, read it back.
 * If it comes back swapped the bug is detected.
 * Preserve the original register value.
 */
static int cp210x_detect_swapped_line_ctl(struct usb_serial_port *port)
{
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	u16 line_ctl_save;
	u16 line_ctl_test;
	int err;

	err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, &line_ctl_save);
	if (err)
		return err;

	err = cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, 0x800);
	if (err)
		return err;

	err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, &line_ctl_test);
	if (err)
		return err;

	if (line_ctl_test == 8) {
		port_priv->has_swapped_line_ctl = true;
		line_ctl_save = swab16(line_ctl_save);
	}

	return cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, line_ctl_save);
}

/*
 * Must always be called instead of cp210x_read_u16_reg(CP210X_GET_LINE_CTL)
 * to workaround cp2108 bug and get correct value.
 */
static int cp210x_get_line_ctl(struct usb_serial_port *port, u16 *ctl)
{
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	int err;

	err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, ctl);
	if (err)
		return err;

	/* Workaround swapped bytes in 16-bit value from CP210X_GET_LINE_CTL */
	if (port_priv->has_swapped_line_ctl)
		*ctl = swab16(*ctl);

	return 0;
}

static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	int result;

	result = cp210x_write_u16_reg(port, CP210X_IFC_ENABLE, UART_ENABLE);
	if (result) {
		dev_err(&port->dev, "%s - Unable to enable UART\n", __func__);
		return result;
	}

	/* Configure the termios structure */
	cp210x_get_termios(tty, port);

	/* The baud rate must be initialised on cp2104 */
	if (tty)
		cp210x_change_speed(tty, port, NULL);

    printk("csust-cekong-cp210-open");

    return usb_serial_generic_open(tty, port);
}

static void cp210x_close(struct usb_serial_port *port)
{
	usb_serial_generic_close(port);

	/* Clear both queues; cp2108 needs this to avoid an occasional hang */
	cp210x_write_u16_reg(port, CP210X_PURGE, PURGE_ALL);

	cp210x_write_u16_reg(port, CP210X_IFC_ENABLE, UART_DISABLE);

    printk("csust-cekong-cp210-close");
}

/*
 * Read how many bytes are waiting in the TX queue.
 */
static int cp210x_get_tx_queue_byte_count(struct usb_serial_port *port,
		u32 *count)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv = usb_get_serial_port_data(port);
	struct cp210x_comm_status *sts;
	int result;

	sts = kmalloc(sizeof(*sts), GFP_KERNEL);
	if (!sts)
		return -ENOMEM;

	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			CP210X_GET_COMM_STATUS, REQTYPE_INTERFACE_TO_HOST,
			0, port_priv->bInterfaceNumber, sts, sizeof(*sts),
			USB_CTRL_GET_TIMEOUT);
	if (result == sizeof(*sts)) {
		*count = le32_to_cpu(sts->ulAmountInOutQueue);
		result = 0;
	} else {
		dev_err(&port->dev, "failed to get comm status: %d\n", result);
		if (result >= 0)
			result = -EIO;
	}

	kfree(sts);

	return result;
}

static bool cp210x_tx_empty(struct usb_serial_port *port)
{
	int err;
	u32 count;

	err = cp210x_get_tx_queue_byte_count(port, &count);
	if (err)
		return true;

	return !count;
}

/*
 * cp210x_get_termios
 * Reads the baud rate, data bits, parity, stop bits and flow control mode
 * from the device, corrects any unsupported values, and configures the
 * termios structure to reflect the state of the device
 */
static void cp210x_get_termios(struct tty_struct *tty,
	struct usb_serial_port *port)
{
	unsigned int baud;

	if (tty) {
		cp210x_get_termios_port(tty->driver_data,
			&tty->termios.c_cflag, &baud);
		tty_encode_baud_rate(tty, baud, baud);
	} else {
		tcflag_t cflag;
		cflag = 0;
		cp210x_get_termios_port(port, &cflag, &baud);
	}
}

/*
 * cp210x_get_termios_port
 * This is the heart of cp210x_get_termios which always uses a &usb_serial_port.
 */
static void cp210x_get_termios_port(struct usb_serial_port *port,
	tcflag_t *cflagp, unsigned int *baudp)
{
	struct device *dev = &port->dev;
	tcflag_t cflag;
	struct cp210x_flow_ctl flow_ctl;
	u32 baud;
	u16 bits;
	u32 ctl_hs;
	u32 flow_repl;

	cp210x_read_u32_reg(port, CP210X_GET_BAUDRATE, &baud);

	dev_dbg(dev, "%s - baud rate = %d\n", __func__, baud);
	*baudp = baud;

	cflag = *cflagp;

	cp210x_get_line_ctl(port, &bits);
	cflag &= ~CSIZE;
	switch (bits & BITS_DATA_MASK) {
	case BITS_DATA_5:
		dev_dbg(dev, "%s - data bits = 5\n", __func__);
		cflag |= CS5;
		break;
	case BITS_DATA_6:
		dev_dbg(dev, "%s - data bits = 6\n", __func__);
		cflag |= CS6;
		break;
	case BITS_DATA_7:
		dev_dbg(dev, "%s - data bits = 7\n", __func__);
		cflag |= CS7;
		break;
	case BITS_DATA_8:
		dev_dbg(dev, "%s - data bits = 8\n", __func__);
		cflag |= CS8;
		break;
	case BITS_DATA_9:
		dev_dbg(dev, "%s - data bits = 9 (not supported, using 8 data bits)\n", __func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
		break;
	default:
		dev_dbg(dev, "%s - Unknown number of data bits, using 8\n", __func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
		break;
	}

	switch (bits & BITS_PARITY_MASK) {
	case BITS_PARITY_NONE:
		dev_dbg(dev, "%s - parity = NONE\n", __func__);
		cflag &= ~PARENB;
		break;
	case BITS_PARITY_ODD:
		dev_dbg(dev, "%s - parity = ODD\n", __func__);
		cflag |= (PARENB|PARODD);
		break;
	case BITS_PARITY_EVEN:
		dev_dbg(dev, "%s - parity = EVEN\n", __func__);
		cflag &= ~PARODD;
		cflag |= PARENB;
		break;
	case BITS_PARITY_MARK:
		dev_dbg(dev, "%s - parity = MARK\n", __func__);
		cflag |= (PARENB|PARODD|CMSPAR);
		break;
	case BITS_PARITY_SPACE:
		dev_dbg(dev, "%s - parity = SPACE\n", __func__);
		cflag &= ~PARODD;
		cflag |= (PARENB|CMSPAR);
		break;
	default:
		dev_dbg(dev, "%s - Unknown parity mode, disabling parity\n", __func__);
		cflag &= ~PARENB;
		bits &= ~BITS_PARITY_MASK;
		cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
		break;
	}

	cflag &= ~CSTOPB;
	switch (bits & BITS_STOP_MASK) {
	case BITS_STOP_1:
		dev_dbg(dev, "%s - stop bits = 1\n", __func__);
		break;
	case BITS_STOP_1_5:
		dev_dbg(dev, "%s - stop bits = 1.5 (not supported, using 1 stop bit)\n", __func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
		break;
	case BITS_STOP_2:
		dev_dbg(dev, "%s - stop bits = 2\n", __func__);
		cflag |= CSTOPB;
		break;
	default:
		dev_dbg(dev, "%s - Unknown number of stop bits, using 1 stop bit\n", __func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
		break;
	}

	cp210x_read_reg_block(port, CP210X_GET_FLOW, &flow_ctl,
			sizeof(flow_ctl));
	ctl_hs = le32_to_cpu(flow_ctl.ulControlHandshake);
	if (ctl_hs & CP210X_SERIAL_CTS_HANDSHAKE) {
		dev_dbg(dev, "%s - flow control = CRTSCTS\n", __func__);
		/*
		 * When the port is closed, the CP210x hardware disables
		 * auto-RTS and RTS is deasserted but it leaves auto-CTS when
		 * in hardware flow control mode. When re-opening the port, if
		 * auto-CTS is enabled on the cp210x, then auto-RTS must be
		 * re-enabled in the driver.
		 */
		flow_repl = le32_to_cpu(flow_ctl.ulFlowReplace);
		flow_repl &= ~CP210X_SERIAL_RTS_MASK;
		flow_repl |= CP210X_SERIAL_RTS_SHIFT(CP210X_SERIAL_RTS_FLOW_CTL);
		flow_ctl.ulFlowReplace = cpu_to_le32(flow_repl);
		cp210x_write_reg_block(port,
				CP210X_SET_FLOW,
				&flow_ctl,
				sizeof(flow_ctl));

		cflag |= CRTSCTS;
	} else {
		dev_dbg(dev, "%s - flow control = NONE\n", __func__);
		cflag &= ~CRTSCTS;
	}

	*cflagp = cflag;
}

struct cp210x_rate {
	speed_t rate;
	speed_t high;
};

static const struct cp210x_rate cp210x_an205_table1[] = {
	{ 300, 300 },
	{ 600, 600 },
	{ 1200, 1200 },
	{ 1800, 1800 },
	{ 2400, 2400 },
	{ 4000, 4000 },
	{ 4800, 4803 },
	{ 7200, 7207 },
	{ 9600, 9612 },
	{ 14400, 14428 },
	{ 16000, 16062 },
	{ 19200, 19250 },
	{ 28800, 28912 },
	{ 38400, 38601 },
	{ 51200, 51558 },
	{ 56000, 56280 },
	{ 57600, 58053 },
	{ 64000, 64111 },
	{ 76800, 77608 },
	{ 115200, 117028 },
	{ 128000, 129347 },
	{ 153600, 156868 },
	{ 230400, 237832 },
	{ 250000, 254234 },
	{ 256000, 273066 },
	{ 460800, 491520 },
	{ 500000, 567138 },
	{ 576000, 670254 },
	{ 921600, UINT_MAX }
};

/*
 * Quantises the baud rate as per AN205 Table 1
 */
static speed_t cp210x_get_an205_rate(speed_t baud)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cp210x_an205_table1); ++i) {
		if (baud <= cp210x_an205_table1[i].high)
			break;
	}

	return cp210x_an205_table1[i].rate;
}

static speed_t cp210x_get_actual_rate(speed_t baud)
{
	unsigned int prescale = 1;
	unsigned int div;

	if (baud <= 365)
		prescale = 4;

	div = DIV_ROUND_CLOSEST(48000000, 2 * prescale * baud);
	baud = 48000000 / (2 * prescale * div);

	return baud;
}

/*
 * CP2101 supports the following baud rates:
 *
 *	300, 600, 1200, 1800, 2400, 4800, 7200, 9600, 14400, 19200, 28800,
 *	38400, 56000, 57600, 115200, 128000, 230400, 460800, 921600
 *
 * CP2102 and CP2103 support the following additional rates:
 *
 *	4000, 16000, 51200, 64000, 76800, 153600, 250000, 256000, 500000,
 *	576000
 *
 * The device will map a requested rate to a supported one, but the result
 * of requests for rates greater than 1053257 is undefined (see AN205).
 *
 * CP2104, CP2105 and CP2110 support most rates up to 2M, 921k and 1M baud,
 * respectively, with an error less than 1%. The actual rates are determined
 * by
 *
 *	div = round(freq / (2 x prescale x request))
 *	actual = freq / (2 x prescale x div)
 *
 * For CP2104 and CP2105 freq is 48Mhz and prescale is 4 for request <= 365bps
 * or 1 otherwise.
 * For CP2110 freq is 24Mhz and prescale is 4 for request <= 300bps or 1
 * otherwise.
 */
static void cp210x_change_speed(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	u32 baud;

	/*
	 * This maps the requested rate to the actual rate, a valid rate on
	 * cp2102 or cp2103, or to an arbitrary rate in [1M, max_speed].
	 *
	 * NOTE: B0 is not implemented.
	 */
	baud = clamp(tty->termios.c_ospeed, priv->min_speed, priv->max_speed);

	if (priv->use_actual_rate)
		baud = cp210x_get_actual_rate(baud);
	else if (baud < 1000000)
		baud = cp210x_get_an205_rate(baud);

	dev_dbg(&port->dev, "%s - setting baud rate to %u\n", __func__, baud);
	if (cp210x_write_u32_reg(port, CP210X_SET_BAUDRATE, baud)) {
		dev_warn(&port->dev, "failed to set baud rate to %u\n", baud);
		if (old_termios)
			baud = old_termios->c_ospeed;
		else
			baud = 9600;
	}

	tty_encode_baud_rate(tty, baud, baud);
}

static void cp210x_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct device *dev = &port->dev;
	unsigned int cflag, old_cflag;
	u16 bits;

	cflag = tty->termios.c_cflag;
	old_cflag = old_termios->c_cflag;

	if (tty->termios.c_ospeed != old_termios->c_ospeed)
		cp210x_change_speed(tty, port, old_termios);

	/* If the number of data bits is to be updated */
	if ((cflag & CSIZE) != (old_cflag & CSIZE)) {
		cp210x_get_line_ctl(port, &bits);
		bits &= ~BITS_DATA_MASK;
		switch (cflag & CSIZE) {
		case CS5:
			bits |= BITS_DATA_5;
			dev_dbg(dev, "%s - data bits = 5\n", __func__);
			break;
		case CS6:
			bits |= BITS_DATA_6;
			dev_dbg(dev, "%s - data bits = 6\n", __func__);
			break;
		case CS7:
			bits |= BITS_DATA_7;
			dev_dbg(dev, "%s - data bits = 7\n", __func__);
			break;
		case CS8:
		default:
			bits |= BITS_DATA_8;
			dev_dbg(dev, "%s - data bits = 8\n", __func__);
			break;
		}
		if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
			dev_dbg(dev, "Number of data bits requested not supported by device\n");
	}

	if ((cflag     & (PARENB|PARODD|CMSPAR)) !=
	    (old_cflag & (PARENB|PARODD|CMSPAR))) {
		cp210x_get_line_ctl(port, &bits);
		bits &= ~BITS_PARITY_MASK;
		if (cflag & PARENB) {
			if (cflag & CMSPAR) {
				if (cflag & PARODD) {
					bits |= BITS_PARITY_MARK;
					dev_dbg(dev, "%s - parity = MARK\n", __func__);
				} else {
					bits |= BITS_PARITY_SPACE;
					dev_dbg(dev, "%s - parity = SPACE\n", __func__);
				}
			} else {
				if (cflag & PARODD) {
					bits |= BITS_PARITY_ODD;
					dev_dbg(dev, "%s - parity = ODD\n", __func__);
				} else {
					bits |= BITS_PARITY_EVEN;
					dev_dbg(dev, "%s - parity = EVEN\n", __func__);
				}
			}
		}
		if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
			dev_dbg(dev, "Parity mode not supported by device\n");
	}

	if ((cflag & CSTOPB) != (old_cflag & CSTOPB)) {
		cp210x_get_line_ctl(port, &bits);
		bits &= ~BITS_STOP_MASK;
		if (cflag & CSTOPB) {
			bits |= BITS_STOP_2;
			dev_dbg(dev, "%s - stop bits = 2\n", __func__);
		} else {
			bits |= BITS_STOP_1;
			dev_dbg(dev, "%s - stop bits = 1\n", __func__);
		}
		if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
			dev_dbg(dev, "Number of stop bits requested not supported by device\n");
	}

	if ((cflag & CRTSCTS) != (old_cflag & CRTSCTS)) {
		struct cp210x_flow_ctl flow_ctl;
		u32 ctl_hs;
		u32 flow_repl;

		cp210x_read_reg_block(port, CP210X_GET_FLOW, &flow_ctl,
				sizeof(flow_ctl));
		ctl_hs = le32_to_cpu(flow_ctl.ulControlHandshake);
		flow_repl = le32_to_cpu(flow_ctl.ulFlowReplace);
		dev_dbg(dev, "%s - read ulControlHandshake=0x%08x, ulFlowReplace=0x%08x\n",
				__func__, ctl_hs, flow_repl);

		ctl_hs &= ~CP210X_SERIAL_DSR_HANDSHAKE;
		ctl_hs &= ~CP210X_SERIAL_DCD_HANDSHAKE;
		ctl_hs &= ~CP210X_SERIAL_DSR_SENSITIVITY;
		ctl_hs &= ~CP210X_SERIAL_DTR_MASK;
		ctl_hs |= CP210X_SERIAL_DTR_SHIFT(CP210X_SERIAL_DTR_ACTIVE);
		if (cflag & CRTSCTS) {
			ctl_hs |= CP210X_SERIAL_CTS_HANDSHAKE;

			flow_repl &= ~CP210X_SERIAL_RTS_MASK;
			flow_repl |= CP210X_SERIAL_RTS_SHIFT(
					CP210X_SERIAL_RTS_FLOW_CTL);
			dev_dbg(dev, "%s - flow control = CRTSCTS\n", __func__);
		} else {
			ctl_hs &= ~CP210X_SERIAL_CTS_HANDSHAKE;

			flow_repl &= ~CP210X_SERIAL_RTS_MASK;
			flow_repl |= CP210X_SERIAL_RTS_SHIFT(
					CP210X_SERIAL_RTS_ACTIVE);
			dev_dbg(dev, "%s - flow control = NONE\n", __func__);
		}

		dev_dbg(dev, "%s - write ulControlHandshake=0x%08x, ulFlowReplace=0x%08x\n",
				__func__, ctl_hs, flow_repl);
		flow_ctl.ulControlHandshake = cpu_to_le32(ctl_hs);
		flow_ctl.ulFlowReplace = cpu_to_le32(flow_repl);
		cp210x_write_reg_block(port, CP210X_SET_FLOW, &flow_ctl,
				sizeof(flow_ctl));
	}

}

static int cp210x_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	return cp210x_tiocmset_port(port, set, clear);
}

static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int set, unsigned int clear)
{
	u16 control = 0;

	if (set & TIOCM_RTS) {
		control |= CONTROL_RTS;
		control |= CONTROL_WRITE_RTS;
	}
	if (set & TIOCM_DTR) {
		control |= CONTROL_DTR;
		control |= CONTROL_WRITE_DTR;
	}
	if (clear & TIOCM_RTS) {
		control &= ~CONTROL_RTS;
		control |= CONTROL_WRITE_RTS;
	}
	if (clear & TIOCM_DTR) {
		control &= ~CONTROL_DTR;
		control |= CONTROL_WRITE_DTR;
	}

	dev_dbg(&port->dev, "%s - control = 0x%.4x\n", __func__, control);

	return cp210x_write_u16_reg(port, CP210X_SET_MHS, control);
}

static void cp210x_dtr_rts(struct usb_serial_port *p, int on)
{
	if (on)
		cp210x_tiocmset_port(p, TIOCM_DTR|TIOCM_RTS, 0);
	else
		cp210x_tiocmset_port(p, 0, TIOCM_DTR|TIOCM_RTS);
}

static int cp210x_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	u8 control;
	int result;

	result = cp210x_read_u8_reg(port, CP210X_GET_MDMSTS, &control);
	if (result)
		return result;

	result = ((control & CONTROL_DTR) ? TIOCM_DTR : 0)
		|((control & CONTROL_RTS) ? TIOCM_RTS : 0)
		|((control & CONTROL_CTS) ? TIOCM_CTS : 0)
		|((control & CONTROL_DSR) ? TIOCM_DSR : 0)
		|((control & CONTROL_RING)? TIOCM_RI  : 0)
		|((control & CONTROL_DCD) ? TIOCM_CD  : 0);

	dev_dbg(&port->dev, "%s - control = 0x%.2x\n", __func__, control);

	return result;
}

static void cp210x_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	u16 state;

	if (break_state == 0)
		state = BREAK_OFF;
	else
		state = BREAK_ON;
	dev_dbg(&port->dev, "%s - turning break %s\n", __func__,
		state == BREAK_OFF ? "off" : "on");
	cp210x_write_u16_reg(port, CP210X_SET_BREAK, state);
}

#ifdef CONFIG_GPIOLIB
static int cp210x_gpio_request(struct gpio_chip *gc, unsigned int offset)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	if (priv->gpio_altfunc & BIT(offset))
		return -ENODEV;

	return 0;
}

static int cp210x_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	u8 req_type = REQTYPE_DEVICE_TO_HOST;
	int result;
	u8 buf;

	if (priv->partnum == CP210X_PARTNUM_CP2105)
		req_type = REQTYPE_INTERFACE_TO_HOST;

	result = usb_autopm_get_interface(serial->interface);
	if (result)
		return result;

	result = cp210x_read_vendor_block(serial, req_type,
					  CP210X_READ_LATCH, &buf, sizeof(buf));
	usb_autopm_put_interface(serial->interface);
	if (result < 0)
		return result;

	return !!(buf & BIT(gpio));
}

static void cp210x_gpio_set(struct gpio_chip *gc, unsigned int gpio, int value)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	struct cp210x_gpio_write buf;
	int result;

	if (value == 1)
		buf.state = BIT(gpio);
	else
		buf.state = 0;

	buf.mask = BIT(gpio);

	result = usb_autopm_get_interface(serial->interface);
	if (result)
		goto out;

	if (priv->partnum == CP210X_PARTNUM_CP2105) {
		result = cp210x_write_vendor_block(serial,
						   REQTYPE_HOST_TO_INTERFACE,
						   CP210X_WRITE_LATCH, &buf,
						   sizeof(buf));
	} else {
		u16 wIndex = buf.state << 8 | buf.mask;

		result = usb_control_msg(serial->dev,
					 usb_sndctrlpipe(serial->dev, 0),
					 CP210X_VENDOR_SPECIFIC,
					 REQTYPE_HOST_TO_DEVICE,
					 CP210X_WRITE_LATCH,
					 wIndex,
					 NULL, 0, USB_CTRL_SET_TIMEOUT);
	}

	usb_autopm_put_interface(serial->interface);
out:
	if (result < 0) {
		dev_err(&serial->interface->dev, "failed to set GPIO value: %d\n",
				result);
	}
}

static int cp210x_gpio_direction_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	return priv->gpio_input & BIT(gpio);
}

static int cp210x_gpio_direction_input(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	if (priv->partnum == CP210X_PARTNUM_CP2105) {
		/* hardware does not support an input mode */
		return -ENOTSUPP;
	}

	/* push-pull pins cannot be changed to be inputs */
	if (priv->gpio_pushpull & BIT(gpio))
		return -EINVAL;

	/* make sure to release pin if it is being driven low */
	cp210x_gpio_set(gc, gpio, 1);

	priv->gpio_input |= BIT(gpio);

	return 0;
}

static int cp210x_gpio_direction_output(struct gpio_chip *gc, unsigned int gpio,
					int value)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	priv->gpio_input &= ~BIT(gpio);
	cp210x_gpio_set(gc, gpio, value);

	return 0;
}

static int cp210x_gpio_set_config(struct gpio_chip *gc, unsigned int gpio,
				  unsigned long config)
{
	struct usb_serial *serial = gpiochip_get_data(gc);
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	enum pin_config_param param = pinconf_to_config_param(config);

	/* Succeed only if in correct mode (this can't be set at runtime) */
	if ((param == PIN_CONFIG_DRIVE_PUSH_PULL) &&
	    (priv->gpio_pushpull & BIT(gpio)))
		return 0;

	if ((param == PIN_CONFIG_DRIVE_OPEN_DRAIN) &&
	    !(priv->gpio_pushpull & BIT(gpio)))
		return 0;

	return -ENOTSUPP;
}

/*
 * This function is for configuring GPIO using shared pins, where other signals
 * are made unavailable by configuring the use of GPIO. This is believed to be
 * only applicable to the cp2105 at this point, the other devices supported by
 * this driver that provide GPIO do so in a way that does not impact other
 * signals and are thus expected to have very different initialisation.
 */
static int cp2105_gpioconf_init(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	struct cp210x_pin_mode mode;
	struct cp210x_dual_port_config config;
	u8 intf_num = cp210x_interface_num(serial);
	u8 iface_config;
	int result;

	result = cp210x_read_vendor_block(serial, REQTYPE_DEVICE_TO_HOST,
					  CP210X_GET_DEVICEMODE, &mode,
					  sizeof(mode));
	if (result < 0)
		return result;

	result = cp210x_read_vendor_block(serial, REQTYPE_DEVICE_TO_HOST,
					  CP210X_GET_PORTCONFIG, &config,
					  sizeof(config));
	if (result < 0)
		return result;

	/*  2 banks of GPIO - One for the pins taken from each serial port */
	if (intf_num == 0) {
		if (mode.eci == CP210X_PIN_MODE_MODEM) {
			/* mark all GPIOs of this interface as reserved */
			priv->gpio_altfunc = 0xff;
			return 0;
		}

		iface_config = config.eci_cfg;
		priv->gpio_pushpull = (u8)((le16_to_cpu(config.gpio_mode) &
						CP210X_ECI_GPIO_MODE_MASK) >>
						CP210X_ECI_GPIO_MODE_OFFSET);
		priv->gc.ngpio = 2;
	} else if (intf_num == 1) {
		if (mode.sci == CP210X_PIN_MODE_MODEM) {
			/* mark all GPIOs of this interface as reserved */
			priv->gpio_altfunc = 0xff;
			return 0;
		}

		iface_config = config.sci_cfg;
		priv->gpio_pushpull = (u8)((le16_to_cpu(config.gpio_mode) &
						CP210X_SCI_GPIO_MODE_MASK) >>
						CP210X_SCI_GPIO_MODE_OFFSET);
		priv->gc.ngpio = 3;
	} else {
		return -ENODEV;
	}

	/* mark all pins which are not in GPIO mode */
	if (iface_config & CP2105_GPIO0_TXLED_MODE)	/* GPIO 0 */
		priv->gpio_altfunc |= BIT(0);
	if (iface_config & (CP2105_GPIO1_RXLED_MODE |	/* GPIO 1 */
			CP2105_GPIO1_RS485_MODE))
		priv->gpio_altfunc |= BIT(1);

	/* driver implementation for CP2105 only supports outputs */
	priv->gpio_input = 0;

	return 0;
}

static int cp2104_gpioconf_init(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	struct cp210x_single_port_config config;
	u8 iface_config;
	u8 gpio_latch;
	int result;
	u8 i;

	result = cp210x_read_vendor_block(serial, REQTYPE_DEVICE_TO_HOST,
					  CP210X_GET_PORTCONFIG, &config,
					  sizeof(config));
	if (result < 0)
		return result;

	priv->gc.ngpio = 4;

	iface_config = config.device_cfg;
	priv->gpio_pushpull = (u8)((le16_to_cpu(config.gpio_mode) &
					CP210X_GPIO_MODE_MASK) >>
					CP210X_GPIO_MODE_OFFSET);
	gpio_latch = (u8)((le16_to_cpu(config.reset_state) &
					CP210X_GPIO_MODE_MASK) >>
					CP210X_GPIO_MODE_OFFSET);

	/* mark all pins which are not in GPIO mode */
	if (iface_config & CP2104_GPIO0_TXLED_MODE)	/* GPIO 0 */
		priv->gpio_altfunc |= BIT(0);
	if (iface_config & CP2104_GPIO1_RXLED_MODE)	/* GPIO 1 */
		priv->gpio_altfunc |= BIT(1);
	if (iface_config & CP2104_GPIO2_RS485_MODE)	/* GPIO 2 */
		priv->gpio_altfunc |= BIT(2);

	/*
	 * Like CP2102N, CP2104 has also no strict input and output pin
	 * modes.
	 * Do the same input mode emulation as CP2102N.
	 */
	for (i = 0; i < priv->gc.ngpio; ++i) {
		/*
		 * Set direction to "input" iff pin is open-drain and reset
		 * value is 1.
		 */
		if (!(priv->gpio_pushpull & BIT(i)) && (gpio_latch & BIT(i)))
			priv->gpio_input |= BIT(i);
	}

	return 0;
}

static int cp2102n_gpioconf_init(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	const u16 config_size = 0x02a6;
	u8 gpio_rst_latch;
	u8 config_version;
	u8 gpio_pushpull;
	u8 *config_buf;
	u8 gpio_latch;
	u8 gpio_ctrl;
	int result;
	u8 i;

	/*
	 * Retrieve device configuration from the device.
	 * The array received contains all customization settings done at the
	 * factory/manufacturer. Format of the array is documented at the
	 * time of writing at:
	 * https://www.silabs.com/community/interface/knowledge-base.entry.html/2017/03/31/cp2102n_setconfig-xsfa
	 */
	config_buf = kmalloc(config_size, GFP_KERNEL);
	if (!config_buf)
		return -ENOMEM;

	result = cp210x_read_vendor_block(serial,
					  REQTYPE_DEVICE_TO_HOST,
					  CP210X_READ_2NCONFIG,
					  config_buf,
					  config_size);
	if (result < 0) {
		kfree(config_buf);
		return result;
	}

	config_version = config_buf[CP210X_2NCONFIG_CONFIG_VERSION_IDX];
	gpio_pushpull = config_buf[CP210X_2NCONFIG_GPIO_MODE_IDX];
	gpio_ctrl = config_buf[CP210X_2NCONFIG_GPIO_CONTROL_IDX];
	gpio_rst_latch = config_buf[CP210X_2NCONFIG_GPIO_RSTLATCH_IDX];

	kfree(config_buf);

	/* Make sure this is a config format we understand. */
	if (config_version != 0x01)
		return -ENOTSUPP;

	priv->gc.ngpio = 4;

	/*
	 * Get default pin states after reset. Needed so we can determine
	 * the direction of an open-drain pin.
	 */
	gpio_latch = (gpio_rst_latch >> 3) & 0x0f;

	/* 0 indicates open-drain mode, 1 is push-pull */
	priv->gpio_pushpull = (gpio_pushpull >> 3) & 0x0f;

	/* 0 indicates GPIO mode, 1 is alternate function */
	priv->gpio_altfunc = (gpio_ctrl >> 2) & 0x0f;

	if (priv->partnum == CP210X_PARTNUM_CP2102N_QFN28) {
		/*
		 * For the QFN28 package, GPIO4-6 are controlled by
		 * the low three bits of the mode/latch fields.
		 * Contrary to the document linked above, the bits for
		 * the SUSPEND pins are elsewhere.  No alternate
		 * function is available for these pins.
		 */
		priv->gc.ngpio = 7;
		gpio_latch |= (gpio_rst_latch & 7) << 4;
		priv->gpio_pushpull |= (gpio_pushpull & 7) << 4;
	}

	/*
	 * The CP2102N does not strictly has input and output pin modes,
	 * it only knows open-drain and push-pull modes which is set at
	 * factory. An open-drain pin can function both as an
	 * input or an output. We emulate input mode for open-drain pins
	 * by making sure they are not driven low, and we do not allow
	 * push-pull pins to be set as an input.
	 */
	for (i = 0; i < priv->gc.ngpio; ++i) {
		/*
		 * Set direction to "input" iff pin is open-drain and reset
		 * value is 1.
		 */
		if (!(priv->gpio_pushpull & BIT(i)) && (gpio_latch & BIT(i)))
			priv->gpio_input |= BIT(i);
	}

	return 0;
}

static int cp210x_gpio_init(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	int result;

	switch (priv->partnum) {
	case CP210X_PARTNUM_CP2104:
		result = cp2104_gpioconf_init(serial);
		break;
	case CP210X_PARTNUM_CP2105:
		result = cp2105_gpioconf_init(serial);
		break;
	case CP210X_PARTNUM_CP2102N_QFN28:
	case CP210X_PARTNUM_CP2102N_QFN24:
	case CP210X_PARTNUM_CP2102N_QFN20:
		result = cp2102n_gpioconf_init(serial);
		break;
	default:
		return 0;
	}

	if (result < 0)
		return result;

	priv->gc.label = "cp210x";
	priv->gc.request = cp210x_gpio_request;
	priv->gc.get_direction = cp210x_gpio_direction_get;
	priv->gc.direction_input = cp210x_gpio_direction_input;
	priv->gc.direction_output = cp210x_gpio_direction_output;
	priv->gc.get = cp210x_gpio_get;
	priv->gc.set = cp210x_gpio_set;
	priv->gc.set_config = cp210x_gpio_set_config;
	priv->gc.owner = THIS_MODULE;
	priv->gc.parent = &serial->interface->dev;
	priv->gc.base = -1;
	priv->gc.can_sleep = true;

	result = gpiochip_add_data(&priv->gc, serial);
	if (!result)
		priv->gpio_registered = true;

	return result;
}

static void cp210x_gpio_remove(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	if (priv->gpio_registered) {
		gpiochip_remove(&priv->gc);
		priv->gpio_registered = false;
	}
}

#else

static int cp210x_gpio_init(struct usb_serial *serial)
{
	return 0;
}

static void cp210x_gpio_remove(struct usb_serial *serial)
{
	/* Nothing to do */
}

#endif

static int cp210x_port_probe(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct cp210x_port_private *port_priv;
	int ret;

    printk("csust-cekong-cp210-probe");
    port_priv = kzalloc(sizeof(*port_priv), GFP_KERNEL);
	if (!port_priv)
		return -ENOMEM;

	port_priv->bInterfaceNumber = cp210x_interface_num(serial);

	usb_set_serial_port_data(port, port_priv);

	ret = cp210x_detect_swapped_line_ctl(port);
	if (ret) {
		kfree(port_priv);
		return ret;
	}
   
    return 0;
}

static int cp210x_port_remove(struct usb_serial_port *port)
{
	struct cp210x_port_private *port_priv;

	port_priv = usb_get_serial_port_data(port);
	kfree(port_priv);
    printk("csust-cekong-cp210-remove");
    return 0;
}

static void cp210x_init_max_speed(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);
	bool use_actual_rate = false;
	speed_t min = 300;
	speed_t max;

	switch (priv->partnum) {
	case CP210X_PARTNUM_CP2101:
		max = 921600;
		break;
	case CP210X_PARTNUM_CP2102:
	case CP210X_PARTNUM_CP2103:
		max = 1000000;
		break;
	case CP210X_PARTNUM_CP2104:
		use_actual_rate = true;
		max = 2000000;
		break;
	case CP210X_PARTNUM_CP2108:
		max = 2000000;
		break;
	case CP210X_PARTNUM_CP2105:
		if (cp210x_interface_num(serial) == 0) {
			use_actual_rate = true;
			max = 2000000;	/* ECI */
		} else {
			min = 2400;
			max = 921600;	/* SCI */
		}
		break;
	case CP210X_PARTNUM_CP2102N_QFN28:
	case CP210X_PARTNUM_CP2102N_QFN24:
	case CP210X_PARTNUM_CP2102N_QFN20:
		use_actual_rate = true;
		max = 3000000;
		break;
	default:
		max = 2000000;
		break;
	}

	priv->min_speed = min;
	priv->max_speed = max;
	priv->use_actual_rate = use_actual_rate;
}

static int cp210x_attach(struct usb_serial *serial)
{
	int result;
	struct cp210x_serial_private *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	result = cp210x_read_vendor_block(serial, REQTYPE_DEVICE_TO_HOST,
					  CP210X_GET_PARTNUM, &priv->partnum,
					  sizeof(priv->partnum));
	if (result < 0) {
		dev_warn(&serial->interface->dev,
			 "querying part number failed\n");
		priv->partnum = CP210X_PARTNUM_UNKNOWN;
	}

	usb_set_serial_data(serial, priv);

	cp210x_init_max_speed(serial);

	result = cp210x_gpio_init(serial);
	if (result < 0) {
		dev_err(&serial->interface->dev, "GPIO initialisation failed: %d\n",
				result);
	}

	return 0;
}

static void cp210x_disconnect(struct usb_serial *serial)
{
	cp210x_gpio_remove(serial);
}

static void cp210x_release(struct usb_serial *serial)
{
	struct cp210x_serial_private *priv = usb_get_serial_data(serial);

	cp210x_gpio_remove(serial);

	kfree(priv);
}

module_usb_serial_driver(serial_drivers, id_table);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
