/*
 * darwin backend for libusb 1.0
 * Copyright (C) 2008-2010 Nathan Hjelm <hjelmn@users.sourceforge.net>
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
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <mach/clock.h>
#include <mach/clock_types.h>
#include <mach/mach_host.h>

#include <mach/mach_port.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

#include "darwin_usb.h"

static mach_port_t  libusb_darwin_mp = 0; /* master port */
static CFRunLoopRef libusb_darwin_acfl = NULL; /* async cf loop */
static int initCount = 0;

/* async event thread */
static pthread_t libusb_darwin_at;

static int darwin_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, unsigned char *buffer, size_t len, int *host_endian);
static int darwin_claim_interface(struct libusb_device_handle *dev_handle, int iface);
static int darwin_release_interface(struct libusb_device_handle *dev_handle, int iface);
static int darwin_reset_device(struct libusb_device_handle *dev_handle);
static void darwin_async_io_callback (void *refcon, IOReturn result, void *arg0);

static const char *darwin_error_str (int result) {
  switch (result) {
  case kIOReturnSuccess:
    return "no error";
  case kIOReturnNotOpen:
    return "device not opened for exclusive access";
  case kIOReturnNoDevice:
    return "no connection to an IOService";
  case kIOUSBNoAsyncPortErr:
    return "no async port has been opened for interface";
  case kIOReturnExclusiveAccess:
    return "another process has device opened for exclusive access";
  case kIOUSBPipeStalled:
    return "pipe is stalled";
  case kIOReturnError:
    return "could not establish a connection to the Darwin kernel";
  case kIOUSBTransactionTimeout:
    return "transaction timed out";
  case kIOReturnBadArgument:
    return "invalid argument";
  case kIOReturnAborted:
    return "transaction aborted";
  case kIOReturnNotResponding:
    return "device not responding";
  case kIOReturnOverrun:
    return "data overrun";
  case kIOReturnCannotWire:
    return "physical memory can not be wired down";
  default:
    return "unknown error";
  }
}

static int darwin_to_libusb (int result) {
  switch (result) {
  case kIOReturnSuccess:
    return LIBUSB_SUCCESS;
  case kIOReturnNotOpen:
  case kIOReturnNoDevice:
    return LIBUSB_ERROR_NO_DEVICE;
  case kIOReturnExclusiveAccess:
    return LIBUSB_ERROR_ACCESS;
  case kIOUSBPipeStalled:
    return LIBUSB_ERROR_PIPE;
  case kIOReturnBadArgument:
    return LIBUSB_ERROR_INVALID_PARAM;
  case kIOUSBTransactionTimeout:
    return LIBUSB_ERROR_TIMEOUT;
  case kIOReturnNotResponding:
  case kIOReturnAborted:
  case kIOReturnError:
  case kIOUSBNoAsyncPortErr:
  default:
    return LIBUSB_ERROR_OTHER;
  }
}


static int ep_to_pipeRef(struct libusb_device_handle *dev_handle, uint8_t ep, uint8_t *pipep, uint8_t *ifcp) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;

  /* current interface */
  struct __darwin_interface *cInterface;

  int8_t i, iface;

  usbi_info (HANDLE_CTX(dev_handle), "converting ep address 0x%02x to pipeRef and interface", ep);

  for (iface = 0 ; iface < USB_MAXINTERFACES ; iface++) {
    cInterface = &priv->interfaces[iface];

    if (dev_handle->claimed_interfaces & (1 << iface)) {
      for (i = 0 ; i < cInterface->num_endpoints ; i++) {
	if (cInterface->endpoint_addrs[i] == ep) {
	  *pipep = i + 1;
	  *ifcp = iface;
	  usbi_info (HANDLE_CTX(dev_handle), "pipe %d on interface %d matches", *pipep, *ifcp);
	  return 0;
	}
      }
    }
  }

  /* No pipe found with the correct endpoint address */
  usbi_warn (HANDLE_CTX(dev_handle), "no pipeRef found with endpoint address 0x%02x.", ep);
  
  return -1;
}

static int usb_setup_device_iterator (io_iterator_t *deviceIterator) {
  return IOServiceGetMatchingServices(libusb_darwin_mp, IOServiceMatching(kIOUSBDeviceClassName), deviceIterator);
}

static usb_device_t **usb_get_next_device (io_iterator_t deviceIterator, UInt32 *locationp) {
  io_cf_plugin_ref_t *plugInInterface = NULL;
  usb_device_t **device;
  io_service_t usbDevice;
  long result;
  SInt32 score;

  if (!IOIteratorIsValid (deviceIterator))
    return NULL;


  while ((usbDevice = IOIteratorNext(deviceIterator))) {
    result = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID,
					       kIOCFPlugInInterfaceID, &plugInInterface,
					       &score);
    if (kIOReturnSuccess == result && plugInInterface)
      break;

    usbi_dbg ("libusb/darwin.c usb_get_next_device: could not set up plugin for service: %s\n", darwin_error_str (result));
  }

  if (!usbDevice)
    return NULL;

  (void)IOObjectRelease(usbDevice);
  (void)(*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(DeviceInterfaceID),
					   (LPVOID)&device);

  (*plugInInterface)->Stop(plugInInterface);
  IODestroyPlugInInterface (plugInInterface);
  
  (*(device))->GetLocationID(device, locationp);

  return device;
}

static kern_return_t darwin_get_device (uint32_t dev_location, usb_device_t ***darwin_device) {
  kern_return_t kresult;
  UInt32        location;
  io_iterator_t deviceIterator;

  kresult = usb_setup_device_iterator (&deviceIterator);
  if (kresult)
    return kresult;

  /* This port of libusb uses locations to keep track of devices. */
  while ((*darwin_device = usb_get_next_device (deviceIterator, &location)) != NULL) {
    if (location == dev_location)
      break;

    (**darwin_device)->Release(*darwin_device);
  }

  IOObjectRelease (deviceIterator);

  if (!(*darwin_device))
    return kIOReturnNoDevice;

  return kIOReturnSuccess;
}



static void darwin_devices_detached (void *ptr, io_iterator_t rem_devices) {  
  struct libusb_context *ctx = (struct libusb_context *)ptr;
  struct libusb_device_handle *handle;
  struct darwin_device_priv *dpriv;
  struct darwin_device_handle_priv *priv;

  io_service_t device;
  long location;
  CFTypeRef locationCF;
  UInt32 message;

  usbi_info (ctx, "a device has been detached");

  while ((device = IOIteratorNext (rem_devices)) != 0) {
    /* get the location from the i/o registry */
    locationCF = IORegistryEntryCreateCFProperty (device, CFSTR(kUSBDevicePropertyLocationID), kCFAllocatorDefault, 0);

    CFNumberGetValue(locationCF, kCFNumberLongType, &location);
    CFRelease (locationCF);
    IOObjectRelease (device);

    pthread_mutex_lock(&ctx->open_devs_lock);
    list_for_each_entry(handle, &ctx->open_devs, list) {
      dpriv = (struct darwin_device_priv *)handle->dev->os_priv;

      /* the device may have been opened several times. write to each handle's event descriptor */
      if (dpriv->location == location  && handle->os_priv) {
	priv  = (struct darwin_device_handle_priv *)handle->os_priv;

	message = MESSAGE_DEVICE_GONE;
	write (priv->fds[1], &message, sizeof (message));
      }
    }

    pthread_mutex_unlock(&ctx->open_devs_lock);
  }
}

