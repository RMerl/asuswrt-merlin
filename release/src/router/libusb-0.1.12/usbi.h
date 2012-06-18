#ifndef _USBI_H_
#define _USBI_H_

#include "usb.h"

#include "error.h"

extern int usb_debug;

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

#define DESC_HEADER_LENGTH		2
#define DEVICE_DESC_LENGTH		18
#define CONFIG_DESC_LENGTH		9
#define INTERFACE_DESC_LENGTH		9
#define ENDPOINT_DESC_LENGTH		7
#define ENDPOINT_AUDIO_DESC_LENGTH	9

struct usb_dev_handle {
  int fd;

  struct usb_bus *bus;
  struct usb_device *device;

  int config;
  int interface;
  int altsetting;

  /* Added by RMT so implementations can store other per-open-device data */
  void *impl_info;
};

/* descriptors.c */
int usb_parse_descriptor(unsigned char *source, char *description, void *dest);
int usb_parse_configuration(struct usb_config_descriptor *config,
	unsigned char *buffer);
void usb_fetch_and_parse_descriptors(usb_dev_handle *udev);
void usb_destroy_configuration(struct usb_device *dev);

/* OS specific routines */
int usb_os_find_busses(struct usb_bus **busses);
int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices);
int usb_os_determine_children(struct usb_bus *bus);
void usb_os_init(void);
int usb_os_open(usb_dev_handle *dev);
int usb_os_close(usb_dev_handle *dev);

void usb_free_dev(struct usb_device *dev);
void usb_free_bus(struct usb_bus *bus);

#endif /* _USBI_H_ */

