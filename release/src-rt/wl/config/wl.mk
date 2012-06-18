# Helper makefile for building Broadcom wl device driver
# This file maps wl driver feature flags (import) to WLFLAGS and WLFILES_SRC (export).
#
# Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# $Id: wl.mk,v 1.369.2.53 2011-01-26 19:40:43 Exp $


#ifdef WL_PPR_SUBBAND
ifeq ($(WL_PPR_SUBBAND),1)
	WLFLAGS += -DWL_PPR_SUBBAND
endif
#endif


# debug/internal
ifeq ($(DEBUG),1)
	WLFLAGS += -DBCMDBG -DWLTEST -DWLTIMER -DWIFI_ACT_FRAME
#ifndef LINUX_HYBRID
	WLFLAGS += -DRWL_WIFI -DWLRWL
	WLRWL = 1
#endif 
else
#ifdef WLTEST
	# mfgtest builds
	ifeq ($(WLTEST),1)
		WLFLAGS += -DWLTEST
	endif
#endif
endif

#ifdef WLMFG
# wl mfg support
ifeq ($(WLMFG),1)
	WLFLAGS += -DWLMFG -DWLTEST
	WLFILES_SRC_HI += src/wl/sys/wlc_mfg.c
endif
#endif


#ifdef BCMDBG_MEM
ifeq ($(BCMDBG_MEM),1)
	WLFLAGS += -DBCMDBG_MEM
endif
#endif

#ifdef BCMDBG_PKT
ifeq ($(BCMDBG_PKT),1)
	WLFLAGS += -DBCMDBG_PKT
endif
#endif

#ifdef BCMDBG_TRAP
# CPU debug traps (lomem access, divide by 0, etc) are enabled except when mogrified out for
# external releases.
WLFLAGS += -DBCMDBG_TRAP
#endif

#ifdef WLLMAC
ifeq ($(WLLMAC),1)
	WLFLAGS += -DWLLMAC -DEXTENDED_SCAN
endif
#endif


## wl driver common

ifndef WL_LOW
WL_LOW = 1
endif

ifndef WL_HIGH
WL_HIGH = 1
endif

## iff one of WLC_LOW and WLC_HIGH is defined, SPLIT is true
WL_SPLIT = 0
ifeq ($(WL_LOW),1)
	ifneq ($(WL_HIGH),1)
		WL_SPLIT = 1
	endif
endif

ifeq ($(WL_HIGH),1)
	ifneq ($(WL_LOW),1)
		WL_SPLIT = 1
		ifeq ($(RPC_NOCOPY),1)
			WLFLAGS += -DBCM_RPC_NOCOPY
		endif
		ifeq ($(RPC_RXNOCOPY),1)
			WLFLAGS += -DBCM_RPC_RXNOCOPY
		endif
		ifeq ($(RPC_TXNOCOPY),1)
			WLFLAGS += -DBCM_RPC_TXNOCOPY
		endif
		ifeq ($(RPC_TOC),1)
			WLFLAGS += -DBCM_RPC_TOC
		endif
		ifeq ($(RPC_ROC),1)
			WLFLAGS += -DBCM_RPC_ROC
		endif
		ifeq ($(BMAC_ENABLE_LINUX_HOST_RPCAGG),1)
			WLFLAGS += -DBMAC_ENABLE_LINUX_HOST_RPCAGG
		endif
		ifeq ($(DBUS_LINUX_RXDPC),1)
			WLFLAGS += -DDBUS_LINUX_RXDPC
		endif
		ifneq ($(BCM_RPC_TP_DBUS_NTXQ),)
			WLFLAGS += -DBCM_RPC_TP_DBUS_NTXQ=$(BCM_RPC_TP_DBUS_NTXQ)
		endif
		ifneq ($(BCM_RPC_TP_DBUS_NRXQ),)
			WLFLAGS += -DBCM_RPC_TP_DBUS_NRXQ=$(BCM_RPC_TP_DBUS_NRXQ)
		endif
		ifeq ($(BCMUSBDEV_EP_FOR_RPCRETURN),1)
			WLFLAGS += -DBCMUSBDEV_EP_FOR_RPCRETURN
		endif
	endif
endif

ifeq ($(WL_LOW),1)
	WLFLAGS += -DWLC_LOW
	ifneq ($(WL_HIGH),1)
		ifneq ($(BCM_RPC_TP_FLOWCTL_QWM_HIGH),)
			WLFLAGS += -DBCM_RPC_TP_FLOWCTL_QWM_HIGH=$(BCM_RPC_TP_FLOWCTL_QWM_HIGH)
		endif
	endif
endif

ifeq ($(WL_HIGH),1)
	WLFLAGS += -DWLC_HIGH
endif

ifeq ($(GTK_RESET),1)
	WLFLAGS += -DGTK_RESET
endif

ifeq ($(WLUSBAP),1)
	WLFLAGS += -DUSBAP
	#WLFLAGS += -DEHCI_FASTPATH_TX -DEHCI_FASTPATH_RX
endif

# split driver infrastructure files
ifeq ($(WL_SPLIT),1)
	WLFILES_SRC += src/shared/bcm_xdr.c
	WLFILES_SRC += src/shared/bcm_rpc.c
	ifneq ($(WLUSBAP),1)
		WLFILES_SRC_HI += src/shared/nvramstubs.c
	endif
	ifeq ($(OSLLX),1)
		WLFILES_SRC_HI += src/shared/linux_rpc_osl.c
	endif

	ifeq ($(OSLNDIS),1)
		WLFILES_SRC_HI += src/shared/ndis_rpc_osl.c
	endif

	ifeq ($(BCMDBUS),1)
		WLFILES_SRC_HI += src/shared/bcm_rpc_tp_dbus.c
	endif

	WLFILES_SRC_HI += src/wl/sys/wlc_bmac_stubs.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rpctx.c
	WLFILES_SRC_LO += src/wl/sys/wlc_high_stubs.c

	ifeq ($(WL_HIGH),1)
		# for SDIO BMAC, OSLREGOPS can not be a global define, dbus need direct register access 
		# OSLREGOPS is defined in wlc_cfg.h
		ifeq ($(BCMSDIO),1)
			ifneq ($(WLVISTA)$(WLWIN7),)
				WLFILES_SRC_HI += src/shared/sdh_ndis.c
			endif
		else
			WLFLAGS += -DBCMBUSTYPE=RPC_BUS
			WLFLAGS += -DOSLREGOPS
		endif
	endif
endif