static void darwin_clear_iterator (io_iterator_t iter) {
  io_service_t device;

  while ((device = IOIteratorNext (iter)) != 0)
    IOObjectRelease (device);
}

static void *event_thread_main (void *arg0) {
  IOReturn kresult;
  struct libusb_context *ctx = (struct libusb_context *)arg0;

  /* hotplug (device removal) source */
  CFRunLoopSourceRef     libusb_notification_cfsource;
  io_notification_port_t libusb_notification_port;
  io_iterator_t          libusb_rem_device_iterator;

  usbi_info (ctx, "creating hotplug event source");

  CFRetain (CFRunLoopGetCurrent ());

  /* add the notification port to the run loop */
  libusb_notification_port     = IONotificationPortCreate (libusb_darwin_mp);
  libusb_notification_cfsource = IONotificationPortGetRunLoopSource (libusb_notification_port);
  CFRunLoopAddSource(CFRunLoopGetCurrent (), libusb_notification_cfsource, kCFRunLoopDefaultMode);

  /* create notifications for removed devices */
  kresult = IOServiceAddMatchingNotification (libusb_notification_port, kIOTerminatedNotification,
					      IOServiceMatching(kIOUSBDeviceClassName),
					      (IOServiceMatchingCallback)darwin_devices_detached,
					      (void *)ctx, &libusb_rem_device_iterator);
 
  if (kresult != kIOReturnSuccess) {
    usbi_err (ctx, "could not add hotplug event source: %s", darwin_error_str (kresult));

    pthread_exit ((void *)kresult);
  }

  /* arm notifiers */
  darwin_clear_iterator (libusb_rem_device_iterator);

  /* let the main thread know about the async runloop */
  libusb_darwin_acfl = CFRunLoopGetCurrent ();

  usbi_info (ctx, "libopenusb/darwin.c event_thread_main: thread ready to receive events");

  /* run the runloop */
  CFRunLoopRun();

  usbi_info (ctx, "libopenusb/darwin.c event_thread_main: thread exiting");

  /* delete notification port */
  CFRunLoopSourceInvalidate (libusb_notification_cfsource);
  IONotificationPortDestroy (libusb_notification_port);

  CFRelease (CFRunLoopGetCurrent ());

  libusb_darwin_acfl = NULL;

  pthread_exit (0);
}

static int darwin_init(struct libusb_context *ctx) {
  IOReturn kresult;

  if (!(initCount++)) {
    /* Create the master port for talking to IOKit */
    if (!libusb_darwin_mp) {
      kresult = IOMasterPort (MACH_PORT_NULL, &libusb_darwin_mp);

      if (kresult != kIOReturnSuccess || !libusb_darwin_mp)
	return darwin_to_libusb (kresult);
    }

    pthread_create (&libusb_darwin_at, NULL, event_thread_main, (void *)ctx);

    while (!libusb_darwin_acfl)
      usleep (10);
  }

  return 0;
}

static void darwin_exit (void) {
  if (!(--initCount)) {
    void *ret;

    /* stop the async runloop */
    CFRunLoopStop (libusb_darwin_acfl);
    pthread_join (libusb_darwin_at, &ret);

    if (libusb_darwin_mp)
      mach_port_deallocate(mach_task_self(), libusb_darwin_mp);

    libusb_darwin_mp = 0;
  }
}

static int darwin_get_device_descriptor(struct libusb_device *dev, unsigned char *buffer, int *host_endian) {
  struct darwin_device_priv *priv = (struct darwin_device_priv *)dev->os_priv;

  /* return cached copy */
  memmove (buffer, &(priv->dev_descriptor), DEVICE_DESC_LENGTH);

  *host_endian = 0;

  return 0;
}

static int get_configuration_index (struct libusb_device *dev, int config_value) {
  struct darwin_device_priv *priv = (struct darwin_device_priv *)dev->os_priv;
  UInt8 i, numConfig;
  IOUSBConfigurationDescriptorPtr desc;
  IOReturn kresult;

  /* is there a simpler way to determine the index? */
  kresult = (*(priv->device))->GetNumberOfConfigurations (priv->device, &numConfig);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  for (i = 0 ; i < numConfig ; i++) {
    (*(priv->device))->GetConfigurationDescriptorPtr (priv->device, i, &desc);

    if (libusb_le16_to_cpu (desc->bConfigurationValue) == config_value)
      return i;
  }

  /* configuration not found */
  return LIBUSB_ERROR_OTHER;
}

static int darwin_get_active_config_descriptor(struct libusb_device *dev, unsigned char *buffer, size_t len, int *host_endian) {
  struct darwin_device_priv *priv = (struct darwin_device_priv *)dev->os_priv;
  UInt8 config_value;
  int config_index;
  IOReturn kresult;

  kresult = (*(priv->device))->GetConfiguration (priv->device, &config_value);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  config_index = get_configuration_index (dev, config_value);
  if (config_index < 0)
    return config_index;

  return darwin_get_config_descriptor (dev, config_index, buffer, len, host_endian);
}

static int darwin_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, unsigned char *buffer, size_t len, int *host_endian) {
  struct darwin_device_priv *priv = (struct darwin_device_priv *)dev->os_priv;
  IOUSBConfigurationDescriptorPtr desc;
  IOReturn kresult;
  usb_device_t **device = NULL;

  if (!priv)
    return LIBUSB_ERROR_OTHER;

  if (!priv->device) {
    kresult = darwin_get_device (priv->location, &device);
    if (kresult || !device) {
      usbi_err (DEVICE_CTX (dev), "could not find device: %s", darwin_error_str (kresult));

      return darwin_to_libusb (kresult);
    }

    /* don't have to open the device to get a config descriptor */
  } else
    device = priv->device;

  kresult = (*device)->GetConfigurationDescriptorPtr (device, config_index, &desc);
  if (kresult == kIOReturnSuccess) {
    /* copy descriptor */
    if (libusb_le16_to_cpu(desc->wTotalLength) < len)
      len = libusb_le16_to_cpu(desc->wTotalLength);

    memmove (buffer, desc, len);

    /* GetConfigurationDescriptorPtr returns the descriptor in USB bus order */
    *host_endian = 0;
  }

  if (!priv->device)
    (*device)->Release (device);

  return darwin_to_libusb (kresult);
}

