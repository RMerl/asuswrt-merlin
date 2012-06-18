/*
 * Core functions for libusb-compat-0.1
 * Copyright (C) 2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (c) 2000-2003 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <libusb.h>

#include "usb.h"
#include "usbi.h"

static libusb_context *ctx = NULL;
static int usb_debug = 0;

enum usbi_log_level {
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
};

#ifdef ENABLE_LOGGING
#define _usbi_log(level, fmt...) usbi_log(level, __FUNCTION__, fmt)
#else
#define _usbi_log(level, fmt...)
#endif

#ifdef ENABLE_DEBUG_LOGGING
#define usbi_dbg(fmt...) _usbi_log(LOG_LEVEL_DEBUG, fmt)
#else
#define usbi_dbg(fmt...)
#endif

#define usbi_info(fmt...) _usbi_log(LOG_LEVEL_INFO, fmt)
#define usbi_warn(fmt...) _usbi_log(LOG_LEVEL_WARNING, fmt)
#define usbi_err(fmt...) _usbi_log(LOG_LEVEL_ERROR, fmt)

API_EXPORTED struct usb_bus *usb_busses = NULL;

#define compat_err(e) -(errno=libusb_to_errno(e))

static int libusb_to_errno(int result)
{
	switch (result) {
	case LIBUSB_SUCCESS:
		return 0;
	case LIBUSB_ERROR_IO:
		return EIO;
	case LIBUSB_ERROR_INVALID_PARAM:
		return EINVAL;
	case LIBUSB_ERROR_ACCESS:
		return EACCES;
	case LIBUSB_ERROR_NO_DEVICE:
		return ENXIO;
	case LIBUSB_ERROR_NOT_FOUND:
		return ENOENT;
	case LIBUSB_ERROR_BUSY:
		return EBUSY;
	case LIBUSB_ERROR_TIMEOUT:
		return ETIMEDOUT;
	case LIBUSB_ERROR_OVERFLOW:
		return EOVERFLOW;
	case LIBUSB_ERROR_PIPE:
		return EPIPE;
	case LIBUSB_ERROR_INTERRUPTED:
		return EINTR;
	case LIBUSB_ERROR_NO_MEM:
		return ENOMEM;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		return ENOSYS;
	default:
		return ERANGE;
	}
}

static void usbi_log(enum usbi_log_level level, const char *function,
	const char *format, ...)
{
	va_list args;
	FILE *stream = stdout;
	const char *prefix;

#ifndef ENABLE_DEBUG_LOGGING
	if (!usb_debug)
		return;
#endif

	switch (level) {
	case LOG_LEVEL_INFO:
		prefix = "info";
		break;
	case LOG_LEVEL_WARNING:
		stream = stderr;
		prefix = "warning";
		break;
	case LOG_LEVEL_ERROR:
		stream = stderr;
		prefix = "error";
		break;
	case LOG_LEVEL_DEBUG:
		stream = stderr;
		prefix = "debug";
		break;
	default:
		stream = stderr;
		prefix = "unknown";
		break;
	}

	fprintf(stream, "libusb-compat %s: %s: ", prefix, function);

	va_start (args, format);
	vfprintf(stream, format, args);
	va_end (args);

	fprintf(stream, "\n");
}

API_EXPORTED void usb_init(void)
{
	int r;
	usbi_dbg("");

	if (!ctx) {
		r = libusb_init(&ctx);
		if (r < 0) {
			usbi_err("initialization failed!");
			return;
		}

		/* usb_set_debug can be called before usb_init */
		if (usb_debug)
			libusb_set_debug(ctx, 3);
	}
}

API_EXPORTED void usb_set_debug(int level)
{
	usb_debug = level;

	/* usb_set_debug can be called before usb_init */
	if (ctx)
		libusb_set_debug(ctx, 3);
}

API_EXPORTED char *usb_strerror(void)
{
	return strerror(errno);
}

