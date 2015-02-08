#
# Linux Traffic Ordering Agent Daemon Makefile
#
# Copyright (C) 2014, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 437617 2013-11-19 14:21:24Z $
#
CFLAGS	+= -s -Wall -Werror
ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif
#CFLAGS	+= -gstabs -DDEBUG
CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include ${WLAN_StdIncPathA} -I$(SRCBASE)/common/include
CFLAGS	+= $(if $(WLAN_ComponentIncPath),$(WLAN_ComponentIncPath),$(addprefix -I,$(wildcard $(SRCBASE)/shared/bcmwifi/include)))
LDFLAGS += -L$(TOP)/nvram${BCMEX} -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared
LDFLAGS += -lgcc_s


vpath %.c $(SRCBASE)/shared/

TOAD_OBJS = toad.o toa_utils.o

TOAST_OBJS = toast.o toa_utils.o

BINARIES = toad toast

ifeq ($(wildcard $(SRCBASE)/router/toad/*.c),)
all:
	-cp -f prebuilt/toad toad
	-cp -f prebuilt/toast toast
else
all: $(BINARIES)
endif


install: all
	install -d $(INSTALLDIR)/usr/sbin/
	install toad $(INSTALLDIR)/usr/sbin/
	install toast $(INSTALLDIR)/usr/sbin/
	$(STRIP) $(INSTALLDIR)/usr/sbin/toad
	$(STRIP) $(INSTALLDIR)/usr/sbin/toast

clean:
	rm -f $(TOAD_OBJS) $(BINARIES)

${TOAD_OBJS}: toa_utils.h

${TOAST_OBJS}: toa_utils.h

toad: ${TOAD_OBJS}
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)

toast: ${TOAST_OBJS}
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