#ifdef WL
ifeq ($(WL),1)
	WLFILES_SRC += src/shared/bcmwifi.c
	WLFILES_SRC += src/shared/bcmevent.c
	WLFILES_SRC += src/wl/sys/wlc_alloc.c

	WLFILES_SRC_LO += src/shared/qmath.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_le15.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_gt15.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_ge24.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_cmn.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_ssn.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_ssn.c
	WLFILES_SRC_LO += src/wl/sys/wlc_phy_shim.c
	WLFILES_SRC_LO += src/wl/sys/wlc_bmac.c

	ifneq ($(MINIAP),1)
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_ht.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_ht.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_lcn40.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_lcn40.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_lcn.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_lcn.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_lp.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_lp.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_abg.c
	endif

	WLFILES_SRC_HI += src/wl/sys/wlc.c
	WLFILES_SRC_HI += src/wl/sys/wlc_assoc.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rate.c
	WLFILES_SRC_HI += src/wl/sys/wlc_stf.c
	ifneq ($(WLWSEC),0)
		WLFLAGS += -DWLWSEC
		WLFILES_SRC_HI += src/wl/sys/wlc_security.c
		WLFILES_SRC_HI += src/wl/sys/wlc_key.c
	endif
	WLFILES_SRC_HI += src/wl/sys/wlc_scb.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rate_sel.c
	WLFILES_SRC_HI += src/wl/sys/wlc_antsel.c
	WLFILES_SRC_HI += src/wl/sys/wlc_bsscfg.c
	WLFILES_SRC_HI += src/wl/sys/wlc_scan.c
	WLFILES_SRC_HI += src/wl/phy/wlc_phy_iovar.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rm.c
	# tpc module is shared by 11h tpc and wl tx power control */
	WLTPC := 1
	ifeq ($(WLTPC),1)
		WLFLAGS += -DWLTPC
	endif
	ifeq ($(WLLMAC),1)
		WLFILES_SRC += src/wl/sys/wlc_lmac.c
		ifeq ($(STA), 1)
			WLFILES_SRC += src/wl/sys/wlc_lmac_sta.c
		endif
		ifeq ($(WLLMACPROTO),1)
			WLFLAGS += -DWLLMACPROTO
			WLFILES_SRC += src/wl/sys/wlc_lmac_proto.c
		endif
		ifeq ($(WLLMAC_ONLY),1)
			WLFLAGS += -DWLLMAC_ONLY
			WLFLAGS += -DWLNOEIND
			WLFILES_SRC_HI += src/wl/sys/wlc_channel_dummy.c
		else
			WLFILES_SRC_HI += src/wl/sys/wlc_event.c
			WLFILES_SRC_HI += src/wl/sys/wlc_channel.c
		endif
	else
		WLFILES_SRC_HI += src/wl/sys/wlc_event.c
		ifeq ($(WLCHANNEL_DUMMY),1)
			WLFILES_SRC_HI += src/wl/sys/wlc_channel_dummy.c
		else
			WLFILES_SRC_HI += src/wl/sys/wlc_channel.c
		endif
	endif
	WLFILES_SRC_HI += src/shared/bcmwpa.c
#ifndef LINUX_CRYPTO
	ifneq ($(LINUX_CRYPTO),1)
		WLFILES_SRC_HI += src/bcmcrypto/rc4.c
		WLFILES_SRC_HI += src/bcmcrypto/tkhash.c
		WLFILES_SRC_HI += src/bcmcrypto/tkmic.c
		WLFILES_SRC_HI += src/bcmcrypto/wep.c
	endif
#endif /* LINUX_CRYPTO */
#ifdef WLEXTLOG
ifeq ($(WLEXTLOG),1)
	WLFLAGS += -DWLEXTLOG
	WLFILES_SRC += src/wl/sys/wlc_extlog.c
endif
#endif
#ifdef WLSCANCACHE
ifeq ($(WLSCANCACHE),1)
	WLFLAGS += -DWLSCANCACHE
	WLFILES_SRC += src/wl/sys/wlc_scandb.c
endif
#endif

endif
#endif /* WL */

#ifdef BCMDBUS
ifeq ($(BCMDBUS),1)
	WLFLAGS += -DBCMDBUS
	WLFILES_SRC += src/shared/dbus.c

	ifeq ($(BCMSDIO),1)
		ifeq ($(WL_HIGH),1)
			WLFILES_SRC_HI += src/shared/siutils.c
			WLFILES_SRC_HI += src/shared/bcmotp.c
			WLFILES_SRC_HI += src/shared/sbutils.c
			WLFILES_SRC_HI += src/shared/aiutils.c
			WLFILES_SRC_HI += src/shared/hndpmu.c
		endif
	endif

	ifeq ($(WLLX),1)
		ifeq ($(BCMSDIO),1)
			WLFILES_SRC += src/shared/dbus_sdh.c
			WLFILES_SRC += src/shared/dbus_sdio_linux.c
		else
			WLFILES_SRC += src/shared/dbus_usb.c
			WLFILES_SRC += src/shared/dbus_usb_linux.c
		endif
	else
		ifeq ($(WLNDIS),1)
			ifeq ($(BCMSDIO),1)
				ifneq ($(WLVISTA)$(WLWIN7),)
					WLFILES_SRC += src/shared/dbus_sdio.c
					WLFILES_SRC += src/shared/dbus_sdio_ndis.c
				else
					WLFILES_SRC += src/shared/dbus_sdh.c
					WLFILES_SRC += src/shared/dbus_sdh_ndis.c
				endif
			else
				WLFILES_SRC += src/shared/dbus_usb.c
				WLFILES_SRC += src/shared/dbus_usb_ndis.c
			endif
		endif
	endif
endif
#endif

#ifndef LINUX_HYBRID
ifeq ($(BCM_DNGL_EMBEDIMAGE),1)
	WLFLAGS += -DBCM_DNGL_EMBEDIMAGE
endif
#endif

# For USBAP to select which images to embed
ifeq ($(EMBED_IMAGE_4319usb),1)
	WLFLAGS += -DEMBED_IMAGE_4319usb
endif
ifeq ($(EMBED_IMAGE_4319sd),1)
	WLFLAGS += -DEMBED_IMAGE_4319sd
endif
ifeq ($(EMBED_IMAGE_4322),1)
	WLFLAGS += -DEMBED_IMAGE_4322
endif

ifeq ($(EMBED_IMAGE_43236b1),1)
	WLFLAGS += -DEMBED_IMAGE_43236b1
endif
ifeq ($(EMBED_IMAGE_43237a0),1)
	WLFLAGS += -DEMBED_IMAGE_43237a0
endif

ifeq ($(WL_DNGL_WD),1)
	WLFLAGS += -DWL_DNGL_WD
ifneq ($(WL_DNGL_WD_DEFAULT_TIMEOUT),)
	WLFLAGS += -DWL_DNGL_WD_DEFAULT_TIMEOUT=$(WL_DNGL_WD_DEFAULT_TIMEOUT)
endif
endif

