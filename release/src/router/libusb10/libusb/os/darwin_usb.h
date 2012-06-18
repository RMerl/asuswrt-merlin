/*
 * darwin backend for libusb 1.0
 * Copyright (C) 2008-2009 Nathan Hjelm <hjelmn@users.sourceforge.net>
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

#if !defined(LIBUSB_DARWIN_H)
#define LIBUSB_DARWIN_H

#include "libusbi.h"

#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

/* IOUSBInterfaceInferface */
#if defined (kIOUSBInterfaceInterfaceID300)

#define usb_interface_t IOUSBInterfaceInterface300
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID300
#define InterfaceVersion 300

#elif defined (kIOUSBInterfaceInterfaceID245)

#define usb_interface_t IOUSBInterfaceInterface245
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID245
#define InterfaceVersion 245

#elif defined (kIOUSBInterfaceInterfaceID220)

#define usb_interface_t IOUSBInterfaceInterface220
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID220
#define InterfaceVersion 220

#elif defined (kIOUSBInterfaceInterfaceID197)

#define usb_interface_t IOUSBInterfaceInterface197
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID197
#define InterfaceVersion 197

#elif defined (kIOUSBInterfaceInterfaceID190)

#define usb_interface_t IOUSBInterfaceInterface190
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID190
#define InterfaceVersion 190

#elif defined (kIOUSBInterfaceInterfaceID182)

#define usb_interface_t IOUSBInterfaceInterface182
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID182
#define InterfaceVersion 182

#else

#error "IOUSBFamily is too old. Please upgrade your OS"

#endif

/* IOUSBDeviceInterface */
#if defined (kIOUSBDeviceInterfaceID320)

#define usb_device_t    IOUSBDeviceInterface320
#define DeviceInterfaceID kIOUSBDeviceInterfaceID320
#define DeviceVersion 320

#elif defined (kIOUSBDeviceInterfaceID300)

#define usb_device_t    IOUSBDeviceInterface300
#define DeviceInterfaceID kIOUSBDeviceInterfaceID300
#define DeviceVersion 300

#elif defined (kIOUSBDeviceInterfaceID245)

#define usb_device_t    IOUSBDeviceInterface245
#define DeviceInterfaceID kIOUSBDeviceInterfaceID245
#define DeviceVersion 245

#elif defined (kIOUSBDeviceInterfaceID197)

#define usb_device_t    IOUSBDeviceInterface197
#define DeviceInterfaceID kIOUSBDeviceInterfaceID197
#define DeviceVersion 197

#elif defined (kIOUSBDeviceInterfaceID187)

#define usb_device_t    IOUSBDeviceInterface187
#define DeviceInterfaceID kIOUSBDeviceInterfaceID187
#define DeviceVersion 187

#elif defined (kIOUSBDeviceInterfaceID182)

#define usb_device_t    IOUSBDeviceInterface182
#define DeviceInterfaceID kIOUSBDeviceInterfaceID182
#define DeviceVersion 182

#else

#error "IOUSBFamily is too old. Please upgrade your OS"

#endif

typedef IOCFPlugInInterface *io_cf_plugin_ref_t;
typedef IONotificationPortRef io_notification_port_t;

/* private structures */
struct darwin_device_priv {
  IOUSBDeviceDescriptor dev_descriptor;
  UInt32                location;
  char                  sys_path[21];
  usb_device_t        **device;
  int                  open_count;
};

struct darwin_device_handle_priv {
  int                  is_open;
  CFRunLoopSourceRef   cfSource;
  int                  fds[2];

  struct __darwin_interface {
    usb_interface_t    **interface;
    uint8_t              num_endpoints;
    CFRunLoopSourceRef   cfSource;
    uint8_t            endpoint_addrs[USB_MAXENDPOINTS];
  } interfaces[USB_MAXINTERFACES];
};

struct darwin_transfer_priv {
  /* Isoc */
  IOUSBIsocFrame *isoc_framelist;

  /* Control */
#if !defined (LIBUSB_NO_TIMEOUT_DEVICE)
  IOUSBDevRequestTO req;
#else
  IOUSBDevRequest req;
#endif

  /* Bulk */
};

enum {
  MESSAGE_DEVICE_GONE,
  MESSAGE_ASYNC_IO_COMPLETE
};



#endif
