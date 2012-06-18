/*
 * Darwin/MacOS X Support
 *
 * (c) 2002-2005 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *
 * (04/17/2005):
 *   - Lots of minor fixes.
 *   - Endpoint table now made by claim_interface to fix a bug.
 *   - Merged Read/Write to make modifications easier.
 * (03/25/2005):
 *   - Fixed a bug when using asynchronous callbacks within a multi-threaded application.
 * (03/14/2005):
 *   - Added an endpoint table to speed up bulk transfers.
 * 0.1.11 (02/22/2005):
 *   - Updated error checking in read/write routines to check completion codes.
 *   - Updated set_configuration so that the open interface is reclaimed before completion.
 *   - Fixed several typos.
 * 0.1.8 (01/12/2004):
 *   - Fixed several memory leaks.
 *   - Readded 10.0 support
 *   - Added support for USB fuctions defined in 10.3 and above
 * (01/02/2003):
 *   - Applied a patch by Philip Edelbrock <phil@edgedesign.us> that fixes a bug in usb_control_msg.
 * (12/16/2003):
 *   - Even better error printing.
 *   - Devices that cannot be opened can have their interfaces opened.
 * 0.1.6 (05/12/2002):
 *   - Fixed problem where libusb holds resources after program completion.
 *   - Mouse should no longer freeze up now.
 * 0.1.2 (02/13/2002):
 *   - Bulk functions should work properly now.
 * 0.1.1 (02/11/2002):
 *   - Fixed major bug (device and interface need to be released after use)
 * 0.1.0 (01/06/2002):
 *   - Tested driver with gphoto (works great as long as Image Capture isn't running)
 * 0.1d  (01/04/2002):
 *   - Implimented clear_halt and resetep
 *   - Uploaded to CVS.
 * 0.1b  (01/04/2002):
 *   - Added usb_debug line to bulk read and write function.
 * 0.1a  (01/03/2002):
 *   - Driver mostly completed using the macosx driver I wrote for my rioutil software.
 *
 * Derived from Linux version by Richard Tobin.
 * Also partly derived from BSD version.
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* standard includes for darwin/os10 (IOKit) */
#include <mach/mach_port.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

#include "usbi.h"

/* some defines */
/* IOUSBInterfaceInferface */
#if defined (kIOUSBInterfaceInterfaceID220)

// #warning "libusb being compiled for 10.4 or later"
#define usb_interface_t IOUSBInterfaceInterface220
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID220
#define InterfaceVersion 220

#elif defined (kIOUSBInterfaceInterfaceID197)

// #warning "libusb being compiled for 10.3 or later"
#define usb_interface_t IOUSBInterfaceInterface197
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID197
#define InterfaceVersion 197

#elif defined (kIOUSBInterfaceInterfaceID190)

// #warning "libusb being compiled for 10.2 or later"
#define usb_interface_t IOUSBInterfaceInterface190
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID190
#define InterfaceVersion 190

#elif defined (kIOUSBInterfaceInterfaceID182)

// #warning "libusb being compiled for 10.1 or later"
#define usb_interface_t IOUSBInterfaceInterface182
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID182
#define InterfaceVersion 182

#else

/* No timeout functions available! Time to upgrade your os. */
#warning "libusb being compiled without support for timeout bulk functions! 10.0 and up"
#define usb_interface_t IOUSBInterfaceInterface
#define InterfaceInterfaceID kIOUSBInterfaceInterfaceID
#define LIBUSB_NO_TIMEOUT_INTERFACE
#define InterfaceVersion 180

#endif

/* IOUSBDeviceInterface */
#if defined (kIOUSBDeviceInterfaceID197)

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

#define usb_device_t    IOUSBDeviceInterface
#define DeviceInterfaceID kIOUSBDeviceInterfaceID
#define LIBUSB_NO_TIMEOUT_DEVICE
#define LIBUSB_NO_SEIZE_DEVICE
#define DeviceVersion 180

#endif

typedef IOReturn io_return_t;
typedef IOCFPlugInInterface *io_cf_plugin_ref_t;
typedef SInt32 s_int32_t;
typedef IOReturn (*rw_async_func_t)(void *self, UInt8 pipeRef, void *buf, UInt32 size,
				    IOAsyncCallback1 callback, void *refcon);
typedef IOReturn (*rw_async_to_func_t)(void *self, UInt8 pipeRef, void *buf, UInt32 size,
				       UInt32 noDataTimeout, UInt32 completionTimeout,
				       IOAsyncCallback1 callback, void *refcon);

