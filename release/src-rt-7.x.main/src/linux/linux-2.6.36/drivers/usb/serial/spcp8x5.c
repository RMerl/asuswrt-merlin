/*
 * spcp8x5 USB to serial adaptor driver
 *
 * Copyright (C) 2010 Johan Hovold (jhovold@gmail.com)
 * Copyright (C) 2006 Linxb (xubin.lin@worldplus.com.cn)
 * Copyright (C) 2006 S1 Corp.
 *
 * Original driver for 2.6.10 pl2303 driver by
 *   Greg Kroah-Hartman (greg@kroah.com)
 * Changes for 2.6.20 by Harald Klein <hari@vt100.at>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>


/* Version Information */
#define DRIVER_VERSION	"v0.10"
#define DRIVER_DESC 	"SPCP8x5 USB to serial adaptor driver"

static int debug;

#define SPCP8x5_007_VID		0x04FC
#define SPCP8x5_007_PID		0x0201
#define SPCP8x5_008_VID		0x04fc
#define SPCP8x5_008_PID		0x0235
#define SPCP8x5_PHILIPS_VID	0x0471
#define SPCP8x5_PHILIPS_PID	0x081e
#define SPCP8x5_INTERMATIC_VID	0x04FC
#define SPCP8x5_INTERMATIC_PID	0x0204
#define SPCP8x5_835_VID		0x04fc
#define SPCP8x5_835_PID		0x0231

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(SPCP8x5_PHILIPS_VID , SPCP8x5_PHILIPS_PID)},
	{ USB_DEVICE(SPCP8x5_INTERMATIC_VID, SPCP8x5_INTERMATIC_PID)},
	{ USB_DEVICE(SPCP8x5_835_VID, SPCP8x5_835_PID)},
	{ USB_DEVICE(SPCP8x5_008_VID, SPCP8x5_008_PID)},
	{ USB_DEVICE(SPCP8x5_007_VID, SPCP8x5_007_PID)},
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, id_table);

struct spcp8x5_usb_ctrl_arg {
	u8 	type;
	u8	cmd;
	u8	cmd_type;
	u16	value;
	u16	index;
	u16	length;
};


/* spcp8x5 spec register define */
#define MCR_CONTROL_LINE_RTS		0x02
#define MCR_CONTROL_LINE_DTR		0x01
#define MCR_DTR				0x01
#define MCR_RTS				0x02

#define MSR_STATUS_LINE_DCD		0x80
#define MSR_STATUS_LINE_RI		0x40
#define MSR_STATUS_LINE_DSR		0x20
#define MSR_STATUS_LINE_CTS		0x10

/* verdor command here , we should define myself */
#define SET_DEFAULT			0x40
#define SET_DEFAULT_TYPE		0x20

#define SET_UART_FORMAT			0x40
#define SET_UART_FORMAT_TYPE		0x21
#define SET_UART_FORMAT_SIZE_5		0x00
#define SET_UART_FORMAT_SIZE_6		0x01
#define SET_UART_FORMAT_SIZE_7		0x02
#define SET_UART_FORMAT_SIZE_8		0x03
#define SET_UART_FORMAT_STOP_1		0x00
#define SET_UART_FORMAT_STOP_2		0x04
#define SET_UART_FORMAT_PAR_NONE	0x00
#define SET_UART_FORMAT_PAR_ODD		0x10
#define SET_UART_FORMAT_PAR_EVEN	0x30
#define SET_UART_FORMAT_PAR_MASK	0xD0
#define SET_UART_FORMAT_PAR_SPACE	0x90

#define GET_UART_STATUS_TYPE		0xc0
#define GET_UART_STATUS			0x22
#define GET_UART_STATUS_MSR		0x06

#define SET_UART_STATUS			0x40
#define SET_UART_STATUS_TYPE		0x23
#define SET_UART_STATUS_MCR		0x0004
#define SET_UART_STATUS_MCR_DTR		0x01
#define SET_UART_STATUS_MCR_RTS		0x02
#define SET_UART_STATUS_MCR_LOOP	0x10