static int process_new_device (struct libusb_context *ctx, usb_device_t **device, UInt32 locationID, struct discovered_devs **_discdevs) {
  struct darwin_device_priv *priv;
  struct libusb_device *dev; 
  struct discovered_devs *discdevs;
  UInt16                address, idVendor, idProduct;
  UInt8                 bDeviceClass, bDeviceSubClass;
  IOUSBDevRequest      req;
  int ret = 0, need_unref = 0;

  do {
    dev = usbi_get_device_by_session_id(ctx, locationID);
    if (!dev) {
      usbi_info (ctx, "allocating new device for location 0x%08x", locationID);
      dev = usbi_alloc_device(ctx, locationID);
      need_unref = 1;
    } else
      usbi_info (ctx, "using existing device for location 0x%08x", locationID);

    if (!dev) {
      ret = LIBUSB_ERROR_NO_MEM;
      break;
    }

    priv = (struct darwin_device_priv *)dev->os_priv;

    /* Set up request for device descriptor */
    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest      = kUSBRqGetDescriptor;
    req.wValue        = kUSBDeviceDesc << 8;
    req.wIndex        = 0;
    req.wLength       = sizeof(IOUSBDeviceDescriptor);
    req.pData         = &(priv->dev_descriptor);
    
    (*(device))->GetDeviceAddress (device, (USBDeviceAddress *)&address);
    (*(device))->GetDeviceProduct (device, &idProduct);
    (*(device))->GetDeviceVendor (device, &idVendor);
    (*(device))->GetDeviceClass (device, &bDeviceClass);
    (*(device))->GetDeviceSubClass (device, &bDeviceSubClass);

    /**** retrieve device descriptors ****/
    /* device must be open for DeviceRequest */
    (*device)->USBDeviceOpen(device);

    ret = (*(device))->DeviceRequest (device, &req);
    if (ret != kIOReturnSuccess) {
      int try_unsuspend = 1;
#if DeviceVersion >= 320
      UInt32 info;

      /* device may be suspended. unsuspend it and try again */
      /* IOUSBFamily 320+ provides a way to detect device suspension but earlier versions do not */
      (void)(*device)->GetUSBDeviceInformation (device, &info);

      try_unsuspend = info & (1 << kUSBInformationDeviceIsSuspendedBit);
#endif

      if (try_unsuspend) {
	/* resume the device */
	(void)(*device)->USBDeviceSuspend (device, 0);

	ret = (*(device))->DeviceRequest (device, &req);

	/* resuspend the device */
	(void)(*device)->USBDeviceSuspend (device, 1);
      }
    }

    (*device)->USBDeviceClose (device);

    if (ret != kIOReturnSuccess) {
      usbi_warn (ctx, "could not retrieve device descriptor: %s. skipping device", darwin_error_str (ret));
      ret = -1;
      break;
    }

    /**** end: retrieve device descriptors ****/
    
    /* catch buggy hubs (which appear to be virtual). Apple's own USB prober has problems with these devices. */
    if (libusb_le16_to_cpu (priv->dev_descriptor.idProduct) != idProduct) {
      /* not a valid device */
      usbi_warn (ctx, "idProduct from iokit (%04x) does not match idProduct in descriptor (%04x). skipping device",
		 idProduct, libusb_le16_to_cpu (priv->dev_descriptor.idProduct));
      ret = -1;
      break;
    }

    dev->bus_number     = locationID >> 24;
    dev->device_address = address;

    /* save our location, we'll need this later */
    priv->location = locationID;
    snprintf(priv->sys_path, 20, "%03i-%04x-%04x-%02x-%02x", address, idVendor, idProduct, bDeviceClass, bDeviceSubClass);

    ret = usbi_sanitize_device (dev);
    if (ret < 0)
      break;

    /* append the device to the list of discovered devices */
    discdevs = discovered_devs_append(*_discdevs, dev);
    if (!discdevs) {
      ret = LIBUSB_ERROR_NO_MEM;
      break;
    }
    
    *_discdevs = discdevs;
  
    usbi_info (ctx, "found device with address %d at %s", dev->device_address, priv->sys_path);
  } while (0);

  if (need_unref)
    libusb_unref_device(dev);

  return ret;
}

static int darwin_get_device_list(struct libusb_context *ctx, struct discovered_devs **_discdevs) {
  io_iterator_t        deviceIterator;
  usb_device_t         **device;
  kern_return_t        kresult;
  UInt32               location;

  if (!libusb_darwin_mp)
    return LIBUSB_ERROR_INVALID_PARAM;
  
  kresult = usb_setup_device_iterator (&deviceIterator);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  while ((device = usb_get_next_device (deviceIterator, &location)) != NULL) {
    (void) process_new_device (ctx, device, location, _discdevs);

    (*(device))->Release(device);
  }

  IOObjectRelease(deviceIterator);

  return 0;
}

static int darwin_open (struct libusb_device_handle *dev_handle) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  usb_device_t  **darwin_device;
  IOReturn kresult;

  if (0 == dpriv->open_count) {
    kresult = darwin_get_device (dpriv->location, &darwin_device);
    if (kresult) {
      usbi_err (HANDLE_CTX (dev_handle), "could not find device: %s", darwin_error_str (kresult));
      return darwin_to_libusb (kresult);
    }

    dpriv->device = darwin_device;

    /* try to open the device */
    kresult = (*(dpriv->device))->USBDeviceOpenSeize (dpriv->device);

    if (kresult != kIOReturnSuccess) {
      usbi_err (HANDLE_CTX (dev_handle), "USBDeviceOpen: %s", darwin_error_str(kresult));

      switch (kresult) {
      case kIOReturnExclusiveAccess:
	/* it is possible to perform some actions on a device that is not open so do not return an error */
	priv->is_open = 0;

	break;
      default:
	(*(dpriv->device))->Release (dpriv->device);
	dpriv->device = NULL;
	return darwin_to_libusb (kresult);
      }
    } else {
      priv->is_open = 1;

      /* create async event source */
      kresult = (*(dpriv->device))->CreateDeviceAsyncEventSource (dpriv->device, &priv->cfSource);

      CFRetain (libusb_darwin_acfl);

      /* add the cfSource to the aync run loop */
      CFRunLoopAddSource(libusb_darwin_acfl, priv->cfSource, kCFRunLoopCommonModes);
    }
  }

  /* device opened successfully */
  dpriv->open_count++;

  /* create a file descriptor for notifications */
  pipe (priv->fds);

  /* set the pipe to be non-blocking */
  fcntl (priv->fds[1], F_SETFD, O_NONBLOCK);

  usbi_add_pollfd(HANDLE_CTX(dev_handle), priv->fds[0], POLLIN);

  usbi_info (HANDLE_CTX (dev_handle), "device open for access");

  return 0;
}

