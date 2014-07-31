# $Id: Makefile,v 1.83 2014/04/18 08:24:41 nanard Exp $
# MiniUPnP project
# http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
# Author: Thomas Bernard
#
# Makefile for miniupnpd (MiniUPnP daemon)
#
# This Makefile should work for *BSD and SunOS/Solaris.
# On Mac OS X, use "bsdmake" to build.
# This Makefile is NOT compatible with GNU Make.
# Linux users, please use Makefile.linux :
#  make -f Makefile.linux
#
# options can be passed to genconfig.sh through CONFIG_OPTIONS :
# $ CONFIG_OPTIONS="--ipv6 --igd2" make
#

CFLAGS ?= -pipe -Os
#CFLAGS = -pipe -O -g -DDEBUG
#CFLAGS += -ansi
CFLAGS += -Wall
CFLAGS += -W
CFLAGS += -Wstrict-prototypes
#CFLAGS += -Wdeclaration-after-statement
#CFLAGS += -Wno-missing-field-initializers
CFLAGS += -fno-common
CC ?= gcc
RM = rm -f
MV = mv
INSTALL = install
STRIP = strip

# OSNAME and FWNAME are used for building OS or FW dependent code.
OSNAME != uname -s
ARCH != uname -m
.ifndef FWNAME
#.if exists(/usr/include/net/pfvar.h)
#FWNAME = pf
#.else
#FWNAME = ipf
#.endif

.if $(OSNAME) == "OpenBSD"
FWNAME = pf
.endif

# better way to find if we are using ipf or pf
.if $(OSNAME) == "FreeBSD"
.if exists(/etc/rc.subr) && exists(/etc/rc.conf)
FWNAME != . /etc/rc.subr; . /etc/rc.conf; \
          if checkyesno ipfilter_enable; then \
          echo "ipf"; elif checkyesno pf_enable; then \
          echo "pf"; elif checkyesno firewall_enable; then \
          echo "ipfw"; else echo "pf"; fi
.else
FWNAME = pf
.endif
.endif

.if $(OSNAME) == "NetBSD"
.if exists(/etc/rc.subr) && exists(/etc/rc.conf)
FWNAME != . /etc/rc.subr; . /etc/rc.conf; \
          if checkyesno pf; then \
          echo "pf"; elif checkyesno ipfilter; then \
          echo "ipf"; else echo "pf"; fi
.else
FWNAME = pf
.endif
.endif

.if $(OSNAME) == "DragonFly"
.if exists(/etc/rc.subr) && exists(/etc/rc.conf)
FWNAME != . /etc/rc.subr; . /etc/rc.conf; \
          if checkyesno pf; then \
          echo "pf"; elif checkyesno ipfilter; then \
          echo "ipf"; else echo "pf"; fi
.else
FWNAME = pf
.endif
.endif

.if $(OSNAME) == "Darwin"
# Firewall is ipfw up to OS X 10.6 Snow Leopard
# and pf since OS X 10.7 Lion (Darwin 11.0)
FWNAME != [ `uname -r | cut -d. -f1` -ge 11  ] && echo "pf" || echo "ipfw"
.endif

.endif

# Solaris specific CFLAGS
.if $(OSNAME) == "SunOS"
CFLAGS += -DSOLARIS2=`uname -r | cut -d. -f2`
.if $(ARCH) == "amd64"
CFLAGS += -m64 -mcmodel=kernel -mno-red-zone -ffreestanding
.elif $(ARCH) == "sparc64"
CFLAGS += -m64 -mcmodel=medlow
.endif
.endif

STDOBJS = miniupnpd.o upnphttp.o upnpdescgen.o upnpsoap.o \
          upnpredirect.o getifaddr.o daemonize.o upnpglobalvars.o \
          options.o upnppermissions.o minissdp.o natpmp.o pcpserver.o \
          upnpevents.o upnputils.o getconnstatus.o \
          upnppinhole.o asyncsendto.o portinuse.o
BSDOBJS = bsd/getifstats.o bsd/ifacewatcher.o bsd/getroute.o
SUNOSOBJS = solaris/getifstats.o bsd/ifacewatcher.o bsd/getroute.o
MACOBJS = mac/getifstats.o bsd/ifacewatcher.o bsd/getroute.o
PFOBJS = pf/obsdrdr.o pf/pfpinhole.o
IPFOBJS = ipf/ipfrdr.o
IPFWOBJS = ipfw/ipfwrdr.o ipfw/ipfwaux.o
MISCOBJS = upnpreplyparse.o minixml.o

ALLOBJS = $(STDOBJS) $(MISCOBJS)
.if $(OSNAME) == "SunOS"
ALLOBJS += $(SUNOSOBJS)
TESTGETIFSTATSOBJS = testgetifstats.o solaris/getifstats.o
TESTGETROUTEOBJS = testgetroute.o upnputils.o bsd/getroute.o
.elif $(OSNAME) == "Darwin"
ALLOBJS += $(MACOBJS)
TESTGETIFSTATSOBJS = testgetifstats.o mac/getifstats.o
TESTGETROUTEOBJS = testgetroute.o upnputils.o bsd/getroute.o
.else
ALLOBJS += $(BSDOBJS)
TESTGETIFSTATSOBJS = testgetifstats.o bsd/getifstats.o
TESTGETROUTEOBJS = testgetroute.o upnputils.o bsd/getroute.o
.endif

