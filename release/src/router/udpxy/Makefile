# @(#) GNU Makefile for udpxy project
#
# Copyright 2008-2012 Pavel V. Cherenkov
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

EXEC := udpxy
UDPXREC := udpxrec

STRIP := /usr/bin/strip
GZIP  := /bin/gzip

MKDEPOPT := -MM
DEBUG_ON := -g

ALL_CFLAGS = -W -Wall -Werror --pedantic $(CFLAGS)

SYSTEM=$(shell uname 2>/dev/null)
ifneq (,$(filter $(SYSTEM),FreeBSD Darwin))
MAKE := gmake
GZIP := /usr/bin/gzip
endif

BUILDFILE = BUILD
BUILDNO := `cat $(BUILDFILE)`

BTYPEFILE = BLDTYPE
BTYPE := `cat $(BTYPEFILE) | tr -d '"'`

VERSIONFILE = VERSION
VERSION := `cat $(VERSIONFILE) | tr -d '"'`

PATCHFILE = PATCH
PATCHNO := `cat $(PATCHFILE)`

CHANGEFILE := CHANGES
READMEFILE := README

ifndef PREFIX
	PREFIX := /usr/local
endif

INSTALLROOT := $(DESTDIR)$(PREFIX)

ARCDIR = ..
ARCFILE := $(ARCDIR)/$(EXEC).$(VERSION).$(BUILDNO)-$(PATCHNO)-$(BTYPE).tar.gz

DEBUG_OPT  := $(COPT) $(DEBUG_ON) -DTRACE_MODULE
PROD_OPT   := $(COPT) -DNDEBUG -DTRACE_MODULE
LEAN_OPT   := $(COPT) -DNDEBUG

UTILDIR := util

SLOOP_FILES := sloop_p.c sloop_s.c

UDPXY_MAN1   := udpxy.1
UDPXREC_MAN1 := udpxrec.1

UDPXY_MANPAGE_EN := doc/en/$(UDPXY_MAN1)
# UDPXY_MANPAGE_RU := doc/ru/$(UDPXY_MAN1)

UDPXREC_MANPAGE_EN := doc/en/$(UDPXREC_MAN1)
# UDPXYREC_MANPAGE_RU := doc/ru/$(UDPXREC_MAN1)

MANPAGE_DIR := $(DESTDIR)/usr/share/man/man1

DOCDIR := doc
DOCFILES := *.txt $(DOCDIR)

SRC := udpxy.c sloop.c rparse.c util.c prbuf.c ifaddr.c ctx.c mkpg.c \
	rtp.c uopt.c dpkt.c netop.c extrn.c main.c

ifneq (yes,$(NO_UDPXREC))
	SRC   += udpxrec.c
	CDEFS += -DUDPXREC_MOD
endif

OBJ := ${SRC:.c=.o}

DEPFILE := $(EXEC).dep
CORES := core.* core

.PHONY: clean distclean distro install uninstall all \
debug ldebug rdebug lean verify touch incbuild 

.c.o :
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) $(CDEFS) $(COPT) -c $< -o $@

release:
	@echo -e "\nMaking a [release] version (use 'debug' target as an alternative)\n"
	@$(MAKE) all "COPT=${PROD_OPT}" "CFLAGS=${CFLAGS}" "CDEFS=${CDEFS}" "CPPFLAGS=${CPPFLAGS}"

debug:
	@echo -e "\nMaking a [debug] version (use 'release' target as an alternative)\n"
	@$(MAKE) all "COPT=${DEBUG_OPT}" "CFLAGS=${CFLAGS}" "CDEFS=${CDEFS}" "CPPFLAGS=${CPPFLAGS}"

lean:
	@echo -e "\nMaking a [lean] version (minimal size)\n"
	@$(MAKE) all "COPT=${LEAN_OPT}" "CFLAGS=${CFLAGS}" "CDEFS=${CDEFS}" "CPPFLAGS=${CPPFLAGS}"