static void darwin_close (struct libusb_device_handle *dev_handle) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  IOReturn kresult;
  int i;

  if (dpriv->open_count == 0) {
    /* something is probably very wrong if this is the case */
    usbi_err (HANDLE_CTX (dev_handle), "Close called on a device that was not open!\n");
    return;
  }

  dpriv->open_count--;

  /* make sure all interfaces are released */
  for (i = 0 ; i < USB_MAXINTERFACES ; i++)
    if (dev_handle->claimed_interfaces & (1 << i))
      libusb_release_interface (dev_handle, i);

  if (0 == dpriv->open_count) {
    if (priv->is_open) {
      /* delete the device's async event source */
      if (priv->cfSource) {
	CFRunLoopRemoveSource (libusb_darwin_acfl, priv->cfSource, kCFRunLoopDefaultMode);
	CFRelease (priv->cfSource);
      }

      /* close the device */
      kresult = (*(dpriv->device))->USBDeviceClose(dpriv->device);
      if (kresult) {
	/* Log the fact that we had a problem closing the file, however failing a
	 * close isn't really an error, so return success anyway */
	usbi_err (HANDLE_CTX (dev_handle), "USBDeviceClose: %s", darwin_error_str(kresult));
      }
    }
  
    kresult = (*(dpriv->device))->Release(dpriv->device);
    if (kresult) {
      /* Log the fact that we had a problem closing the file, however failing a
       * close isn't really an error, so return success anyway */
      usbi_err (HANDLE_CTX (dev_handle), "Release: %s", darwin_error_str(kresult));
    }

    dpriv->device = NULL;
  }

  /* file descriptors are maintained per-instance */
  usbi_remove_pollfd (HANDLE_CTX (dev_handle), priv->fds[0]);
  close (priv->fds[1]);
  close (priv->fds[0]);

  priv->fds[0] = priv->fds[1] = -1;
}

static int darwin_get_configuration(struct libusb_device_handle *dev_handle, int *config) {
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  UInt8 configNum;
  IOReturn kresult;

  kresult = (*(dpriv->device))->GetConfiguration (dpriv->device, &configNum);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  *config = (int) configNum;

  return 0;
}

static int darwin_set_configuration(struct libusb_device_handle *dev_handle, int config) {
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  IOReturn kresult;
  int i;

  /* Setting configuration will invalidate the interface, so we need
     to reclaim it. First, dispose of existing interfaces, if any. */
  for (i = 0 ; i < USB_MAXINTERFACES ; i++)
    if (dev_handle->claimed_interfaces & (1 << i))
      darwin_release_interface (dev_handle, i);

  kresult = (*(dpriv->device))->SetConfiguration (dpriv->device, config);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  /* Reclaim any interfaces. */
  for (i = 0 ; i < USB_MAXINTERFACES ; i++)
    if (dev_handle->claimed_interfaces & (1 << i))
      darwin_claim_interface (dev_handle, i);

  return 0;
}

static int darwin_get_interface (usb_device_t **darwin_device, uint8_t ifc, io_service_t *usbInterfacep) {
  IOUSBFindInterfaceRequest request;
  uint8_t                   current_interface;
  kern_return_t             kresult;
  io_iterator_t             interface_iterator;

  *usbInterfacep = IO_OBJECT_NULL;
  
  /* Setup the Interface Request */
  request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

  kresult = (*(darwin_device))->CreateInterfaceIterator(darwin_device, &request, &interface_iterator);
  if (kresult)
    return kresult;

  for ( current_interface = 0 ; current_interface <= ifc ; current_interface++ )
    *usbInterfacep = IOIteratorNext(interface_iterator);
  
  /* done with the interface iterator */
  IOObjectRelease(interface_iterator);
  
  return 0;
}

static int get_endpoints (struct libusb_device_handle *dev_handle, int iface) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;

  /* current interface */
  struct __darwin_interface *cInterface = &priv->interfaces[iface];

  kern_return_t kresult;

  u_int8_t numep, direction, number;
  u_int8_t dont_care1, dont_care3;
  u_int16_t dont_care2;
  int i;

  usbi_info (HANDLE_CTX (dev_handle), "building table of endpoints.");
  
  /* retrieve the total number of endpoints on this interface */
  kresult = (*(cInterface->interface))->GetNumEndpoints(cInterface->interface, &numep);
  if (kresult) {
    usbi_err (HANDLE_CTX (dev_handle), "can't get number of endpoints for interface: %s", darwin_error_str(kresult));
    return darwin_to_libusb (kresult);
  }

  /* iterate through pipe references */
  for (i = 1 ; i <= numep ; i++) {
    kresult = (*(cInterface->interface))->GetPipeProperties(cInterface->interface, i, &direction, &number, &dont_care1,
							    &dont_care2, &dont_care3);

    if (kresult != kIOReturnSuccess) {
      usbi_err (HANDLE_CTX (dev_handle), "error getting pipe information for pipe %d: %s", i, darwin_error_str(kresult));

      return darwin_to_libusb (kresult);
    }

    usbi_info (HANDLE_CTX (dev_handle), "interface: %i pipe %i: dir: %i number: %i", iface, i, direction, number);

    cInterface->endpoint_addrs[i - 1] = ((direction << 7 & LIBUSB_ENDPOINT_DIR_MASK) | (number & LIBUSB_ENDPOINT_ADDRESS_MASK));
  }

  cInterface->num_endpoints = numep;
  
  return 0;
}

