# dnsmasq is Copyright (c) 2000-2010 Simon Kelley
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 dated June, 1991, or
#  (at your option) version 3 dated 29 June, 2007.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#    
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

PREFIX = /usr/local
BINDIR = ${PREFIX}/sbin
MANDIR = ${PREFIX}/share/man
LOCALEDIR = ${PREFIX}/share/locale

PKG_CONFIG = pkg-config
INSTALL = install
MSGMERGE = msgmerge
MSGFMT = msgfmt
XGETTEXT = xgettext

CFLAGS = -Wall -W -O2

#CFLAGS += -I../shared -I../../../include
#ASUS_LDFLAGS = -L../shared -lshared -L../../nvram -lnvram

CFLAGS = -I$(TOP)/shared -I$(SRCBASE)/include
ASUS_LDFLAGS = -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared
#################################################################

SRC = src
PO = po
MAN = man

DNSMASQ_CFLAGS=`echo $(COPTS) | ../bld/pkg-wrapper HAVE_DBUS $(PKG_CONFIG) --cflags dbus-1` 
DNSMASQ_LIBS=  `echo $(COPTS) | ../bld/pkg-wrapper HAVE_DBUS $(PKG_CONFIG) --libs dbus-1` 
SUNOS_LIBS= `if uname | grep SunOS 2>&1 >/dev/null; then echo -lsocket -lnsl -lposix4; fi`

OBJS = cache.o rfc1035.o util.o option.o forward.o network.o \
       dnsmasq.o dhcp.o lease.o rfc2131.o netlink.o dbus.o bpf.o \
       helper.o tftp.o log.o

all :
	@cd $(SRC) && $(MAKE) \
 BUILD_CFLAGS="$(DNSMASQ_CFLAGS)" \
 BUILD_LIBS="$(DNSMASQ_LIBS) $(SUNOS_LIBS)" \
 -f ../Makefile dnsmasq 

clean :
	rm -f *~ $(SRC)/*.mo contrib/*/*~ */*~ $(SRC)/*.pot 
	rm -f $(SRC)/*.o $(SRC)/dnsmasq.a $(SRC)/dnsmasq core */core

install : all install-common

install-common :
	$(INSTALL) -d $(DESTDIR)$(BINDIR) -d $(DESTDIR)$(MANDIR)/man8
	$(INSTALL) -m 644 $(MAN)/dnsmasq.8 $(DESTDIR)$(MANDIR)/man8 
	$(INSTALL) -m 755 $(SRC)/dnsmasq $(DESTDIR)$(BINDIR)

all-i18n :
	@cd $(SRC) && $(MAKE) \
 I18N=-DLOCALEDIR='\"$(LOCALEDIR)\"' \
 BUILD_CFLAGS="$(DNSMASQ_CFLAGS) `$(PKG_CONFIG) --cflags libidn`" \
 BUILD_LIBS="$(DNSMASQ_LIBS) $(SUNOS_LIBS) `$(PKG_CONFIG) --libs libidn`"  \
 -f ../Makefile dnsmasq
	@cd $(PO); for f in *.po; do \
		cd ../$(SRC) && $(MAKE) \
 -f ../Makefile $${f%.po}.mo; \
	done

install-i18n : all-i18n install-common
	cd $(SRC); ../bld/install-mo $(DESTDIR)$(LOCALEDIR) $(INSTALL)
	cd $(MAN); ../bld/install-man $(DESTDIR)$(MANDIR) $(INSTALL)

merge :
	@cd $(SRC) && $(MAKE) -f ../Makefile dnsmasq.pot
	@cd $(PO); for f in *.po; do \
		echo -n msgmerge $$f && $(MSGMERGE) --no-wrap -U $$f ../$(SRC)/dnsmasq.pot; \
	done


# rules below are targets in recusive makes with cwd=$(SRC)

.c.o:
	$(CC) $(CFLAGS) $(COPTS) $(I18N) $(BUILD_CFLAGS) $(RPM_OPT_FLAGS) -c $<

dnsmasq : $(OBJS)
	$(CC) $(LDFLAGS) $(ASUS_LDFLAGS) -o $@ $(OBJS) $(BUILD_LIBS) $(LIBS) 

dnsmasq.pot : $(OBJS:.o=.c) dnsmasq.h config.h
	$(XGETTEXT) -d dnsmasq --foreign-user --omit-header --keyword=_ -o $@ -i $(OBJS:.o=.c)

%.mo : ../po/%.po dnsmasq.pot
	$(MSGMERGE) -o - ../po/$*.po dnsmasq.pot | $(MSGFMT) -o $*.mo -


.PHONY : all clean install install-common all-i18n install-i18n merge 
