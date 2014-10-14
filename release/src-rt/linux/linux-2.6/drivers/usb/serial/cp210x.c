/*
 * Silicon Laboratories CP210x USB to RS232 serial adaptor driver
 *
 * Copyright (C) 2005 Craig Shelley (craig@microtron.org.uk)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
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

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.09"
#define DRIVER_DESC "Silicon Labs CP210x RS232 serial adaptor driver"

/*
 * Function Prototypes
 */
static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *);
static void cp210x_close(struct usb_serial_port *);
static void cp210x_get_termios(struct tty_struct *,
	struct usb_serial_port *port);
static void cp210x_get_termios_port(struct usb_serial_port *port,
	unsigned int *cflagp, unsigned int *baudp);
static void cp210x_set_termios(struct tty_struct *, struct usb_serial_port *,
							struct ktermios*);
static int cp210x_tiocmget(struct tty_struct *);
static int cp210x_tiocmset(struct tty_struct *, unsigned int, unsigned int);
static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int, unsigned int);
static void cp210x_break_ctl(struct tty_struct *, int);
static int cp210x_startup(struct usb_serial *);
static void cp210x_dtr_rts(struct usb_serial_port *p, int on);

static int debug;

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x045B, 0x0053) }, /* Renesas RX610 RX-Stick */
	{ USB_DEVICE(0x0471, 0x066A) }, /* AKTAKOM ACE-1001 cable */
	{ USB_DEVICE(0x0489, 0xE000) }, /* Pirelli Broadband S.p.A, DP-L10 SIP/GSM Mobile */
	{ USB_DEVICE(0x0489, 0xE003) }, /* Pirelli Broadband S.p.A, DP-L10 SIP/GSM Mobile */
	{ USB_DEVICE(0x0745, 0x1000) }, /* CipherLab USB CCD Barcode Scanner 1000 */
	{ USB_DEVICE(0x08e6, 0x5501) }, /* Gemalto Prox-PU/CU contactless smartcard reader */
	{ USB_DEVICE(0x08FD, 0x000A) }, /* Digianswer A/S , ZigBee/802.15.4 MAC Device */
	{ USB_DEVICE(0x0BED, 0x1100) }, /* MEI (TM) Cashflow-SC Bill/Voucher Acceptor */
	{ USB_DEVICE(0x0BED, 0x1101) }, /* MEI series 2000 Combo Acceptor */
	{ USB_DEVICE(0x0FCF, 0x1003) }, /* Dynastream ANT development board */
	{ USB_DEVICE(0x0FCF, 0x1004) }, /* Dynastream ANT2USB */
	{ USB_DEVICE(0x0FCF, 0x1006) }, /* Dynastream ANT development board */
	{ USB_DEVICE(0x10A6, 0xAA26) }, /* Knock-off DCU-11 cable */
	{ USB_DEVICE(0x10AB, 0x10C5) }, /* Siemens MC60 Cable */
	{ USB_DEVICE(0x10B5, 0xAC70) }, /* Nokia CA-42 USB */
	{ USB_DEVICE(0x10C4, 0x0F91) }, /* Vstabi */
	{ USB_DEVICE(0x10C4, 0x1101) }, /* Arkham Technology DS101 Bus Monitor */
	{ USB_DEVICE(0x10C4, 0x1601) }, /* Arkham Technology DS101 Adapter */
	{ USB_DEVICE(0x10C4, 0x800A) }, /* SPORTident BSM7-D-USB main station */
	{ USB_DEVICE(0x10C4, 0x803B) }, /* Pololu USB-serial converter */
	{ USB_DEVICE(0x10C4, 0x8044) }, /* Cygnal Debug Adapter */
	{ USB_DEVICE(0x10C4, 0x804E) }, /* Software Bisque Paramount ME build-in converter */
	{ USB_DEVICE(0x10C4, 0x8053) }, /* Enfora EDG1228 */
	{ USB_DEVICE(0x10C4, 0x8054) }, /* Enfora GSM2228 */
	{ USB_DEVICE(0x10C4, 0x8066) }, /* Argussoft In-System Programmer */
	{ USB_DEVICE(0x10C4, 0x806F) }, /* IMS USB to RS422 Converter Cable */
	{ USB_DEVICE(0x10C4, 0x807A) }, /* Crumb128 board */
	{ USB_DEVICE(0x10C4, 0x80CA) }, /* Degree Controls Inc */
	{ USB_DEVICE(0x10C4, 0x80DD) }, /* Tracient RFID */
	{ USB_DEVICE(0x10C4, 0x80F6) }, /* Suunto sports instrument */
	{ USB_DEVICE(0x10C4, 0x8115) }, /* Arygon NFC/Mifare Reader */
	{ USB_DEVICE(0x10C4, 0x813D) }, /* Burnside Telecom Deskmobile */
	{ USB_DEVICE(0x10C4, 0x813F) }, /* Tams Master Easy Control */
	{ USB_DEVICE(0x10C4, 0x814A) }, /* West Mountain Radio RIGblaster P&P */
	{ USB_DEVICE(0x10C4, 0x814B) }, /* West Mountain Radio RIGtalk */
	{ USB_DEVICE(0x10C4, 0x8156) }, /* B&G H3000 link cable */
	{ USB_DEVICE(0x10C4, 0x815E) }, /* Helicomm IP-Link 1220-DVM */
	{ USB_DEVICE(0x10C4, 0x818B) }, /* AVIT Research USB to TTL */
	{ USB_DEVICE(0x10C4, 0x819F) }, /* MJS USB Toslink Switcher */
	{ USB_DEVICE(0x10C4, 0x81A6) }, /* ThinkOptics WavIt */
	{ USB_DEVICE(0x10C4, 0x81AC) }, /* MSD Dash Hawk */
	{ USB_DEVICE(0x10C4, 0x81AD) }, /* INSYS USB Modem */
	{ USB_DEVICE(0x10C4, 0x81C8) }, /* Lipowsky Industrie Elektronik GmbH, Baby-JTAG */
	{ USB_DEVICE(0x10C4, 0x81E2) }, /* Lipowsky Industrie Elektronik GmbH, Baby-LIN */
	{ USB_DEVICE(0x10C4, 0x81E7) }, /* Aerocomm Radio */
	{ USB_DEVICE(0x10C4, 0x81E8) }, /* Zephyr Bioharness */
	{ USB_DEVICE(0x10C4, 0x81F2) }, /* C1007 HF band RFID controller */
	{ USB_DEVICE(0x10C4, 0x8218) }, /* Lipowsky Industrie Elektronik GmbH, HARP-1 */
	{ USB_DEVICE(0x10C4, 0x822B) }, /* Modem EDGE(GSM) Comander 2 */
	{ USB_DEVICE(0x10C4, 0x826B) }, /* Cygnal Integrated Products, Inc., Fasttrax GPS demonstration module */
	{ USB_DEVICE(0x10C4, 0x8293) }, /* Telegesys ETRX2USB */
	{ USB_DEVICE(0x10C4, 0x82F9) }, /* Procyon AVS */
	{ USB_DEVICE(0x10C4, 0x8341) }, /* Siemens MC35PU GPRS Modem */
	{ USB_DEVICE(0x10C4, 0x8382) }, /* Cygnal Integrated Products, Inc. */
	{ USB_DEVICE(0x10C4, 0x83A8) }, /* Amber Wireless AMB2560 */
	{ USB_DEVICE(0x10C4, 0x83D8) }, /* DekTec DTA Plus VHF/UHF Booster/Attenuator */
	{ USB_DEVICE(0x10C4, 0x8411) }, /* Kyocera GPS Module */
	{ USB_DEVICE(0x10C4, 0x8418) }, /* IRZ Automation Teleport SG-10 GSM/GPRS Modem */
	{ USB_DEVICE(0x10C4, 0x846E) }, /* BEI USB Sensor Interface (VCP) */
	{ USB_DEVICE(0x10C4, 0x8477) }, /* Balluff RFID */
	{ USB_DEVICE(0x10C4, 0x85EA) }, /* AC-Services IBUS-IF */
	{ USB_DEVICE(0x10C4, 0x85EB) }, /* AC-Services CIS-IBUS */
	{ USB_DEVICE(0x10C4, 0x8664) }, /* AC-Services CAN-IF */
	{ USB_DEVICE(0x10C4, 0x8665) }, /* AC-Services OBD-IF */
	{ USB_DEVICE(0x10C4, 0xEA60) }, /* Silicon Labs factory default */
	{ USB_DEVICE(0x10C4, 0xEA61) }, /* Silicon Labs factory default */
	{ USB_DEVICE(0x10C4, 0xEA71) }, /* Infinity GPS-MIC-1 Radio Monophone */
	{ USB_DEVICE(0x10C4, 0xF001) }, /* Elan Digital Systems USBscope50 */
	{ USB_DEVICE(0x10C4, 0xF002) }, /* Elan Digital Systems USBwave12 */
	{ USB_DEVICE(0x10C4, 0xF003) }, /* Elan Digital Systems USBpulse100 */
	{ USB_DEVICE(0x10C4, 0xF004) }, /* Elan Digital Systems USBcount50 */
	{ USB_DEVICE(0x10C5, 0xEA61) }, /* Silicon Labs MobiData GPRS USB Modem */
	{ USB_DEVICE(0x10CE, 0xEA6A) }, /* Silicon Labs MobiData GPRS USB Modem 100EU */
	{ USB_DEVICE(0x13AD, 0x9999) }, /* Baltech card reader */
	{ USB_DEVICE(0x1555, 0x0004) }, /* Owen AC4 USB-RS485 Converter */
	{ USB_DEVICE(0x166A, 0x0303) }, /* Clipsal 5500PCU C-Bus USB interface */
	{ USB_DEVICE(0x16D6, 0x0001) }, /* Jablotron serial interface */
	{ USB_DEVICE(0x16DC, 0x0010) }, /* W-IE-NE-R Plein & Baus GmbH PL512 Power Supply */
	{ USB_DEVICE(0x16DC, 0x0011) }, /* W-IE-NE-R Plein & Baus GmbH RCM Remote Control for MARATON Power Supply */
	{ USB_DEVICE(0x16DC, 0x0012) }, /* W-IE-NE-R Plein & Baus GmbH MPOD Multi Channel Power Supply */
	{ USB_DEVICE(0x16DC, 0x0015) }, /* W-IE-NE-R Plein & Baus GmbH CML Control, Monitoring and Data Logger */
	{ USB_DEVICE(0x17F4, 0xAAAA) }, /* Wavesense Jazz blood glucose meter */
	{ USB_DEVICE(0x1843, 0x0200) }, /* Vaisala USB Instrument Cable */
	{ USB_DEVICE(0x18EF, 0xE00F) }, /* ELV USB-I2C-Interface */
	{ USB_DEVICE(0x1BE3, 0x07A6) }, /* WAGO 750-923 USB Service Cable */
	{ USB_DEVICE(0x413C, 0x9500) }, /* DW700 GPS USB interface */
	{ } /* Terminating Entry */
};

MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver cp210x_driver = {
	.name		= "cp210x",
	.probe		= usb_serial_probe,
	.disconnect	= usb_serial_disconnect,
	.id_table	= id_table,
	.no_dynamic_id	= 	1,
};

static struct usb_serial_driver cp210x_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name = 	"cp210x",
	},
	.usb_driver		= &cp210x_driver,
	.id_table		= id_table,
	.num_ports		= 1,
	.bulk_in_size		= 256,
	.bulk_out_size		= 256,
	.open			= cp210x_open,
	.close			= cp210x_close,
	.break_ctl		= cp210x_break_ctl,
	.set_termios		= cp210x_set_termios,
	.tiocmget 		= cp210x_tiocmget,
	.tiocmset		= cp210x_tiocmset,
	.attach			= cp210x_startup,
	.dtr_rts		= cp210x_dtr_rts
};

/* Config request types */
#define REQTYPE_HOST_TO_DEVICE	0x41
#define REQTYPE_DEVICE_TO_HOST	0xc1

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

/*
 * cp210x_get_config
 * Reads from the CP210x configuration registers
 * 'size' is specified in bytes.
 * 'data' is a pointer to a pre-allocated array of integers large
 * enough to hold 'size' bytes (with 4 bytes to each integer)
 */
static int cp210x_get_config(struct usb_serial_port *port, u8 request,
		unsigned int *data, int size)
{
	struct usb_serial *serial = port->serial;
	__le32 *buf;
	int result, i, length;

	/* Number of integers required to contain the array */
	length = (((size - 1) | 3) + 1)/4;

	buf = kcalloc(length, sizeof(__le32), GFP_KERNEL);
	if (!buf) {
		dev_err(&port->dev, "%s - out of memory.\n", __func__);
		return -ENOMEM;
	}

	/* Issue the request, attempting to read 'size' bytes */
	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
				request, REQTYPE_DEVICE_TO_HOST, 0x0000,
				0, buf, size, 300);

	/* Convert data into an array of integers */
	for (i = 0; i < length; i++)
		data[i] = le32_to_cpu(buf[i]);

	kfree(buf);

	if (result != size) {
		dbg("%s - Unable to send config request, "
				"request=0x%x size=%d result=%d\n",
				__func__, request, size, result);
		return -EPROTO;
	}

	return 0;
}