static int darwin_claim_interface(struct libusb_device_handle *dev_handle, int iface) {
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;
  io_service_t          usbInterface = IO_OBJECT_NULL;
  IOReturn kresult;
  IOCFPlugInInterface **plugInInterface = NULL;
  SInt32                score;
  uint8_t               new_config;

  /* current interface */
  struct __darwin_interface *cInterface = &priv->interfaces[iface];

  kresult = darwin_get_interface (dpriv->device, iface, &usbInterface);
  if (kresult != kIOReturnSuccess)
    return darwin_to_libusb (kresult);

  /* make sure we have an interface */
  if (!usbInterface) {
    u_int8_t nConfig;     /* Index of configuration to use */
    IOUSBConfigurationDescriptorPtr configDesc; /* to describe which configuration to select */
    /* Only a composite class device with no vendor-specific driver will
       be configured. Otherwise, we need to do it ourselves, or there
       will be no interfaces for the device. */

    usbi_info (HANDLE_CTX (dev_handle), "no interface found; selecting configuration" );

    kresult = (*(dpriv->device))->GetNumberOfConfigurations (dpriv->device, &nConfig);
    if (kresult != kIOReturnSuccess) {
      usbi_err (HANDLE_CTX (dev_handle), "GetNumberOfConfigurations: %s", darwin_error_str(kresult));
      return darwin_to_libusb(kresult);
    }
    
    if (nConfig < 1) {
      usbi_err (HANDLE_CTX (dev_handle), "GetNumberOfConfigurations: no configurations");
      return LIBUSB_ERROR_OTHER;
    }

    usbi_info (HANDLE_CTX (dev_handle), "device has %d configuration%s. using the first",
	      (int)nConfig, (nConfig > 1 ? "s" : "") );

    /* Always use the first configuration */
    kresult = (*(dpriv->device))->GetConfigurationDescriptorPtr (dpriv->device, 0, &configDesc);
    if (kresult != kIOReturnSuccess) {
      usbi_err (HANDLE_CTX (dev_handle), "GetConfigurationDescriptorPtr: %s",
		darwin_error_str(kresult));

      new_config = 1;
    } else
      new_config = configDesc->bConfigurationValue;

    usbi_info (HANDLE_CTX (dev_handle), "new configuration value is %d", new_config);

    /* set the configuration */
    kresult = darwin_set_configuration (dev_handle, new_config);
    if (kresult != LIBUSB_SUCCESS) {
      usbi_err (HANDLE_CTX (dev_handle), "could not set configuration");
      return kresult;
    }

    kresult = darwin_get_interface (dpriv->device, iface, &usbInterface);
    if (kresult) {
      usbi_err (HANDLE_CTX (dev_handle), "darwin_get_interface: %s", darwin_error_str(kresult));
      return darwin_to_libusb (kresult);
    }
  }

  if (!usbInterface) {
    usbi_err (HANDLE_CTX (dev_handle), "interface not found");
    return LIBUSB_ERROR_NOT_FOUND;
  }
  
  /* get an interface to the device's interface */
  kresult = IOCreatePlugInInterfaceForService (usbInterface, kIOUSBInterfaceUserClientTypeID,
					       kIOCFPlugInInterfaceID, &plugInInterface, &score);
  if (kresult) {
    usbi_err (HANDLE_CTX (dev_handle), "IOCreatePlugInInterfaceForService: %s", darwin_error_str(kresult));
    return darwin_to_libusb (kresult);
  }

  if (!plugInInterface) {
    usbi_err (HANDLE_CTX (dev_handle), "plugin interface not found");
    return LIBUSB_ERROR_NOT_FOUND;
  }

  /* ignore release error */
  (void)IOObjectRelease (usbInterface);
  
  /* Do the actual claim */
  kresult = (*plugInInterface)->QueryInterface(plugInInterface,
					       CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
					       (LPVOID)&cInterface->interface);
  if (kresult || !cInterface->interface) {
    usbi_err (HANDLE_CTX (dev_handle), "QueryInterface: %s", darwin_error_str(kresult));
    return darwin_to_libusb (kresult);
  }
  
  /* We no longer need the intermediate plug-in */
  (*plugInInterface)->Release(plugInInterface);

  /* claim the interface */
  kresult = (*(cInterface->interface))->USBInterfaceOpen(cInterface->interface);
  if (kresult) {
    usbi_err (HANDLE_CTX (dev_handle), "USBInterfaceOpen: %s", darwin_error_str(kresult));
    return darwin_to_libusb (kresult);
  }

  /* update list of endpoints */
  kresult = get_endpoints (dev_handle, iface);
  if (kresult) {
    /* this should not happen */
    darwin_release_interface (dev_handle, iface);
    usbi_err (HANDLE_CTX (dev_handle), "could not build endpoint table");
    return kresult;
  }

  cInterface->cfSource = NULL;

  /* create async event source */
  kresult = (*(cInterface->interface))->CreateInterfaceAsyncEventSource (cInterface->interface, &cInterface->cfSource);
  if (kresult != kIOReturnSuccess) {
    usbi_err (HANDLE_CTX (dev_handle), "could not create async event source");

    /* can't continue without an async event source */
    (void)darwin_release_interface (dev_handle, iface);

    return darwin_to_libusb (kresult);
  }

  /* add the cfSource to the async thread's run loop */
  CFRunLoopAddSource(libusb_darwin_acfl, cInterface->cfSource, kCFRunLoopDefaultMode);

  usbi_info (HANDLE_CTX (dev_handle), "interface opened");

  return 0;
}

static int darwin_release_interface(struct libusb_device_handle *dev_handle, int iface) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;
  IOReturn kresult;

  /* current interface */
  struct __darwin_interface *cInterface = &priv->interfaces[iface];

  /* Check to see if an interface is open */
  if (!cInterface->interface)
    return LIBUSB_SUCCESS;

  /* clean up endpoint data */
  cInterface->num_endpoints = 0;
  
  /* delete the interface's async event source */
  if (cInterface->cfSource) {
    CFRunLoopRemoveSource (libusb_darwin_acfl, cInterface->cfSource, kCFRunLoopDefaultMode);
    CFRelease (cInterface->cfSource);
  }

  kresult = (*(cInterface->interface))->USBInterfaceClose(cInterface->interface);
  if (kresult)
    usbi_err (HANDLE_CTX (dev_handle), "USBInterfaceClose: %s", darwin_error_str(kresult));

  kresult = (*(cInterface->interface))->Release(cInterface->interface);
  if (kresult != kIOReturnSuccess)
    usbi_err (HANDLE_CTX (dev_handle), "Release: %s", darwin_error_str(kresult));

  cInterface->interface = IO_OBJECT_NULL;

  return darwin_to_libusb (kresult);
}

static int darwin_set_interface_altsetting(struct libusb_device_handle *dev_handle, int iface, int altsetting) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;
  IOReturn kresult;

  /* current interface */
  struct __darwin_interface *cInterface = &priv->interfaces[iface];

  if (!cInterface->interface)
    return LIBUSB_ERROR_NO_DEVICE;

  kresult = (*(cInterface->interface))->SetAlternateInterface (cInterface->interface, altsetting);
  if (kresult != kIOReturnSuccess)
    darwin_reset_device (dev_handle);

  /* update list of endpoints */
  kresult = get_endpoints (dev_handle, iface);
  if (kresult) {
    /* this should not happen */
    darwin_release_interface (dev_handle, iface);
    usbi_err (HANDLE_CTX (dev_handle), "could not build endpoint table");
    return kresult;
  }

  return darwin_to_libusb (kresult);
}

static int darwin_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint) {
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)dev_handle->os_priv;

  /* current interface */
  struct __darwin_interface *cInterface;
  uint8_t pipeRef, iface;
  IOReturn kresult;

  /* determine the interface/endpoint to use */
  if (ep_to_pipeRef (dev_handle, endpoint, &pipeRef, &iface) != 0) {
    usbi_err (HANDLE_CTX (dev_handle), "endpoint not found on any open interface");

    return LIBUSB_ERROR_NOT_FOUND;
  }

  cInterface = &priv->interfaces[iface];