#if !defined(IO_OBJECT_NULL)
#define IO_OBJECT_NULL ((io_object_t)0)
#endif

/* Darwin/OS X impl does not use fd field, instead it uses this */
struct darwin_dev_handle {
  usb_device_t **device;
  usb_interface_t **interface;
  int open;

  /* stored translation table for pipes to endpoints */
  int num_endpoints;
  unsigned char *endpoint_addrs;
};

static CFMutableDictionaryRef matchingDict;
static IONotificationPortRef gNotifyPort;
static mach_port_t masterPort = MACH_PORT_NULL;

static void darwin_cleanup (void)
{
  IONotificationPortDestroy(gNotifyPort);
  mach_port_deallocate(mach_task_self(), masterPort);
}

static char *darwin_error_str (int result) {
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
  default:
    return "unknown error";
  }
}

/* not a valid errorno outside darwin.c */
#define LUSBDARWINSTALL (ELAST+1)

static int darwin_to_errno (int result) {
  switch (result) {
  case kIOReturnSuccess:
    return 0;
  case kIOReturnNotOpen:
    return EBADF;
  case kIOReturnNoDevice:
  case kIOUSBNoAsyncPortErr:
    return ENXIO;
  case kIOReturnExclusiveAccess:
    return EBUSY;
  case kIOUSBPipeStalled:
    return LUSBDARWINSTALL;
  case kIOReturnBadArgument:
    return EINVAL;
  case kIOUSBTransactionTimeout:
    return ETIMEDOUT;
  case kIOReturnError:
  default:
    return 1;
  }
}

static int usb_setup_iterator (io_iterator_t *deviceIterator)
{
  int result;

  /* set up the matching dictionary for class IOUSBDevice and its subclasses.
     It will be consumed by the IOServiceGetMatchingServices call */
  if ((matchingDict = IOServiceMatching(kIOUSBDeviceClassName)) == NULL) {
    darwin_cleanup ();
    
    USB_ERROR_STR(-1, "libusb/darwin.c usb_setup_iterator: Could not create a matching dictionary.\n");
  }

  result = IOServiceGetMatchingServices(masterPort, matchingDict, deviceIterator);
  matchingDict = NULL;

  if (result != kIOReturnSuccess)
    USB_ERROR_STR (-darwin_to_errno (result), "libusb/darwin.c usb_setup_iterator: IOServiceGetMatchingServices: %s\n",
		   darwin_error_str(result));

  return 0;
}

static usb_device_t **usb_get_next_device (io_iterator_t deviceIterator, UInt32 *locationp)
{
  io_cf_plugin_ref_t *plugInInterface = NULL;
  usb_device_t **device;
  io_service_t usbDevice;
  long result, score;

  if (!IOIteratorIsValid (deviceIterator) || !(usbDevice = IOIteratorNext(deviceIterator)))
    return NULL;
  
  result = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID,
					     kIOCFPlugInInterfaceID, &plugInInterface,
					     &score);
  
  result = IOObjectRelease(usbDevice);
  if (result || !plugInInterface)
    return NULL;
  
  (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(DeviceInterfaceID),
				     (LPVOID)&device);
  
  (*plugInInterface)->Stop(plugInInterface);
  IODestroyPlugInInterface (plugInInterface);
  plugInInterface = NULL;
  
  (*(device))->GetLocationID(device, locationp);

  return device;
}