#define SET_WORKING_MODE		0x40
#define SET_WORKING_MODE_TYPE		0x24
#define SET_WORKING_MODE_U2C		0x00
#define SET_WORKING_MODE_RS485		0x01
#define SET_WORKING_MODE_PDMA		0x02
#define SET_WORKING_MODE_SPP		0x03

#define SET_FLOWCTL_CHAR		0x40
#define SET_FLOWCTL_CHAR_TYPE		0x25

#define GET_VERSION			0xc0
#define GET_VERSION_TYPE		0x26

#define SET_REGISTER			0x40
#define SET_REGISTER_TYPE		0x27

#define	GET_REGISTER			0xc0
#define GET_REGISTER_TYPE		0x28

#define SET_RAM				0x40
#define SET_RAM_TYPE			0x31

#define GET_RAM				0xc0
#define GET_RAM_TYPE			0x32

/* how come ??? */
#define UART_STATE			0x08
#define UART_STATE_TRANSIENT_MASK	0x75
#define UART_DCD			0x01
#define UART_DSR			0x02
#define UART_BREAK_ERROR		0x04
#define UART_RING			0x08
#define UART_FRAME_ERROR		0x10
#define UART_PARITY_ERROR		0x20
#define UART_OVERRUN_ERROR		0x40
#define UART_CTS			0x80

enum spcp8x5_type {
	SPCP825_007_TYPE,
	SPCP825_008_TYPE,
	SPCP825_PHILIP_TYPE,
	SPCP825_INTERMATIC_TYPE,
	SPCP835_TYPE,
};

static struct usb_driver spcp8x5_driver = {
	.name =			"spcp8x5",
	.probe =		usb_serial_probe,
	.disconnect =		usb_serial_disconnect,
	.id_table =		id_table,
	.no_dynamic_id =	1,
};


struct spcp8x5_private {
	spinlock_t 	lock;
	enum spcp8x5_type	type;
	wait_queue_head_t	delta_msr_wait;
	u8 			line_control;
	u8 			line_status;
};

/* desc : when device plug in,this function would be called.
 * thanks to usb_serial subsystem,then do almost every things for us. And what
 * we should do just alloc the buffer */
static int spcp8x5_startup(struct usb_serial *serial)
{
	struct spcp8x5_private *priv;
	int i;
	enum spcp8x5_type type = SPCP825_007_TYPE;
	u16 product = le16_to_cpu(serial->dev->descriptor.idProduct);

	if (product == 0x0201)
		type = SPCP825_007_TYPE;
	else if (product == 0x0231)
		type = SPCP835_TYPE;
	else if (product == 0x0235)
		type = SPCP825_008_TYPE;
	else if (product == 0x0204)
		type = SPCP825_INTERMATIC_TYPE;
	else if (product == 0x0471 &&
		 serial->dev->descriptor.idVendor == cpu_to_le16(0x081e))
		type = SPCP825_PHILIP_TYPE;
	dev_dbg(&serial->dev->dev, "device type = %d\n", (int)type);

	for (i = 0; i < serial->num_ports; ++i) {
		priv = kzalloc(sizeof(struct spcp8x5_private), GFP_KERNEL);
		if (!priv)
			goto cleanup;

		spin_lock_init(&priv->lock);
		init_waitqueue_head(&priv->delta_msr_wait);
		priv->type = type;
		usb_set_serial_port_data(serial->port[i] , priv);
	}

	return 0;
cleanup:
	for (--i; i >= 0; --i) {
		priv = usb_get_serial_port_data(serial->port[i]);
		kfree(priv);
		usb_set_serial_port_data(serial->port[i] , NULL);
	}
	return -ENOMEM;
}

