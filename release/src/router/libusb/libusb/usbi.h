/*
 * Internal header for libusb-compat-0.1
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

#ifndef __LIBUSB_USBI_H__
#define __LIBUSB_USBI_H__

/* Some quick and generic macros for the simple kind of lists we use */
#define LIST_ADD(begin, ent) \
	do { \
	  if (begin) { \
	    ent->next = begin; \
	    ent->next->prev = ent; \
	  } else \
	    ent->next = NULL; \
	  ent->prev = NULL; \
	  begin = ent; \
	} while(0)

#define LIST_DEL(begin, ent) \
	do { \
	  if (ent->prev) \
	    ent->prev->next = ent->next; \
	  else \
	    begin = ent->next; \
	  if (ent->next) \
	    ent->next->prev = ent->prev; \
	  ent->prev = NULL; \
	  ent->next = NULL; \
	} while (0)

struct usb_dev_handle {
	libusb_device_handle *handle;
	struct usb_device *device;

	/* libusb-0.1 is buggy w.r.t. interface claiming. it allows you to claim
	 * multiple interfaces but only tracks the most recently claimed one,
	 * which is used for usb_set_altinterface(). we clone the buggy behaviour
	 * here. */
	int last_claimed_interface;
};

#endif

