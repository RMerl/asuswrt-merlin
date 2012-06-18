/*
 * Linux USB support
 *
 * Copyright (c) 2000-2003 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <stdlib.h>	/* getenv, etc */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>

#include "linux.h"
#include "usbi.h"

static char usb_path[PATH_MAX + 1] = "";

static int device_open(struct usb_device *dev)
{
  char filename[PATH_MAX + 1];
  int fd;

  snprintf(filename, sizeof(filename) - 1, "%s/%s/%s",
    usb_path, dev->bus->dirname, dev->filename);

  fd = open(filename, O_RDWR);
  if (fd < 0) {
    fd = open(filename, O_RDONLY);
    if (fd < 0)
      USB_ERROR_STR(-errno, "failed to open %s: %s",
	filename, strerror(errno));
  }

  return fd;
}

int usb_os_open(usb_dev_handle *dev)
{
  dev->fd = device_open(dev->device);

  return 0;
}

int usb_os_close(usb_dev_handle *dev)
{
  if (dev->fd < 0)
    return 0;

  if (close(dev->fd) == -1)
    /* Failing trying to close a file really isn't an error, so return 0 */
    USB_ERROR_STR(0, "tried to close device fd %d: %s", dev->fd,
	strerror(errno));

  return 0;
}

int usb_set_configuration(usb_dev_handle *dev, int configuration)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_SETCONFIG, &configuration);
  if (ret < 0)
    USB_ERROR_STR(-errno, "could not set config %d: %s", configuration,
	strerror(errno));

  dev->config = configuration;

  return 0;
}

int usb_claim_interface(usb_dev_handle *dev, int interface)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_CLAIMINTF, &interface);
  if (ret < 0) {
    if (errno == EBUSY && usb_debug > 0)
      fprintf(stderr, "Check that you have permissions to write to %s/%s and, if you don't, that you set up hotplug (http://linux-hotplug.sourceforge.net/) correctly.\n", dev->bus->dirname, dev->device->filename);

    USB_ERROR_STR(-errno, "could not claim interface %d: %s", interface,
	strerror(errno));
  }

  dev->interface = interface;

  return 0;
}

int usb_release_interface(usb_dev_handle *dev, int interface)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_RELEASEINTF, &interface);
  if (ret < 0)
    USB_ERROR_STR(-errno, "could not release intf %d: %s", interface,
    	strerror(errno));

  dev->interface = -1;

  return 0;
}

int usb_set_altinterface(usb_dev_handle *dev, int alternate)
{
  int ret;
  struct usb_setinterface setintf;

  if (dev->interface < 0)
    USB_ERROR(-EINVAL);

  setintf.interface = dev->interface;
  setintf.altsetting = alternate;

  ret = ioctl(dev->fd, IOCTL_USB_SETINTF, &setintf);
  if (ret < 0)
    USB_ERROR_STR(-errno, "could not set alt intf %d/%d: %s",
	dev->interface, alternate, strerror(errno));

  dev->altsetting = alternate;

  return 0;
}

/*
 * Linux usbfs has a limit of one page size for synchronous bulk read/write.
 * 4096 is the most portable maximum we can do for now.
 * Linux usbfs has a limit of 16KB for the URB interface. We use this now
 * to get better performance for USB 2.0 devices.
 */
#define MAX_READ_WRITE	(16 * 1024)

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
	int value, int index, char *bytes, int size, int timeout)
{
  struct usb_ctrltransfer ctrl;
  int ret;

  ctrl.bRequestType = requesttype;
  ctrl.bRequest = request;
  ctrl.wValue = value;
  ctrl.wIndex = index;
  ctrl.wLength = size;

  ctrl.data = bytes;
  ctrl.timeout = timeout;

  ret = ioctl(dev->fd, IOCTL_USB_CONTROL, &ctrl);
  if (ret < 0)
    USB_ERROR_STR(-errno, "error sending control message: %s", strerror(errno));

  return ret;
}

#define URB_USERCONTEXT_COOKIE		((void *)0x1)