/* call when the device plug out. free all the memory alloced by probe */
static void spcp8x5_release(struct usb_serial *serial)
{
	int i;

	for (i = 0; i < serial->num_ports; i++)
		kfree(usb_get_serial_port_data(serial->port[i]));
}

/* set the modem control line of the device.
 * NOTE spcp825-007 not supported this */
static int spcp8x5_set_ctrlLine(struct usb_device *dev, u8 value,
				enum spcp8x5_type type)
{
	int retval;
	u8 mcr = 0 ;

	if (type == SPCP825_007_TYPE)
		return -EPERM;

	mcr = (unsigned short)value;
	retval = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				 SET_UART_STATUS_TYPE, SET_UART_STATUS,
				 mcr, 0x04, NULL, 0, 100);
	if (retval != 0)
		dev_dbg(&dev->dev, "usb_control_msg return %#x\n", retval);
	return retval;
}

/* get the modem status register of the device
 * NOTE spcp825-007 not supported this */
static int spcp8x5_get_msr(struct usb_device *dev, u8 *status,
			   enum spcp8x5_type type)
{
	u8 *status_buffer;
	int ret;

	/* I return Permited not support here but seem inval device
	 * is more fix */
	if (type == SPCP825_007_TYPE)
		return -EPERM;
	if (status == NULL)
		return -EINVAL;

	status_buffer = kmalloc(1, GFP_KERNEL);
	if (!status_buffer)
		return -ENOMEM;
	status_buffer[0] = status[0];

	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      GET_UART_STATUS, GET_UART_STATUS_TYPE,
			      0, GET_UART_STATUS_MSR, status_buffer, 1, 100);
	if (ret < 0)
		dev_dbg(&dev->dev, "Get MSR = 0x%p failed (error = %d)",
			status_buffer, ret);

	dev_dbg(&dev->dev, "0xc0:0x22:0:6  %d - 0x%p ", ret, status_buffer);
	status[0] = status_buffer[0];
	kfree(status_buffer);

	return ret;
}

/* select the work mode.
 * NOTE this function not supported by spcp825-007 */
static void spcp8x5_set_workMode(struct usb_device *dev, u16 value,
				 u16 index, enum spcp8x5_type type)
{
	int ret;

	/* I return Permited not support here but seem inval device
	 * is more fix */
	if (type == SPCP825_007_TYPE)
		return;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			      SET_WORKING_MODE_TYPE, SET_WORKING_MODE,
			      value, index, NULL, 0, 100);
	dev_dbg(&dev->dev, "value = %#x , index = %#x\n", value, index);
	if (ret < 0)
		dev_dbg(&dev->dev,
			"RTSCTS usb_control_msg(enable flowctrl) = %d\n", ret);
}

static int spcp8x5_carrier_raised(struct usb_serial_port *port)
{
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	if (priv->line_status & MSR_STATUS_LINE_DCD)
		return 1;
	return 0;
}

static void spcp8x5_dtr_rts(struct usb_serial_port *port, int on)
{
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	u8 control;

	spin_lock_irqsave(&priv->lock, flags);
	if (on)
		priv->line_control = MCR_CONTROL_LINE_DTR
						| MCR_CONTROL_LINE_RTS;
	else
		priv->line_control &= ~ (MCR_CONTROL_LINE_DTR
						| MCR_CONTROL_LINE_RTS);
	control = priv->line_control;
	spin_unlock_irqrestore(&priv->lock, flags);
	spcp8x5_set_ctrlLine(port->serial->dev, control , priv->type);
}

static void spcp8x5_init_termios(struct tty_struct *tty)
{
	/* for the 1st time call this function */
	*(tty->termios) = tty_std_termios;
	tty->termios->c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	tty->termios->c_ispeed = 115200;
	tty->termios->c_ospeed = 115200;
}

/* set the serial param for transfer. we should check if we really need to
 * transfer. if we set flow control we should do this too. */
