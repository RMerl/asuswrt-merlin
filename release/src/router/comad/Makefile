#
# Linux comad Makefile
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
# $Id: Makefile 399877 2013-05-02 05:23:42Z $
#

CFLAGS	:= -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall -Werror
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2
LDFLAGS += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared
LDFLAGS += -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib -lbcm
LDFLAGS += $(EXTRA_LDFLAGS)


vpath %.c $(SRCBASE)/shared/ $(SRCBASE)/shared/bcmwifi/src

all: comad

OBJS = comad_main.o

install: all
	install -d $(INSTALLDIR)/usr/sbin/
	install comad $(INSTALLDIR)/usr/sbin/
	$(STRIP) $(INSTALLDIR)/usr/sbin/comad

clean:
	rm -f *.o *.a *.so comad

comad: ${OBJS}
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