## wl OSL
#ifdef WLVX
ifeq ($(WLVX),1)
	WLFILES_SRC += src/wl/sys/wl_vx.c
	WLFILES_SRC += src/shared/bcmstdlib.c
	WLFLAGS += -DSEC_TXC_ENABLED
endif
#endif

#ifdef WLBSD
ifeq ($(WLBSD),1)
	WLFILES_SRC += src/wl/sys/wl_bsd.c
endif
#endif

#ifdef WLUM
ifeq ($(WLUM),1)
	WLFILES_SRC += src/wl/sys/wl_usermode.c
	WLFLAGS += -DUSER_MODE
endif
#endif

#ifdef WLLX
ifeq ($(WLLX),1)
	ifneq ($(WL_HIGH),1)
		WLFILES_SRC_LO += src/wl/sys/wl_linux_bmac.c
		WLFILES_SRC_LO += src/shared/bcm_rpc_char.c
	endif

        WLFILES_SRC_HI += src/wl/sys/wl_linux.c
#ifdef WLUMK
	ifeq ($(WLUMK),1)
		WLFILES_SRC_HI += src/wl/sys/wl_cdev.c
		WLFLAGS += -DWL_UMK
	endif
#endif
endif
#endif

#ifdef WLLXIW
ifeq ($(WLLXIW),1)
	WLFILES_SRC_HI += src/wl/sys/wl_iw.c
	WLFLAGS += -DWLLXIW
endif
#endif

#ifdef WLCFE
ifeq ($(WLCFE),1)
	WLFILES_SRC += src/wl/sys/wl_cfe.c
endif
#endif

#ifdef WLRTE
ifeq ($(WLRTE),1)
	WLFILES_SRC += src/wl/sys/wl_rte.c
	ifneq ($(WL_HIGH),1)
		WLFILES_SRC_LO += src/shared/bcm_rpc_tp_rte.c
	endif
endif
#endif

ifeq ($(BCMECICOEX),1)
	WLFLAGS += -DBCMECICOEX
endif

ifeq ($(CCA_STATS),1)
	WLFLAGS += -DCCA_STATS
	ifeq ($(ISID_STATS),1)
		WLFLAGS += -DISID_STATS
	endif
endif

ifeq ($(DNGL_WD_KEEP_ALIVE),1)
	WLFLAGS += -DDNGL_WD_KEEP_ALIVE
endif


#ifdef NDIS
# anything Windows/NDIS specific for 2k/xp/vista/windows7
ifeq ($(WLNDIS),1)
	WLFILES_SRC += src/wl/sys/wl_ndis.c

	ifeq ($(WLNDIS_DHD),)
		WLFILES_SRC += src/wl/sys/nhd_ndis.c
		ifeq ($(WL_SPLIT),1)
			ifdef USEDDK
				WLFLAGS += -DMEMORY_TAG="'7034'"
			else
				WLFLAGS += -DMEMORY_TAG='DWMB'
			endif
		else
			ifdef USEDDK
				WLFLAGS += -DMEMORY_TAG="'7034'"
			else
				WLFLAGS += -DMEMORY_TAG='NWMB'
			endif
		endif
	else
		WLFILES_SRC += src/dhd/sys/dhd_ndis.c
		ifdef USEDDK
			WLFLAGS += -DMEMORY_TAG="'7034'"
		else
			WLFLAGS += -DMEMORY_TAG='DWMB'
		endif
	endif

	WLFILES_SRC += src/wl/sys/wl_ndconfig.c
	WLFILES_SRC += src/shared/bcmwifi.c
	WLFILES_SRC += src/shared/bcmstdlib.c

	# support host supplied nvram variables
	ifeq ($(WLTEST),1)
		ifeq ($(WLHOSTVARS), 1)
			WLFLAGS += -DBCMHOSTVARS
		endif
	else
		ifeq ($(BCMEXTNVM),1)
			ifeq ($(WLHOSTVARS), 1)
				WLFLAGS += -DBCMHOSTVARS
			endif
		endif
	endif

	ifneq ($(WLVISTA)$(WLWIN7),)
		WLFLAGS += -DEXT_STA
		WLFLAGS += -DWL_MONITOR
	        WLFLAGS += -DIBSS_PEER_GROUP_KEY
	        WLFLAGS += -DIBSS_PEER_DISCOVERY_EVENT
	        WLFLAGS += -DIBSS_PEER_MGMT
	endif

	ifeq ($(WLWIN7),1)
		WLFLAGS += -DAP
	endif

	ifeq ($(WLXP),1)
		WLFLAGS += -DWLNOEIND
	endif

	ifeq ($(WLVIF),1)
		WLFLAGS += -DWLVIF
	endif

	# HIGH driver for BMAC ?? any ndis/xp/vista ?
	ifeq ($(WL_SPLIT),1)
	endif

	# DHD host: ?? to clean up and to support all other DHD OSes
	ifeq ($(WLNDIS_DHD),1)
		WLFLAGS += -DSHOW_EVENTS -DBCMPERFSTATS
		WLFLAGS += -DBDC -DBCMDONGLEHOST	
		WLFLAGS += -DBCM4325 

		ifneq ($(BCMSDIO),1)
			WLFLAGS += -DBCMDHDUSB
			WLFLAGS += -DBCM4328 -DBCM4322
		endif

		WLFILES_SRC += src/shared/bcmevent.c
		WLFILES_SRC += src/dhd/sys/dhd_cdc.c
		WLFILES_SRC += src/dhd/sys/dhd_common.c

		BCMPCI = 0

		ifneq ($(BCMDBUS),1)
			WLFILES_SRC += src/dhd/sys/dhd_usb_ndis.c
		endif

		ifneq ($(WLVISTA)$(WLWIN7),)
			WLFILES_SRC += src/wl/sys/wlc_rate.c
		endif

		ifeq ($(WLWIN7),1)
			WLFILES_SRC += src/wl/sys/wlc_ap.c
			WLFILES_SRC += src/wl/sys/wlc_apps.c
		endif

		ifeq ($(WLXP),1)
			WLFLAGS += -DNDIS_DMAWAR 
			# move these non-wl flag to makefiles
			WLFLAGS += -DBINARY_COMPATIBLE -DWIN32_LEAN_AND_MEAN=1 
		endif

#ifdef WLBTAMP
		ifeq ($(WLBTAMP),1)
			WLFLAGS += -DWLBTAMP
			WLFILES_SRC += src/dhd/sys/dhd_bta.c
#ifdef WLBTWUSB
			ifeq ($(WLBTWUSB),1)
				WLFLAGS += -DWLBTWUSB
				WLFILES_SRC += src/wl/sys/bt_int.c
			endif
#endif /* WLBTWUSB */
		endif
#endif /* WLBTAMP */
	endif
endif
#endif