static void spcp8x5_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;
	unsigned int old_cflag = old_termios->c_cflag;
	unsigned short uartdata;
	unsigned char buf[2] = {0, 0};
	int baud;
	int i;
	u8 control;


	/* check that they really want us to change something */
	if (!tty_termios_hw_change(tty->termios, old_termios))
		return;

	/* set DTR/RTS active */
	spin_lock_irqsave(&priv->lock, flags);
	control = priv->line_control;
	if ((old_cflag & CBAUD) == B0) {
		priv->line_control |= MCR_DTR;
		if (!(old_cflag & CRTSCTS))
			priv->line_control |= MCR_RTS;
	}
	if (control != priv->line_control) {
		control = priv->line_control;
		spin_unlock_irqrestore(&priv->lock, flags);
		spcp8x5_set_ctrlLine(serial->dev, control , priv->type);
	} else {
		spin_unlock_irqrestore(&priv->lock, flags);
	}

	/* Set Baud Rate */
	baud = tty_get_baud_rate(tty);
	switch (baud) {
	case 300:	buf[0] = 0x00;	break;
	case 600:	buf[0] = 0x01;	break;
	case 1200:	buf[0] = 0x02;	break;
	case 2400:	buf[0] = 0x03;	break;
	case 4800:	buf[0] = 0x04;	break;
	case 9600:	buf[0] = 0x05;	break;
	case 19200:	buf[0] = 0x07;	break;
	case 38400:	buf[0] = 0x09;	break;
	case 57600:	buf[0] = 0x0a;	break;
	case 115200:	buf[0] = 0x0b;	break;
	case 230400:	buf[0] = 0x0c;	break;
	case 460800:	buf[0] = 0x0d;	break;
	case 921600:	buf[0] = 0x0e;	break;
/*	case 1200000:	buf[0] = 0x0f;	break; */
/*	case 2400000:	buf[0] = 0x10;	break; */
	case 3000000:	buf[0] = 0x11;	break;
/*	case 6000000:	buf[0] = 0x12;	break; */
	case 0:
	case 1000000:
			buf[0] = 0x0b;	break;
	default:
		dev_err(&port->dev, "spcp825 driver does not support the "
			"baudrate requested, using default of 9600.\n");
	}

	/* Set Data Length : 00:5bit, 01:6bit, 10:7bit, 11:8bit */
	if (cflag & CSIZE) {
		switch (cflag & CSIZE) {
		case CS5:
			buf[1] |= SET_UART_FORMAT_SIZE_5;
			break;
		case CS6:
			buf[1] |= SET_UART_FORMAT_SIZE_6;
			break;
		case CS7:
			buf[1] |= SET_UART_FORMAT_SIZE_7;
			break;
		default:
		case CS8:
			buf[1] |= SET_UART_FORMAT_SIZE_8;
			break;
		}
	}

	/* Set Stop bit2 : 0:1bit 1:2bit */
	buf[1] |= (cflag & CSTOPB) ? SET_UART_FORMAT_STOP_2 :
				     SET_UART_FORMAT_STOP_1;

	/* Set Parity bit3-4 01:Odd 11:Even */
	if (cflag & PARENB) {
		buf[1] |= (cflag & PARODD) ?
		SET_UART_FORMAT_PAR_ODD : SET_UART_FORMAT_PAR_EVEN ;
	} else
		buf[1] |= SET_UART_FORMAT_PAR_NONE;

	uartdata = buf[0] | buf[1]<<8;

	i = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			    SET_UART_FORMAT_TYPE, SET_UART_FORMAT,
			    uartdata, 0, NULL, 0, 100);
	if (i < 0)
		dev_err(&port->dev, "Set UART format %#x failed (error = %d)\n",
			uartdata, i);
	dbg("0x21:0x40:0:0  %d", i);

	if (cflag & CRTSCTS) {
		/* enable hardware flow control */
		spcp8x5_set_workMode(serial->dev, 0x000a,
				     SET_WORKING_MODE_U2C, priv->type);
	}
	return;
}

