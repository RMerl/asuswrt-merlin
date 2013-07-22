include $(top_srcdir)/config/include-Makefile.am

AM_CPPFLAGS += -I$(ul_libmount_incdir) \
	       -I$(ul_libmount_srcdir) \
	       -I$(ul_libblkid_incdir)

# includes
mountincdir = $(includedir)/libmount
nodist_mountinc_HEADERS = libmount.h

usrlib_exec_LTLIBRARIES = libmount.la
libmount_la_SOURCES =	mountP.h version.c utils.c test.c init.c cache.c \
			optstr.c optmap.c iter.c lock.c \
			fs.c tab.c tab_parse.c tab_update.c tab_diff.c \
			context.c context_mount.c context_umount.c \
			context_loopdev.c \
			$(mountinc_HEADERS) \
			$(top_srcdir)/lib/at.c \
			$(top_srcdir)/include/list.h \
			$(top_srcdir)/lib/mangle.c \
			$(top_srcdir)/lib/canonicalize.c \
			$(top_srcdir)/lib/strutils.c \
			$(top_srcdir)/lib/env.c \
			$(top_srcdir)/lib/loopdev.c \
			$(top_srcdir)/lib/sysfs.c \
			$(top_srcdir)/lib/linux_version.c

nodist_libmount_la_SOURCES = mountP.h

libmount_la_LIBADD = $(ul_libblkid_la) $(SELINUX_LIBS)

libmount_la_DEPENDENCIES = $(libmount_la_LIBADD) libmount.sym libmount.h.in

libmount_la_LDFLAGS = -Wl,--version-script=$(ul_libmount_srcdir)/libmount.sym \
                      -version-info $(LIBMOUNT_VERSION_INFO)

TESTS_LIBS = -luuid

if HAVE_SELINUX
TESTS_LIBS += $(SELINUX_LIBS)
endif

EXTRA_DIST = libmount.sym libmount.h.in
CLEANFILES = $(tests)

# move lib from $(usrlib_execdir) to $(libdir) if needed
install-exec-hook:
	if test "$(usrlib_execdir)" != "$(libdir)"; then \
		mkdir -p $(DESTDIR)$(libdir); \
		mv $(DESTDIR)$(usrlib_execdir)/libmount.so.* $(DESTDIR)$(libdir); \
		so_img_name=$$(readlink $(DESTDIR)$(usrlib_execdir)/libmount.so); \
		so_img_rel_target=$$(echo $(usrlib_execdir) | sed 's,\(^/\|\)[^/][^/]*,..,g'); \
		(cd $(DESTDIR)$(usrlib_execdir) && \
			rm -f libmount.so && \
			$(LN_S) $$so_img_rel_target$(libdir)/$$so_img_name libmount.so); \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/libmount.so*


tests = test_version test_cache test_optstr test_lock \
	test_tab test_utils test_tab_update test_context \
	test_tab_diff

tests: all $(tests)
test_%: %.c
	$(AM_V_CC)$(COMPILE) -DTEST_PROGRAM $< .libs/libmount.a \
		$(ul_libblkid_builddir)/.libs/libblkid.a -o $@ \
		$(TESTS_LIBS)

