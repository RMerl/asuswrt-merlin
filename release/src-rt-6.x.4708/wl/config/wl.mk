# Helper makefile for building Broadcom wl device driver
# This file maps wl driver feature flags (import) to WLFLAGS and WLFILES_SRC (export).
#
# Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
# $Id: wl.mk 555412 2015-05-08 21:08:52Z $

WLFLAGS += -DBCM943217ROUTER_ACI_SCANMORECH
WLFLAGS += -DBPHY_DESENSE



# debug/internal
ifeq ($(DEBUG),1)
	WLFLAGS += -DBCMDBG -DWLTEST -DRWL_WIFI -DWIFI_ACT_FRAME -DWLRWL -DWL_EXPORT_CURPOWER
	WLRWL = 1
else ifeq ($(WLDEBUG),1)
	BCMUTILS = 1
	OSLLX = 1
	WLFLAGS += -DBCMDBG -DWLTEST -DWIFI_ACT_FRAME -DWL_EXPORT_CURPOWER
else
#ifdef WLTEST
	# mfgtest builds
	ifeq ($(WLTEST),1)
		WLFLAGS += -DWLTEST -DWL_EXPORT_CURPOWER
	endif
#endif
endif

#ifdef BCMDBG
ifeq ($(BCMDBG),1)
	WLFLAGS += -DWL_EXPORT_CURPOWER
endif
#endif

ifeq ($(HSPOT),1)
	WLBSSLOAD = 1
	L2_FILTER = 1
	WLDLS = 1
	WLWNM = 1
	WIFI_ACT_FRAME = 1
	WL11U = 1
	WLPROBRESP_SW = 1
endif

WLFLAGS += -DPPR_API

ifdef BCMPHYCORENUM
	WLFLAGS += -DBCMPHYCORENUM=$(BCMPHYCORENUM)
endif

#ifdef WL11AC
ifneq ($(WL11AC),0)
	WL11AC := 1
	WLFLAGS += -DWL11AC
        WLFLAGS += -DWL11N_STBC_RX_ENABLED
ifneq ($(WLTXBF),0)
	WLFLAGS += -DWL_BEAMFORMING
	ifeq ($(WLMEDIA_TXBF_ENB),1)
		WLFLAGS += -DWLMEDIA_TXBF_ENB
	endif
endif

endif
#endif

ifeq ($(WLATF),1)
	WLATF := 1
	WLFLAGS += -DWLATF
endif




#ifdef PKTQ_LOG
ifeq ($(PKTQ_LOG),1)
	WLFLAGS += -DPKTQ_LOG
	ifeq ($(SCB_BS_DATA),1)
		WLFILES_SRC_HI += src/wl/sys/wlc_bs_data.c
		WLFLAGS += -DSCB_BS_DATA
	endif
endif
#endif

ifeq ($(PSPRETEND),1)
	WLFLAGS += -DPSPRETEND
	WLFLAGS += -DWL_CS_PKTRETRY -DWL_CS_RESTRICT_RELEASE
endif

#ifdef BCMDBG_TRAP
# CPU debug traps (lomem access, divide by 0, etc) are enabled except when mogrified out for
# external releases.
WLFLAGS += -DBCMDBG_TRAP
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
		ifeq ($(BCMUSBDEV_COMPOSITE),1)
			WLFLAGS += -DBCMUSBDEV_COMPOSITE
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

ifeq ($(USBAP),1)
	WLFLAGS += -DUSBAP
	WLFLAGS += -DEHCI_FASTPATH_TX -DEHCI_FASTPATH_RX
endif

# split driver infrastructure files
ifeq ($(WL_SPLIT),1)
	WLFILES_SRC += src/shared/bcm_xdr.c
	WLFILES_SRC += src/shared/bcm_rpc.c
	ifneq ($(USBAP),1)
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

	WLFILES_SRC_HI += src/wl/ppr/src/wlc_ppr.c

	ifeq ($(WL_HIGH),1)
		ifeq ($(BCMSDIO),1)
			ifneq ($(WLVISTA)$(WLWIN7)$(WLWIN8),)
				WLFILES_SRC_HI += src/shared/sdh_ndis.c
			endif
		else
			WLFLAGS += -DBCMBUSTYPE=RPC_BUS
			WLFLAGS += -DOSLREGOPS
			ifeq ($(WLNDIS),1)
				WLFLAGS += -DBCMUSB
			endif
		endif
	endif
	WLFLAGS += -DPREATTACH_NORECLAIM
endif

#ifdef WL
ifeq ($(WL),1)
	WLFILES_SRC += src/shared/bcmwifi/src/bcmwifi_channels.c
	WLFILES_SRC += src/shared/bcmevent.c
	WLFILES_SRC += src/shared/bcm_mpool.c
	WLFILES_SRC += src/shared/bcm_notif.c
	WLFILES_SRC += src/wl/sys/wlc_alloc.c
	WLFILES_SRC += src/wl/sys/wlc_intr.c
	WLFILES_SRC += src/wl/sys/wlc_hw.c

