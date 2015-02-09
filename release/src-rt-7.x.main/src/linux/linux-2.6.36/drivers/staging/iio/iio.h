/* The industrial I/O core
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _INDUSTRIAL_IO_H_
#define _INDUSTRIAL_IO_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include "sysfs.h"
#include "chrdev.h"

/* IIO TODO LIST */
/*
 * Provide means of adjusting timer accuracy.
 * Currently assumes nano seconds.
 */

/* Event interface flags */
#define IIO_BUSY_BIT_POS 1

struct iio_dev;

/**
 * iio_get_time_ns() - utility function to get a time stamp for events etc
 **/
static inline s64 iio_get_time_ns(void)
{
	struct timespec ts;
	/*
	 * calls getnstimeofday.
	 * If hrtimers then up to ns accurate, if not microsecond.
	 */
	ktime_get_real_ts(&ts);

	return timespec_to_ns(&ts);
}

/**
 * iio_add_event_to_list() - Wraps adding to event lists
 * @el:		the list element of the event to be handled.
 * @head:	the list associated with the event handler being used.
 *
 * Does reference counting to allow shared handlers.
 **/
void iio_add_event_to_list(struct iio_event_handler_list *el,
			   struct list_head *head);

/**
 * iio_remove_event_from_list() - Wraps removing from event list
 * @el:		element to be removed
 * @head:	associate list head for the interrupt handler.
 *
 * Does reference counting to allow shared handlers.
 **/
void iio_remove_event_from_list(struct iio_event_handler_list *el,
				struct list_head *head);

/* Device operating modes */
#define INDIO_DIRECT_MODE		0x01
#define INDIO_RING_TRIGGERED		0x02
#define INDIO_RING_HARDWARE_BUFFER	0x08

#define INDIO_ALL_RING_MODES (INDIO_RING_TRIGGERED | INDIO_RING_HARDWARE_BUFFER)

/* Vast majority of this is set by the industrialio subsystem on a
 * call to iio_device_register. */

/**
 * struct iio_dev - industrial I/O device
 * @id:			[INTERN] used to identify device internally
 * @dev_data:		[DRIVER] device specific data
 * @modes:		[DRIVER] operating modes supported by device
 * @currentmode:	[DRIVER] current operating mode
 * @dev:		[DRIVER] device structure, should be assigned a parent
 *			and owner
 * @attrs:		[DRIVER] general purpose device attributes
 * @driver_module:	[DRIVER] module structure used to ensure correct
 *			ownership of chrdevs etc
 * @num_interrupt_lines:[DRIVER] number of physical interrupt lines from device
 * @interrupts:		[INTERN] interrupt line specific event lists etc
 * @event_attrs:	[DRIVER] event control attributes
 * @event_conf_attrs:	[DRIVER] event configuration attributes
 * @event_interfaces:	[INTERN] event chrdevs associated with interrupt lines
 * @ring:		[DRIVER] any ring buffer present
 * @mlock:		[INTERN] lock used to prevent simultaneous device state
 *			changes
 * @scan_el_attrs:	[DRIVER] control of scan elements if that scan mode
 *			control method is used
 * @scan_count:	[INTERN] the number of elements in the current scan mode
 * @scan_mask:		[INTERN] bitmask used in masking scan mode elements
 * @available_scan_masks: [DRIVER] optional array of allowed bitmasks
 * @scan_timestamp:	[INTERN] does the scan mode include a timestamp
 * @trig:		[INTERN] current device trigger (ring buffer modes)
 * @pollfunc:		[DRIVER] function run on trigger being recieved
 **/
struct iio_dev {
	int				id;
	void				*dev_data;
	int				modes;
	int				currentmode;
	struct device			dev;
	const struct attribute_group	*attrs;
	struct module			*driver_module;

	int				num_interrupt_lines;
	struct iio_interrupt		**interrupts;
	struct attribute_group		*event_attrs;
	struct attribute_group		*event_conf_attrs;

	struct iio_event_interface	*event_interfaces;

	struct iio_ring_buffer		*ring;
	struct mutex			mlock;

	struct attribute_group		*scan_el_attrs;
	int				scan_count;

