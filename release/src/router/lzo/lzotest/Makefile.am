## Process this file with automake to create Makefile.in
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)

noinst_PROGRAMS = lzotest

lzotest_SOURCES = lzotest.c
lzotest_LDADD = ../src/liblzo2.la
lzotest_DEPENDENCIES = ../src/liblzo2.la

EXTRA_DIST = asm.h db.h wrap.h wrapmisc.h mygetopt.ch mygetopt.h