#enable offloads flag to use the functionality
	ifeq ($(WLRXOE),1)
		WLFLAGS += -DWLRXOE
		WLFILES_SRC_LO += src/shared/bcm_ol_msg.c
		WLFILES_SRC_LO += src/bcmcrypto/aeskeywrap.c
		WLFILES_SRC_LO += src/bcmcrypto/rijndael-alg-fst.c
		WLFILES_SRC_LO += src/bcmcrypto/md5.c
		WLFILES_SRC_LO += src/bcmcrypto/hmac.c
		WLFILES_SRC_LO += src/bcmcrypto/prf.c
		WLFILES_SRC_LO += src/bcmcrypto/aes.c
		WLFILES_SRC_LO += src/bcmcrypto/sha1.c
		WLFILES_SRC_LO += src/shared/bcmwpa.c
		WLFILES_SRC_LO += src/wl/sys/wlc_dngl_ol.c
		WLFILES_SRC_LO += src/wl/sys/wlc_bcnol.c
		WLFILES_SRC_LO += src/wl/sys/wlc_rssiol.c
		WLFILES_SRC_LO += src/wl/sys/wlc_eventlog_ol.c
		WLFILES_SRC_LO += src/bcmcrypto/tkhash.c
		WLFILES_SRC_LO += src/bcmcrypto/rc4.c
		WLFILES_SRC_LO += src/wl/sys/wlc_pktfilterol.c
		WLFILES_SRC_LO += src/wl/sys/wlc_wowlol.c
		WLFILES_SRC_LO += src/bcmcrypto/tkmic.c
		ifeq ($(L2KEEPALIVEOL),1)
		WLFLAGS += -DL2KEEPALIVEOL
		WLFILES_SRC_LO += src/wl/sys/wlc_l2keepaliveol.c
		endif
		ifeq ($(GTKOL),1)
			WLFLAGS += -DGTKOL
			WLFILES_SRC_LO += src/wl/sys/wlc_gtkol.c
		endif
	endif
	ifeq ($(SCANOL),1)
		WLFLAGS += -DSCANOL
		WLFILES_SRC_LO += src/wl/sys/wlc_scan.c
		WLFILES_SRC_LO += src/wl/sys/wlc_scanol.c
	endif
	ifeq ($(SCANOL_HIGH),1)
		WLFLAGS += -DSCANOL_HIGH
		WLFILES_SRC_HI += src/wl/sys/wlc_scanol_iovar.c
	endif
	ifeq ($(MACOL),1)
		WLFLAGS += -DMACOL
		WLFILES_SRC_LO += src/wl/sys/wlc_macol.c
	endif
	ifeq ($(MACOL_HIGH),1)
		WLFLAGS += -DMACOL_HIGH
		WLFILES_SRC_HI += src/wl/sys/wlc_macol_iovar.c
	endif
	ifeq ($(WLPFN),1)
		WLFLAGS += -DWLPFN
		WLFILES_SRC_LO += src/wl/sys/wl_pfn.c
	endif
	WLFILES_SRC_LO += src/shared/qmath.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_gt15.c
	WLFILES_SRC_LO += src/wl/sys/d11ucode_ge24.c
	WLFILES_SRC_LO += src/wl/ppr/src/wlc_ppr.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_cmn.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_ssn.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_extended_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phy_radio_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_n.c
	WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_ssn.c
	WLFILES_SRC_LO += src/wl/sys/wlc_phy_shim.c
	WLFILES_SRC_LO += src/wl/sys/wlc_bmac.c
	WLFILES_SRC_LO += src/wl/sys/wlc_rate_def.c

	ifneq ($(MINIAP),1)
		WLFILES_SRC_LO += src/wl/sys/d11ucode_ge40.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_ac.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phytbl_acdc.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_ac.c
		WLFILES_SRC_LO += src/wl/phy/wlc_phy_ac_gains.c
		WLFILES_SRC_LO += src/wl/sys/d11ucode_le15.c
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
	WLFILES_SRC_HI += src/wl/sys/wlc_stamon.c
	WLFILES_SRC_HI += src/wl/sys/wlc_monitor.c
ifeq ($(ATE),1)
	WLFILES_SRC_HI += src/wl/sys/wlu.c
	WLFILES_SRC_HI += src/wl/sys/wlu_common.c
	WLFILES_SRC_HI += src/wl/shared/miniopt.c
endif
	WLFILES_SRC_HI += src/wl/sys/wlc_utils.c
	WLFILES_SRC_HI += src/wl/sys/wlc_prot.c
	WLFILES_SRC_HI += src/wl/sys/wlc_prot_g.c
	WLFILES_SRC_HI += src/wl/sys/wlc_prot_n.c
	WLFILES_SRC_HI += src/wl/sys/wlc_assoc.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rate.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rate_def.c
	WLFILES_SRC_HI += src/wl/sys/wlc_stf.c
	WLFILES_SRC_HI += src/wl/sys/wlc_lq.c
	ifneq ($(WLOBSSPROT),0)
		WLFLAGS += -DWL_OBSSPROT
	endif
	ifneq ($(WLWSEC),0)
		WLFLAGS += -DWLWSEC
		WLFILES_SRC_HI += src/wl/sys/wlc_security.c
		WLFILES_SRC_HI += src/wl/sys/wlc_key.c
	endif
	WLFILES_SRC_HI += src/wl/sys/wlc_scb.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rate_sel.c
	WLFILES_SRC_HI += src/wl/sys/wlc_scb_ratesel.c

        ifeq ($(WL_PROXDETECT),1)
		WLFLAGS += -DWL_PROXDETECT
		WLFILES_SRC_HI += src/wl/proxd/src/wlc_pdsvc.c
		WLFILES_SRC_HI += src/wl/proxd/src/wlc_pdrssi.c
	endif

#ifdef WL_LPC
	ifeq ($(WL_LPC),1)
		WLFLAGS += -DWL_LPC
		WLFILES_SRC_LO += src/wl/sys/wlc_power_sel.c
		WLFILES_SRC_LO += src/wl/sys/wlc_scb_powersel.c
	else
		ifeq ($(LP_P2P_SOFTAP),1)
			WLFLAGS += -DLP_P2P_SOFTAP
		endif
	endif
#ifdef WL_LPC_DEBUG
	ifeq ($(WL_LPC_DEBUG),1)
		WLFLAGS += -DWL_LPC_DEBUG
	endif
#endif
#endif
	ifeq ($(WL_RELMCAST),1)
		WLFLAGS += -DWL_RELMCAST
		WLFILES_SRC_HI += src/wl/rel_mcast/src/wlc_relmcast.c
	endif
	WLFILES_SRC_HI += src/wl/sys/wlc_antsel.c
	WLFILES_SRC_HI += src/wl/sys/wlc_bsscfg.c
	WLFILES_SRC_HI += src/wl/sys/wlc_vndr_ie_list.c
	WLFILES_SRC_HI += src/wl/sys/wlc_scan.c
	WLFILES_SRC_HI += src/wl/phy/wlc_phy_iovar.c
	WLFILES_SRC_HI += src/wl/sys/wlc_rm.c
	WLFILES_SRC_HI += src/wl/sys/wlc_tso.c
	ifeq ($(WL11AC),1)
		WLFILES_SRC_HI += src/wl/sys/wlc_vht.c
		WLFILES_SRC_HI += src/wl/sys/wlc_txbf.c
		ifeq ($(WLOLPC),1)
			WLFLAGS += -DWLOLPC
			WLFILES_SRC_HI += src/wl/olpc/src/wlc_olpc_engine.c
		endif
	endif
	ifeq ($(WLATF),1)
		WLFILES_SRC_HI += src/wl/sys/wlc_airtime.c
	endif
