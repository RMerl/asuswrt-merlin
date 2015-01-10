include $(top_srcdir)/config/include-Makefile.am

SUBDIRS = src

if ENABLE_GTK_DOC
SUBDIRS += docs
endif

# pkg-config stuff
pkgconfigdir = $(usrlib_execdir)/pkgconfig
pkgconfig_DATA = mount.pc

EXTRA_DIST = mount.pc.in
