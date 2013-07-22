include $(top_srcdir)/config/include-Makefile.am

AM_CPPFLAGS += -I$(ul_libuuid_srcdir)

noinst_PROGRAMS = tst_uuid
tst_uuid_LDADD = libuuid.la $(SOCKET_LIBS) #$(ul_libuuid_la)

# includes
uuidincdir = $(includedir)/uuid
uuidinc_HEADERS = uuid.h

usrlib_exec_LTLIBRARIES = libuuid.la
libuuid_la_SOURCES = clear.c compare.c copy.c gen_uuid.c \
           isnull.c pack.c parse.c unpack.c unparse.c uuidd.h \
           uuidd.h uuidP.h uuid_time.c $(uuidinc_HEADERS)

libuuid_la_DEPENDENCIES = uuid.sym

libuuid_la_LIBADD = $(SOCKET_LIBS)

libuuid_la_LDFLAGS = -Wl,--version-script=$(ul_libuuid_srcdir)/uuid.sym \
                     -version-info $(LIBUUID_VERSION_INFO)

EXTRA_DIST = uuid.sym gen_uuid_nt.c

# move lib from $(usrlib_execdir) to $(libdir) if needed
install-exec-hook:
	if test "$(usrlib_execdir)" != "$(libdir)"; then \
		mkdir -p $(DESTDIR)$(libdir); \
		mv $(DESTDIR)$(usrlib_execdir)/libuuid.so.* $(DESTDIR)$(libdir); \
		so_img_name=$$(readlink $(DESTDIR)$(usrlib_execdir)/libuuid.so); \
		so_img_rel_target=$$(echo $(usrlib_execdir) | sed 's,\(^/\|\)[^/][^/]*,..,g'); \
		(cd $(DESTDIR)$(usrlib_execdir) && \
			rm -f libuuid.so && \
			$(LN_S) $$so_img_rel_target$(libdir)/$$so_img_name libuuid.so); \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/libuuid.so*