/* Reading and writing are the same except for the endpoint */
static int usb_urb_transfer(usb_dev_handle *dev, int ep, int urbtype,
	char *bytes, int size, int timeout)
{
  struct usb_urb urb;
  int bytesdone = 0, requested;
  struct timeval tv, tv_ref, tv_now;
  struct usb_urb *context;
  int ret, waiting;

  /*
   * HACK: The use of urb.usercontext is a hack to get threaded applications
   * sort of working again. Threaded support is still not recommended, but
   * this should allow applications to work in the common cases. Basically,
   * if we get the completion for an URB we're not waiting for, then we update
   * the usercontext pointer to 1 for the other threads URB and it will see
   * the change after it wakes up from the the timeout. Ugly, but it works.
   */

  /*
   * Get actual time, and add the timeout value. The result is the absolute
   * time where we have to quit waiting for an message.
   */
  gettimeofday(&tv_ref, NULL);
  tv_ref.tv_sec = tv_ref.tv_sec + timeout / 1000;
  tv_ref.tv_usec = tv_ref.tv_usec + (timeout % 1000) * 1000;

  if (tv_ref.tv_usec > 1000000) {
    tv_ref.tv_usec -= 1000000;
    tv_ref.tv_sec++;
  }

  do {
    fd_set writefds;

    requested = size - bytesdone;
    if (requested > MAX_READ_WRITE)
      requested = MAX_READ_WRITE;

    urb.type = urbtype;
    urb.endpoint = ep;
    urb.flags = 0;
    urb.buffer = bytes + bytesdone;
    urb.buffer_length = requested;
    urb.signr = 0;
    urb.actual_length = 0;
    urb.number_of_packets = 0;	/* don't do isochronous yet */
    urb.usercontext = NULL;

    ret = ioctl(dev->fd, IOCTL_USB_SUBMITURB, &urb);
    if (ret < 0) {
      USB_ERROR_STR(-errno, "error submitting URB: %s", strerror(errno));
      return ret;
    }

    FD_ZERO(&writefds);
    FD_SET(dev->fd, &writefds);

restart:
    waiting = 1;
    context = NULL;
    while (!urb.usercontext && ((ret = ioctl(dev->fd, IOCTL_USB_REAPURBNDELAY, &context)) == -1) && waiting) {
      tv.tv_sec = 0;
      tv.tv_usec = 1000; // 1 msec
      select(dev->fd + 1, NULL, &writefds, NULL, &tv); //sub second wait
      /* 32767 is a magic number for no timeout */
      if (timeout && timeout != 32767) {
        /* compare with actual time, as the select timeout is not that precise */
        gettimeofday(&tv_now, NULL);

        if ((tv_now.tv_sec > tv_ref.tv_sec) ||
            ((tv_now.tv_sec == tv_ref.tv_sec) && (tv_now.tv_usec >= tv_ref.tv_usec)))
          waiting = 0;
      }
    }

    if (context && context != &urb) {
      context->usercontext = URB_USERCONTEXT_COOKIE;
      /* We need to restart since we got a successful URB, but not ours */
      goto restart;
    }

    /*
     * If there was an error, that wasn't EAGAIN (no completion), then
     * something happened during the reaping and we should return that
     * error now
     */
    if (ret < 0 && !urb.usercontext && errno != EAGAIN)
      USB_ERROR_STR(-errno, "error reaping URB: %s", strerror(errno));

    bytesdone += urb.actual_length;
  } while ((ret == 0 || urb.usercontext) && bytesdone < size && urb.actual_length == requested);

  /* If the URB didn't complete in success or error, then let's unlink it */
  if (ret < 0 && !urb.usercontext) {
    int rc;

    if (!waiting)
      rc = -ETIMEDOUT;
    else
      rc = urb.status;

    ret = ioctl(dev->fd, IOCTL_USB_DISCARDURB, &urb);
    if (ret < 0 && errno != EINVAL && usb_debug >= 1)
      fprintf(stderr, "error discarding URB: %s", strerror(errno));

    /*
     * When the URB is unlinked, it gets moved to the completed list and
     * then we need to reap it or else the next time we call this function,
     * we'll get the previous completion and exit early
     */
    ioctl(dev->fd, IOCTL_USB_REAPURB, &context);

    return rc;
  }

  return bytesdone;
}