int usb_os_open(usb_dev_handle *dev)
{
  struct darwin_dev_handle *device;

  io_return_t result;
  io_iterator_t deviceIterator;

  usb_device_t **darwin_device;

  UInt32 location = *((UInt32 *)dev->device->dev);
  UInt32 dlocation;

  if (!dev)
    USB_ERROR(-ENXIO);

  if (masterPort == MACH_PORT_NULL)
    USB_ERROR(-EINVAL);

  device = calloc(1, sizeof(struct darwin_dev_handle));
  if (!device)
    USB_ERROR(-ENOMEM);

  if (usb_debug > 3)
    fprintf(stderr, "usb_os_open: %04x:%04x\n",
	    dev->device->descriptor.idVendor,
	    dev->device->descriptor.idProduct);

  if ((result = usb_setup_iterator (&deviceIterator)) < 0)
    return result;

  /* This port of libusb uses locations to keep track of devices. */
  while ((darwin_device = usb_get_next_device (deviceIterator, &dlocation)) != NULL) {
    if (dlocation == location)
      break;

    (*darwin_device)->Release(darwin_device);
  }

  IOObjectRelease(deviceIterator);
  device->device = darwin_device;

  if (device->device == NULL)
    USB_ERROR_STR (-ENOENT, "usb_os_open: %s\n", "Device not found!");

#if !defined (LIBUSB_NO_SEIZE_DEVICE)
  result = (*(device->device))->USBDeviceOpenSeize (device->device);
#else
  /* No Seize in OS X 10.0 (Darwin 1.4) */
  result = (*(device->device))->USBDeviceOpen (device->device);
#endif

  if (result != kIOReturnSuccess) {
    switch (result) {
    case kIOReturnExclusiveAccess:
      if (usb_debug > 0)
	fprintf (stderr, "usb_os_open(USBDeviceOpenSeize): %s\n", darwin_error_str(result));
      break;
    default:
      (*(device->device))->Release (device->device);
      USB_ERROR_STR(-darwin_to_errno (result), "usb_os_open(USBDeviceOpenSeize): %s",
		    darwin_error_str(result));
    }
    
    device->open = 0;
  } else
    device->open = 1;
    
  dev->impl_info = device;
  dev->interface = -1;
  dev->altsetting = -1;

  device->num_endpoints  = 0;
  device->endpoint_addrs = NULL;

  return 0;
}

int usb_os_close(usb_dev_handle *dev)
{
  struct darwin_dev_handle *device;
  io_return_t result;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  usb_release_interface(dev, dev->interface);

  if (usb_debug > 3)
    fprintf(stderr, "usb_os_close: %04x:%04x\n",
	    dev->device->descriptor.idVendor,
	    dev->device->descriptor.idProduct);

  if (device->open == 1)
    result = (*(device->device))->USBDeviceClose(device->device);
  else
    result = kIOReturnSuccess;

  /* device may not need to be released, but if it has to... */
  (*(device->device))->Release(device->device);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_os_close(USBDeviceClose): %s", darwin_error_str(result));

  free (device);

  return 0;
}

static int get_endpoints (struct darwin_dev_handle *device)
{
  io_return_t ret;

  u_int8_t numep, direction, number;
  u_int8_t dont_care1, dont_care3;
  u_int16_t dont_care2;

  int i;

  if (device == NULL || device->interface == NULL)
    return -EINVAL;

  if (usb_debug > 1)
    fprintf(stderr, "libusb/darwin.c get_endpoints: building table of endpoints.\n");

  /* retrieve the total number of endpoints on this interface */
  ret = (*(device->interface))->GetNumEndpoints(device->interface, &numep);
  if ( ret ) {
    if ( usb_debug > 1 )
      fprintf ( stderr, "get_endpoints: interface is %p\n", device->interface );

    USB_ERROR_STR ( -ret, "get_endpoints: can't get number of endpoints for interface" );
  }

  free (device->endpoint_addrs);
  device->endpoint_addrs = calloc (sizeof (unsigned char), numep);

  /* iterate through pipe references */
  for (i = 1 ; i <= numep ; i++) {
    ret = (*(device->interface))->GetPipeProperties(device->interface, i, &direction, &number,
						    &dont_care1, &dont_care2, &dont_care3);

    if (ret != kIOReturnSuccess) {
      fprintf (stderr, "get_endpoints: an error occurred getting pipe information on pipe %d\n",
	       i );
      USB_ERROR_STR(-darwin_to_errno(ret), "get_endpoints(GetPipeProperties): %s", darwin_error_str(ret));
    }

    if (usb_debug > 1)
      fprintf (stderr, "get_endpoints: Pipe %i: DIR: %i number: %i\n", i, direction, number);

    device->endpoint_addrs[i - 1] = ((direction << 7 & USB_ENDPOINT_DIR_MASK) |
				     (number & USB_ENDPOINT_ADDRESS_MASK));
  }

  device->num_endpoints = numep;

  if (usb_debug > 1)
    fprintf(stderr, "libusb/darwin.c get_endpoints: complete.\n");
  
  return 0;
}

