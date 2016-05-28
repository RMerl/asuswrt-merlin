#!make
#
# Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
#
# http://www.sgi.com
#
# For further information regarding this notice, see:
#
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

SHELL   = sh

include /etc/pcp.conf

IAM	= samba
CFILES	= $(IAM).c

LIBTARGET = pmda_$(IAM).so
CMDTARGET = pmda$(IAM)
TARGETS = $(LIBTARGET) $(CMDTARGET)

DEBUG   = -DDEBUG
CFLAGS	= $(DEBUG)
LDOPTS	=
LDLIBS	= -lpcp_pmda -lpcp
DSOOPTS	= -shared
LDIRT	= profile.h metrics.h so_locations *.log help.dir help.pag *.pmda_$(IAM).so

PROFILEHEADER = ../source/include/smbprofile.h

INSTALL	= install
CC	= cc

default: $(TARGETS)

install:	default

$(CMDTARGET):	profile.h metrics.h $(CFILES)
	$(CC) $(CFLAGS) $(CFILES) $(LDOPTS) $(LDLIBS) -o $@

$(LIBTARGET):	profile.h metrics.h $(CFILES)
	$(CC) $(CFLAGS) $(DSOOPTS) $(LDOPTS) $(CFILES) $(LDLIBS) -o $@

metrics.h:	profile.h mkheader.pl
	./mkheader.pl

profile.h: 	$(PROFILEHEADER)
	ln -s $(PROFILEHEADER) $@

clobber clean:
	rm -f $(LDIRT) $(TARGETS)