	u32				scan_mask;
	u32				*available_scan_masks;
	bool				scan_timestamp;
	struct iio_trigger		*trig;
	struct iio_poll_func		*pollfunc;
};

/*
 * These are mainly provided to allow for a change of implementation if a device
 * has a large number of scan elements
 */
#define IIO_MAX_SCAN_LENGTH 31

/* note 0 used as error indicator as it doesn't make sense. */
static inline u32 iio_scan_mask_match(u32 *av_masks, u32 mask)
{
	while (*av_masks) {
		if (!(~*av_masks & mask))
			return *av_masks;
		av_masks++;
	}
	return 0;
}

static inline int iio_scan_mask_query(struct iio_dev *dev_info, int bit)
{
	u32 mask;

	if (bit > IIO_MAX_SCAN_LENGTH)
		return -EINVAL;

	if (!dev_info->scan_mask)
		return 0;

	if (dev_info->available_scan_masks)
		mask = iio_scan_mask_match(dev_info->available_scan_masks,
					dev_info->scan_mask);
	else
		mask = dev_info->scan_mask;

	if (!mask)
		return -EINVAL;

	return !!(mask & (1 << bit));
};

static inline int iio_scan_mask_set(struct iio_dev *dev_info, int bit)
{
	u32 mask;
	u32 trialmask = dev_info->scan_mask | (1 << bit);

	if (bit > IIO_MAX_SCAN_LENGTH)
		return -EINVAL;
	if (dev_info->available_scan_masks) {
		mask = iio_scan_mask_match(dev_info->available_scan_masks,
					trialmask);
		if (!mask)
			return -EINVAL;
	}
	dev_info->scan_mask = trialmask;
	dev_info->scan_count++;

	return 0;
};

static inline int iio_scan_mask_clear(struct iio_dev *dev_info, int bit)
{
	if (bit > IIO_MAX_SCAN_LENGTH)
		return -EINVAL;
	dev_info->scan_mask &= ~(1 << bit);
	dev_info->scan_count--;
	return 0;
};

/**
 * iio_scan_mask_count_to_right() - how many scan elements occur before here
 * @dev_info: the iio_device whose scan mode we are querying
 * @bit: which number scan element is this
 **/
static inline int iio_scan_mask_count_to_right(struct iio_dev *dev_info,
						int bit)
{
	int count = 0;
	int mask = (1 << bit);
	if (bit > IIO_MAX_SCAN_LENGTH)
		return -EINVAL;
	while (mask) {
		mask >>= 1;
		if (mask & dev_info->scan_mask)
			count++;
	}

	return count;
}

/**
 * iio_device_register() - register a device with the IIO subsystem
 * @dev_info:		Device structure filled by the device driver
 **/
int iio_device_register(struct iio_dev *dev_info);

/**
 * iio_device_unregister() - unregister a device from the IIO subsystem
 * @dev_info:		Device structure representing the device.
 **/
void iio_device_unregister(struct iio_dev *dev_info);

/**
 * struct iio_interrupt - wrapper used to allow easy handling of multiple
 *			physical interrupt lines
 * @dev_info:		the iio device for which the is an interrupt line
 * @line_number:	associated line number
 * @id:			idr allocated unique id number
 * @irq:		associate interrupt number
 * @ev_list:		event handler list for associated events
 * @ev_list_lock:	ensure only one access to list at a time
 **/
struct iio_interrupt {
	struct iio_dev			*dev_info;
	int				line_number;
	int				id;
	int				irq;
	struct list_head		ev_list;
	spinlock_t			ev_list_lock;
};

#define to_iio_interrupt(i) container_of(i, struct iio_interrupt, ev_list)

/**
 * iio_register_interrupt_line() - Tell IIO about interrupt lines
 *
 * @irq:		Typically provided via platform data
 * @dev_info:		IIO device info structure for device
 * @line_number:	Which interrupt line of the device is this?
 * @type:		Interrupt type (e.g. edge triggered etc)
 * @name:		Identifying name.
 **/
int iio_register_interrupt_line(unsigned int			irq,
				struct iio_dev			*dev_info,
				int				line_number,
				unsigned long			type,
				const char			*name);

void iio_unregister_interrupt_line(struct iio_dev *dev_info,
				   int line_number);



