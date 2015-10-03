# -*- tab-width: 4 -*-
#
# Copyright (c) 2002-2004, Apple Computer, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# 1.  Redistributions of source code must retain the above copyright notice, 
#     this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright notice, 
#     this list of conditions and the following disclaimer in the documentation 
#     and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its 
#     contributors may be used to endorse or promote products derived from this 
#     software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY 
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY 
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# IMPORTANT NOTE: This is a Makefile for *GNU make*
# On some systems, a different program may be the default "make" command.
# If "make os=xxx" gives lots of errors like "Missing dependency operator",
# then try typing "gmake os=xxx" instead.
#
# $Log: Makefile,v $
# Revision 1.83  2009/02/02 19:44:06  cheshire
# Use "-fwrapv" option to tell compiler that the code is written assuming that
# signed arithmetic is implemented using the twos-complement representation
# (this is pretty much universally true on today's processors).
# <http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Code-Gen-Options.html>
# Without this option, gcc may decide that because the C language
# does not require processors to use twos-complement representation, that means
# gcc is free to do some "creative" optimizations that don't preserve the overflow
# behaviour of twos-complement arithmetic. See also "-fstrict-overflow":
# <http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Optimize-Options.html>
#
# Revision 1.82  2009/01/12 22:48:00  cheshire
# Don't need to include "." in the "#include" search path
#
# Revision 1.81  2009/01/11 03:20:06  mkrochma
# <rdar://problem/5797526> Fixes from Igor Seleznev to get mdnsd working on Solaris
#
# Revision 1.80  2008/11/03 23:27:51  cheshire
# Defined __MAC_OS_X_VERSION_MIN_REQUIRED=__MAC_OS_X_VERSION_10_4 to stop build failures on Leopard
#
# Revision 1.79  2008/10/24 01:59:59  mcguire
# <rdar://problem/6257159> Build Failure: Need to stop using Jam
#
# Revision 1.78  2007/10/22 20:16:49  cheshire
# Got rid of jaguar and panther from list of target platforms;
# changed "os=tiger" to "os=x" (which works with both Tiger and Leopard)
#
# Revision 1.77  2007/10/22 20:04:51  cheshire
# Need to include PlatformCommon.c.o in embedded targets
#
# Revision 1.76  2007/07/31 23:39:02  mcguire
# Don't bail on errors in flex-generated .c files
#
# Revision 1.75  2006/08/24 22:41:23  herscher
# <rdar://problem/4580067> POSIX: dnsextd_parser doesn't compile on Linux
#
# Revision 1.74  2006/08/14 23:07:11  cheshire
# Added "tab-width" emacs header line
#
# Revision 1.73  2006/07/07 00:54:08  cheshire
# <rdar://problem/4472013> Add Private DNS server functionality to dnsextd
# Put intermediate files into "objects" folder instead of mDNSShared source code folder
#
# Revision 1.72  2006/07/05 23:53:58  cheshire
# <rdar://problem/4472013> Add Private DNS server functionality to dnsextd
#
# Revision 1.71  2006/06/20 23:07:04  rpantos
# <rdar://problem/3839132> Java needs to implement DNSServiceRegisterRecord equivalent
#
# Revision 1.70  2006/05/03 23:35:10  cheshire
# Add missing dependency: NetMonitor.c textually imports mDNS.c
#
# Revision 1.69  2006/02/26 23:18:50  cheshire
# <rdar://problem/4427969> FreeBSD 4 requires "-pthread" option to compile threaded code
#
# Revision 1.68  2006/02/26 01:36:54  cheshire
# Rename the poorly named "LIBFLAGS" as "LINKOPTS"
#
# Revision 1.67  2006/02/25 23:14:29  cheshire
# Add comment suggesting using "gmake" command
#
# Revision 1.66  2006/01/06 01:06:17  cheshire
# <rdar://problem/3978979> Compile library and client programs in one pass
#
# Revision 1.65  2005/12/21 21:15:57  cheshire
# Add missing dependency: Identify.c textually imports mDNS.c
#
# Revision 1.64  2005/10/25 23:55:47  cheshire
# Add tiger to list of target platforms
#
# Revision 1.63  2005/10/11 21:30:44  cheshire
# Add "-Wunreachable-code" (commented out for now)
#
# Revision 1.62  2005/06/30 21:46:55  cheshire
# <rdar://problem/4167287> Solaris should use unix domain sockets, not loopback
#
# Revision 1.61  2005/06/30 20:46:05  cheshire
# Added Makefile rule to build threadsafe object files where necessary using "-D_REENTRANT".
#
# Revision 1.60  2005/06/30 10:42:38  cheshire
# Turn on "-Werror" and "-O" for better error reporting
#
# Revision 1.59  2005/04/14 21:07:10  rpantos
# Bug #: 4089257, Clean build broken for Java support on POSIX
# Submitted by: Roger Pantos
# Reviewed by: Kiren Sekar
#
# Revision 1.58  2005/04/08 21:37:57  ksekar
# <rdar://problem/3792767> get_ifi_info doesn't return IPv6 interfaces on Linux
#
# Revision 1.57  2005/03/17 04:02:28  cheshire
# <rdar://problem/3986419> mDNSResponder won't compile with gcc4 on Tiger
# Changed Makefile to link using gcc instead of libtool
#
# Revision 1.56  2005/02/02 02:25:21  cheshire
# <rdar://problem/3980388> /var/run/mDNSResponder should be /var/run/mdnsd on Linux
#
# Revision 1.55  2005/01/27 22:55:00  cheshire
# Add "make os=tiger" target which uses gcc4 and "-Wdeclaration-after-statement"
#
# Revision 1.54  2004/12/17 19:33:03  cheshire
# Add "-lresolv" for Solaris
#
# Revision 1.53  2004/12/01 20:04:31  cheshire
# Tidy up alignment
#
# Revision 1.52  2004/12/01 19:46:12  cheshire
# Add install case for Suse 9 (rc*.d directories *inside* the init.d directory)
#
# Revision 1.51  2004/12/01 03:30:29  cheshire
# <rdar://problem/3889346> Add Unicast DNS support to mDNSPosix
#
# Revision 1.50  2004/12/01 01:14:20  cheshire
# Add $(LIBFLAGS) to cc command to build dnsextd (required for Solaris)
#
# Revision 1.49  2004/11/11 01:44:52  cheshire
# Updated error message
#
# Revision 1.48  2004/10/06 02:22:19  cheshire
# Changed MacRoman copyright symbol (should have been UTF-8 in any case :-) to ASCII-compatible "(c)"
#
# Revision 1.47  2004/10/01 22:15:54  rpantos
# rdar://problem/3824265: Replace APSL in client lib with BSD license.
#
# Revision 1.46  2004/09/24 21:15:25  cheshire
# <rdar://problem/3724985> Library "libmdns" misnamed; should be "libdns_sd"
#
# Revision 1.45  2004/09/22 16:23:41  cheshire
# Modify installation for compatibility with Gentoo Linux
# (Thanks to David Black for this information)
#
# Revision 1.44  2004/09/17 01:08:53  cheshire
# Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
#   The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
#   declared in that file are ONLY appropriate to single-address-space embedded applications.
#   For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.
#
# Revision 1.43  2004/09/17 00:30:11  cheshire
# Added some '@' signs to make build output less verbose --
# when there's too much on the screen it's easy to miss build errors and warnings
#
# Revision 1.42  2004/08/24 22:04:37  cheshire
# Need to specify -lpthread for building dnsextd
#
# Revision 1.41  2004/08/11 00:43:26  ksekar
# <rdar://problem/3722542>: DNS Extension daemon for DNS Update Lease
#
# Revision 1.40  2004/07/08 21:45:55  cheshire
# Make nss_mdns only build on Linux. We can add it to other targets (Solaris,
# FreeBSD, etc., as we verify them). In particular, NSS is NOT supported on
# OS X, so including it for "os=jaguar" or "os=panther" broke those builds.
#
# Revision 1.39  2004/06/29 03:34:28  cheshire
# Add 'dot-local' Name Service Switch support from Andrew White at NICTA
#
# Revision 1.38  2004/06/25 02:19:40  rpantos
# And FreeBSD...
#
# Revision 1.37  2004/06/25 00:51:09  rpantos
# And fix the Java build for Posix on Solaris, too.
#
# Revision 1.36  2004/06/25 00:26:27  rpantos
# Changes to fix the Posix build on Solaris.
#
# Revision 1.35  2004/06/18 18:51:31  cheshire
# Add (commented out) "-pedantic" for when we want to check for "mixed declarations and code" warnings
#
# Revision 1.34  2004/05/25 18:29:33  cheshire
# Move DNSServiceConstructFullName() from dnssd_clientstub.c to dnssd_clientlib.c,
# so that it's also accessible to dnssd_clientshim.c (single address space) clients.
#
# Revision 1.33  2004/04/30 16:46:32  rpantos
# Add support for building Java libraries.
#
# Revision 1.32  2004/04/14 23:09:29  ksekar
# Support for TSIG signed dynamic updates.
#
# Revision 1.31  2004/03/15 19:07:06  cheshire
# Fix error message
#
# Revision 1.30  2004/03/11 18:58:29  rpantos
# Fix Kill /etc/rc scripts so they run at halt & reboot.
#
# Revision 1.29  2004/03/04 23:35:41  cheshire
# Instead of using a dummy target to generate an error message, use "$(error text...)"
#
# Revision 1.28  2004/03/04 23:33:42  cheshire
# Fixes from Alfred Perlstein for FreeBSD's benefit
#
# Revision 1.27  2004/02/11 21:00:21  cheshire
# Update URL for GNU Make manual page
#
# Revision 1.26  2004/02/05 21:28:30  cheshire
# Fixes so that "sudo make install" works on *BSD
#
# Revision 1.25  2004/02/05 20:00:22  cheshire
# Define mdnsd's PID file to be /var/run/mdnsd.pid on Posix builds
#
# Revision 1.24  2004/02/05 01:00:01  rpantos
# Fix some issues that turned up when building for FreeBSD.
#
# Revision 1.23  2004/02/04 01:50:54  cheshire
# Make InstalledStartup conditional, so it automatically installs into
# either /etc/init.d/ or /etc/rc.d/init.d/ as appropriate
#
# Revision 1.22  2004/01/20 01:41:21  rpantos
# Define USES_NETLINK for Linux builds.
#
# Revision 1.21  2003/12/17 00:51:22  cheshire
# Changed mDNSNetMonitor and mDNSIdentify to link the object files
# instead of #including the "DNSCommon.c" "uDNS.c" and source files
#
# Revision 1.20  2003/12/13 03:05:28  ksekar
# Bug #: <rdar://problem/3192548>: DynDNS: Unicast query of service records
#
# Revision 1.19  2003/12/11 19:42:13  cheshire
# Change name "mDNSResponderd" to "mdnsd" for consistency with standard Linux (Unix) naming conventions
#
# Revision 1.18  2003/12/11 19:38:34  cheshire
# Add APSL
#
# Revision 1.17  2003/12/11 03:16:49  rpantos
# One more change for OS X build: make install work a little better.
#
# Revision 1.16  2003/12/11 03:03:51  rpantos
# Clean up mDNSPosix so that it builds on OS X again.
#
# Revision 1.15  2003/12/08 20:47:02  rpantos
# Add support for mDNSResponder on Linux.
#
# Revision 1.14  2003/11/14 20:59:09  cheshire
# Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
# Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.
#
# Revision 1.13  2003/08/06 18:20:51  cheshire
# Makefile cleanup
#
# Revision 1.12  2003/08/01 02:20:02  cheshire
# Add mDNSIdentify tool, used to discover what version of mDNSResponder a particular host is running
#
# Revision 1.11  2003/07/14 18:11:54  cheshire
# Fix stricter compiler warnings
#
# Revision 1.10  2003/06/18 05:47:41  cheshire
# Enable stricter warnings on Jaguar and Panther builds
#
# Revision 1.9  2003/06/04 18:34:45  ksekar
# Bug #: <rdar://problem/3218120>: mDNSPosix does not build on Panther that has socklen_t
# Changed build targets "osx10.2" and "osx10.3" to "jaguar" and "panther".
#
# Revision 1.8  2003/06/04 00:23:12  ksekar
# Bug #: <rdar://problem/3218120>: mDNSPosix does not build on Panther that has socklen_t
# Created separate target OS's for 10.2 and 10.3.
#
# Revision 1.7  2003/04/16 02:11:37  cheshire
# Remove unnecessary $(CFLAGS) from linking rules
#
# Revision 1.6  2003/04/04 01:37:14  cheshire
# Added NetMonitor.c
#