static int find_busses(struct usb_bus **ret)
{
	libusb_device **dev_list = NULL;
	struct usb_bus *busses = NULL;
	struct usb_bus *bus;
	int dev_list_len = 0;
	int i;
	int r;

	r = libusb_get_device_list(ctx, &dev_list);
	if (r < 0) {
		usbi_err("get_device_list failed with error %d", r);
		return compat_err(r);
	}

	if (r == 0) {
		libusb_free_device_list(dev_list, 1);
		/* no buses */
		return 0;
	}

	/* iterate over the device list, identifying the individual busses.
	 * we use the location field of the usb_bus structure to store the
	 * bus number. */

	dev_list_len = r;
	for (i = 0; i < dev_list_len; i++) {
		libusb_device *dev = dev_list[i];
		uint8_t bus_num = libusb_get_bus_number(dev);

		/* if we already know about it, continue */
		if (busses) {
			bus = busses;
			int found = 0;
			do {
				if (bus_num == bus->location) {
					found = 1;
					break;
				}
			} while ((bus = bus->next) != NULL);
			if (found)
				continue;
		}

		/* add it to the list of busses */
		bus = malloc(sizeof(*bus));
		if (!bus)
			goto err;

		memset(bus, 0, sizeof(*bus));
		bus->location = bus_num;
		sprintf(bus->dirname, "%03d", bus_num);
		LIST_ADD(busses, bus);
	}

	libusb_free_device_list(dev_list, 1);
	*ret = busses;
	return 0;

err:
	bus = busses;
	while (bus) {
		struct usb_bus *tbus = bus->next;
		free(bus);
		bus = tbus;
	}
	return -ENOMEM;
}

API_EXPORTED int usb_find_busses(void)
{
	struct usb_bus *new_busses = NULL;
	struct usb_bus *bus;
	int changes = 0;
	int r;

	/* libusb-1.0 initialization might have failed, but we can't indicate
	 * this with libusb-0.1, so trap that situation here */
	if (!ctx)
		return 0;
	
	usbi_dbg("");
	r = find_busses(&new_busses);
	if (r < 0) {
		usbi_err("find_busses failed with error %d", r);
		return r;
	}

	/* walk through all busses we already know about, removing duplicates
	 * from the new list. if we do not find it in the new list, the bus
	 * has been removed. */

	bus = usb_busses;
	while (bus) {
		struct usb_bus *tbus = bus->next;
		struct usb_bus *nbus = new_busses;
		int found = 0;
		usbi_dbg("in loop");

		while (nbus) {
			struct usb_bus *tnbus = nbus->next;

			if (bus->location == nbus->location) {
				LIST_DEL(new_busses, nbus);
				free(nbus);
				found = 1;
				break;
			}
			nbus = tnbus;
		}

		if (!found) {
			/* bus removed */
			usbi_dbg("bus %d removed", bus->location);
			changes++;
			LIST_DEL(usb_busses, bus);
			free(bus);
		}

		bus = tbus;
	}

	/* anything remaining in new_busses is a new bus */
	bus = new_busses;
	while (bus) {
		struct usb_bus *tbus = bus->next;
		usbi_dbg("bus %d added", bus->location);
		LIST_DEL(new_busses, bus);
		LIST_ADD(usb_busses, bus);
		changes++;
		bus = tbus;
	}

	return changes;
}

static int find_devices(libusb_device **dev_list, int dev_list_len,
	struct usb_bus *bus, struct usb_device **ret)
{
	struct usb_device *devices = NULL;
	struct usb_device *dev;
	int i;

	for (i = 0; i < dev_list_len; i++) {
		libusb_device *newlib_dev = dev_list[i];
		uint8_t bus_num = libusb_get_bus_number(newlib_dev);

		if (bus_num != bus->location)
			continue;

		dev = malloc(sizeof(*dev));
		if (!dev)
			goto err;

		/* No need to reference the device now, just take the pointer. We
		 * increase the reference count later if we keep the device. */
		dev->dev = newlib_dev;

		dev->bus = bus;
		dev->devnum = libusb_get_device_address(newlib_dev);
		sprintf(dev->filename, "%03d", dev->devnum);
		LIST_ADD(devices, dev);
	}

	*ret = devices;
	return 0;

err:
	dev = devices;
	while (dev) {
		struct usb_device *tdev = dev->next;
		free(dev);
		dev = tdev;
	}
	return -ENOMEM;
}

static void clear_endpoint_descriptor(struct usb_endpoint_descriptor *ep)
{
	if (ep->extra)
		free(ep->extra);
}