#ifdef WL11H
	ifeq ($(WL11H),1)
		WLFLAGS += -DWL11H
		WLFILES_SRC_HI += src/wl/sys/wlc_11h.c
		WLFLAGS += -DWLCSA
		WLFILES_SRC_HI += src/wl/sys/wlc_csa.c
		WLFLAGS += -DWLQUIET
		WLFILES_SRC_HI += src/wl/sys/wlc_quiet.c
	endif
#endif /* WL11H */
	# tpc module is shared by 11h tpc and wl tx power control */
	WLTPC := 1
	ifeq ($(WLTPC),1)
		WLFLAGS += -DWLTPC
		WLFILES_SRC_HI += src/wl/sys/wlc_tpc.c
#ifdef WL_AP_TPC
		ifeq ($(WL_AP_TPC),1)
			WLFLAGS += -DWL_AP_TPC
		endif
#endif
		ifeq ($(WL_CHANSPEC_TXPWR_MAX),1)
			WLFLAGS += -DWL_CHANSPEC_TXPWR_MAX
		endif
	endif
	WLFILES_SRC_HI += src/wl/sys/wlc_dfs.c
	ifeq ($(WLC_DISABLE_DFS_RADAR_SUPPORT),1)
		WLFLAGS += -DWLC_DISABLE_DFS_RADAR_SUPPORT
	endif
#ifdef WL11D
	ifeq ($(WL11D),1)
		WLFLAGS += -DWL11D
		WLFILES_SRC_HI += src/wl/sys/wlc_11d.c
	endif
#endif /* WL11D */
	# cntry module is shared by 11h/11d and wl channel */
	WLCNTRY := 1
	ifeq ($(WLCNTRY),1)
		WLFLAGS += -DWLCNTRY
		WLFILES_SRC_HI += src/wl/sys/wlc_cntry.c
	endif
	WLFILES_SRC_HI += src/wl/sys/wlc_event.c
	ifeq ($(WLCHANNEL_DUMMY),1)
		WLFILES_SRC_HI += src/wl/sys/wlc_channel_dummy.c
	else
		WLFILES_SRC_HI += src/wl/sys/wlc_channel.c
		WLFILES_SRC_HI += src/wl/clm/src/wlc_clm.c
		WLFILES_SRC_HI += src/wl/clm/src/wlc_clm_data.c
		ifeq ($(WLCLMINC),1)
			WLFILES_SRC_HI += src/wl/clm/src/wlc_clm_data_inc.c
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
	WLFILES_SRC_HI += src/wl/sys/wlc_scandb.c
	ifeq ($(SCANOL),1)
		WLFILES_SRC_LO += src/wl/sys/wlc_scandb.c
	endif
endif
#endif
	WLFILES_SRC_HI += src/wl/sys/wlc_hrt.c
endif
#endif /* WL */

#ifdef BCMDBUS
ifeq ($(BCMDBUS),1)
	WLFLAGS += -DBCMDBUS
	ifneq ($(BCM_EXTERNAL_MODULE),1)
		WLFILES_SRC += src/shared/dbus.c
	endif

	ifeq ($(BCMTRXV2),1)
		WLFLAGS += -DBCMTRXV2
	endif

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
			WLFILES_SRC += src/shared/dbus_sdh_linux.c
		else
			ifneq ($(BCM_EXTERNAL_MODULE),1)
				WLFILES_SRC += src/shared/dbus_usb.c
				WLFILES_SRC += src/shared/dbus_usb_linux.c
			endif
		endif
	else
		ifeq ($(WLNDIS),1)
			ifeq ($(BCMSDIO),1)
				ifneq ($(WLWIN7)$(WLWIN8),)
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

ifeq ($(EMBED_IMAGE_43236b),1)
	WLFLAGS += -DEMBED_IMAGE_43236b
endif
ifeq ($(EMBED_IMAGE_43526a),1)
	WLFLAGS += -DEMBED_IMAGE_43526a
endif
ifeq ($(EMBED_IMAGE_43526b),1)
	WLFLAGS += -DEMBED_IMAGE_43526b
endif
ifeq ($(EMBED_IMAGE_4360b),1)
	WLFLAGS += -DEMBED_IMAGE_4360b
endif
ifeq ($(EMBED_IMAGE_4325sd),1)
	WLFLAGS += -DEMBED_IMAGE_4325sd
endif
ifeq ($(DNGL_WD_KEEP_ALIVE),1)
	WLFLAGS += -DDNGL_WD_KEEP_ALIVE
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

#ifdef WLLX
ifeq ($(WLLX),1)
	ifneq ($(WL_HIGH),1)
		WLFILES_SRC_LO += src/wl/sys/wl_linux_bmac.c
		WLFILES_SRC_LO += src/shared/bcm_rpc_char.c
	endif

        WLFILES_SRC_HI += src/wl/sys/wl_linux.c
endif
#endif

#ifdef WLLXIW
ifeq ($(WLLXIW),1)
	WLFILES_SRC_HI += src/wl/sys/wl_iw.c
	WLFLAGS += -DWLLXIW
endif
#endif

#ifdef WLLXCFG80211
ifdef ($(WLLXCFG80211),1)
	WLFILES_SRC_HI += src/wl/sys/wl_cfg80211_hybrid.c
endif
#endif

ifeq ($(BCM_STA_CFG80211),1)
	WLFILES_SRC_HI += src/wl/sys/wl_cfg80211.c
	WLFILES_SRC_HI += src/wl/sys/wl_cfgp2p.c
	WLFILES_SRC_HI += src/wl/sys/wldev_common.c
	WLFILES_SRC_HI += src/wl/sys/wl_linux_mon.c