# This Makefile builds an mDNSResponder daemon and a libdns_sd.so shared library 
# for Linux. It also builds several example programs for embedded systems. 
#
# Make with no arguments to build all production targets.
# 'make DEBUG=1' to build debugging targets.
# 'make clean' or 'make clean DEBUG=1' to delete prod/debug objects & targets
# 'sudo make install [DEBUG=1]' to install mdnsd daemon and libdns_sd.
#
# Notes:
# $@ means "The file name of the target of the rule"
# $< means "The name of the first prerequisite"
# $* means "The stem with which an implicit rule matches"
# $+ means "The names of all the prerequisites, with spaces between them, exactly as given"
# For more magic automatic variables, see
# <http://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html>

#############################################################################

LIBVERS = 1

COREDIR = ../mDNSCore
SHAREDDIR ?= ../mDNSShared
JDK = /usr/jdk

#CC = @cc
BISON = @bison
FLEX = @flex
#LD = ld -shared
#CP = cp
#RM = rm
#LN = ln -s -f
CFLAGS_COMMON = -I$(COREDIR) -I$(SHAREDDIR) -I$(OBJDIR) -fwrapv -W -Wall -DPID_FILE=\"/var/run/mdnsd.pid\" -DMDNS_UDS_SERVERPATH=\"/var/run/mdnsd\"
CFLAGS_PTHREAD =
LINKOPTS =
LINKOPTS_PTHREAD = -lpthread
LDSUFFIX = so
JAVACFLAGS_OS = -fPIC -shared -ldns_sd

