include $(top_srcdir)/config/include-Makefile.am

AM_CPPFLAGS += -I$(ul_libblkid_incdir) -I$(ul_libblkid_srcdir)
libblkid_topology_la_LIBADD =

noinst_LTLIBRARIES = libblkid_topology.la
libblkid_topology_la_SOURCES =	topology.c \
				topology.h

if LINUX
libblkid_topology_la_SOURCES += sysfs.c \
				dm.c \
				lvm.c \
				ioctl.c \
				md.c \
				evms.c
endif