endif

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

ifeq ($(DNGL_WD_KEEP_ALIVE),1)
	WLFLAGS += -DDNGL_WD_KEEP_ALIVE
endif


ifeq ($(TRAFFIC_MGMT),1)
	WLFLAGS += -DTRAFFIC_MGMT
	WLFILES_SRC += src/wl/sys/wlc_traffic_mgmt.c

	ifeq ($(TRAFFIC_SHAPING),1)
		WLFLAGS += -DTRAFFIC_SHAPING
	endif

	ifeq ($(TRAFFIC_MGMT_RSSI_POLICY),1)
		WLFLAGS += -DTRAFFIC_MGMT_RSSI_POLICY=$(AP)
	endif

	ifeq ($(WLINTFERSTAT),1)
        WLFLAGS += -DWLINTFERSTAT
	endif

	ifeq ($(TRAFFIC_MGMT_DWM),1)
		WLFLAGS += -DTRAFFIC_MGMT_DWM
	endif
endif

ifeq ($(WLSTAPRIO),1)
	WLFLAGS += -DWL_STAPRIO
	WLFILES_SRC += src/wl/sys/wlc_staprio.c
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
	WLFILES_SRC += src/shared/bcmwifi/src/bcmwifi_channels.c
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

	ifneq ($(WLVISTA)$(WLWIN7)$(WLWIN8),)
		WLFLAGS += -DEXT_STA
		WLFLAGS += -DWL_MONITOR
	        WLFLAGS += -DIBSS_PEER_GROUP_KEY
	        WLFLAGS += -DIBSS_PEER_DISCOVERY_EVENT
	        WLFLAGS += -DIBSS_PEER_MGMT
	endif

	ifneq ($(WLWIN7)$(WLWIN8),)
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

		ifneq ($(WLVISTA)$(WLWIN7)$(WLWIN8),)
			WLFILES_SRC += src/wl/sys/wlc_rate.c
		endif

		ifneq ($(WLWIN7)$(WLWIN8),)
			WLFILES_SRC += src/wl/sys/wlc_ap.c
			WLFILES_SRC += src/wl/sys/wlc_apps.c
		endif

		ifeq ($(WLXP),1)
			WLFLAGS += -DNDIS_DMAWAR
			# move these non-wl flag to makefiles
			WLFLAGS += -DBINARY_COMPATIBLE -DWIN32_LEAN_AND_MEAN=1
		endif

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
	ifeq ($(WLVISTA),1)
		# DONGLE vista needs WL_MONITOR to pass RTM
		WLFLAGS += -DEXT_STA -DWL_MONITOR -DWLCNTSCB
		WLFILES_SRC += src/wl/sys/wl_gtkrefresh.c
	endif
endif
#endif


#ifdef BINOSL
ifeq ($(BINOSL),1)
	WLFLAGS += -DBINOSL
endif
#endif

## wl features
# ACCONF -- 0 is remove from code, else bit mask of supported acphy revs
ifneq ($(ACCONF),)
	WLFLAGS += -DACCONF=$(ACCONF)
endif

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

#ifdef SOFTAP
ifeq ($(SOFTAP),1)
	WLFLAGS += -DSOFTAP
endif
#endif

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

	ifeq ($(DWDS),1)
		WLFLAGS += -DDWDS
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

#ifdef EXT_STA_DONGLE
# Microsoft Extensible STA for Dongle
ifeq ($(EXT_STA_DONGLE),1)
	WLFLAGS += -DEXT_STA
	WLFLAGS += -DIBSS_PEER_GROUP_KEY
	WLFLAGS += -DIBSS_PEER_DISCOVERY_EVENT
	WLFLAGS += -DIBSS_PEER_MGMT
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
ifeq ($(IGMP_UCQUERY), 1)
	WLFLAGS += -DWL_IGMP_UCQUERY
endif
ifeq ($(UCAST_UPNP), 1)
	WLFLAGS += -DWL_UCAST_UPNP
endif
ifeq ($(IGMPQ_FILTER), 1)
	WLFLAGS += -DWL_WMF_IGMP_QUERY_FILTER
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

#ifdef PSTA
# Proxy STA
ifeq ($(PSTA),1)
	WLFILES_SRC_HI += src/wl/sys/wlc_psta.c
	WLFLAGS += -DPSTA
endif
#endif

#ifdef DPSTA
# Dualband Proxy STA
ifeq ($(DPSTA),1)
	WLFLAGS += -DDPSTA
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

#ifdef WL_STA_MONITOR
ifeq ($(WL_STA_MONITOR),1)
	WLFLAGS += -DWL_STA_MONITOR
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

#ifdef WLPIO
# WLPIO
ifeq ($(WLPIO),1)
	WLFLAGS += -DWLPIO
	WLFILES_SRC_LO += src/wl/sys/wlc_pio.c
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
	L2_FILTER := 1
	WLFLAGS += -DWL11U -DWIFI_ACT_FRAME
	WLFILES_SRC_HI += src/wl/sys/wlc_11u.c
endif
#endif

#ifdef WLPROBRESP_SW
# WLPROBRESP_SW
ifeq ($(WLPROBRESP_SW),1)
	WLFLAGS += -DWLPROBRESP_SW
	WLFILES_SRC_HI += src/wl/sys/wlc_probresp.c
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
ifndef DELTASTATS
	DELTASTATS := 1
endif
endif
#endif

# DELTASTATS
ifeq ($(DELTASTATS),1)
	WLFLAGS += -DDELTASTATS
endif

#ifdef WLCHANIM
# WLCHANIM
ifeq ($(WLCHANIM),1)
	WLFLAGS += -DWLCHANIM
endif
#endif

#ifdef WLCNTSCB
# WLCNTSCB
ifeq ($(WLCNTSCB),1)
	WLFLAGS += -DWLCNTSCB
endif
#endif


ifeq ($(WL_OKC),1)
	WLFLAGS += -DWL_OKC
	WLFILES_SRC_HI += src/wl/sys/wlc_okc.c
endif
ifeq ($(WL_RCC),1)
	WLFILES_SRC_HI += src/wl/sys/wlc_okc.c
