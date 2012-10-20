#
# Broadcom Linux Router WPS  Makefile
#
# Copyright (C) 2011, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#

#
ifneq (true,$(parallel))
  .NOTPARALLEL:
endif

# Include linux kernel and router config
include $(TOP)/.config
include $(LINUXDIR)/.config

# Build type
BLDTYPE = release
#BLDTYPE = debug

# Get wireless driver support mode
ifneq ($(CONFIG_WL_USE_STA)$(CONFIG_WL_USE_STA_ONCHIP_G)$(CONFIG_WL_USE_APSTA)$(CONFIG_WL_USE_APSTA_ONCHIP_G),)
  ifneq ($(CONFIG_WL_USE_STA)$(CONFIG_WL_USE_STA_ONCHIP_G),)
    WL_MODE = sta
    UPNP_WFA_DEVICE = 0
  else
    WL_MODE = apsta
  endif
else
  WL_MODE = ap
endif

# UPnP WFA device support, default YES
UPNP_WFA_DEVICE ?= 1
ifeq ("$(CONFIG_UPNP)$(CONFIG_LIBUPNP)","")
UPNP_WFA_DEVICE = 0
endif
ifeq ($(WL_MODE),sta)
UPNP_WFA_DEVICE = 0
endif

# Set compile CFLAGS
export CFLAGS += -DBCMUPNP
export CFLAGS += -DWPS_AP_M2D

# Enable WWTP option cause the security hole described in WSC 2.0 specification section 4.3.1
#export CFLAGS += -DWPS_ADDCLIENT_WWTP

export CFLAGS += -s

ifeq ($(BLDTYPE),debug)
export CXXFLAGS += -Wall -D_REENTRANT -D_TUDEBUGTRACE -D__linux__ -DWPS_MODE_AP
export CFLAGS += -Wall -Wnested-externs  -D_REENTRANT -D_TUDEBUGTRACE -D__linux__
else
export CXXFLAGS += -Os -Wall -D_REENTRANT -D__linux__ -DWPS_MODE_AP
export CFLAGS += -Os -Wall -Wnested-externs -D_REENTRANT -D__linux__
endif
ifeq ($(UPNP_WFA_DEVICE),1)
  export CXXFLAGS += -DUPNP_WFA_DEVICE
  export CFLAGS += -DUPNP_WFA_DEVICE
  ifeq ($(CONFIG_LIBUPNP),y)
    export CXXFLAGS += -D__CONFIG_LIBUPNP__
    export CFLAGS += -D__CONFIG_LIBUPNP__
  endif
endif

# Set compile include fiels search path
export INCLUDE = -I$(SRCBASE)/include -I$(SRCBASE)/include/bcmcrypto \
		 -I$(SRCBASE)/router/shared -I$(SRCBASE)/wps/common/include -I$(SRCBASE)/wps/brcm_apps/include \
		 -I$(SRCBASE)/router/libbcm -I$(SRCBASE)/router/eapd
ifeq ($(UPNP_WFA_DEVICE),1)
  ifeq ($(CONFIG_LIBUPNP),y)
    export INCLUDE += -I$(SRCBASE)/router/libupnp/include -I$(SRCBASE)/wps/brcm_apps/upnp/WFADevice
  else
    export INCLUDE += -I$(SRCBASE)/router/bcmupnp/include -I$(SRCBASE)/router/bcmupnp/device \
		      -I$(SRCBASE)/router/bcmupnp/device/WFADevice
  endif
endif

# Set link libraries search path
export LIBPATH  = -L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib
export LIBPATH += -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib
export LIBPATH += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib
export LIBPATH += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib
export LIBPATH += -L$(SRCBASE)/router/wps/lib
export LIBPATH += -L$(SRCBASE)/router/wps/prebuilt
ifeq ($(UPNP_WFA_DEVICE),1)
  ifeq ($(CONFIG_LIBUPNP),y)
    export LIBPATH += -L$(TOP)/libupnp -L$(INSTALLDIR)/libupnp/usr/lib
  endif
endif
export LDFLAGS = -r


WPSSRC = $(SRCBASE)/wps
WPSCOMSRC = $(WPSSRC)/common
WPSAPPSSRC = $(WPSSRC)/brcm_apps
OBJDIR = obj

#
# wps library variants
#
WPSM_AP_LIB = wps_ap
WPSM_AP_LIB_FILE = lib/libwps_ap.a
WPSM_STA_LIB = wps_sta
WPSM_STA_LIB_FILE = lib/libwps_sta.a
WPSM_APSTA_LIB = wps_apsta
WPSM_APSTA_LIB_FILE = lib/libwps_apsta.a

