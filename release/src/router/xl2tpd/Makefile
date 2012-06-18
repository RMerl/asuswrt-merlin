#
# Layer Two Tunneling Protocol Daemon
# Copyright (C)1998 Adtran, Inc.
#
# Mark Spencer <markster@marko.net>
#
# This is free software.  You may distribute it under
# the terms of the GNU General Public License,
# version 2, or at your option any later version.
#
# Note on debugging flags:
# -DDEBUG_ZLB shows all ZLB exchange traffic
# -DDEBUG_HELLO debugs when hello messages are sent
# -DDEBUG_CLOSE debugs call and tunnel closing
# -DDEBUG_FLOW debugs flow control system
# -DDEBUG_FILE debugs file format
# -DDEBUG_AAA debugs authentication, accounting, and access control
# -DDEBUG_PAYLOAD shows info on every payload packet
# -DDEBUG_CONTROL shows info on every control packet and the l2tp-control pipe
# -DDEBUG_PPPD shows the command line of pppd and how we signal pppd (see below)
# -DDEBUG_HIDDEN debugs hidden AVP's
# -DDEBUG_ENTROPY debug entropy generation
# -DDEBUG_CONTROL_XMIT
# -DDEBUG_MAGIC
# -DDEBUG_FLOW_MORE
# -DDEBUG_AUTH
#
# -DTEST_HIDDEN makes Assigned Call ID sent as a hidden AVP
#
# -DTRUST_PPPD_TO_DIE 
#
# Defining TRUST_PPPD_TO_DIE disables a workaround for broken pppds. Do NOT
# define this unless you fully trust your version of pppd to honour SIGTERM. 
# However, if you experience hanging pppd's, which cause xl2tpd to also hang,
# enable this. 
# The cost of not trusting pppd to die (and shoot it down hard), is that your
# pppd's ip-down scripts will not have a chance to run.
#
# For more details see: http://bugs.xelerance.com/view.php?id=739
#
# Confirmed bad versions of pppd:
# - ppp-2.4.2-6.4.RHEL4
# Confirmed good version of pppd:
# - recent Ubuntu/Debian pppd's
#
# ppp 2.4.3 sends a SIGTERM after 5 seconds, so it should be safe to
# trust pppd. This work around will be removed in the near future.

# DFLAGS= -g -DDEBUG_HELLO -DDEBUG_CLOSE -DDEBUG_FLOW -DDEBUG_PAYLOAD -DDEBUG_CONTROL -DDEBUG_CONTROL_XMIT -DDEBUG_FLOW_MORE -DDEBUG_MAGIC -DDEBUG_ENTROPY -DDEBUG_HIDDEN -DDEBUG_PPPD -DDEBUG_AAA -DDEBUG_FILE -DDEBUG_FLOW -DDEBUG_HELLO -DDEBUG_CLOSE -DDEBUG_ZLB -DDEBUG_AUTH
DFLAGS?= -DTRUST_PPPD_TO_DIE

# Uncomment the next line for Linux. KERNELSRC is needed for if_pppol2tp.h,
# but we use a local copy if we don't find it.
#
#KERNELSRC=/lib/modules/`uname -r`/build/
KERNELSRC?=./linux
OSFLAGS?= -DLINUX -I$(KERNELSRC)/include/
#
# Uncomment the following to use the kernel interface under Linux
# This requires the pppol2tp-linux-2.4.27.patch patch from contrib
# or a 2.6.23+ kernel. On some distributions kernel include files
# are packages seperately (eg kernel-headers on Fedora)
# Note: 2.6.23+ support still needs some changes in the xl2tpd source
#
OSFLAGS+= -DUSE_KERNEL
#
#
# Uncomment the next line for FreeBSD
#
#OSFLAGS?= -DFREEBSD
#
# Uncomment the next line for Solaris. For solaris, at least,
# we don't want to specify -I/usr/include because it is in
# the basic search path, and will over-ride some gcc-specific
# include paths and cause problems.
#
#CC?=gcc
#OSFLAGS?= -DSOLARIS
#OSLIBS?= -lnsl -lsocket

# Uncomment the next two lines for OpenBSD
#
#OSFLAGS?= -DOPENBSD
#LDLIBS?= -lutil

# Feature flags
#
# Comment the following line to disable xl2tpd maintaining IP address
# pools to pass to pppd to control IP address allocation

IPFLAGS?= -DIP_ALLOCATION

CFLAGS+= $(DFLAGS) -O2 -fno-builtin -Wall -DSANITY $(OSFLAGS) $(IPFLAGS)
HDRS=l2tp.h avp.h misc.h control.h call.h scheduler.h file.h aaa.h md5.h
OBJS=xl2tpd.o pty.o misc.o control.o avp.o call.o network.o avpsend.o scheduler.o file.o aaa.o md5.o
SRCS=${OBJS:.o=.c} ${HDRS}
#LIBS= $(OSLIBS) # -lefence # efence for malloc checking
EXEC=xl2tpd

PREFIX?=/usr/local
SBINDIR?=$(DESTDIR)${PREFIX}/sbin
BINDIR?=$(DESTDIR)${PREFIX}/bin
MANDIR?=$(DESTDIR)${PREFIX}/share/man


all: $(EXEC) pfc

clean:
	rm -f $(OBJS) $(EXEC) pfc.o pfc

$(EXEC): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

pfc:
	$(CC) $(CFLAGS) -c contrib/pfc.c
	$(CC) $(LDFLAGS) -lpcap $(LDLIBS) -o pfc pfc.o

romfs:
	$(ROMFSINST) /bin/$(EXEC)

install: ${EXEC} pfc
	install -d -m 0755 ${SBINDIR}
	install -m 0755 $(EXEC) ${SBINDIR}/$(EXEC)
	install -d -m 0755 ${MANDIR}/man5
	install -d -m 0755 ${MANDIR}/man8
	install -m 0644 doc/xl2tpd.8 ${MANDIR}/man8/
	install -m 0644 doc/xl2tpd.conf.5 doc/l2tp-secrets.5 \
		 ${MANDIR}/man5/
	# pfc
	install -d -m 0755 ${BINDIR}
	install -m 0755 pfc ${BINDIR}/pfc
	install -d -m 0755 ${MANDIR}/man1
	install -m 0644 contrib/pfc.1 ${MANDIR}/man1/

# openbsd
#	install -d -m 0755 /var/run/xl2tpd
#	mkfifo /var/run/l2tp-control


TAGS:	${SRCS}
	etags ${SRCS}
