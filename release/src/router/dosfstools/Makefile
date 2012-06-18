# Makefile
#
# Copyright (C) 2008 Daniel Baumann <daniel@debian.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# On Debian systems, the complete text of the GNU General Public License
# can be found in /usr/share/common-licenses/GPL-3 file.

DESTDIR =
PREFIX = /usr/local
SBINDIR = $(PREFIX)/sbin
DOCDIR = $(PREFIX)/share/doc
MANDIR = $(PREFIX)/share/man

#OPTFLAGS = -O2 -fomit-frame-pointer -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
OPTFLAGS = -O2 -fomit-frame-pointer $(shell getconf LFS_CFLAGS)
#WARNFLAGS = -Wall -pedantic -std=c99
WARNFLAGS = -Wall
DEBUGFLAGS = -g
CFLAGS += $(OPTFLAGS) $(WARNFLAGS) $(DEBUGFLAGS)

VPATH = src

all: build

build: dosfsck dosfslabel mkdosfs

dosfsck: boot.o check.o common.o fat.o file.o io.o lfn.o dosfsck.o

dosfslabel: boot.o check.o common.o fat.o file.o io.o lfn.o dosfslabel.o

mkdosfs: mkdosfs.o

rebuild: distclean build

#install: install-bin install-doc install-man
install: dosfsck mkdosfs
	install -D mkdosfs $(INSTALLDIR)/sbin/mkdosfs
	$(STRIP) $(INSTALLDIR)/sbin/mkdosfs

	install -D dosfsck $(INSTALLDIR)/sbin/dosfsck
	$(STRIP) $(INSTALLDIR)/sbin/dosfsck
	cd $(INSTALLDIR)/sbin && ln -sf dosfsck fsck.msdos
	cd $(INSTALLDIR)/sbin && ln -sf dosfsck fsck.vfat

install-bin: build
	install -d -m 0755 $(DESTDIR)/$(SBINDIR)
	install -m 0755 dosfsck dosfslabel mkdosfs $(DESTDIR)/$(SBINDIR)

	ln -sf dosfsck $(DESTDIR)/$(SBINDIR)/fsck.msdos
	ln -sf dosfsck $(DESTDIR)/$(SBINDIR)/fsck.vfat
	ln -sf mkdosfs $(DESTDIR)/$(SBINDIR)/mkfs.msdos
	ln -sf mkdosfs $(DESTDIR)/$(SBINDIR)/mkfs.vfat

install-doc:
	install -d -m 0755 $(DESTDIR)/$(DOCDIR)/dosfstools
	install -p -m 0644 ChangeLog doc/* $(DESTDIR)/$(DOCDIR)/dosfstools

install-man:
	install -d -m 0755 $(DESTDIR)/$(MANDIR)/man8
	install -p -m 0644 man/*.8 $(DESTDIR)/$(MANDIR)/man8

	ln -sf dosfsck.8 $(DESTDIR)/$(MANDIR)/man8/fsck.msdos.8
	ln -sf dosfsck.8 $(DESTDIR)/$(MANDIR)/man8/fsck.vfat.8
	ln -sf mkdosfs.8 $(DESTDIR)/$(MANDIR)/man8/mkfs.msdos.8
	ln -sf mkdosfs.8 $(DESTDIR)/$(MANDIR)/man8/mkfs.vfat.8

uninstall: uninstall-bin uninstall-doc uninstall-man

uninstall-bin:
	rm -f $(DESTDIR)/$(SBINDIR)/dosfsck
	rm -f $(DESTDIR)/$(SBINDIR)/dosfslabel
	rm -f $(DESTDIR)/$(SBINDIR)/mkdosfs

	rm -f $(DESTDIR)/$(SBINDIR)/fsck.msdos
	rm -f $(DESTDIR)/$(SBINDIR)/fsck.vfat
	rm -f $(DESTDIR)/$(SBINDIR)/mkfs.msdos
	rm -f $(DESTDIR)/$(SBINDIR)/mkfs.vfat

	rmdir --ignore-fail-on-non-empty $(DESTDIR)/$(SBINDIR)

uninstall-doc:
	rm -rf $(DESTDIR)/$(DOCDIR)/dosfstools

	rmdir --ignore-fail-on-non-empty $(DESTDIR)/$(DOCDIR)

uninstall-man:
	rm -f $(DESTDIR)/$(MANDIR)/man8/dosfsck.8
	rm -f $(DESTDIR)/$(MANDIR)/man8/dosfslabel.8
	rm -f $(DESTDIR)/$(MANDIR)/man8/mkdosfs.8

	rm -f $(DESTDIR)/$(MANDIR)/man8/fsck.msdos.8
	rm -f $(DESTDIR)/$(MANDIR)/man8/fsck.vfat.8
	rm -f $(DESTDIR)/$(MANDIR)/man8/mkfs.msdos.8
	rm -f $(DESTDIR)/$(MANDIR)/man8/mkfs.vfat.8

	rmdir --ignore-fail-on-non-empty $(DESTDIR)/$(MANDIR)/man8
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/$(MANDIR)

reinstall: distclean install

clean:
	rm -f *.o dosfsck dosfslabel mkdosfs

distclean: clean
	rm -f dosfsck dosfslabel mkdosfs

.PHONY: build rebuild install install-bin install-doc install-man uninstall uninstall-bin uninstall-doc uninstall-man reinstall clean distclean