static void clear_interface_descriptor(struct usb_interface_descriptor *iface)
{
	if (iface->extra)
		free(iface->extra);
	if (iface->endpoint) {
		int i;
		for (i = 0; i < iface->bNumEndpoints; i++)
			clear_endpoint_descriptor(iface->endpoint + i);
		free(iface->endpoint);
	}
}

static void clear_interface(struct usb_interface *iface)
{
	if (iface->altsetting) {
		int i;
		for (i = 0; i < iface->num_altsetting; i++)
			clear_interface_descriptor(iface->altsetting + i);
		free(iface->altsetting);
	}
}

static void clear_config_descriptor(struct usb_config_descriptor *config)
{
	if (config->extra)
		free(config->extra);
	if (config->interface) {
		int i;
		for (i = 0; i < config->bNumInterfaces; i++)
			clear_interface(config->interface + i);
		free(config->interface);
	}
}

static void clear_device(struct usb_device *dev)
{
	int i;
	for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
		clear_config_descriptor(dev->config + i);
}

static int copy_endpoint_descriptor(struct usb_endpoint_descriptor *dest,
	const struct libusb_endpoint_descriptor *src)
{
	memcpy(dest, src, USB_DT_ENDPOINT_AUDIO_SIZE);

	dest->extralen = src->extra_length;
	if (src->extra_length) {
		dest->extra = malloc(src->extra_length);
		if (!dest->extra)
			return -ENOMEM;
		memcpy(dest->extra, src->extra, src->extra_length);
	}

	return 0;
}

static int copy_interface_descriptor(struct usb_interface_descriptor *dest,
	const struct libusb_interface_descriptor *src)
{
	int i;
	int num_endpoints = src->bNumEndpoints;
	size_t alloc_size = sizeof(struct usb_endpoint_descriptor) * num_endpoints;

	memcpy(dest, src, USB_DT_INTERFACE_SIZE);
	dest->endpoint = malloc(alloc_size);
	if (!dest->endpoint)
		return -ENOMEM;
	memset(dest->endpoint, 0, alloc_size);

	for (i = 0; i < num_endpoints; i++) {
		int r = copy_endpoint_descriptor(dest->endpoint + i, &src->endpoint[i]);
		if (r < 0) {
			clear_interface_descriptor(dest);
			return r;
		}
	}

	dest->extralen = src->extra_length;
	if (src->extra_length) {
		dest->extra = malloc(src->extra_length);
		if (!dest->extra) {
			clear_interface_descriptor(dest);
			return -ENOMEM;
		}
		memcpy(dest->extra, src->extra, src->extra_length);
	}

	return 0;
}

static int copy_interface(struct usb_interface *dest,
	const struct libusb_interface *src)
{
	int i;
	int num_altsetting = src->num_altsetting;
	size_t alloc_size = sizeof(struct usb_interface_descriptor)
		* num_altsetting;

	dest->num_altsetting = num_altsetting;
	dest->altsetting = malloc(alloc_size);
	if (!dest->altsetting)
		return -ENOMEM;
	memset(dest->altsetting, 0, alloc_size);

	for (i = 0; i < num_altsetting; i++) {
		int r = copy_interface_descriptor(dest->altsetting + i,
			&src->altsetting[i]);
		if (r < 0) {
			clear_interface(dest);
			return r;
		}
	}

	return 0;
}

static int copy_config_descriptor(struct usb_config_descriptor *dest,
	const struct libusb_config_descriptor *src)
{
	int i;
	int num_interfaces = src->bNumInterfaces;
	size_t alloc_size = sizeof(struct usb_interface) * num_interfaces;

	memcpy(dest, src, USB_DT_CONFIG_SIZE);
	dest->interface = malloc(alloc_size);
	if (!dest->interface)
		return -ENOMEM;
	memset(dest->interface, 0, alloc_size);

	for (i = 0; i < num_interfaces; i++) {
		int r = copy_interface(dest->interface + i, &src->interface[i]);
		if (r < 0) {
			clear_config_descriptor(dest);
			return r;
		}
	}

	dest->extralen = src->extra_length;
	if (src->extra_length) {
		dest->extra = malloc(src->extra_length);
		if (!dest->extra) {
			clear_config_descriptor(dest);
			return -ENOMEM;
		}
		memcpy(dest->extra, src->extra, src->extra_length);
	}

	return 0;
}