/*
 * cp210x_set_config
 * Writes to the CP210x configuration registers
 * Values less than 16 bits wide are sent directly
 * 'size' is specified in bytes.
 */
static int cp210x_set_config(struct usb_serial_port *port, u8 request,
		unsigned int *data, int size)
{
	struct usb_serial *serial = port->serial;
	__le32 *buf;
	int result, i, length;

	/* Number of integers required to contain the array */
	length = (((size - 1) | 3) + 1)/4;

	buf = kmalloc(length * sizeof(__le32), GFP_KERNEL);
	if (!buf) {
		dev_err(&port->dev, "%s - out of memory.\n",
				__func__);
		return -ENOMEM;
	}

	/* Array of integers into bytes */
	for (i = 0; i < length; i++)
		buf[i] = cpu_to_le32(data[i]);

	if (size > 2) {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0),
				request, REQTYPE_HOST_TO_DEVICE, 0x0000,
				0, buf, size, 300);
	} else {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0),
				request, REQTYPE_HOST_TO_DEVICE, data[0],
				0, NULL, 0, 300);
	}

	kfree(buf);

	if ((size > 2 && result != size) || result < 0) {
		dbg("%s - Unable to send request, "
				"request=0x%x size=%d result=%d\n",
				__func__, request, size, result);
		return -EPROTO;
	}

	return 0;
}