ifeq ($(ADV_PS_POLL),1)
	WLFLAGS += -DADV_PS_POLL
endif

#ifdef DHDOID
ifeq ($(DHDOID),1)
	WLFLAGS += -DWLC_HOSTOID
	WLFILES_SRC += src/wl/sys/wlc_hostoid.c
endif
#endif


#ifdef BINOSL
ifeq ($(BINOSL),1)
	WLFLAGS += -DBINOSL
endif
#endif

## wl features
# NCONF -- 0 is remove from code, else bit mask of supported nphy revs
ifneq ($(NCONF),)
	WLFLAGS += -DNCONF=$(NCONF)
endif

# HTCONF -- 0 is remove from code, else bit mask of supported htphy revs
ifneq ($(HTCONF),)
	WLFLAGS += -DHTCONF=$(HTCONF)
endif

# ACONF -- 0 is remove from code, else bit mask of supported aphy revs
ifneq ($(ACONF),)
	WLFLAGS += -DACONF=$(ACONF)
endif

# GCONF -- 0 is remove from code, else bit mask of supported gphy revs
ifneq ($(GCONF),)
	WLFLAGS += -DGCONF=$(GCONF)
endif

# LPCONF -- 0 is remove from code, else bit mask of supported lpphy revs
ifneq ($(LPCONF),)
	WLFLAGS += -DLPCONF=$(LPCONF)
endif

# SSLPNCONF -- 0 is remove from code, else bit mask of supported sslpnphy revs
ifneq ($(SSLPNCONF),)
	WLFLAGS += -DSSLPNCONF=$(SSLPNCONF)
endif

# LCNCONF -- 0 is remove from code, else bit mask of supported lcnphy revs
ifneq ($(LCNCONF),)
	WLFLAGS += -DLCNCONF=$(LCNCONF)
endif

#ifdef AP
# ap
ifeq ($(AP),1)
	WLFILES_SRC_HI += src/wl/sys/wlc_ap.c
	WLFILES_SRC_HI += src/wl/sys/wlc_apps.c
	WLFLAGS += -DAP

	ifeq ($(MBSS),1)
		WLFLAGS += -DMBSS
	endif

	ifeq ($(WDS),1)
		WLFLAGS += -DWDS
	endif

	# Channel Select
	ifeq ($(APCS),1)
		WLFILES_SRC_HI += src/wl/sys/wlc_apcs.c
		WLFLAGS += -DAPCS
	endif

	# WME_PER_AC_TX_PARAMS
	ifeq ($(WME_PER_AC_TX_PARAMS),1)
		WLFLAGS += -DWME_PER_AC_TX_PARAMS
	endif

	# WME_PER_AC_TUNING
	ifeq ($(WME_PER_AC_TUNING),1)
		WLFLAGS += -DWME_PER_AC_TUNING
	endif

endif
#endif

#ifdef STA
# sta
ifeq ($(STA),1)
	WLFLAGS += -DSTA
endif
#endif

#ifdef APSTA
# apsta
ifeq ($(APSTA),1)
	WLFLAGS += -DAPSTA
endif
# apsta
#endif

#ifdef WET
# wet
ifeq ($(WET),1)
	WLFLAGS += -DWET
	WLFILES_SRC_HI += src/wl/sys/wlc_wet.c
endif
#endif

#ifdef RXCHAIN_PWRSAVE
ifeq ($(RXCHAIN_PWRSAVE), 1)
	WLFLAGS += -DRXCHAIN_PWRSAVE
endif
#endif

#ifdef RADIO_PWRSAVE
ifeq ($(RADIO_PWRSAVE), 1)
	WLFLAGS += -DRADIO_PWRSAVE
endif
#endif

#ifdef WMF
ifeq ($(WMF), 1)
	WLFILES_SRC_HI += src/wl/sys/wlc_wmf.c
	WLFLAGS += -DWMF
endif
#endif

#ifdef MCAST_REGEN
ifeq ($(MCAST_REGEN), 1)
	WLFLAGS += -DMCAST_REGEN
endif
#endif

#ifdef  ROUTER_COMA
ifeq ($(ROUTER_COMA), 1)
	WLFILES_SRC_HI += src/shared/hndmips.c
	WLFILES_SRC_HI += src/shared/hndchipc.c
	WLFLAGS += -DROUTER_COMA
endif
#endif


#ifdef WLOVERTHRUSTER
ifeq ($(WLOVERTHRUSTER), 1)
	WLFLAGS += -DWLOVERTHRUSTER
endif
#endif

#ifdef MAC_SPOOF
# mac spoof
ifeq ($(MAC_SPOOF),1)
	WLFLAGS += -DMAC_SPOOF
endif
#endif

#ifndef LINUX_HYBRID
# Router IBSS Security Support
ifeq ($(ROUTER_SECURE_IBSS),1)
         WLFLAGS += -DIBSS_PEER_GROUP_KEY
         WLFLAGS += -DIBSS_PSK
         WLFLAGS += -DIBSS_PEER_MGMT
         WLFLAGS += -DIBSS_PEER_DISCOVERY_EVENT
endif
#endif

#ifdef WLTIMER
# timer
ifeq ($(WLTIMER),1)
	WLFLAGS += -DWLTIMER
endif
#endif

#ifdef WLLED
# led
ifeq ($(WLLED),1)
	WLFLAGS += -DWLLED
	WLFILES_SRC_HI += src/wl/sys/wlc_led.c
endif
#endif

#ifdef WL_MONITOR
# MONITOR
ifeq ($(WL_MONITOR),1)
	WLFLAGS += -DWL_MONITOR
endif
#endif

#ifdef WL_PROMISC
# PROMISC
ifeq ($(PROMISC),1)
	WLFLAGS += -DWL_PROMISC
endif
#endif

ifeq ($(WL_ALL_PASSIVE),1)
	WLFLAGS += -DWL_ALL_PASSIVE
endif

#ifdef ND_ALL_PASSIVE
ifeq ($(ND_ALL_PASSIVE),1)
	WLFLAGS += -DND_ALL_PASSIVE
endif
#endif

#ifdef WME
# WME
ifeq ($(WME),1)
	WLFLAGS += -DWME
	ifeq ($(WLCAC), 1)
		ifeq ($(WL), 1)
			WLFLAGS += -DWLCAC
			WLFILES_SRC_HI += src/wl/sys/wlc_cac.c
		endif
	endif
endif
#endif

#ifdef WLBA
# WLBA
ifeq ($(WLBA),1)
	WLFLAGS += -DWLBA
	WLFILES_SRC_HI += src/wl/sys/wlc_ba.c
endif
#endif

#ifdef WLPIO
# WLPIO 
ifeq ($(WLPIO),1)
	WLFLAGS += -DWLPIO
	WLFILES_SRC_LO += src/wl/sys/wlc_pio.c
