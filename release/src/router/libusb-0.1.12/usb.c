/*
 * Main API entry point
 *
 * Copyright (c) 2000-2003 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <stdlib.h>	/* getenv */
#include <stdio.h>	/* stderr */
#include <string.h>	/* strcmp */
#include <errno.h>

#include "usbi.h"

int usb_debug = 0;
struct usb_bus *usb_busses = NULL;

int usb_find_busses(void)
{
  struct usb_bus *busses, *bus;
  int ret, changes = 0;

  ret = usb_os_find_busses(&busses);
  if (ret < 0)
    return ret;

  /*
   * Now walk through all of the busses we know about and compare against
   * this new list. Any duplicates will be removed from the new list.
   * If we don't find it in the new list, the bus was removed. Any
   * busses still in the new list, are new to us.
   */
  bus = usb_busses;
  while (bus) {
    int found = 0;
    struct usb_bus *nbus, *tbus = bus->next;

    nbus = busses;
    while (nbus) {
      struct usb_bus *tnbus = nbus->next;

      if (!strcmp(bus->dirname, nbus->dirname)) {
        /* Remove it from the new busses list */
        LIST_DEL(busses, nbus);

        usb_free_bus(nbus);
        found = 1;
        break;
      }

      nbus = tnbus;
    }

    if (!found) {
      /* The bus was removed from the system */
      LIST_DEL(usb_busses, bus);
      usb_free_bus(bus);
      changes++;
    }

    bus = tbus;
  }

  /*
   * Anything on the *busses list is new. So add them to usb_busses and
   * process them like the new bus it is.
   */
  bus = busses;
  while (bus) {
    struct usb_bus *tbus = bus->next;

    /*
     * Remove it from the temporary list first and add it to the real
     * usb_busses list.
     */
    LIST_DEL(busses, bus);

    LIST_ADD(usb_busses, bus);

    changes++;

    bus = tbus;
  }

  return changes;
}

int usb_find_devices(void)
{
  struct usb_bus *bus;
  int ret, changes = 0;

  for (bus = usb_busses; bus; bus = bus->next) {
    struct usb_device *devices, *dev;

    /* Find all of the devices and put them into a temporary list */
    ret = usb_os_find_devices(bus, &devices);
    if (ret < 0)
      return ret;

    /*
     * Now walk through all of the devices we know about and compare
     * against this new list. Any duplicates will be removed from the new
     * list. If we don't find it in the new list, the device was removed.
     * Any devices still in the new list, are new to us.
     */
    dev = bus->devices;
    while (dev) {
      int found = 0;
      struct usb_device *ndev, *tdev = dev->next;

      ndev = devices;
      while (ndev) {
        struct usb_device *tndev = ndev->next;

        if (!strcmp(dev->filename, ndev->filename)) {
          /* Remove it from the new devices list */
          LIST_DEL(devices, ndev);

          usb_free_dev(ndev);
          found = 1;
          break;
        }

        ndev = tndev;
      }

      if (!found) {
        /* The device was removed from the system */
        LIST_DEL(bus->devices, dev);
        usb_free_dev(dev);
        changes++;
      }

      dev = tdev;
    }

    /*
     * Anything on the *devices list is new. So add them to bus->devices and
     * process them like the new device it is.
     */
    dev = devices;
    while (dev) {
      struct usb_device *tdev = dev->next;

      /*
       * Remove it from the temporary list first and add it to the real
       * bus->devices list.
       */
      LIST_DEL(devices, dev);

      LIST_ADD(bus->devices, dev);

      /*
       * Some ports fetch the descriptors on scanning (like Linux) so we don't
       * need to fetch them again.
       */
      if (!dev->config) {
        usb_dev_handle *udev;

        udev = usb_open(dev);
        if (udev) {
          usb_fetch_and_parse_descriptors(udev);

          usb_close(udev);
        }
      }

      changes++;

      dev = tdev;
    }

    usb_os_determine_children(bus);
  }

  return changes;
}

void usb_set_debug(int level)
{
  if (usb_debug || level)
    fprintf(stderr, "usb_set_debug: Setting debugging level to %d (%s)\n",
	level, level ? "on" : "off");

  usb_debug = level;
}

void usb_init(void)
{
  if (getenv("USB_DEBUG"))
    usb_set_debug(atoi(getenv("USB_DEBUG")));

  usb_os_init();
}

usb_dev_handle *usb_open(struct usb_device *dev)
{
  usb_dev_handle *udev;

  udev = malloc(sizeof(*udev));
  if (!udev)
    return NULL;

  udev->fd = -1;
  udev->device = dev;
  udev->bus = dev->bus;
  udev->config = udev->interface = udev->altsetting = -1;

  if (usb_os_open(udev) < 0) {
    free(udev);
    return NULL;
  }

  return udev;
}

int usb_get_string(usb_dev_handle *dev, int index, int langid, char *buf,
	size_t buflen)
{
  /*
   * We can't use usb_get_descriptor() because it's lacking the index
   * parameter. This will be fixed in libusb 1.0
   */
  return usb_control_msg(dev, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR,
			(USB_DT_STRING << 8) + index, langid, buf, buflen, 1000);
}

int usb_get_string_simple(usb_dev_handle *dev, int index, char *buf, size_t buflen)
{
  char tbuf[255];	/* Some devices choke on size > 255 */
  int ret, langid, si, di;

  /*
   * Asking for the zero'th index is special - it returns a string
   * descriptor that contains all the language IDs supported by the
   * device. Typically there aren't many - often only one. The
   * language IDs are 16 bit numbers, and they start at the third byte
   * in the descriptor. See USB 2.0 specification, section 9.6.7, for
   * more information on this. */
  ret = usb_get_string(dev, 0, 0, tbuf, sizeof(tbuf));
  if (ret < 0)
    return ret;

  if (ret < 4)
    return -EIO;

  langid = tbuf[2] | (tbuf[3] << 8);

  ret = usb_get_string(dev, index, langid, tbuf, sizeof(tbuf));
  if (ret < 0)
    return ret;

  if (tbuf[1] != USB_DT_STRING)
    return -EIO;

  if (tbuf[0] > ret)
    return -EFBIG;

  for (di = 0, si = 2; si < tbuf[0]; si += 2) {
    if (di >= (buflen - 1))
      break;

    if (tbuf[si + 1])	/* high byte */
      buf[di++] = '?';
    else
      buf[di++] = tbuf[si];
  }

  buf[di] = 0;

  return di;
}

int usb_close(usb_dev_handle *dev)
{
  int ret;

  ret = usb_os_close(dev);
  free(dev);

  return ret;
}

struct usb_device *usb_device(usb_dev_handle *dev)
{
  return dev->device;
}

void usb_free_dev(struct usb_device *dev)
{
  usb_destroy_configuration(dev);
  free(dev->children);
  free(dev);
}

struct usb_bus *usb_get_busses(void)
{
  return usb_busses;
}

void usb_free_bus(struct usb_bus *bus)
{
  free(bus);
}

