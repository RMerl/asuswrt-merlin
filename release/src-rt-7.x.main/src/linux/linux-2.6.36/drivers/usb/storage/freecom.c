/* Driver for Freecom USB/IDE adaptor
 *
 * Freecom v0.1:
 *
 * First release
 *
 * Current development and maintenance by:
 *   (C) 2000 David Brown <usb-storage@davidb.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This driver was developed with information provided in FREECOM's USB
 * Programmers Reference Guide.  For further information contact Freecom
 * (http://www.freecom.de/)
 */

#include <linux/module.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>

#include "usb.h"
#include "transport.h"
#include "protocol.h"
#include "debug.h"

MODULE_DESCRIPTION("Driver for Freecom USB/IDE adaptor");
MODULE_AUTHOR("David Brown <usb-storage@davidb.org>");
MODULE_LICENSE("GPL");

#ifdef CONFIG_USB_STORAGE_DEBUG
static void pdump (void *, int);
#endif

/* Bits of HD_STATUS */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08

/* All of the outgoing packets are 64 bytes long. */
struct freecom_cb_wrap {
	u8    Type;		/* Command type. */
	u8    Timeout;		/* Timeout in seconds. */
	u8    Atapi[12];	/* An ATAPI packet. */
	u8    Filler[50];	/* Padding Data. */
};

struct freecom_xfer_wrap {
	u8    Type;		/* Command type. */
	u8    Timeout;		/* Timeout in seconds. */
	__le32   Count;		/* Number of bytes to transfer. */
	u8    Pad[58];
} __attribute__ ((packed));

struct freecom_ide_out {
	u8    Type;		/* Type + IDE register. */
	u8    Pad;
	__le16   Value;		/* Value to write. */
	u8    Pad2[60];
};

struct freecom_ide_in {
	u8    Type;		/* Type | IDE register. */
	u8    Pad[63];
};

struct freecom_status {
	u8    Status;
	u8    Reason;
	__le16   Count;
	u8    Pad[60];
};

/* Freecom stuffs the interrupt status in the INDEX_STAT bit of the ide
 * register. */
#define FCM_INT_STATUS		0x02 /* INDEX_STAT */
#define FCM_STATUS_BUSY		0x80

/* These are the packet types.  The low bit indicates that this command
 * should wait for an interrupt. */
#define FCM_PACKET_ATAPI	0x21
#define FCM_PACKET_STATUS	0x20

/* Receive data from the IDE interface.  The ATAPI packet has already
 * waited, so the data should be immediately available. */
#define FCM_PACKET_INPUT	0x81

/* Send data to the IDE interface. */
#define FCM_PACKET_OUTPUT	0x01

/* Write a value to an ide register.  Or the ide register to write after
 * munging the address a bit. */
#define FCM_PACKET_IDE_WRITE	0x40
#define FCM_PACKET_IDE_READ	0xC0

/* All packets (except for status) are 64 bytes long. */
#define FCM_PACKET_LENGTH		64
#define FCM_STATUS_PACKET_LENGTH	4

static int init_freecom(struct us_data *us);


/*
 * The table of devices
 */
#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName, useProtocol, useTransport, \
		    initFunction, flags) \
{ USB_DEVICE_VER(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax), \
  .driver_info = (flags)|(USB_US_TYPE_STOR<<24) }

struct usb_device_id freecom_usb_ids[] = {
#	include "unusual_freecom.h"
	{ }		/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, freecom_usb_ids);

#undef UNUSUAL_DEV

/*
 * The flags table
 */
#define UNUSUAL_DEV(idVendor, idProduct, bcdDeviceMin, bcdDeviceMax, \
		    vendor_name, product_name, use_protocol, use_transport, \
		    init_function, Flags) \
{ \
	.vendorName = vendor_name,	\
	.productName = product_name,	\
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
	.initFunction = init_function,	\
}

static struct us_unusual_dev freecom_unusual_dev_list[] = {
#	include "unusual_freecom.h"
	{ }		/* Terminating entry */
};

#undef UNUSUAL_DEV

