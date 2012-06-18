## Process this file with automake to create Makefile.in
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)

noinst_PROGRAMS = align chksum promote sizes

align_SOURCES = align.c
chksum_SOURCES = chksum.c
promote_SOURCES = promote.c
sizes_SOURCES = sizes.c

align_LDADD = ../src/liblzo2.la
chksum_LDADD = ../src/liblzo2.la