static int claim_interface (usb_dev_handle *dev, int interface)
{
  io_iterator_t interface_iterator;
  io_service_t  usbInterface = IO_OBJECT_NULL;
  io_return_t result;
  io_cf_plugin_ref_t *plugInInterface = NULL;

  IOUSBFindInterfaceRequest request;

  struct darwin_dev_handle *device;
  long score;
  int current_interface;

  device = dev->impl_info;

  request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

  result = (*(device->device))->CreateInterfaceIterator(device->device, &request, &interface_iterator);
  if (result != kIOReturnSuccess)
    USB_ERROR_STR (-darwin_to_errno(result), "claim_interface(CreateInterfaceIterator): %s",
		   darwin_error_str(result));

  for ( current_interface=0 ; current_interface <= interface ; current_interface++ ) {
    usbInterface = IOIteratorNext(interface_iterator);
    if ( usb_debug > 3 )
      fprintf ( stderr, "Interface %d of device is 0x%08x\n",
		current_interface, usbInterface );
  }

  current_interface--;

  /* the interface iterator is no longer needed, release it */
  IOObjectRelease(interface_iterator);

  if (!usbInterface) {
    u_int8_t nConfig;			     /* Index of configuration to use */
    IOUSBConfigurationDescriptorPtr configDesc; /* to describe which configuration to select */
    /* Only a composite class device with no vendor-specific driver will
       be configured. Otherwise, we need to do it ourselves, or there
       will be no interfaces for the device. */

    if ( usb_debug > 3 )
      fprintf ( stderr,"claim_interface: No interface found; selecting configuration\n" );

    result = (*(device->device))->GetNumberOfConfigurations ( device->device, &nConfig );
    if (result != kIOReturnSuccess)
      USB_ERROR_STR(-darwin_to_errno(result), "claim_interface(GetNumberOfConfigurations): %s",
		    darwin_error_str(result));
    
    if (nConfig < 1)
      USB_ERROR_STR(-ENXIO ,"claim_interface(GetNumberOfConfigurations): no configurations");
    else if ( nConfig > 1 && usb_debug > 0 )
      fprintf ( stderr, "claim_interface: device has more than one"
		" configuration, using the first (warning)\n" );

    if ( usb_debug > 3 )
      fprintf ( stderr, "claim_interface: device has %d configuration%s\n",
		(int)nConfig, (nConfig>1?"s":"") );

    /* Always use the first configuration */
    result = (*(device->device))->GetConfigurationDescriptorPtr ( (device->device), 0, &configDesc );
    if (result != kIOReturnSuccess) {
      if (device->open == 1) {
        (*(device->device))->USBDeviceClose ( (device->device) );
        (*(device->device))->Release ( (device->device) );
      }

      USB_ERROR_STR(-darwin_to_errno(result), "claim_interface(GetConfigurationDescriptorPtr): %s",
		    darwin_error_str(result));
    } else if ( usb_debug > 3 )
      fprintf ( stderr, "claim_interface: configuration value is %d\n",
		configDesc->bConfigurationValue );

    if (device->open == 1) {
      result = (*(device->device))->SetConfiguration ( (device->device), configDesc->bConfigurationValue );

      if (result != kIOReturnSuccess) {
	(*(device->device))->USBDeviceClose ( (device->device) );
	(*(device->device))->Release ( (device->device) );

	USB_ERROR_STR(-darwin_to_errno(result), "claim_interface(SetConfiguration): %s",
		      darwin_error_str(result));
      }

      dev->config = configDesc->bConfigurationValue;
    }
    
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

    /* Now go back and get the chosen interface */
    result = (*(device->device))->CreateInterfaceIterator(device->device, &request, &interface_iterator);
    if (result != kIOReturnSuccess)
      USB_ERROR_STR (-darwin_to_errno(result), "claim_interface(CreateInterfaceIterator): %s",
		     darwin_error_str(result));

    for (current_interface = 0 ; current_interface <= interface ; current_interface++) {
      usbInterface = IOIteratorNext(interface_iterator);

      if ( usb_debug > 3 )
	fprintf ( stderr, "claim_interface: Interface %d of device is 0x%08x\n",
		  current_interface, usbInterface );
    }
    current_interface--;

    /* the interface iterator is no longer needed, release it */
    IOObjectRelease(interface_iterator);

    if (!usbInterface)
      USB_ERROR_STR (-ENOENT, "claim_interface: interface iterator returned NULL");
  }

  result = IOCreatePlugInInterfaceForService(usbInterface,
					     kIOUSBInterfaceUserClientTypeID,
					     kIOCFPlugInInterfaceID,
					     &plugInInterface, &score);
  /* No longer need the usbInterface object after getting the plug-in */
  result = IOObjectRelease(usbInterface);
  if (result || !plugInInterface)
    USB_ERROR(-ENOENT);

  /* Now create the device interface for the interface */
  result = (*plugInInterface)->QueryInterface(plugInInterface,
					      CFUUIDGetUUIDBytes(InterfaceInterfaceID),
					      (LPVOID) &device->interface);

  /* No longer need the intermediate plug-in */
  (*plugInInterface)->Stop(plugInInterface);
  IODestroyPlugInInterface (plugInInterface);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "claim_interface(QueryInterface): %s",
		  darwin_error_str(result));

  if (!device->interface)
    USB_ERROR(-EACCES);

  if ( usb_debug > 3 )
    fprintf ( stderr, "claim_interface: Interface %d of device from QueryInterface is %p\n",
	      current_interface, device->interface);

  /* claim the interface */
  result = (*(device->interface))->USBInterfaceOpen(device->interface);
  if (result)
    USB_ERROR_STR(-darwin_to_errno(result), "claim_interface(USBInterfaceOpen): %s",
		  darwin_error_str(result));

  result = get_endpoints (device);

  if (result) {
    /* this should not happen */
    usb_release_interface (dev, interface);
    USB_ERROR_STR ( result, "claim_interface: could not build endpoint table");
  }

  return 0;
}

