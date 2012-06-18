
MAINTAINERCLEANFILES = $(srcdir)/Makefile.in

if INSTALL_LIBRARY
rootlib_LTLIBRARIES=#Create directory
lib_LTLIBRARIES    = libntfs-3g.la
pkgconfig_DATA     = libntfs-3g.pc
else
noinst_LTLIBRARIES = libntfs-3g.la
endif

libntfs_3g_la_CFLAGS  = $(AM_CFLAGS) -I$(top_srcdir)/include/ntfs-3g

libntfs_3g_la_LDFLAGS = -version-info $(LIBNTFS_3G_VERSION) -no-undefined

if FUSE_INTERNAL
libntfs_3g_la_LIBADD  = $(top_builddir)/libfuse-lite/libfuse-lite.la
endif

libntfs_3g_la_SOURCES =	\
	acls.c 	\
	attrib.c 	\
	attrlist.c 	\
	bitmap.c 	\
	bootsect.c 	\
	cache.c 	\
	collate.c 	\
	compat.c 	\
	compress.c 	\
	debug.c 	\
	device.c 	\
	dir.c 		\
	efs.c 		\
	index.c 	\
	inode.c 	\
	lcnalloc.c 	\
	logfile.c 	\
	logging.c 	\
	mft.c 		\
	misc.c 		\
	mst.c 		\
	object_id.c 	\
	reparse.c 	\
	runlist.c 	\
	security.c 	\
	unistr.c 	\
	volume.c

if NTFS_DEVICE_DEFAULT_IO_OPS
if WINDOWS
libntfs_3g_la_SOURCES += win32_io.c
else
libntfs_3g_la_SOURCES += unix_io.c
endif
endif

# We may need to move .so files to root
# And create ldscript or symbolic link from /usr
install-exec-hook: install-rootlibLTLIBRARIES
if INSTALL_LIBRARY
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(MV) -f "$(DESTDIR)/$(libdir)"/libntfs-3g.so* "$(DESTDIR)/$(rootlibdir)";  \
	fi
if GENERATE_LDSCRIPT
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(install_sh_PROGRAM) "libntfs-3g.script.so" "$(DESTDIR)/$(libdir)/libntfs-3g.so"; \
	fi
else
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(LN_S) "$(rootlibdir)/libntfs-3g.so" "$(DESTDIR)/$(libdir)/libntfs-3g.so"; \
	fi
endif
endif

uninstall-local:
if INSTALL_LIBRARY
	$(RM) -f "$(DESTDIR)/$(rootlibdir)"/libntfs-3g.so*
endif

