#
# milli_httpd Makefile
#
# Copyright (C) 2001 Broadcom Corporation
#
# $Id: Makefile,v 1.1.1.1 2008/07/21 09:17:45 james26_jang Exp $
#
#CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall -I. -Wall -Dlinux $(GLOBAL_OPTIONS)
 
#CFLAGS	+= -g -DDEBUG
#CFLAGS	+= -s -O2 -DNO_PARALLEL
#LDFLAGS	+= -L$(TOP)/nvram -L$(TOP)/shared -L$(TOP)/netconf -lnvram -lshared -lnetconf

#vpath %.c $(TOP)/shared

include ../.config
CFLAGS  += -I. -I$(TOP)/shared  -I$(SRCBASE)/include -Wall
ifeq ($(RTCONFIG_BCMWL6),y)
CFLAGS  += -I$(SRCBASE)/shared/bcmwifi/include
endif
CFLAGS  += -Dlinux -DNO_PARALLEL -DPRNINFO
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram${BCMEX} -lnvram 
ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
LDFLAGS += -lgcc_s
endif

ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

all: infosvr

clean:
	rm -f *.o *~ infosvr

install: all
# Neo
#	install -d $(INSTALLDIR)/usr/sbin
#	install httpd $(INSTALLDIR)/usr/sbin
#	$(STRIP) $(INSTALLDIR)/usr/sbin/httpd

	install -d $(INSTALLDIR)/usr/sbin
	$(STRIP) infosvr
	install infosvr $(INSTALLDIR)/usr/sbin


infosvr: ./infosvr.o ./common.o ./packet.o ./interface.o ./storage.o
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)
	$(STRIP) $@

