#
# Broadcom Linux Router WPS Makefile
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
REBUILD = $(shell if [ -f "prebuilt/wps_monitor" ]; then echo 0; else echo 1; fi)

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
ifeq ($(CONFIG_NFC),y)
export WPS_NFC_DEVICE = 1
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
# But WPS 2 Test plan 4.1.13 still need this feature support.
CFLAGS += -DWPS_ADDCLIENT_WWTP

CFLAGS += -Wall -Wnested-externs -D_REENTRANT -D__linux__
#CFLAGS += -Werror

ifeq ($(BLDTYPE), debug)
CFLAGS += -g
endif


CFLAGS += -I$(SRCBASE)/include ${WLAN_StdIncPathA} -I$(SRCBASE)/common/include -I$(SRCBASE)/wps/common/include	\
	-I$(SRCBASE)/router/libbcm -I$(SRCBASE)/router/eapd${BCMEX}$(EX7)	\
	-I$(SRCBASE)/router/shared -I$(SRCBASE)/wps/brcm_apps/include	\
	-I$(SRCBASE)/include/bcmcrypto ${WLAN_ComponentIncPath}

LDFLAGS += -lgcc_s

ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

WPSM_LIBS = -lnvram -lbcm -lshared -lbcmcrypto -lm
# Set link libraries search path
export LIBPATH  = -L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib
export LIBPATH += -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib
export LIBPATH += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib
export LIBPATH += -L$(TOP)/nvram${BCMEX} -L$(INSTALLDIR)/nvram/usr/lib
export LIBPATH += -L$(SRCBASE)/router/wps${BCMEX}$(EX7)/lib
export LIBPATH += -L$(SRCBASE)/router/wps${BCMEX}$(EX7)/prebuilt
ifeq ($(WPS_UPNP_DEVICE),1)
# Always use LIBUPNP
export LIBPATH += -L$(TOP)/libupnp${BCMEX}$(EX7) -L$(INSTALLDIR)/libupnp/usr/lib
WPSM_LIBS += -lupnp
CFLAGS += -I$(SRCBASE)/router/libupnp${BCMEX}$(EX7)/include -I$(SRCBASE)/wps/brcm_apps/upnp/WFADevice
endif

ifeq ($(WPS_NFC_DEVICE),1)
export LIBPATH += -L$(TOP)/nfc -L$(INSTALLDIR)/nfc/usr/lib
WPSM_LIBS += -lnsa -lipcnsa -lpthread
CFLAGS += -I$(SRCBASE)/wps/brcm_apps/nfc
CFLAGS += -I$(BASEDIR)/components/apps/nfc/3rdparty/embedded/nsa_examples/linux/libnsa/include
CFLAGS += -include $(BASEDIR)/components/apps/nfc/3rdparty/embedded/nsa_examples/linux/libnsa/include/buildcfg.h
endif

ifeq ($(RTCONFIG_BCM7),y)
CFLAGS += -DBCM7
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


ifneq ($(wildcard ./prebuilt),)
all: osl wps_monitor
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
	$(SRCBASE)/router/wps${BCMEX}$(EX7)/mkversion.sh $(WPSSRC)/common/include/version.h.in $(WPSSRC)/common/include/wps_version.h "$(PROD_TAG)"

# target
# force compile wps_monitor in case the library changed.
ifeq ($(wildcard ./prebuilt),)
wps_monitor : objs FORCE
	$(CC) $(LIBPATH) $(WPS_OSLOBJS) -l$(WPS_LIB_FILE) $(WPSM_LIBS) $(LDFLAGS) -o $@
else
wps_monitor :
endif

FORCE:


clean:
	find . \( -name '*.[od]' -o -name '*.tmp' \) -type f -print | xargs rm -f
	find -name obj | xargs rm -rf
	rm -rf lib
ifeq ($(REBUILD),1)
	find -name wps_monitor | xargs rm -f
endif

# install
install: all
	install -d $(INSTALLDIR)/wps${BCMEX}$(EX7)/bin
ifeq ($(REBUILD),0)
	install prebuilt/wps_monitor $(INSTALLDIR)/wps${BCMEX}$(EX7)/bin
else
	install wps_monitor $(INSTALLDIR)/wps${BCMEX}$(EX7)/bin
endif
	$(STRIP) $(INSTALLDIR)/wps${BCMEX}$(EX7)/bin/wps_monitor

.PHONY: all clean install libs dirs osl
