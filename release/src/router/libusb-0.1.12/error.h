#ifndef _ERROR_H_
#define _ERROR_H_

typedef enum {
  USB_ERROR_TYPE_NONE = 0,
  USB_ERROR_TYPE_STRING,
  USB_ERROR_TYPE_ERRNO,
} usb_error_type_t;

extern char usb_error_str[1024];
extern int usb_error_errno;
extern usb_error_type_t usb_error_type;

#define USB_ERROR(x) \
	do { \
          usb_error_type = USB_ERROR_TYPE_ERRNO; \
          usb_error_errno = x; \
	  return x; \
	} while (0)

#define USB_ERROR_STR(x, format, args...) \
	do { \
	  usb_error_type = USB_ERROR_TYPE_STRING; \
	  snprintf(usb_error_str, sizeof(usb_error_str) - 1, format, ## args); \
          if (usb_debug >= 2) \
            fprintf(stderr, "USB error: %s\n", usb_error_str); \
	  return x; \
	} while (0)

#endif /* _ERROR_H_ */