# Set up diverging paths for debug vs. prod builds
DEBUG=0
ifeq ($(DEBUG),1)
CFLAGS_DEBUG = -g -DMDNS_DEBUGMSGS=2 
OBJDIR = objects/debug
BUILDDIR = build/debug
STRIP = echo 
else
# We use -Os for two reasons:
# 1. We want to make small binaries, suitable for putting into hardware devices
# 2. Some of the code analysis warnings only work when some form of optimization is enabled
CFLAGS_DEBUG = -Os -DMDNS_DEBUGMSGS=0 
OBJDIR ?= objects/prod
BUILDDIR ?= build/prod
#STRIP = strip -S 
endif

# Configure per-OS peculiarities
ifeq ($(os),solaris)
CFLAGS_DEBUG = -O0 -DMDNS_DEBUGMSGS=0
CFLAGS_OS = -DNOT_HAVE_DAEMON -DNOT_HAVE_SA_LEN -DNOT_HAVE_SOCKLEN_T -DNOT_HAVE_IF_NAMETOINDEX \
	 -DLOG_PERROR=0 -D_XPG4_2 -D__EXTENSIONS__ -DHAVE_BROKEN_RECVIF_NAME -DTARGET_OS_SOLARIS
CC = gcc
LD = gcc -shared
LINKOPTS = -lsocket -lnsl -lresolv
JAVACFLAGS_OS += -I$(JDK)/include/solaris
ifneq ($(DEBUG),1)
STRIP = strip
endif
else

