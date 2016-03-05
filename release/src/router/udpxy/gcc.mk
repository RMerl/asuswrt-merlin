# @(#) gcc makefile for udpxy project
#
# Copyright 2008 Pavel V. Cherenkov
#
#  This file is part of udpxy.
#
#  udpxy is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  udpxy is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
#

.SUFFIXES : .o .c .d

EXEC = udpxy
UPXC = upxc
CC = gcc
CFLAGS = -W -Wall -Werror --pedantic

BUILDFILE = BUILD
BUILDNO = `cat $(BUILDFILE)`

VERSIONFILE = VERSION
VERSION = `cat $(VERSIONFILE) | tr -d '"'`

CHANGEFILE = CHANGES
READMEFILE = README

ARCDIR = ..
ARCFILE = $(ARCDIR)/$(EXEC).$(VERSION)-$(BUILDNO).tgz
WL_ARCFILE = $(ARCDIR)/$(EXEC).wl.$(VERSION)-$(BUILDNO).tgz

COMMON_OPT =
DEBUG_OPT = $(COMMON_OPT) -g -DTRACE_MODULE
PROD_OPT = $(COMMON_OPT) -DNDEBUG -DTRACE_MODULE
LEAN_OPT = -DNDEBUG

MKDEPOPT = -M

SRC = udpxy.c rparse.c util.c prbuf.c ifaddr.c ctx.c mkpg.c \
	  rtp.c uopt.c dpkt.c netop.c extrn.c udpxrec.c main.c
OBJ = ${SRC:.c=.o}
DEP = ${SRC:.c=.d}

UPXC_SRC = upxc.c util.c rtp.c dpkt.c extrn.c
UPXC_OBJ = ${UPXC_SRC:.c=.o}
UPXC_DEP = upxc.dep

WLDIR = udpxy-wl

CORES = core.* core

.c.d :
	$(CC) $(CFLAGS) $(MKDEPOPT) $< -o $@

.c.o :
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $< -o $@

release:
	@echo -e "\nMaking a [release] version (use 'debug' target as an alternative)\n"
	@make all "COPT=${PROD_OPT}"
	@strip $(EXEC)

debug:
	@echo -e "\nMaking a [debug] version (use 'release' target as an alternative)\n"
	@make all "COPT=${DEBUG_OPT}"

lean:
	@echo -e "\nMaking a [lean] version (minimal size)\n"
	@make all "COPT=${LEAN_OPT}"
	@strip $(EXEC)

verify:
	@echo -e "\nVerifying all build targets\n"
	@make clean
	@make lean
	@make clean
	@make release
	@make clean
	@make debug
	make clean

all:	$(DEP) $(UPXC_DEP) $(EXEC)
# include $(UPXC) (if needed) in all target to build

deps:
	$(CC) $(CFLAGS) $(MKDEPOPT) $(SRC)

-include $(DEP) $(UPXC_DEP)

$(DEP): $(SRC)
$(UPXC_DEP) : $(UPXC_SRC)

$(EXEC) : $(OBJ)
	@rm -f $(EXEC)
	$(CC) $(CFLAGS) $(COPT) -o $(EXEC) $(OBJ)
	@ls -l $(EXEC)

$(UPXC) : $(UPXC_OBJ)
	rm -f $(UPXC)
	$(CC) $(CFLAGS) $(COPT) -o $(UPXC) $(UPXC_OBJ)
	ls -l $(UPXC)

strip:
	@echo "Stripping $(EXEC)"
	@strip $(EXEC) $(UPXC)
	@ls -l $(EXEC) $(UPXC)

touch:
	touch $(SRC) $(UPXC_SRC)

clean:
	rm -f $(CORES) $(DEP) $(OBJ) $(UPXC_DEP) $(UPXC_OBJ) $(EXEC) $(UPXC)

incbuild:
	@expr `cat $(BUILDFILE)` + 1 > $(BUILDFILE)
	@echo "Set build number to: `cat $(BUILDFILE)`"
	@make touch

tar:
	tar -cvzf $(ARCFILE) $(SRC) *.h *.mk *.txt \
		$(BUILDFILE) $(VERSIONFILE) $(CHANGEFILE) $(READMEFILE)
	@ls -l $(ARCFILE)

wl-distro:
	rm -fr $(WLDIR)
	mkdir $(WLDIR)
	cp $(SRC) $(UPXC_SRC) *.h gpl.txt \
		$(BUILDFILE) $(VERSIONFILE) $(CHANGEFILE) $(READMEFILE) $(WLDIR)
	cp wl500-gcc.mk $(WLDIR)/makefile
	tar -cvzf $(WL_ARCFILE) $(WLDIR)/*
	@ls -l $(WL_ARCFILE)
	rm -fr $(WLDIR)

# __EOF__