endif
#endif

#ifdef WLAFTERBURNER
# WLAFTERBURNER
ifeq ($(WLAFTERBURNER),1)
	WLFLAGS += -DWLAFTERBURNER
endif
#endif

#ifdef CRAM
# CRAM
ifeq ($(CRAM),1)
	WLFLAGS += -DCRAM
	WLFILES_SRC_HI += src/wl/sys/wlc_cram.c
endif
#endif

#ifdef WL11N
# 11N
ifeq ($(WL11N),1)
	WLFLAGS += -DWL11N
endif
#endif

#ifdef WL11N_20MHZONLY
# 11N 20MHz only optimization
ifeq ($(WL11N_20MHZONLY),1)
	WLFLAGS += -DWL11N_20MHZONLY
endif
#endif

#ifdef WL11N_SINGLESTREAM
# 11N single stream optimization
ifeq ($(WL11N_SINGLESTREAM),1)
	WLFLAGS += -DWL11N_SINGLESTREAM
endif
#endif

#ifdef WL11H
# 11H
ifeq ($(WL11H),1)
	WLFLAGS += -DWL11H
endif
#endif

#ifdef WL11D
# 11D
ifeq ($(WL11D),1)
	WLFLAGS += -DWL11D
endif
#endif

#ifdef WL11U
# 11U
ifeq ($(WL11U),1)
	WLFLAGS += -DWL11U
endif
#endif

#ifdef WLPROBRESP_SW
# WLPROBRESP_SW
ifeq ($(WLPROBRESP_SW),1)
	WLFLAGS += -DWLPROBRESP_SW
endif
#endif

#ifdef DBAND
# DBAND
ifeq ($(DBAND),1)
	WLFLAGS += -DDBAND
endif
#endif

#ifdef WLRM
# WLRM
ifeq ($(WLRM),1)
	WLFLAGS += -DWLRM
endif
#endif

#ifdef WLCQ
# WLCQ
ifeq ($(WLCQ),1)
	WLFLAGS += -DWLCQ
endif
#endif

#ifdef WLCNT
# WLCNT
ifeq ($(WLCNT),1)
	WLFLAGS += -DWLCNT
endif
#endif

#ifdef WLCHANIM
# WLCHANIM
ifeq ($(WLCHANIM),1)
	WLFLAGS += -DWLCHANIM
endif
#endif

# WL_AP_TPC
#ifdef WL_AP_TPC
ifeq ($(WL_AP_TPC),1)
         WLFLAGS += -DWL_AP_TPC
endif
#endif

ifndef DELTASTATS
	ifeq ($(WLCNT),1)
		DELTASTATS := 1
	endif
endif

ifndef WLMULTIQUEUE
	ifeq ($(WLMCHAN),1)
		WLMULTIQUEUE := 1
	endif
endif

# DELTASTATS
ifeq ($(DELTASTATS),1)
	WLFLAGS += -DDELTASTATS
endif

#ifdef WLCNTSCB
# WLCNTSCB
ifeq ($(WLCNTSCB),1)
	WLFLAGS += -DWLCNTSCB
endif
#endif


#ifdef WLCOEX
# WLCOEX
ifeq ($(WLCOEX),1)
	WLFLAGS += -DWLCOEX
endif
#endif

## wl security
# external linux supplicant
#ifdef LINUX_CRYPTO
ifeq ($(LINUX_CRYPTO), 1)
	WLFLAGS += -DLINUX_CRYPTO
endif
#endif

ifeq ($(WLFBT),1)
	WLFLAGS += -DWLFBT
endif

ifeq ($(WLWNM),1)
	WLFLAGS += -DWLWNM
endif

#ifdef BCMSUP_PSK
# in-driver supplicant
ifeq ($(BCMSUP_PSK),1)
	WLFLAGS += -DBCMSUP_PSK -DBCMINTSUP
	WLFILES_SRC_HI += src/wl/sys/wlc_sup.c
	WLFILES_SRC_HI += src/bcmcrypto/aes.c
	WLFILES_SRC_HI += src/bcmcrypto/aeskeywrap.c
	WLFILES_SRC_HI += src/bcmcrypto/hmac.c
	WLFILES_SRC_HI += src/bcmcrypto/prf.c
	WLFILES_SRC_HI += src/bcmcrypto/sha1.c
	ifeq ($(WLFBT),1)
	WLFILES_SRC_HI += src/bcmcrypto/hmac_sha256.c
	WLFILES_SRC_HI += src/bcmcrypto/sha256.c
	endif
	# NetBSD 2.0 has MD5 and AES built in
	ifneq ($(OSLBSD),1)
		WLFILES_SRC_HI += src/bcmcrypto/md5.c
		WLFILES_SRC_HI += src/bcmcrypto/rijndael-alg-fst.c
	endif
	WLFILES_SRC_HI += src/bcmcrypto/passhash.c
endif
#endif


#ifdef BCMAUTH_PSK
# in-driver authenticator
ifeq ($(BCMAUTH_PSK),1)
	WLFLAGS += -DBCMAUTH_PSK
	WLFILES_SRC_HI += src/wl/sys/wlc_auth.c
endif
#endif

#ifdef BCMCCMP
# Soft AES CCMP
ifeq ($(BCMCCMP),1)
	WLFLAGS += -DBCMCCMP
	WLFILES_SRC_HI += src/bcmcrypto/aes.c
	# BSD has AES built in
	ifneq ($(BSD),1)
		WLFILES_SRC_HI +=src/bcmcrypto/rijndael-alg-fst.c
	endif
endif
#endif

#ifdef MFP
# Management Frame Protection
ifeq ($(MFP),1)
	WLFLAGS += -DMFP
	WLFILES_SRC_HI += src/bcmcrypto/aes.c
	WLFILES_SRC_HI += src/bcmcrypto/sha256.c
	WLFILES_SRC_HI += src/bcmcrypto/hmac_sha256.c
	# BSD has AES built in
	ifneq ($(BSD),1)
		WLFILES_SRC_HI +=src/bcmcrypto/rijndael-alg-fst.c
	endif
endif
#endif


#ifdef BCMWAPI_WPI
ifeq ($(BCMWAPI_WPI),1)
	WLFILES_SRC_HI += src/bcmcrypto/sms4.c
	WLFLAGS += -DBCMWAPI_WPI
	ifeq ($(BCMSMS4_TEST),1)
		WLFLAGS += -DBCMSMS4_TEST
	endif
endif
#endif

#ifdef WIFI_ACT_FRAME
# WIFI_ACT_FRAME 
ifeq ($(WIFI_ACT_FRAME),1)
	WLFLAGS += -DWIFI_ACT_FRAME 
endif
#endif