endif

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
	WLFLAGS += -DWLFBTOVERDS
	WLFLAGS += -DBCMINTSUP
	WLFLAGS += -DWLCAC
	WLFILES_SRC_HI += src/wl/sys/wlc_sup.c
	WLFILES_SRC_HI += src/bcmcrypto/aes.c
	WLFILES_SRC_HI += src/bcmcrypto/aeskeywrap.c
	WLFILES_SRC_HI += src/bcmcrypto/prf.c
	WLFILES_SRC_HI += src/bcmcrypto/sha1.c
	WLFILES_SRC_HI += src/bcmcrypto/hmac_sha256.c
	WLFILES_SRC_HI += src/bcmcrypto/sha256.c
	WLFILES_SRC_HI += src/wl/sys/wlc_cac.c
	# NetBSD 2.0 has MD5 and AES built in
	ifneq ($(OSLBSD),1)
		WLFILES_SRC_HI += src/bcmcrypto/md5.c
		WLFILES_SRC_HI += src/bcmcrypto/rijndael-alg-fst.c
	endif
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

#ifdef WLTDLS
ifeq ($(WLTDLS), 1)
ifndef ($(WLFBT),1)
	WLFILES_SRC_HI += src/bcmcrypto/sha256.c
	WLFILES_SRC_HI += src/bcmcrypto/hmac_sha256.c
endif
endif
#endif

#ifdef BCMCCMP
# Soft AES CCMP
ifeq ($(BCMCCMP),1)
	WLFLAGS += -DBCMCCMP
	WLFILES_SRC_HI += src/bcmcrypto/aes.c
	# BSD has AES built in
	ifneq ($(BSD),1)
		WLFILES_SRC_HI += src/bcmcrypto/rijndael-alg-fst.c
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
	WLFILES_SRC_HI += src/bcmcrypto/prf.c
	WLFILES_SRC_HI += src/bcmcrypto/sha1.c
	WLFILES_SRC_HI += src/wl/sys/wlc_mfp.c
	# BSD has AES built in
	ifneq ($(BSD),1)
		WLFILES_SRC_HI += src/bcmcrypto/rijndael-alg-fst.c
	endif
	ifneq ($(AP)$(MFP_TEST),)
		WLFLAGS += -DMFP_TEST
		WLFILES_SRC_HI += src/wl/sys/wlc_mfp_test.c
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

#ifdef WLAMSDU_TX
ifeq ($(WLAMSDU_TX),1)
	WLFLAGS += -DWLAMSDU_TX
endif
#endif

#ifdef WLAMSDU_SWDEAGG
ifeq ($(WLAMSDU_SWDEAGG),1)
	WLFLAGS += -DWLAMSDU_SWDEAGG
endif
#endif

#ifdef WLNAR
ifeq ($(WLNAR),1)
    WLFILES_SRC_HI += src/wl/sys/wlc_nar.c
    WLFLAGS += -DWLNAR
endif
#endif

#ifdef WLTAF
ifeq ($(WLTAF),1)
    WLFILES_SRC_HI += src/wl/sys/wlc_taf.c
    WLFLAGS += -DWLTAF
endif
#endif
#ifdef WLAMPDU
ifeq ($(WLAMPDU),1)
	WLFLAGS += -DWLAMPDU
	WLFILES_SRC_HI += src/wl/sys/wlc_ampdu.c
	WLFILES_SRC_HI += src/wl/sys/wlc_ampdu_rx.c
	WLFILES_SRC_HI += src/wl/sys/wlc_ampdu_cmn.c
	ifeq ($(WLAMPDU_UCODE),1)
		WLFLAGS += -DWLAMPDU_UCODE -DWLAMPDU_MAC
	endif
	ifeq ($(WLAMPDU_HW),1)
		WLFLAGS += -DWLAMPDU_HW -DWLAMPDU_MAC
	endif
	ifeq ($(WLAMPDU_AQM),1)
		WLFLAGS += -DWLAMPDU_AQM -DWLAMPDU_MAC
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
	WLFILES_SRC_HI += src/wl/sys/d11ucode_p2p.c
	WLFILES_SRC_HI += src/wl/sys/d11ucode_ge40.c
	WLFILES_SRC_HI += src/wl/sys/d11ucode_ge24.c
	WLFILES_SRC_HI += src/wl/sys/wlc_wowl.c
	WLFILES_SRC_HI += src/wl/sys/wowlaestbls.c
endif
ifeq ($(WOWL_OS_OFFLOADS),1)
ifneq ($(WLTEST),1)
	WLFLAGS += -DWOWL_OS_OFFLOADS
endif
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


#ifdef WLTDLS
ifeq ($(TDLS_TESTBED), 1)
	WLFLAGS += -DTDLS_TESTBED
endif
ifeq ($(WLTDLS), 1)
	WLFLAGS += -DWLTDLS
	WLFILES_SRC_HI += src/wl/sys/wlc_tdls.c
endif
#endif

#ifdef WLDLS
ifeq ($(WLDLS), 1)
	WLFLAGS += -DWLDLS
	WLFILES_SRC_HI += src/wl/sys/wlc_dls.c
endif
#endif

#ifdef WLBSSLOAD
# WLBSSLOAD
ifeq ($(WLBSSLOAD),1)
	WLFLAGS += -DWLBSSLOAD
	WLFILES_SRC_HI += src/wl/sys/wlc_bssload.c
endif
#endif

#ifdef L2_FILTER
ifeq ($(L2_FILTER),1)
	WLFLAGS += -DL2_FILTER
	ifeq ($(L2_FILTER_STA),1)
		WLFLAGS += -DL2_FILTER_STA
    endif
	WLFILES_SRC += src/wl/sys/wlc_l2_filter.c
endif
#endif

#ifdef WLP2P
ifeq ($(WLP2P),1)
	WLFLAGS += -DWLP2P
	WLFILES_SRC_HI += src/wl/sys/wlc_p2p.c
	WLFLAGS += -DWL_BSSCFG_TX_SUPR -DWIFI_ACT_FRAME
	WLMCNX := 1