ldebug:
	@echo -e "\nMaking a [lean] version with debug info\n"
	@$(MAKE) all "COPT=${LEAN_OPT} ${DEBUG_ON}" "CFLAGS=${CFLAGS}" "CDEFS=${CDEFS}" "CPPFLAGS=${CPPFLAGS}"

rdebug:
	@echo -e "\nMaking a [release] version with debug info\n"
	@$(MAKE) all "COPT=${PROD_OPT} ${DEBUG_ON}" "CDEFS=${CDEFS}" "CPPFLAGS=${CPPFLAGS}"

verify:
	@echo -e "\nVerifying all build targets\n"
	@$(MAKE) clean
	@$(MAKE) release
	@$(MAKE) clean
	@$(MAKE) lean
	@$(MAKE) clean
	@$(MAKE) ldebug
	@$(MAKE) clean
	@$(MAKE) rdebug
	@$(MAKE) clean
	@$(MAKE) debug
	@$(MAKE) clean

mk_deps := $(CC) $(CFLAGS) $(CDEFS) $(MKDEPOPT) $(SRC)

all:	$(DEPFILE) $(EXEC)

$(DEPFILE): $(SRC)
	$(mk_deps) > $(DEPFILE)

$(EXEC) : $(DEPFILE) $(OBJ)
	@rm -f $(EXEC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(COPT) -o $(EXEC) $(OBJ)
	@ls -l $(EXEC)
ifneq (yes, $(NO_UDPXREC))
	@rm -f $(UDPXREC)
	ln -s $(EXEC) $(UDPXREC)
endif

clean:
	rm -f $(CORES) $(DEPFILE) $(OBJ) $(EXEC) $(UDPXREC)

distclean: clean

incbuild:
	@expr `cat $(BUILDFILE)` + 1 > $(BUILDFILE)
	@echo "Set build number to: `cat $(BUILDFILE)`"
	@$(MAKE) touch

DISTRO_DIR := udpxy-$(VERSION).$(BUILDNO)-$(PATCHNO)
DISTRO_FILES := $(SRC) $(SLOOP_FILES) *.h Makefile $(DOCFILES) $(BUILDFILE) $(BTYPEFILE) \
	$(VERSIONFILE) $(CHANGEFILE) $(READMEFILE) $(PATCHFILE)

distro: dist

dist:
	@rm -fr $(DISTRO_DIR)
	@mkdir  $(DISTRO_DIR)
	@mkdir  $(DISTRO_DIR)/$(UTILDIR)
	cp -R $(DISTRO_FILES) $(DISTRO_DIR)
	tar -cvzf $(ARCFILE) $(DISTRO_DIR)
	@ls -l $(ARCFILE)
	@rm -fr $(DISTRO_DIR)

install: $(EXEC)
	@mkdir -p -m 755 $(INSTALLROOT)/bin
	@cp $(EXEC) $(INSTALLROOT)/bin
	@ls -l $(INSTALLROOT)/bin/$(EXEC)
	@mkdir -p -m 755 $(MANPAGE_DIR)
	@$(GZIP) -c $(UDPXY_MANPAGE_EN) > $(MANPAGE_DIR)/$(UDPXY_MAN1).gz
ifneq (yes, $(NO_UDPXREC))
	@cp $(UDPXREC) $(INSTALLROOT)/bin/$(UDPXREC)
	@ls -l $(INSTALLROOT)/bin/$(UDPXREC)
	@$(GZIP) -c $(UDPXREC_MANPAGE_EN) > $(MANPAGE_DIR)/$(UDPXREC_MAN1).gz
endif
	@echo "Installation of udpxy is complete"

install-strip: install
	$(STRIP) $(INSTALLROOT)/bin/$(EXEC)
	@ls -l $(INSTALLROOT)/bin/$(EXEC)
	@echo "udpxy executable stripped"

uninstall:
	@rm -f $(INSTALLROOT)/bin/$(EXEC) $(INSTALLROOT)/bin/$(UDPXREC)
	@rm -f $(MANPAGE_DIR)/$(UDPXY_MAN1).gz $(MANPAGE_DIR)/$(UDPXREC_MAN1).gz
	@echo "udpxy successfully uninstalled"

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPFILE)
endif

# __EOF__

