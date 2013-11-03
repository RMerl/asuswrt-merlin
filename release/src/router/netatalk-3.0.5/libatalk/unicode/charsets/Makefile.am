# Makefile.am for libatalk/unicode/charsets

noinst_LTLIBRARIES = libcharsets.la

AM_CFLAGS = -I$(top_srcdir)/sys @CFLAGS@ @ICONV_CFLAGS@
LIBS = 

libcharsets_la_SOURCES = \
	mac_roman.c	\
	mac_greek.c	\
	mac_hebrew.c	\
	mac_centraleurope.c	\
	mac_turkish.c \
	mac_cyrillic.c \
	mac_japanese.c \
	mac_chinese_trad.c \
	mac_chinese_simp.c \
	mac_korean.c \
	generic_cjk.c \
	generic_mb.c

noinst_HEADERS = \
	mac_roman.h \
	mac_greek.h \
	mac_centraleurope.h \
	mac_hebrew.h \
	mac_turkish.h \
	mac_cyrillic.h \
	mac_japanese.h \
	mac_chinese_trad.h \
	mac_chinese_simp.h \
	mac_korean.h \
	generic_cjk.h \
	generic_mb.h
