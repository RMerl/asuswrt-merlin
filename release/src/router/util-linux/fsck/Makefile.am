include $(top_srcdir)/config/include-Makefile.am

sbin_PROGRAMS = fsck
dist_man_MANS = fsck.8

fsck_SOURCES = fsck.c fsck.h $(top_srcdir)/lib/ismounted.c \
	       $(top_srcdir)/lib/fsprobe.c $(top_srcdir)/lib/canonicalize.c
fsck_LDADD =
fsck_CFLAGS = $(AM_CFLAGS)

fsck_LDADD += $(ul_libblkid_la)
fsck_CFLAGS += -I$(ul_libblkid_incdir)