ifndef WLMCHAN
	WLMCHAN := 1
endif
endif
#endif /* WLP2P */
#ifdef WLOFFLD
ifeq ($(WLTEST),1)
	WLOFFLD=0
endif
ifeq ($(WLOFFLD),1)
	ifeq ($(WL_SPLIT),0)
		WLFLAGS += -DWLOFFLD
		WLFLAGS += -DWL_OFFLOADSTATS
		WLFILES_SRC_HI += src/wl/sys/wlc_offloads.c
		WLFILES_SRC_HI += src/shared/bcm_ol_msg.c

	ifeq ($(NET_DETECT),1)
		WLFLAGS += -DNET_DETECT
		WLFILES_SRC_HI += src/wl/sys/wlc_net_detect.c
	endif

		WLFLAGS += -DEMBED_IMAGE_4352PCI
		WLFLAGS += -DEMBED_IMAGE_4350PCI
	endif
endif
#endif /* WLOFFLD */

#ifdef SRHWVSDB
ifeq ($(SRHWVSDB),1)
	WLFLAGS += -DSRHWVSDB
endif
ifeq ($(PHY_WLSRVSDB),1)
	# cpp define WLSRVSDB is used in this branch by PHY code only
	WLFLAGS += -DWLSRVSDB
endif
#endif /* SRHWVSDB */

#ifdef WLMCHAN
ifeq ($(WLMCHAN),1)
	WLMCNX := 1
endif
#endif

ifeq ($(WLMCNX),1)
	WLFLAGS += -DWLP2P_UCODE
	WLFILES_SRC_LO += src/wl/sys/d11ucode_p2p.c
	WLFLAGS += -DWLMCNX
	WLFILES_SRC_HI += src/wl/sys/wlc_mcnx.c
	ifeq ($(FULLDNGLBLD),1)
		ifneq ($(WLP2P_UCODE_ONLY),1)
			# force P2P UCODE for MCNX on dongle
			WLP2P_UCODE_ONLY := 1
		endif
	endif
endif

ifeq ($(WLP2P_UCODE_ONLY),1)
	WLFLAGS += -DWLP2P_UCODE_ONLY
endif

#ifdef WLMCHAN
ifeq ($(WLMCHAN),1)
ifeq ($(WL_HIGH),1)
	WLFLAGS += -DWLTXPWR_CACHE
else
	ifneq ($(PHY_WLSRVSDB),1)
		WLFLAGS += -DWLTXPWR_CACHE
	endif
endif
	WLFLAGS += -DWLMCHAN
	WLFILES_SRC_HI += src/wl/sys/wlc_mchan.c
ifndef WLMULTIQUEUE
	WLMULTIQUEUE := 1
endif
endif
#endif /* WLMCHAN */

#ifdef WLMULTIQUEUE
ifeq ($(WLMULTIQUEUE), 1)
	WLFLAGS += -DWL_MULTIQUEUE
endif
#endif /* WLMULTIQUEUE */

ifeq ($(WLMCHAN), 1)
	ifeq ($(WL_SPLIT), 1)
		CCA_STATS = 0
	endif
endif

ifeq ($(CCA_STATS),1)
	WLFLAGS += -DCCA_STATS
	WLFILES_SRC_HI += src/wl/sys/wlc_cca.c
	ifeq ($(ISID_STATS),1)
		WLFLAGS += -DISID_STATS
		WLFILES_SRC_HI += src/wl/sys/wlc_interfere.c
	endif
endif

#ifdef WLRWL
ifeq ($(WLRWL),1)
	WLFLAGS += -DWLRWL -DRWL_WIFI
	WLFILES_SRC_HI += src/wl/sys/wlc_rwl.c
endif
#endif

ifeq ($(D0_COALESCING),1)
	WLFLAGS += -DD0_COALESCING
	WLFILES_SRC_HI += src/wl/sys/wl_d0_filter.c
	WLMCHAN := 1
endif

ifneq ($(WLNDIS_DHD),1)
endif

#ifdef WLPLT
ifeq ($(WLPLT),1)
	WLFLAGS += -DWLPLT
	WLFILES_SRC_HI += src/wl/sys/wlc_plt.c
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

ifeq ($(WLMEDIA_FLAMES),1)
        WLFLAGS += -DWLMEDIA_FLAMES
endif

ifeq ($(STBLINUX),1)
	WLFLAGS += -DSTB
	ifeq ($(BCMEXTNVM),1)
		WLFLAGS += -DBCMEXTNVM -DBCM47XX
		WLFILES_SRC_LO += src/shared/bcmsromio.c
	endif
endif

#ifdef WL_WFDLL
ifeq ($(WL_WFDLL),1)
	WLFLAGS += -DWFD_PHY_LL
ifeq ($(WL_WFDLLDBG),1)
	WLFLAGS += -DWFD_PHY_LL_DEBUG
endif
endif

#ifdef WLPKTDLYSTAT
ifeq ($(WLPKTDLYSTAT),1)
	WLFLAGS += -DWLPKTDLYSTAT
endif
#endif

#ifdef WLPKTDLYSTAT_IND
ifeq ($(WLPKTDLYSTAT_IND),1)
	WLFLAGS += -DWLPKTDLYSTAT_IND
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
	WLFILES_SRC += src/shared/bcmwifi/src/bcmwifi_channels.c
endif

ifeq ($(NVRAM),1)
	WLFILES_SRC_LO += src/dongle/rte/test/nvram.c
	WLFILES_SRC_LO += src/dongle/rte/sim/nvram.c
	WLFILES_SRC_LO += src/shared/nvram.c
endif

ifeq ($(NVRAMVX),1)
	WLFILES_SRC_LO += src/shared/nvram_rw.c
endif
#endif /* LINUX_HYBRID */

#ifdef BCMNVRAMR
ifeq ($(BCMNVRAMR),1)
	WLFILES_SRC_LO += src/shared/nvram_ro.c
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
				WLFILES_SRC_LO += src/shared/nvram_ro.c
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
	ifeq ($(SCANOL),0)
		WLFLAGS += -DWLPFN
		WLFILES_SRC += src/wl/sys/wl_pfn.c
		ifeq ($(WLPFN_AUTO_CONNECT),1)
			WLFLAGS += -DWLPFN_AUTO_CONNECT
		endif
	endif
