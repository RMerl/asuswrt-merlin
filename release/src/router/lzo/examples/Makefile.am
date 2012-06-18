## Process this file with automake to create Makefile.in
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)

noinst_PROGRAMS = dict lzopack overlap precomp precomp2 simple

dict_SOURCES = dict.c
lzopack_SOURCES = lzopack.c
overlap_SOURCES = overlap.c
precomp_SOURCES = precomp.c
precomp2_SOURCES = precomp2.c
simple_SOURCES = simple.c

LDADD = ../src/liblzo2.la
DEPENDENCIES = ../src/liblzo2.la

EXTRA_DIST = portab.h portab_a.h