ifeq ($(os),linux)
CFLAGS_OS = -DNOT_HAVE_SA_LEN -DUSES_NETLINK -DHAVE_LINUX -DTARGET_OS_LINUX
FLEXFLAGS_OS = -l
JAVACFLAGS_OS += -I$(JDK)/include/linux
OPTIONALTARG = nss_mdns
OPTINSTALL   = InstalledNSS
else

ifeq ($(os),netbsd)
CFLAGS_OS =
LDCONFIG = ldconfig
else

ifeq ($(os),freebsd)
# If not already defined, set LOCALBASE to /usr/local
# FreeBSD requires the startup script to end in ".sh"
LOCALBASE?=/usr/local
INSTBASE=$(LOCALBASE)
STARTUPSCRIPTNAME=mdns.sh
CFLAGS_OS =
# FreeBSD 4 requires threaded code to be compiled and linked using the "-pthread" option,
# and requires that the "-lpthread" link option NOT be used
# This appies only to FreeBSD -- "man cc" on FreeBSD says:
#   FreeBSD SPECIFIC OPTIONS
#     -pthread
#       Link a user-threaded process against libc_r instead of libc.
CFLAGS_PTHREAD   = -pthread -D_THREAD_SAFE
LINKOPTS_PTHREAD = -pthread
JAVACFLAGS_OS += -I$(JDK)/include/freebsd
LDCONFIG = ldconfig
else

ifeq ($(os),openbsd)
CFLAGS_OS = -DHAVE_BROKEN_RECVDSTADDR
LDCONFIG = ldconfig
else

ifeq ($(os),x)
# We have to define __MAC_OS_X_VERSION_MIN_REQUIRED=__MAC_OS_X_VERSION_10_4 or on Leopard
# we get build failures: ‘daemon’ is deprecated (declared at /usr/include/stdlib.h:283)
CFLAGS_OS = -DHAVE_IPV6 -no-cpp-precomp -Werror -Wdeclaration-after-statement \
	-D__MAC_OS_X_VERSION_MIN_REQUIRED=__MAC_OS_X_VERSION_10_4 #-Wunreachable-code
#CC = @gcc-4.0
#LD = $(CC) -dynamiclib
LINKOPTS = -lSystem
LDSUFFIX = dylib
JDK = /System/Library/Frameworks/JavaVM.framework/Home
JAVACFLAGS_OS = -dynamiclib -I/System/Library/Frameworks/JavaVM.framework/Headers -framework JavaVM 
else

$(error ERROR: Must specify target OS on command-line, e.g. "make os=x [target]".\
Supported operating systems include: x, linux, netbsd, freebsd, openbsd, solaris) 
endif
endif
endif
endif
endif
endif

NSSLIBNAME  := libnss_mdns
NSSVERSION  := 0.2
NSSLIBFILE  := $(NSSLIBNAME)-$(NSSVERSION).so
NSSLINKNAME := $(NSSLIBNAME).so.2
NSSINSTPATH := /lib

# If not otherwise defined, we install into /usr/lib and /usr/include
# and our startup script is called mdns (e.g. /etc/init.d/mdns)
INSTBASE?=/usr
STARTUPSCRIPTNAME?=mdns