/**
 * iio_push_event() - try to add event to the list for userspace reading
 * @dev_info:		IIO device structure
 * @ev_line:		Which event line (hardware interrupt)
 * @ev_code:		What event
 * @timestamp:		When the event occurred
 **/
int iio_push_event(struct iio_dev *dev_info,
		  int ev_line,
		  int ev_code,
		  s64 timestamp);

/**
 * __iio_push_event() - tries to add an event to the list associated with a chrdev
 * @ev_int:		the event interface to which we are pushing the event
 * @ev_code:		the outgoing event code
 * @timestamp:		timestamp of the event
 * @shared_pointer_p:	the shared event pointer
 **/
int __iio_push_event(struct iio_event_interface *ev_int,
		    int ev_code,
		    s64 timestamp,
		    struct iio_shared_ev_pointer*
		    shared_pointer_p);
/**
 * __iio_change_event() - change an event code in case of event escalation
 * @ev:			the event to be changed
 * @ev_code:		new event code
 * @timestamp:		new timestamp
 **/
void __iio_change_event(struct iio_detected_event_list *ev,
			int ev_code,
			s64 timestamp);

/**
 * iio_setup_ev_int() - configure an event interface (chrdev)
 * @name:		name used for resulting sysfs directory etc.
 * @ev_int:		interface we are configuring
 * @owner:		module that is responsible for registering this ev_int
 * @dev:		device whose ev_int this is
 **/
int iio_setup_ev_int(struct iio_event_interface *ev_int,
		     const char *name,
		     struct module *owner,
		     struct device *dev);

void iio_free_ev_int(struct iio_event_interface *ev_int);

/**
 * iio_allocate_chrdev() - Allocate a chrdev
 * @handler:	struct that contains relevant file handling for chrdev
 * @dev_info:	iio_dev for which chrdev is being created
 **/
int iio_allocate_chrdev(struct iio_handler *handler, struct iio_dev *dev_info);
void iio_deallocate_chrdev(struct iio_handler *handler);

/* Used to distinguish between bipolar and unipolar scan elemenents.
 * Whilst this may seem obvious, we may well want to change the representation
 * in the future!*/
#define IIO_SIGNED(a) -(a)
#define IIO_UNSIGNED(a) (a)

extern dev_t iio_devt;
extern struct bus_type iio_bus_type;

/**
 * iio_put_device() - reference counted deallocation of struct device
 * @dev: the iio_device containing the device
 **/
static inline void iio_put_device(struct iio_dev *dev)
{
	if (dev)
		put_device(&dev->dev);
};

/**
 * to_iio_dev() - get iio_dev for which we have the struct device
 * @d: the struct device
 **/
static inline struct iio_dev *to_iio_dev(struct device *d)
{
	return container_of(d, struct iio_dev, dev);
};

/**
 * iio_dev_get_devdata() - helper function gets device specific data
 * @d: the iio_dev associated with the device
 **/
static inline void *iio_dev_get_devdata(struct iio_dev *d)
{
	return d->dev_data;
}

/**
 * iio_allocate_device() - allocate an iio_dev from a driver
 **/
struct iio_dev *iio_allocate_device(void);

/**
 * iio_free_device() - free an iio_dev from a driver
 * @dev: the iio_dev associated with the device
 **/
void iio_free_device(struct iio_dev *dev);

/**
 * iio_put() - internal module reference count reduce
 **/
void iio_put(void);

/**
 * iio_get() - internal module reference count increase
 **/
void iio_get(void);

/**
 * iio_device_get_chrdev_minor() - get an unused minor number
 **/
int iio_device_get_chrdev_minor(void);
void iio_device_free_chrdev_minor(int val);

/**
 * iio_ring_enabled() - helper function to test if any form of ring is enabled
 * @dev_info:		IIO device info structure for device
 **/
static inline bool iio_ring_enabled(struct iio_dev *dev_info)
{
	return dev_info->currentmode
		& (INDIO_RING_TRIGGERED
		   | INDIO_RING_HARDWARE_BUFFER);
};

struct idr;

int iio_get_new_idr_val(struct idr *this_idr);
void iio_free_idr_val(struct idr *this_idr, int id);
#endif /* _INDUSTRIAL_IO_H_ */
