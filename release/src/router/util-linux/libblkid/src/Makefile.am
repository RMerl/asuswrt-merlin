include $(top_srcdir)/config/include-Makefile.am

SUBDIRS = superblocks topology partitions .

common_ldadd =
common_cflags =

if BUILD_LIBUUID
common_ldadd += $(ul_libuuid_la)
common_cflags += -I$(ul_libuuid_srcdir)
endif

AM_CPPFLAGS += -I$(ul_libblkid_incdir) -I$(ul_libblkid_srcdir) $(common_cflags)

# includes
blkidincdir = $(includedir)/blkid
nodist_blkidinc_HEADERS = blkid.h

usrlib_exec_LTLIBRARIES = libblkid.la
libblkid_la_SOURCES = cache.c dev.c devname.c devno.c getsize.c llseek.c  \
		     probe.c read.c resolve.c save.c tag.c version.c verify.c \
		     encode.c blkidP.h superblocks/superblocks.h \
		     config.c evaluate.c \
		     $(blkidinc_HEADERS) \
		     $(top_srcdir)/lib/blkdev.c \
		     $(top_srcdir)/lib/linux_version.c \
		     $(top_srcdir)/lib/canonicalize.c \
		     $(top_srcdir)/lib/md5.c \
		     $(top_srcdir)/lib/crc32.c \
		     $(top_srcdir)/include/list.h \
		     $(top_srcdir)/lib/env.c \
		     $(top_srcdir)/lib/strutils.c \
		     $(top_srcdir)/lib/at.c \
		     $(top_srcdir)/lib/sysfs.c

nodist_libblkid_la_SOURCES = blkid.h

libblkid_la_LIBADD = superblocks/libblkid_superblocks.la \
		     topology/libblkid_topology.la \
		     partitions/libblkid_partitions.la \
		     $(common_ldadd)

libblkid_la_DEPENDENCIES = $(libblkid_la_LIBADD) blkid.sym blkid.h.in

libblkid_la_LDFLAGS = -Wl,--version-script=$(ul_libblkid_srcdir)/blkid.sym \
                      -version-info $(LIBBLKID_VERSION_INFO)

tests = test_cache test_config test_dev test_devname test_devno \
	test_read test_resolve test_save test_tag test_verify test_evaluate

EXTRA_DIST = blkid.sym tst_types.c blkid.h.in
CLEANFILES = $(tests)

tests: all $(tests)
test_%: %.c
	$(AM_V_CC)$(COMPILE) -DTEST_PROGRAM $< .libs/libblkid.a -o $@ -luuid


# move lib from $(usrlib_execdir) to $(libdir) if needed
install-exec-hook:
	if test "$(usrlib_execdir)" != "$(libdir)"; then \
		mkdir -p $(DESTDIR)$(libdir); \
		mv $(DESTDIR)$(usrlib_execdir)/libblkid.so.* $(DESTDIR)$(libdir); \
		so_img_name=$$(readlink $(DESTDIR)$(usrlib_execdir)/libblkid.so); \
		so_img_rel_target=$$(echo $(usrlib_execdir) | sed 's,\(^/\|\)[^/][^/]*,..,g'); \
		(cd $(DESTDIR)$(usrlib_execdir) && \
			rm -f libblkid.so && \
			$(LN_S) $$so_img_rel_target$(libdir)/$$so_img_name libblkid.so); \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/libblkid.so*