int usb_set_configuration (usb_dev_handle *dev, int configuration)
{
  struct darwin_dev_handle *device;
  io_return_t result;
  int interface;

  if ( usb_debug > 3 )
    fprintf ( stderr, "usb_set_configuration: called for config %x\n", configuration );

  if (!dev)
    USB_ERROR_STR ( -ENXIO, "usb_set_configuration: called with null device\n" );

  if ((device = dev->impl_info) == NULL)
    USB_ERROR_STR ( -ENOENT, "usb_set_configuration: device not properly initialized" );

  /* Setting configuration will invalidate the interface, so we need
     to reclaim it. First, dispose of existing interface, if any. */
  interface = dev->interface;

  if ( device->interface )
    usb_release_interface(dev, dev->interface);

  result = (*(device->device))->SetConfiguration(device->device, configuration);

  if (result)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_set_configuration(SetConfiguration): %s",
		  darwin_error_str(result));

  /* Reclaim interface */
  if (interface != -1)
    result = usb_claim_interface (dev, interface);

  dev->config = configuration;

  return result;
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
  struct darwin_dev_handle *device = dev->impl_info;

  io_return_t result;

  if ( usb_debug > 3 )
    fprintf ( stderr, "usb_claim_interface: called for interface %d\n", interface );

  if (!device)
    USB_ERROR_STR ( -ENOENT, "usb_claim_interface: device is NULL" );

  if (!(device->device))
    USB_ERROR_STR ( -EINVAL, "usb_claim_interface: device->device is NULL" );

  /* If we have already claimed an interface, release it */
  if ( device->interface )
    usb_release_interface(dev, dev->interface);

  result = claim_interface ( dev, interface );
  if ( result )
    USB_ERROR_STR ( result, "usb_claim_interface: couldn't claim interface" );

  dev->interface = interface;

  /* interface is claimed and async IO is set up: return 0 */
  return 0;
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
  struct darwin_dev_handle *device;
  io_return_t result;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  /* interface is not open */
  if (!device->interface)
    return 0;

  result = (*(device->interface))->USBInterfaceClose(device->interface);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_release_interface(USBInterfaceClose): %s",
		  darwin_error_str(result));

  result = (*(device->interface))->Release(device->interface);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_release_interface(Release): %s",
		  darwin_error_str(result));

  device->interface = NULL;

  free (device->endpoint_addrs);

  device->num_endpoints  = 0;
  device->endpoint_addrs = NULL;

  dev->interface = -1;
  dev->altsetting = -1;

  return 0;
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
  struct darwin_dev_handle *device;
  io_return_t result;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  /* interface is not open */
  if (!device->interface)
    USB_ERROR_STR(-EACCES, "usb_set_altinterface: interface used without being claimed");

  result = (*(device->interface))->SetAlternateInterface(device->interface, alternate);

  if (result)
    USB_ERROR_STR(result, "usb_set_altinterface: could not set alternate interface");

  dev->altsetting = alternate;

  result = get_endpoints (device);
  if (result) {
    /* this should not happen */
    USB_ERROR_STR ( result, "usb_set_altinterface: could not build endpoint table");
  }

  return 0;
}

/* simple function that figures out what pipeRef is associated with an endpoint */
static int ep_to_pipeRef (struct darwin_dev_handle *device, int ep)
{
  int i;

  if (usb_debug > 1)
    fprintf(stderr, "libusb/darwin.c ep_to_pipeRef: Converting ep address to pipeRef.\n");

  for (i = 0 ; i < device->num_endpoints ; i++)
    if (device->endpoint_addrs[i] == ep)
      return i + 1;

  /* No pipe found with the correct endpoint address */
  if (usb_debug > 1)
    fprintf(stderr, "libusb/darwin.c ep_to_pipeRef: No pipeRef found with endpoint address 0x%02x.\n", ep);
  
  return -1;
}

