#
# Copyright (C) 2015, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: wps_common_lib.mk 403251 2013-05-20 05:15:55Z $
#
# Linux makefile
#

WLAN_ComponentsInUse := bcmwifi
-include $(SRCBASE)/makefiles/WLAN_Common.mk

BLDTYPE = release
#BLDTYPE = debug

ifeq ($(BLDTYPE),debug)
export CFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
export CXXFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
else
export CFLAGS = -Wall -Wnested-externs
export CXXFLAGS = -Wall -Wnested-externs
endif

ifdef WCN_NET_SUPPORT
export CFLAGS += -DWCN_NET_SUPPORT   
export CXXFLAGS += -DWCN_NET_SUPPORT   
endif


ifeq ($(CC), arm-linux-gcc)
CFLAGS += -mstructure-size-boundary=8
STRIP = arm-linux-strip
endif
ifeq ($(CC), mipsel-uclibc-gcc)
STRIP = mipsel-uclibc-strip
endif
ifeq ($(CC), mipsel-linux-gcc)
STRIP = mipsel-linux-strip
endif
ifeq ($(CC), gcc)
STRIP = strip
endif

export LD = $(CC)
export LDFLAGS = -r

export INCLUDE = -I$(SRCBASE)/include -I$(SRCBASE)/common/include -I./include $(WLAN_ComponentIncPathR) $(WLAN_StdIncPathR)

# Include external openssl path
ifeq ($(EXTERNAL_OPENSSL),1)
export INCLUDE += -DEXTERNAL_OPENSSL -include wps_openssl.h -I$(EXTERNAL_OPENSSL_INC) -I$(EXTERNAL_OPENSSL_INC)/openssl
else
export INCLUDE += -I$(SRCBASE)/include/bcmcrypto 
endif

LIBDIR = .

OBJS =  $(addprefix $(LIBDIR)/, shared/tutrace.o \
	shared/dev_config.o \
	shared/wps_sslist.o \
	enrollee/enr_reg_sm.o \
	shared/reg_proto_utils.o \
	shared/reg_proto_msg.o \
	shared/tlv.o \
	shared/state_machine.o \
	shared/wps_utils.o \
	shared/ie_utils.o \
	shared/buffobj.o )

# Add OpenSSL wrap API if EXTERNAL_OPENSSL defined
ifeq ($(EXTERNAL_OPENSSL),1)
OBJS += $(addprefix $(LIBDIR)/, shared/wps_openssl.o)
endif

#Be aware share library may has "-m32" issue.
ifeq ($(CC), gcc)
default: $(LIBDIR)/libwpscom.a
else
default: $(LIBDIR)/libwpscom.a $(LIBDIR)/libwpscom.so
endif

$(LIBDIR)/libwpscom.a: $(OBJS)
	$(AR) cr  $@ $^ 

$(LIBDIR)/libwpscom.so: $(OBJS)
	$(LD) -shared -o $@ $^

$(LIBDIR)/shared/%.o :  shared/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/ap/%.o :  ap/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/enrollee/%.o :  enrollee/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/registrar/%.o :  registrar/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