/* open the serial port. do some usb system call. set termios and get the line
 * status of the device. */
static int spcp8x5_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct ktermios tmp_termios;
	struct usb_serial *serial = port->serial;
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	int ret;
	unsigned long flags;
	u8 status = 0x30;
	/* status 0x30 means DSR and CTS = 1 other CDC RI and delta = 0 */

	dbg("%s -  port %d", __func__, port->number);

	usb_clear_halt(serial->dev, port->write_urb->pipe);
	usb_clear_halt(serial->dev, port->read_urb->pipe);

	ret = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			      0x09, 0x00,
			      0x01, 0x00, NULL, 0x00, 100);
	if (ret)
		return ret;

	spcp8x5_set_ctrlLine(serial->dev, priv->line_control , priv->type);

	/* Setup termios */
	if (tty)
		spcp8x5_set_termios(tty, port, &tmp_termios);

	spcp8x5_get_msr(serial->dev, &status, priv->type);

	/* may be we should update uart status here but now we did not do */
	spin_lock_irqsave(&priv->lock, flags);
	priv->line_status = status & 0xf0 ;
	spin_unlock_irqrestore(&priv->lock, flags);

	port->port.drain_delay = 256;

	return usb_serial_generic_open(tty, port);
}

static void spcp8x5_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	unsigned long flags;
	u8 status;
	char tty_flag;

	/* get tty_flag from status */
	tty_flag = TTY_NORMAL;

	spin_lock_irqsave(&priv->lock, flags);
	status = priv->line_status;
	priv->line_status &= ~UART_STATE_TRANSIENT_MASK;
	spin_unlock_irqrestore(&priv->lock, flags);
	/* wake up the wait for termios */
	wake_up_interruptible(&priv->delta_msr_wait);

	if (!urb->actual_length)
		return;

	tty = tty_port_tty_get(&port->port);
	if (!tty)
		return;

	if (status & UART_STATE_TRANSIENT_MASK) {
		/* break takes precedence over parity, which takes precedence
		 * over framing errors */
		if (status & UART_BREAK_ERROR)
			tty_flag = TTY_BREAK;
		else if (status & UART_PARITY_ERROR)
			tty_flag = TTY_PARITY;
		else if (status & UART_FRAME_ERROR)
			tty_flag = TTY_FRAME;
		dev_dbg(&port->dev, "tty_flag = %d\n", tty_flag);

		/* overrun is special, not associated with a char */
		if (status & UART_OVERRUN_ERROR)
			tty_insert_flip_char(tty, 0, TTY_OVERRUN);

		if (status & UART_DCD)
			usb_serial_handle_dcd_change(port, tty,
				   priv->line_status & MSR_STATUS_LINE_DCD);
	}

	tty_insert_flip_string_fixed_flag(tty, data, tty_flag,
							urb->actual_length);
	tty_flip_buffer_push(tty);
	tty_kref_put(tty);
}

static int spcp8x5_wait_modem_info(struct usb_serial_port *port,
				   unsigned int arg)
{
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int prevstatus;
	unsigned int status;
	unsigned int changed;

	spin_lock_irqsave(&priv->lock, flags);
	prevstatus = priv->line_status;
	spin_unlock_irqrestore(&priv->lock, flags);

	while (1) {
		/* wake up in bulk read */
		interruptible_sleep_on(&priv->delta_msr_wait);

		/* see if a signal did it */
		if (signal_pending(current))
			return -ERESTARTSYS;

		spin_lock_irqsave(&priv->lock, flags);
		status = priv->line_status;
		spin_unlock_irqrestore(&priv->lock, flags);

		changed = prevstatus^status;

		if (((arg & TIOCM_RNG) && (changed & MSR_STATUS_LINE_RI)) ||
		    ((arg & TIOCM_DSR) && (changed & MSR_STATUS_LINE_DSR)) ||
		    ((arg & TIOCM_CD)  && (changed & MSR_STATUS_LINE_DCD)) ||
		    ((arg & TIOCM_CTS) && (changed & MSR_STATUS_LINE_CTS)))
			return 0;

		prevstatus = status;
	}
	/* NOTREACHED */
	return 0;
}

