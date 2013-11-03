# Makefile.am for libatalk/util/

noinst_LTLIBRARIES = libunicode.la

AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/sys @ICONV_CFLAGS@

SUBDIRS = charsets

LIBUNICODE_DEPS = charsets/libcharsets.la 

libunicode_la_SOURCES = \
	util_unistr.c	\
	iconv.c		\
	charcnv.c	\
	utf8.c		\
	utf16_case.c

libunicode_la_LIBADD = $(LIBUNICODE_DEPS)

noinst_HEADERS = utf16_casetable.h precompose.h byteorder.h

LIBS=@ICONV_LIBS@