/*
 * cp210x_set_config_single
 * Convenience function for calling cp210x_set_config on single data values
 * without requiring an integer pointer
 */
static inline int cp210x_set_config_single(struct usb_serial_port *port,
		u8 request, unsigned int data)
{
	return cp210x_set_config(port, request, &data, 2);
}

/*
 * cp210x_quantise_baudrate
 * Quantises the baud rate as per AN205 Table 1
 */
static unsigned int cp210x_quantise_baudrate(unsigned int baud) {
	if      (baud <= 56)       baud = 0;
	else if (baud <= 300)      baud = 300;
	else if (baud <= 600)      baud = 600;
	else if (baud <= 1200)     baud = 1200;
	else if (baud <= 1800)     baud = 1800;
	else if (baud <= 2400)     baud = 2400;
	else if (baud <= 4000)     baud = 4000;
	else if (baud <= 4803)     baud = 4800;
	else if (baud <= 7207)     baud = 7200;
	else if (baud <= 9612)     baud = 9600;
	else if (baud <= 14428)    baud = 14400;
	else if (baud <= 16062)    baud = 16000;
	else if (baud <= 19250)    baud = 19200;
	else if (baud <= 28912)    baud = 28800;
	else if (baud <= 38601)    baud = 38400;
	else if (baud <= 51558)    baud = 51200;
	else if (baud <= 56280)    baud = 56000;
	else if (baud <= 58053)    baud = 57600;
	else if (baud <= 64111)    baud = 64000;
	else if (baud <= 77608)    baud = 76800;
	else if (baud <= 117028)   baud = 115200;
	else if (baud <= 129347)   baud = 128000;
	else if (baud <= 156868)   baud = 153600;
	else if (baud <= 237832)   baud = 230400;
	else if (baud <= 254234)   baud = 250000;
	else if (baud <= 273066)   baud = 256000;
	else if (baud <= 491520)   baud = 460800;
	else if (baud <= 567138)   baud = 500000;
	else if (baud <= 670254)   baud = 576000;
	else if (baud <= 1053257)  baud = 921600;
	else if (baud <= 1474560)  baud = 1228800;
	else if (baud <= 2457600)  baud = 1843200;
	else                       baud = 3686400;
	return baud;
}

