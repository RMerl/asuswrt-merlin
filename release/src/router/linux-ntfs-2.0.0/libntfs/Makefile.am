#
# Before making a release, the LTVERSION string should be modified.
# The string is of the form CURRENT:REVISION:AGE.
#
# CURRENT (C)
#	The most recent interface number that this library implements.
#
# REVISION (R)
#	The implementation number that this library implements.
#
# AGE (A)
#	The difference between the newest and oldest interfaces that this
#	library implements. In other works, the library implements all the
#	interface numbers in the range from number 'CURRENT - AGE' to
#	'CURRENT'.
#
# This means that:
#
#   - If interfaces have been changed or added, but binary compatibility has
#     been preserved, change to C+1:0:A+1
#
#   - If binary compatibility has been broken (eg removed or changed
#     interfaces) change to C+1:0:0
#
#   - If the interface is the same as the previous version, change to C:R+1:A
#

# For LTVERSION_LIBNTFS see configure.ac!
LTVERSION_LIBNTFS_GNOMEVFS = 1:0:0


linux_ntfsincludedir = -I$(top_srcdir)/include/ntfs

lib_LTLIBRARIES = libntfs.la

libntfs_la_LDFLAGS = -version-info $(LTVERSION_LIBNTFS) -no-undefined
libntfs_la_CFLAGS = $(LIBNTFS_CFLAGS) \
	-DLTVERSION_LIBNTFS=\"$(LTVERSION_LIBNTFS)\"

if ENABLE_CRYPTO
libntfs_la_LDFLAGS += `libgnutls-config --libs` `pkg-config --libs libconfig`
libntfs_la_CFLAGS += `libgnutls-config --cflags` `pkg-config --cflags libconfig`
endif

libntfs_la_SOURCES =	\
	attrib.c	\
	attrlist.c	\
	bitmap.c	\
	bootsect.c	\
	collate.c	\
	compat.c	\
	compress.c	\
	crypto.c	\
	debug.c		\
	device.c	\
	device_io.c	\
	dir.c		\
	index.c		\
	inode.c		\
	lcnalloc.c	\
	logfile.c	\
	logging.c	\
	misc.c		\
	mft.c		\
	mst.c		\
	runlist.c	\
	security.c	\
	unistr.c	\
	version.c	\
	volume.c

if ENABLE_GNOME_VFS

gnomevfsmoduleslibdir = $(libdir)/gnome-vfs-2.0/modules
gnomevfsmoduleslib_LTLIBRARIES = libntfs-gnomevfs.la

gnomevfsmodulesconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
gnomevfsmodulesconf_DATA = libntfs.conf

endif

libntfs_gnomevfs_la_LDFLAGS = -version-info $(LTVERSION_LIBNTFS_GNOMEVFS)
libntfs_gnomevfs_la_LIBADD = libntfs.la
libntfs_gnomevfs_la_LIBS = $(LIBNTFS_GNOMEVFS_LIBS)
libntfs_gnomevfs_la_CFLAGS = $(LIBNTFS_GNOMEVFS_CFLAGS)
libntfs_gnomevfs_la_SOURCES =	\
	gnome-vfs-method.c	\
	gnome-vfs-module.c

man_MANS = libntfs.8 \
	libntfs-gnomevfs.8

AM_CPPFLAGS = $(linux_ntfsincludedir) $(all_includes)

EXTRA_DIST = unix_io.c win32_io.c libntfs.conf.in config

MAINTAINERCLEANFILES = Makefile.in

libs:	$(lib_LTLIBRARIES)