/* argument to handle multiple parameters to rw_completed */
struct rw_complete_arg {
  UInt32        io_size;
  IOReturn      result;
  CFRunLoopRef  cf_loop;
};

static void rw_completed(void *refcon, io_return_t result, void *io_size)
{
  struct rw_complete_arg *rw_arg = (struct rw_complete_arg *)refcon;

  if (usb_debug > 2)
    fprintf(stderr, "io async operation completed: %s, size=%lu, result=0x%08x\n", darwin_error_str(result),
	    (UInt32)io_size, result);

  rw_arg->io_size = (UInt32)io_size;
  rw_arg->result  = result;

  CFRunLoopStop(rw_arg->cf_loop);
}

static int usb_bulk_transfer (usb_dev_handle *dev, int ep, char *bytes, int size, int timeout,
			      rw_async_func_t rw_async, rw_async_to_func_t rw_async_to)
{
  struct darwin_dev_handle *device;

  io_return_t result = -1;

  CFRunLoopSourceRef cfSource;
  int pipeRef;

  struct rw_complete_arg rw_arg;

  u_int8_t  transferType;

  /* None of the values below are used in libusb for bulk transfers */
  u_int8_t  direction, number, interval;
  u_int16_t maxPacketSize;

  if (!dev)
    USB_ERROR_STR ( -ENXIO, "usb_bulk_transfer: Called with NULL device" );

  if ((device = dev->impl_info) == NULL)
    USB_ERROR_STR ( -ENOENT, "usb_bulk_transfer: Device not open" );

  /* interface is not open */
  if (!device->interface)
    USB_ERROR_STR(-EACCES, "usb_bulk_transfer: Interface used before it was opened");


  /* Set up transfer */
  if ((pipeRef = ep_to_pipeRef(device, ep)) < 0)
    USB_ERROR_STR ( -EINVAL, "usb_bulk_transfer: Invalid pipe reference" );

  (*(device->interface))->GetPipeProperties (device->interface, pipeRef, &direction, &number,
					     &transferType, &maxPacketSize, &interval);

  (*(device->interface))->CreateInterfaceAsyncEventSource(device->interface, &cfSource);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);

  bzero((void *)&rw_arg, sizeof(struct rw_complete_arg));
  rw_arg.cf_loop = CFRunLoopGetCurrent();
  /* Transfer set up complete */

  if (usb_debug > 0)
    fprintf (stderr, "libusb/darwin.c usb_bulk_transfer: Transfering %i bytes of data on endpoint 0x%02x\n",
	     size, ep);

  /* Bulk transfer */
  if (transferType == kUSBInterrupt && usb_debug > 3)
    fprintf (stderr, "libusb/darwin.c usb_bulk_transfer: USB pipe is an interrupt pipe. Timeouts will not be used.\n");

  if ( transferType != kUSBInterrupt && rw_async_to != NULL)

    result = rw_async_to (device->interface, pipeRef, bytes, size, timeout, timeout,
			  (IOAsyncCallback1)rw_completed, (void *)&rw_arg);
  else
    result = rw_async (device->interface, pipeRef, bytes, size, (IOAsyncCallback1)rw_completed,
		       (void *)&rw_arg);

  if (result == kIOReturnSuccess) {
    /* wait for write to complete */
    if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, (timeout+999)/1000, true) == kCFRunLoopRunTimedOut) {
      (*(device->interface))->AbortPipe(device->interface, pipeRef);
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true); /* Pick up aborted callback */
      if (usb_debug)
	fprintf(stderr, "usb_bulk_read: input timed out\n");
    }
  }

  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
  
  /* Check the return code of both the write and completion functions. */
  if (result != kIOReturnSuccess || (rw_arg.result != kIOReturnSuccess && 
      rw_arg.result != kIOReturnAborted) ) {
    int error_code;
    char *error_str;

    if (result == kIOReturnSuccess) {
      error_code = darwin_to_errno (rw_arg.result);
      error_str  = darwin_error_str (rw_arg.result);
    } else {
      error_code = darwin_to_errno(result);
      error_str  = darwin_error_str (result);
    }
    
    if (transferType != kUSBInterrupt && rw_async_to != NULL)
      USB_ERROR_STR(-error_code, "usb_bulk_transfer (w/ Timeout): %s", error_str);
    else
      USB_ERROR_STR(-error_code, "usb_bulk_transfer (No Timeout): %s", error_str);
  }

  return rw_arg.io_size;
}