static int cp210x_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	int result;

	dbg("%s - port %d", __func__, port->number);

	if (cp210x_set_config_single(port, CP210X_IFC_ENABLE, UART_ENABLE)) {
		dev_err(&port->dev, "%s - Unable to enable UART\n",
				__func__);
		return -EPROTO;
	}

	result = usb_serial_generic_open(tty, port);
	if (result)
		return result;

	/* Configure the termios structure */
	cp210x_get_termios(tty, port);
	return 0;
}

static void cp210x_close(struct usb_serial_port *port)
{
	dbg("%s - port %d", __func__, port->number);

	usb_serial_generic_close(port);

	mutex_lock(&port->serial->disc_mutex);
	if (!port->serial->disconnected)
		cp210x_set_config_single(port, CP210X_IFC_ENABLE, UART_DISABLE);
	mutex_unlock(&port->serial->disc_mutex);
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
			&tty->termios->c_cflag, &baud);
		tty_encode_baud_rate(tty, baud, baud);
	}

	else {
		unsigned int cflag;
		cflag = 0;
		cp210x_get_termios_port(port, &cflag, &baud);
	}
}

/*
 * cp210x_get_termios_port
 * This is the heart of cp210x_get_termios which always uses a &usb_serial_port.
 */
static void cp210x_get_termios_port(struct usb_serial_port *port,
	unsigned int *cflagp, unsigned int *baudp)
{
	unsigned int cflag, modem_ctl[4];
	unsigned int baud;
	unsigned int bits;

	dbg("%s - port %d", __func__, port->number);

	cp210x_get_config(port, CP210X_GET_BAUDDIV, &baud, 2);
	/* Convert to baudrate */
	if (baud)
		baud = cp210x_quantise_baudrate((BAUD_RATE_GEN_FREQ + baud/2)/ baud);

	dbg("%s - baud rate = %d", __func__, baud);
	*baudp = baud;

	cflag = *cflagp;

	cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
	cflag &= ~CSIZE;
	switch (bits & BITS_DATA_MASK) {
	case BITS_DATA_5:
		dbg("%s - data bits = 5", __func__);
		cflag |= CS5;
		break;
	case BITS_DATA_6:
		dbg("%s - data bits = 6", __func__);
		cflag |= CS6;
		break;
	case BITS_DATA_7:
		dbg("%s - data bits = 7", __func__);
		cflag |= CS7;
		break;
	case BITS_DATA_8:
		dbg("%s - data bits = 8", __func__);
		cflag |= CS8;
		break;
	case BITS_DATA_9:
		dbg("%s - data bits = 9 (not supported, using 8 data bits)",
								__func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	default:
		dbg("%s - Unknown number of data bits, using 8", __func__);
		cflag |= CS8;
		bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	switch (bits & BITS_PARITY_MASK) {
	case BITS_PARITY_NONE:
		dbg("%s - parity = NONE", __func__);
		cflag &= ~PARENB;
		break;
	case BITS_PARITY_ODD:
		dbg("%s - parity = ODD", __func__);
		cflag |= (PARENB|PARODD);
		break;
	case BITS_PARITY_EVEN:
		dbg("%s - parity = EVEN", __func__);
		cflag &= ~PARODD;
		cflag |= PARENB;
		break;
	case BITS_PARITY_MARK:
		dbg("%s - parity = MARK (not supported, disabling parity)",
				__func__);
		cflag &= ~PARENB;
		bits &= ~BITS_PARITY_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	case BITS_PARITY_SPACE:
		dbg("%s - parity = SPACE (not supported, disabling parity)",
				__func__);
		cflag &= ~PARENB;
		bits &= ~BITS_PARITY_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	default:
		dbg("%s - Unknown parity mode, disabling parity", __func__);
		cflag &= ~PARENB;
		bits &= ~BITS_PARITY_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	cflag &= ~CSTOPB;
	switch (bits & BITS_STOP_MASK) {
	case BITS_STOP_1:
		dbg("%s - stop bits = 1", __func__);
		break;
	case BITS_STOP_1_5:
		dbg("%s - stop bits = 1.5 (not supported, using 1 stop bit)",
								__func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	case BITS_STOP_2:
		dbg("%s - stop bits = 2", __func__);
		cflag |= CSTOPB;
		break;
	default:
		dbg("%s - Unknown number of stop bits, using 1 stop bit",
								__func__);
		bits &= ~BITS_STOP_MASK;
		cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2);
		break;
	}

	cp210x_get_config(port, CP210X_GET_FLOW, modem_ctl, 16);
	if (modem_ctl[0] & 0x0008) {
		dbg("%s - flow control = CRTSCTS", __func__);
		cflag |= CRTSCTS;
	} else {
		dbg("%s - flow control = NONE", __func__);
		cflag &= ~CRTSCTS;
	}

	*cflagp = cflag;
}

static void cp210x_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	unsigned int cflag, old_cflag;
	unsigned int baud = 0, bits;
	unsigned int modem_ctl[4];

	dbg("%s - port %d", __func__, port->number);

	if (!tty)
		return;

	tty->termios->c_cflag &= ~CMSPAR;
	cflag = tty->termios->c_cflag;
	old_cflag = old_termios->c_cflag;
	baud = cp210x_quantise_baudrate(tty_get_baud_rate(tty));

	/* If the baud rate is to be updated*/
	if (baud != tty_termios_baud_rate(old_termios) && baud != 0) {
		dbg("%s - Setting baud rate to %d baud", __func__,
				baud);
		if (cp210x_set_config_single(port, CP210X_SET_BAUDDIV,
					((BAUD_RATE_GEN_FREQ + baud/2) / baud))) {
			dbg("Baud rate requested not supported by device");
			baud = tty_termios_baud_rate(old_termios);
		}
	}
	/* Report back the resulting baud rate */
	tty_encode_baud_rate(tty, baud, baud);

	/* If the number of data bits is to be updated */
	if ((cflag & CSIZE) != (old_cflag & CSIZE)) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_DATA_MASK;
		switch (cflag & CSIZE) {
		case CS5:
			bits |= BITS_DATA_5;
			dbg("%s - data bits = 5", __func__);
			break;
		case CS6:
			bits |= BITS_DATA_6;
			dbg("%s - data bits = 6", __func__);
			break;
		case CS7:
			bits |= BITS_DATA_7;
			dbg("%s - data bits = 7", __func__);
			break;
		case CS8:
			bits |= BITS_DATA_8;
			dbg("%s - data bits = 8", __func__);
			break;
		/*case CS9:
			bits |= BITS_DATA_9;
			dbg("%s - data bits = 9", __func__);
			break;*/
		default:
			dbg("cp210x driver does not "
					"support the number of bits requested,"
					" using 8 bit mode\n");
				bits |= BITS_DATA_8;
				break;
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Number of data bits requested "
					"not supported by device\n");
	}

	if ((cflag & (PARENB|PARODD)) != (old_cflag & (PARENB|PARODD))) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_PARITY_MASK;
		if (cflag & PARENB) {
			if (cflag & PARODD) {
				bits |= BITS_PARITY_ODD;
				dbg("%s - parity = ODD", __func__);
			} else {
				bits |= BITS_PARITY_EVEN;
				dbg("%s - parity = EVEN", __func__);
			}
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Parity mode not supported "
					"by device\n");
	}

	if ((cflag & CSTOPB) != (old_cflag & CSTOPB)) {
		cp210x_get_config(port, CP210X_GET_LINE_CTL, &bits, 2);
		bits &= ~BITS_STOP_MASK;
		if (cflag & CSTOPB) {
			bits |= BITS_STOP_2;
			dbg("%s - stop bits = 2", __func__);
		} else {
			bits |= BITS_STOP_1;
			dbg("%s - stop bits = 1", __func__);
		}
		if (cp210x_set_config(port, CP210X_SET_LINE_CTL, &bits, 2))
			dbg("Number of stop bits requested "
					"not supported by device\n");
	}

	if ((cflag & CRTSCTS) != (old_cflag & CRTSCTS)) {
		cp210x_get_config(port, CP210X_GET_FLOW, modem_ctl, 16);
		dbg("%s - read modem controls = 0x%.4x 0x%.4x 0x%.4x 0x%.4x",
				__func__, modem_ctl[0], modem_ctl[1],
				modem_ctl[2], modem_ctl[3]);

		if (cflag & CRTSCTS) {
			modem_ctl[0] &= ~0x7B;
			modem_ctl[0] |= 0x09;
			modem_ctl[1] = 0x80;
			dbg("%s - flow control = CRTSCTS", __func__);
		} else {
			modem_ctl[0] &= ~0x7B;
			modem_ctl[0] |= 0x01;
			modem_ctl[1] |= 0x40;
			dbg("%s - flow control = NONE", __func__);
		}

		dbg("%s - write modem controls = 0x%.4x 0x%.4x 0x%.4x 0x%.4x",
				__func__, modem_ctl[0], modem_ctl[1],
				modem_ctl[2], modem_ctl[3]);
		cp210x_set_config(port, CP210X_SET_FLOW, modem_ctl, 16);
	}

}

