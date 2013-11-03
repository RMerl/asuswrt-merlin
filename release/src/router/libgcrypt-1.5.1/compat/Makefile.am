# Makefile for compat directory
# Copyright (C) 2010 Free Software Foundation, Inc.
#
# This file is part of Libgcrypt.
#
# Libgcrypt is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Libgcrypt is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>.

# Process this file with automake to produce Makefile.in

# We use this libcompat to work around problems with LIBOBJ stuff.
# For example, we need some of the compat files also in tests/ but the
# suggested way to do this (using the automake option subdir-objects)
# leads to problems with "make distclean": The distclean target in
# tests is run before the one src and it removes the deps files of the
# libobj files which are in src.  Now when it comes to run make in src
# the icnluded files are gone - bummer.  Instead of try to fix this
# issue it seems better not to use subdir-objects but build them all
# into a compat library and always link against that library.  This
# also avoids the problem that a dependency on LTLIBOBJ is not setup
# if -- disable-static was used.

# Need to include ../src in addition to top_srcdir because gcrypt.h is
# a built header.
AM_CPPFLAGS = -I../src -I$(top_srcdir)/src $(GPG_ERROR_CFLAGS)

noinst_LTLIBRARIES = libcompat.la

# We only need one file so that the library is guaranteed to have at
# least one member.
libcompat_la_SOURCES = compat.c libcompat.h
libcompat_la_DEPENDENCIES = @LTLIBOBJS@
libcompat_la_LIBADD =       @LTLIBOBJS@

# AC_LIBOBJ files are:
#    getpid.c
#    clock.c
#