WPSM_LIBS = -lnvram -lbcm -lshared -lbcmcrypto -lm -lc


#
# wps common objects
#
WPSM_OBJS = $(OBJDIR)/shared/tutrace.o $(OBJDIR)/shared/dev_config.o \
	$(OBJDIR)/shared/slist.o $(OBJDIR)/enrollee/enr_reg_sm.o \
	$(OBJDIR)/shared/reg_proto_utils.o $(OBJDIR)/shared/reg_proto_msg.o \
	$(OBJDIR)/shared/tlv.o $(OBJDIR)/shared/state_machine.o \
	$(OBJDIR)/shared/buffobj.o $(OBJDIR)/shared/wps_utils.o

# wps common ap objects
WPSM_AP_OBJS = $(OBJDIR)/ap/ap_api.o $(OBJDIR)/ap/ap_eap_sm.o \
	$(OBJDIR)/ap/ap_ssr.o $(OBJDIR)/registrar/reg_sm.o
ifeq ($(UPNP_WFA_DEVICE),1)
WPSM_AP_OBJS += $(OBJDIR)/ap/ap_upnp_sm.o
endif

# wps common sta enrollee objects
WPSM_STA_OBJS = $(OBJDIR)/enrollee/enr_api.o $(OBJDIR)/sta/sta_eap_sm.o \
	$(OBJDIR)/enrollee/enr_reg_sm.o $(OBJDIR)/registrar/reg_sm.o


#
# wps monitor objects
#
WPSM_OBJS +=  $(OBJDIR)/application/apps/wps_monitor.o $(OBJDIR)/application/apps/wps_aplockdown.o \
	$(OBJDIR)/application/apps/wps_pb.o $(OBJDIR)/application/apps/wps_led.o \
	$(OBJDIR)/application/apps/wps_eap.o $(OBJDIR)/application/apps/wps_ie.o \
	$(OBJDIR)/application/apps/wps_ui.o $(OBJDIR)/application/arch/bcm947xx/wps_gpio.o \
	$(OBJDIR)/application/arch/bcm947xx/wps_hal.o $(OBJDIR)/application/arch/bcm947xx/wps_wl.o

# wps monitor ap objects
WPSM_AP_OBJS += $(OBJDIR)/application/apps/wps_ap.o
ifeq ($(UPNP_WFA_DEVICE),1)
  ifeq ($(CONFIG_LIBUPNP),y)
    WPSM_AP_OBJS += $(OBJDIR)/application/apps/wps_libupnp.o
  else
    WPSM_AP_OBJS += $(OBJDIR)/application/apps/wps_upnp.o
  endif
endif
ifeq ($(CONFIG_WFI),y)
WPSM_AP_OBJS += $(OBJDIR)/application/apps/wps_wfi.o
CFLAGS += -DBCMWFI
endif
ifneq ($(WL_MODE),sta)
ifeq ($(UPNP_WFA_DEVICE),1)
ifeq ($(CONFIG_LIBUPNP),y)
WPSM_AP_OBJS += $(OBJDIR)/application/upnp/WFADevice/soap_x_wfawlanconfig.o \
		$(OBJDIR)/application/upnp/WFADevice/WFADevice.o \
		$(OBJDIR)/application/upnp/WFADevice/WFADevice_table.o \
		$(OBJDIR)/application/upnp/WFADevice/xml_WFADevice.o \
		$(OBJDIR)/application/upnp/WFADevice/xml_x_wfawlanconfig.o
WPSM_LIBS += -lupnp
endif
endif
endif

# wps monitor sta enrollee objects
WPSM_STA_OBJS += $(OBJDIR)/application/apps/wps_enr_core.o $(OBJDIR)/application/arch/bcm947xx/wps_sta_wl.o

# wps monitor osl objects
WPSM_OSL_OBJS = $(OBJDIR)/application/linux/wps_linux_main.o $(OBJDIR)/application/linux/wps_linux_osl.o
WPSM_STA_OSL_OBJS = $(OBJDIR)/application/linux/wps_enr_linux_osl.o

