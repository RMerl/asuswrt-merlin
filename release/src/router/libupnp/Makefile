#
# Copyright (C) 2014, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 403251 2013-05-20 05:15:55Z $
#

include ../common.mak

UPNPLIB_NAME = libupnp.so
UPNPLIB = $(SRCBASE)/router/libupnp
SRCPATH = $(UPNPLIB)/upnp

INCLUDES += -I$(UPNPLIB)/include -I$(SRCBASE)/include -I$(SRCBASE)/include/bcmcrypto -I$(SRCBASE)/router/shared
CFLAGS += ${INCLUDES}

CFLAGS += -Wall -Wunused -g -s -Werror -fPIC

ifeq ($(RTCONFIG_BCM7),y)
CFLAGS += -DBCM7
endif

LDFLAGS = -L$(TOP)/libbcmcrypto -lbcmcrypto

ifeq ($(RTCONFIG_BCMARM),y)
LDFLAGS += -lgcc_s
endif

vpath %.c $(SRCPATH) $(UPNPLIB)/linux
ifeq ($(RTCONFIG_BCMARM),y)
vpath %.o $(UPNPLIB)/prebuilt$(BCMEX)$(EX7)
else
vpath %.o $(UPNPLIB)/prebuilt.mipsel
endif

SRCFILES = upnp.c upnp_ssdp.c upnp_http.c upnp_gena.c upnp_soap.c \
	upnp_description.c upnp_device.c upnp_util.c upnp_msg.c \
	upnp_linux_osl.c

OBJFILES = ${SRCFILES:.c=.o}

#ifeq ($(wildcard ./upnp),)
#all:
#	cp -f ./prebuilt/$(UPNPLIB_NAME) .
#else
all: $(OBJFILES)
	$(LD) -shared -o $(UPNPLIB_NAME) $^
#endif

install: all
	install -d $(INSTALLDIR)/usr/lib
	install -m 755 $(UPNPLIB_NAME) $(INSTALLDIR)/usr/lib
	$(STRIP) $(INSTALLDIR)/usr/lib/$(UPNPLIB_NAME)

clean:
	rm -f $(UPNPLIB_NAME) $(OBJFILES)

.PHONY: clean
