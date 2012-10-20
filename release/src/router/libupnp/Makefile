#
# Copyright (C) 2011, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 311089 2012-01-27 13:47:40Z $
#

UPNPLIB_NAME = libupnp.so
UPNPLIB = $(SRCBASE)/router/libupnp
SRCPATH = $(UPNPLIB)/upnp

INCLUDES += -I$(UPNPLIB)/include -I$(SRCBASE)/include -I$(SRCBASE)/include/bcmcrypto -I$(SRCBASE)/router/shared
CFLAGS += ${INCLUDES}

CFLAGS += -Wall -Wunused -g -s -Werror -fPIC


LDFLAGS = -L$(TOP)/libbcmcrypto -lbcmcrypto

vpath %.c $(SRCPATH) $(UPNPLIB)/linux
vpath %.o $(UPNPLIB)/prebuilt

SRCFILES = upnp.c upnp_ssdp.c upnp_http.c upnp_gena.c upnp_soap.c \
	upnp_description.c upnp_device.c upnp_util.c upnp_msg.c \
	upnp_linux_osl.c

OBJFILES = ${SRCFILES:.c=.o}

ifeq ($(wildcard ./upnp),)
all:
	cp -f ./prebuilt/$(UPNPLIB_NAME) .
else
all: $(OBJFILES)
	$(LD) -shared -o $(UPNPLIB_NAME) $^
endif

install: all
	install -d $(INSTALLDIR)/usr/lib
	install -m 755 $(UPNPLIB_NAME) $(INSTALLDIR)/usr/lib
	$(STRIP) $(INSTALLDIR)/usr/lib/$(UPNPLIB_NAME)

clean:
	rm -f $(UPNPLIB_NAME) $(OBJFILES)

.PHONY: clean
