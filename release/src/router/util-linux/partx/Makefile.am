include $(top_srcdir)/config/include-Makefile.am

usrsbin_exec_PROGRAMS = addpart delpart
dist_man_MANS = addpart.8 delpart.8

usrsbin_exec_PROGRAMS += partx
partx_SOURCES = partx.c partx.h \
		$(top_srcdir)/lib/blkdev.c \
		$(top_srcdir)/lib/sysfs.c \
		$(top_srcdir)/lib/tt.c \
		$(top_srcdir)/lib/at.c \
		$(top_srcdir)/lib/mbsalign.c \
		$(top_srcdir)/lib/strutils.c \
		$(top_srcdir)/lib/linux_version.c

partx_CFLAGS = -I$(ul_libblkid_incdir)
partx_LDADD = $(ul_libblkid_la)
dist_man_MANS += partx.8