# BCMDMA32
ifeq ($(BCMDMA32),1)
	WLFLAGS += -DBCMDMA32
endif

ifeq ($(BCMDMA64OSL),1)
	WLFLAGS += -DBCMDMA64OSL
endif

ifeq ($(BCMDMASGLISTOSL),1)
	WLFLAGS += -DBCMDMASGLISTOSL
endif

# Early DMA TX Free for LOW driver
ifeq ($(WL_DMA_TX_FREE),1)
	ifneq ($(WL_HIGH),1)
		ifeq ($(PT_GIANT),1)
			WLFLAGS += -DDMA_TX_FREE
		endif
	endif
endif

## wl over jtag
#ifdef BCMJTAG
ifeq ($(BCMJTAG),1)
	WLFLAGS += -DBCMJTAG -DBCMSLTGT
	WLFILES_SRC += src/shared/bcmjtag.c
	WLFILES_SRC += src/shared/bcmjtag_linux.c
	WLFILES_SRC += src/shared/ejtag.c
	WLFILES_SRC += src/shared/jtagm.c
endif
#endif

#ifdef WLAMSDU
ifeq ($(WLAMSDU),1)
	WLFLAGS += -DWLAMSDU
	WLFILES_SRC_HI += src/wl/sys/wlc_amsdu.c
endif
#endif

#ifdef WLAMSDU_SWDEAGG
ifeq ($(WLAMSDU_SWDEAGG),1)
	WLFLAGS += -DWLAMSDU_SWDEAGG
endif
#endif

#ifdef WLAMPDU
ifeq ($(WLAMPDU),1)
	WLFLAGS += -DWLAMPDU
	WLFILES_SRC_HI += src/wl/sys/wlc_ampdu.c
	ifeq ($(WLAMPDU_UCODE),1)
		WLFLAGS += -DWLAMPDU_UCODE -DWLAMPDU_MAC
	endif
	ifeq ($(WLAMPDU_HW),1)
		WLFLAGS += -DWLAMPDU_HW -DWLAMPDU_MAC
	endif
	ifeq ($(WLAMPDU_PRECEDENCE),1)
		WLFLAGS += -DWLAMPDU_PRECEDENCE
	endif
endif
#endif

#ifdef WOWL
ifeq ($(WOWL),1)
	WLFLAGS += -DWOWL
	WLFILES_SRC_HI += src/wl/sys/d11ucode_wowl.c
	WLFILES_SRC_HI += src/wl/sys/wlc_wowl.c
	WLFILES_SRC_HI += src/wl/sys/wowlaestbls.c
endif
#endif

#ifdef BTC2WIRE
ifeq ($(BTC2WIRE),1)
	WLFLAGS += -DBTC2WIRE
	WLFILES_SRC_LO += src/wl/sys/d11ucode_2w.c
endif
#endif

#ifdef WL_ASSOC_RECREATE
ifeq ($(WL_ASSOC_RECREATE),1)
	ifeq ($(STA),1)
		WLFLAGS += -DWL_ASSOC_RECREATE
	endif
endif
#endif


#ifdef WLP2P
ifeq ($(WLP2P),1)
	WLFLAGS += -DWLP2P -DWL_BSSCFG_TX_SUPR -DWIFI_ACT_FRAME
	WLFILES_SRC_HI += src/wl/sys/wlc_p2p.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_p2p.c
	ifeq ($(WLMCHAN), 1)
		WLFLAGS += -DWLMCHAN
		WLFILES_SRC_HI += src/wl/sys/wlc_mchan.c
	endif
	ifeq ($(WLMULTIQUEUE), 1)
		WLFLAGS += -DWL_MULTIQUEUE
	endif
endif
#endif

#ifdef WLRWL
ifeq ($(WLRWL),1)
	WLFLAGS += -DWLRWL
	WLFILES_SRC_HI += src/wl/sys/wlc_rwl.c
endif
#endif

ifneq ($(WLNDIS_DHD),1)
#ifdef WLBTAMP
	ifeq ($(AP)$(STA),11)
	ifeq ($(WLBTAMP),1)
		WLFLAGS += -DWLBTAMP
		WLFILES_SRC_HI += src/wl/sys/wlc_bta.c
#ifdef WLBTWUSB
		ifeq ($(WLBTWUSB),1)
			WLFLAGS += -DWLBTWUSB
			WLFILES_SRC_HI += src/wl/sys/bt_int.c
		endif
#endif /* WLBTWUSB */
	endif
	endif
#endif /* WLBTAMP */
endif

#ifdef WLPLT
ifeq ($(WLPLT),1)
	WLFLAGS += -DWLPLT
	WLFILES_SRC_HI += src/wl/sys/wlc_plt.c
endif
#endif


#ifdef WLNINTENDO2
ifeq ($(WLNINTENDO2),1)
	WLFLAGS += -DWLNINTENDO2
endif
#endif

#ifdef WLMEDIA
ifeq ($(WLMEDIA),1)
	WLFLAGS += -DWLMEDIA_EN
	WLFLAGS += -DWLMEDIA_RATESTATS
	WLFLAGS += -DWLMEDIA_MULTIQUEUE
	WLFLAGS += -DWLMEDIA_TXFIFO_FLUSH_SCB
	WLFLAGS += -DWLMEDIA_AMPDUSTATS
	WLFLAGS += -DWLMEDIA_TXFAILEVENT
	WLFLAGS += -DWLMEDIA_LQSTATS
	WLFLAGS += -DWLMEDIA_CALDBG
	WLFLAGS += -DWLMEDIA_EXT
	WLFLAGS += -DWLMEDIA_TXFILTER_OVERRIDE
	WLFLAGS += -DWLMEDIA_TSF
	WLFLAGS += -DWLMEDIA_PEAKRATE
endif
#endif

#ifdef WLPKTDLYSTAT
ifeq ($(WLPKTDLYSTAT),1)
	WLFLAGS += -DWLPKTDLYSTAT
endif
#endif

## --- which buses

# silicon backplane

#ifdef BCMSIBUS
ifeq ($(BCMSIBUS),1)
	WLFLAGS += -DBCMBUSTYPE=SI_BUS
	BCMPCI = 0
endif
#endif

ifeq ($(SOCI_SB),1)
	WLFLAGS += -DBCMCHIPTYPE=SOCI_SB
else
	ifeq ($(SOCI_AI),1)
		WLFLAGS += -DBCMCHIPTYPE=SOCI_AI
	endif
endif



#ifndef LINUX_HYBRID
# AP/ROUTER with SDSTD
ifeq ($(WLAPSDSTD),1)
	WLFILES_SRC += src/shared/nvramstubs.c
	WLFILES_SRC += src/shared/bcmsrom.c
endif
#endif

## --- basic shared files

