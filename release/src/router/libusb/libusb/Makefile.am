include_HEADERS = usb.h
lib_LTLIBRARIES = libusb.la

libusb_la_SOURCES = core.c usbi.h
libusb_la_CFLAGS = $(AM_CFLAGS) $(LIBUSB_1_0_CFLAGS)
libusb_la_LIBADD = $(LIBUSB_1_0_LIBS)
libusb_la_LDFLAGS = -version-info $(LT_MAJOR):$(LT_REVISION):$(LT_AGE) \
	-release 0.1

