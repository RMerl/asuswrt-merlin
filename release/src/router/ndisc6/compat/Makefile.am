# Makefile.am - compat/ directory Makefile for ndisc6
#
# ***********************************************************************
# *  Copyright © 2006-2008 Rémi Denis-Courmont.                         *
# *  This program is free software; you can redistribute and/or modify  *
# *  it under the terms of the GNU General Public License as published  *
# *  by the Free Software Foundation; version 2 of the license, or (at  *
# *  your option) any later version.                                    *
# *                                                                     *
# *  This program is distributed in the hope that it will be useful,    *
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
# *  See the GNU General Public License for more details.               *
# *                                                                     *
# *  You should have received a copy of the GNU General Public License  *
# *  along with this program; if not, you can get it from:              *
# *  http://www.gnu.org/copyleft/gpl.html                               *
# ***********************************************************************

AM_CPPFLAGS = -I@top_srcdir@

noinst_LIBRARIES = libcompat.a
libcompat_a_SOURCES = fixups.h dummy.c
libcompat_a_LIBADD = $(LIBOBJS)

