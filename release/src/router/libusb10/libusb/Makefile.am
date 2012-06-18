lib_LTLIBRARIES = libusb-1.0.la

LINUX_USBFS_SRC = os/linux_usbfs.h os/linux_usbfs.c
DARWIN_USB_SRC = os/darwin_usb.h os/darwin_usb.c

EXTRA_DIST = $(LINUX_USBFS_SRC) $(DARWIN_USB_SRC)

if OS_LINUX
OS_SRC = $(LINUX_USBFS_SRC)
endif

if OS_DARWIN
OS_SRC = $(DARWIN_USB_SRC)
AM_CFLAGS_EXT = -no-cpp-precomp
endif

libusb_1_0_la_CFLAGS = $(VISIBILITY_CFLAGS) $(AM_CFLAGS) -pthread
libusb_1_0_la_SOURCES = libusbi.h core.c descriptor.c io.c sync.c $(OS_SRC)

hdrdir = $(includedir)/libusb-1.0
hdr_HEADERS = libusb.h