/* Reading and writing are the same except for the endpoint */
static int usb_urb_transfer_sp(usb_dev_handle *dev, int ep, int urbtype,
	char *bytes, int size, int timeout, int *actual_length, int max_rw)
{
  struct usb_urb urb;
  int bytesdone = 0, requested;
  struct timeval tv, tv_ref, tv_now;
  struct usb_urb *context;
  int ret, waiting;

  /*
   * HACK: The use of urb.usercontext is a hack to get threaded applications
   * sort of working again. Threaded support is still not recommended, but
   * this should allow applications to work in the common cases. Basically,
   * if we get the completion for an URB we're not waiting for, then we update
   * the usercontext pointer to 1 for the other threads URB and it will see
   * the change after it wakes up from the the timeout. Ugly, but it works.
   */

  /*
   * Get actual time, and add the timeout value. The result is the absolute
   * time where we have to quit waiting for an message.
   */
  gettimeofday(&tv_ref, NULL);
  tv_ref.tv_sec = tv_ref.tv_sec + timeout / 1000;
  tv_ref.tv_usec = tv_ref.tv_usec + (timeout % 1000) * 1000;

  if (tv_ref.tv_usec > 1000000) {
    tv_ref.tv_usec -= 1000000;
    tv_ref.tv_sec++;
  }

  do {
    fd_set writefds;

    requested = size - bytesdone;
    if (requested > max_rw)
      requested = max_rw;

    urb.type = urbtype;
    urb.endpoint = ep;
    urb.flags = 0;
    urb.buffer = bytes + bytesdone;
    urb.buffer_length = requested;
    urb.signr = 0;
    urb.actual_length = 0;
    urb.number_of_packets = 0;	/* don't do isochronous yet */
    urb.usercontext = NULL;

    ret = ioctl(dev->fd, IOCTL_USB_SUBMITURB, &urb);
    if (ret < 0) {
      USB_ERROR_STR(-errno, "error submitting URB: %s", strerror(errno));
      return ret;
    }

    FD_ZERO(&writefds);
    FD_SET(dev->fd, &writefds);

restart:
    waiting = 1;
    context = NULL;
    while (!urb.usercontext && ((ret = ioctl(dev->fd, IOCTL_USB_REAPURBNDELAY, &context)) == -1) && waiting) {
      tv.tv_sec = 0;
      tv.tv_usec = 1000; // 1 msec
      select(dev->fd + 1, NULL, &writefds, NULL, &tv); //sub second wait
#if 0
      if (timeout) {
#else /* 32767 is a magic number for no timeout */
      if (timeout && timeout != 32767) {
#endif
        /* compare with actual time, as the select timeout is not that precise */
        gettimeofday(&tv_now, NULL);

        if ((tv_now.tv_sec > tv_ref.tv_sec) ||
            ((tv_now.tv_sec == tv_ref.tv_sec) && (tv_now.tv_usec >= tv_ref.tv_usec)))
          waiting = 0;
      }
    }

    if (context && context != &urb) {
      context->usercontext = URB_USERCONTEXT_COOKIE;
      /* We need to restart since we got a successful URB, but not ours */
      goto restart;
    }

    /*
     * If there was an error, that wasn't EAGAIN (no completion), then
     * something happened during the reaping and we should return that
     * error now
     */
    if (ret < 0 && !urb.usercontext && errno != EAGAIN)
      USB_ERROR_STR(-errno, "error reaping URB: %s", strerror(errno));

    bytesdone += urb.actual_length;
    *actual_length = bytesdone;
  } while ((ret == 0 || urb.usercontext) && bytesdone < size && urb.actual_length == requested);

  /* If the URB didn't complete in success or error, then let's unlink it */
  if (ret < 0 && !urb.usercontext) {
    int rc;

    if (!waiting)
      rc = -ETIMEDOUT;
    else
      rc = urb.status;

    ret = ioctl(dev->fd, IOCTL_USB_DISCARDURB, &urb);
    if (ret < 0 && errno != EINVAL && usb_debug >= 1)
      fprintf(stderr, "error discarding URB: %s", strerror(errno));

    /*
     * When the URB is unlinked, it gets moved to the completed list and
     * then we need to reap it or else the next time we call this function,
     * we'll get the previous completion and exit early
     */
    ioctl(dev->fd, IOCTL_USB_REAPURB, &context);

    return rc;
  }

  return bytesdone;
}

int usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  /* Ensure the endpoint address is correct */
  return usb_urb_transfer(dev, ep, USB_URB_TYPE_BULK, bytes, size,
		timeout);
}

int usb_bulk_write_sp(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout, int *actual_length, int max_rw)
{
  /* Ensure the endpoint address is correct */
  return usb_urb_transfer_sp(dev, ep, USB_URB_TYPE_BULK, bytes, size,
		timeout, actual_length, max_rw);
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  /* Ensure the endpoint address is correct */
  ep |= USB_ENDPOINT_IN;
  return usb_urb_transfer(dev, ep, USB_URB_TYPE_BULK, bytes, size,
		timeout);
}

