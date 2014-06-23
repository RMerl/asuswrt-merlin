#
# Linux netconf Makefile
#
# Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile 341899 2012-06-29 04:06:38Z $
#

#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s

ifeq (2_6_36,$(LINUX_VERSION))
CFLAGS += -DLINUX26 -DLINUX_2_6_36
CFLAGS  += -I$(TOP)/iptables-1.4.x/include
LDFLAGS += -L. -L$(TOP)/iptables-1.4.x/libiptc/.libs
CFLAGS += -D__EXPORTED_HEADERS__ -fPIC
else
CFLAGS += -DLINUX26
CFLAGS  += -I$(TOP)/iptables/include
LDFLAGS += -L. -L$(TOP)/iptables/libiptc
endif

ifeq (2_6_36,$(LINUX_VERSION))
CFLAGS  += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(SRCBASE)/common/include -I$(LINUXDIR)/include -Wall
else
CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(LINUXDIR)/include -Wall
endif

all: libnetconf.so #netconf

install: all
	install -D libnetconf.so $(INSTALLDIR)/usr/lib/libnetconf.so
	$(STRIP) $(INSTALLDIR)/usr/lib/libnetconf.so

clean:
	rm -f *.o *.a *.so netconf

libnetconf.so: netconf_linux.o
	$(LD) -shared -o $@ $^ $(LDFLAGS) -liptc

netconf: main.o libnetconf.so
	$(CC) -o $@ $< $(LDFLAGS) -lnetconf