static int initialize_device(struct usb_device *dev)
{
	libusb_device *newlib_dev = dev->dev;
	int num_configurations;
	size_t alloc_size;
	int r;
	int i;

	/* device descriptor is identical in both libs */
	r = libusb_get_device_descriptor(newlib_dev,
		(struct libusb_device_descriptor *) &dev->descriptor);
	if (r < 0) {
		usbi_err("error %d getting device descriptor", r);
		return compat_err(r);
	}

	num_configurations = dev->descriptor.bNumConfigurations;
	alloc_size = sizeof(struct usb_config_descriptor) * num_configurations;
	dev->config = malloc(alloc_size);
	if (!dev->config)
		return -ENOMEM;
	memset(dev->config, 0, alloc_size);

	/* even though structures are identical, we can't just use libusb-1.0's
	 * config descriptors because we have to store all configurations in
	 * a single flat memory area (libusb-1.0 provides separate allocations).
	 * we hand-copy libusb-1.0's descriptors into our own structures. */
	for (i = 0; i < num_configurations; i++) {
		struct libusb_config_descriptor *newlib_config;
		r = libusb_get_config_descriptor(newlib_dev, i, &newlib_config);
		if (r < 0) {
			clear_device(dev);
			free(dev->config);
			return compat_err(r);
		}
		r = copy_config_descriptor(dev->config + i, newlib_config);
		libusb_free_config_descriptor(newlib_config);
		if (r < 0) {
			clear_device(dev);
			free(dev->config);
			return r;
		}
	}

	/* libusb doesn't implement this and it doesn't seem that important. If
	 * someone asks for it, we can implement it in v1.1 or later. */
	dev->num_children = 0;
	dev->children = NULL;

	libusb_ref_device(newlib_dev);
	return 0;
}

static void free_device(struct usb_device *dev)
{
	clear_device(dev);
	libusb_unref_device(dev->dev);
	free(dev);
}

API_EXPORTED int usb_find_devices(void)
{
	struct usb_bus *bus;
	libusb_device **dev_list;
	int dev_list_len;
	int r;
	int changes = 0;

	/* libusb-1.0 initialization might have failed, but we can't indicate
	 * this with libusb-0.1, so trap that situation here */
	if (!ctx)
		return 0;

	usbi_dbg("");
	dev_list_len = libusb_get_device_list(ctx, &dev_list);
	if (dev_list_len < 0)
		return compat_err(dev_list_len);

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *new_devices = NULL;
		struct usb_device *dev;

		r = find_devices(dev_list, dev_list_len, bus, &new_devices);
		if (r < 0) {
			libusb_free_device_list(dev_list, 1);
			return r;
		}

		/* walk through the devices we already know about, removing duplicates
		 * from the new list. if we do not find it in the new list, the device
		 * has been removed. */
		dev = bus->devices;
		while (dev) {
			int found = 0;
			struct usb_device *tdev = dev->next;
			struct usb_device *ndev = new_devices;

			while (ndev) {
				if (ndev->devnum == dev->devnum) {
					LIST_DEL(new_devices, ndev);
					free(ndev);
					found = 1;
					break;
				}
				ndev = ndev->next;
			}

			if (!found) {
				usbi_dbg("device %d.%d removed",
					dev->bus->location, dev->devnum);
				LIST_DEL(bus->devices, dev);
				free_device(dev);
				changes++;
			}

			dev = tdev;
		}

		/* anything left in new_devices is a new device */
		dev = new_devices;
		while (dev) {
			struct usb_device *tdev = dev->next;
			r = initialize_device(dev);	
			if (r < 0) {
				usbi_err("couldn't initialize device %d.%d (error %d)",
					dev->bus->location, dev->devnum, r);
				dev = tdev;
				continue;
			}
			usbi_dbg("device %d.%d added", dev->bus->location, dev->devnum);
			LIST_DEL(new_devices, dev);
			LIST_ADD(bus->devices, dev);
			changes++;
			dev = tdev;
		}
	}

	libusb_free_device_list(dev_list, 1);
	return changes;
}

API_EXPORTED struct usb_bus *usb_get_busses(void)
{
	return usb_busses;
}

