#
# BCM shared library Makefile for Linux router
#
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile,v 1.2 2004-09-02 02:43:14 Exp $
#

include $(TOP)/.config


CFLAGS	+= -I. -I$(SRCBASE)/include -I$(SRCBASE)/router/shared -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2 -fPIC
LDFLAGS += -L.

OBJS := bcmgpio.o

ifeq ($(wildcard ./*.c),)
all:
	cp -f ./prebuilt/libbcm.so . 
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