/*
 * FIXME: Packetize large buffers here. 2.4 HCDs (atleast, haven't checked
 * 2.5 HCDs yet) don't handle multi-packet Interrupt transfers. So we need
 * to lookup the endpoint packet size and packetize appropriately here.
 */
int usb_interrupt_write(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  /* Ensure the endpoint address is correct */
  return usb_urb_transfer(dev, ep, USB_URB_TYPE_INTERRUPT, bytes, size,
		timeout);
}

int usb_interrupt_read(usb_dev_handle *dev, int ep, char *bytes, int size,
	int timeout)
{
  /* Ensure the endpoint address is correct */
  ep |= USB_ENDPOINT_IN;
  return usb_urb_transfer(dev, ep, USB_URB_TYPE_INTERRUPT, bytes, size,
		timeout);
}

int usb_os_find_busses(struct usb_bus **busses)
{
  struct usb_bus *fbus = NULL;
  DIR *dir;
  struct dirent *entry;

  dir = opendir(usb_path);
  if (!dir)
    USB_ERROR_STR(-errno, "couldn't opendir(%s): %s", usb_path,
	strerror(errno));

  while ((entry = readdir(dir)) != NULL) {
    struct usb_bus *bus;

    /* Skip anything starting with a . */
    if (entry->d_name[0] == '.')
      continue;

    if (!strchr("0123456789", entry->d_name[strlen(entry->d_name) - 1])) {
      if (usb_debug >= 2)
        fprintf(stderr, "usb_os_find_busses: Skipping non bus directory %s\n",
		entry->d_name);
      continue;
    }

    bus = malloc(sizeof(*bus));
    if (!bus)
      USB_ERROR(-ENOMEM);

    memset((void *)bus, 0, sizeof(*bus));

    strncpy(bus->dirname, entry->d_name, sizeof(bus->dirname) - 1);
    bus->dirname[sizeof(bus->dirname) - 1] = 0;

    LIST_ADD(fbus, bus);

    if (usb_debug >= 2)
       fprintf(stderr, "usb_os_find_busses: Found %s\n", bus->dirname);
  }

  closedir(dir);

  *busses = fbus;

  return 0;
}