API_EXPORTED usb_dev_handle *usb_open(struct usb_device *dev)
{
	int r;
	usbi_dbg("");

	usb_dev_handle *udev = malloc(sizeof(*udev));
	if (!udev)
		return NULL;

	r = libusb_open((libusb_device *) dev->dev, &udev->handle);
	if (r < 0) {
		usbi_err("could not open device, error %d", r);
		free(udev);
		errno = libusb_to_errno(r);
		return NULL;
	}

	udev->last_claimed_interface = -1;
	udev->device = dev;
	return udev;
}

API_EXPORTED int usb_close(usb_dev_handle *dev)
{
	usbi_dbg("");
	libusb_close(dev->handle);
	free(dev);
	return 0;
}

API_EXPORTED struct usb_device *usb_device(usb_dev_handle *dev)
{
	return dev->device;
}

API_EXPORTED int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
	usbi_dbg("configuration %d", configuration);
	return compat_err(libusb_set_configuration(dev->handle, configuration));
}

API_EXPORTED int usb_claim_interface(usb_dev_handle *dev, int interface)
{
	int r;
	usbi_dbg("interface %d", interface);

	r = libusb_claim_interface(dev->handle, interface);
	if (r == 0) {
		dev->last_claimed_interface = interface;
		return 0;
	}

	return compat_err(r);
}

API_EXPORTED int usb_release_interface(usb_dev_handle *dev, int interface)
{
	int r;
	usbi_dbg("interface %d", interface);

	r = libusb_release_interface(dev->handle, interface);
	if (r == 0)
		dev->last_claimed_interface = -1;

	return compat_err(r);
}

API_EXPORTED int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
	usbi_dbg("alternate %d", alternate);
	if (dev->last_claimed_interface < 0)
		return -(errno=EINVAL);
	
	return compat_err(libusb_set_interface_alt_setting(dev->handle,
		dev->last_claimed_interface, alternate));
}

API_EXPORTED int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
	return compat_err(usb_clear_halt(dev, ep));
}

API_EXPORTED int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
	usbi_dbg("endpoint %x", ep);
	return compat_err(libusb_clear_halt(dev->handle, ep & 0xff));
}

API_EXPORTED int usb_reset(usb_dev_handle *dev)
{
	usbi_dbg("");
	return compat_err(libusb_reset_device(dev->handle));
}

static int usb_bulk_io(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	int actual_length;
	int r;
	usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_bulk_transfer(dev->handle, ep & 0xff, bytes, size,
		&actual_length, timeout);
	
	/* if we timed out but did transfer some data, report as successful short
	 * read. FIXME: is this how libusb-0.1 works? */
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return compat_err(r);
}

API_EXPORTED int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	if (!(ep & USB_ENDPOINT_IN)) {
		/* libusb-0.1 will strangely fix up a read request from endpoint
		 * 0x01 to be from endpoint 0x81. do the same thing here, but
		 * warn about this silly behaviour. */
		usbi_warn("endpoint %x is missing IN direction bit, fixing");
		ep |= USB_ENDPOINT_IN;
	}

	return usb_bulk_io(dev, ep, bytes, size, timeout);
}

API_EXPORTED int usb_bulk_write_sp(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout, int *actual_length, int max_rw)
{
	if (ep & USB_ENDPOINT_IN) {
		/* libusb-0.1 on BSD strangely fix up a write request to endpoint
		 * 0x81 to be to endpoint 0x01. do the same thing here, but
		 * warn about this silly behaviour. */
		usbi_warn("endpoint %x has excessive IN direction bit, fixing");
		ep &= ~USB_ENDPOINT_IN;
	}

	int r;
	usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_bulk_transfer(dev->handle, ep & 0xff, bytes, size,
		actual_length, timeout);
	
	/* if we timed out but did transfer some data, report as successful short
	 * read. FIXME: is this how libusb-0.1 works? */
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return compat_err(r);
}

API_EXPORTED int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	if (ep & USB_ENDPOINT_IN) {
		/* libusb-0.1 on BSD strangely fix up a write request to endpoint
		 * 0x81 to be to endpoint 0x01. do the same thing here, but
		 * warn about this silly behaviour. */
		usbi_warn("endpoint %x has excessive IN direction bit, fixing");
		ep &= ~USB_ENDPOINT_IN;
	}

	return usb_bulk_io(dev, ep, bytes, size, timeout);
}