#ifdef BCMHIGHSDIO
ifeq ($(BCMHIGHSDIO),1)
	WLFLAGS += -DBCMHIGHSDIO
endif
#endif

#ifdef BCMLOSDIO
ifeq ($(BCMLOSDIO),1)
	WLFLAGS += -DBCMLOSDIO
endif
#endif

#ifdef BCMHIGHUSB
ifeq ($(BCMHIGHUSB),1)
	WLFLAGS += -DBCMHIGHUSB
endif
#endif

#ifdef BCMLOUSB
ifeq ($(BCMLOUSB),1)
	WLFLAGS += -DBCMLOUSB
endif
#endif

#ifdef HNDDMA
ifeq ($(HNDDMA),1)
	WLFILES_SRC_LO += src/shared/hnddma.c
endif
#endif

#ifdef MSGTRACE
ifeq ($(MSGTRACE),1)
	WLFILES_SRC += src/shared/msgtrace.c
	WLFLAGS += -DMSGTRACE
endif
#endif

#ifdef BCMUTILS
ifeq ($(BCMUTILS),1)
	WLFILES_SRC += src/shared/bcmutils.c
endif
#endif


#ifdef BCMSROM
ifeq ($(BCMSROM),1)
	ifeq ($(BCMSDIO),1)
		ifeq ($(WL_HIGH),1)
			WLFILES_SRC_HI += src/shared/bcmsrom.c
			WLFILES_SRC_HI += src/shared/bcmotp.c
		endif
	endif
	WLFILES_SRC_LO += src/shared/bcmsrom.c
	WLFILES_SRC_LO += src/shared/bcmotp.c
endif
#endif

#ifdef BCMOTP
ifeq ($(BCMOTP),1)
	WLFILES_SRC_LO += src/shared/bcmotp.c
	WLFLAGS += -DBCMNVRAMR
endif
#endif

#ifdef SIUTILS
ifeq ($(SIUTILS),1)
	WLFILES_SRC_LO += src/shared/siutils.c
	WLFILES_SRC_LO += src/shared/sbutils.c
	WLFILES_SRC_LO += src/shared/aiutils.c
	WLFILES_SRC_LO += src/shared/hndpmu.c
	ifneq ($(BCMPCI), 0)
		WLFILES_SRC_LO += src/shared/nicpci.c
	endif
endif
#endif /* SIUTILS */

#ifdef SBMIPS
ifeq ($(SBMIPS),1)
	WLFLAGS += -DBCMMIPS
	WLFILES_SRC_LO += src/shared/hndmips.c
	WLFILES_SRC_LO += src/shared/hndchipc.c
endif
#endif

#ifdef SBPCI
ifeq ($(SBPCI),1)
	WLFILES_SRC_LO += src/shared/hndpci.c
endif
#endif

#ifdef SFLASH
ifeq ($(SFLASH),1)
	WLFILES_SRC_LO += src/shared/sflash.c
endif
#endif

#ifdef FLASHUTL
ifeq ($(FLASHUTL),1)
	WLFILES_SRC_LO += src/shared/flashutl.c
endif
#endif

## --- shared OSL
#ifdef OSLUM
# linux user mode
ifeq ($(OSLUM),1)
	WLFILES_SRC += src/shared/usermode_osl.c
	WLFLAGS += -DUSER_MODE
endif
#endif

#ifdef OSLLX
# linux osl
ifeq ($(OSLLX),1)
	WLFILES_SRC += src/shared/linux_osl.c
endif
#endif

#ifdef OSLVX
# vx osl
ifeq ($(OSLVX),1)
	WLFILES_SRC += src/shared/vx_osl.c
	WLFILES_SRC += src/shared/bcmallocache.c
endif
#endif

#ifdef OSLBSD
# bsd osl
ifeq ($(OSLBSD),1)
	WLFILES_SRC += src/shared/bsd_osl.c
	WLFILES_SRC += src/shared/nvramstubs.c
endif
#endif

#ifdef OSLCFE
ifeq ($(OSLCFE),1)
	WLFILES_SRC += src/shared/cfe_osl.c
endif
#endif

#ifdef OSLRTE
ifeq ($(OSLRTE),1)
	WLFILES_SRC += src/shared/hndrte_osl.c
endif
#endif

#ifdef OSLNDIS
ifeq ($(OSLNDIS),1)
	WLFILES_SRC += src/shared/ndshared.c
	WLFILES_SRC += src/shared/ndis_osl.c
endif
#endif

#ifndef LINUX_HYBRID
ifeq ($(CONFIG_USBRNDIS_RETAIL),1)
	WLFLAGS += -DCONFIG_USBRNDIS_RETAIL
	WLFILES_SRC += src/wl/sys/wl_ndconfig.c
	WLFILES_SRC += src/shared/bcmwifi.c
endif

ifeq ($(NVRAM),1)
	WLFILES_SRC_LO += src/dongle/rte/test/nvram.c
	WLFILES_SRC_LO += src/dongle/rte/sim/nvram.c
	WLFILES_SRC_LO += src/shared/nvram/nvram.c
endif

ifeq ($(NVRAMVX),1)
	WLFILES_SRC_LO += src/shared/nvram/nvram_rw.c
endif
#endif /* LINUX_HYBRID */

#ifdef BCMNVRAMR
ifeq ($(BCMNVRAMR),1)
	WLFILES_SRC_LO += src/shared/nvram/nvram_ro.c
	WLFILES_SRC_LO += src/shared/sflash.c
	WLFILES_SRC_LO += src/shared/bcmotp.c
	WLFLAGS += -DBCMNVRAMR
endif
#else /* !BCMNVRAMR */
ifneq ($(BCMNVRAMR),1)
	ifeq ($(WLLXNOMIPSEL),1)
		ifneq ($(WLUMK),1)
			WLFILES_SRC += src/shared/nvramstubs.c
		endif
	else
		ifeq ($(WLNDIS),1)
			WLFILES_SRC += src/shared/nvramstubs.c
		else
			ifeq ($(BCMNVRAMW),1)
				WLFILES_SRC_LO += src/shared/nvram/nvram_ro.c
				WLFILES_SRC_LO += src/shared/sflash.c
			endif
		endif
	endif
	ifeq ($(BCMNVRAMW),1)
		WLFILES_SRC_LO += src/shared/bcmotp.c
		WLFLAGS += -DBCMNVRAMW
	endif
endif
#endif /* !BCMNVRAMR */

# Define one OTP mechanism, or none to support all dynamically
ifeq ($(BCMHNDOTP),1)
	WLFLAGS += -DBCMHNDOTP
endif
ifeq ($(BCMIPXOTP),1)
	WLFLAGS += -DBCMIPXOTP
endif


#ifdef WLDIAG
ifeq ($(WLDIAG),1)
	WLFLAGS += -DWLDIAG
	WLFILES_SRC_LO += src/wl/sys/wlc_diag.c