int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
  struct usb_device *fdev = NULL;
  DIR *dir;
  struct dirent *entry;
  char dirpath[PATH_MAX + 1];

  snprintf(dirpath, PATH_MAX, "%s/%s", usb_path, bus->dirname);

  dir = opendir(dirpath);
  if (!dir)
    USB_ERROR_STR(-errno, "couldn't opendir(%s): %s", dirpath,
	strerror(errno));

  while ((entry = readdir(dir)) != NULL) {
    unsigned char device_desc[DEVICE_DESC_LENGTH];
    char filename[PATH_MAX + 1];
    struct usb_device *dev;
    struct usb_connectinfo connectinfo;
    int i, fd, ret;

    /* Skip anything starting with a . */
    if (entry->d_name[0] == '.')
      continue;

    dev = malloc(sizeof(*dev));
    if (!dev)
      USB_ERROR(-ENOMEM);

    memset((void *)dev, 0, sizeof(*dev));

    dev->bus = bus;

    strncpy(dev->filename, entry->d_name, sizeof(dev->filename) - 1);
    dev->filename[sizeof(dev->filename) - 1] = 0;

    snprintf(filename, sizeof(filename) - 1, "%s/%s", dirpath, entry->d_name);
    fd = open(filename, O_RDWR);
    if (fd < 0) {
      fd = open(filename, O_RDONLY);
      if (fd < 0) {
        if (usb_debug >= 2)
          fprintf(stderr, "usb_os_find_devices: Couldn't open %s\n",
                  filename);

        free(dev);
        continue;
      }
    }

    /* Get the device number */
    ret = ioctl(fd, IOCTL_USB_CONNECTINFO, &connectinfo);
    if (ret < 0) {
      if (usb_debug)
        fprintf(stderr, "usb_os_find_devices: couldn't get connect info\n");
    } else
      dev->devnum = connectinfo.devnum;

    ret = read(fd, (void *)device_desc, DEVICE_DESC_LENGTH);
    if (ret < 0) {
      if (usb_debug)
        fprintf(stderr, "usb_os_find_devices: Couldn't read descriptor\n");

      free(dev);

      goto err;
    }

    /*
     * Linux kernel converts the words in this descriptor to CPU endian, so
     * we use the undocumented W character for usb_parse_descriptor() that
     * doesn't convert endianess when parsing the descriptor
     */
    usb_parse_descriptor(device_desc, "bbWbbbbWWWbbbb", &dev->descriptor);

    LIST_ADD(fdev, dev);

    if (usb_debug >= 2)
      fprintf(stderr, "usb_os_find_devices: Found %s on %s\n",
		dev->filename, bus->dirname);

    /* Now try to fetch the rest of the descriptors */
    if (dev->descriptor.bNumConfigurations > USB_MAXCONFIG)
      /* Silent since we'll try again later */
      goto err;

    if (dev->descriptor.bNumConfigurations < 1)
      /* Silent since we'll try again later */
      goto err;

    dev->config = (struct usb_config_descriptor *)malloc(dev->descriptor.bNumConfigurations * sizeof(struct usb_config_descriptor));
    if (!dev->config)
      /* Silent since we'll try again later */
      goto err;

    memset(dev->config, 0, dev->descriptor.bNumConfigurations *
          sizeof(struct usb_config_descriptor));

    for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
      unsigned char buffer[8], *bigbuffer;
      struct usb_config_descriptor config;

      /* Get the first 8 bytes so we can figure out what the total length is */
      ret = read(fd, (void *)buffer, 8);
      if (ret < 8) {
        if (usb_debug >= 1) {
          if (ret < 0)
            fprintf(stderr, "Unable to get descriptor (%d)\n", ret);
          else
            fprintf(stderr, "Config descriptor too short (expected %d, got %d)\n", 8, ret);
        }

        goto err;
      }

      usb_parse_descriptor(buffer, "bbw", &config);

      bigbuffer = malloc(config.wTotalLength);
      if (!bigbuffer) {
        if (usb_debug >= 1)
          fprintf(stderr, "Unable to allocate memory for descriptors\n");
        goto err;
      }

      /* Read the rest of the config descriptor */
      memcpy(bigbuffer, buffer, 8);

      ret = read(fd, (void *)(bigbuffer + 8), config.wTotalLength - 8);
      if (ret < config.wTotalLength - 8) {
        if (usb_debug >= 1) {
          if (ret < 0)
            fprintf(stderr, "Unable to get descriptor (%d)\n", ret);
          else
            fprintf(stderr, "Config descriptor too short (expected %d, got %d)\n", config.wTotalLength, ret);
        }

        free(bigbuffer);
        goto err;
      }

      ret = usb_parse_configuration(&dev->config[i], bigbuffer);
      if (usb_debug >= 2) {
        if (ret > 0)
          fprintf(stderr, "Descriptor data still left\n");
        else if (ret < 0)
          fprintf(stderr, "Unable to parse descriptors\n");
      }

      free(bigbuffer);
    }

err:
    close(fd);
  }

  closedir(dir);

  *devices = fdev;

  return 0;
}

int usb_os_determine_children(struct usb_bus *bus)
{
  struct usb_device *dev, *devices[256];
  struct usb_ioctl command;
  int ret, i, i1;

  /* Create a list of devices first */
  memset(devices, 0, sizeof(devices));
  for (dev = bus->devices; dev; dev = dev->next)
    if (dev->devnum)
      devices[dev->devnum] = dev;

  /* Now fetch the children for each device */
  for (dev = bus->devices; dev; dev = dev->next) {
    struct usb_hub_portinfo portinfo;
    int fd;

    fd = device_open(dev);
    if (fd < 0)
      continue;

    /* Query the hub driver for the children of this device */
    if (dev->config && dev->config->interface && dev->config->interface->altsetting)
      command.ifno = dev->config->interface->altsetting->bInterfaceNumber;
    else
      command.ifno = 0;
    command.ioctl_code = IOCTL_USB_HUB_PORTINFO;
    command.data = &portinfo;
    ret = ioctl(fd, IOCTL_USB_IOCTL, &command);
    if (ret < 0) {
      /* errno == ENOSYS means the device probably wasn't a hub */
      if (errno != ENOSYS && usb_debug > 1)
        fprintf(stderr, "error obtaining child information: %s\n",
		strerror(errno));

      close(fd);
      continue;
    }

    dev->num_children = 0;
    for (i = 0; i < portinfo.numports; i++)
      if (portinfo.port[i])
        dev->num_children++;

    /* Free any old children first */
    free(dev->children);

    dev->children = malloc(sizeof(struct usb_device *) * dev->num_children);
    if (!dev->children) {
      if (usb_debug > 1)
        fprintf(stderr, "error allocating %zu bytes memory for dev->children\n",
                sizeof(struct usb_device *) * dev->num_children);

      dev->num_children = 0;
      close(fd);
      continue;
    }

    for (i = 0, i1 = 0; i < portinfo.numports; i++) {
      if (!portinfo.port[i])
        continue;

      dev->children[i1++] = devices[portinfo.port[i]];

      devices[portinfo.port[i]] = NULL;
    }

    close(fd);
  }

  /*
   * There should be one device left in the devices list and that should be
   * the root device
   */
  for (i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
    if (devices[i])
      bus->root_dev = devices[i];
  }

  return 0;
}

