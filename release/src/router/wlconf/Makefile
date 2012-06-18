#
# Router Wireless Interface Configuration Utility Makefile
#
# Copyright 2007, Broadcom Corporation
# All Rights Reserved.                
#                                     
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior      
# written permission of Broadcom Corporation.                            
#
# $Id$
#
include ../common.mak

CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -Os -DBCMWPA2 $(EXTRACFLAGS)
LDFLAGS	+= -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared


OBJS := wlconf.o
vpath %.c $(SRCBASE)/wlconf

ifeq ($(wildcard $(SRCBASE)/wlconf/*.c),)
all:
	-cp -f $(SRCBASE)/wlconf/prebuilt/wlconf wlconf
else
all: wlconf
endif

clean:
	rm -f *.o wlconf

install: all
	install -d $(INSTALLDIR)/usr/sbin
	install wlconf $(INSTALLDIR)/usr/sbin
	$(STRIP) $(INSTALLDIR)/usr/sbin/wlconf

wlconf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