endif
#endif /* WLPFN */

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

#ifdef MDNS
ifeq ($(MDNS),1)
	WLFLAGS += -DMDNS
	WLFILES_SRC += src/wl/sys/wl_mdns_main.c
	WLFILES_SRC += src/wl/sys/wl_mdns_common.c
endif
#endif

#ifdef P2PO
ifeq ($(P2PO),1)
	WLFLAGS += -DP2PO
	WLFLAGS += -DWL_EVENTQ
	WLFLAGS += -DBCM_DECODE_NO_ANQP
	WLFLAGS += -DBCM_DECODE_NO_HOTSPOT_ANQP
	WLFILES_SRC += src/wl/sys/wl_p2po.c
	WLFILES_SRC += src/wl/sys/wl_p2po_disc.c
	WLFILES_SRC += src/wl/sys/wl_gas.c
	WLFILES_SRC += src/wl/sys/wl_tmr.c
	WLFILES_SRC += src/wl/sys/bcm_p2p_disc.c
	WLFILES_SRC += src/wl/sys/wl_eventq.c
	WLFILES_SRC += src/wl/gas/src/bcm_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_ie.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_anqp.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_hspot_anqp.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_p2p.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_ie.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_anqp.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_hspot_anqp.c
	WLFILES_SRC += src/wl/encode/src/trace.c
endif
#endif

#ifdef ANQPO
ifeq ($(ANQPO),1)
	WLFLAGS += -DANQPO
	WLFLAGS += -DWL_EVENTQ
	WLFLAGS += -DBCM_DECODE_NO_ANQP
	WLFLAGS += -DBCM_DECODE_NO_HOTSPOT_ANQP
	WLFILES_SRC += src/wl/sys/wl_anqpo.c
	WLFILES_SRC += src/wl/sys/wl_gas.c
	WLFILES_SRC += src/wl/sys/wl_tmr.c
	WLFILES_SRC += src/wl/sys/wl_eventq.c
	WLFILES_SRC += src/wl/gas/src/bcm_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_ie.c
	WLFILES_SRC += src/wl/encode/src/bcm_decode_p2p.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_gas.c
	WLFILES_SRC += src/wl/encode/src/bcm_encode_ie.c
endif
#endif

#ifdef TCPKAOE
ifeq ($(TCPKAOE),1)
	WLFLAGS += -DTCPKAOE
	WLFILES_SRC += src/wl/sys/wl_tcpkeepol.c
endif
#endif

#ifdef NWOE
ifeq ($(NWOE),1)
	WLFLAGS += -DNWOE
	WLFILES_SRC += src/wl/sys/wl_nwoe.c
	WLFILES_SRC += src/wl/lwip/src/core/def.c
	WLFILES_SRC += src/wl/lwip/src/core/dns.c
	WLFILES_SRC += src/wl/lwip/src/core/mem.c
	WLFILES_SRC += src/wl/lwip/src/core/netif.c
	WLFILES_SRC += src/wl/lwip/src/core/raw.c
	WLFILES_SRC += src/wl/lwip/src/core/stats.c
	WLFILES_SRC += src/wl/lwip/src/core/tcp.c
	WLFILES_SRC += src/wl/lwip/src/core/tcp_out.c
	WLFILES_SRC += src/wl/lwip/src/core/udp.c
	WLFILES_SRC += src/wl/lwip/src/core/dhcp.c
	WLFILES_SRC += src/wl/lwip/src/core/init.c
	WLFILES_SRC += src/wl/lwip/src/core/memp.c
	WLFILES_SRC += src/wl/lwip/src/core/pbuf.c
	WLFILES_SRC += src/wl/lwip/src/core/sys.c
	WLFILES_SRC += src/wl/lwip/src/core/tcp_in.c
	WLFILES_SRC += src/wl/lwip/src/core/timers.c
	WLFILES_SRC += src/wl/lwip/src/netif/etharp.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/autoip.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/icmp.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/igmp.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/inet.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/inet_chksum.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/ip_addr.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/ip.c
	WLFILES_SRC += src/wl/lwip/src/core/ipv4/ip_frag.c
endif
#endif

#ifdef WLNDOE
ifeq ($(WLNDOE),1)
	WLFLAGS += -DWLNDOE
	WLFILES_SRC += src/wl/sys/wl_ndoe.c
endif
#endif

#ifdef PLC
ifeq ($(PLC),1)
	WLFLAGS += -DPLC
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

ifeq ($(WL11K_AP),1)
	WLFLAGS += -DWL11K_AP
	WL11K = 1
endif
ifeq ($(WL11K),1)
	WLFLAGS += -DWL11K
	WLFILES_SRC += src/wl/sys/wlc_rrm.c
endif

ifeq ($(WLWNM_AP),1)
	WLWNM := 1
	WLFLAGS += -DWLWNM_AP
endif

ifeq ($(WLWNM),1)
	WLFLAGS += -DWLWNM
	ifeq ($(STA),1)
		ifneq ($(WLWNM_AP),1)
			KEEP_ALIVE = 1
		endif
	endif
	WLFILES_SRC += src/wl/sys/wlc_wnm.c
endif

ifeq ($(KEEP_ALIVE),1)
	WLFLAGS += -DKEEP_ALIVE
	WLFILES_SRC += src/wl/sys/wl_keep_alive.c
endif
#endif  ##LINUX_HYBRID

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
	WLC_PATCH_IOCTL ?= 1
	ifeq ($(WLC_PATCH_IOCTL),1)
		WLFLAGS += -DWLC_PATCH_IOCTL
	endif
	WLFILES_SRC += $(WLPATCHFILE)
endif

ifeq ($(SAMPLE_COLLECT),1)
	WLFLAGS += -DSAMPLE_COLLECT
endif

ifeq ($(SMF_STATS),1)
	WLFLAGS += -DSMF_STATS
endif

#ifdef PHYMON
ifeq ($(PHYMON),1)
	WLFLAGS += -DPHYMON
endif
#endif