ifeq ($(HAVE_IPV6),1)
CFLAGS_OS += -DHAVE_IPV6=1
else
ifeq ($(HAVE_IPV6),0)
CFLAGS_OS += -DHAVE_IPV6=0
endif
endif

# If directory /usr/share/man exists, then we install man pages into that, else /usr/man
ifeq ($(wildcard /usr/share/man), /usr/share/man)
MANPATH := /usr/share/man
else
MANPATH := /usr/man
endif

# If directories /etc/init.d/rc*.d exist, then we install into that (Suse)
ifeq ($(wildcard /etc/init.d/rc2.d/), /etc/init.d/rc2.d/)
STARTUPSCRIPTDIR = /etc/init.d
RUNLEVELSCRIPTSDIR = /etc/init.d
else
# else if directory /etc/rc.d/init.d/ exists, then we install into that (old Linux)
ifeq ($(wildcard /etc/rc.d/init.d/), /etc/rc.d/init.d/)
STARTUPSCRIPTDIR = /etc/rc.d/init.d
RUNLEVELSCRIPTSDIR = /etc/rc.d
else
# else if directory /etc/init.d/ exists, then we install into that (new Linux)
ifeq ($(wildcard /etc/init.d/), /etc/init.d/)
STARTUPSCRIPTDIR = /etc/init.d
RUNLEVELSCRIPTSDIR = /etc
else
# else install into /etc/rc.d/ (*BSD)
STARTUPSCRIPTDIR = $(INSTBASE)/etc/rc.d
endif
endif
endif

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_OS) $(CFLAGS_DEBUG)

#############################################################################

#all: setup Daemon libdns_sd Clients SAClient SAResponder SAProxyResponder Identify NetMonitor dnsextd $(OPTIONALTARG)
all: SAProxyResponder NetMonitor Query 

install: setup InstalledDaemon InstalledStartup InstalledLib InstalledManPages InstalledClients $(OPTINSTALL)

# 'setup' sets up the build directory structure the way we want
setup:
	@if test ! -d $(OBJDIR)   ; then mkdir -p $(OBJDIR)   ; fi
	@if test ! -d $(BUILDDIR) ; then mkdir -p $(BUILDDIR) ; fi

# clean removes targets and objects
clean:
	@if test -d $(OBJDIR)   ; then rm -r $(OBJDIR)   ; fi
	@if test -d $(BUILDDIR) ; then rm -r $(BUILDDIR) ; fi
	@$(MAKE) -C ../Clients clean

#############################################################################

# daemon target builds the daemon
DAEMONOBJS = $(OBJDIR)/PosixDaemon.c.o $(OBJDIR)/mDNSPosix.c.o $(OBJDIR)/mDNSUNP.c.o $(OBJDIR)/mDNS.c.o \
             $(OBJDIR)/DNSDigest.c.o $(OBJDIR)/uDNS.c.o $(OBJDIR)/DNSCommon.c.o $(OBJDIR)/uds_daemon.c.o \
             $(OBJDIR)/mDNSDebug.c.o $(OBJDIR)/dnssd_ipc.c.o $(OBJDIR)/GenLinkedList.c.o $(OBJDIR)/PlatformCommon.c.o 

# dnsextd target build dnsextd
DNSEXTDOBJ = $(OBJDIR)/mDNSPosix.c.o $(OBJDIR)/mDNSUNP.c.o $(OBJDIR)/mDNSDebug.c.o $(OBJDIR)/GenLinkedList.c.o $(OBJDIR)/DNSDigest.c.o \
             $(OBJDIR)/DNSCommon.c.o $(OBJDIR)/PlatformCommon.c.o $(OBJDIR)/dnsextd_parser.y.o $(OBJDIR)/dnsextd_lexer.l.o

Daemon: setup $(BUILDDIR)/mdnsd
	@echo "Responder daemon done"

$(BUILDDIR)/mdnsd: $(DAEMONOBJS)
	$(CC) -o $@ $+ $(LINKOPTS)
	@$(STRIP) $@

# libdns_sd target builds the client library
libdns_sd: setup $(BUILDDIR)/libdns_sd.$(LDSUFFIX)
	@echo "Client library done"

CLIENTLIBOBJS = $(OBJDIR)/dnssd_clientlib.c.so.o $(OBJDIR)/dnssd_clientstub.c.so.o $(OBJDIR)/dnssd_ipc.c.so.o

$(BUILDDIR)/libdns_sd.$(LDSUFFIX): $(CLIENTLIBOBJS)
	@$(LD) $(LINKOPTS) -o $@ $+
	@$(STRIP) $@

Clients: setup libdns_sd ../Clients/build/dns-sd
	@echo "Clients done"

../Clients/build/dns-sd:
	@$(MAKE) -C ../Clients

# nss_mdns target builds the Name Service Switch module
nss_mdns: setup $(BUILDDIR)/$(NSSLIBFILE)
	@echo "Name Service Switch module done"

