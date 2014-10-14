/*
 * Interface to the libusual.
 *
 * Copyright (c) 2005 Pete Zaitcev <zaitcev@redhat.com>
 * Copyright (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 * Copyright (c) 1999 Michael Gee (michael@linuxspecific.com)
 */

#ifndef __LINUX_USB_USUAL_H
#define __LINUX_USB_USUAL_H


/* We should do this for cleanliness... But other usb_foo.h do not do this. */
/* #include <linux/usb.h> */

/*
 * The flags field, which we store in usb_device_id.driver_info.
 * It is compatible with the old usb-storage flags in lower 24 bits.
 */

/*
 * Static flag definitions.  We use this roundabout technique so that the
 * proc_info() routine can automatically display a message for each flag.
 */
#define US_DO_ALL_FLAGS						\
	US_FLAG(SINGLE_LUN,	0x00000001)			\
		/* allow access to only LUN 0 */		\
	US_FLAG(NEED_OVERRIDE,	0x00000002)			\
		/* unusual_devs entry is necessary */		\
	US_FLAG(SCM_MULT_TARG,	0x00000004)			\
		/* supports multiple targets */			\
	US_FLAG(FIX_INQUIRY,	0x00000008)			\
		/* INQUIRY response needs faking */		\
	US_FLAG(FIX_CAPACITY,	0x00000010)			\
		/* READ CAPACITY response too big */		\
	US_FLAG(IGNORE_RESIDUE,	0x00000020)			\
		/* reported residue is wrong */			\
	US_FLAG(BULK32,		0x00000040)			\
		/* Uses 32-byte CBW length */			\
	US_FLAG(NOT_LOCKABLE,	0x00000080)			\
		/* PREVENT/ALLOW not supported */		\
	US_FLAG(GO_SLOW,	0x00000100)			\
		/* Need delay after Command phase */		\
	US_FLAG(NO_WP_DETECT,	0x00000200)			\
		/* Don't check for write-protect */		\
	US_FLAG(MAX_SECTORS_64,	0x00000400)			\
		/* Sets max_sectors to 64    */			\
	US_FLAG(IGNORE_DEVICE,	0x00000800)			\
		/* Don't claim device */			\
	US_FLAG(CAPACITY_HEURISTICS,	0x00001000)		\
		/* sometimes sizes is too big */		\
	US_FLAG(MAX_SECTORS_MIN,0x00002000)			\
		/* Sets max_sectors to arch min */		\
	US_FLAG(BULK_IGNORE_TAG,0x00004000)			\
		/* Ignore tag mismatch in bulk operations */    \
	US_FLAG(SANE_SENSE,     0x00008000)			\
		/* Sane Sense (> 18 bytes) */			\
	US_FLAG(CAPACITY_OK,	0x00010000)			\
		/* READ CAPACITY response is correct */		\
	US_FLAG(BAD_SENSE,	0x00020000)			\
		/* Bad Sense (never more than 18 bytes) */	\
	US_FLAG(NO_READ_DISC_INFO,	0x00040000)		\
		/* cannot handle READ_DISC_INFO */		\
	US_FLAG(NO_READ_CAPACITY_16,	0x00080000)		\
		/* cannot handle READ_CAPACITY_16 */		\
	US_FLAG(INITIAL_READ10,	0x00100000)			\
		/* Initial READ(10) (and others) must be retried */

#define US_FLAG(name, value)	US_FL_##name = value ,
enum { US_DO_ALL_FLAGS };
#undef US_FLAG

/*
 * The bias field for libusual and friends.
 */
#define USB_US_TYPE_NONE   0
#define USB_US_TYPE_STOR   1		/* usb-storage */
#define USB_US_TYPE_UB     2		/* ub */

#define USB_US_TYPE(flags) 		(((flags) >> 24) & 0xFF)
#define USB_US_ORIG_FLAGS(flags)	((flags) & 0x00FFFFFF)

#include <linux/usb/storage.h>

/*
 */
extern int usb_usual_ignore_device(struct usb_interface *intf);
extern struct usb_device_id usb_storage_usb_ids[];

#ifdef CONFIG_USB_LIBUSUAL

extern void usb_usual_set_present(int type);
extern void usb_usual_clear_present(int type);
extern int usb_usual_check_type(const struct usb_device_id *, int type);
#else

#define usb_usual_set_present(t)	do { } while(0)
#define usb_usual_clear_present(t)	do { } while(0)
#define usb_usual_check_type(id, t)	(0)
#endif /* CONFIG_USB_LIBUSUAL */

#endif /* __LINUX_USB_USUAL_H */