#if (InterfaceVersion < 190)
  kresult = (*(cInterface->interface))->ClearPipeStall(cInterface->interface, pipeRef);
#else
  /* newer versions of darwin support clearing additional bits on the device's endpoint */
  kresult = (*(cInterface->interface))->ClearPipeStallBothEnds(cInterface->interface, pipeRef);
#endif
  if (kresult)
    usbi_err (HANDLE_CTX (dev_handle), "ClearPipeStall: %s", darwin_error_str (kresult));

  return darwin_to_libusb (kresult);
}

static int darwin_reset_device(struct libusb_device_handle *dev_handle) {
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  IOReturn kresult;

  kresult = (*(dpriv->device))->ResetDevice (dpriv->device);
  if (kresult)
    usbi_err (HANDLE_CTX (dev_handle), "ResetDevice: %s", darwin_error_str (kresult));

  return darwin_to_libusb (kresult);
}

static int darwin_kernel_driver_active(struct libusb_device_handle *dev_handle, int interface) {
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)dev_handle->dev->os_priv;
  io_service_t usbInterface;
  CFTypeRef driver;
  IOReturn kresult;

  kresult = darwin_get_interface (dpriv->device, interface, &usbInterface);
  if (kresult) {
    usbi_err (HANDLE_CTX (dev_handle), "darwin_get_interface: %s", darwin_error_str(kresult));

    return darwin_to_libusb (kresult);
  }
  
  driver = IORegistryEntryCreateCFProperty (usbInterface, kIOBundleIdentifierKey, kCFAllocatorDefault, 0);
  IOObjectRelease (usbInterface);

  if (driver) {
    CFRelease (driver);

    return 1;
  }

  /* no driver */
  return 0;
}

/* attaching/detaching kernel drivers is not currently supported (maybe in the future?) */
static int darwin_attach_kernel_driver (struct libusb_device_handle *dev_handle, int interface) {
  return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int darwin_detach_kernel_driver (struct libusb_device_handle *dev_handle, int interface) {
  return LIBUSB_ERROR_NOT_SUPPORTED;
}

static void darwin_destroy_device(struct libusb_device *dev) {
}

static int submit_bulk_transfer(struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)transfer->dev_handle->os_priv;

  IOReturn               ret;
  uint8_t                is_read; /* 0 = we're reading, 1 = we're writing */
  uint8_t                transferType;
  /* None of the values below are used in libusb for bulk transfers */
  uint8_t                direction, number, interval, pipeRef, iface;
  uint16_t               maxPacketSize;

  struct __darwin_interface *cInterface;

  /* are we reading or writing? */
  is_read = transfer->endpoint & LIBUSB_ENDPOINT_IN;
  
  if (ep_to_pipeRef (transfer->dev_handle, transfer->endpoint, &pipeRef, &iface) != 0) {
    usbi_err (TRANSFER_CTX (transfer), "endpoint not found on any open interface");

    return LIBUSB_ERROR_NOT_FOUND;
  }

  cInterface = &priv->interfaces[iface];

  (*(cInterface->interface))->GetPipeProperties (cInterface->interface, pipeRef, &direction, &number,
						 &transferType, &maxPacketSize, &interval);
  
  /* submit the request */
  /* timeouts are unavailable on interrupt endpoints */
  if (transferType == kUSBInterrupt) {
    if (is_read)
      ret = (*(cInterface->interface))->ReadPipeAsync(cInterface->interface, pipeRef, transfer->buffer,
						      transfer->length, darwin_async_io_callback, itransfer);
    else
      ret = (*(cInterface->interface))->WritePipeAsync(cInterface->interface, pipeRef, transfer->buffer,
						       transfer->length, darwin_async_io_callback, itransfer);
  } else {
    if (is_read)
      ret = (*(cInterface->interface))->ReadPipeAsyncTO(cInterface->interface, pipeRef, transfer->buffer,
							transfer->length, transfer->timeout, transfer->timeout,
							darwin_async_io_callback, (void *)itransfer);
    else
      ret = (*(cInterface->interface))->WritePipeAsyncTO(cInterface->interface, pipeRef, transfer->buffer,
							 transfer->length, transfer->timeout, transfer->timeout,
							 darwin_async_io_callback, (void *)itransfer);
  }

  if (ret)
    usbi_err (TRANSFER_CTX (transfer), "bulk transfer failed (dir = %s): %s (code = 0x%08x)", is_read ? "In" : "Out",
	       darwin_error_str(ret), ret);

  return darwin_to_libusb (ret);
}

static int submit_iso_transfer(struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_transfer_priv *tpriv = usbi_transfer_get_os_priv(itransfer);
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)transfer->dev_handle->os_priv;

  IOReturn                kresult;
  uint8_t                 is_read; /* 0 = we're writing, 1 = we're reading */
  uint8_t                 pipeRef, iface;
  UInt64                  frame;
  AbsoluteTime            atTime;
  int                     i;

  struct __darwin_interface *cInterface;

  /* are we reading or writing? */
  is_read = transfer->endpoint & LIBUSB_ENDPOINT_IN;
  
  /* construct an array of IOUSBIsocFrames */
  tpriv->isoc_framelist = (IOUSBIsocFrame*) calloc (transfer->num_iso_packets, sizeof(IOUSBIsocFrame));
  if (!tpriv->isoc_framelist)
    return LIBUSB_ERROR_NO_MEM;

  /* copy the frame list from the libusb descriptor (the structures differ only is member order) */
  for (i = 0 ; i < transfer->num_iso_packets ; i++)
    tpriv->isoc_framelist[i].frReqCount = transfer->iso_packet_desc[i].length;

  /* determine the interface/endpoint to use */
  if (ep_to_pipeRef (transfer->dev_handle, transfer->endpoint, &pipeRef, &iface) != 0) {
    usbi_err (TRANSFER_CTX (transfer), "endpoint not found on any open interface");

    return LIBUSB_ERROR_NOT_FOUND;
  }

  cInterface = &priv->interfaces[iface];

  /* Last but not least we need the bus frame number */
  kresult = (*(cInterface->interface))->GetBusFrameNumber(cInterface->interface, &frame, &atTime);
  if (kresult) {
    usbi_err (TRANSFER_CTX (transfer), "failed to get bus frame number: %d", kresult);
    free(tpriv->isoc_framelist);
    tpriv->isoc_framelist = NULL;

    return darwin_to_libusb (kresult);
  }

  /* schedule for a frame a little in the future */
  frame += 2;
	
  /* submit the request */
  if (is_read)
    kresult = (*(cInterface->interface))->ReadIsochPipeAsync(cInterface->interface, pipeRef, transfer->buffer, frame,
							     transfer->num_iso_packets, tpriv->isoc_framelist, darwin_async_io_callback,
							     itransfer);
  else
    kresult = (*(cInterface->interface))->WriteIsochPipeAsync(cInterface->interface, pipeRef, transfer->buffer, frame,
							      transfer->num_iso_packets, tpriv->isoc_framelist, darwin_async_io_callback,
							      itransfer);

  if (kresult != kIOReturnSuccess) {
    usbi_err (TRANSFER_CTX (transfer), "isochronous transfer failed (dir: %s): %s", is_read ? "In" : "Out",
	       darwin_error_str(kresult));
    free (tpriv->isoc_framelist);
    tpriv->isoc_framelist = NULL;
  }

  return darwin_to_libusb (kresult);
}

