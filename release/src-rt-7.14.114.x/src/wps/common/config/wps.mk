# Helper makefile for building Broadcom wps libaries
# This file maps wps feature flags (import) to WPSFLAGS and WPSFILES (export).
#
# Copyright (C) 2015, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
# $Id: $

WPSFILES :=

ifeq ($(BLDTYPE), debug)
WPSFLAGS += -D_TUDEBUGTRACE
endif

ifeq ($(WCN_NET), 1)
WPSFLAGS += -DWCN_NET_SUPPORT   
endif

# Include external openssl path
ifeq ($(EXTERNAL_OPENSSL),1)
WPS_CRYPT = 0
WPSFLAGS += -DEXTERNAL_OPENSSL
WPSFILES += src/wps/common/shared/wps_openssl.c
endif

## wps common

## shared code 
WPSFILES += src/wps/common/shared/tutrace.c
WPSFILES += src/wps/common/shared/dev_config.c
WPSFILES += src/wps/common/shared/wps_sslist.c
WPSFILES += src/wps/common/enrollee/enr_reg_sm.c
WPSFILES += src/wps/common/registrar/reg_sm.c 
WPSFILES += src/wps/common/shared/reg_proto_utils.c
WPSFILES += src/wps/common/shared/reg_proto_msg.c
WPSFILES += src/wps/common/shared/tlv.c
WPSFILES += src/wps/common/shared/state_machine.c
WPSFILES += src/wps/common/shared/wps_utils.c
WPSFILES += src/wps/common/shared/ie_utils.c
WPSFILES += src/wps/common/shared/buffobj.c

# AP or APSTA
ifeq ($(WPS_AP), 1)
WPSFLAGS += -DBCMWPSAP
WPSFILES += src/wps/common/ap/ap_api.c
WPSFILES += src/wps/common/ap/ap_ssr.c
WPSFILES += src/wps/common/ap/ap_eap_sm.c
endif

# STA supports
ifeq ($(WPS_STA), 1)
WPSFLAGS += -DBCMWPSAPSTA
WPSFILES += src/wps/common/sta/sta_eap_sm.c
endif

# WPS monitor support
ifeq ($(WPS_ROUTER), 1)
WPSFILES += src/wps/brcm_apps/apps/wps_monitor.c
WPSFILES += src/wps/brcm_apps/apps/wps_aplockdown.c
WPSFILES += src/wps/brcm_apps/apps/wps_pb.c
WPSFILES += src/wps/brcm_apps/apps/wps_eap.c
WPSFILES += src/wps/brcm_apps/apps/wps_ie.c
WPSFILES += src/wps/brcm_apps/apps/wps_ui.c
WPSFILES += src/wps/brcm_apps/apps/wps_led.c

WPS_ROUTERHALFILES += src/wps/brcm_apps/arch/bcm947xx/wps_gpio.c
WPS_ROUTERHALFILES += src/wps/brcm_apps/arch/bcm947xx/wps_hal.c
WPS_ROUTERHALFILES += src/wps/brcm_apps/arch/bcm947xx/wps_wl.c

	# WFI supports
	ifeq ($(WPS_WFI),1)
	WPSFILES += src/wps/brcm_apps/apps/wps_wfi.c
	WPSFLAGS += -DBCMWFI
	endif

	ifeq ($(WPS_AP), 1)
	WPSFILES += src/wps/brcm_apps/apps/wps_ap.c 
		ifeq ($(WPS_UPNP_DEVICE),1)
			WPSFILES += src/wps/brcm_apps/upnp/WFADevice/soap_x_wfawlanconfig.c
			WPSFILES += src/wps/brcm_apps/upnp/WFADevice/WFADevice.c
			WPSFILES += src/wps/brcm_apps/upnp/WFADevice/WFADevice_table.c
			WPSFILES += src/wps/brcm_apps/upnp/WFADevice/xml_x_wfawlanconfig.c
			# Release xml_WFADevice.c for customization
			WPS_ROUTERHALFILES += src/wps/brcm_apps/upnp/WFADevice/xml_WFADevice.c
			WPSFILES += src/wps/common/ap/ap_upnp_sm.c
			WPSFILES += src/wps/brcm_apps/apps/wps_libupnp.c
			WPSFLAGS += -DWPS_UPNP_DEVICE
		endif
	endif

	ifeq ($(WPS_STA), 1)
	WPSFILES += src/wps/brcm_apps/apps/wps_sta.c
	WPS_ROUTERHALFILES += src/wps/brcm_apps/arch/bcm947xx/wps_sta_wl.c
	endif

	# NFC support
	ifeq ($(WPS_NFC_DEVICE), 1)
	WPSFILES += src/wps/brcm_apps/apps/wps_nfc.c
	WPSFILES += src/wps/brcm_apps/nfc/app_generic.c
	WPSFILES += src/wps/brcm_apps/nfc/app_mgt.c
	WPSFILES += src/wps/brcm_apps/nfc/app_nsa_utils.c
	endif
WPSFLAGS += -DWPS_ROUTER
endif # end WPS ROUTER

# Enrollee supports
ifeq ($(WPS_ENR),1)
WPSFILES += src/wps/common/enrollee/enr_api.c
endif

ifeq ($(WPS_CRYPT), 1)
CRYPTDIR = $(SRCBASE)/bcmcrypto
WPSFILES += src/bcmcrypto/aes.c
WPSFILES += src/bcmcrypto/rijndael-alg-fst.c
WPSFILES += src/bcmcrypto/dh.c
WPSFILES += src/bcmcrypto/bn.c
WPSFILES += src/bcmcrypto/sha256.c
WPSFILES += src/bcmcrypto/hmac_sha256.c
WPSFILES += src/bcmcrypto/random.c
endif

# NFC support
ifeq ($(WPS_NFC_DEVICE), 1)
WPSFILES += src/wps/common/shared/nfc_utils.c
WPSFLAGS += -DWPS_NFC_DEVICE
endif

export WPS_FLAGS = $(WPSFLAGS)
export WPS_FILES = $(WPSFILES)
export WPS_HALFILES = $(WPS_ROUTERHALFILES)
