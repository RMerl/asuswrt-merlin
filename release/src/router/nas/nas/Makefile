#
# 802.1x Authenticator (Network Access Server) Embedded Linux Build Makefile
#
# Copyright (C) 2011, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 315332 2012-02-16 07:06:36Z $
#

include ../../common.mak
CFLAGS	+= $(EXTRACFLAGS)

CFLAGS	+= -I.. -I$(SRCBASE)/include -I$(SRCBASE)/router/shared -Wall -Werror
CFLAGS	+= $(WLAN_ComponentIncPath)

#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s
CFLAGS  += -DNAS_WKSP_BUILD_NAS_AUTH
CFLAGS  += -DNAS_WKSP_BUILD_NAS_SUPPL
#CFLAGS  += -DNAS_GTK_PER_STA
CFLAGS  += -DNAS_RADIUS
CFLAGS  += -DNAS_WKSP_ON_DEMAND


CFLAGS	+= -DBCMSUPPL

# EAPD include path
CFLAGS	+= -I$(SRCBASE)/router/eapd

LDFLAGS += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared
LDFLAGS += -L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib -lbcmcrypto

vpath %.c $(SRCBASE)/router/nas $(SRCBASE)/shared $(SRCBASE)/router/shared

# build target
NASOBJS = mppe.o wpa.o nas.o nas_linux.o nas_wl.o nas_wksp.o
NASOBJS += nas_radius.o nas_wksp_radius.o
UTLOBJS = bcmwpa.o bcmutils.o

ifeq ($(wildcard ../*.c), )
all:
	-cp ./prebuilt/nas .
else
all: nas
endif

nas: $(NASOBJS) $(UTLOBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# install to target
install: all
	install -D nas $(INSTALLDIR)/usr/sbin/nas
	$(STRIP) $(INSTALLDIR)/usr/sbin/nas

# clean up mess
clean:
	rm -f *.o *~ nas