endif
#endif

#ifdef BCMDBG
ifneq ($(BCMDBG),1)
	ifeq ($(WLTINYDUMP),1)
		WLFLAGS += -DWLTINYDUMP
	endif
endif
#endif

#ifdef BCMQT
ifeq ($(BCMQT),1)
	# Set flag to indicate emulated chip
	WLFLAGS += -DBCMSLTGT -DBCMQT
	ifeq ($(WLRTE),1)
		# Use of RTE implies embedded (CPU emulated)
		WLFLAGS += -DBCMQT_CPU
	endif
endif
#endif

#ifdef WLPFN
ifeq ($(WLPFN),1)
	WLFLAGS += -DWLPFN
	WLFILES_SRC += src/wl/sys/wl_pfn.c
	ifeq ($(WLPFN_AUTO_CONNECT),1)
		WLFLAGS += -DWLPFN_AUTO_CONNECT
	endif
endif
#endif

#ifdef TOE
ifeq ($(TOE),1)
	WLFLAGS += -DTOE
	WLFILES_SRC += src/wl/sys/wl_toe.c
endif
#endif

#ifdef ARPOE
ifeq ($(ARPOE),1)
	WLFLAGS += -DARPOE
	WLFILES_SRC += src/wl/sys/wl_arpoe.c
endif
#endif

#ifdef PLC
ifeq ($(PLC),1)
	WLFLAGS += -DPLC
	WLFILES_SRC += src/wl/sys/wl_plc_linux.c
endif
#endif

#ifdef PCOEM_LINUXSTA
ifeq ($(PCOEM_LINUXSTA),1)
	WLFLAGS += -DPCOEM_LINUXSTA
endif
#endif

#ifdef LINUXSTA_PS
ifeq ($(LINUXSTA_PS),1)
	WLFLAGS += -DLINUXSTA_PS
endif
#endif

#ifndef LINUX_HYBRID
ifeq ($(KEEP_ALIVE),1)
	WLFLAGS += -DKEEP_ALIVE
	WLFILES_SRC += src/wl/sys/wl_keep_alive.c
endif

#ifdef OPENSRC_IOV_IOCTL
ifeq ($(OPENSRC_IOV_IOCTL),1)
	WLFLAGS += -DOPENSRC_IOV_IOCTL
endif
#endif

ifeq ($(PACKET_FILTER),1)
	WLFLAGS += -DPACKET_FILTER
	WLFILES_SRC += src/wl/sys/wlc_pkt_filter.c
endif

ifeq ($(SEQ_CMDS),1)
	WLFLAGS += -DSEQ_CMDS
	WLFILES_SRC_HI += src/wl/sys/wlc_seq_cmds.c
endif

ifeq ($(RECEIVE_THROTTLE),1)
	WLFLAGS += -DWL_PM2_RCV_DUR_LIMIT
endif

ifeq ($(ASYNC_TSTAMPED_LOGS),1)
	WLFLAGS += -DBCMTSTAMPEDLOGS
endif

ifeq ($(WL11K),1)
	WLFLAGS += -DWL11K
	WLFILES_SRC += src/wl/sys/wlc_rrm.c
endif
ifeq ($(WLWNM),1)
	WLFLAGS += -DWLWNM
	WLFILES_SRC += src/wl/sys/wlc_wnm.c
endif
#endif

# Sort and remove duplicates from WLFILES* 
ifeq ($(WL_LOW),1)
	WLFILES_SRC += $(sort $(WLFILES_SRC_LO))
endif
ifeq ($(WL_HIGH),1)
	WLFILES_SRC += $(sort $(WLFILES_SRC_HI))
endif

# wl patch code
ifneq ($(WLPATCHFILE), )
	WLFLAGS += -DWLC_PATCH
	WLFILES_SRC += $(WLPATCHFILE)
endif

ifeq ($(SAMPLE_COLLECT),1)
	WLFLAGS += -DSAMPLE_COLLECT
endif

# add a flag to indicate the split to linux kernels
WLFLAGS += -DPHY_HAL

ifeq ($(SMF_STATS),1)
	WLFLAGS += -DSMF_STATS
endif

#ifdef PHYMON
ifeq ($(PHYMON),1)
	WLFLAGS += -DPHYMON
endif
#endif

#ifdef USBSHIM
ifeq ($(USBSHIM),1)
        WLFLAGS += -DUSBSHIM
endif # USBSHIM
#endif

#ifdef BCM_DCS
ifeq ($(BCM_DCS),1)
	WLFLAGS += -DBCM_DCS
endif
#endif

ifeq ($(WL_THREAD),1)
	WLFLAGS += -DWL_THREAD
endif

ifeq ($(USBOS_THREAD),1)
	WLFLAGS += -DUSBOS_THREAD
endif
ifeq ($(WL_NVRAM_FILE),1)
	WLFLAGS += -DWL_NVRAM_FILE
endif

ifeq ($(WL_FW_DECOMP),1)
	WLFLAGS += -DWL_FW_DECOMP
	WLFILES_SRC_HI += src/shared/zlib/adler32.c
	WLFILES_SRC_HI += src/shared/zlib/inffast.c
	WLFILES_SRC_HI += src/shared/zlib/inflate.c
	WLFILES_SRC_HI += src/shared/zlib/infcodes.c
	WLFILES_SRC_HI += src/shared/zlib/infblock.c
	WLFILES_SRC_HI += src/shared/zlib/inftrees.c
	WLFILES_SRC_HI += src/shared/zlib/infutil.c
	WLFILES_SRC_HI += src/shared/zlib/zutil.c
	WLFILES_SRC_HI += src/shared/zlib/crc32.c
endif

ifeq ($(WL_WOWL_MEDIA),1)
	WLFLAGS += -DWL_WOWL_MEDIA
endif

ifeq ($(WL_USB_ZLP_PAD),1)
	WLFLAGS += -DWL_USB_ZLP_PAD
endif

ifeq ($(WL_URB_ZPKT),1)
	WLFLAGS += -DWL_URB_ZPKT
endif

ifeq ($(WL_VMEM_NVRAM_DECOMP),1)
	WLFLAGS += -DWL_VMEM_NVRAM_DECOMP
endif


# add a flag to indicate the split to linux kernels
WLFLAGS += -DPHY_HAL

ifeq ($(MEDIA_IPTV),1)
	WLFLAGS += -DWLMEDIA_IPTV
	WLFLAGS += -DWET_TUNNEL
	WLFILES_SRC_HI += src/wl/sys/wlc_wet_tunnel.c
endif

# Legacy WLFILES pathless definition, please use new src relative path
# in make files. 
WLFILES := $(sort $(notdir $(WLFILES_SRC)))
