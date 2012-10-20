#
# Broadcom Linux Router WPS Makefile
# 
# Copyright (C) 2011, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
# $Id: $
#

ifneq (true,$(parallel))
  .NOTPARALLEL:
endif

# Build type
export BLDTYPE = release
#export BLDTYPE = debug


include $(TOP)/.config

# linux kernel config
include $(LINUXDIR)/.config

export WPSSRC = $(SRCBASE)/wps

# Get wireless driver support mode
ifneq ($(CONFIG_WL_USE_STA)$(CONFIG_WL_USE_STA_ONCHIP_G)$(CONFIG_WL_USE_APSTA)$(CONFIG_WL_USE_APSTA_ONCHIP_G),)
  ifneq ($(CONFIG_WL_USE_STA)$(CONFIG_WL_USE_STA_ONCHIP_G),)
    export WL_MODE = sta
    export WPS_CONF_FILE = wpsconfig_router_sta
    WPS_LIB_FILE = wps_sta
  else
    export WL_MODE = apsta
    export WPS_CONF_FILE = wpsconfig_router_apsta
    WPS_LIB_FILE = wps_apsta
  endif
else
  export WL_MODE = ap
  export WPS_CONF_FILE = wpsconfig_router_ap
  WPS_LIB_FILE = wps_ap
endif

ifeq ($(RTCONFIG_4M_SFP),y)
# Force to support AP only
export WL_MODE = ap
export WPS_CONF_FILE = wpsconfig_router_ap
WPS_LIB_FILE = wps_ap
endif

# Include wps common make file
include $(WPSSRC)/common/config/$(WPS_CONF_FILE)
ifneq ($(CONFIG_WFI),y)
WPS_WFI = 0
endif
ifneq ($(CONFIG_LIBUPNP),y)
WPS_UPNP_DEVICE = 0
endif
ifeq ($(CONFIG_PLC),y)
CFLAGS += -DWPS_PLC
endif
include $(WPSSRC)/common/config/wps.mk
EXTRA_CFLAGS = $(WPS_FLAGS)

# Generate list of supported variants
VLIST  = AP STA APSTA

# Set suffix and config based on the selected variant
WPS_SUFFIX      := $(strip $(foreach v,$(VLIST),$(if $(CONFIG_WL_USE_$(v)),$(v))))

# Module file names use lowercase of the suffix
wps_suffix      := $(shell echo "$(WPS_SUFFIX)" | tr A-Z a-z)

# Always do variants build
wpssubdirs := $(addprefix wps_,$(shell echo "$(VLIST)" | tr A-Z a-z))

#
# Set CFLAGS
#
CFLAGS += -DWPS_AP_M2D

# Enable WWTP option cause the security hole described in WSC 2.0 specification section 4.3.1
#CFLAGS += -DWPS_ADDCLIENT_WWTP

CFLAGS += -Wall -Wnested-externs -D_REENTRANT -D__linux__
#CFLAGS += -Werror

ifeq ($(BLDTYPE), debug)
CFLAGS += -g
endif


CFLAGS += -I$(SRCBASE)/include -I$(SRCBASE)/wps/common/include	\
	-I$(SRCBASE)/router/libbcm -I$(SRCBASE)/router/eapd	\
	-I$(SRCBASE)/router/shared -I$(SRCBASE)/wps/brcm_apps/include	\
	-I$(SRCBASE)/include/bcmcrypto ${WLAN_ComponentIncPath}

LDFLAGS = -r

WPSM_LIBS = -lnvram -lbcm -lshared -lbcmcrypto -lm -lc
# Set link libraries search path
export LIBPATH  = -L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib
export LIBPATH += -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib
export LIBPATH += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib
export LIBPATH += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib
export LIBPATH += -L$(SRCBASE)/router/wps/lib
export LIBPATH += -L$(SRCBASE)/router/wps/prebuilt
ifeq ($(WPS_UPNP_DEVICE),1)
# Always use LIBUPNP
export LIBPATH += -L$(TOP)/libupnp -L$(INSTALLDIR)/libupnp/usr/lib
WPSM_LIBS += -lupnp
CFLAGS += -I$(SRCBASE)/router/libupnp/include -I$(SRCBASE)/wps/brcm_apps/upnp/WFADevice
endif

WPS_OSL := $(WPS_ROUTERHALFILES)
WPS_OSL += src/wps/brcm_apps/linux/wps_linux_main.c

WPS_OSL += src/wps/brcm_apps/linux/wps_linux_osl.c
ifneq ($(WL_MODE), ap)
WPS_OSL += src/wps/brcm_apps/linux/wps_enr_linux_osl.c
endif

WPS_OSLOBJS := $(foreach file, $(WPS_OSL), \
	 $(patsubst src/%.c,obj/%.o,$(file)))

WPS_DIRS := $(foreach file, $(WPS_OSLOBJS), \
	 $(dir $(file)))

WPS_DIRLIST = $(sort $(WPS_DIRS)) 


ifeq ($(wildcard $(SRCBASE)/wps/brcm_apps/linux),)
all:
	cp -f ./prebuilt/wps_monitor .
else
all: mkversion sta_build_check dirs variants_build osl wps_monitor
endif

osl:
	mkdir -p $(WPS_DIRLIST)
	@echo "==>$(WPS_CONF_FILE)"

ifneq ($(wildcard ./prebuilt),)
objs:
else
objs: variants_build
endif
	+$(MAKE) parallel=true $(WPS_OSLOBJS)

$(WPS_OSLOBJS) : obj%.o: $(addprefix $(SRCBASE)/../,src%.c)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Variants build rules
variants_build: $(patsubst %, _dir_%, $(wpssubdirs))
$(patsubst %, _dir_%, $(wpssubdirs)):
	$(if $(wildcard $(patsubst _dir_%, %, $@)),+$(MAKE) -C $(patsubst _dir_%, %, $@))

# Set link libraries search path
export LIBPATH += -L. -L./prebuilt


sta_build_check:
ifeq ($(WL_MODE),sta)
ifeq ($(WPS_UPNP_DEVICE),1)
	@echo "ERROR: UPNP_WFA_DEVICE is not supported on STA mode"
	@exit 1
endif
endif
	@true


dirs :
	mkdir -p lib

mkversion :
	$(SRCBASE)/router/wps/mkversion.sh $(WPSSRC)/common/include/version.h.in $(WPSSRC)/common/include/wps_version.h "$(PROD_TAG)"

# target
# force compile wps_monitor in case the library changed.
wps_monitor : objs FORCE
	$(CC) $(LIBPATH) $(WPS_OSLOBJS) $(WPSM_LIBS) -l$(WPS_LIB_FILE) -o $@

FORCE:


clean:
	find . \( -name '*.[od]' -o -name '*.tmp' \) -type f -print | xargs rm -f
	find -name obj | xargs rm -rf
	rm -rf lib
	rm -rf wps_monitor 

# install
install: all
	install -d $(INSTALLDIR)/wps/bin
	install wps_monitor $(INSTALLDIR)/wps/bin
	$(STRIP) $(INSTALLDIR)/wps/bin/wps_monitor

.PHONY: all clean install libs dirs osl