ifeq ($(WL_MODE),sta)
export CXXFLAGS += -DBCMWPSAPSTA
export CFLAGS += -DBCMWPSAPSTA
WPSM_LIBS += -l$(WPSM_STA_LIB)
WPSM_OSL_OBJS += $(WPSM_STA_OSL_OBJS)
endif
ifeq ($(WL_MODE),apsta)
export CXXFLAGS += -DBCMWPSAP -DBCMWPSAPSTA
export CFLAGS += -DBCMWPSAP -DBCMWPSAPSTA
WPSM_LIBS += -l$(WPSM_APSTA_LIB)
WPSM_OSL_OBJS += $(WPSM_STA_OSL_OBJS)
endif
ifeq ($(WL_MODE),ap)
export CXXFLAGS += -DBCMWPSAP
export CFLAGS += -DBCMWPSAP
WPSM_LIBS += -l$(WPSM_AP_LIB)
endif

ifneq ($(wildcard ./prebuilt),)
WPSM_LIB_FILES =
all: dirs wps_monitor
else
WPSM_LIB_FILES = $(WPSM_AP_LIB_FILE) $(WPSM_STA_LIB_FILE) $(WPSM_APSTA_LIB_FILE)
all: mkversion sta_build_check libs wps_monitor
endif


mkversion :
	$(SRCBASE)/router/wps/mkversion.sh $(WPSSRC)/common/include/version.h.in $(WPSSRC)/common/include/wps_version.h "$(PROD_TAG)"

# all library variants
libs : dirs objs $(WPSM_LIB_FILES)

dirs :
	mkdir -p $(OBJDIR)/ap
	mkdir -p $(OBJDIR)/enrollee
	mkdir -p $(OBJDIR)/registrar
	mkdir -p $(OBJDIR)/shared
	mkdir -p $(OBJDIR)/sta
	mkdir -p $(OBJDIR)/bcmcrypto
	mkdir -p $(OBJDIR)/application/apps
	mkdir -p $(OBJDIR)/application/linux
	mkdir -p $(OBJDIR)/application/arch/bcm947xx
ifeq ($(UPNP_WFA_DEVICE),1)
ifeq ($(CONFIG_LIBUPNP),y)
	mkdir -p $(OBJDIR)/application/upnp/WFADevice
endif
endif
	mkdir -p lib

sta_build_check:
ifeq ($(WL_MODE),sta)
ifeq ($(UPNP_WFA_DEVICE),1)
	@echo "ERROR: UPNP_WFA_DEVICE is not supported on STA mode"
	@exit 1
endif
endif
	@true

objs: dirs
	+$(MAKE) parallel=true $(WPSM_OBJS) $(WPSM_AP_OBJS) $(WPSM_STA_OBJS)

# library rules
$(WPSM_AP_LIB_FILE) : objs
	$(AR) cr $@ $(WPSM_OBJS) $(WPSM_AP_OBJS)

$(WPSM_STA_LIB_FILE) : objs
	$(AR) cr $@ $(WPSM_OBJS) $(WPSM_STA_OBJS)

$(WPSM_APSTA_LIB_FILE) : objs
	$(AR) cr $@ $(WPSM_OBJS) $(WPSM_AP_OBJS) $(WPSM_STA_OBJS)

# target
ifeq ($(wildcard ./prebuilt),)
wps_monitor : $(WPSM_OSL_OBJS) $(WPSM_LIB_FILES)
	$(CC) $(WPSM_OSL_OBJS) $(LIBPATH) $(WPSM_LIBS) -o $@
else
wps_monitor :
endif


# object rules
$(OBJDIR)/application/linux/%.o : $(WPSAPPSSRC)/linux/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(OBJDIR)/application/apps/%.o : $(WPSAPPSSRC)/apps/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(OBJDIR)/application/arch/bcm947xx/%.o : $(WPSAPPSSRC)/arch/bcm947xx/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

ifeq ($(UPNP_WFA_DEVICE),1)
ifeq ($(CONFIG_LIBUPNP),y)
$(OBJDIR)/application/upnp/WFADevice/%.o : $(WPSAPPSSRC)/upnp/WFADevice/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<
endif
endif

$(OBJDIR)/shared/%.o : $(WPSCOMSRC)/shared/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(OBJDIR)/ap/%.o : $(WPSCOMSRC)/ap/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(OBJDIR)/enrollee/%.o : $(WPSCOMSRC)/enrollee/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(OBJDIR)/registrar/%.o : $(WPSCOMSRC)/registrar/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(OBJDIR)/sta/%.o : $(WPSCOMSRC)/sta/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

# clean
clean:
	rm -rf $(OBJDIR)
	rm -rf lib
	find -name wps_monitor | xargs rm -f

# install
install: all
	install -d $(INSTALLDIR)/wps/bin
	install wps_monitor $(INSTALLDIR)/wps/bin
	$(STRIP) $(INSTALLDIR)/wps/bin/wps_monitor

.PHONY: all clean install libs dirs objs