int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size, int timeout)
{
  int result;
  rw_async_to_func_t to_func = NULL;
  struct darwin_dev_handle *device;
  
  if (dev == NULL || dev->impl_info == NULL)
    return -EINVAL;

  device = dev->impl_info;

#if !defined (LIBUSB_NO_TIMEOUT_INTERFACE)
  to_func = (*(device->interface))->WritePipeAsyncTO;
#endif

  if ((result = usb_bulk_transfer (dev, ep, bytes, size, timeout,
				   (*(device->interface))->WritePipeAsync, to_func)) < 0)
    USB_ERROR_STR (result, "usb_bulk_write: An error occured during write (see messages above)");
  
  return result;
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size, int timeout)
{
  int result;
  rw_async_to_func_t to_func = NULL;
  struct darwin_dev_handle *device;
  
  if (dev == NULL || dev->impl_info == NULL)
    return -EINVAL;

  ep |= 0x80;
  
  device = dev->impl_info;

#if !defined (LIBUSB_NO_TIMEOUT_INTERFACE)
  to_func = (*(device->interface))->ReadPipeAsyncTO;
#endif

  if ((result = usb_bulk_transfer (dev, ep, bytes, size, timeout,
				   (*(device->interface))->ReadPipeAsync, to_func)) < 0)
    USB_ERROR_STR (result, "usb_bulk_read: An error occured during read (see messages above)");
  
  return result;
}

/* interrupt endpoints seem to be treated just like any other endpoint under OSX/Darwin */
int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  return usb_bulk_write (dev, ep, bytes, size, timeout);
}

int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  return usb_bulk_read (dev, ep, bytes, size, timeout);
}

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
		    int value, int index, char *bytes, int size, int timeout)
{
  struct darwin_dev_handle *device = dev->impl_info;

  io_return_t result;

#if !defined (LIBUSB_NO_TIMEOUT_DEVICE)
  IOUSBDevRequestTO urequest;
#else
  IOUSBDevRequest urequest;
#endif

  if (usb_debug >= 3)
    fprintf(stderr, "usb_control_msg: %d %d %d %d %p %d %d\n",
            requesttype, request, value, index, bytes, size, timeout);

  bzero(&urequest, sizeof(urequest));

  urequest.bmRequestType = requesttype;
  urequest.bRequest = request;
  urequest.wValue = value;
  urequest.wIndex = index;
  urequest.wLength = size;
  urequest.pData = bytes;
#if !defined (LIBUSB_NO_TIMEOUT_DEVICE)
  urequest.completionTimeout = timeout;
  urequest.noDataTimeout = timeout;

  result = (*(device->device))->DeviceRequestTO(device->device, &urequest);
#else
  result = (*(device->device))->DeviceRequest(device->device, &urequest);
#endif
  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_control_msg(DeviceRequestTO): %s", darwin_error_str(result));

  /* Bytes transfered is stored in the wLenDone field*/
  return urequest.wLenDone;
}

int usb_os_find_busses(struct usb_bus **busses)
{
  struct usb_bus *fbus = NULL;

  io_iterator_t deviceIterator;
  io_return_t result;

  usb_device_t **device;

  UInt32 location;

  char buf[20];
  int i = 1;

  /* Create a master port for communication with IOKit (this should
     have been created if the user called usb_init() )*/
  if (masterPort == MACH_PORT_NULL) {
    usb_init ();

    if (masterPort == MACH_PORT_NULL)
      USB_ERROR(-ENOENT);
  }

  if ((result = usb_setup_iterator (&deviceIterator)) < 0)
    return result;

  while ((device = usb_get_next_device (deviceIterator, &location)) != NULL) {
    struct usb_bus *bus;

    if (location & 0x00ffffff)
      continue;

    bus = calloc(1, sizeof(struct usb_bus));
    if (bus == NULL)
      USB_ERROR(-ENOMEM);
    
    sprintf(buf, "%03i", i++);
    bus->location = location;

    strncpy(bus->dirname, buf, sizeof(bus->dirname) - 1);
    bus->dirname[sizeof(bus->dirname) - 1] = 0;
    
    LIST_ADD(fbus, bus);
    
    if (usb_debug >= 2)
      fprintf(stderr, "usb_os_find_busses: Found %s\n", bus->dirname);

    (*(device))->Release(device);
  }

  IOObjectRelease(deviceIterator);

  *busses = fbus;

  return 0;
}