static int usb_interrupt_io(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	int actual_length;
	int r;
	usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_interrupt_transfer(dev->handle, ep & 0xff, bytes, size,
		&actual_length, timeout);
	
	/* if we timed out but did transfer some data, report as successful short
	 * read. FIXME: is this how libusb-0.1 works? */
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return compat_err(r);
}

API_EXPORTED int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	if (!(ep & USB_ENDPOINT_IN)) {
		/* libusb-0.1 will strangely fix up a read request from endpoint
		 * 0x01 to be from endpoint 0x81. do the same thing here, but
		 * warn about this silly behaviour. */
		usbi_warn("endpoint %x is missing IN direction bit, fixing");
		ep |= USB_ENDPOINT_IN;
	}
	return usb_interrupt_io(dev, ep, bytes, size, timeout);
}

API_EXPORTED int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes,
	int size, int timeout)
{
	if (ep & USB_ENDPOINT_IN) {
		/* libusb-0.1 on BSD strangely fix up a write request to endpoint
		 * 0x81 to be to endpoint 0x01. do the same thing here, but
		 * warn about this silly behaviour. */
		usbi_warn("endpoint %x has excessive IN direction bit, fixing");
		ep &= ~USB_ENDPOINT_IN;
	}

	return usb_interrupt_io(dev, ep, bytes, size, timeout);
}

API_EXPORTED int usb_control_msg(usb_dev_handle *dev, int bmRequestType,
	int bRequest, int wValue, int wIndex, char *bytes, int size, int timeout)
{
	int r;
	usbi_dbg("RQT=%x RQ=%x V=%x I=%x len=%d timeout=%d", bmRequestType,
		bRequest, wValue, wIndex, size, timeout);

	r = libusb_control_transfer(dev->handle, bmRequestType & 0xff,
		bRequest & 0xff, wValue & 0xffff, wIndex & 0xffff, bytes, size & 0xffff,
		timeout);

	if (r >= 0)
		return r;

	return compat_err(r);
}

API_EXPORTED int usb_get_string(usb_dev_handle *dev, int desc_index, int langid,
	char *buf, size_t buflen)
{
	int r;
	r = libusb_get_string_descriptor(dev->handle, desc_index & 0xff,
		langid & 0xffff, buf, (int) buflen);
	if (r >= 0)
		return r;
	return compat_err(r);
}

API_EXPORTED int usb_get_string_simple(usb_dev_handle *dev, int desc_index,
	char *buf, size_t buflen)
{
	int r;
	r = libusb_get_string_descriptor_ascii(dev->handle, desc_index & 0xff,
		buf, (int) buflen);
	if (r >= 0)
		return r;
	return compat_err(r);
}

API_EXPORTED int usb_get_descriptor(usb_dev_handle *dev, unsigned char type,
	unsigned char desc_index, void *buf, int size)
{
	int r;
	r = libusb_get_descriptor(dev->handle, type, desc_index, buf, size);
	if (r >= 0)
		return r;
	return compat_err(r);
}

API_EXPORTED int usb_get_descriptor_by_endpoint(usb_dev_handle *dev, int ep,
	unsigned char type, unsigned char desc_index, void *buf, int size)
{
	/* this function doesn't make much sense - the specs don't talk about
	 * getting a descriptor "by endpoint". libusb-1.0 does not provide this
	 * functionality so we just send a control message directly */
	int r;
	r = libusb_control_transfer(dev->handle,
		LIBUSB_ENDPOINT_IN | (ep & 0xff), LIBUSB_REQUEST_GET_DESCRIPTOR,
		(type << 8) | desc_index, 0, buf, size, 1000);
	if (r >= 0)
		return r;
	return compat_err(r);
}

API_EXPORTED int usb_get_driver_np(usb_dev_handle *dev, int interface,
	char *name, unsigned int namelen)
{
	int r = libusb_kernel_driver_active(dev->handle, interface);
	if (r == 1) {
		/* libusb-1.0 doesn't expose driver name, so fill in a dummy value */
		snprintf(name, namelen, "dummy");
		return 0;
	} else if (r == 0) {
		return -(errno=ENODATA);
	} else {
		return compat_err(r);
	}
}

API_EXPORTED int usb_detach_kernel_driver_np(usb_dev_handle *dev, int interface)
{
	return compat_err(libusb_detach_kernel_driver(dev->handle, interface));
}