static int submit_control_transfer(struct usbi_transfer *itransfer) {  
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct libusb_control_setup *setup = (struct libusb_control_setup *) transfer->buffer;
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)transfer->dev_handle->dev->os_priv;
  struct darwin_transfer_priv *tpriv = usbi_transfer_get_os_priv(itransfer);

  IOReturn               kresult;

  bzero(&tpriv->req, sizeof(tpriv->req));

  /* IOUSBDeviceInterface expects the request in cpu endianess */
  tpriv->req.bmRequestType     = setup->bmRequestType;
  tpriv->req.bRequest          = setup->bRequest;
  /* these values should be in bus order from libusb_fill_control_setup */
  tpriv->req.wValue            = OSSwapLittleToHostInt16 (setup->wValue);
  tpriv->req.wIndex            = OSSwapLittleToHostInt16 (setup->wIndex);
  tpriv->req.wLength           = OSSwapLittleToHostInt16 (setup->wLength);
  /* data is stored after the libusb control block */
  tpriv->req.pData             = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;
  tpriv->req.completionTimeout = transfer->timeout;
  tpriv->req.noDataTimeout     = transfer->timeout;

  /* all transfers in libusb-1.0 are async */
  kresult = (*(dpriv->device))->DeviceRequestAsyncTO(dpriv->device, &(tpriv->req), darwin_async_io_callback, itransfer);

  if (kresult != kIOReturnSuccess)
    usbi_err (TRANSFER_CTX (transfer), "control request failed: %s", darwin_error_str(kresult));
  
  return darwin_to_libusb (kresult);
}

static int darwin_submit_transfer(struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

  switch (transfer->type) {
  case LIBUSB_TRANSFER_TYPE_CONTROL:
    return submit_control_transfer(itransfer);
  case LIBUSB_TRANSFER_TYPE_BULK:
  case LIBUSB_TRANSFER_TYPE_INTERRUPT:
    return submit_bulk_transfer(itransfer);
  case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
    return submit_iso_transfer(itransfer);
  default:
    usbi_err (TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
    return LIBUSB_ERROR_INVALID_PARAM;
  }
}

static int cancel_control_transfer(struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_device_priv *dpriv = (struct darwin_device_priv *)transfer->dev_handle->dev->os_priv;
  IOReturn kresult;

  usbi_info (ITRANSFER_CTX (itransfer), "WARNING: aborting all transactions control pipe");

  kresult = (*(dpriv->device))->USBDeviceAbortPipeZero (dpriv->device);

  return darwin_to_libusb (kresult);
}

static int darwin_abort_transfers (struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)transfer->dev_handle->os_priv;
  struct __darwin_interface *cInterface;
  uint8_t pipeRef, iface;
  IOReturn kresult;

  if (ep_to_pipeRef (transfer->dev_handle, transfer->endpoint, &pipeRef, &iface) != 0) {
    usbi_err (TRANSFER_CTX (transfer), "endpoint not found on any open interface");

    return LIBUSB_ERROR_NOT_FOUND;
  }

  cInterface = &priv->interfaces[iface];

  usbi_info (ITRANSFER_CTX (itransfer), "WARNING: aborting all transactions on interface %d pipe %d", iface, pipeRef);

  /* abort transactions */
  (*(cInterface->interface))->AbortPipe (cInterface->interface, pipeRef);
  
  usbi_info (ITRANSFER_CTX (itransfer), "calling clear pipe stall to clear the data toggle bit");

  /* clear the data toggle bit */
#if (InterfaceVersion < 190)
  kresult = (*(cInterface->interface))->ClearPipeStall(cInterface->interface, pipeRef);
#else
  /* newer versions of darwin support clearing additional bits on the device's endpoint */
  kresult = (*(cInterface->interface))->ClearPipeStallBothEnds(cInterface->interface, pipeRef);
#endif

  return darwin_to_libusb (kresult);
}

static int darwin_cancel_transfer(struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

  switch (transfer->type) {
  case LIBUSB_TRANSFER_TYPE_CONTROL:
    return cancel_control_transfer(itransfer);
  case LIBUSB_TRANSFER_TYPE_BULK:
  case LIBUSB_TRANSFER_TYPE_INTERRUPT:
  case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
    return darwin_abort_transfers (itransfer);
  default:
    usbi_err (TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
    return LIBUSB_ERROR_INVALID_PARAM;
  }
}

static void darwin_clear_transfer_priv (struct usbi_transfer *itransfer) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_transfer_priv *tpriv = usbi_transfer_get_os_priv(itransfer);

  if (transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS && tpriv->isoc_framelist) {
    free (tpriv->isoc_framelist);
    tpriv->isoc_framelist = NULL;
  }
}

static void darwin_async_io_callback (void *refcon, IOReturn result, void *arg0) {
  struct usbi_transfer *itransfer = (struct usbi_transfer *)refcon;
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)transfer->dev_handle->os_priv;
  UInt32 message;

  usbi_info (ITRANSFER_CTX (itransfer), "an async io operation has completed");

  /* send a completion message to the device's file descriptor */
  message = MESSAGE_ASYNC_IO_COMPLETE;
  write (priv->fds[1], &message, sizeof (message));
  write (priv->fds[1], &itransfer, sizeof (itransfer));
  write (priv->fds[1], &result, sizeof (IOReturn));
  write (priv->fds[1], &arg0, sizeof (UInt32));
}

static int darwin_transfer_status (struct usbi_transfer *itransfer, kern_return_t result) {
  switch (result) {
  case kIOReturnSuccess:
    return LIBUSB_TRANSFER_COMPLETED;
  case kIOReturnAborted:
    return LIBUSB_TRANSFER_CANCELLED;
  case kIOUSBPipeStalled:
    usbi_warn (ITRANSFER_CTX (itransfer), "transfer error: pipe is stalled");
    return LIBUSB_TRANSFER_STALL;
  case kIOReturnOverrun:
    usbi_err (ITRANSFER_CTX (itransfer), "transfer error: data overrun", darwin_error_str (result));
    return LIBUSB_TRANSFER_OVERFLOW;
  case kIOUSBTransactionTimeout:
    usbi_err (ITRANSFER_CTX (itransfer), "transfer error: timed out");
    return LIBUSB_TRANSFER_TIMED_OUT;
  default:
    usbi_err (ITRANSFER_CTX (itransfer), "transfer error: %s (value = 0x%08x)", darwin_error_str (result), result);
    return LIBUSB_TRANSFER_ERROR;
  }
}