$(BUILDDIR)/$(NSSLIBFILE): $(CLIENTLIBOBJS) $(OBJDIR)/nss_mdns.c.so.o
	@$(LD) $(LINKOPTS) -o $@ $+
	@$(STRIP) $@

#############################################################################

# The Install targets place built stuff in their proper places
InstalledDaemon: $(INSTBASE)/sbin/mdnsd
	@echo $+ " installed"

InstalledLib: $(INSTBASE)/lib/libdns_sd.$(LDSUFFIX).$(LIBVERS) $(INSTBASE)/include/dns_sd.h
	@echo $+ " installed"

InstalledStartup: $(STARTUPSCRIPTDIR)/$(STARTUPSCRIPTNAME)
	@echo $+ " installed"

InstalledManPages: $(MANPATH)/man8/mdnsd.8
	@echo $+ " installed"

InstalledClients: $(INSTBASE)/bin/dns-sd
	@echo $+ " installed"

InstalledNSS: $(NSSINSTPATH)/$(NSSLINKNAME) /etc/nss_mdns.conf $(MANPATH)/man5/nss_mdns.conf.5 $(MANPATH)/man8/libnss_mdns.8
	@echo $+ " installed"

# Note: If daemon already installed, we make sure it's stopped before overwriting it
$(INSTBASE)/sbin/mdnsd: $(BUILDDIR)/mdnsd
	@if test -x $@; then $(STARTUPSCRIPTDIR)/$(STARTUPSCRIPTNAME) stop; fi
	$(CP) $< $@

$(INSTBASE)/lib/libdns_sd.$(LDSUFFIX).$(LIBVERS): $(BUILDDIR)/libdns_sd.$(LDSUFFIX)
	$(CP) $< $@
	$(LN) $@ $(INSTBASE)/lib/libdns_sd.$(LDSUFFIX)
ifdef LDCONFIG
    # -m means 'merge into existing database', -R means 'rescan directories'
	$(LDCONFIG) -mR
endif

$(INSTBASE)/include/dns_sd.h: $(SHAREDDIR)/dns_sd.h
	$(CP) $< $@

# We make this target dependent on $(INSTBASE)/sbin/mdnsd because we need to ensure
# that the daemon is installed *before* we try to execute the command to start it.
$(STARTUPSCRIPTDIR)/$(STARTUPSCRIPTNAME): mdnsd.sh $(STARTUPSCRIPTDIR) $(INSTBASE)/sbin/mdnsd
	$(CP) $< $@
	chmod ugo+x $@
	$@ start
ifdef RUNLEVELSCRIPTSDIR
ifeq ($(wildcard $(RUNLEVELSCRIPTSDIR)/runlevels/default), $(RUNLEVELSCRIPTSDIR)/runlevels/default)
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/runlevels/default/mdns
else
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc2.d/S52mdns
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc3.d/S52mdns
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc4.d/S52mdns
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc5.d/S52mdns
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc0.d/K16mdns
	$(LN) $@ $(RUNLEVELSCRIPTSDIR)/rc6.d/K16mdns
endif
endif

$(MANPATH)/man5/%.5: %.5
	cp $< $@
	chmod 444 $@

$(MANPATH)/man8/%.8: %.8
	cp $< $@
	chmod 444 $@

$(MANPATH)/man8/mdnsd.8: $(SHAREDDIR)/mDNSResponder.8
	cp $< $@
	chmod 444 $@

$(INSTBASE)/bin/dns-sd: ../Clients/build/dns-sd
	$(CP) $< $@

$(NSSINSTPATH)/$(NSSLINKNAME): $(NSSINSTPATH)/$(NSSLIBFILE)
	$(LN) $< $@
	ldconfig

$(NSSINSTPATH)/$(NSSLIBFILE): $(BUILDDIR)/$(NSSLIBFILE)
	$(CP) $< $@
	chmod 444 $@

/etc/nss_mdns.conf: nss_mdns.conf
	$(CP) $< $@
	chmod 444 $@
	# Check the nsswitch.conf file.
	# If 'mdns' does not already appear on the "hosts:" line, then add it right before 'dns'
	cp -f /etc/nsswitch.conf /etc/nsswitch.conf.pre-mdns
	sed -e '/mdns/!s/^\(hosts:.*\)dns\(.*\)/\1mdns dns\2/' /etc/nsswitch.conf.pre-mdns > /etc/nsswitch.conf

#############################################################################

# The following targets build Java wrappers for the dns-sd.h API.
# Note that the JavaForXcode targets are used when building the project for OS X using Xcode

JAVAC = $(JDK)/bin/javac
JAVAH = $(JDK)/bin/javah
JAVADOC = $(JDK)/bin/javadoc
JAR = $(JDK)/bin/jar
JAVACFLAGS = $(CFLAGS) $(JAVACFLAGS_OS) -I$(JDK)/include