static int cp210x_tiocmset (struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	return cp210x_tiocmset_port(port, set, clear);
}

static int cp210x_tiocmset_port(struct usb_serial_port *port,
		unsigned int set, unsigned int clear)
{
	unsigned int control = 0;

	dbg("%s - port %d", __func__, port->number);

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

	dbg("%s - control = 0x%.4x", __func__, control);

	return cp210x_set_config(port, CP210X_SET_MHS, &control, 2);
}

static void cp210x_dtr_rts(struct usb_serial_port *p, int on)
{
	if (on)
		cp210x_tiocmset_port(p, TIOCM_DTR|TIOCM_RTS, 0);
	else
		cp210x_tiocmset_port(p, 0, TIOCM_DTR|TIOCM_RTS);
}

static int cp210x_tiocmget (struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	unsigned int control;
	int result;

	dbg("%s - port %d", __func__, port->number);

	cp210x_get_config(port, CP210X_GET_MDMSTS, &control, 1);

	result = ((control & CONTROL_DTR) ? TIOCM_DTR : 0)
		|((control & CONTROL_RTS) ? TIOCM_RTS : 0)
		|((control & CONTROL_CTS) ? TIOCM_CTS : 0)
		|((control & CONTROL_DSR) ? TIOCM_DSR : 0)
		|((control & CONTROL_RING)? TIOCM_RI  : 0)
		|((control & CONTROL_DCD) ? TIOCM_CD  : 0);

	dbg("%s - control = 0x%.2x", __func__, control);

	return result;
}

static void cp210x_break_ctl (struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	unsigned int state;

	dbg("%s - port %d", __func__, port->number);
	if (break_state == 0)
		state = BREAK_OFF;
	else
		state = BREAK_ON;
	dbg("%s - turning break %s", __func__,
			state == BREAK_OFF ? "off" : "on");
	cp210x_set_config(port, CP210X_SET_BREAK, &state, 2);
}

static int cp210x_startup(struct usb_serial *serial)
{
	/* cp210x buffers behave strangely unless device is reset */
	usb_reset_device(serial->dev);
	return 0;
}

static int __init cp210x_init(void)
{
	int retval;

	retval = usb_serial_register(&cp210x_device);
	if (retval)
		return retval; /* Failed to register */

	retval = usb_register(&cp210x_driver);
	if (retval) {
		/* Failed to register */
		usb_serial_deregister(&cp210x_device);
		return retval;
	}

	/* Success */
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");
	return 0;
}

static void __exit cp210x_exit(void)
{
	usb_deregister(&cp210x_driver);
	usb_serial_deregister(&cp210x_device);
}

module_init(cp210x_init);
module_exit(cp210x_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable verbose debugging messages");