static void darwin_handle_callback (struct usbi_transfer *itransfer, kern_return_t result, UInt32 io_size) {
  struct libusb_transfer *transfer = __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
  struct darwin_transfer_priv *tpriv = usbi_transfer_get_os_priv(itransfer);
  int isIsoc      = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS == transfer->type;
  int isBulk      = LIBUSB_TRANSFER_TYPE_BULK == transfer->type;
  int isControl   = LIBUSB_TRANSFER_TYPE_CONTROL == transfer->type;
  int isInterrupt = LIBUSB_TRANSFER_TYPE_INTERRUPT == transfer->type;
  int i;

  if (!isIsoc && !isBulk && !isControl && !isInterrupt) {
    usbi_err (TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
    return;
  }

  usbi_info (ITRANSFER_CTX (itransfer), "handling %s completion with kernel status %d",
	     isControl ? "control" : isBulk ? "bulk" : isIsoc ? "isoc" : "interrupt", result);

  if (kIOReturnSuccess == result) {
    if (isIsoc && tpriv->isoc_framelist) {
      /* copy isochronous results back */

      for (i = 0; i < transfer->num_iso_packets ; i++) {
	struct libusb_iso_packet_descriptor *lib_desc = transfer->iso_packet_desc;
	lib_desc->status = darwin_to_libusb (tpriv->isoc_framelist[i].frStatus);
	lib_desc->actual_length = tpriv->isoc_framelist[i].frActCount;
      }
    } else if (!isIsoc)
      itransfer->transferred += io_size;
  }

  /* it is ok to handle cancelled transfers without calling usbi_handle_transfer_cancellation (we catch timeout transfers) */
  usbi_handle_transfer_completion (itransfer, darwin_transfer_status (itransfer, result));
}

static int op_handle_events(struct libusb_context *ctx, struct pollfd *fds, nfds_t nfds, int num_ready) {
  struct usbi_transfer *itransfer;
  UInt32 io_size;
  IOReturn kresult;
  int i = 0, ret;
  UInt32 message;

  pthread_mutex_lock(&ctx->open_devs_lock);
  for (i = 0; i < nfds && num_ready > 0; i++) {
    struct pollfd *pollfd = &fds[i];
    struct libusb_device_handle *handle;
    struct darwin_device_handle_priv *hpriv = NULL;

    usbi_info (ctx, "checking fd %i with revents = %x", fds[i], pollfd->revents);

    if (!pollfd->revents)
      continue;

    num_ready--;
    list_for_each_entry(handle, &ctx->open_devs, list) {
      hpriv =  (struct darwin_device_handle_priv *)handle->os_priv;
      if (hpriv->fds[0] == pollfd->fd)
	break;
    }

    if (!(pollfd->revents & POLLERR)) {
      ret = read (hpriv->fds[0], &message, sizeof (message));
      if (ret < sizeof (message))
	continue;
    } else
      /* could not poll the device-- response is to delete the device (this seems a little heavy-handed) */
      message = MESSAGE_DEVICE_GONE;

    switch (message) {
    case MESSAGE_DEVICE_GONE:
      /* remove the device's async port from the runloop */
      if (hpriv->cfSource) {
	if (libusb_darwin_acfl)
	  CFRunLoopRemoveSource (libusb_darwin_acfl, hpriv->cfSource, kCFRunLoopDefaultMode);
	CFRelease (hpriv->cfSource);
	hpriv->cfSource = NULL;
      }

      usbi_remove_pollfd(HANDLE_CTX(handle), hpriv->fds[0]);
      usbi_handle_disconnect(handle);
      
      /* done with this device */
      continue;
    case MESSAGE_ASYNC_IO_COMPLETE:
      read (hpriv->fds[0], &itransfer, sizeof (itransfer));
      read (hpriv->fds[0], &kresult, sizeof (IOReturn));
      read (hpriv->fds[0], &io_size, sizeof (UInt32));

      darwin_handle_callback (itransfer, kresult, io_size);
      break;
    default:
      usbi_err (ctx, "unknown message received from device pipe");
    }
  }

  pthread_mutex_unlock(&ctx->open_devs_lock);

  return 0;
}

static int darwin_clock_gettime(int clk_id, struct timespec *tp) {
  mach_timespec_t sys_time;
  clock_serv_t clock_ref;

  switch (clk_id) {
  case USBI_CLOCK_REALTIME:
    /* CLOCK_REALTIME represents time since the epoch */
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock_ref);
    break;
  case USBI_CLOCK_MONOTONIC:
    /* use system boot time as reference for the monotonic clock */
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_ref);
    break;
  default:
    return LIBUSB_ERROR_INVALID_PARAM;
  }

  clock_get_time (clock_ref, &sys_time);

  tp->tv_sec  = sys_time.tv_sec;
  tp->tv_nsec = sys_time.tv_nsec;

  return 0;
}

const struct usbi_os_backend darwin_backend = {
	.name = "Darwin",
	.init = darwin_init,
	.exit = darwin_exit,
	.get_device_list = darwin_get_device_list,
	.get_device_descriptor = darwin_get_device_descriptor,
	.get_active_config_descriptor = darwin_get_active_config_descriptor,
	.get_config_descriptor = darwin_get_config_descriptor,

	.open = darwin_open,
	.close = darwin_close,
	.get_configuration = darwin_get_configuration,
	.set_configuration = darwin_set_configuration,
	.claim_interface = darwin_claim_interface,
	.release_interface = darwin_release_interface,

	.set_interface_altsetting = darwin_set_interface_altsetting,
	.clear_halt = darwin_clear_halt,
	.reset_device = darwin_reset_device,

	.kernel_driver_active = darwin_kernel_driver_active,
	.detach_kernel_driver = darwin_detach_kernel_driver,
	.attach_kernel_driver = darwin_attach_kernel_driver,

	.destroy_device = darwin_destroy_device,

	.submit_transfer = darwin_submit_transfer,
	.cancel_transfer = darwin_cancel_transfer,
	.clear_transfer_priv = darwin_clear_transfer_priv,

	.handle_events = op_handle_events,

	.clock_gettime = darwin_clock_gettime,

	.device_priv_size = sizeof(struct darwin_device_priv),
	.device_handle_priv_size = sizeof(struct darwin_device_handle_priv),
	.transfer_priv_size = sizeof(struct darwin_transfer_priv),
	.add_iso_packet_size = 0,
};

