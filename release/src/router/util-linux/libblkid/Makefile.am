include $(top_srcdir)/config/include-Makefile.am

SUBDIRS = src samples

if ENABLE_GTK_DOC
SUBDIRS += docs
endif

# pkg-config stuff
pkgconfigdir = $(usrlib_execdir)/pkgconfig
pkgconfig_DATA = blkid.pc

dist_man_MANS = libblkid.3

EXTRA_DIST = COPYING.libblkid README.blkid blkid.pc.in libblkid.3