static int spcp8x5_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port = tty->driver_data;
	dbg("%s (%d) cmd = 0x%04x", __func__, port->number, cmd);

	switch (cmd) {
	case TIOCMIWAIT:
		dbg("%s (%d) TIOCMIWAIT", __func__,  port->number);
		return spcp8x5_wait_modem_info(port, arg);

	default:
		dbg("%s not supported = 0x%04x", __func__, cmd);
		break;
	}

	return -ENOIOCTLCMD;
}

static int spcp8x5_tiocmset(struct tty_struct *tty, struct file *file,
			    unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	u8 control;

	spin_lock_irqsave(&priv->lock, flags);
	if (set & TIOCM_RTS)
		priv->line_control |= MCR_RTS;
	if (set & TIOCM_DTR)
		priv->line_control |= MCR_DTR;
	if (clear & TIOCM_RTS)
		priv->line_control &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		priv->line_control &= ~MCR_DTR;
	control = priv->line_control;
	spin_unlock_irqrestore(&priv->lock, flags);

	return spcp8x5_set_ctrlLine(port->serial->dev, control , priv->type);
}

static int spcp8x5_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct usb_serial_port *port = tty->driver_data;
	struct spcp8x5_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int mcr;
	unsigned int status;
	unsigned int result;

	spin_lock_irqsave(&priv->lock, flags);
	mcr = priv->line_control;
	status = priv->line_status;
	spin_unlock_irqrestore(&priv->lock, flags);

	result = ((mcr & MCR_DTR)			? TIOCM_DTR : 0)
		  | ((mcr & MCR_RTS)			? TIOCM_RTS : 0)
		  | ((status & MSR_STATUS_LINE_CTS)	? TIOCM_CTS : 0)
		  | ((status & MSR_STATUS_LINE_DSR)	? TIOCM_DSR : 0)
		  | ((status & MSR_STATUS_LINE_RI)	? TIOCM_RI  : 0)
		  | ((status & MSR_STATUS_LINE_DCD)	? TIOCM_CD  : 0);

	return result;
}

/* All of the device info needed for the spcp8x5 SIO serial converter */
static struct usb_serial_driver spcp8x5_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"SPCP8x5",
	},
	.id_table		= id_table,
	.usb_driver		= &spcp8x5_driver,
	.num_ports		= 1,
	.open 			= spcp8x5_open,
	.dtr_rts		= spcp8x5_dtr_rts,
	.carrier_raised		= spcp8x5_carrier_raised,
	.set_termios 		= spcp8x5_set_termios,
	.init_termios		= spcp8x5_init_termios,
	.ioctl 			= spcp8x5_ioctl,
	.tiocmget 		= spcp8x5_tiocmget,
	.tiocmset 		= spcp8x5_tiocmset,
	.attach 		= spcp8x5_startup,
	.release 		= spcp8x5_release,
	.process_read_urb	= spcp8x5_process_read_urb,
};

static int __init spcp8x5_init(void)
{
	int retval;
	retval = usb_serial_register(&spcp8x5_device);
	if (retval)
		goto failed_usb_serial_register;
	retval = usb_register(&spcp8x5_driver);
	if (retval)
		goto failed_usb_register;
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");
	return 0;
failed_usb_register:
	usb_serial_deregister(&spcp8x5_device);
failed_usb_serial_register:
	return retval;
}

static void __exit spcp8x5_exit(void)
{
	usb_deregister(&spcp8x5_driver);
	usb_serial_deregister(&spcp8x5_device);
}

module_init(spcp8x5_init);
module_exit(spcp8x5_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");
