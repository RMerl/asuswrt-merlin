include $(top_srcdir)/config/include-Makefile.am

SUBDIRS = src samples

if ENABLE_GTK_DOC
SUBDIRS += docs
endif

# pkg-config stuff
pkgconfigdir = $(usrlib_execdir)/pkgconfig
pkgconfig_DATA = mount.pc

EXTRA_DIST = COPYING.libmount mount.pc.in