static int check_usb_vfs(const char *dirname)
{
  DIR *dir;
  struct dirent *entry;
  int found = 0;

  dir = opendir(dirname);
  if (!dir)
    return 0;

  while ((entry = readdir(dir)) != NULL) {
    /* Skip anything starting with a . */
    if (entry->d_name[0] == '.')
      continue;

    /* We assume if we find any files that it must be the right place */
    found = 1;
    break;
  }

  closedir(dir);

  return found;
}

void usb_os_init(void)
{
  /* Find the path to the virtual filesystem */
  if (getenv("USB_DEVFS_PATH")) {
    if (check_usb_vfs(getenv("USB_DEVFS_PATH"))) {
      strncpy(usb_path, getenv("USB_DEVFS_PATH"), sizeof(usb_path) - 1);
      usb_path[sizeof(usb_path) - 1] = 0;
    } else if (usb_debug)
      fprintf(stderr, "usb_os_init: couldn't find USB VFS in USB_DEVFS_PATH\n");
  }

  if (!usb_path[0]) {
    if (check_usb_vfs("/dev/bus/usb")) {
      strncpy(usb_path, "/dev/bus/usb", sizeof(usb_path) - 1);
      usb_path[sizeof(usb_path) - 1] = 0;
    } else if (check_usb_vfs("/proc/bus/usb")) {
      strncpy(usb_path, "/proc/bus/usb", sizeof(usb_path) - 1);
      usb_path[sizeof(usb_path) - 1] = 0;
    } else
      usb_path[0] = 0;	/* No path, no USB support */
  }

  if (usb_debug) {
    if (usb_path[0])
      fprintf(stderr, "usb_os_init: Found USB VFS at %s\n", usb_path);
    else
      fprintf(stderr, "usb_os_init: No USB VFS found, is it mounted?\n");
  }
}

int usb_resetep(usb_dev_handle *dev, unsigned int ep)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_RESETEP, &ep);
  if (ret)
    USB_ERROR_STR(-errno, "could not reset ep %d: %s", ep,
    	strerror(errno));

  return 0;
}

int usb_clear_halt(usb_dev_handle *dev, unsigned int ep)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_CLEAR_HALT, &ep);
  if (ret)
    USB_ERROR_STR(-errno, "could not clear/halt ep %d: %s", ep,
    	strerror(errno));

  return 0;
}

int usb_reset(usb_dev_handle *dev)
{
  int ret;

  ret = ioctl(dev->fd, IOCTL_USB_RESET, NULL);
  if (ret)
     USB_ERROR_STR(-errno, "could not reset: %s", strerror(errno));

  return 0;
}

int usb_get_driver_np(usb_dev_handle *dev, int interface, char *name,
	unsigned int namelen)
{
  struct usb_getdriver getdrv;
  int ret;

  getdrv.interface = interface;
  ret = ioctl(dev->fd, IOCTL_USB_GETDRIVER, &getdrv);
  if (ret)
    USB_ERROR_STR(-errno, "could not get bound driver: %s", strerror(errno));

  strncpy(name, getdrv.driver, namelen - 1);
  name[namelen - 1] = 0;

  return 0;
}

int usb_detach_kernel_driver_np(usb_dev_handle *dev, int interface)
{
  struct usb_ioctl command;
  int ret;

  command.ifno = interface;
  command.ioctl_code = IOCTL_USB_DISCONNECT;
  command.data = NULL;

  ret = ioctl(dev->fd, IOCTL_USB_IOCTL, &command);
  if (ret)
    USB_ERROR_STR(-errno, "could not detach kernel driver from interface %d: %s",
        interface, strerror(errno));

  return 0;
}