JavaForXcode_: setup $(BUILDDIR)/dns_sd.jar $(PROJECT_DERIVED_FILE_DIR)/DNSSD.java.h
	@echo $@ done
	
$(PROJECT_DERIVED_FILE_DIR)/DNSSD.java.h: $(OBJDIR)/DNSSD.java.h
	@if test ! -d $(PROJECT_DERIVED_FILE_DIR) ; then mkdir -p $(PROJECT_DERIVED_FILE_DIR) ; fi
	$(CP) $< $@

JavaForXcode_clean:
	@if test -d $(OBJDIR) ; then rm -r $(OBJDIR) ; fi
	@if test -f $(PROJECT_DERIVED_FILE_DIR)/DNSSD.java.h ; then $(RM) $(PROJECT_DERIVED_FILE_DIR)/DNSSD.java.h ; fi
	@if test -f $(BUILDDIR)/dns_sd.jar ; then $(RM) $(BUILDDIR)/dns_sd.jar ; fi
	@echo $@ done

JavaForXcode_installhdrs:
	@echo $@ NOOP

JavaForXcode_install: JavaForXcode_ $(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/Java/Extensions/dns_sd.jar
	@echo $@ done

$(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/Java/Extensions/dns_sd.jar: $(BUILDDIR)/dns_sd.jar
	@if test ! -d $(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/Java/Extensions ; then mkdir -p $(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/Java/Extensions ; fi
	$(CP) $< $@

Java: setup $(BUILDDIR)/dns_sd.jar $(BUILDDIR)/libjdns_sd.$(LDSUFFIX)
	@echo "Java wrappers done"

JAVASRC	= $(SHAREDDIR)/Java
JARCONTENTS =	$(OBJDIR)/com/apple/dnssd/DNSSDService.class \
				$(OBJDIR)/com/apple/dnssd/DNSSDException.class \
				$(OBJDIR)/com/apple/dnssd/DNSRecord.class \
				$(OBJDIR)/com/apple/dnssd/TXTRecord.class \
				$(OBJDIR)/com/apple/dnssd/DNSSDRegistration.class \
				$(OBJDIR)/com/apple/dnssd/BaseListener.class \
				$(OBJDIR)/com/apple/dnssd/BrowseListener.class \
				$(OBJDIR)/com/apple/dnssd/ResolveListener.class \
				$(OBJDIR)/com/apple/dnssd/RegisterListener.class \
				$(OBJDIR)/com/apple/dnssd/QueryListener.class \
				$(OBJDIR)/com/apple/dnssd/DomainListener.class \
				$(OBJDIR)/com/apple/dnssd/RegisterRecordListener.class \
				$(OBJDIR)/com/apple/dnssd/DNSSDRecordRegistrar.class \
				$(OBJDIR)/com/apple/dnssd/DNSSD.class

$(BUILDDIR)/dns_sd.jar: $(JARCONTENTS) setup
	$(JAR) -cf $@ -C $(OBJDIR) com

$(BUILDDIR)/libjdns_sd.$(LDSUFFIX): $(JAVASRC)/JNISupport.c $(OBJDIR)/DNSSD.java.h setup libdns_sd
	$(CC) -o $@ $< $(JAVACFLAGS) -I$(OBJDIR) -L$(BUILDDIR)

$(OBJDIR)/com/apple/dnssd/%.class:	$(JAVASRC)/%.java
	$(JAVAC) -d $(OBJDIR) -classpath $(OBJDIR) $<

$(OBJDIR)/DNSSD.java.h: $(OBJDIR)/com/apple/dnssd/DNSSD.class
	$(JAVAH) -force -classpath $(OBJDIR) -o $@ \
		com.apple.dnssd.AppleDNSSD \
		com.apple.dnssd.AppleBrowser \
		com.apple.dnssd.AppleResolver \
		com.apple.dnssd.AppleRegistration \
		com.apple.dnssd.AppleQuery \
		com.apple.dnssd.AppleDomainEnum \
		com.apple.dnssd.AppleService \
		com.apple.dnssd.AppleDNSRecord \
		com.apple.dnssd.AppleRecordRegistrar

#############################################################################

# The following target builds documentation for the Java wrappers.

JavaDoc: Java setup
	$(JAVADOC) $(JAVASRC)/*.java -classpath $(OBJDIR) -d $(BUILDDIR) -public

#############################################################################

# The following targets build embedded example programs
SPECIALOBJ = $(OBJDIR)/mDNSPosix.c.o $(OBJDIR)/mDNSUNP.c.o $(OBJDIR)/mDNSDebug.c.o $(OBJDIR)/GenLinkedList.c.o \
	$(OBJDIR)/DNSDigest.c.o $(OBJDIR)/uDNS.c.o $(OBJDIR)/DNSCommon.c.o $(OBJDIR)/PlatformCommon.c.o
COMMONOBJ  = $(SPECIALOBJ) $(OBJDIR)/mDNS.c.o
APPOBJ     = $(COMMONOBJ) $(OBJDIR)/ExampleClientApp.c.o

SAClient: setup $(BUILDDIR)/mDNSClientPosix
	@echo "Embedded Standalone Client done"

SAResponder: setup $(BUILDDIR)/mDNSResponderPosix
	@echo "Embedded Standalone Responder done"

SAProxyResponder: setup $(BUILDDIR)/mDNSProxyResponderPosix
	@echo "Embedded Standalone ProxyResponder done"

Identify: setup $(BUILDDIR)/mDNSIdentify
	@echo "Identify done"

NetMonitor: setup $(BUILDDIR)/mDNSNetMonitor
	@echo "NetMonitor done"

dnsextd: setup $(BUILDDIR)/dnsextd
	@echo "dnsextd done"

Query: setup $(BUILDDIR)/mDNSQuery
	@echo "mDNSQuery done"

$(BUILDDIR)/mDNSClientPosix:         $(APPOBJ)     $(OBJDIR)/Client.c.o
	$(CC) $+ -o $@ $(LINKOPTS)

$(BUILDDIR)/mDNSResponderPosix:      $(COMMONOBJ)  $(OBJDIR)/Responder.c.o
	$(CC) $+ -o $@ $(LINKOPTS)

$(BUILDDIR)/mDNSProxyResponderPosix: $(COMMONOBJ)  $(OBJDIR)/ProxyResponder.c.o
	$(CC) $+ -o $@ $(LINKOPTS)

$(BUILDDIR)/mDNSIdentify:            $(SPECIALOBJ) $(OBJDIR)/Identify.c.o
	$(CC) $+ -o $@ $(LINKOPTS)

$(OBJDIR)/Identify.c.o:              $(COREDIR)/mDNS.c # Note: Identify.c textually imports mDNS.c

$(BUILDDIR)/mDNSNetMonitor:          $(SPECIALOBJ) $(OBJDIR)/NetMonitor.c.o
	$(CC) $+ -o $@ $(LINKOPTS) 

$(OBJDIR)/NetMonitor.c.o:            $(COREDIR)/mDNS.c # Note: NetMonitor.c textually imports mDNS.c

$(BUILDDIR)/dnsextd:                 $(DNSEXTDOBJ) $(OBJDIR)/dnsextd.c.threadsafe.o
	$(CC) $+ -o $@ $(LINKOPTS) $(LINKOPTS_PTHREAD)

$(BUILDDIR)/mDNSQuery:               $(COMMONOBJ)  $(OBJDIR)/Query.c.o
	$(CC) $+ -o $@ $(LINKOPTS)

#############################################################################

# Implicit rules
$(OBJDIR)/%.c.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.c.o:	$(COREDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.c.o:	$(SHAREDDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.c.threadsafe.o:	%.c
	$(CC) $(CFLAGS) $(CFLAGS_PTHREAD) -D_REENTRANT -c -o $@ $<

$(OBJDIR)/%.c.threadsafe.o:	$(SHAREDDIR)/%.c
	$(CC) $(CFLAGS) $(CFLAGS_PTHREAD) -D_REENTRANT -c -o $@ $<

$(OBJDIR)/%.c.so.o:	%.c
	$(CC) $(CFLAGS) -c -fPIC -o $@ $<

$(OBJDIR)/%.c.so.o:	$(SHAREDDIR)/%.c
	$(CC) $(CFLAGS) -c -fPIC -o $@ $<

$(OBJDIR)/%.y.o: $(SHAREDDIR)/%.y
	$(BISON)              -o $(OBJDIR)/$*.c -d $<
	$(CC) $(CFLAGS) -c -o $@ $(OBJDIR)/$*.c

$(OBJDIR)/%.l.o: $(SHAREDDIR)/%.l
	$(FLEX) $(FLEXFLAGS_OS) -i             -o$(OBJDIR)/$*.l.c $<
	$(CC) $(CFLAGS) -Wno-error -c -o $@ $(OBJDIR)/$*.l.c

romfs:
	install -d $(INSTALLDIR)/usr/sbin
	$(STRIP) build/prod/mDNSProxyResponderPosix 
	install build/prod/mDNSProxyResponderPosix $(INSTALLDIR)/usr/sbin/mDNSResponder
	$(STRIP) build/prod/mDNSNetMonitor
	install build/prod/mDNSNetMonitor $(INSTALLDIR)/usr/sbin/mDNSNetMonitor
	$(STRIP) build/prod/mDNSQuery
	install build/prod/mDNSQuery $(INSTALLDIR)/usr/sbin/mDNSQuery