.if $(FWNAME) == "pf"
ALLOBJS += $(PFOBJS)
.elif $(FWNAME) == "ipfw"
ALLOBJS += $(IPFWOBJS)
.else
ALLOBJS += $(IPFOBJS)
.endif

TESTUPNPDESCGENOBJS = testupnpdescgen.o upnpdescgen.o
TESTUPNPPERMISSIONSOBJS = testupnppermissions.o upnppermissions.o
TESTGETIFADDROBJS = testgetifaddr.o getifaddr.o
MINIUPNPDCTLOBJS = miniupnpdctl.o
TESTASYNCSENDTOOBJS = testasyncsendto.o asyncsendto.o upnputils.o bsd/getroute.o
TESTPORTINUSEOBJS = testportinuse.o portinuse.o getifaddr.o

EXECUTABLES = miniupnpd testupnpdescgen testgetifstats \
              testupnppermissions miniupnpdctl \
              testgetifaddr testgetroute testasyncsendto \
              testportinuse

.if $(OSNAME) == "Darwin"
LIBS =
.else
LIBS = -lkvm
.endif
.if $(OSNAME) == "SunOS"
LIBS += -lsocket -lnsl -lkstat -lresolv
.endif

LIBS += -lssl -lcrypto

# set PREFIX variable to install in the wanted place

INSTALLBINDIR = $(PREFIX)/sbin
INSTALLETCDIR = $(PREFIX)/etc
# INSTALLMANDIR = $(PREFIX)/man
INSTALLMANDIR = /usr/share/man

all:	$(EXECUTABLES)

clean:
	$(RM) $(STDOBJS) $(BSDOBJS) $(SUNOSOBJS) $(MACOBJS) $(EXECUTABLES) \
	testupnpdescgen.o \
	$(MISCOBJS) config.h testgetifstats.o testupnppermissions.o \
	miniupnpdctl.o testgetifaddr.o testgetroute.o testasyncsendto.o \
	testportinuse.o \
	$(PFOBJS) $(IPFOBJS) $(IPFWOBJS)

install:	miniupnpd genuuid
	$(STRIP) miniupnpd
	$(INSTALL) -d $(DESTDIR)$(INSTALLBINDIR)
	$(INSTALL) -m 755 miniupnpd $(DESTDIR)$(INSTALLBINDIR)
	$(INSTALL) -d $(DESTDIR)$(INSTALLETCDIR)
	$(INSTALL) -b miniupnpd.conf $(DESTDIR)$(INSTALLETCDIR)
	# TODO : install man page correctly
#	$(INSTALL) -d $(INSTALLMANDIR)
#	$(INSTALL) miniupnpd.8 $(INSTALLMANDIR)/cat8/miniupnpd.0

# genuuid is using the uuid cli tool available under OpenBSD 4.0 in
# the uuid-1.5.0 package
# any other cli tool returning a uuid on stdout should work.
UUID != if which uuidgen 2>&1 > /dev/null; then \
        echo `uuidgen` ; \
        elif which uuid 2>&1 > /dev/null; then \
        echo `uuid` ; \
        else echo "00000000-0000-0000-0000-000000000000"; \
        fi

genuuid:
	$(MV) miniupnpd.conf miniupnpd.conf.before
	sed -e "s/^uuid=[-0-9a-fA-F]*/uuid=$(UUID)/" miniupnpd.conf.before > miniupnpd.conf
	$(RM) miniupnpd.conf.before

depend:	config.h
	mkdep $(ALLOBJS:.o=.c) testupnpdescgen.c testgetifstats.c \
    testupnppermissions.c miniupnpdctl.c testgetifaddr.c \
	testgetroute.c testportinuse.c testasyncsendto.c

miniupnpd: config.h $(ALLOBJS)
	$(CC) $(CFLAGS) -o $@ $(ALLOBJS) $(LIBS)

# BSDmake :
#	$(CC) $(CFLAGS) -o $@ $> $(LIBS)

miniupnpdctl:	config.h $(MINIUPNPDCTLOBJS)
	$(CC) $(CFLAGS) -o $@ $(MINIUPNPDCTLOBJS)

testupnpdescgen:	config.h $(TESTUPNPDESCGENOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTUPNPDESCGENOBJS) $(LIBS)

testgetifstats:	config.h $(TESTGETIFSTATSOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTGETIFSTATSOBJS) $(LIBS)

testgetifaddr:	config.h $(TESTGETIFADDROBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTGETIFADDROBJS) $(LIBS)

testupnppermissions:	config.h $(TESTUPNPPERMISSIONSOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTUPNPPERMISSIONSOBJS) $(LIBS)

testgetroute:	config.h $(TESTGETROUTEOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTGETROUTEOBJS) $(LIBS)

testasyncsendto:	config.h $(TESTASYNCSENDTOOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTASYNCSENDTOOBJS)

testportinuse:	config.h $(TESTPORTINUSEOBJS)
	$(CC) $(CFLAGS) -o $@ $(TESTPORTINUSEOBJS) $(LIBS)

# gmake :
#	$(CC) $(CFLAGS) -o $@ $^
# BSDmake :
#	$(CC) $(CFLAGS) -o $@ $>

config.h:	genconfig.sh VERSION
	./genconfig.sh $(CONFIG_OPTIONS)

.SUFFIXES:	.o .c
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

#	$(CC) $(CFLAGS) -c -o $(.TARGET) $(.IMPSRC)


