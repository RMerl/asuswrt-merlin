EXTRA_DIST =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =

include canon/Makefile-files
include fuji/Makefile-files
include olympus/Makefile-files
include pentax/Makefile-files

# The -no-undefined makes it possible to build DLLs for Windows,
# or shared libraries for Tru64 or AIX (according to the autobook
# chapter on "Portable Library Design"). It doesn't seem to hurt
# elsewhere, so we can leave it in.
lib_LTLIBRARIES += libexif.la
libexif_la_LDFLAGS = \
	-export-symbols $(srcdir)/libexif.sym \
	-no-undefined -version-info @LIBEXIF_VERSION_INFO@
libexif_la_SOURCES =		\
	exif-byte-order.c	\
	exif-content.c		\
	exif-data.c		\
	exif-entry.c		\
	exif-format.c		\
	exif-ifd.c		\
	exif-loader.c		\
	exif-log.c		\
	exif-mem.c		\
	exif-mnote-data.c	\
	exif-mnote-data-priv.h	\
	exif-tag.c		\
	exif-utils.c		\
	i18n.h
libexif_la_DEPENDENCIES = \
	$(srcdir)/libexif.sym \
	libmnote-canon.la		\
	libmnote-fuji.la		\
	libmnote-olympus.la	\
	libmnote-pentax.la
libexif_la_LIBADD =			\
	$(LTLIBINTL)			\
	libmnote-canon.la		\
	libmnote-fuji.la	\
	libmnote-olympus.la	\
	libmnote-pentax.la

libexifincludedir = $(includedir)/libexif
libexifinclude_HEADERS = 	\
	exif-byte-order.h	\
	exif-content.h		\
	exif-data.h		\
	exif-data-type.h \
	exif-entry.h		\
	exif-format.h		\
	exif-ifd.h		\
	exif-loader.h		\
	exif-log.h		\
	exif-mem.h		\
	exif-mnote-data.h	\
	exif-tag.h		\
	exif-utils.h		\
	_stdint.h

EXTRA_DIST += exif-system.h exif.h

EXTRA_DIST += libexif.sym

DISTCLEANFILES = _stdint.h