static int
freecom_readdata (struct scsi_cmnd *srb, struct us_data *us,
		unsigned int ipipe, unsigned int opipe, int count)
{
	struct freecom_xfer_wrap *fxfr =
		(struct freecom_xfer_wrap *) us->iobuf;
	int result;

	fxfr->Type = FCM_PACKET_INPUT | 0x00;
	fxfr->Timeout = 0;    /* Short timeout for debugging. */
	fxfr->Count = cpu_to_le32 (count);
	memset (fxfr->Pad, 0, sizeof (fxfr->Pad));

	US_DEBUGP("Read data Freecom! (c=%d)\n", count);

	/* Issue the transfer command. */
	result = usb_stor_bulk_transfer_buf (us, opipe, fxfr,
			FCM_PACKET_LENGTH, NULL);
	if (result != USB_STOR_XFER_GOOD) {
		US_DEBUGP ("Freecom readdata transport error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Now transfer all of our blocks. */
	US_DEBUGP("Start of read\n");
	result = usb_stor_bulk_srb(us, ipipe, srb);
	US_DEBUGP("freecom_readdata done!\n");

	if (result > USB_STOR_XFER_SHORT)
		return USB_STOR_TRANSPORT_ERROR;
	return USB_STOR_TRANSPORT_GOOD;
}

static int
freecom_writedata (struct scsi_cmnd *srb, struct us_data *us,
		int unsigned ipipe, unsigned int opipe, int count)
{
	struct freecom_xfer_wrap *fxfr =
		(struct freecom_xfer_wrap *) us->iobuf;
	int result;

	fxfr->Type = FCM_PACKET_OUTPUT | 0x00;
	fxfr->Timeout = 0;    /* Short timeout for debugging. */
	fxfr->Count = cpu_to_le32 (count);
	memset (fxfr->Pad, 0, sizeof (fxfr->Pad));

	US_DEBUGP("Write data Freecom! (c=%d)\n", count);

	/* Issue the transfer command. */
	result = usb_stor_bulk_transfer_buf (us, opipe, fxfr,
			FCM_PACKET_LENGTH, NULL);
	if (result != USB_STOR_XFER_GOOD) {
		US_DEBUGP ("Freecom writedata transport error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Now transfer all of our blocks. */
	US_DEBUGP("Start of write\n");
	result = usb_stor_bulk_srb(us, opipe, srb);

	US_DEBUGP("freecom_writedata done!\n");
	if (result > USB_STOR_XFER_SHORT)
		return USB_STOR_TRANSPORT_ERROR;
	return USB_STOR_TRANSPORT_GOOD;
}

/*
 * Transport for the Freecom USB/IDE adaptor.
 *
 */
static int freecom_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct freecom_cb_wrap *fcb;
	struct freecom_status  *fst;
	unsigned int ipipe, opipe;		/* We need both pipes. */
	int result;
	unsigned int partial;
	int length;

	fcb = (struct freecom_cb_wrap *) us->iobuf;
	fst = (struct freecom_status *) us->iobuf;

	US_DEBUGP("Freecom TRANSPORT STARTED\n");

	/* Get handles for both transports. */
	opipe = us->send_bulk_pipe;
	ipipe = us->recv_bulk_pipe;

	/* The ATAPI Command always goes out first. */
	fcb->Type = FCM_PACKET_ATAPI | 0x00;
	fcb->Timeout = 0;
	memcpy (fcb->Atapi, srb->cmnd, 12);
	memset (fcb->Filler, 0, sizeof (fcb->Filler));

	US_DEBUG(pdump (srb->cmnd, 12));

	/* Send it out. */
	result = usb_stor_bulk_transfer_buf (us, opipe, fcb,
			FCM_PACKET_LENGTH, NULL);

	/* The Freecom device will only fail if there is something wrong in
	 * USB land.  It returns the status in its own registers, which
	 * come back in the bulk pipe. */
	if (result != USB_STOR_XFER_GOOD) {
		US_DEBUGP ("freecom transport error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* There are times we can optimize out this status read, but it
	 * doesn't hurt us to always do it now. */
	result = usb_stor_bulk_transfer_buf (us, ipipe, fst,
			FCM_STATUS_PACKET_LENGTH, &partial);
	US_DEBUGP("foo Status result %d %u\n", result, partial);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	US_DEBUG(pdump ((void *) fst, partial));

	/* The firmware will time-out commands after 20 seconds. Some commands
	 * can legitimately take longer than this, so we use a different
	 * command that only waits for the interrupt and then sends status,
	 * without having to send a new ATAPI command to the device.
	 *
	 * NOTE: There is some indication that a data transfer after a timeout
	 * may not work, but that is a condition that should never happen.
	 */
	while (fst->Status & FCM_STATUS_BUSY) {
		US_DEBUGP("20 second USB/ATAPI bridge TIMEOUT occurred!\n");
		US_DEBUGP("fst->Status is %x\n", fst->Status);

		/* Get the status again */
		fcb->Type = FCM_PACKET_STATUS;
		fcb->Timeout = 0;
		memset (fcb->Atapi, 0, sizeof(fcb->Atapi));
		memset (fcb->Filler, 0, sizeof (fcb->Filler));

		/* Send it out. */
		result = usb_stor_bulk_transfer_buf (us, opipe, fcb,
				FCM_PACKET_LENGTH, NULL);

		/* The Freecom device will only fail if there is something
		 * wrong in USB land.  It returns the status in its own
		 * registers, which come back in the bulk pipe.
		 */
		if (result != USB_STOR_XFER_GOOD) {
			US_DEBUGP ("freecom transport error\n");
			return USB_STOR_TRANSPORT_ERROR;
		}

		/* get the data */
		result = usb_stor_bulk_transfer_buf (us, ipipe, fst,
				FCM_STATUS_PACKET_LENGTH, &partial);

		US_DEBUGP("bar Status result %d %u\n", result, partial);
		if (result != USB_STOR_XFER_GOOD)
			return USB_STOR_TRANSPORT_ERROR;

		US_DEBUG(pdump ((void *) fst, partial));
	}

	if (partial != 4)
		return USB_STOR_TRANSPORT_ERROR;
	if ((fst->Status & 1) != 0) {
		US_DEBUGP("operation failed\n");
		return USB_STOR_TRANSPORT_FAILED;
	}

	/* The device might not have as much data available as we
	 * requested.  If you ask for more than the device has, this reads
	 * and such will hang. */
	US_DEBUGP("Device indicates that it has %d bytes available\n",
			le16_to_cpu (fst->Count));
	US_DEBUGP("SCSI requested %d\n", scsi_bufflen(srb));

	/* Find the length we desire to read. */
	switch (srb->cmnd[0]) {
	case INQUIRY:
	case REQUEST_SENSE:	/* 16 or 18 bytes? spec says 18, lots of devices only have 16 */
	case MODE_SENSE:
	case MODE_SENSE_10:
		length = le16_to_cpu(fst->Count);
		break;
	default:
		length = scsi_bufflen(srb);
	}

	/* verify that this amount is legal */
	if (length > scsi_bufflen(srb)) {
		length = scsi_bufflen(srb);
		US_DEBUGP("Truncating request to match buffer length: %d\n", length);
	}

	/* What we do now depends on what direction the data is supposed to
	 * move in. */

	switch (us->srb->sc_data_direction) {
	case DMA_FROM_DEVICE:
		/* catch bogus "read 0 length" case */
		if (!length)
			break;
		/* Make sure that the status indicates that the device
		 * wants data as well. */
		if ((fst->Status & DRQ_STAT) == 0 || (fst->Reason & 3) != 2) {
			US_DEBUGP("SCSI wants data, drive doesn't have any\n");
			return USB_STOR_TRANSPORT_FAILED;
		}
		result = freecom_readdata (srb, us, ipipe, opipe, length);
		if (result != USB_STOR_TRANSPORT_GOOD)
			return result;

		US_DEBUGP("FCM: Waiting for status\n");
		result = usb_stor_bulk_transfer_buf (us, ipipe, fst,
				FCM_PACKET_LENGTH, &partial);
		US_DEBUG(pdump ((void *) fst, partial));

		if (partial != 4 || result > USB_STOR_XFER_SHORT)
			return USB_STOR_TRANSPORT_ERROR;
		if ((fst->Status & ERR_STAT) != 0) {
			US_DEBUGP("operation failed\n");
			return USB_STOR_TRANSPORT_FAILED;
		}
		if ((fst->Reason & 3) != 3) {
			US_DEBUGP("Drive seems still hungry\n");
			return USB_STOR_TRANSPORT_FAILED;
		}
		US_DEBUGP("Transfer happy\n");
		break;

	case DMA_TO_DEVICE:
		/* catch bogus "write 0 length" case */
		if (!length)
			break;
		/* Make sure the status indicates that the device wants to
		 * send us data. */
		/* !!IMPLEMENT!! */
		result = freecom_writedata (srb, us, ipipe, opipe, length);
		if (result != USB_STOR_TRANSPORT_GOOD)
			return result;

		US_DEBUGP("FCM: Waiting for status\n");
		result = usb_stor_bulk_transfer_buf (us, ipipe, fst,
				FCM_PACKET_LENGTH, &partial);

		if (partial != 4 || result > USB_STOR_XFER_SHORT)
			return USB_STOR_TRANSPORT_ERROR;
		if ((fst->Status & ERR_STAT) != 0) {
			US_DEBUGP("operation failed\n");
			return USB_STOR_TRANSPORT_FAILED;
		}
		if ((fst->Reason & 3) != 3) {
			US_DEBUGP("Drive seems still hungry\n");
			return USB_STOR_TRANSPORT_FAILED;
		}

		US_DEBUGP("Transfer happy\n");
		break;


	case DMA_NONE:
		/* Easy, do nothing. */
		break;

	default:
		/* should never hit here -- filtered in usb.c */
		US_DEBUGP ("freecom unimplemented direction: %d\n",
				us->srb->sc_data_direction);
		/* Return fail, SCSI seems to handle this better. */
		return USB_STOR_TRANSPORT_FAILED;
		break;
	}

	return USB_STOR_TRANSPORT_GOOD;
}

static int init_freecom(struct us_data *us)
{
	int result;
	char *buffer = us->iobuf;

	/* The DMA-mapped I/O buffer is 64 bytes long, just right for
	 * all our packets.  No need to allocate any extra buffer space.
	 */

	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
			0x4c, 0xc0, 0x4346, 0x0, buffer, 0x20, 3*HZ);
	buffer[32] = '\0';
	US_DEBUGP("String returned from FC init is: %s\n", buffer);

	/* Special thanks to the people at Freecom for providing me with
	 * this "magic sequence", which they use in their Windows and MacOS
	 * drivers to make sure that all the attached perhiperals are
	 * properly reset.
	 */

	/* send reset */
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			0x4d, 0x40, 0x24d8, 0x0, NULL, 0x0, 3*HZ);
	US_DEBUGP("result from activate reset is %d\n", result);

	/* wait 250ms */
	mdelay(250);

	/* clear reset */
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			0x4d, 0x40, 0x24f8, 0x0, NULL, 0x0, 3*HZ);
	US_DEBUGP("result from clear reset is %d\n", result);

	/* wait 3 seconds */
	mdelay(3 * 1000);

	return USB_STOR_TRANSPORT_GOOD;
}

static int usb_stor_freecom_reset(struct us_data *us)
{
	printk (KERN_CRIT "freecom reset called\n");

	/* We don't really have this feature. */
	return FAILED;
}

#ifdef CONFIG_USB_STORAGE_DEBUG
static void pdump (void *ibuffer, int length)
{
	static char line[80];
	int offset = 0;
	unsigned char *buffer = (unsigned char *) ibuffer;
	int i, j;
	int from, base;

	offset = 0;
	for (i = 0; i < length; i++) {
		if ((i & 15) == 0) {
			if (i > 0) {
				offset += sprintf (line+offset, " - ");
				for (j = i - 16; j < i; j++) {
					if (buffer[j] >= 32 && buffer[j] <= 126)
						line[offset++] = buffer[j];
					else
						line[offset++] = '.';
				}
				line[offset] = 0;
				US_DEBUGP("%s\n", line);
				offset = 0;
			}
			offset += sprintf (line+offset, "%08x:", i);
		} else if ((i & 7) == 0) {
			offset += sprintf (line+offset, " -");
		}
		offset += sprintf (line+offset, " %02x", buffer[i] & 0xff);
	}

	/* Add the last "chunk" of data. */
	from = (length - 1) % 16;
	base = ((length - 1) / 16) * 16;

	for (i = from + 1; i < 16; i++)
		offset += sprintf (line+offset, "   ");
	if (from < 8)
		offset += sprintf (line+offset, "  ");
	offset += sprintf (line+offset, " - ");

	for (i = 0; i <= from; i++) {
		if (buffer[base+i] >= 32 && buffer[base+i] <= 126)
			line[offset++] = buffer[base+i];
		else
			line[offset++] = '.';
	}
	line[offset] = 0;
	US_DEBUGP("%s\n", line);
	offset = 0;
}
#endif

static int freecom_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
{
	struct us_data *us;
	int result;

	result = usb_stor_probe1(&us, intf, id,
			(id - freecom_usb_ids) + freecom_unusual_dev_list);
	if (result)
		return result;

	us->transport_name = "Freecom";
	us->transport = freecom_transport;
	us->transport_reset = usb_stor_freecom_reset;
	us->max_lun = 0;

	result = usb_stor_probe2(us);
	return result;
}

static struct usb_driver freecom_driver = {
	.name =		"ums-freecom",
	.probe =	freecom_probe,
	.disconnect =	usb_stor_disconnect,
	.suspend =	usb_stor_suspend,
	.resume =	usb_stor_resume,
	.reset_resume =	usb_stor_reset_resume,
	.pre_reset =	usb_stor_pre_reset,
	.post_reset =	usb_stor_post_reset,
	.id_table =	freecom_usb_ids,
	.soft_unbind =	1,
};

static int __init freecom_init(void)
{
	return usb_register(&freecom_driver);
}

static void __exit freecom_exit(void)
{
	usb_deregister(&freecom_driver);
}

module_init(freecom_init);
module_exit(freecom_exit);
