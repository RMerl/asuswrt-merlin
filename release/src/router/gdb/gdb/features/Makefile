# Copyright (C) 2007 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


# This file requires GNU make!

# This Makefile updates files in ../regformats from their XML master
# copies.  Because this relies on xsltproc, it is not normally run
# during the build process; it should be run by hand when one of the
# affected XML files is changed, and the results should be kept in the
# GDB repository.

WHICH = arm-with-iwmmxt mips-linux mips64-linux

# Record which registers should be sent to GDB by default after stop.
arm-with-iwmmxt-expedite = r11,sp,pc
mips-linux-expedite = r29,pc
mips64-linux-expedite = r29,pc

XSLTPROC = xsltproc
outdir = ../regformats
OUTPUTS = $(patsubst %,$(outdir)/%.dat,$(WHICH))

all: $(OUTPUTS)

$(outdir)/%.dat: %.xml number-regs.xsl sort-regs.xsl gdbserver-regs.xsl
	echo "# DO NOT EDIT: generated from $<" > $(outdir)/$*.tmp
	echo "name:`echo $* | sed 's/-/_/g'`" >> $(outdir)/$*.tmp
	echo "expedite:$($*-expedite)" >> $(outdir)/$*.tmp
	$(XSLTPROC) --xinclude number-regs.xsl $< | \
	  $(XSLTPROC) sort-regs.xsl - | \
	  $(XSLTPROC) gdbserver-regs.xsl - >> $(outdir)/$*.tmp
	sh ../../move-if-change $(outdir)/$*.tmp $(outdir)/$*.dat

# Other dependencies.
$(outdir)/arm-with-iwmmxt.dat: arm-core.xml xscale-iwmmxt.xml