ifeq ($(BCM_EXTERNAL_MODULE),1)
	WLFLAGS += -DLINUX_EXTERNAL_MODULE_DBUS
endif

#ifdef BCM_DCS
ifeq ($(BCM_DCS),1)
	WLFLAGS += -DBCM_DCS
endif
#endif

ifeq ($(WLMCHAN), 1)
	ifeq ($(WL_SPLIT), 1)
		WL_THREAD = 1
		USBOS_THREAD = 1
	endif
endif

ifeq ($(WL_THREAD),1)
	WLFLAGS += -DWL_THREAD
endif

ifneq ($(WL_THREADNICE),)
	WLFLAGS += -DWL_THREADNICE=$(WL_THREADNICE)
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

ifeq ($(WL_LTR),1)
	WLFLAGS += -DWL_LTR
endif

ifeq ($(WL_PRE_AC_DELAY_NOISE_INT),1)
	WLFLAGS += -DWL_PRE_AC_DELAY_NOISE_INT
endif

ifeq ($(SAVERESTORE),1)
	WLFLAGS += -DSAVERESTORE
	SR_ESSENTIALS := 1
endif

# minimal functionality required to operate SR engine. Examples: sr binary download, sr enable.
ifeq ($(SR_ESSENTIALS),1)
	WLFLAGS += -DSR_ESSENTIALS
	WLFILES_SRC_LO += src/shared/sr_array.c
	WLFILES_SRC_LO += src/shared/saverestore.c
endif

#ifdef PMU_OPT
ifeq ($(PMU_OPT),1)
	WLFLAGS += -DPMU_OPT
endif
#endif

#ifdef PMU_OPT_REV6
ifeq ($(PMU_OPT_REV6),1)
	WLFLAGS += -DPMU_OPT_REV6
endif
#endif

#ifdef BCM_REQUEST_FW
ifeq ($(BCM_REQUEST_FW), 1)
	WLFLAGS += -DBCM_REQUEST_FW
endif
#endif

# HW CSO support (D11 rev40 feature)
ifeq ($(WLCSO),1)
	WLFLAGS += -DWLCSO
endif

# add a flag to indicate the split to linux kernels
WLFLAGS += -DPHY_HAL

# compile only 1x1 ACPHY related code
ifeq ($(ACPHY_1X1_ONLY),1)
WLFLAGS += -DACPHY_1X1_ONLY
endif

#ifdef WET_TUNNEL
ifeq ($(WET_TUNNEL),1)
	WLFLAGS += -DWET_TUNNEL
	WLFILES_SRC_HI += src/wl/sys/wlc_wet_tunnel.c
endif
#endif

#ifdef WLDURATION
ifeq ($(WLDURATION),1)
	WLFLAGS += -DWLDURATION
	WLFILES_SRC += src/wl/sys/wlc_duration.c
endif
#endif

#ifdef WL_BCN_COALESCING
ifeq ($(WL_BCN_COALESCING),1)
	WLFLAGS += -DWL_BCN_COALESCING
	WLFILES_SRC += src/wl/sys/wlc_bcn_clsg.c
endif
#endif

#ifdef TINY_PKTJOIN
ifeq ($(TINY_PKTJOIN),1)
	WLFLAGS += -DTINY_PKTJOIN
endif
#endif

#ifdef WL_RXEARLYRC
ifeq ($(WL_RXEARLYRC),1)
	WLFLAGS += -DWL_RXEARLYRC
endif
#endif

#ifdef WLMCHAN
#ifdef PROP_TXSTATUS
ifeq ($(WLMCHAN),1)
ifeq ($(PROP_TXSTATUS),1)
	ifeq ($(WL_SPLIT),0)
		WLFLAGS += -DWLMCHANPRECLOSE
		WLFLAGS += -DBBPLL_PARR
	endif
endif
endif
#endif  ## PROP_TXSTATUS
#endif  ## WLMCHAN

#ifdef WLRXOV
ifeq ($(WLRXOV),1)
	WLFLAGS += -DWLRXOV
endif
#endif

ifeq ($(PKTC_DONGLE),1)
	WLFLAGS += -DPKTC_DONGLE
endif

# Legacy WLFILES pathless definition, please use new src relative path
# in make files.
WLFILES := $(sort $(notdir $(WLFILES_SRC)))

ifeq ($(AMPDU_HOSTREORDER), 1)
	WLFLAGS += -DBRCMAPIVTW=128 -DWLAMPDU_HOSTREORDER
endif

# Work-arounds for ROM compatibility - relocate struct fields that were excluded in ROMs,
# but are required in ROM offload builds.
ifeq ($(WLC_PUB_OLDROM_BEAMFORMING_FIELDS),1)
	EXTRA_DFLAGS += -DWLC_PUB_OLDROM_BEAMFORMING_FIELDS
endif

ifeq ($(WLC_STF_OLDROM_BEAMFORMING_FIELDS),1)
	EXTRA_DFLAGS += -DWLC_STF_OLDROM_BEAMFORMING_FIELDS
endif


# used by the 4335c0 ROM. It avoids ROM abandons for every function that invokes this macro due to
# additional devices added post tape-out.
ifeq ($(IS_SINGLEBAND_5G_4335C0_ROM_COMPAT),1)
	EXTRA_DFLAGS += -DIS_SINGLEBAND_5G_4335C0_ROM_COMPAT
endif

ifeq ($(WL_EVDATA_BSSINFO),1)
	EXTRA_DFLAGS += -DWL_EVDATA_BSSINFO
endif

# Work-arounds for ROM compatibility to handle ROM that exclude MFP support.
ifeq ($(WLC_MFP_ROM_COMPAT),1)
	EXTRA_DFLAGS += -DWLC_MFP_ROM_COMPAT
endif

# This feature disables mac sleep on 11AC router platforms (zero packet loss).
ifeq ($(WLMAC_RX_NO_SLEEP),1)
	WLFLAGS += -DWLMAC_RX_NO_SLEEP
endif

# enabling secure DMA feature

ifeq ($(BCM_SECURE_DMA),1)
	WLFLAGS += -DBCM_SECURE_DMA
endif
