INCLUDES = -I$(top_srcdir)
noinst_PROGRAMS = lsusb dpfp dpfp_threaded

lsusb_SOURCES = lsusb.c
lsusb_LDADD = ../libusb/libusb-1.0.la -lusb-1.0

dpfp_SOURCES = dpfp.c
dpfp_LDADD = ../libusb/libusb-1.0.la -lusb-1.0

dpfp_threaded_SOURCES = dpfp_threaded.c
dpfp_threaded_CFLAGS = -pthread $(AM_CFLAGS)
dpfp_threaded_LDADD = ../libusb/libusb-1.0.la -lusb-1.0