int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  struct usb_device *fdev = NULL;

  io_iterator_t deviceIterator;
  io_return_t result;

  usb_device_t **device;

  u_int16_t address;
  UInt32 location;
  UInt32 bus_loc = bus->location;

  /* for use in retrieving device description */
  IOUSBDevRequest req;

  /* a master port should have been created by usb_os_init */
  if (masterPort == MACH_PORT_NULL)
    USB_ERROR(-ENOENT);

  if ((result = usb_setup_iterator (&deviceIterator)) < 0)
    return result;

  /* Set up request for device descriptor */
  req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
  req.bRequest = kUSBRqGetDescriptor;
  req.wValue = kUSBDeviceDesc << 8;
  req.wIndex = 0;
  req.wLength = sizeof(IOUSBDeviceDescriptor);


  while ((device = usb_get_next_device (deviceIterator, &location)) != NULL) {
    unsigned char device_desc[DEVICE_DESC_LENGTH];

    result = (*(device))->GetDeviceAddress(device, (USBDeviceAddress *)&address);

    if (usb_debug >= 2)
      fprintf(stderr, "usb_os_find_devices: Found USB device at location 0x%08lx\n", location);

    /* first byte of location appears to be associated with the device's bus */
    if (location >> 24 == bus_loc >> 24) {
      struct usb_device *dev;

      dev = calloc(1, sizeof(struct usb_device));
      if (dev == NULL)
	USB_ERROR(-ENOMEM);

      dev->bus = bus;

      req.pData = device_desc;
      result = (*(device))->DeviceRequest(device, &req);

      usb_parse_descriptor(device_desc, "bbwbbbbwwwbbbb", &dev->descriptor);

      sprintf(dev->filename, "%03i-%04x-%04x-%02x-%02x", address,
	      dev->descriptor.idVendor, dev->descriptor.idProduct,
	      dev->descriptor.bDeviceClass, dev->descriptor.bDeviceSubClass);

      dev->dev = (USBDeviceAddress *)malloc(4);
      memcpy(dev->dev, &location, 4);

      LIST_ADD(fdev, dev);

      if (usb_debug >= 2)
	fprintf(stderr, "usb_os_find_devices: Found %s on %s at location 0x%08lx\n",
		dev->filename, bus->dirname, location);
    }

    /* release the device now */
    (*(device))->Release(device);
  }

  IOObjectRelease(deviceIterator);

  *devices = fdev;

  return 0;
}

int usb_os_determine_children(struct usb_bus *bus)
{
  /* Nothing yet */
  return 0;
}

void usb_os_init(void)
{
  if (masterPort == MACH_PORT_NULL) {
    IOMasterPort(masterPort, &masterPort);
    
    gNotifyPort = IONotificationPortCreate(masterPort);
  }
}

void usb_os_cleanup (void)
{
  if (masterPort != MACH_PORT_NULL)
    darwin_cleanup ();
}

int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  struct darwin_dev_handle *device;

  io_return_t result = -1;

  int pipeRef;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  /* interface is not open */
  if (!device->interface)
    USB_ERROR_STR(-EACCES, "usb_resetep: interface used without being claimed");

  if ((pipeRef = ep_to_pipeRef(device, ep)) == -1)
    USB_ERROR(-EINVAL);

  result = (*(device->interface))->ResetPipe(device->interface, pipeRef);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_resetep(ResetPipe): %s", darwin_error_str(result));

  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  struct darwin_dev_handle *device;

  io_return_t result = -1;

  int pipeRef;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  /* interface is not open */
  if (!device->interface)
    USB_ERROR_STR(-EACCES, "usb_clear_halt: interface used without being claimed");

  if ((pipeRef = ep_to_pipeRef(device, ep)) == -1)
    USB_ERROR(-EINVAL);

  result = (*(device->interface))->ClearPipeStall(device->interface, pipeRef);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_clear_halt(ClearPipeStall): %s", darwin_error_str(result));

  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  struct darwin_dev_handle *device;
  
  io_return_t result;

  if (!dev)
    USB_ERROR(-ENXIO);

  if ((device = dev->impl_info) == NULL)
    USB_ERROR(-ENOENT);

  if (!device->device)
    USB_ERROR_STR(-ENOENT, "usb_reset: no such device");

  result = (*(device->device))->ResetDevice(device->device);

  if (result != kIOReturnSuccess)
    USB_ERROR_STR(-darwin_to_errno(result), "usb_reset(ResetDevice): %s", darwin_error_str(result));
  
  return 0;
}
