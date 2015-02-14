#
# BCM shared library Makefile for Linux router
#
# Copyright (C) 2013, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 382668 2013-02-03 04:41:22Z $
#

include $(TOP)/.config


CFLAGS	+= -I. -I$(SRCBASE)/include ${WLAN_StdIncPathA} -I$(SRCBASE)/common/include -I$(SRCBASE)/router/shared -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s
ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif
LDFLAGS += -L.

OBJS := bcmgpio.o


ifeq ($(wildcard ./*.c),)
all:
ifeq ($(RTCONFIG_BCMARM),y)
	cp -f ./prebuilt_arm$(EX7)/libbcm.so .
else
	cp -f ./prebuilt/libbcm.so .
endif
else
all: libbcm.so
endif

install: all
	install -d $(INSTALLDIR)/usr/lib
	install -m 755 libbcm.so $(INSTALLDIR)/usr/lib
	$(STRIP) $(INSTALLDIR)/usr/lib/libbcm.so

clean:
	rm -f *.o *.so

libbcm.so: $(OBJS)
	$(LD) -shared -o $@ $^
