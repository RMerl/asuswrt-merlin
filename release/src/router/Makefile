#
# Broadcom Linux Router Makefile
#
# Copyright 2016, Broadcom
# All Rights Reserved.
#
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
#

include common.mak
include $(SRCBASE)/.config

ifeq ($(RTCONFIG_RALINK),y)
else ifeq ($(RTCONFIG_QCA),y)
export CFLAGS += $(EXTRACFLAGS)
else
# Broadcom
export CFLAGS += -DBCMWPA2
ifeq ($(RTCONFIG_BCMWL6),y)
export CFLAGS += -DBCMQOS
export CFLAGS += -DEXT_ACS
export CFLAGS += -DD11AC_IOTYPES
export CFLAGS += -DPHYMON
export CFLAGS += -DPROXYARP
export CFLAGS += -DTRAFFIC_MGMT
export CFLAGS += -DTRAFFIC_MGMT_RSSI_POLICY
ifeq ($(RTCONFIG_BCMARM),y)
export CONFIG_MFP=y
export CFLAGS += -DMFP
export CFLAGS += -D__CONFIG_MFP__
endif
endif
export CFLAGS += $(EXTRACFLAGS)
endif

# New Broadcom/Ralink/MTK/QCA platform.
KPATH_2636_OR_3X = $(if $(or $(findstring linux-2.6.36,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR)),$(findstring linux-3.3.x,$(LINUXDIR)),$(findstring linux-3.4.x,$(LINUXDIR)),$(findstring linux-3.14.x,$(LINUXDIR))),y)

# New Ralink/MTK/QCA platform.
KPATH_2636X_OR_3X = $(if $(or $(findstring linux-2.6.36.x,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR)),$(findstring linux-3.3.x,$(LINUXDIR)),$(findstring linux-3.4.x,$(LINUXDIR)),$(findstring linux-3.14.x,$(LINUXDIR))),y)

# New Ralink/MTK/QCA platform.
KPATH_3X = $(if $(or $(findstring linux-3.x,$(LINUXDIR)),$(findstring linux-3.3.x,$(LINUXDIR)),$(findstring linux-3.4.x,$(LINUXDIR)),$(findstring linux-3.14.x,$(LINUXDIR))),y)

ifneq ($(findstring 4G-,$(BUILD_NAME)),)
MODEL = RT$(subst -,,$(BUILD_NAME))
else ifneq ($(findstring DSL,$(BUILD_NAME)),)
MODEL = $(subst -,_,$(BUILD_NAME))
else
MODEL = $(subst -,,$(subst +,P,$(BUILD_NAME)))
endif
export CFLAGS += -D$(MODEL)
export $(MODEL)=y

export OLD_SRC=$(SRCBASE)/../src
ifeq ($(RTCONFIG_BCMARM),y)
#export CFLAGS += -I$(SRCBASE)/common/include

LINUX_VERSION=2_6_36
LINUX_KERNEL=2.6.36
export PLATFORM LIBDIR USRLIBDIR LINUX_VERSION

ifeq ($(RTCONFIG_BCM7),y)
export BCMSRC=src-rt-7.x.main/src
export OLD_SRC=$(SRCBASE)/../../src
export  BUILD_MFG := 0
export  EX7 := _7
else ifeq ($(RTCONFIG_BCM_794),y)
export BCMSRC=src-rt-7.14.94.x/src
export OLD_SRC=$(SRCBASE)/../../src
export  BUILD_MFG := 0
export  EX7 := _794
else ifeq ($(RTCONFIG_BCM_7114),y)
export BCMSRC=src-rt-7.14.114.x/src
export OLD_SRC=$(SRCBASE)/../../src
export  BUILD_MFG := 0
export  EX7 := _7114
else ifeq ($(RTCONFIG_BCM9),y)
export BCMSRC=src-rt-9.x/src
export OLD_SRC=$(SRCBASE)/../../src
export  BUILD_MFG := 0
export  EX7 := _9
else
WLAN_ComponentsInUse := bcmwifi clm ppr olpc
export BCMSRC=src-rt-6.x.4708
endif
include $(SRCBASE)/makefiles/WLAN_Common.mk
export BASEDIR := $(WLAN_TreeBaseA)
export LINUXDIR := $(SRCBASE)/linux/linux-2.6.36

export EXTRALDFLAGS = -lgcc_s
export EXTRALDFLAGS2 = -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared


export LD_LIBRARY_PATH := $(SRCBASE)/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3/lib
ifeq (2_6_36,$(LINUX_VERSION))
export  BUILD_MFG := 0
endif
SUBMAKE_SETTINGS = SRCBASE=$(SRCBASE) BASEDIR=$(BASEDIR)
SUBMAKE_SETTINGS += ARCH=$(ARCH)
export CFLAGS += -O2
export OPTCFLAGS = -O2
WLCFGDIR=$(SRCBASE)/wl/config

CRAMFSDIR := cramfs

export MKSYM := $(shell which $(TOP)/misc/mksym.pl)
endif

ifeq ($(or $(RTCONFIG_BCMWL6),$(RTAC53U)),y)

WLAN_ComponentsInUse := bcmwifi clm ppr

ifeq ($(or $(RTCONFIG_BCMWL6A),$(RTAC53U)),y)
WLAN_ComponentsInUse += olpc
endif

include ../../$(SRCBASE)/makefiles/WLAN_Common.mk
endif

ifneq ($(RTCONFIG_BCMARM),y)
KDIR=$(TOP)/kernel_header
KERNEL_HEADER_DIR=$(TOP)/kernel_header/include
else
KDIR=$(LINUXDIR)
KERNEL_HEADER_DIR=$(LINUXDIR)/include
endif

export KDIR KERNEL_HEADER_DIR
ifeq ($(RTCONFIG_DHDAP),y)
export CONFIG_DHDAP=y
export CFLAGS += -D__CONFIG_DHDAP__
ifeq ($(RTCONFIG_BCM_7114),y)
export SRCBASE_DHD := $(SRCBASE)/../dhd/src
export SRCBASE_FW := $(SRCBASE)/../4365/src
export SRCBASE_SYS := $(SRCBASE_DHD)
include Makefile.fw
else	# 7114
export DHDAP_USE_SEPARATE_CHECKOUTS := 1
export SRCBASE_DHD := $(SRCBASE)/../../dhd/src
export SRCBASE_FW  := $(SRCBASE)/../../43602/src
PCIEFD_TARGETS_LIST	:= 43602a1-roml
ifeq ($(WLTEST),1)
PCIEFD_TARGET_NAME	:= pcie-ag-splitrx-fdap-mbss-mfgtest-seqcmds-phydbg-txbf-pktctx-amsdutx-ampduretry-chkd2hdma
else
PCIEFD_TARGET_NAME	:= pcie-ag-splitrx-fdap-mbss-mfp-wl11k-wl11u-txbf-pktctx-amsdutx-ampduretry-chkd2hdma-proptxstatus
endif

PCIEFD_EMBED_HEADER_TEMPLATE := $(SRCBASE_DHD)/shared/rtecdc_router.h.in
PCIEFD_EMBED_HEADER	:= $(SRCBASE_DHD)/shared/rtecdc_router.h
obj-pciefd		:= $(patsubst %,%-obj,$(PCIEFD_TARGETS_LIST))
install-pciefd		:= $(patsubst %,%-install,$(PCIEFD_TARGETS_LIST))
endif	#  7114
endif

ifeq ($(RTCONFIG_WLEXE),y)
ifeq ($(RTCONFIG_BCM9),y)
export SRCBASE_SYS := $(SRCBASE)/../sys/src
export CONFIG_WLEXE
export RWL ?= 0
endif
endif

ifeq ($(RTCONFIG_GMAC3),y)
export CFLAGS += -D__CONFIG_GMAC3__
endif

ifeq ($(RTCONFIG_TOAD),y)
export CFLAGS += -D__CONFIG_TOAD__
endif

ifeq ($(RTCONFIG_BCMBSD),y)
export CFLAGS += -DBCM_BSD
endif
ifeq ($(RTCONFIG_BCMSSD),y)
export CFLAGS += -DBCM_SSD
endif

ifeq ($(RTCONFIG_WLCLMLOAD),y)
export CFLAGS += -DWLCLMLOAD
export WLCLMLOAD := 1
endif

ifeq ($(RTCONFIG_BCMASPMD),y)
export CFLAGS += -DBCM_ASPMD
endif

ifeq ($(RTCONFIG_BCMEVENTD),y)
export CFLAGS += -DBCM_EVENTD
endif

ifeq ($(RTCONFIG_BCMDCS),y)
export CFLAGS += -DBCM_DCS
endif

ifeq ($(RTCONFIG_EMF),y)
export CFLAGS += -D__CONFIG_EMF__
export CONFIG_EMF_ENABLED := $(RTCONFIG_EMF)
endif

ifeq ($(RTCONFIG_HSPOT),y)
export CONFIG_HSPOT=y
#export CFLAGS += -D__CONFIG_HSPOT__
export CFLAGS += -DNAS_GTK_PER_STA
ifeq ($(or $(RTCONFIG_BCM7),$(RTCONFIG_BCM_7114)),y)
export CFLAGS += -DHSPOT_OSEN
endif
endif

ifeq ($(RTCONFIG_WPS),y)
ifeq ($(CONFIG_BCMWL5),y)
endif
export CFLAGS += -D__CONFIG_WPS__
# WFA WPS 2.0 Testbed extra caps
#export CFLAGS += -DWFA_WPS_20_TESTBED
endif

ifeq ($(RTCONFIG_NORTON),y)
export CFLAGS += -D__CONFIG_NORTON__
endif

ifeq ($(RTCONFIG_DEBUG),y)
export CFLAGS += -g
endif

ifneq (,$(filter y,$(RTCONFIG_SFP) $(RTCONFIG_4M_SFP)))
export  MUVER := _1.4
endif

#
#
#
SEP=echo "\033[41;1m   $@   \033[0m"

#
# standard packages
#
obj-$(RTCONFIG_NOTIFICATION_CENTER) += sqlite
obj-$(RTCONFIG_NOTIFICATION_CENTER) += nt_center

obj-y += busybox
obj-y += shared

obj-$(RTCONFIG_PROTECTION_SERVER) += protect_srv

obj-$(RTCONFIG_QTN) += libqcsapi_client
obj-$(RTCONFIG_QTN) += qtnimage
obj-$(RTCONFIG_DMALLOC) += dmalloc

obj-$(RTCONFIG_DHDAP) += dhd $(if $(RTCONFIG_BCM7),pciefd)
obj-$(RTCONFIG_BCM_7114) += dhd_monitor
obj-y += nvram$(BCMEX)
#obj-$(RTCONFIG_BCMARM) += ctf_arm
#obj-$(RTAC53U) += ctf_5358
#ifneq ($(RTCONFIG_RALINK)$(RTCONFIG_QCA),y)
#obj-y += ctf
#endif
ifeq ($(RTCONFIG_BCMARM),y)
ifeq ($(RTCONFIG_CFEZ),y)
obj-y += envram_bin
endif
endif
obj-$(RTCONFIG_DPSTA) += dpsta

ifeq ($(RTCONFIG_RALINK),y)
#################################################################################
# Ralink/MTK-specific packages
#################################################################################
obj-y += wireless_tools

obj-y += $(if $(RTN65U),rt2880_app,ated flash)
ifeq ($(RTN65U),)
obj-y += $(if $(or $(RTN14U),$(RTAC52U),$(RTAC51U),$(RTN11P),$(RTN300),$(RTN54U)$(RTAC1200HP),$(RTN56UB1),$(RTN56UB2),$(RTAC54U)),reg,hw_nat)
endif

else ifeq ($(RTCONFIG_QCA),y)
#################################################################################
# QCA-specific packages
#################################################################################
obj-y += wireless_tools
obj-y += libnl-tiny-0.1
obj-y += swconfig
ifeq ($(RTCONFIG_SOC_IPQ40XX),y)
SWCONFIG_PKG := qca-swconfig.ipq40xx
else
SWCONFIG_PKG := swconfig
endif

# QCA9558.ILQ 2.0
obj-$(RTCONFIG_SOC_QCA9557) += shortcut-fe
obj-$(QCA953X) += shortcut-fe
obj-$(QCA956X) += shortcut-fe
ifeq ($(SWITCH_CHIP),QCA8337N)
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-ssdk
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-ssdk30
endif
obj-$(or $(RTCONFIG_WIFI_QCA9557_QCA9882),$(QCA953X),$(QCA956X)) += qca-wifi
#obj-$(RTCONFIG_WIFI_QCA9557_QCA9882) += qca-art2
obj-$(or $(RTCONFIG_WIFI_QCA9557_QCA9882),$(QCA953X),$(QCA956X)) += qca-hostap

# IPQ8064.ILQ 1.3
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-wifi-qca9990
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-wifi-fw-qca9990
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qcmbr
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-hostap-qca9990
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-fw
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-drv
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-gmac
#obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-macsec	# need IPQ8066, IPQ8068
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-ecm
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-crypto
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-cfi	# need kmod-crypto-ocf
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-nss-clients
obj-$(RTCONFIG_WIFI_QCA9990_QCA9990) += qca-thermald

# IPQ8064.ILQ 3.0
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-wifi-qca9994
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-wifi-fw-qca9994
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qcmbr
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-hostap-qca9994 libnl-bf openssl
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += $(if $(RTCONFIG_SWITCH_QCA8337N),qca-mcs30-lkm)
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-fw
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-drv
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-gmac
#obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-macsec	# need IPQ8066, IPQ8068
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-ecm shortcut-fe30
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-crypto
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-cfi	# need kmod-crypto-ocf
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-nss30-clients
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-thermald
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += shortcut-fe30 libnl-bf
obj-$(RTCONFIG_WIFI_QCA9994_QCA9994) += qca-whc30-lbd libqcacommon

# IPQ8064.ILQ 1.3, IPQ8064.ILQ 3.0
obj-$(RTCONFIG_SOC_IPQ8064) += devmem2
obj-$(RTCONFIG_SOC_IPQ8064) += taskset

# IPQ4019 ILQ 1.1 
#obj-$(RTCONFIG_SOC_IPQ40XX) += qca-nss-drv.ipq40xx
#obj-$(RTCONFIG_SOC_IPQ40XX) += qca-nss-gmac.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-wifi.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-wifi-fw.ipq40xx
obj-$(RTCONFIG_PCIE_QCA9984) += qca-wifi-fw.qca9984
obj-$(RTCONFIG_PCIE_QCA9888) += qca-wifi-fw.qca9888
obj-$(RTCONFIG_SOC_IPQ40XX) += qcmbr.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-hostap.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-rfs.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += shortcut-fe.ipq40xx libnl-bf
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-ssdk.ipq40xx
obj-$(RTCONFIG_SOC_IPQ40XX) += qca-ssdk-shell.ipq40xx

obj-$(RTCONFIG_SOC_IPQ40XX) += ethtool-3.7

obj-$(RTCONFIG_HIVE_BLUEZ)  += qca-hyfi-qdisc.ipq40xx
obj-$(RTCONFIG_HIVE_BLUEZ)  += qca-hyfi-bridge.ipq40xx

ifeq ($(PLC_UTILS),y)
obj-y += plc-utils
endif

ifeq ($(ART2),y)
obj-y += LinuxART2
endif

else
#################################################################################
# Broadcom-specific packages
#################################################################################
ifneq ($(RTCONFIG_BCMARM),y)
obj-y += lzma-loader
endif

endif

ifeq ($(RTCONFIG_DSL),y)
obj-y += dsl_drv_tool
endif

ifeq ($(KPATH_2636_OR_3X),y)
IPTABLES := iptables-1.4.x
IPTC_LIBDIR := $(TOP)/$(IPTABLES)/libiptc/.libs
IPTC_LIBS := -lip4tc $(if $(RTCONFIG_IPV6),-lip6tc,) -liptc
obj-y += iptables-1.4.x
obj-y += iproute2-3.x
else
IPTABLES := iptables
IPTC_LIBDIR := $(TOP)/$(IPTABLES)
IPTC_LIBS := -liptc
obj-y += iptables
obj-y += iproute2
endif

obj-$(RTCONFIG_BWDPI) += sqlite
obj-$(RTCONFIG_BWDPI) += bwdpi
obj-$(RTCONFIG_BWDPI) += bwdpi_sqlite
obj-$(RTCONFIG_BWDPI) += bwdpi_bin
#obj-$(RTCONFIG_SPEEDTEST) += curl
obj-y += curl

obj-$(RTCONFIG_TRAFFIC_LIMITER) += sqlite
obj-$(RTCONFIG_TRAFFIC_LIMITER) += traffic_limiter

ifeq ($(RTCONFIG_BWDPI),y)
export DPI := _dpi
endif

obj-y += rc
obj-y += rom
obj-y += others
obj-y += httpd json-c
obj-y += www
obj-y += bridge
obj-y += dnsmasq
obj-$(RTCONFIG_DNSSEC) += nettle
obj-y += etc
# obj-y += vlan # use Busybox vconfig
#obj-y += utils
obj-y += ntpclient

ifneq ($(RTCONFIG_4M_SFP),y)
obj-y += rstats
obj-y += cstats
endif

ifeq ($(RTCONFIG_DSL),y)
obj-y += spectrum
endif

ifeq ($(RTCONFIG_RALINK),y)
obj-y += 802.1x
obj-y += libupnp-1.3.1
obj-y += wsc_upnp
endif

# !!TB - updated Broadcom Wireless driver
ifeq ($(RTCONFIG_RALINK),y)
else ifeq ($(RTCONFIG_QCA),y)
else
#obj-y += et
obj-y += utils$(BCMEX)$(EX7)
obj-$(RTCONFIG_EMF) += emf$(BCMEX)$(EX7)
obj-$(RTCONFIG_EMF) += igs$(BCMEX)$(EX7)
ifneq ($(RTCONFIG_BCM_794),y)	# do not build wl/dhd since not ready
obj-y += wlconf$(BCMEX)$(EX7)
obj-y += nas$(BCMEX)$(EX7)
obj-$(RTCONFIG_HSPOT) += hspot_ap$(BCMEX)$(EX7)
obj-y += eapd$(BCMEX)$(EX7)/linux
obj-$(RTCONFIG_WPS) += wps$(BCMEX)$(EX7)
obj-$(RTCONFIG_WLEXE) += wlexe
endif
obj-$(CONFIG_LIBBCM) += libbcm
obj-$(CONFIG_LIBUPNP) += libupnp$(BCMEX)$(EX7)
endif

obj-$(RTCONFIG_BCMEVENTD) += eventd
obj-$(RTCONFIG_BCMASPMD) += aspmd

obj-$(RTCONFIG_WLCLMLOAD) += clm

obj-y += libbcmcrypto
obj-$(RTCONFIG_HTTPS) += mssl
#obj-y += mdu
obj-$(RTCONFIG_TFTP) += tftp

# !!TB
ifeq ($(RTCONFIG_USB),y)
#obj-y += p910nd
obj-y += scsi-idle
obj-y += sd-idle
obj-y += libusb10
obj-y += libusb
obj-y += libusb-0.1.12
ifeq ($(RTCONFIG_USB),y)
ifneq ($(RTCONFIG_8M_SFP),y)
obj-y += hub-ctrl
endif
endif
obj-$(RTCONFIG_USB_PRINTER) += u2ec
obj-$(RTCONFIG_USB_PRINTER) += LPRng

ifeq ($(RTCONFIG_INTERNAL_GOBI),y)
obj-$(RTCONFIG_INTERNAL_GOBI) += usb-gobi
else
obj-$(RTCONFIG_USB_MODEM) += sdparm-1.02
obj-$(RTCONFIG_USB_MODEM) += comgt-0.32
obj-$(RTCONFIG_USB_MODEM) += $(if $(RTCONFIG_QCA),,uqmi)
endif

ifeq ($(KPATH_3X),y)
obj-$(RTCONFIG_USB_MODEM) += usb-modeswitch-1.2.3
else ifeq ($(KPATH_2636_OR_3X),y)
obj-$(RTCONFIG_USB_MODEM) += $(if $(or $(RTN56UB1),$(RTN56UB2)),usbmodeswitch-1.2.3,usb-modeswitch)
else
ifeq ($(RTCONFIG_BCMWL6),y)
obj-$(RTCONFIG_USB_MODEM) += usb-modeswitch
else
obj-$(RTCONFIG_USB_MODEM) += usb-modeswitch-1.2.3
endif
endif

# Remove all possible usbmodeswitch from obj-y for 4G-AC55U.
# We may need to remove usbmodeswitch from obj-y for all LTE router.
ifeq ($(RTCONFIG_INTERNAL_GOBI),y)
obj-y := $(filter-out usbmodeswitch% usb-modeswitch%, $(obj-y))
endif

obj-$(RTCONFIG_USB_BECEEM) += madwimax-0.1.1 # for Samsung WiMAX
obj-$(RTCONFIG_USB_BECEEM) += openssl-1.0.0q # for Beceem, GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += Beceem_BCMS250$(BCMEX) # for Beceem WiMAX
obj-$(RTCONFIG_USB_BECEEM) += wpa_supplicant-0.7.3 # for GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += zlib # for GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += gctwimax-0.0.3rc4 # for GCT WiMAX

obj-$(RTCONFIG_USB_SMS_MODEM) += libiconv-1.14
obj-$(RTCONFIG_USB_SMS_MODEM) += smspdu

obj-y += libdisk

obj-y += e2fsprogs
obj-$(RTCONFIG_NFS) += nfs-utils
obj-$(RTCONFIG_NFS) += portmap

#obj-y += dosfstools
#obj-$(RTCONFIG_FTP) += $(if $(RTCONFIG_QCA),vsftpd-3.x,vsftpd)
obj-$(RTCONFIG_FTP) += vsftpd-3.x

ifeq ($(CONFIG_LINUX26),y)
ifeq ($(RTCONFIG_SAMBASRV),y)
NEED_EX_NLS = y
endif
ifeq ($(RTCONFIG_USB_EXTRAS),y)
NEED_EX_USB = y
endif
endif

ifeq ($(RTCONFIG_SAMBASRV),y)
NEED_SAMBA3 = y
endif
endif

ifeq ($(RTCONFIG_IPV6),y)
export RTCONFIG_IPV6 := y
else
RTCONFIG_IPV6 :=
endif

# install before samba3(.6.x) for not overwriting some binaries
obj-$(RTCONFIG_WEBDAV) += samba-3.5.8

ifeq ($(CONFIG_BCMWL5),y)
ifneq ($(RTCONFIG_8M_SFP),y)
obj-$(NEED_SAMBA3) += $(if $(RTCONFIG_SAMBA36X),samba-3.6.x,samba-3.0.33)
else
obj-$(NEED_SAMBA3) += $(if $(RTCONFIG_SAMBA36X),samba-3.6.x,samba3)
endif
else
obj-$(NEED_SAMBA3) += $(if $(RTCONFIG_SAMBA36X),samba-3.6.x,samba3)
endif
obj-$(RTCONFIG_SAMBA36X) += libiconv-1.14

ifeq ($(RTCONFIG_NTFS),y)
obj-$(RTCONFIG_OPEN_NTFS3G) += ntfs-3g
obj-$(RTCONFIG_PARAGON_NTFS) += ufsd
obj-$(RTCONFIG_TUXERA_NTFS) += tuxera
endif
ifeq ($(RTCONFIG_HFS),y)
obj-$(RTCONFIG_PARAGON_HFS) += ufsd
obj-$(RTCONFIG_TUXERA_HFS) += tuxera
#obj-$(RTCONFIG_KERNEL_HFSPLUS) += diskdev_cmds-332.14
endif
obj-$(RTCONFIG_EBTABLES) += ebtables
obj-$(RTCONFIG_IPV6) += odhcp6c
obj-$(RTCONFIG_6RELAYD) += 6relayd
#obj-$(RTCONFIG_IPV6) += ecmh

obj-$(RTCONFIG_MEDIA_SERVER) += zlib
obj-$(RTCONFIG_MEDIA_SERVER) += sqlite
obj-$(RTCONFIG_MEDIA_SERVER) += ffmpeg
obj-$(RTCONFIG_MEDIA_SERVER) += libogg
obj-$(RTCONFIG_MEDIA_SERVER) += flac
obj-$(RTCONFIG_MEDIA_SERVER) += jpeg
obj-$(RTCONFIG_MEDIA_SERVER) += libexif
obj-$(RTCONFIG_MEDIA_SERVER) += libid3tag
obj-$(RTCONFIG_MEDIA_SERVER) += libvorbis
obj-$(RTCONFIG_MEDIA_SERVER) += minidlna
obj-$(RTCONFIG_MEDIA_SERVER) += libgdbm
obj-$(RTCONFIG_MEDIA_SERVER) += mt-daapd

#MEDIA_SERVER_STATIC=y

#add by gauss for cloudsync
obj-$(RTCONFIG_CLOUDSYNC) += openssl
obj-$(RTCONFIG_CLOUDSYNC) += libxml2
obj-$(RTCONFIG_CLOUDSYNC) += curl
obj-$(RTCONFIG_CLOUDSYNC) += asuswebstorage$(BCMEX)
obj-$(RTCONFIG_CLOUDSYNC) += inotify

obj-$(RTCONFIG_CLOUDSYNC) += zlib # for neon
obj-$(RTCONFIG_CLOUDSYNC) += neon
obj-$(RTCONFIG_CLOUDSYNC) += webdav_client$(BCMEX)
obj-$(RTCONFIG_DROPBOXCLIENT) += dropbox_client

obj-$(RTCONFIG_FTPCLIENT) += libiconv-1.14
obj-$(RTCONFIG_FTPCLIENT) += ftpclient
obj-$(RTCONFIG_SAMBACLIENT) += sambaclient
obj-$(RTCONFIG_USBCLIENT) += usbclient

#aaews,asusnatnl related
obj-$(RTCONFIG_TUNNEL) += wb
obj-$(RTCONFIG_TUNNEL) += aaews
obj-$(RTCONFIG_TUNNEL) += mastiff
obj-$(RTCONFIG_TUNNEL) += asusnatnl


#For timemachine
obj-$(RTCONFIG_TIMEMACHINE) += openssl
obj-$(RTCONFIG_TIMEMACHINE) += libgpg-error-1.10
obj-$(RTCONFIG_TIMEMACHINE) += libgcrypt-1.5.1
obj-$(RTCONFIG_TIMEMACHINE) += db-4.8.30
obj-$(RTCONFIG_TIMEMACHINE) += netatalk-3.0.5
ifeq ($(or $(RTCONFIG_TIMEMACHINE),$(RTCONFIG_MDNS)),y)
obj-y += libdaemon
obj-y += avahi-0.6.31
obj-y += expat-2.0.1
else
ifeq ($(or $(RTCONFIG_MEDIA_SERVER),$(RTCONFIG_BWDPI)),y)
obj-y += mDNSResponder
endif
endif

obj-$(RTCONFIG_FBWIFI) += fb_wifi
obj-$(RTCONFIG_FBWIFI) += httpd_uam

#add for snmpd
SNMPD_CFLAGS += -D$(BUILD_NAME)
ifeq ($(BUILD_NAME), $(filter $(BUILD_NAME), RT-N66U RT-AC66U RT-AC68U))
SNMPD_CFLAGS += -DDUAL_BAND
endif
obj-$(RTCONFIG_SNMPD) += openssl
#obj-$(RTCONFIG_SNMPD) += libnmp
obj-$(RTCONFIG_SNMPD) += net-snmp-5.7.2
ifeq ($(RTCONFIG_SNMPD),y)
export MIB_MODULE_PASSPOINT =
endif

#for chillispot
obj-$(RTCONFIG_CHILLISPOT) += chillispot
obj-$(RTCONFIG_COOVACHILLI) += coovachilli
obj-$(RTCONFIG_FREERADIUS) += talloc
obj-$(RTCONFIG_FREERADIUS) += sqlite
obj-$(RTCONFIG_FREERADIUS) += freeradius

#
# configurable packages
#
RTCONFIG_PPP_BASE = y
obj-$(RTCONFIG_PPP_BASE) += pppd rp-pppoe
obj-$(RTCONFIG_L2TP) += rp-l2tp
obj-$(RTCONFIG_PPTP) += accel-pptp
obj-$(RTCONFIG_EAPOL) += wpa_supplicant
obj-$(RTCONFIG_TELENET) += lanauth
obj-$(RTCONFIG_HTTPS) += openssl
obj-$(RTCONFIG_HTTPS) += wget zlib
obj-$(RTCONFIG_SSH) += dropbear
obj-$(RTCONFIG_WTFAST) += wtfast
#obj-$(RTCONFIG_ZEBRA) += zebra
obj-$(RTCONFIG_QUAGGA) += quagga
#	obj-$(RTCONFIG_IPP2P) += ipp2p
obj-$(RTCONFIG_LZO) += lzo
obj-$(RTCONFIG_OPENVPN) += openpam
obj-$(RTCONFIG_OPENVPN) += openvpn

obj-$(CONFIG_LINUX26) += hotplug2
obj-$(CONFIG_LINUX26) += udev

obj-$(RTCONFIG_ACCEL_PPTPD) += accel-pptpd
obj-$(RTCONFIG_PPTPD) += pptpd

obj-$(RTCONFIG_WEBDAV) += sqlite
obj-$(RTCONFIG_WEBDAV) += libxml2
obj-$(RTCONFIG_WEBDAV) += pcre-8.31
obj-$(RTCONFIG_WEBDAV) += lighttpd-1.4.39
obj-$(RTCONFIG_WEBDAV) += libexif

obj-$(RTCONFIG_IXIAEP) += ixia_endpoint$(BCMEX)

# Add for ASUS Components
obj-$(RTCONFIG_UPNPC) += miniupnpc
obj-y += miniupnpd$(MUVER)
obj-y += igmpproxy
obj-y += snooper
ifeq ($(RTCONFIG_BCMWL6),y)
ifneq ($(RTCONFIG_BCMARM),y)
obj-y += netconf
obj-y += igmp
endif
endif
obj-y += udpxy
ifeq ($(CONFIG_BCMWL5),y)
ifeq ($(RTCONFIG_BCMARM),y)
obj-y += lltd.arm
obj-y += taskset
obj-$(RTCONFIG_COMA) += comad
else
obj-y += lltd
endif
ifeq ($(RTCONFIG_BCMWL6),y)
obj-y += acsd$(BCMEX)$(EX7)
endif
else
obj-y += lldt
endif
obj-$(RTCONFIG_JFFS2USERICON) += lltdc

obj-$(RTCONFIG_TOAD) += toad
ifeq ($(RTCONFIG_BCMARM),y)
ifneq ($(RTCONFIG_QTN),y)
ifeq ($(RTCONFIG_BCM_7114),y)
obj-$(RTCONFIG_BCMBSD) += bsd_arm_7114
else
obj-$(RTCONFIG_BCMBSD) += bsd
endif
endif
endif
obj-$(RTCONFIG_BCMSSD) += ssd
obj-y += networkmap
obj-y += infosvr
obj-y += ez-ipupdate
ifneq ($(or $(RTN56U),$(DSLN55U)),y)
obj-y += phddns
endif

obj-$(RTCONFIG_STRACE) += strace

obj-$(RTCONFIG_LLDP) += openlldp

obj-$(RTCONFIG_IPERF) += iperf

obj-$(RTCONFIG_PUSH_EMAIL) += zlib # for libxml2, curl.
obj-$(RTCONFIG_PUSH_EMAIL) += openssl
obj-$(RTCONFIG_PUSH_EMAIL) += libxml2
obj-$(RTCONFIG_PUSH_EMAIL) += curl
obj-$(RTCONFIG_PUSH_EMAIL) += wb
obj-$(RTCONFIG_PUSH_EMAIL) += email-3.1.3
obj-$(RTCONFIG_PUSH_EMAIL) += push_log

obj-$(RTCONFIG_NORTON) += norton${BCMEX}

#obj-$(UBI) += mtd-utils
#obj-$(UBIFS) += mtd-utils
#obj-$(UBIFS) += util-linux lzo zlib
ifeq ($(RTCONFIG_RALINK),y)
obj-$(RA_SKU) += ra_SingleSKU
endif
obj-$(RTCONFIG_BCMARM) += wl$(BCMEX)$(EX7)
obj-$(RTCONFIG_BCMARM) += et_arm$(EX7)

obj-$(RTCONFIG_EXT_RTL8365MB) += rtl_bin
obj-$(RTCONFIG_EXT_RTL8370MB) += rtl_bin
obj-$(RTCONFIG_LACP) += lacp

#obj-y += bonnie

ifeq ($(RTN65U), y)
obj-y += asm1042
endif

ifeq ($(RTCONFIG_BCMARM),y)
obj-prelibs =$(filter busybox% nvram$(BCMEX) libbcmcrypto shared netconf libupnp$(BCMEX)$(EX7) libz libbcm pciefd, $(obj-y))
obj-postlibs := $(filter-out $(obj-prelibs), $(obj-y))
endif

obj-$(RTCONFIG_OPENSWAN) += $(if $(RTCONFIG_WIFI_QCA9994_QCA9994),openswan-nss gmp,)
obj-$(RTCONFIG_QUICKSEC) += quicksec-6.0-fips
obj-$(RTCONFIG_STRONGSWAN) += strongswan-5.2.1

ifneq ($(RTCONFIG_4M_SFP),y)
obj-y += netstat-nat
endif

obj-$(RTCONFIG_TCPDUMP) += libpcap
obj-$(RTCONFIG_TCPDUMP) += tcpdump-4.4.0

# Merlin addons
ifneq ($(or $(findstring linux-2.6.36,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR))),)
obj-y += libnfnetlink
obj-y += libmnl
obj-y += ipset_arm
else
obj-y += ipset
endif
obj-$(RTCONFIG_NANO) += ncurses
obj-$(RTCONFIG_NANO) += nano

obj-$(RTCONFIG_BLINK_LED) += bled
obj-$(RTCONFIG_BONDING) += ifenslave

obj-$(RTCONFIG_GEOIP) += GeoIP-1.6.2

obj-$(RTCONFIG_TRANSMISSION) += Transmission curl libevent-2.0.21

ifeq ($(RTCONFIG_TOR),y)
obj-y += openssl
obj-y += zlib
obj-y += libevent-2.0.21
obj-y += tor
endif

#For TR-069 agent
obj-$(RTCONFIG_TR069) += openssl
obj-$(RTCONFIG_TR069) += tr069

obj-$(RTCONFIG_CLOUDCHECK) += cloudcheck

obj-$(RTCONFIG_GETREALIP) += ministun
obj-$(RTCONFIG_OPENVPN) += ministun

ifneq ($(find lighttpd-1.4.39, $(obj-y)),"")
released_dir += APP-IPK
endif

obj-clean := $(foreach obj, $(obj-y) $(obj-n) $(obj-), $(obj)-clean)
obj-install := $(foreach obj,$(obj-y),$(obj)-install)

MKSQUASHFS_TARGET = mksquashfs
MKSQUASHFS = $(MKSQUASHFS_TARGET)

LINUX_ARCH_ASM_INCL_DIR = $(if ($KPATH_3X),$(LINUXDIR)/arch/mips/include/asm,$(LINUXDIR)/include/asm-mips)

#
# Basic rules
#

all: clean-build kernel_header libc version obj_prelibs kernel $(obj-y)

ifeq ($(RTCONFIG_BCMARM),y)
version:  $(SRCBASE)/include/epivers.h

$(SRCBASE)/include/epivers.h:
	$(MAKE) -C $(SRCBASE)/include
ifeq ($(RTCONFIG_DHDAP),y)
	$(MAKE) -C $(SRCBASE_DHD)/include
	$(MAKE) -C $(SRCBASE_FW)/include
endif
endif

ifeq ($(RTCONFIG_BCMARM),y)
	+$(MAKE) $(MAKE_ARGS) post_preplibs
endif

ifeq ($(RTCONFIG_BCMARM),y)
obj_prelibs:
	+$(MAKE) parallel=true $(MAKE_ARGS) $(obj-prelibs)

obj_postlibs:
	+$(MAKE) parallel=true $(MAKE_ARGS) $(obj-postlibs)

post_preplibs:  obj_postlibs
else
obj_prelibs: dummy
endif

ifneq ($(RTCONFIG_BCMARM),y)
kernel_header: $(LINUXDIR)/.config
	$(MAKE) -C $(LINUXDIR) ARCH=$(ARCH) headers_install INSTALL_HDR_PATH=$(TOP)/$@
ifeq ($(RTCONFIG_RALINK),y)
	@if [ -e $(LINUX_ARCH_ASM_INCL_DIR)/rt2880/rt_mmap.h ] ; then \
		mkdir -p $@/include/asm/rt2880 ; \
		cp $(LINUX_ARCH_ASM_INCL_DIR)/rt2880/rt_mmap.h $@/include/asm/rt2880/ ; \
	fi
endif
ifeq ($(RTCONFIG_QCA),y)
ifeq ($(CONFIG_LINUX3_14),y)
	cp -fr $(LINUXDIR)/include/uapi/* $@/include/uapi/
else
	cp $(LINUXDIR)/include/net/switch.h $@/include/linux/
endif
endif
	$(MAKE) -C $(LINUXDIR) prepare scripts
	find $(LINUXDIR)/include -name autoconf.h -exec cp {} $@/include/linux/ \;
	cp $(LINUXDIR)/Makefile $@/
else
kernel_header: $(LINUXDIR)/.config

endif

kernel: $(LINUXDIR)/.config
	$(SEP)

ifneq ($(RTCONFIG_BCMARM),y)
ifeq ($(RTCONFIG_RALINK),y)
	@if ! grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
		$(MAKE) -C $(LINUXDIR) vmlinux CC=$(KERNELCC) LD=$(KERNELLD) $(PARALLEL_BUILD); \
	fi
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
		$(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) LD=$(KERNELLD) $(PARALLEL_BUILD); \
	fi
else ifeq ($(RTCONFIG_QCA),y)
	$(MAKE) -C $(LINUXDIR) vmlinux CC=$(KERNELCC) LD=$(KERNELLD)
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) LD=$(KERNELLD); \
	fi
ifeq ($(CONFIG_LINUX3_14),y)
	$(MAKE) -C $(LINUXDIR) dtbs
endif
else
	# Broadcrom MIPS SoC
	@if ! grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
		$(MAKE) -C $(LINUXDIR) zImage CC=$(KERNELCC) $(PARALLEL_BUILD); \
	fi
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
		$(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) $(PARALLEL_BUILD); \
	fi

ifeq ($(CONFIG_LINUX26),y)
	$(MAKE) -C $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed srctree=$(LINUXDIR) $(PARALLEL_BUILD)
endif
endif
else	# RTCONFIG_BCMARM
	(echo '.NOTPARALLEL:' ; cat ${LINUXDIR}/Makefile) |\
		$(MAKE) -C ${LINUXDIR} -f - $(SUBMAKE_SETTINGS) zImage
	+$(MAKE) CONFIG_SQUASHFS=$(CONFIG_SQUASHFS) -C $(SRCBASE)/router/compressed ARCH=$(ARCH)

	$(if $(shell grep "CONFIG_MODULES=y" ${LINUXDIR}/.config), \
	(echo '.NOTPARALLEL:' ; cat ${LINUXDIR}/Makefile) | $(MAKE) -C ${LINUXDIR} -f - $(SUBMAKE_SETTINGS) MFG_WAR=1 zImage ; \
	(echo '.NOTPARALLEL:' ; cat ${LINUXDIR}/Makefile) | $(MAKE) -C ${LINUXDIR} -f - ARCH=$(ARCH) modules)
	# Preserve the debug versions of these and strip for release
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/vmlinux)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/wl/wl.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/et/et.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/ctf/ctf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/dhd/dhd.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/bcm57xx/bcm57xx.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/emf/emf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/igs/igs.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/dpsta/dpsta.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/connector/cn.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/scsi/scsi_wait_scan.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/usb/host/xhci-hcd.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/usb/host/ehci-hcd.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/usb/host/ohci-hcd.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/lib/libcrc32c.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/net/sched/sch_tbf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/net/sched/sch_hfsc.ko)
ifeq ($(RTCONFIG_EXT_RTL8365MB),y)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/char/rtl8365mb/rtl8365mb.ko)
endif
ifeq ($(RTCONFIG_RTL8370MB),y)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/char/rtl8365mb/rtl8365mb.ko)
endif
endif   # RTCONFIG_BCMARM

ifeq ($(RTCONFIG_RALINK),y)
else ifeq ($(RTCONFIG_QCA),y)
else
ifneq ($(RTCONFIG_BCMARM),y)
lzma-loader:
	@[ ! -d $(SRCBASE)/lzma-loader ] || \
		$(MAKE) -C $(SRCBASE)/lzma-loader CROSS_COMPILE=$(CROSS_COMPILE) LD=$(LD)

lzma-loader-clean:
	@[ ! -d $(SRCBASE)/lzma-loader ] || \
		$(MAKE) -C $(SRCBASE)/lzma-loader clean

lzma-loader-install: lzma-loader
	@$(SEP)
endif
endif

kmod: dummy
	$(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) $(PARALLEL_BUILD)

testfind:
	cd $(TARGETDIR)/lib/modules/* && find -name "*.o" -exec mv -i {} . \; || true
	cd $(TARGETDIR)/lib/modules/* && find -type d -delete || true

install package: $(obj-install) $(LINUXDIR)/.config gen_target gen_gpl_excludes_router

gen_target:
	@$(SEP)

	install -d $(TARGETDIR)


# kernel modules
	$(MAKE) -C $(LINUXDIR) modules_install DEPMOD=/bin/true INSTALL_MOD_PATH=$(TARGETDIR)
	@if grep -q "CONFIG_RT3352_INIC_AP=y" $(LINUXDIR)/.config ; then \
		install -d $(TARGETDIR)/iNIC_RT3352/ ; \
		install -D $(LINUXDIR)/drivers/net/wireless/iNIC_RT3352/firmware/mii/iNIC_ap.bin $(TARGETDIR)/iNIC_RT3352/ ; \
	else \
		rm -rf $(TARGETDIR)/iNIC_RT3352/ ; \
	fi
##!!TB	find $(TARGETDIR)/lib/modules -name *.o -exec mipsel-linux-strip --strip-unneeded {} \;
	find $(TARGETDIR)/lib/modules -name *.o -exec $(STRIP) --strip-debug -x -R .comment -R .pdr -R .mdebug.abi32 -R .note.gnu.build-id -R .gnu.attributes -R .reginfo {} \;
	find $(TARGETDIR)/lib/modules -name *.ko -exec $(STRIP) --strip-debug -x -R .comment -R .pdr -R .mdebug.abi32 -R .note.gnu.build-id -R .gnu.attributes -R .reginfo {} \;
#	-cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv diag/* . && rm -rf diag

# nice and clean
ifeq ($(RTCONFIG_RALINK)$(RTCONFIG_QCA),y)
	-cd $(TARGETDIR)/lib/modules/*/kernel/drivers/ && mv nvram$(BCMEX)/* . && rm -rf nvram$(BCMEX) || true
else
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv et.4702/* . && rm -rf et.4702 || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv et/* . && rm -rf et || true
ifeq ($(RTCONFIG_BRCM_USBAP),y)
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv wl/wl_high.ko . || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv wl/wl_high/wl_high.ko . || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv wl/wl_sta/wl_high.ko . || true
endif
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv wl/wl.ko . && rm -rf wl || true
endif
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv cifs/* . && rm -rf cifs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv jffs2/* . && rm -rf jffs2 || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv jffs/* . && rm -rf jffs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/lib && mv zlib_inflate/* . && rm -rf zlib_inflate || true
	cd $(TARGETDIR)/lib/modules/*/kernel/lib && mv zlib_deflate/* . && rm -rf zlib_deflate || true
	cd $(TARGETDIR)/lib/modules/*/kernel/lib && mv lzo/* . && rm -rf lzo || true
	rm -rf $(TARGETDIR)/lib/modules/*/pcmcia

##!!TB
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv ext2/* . && rm -rf ext2 || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv ext3/* . && rm -rf ext3 || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv jbd/* . && rm -rf jbd || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv fat/* . && rm -rf fat || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv vfat/* . && rm -rf vfat || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv msdos/* . && rm -rf msdos || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv fuse/* . && rm -rf fuse || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv ntfs/* . && rm -rf ntfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv smbfs/* . && rm -rf smbfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv reiserfs/* . && rm -rf reiserfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv hfsplus/* . && rm -rf hfsplus || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv lockd/* . && rm -rf lockd || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv nfsd/* . && rm -rf nfsd || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv nfs/* . && rm -rf nfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv xfs/* . && rm -rf xfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv nls/* . && rm -rf nls || true
	cd $(TARGETDIR)/lib/modules/*/kernel/fs && mv exportfs/* . && rm -rf exportfs || true
	cd $(TARGETDIR)/lib/modules/*/kernel/net && mv sunrpc/* . && rm -rf sunrpc || true
	cd $(TARGETDIR)/lib/modules/*/kernel/net && mv auth_gss/* . && rm -rf auth_gss || true
	cd $(TARGETDIR)/lib/modules/*/kernel/sound/core && mv oss/* . && rm -rf oss || true
	cd $(TARGETDIR)/lib/modules/*/kernel/sound/core && mv seq/* . && rm -rf seq || true
	cd $(TARGETDIR)/lib/modules/*/kernel/sound && mv core/* . && rm -rf core || true
	cd $(TARGETDIR)/lib/modules/*/kernel/sound && mv usb/* . && rm -rf usb || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv hcd/* . && rm -rf hcd || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv host/* . && rm -rf host || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv storage/* . && rm -rf storage || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv serial/* . && rm -rf serial || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv core/* . && rm -rf core || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv class/* . && rm -rf class || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/usb && mv misc/* . && rm -rf misc || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/hid && mv usbhid/* . && rm -rf usbhid || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/input && mv joystick/* . && rm -rf joystick || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/input && mv keyboard/* . && rm -rf keyboard || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/input && mv misc/* . && rm -rf misc || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/input && mv mouse/* . && rm -rf mouse || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/media/video && mv uvc/* . && rm -rf uvc || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/media && mv video/* . && rm -rf video || true

	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv bcm57xx/* . && rm -rf bcm57xx || true

	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv emf/* . && rm -rf emf || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv igs/* . && rm -rf igs || true
ifeq ($(RTAC53U),y)
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv ctf_5358/* . && rm -rf ctf_5358 || true
else
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv ctf/* . && rm -rf ctf || true
endif
ifeq ($(RTCONFIG_EXT_RTL8365MB),y)
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/char && mv rtl8365mb/* . && rm -rf rtl8365mb || true
endif
ifeq ($(RTCONFIG_RTL8370MB),y)
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/char && mv rtl8365mb/* . && rm -rf rtl8365mb || true
endif
	cd $(TARGETDIR)/lib/modules && rm -f */source || true

# misc
	for dir in $(wildcard $(patsubst %,$(INSTALLDIR)/%,$(obj-y))) ; do \
		(cd $${dir} && tar cpf - .) | (cd $(TARGETDIR) && tar xpf -) \
	done

ifneq ($(RTCONFIG_L7),y)
	rm -f $(TARGETDIR)/usr/lib/iptables/libipt_layer7.so
endif
ifneq ($(RTCONFIG_SNMPD),y)
	rm -f $(TARGETDIR)/usr/lib/libnmp.so
endif

# uClibc
	install $(LIBDIR)/ld-uClibc.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libcrypt.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libpthread.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libgcc_s.so.1 $(TARGETDIR)/lib/
	$(STRIP) $(TARGETDIR)/lib/libgcc_s.so.1
	install $(LIBDIR)/libc.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libdl.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libm.so.0 $(TARGETDIR)/lib/
	install $(LIBDIR)/libnsl.so.0 $(TARGETDIR)/lib/
ifeq ($(RTCONFIG_SSH),y)
	install $(LIBDIR)/libutil.so.0 $(TARGETDIR)/lib/
endif
ifneq ($(RTCONFIG_OPTIMIZE_SHARED_LIBS),y)
	install $(LIBDIR)/libresolv.so.0 $(TARGETDIR)/lib/
	$(STRIP) $(TARGETDIR)/lib/*.so.0
endif

	@cd $(TARGETDIR) && $(TOP)/others/rootprep${BCMEX}.sh
	@echo ---

ifeq ($(RTCONFIG_QCA),y)
	#FIXME
else
ifeq ($(RTCONFIG_OPTIMIZE_SHARED_LIBS),y)
ifneq ($(RTCONFIG_BCMARM),y)
	@$(SRCBASE)/btools/libfoo.pl
endif
else
	@$(SRCBASE)/btools/libfoo.pl --noopt
endif
endif

	@chmod 0555 $(TARGETDIR)/lib/*.so*
	@chmod 0555 $(TARGETDIR)/usr/lib/*.so*

# !!TB - moved to run after libfoo.pl - to make sure shared libs include all symbols needed by extras
# separated/copied extra stuff
	@rm -rf $(PLATFORMDIR)/extras
	@mkdir $(PLATFORMDIR)/extras
	@mkdir $(PLATFORM)/extras/bluetooth
	@mkdir $(PLATFORM)/extras/bluetooth/net
	@mkdir $(PLATFORM)/extras/bluetooth/drivers
	@mv $(TARGETDIR)/lib/modules/*/kernel/net/ipv4/ip_gre.*o $(PLATFORMDIR)/extras/ || true

	$(if $(RTCONFIG_OPENVPN),@cp -f,$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv)) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/tun.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_EBTABLES),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/net/bridge/netfilter/ebt*.*o $(PLATFORMDIR)/extras/ || true

	@cp $(TARGETDIR)/lib/modules/*/kernel/net/ipv4/netfilter/ip_set*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/net/ifb.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/net/sched/sch_red.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/fs/ntfs.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/fs/smbfs.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/fs/reiserfs.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/fs/hfsplus.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_NFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nfs.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_NFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nfsd.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_NFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/lockd.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_NFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/exportfs.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_NFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/net/sunrpc.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/net/auth_rpcgss.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/net/rpcsec_gss_krb5.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/fs/xfs.*o $(PLATFORMDIR)/extras/ || true
#	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/scsi/sr_mod.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/scanner.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/bluetooth $(PLATFORMDIR)/extras/bluetooth/drivers/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/net/bluetooth $(PLATFORMDIR)/extras/bluetooth/net/ || true

	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/usbserial.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/option.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/*acm.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/cdc-wdm.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/usbnet.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/asix.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_ether.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/rndis_host.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_ncm.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/qmi_wwan.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_mbim.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -rf,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/hso* $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/ipheth.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/sierra.*o $(PLATFORMDIR)/extras/ || true

	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/usbkbd.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/usbmouse.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/hid*.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/ipw.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/audio.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/ov51*.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/pwc*.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/emi*.*o $(PLATFORMDIR)/extras/ || true
	@cp -f $(TARGETDIR)/lib/modules/*/kernel/drivers/net/mii.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/media/* $(PLATFORMDIR)/extras/ || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/media || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/sound/* $(PLATFORMDIR)/extras/ || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/sound || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/sound/* $(PLATFORMDIR)/extras/ || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/sound || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/input/* $(PLATFORMDIR)/extras/ || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/input || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/hid/* $(PLATFORMDIR)/extras/ || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/hid || true
	@cp -f $(TARGETDIR)/lib/modules/*/kernel/drivers/net/bcm57*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_PPTP),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/pptp.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_L2TP),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/pppol2tp.*o $(PLATFORMDIR)/extras/ || true
	@mv $(TARGETDIR)/lib/modules/*/kernel/drivers/net/ppp_deflate.*o  $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_IPSEC),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/crypto/* $(PLATFORMDIR)/extras/ || true
	[ ! -f $(TARGETDIR)/lib/modules/*/kernel/crypto/* ] && rm -rf $(TARGETDIR)/lib/modules/*/kernel/crypto || true

	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_cp9*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_cp1251.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_euc-jp.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_sjis.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_gb2312.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_euc-kr.*o $(PLATFORMDIR)/extras/ || true
	$(if $(NEED_EX_NLS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_big5.*o $(PLATFORMDIR)/extras/ || true

	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/nls_*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/usb/*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/cdrom/*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/scsi/*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/ext2.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/ext3.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/jbd.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/mbcache.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/fat.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/vfat.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/msdos.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/fuse.*o $(PLATFORMDIR)/extras/ || true
ifneq ($(RTCONFIG_USB),y)
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/usb || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/scsi || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb || true
endif

	$(if $(RTCONFIG_USB_EXTRAS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/connector/cn.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_EXTRAS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/block/loop.*o $(PLATFORMDIR)/extras/ || true
ifneq ($(RTCONFIG_USB_EXTRAS),y)
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/connector || true
	@rm -rf $(TARGETDIR)/lib/modules/*/kernel/drivers/block || true
endif
#	$(if $(RTCONFIG_CIFS),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/cifs.*o $(PLATFORMDIR)/extras/ || true
	@cp -f $(TARGETDIR)/lib/modules/*/kernel/fs/cifs.*o $(PLATFORMDIR)/extras/ || true
	$(if $(or $(RTCONFIG_BRCM_NAND_JFFS2),$(RTCONFIG_JFFS2)),$(if $(RTCONFIG_JFFSV1),@mv,@cp -f),@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/jffs2.*o $(PLATFORMDIR)/extras/ || true
	$(if $(or $(RTCONFIG_BRCM_NAND_JFFS2),$(RTCONFIG_JFFS2)),$(if $(RTCONFIG_JFFSV1),@mv,@cp -f),@mv) $(TARGETDIR)/lib/modules/*/kernel/lib/zlib_*.*o $(PLATFORMDIR)/extras/ || true
	$(if $(or $(RTCONFIG_BRCM_NAND_JFFS2),$(RTCONFIG_JFFS2)),$(if $(RTCONFIG_JFFSV1),@cp -f,@mv),@mv) $(TARGETDIR)/lib/modules/*/kernel/fs/jffs.*o $(PLATFORMDIR)/extras/ || true
	[ ! -f $(TARGETDIR)/lib/modules/*/kernel/lib/* ] && rm -rf $(TARGETDIR)/lib/modules/*/kernel/lib || true
	$(if $(RTCONFIG_L7),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/net/ipv4/netfilter/ipt_layer7.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_L7),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/net/netfilter/xt_layer7.*o $(PLATFORMDIR)/extras/ || true

	@mkdir -p $(TARGETDIR)/asus_jffs
	@mkdir -p $(PLATFORMDIR)/extras/apps
	@mkdir -p $(PLATFORMDIR)/extras/lib

	@mv $(TARGETDIR)/usr/sbin/ttcp $(PLATFORMDIR)/extras/apps/ || true
	@cp -f $(TARGETDIR)/usr/sbin/mii-tool $(PLATFORMDIR)/extras/apps/ || true
	$(if $(RTCONFIG_RALINK),@mv,@cp -f)  $(TARGETDIR)/usr/sbin/robocfg $(PLATFORMDIR)/extras/apps/ || true

	$(if $(NEED_EX_USB),@cp -f,@mv) $(TARGETDIR)/usr/lib/libusb* $(PLATFORMDIR)/extras/lib/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/usr/sbin/chat $(PLATFORMDIR)/extras/apps/ || true

	@mkdir -p $(TARGETDIR)/rom/etc/l7-protocols
ifeq ($(RTCONFIG_L7PAT),y)
	@cd layer7 && ./squish.sh
	cp layer7/squished/*.pat $(TARGETDIR)/rom/etc/l7-protocols
endif

	busybox/examples/depmod.pl -k $(LINUXDIR)/vmlinux -b $(TARGETDIR)/lib/modules/*/

ifeq ($(RTCONFIG_ROMCFE),y)
	-cp $(SRCBASE)/cfe/cfe_`echo $(BUILD_NAME)|tr A-Z a-z`.bin $(TARGETDIR)/rom/cfe
endif
ifeq ($(RTCONFIG_ROMATECCODEFIX),y)
	-cp $(SRCBASE)/router/rom/cfe/`echo $(BUILD_NAME)|tr A-Z a-z`_fixlist.txt $(TARGETDIR)/rom/mac_chklist.txt
endif
ifeq ($(RTCONFIG_QTN),y)
	# install qtn boot-code and firmware
	install -D qtnimage/topaz-linux.lzma.img $(TARGETDIR)/rom/qtn/boot/topaz-linux.lzma.img
	install -D qtnimage/u-boot.bin $(TARGETDIR)/rom/qtn/boot/u-boot.bin
	ln -s /rom/qtn/boot/topaz-linux.lzma.img $(TARGETDIR)/rom/qtn/topaz-linux.lzma.img
	ln -s /rom/qtn/boot/u-boot.bin $(TARGETDIR)/rom/qtn/u-boot.bin
	install -D qtnimage/router_command.sh $(TARGETDIR)/rom/qtn/router_command.sh
	install -D qtnimage/qtn_dbg.sh $(TARGETDIR)/rom/qtn/qtn_dbg.sh
	install -D qtnimage/start-stateless-slave $(TARGETDIR)/rom/qtn/start-stateless-slave
	install -D qtnimage/tweak_qcomm $(TARGETDIR)/rom/qtn/tweak_qcomm
endif
ifeq ($(RTCONFIG_BWDPI),y)
	install -d $(TARGETDIR)/usr/bwdpi/
	cp -f bwdpi_bin/*.so $(TARGETDIR)/usr/lib/
	cp -f bwdpi_bin/*.so.0.1 $(TARGETDIR)/usr/lib/
	cp -f bwdpi_bin/*.ko $(TARGETDIR)/usr/bwdpi/
	cp -f bwdpi_bin/*.trf $(TARGETDIR)/usr/bwdpi/
	install -D bwdpi_bin/qosd $(TARGETDIR)/usr/sbin/qosd
	install -D bwdpi_bin/wred $(TARGETDIR)/usr/sbin/wred
	install -D bwdpi_bin/wred_set_conf $(TARGETDIR)/usr/sbin/wred_set_conf
	install -D bwdpi_bin/data_colld $(TARGETDIR)/usr/sbin/data_colld
	install -D bwdpi_bin/dc_monitor.sh $(TARGETDIR)/usr/sbin/dc_monitor.sh
	install -D bwdpi_bin/bwdpi-rule-agent $(TARGETDIR)/usr/sbin/bwdpi-rule-agent
	install -D bwdpi_bin/ntdasus2014.cert $(TARGETDIR)/usr/bwdpi/ntdasus2014.cert
	install -D $(shell dirname $(shell which $(CXX)))/../lib/librt.so.0 $(TARGETDIR)/lib/librt.so.0
endif

ifeq ($(RTCONFIG_BCMARM),y)
ifeq ($(RTCONFIG_CFEZ),y)
	install -D envram_bin/envrams $(TARGETDIR)/usr/sbin/envrams
	install -D envram_bin/envram $(TARGETDIR)/usr/sbin/envram
endif
endif

	@echo ---

	@rm -f $(TARGETDIR)/lib/modules/*/build

gen_gpl_excludes_router:
	@[ ! -d ../../../buildtools/ ] || ../../../buildtools/gpl_excludes_router.sh $(SRCBASE) "$(obj-y) $(released_dir)"

image:
	@rm -f $(PLATFORMDIR)/target.image
ifneq (,$(filter y,$(RTCONFIG_RALINK) $(RTCONFIG_QCA)))
	@$(MAKE) -C $(LINUXDIR)/scripts/squashfs $(MKSQUASHFS_TARGET)
	@$(LINUXDIR)/scripts/squashfs/$(MKSQUASHFS) $(TARGETDIR) $(PLATFORMDIR)/target.image -all-root -noappend -nopad
else ifeq ($(RTCONFIG_BCMARM),y)
	@$(SRCBASE)/ctools/mksquashfs $(TARGETDIR) $(PLATFORMDIR)/target.image -all-root -noappend
else
	@$(MAKE) -C $(LINUXDIR)/scripts/squashfs $(MKSQUASHFS_TARGET)
	@$(LINUXDIR)/scripts/squashfs/$(MKSQUASHFS) $(TARGETDIR) $(PLATFORMDIR)/target.image -all-root -noappend
endif
#	Package kernel and filesystem
#	if grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
#		cp $(PLATFORMDIR)/target.image $(LINUXDIR)/arch/mips/ramdisk/$${CONFIG_EMBEDDED_RAMDISK_IMAGE} ; \
#		$(MAKE) -C $(LINUXDIR) zImage ; \
#	else \
#		cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/vmlinuz $(PLATFORMDIR)/ ; \
#		trx -o $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/target.image ; \
#	fi

# 	Pad self-booting Linux to a 64 KB boundary
#	cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/zImage $(PLATFORMDIR)/
#	dd conv=sync bs=64k < $(PLATFORMDIR)/zImage > $(PLATFORMDIR)/linux.bin
# 	Append filesystem to self-booting Linux
#	cat $(PLATFORMDIR)/target.image >> $(PLATFORMDIR)/linux.bin


libc:	$(LIBDIR)/ld-uClibc.so.0
#	$(MAKE) -C ../../../tools-src/uClibc all
#	$(MAKE) -C ../../../tools-src/uClibc install


#
# cleaners
#

clean: clean-build $(obj-clean)
	rm -rf layer7/squished
	make -C config clean

clean-build: dummy
	rm -rf $(TARGETDIR)
	rm -rf $(INSTALLDIR)
	rm -f $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/target.image
	rm -rf $(PLATFORMDIR)/extras
	rm -rf kernel_header

distclean: clean
ifneq ($(INSIDE_MAK),1)
	$(MAKE) -C $(SRCBASE) $@ INSIDE_MAK=1
endif
#	-rm -f $(LIBDIR)/*.so.0  $(LIBDIR)/*.so

#
# configuration
#

CONFIG_IN := config/config.in

config/conf config/mconf:
	$(MAKE) -C config

rconf: config/conf
	config/conf $(CONFIG_IN)

rmconf: config/mconf
	config/mconf $(CONFIG_IN)

roldconf: config/conf
	config/conf -o $(CONFIG_IN)
	$(MAKE) shared-clean libdisk-clean rc-clean nvram$(BCMEX)-clean httpd-clean prebuilt-clean libbcmcrypto-clean
#	@$(MAKE) iptables-clean ebtables-clean pppd-clean zebra-clean openssl-clean mssl-clean mdu-clean pppd-clean
	$(MAKE) dnsmasq-clean iproute2-clean networkmap-clean
ifeq ($(RTCONFIG_BCMARM),y)
	$(MAKE) compressed-clean
endif
#ifeq ($(RTCONFIG_DHDAP),y)
#ifeq ($(RTCONFIG_BCM7),y)
#ifneq ($(wildcard $(SRCBASE_FW)/wl/sys),)
#ifeq ($(wildcard /projects/hnd/tools/linux/hndtools-armeabi-2011.09),)
#	# build 43602 src and to match its path
#	sudo mkdir /projects/hnd/tools/linux -p
#	sudo rm -rf /projects/hnd/tools/linux/hndtools-armeabi-2011.09
#	sudo ln -sf $(SRCBASE)/toolchains/hndtools-armeabi-2011.09 /projects/hnd/tools/linux/hndtools-armeabi-2011.09
#endif
#endif
#else ifeq ($(RTCONFIG_BCM_7114),y)
#ifeq ($(wildcard /projects/hnd/tools/linux/hndtools-armeabi-2013.11),)
#	# build pciefd
#	sudo mkdir /projects/hnd/tools/linux/bin -p
#	sudo rm -rf /projects/hnd/tools/linux/hndtools-armeabi-2013.11
#	sudo ln -sf $(SRCBASE)/toolchains/hndtools-armeabi-2013.11 /projects/hnd/tools/linux/hndtools-armeabi-2013.11
#	sudo ln -sf $(SRCBASE)/toolchains/fwtag.ini /projects/hnd/tools/linux/bin/fwtag.ini
#endif
#endif
#endif

kconf:
	$(MAKE) -C $(LINUXDIR) config

kmconf:
	$(MAKE) -C $(LINUXDIR) menuconfig

koldconf:
	$(MAKE) -C $(LINUXDIR) oldconfig
	$(MAKE) -C $(LINUXDIR) include/linux/version.h

bboldconf:
	$(MAKE) -C busybox oldconfig

config conf: rconf kconf

menuconfig mconf: rmconf kmconf

oldconfig oldconf: koldconf roldconf bboldconf


#
# overrides and extra dependencies
#

busybox: dummy $(if $(RTCONFIG_PROTECTION_SERVER),protect_srv-stage,)
	@$(MAKE) -C busybox $(PARALLEL_BUILD) \
		EXTRA_CFLAGS="-fPIC $(EXTRACFLAGS) -I$(STAGEDIR)/usr/include  \
			$(if $(RTCONFIG_PROTECTION_SERVER),-DSECURITY_NOTIFY,)" \
		EXTRA_LDFLAGS="-L$(STAGEDIR)/usr/lib" \
		EXTRA_LDLIBS="$(if $(RTCONFIG_PROTECTION_SERVER),ptcsrv,)"

busybox-install: $(if $(RTCONFIG_PROTECTION_SERVER),protect_srv-stage,)
	$(MAKE) -C busybox install CONFIG_PREFIX=$(INSTALLDIR)/busybox \
		EXTRA_CFLAGS="-fPIC $(EXTRACFLAGS) -I$(STAGEDIR)/usr/include  \
			$(if $(RTCONFIG_PROTECTION_SERVER),-DSECURITY_NOTIFY,)" \
		EXTRA_LDFLAGS="-L$(STAGEDIR)/usr/lib" \
		EXTRA_LDLIBS="$(if $(RTCONFIG_PROTECTION_SERVER),ptcsrv,)"
	chmod 4755 $(INSTALLDIR)/busybox/bin/busybox

busybox-clean:
	$(MAKE) -C busybox distclean

busybox-config:
	$(MAKE) -C busybox menuconfig

e2fsprogs/Makefile: e2fsprogs/Makefile.in
ifeq ($(RTCONFIG_BCMARM),y)
	cd e2fsprogs && CFLAGS="-Os $(EXTRACFLAGS)" \
	$(CONFIGURE) --prefix=/usr --sysconfdir=/etc --enable-elf-shlibs ac_cv_lib_pthread_sem_init=no \
		--disable-tls --disable-rpath --disable-nls \
		--disable-debugfs --disable-imager --disable-resizer --disable-defrag --disable-uuidd
else
	cd e2fsprogs && CFLAGS="-Os $(EXTRACFLAGS)" \
	$(CONFIGURE) LDFLAGS="$(if $(QCA),-ldl)" \
		--prefix=/usr --sysconfdir=/etc --enable-elf-shlibs \
		--disable-tls --disable-rpath --disable-nls \
		--disable-debugfs --disable-imager --disable-resizer --disable-defrag --disable-uuidd
endif

6relayd: 6relayd/Makefile
	@EXT_CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections -fPIC" \
	EXT_LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	PREFIX="/usr" \
	$(MAKE) -C 6relayd

6relayd-install: 6relayd
	install -D 6relayd/src/6relayd $(INSTALLDIR)/6relayd/usr/sbin/6relayd
	$(STRIP) $(INSTALLDIR)/6relayd/usr/sbin/6relayd

6relayd-clean:
	-@$(MAKE) -C 6relayd clean

e2fsprogs: e2fsprogs/Makefile
	$(MAKE) -C $@ $(PARALLEL_BUILD)

e2fsprogs-clean:
	-@$(MAKE) -C e2fsprogs clean
	@rm -f e2fsprogs/Makefile

e2fsprogs-install:
	install -D e2fsprogs/lib/libblkid.so.1.0 $(INSTALLDIR)/e2fsprogs/usr/lib/libblkid.so.1.0
	install -D e2fsprogs/lib/libcom_err.so.2.1 $(INSTALLDIR)/e2fsprogs/usr/lib/libcom_err.so.2.1
	install -D e2fsprogs/lib/libe2p.so.2.3 $(INSTALLDIR)/e2fsprogs/usr/lib/libe2p.so.2.3
	install -D e2fsprogs/lib/libext2fs.so.2.4 $(INSTALLDIR)/e2fsprogs/usr/lib/libext2fs.so.2.4
	install -D e2fsprogs/lib/libuuid.so.1.2 $(INSTALLDIR)/e2fsprogs/usr/lib/libuuid.so.1.2
	$(STRIP) $(INSTALLDIR)/e2fsprogs/usr/lib/*.so.*
	ln -sf libblkid.so.1.0 $(INSTALLDIR)/e2fsprogs/usr/lib/libblkid.so.1
	ln -sf libcom_err.so.2.1 $(INSTALLDIR)/e2fsprogs/usr/lib/libcom_err.so.2
	ln -sf libe2p.so.2.3 $(INSTALLDIR)/e2fsprogs/usr/lib/libe2p.so.2
	ln -sf libext2fs.so.2.4 $(INSTALLDIR)/e2fsprogs/usr/lib/libext2fs.so.2
	ln -sf libuuid.so.1.2 $(INSTALLDIR)/e2fsprogs/usr/lib/libuuid.so.1
	install -D e2fsprogs/e2fsck/e2fsck $(INSTALLDIR)/e2fsprogs/usr/sbin/e2fsck
	install -D e2fsprogs/misc/mke2fs $(INSTALLDIR)/e2fsprogs/usr/sbin/mke2fs
	install -D e2fsprogs/misc/tune2fs $(INSTALLDIR)/e2fsprogs/usr/sbin/tune2fs
	install -D e2fsprogs/misc/badblocks $(INSTALLDIR)/e2fsprogs/usr/sbin/badblocks
	$(STRIP) $(INSTALLDIR)/e2fsprogs/usr/sbin/*
	ln -sf e2fsck $(INSTALLDIR)/e2fsprogs/usr/sbin/fsck.ext2
	ln -sf e2fsck $(INSTALLDIR)/e2fsprogs/usr/sbin/fsck.ext3
ifeq (2_6_36,$(LINUX_VERSION))
	ln -sf e2fsck $(INSTALLDIR)/e2fsprogs/usr/sbin/fsck.ext4
endif
	ln -sf mke2fs $(INSTALLDIR)/e2fsprogs/usr/sbin/mkfs.ext2
	ln -sf mke2fs $(INSTALLDIR)/e2fsprogs/usr/sbin/mkfs.ext3
ifeq (2_6_36,$(LINUX_VERSION))
	ln -sf mke2fs $(INSTALLDIR)/e2fsprogs/usr/sbin/mkfs.ext4
endif
	install -D -m 0644 e2fsprogs/e2fsck/e2fsck.conf $(INSTALLDIR)/rom/rom/etc/e2fsck.conf

infosvr: shared nvram${BCMEX}

httpd: shared nvram$(BCMEX) libdisk $(if $(RTCONFIG_HTTPS),mssl) $(if $(RTCONFIG_PUSH_EMAIL),push_log) $(if $(RTCONFIG_BWDPI),bwdpi) $(if $(RTCONFIG_BWDPI),bwdpi_sqlite) json-c

	@$(SEP)
	@-rm -f httpd/prebuild/*.o || true
	@-cp -f httpd/prebuild/models/$(BUILD_NAME)/* httpd/prebuild/
	@$(MAKE) -C httpd

httpd_uam: shared nvram$(BCMEX)$(EX7) libdisk $(if $(RTCONFIG_HTTPS),mssl) $(if $(RTCONFIG_PUSH_EMAIL),push_log) $(if $(RTCONFIG_FBWIFI),fb_wifi)
	@$(SEP)
	@$(MAKE) -C httpd_uam

bonnie: bonnie/Makefile
	$(MAKE) CXX=$(CXX) -C bonnie

bonnie/Makefile:
	$(MAKE) bonnie-configure

bonnie-configure:
	( cd bonnie ; \
		$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
	)

bonnie-install:
	install -D bonnie/bonnie++ $(INSTALLDIR)/bonnie/sbin/bonnie++
	$(STRIP) $(INSTALLDIR)/bonnie/sbin/bonnie++
ifeq ($(RTCONFIG_BCMARM),y)
	install -D $(shell dirname $(shell which $(CXX)))/../arm-brcm-linux-uclibcgnueabi/lib/libstdc++.so.6 $(INSTALLDIR)/bonnie/usr/lib/libstdc++.so.6
else
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/bonnie/usr/lib/libstdc++.so.6
endif

bonnie-clean:
	[ ! -f bonnie/Makefile ] || $(MAKE) -C bonnie clean
	@rm -f bonnie/Makefile

www-install:
	@$(MAKE) -C www install INSTALLDIR=$(INSTALLDIR)/www TOMATO_EXPERIMENTAL=$(TOMATO_EXPERIMENTAL)

ifeq ($(RTCONFIG_OPENVPN),y)
OPENSSL_CIPHERS:=enable-rc5 no-rc4
else
#OPENSSL_CIPHERS:=no-dh no-idea no-rc2 no-rc5 no-aes no-aes192 no-cast no-des no-modes no-tls1 no-tlsext
OPENSSL_CIPHERS:=no-rc4
endif

#OPENSSL_CIPHERS:=enable-aes enable-tls1 enable-tlsext

openssl-1.0.0q/Makefile:
	cd openssl-1.0.0q && \
	./Configure $(HOSTCONFIG) --openssldir=/etc --cross-compile-prefix=' ' \
	-ffunction-sections -fdata-sections -Wl,--gc-sections \
	shared $(OPENSSL_CIPHERS) no-ssl2 no-ssl3 
#	no-sha0 no-smime no-camellia no-krb5 no-rmd160 no-ripemd \
#	no-seed no-capieng no-cms no-gms no-gmp no-rfc3779 \
#	no-ec no-ecdh no-ecdsa no-err no-hw no-jpake no-threads \
#	no-zlib no-engine no-engines no-sse2 no-perlasm \
#	no-dtls1 no-store no-psk no-md2 no-mdc2 no-ts

	-@$(MAKE) -C openssl-1.0.0q clean depend
	@touch $@

openssl-1.0.0q: openssl-1.0.0q/Makefile
	$(MAKE) -C openssl-1.0.0q && $(MAKE) $@-stage

openssl-1.0.0q-clean:
	[ ! -f openssl-1.0.0q/Makefile ] || $(MAKE) -C openssl-1.0.0q clean
	@rm -f openssl-1.0.0q/Makefile

openssl-1.0.0q-install:
	install -D openssl-1.0.0q/libcrypto-1.0.0q.so.1.0.0 $(INSTALLDIR)/openssl-1.0.0q/usr/lib/libcrypto-1.0.0q.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl-1.0.0q/usr/lib/libcrypto-1.0.0q.so.1.0.0
	cd $(INSTALLDIR)/openssl-1.0.0q/usr/lib && ln -sf libcrypto-1.0.0q.so.1.0.0 libcrypto-1.0.0q.so

#	install -D openssl-1.0.0q/apps/openssl-1.0.0q $(INSTALLDIR)/openssl-1.0.0q/usr/sbin/openssl-1.0.0q
#	$(STRIP) $(INSTALLDIR)/openssl-1.0.0q/usr/sbin/openssl-1.0.0q
#	chmod 0500 $(INSTALLDIR)/openssl-1.0.0q/usr/sbin/openssl-1.0.0q

	install -D openssl-1.0.0q/libssl-1.0.0q.so.1.0.0 $(INSTALLDIR)/openssl-1.0.0q/usr/lib/libssl-1.0.0q.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl-1.0.0q/usr/lib/libssl-1.0.0q.so.1.0.0
	cd $(INSTALLDIR)/openssl-1.0.0q/usr/lib && ln -sf libssl-1.0.0q.so.1.0.0 libssl-1.0.0q.so

openssl-1.0.0q-stage:
	@$(MAKE) -C openssl-1.0.0q install_sw INSTALL_PREFIX=$(STAGEDIR)

openssl/Makefile:
	cd openssl && \
	./Configure $(HOSTCONFIG) --openssldir=/etc --cross-compile-prefix=' ' \
	-ffunction-sections -fdata-sections -Wl,--gc-sections \
	shared $(OPENSSL_CIPHERS)
#	no-sha0 no-smime no-camellia no-krb5 no-rmd160 no-ripemd \
#	no-seed no-capieng no-cms no-gms no-gmp no-rfc3779 \
#	no-ec no-ecdh no-ecdsa no-err no-hw no-jpake no-threads \
#	no-zlib no-engine no-engines no-sse2 no-perlasm \
#	no-dtls1 no-store no-psk no-md2 no-mdc2 no-ts

	-@$(MAKE) -C openssl clean depend
	@touch $@

openssl: openssl/Makefile
	$(MAKE) -C openssl && $(MAKE) $@-stage

openssl-clean:
	[ ! -f openssl/Makefile ] || $(MAKE) -C openssl clean
	@rm -f openssl/Makefile

openssl-install:
	install -D openssl/libcrypto.so.1.0.0 $(INSTALLDIR)/openssl/usr/lib/libcrypto.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl/usr/lib/libcrypto.so.1.0.0
	cd $(INSTALLDIR)/openssl/usr/lib && ln -sf libcrypto.so.1.0.0 libcrypto.so

	install -D openssl/apps/openssl $(INSTALLDIR)/openssl/usr/sbin/openssl
	$(STRIP) $(INSTALLDIR)/openssl/usr/sbin/openssl
	chmod 0500 $(INSTALLDIR)/openssl/usr/sbin/openssl

# add for webdav
	install -D openssl/libssl.so.1.0.0 $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	cd $(INSTALLDIR)/openssl/usr/lib && ln -sf libssl.so.1.0.0 libssl.so

	install -D -m 0500 httpd/gencert.sh $(INSTALLDIR)/openssl/usr/sbin/gencert.sh

#	perl -e 'while (<>) { s/.SECS/time()-(24*60*60)/e; print; }' < httpd/gencert.sh > $(INSTALLDIR)/openssl/usr/sbin/gencert.sh
#	chmod 0500 $(INSTALLDIR)/openssl/usr/sbin/gencert.sh

ifeq ($(RTCONFIG_FTP_SSL),y)
# !!TB
	install -D openssl/libssl.so.1.0.0 $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	cd $(INSTALLDIR)/openssl/usr/lib && ln -sf libssl.so.1.0.0 libssl.so
endif

ifeq ($(RTCONFIG_OPENVPN),y)
	install -D openssl/libssl.so.1.0.0 $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	cd $(INSTALLDIR)/openssl/usr/lib && ln -sf libssl.so.1.0.0 libssl.so
endif

ifeq ($(RTCONFIG_HTTPS),y)
	install -D openssl/libssl.so.1.0.0 $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	$(STRIP) $(INSTALLDIR)/openssl/usr/lib/libssl.so.1.0.0
	cd $(INSTALLDIR)/openssl/usr/lib && ln -sf libssl.so.1.0.0 libssl.so
endif

openssl-stage:
	@$(MAKE) -C openssl install_sw INSTALL_PREFIX=$(STAGEDIR)

mssl:	openssl

mdu:	shared mssl

wb: openssl libxml2 curl

push_log: wb

rc: nvram$(BCMEX) shared libbcmcrypto libdisk $(if $(RTCONFIG_FBWIFI),fb_wifi) $(if $(RTCONFIG_PUSH_EMAIL),push_log) $(if $(RTCONFIG_QTN),libqcsapi_client) $(if $(CONFIG_LIBBCM),libbcm) $(if $(RTCONFIG_BWDPI),bwdpi) $(if $(RTCONFIG_USB_SMS_MODEM),smspdu)
	@$(SEP)
# Dig into our collection of prebuilt objects based on model
	@-rm -f rc/prebuild/*.o || true
	@-cp -f rc/prebuild/models/$(BUILD_NAME)/* rc/prebuild/
	@$(MAKE) -C rc

networkmap: shared

bridge/stamp-h1:
ifeq ($(CONFIG_LINUX3_14),y)
	cd bridge && CFLAGS="-Os -g $(EXTRACFLAGS)" \
	$(CONFIGURE) --prefix="" --with-linux-headers=$(KDIR)/include
else
	cd bridge && CFLAGS="-Os -g $(EXTRACFLAGS)" \
	$(CONFIGURE) --prefix="" --with-linux-headers=$(LINUXDIR)/include
endif
	touch $@

bridge: bridge/stamp-h1
	@$(SEP)
	@$(MAKE) -C bridge

bridge-clean:
	-@$(MAKE) -C bridge clean
	@rm -f bridge/stamp-h1

bridge-install:
	install -D bridge/brctl/brctl $(INSTALLDIR)/bridge/usr/sbin/brctl
	$(STRIP) $(INSTALLDIR)/bridge/usr/sbin/brctl

rstpd:
	$(MAKE) -C rstpd

rstpd-clean:
	$(MAKE) -C rstpd clean

rstpd-install:
	install -D rstpd/rstpd $(INSTALLDIR)/rstpd/usr/sbin/rstpd
	$(STRIP) $(INSTALLDIR)/rstpd/usr/sbin/rstpd

dmalloc: dmalloc/Makefile dmalloc/dmalloc.h.2 dmalloc/settings.h
	$(MAKE) -C dmalloc all shlib cxx && $(MAKE) $@-stage

dmalloc/Makefile dmalloc/dmalloc.h.2 dmalloc/settings.h:
	$(MAKE) dmalloc-configure

dmalloc-configure:
	( cd dmalloc ; \
		$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		--enable-cxx --enable-threads --enable-shlib --with-pagesize=12 \
	)

dmalloc-install: dmalloc
	install -D $(STAGEDIR)/usr/sbin/dmalloc $(INSTALLDIR)/dmalloc/usr/sbin/dmalloc
	install -d $(INSTALLDIR)/dmalloc/usr/lib
	install -D $(STAGEDIR)/usr/lib/libdmalloc*.so* $(INSTALLDIR)/dmalloc/usr/lib
	$(STRIP) $(INSTALLDIR)/dmalloc/usr/sbin/dmalloc
	$(STRIP) $(INSTALLDIR)/dmalloc/usr/lib/*.so*

dmalloc-clean:
	[ ! -f dmalloc/Makefile ] || $(MAKE) -C dmalloc clean
	@rm -f dmalloc/{conf.h,dmalloc.h,dmalloc.h.2,settings.h,Makefile}

ifeq ($(RTCONFIG_BCM7),y)
$(obj-pciefd) :
# Build PCIEFD firmware only if it is not prebuilt
ifeq ($(RTCONFIG_DHDAP),y)
ifneq ($(wildcard $(SRCBASE_FW)/wl/sys),)
	+$(MAKE) CROSS_COMPILE=arm-none-eabi -C $(SRCBASE_FW)/dongle/rte/wl $(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)
	if [ -f $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/rtecdc_$(patsubst %-roml-obj,%,$@).h ]; then \
		cp $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/rtecdc_$(patsubst %-roml-obj,%,$@).h $(SRCBASE_DHD)/shared/rtecdc_$(patsubst %-roml-obj,%,$@).h && \
		echo "#include <rtecdc_$(patsubst %-roml-obj,%,$@).h>" >> $(PCIEFD_EMBED_HEADER); \
	fi;
	if [ -f $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/rtecdc_$(patsubst %-ram-obj,%,$@).h ]; then \
		cp $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/rtecdc_$(patsubst %-ram-obj,%,$@).h $(SRCBASE_DHD)/shared/rtecdc_$(patsubst %-ram-obj,%,$@).h && \
		echo "#include <rtecdc_$(patsubst %-ram-obj,%,$@).h>" >> $(PCIEFD_EMBED_HEADER); \
	fi;
	if [ -f $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/wlc_clm_data.c ]; then \
		cp $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/wlc_clm_data.c $(SRCBASE_FW)/wl/clm/src/wlc_clm_data.c.GEN && \
		cp $(SRCBASE_FW)/dongle/rte/wl/builds/$(patsubst %-obj,%,$@)/$(PCIEFD_TARGET_NAME)/wlc_clm_data_inc.c $(SRCBASE_FW)/wl/clm/src/wlc_clm_data_inc.c.GEN; \
	fi;
endif
endif

pciefd-cleangen: pciefd-clean
# Clean PCIEFD firmware only if it is not prebuilt
ifeq ($(RTCONFIG_DHDAP),y)
ifneq ($(wildcard $(SRCBASE_FW)/wl/sys),)
	rm -f  $(PCIEFD_EMBED_HEADER)
	cp -f $(PCIEFD_EMBED_HEADER_TEMPLATE) $(PCIEFD_EMBED_HEADER)
endif
endif

pciefd: pciefd-cleangen $(obj-pciefd)

pciefd-clean :
ifeq ($(RTCONFIG_DHDAP),y)
ifneq ($(wildcard $(SRCBASE_FW)/wl/sys),)
	+$(MAKE) CROSS_COMPILE=arm-none-eabi -C $(SRCBASE_FW)/dongle/rte/wl clean
	rm -f $(SRCBASE_DHD)/shared/rtecdc*.h
endif
endif

pciefd-install :
	# Nothing to be done here
	@true
endif	# BCM7

ifeq ($(RTCONFIG_DHDAP),y)
dhd:
	@true
ifneq ($(wildcard $(SRCBASE_DHD)/dhd/exe),)
	$(MAKE) TARGET_PREFIX=$(CROSS_COMPILE) -C $(SRCBASE_DHD)/dhd/exe
endif

dhd-clean :
ifneq ($(wildcard $(SRCBASE_DHD)/dhd/exe),)
	$(MAKE) TARGET_PREFIX=$(CROSS_COMPILE) -C $(SRCBASE_DHD)/dhd/exe clean
	rm -f $(INSTALLDIR)/dhd/usr/sbin/dhd
	cd $(SRCBASE_DHD) && rm -f `find ./ -name "*.cmd" && find ./ -name "*.o"`
endif

dhd-install :
ifneq ($(wildcard $(SRCBASE_DHD)/dhd/exe),)
	install -d $(INSTALLDIR)/dhd/usr/sbin
	install $(SRCBASE_DHD)/dhd/exe/dhd $(INSTALLDIR)/dhd/usr/sbin
	$(STRIP) $(INSTALLDIR)/dhd/usr/sbin/dhd
endif
endif

.PHONY: dhd_monitor

dhd_monitor: shared nvram$(BCMEX)
ifeq ($(RTCONFIG_DHDAP),y)
	+$(MAKE) LINUXDIR=$(LINUXDIR) EXTRA_LDFLAGS=$(EXTRA_LDFLAGS) -C dhd_monitor dhd_monitor
endif

dhd_monitor-install: dhd_monitor
ifeq ($(RTCONFIG_DHDAP),y)
	+$(MAKE) -C dhd_monitor INSTALLDIR=$(INSTALLDIR)/dhd_monitor install
endif

dhd_monitor-clean:
ifeq ($(RTCONFIG_DHDAP),y)
	+$(MAKE) -C dhd_monitor clean
endif


dnsmasq:  $(if $(RTCONFIG_DNSSEC),nettle,)
	@$(SEP)
	@$(MAKE) -C dnsmasq \
	COPTS="-DHAVE_BROKEN_RTC -DHAVE_LEASEFILE_EXPIRE -DNO_AUTH -DNO_INOTIFY -DNO_GMP \
		$(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6) \
		$(if $(RTCONFIG_USB_EXTRAS)||$(RTCONFIG_TR069),,-DNO_SCRIPT) \
		$(if $(RTCONFIG_USB_EXTRAS),,-DNO_TFTP) \
		$(if $(RTCONFIG_DNSSEC),-I$(TOP)/nettle/include -I$(TOP)/gmp -DHAVE_DNSSEC -DHAVE_DNSSEC_STATIC,)" \
	CFLAGS="-Os -ffunction-sections -fdata-sections $(EXTRACFLAGS)" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections \
		$(if $(RTCONFIG_DNSSEC),-L$(TOP)/nettle/lib -L$(TOP)/gmp/.libs,)" \
	$(if $(RTCONFIG_DNSSEC),PKG_CONFIG_PATH="$(TOP)/nettle/lib/pkgconfig",)

dnsmasq-install:
	install -D dnsmasq/src/dnsmasq $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq
	$(STRIP) $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq

nettle/stamp-h1:
	@cd nettle && \
	CPPFLAGS="-I$(TOP)/gmp" \
	CFLAGS="-O2 -Wall $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	LDFLAGS="-L$(TOP)/gmp/.libs -ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	$(CONFIGURE) prefix=$(TOP)/nettle --enable-mini-gmp --disable-documentation --disable-shared
	@touch nettle/stamp-h1

nettle: nettle/stamp-h1
	@$(SEP)
	@$(MAKE) -C nettle $(PARALLEL_BUILD)
	@$(MAKE) -C nettle install

nettle-clean:
	-@$(MAKE) -C nettle clean
	@rm -f nettle/stamp-h1
	@rm -rf nettle/include nettle/lib

iptables:
	@$(SEP)
	$(MAKE) -C iptables BINDIR=/usr/sbin LIBDIR=/usr/lib KERNEL_DIR=$(LINUXDIR) COPT_FLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE"

iptables-install:
	install -D iptables/iptables $(INSTALLDIR)/iptables/usr/sbin/iptables
	cd $(INSTALLDIR)/iptables/usr/sbin && \
		ln -sf iptables iptables-save && \
		ln -sf iptables iptables-restore

	install -d $(INSTALLDIR)/iptables/usr/lib/iptables
	install -D iptables/extensions/*.so $(INSTALLDIR)/iptables/usr/lib/iptables/

	install -D iptables/libiptc.so $(INSTALLDIR)/iptables/usr/lib/libiptc.so

	$(STRIP) $(INSTALLDIR)/iptables/usr/sbin/iptables
	$(STRIP) $(INSTALLDIR)/iptables/usr/lib/iptables/*.so
	$(STRIP) $(INSTALLDIR)/iptables/usr/lib/libiptc.so

ifeq ($(RTCONFIG_IPV6),y)
	install iptables/ip6tables $(INSTALLDIR)/iptables/usr/sbin/ip6tables
	$(STRIP) $(INSTALLDIR)/iptables/usr/sbin/ip6tables
	cd $(INSTALLDIR)/iptables/usr/sbin && \
		ln -sf ip6tables ip6tables-save && \
		ln -sf ip6tables ip6tables-restore
endif

iptables-clean:
	-@$(MAKE) -C iptables KERNEL_DIR=$(LINUXDIR) clean

iptables-1.4.21/configure:
	cd iptables-1.4.21 && ./autogen.sh

iptables-1.4.21/Makefile: iptables-1.4.21/configure
	cd iptables-1.4.21 && $(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		--with-kernel=$(LINUXDIR) \
		$(if $(RTCONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		CFLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE"

iptables-1.4.21: iptables-1.4.21/Makefile
	@$(SEP)
	$(MAKE) -C $@ KERNEL_DIR=$(LINUXDIR) COPT_FLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE"

iptables-1.4.21-install:
	install -D iptables-1.4.21/iptables/.libs/xtables-multi $(INSTALLDIR)/iptables-1.4.21/usr/sbin/xtables-multi
	cd $(INSTALLDIR)/iptables-1.4.21/usr/sbin && \
		ln -sf xtables-multi iptables-restore && \
		ln -sf xtables-multi iptables-save && \
		ln -sf xtables-multi iptables
	install -d $(INSTALLDIR)/iptables-1.4.21/usr/lib/xtables
	install -D iptables-1.4.21/libiptc/.libs/libiptc.so $(INSTALLDIR)/iptables-1.4.21/usr/lib/libiptc.so
	install -D iptables-1.4.21/libiptc/.libs/libip4tc.so $(INSTALLDIR)/iptables-1.4.21/usr/lib/libip4tc.so
	cd $(INSTALLDIR)/iptables-1.4.21/usr/lib && \
		ln -sf libiptc.so  libiptc.so.0 && \
		ln -sf libiptc.so  libiptc.so.0.0.0 && \
		ln -sf libip4tc.so libip4tc.so.0 && \
		ln -sf libip4tc.so libip4tc.so.0.0.0
	install -D iptables-1.4.21/libxtables/.libs/lib*.so $(INSTALLDIR)/iptables-1.4.21/usr/lib/
	cd $(INSTALLDIR)/iptables-1.4.21/usr/lib && \
		ln -sf libxtables.so libxtables.so.10 && \
		ln -sf libxtables.so libxtables.so.10.0.0
	install -D iptables-1.4.21/extensions/*.so $(INSTALLDIR)/iptables-1.4.21/usr/lib/xtables

iptables-1.4.x/configure:
	cd iptables-1.4.x && ./autogen.sh

iptables-1.4.x/Makefile: iptables-1.4.x/configure
	cd iptables-1.4.x && $(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		--with-kernel=$(LINUXDIR) \
		$(if $(RTCONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		CFLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE"

iptables-1.4.x: iptables-1.4.x/Makefile libnfnetlink
	@$(SEP)
	$(MAKE) -C $@ KERNEL_DIR=$(LINUXDIR) COPT_FLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE" \
		CFLAGS="-Wall -Os -D_GNU_SOURCE $(EXTRACFLAGS) -I$(TOP)/libnfnetlink/include" \
		LDFLAGS="-L$(TOP)/libnfnetlink/src/.libs -lnfnetlink"

iptables-1.4.x-install:
	install -D iptables-1.4.x/iptables/.libs/xtables-multi $(INSTALLDIR)/iptables-1.4.x/usr/sbin/xtables-multi
	cd $(INSTALLDIR)/iptables-1.4.x/usr/sbin && \
		ln -sf xtables-multi iptables-save && \
		ln -sf xtables-multi iptables-restore && \
		ln -sf xtables-multi iptables-save && \
		ln -sf xtables-multi iptables
	install -d $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables
	install -D iptables-1.4.x/libiptc/.libs/libiptc.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/libiptc.so
	install -D iptables-1.4.x/libiptc/.libs/libip4tc.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/libip4tc.so
	cd $(INSTALLDIR)/iptables-1.4.x/usr/lib && \
		ln -sf libiptc.so  libiptc.so.0 && \
		ln -sf libiptc.so  libiptc.so.0.0.0 && \
		ln -sf libip4tc.so libip4tc.so.0 && \
		ln -sf libip4tc.so libip4tc.so.0.0.0
	install -D iptables-1.4.x/libxtables/.libs/lib*.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/
	cd $(INSTALLDIR)/iptables-1.4.x/usr/lib && \
		ln -sf libxtables.so libxtables.so.7 && \
		ln -sf libxtables.so libxtables.so.7.0.0
	install -D iptables-1.4.x/extensions/*.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables

ifeq ($(RTCONFIG_IPV6),y)
	cd $(INSTALLDIR)/iptables-1.4.x/usr/sbin && \
		ln -sf xtables-multi ip6tables-restore && \
		ln -sf xtables-multi ip6tables
	install -D iptables-1.4.x/libiptc/.libs/libip6tc.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/libip6tc.so
	cd $(INSTALLDIR)/iptables-1.4.x/usr/lib && \
		ln -sf libip6tc.so libip6tc.so.0 && \
		ln -sf libip6tc.so libip6tc.so.0.0.0
endif

	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/sbin/xtables-multi
	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/lib/*.so*
	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables/*.so*

iptables-1.4.x-clean:
	[ ! -f iptables-1.4.x/Makefile ] || $(MAKE) -C iptables-1.4.x KERNEL_DIR=$(LINUXDIR) distclean
	@rm -f iptables-1.4.x/Makefile

libnfnetlink/stamp-h1:
	cd $(TOP)/libnfnetlink && CC=$(CC) STRIP=$(STRIP) \
		CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
		$(CONFIGURE) --prefix=/usr --enable-shared --enable-static
	touch libnfnetlink/stamp-h1

libnfnetlink: libnfnetlink/stamp-h1
	@$(SEP)
	$(MAKE) -C libnfnetlink

libnfnetlink-install:
	install -D libnfnetlink/src/.libs/libnfnetlink.so.0.2.0 $(INSTALLDIR)/libnfnetlink/usr/lib/libnfnetlink.so.0.2.0
	$(STRIP) -s $(INSTALLDIR)/libnfnetlink/usr/lib/libnfnetlink.so.0.2.0
	cd $(INSTALLDIR)/libnfnetlink/usr/lib/ && \
		ln -sf libnfnetlink.so.0.2.0 libnfnetlink.so.0 && \
		ln -sf libnfnetlink.so.0.2.0 libnfnetlink.so


upnp: nvram$(BCMEX) shared $(IPTABLES)

miniupnpd/config.h: shared/version.h
ifeq ($(RTCONFIG_IGD2),y)
	cd miniupnpd && ./genconfig.sh --vendorcfg --leasefile $(if $(RTCONFIG_IPV6),--ipv6,) --igd2 --portinuse
else
	cd miniupnpd && ./genconfig.sh --vendorcfg --leasefile --portinuse
endif

miniupnpd: $(IPTABLES) e2fsprogs miniupnpd/config.h
	@$(SEP)
	cp -f ./shared/version.h miniupnpd$(MUVER)/.
	PKG_CONFIG=false ARCH=$(PLATFORM) \
	$(MAKE) -C $@ -f Makefile.merlin $(PARALLEL_BUILD) \
	    IPTABLESPATH=$(TOP)/$(IPTABLES) \
	    EXTRACFLAGS="-Os $(EXTRACFLAGS) -idirafter$(KERNEL_HEADER_DIR) -ffunction-sections -fdata-sections -I$(TOP)/e2fsprogs/lib" \
	    LDFLAGS="$(EXTRALDFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(IPTC_LIBDIR) -L$(TOP)/e2fsprogs/lib" \
	    LDLIBS="-Wl,--as-needed $(IPTC_LIBS) -luuid"

miniupnpd-clean:
	-@$(MAKE) -C miniupnpd -f Makefile.merlin clean
	-@rm miniupnpd/config.h

miniupnpd_1.4: $(IPTABLES) shared/version.h
	@$(SEP)
	cp -f shared/version.h $@/
	$(MAKE) -C $@ -f Makefile.asus \
	    IPTABLESPATH=$(TOP)/$(IPTABLES) \
	    EXTRACFLAGS="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	    LDFLAGS="$(EXTRALDFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(IPTC_LIBDIR)" \
	    LDLIBS="-Wl,--as-needed $(IPTC_LIBS)"

miniupnpd_1.4-clean:
	-@$(MAKE) -C miniupnpd_1.4 -f Makefile.asus clean

miniupnpd$(MUVER)-install:
	install -D miniupnpd$(MUVER)/miniupnpd $(INSTALLDIR)/miniupnpd$(MUVER)/usr/sbin/miniupnpd
	$(STRIP) $(INSTALLDIR)/miniupnpd$(MUVER)/usr/sbin/miniupnpd

miniupnpc:
	$(MAKE) -C miniupnpc

miniupnpc-clean:
	-@$(MAKE) -C miniupnpc clean

miniupnpc-install:
	install -D miniupnpc/upnpc-static $(INSTALLDIR)/miniupnpc/usr/sbin/miniupnpc
	$(STRIP) $(INSTALLDIR)/miniupnpc/usr/sbin/miniupnpc

# !!TB
shared: busybox $(if $(RTCONFIG_QTN),libqcsapi_client) $(if $(RTCONFIG_HTTPS),openssl)
# Dig into our collection of prebuilt objects based on model.
	@-rm -f shared/prebuild/*.o || true
	@-cp -f shared/prebuild/models/$(BUILD_NAME)/* shared/prebuild/

ifeq ($(RTCONFIG_RALINK)$(RTCONFIG_QCA),y)
	make -C wireless_tools wireless.h	
endif
	$(MAKE) -C shared

ifeq ($(RTCONFIG_FTP_SSL),y)
ftp_dep=openssl
else
	ftp_dep=
endif

vsftpd: $(ftp_dep)
	@$(SEP)
ifeq ($(RTCONFIG_FTP_SSL),y)
	-cp vsftpd/builddefs-ssl.h vsftpd/builddefs.h
else
	-cp vsftpd/builddefs-nossl.h vsftpd/builddefs.h
endif
	$(MAKE) -C vsftpd

vsftpd-install:
	install -D vsftpd/vsftpd $(INSTALLDIR)/vsftpd/usr/sbin/vsftpd
	$(STRIP) -s $(INSTALLDIR)/vsftpd/usr/sbin/vsftpd

vsftpd-3.x: $(ftp_dep)
	@$(SEP)
ifeq ($(RTCONFIG_FTP_SSL),y)
	-cp vsftpd-3.x/builddefs-ssl.h vsftpd-3.x/builddefs.h
else
	-cp vsftpd-3.x/builddefs-nossl.h vsftpd-3.x/builddefs.h
endif
	$(MAKE) -C $@

vsftpd-3.x-install:
	install -D vsftpd-3.x/vsftpd $(INSTALLDIR)/vsftpd-3.x/usr/sbin/vsftpd
	$(STRIP) -s $(INSTALLDIR)/vsftpd-3.x/usr/sbin/vsftpd

ntfs-3g/stamp-h1:
	cd ntfs-3g && \
	CC=$(CC) CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC -ldl" \
		$(CONFIGURE) --enable-shared=no --enable-static=no \
		--disable-library --disable-ldconfig --disable-mount-helper --with-fuse=internal \
		--disable-posix-acls --disable-nfconv --disable-dependency-tracking
	touch $@

ntfs-3g: ntfs-3g/stamp-h1
	@$(MAKE) -C ntfs-3g

ntfs-3g-clean:
	-@$(MAKE) -C ntfs-3g clean
	@rm -f ntfs-3g/stamp-h1

ntfs-3g-install: ntfs-3g
	install -D ntfs-3g/src/ntfs-3g $(INSTALLDIR)/ntfs-3g/bin/ntfs-3g
	$(STRIP) -s $(INSTALLDIR)/ntfs-3g/bin/ntfs-3g
	install -d $(INSTALLDIR)/ntfs-3g/sbin && cd $(INSTALLDIR)/ntfs-3g/sbin && \
		ln -sf ../bin/ntfs-3g mount.ntfs-3g && \
		ln -sf ../bin/ntfs-3g mount.ntfs

linux-ntfs-2.0.0/stamp-h1:
	cd linux-ntfs-2.0.0 && \
	$(CONFIGURE) --disable-ntfsmount
	touch $@

linux-ntfs-2.0.0: linux-ntfs-2.0.0/stamp-h1
	@$(MAKE) -C linux-ntfs-2.0.0 all

linux-ntfs-2.0.0-install:
	install -D linux-ntfs-2.0.0/libntfs/.libs/libntfs.so.10.0.0 $(INSTALLDIR)/linux-ntfs-2.0.0/usr/lib/libntfs.so.10.0.0
	$(STRIP) $(INSTALLDIR)/linux-ntfs-2.0.0/usr/lib/libntfs.so.10.0.0
	cd $(INSTALLDIR)/linux-ntfs-2.0.0/usr/lib && \
		ln -sf libntfs.so.10.0.0 libntfs.so.10 && \
		ln -sf libntfs.so.10.0.0 libntfs.so
	install -D linux-ntfs-2.0.0/ntfsprogs/.libs/ntfsfix $(INSTALLDIR)/linux-ntfs-2.0.0/usr/sbin/ntfsfix
	$(STRIP) $(INSTALLDIR)/linux-ntfs-2.0.0/usr/sbin/*

linux-ntfs-2.0.0-clean:
	-@$(MAKE) -C linux-ntfs-2.0.0 clean
	@rm -f linux-ntfs-2.0.0/stamp-h1

libupnp-1.3.1: libupnp-1.3.1/Makefile
	$(MAKE) -C $@

libupnp-1.3.1-install: libupnp-1.3.1
	@echo "do nothing"
#	install -d $(INSTALLDIR)/libupnp-1.3.1/usr/lib
#	install -m 775 libupnp-1.3.1/ixml/.libs/libixml.so.2.0.0 $(INSTALLDIR)/libupnp-1.3.1/usr/lib/libixml.so.2.0.0
#	install -m 775 libupnp-1.3.1/threadutil/.libs/libthreadutil.so.2.0.0 $(INSTALLDIR)/libupnp-1.3.1/usr/lib/libthreadutil.so.2.0.0
#	install -m 775 libupnp-1.3.1/upnp/.libs/libupnp.so.2.0.1 $(INSTALLDIR)/libupnp-1.3.1/usr/lib/libupnp.so.2.0.1
#	$(STRIP) $(INSTALLDIR)/libupnp-1.3.1/usr/lib/*.so.*
#	cd $(INSTALLDIR)/libupnp-1.3.1/usr/lib && \
#		ln -sf libixml.so.2.0.0 libixml.so.2 && \
#		ln -sf libixml.so.2.0.0 libixml.so
#	cd $(INSTALLDIR)/libupnp-1.3.1/usr/lib && \
#		ln -sf libthreadutil.so.2.0.0 libthreadutil.so.2 && \
#		ln -sf libthreadutil.so.2.0.0 libthreadutil.so
#	cd $(INSTALLDIR)/libupnp-1.3.1/usr/lib && \
#		ln -sf libupnp.so.2.0.1 libupnp.so.2 && \
#		ln -sf libupnp.so.2.0.1 libupnp.so

libupnp-1.3.1/Makefile: libupnp-1.3.1/Makefile.in
	( cd libupnp-1.3.1; \
	  config.aux/missing aclocal; \
	  config.aux/missing automake; \
	  config.aux/missing autoconf; \
	  $(CONFIGURE) --prefix=/usr --disable-dependency-tracking \
	 )

libusb10/stamp-h1:
	cd libusb10 && CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="-lpthread -ldl" \
	$(CONFIGURE) --prefix=/usr ac_cv_lib_rt_clock_gettime=no
	-@$(MAKE) -C libusb10 clean
	touch $@

libusb10: libusb10/stamp-h1
	$(MAKE) -C $@

libusb10-install: libusb10
	install -D libusb10/libusb/.libs/libusb-1.0.so.0.0.0 $(INSTALLDIR)/libusb10/usr/lib/libusb-1.0.so.0
	$(STRIP) $(INSTALLDIR)/libusb10/usr/lib/*.so.*
	cd $(INSTALLDIR)/libusb10/usr/lib && \
		ln -sf libusb-1.0.so.0 libusb-1.0.so.0.0.0 && \
		ln -sf libusb-1.0.so.0 libusb-1.0.so

libusb10-clean:
	-@$(MAKE) -C libusb10 clean
	@rm -f libusb10/stamp-h1

libusb/stamp-h1:
	cd libusb && CFLAGS="-Wall -Os $(EXTRACFLAGS)" \
	$(CONFIGURE) --prefix=/usr \
		LIBUSB_1_0_CFLAGS="-I$(TOP)/libusb10/libusb" \
		LIBUSB_1_0_LIBS="-L$(TOP)/libusb10/libusb/.libs -lusb-1.0 -lpthread -ldl\
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share"
	-@$(MAKE) -C libusb clean
	touch $@

libusb: libusb10 libusb/stamp-h1
	$(MAKE) -C $@

libusb-install: libusb
	install -D libusb/libusb/.libs/libusb-0.1.so.4.4.4 $(INSTALLDIR)/libusb/usr/lib/libusb-0.1.so.4
	$(STRIP) $(INSTALLDIR)/libusb/usr/lib/*.so.*
	cd $(INSTALLDIR)/libusb/usr/lib && \
		ln -sf libusb-0.1.so.4 libusb-0.1.so.4.4.4 && \
		ln -sf libusb-0.1.so.4 libusb.so

libusb-clean:
	-@$(MAKE) -C libusb clean
	@rm -f libusb/stamp-h1

usb-modeswitch-1.2.3: libusb-0.1.12
	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) -I$(TOP)/libusb-0.1.12" LIBS="\
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread -ldl\
		-L$(TOP)/libusb-0.1.12/.libs -lusb"

usb-modeswitch-1.2.3-install: usb-modeswitch-1.2.3
	install -D usb-modeswitch-1.2.3/usb_modeswitch $(INSTALLDIR)/usb-modeswitch-1.2.3/usr/sbin/usb_modeswitch
	$(STRIP) -s $(INSTALLDIR)/usb-modeswitch-1.2.3/usr/sbin/usb_modeswitch
	@mkdir -p $(TARGETDIR)/rom/etc
	@sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < $(TOP)/usb-modeswitch-1.2.3/usb_modeswitch.conf > $(TARGETDIR)/rom/etc/usb_modeswitch.conf

usb-modeswitch: libusb10
	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) -I$(TOP)/libusb10/libusb" LIBS="\
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread -ldl \
		-L$(TOP)/libusb10/libusb/.libs -lusb-1.0"

usb-modeswitch-install: usb-modeswitch
	install -D usb-modeswitch/usb_modeswitch $(INSTALLDIR)/usb-modeswitch/usr/sbin/usb_modeswitch
	$(STRIP) -s $(INSTALLDIR)/usb-modeswitch/usr/sbin/usb_modeswitch
	@mkdir -p $(TARGETDIR)/rom/etc
	@sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < $(TOP)/usb-modeswitch/usb_modeswitch.conf > $(TARGETDIR)/rom/etc/usb_modeswitch.conf

libusb-0.1.12/stamp-h1:
	cd libusb-0.1.12 && CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="-lpthread -ldl" \
	$(CONFIGURE) --prefix=/usr --disable-build-docs --disable-dependency-tracking
	-@$(MAKE) -C libusb-0.1.12 clean
	touch $@

libusb-0.1.12: libusb-0.1.12/stamp-h1
	$(MAKE) -C $@

libusb-0.1.12-install: libusb-0.1.12
#	install -D libusb-0.1.12/.libs/libusb-0.1.so.4.4.4 $(INSTALLDIR)/libusb-0.1.12/usr/lib/libusb-0.1.so.4.4.4
#	$(STRIP) $(INSTALLDIR)/libusb-0.1.12/usr/lib/*.so.*
#	cd $(INSTALLDIR)/libusb-0.1.12/usr/lib && \
#	ln -sf libusb-0.1.so.4.4.4 libusb-0.1.so.4 && \
#	ln -sf libusb-0.1.so.4.4.4 libusb.so
	@echo "do nothing"

libusb-0.1.12-clean:
	-@$(MAKE) -C libusb-0.1.12 clean
	@rm -f libusb-0.1.12/stamp-h1

libdaemon/stamp-h1:
	cd libdaemon && $(CONFIGURE) ac_cv_func_setpgrp_void=yes \
	--disable-dependency-tracking
	touch $@

libdaemon: libdaemon/stamp-h1
	$(MAKE) -C $@ && $(MAKE) $@-stage

libdaemon-install: libdaemon
	install -D libdaemon/libdaemon/.libs/libdaemon.so.0.5.0 $(INSTALLDIR)/libdaemon/usr/lib/libdaemon.so.0.5.0
	$(STRIP) $(INSTALLDIR)/libdaemon/usr/lib/*.so.*
	cd $(INSTALLDIR)/libdaemon/usr/lib && \
		ln -sf libdaemon.so.0.5.0 libdaemon.so && \
		ln -sf libdaemon.so.0.5.0 libdaemon.so.0

libdaemon-clean:
	-@$(MAKE) -C libdaemon distclean
	@rm -f libdaemon/stamp-h1

lsof: lsof/Makefile
	@$(SEP)
	$(MAKE) -C $@

lsof/Makefile:
	( cd lsof ; \
		LSOF_CC=$(CC) \
		LSOF_INCLUDE=$(TOOLCHAIN)/include \
		LSOF_VSTR="asuswrt" \
		./Configure -n linux \
	)

lsof-install:
	install -D lsof/lsof $(INSTALLDIR)/lsof/usr/sbin/lsof
	$(STRIP) $(INSTALLDIR)/lsof/usr/sbin/lsof

lsof-clean:
	( cd lsof ; ./Configure -clean )
	@rm -f lsof/Makefile

mtd-utils: util-linux lzo zlib
	$(MAKE) CPPFLAGS="-I$(STAGEDIR)/usr/include" \
		CFLAGS="-I$(TOP)/mtd-utils/include  -I$(TOP)/mtd-utils/ubi-utils/include" \
		LDFLAGS="-L$(STAGEDIR)/usr/lib" \
		WITHOUT_XATTR=1 -C $@

mtd-utils-install:
	$(MAKE) WITHOUT_XATTR=1 WITHOUT_LZO=1 DESTDIR=$(INSTALLDIR)/mtd-utils -C mtd-utils install

mtd-utils-clean:
	$(MAKE) -C mtd-utils clean

util-linux: util-linux/Makefile
	$(MAKE) -C $@/libuuid/src && $(MAKE) $@-stage

util-linux/Makefile: util-linux/configure
	$(MAKE) util-linux-configure

util-linux/configure:
	( cd util-linux ; ./autogen.sh )

util-linux-configure:
	( export scanf_cv_alloc_modifier=no; \
		cd util-linux ; \
		$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		--enable-libuuid \
		--disable-nls --disable-tls --disable-libblkid --disable-mount --disable-libmount \
		--disable-fsck --disable-cramfs --disable-partx --disable-uuidd --disable-mountpoint \
		--disable-fallocate --disable-unshare --disable-agetty \
		--disable-cramfs --disable-switch_root --disable-pivot_root \
		--disable-kill --disable-rename --disable-chsh-only-listed \
		--disable-schedutils --disable-wall --disable-pg-bell --disable-require-password \
		--disable-use-tty-group --disable-makeinstall-chown --disable-makeinstall-setuid\
		--without-ncurses --without-selinux --without-audit \
	)

util-linux-stage:
	$(MAKE) -C util-linux/libuuid/src DESTDIR=$(STAGEDIR) \
		install-usrlib_execLTLIBRARIES install-uuidincHEADERS

util-linux-install: util-linux
	install -D $(STAGEDIR)/usr/lib/libuuid.so.1 $(INSTALLDIR)/util-linux/usr/lib/libuuid.so.1
	$(STRIP) $(INSTALLDIR)/util-linux/usr/lib/*.so*

util-linux-clean:
	[ ! -f util-linux/Makefile ] || $(MAKE) -C util-linux distclean
	@rm -f util-linux/Makefile

odhcp6c: odhcp6c/Makefile
	@EXT_CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections -fPIC" \
	EXT_LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	PREFIX="/usr" \
	$(MAKE) -C odhcp6c

odhcp6c-install: odhcp6c
	install -D odhcp6c/src/odhcp6c $(INSTALLDIR)/odhcp6c/usr/sbin/odhcp6c
	$(STRIP) $(INSTALLDIR)/odhcp6c/usr/sbin/odhcp6c

odhcp6c-clean:
	-@$(MAKE) -C odhcp6c clean

ecmh/stamp-h1:
	@touch ecmh/src/stamp-h1

ecmh: ecmh/stamp-h1
	@$(MAKE) -C ecmh CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD)

ecmh-clean:
	-@$(MAKE) -C ecmh clean
	@rm -f ecmh/src/stamp-h1

ecmh-install: ecmh
	install -D ecmh/src/ecmh $(INSTALLDIR)/ecmh/bin/ecmh
	$(STRIP) $(INSTALLDIR)/ecmh/bin/ecmh

p910nd:
samba:
samba3:
	@$(MAKE) -C samba3
samba-3.6.x: libiconv-1.14
	@$(MAKE) -C samba-3.6.x $(PARALLEL_BUILD)
samba-3.5.8:
	@$(MAKE) -C samba-3.5.8 $(PARALLEL_BUILD)

#samba-3.5.8-install:
#	install -D samba-3.5.8/source3/bin/libsmbclient.so $(INSTALLDIR)/samba-3.5.8/usr/lib/libsmbclient.so
#	$(STRIP) $(INSTALLDIR)/samba-3.5.8/usr/lib/libsmbclient.so

nvram$(BCMEX): shared

prebuilt: shared

vlan:
	@$(SEP)
	@$(MAKE) -C vlan CROSS=$(CROSS_COMPILE)	# STRIPTOOL=$(STRIP)

vlan-install:
	$(MAKE) -C vlan CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install	# STRIPTOOL=$(STRIP)
	$(STRIP) $(INSTALLDIR)/vlan/usr/sbin/vconfig

accel-pptpd: kernel_header pppd accel-pptpd/pptpd-1.3.3/Makefile
	@$(MAKE) -C accel-pptpd server

accel-pptpd/pptpd-1.3.3/Makefile:
	$(MAKE) accel-pptpd-configure

accel-pptpd-configure:
	( cd accel-pptpd/pptpd-1.3.3 && CFLAGS="$(CFLAGS) -g -O2 $(EXTRACFLAGS) -idirafter $(KDIR)/include" \
		$(CONFIGURE) --prefix=/usr --bindir=/usr/sbin --libdir=/usr/lib \
			--enable-bcrelay KDIR=$(KDIR) PPPDIR=$(TOP)/pppd \
	)

accel-pptpd-clean:
	[ ! -f accel-pptpd/pptpd-1.3.3/Makefile ] || $(MAKE) -C accel-pptpd distclean
	@rm -f accel-pptpd/pptpd-1.3.3/Makefile

accel-pptpd-install: accel-pptpd
	$(MAKE) -C accel-pptpd server_install

pptpd/stamp-h1:
	cd $(TOP)/pptpd && $(CONFIGURE) --prefix=$(INSTALLDIR)/pptpd --enable-bcrelay CC=$(CC) STRIP=$(STRIP) AR=$(AR) LD=$(LD) NM=$(NM) RANLIB=$(RANLIB)
	touch $@

pptpd: pptpd/stamp-h1

pptpd-install: pptpd
	@echo pptpd
	@install -D pptpd/pptpd $(INSTALLDIR)/pptpd/usr/sbin/pptpd
	@install -D pptpd/bcrelay $(INSTALLDIR)/pptpd/usr/sbin/bcrelay
	@install -D pptpd/pptpctrl $(INSTALLDIR)/pptpd/usr/sbin/pptpctrl
	@$(STRIP) $(INSTALLDIR)/pptpd/usr/sbin/pptpd
	@$(STRIP) $(INSTALLDIR)/pptpd/usr/sbin/bcrelay
	@$(STRIP) $(INSTALLDIR)/pptpd/usr/sbin/pptpctrl

pptpd-clean:
	-@make -C pptpd clean
	@rm -f pptpd/stamp-h1

rp-pppoe/src/stamp-h1:
	cd rp-pppoe/src && CFLAGS="-g -O2 $(EXTRACFLAGS) -idirafter $(KDIR)/include" \
	$(CONFIGURE) --prefix=/usr --enable-plugin=$(TOP)/pppd \
	ac_cv_linux_kernel_pppoe=yes rpppoe_cv_pack_bitfields=rev --disable-debugging
	touch $@

rp-pppoe: pppd rp-pppoe/src/stamp-h1
	@$(MAKE) -C rp-pppoe/src pppoe-relay rp-pppoe.so

rp-pppoe-clean:
	-@$(MAKE) -C rp-pppoe/src clean
	@rm -f rp-pppoe/src/stamp-h1

rp-pppoe-install: rp-pppoe
	install -D rp-pppoe/src/rp-pppoe.so $(INSTALLDIR)/rp-pppoe/usr/lib/pppd/rp-pppoe.so
	$(STRIP) $(INSTALLDIR)/rp-pppoe/usr/lib/pppd/*.so
	install -D rp-pppoe/src/pppoe-relay $(INSTALLDIR)/rp-pppoe/usr/sbin/pppoe-relay
	$(STRIP) $(INSTALLDIR)/rp-pppoe/usr/sbin/pppoe-relay

rp-l2tp/stamp-h1: kernel_header
	cd rp-l2tp && CFLAGS="-O2 -Wall $(EXTRACFLAGS) -idirafter $(KDIR)/include $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)" \
	$(CONFIGURE) --prefix=/usr --sysconfdir=/tmp
	touch $@

rp-l2tp: pppd rp-l2tp/stamp-h1
	$(MAKE) -C rp-l2tp

rp-l2tp-clean:
	-@$(MAKE) -C rp-l2tp distclean
	@rm -f rp-l2tp/stamp-h1

rp-l2tp-install:
	install -d $(INSTALLDIR)/rp-l2tp/usr/lib/l2tp
	install rp-l2tp/handlers/*.so $(INSTALLDIR)/rp-l2tp/usr/lib/l2tp
	$(STRIP) $(INSTALLDIR)/rp-l2tp/usr/lib/l2tp/*.so
	install -D rp-l2tp/handlers/l2tp-control $(INSTALLDIR)/rp-l2tp/usr/sbin/l2tp-control
	$(STRIP) $(INSTALLDIR)/rp-l2tp/usr/sbin/l2tp-control
	install -D rp-l2tp/l2tpd $(INSTALLDIR)/rp-l2tp/usr/sbin/l2tpd
	$(STRIP) $(INSTALLDIR)/rp-l2tp/usr/sbin/l2tpd

xl2tpd: pppd
	CFLAGS="-g $(EXTRACFLAGS)" $(MAKE) -C $@ PREFIX=/usr xl2tpd

xl2tpd-install: xl2tpd
	install -D xl2tpd/xl2tpd $(INSTALLDIR)/xl2tpd/usr/sbin/xl2tpd
	$(STRIP) $(INSTALLDIR)/xl2tpd/usr/sbin/xl2tpd

pptp-client-install:
	install -D pptp-client/pptp $(INSTALLDIR)/pptp-client/usr/sbin/pptp
	$(STRIP) $(INSTALLDIR)/pptp-client/usr/sbin/pptp

accel-pptp/stamp-h1: kernel_header $(LINUXDIR)/include/linux/version.h
	cd accel-pptp && CFLAGS="-g -O2 $(EXTRACFLAGS) -idirafter $(KDIR)/include $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)" \
	$(CONFIGURE) --prefix=/usr KDIR=$(KDIR) PPPDIR=$(TOP)/pppd
	touch $@

accel-pptp: pppd accel-pptp/stamp-h1
	@$(MAKE) -C accel-pptp

accel-pptp-clean:
	-@$(MAKE) -C accel-pptp clean
	@rm -f accel-pptp/stamp-h1

accel-pptp-install: accel-pptp
	install -D accel-pptp/src/.libs/pptp.so $(INSTALLDIR)/accel-pptp/usr/lib/pppd/pptp.so
	$(STRIP) $(INSTALLDIR)/accel-pptp/usr/lib/pppd/pptp.so

pppd/stamp-h1: kernel_header
	cd pppd && $(CONFIGURE) --prefix=/usr --sysconfdir=/tmp
	touch $@

pppd: pppd/stamp-h1
	@$(SEP)
	@$(MAKE) -C pppd MFLAGS='$(if $(RTCONFIG_IPV6),HAVE_INET6=y,) \
	EXTRACFLAGS="$(EXTRACFLAGS) -idirafter $(KDIR)/include $(if $(RTCONFIG_VPNC),-DRTCONFIG_VPNC,)"'

pppd-clean:
	-@$(MAKE) -C pppd clean
	@rm -f pppd/stamp-h1

pppd-install:
	install -D pppd/pppd/pppd $(INSTALLDIR)/pppd/usr/sbin/pppd
	$(STRIP) $(INSTALLDIR)/pppd/usr/sbin/pppd
	install -D pppd/chat/chat $(INSTALLDIR)/pppd/usr/sbin/chat
	$(STRIP) $(INSTALLDIR)/pppd/usr/sbin/chat
ifeq ($(RTCONFIG_L2TP),y)
	install -D pppd/pppd/plugins/pppol2tp/pppol2tp.so $(INSTALLDIR)/pppd/usr/lib/pppd/pppol2tp.so
	$(STRIP) $(INSTALLDIR)/pppd/usr/lib/pppd/*.so
endif

ez-ipupdate-install:
	install -D ez-ipupdate/ez-ipupdate $(INSTALLDIR)/ez-ipupdate/usr/sbin/ez-ipupdate
	$(STRIP) $(INSTALLDIR)/ez-ipupdate/usr/sbin/ez-ipupdate

quagga/stamp-h1:
	@cd quagga && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	$(CONFIGURE) --disable-ripngd --disable-ospfd --disable-doc --disable-ospfclient \
	--disable-ospf6d --disable-bgpd --disable-bgp-announce --disable-babeld \
	--disable-isisd
	@touch $@

quagga: quagga/stamp-h1
	@$(MAKE) -C quagga CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) $(PARALLEL_BUILD)

quagga-clean:
	-@$(MAKE) -C quagga clean
	@rm -f quagga/stamp-h1

quagga-install:
	install -d $(INSTALLDIR)/quagga/usr/lib
	libtool --mode=install install -c quagga/lib/libzebra.la $(INSTALLDIR)/quagga/usr/lib
	libtool --finish $(INSTALLDIR)/quagga/usr/lib
	rm -f $(INSTALLDIR)/quagga/usr/lib/libzebra.a
	rm -f $(INSTALLDIR)/quagga/usr/lib/libzebra.la
	install -d $(INSTALLDIR)/quagga/usr/sbin
	install -d $(INSTALLDIR)/quagga/usr/local/etc
	libtool --mode=install install -c quagga/zebra/zebra $(INSTALLDIR)/quagga/usr/sbin
	install -c -m 777 quagga/zebra/zebra.conf.sample $(INSTALLDIR)/quagga/usr/local/etc/zebra.conf
	libtool --mode=install install -c quagga/ripd/ripd $(INSTALLDIR)/quagga/usr/sbin
	install -c -m 777 quagga/ripd/ripd.conf.sample $(INSTALLDIR)/quagga/usr/local/etc/ripd.conf
	libtool --mode=install install -c quagga/watchquagga/watchquagga $(INSTALLDIR)/quagga/usr/sbin
	$(STRIP) $(INSTALLDIR)/quagga/usr/sbin/zebra
	$(STRIP) $(INSTALLDIR)/quagga/usr/sbin/ripd
	$(STRIP) $(INSTALLDIR)/quagga/usr/sbin/watchquagga
	install -D $(shell dirname $(shell which $(CXX)))/../lib/librt.so.0 $(TARGETDIR)/lib/librt.so.0 

zebra/stamp-h1:
	@cd zebra && rm -f config.cache && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	$(CONFIGURE) --sysconfdir=/etc \
	--enable-netlink $(if $(RTCONFIG_IPV6),--enable-ipv6,--disable-ipv6) --disable-ripngd --disable-ospfd --disable-doc \
	--disable-ospf6d --disable-bgpd --disable-bgpd-announce
	@touch $@

zebra: zebra/stamp-h1
	@$(MAKE) -C zebra CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD)

zebra-clean:
	-@$(MAKE) -C zebra clean
	@rm -f zebra/stamp-h1

zebra-install: zebra
	install -D zebra/zebra/zebra $(INSTALLDIR)/zebra/usr/sbin/zebra
	install -D zebra/ripd/ripd $(INSTALLDIR)/zebra/usr/sbin/ripd
	install -D zebra/lib/libzebra.so $(INSTALLDIR)/zebra/usr/lib/libzebra.so
	$(STRIP) $(INSTALLDIR)/zebra/usr/sbin/zebra
	$(STRIP) $(INSTALLDIR)/zebra/usr/sbin/ripd
	$(STRIP) $(INSTALLDIR)/zebra/usr/lib/libzebra.so

bpalogin-install:
	install -D bpalogin/bpalogin $(INSTALLDIR)/bpalogin/usr/sbin/bpalogin
	$(STRIP) $(INSTALLDIR)/bpalogin/usr/sbin/bpalogin

wpa_supplicant-0.7.3/stamp-h1:
	touch $@

wpa_supplicant-0.7.3: $@/stamp-h1
	$(MAKE) -C $@/src/eap_peer

wpa_supplicant-0.7.3-install: wpa_supplicant-0.7.3
	install -D wpa_supplicant-0.7.3/src/eap_peer/libeap.so.0.0.0 $(INSTALLDIR)/wpa_supplicant-0.7.3/usr/lib/libeap.so.0.0.0
	$(STRIP) $(INSTALLDIR)/wpa_supplicant-0.7.3/usr/lib/libeap.so.0.0.0
	cd $(INSTALLDIR)/wpa_supplicant-0.7.3/usr/lib && \
	ln -sf libeap.so.0.0.0 $(INSTALLDIR)/wpa_supplicant-0.7.3/usr/lib/libeap.so.0
	ln -sf libeap.so.0.0.0 $(INSTALLDIR)/wpa_supplicant-0.7.3/usr/lib/libeap.so

wpa_supplicant-0.7.3-clean:
	-@$(MAKE) -C wpa_supplicant-0.7.3/src/eap_peer clean
	@rm -f wpa_supplicant-0.7.3/stamp-h1

gctwimax-0.0.3rc4/stamp-h1:
	touch $@

gctwimax-0.0.3rc4: $@/stamp-h1
	$(MAKE) -C $@

gctwimax-0.0.3rc4-install: gctwimax-0.0.3rc4
	install -D gctwimax-0.0.3rc4/gctwimax $(INSTALLDIR)/gctwimax-0.0.3rc4/usr/sbin/gctwimax
	$(STRIP) $(INSTALLDIR)/gctwimax-0.0.3rc4/usr/sbin/gctwimax
	#install -D gctwimax-0.0.3rc4/src/event.sh $(INSTALLDIR)/gctwimax-0.0.3rc4/usr/share/event.sh
	#install -D gctwimax-0.0.3rc4/src/gctwimax.conf $(INSTALLDIR)/gctwimax-0.0.3rc4/usr/share/gctwimax.conf

gctwimax-0.0.3rc4-clean:
	-@$(MAKE) -C gctwimax-0.0.3rc4 clean
	@rm -f gctwimax-0.0.3rc4/stamp-h1

wpa_supplicant:
	$(MAKE) -C $@/wpa_supplicant EXTRACFLAGS="-Os -I$(TOP)/openssl/include -L$(TOP)/openssl $(EXTRACFLAGS)" \
	LDFLAGS="-L$(TOP)/openssl"

wpa_supplicant-install: wpa_supplicant
	install -D wpa_supplicant/wpa_supplicant/wpa_supplicant $(INSTALLDIR)/wpa_supplicant/usr/sbin/wpa_supplicant
	install -D wpa_supplicant/wpa_supplicant/wpa_cli $(INSTALLDIR)/wpa_supplicant/usr/sbin/wpa_cli
	$(STRIP) $(INSTALLDIR)/wpa_supplicant/usr/sbin/*

wpa_supplicant-clean:
	-@$(MAKE) -C wpa_supplicant/wpa_supplicant clean

tr069:
	cp tr069/Makefile.ASUSWRT tr069/Makefile
	$(MAKE) -C $@
	$(MAKE) -C $@/test

tr069-clean:
	-$(MAKE) -C tr069 clean
	-$(MAKE) -C tr069/test clean

tr069-install: tr069
	install -D tr069/build/bin/tr $(INSTALLDIR)/tr069/sbin/tr069
	install -D tr069/test/sendtocli $(INSTALLDIR)/tr069/sbin/sendtocli
	install -D tr069/test/notify $(INSTALLDIR)/tr069/sbin/notify
	install -D tr069/test/udpclient $(INSTALLDIR)/tr069/sbin/udpclient
ifeq ($(RTCONFIG_TR181),y)
ifeq ($(RTCONFIG_DSL),y)
	install -D tr069/conf/ASUSWRT/tr_181.xml.dsl $(INSTALLDIR)/tr069/usr/tr/tr.xml
else
	install -D tr069/conf/ASUSWRT/tr_181.xml $(INSTALLDIR)/tr069/usr/tr/tr.xml
endif
else
ifeq ($(RTCONFIG_DSL),y)
	install -D tr069/conf/ASUSWRT/tr_98.xml.dsl $(INSTALLDIR)/tr069/usr/tr/tr.xml
else
	install -D tr069/conf/ASUSWRT/tr_98.xml $(INSTALLDIR)/tr069/usr/tr/tr.xml
endif
endif

nt_center/stamp-h1: 
	touch $@

nt_center: nt_center/stamp-h1
	$(MAKE) -C nt_center


nt_center-install: nt_center
	install -d $(INSTALLDIR)/nt_center/usr/lib/
	install -d $(INSTALLDIR)/nt_center/usr/sbin/
	install -D nt_center/lib/libnt.so $(INSTALLDIR)/nt_center/usr/lib/libnt.so
	install -D nt_center/nt_center $(INSTALLDIR)/nt_center/usr/sbin/nt_center
	install -D nt_center/nt_monitor $(INSTALLDIR)/nt_center/usr/sbin/nt_monitor
	install -D nt_center/actMail/nt_actMail $(INSTALLDIR)/nt_center/usr/sbin/nt_actMail
	install -D nt_center/Notify_Event2NC $(INSTALLDIR)/nt_center/usr/sbin/Notify_Event2NC
	install -D nt_center/lib/nt_db $(INSTALLDIR)/nt_center/usr/sbin/nt_db
	#install -D nt_center/lib/db_test $(INSTALLDIR)/nt_center/usr/sbin/db_test
	$(STRIP) $(INSTALLDIR)/nt_center/usr/lib/libnt.so
	$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/nt_center
	$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/nt_monitor
	$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/nt_actMail
	$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/Notify_Event2NC
	$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/nt_db
	#$(STRIP) $(INSTALLDIR)/nt_center/usr/sbin/db_test

nt_center-clean:
	-$(MAKE) -C nt_center clean
	@rm -f nt_center/stamp-h1

protect_srv/stamp-h1:
	touch $@

protect_srv-stage:
	$(MAKE) -C protect_srv/lib
	install -D protect_srv/lib/libptcsrv.so $(STAGEDIR)/usr/lib/libptcsrv.so
	install -D -m 644 -p protect_srv/include/protect_srv.h $(STAGEDIR)/usr/include/protect_srv.h
	install -D -m 644 -p protect_srv/include/libptcsrv.h $(STAGEDIR)/usr/include/libptcsrv.h

protect_srv: shared nvram$(BCMEX) protect_srv/stamp-h1
	$(MAKE) -C $@ && $(MAKE) $@-stage

protect_srv-install: protect_srv
	install -d $(INSTALLDIR)/protect_srv/usr/lib/
	install -d $(INSTALLDIR)/protect_srv/usr/sbin/
	install -D protect_srv/lib/libptcsrv.so $(INSTALLDIR)/protect_srv/usr/lib/libptcsrv.so
	install -D protect_srv/protect_srv $(INSTALLDIR)/protect_srv/usr/sbin/protect_srv
	install -D protect_srv/Send_Event2ptcsrv $(INSTALLDIR)/protect_srv/usr/sbin/Send_Event2ptcsrv
	$(STRIP) $(INSTALLDIR)/protect_srv/usr/lib/libptcsrv.so
	$(STRIP) $(INSTALLDIR)/protect_srv/usr/sbin/protect_srv

protect_srv-clean:
	-$(MAKE) -C protect_srv clean
	@rm -f protect_srv/stamp-h1

#	$(STRIP) $(INSTALLDIR)/tr/sbin/*

#	libnet:
#		@$(SEP)
#		@-mkdir -p libnet/lib
#		@$(MAKE) -C libnet CC=$(CC) AR=$(AR) RANLIB=$(RANLIB)

libpcap/stamp-h1:
	cd libpcap && $(CONFIGURE) --with-pcap=linux
	touch $@

libpcap: libpcap/stamp-h1
	@$(SEP)
	@$(MAKE) -C libpcap CC=$(CC) AR=$(AR) RANLIB=$(RANLIB)

libpcap-install: libpcap
	@echo "do nothing"
	#install -D libpcap/libpcap.so.1.4.0 $(INSTALLDIR)/libpcap/usr/lib/libpcap.so.1.4.0
	#$(STRIP) $(INSTALLDIR)/libpcap/usr/lib/libpcap.so.1.4.0
	#cd $(INSTALLDIR)/libpcap/usr/lib && ln -sf libpcap.so.1.4.0 libpcap.so

libpcap-clean:
	[ ! -f libpcap/Makefile ] || $(MAKE) -C libpcap clean
	@rm -f libpcap/stamp-h1

tcpdump-4.4.0/stamp-h1:
	cd tcpdump-4.4.0 && $(CONFIGURE) ac_cv_linux_vers=2
	touch $@

tcpdump-4.4.0:  libpcap tcpdump-4.4.0/stamp-h1
	@$(SEP)
	@$(MAKE) -C tcpdump-4.4.0 CC=$(CC) AR=$(AR) RANLIB=$(RANLIB)

tcpdump-4.4.0-install: tcpdump-4.4.0
	install -D tcpdump-4.4.0/tcpdump $(INSTALLDIR)/tcpdump-4.4.0/usr/sbin/tcpdump
	$(STRIP) $(INSTALLDIR)/tcpdump-4.4.0/usr/sbin/tcpdump

tcpdump-4.4.0-clean:
	[ ! -f tcpdump-4.4.0/Makefile ] || $(MAKE) -C tcpdump-4.4.0 clean
	@rm -f tcpdump-4.4.0/stamp-h1

libqcsapi_client-install: libqcsapi_client
	install -d $(INSTALLDIR)/libqcsapi_client/usr/lib/
	install -d $(INSTALLDIR)/libqcsapi_client/usr/sbin/
	install -D libqcsapi_client/libqcsapi_client.so.1.0.1 $(INSTALLDIR)/libqcsapi_client/usr/lib/
	install -D libqcsapi_client/qcsapi_sockrpc $(INSTALLDIR)/libqcsapi_client/usr/sbin/
	install -D libqcsapi_client/qcsapi_sockraw $(INSTALLDIR)/libqcsapi_client/usr/sbin/
	# install -D libqcsapi_client/c_rpc_qcsapi_sample $(INSTALLDIR)/libqcsapi_client/usr/sbin/
	$(STRIP) $(INSTALLDIR)/libqcsapi_client/usr/lib/*.so.*
	$(STRIP) $(INSTALLDIR)/libqcsapi_client/usr/sbin/*
	cd $(INSTALLDIR)/libqcsapi_client/usr/lib && \
		ln -sf libqcsapi_client.so.1.0.1 libqcsapi_client.so.1 && \
		ln -sf libqcsapi_client.so.1.0.1 libqcsapi_client.so

libbcm:
	@[ ! -f libbcm/Makefile ] || $(MAKE) -C libbcm

libbcm-install:
	install -D libbcm/libbcm.so $(INSTALLDIR)/libbcm/usr/lib/libbcm.so
	$(STRIP) $(INSTALLDIR)/libbcm/usr/lib/libbcm.so

.PHONY: libnl-tiny-0.1 $(SWCONFIG_PKG)
libnl-tiny-0.1:
	$(MAKE) -C $@

libnl-tiny-0.1-clean:
	$(MAKE) -C libnl-tiny-0.1 clean

libnl-tiny-0.1-install: libnl-tiny-0.1
	install -D libnl-tiny-0.1/libnl-tiny.so $(INSTALLDIR)/libnl-tiny-0.1/usr/lib/libnl-tiny.so
	$(STRIP) $(INSTALLDIR)/libnl-tiny-0.1/usr/lib/libnl-tiny.so

swconfig: libnl-tiny-0.1
	$(MAKE) -C $(SWCONFIG_PKG)

swconfig-clean:
	$(MAKE) -C $(SWCONFIG_PKG) clean

swconfig-install: swconfig
	install -D $(SWCONFIG_PKG)/swconfig $(INSTALLDIR)/swconfig/usr/sbin/swconfig
	$(STRIP) $(INSTALLDIR)/swconfig/usr/sbin/swconfig

iperf: iperf/Makefile
	@$(SEP)
	$(MAKE) -C $@

iperf/Makefile:
	# libstdc++.so.6 is required if you want to remove CFLAGS=-static below.
	( cd iperf ; CFLAGS=-static $(CONFIGURE) ac_cv_func_malloc_0_nonnull=yes )

iperf-install:
	install -D iperf/src/iperf $(INSTALLDIR)/iperf/usr/bin/iperf
	$(STRIP) $(INSTALLDIR)/iperf/usr/bin/iperf

iperf-clean:
	[ ! -f iperf/Makefile ] || $(MAKE) -C iperf distclean

libevent-2.0.21: libevent-2.0.21/Makefile
	$(MAKE) -C $@ $(PARALLEL_BUILD) && $(MAKE) $@-stage

libevent-2.0.21/Makefile:
	$(MAKE) libevent-2.0.21-configure

libevent-2.0.21-configure:
	( cd libevent-2.0.21 ; CFLAGS="$(CFLAGS) -Os -Wall $(EXTRACFLAGS)" \
		$(CONFIGURE) --prefix=/usr --bindir=/usr/sbin --libdir=/usr/lib \
	)

libevent-2.0.21-install: libevent-2.0.21
	install -D $(STAGEDIR)/usr/lib/libevent-2.0.so.5 $(INSTALLDIR)/libevent-2.0.21/usr/lib/libevent-2.0.so.5
	$(STRIP) $(INSTALLDIR)/libevent-2.0.21/usr/lib/libevent-2.0.so*

libevent-2.0.21-clean:
	[ ! -f libevent-2.0.21/Makefile ] || $(MAKE) -C libevent-2.0.21 clean
	@rm -f libevent-2.0.21/Makefile

tor: openssl zlib libevent-2.0.21 tor/Makefile
	@$(SEP)
	$(MAKE) -C $@ $(PARALLEL_BUILD)

tor/Makefile:
	(cd tor ; $(CONFIGURE) --enable-static-libevent --with-libevent-dir=$(STAGEDIR)/usr \
					--with-openssl-dir=$(TOP)/openssl \
					--with-zlib-dir=$(TOP)/zlib \
					--disable-asciidoc --disable-unittests \
					--disable-tool-name-check)

tor-install:
	install -D tor/src/or/tor $(INSTALLDIR)/tor/usr/sbin/Tor
	$(STRIP) $(INSTALLDIR)/tor/usr/sbin/Tor

tor-clean:
	[ ! -f tor/Makefile ] || $(MAKE) -C tor clean
	@rm -f tor/Makefile

iproute2:
	@$(SEP)
	@$(MAKE) -C $@ KERNEL_INCLUDE=$(LINUXDIR)/include EXTRACFLAGS="$(EXTRACFLAGS) $(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6)" $(PARALLEL_BUILD)

iproute2-install:
	install -D iproute2/tc/tc $(INSTALLDIR)/iproute2/usr/sbin/tc
	install -D iproute2/tc/tc $(INSTALLDIR)/iproute2/usr/sbin/realtc
	$(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/tc
	$(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/realtc
	install -D iproute2/ip/ip $(INSTALLDIR)/iproute2/usr/sbin/ip
	$(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/ip

iproute2-3.x: kernel_header iptables-1.4.x
	@$(SEP)
	@$(MAKE) -C $@ IPTABLES_DIR=$(TOP)/iptables-1.4.x KERNEL_INCLUDE=$(TOP)/kernel_header/include EXTRACFLAGS="$(EXTRACFLAGS) $(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6)" && $(MAKE) -C $@ stage

iproute2-3.x-install:
	install -D iproute2-3.x/tc/tc $(INSTALLDIR)/iproute2-3.x/usr/sbin/tc
	install -D iproute2-3.x/tc/tc $(INSTALLDIR)/iproute2-3.x/usr/sbin/realtc
	$(STRIP) $(INSTALLDIR)/iproute2-3.x/usr/sbin/tc
	$(STRIP) $(INSTALLDIR)/iproute2-3.x/usr/sbin/realtc
	install -D iproute2-3.x/ip/ip $(INSTALLDIR)/iproute2-3.x/usr/sbin/ip
	$(STRIP) $(INSTALLDIR)/iproute2-3.x/usr/sbin/ip
	@if [ -e iproute2-3.x/tc/m_xt.so ] ; then \
		install -D iproute2-3.x/tc/m_xt.so $(INSTALLDIR)/iproute2-3.x/usr/lib/tc/m_xt.so ; \
		ln -sf m_xt.so $(INSTALLDIR)/iproute2-3.x/usr/lib/tc/m_ipt.so ; \
		$(STRIP) $(INSTALLDIR)/iproute2-3.x/usr/lib/tc/*.so ; \
	fi

iproute2-3.x-clean:
	-@$(MAKE) -C iproute2-3.x clean
	-rm -f iproute2-3.x/Config

ntpclient: nvram$(BCMEX) shared

rstats: nvram$(BCMEX) shared


dropbear/config.h: dropbear/config.h.in $(if $(RTCONFIG_PROTECTION_SERVER),protect_srv-stage,)
	cd dropbear && \
		CPPFLAGS="-O2 -Wall $(EXTRACFLAGS) -DARGTYPE=3 -ffunction-sections -fdata-sections -I$(STAGEDIR)/usr/include" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -L$(STAGEDIR)/usr/lib" \
		LIBS="$(if $(RTCONFIG_PROTECTION_SERVER),-lptcsrv)" \
		$(CONFIGURE) --disable-zlib --disable-pam \
		--enable-openpty --enable-syslog --enable-shadow --enable-bundled-libtom \
		--disable-lastlog \
		--disable-utmp --disable-utmpx \
		--disable-wtmp --disable-wtmpx \
		--disable-loginfunc \
		--disable-pututline --disable-pututxline


dropbear: dropbear/config.h
	@$(SEP)
	@$(MAKE) -C $@ PROGRAMS="dropbear dbclient dropbearkey scp" MULTI=1 $(PARALLEL_BUILD)

dropbear-install:
	install -D dropbear/dropbearmulti $(INSTALLDIR)/dropbear/usr/bin/dropbearmulti
	$(STRIP) $(INSTALLDIR)/dropbear/usr/bin/dropbearmulti
	cd $(INSTALLDIR)/dropbear/usr/bin && \
	ln -sf dropbearmulti dropbearkey && \
	ln -sf dropbearmulti dbclient && \
	ln -sf dropbearmulti ssh && \
	ln -sf dropbearmulti scp
	install -d $(INSTALLDIR)/dropbear/usr/sbin
	cd $(INSTALLDIR)/dropbear/usr/sbin && \
	ln -sf ../bin/dropbearmulti dropbear

dropbear-clean:
	-@$(MAKE) -C dropbear clean
	@rm -f dropbear/config.h

wtfast-configure:
	@true

wtfast-install:
	install -D wtfast/wtfslhd/wtfslhd $(INSTALLDIR)/wtfast/usr/sbin/wtfslhd

# Media libraries

sqlite/stamp-h1:
	cd sqlite && \
	CC=$(CC) CFLAGS="-O2 $(EXTRACFLAGS) -ffunction-sections -fdata-sections -fPIC" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -lpthread -ldl" \
		$(CONFIGURE) --prefix=/usr --enable-shared --enable-static \
		--disable-readline --disable-dynamic-extensions --enable-threadsafe \
		--disable-dependency-tracking
	touch $@

sqlite: sqlite/stamp-h1
	@$(MAKE) -C sqlite all

sqlite-clean:
	-@$(MAKE) -C sqlite clean
	@rm -f sqlite/stamp-h1

sqlite-install:
	@$(SEP)

# commented out for mt-daapd-svn-1696
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D sqlite/.libs/libsqlite3.so.0 $(INSTALLDIR)/sqlite/usr/lib/libsqlite3.so.0
	$(STRIP) $(INSTALLDIR)/sqlite/usr/lib/libsqlite3.so.0
endif

ifeq ($(RTCONFIG_FREERADIUS),y)
	install -D sqlite/.libs/sqlite3 $(INSTALLDIR)/sqlite/usr/sbin/sqlite3
	$(STRIP) $(INSTALLDIR)/sqlite/usr/sbin/sqlite3
endif

FFMPEG_FILTER_CONFIG= $(foreach c, $(2), --$(1)="$(c)")

FFMPEG_DECODERS:=aac ac3 atrac3 h264 jpegls mp3 mpeg1video mpeg2video mpeg4 mpeg4aac mpegvideo png wmav1 wmav2 svq3
FFMPEG_CONFIGURE_DECODERS:=$(call FFMPEG_FILTER_CONFIG,enable-decoder,$(FFMPEG_DECODERS))

FFMPEG_PARSERS:=aac ac3 h264 mpeg4video mpegaudio mpegvideo
FFMPEG_CONFIGURE_PARSERS:=$(call FFMPEG_FILTER_CONFIG,enable-parser,$(FFMPEG_PARSERS))

FFMPEG_PROTOCOLS:=file
FFMPEG_CONFIGURE_PROTOCOLS:=$(call FFMPEG_FILTER_CONFIG,enable-protocol,$(FFMPEG_PROTOCOLS))

FFMPEG_DISABLED_DEMUXERS:=amr apc ape ass bethsoftvid bfi c93 daud dnxhd dsicin dxa ffm gsm gxf idcin iff image2 image2pipe ingenient ipmovie lmlm4 mm mmf msnwc_tcp mtv mxf nsv nut oma pva rawvideo rl2 roq rpl segafilm shorten siff smacker sol str thp tiertexseq tta txd vmd voc wc3 wsaud wsvqa xa yuv4mpegpipe
FFMPEG_CONFIGURE_DEMUXERS:=$(call FFMPEG_FILTER_CONFIG,disable-demuxer,$(FFMPEG_DISABLED_DEMUXERS))

ffmpeg/stamp-h1: zlib
	cd ffmpeg && symver_asm_label=no symver_gnu_asm=no symver=no CC=$(CC) LDFLAGS="-ldl"\
		./configure --enable-cross-compile --arch=$(ARCH) --target_os=linux \
		--cross-prefix=$(CROSS_COMPILE) --enable-shared --enable-gpl --disable-doc \
		--enable-pthreads --enable-small --disable-encoders --disable-filters \
		--disable-muxers --disable-devices --disable-ffmpeg --disable-ffplay \
		--disable-ffserver --disable-ffprobe --disable-avdevice --disable-swscale \
		--disable-hwaccels --disable-network --disable-bsfs \
		--enable-demuxers $(FFMPEG_CONFIGURE_DEMUXERS) \
		--disable-decoders $(FFMPEG_CONFIGURE_DECODERS) \
		--disable-parsers $(FFMPEG_CONFIGURE_PARSERS) \
		--disable-protocols $(FFMPEG_CONFIGURE_PROTOCOLS) \
		--disable-postproc --disable-avfilter \
		--extra-cflags="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections -fPIC -I$(TOP)/zlib" \
		--extra-ldflags="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
		--extra-libs="-L$(TOP)/zlib -lz" \
		--enable-zlib --disable-debug --prefix=''
	touch $@

ffmpeg: ffmpeg/stamp-h1 zlib
	@$(MAKE) -C ffmpeg all $(PARALLEL_BUILD)

ffmpeg-clean:
	-@$(MAKE) -C ffmpeg clean
	@rm -f ffmpeg/stamp-h1 ffmpeg/config.h ffmpeg/config.mak

ffmpeg-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D ffmpeg/libavformat/libavformat.so.52 $(INSTALLDIR)/ffmpeg/usr/lib/libavformat.so.52
	install -D ffmpeg/libavcodec/libavcodec.so.52 $(INSTALLDIR)/ffmpeg/usr/lib/libavcodec.so.52
	install -D ffmpeg/libavutil/libavutil.so.50 $(INSTALLDIR)/ffmpeg/usr/lib/libavutil.so.50
	$(STRIP) $(INSTALLDIR)/ffmpeg/usr/lib/libavformat.so.52
	$(STRIP) $(INSTALLDIR)/ffmpeg/usr/lib/libavcodec.so.52
	$(STRIP) $(INSTALLDIR)/ffmpeg/usr/lib/libavutil.so.50
endif

libogg/stamp-h1:
	cd libogg && \
	CFLAGS="-Os $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	LDFLAGS="-fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix=''
	touch $@

libogg: libogg/stamp-h1
	@$(MAKE) -C libogg all

libogg-clean:
	-@$(MAKE) -C libogg clean
	@rm -f libogg/stamp-h1

libogg-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libogg/src/.libs/libogg.so.0 $(INSTALLDIR)/libogg/usr/lib/libogg.so.0
	$(STRIP) $(INSTALLDIR)/libogg/usr/lib/libogg.so.0
endif

flac/stamp-h1: libogg
	cd flac && \
	CFLAGS="-Os $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	LDFLAGS="-fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix='' --disable-rpath \
		--disable-doxygen-docs --disable-xmms-plugin --disable-cpplibs \
		--without-libiconv-prefix --disable-altivec --disable-3dnow --disable-sse \
		--with-ogg-includes=$(TOP)/libogg/include --with-ogg-libraries=$(TOP)/libogg/src/.libs
	touch $@

flac: flac/stamp-h1 libogg
	@$(MAKE) -C flac/src/libFLAC all $(PARALLEL_BUILD)

flac-clean:
	-@$(MAKE) -C flac clean
	@rm -f flac/stamp-h1

flac-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D flac/src/libFLAC/.libs/libFLAC.so.8 $(INSTALLDIR)/flac/usr/lib/libFLAC.so.8
	$(STRIP) $(INSTALLDIR)/flac/usr/lib/libFLAC.so.8
endif

jpeg/stamp-h1:
	cd jpeg && \
	CFLAGS="-Os $(EXTRACFLAGS) -fPIC" CC=$(CC) AR2="touch" $(CONFIGURE) --enable-maxmem=1 --enable-shared --enable-static --prefix=''
	touch $@

jpeg: jpeg/stamp-h1
	@$(MAKE) -C jpeg LIBTOOL="" O=o A=a CC=$(CC) AR2="touch" libjpeg.a libjpeg.so $(PARALLEL_BUILD)

jpeg-clean:
	-@$(MAKE) -C jpeg clean
	@rm -f jpeg/stamp-h1

jpeg-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D jpeg/libjpeg.so $(INSTALLDIR)/jpeg/usr/lib/libjpeg.so
	$(STRIP) $(INSTALLDIR)/jpeg/usr/lib/libjpeg.so
endif

libexif/stamp-h1:
	cd libexif && CFLAGS="-Os -Wall $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	LDFLAGS="-fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix='' \
		--disable-docs --disable-rpath --disable-nls --without-libiconv-prefix --without-libintl-prefix
	touch $@

libexif: libexif/stamp-h1
	@$(MAKE) -C libexif all $(PARALLEL_BUILD)

libexif-clean:
	-@$(MAKE) -C libexif clean
	@rm -f libexif/stamp-h1

libexif-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libexif/libexif/.libs/libexif.so.12 $(INSTALLDIR)/libexif/usr/lib/libexif.so.12
	$(STRIP) $(INSTALLDIR)/libexif/usr/lib/libexif.so.12
endif

zlib/stamp-h1:
	cd zlib && \
	CC=$(CC) AR="ar rc" RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS) -fPIC" LDSHAREDLIBC="$(EXTRALDFLAGS)" \
	./configure --shared --prefix=/usr --libdir=/usr/lib
	touch $@

zlib: zlib/stamp-h1
	@$(MAKE) -C zlib CC=$(CC) AR="ar rc" RANLIB=$(RANLIB) LD=$(LD) all $(PARALLEL_BUILD) && $(MAKE) $@-stage

zlib-clean:
	-@$(MAKE) -C zlib clean
	@rm -f zlib/stamp-h1

zlib-install:
	@$(SEP)
# commented out for mt-daapd-svn-1696
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -d $(INSTALLDIR)/zlib/usr/lib
	install -D zlib/libz.so.1 $(INSTALLDIR)/zlib/usr/lib/
	$(STRIP) $(INSTALLDIR)/zlib/usr/lib/libz.so.1
endif
ifeq ($(or $(RTCONFIG_USB_BECEEM),$(RTCONFIG_MEDIA_SERVER),$(RTCONFIG_CLOUDSYNC),$(RTCONFIG_PUSH_EMAIL)),y)
	install -d $(INSTALLDIR)/zlib/usr/lib
	install -D zlib/libz.so.1 $(INSTALLDIR)/zlib/usr/lib/
	$(STRIP) $(INSTALLDIR)/zlib/usr/lib/libz.so.1
endif

libid3tag/stamp-h1: zlib
	cd libid3tag && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" CPPFLAGS="-I$(TOP)/zlib" \
	LDFLAGS="-L$(TOP)/zlib -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix='' \
		--disable-debugging --disable-profiling --disable-dependency-tracking
	touch $@

libid3tag: libid3tag/stamp-h1 zlib
	@$(MAKE) -C libid3tag all $(PARALLEL_BUILD)

libid3tag-clean:
	-@$(MAKE) -C libid3tag clean
	@rm -f libid3tag/stamp-h1

libid3tag-install:
	@$(SEP)
#commented out for mt-daapd-svn-1696
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libid3tag/.libs/libid3tag.so.0 $(INSTALLDIR)/libid3tag/usr/lib/libid3tag.so.0
	$(STRIP) $(INSTALLDIR)/libid3tag/usr/lib/libid3tag.so.0
endif

libvorbis/stamp-h1: libogg
	cd libvorbis && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	CPPFLAGS="-I$(TOP)/libogg/include" \
	LDFLAGS="-L$(TOP)/libogg/src/.libs -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -ldl" \
	$(CONFIGURE) --enable-shared --enable-static --prefix='' --disable-oggtest \
		--with-ogg-includes="$(TOP)/libogg/include" \
		--with-ogg-libraries="$(TOP)/libogg/src/.libs"
	touch $@
	touch libvorbis/aclocal.m4

libvorbis: libvorbis/stamp-h1
	@$(MAKE) -C libvorbis/lib all $(PARALLEL_BUILD)

libvorbis-clean:
	-@$(MAKE) -C libvorbis clean
	@rm -f libvorbis/stamp-h1

libvorbis-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libvorbis/lib/.libs/libvorbis.so.0 $(INSTALLDIR)/libvorbis/usr/lib/libvorbis.so.0
	$(STRIP) $(INSTALLDIR)/libvorbis/usr/lib/libvorbis.so.0
endif

minidlna: zlib sqlite ffmpeg libogg flac jpeg libexif libid3tag libvorbis minidlna/Makefile
	@$(SEP)
	@$(MAKE) -C minidlna CC=$(CC) $(if $(RTCONFIG_LIMIT_MEDIA_SERVER),MS_LIMIT=1) $(PARALLEL_BUILD)

minidlna/Makefile: minidlna/configure
	$(MAKE) minidlna-configure

minidlna/configure:
	( cd minidlna; ./autogen.sh )

minidlna-configure:
	cd minidlna && \
	LIBS="-ldl -lm -lpthread -lz -lexif -ljpeg -lsqlite3 -lid3tag -lFLAC -lvorbis -logg -lavformat -lavcodec -lavutil" \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -ffunction-sections -fdata-sections \
		$(if $(RTN65U,y),-DRTN56U=1) $(if $(or $(RTN66U), $(RTAC66U)),-DRTN66U=1)" \
		$(if $(RTAC68U,y),-I$(SRCBASE)/include) \
	CPPFLAGS="-I$(TOP)/zlib \
		-I$(TOP)/ffmpeg/libavutil -I$(TOP)/ffmpeg/libavcodec -I$(TOP)/ffmpeg/libavformat \
		-I$(TOP)/ffmpeg/libswscale -I$(TOP)/ffmpeg \
		-I$(TOP)/flac/include -I$(TOP)/sqlite -I$(TOP)/jpeg \
		-I$(TOP)/libexif -I$(TOP)/libid3tag -I$(TOP)/libogg/include \
		-I$(TOP)/libvorbis/include -I$(TOP)/shared" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections \
		-L$(TOP)/zlib -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections \
		-L$(TOP)/zlib -L$(TOP)/sqlite/.libs -L$(TOP)/jpeg -L$(TOP)/libvorbis/lib/.libs \
		-L$(TOP)/libogg/src/.libs -L$(TOP)/libexif/libexif/.libs -L$(TOP)/flac/src/libFLAC/.libs \
		-L$(TOP)/ffmpeg/libavutil -L$(TOP)/ffmpeg/libavcodec -L$(TOP)/ffmpeg/libavformat \
		-L$(TOP)/libid3tag/.libs" \
		$(if $(RTAC68U,y),-L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared) \
	ac_cv_header_linux_netlink_h=no \
	$(CONFIGURE)  --with-db-path=/tmp/minidlna --with-log-path=/tmp/minidlna \
		--with-os-name=Asuswrt --with-os-version=378.xx --with-os-url="http://asuswrt.lostrealm.ca" \
		--enable-tivo && \
	echo "#undef USE_DAEMON" >> config.h

minidlna-clean:
	-@$(MAKE) -C minidlna clean
	@rm -f minidlna/Makefile
	@rm -f minidlna/configure

minidlna-install:
	install -D minidlna/minidlnad $(INSTALLDIR)/minidlna/usr/sbin/minidlna
	$(STRIP) $(INSTALLDIR)/minidlna/usr/sbin/minidlna

libgdbm/stamp-h1:
	cd libgdbm && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	CPPFLAGS="-I$(TOP)/zlib" \
	LDFLAGS="-fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix=''
	touch $@

libgdbm: libgdbm/stamp-h1
	@$(MAKE) -C libgdbm

libgdbm-clean:
	-@$(MAKE) -C libgdbm clean
	@rm -f libgdbm/stamp-h1

libgdbm-install:
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libgdbm/.libs/libgdbm.so.3.0.0 $(INSTALLDIR)/libgdbm/usr/lib/libgdbm.so.3.0.0
	$(STRIP) $(INSTALLDIR)/libgdbm/usr/lib/libgdbm.so.3.0.0
	cd $(INSTALLDIR)/libgdbm/usr/lib && \
		ln -sf libgdbm.so.3.0.0 libgdbm.so.3 && \
		ln -sf libgdbm.so.3.0.0 libgdbm.so
endif

mt-daapd: zlib libid3tag libgdbm
	@$(SEP)
	@$(MAKE) -C mt-daapd CC=$(CC) $(if $(MEDIA_SERVER_STATIC),STATIC=1) all $(PARALLEL_BUILD)

mDNSResponder:
	@$(SEP)
	@$(MAKE) -C mDNSResponder CC=$(CC) all $(PARALLEL_BUILD)


igmpproxy/Makefile:
	cd igmpproxy && CFLAGS="-O2 -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --prefix=/usr --disable-dependency-tracking

igmpproxy: igmpproxy/Makefile
	@$(SEP)
	@$(MAKE) -C igmpproxy/src

igmpproxy-install: igmpproxy
	install -D igmpproxy/src/igmpproxy $(INSTALLDIR)/igmpproxy/usr/sbin/igmpproxy
	$(STRIP) $(INSTALLDIR)/igmpproxy/usr/sbin/igmpproxy

igmpproxy-clean:
	[ ! -f igmpproxy/Makefile ] || $(MAKE) -C igmpproxy distclean
	@rm -f igmpproxy/Makefile

snooper:
ifeq ($(RTCONFIG_QCA),y)
	@true
else
	$(MAKE) -C snooper EXTRACFLAGS="-I$(SRCBASE)/include $(EXTRACFLAGS)" \
		SWITCH=$(if $(RTCONFIG_RALINK),ralink,broadcom)
endif

snooper-install:
ifeq ($(RTCONFIG_QCA),y)
	@true
else
	install -D snooper/snooper $(INSTALLDIR)/snooper/usr/sbin/snooper
	$(STRIP) $(INSTALLDIR)/snooper/usr/sbin/snooper
endif

snooper-clean:
	$(MAKE) -C snooper clean

udpxy:
	$(MAKE) -C $@ release NO_UDPXREC=yes \
	CDEFS="-O2 -ffunction-sections -fdata-sections $(EXTRACFLAGS)" \
	LDFLAGS="-Wl,--gc-sections"

udpxy-install:
	install -D udpxy/udpxy $(INSTALLDIR)/udpxy/usr/sbin/udpxy
	$(STRIP) $(INSTALLDIR)/udpxy/usr/sbin/udpxy

netconf: $(IPTABLES)
	+$(MAKE) LINUXDIR=$(LINUXDIR) -C netconf

igmp: netconf
	$(MAKE) -C igmp

igmp-install: netconf
	install -d $(TARGETDIR)
	+$(MAKE) -C igmp INSTALLDIR=$(INSTALLDIR) install

igmp-clean:
	$(MAKE) -C igmp clean

wps$(BCMEX)$(EX7): nvram$(BCMEX) shared libupnp$(BCMEX)$(EX7)
ifeq ($(RTCONFIG_WPS),y)
	-rm -rf $(OLD_SRC)/wps
	-ln -s $(SRCBASE)/wps $(OLD_SRC)/wps
	[ ! -f wps$(BCMEX)$(EX7)/Makefile ] || $(MAKE) -C wps$(BCMEX)$(EX7) EXTRA_LDFLAGS=$(EXTRALDFLAGS)
else
	# Prevent to use generic rules"
	@true
endif

wps$(BCMEX)$(EX7)-install:
ifeq ($(RTCONFIG_WPS),y)
	[ ! -f wps$(BCMEX)$(EX7)/Makefile ] || $(MAKE) -C wps$(BCMEX)$(EX7) INSTALLDIR=$(INSTALLDIR) install
else
	# Prevent to use generic rules"
	@true
endif

wps$(BCMEX)$(EX7)-clean:
ifeq ($(RTCONFIG_WPS),y)
	[ ! -f wps$(BCMEX)$(EX7)/Makefile ] || $(MAKE) -C wps$(BCMEX)$(EX7) clean
else
	# Prevent to use generic rules"
	@true
endif

CHIP=0
CLM_BLOBS=router
CLM_BLOB_IMAGES := $(CLM_BLOBS:=.clm_bin)
OBJCOPY         := $(CROSS_COMPILE)objcopy
OBJCOPYFLAGS    := -R .reginfo -R .note -R .comment -R .mdebug -R .logstrs -R .lognums -S
OBJCOPYBIN      := $(OBJCOPY) -O binary $(OBJCOPYFLAGS)
CLMBASE=$(SRCBASE_FW)/wl/clm
ifeq ($(wildcard $(CLMBASE)/private),)
%.clm_bin: $(info "CLM In $(CLMBASE)/src/ NOT CREATING new clm_blob ...");
ifeq ($(CONFIG_DHDAP),y)
ifneq ($(wildcard $(SRCBASE_FW)/wl/clm/private),)
	cp $(SRCBASE_FW)/wl/clm/src/$*.clm_blob $(CLMBASE)/src
	mkdir -p $(SRCBASE_FW)/$(WLAN_GEN_BASEDIR)/wl/clm/src
	$(foreach clmb,$(CLM_BLOBS), \
		cp $(CLMBASE)/src/$(clmb).clm_blob $(SRCBASE_FW)/$(WLAN_GEN_BASEDIR)/wl/clm/src/$(clmb).clm_blob.GEN)
endif
endif
else
%.clm_bin: CHIPID := $(shell if [[ $(CHIP) -gt 5999 ]] ; then echo $(CHIP); else echo $$((0x$(CHIP)));  fi)
%.clm_bin: $(CLMBASE)/types/%.clm $(CLMBASE)/private/wlc_clm_data.xml
	@echo ClmCompiling config file $*.clm to $*.clm_bin and $*.clm_bin.  Config file content:
	cat $(CLMBASE)/types/$*.clm
	$(SRCBASE_FW)/tools/build/ClmCompiler --clmapi_include_dir $(CLMBASE)/include --bcmwifi_include_dir $(SRCBASE_FW)/shared/bcmwifi/include --config_file $(CLMBASE)/types/$*.clm $(CLMBASE)/private/wlc_clm_data.xml $(CLMBASE)/src/$*.c
	#@echo C compile $*.c to $*.o
	##$(LOUD)$(CC) $(call file_cflags,$*.c) -D_FILENAME_=\"$*.c\" -c -o $*.o $*.c
	$(LOUD)$(CC) $(call file_cflags,$*.c) -D_FILENAME_=\"$*.c\" -I$(CLMBASE)/include -I$(SRCBASE_FW)/shared/bcmwifi/include -c -o $(CLMBASE)/src/$*.o $(CLMBASE)/src/$*.c
	#@echo Linking $*.o to $*.exe
	( \
		echo 'SECTIONS {'; \
		echo '  . = 0;'; \
		echo '  .clmdata : {'; \
		echo '      *(.*.clm_header)'; \
		echo '      *(.rodata.*)'; \
		echo '   }'; \
		echo '}'; \
	) >$(CLMBASE)/src/$*.lds
	@echo Done $*.lds .....
	##$(LD) -S -Map=$*.map -T $*.lds -o $*.exe $*.o
	$(LD) -S -Map=$(CLMBASE)/src/$*.map -T $(CLMBASE)/src/$*.lds -o $(CLMBASE)/src/$*.exe $(CLMBASE)/src/$*.o
	#$(SIZE) $(CLMBASE)/src/$*.exe
	@echo Done $*.exe .....
	#@echo objcopy $*.exe to $*.clm_bin
	$(OBJCOPYBIN) $(CLMBASE)/src/$*.exe $(CLMBASE)/src/$*.clm_bin
	@echo Done $*.clm_bin
	@echo Create clm_blob .....
	python $(SRCBASE_FW)/tools/build/blob.py $(CLMBASE)/src/$*.clm_bin $(CLMBASE)/src/$*.clm_blob '$(CHIPID)'
	$(RM) $(CLMBASE)/src/$*.o $(CLMBASE)/src/$*.lds $(CLMBASE)/src/$*.map $(CLMBASE)/src/$*.exe
ifneq ($(WLAN_GEN_BASEDIR),)
	mkdir -p $(SRCBASE_FW)/$(WLAN_GEN_BASEDIR)/wl/clm/src
	$(foreach clmb,$(CLM_BLOBS), \
		cp $(CLMBASE)/src/$(clmb).clm_blob $(SRCBASE_FW)/$(WLAN_GEN_BASEDIR)/wl/clm/src/$(clmb).clm_blob.GEN)
endif
endif

clm: $(CLM_BLOB_IMAGES)

clm-install:
	@echo clm-install INSTALLDIR = $(INSTALLDIR) ....
	@echo clm-install TARGETDIR = $(TARGETDIR) ....
	install -d $(TARGETDIR)/brcm/clm
	install -D $(SRCBASE_FW)/wl/clm/src/*.clm_blob $(TARGETDIR)/brcm/clm

clm-clean:
	$(foreach clmb,$(CLM_BLOBS), \
		@echo Removing from $(CLMBASE)/src $(clmb).c $(clmb).clm_bin $(clmb).clm_blob; \
		$(RM) $(CLMBASE)/src/$(clmb).c; \
		$(RM) $(CLMBASE)/src/$(clmb).clm_bin; \
		$(RM) $(TARGETDIR)/brcm/clm/$(clmb).clm_blob )

SRC_WLEXE = $(SRCBASE_SYS)/wl/exe
SRC_PRE_WLEXE = $(SRCBASE)/wl/exe

.PHONY: wlexe

$(SRC_WLEXE)/../../include/epivers.h :
ifneq ($(wildcard $(SRC_WLEXE)/GNUmakefile),)
	$(MAKE) -C $(SRC_WLEXE)/../../include
endif

wlexe: $(SRC_WLEXE)/../../include/epivers.h
ifeq ($(RTCONFIG_WLEXE),y)
ifneq ($(wildcard $(SRC_WLEXE)/*.c),)

ifneq ($(wildcard $(SRC_WLEXE)/GNUmakefile),)
ifeq ($(ARCH),arm)
	$(MAKE) TARGETARCH=arm_le ARCH_SFX="" TARGET_PREFIX=$(CROSS_COMPILE) -C $(SRC_WLEXE)
else
	$(MAKE) TARGETARCH=mips ARCH_SFX="" TARGET_PREFIX=$(CROSS_COMPILE) -C $(SRC_WLEXE)
endif
	[ -d wlexe ] || install -d wlexe
	install $(SRC_WLEXE)/wl wlexe

ifeq ($(ARCH),arm)
	install $(SRC_WLEXE)/socket_noasd/arm_le/wl_server_socket wlexe
else
	install $(SRC_WLEXE)/socket_noasd/mips/wl_server_socket wlexe
endif
endif

else
	[ -d wlexe ] || install -d wlexe
	install $(SRC_PRE_WLEXE)/prebuilt/wl wlexe

endif
endif

.PHONY: wlexe-clean
wlexe-clean:
ifneq ($(wildcard $(SRC_WLEXE)/GNUmakefile),)
	$(MAKE) TARGETARCH=arm_le TARGET_PREFIX=$(CROSS_COMPILE) -C $(SRC_WLEXE) clean
	$(RM) wlexe/wl	
	$(RM) wlexe/wl_server_socket
endif

.PHONY: wlexe-install
wlexe-install:
ifeq ($(RTCONFIG_WLEXE),y)
ifneq ($(wildcard $(SRC_WLEXE)/GNUmakefile),)
	[ ! -d wlexe ] || install -D -m 755 wlexe/wl $(INSTALLDIR)/wlexe/usr/sbin/wl
	[ ! -d wlexe ] || install -D -m 755 wlexe/wl_server_socket $(INSTALLDIR)/wlexe/usr/sbin/wl_server_socket
	[ ! -d wlexe ] || $(STRIP) $(INSTALLDIR)/wlexe/usr/sbin/wl
	[ ! -d wlexe ] || $(STRIP) $(INSTALLDIR)/wlexe/usr/sbin/wl_server_socket
endif
endif

udev:
	$(MAKE) -C $@ CROSS_COMPILE=$(CROSS_COMPILE) EXTRACFLAGS="$(EXTRACFLAGS)" \
		PROGRAMS=udevtrigger

udev-install: udev
	install -d $(INSTALLDIR)
	install -d $(TARGETDIR)
	$(MAKE) -C udev DESTDIR=$(INSTALLDIR) prefix=/udev install-udevtrigger

hotplug2:
	$(MAKE) -C $@ CROSS_COMPILE=$(CROSS_COMPILE) EXTRACFLAGS="$(EXTRACFLAGS)"

hotplug2-install: hotplug2
	$(MAKE) -C hotplug2 install PREFIX=$(INSTALLDIR)/hotplug2 SUBDIRS=""
	$(MAKE) -C hotplug2/examples install PREFIX=$(INSTALLDIR)/hotplug2/rom KERNELVER=$(LINUX_KERNEL)

emf$(BCMEX)$(EX7):
	$(MAKE) -C emf$(BCMEX)$(EX7)/emfconf CROSS=$(CROSS_COMPILE)

emf$(BCMEX)$(EX7)-install:
ifeq ($(RTCONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf$(BCMEX)$(EX7)/emfconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

emf$(BCMEX)$(EX7)-clean:
	-@$(MAKE) -C emf$(BCMEX)$(EX7)/emfconf clean

igs$(BCMEX)$(EX7):
	$(MAKE) -C emf$(BCMEX)$(EX7)/igsconf CROSS=$(CROSS_COMPILE)

igs$(BCMEX)$(EX7)-install:
ifeq ($(RTCONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf$(BCMEX)$(EX7)/igsconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

igs$(BCMEX)$(EX7)-clean:
	-@$(MAKE) -C emf$(BCMEX)$(EX7)/igsconf clean

ebtables: kernel_header dummy
	$(MAKE) -C ebtables CC=$(CC) LD=$(LD) \
	CFLAGS="-Os $(EXTRACFLAGS) -Wall -Wunused" $(PARALLEL_BUILD) \
	BINDIR="/usr/sbin" LIBDIR="/usr/lib/ebtables" KERNEL_INCLUDES=$(TOP)/kernel_header/include $(if $(RTCONFIG_IPV6),DO_IPV6=1,) $(if $(OLD_QCA),USE_ARPNAT=1,)

ebtables-install: ebtables
	install -D ebtables/ebtables $(INSTALLDIR)/ebtables/usr/sbin/ebtables

	install -d $(INSTALLDIR)/ebtables/usr/lib
	install -d $(INSTALLDIR)/ebtables/usr/lib/ebtables
	install -D ebtables/*.so $(INSTALLDIR)/ebtables/usr/lib/
	install -D ebtables/extensions/*.so $(INSTALLDIR)/ebtables/usr/lib/ebtables/

	$(STRIP) $(INSTALLDIR)/ebtables/usr/sbin/ebtables
	$(STRIP) $(INSTALLDIR)/ebtables/usr/lib/ebtables/*.so
	$(STRIP) $(INSTALLDIR)/ebtables/usr/lib/libebt*.so

ebtables-clean:
	-@make -C ebtables clean

lzo/stamp-h1:
	cd lzo && \
	CFLAGS="-O3 -Wall $(EXTRACFLAGS)" $(CONFIGURE) --enable-shared --enable-static \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib
	touch $@

lzo: lzo/stamp-h1
	$(MAKE) -C lzo $(PARALLEL_BUILD) && $(MAKE) $@-stage

lzo-clean:
	-@$(MAKE) -C lzo clean
	@rm -f lzo/stamp-h1

lzo-install:
	install -D lzo/src/.libs/liblzo2.so $(INSTALLDIR)/lzo/usr/lib/liblzo2.so.2
	$(STRIP) $(INSTALLDIR)/lzo/usr/lib/liblzo2.so.2
	cd $(INSTALLDIR)/lzo/usr/lib && ln -sf liblzo2.so.2 liblzo2.so

openpam: openpam/Makefile
	$(MAKE) -C $@ $(PARALLEL_BUILD) && $(MAKE) $@-stage

openpam/Makefile:
	$(MAKE) openpam-configure

openpam-configure:
	( cd openpam && ./autogen.sh && \
		LDFLAGS=-ldl \
		$(CONFIGURE) \
		--without-doc --with-pam-unix \
	)

openpam-install:
	install -D openpam/lib/libpam/.libs/libpam.so.2.0.0 $(INSTALLDIR)/openpam/usr/lib/libpam.so.2
	$(STRIP) -s $(INSTALLDIR)/openpam/usr/lib/libpam.so.2
	install -D openpam/modules/pam_unix/.libs/pam_unix.so.2.0.0 $(INSTALLDIR)/openpam/usr/lib/pam_unix.so
	$(STRIP) -s $(INSTALLDIR)/openpam/usr/lib/pam_unix.so

openpam-clean:
	[ ! -f openpam/Makefile ] || $(MAKE) -C openpam distclean
	@rm -f openpam/Makefile

openvpn: shared openssl lzo openpam zlib openvpn/Makefile
	$(MAKE) -C $@ $(PARALLEL_BUILD)

openvpn/Makefile:
	$(MAKE) openvpn-configure

openvpn-configure:
	( cd openvpn ; ./autogen.sh && export OPENSSL_CRYPTO_CFLAGS="-I$(TOP)/openssl"; \
		export OPENSSL_CRYPTO_LIBS="-L$(TOP)/openssl -lcrypto"; \
		export OPENSSL_SSL_CFLAGS="-I$(TOP)/openssl"; \
		export OPENSSL_SSL_LIBS="-L$(TOP)/openssl -lssl"; \
		CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections $(if $(RTCONFIG_HTTPS),-I$(TOP)/openssl/include/openssl) -std=gnu99" \
		LDFLAGS="$(EXTRALDFLAGS) -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client -lqcsapi_client) $(if $(RTCONFIG_HTTPS),-L$(TOP)/openssl -lcrypto -lssl) -L$(TOP)/zlib -lz -lpthread -ldl -L$(TOP)/lzo/src/.libs -L$(TOP)/openpam/lib/libpam/.libs -ffunction-sections -fdata-sections -Wl,--gc-sections" \
		CPPFLAGS="-I$(TOP)/lzo/include -I$(TOP)/openssl/include -I$(TOP)/openpam/include -I$(TOP)/shared -I$(SRCBASE)/include" \
		IPROUTE="/usr/sbin/ip" IFCONFIG="/sbin/ifconfig" ROUTE="/sbin/route" \
		$(CONFIGURE) --prefix=/usr --bindir=/usr/sbin --libdir=/usr/lib \
			--disable-debug --enable-management --disable-small \
			--disable-selinux --disable-socks \
			--enable-plugin-auth-pam --enable-iproute2 \
			ac_cv_lib_resolv_gethostbyname=no \
	)

openvpn-clean:
	[ ! -f openvpn/Makefile ] || $(MAKE) -C openvpn clean
	@rm -f openvpn/Makefile

openvpn-install:
	install -D openvpn/src/openvpn/.libs/openvpn $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	$(STRIP) -s $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	chmod 0500 $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	install -D openvpn/src/plugins/auth-pam/.libs/openvpn-plugin-auth-pam.so $(INSTALLDIR)/openvpn/usr/lib/openvpn-plugin-auth-pam.so
	$(STRIP) -s $(INSTALLDIR)/openvpn/usr/lib/openvpn-plugin-auth-pam.so
	@mkdir -p $(TARGETDIR)/rom/easy-rsa
	install openvpn/easy-rsa/2.0/* $(TARGETDIR)/rom/easy-rsa

sdparm-1.02: sdparm-1.02/Makefile
	$(MAKE) -C $@

sdparm-1.02/Makefile: sdparm-1.02/configure
	cd sdparm-1.02 && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="$(EXTRALDFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/bin \
		--libdir=/usr/lib

sdparm-1.02/configure:
	cd sdparm-1.02 && ./autogen.sh

sdparm-1.02-install: sdparm-1.02
	@[ ! -d sdparm-1.02 ] || install -D sdparm-1.02/src/sdparm $(INSTALLDIR)/sdparm-1.02/usr/bin/sdparm
	@$(STRIP) $(INSTALLDIR)/sdparm-1.02/usr/bin/sdparm

sdparm-1.02-clean:
	[ ! -f sdparm-1.02/Makefile ] || $(MAKE) -C sdparm-1.02 distclean
	@rm -f sdparm-1.02/Makefile

.PHONY: strace
strace: strace/Makefile
	$(MAKE) -C $@

strace/Makefile: strace/configure
	$(MAKE) strace-configure

strace/configure:
	( cd strace && autoreconf -i -f )

strace-configure:
	( cd strace && $(CONFIGURE) --bindir=/sbin )

strace-install:
	@install -D strace/strace $(INSTALLDIR)/strace/sbin/strace
	@$(STRIP) $(INSTALLDIR)/strace/sbin/strace

strace-clean:
	[ ! -f strace/Makefile ] || $(MAKE) -C strace distclean
	@rm -f strace/Makefile

.PHONY: termcap
termcap: termcap/Makefile
	$(MAKE) -C $@

termcap/Makefile:
	( cd termcap && $(CONFIGURE) && touch termcap.info)

termcap-install:
	# We don't need to install termcap to target filesystem.

termcap-clean:
	[ ! -f termcap/Makefile ] || $(MAKE) -C termcap distclean
	@rm -f termcap/Makefile

.PHONY: gdb
gdb: termcap gdb/Makefile
	$(MAKE) -C $@

gdb/Makefile:
	( cd gdb && \
	  TUI_LIBRARY=$(TOP)/termcap/libtermcap.a \
	  LDFLAGS=-L$(TOP)/termcap \
	  CFLAGS_FOR_BUILD= \
	  $(CONFIGURE) --bindir=/sbin --enable-werror=no \
	)

gdb-install:
	@install -D gdb/gdb/gdb $(INSTALLDIR)/gdb/sbin/gdb
	@install -D gdb/gdb/gdbserver/gdbserver $(INSTALLDIR)/gdb/sbin/gdbserver
	@$(STRIP) $(INSTALLDIR)/gdb/sbin/gdb
	@$(STRIP) $(INSTALLDIR)/gdb/sbin/gdbserver

gdb-clean:
	[ ! -f gdb/Makefile ] || $(MAKE) -C gdb distclean
	@rm -f gdb/Makefile

.PHONY: openlldp
openlldp: openlldp/Makefile
	$(MAKE) -C $@

openlldp/Makefile:
	( cd openlldp && \
	  $(CONFIGURE) --bindir=/sbin \
	)

openlldp-install:
	@install -D openlldp/src/lldpd $(INSTALLDIR)/openlldp/sbin/lldpd
	@$(STRIP) $(INSTALLDIR)/openlldp/sbin/lldpd

openlldp-clean:
	[ ! -f openlldp/Makefile ] || $(MAKE) -C openlldp distclean
	@rm -f openlldp/Makefile

hspot_ap$(BCMEX)$(EX7): shared nvram$(BCMEX)
	@echo "do nothing"

hspot_ap$(BCMEX)$(EX7)-install:
	@echo "do nothing"

nas$(BCMEX)$(EX7): nvram$(BCMEX)

eapd$(BCMEX)$(EX7)-clean:
	-@cd eapd$(BCMEX)$(EX7)/linux && make clean

lighttpd-1.4.39/Makefile:
	@$(SEP)
#ifneq ($(RTCONFIG_BCMARM),y)
#	cd lighttpd-1.4.39 && ./autogen.sh
#endif

lighttpd-1.4.39/stamp-h1: lighttpd-1.4.39/Makefile

ifneq ($(RTCONFIG_QTN),y)
	cd lighttpd-1.4.39 && ./preconfigure-script touch $@
else
	cd lighttpd-1.4.39 && ./preconfigure-script-qtn touch $@
endif

lighttpd-1.4.39: shared nvram$(BCMEX)$(EX7) libdisk samba-3.5.8 pcre-8.31 libxml2 sqlite libexif openssl lighttpd-1.4.39/stamp-h1
	$(MAKE) -C lighttpd-1.4.39 $(PARALLEL_BUILD)

lighttpd-1.4.39-install:
	if [ -f lighttpd-1.4.39/Makefile ] ; then \
		install -D lighttpd-1.4.39/src/.libs/lighttpd $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd ; \
		install -D lighttpd-1.4.39/src/.libs/lighttpd-monitor $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd-monitor ; \
		install -D lighttpd-1.4.39/src/.libs/lighttpd-arpping $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd-arpping ; \
		$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd ; \
		$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd-monitor ; \
		$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.39/usr/sbin/lighttpd-arpping ; \
		install -d $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/ ; \
		install -D lighttpd-1.4.39/src/.libs/*.so $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/ ; \
		install -D samba-3.5.8/source3/bin/libsmbclient.so.0 $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/libsmbclient.so.0 ; \
		install -D sqlite/.libs/libsqlite3.so.0 $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/libsqlite3.so.0 ; \
		install -D libexif/libexif/.libs/libexif.so.12 $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/libexif.so.12 ; \
		mkdir -p $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd/css && mkdir -p $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd/js ; \
		cp -rf lighttpd-1.4.39/external_file/css/ $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd ; \
		cp -rf lighttpd-1.4.39/external_file/js/ $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd ; \
		cp -f APP-IPK/AiCloud-ipk/CONTROL/control $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd ; \
		cp -f APP-IPK/smartsync-pkg-1.0/CONTROL/control $(INSTALLDIR)/lighttpd-1.4.39/usr/lighttpd/smartsync_control ; \
		$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/*.so ; \
		$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.39/usr/lib/*.so.0 ; \
	fi

lighttpd-1.4.39-clean:
	-@$(MAKE) -C lighttpd-1.4.39 clean
	@rm -f lighttpd-1.4.39/stamp-h1

pcre-8.31/stamp-h1:
	cd pcre-8.31 && \
	CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="$(EXTRALDFLAGS)" \
	./$(CONFIGURE) --prefix=/usr --disable-dependency-tracking
	touch $@
	[ -d pcre-8.31/m4 ] || mkdir pcre-8.31/m4

pcre-8.31: pcre-8.31/stamp-h1

pcre-8.31-install:
	@$(SEP)
	install -D pcre-8.31/.libs/libpcre.so.1.0.1 $(INSTALLDIR)/pcre-8.31/usr/lib/libpcre.so.1.0.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcre.so.1.0.1 libpcre.so.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcre.so.1.0.1 libpcre.so
	$(STRIP) -s $(INSTALLDIR)/pcre-8.31/usr/lib/libpcre.so.1.0.1
	install -D pcre-8.31/.libs/libpcreposix.so.0.0.1 $(INSTALLDIR)/pcre-8.31/usr/lib/libpcreposix.so.0.0.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcreposix.so.0.0.1 libpcreposix.so.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcreposix.so.0.0.1 libpcreposix.so
	$(STRIP) -s $(INSTALLDIR)/pcre-8.31/usr/lib/libpcreposix.so.0.0.1

pcre-8.31-clean:
	-@$(MAKE) -C pcre-8.31 clean
	@rm -f pcre-8.31/stamp-h1

libxml2/stamp-h1:
	cd libxml2 && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" LDFLAGS=-ldl \
	./$(CONFIGURE) --prefix=/usr --without-python --disable-dependency-tracking
	touch $@

libxml2: libxml2/stamp-h1
	$(MAKE) -C libxml2 all $(PARALLEL_BUILD)

libxml2-install:
	@$(SEP)
	install -D libxml2/.libs/libxml2.so.2 $(INSTALLDIR)/libxml2/usr/lib/libxml2.so.2
	$(STRIP) $(INSTALLDIR)/libxml2/usr/lib/libxml2.so.2
	cd $(INSTALLDIR)/libxml2/usr/lib && ln -sf libxml2.so.2 libxml2.so

libxml2-clean:
	-@$(MAKE) -C libxml2 clean
	@rm -f libxml2/stamp-h1

#libiconv-1.14
libiconv-1.14/stamp-h1:
	cd libiconv-1.14 && \
	$(CONFIGURE) CC=$(CC) --prefix=$(TOP)/libiconv-1.14/out
	touch $@

libiconv-1.14:libiconv-1.14/stamp-h1
	@$(MAKE) -C libiconv-1.14

libiconv-1.14-install:
	@$(SEP)
	install -D libiconv-1.14/lib/.libs/libiconv.so.2.5.1 $(INSTALLDIR)/libiconv-1.14/usr/lib/libiconv.so.2.5.1
	$(STRIP) $(INSTALLDIR)/libiconv-1.14/usr/lib/libiconv.so.2.5.1
	cd $(INSTALLDIR)/libiconv-1.14/usr/lib && ln -sf libiconv.so.2.5.1 libiconv.so && ln -sf libiconv.so.2.5.1 libiconv.so.2
ifeq ($(RTCONFIG_USB_SMS_MODEM),y)
	install -D libiconv-1.14/src/.libs/iconv_no_i18n $(INSTALLDIR)/libiconv-1.14/usr/bin/iconv
	$(STRIP) $(INSTALLDIR)/libiconv-1.14/usr/bin/iconv
endif

libiconv-1.14-clean:
	-@$(MAKE) -C libiconv-1.14 clean
	@rm -f libiconv-1.14/stamp-h1

#ftpclient
ftpclient/stamp-h1: 
	@$(MAKE) -C ftpclient

ftpclient:ftpclient/stamp-h1

ftpclient-install:
	@$(SEP)
	install -D ftpclient/ftpclient $(INSTALLDIR)/ftpclient/usr/sbin/ftpclient
	$(STRIP) $(INSTALLDIR)/ftpclient/usr/sbin/ftpclient
ifeq ($(RTCONFIG_BCMARM),y)
	install -D $(shell dirname $(shell which $(CXX)))/../arm-brcm-linux-uclibcgnueabi/lib/libstdc++.so.6 $(INSTALLDIR)/ftpclient/usr/lib/libstdc++.so.6
else
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/ftpclient/usr/lib/libstdc++.so.6
endif

ftpclient-clean:
	-@$(MAKE) -C ftpclient clean

#sambaclient
sambaclient/stamp-h1: 
	touch $@

sambaclient: sambaclient/stamp-h1
	@$(MAKE) -C sambaclient

sambaclient-install:
	@$(SEP)
	install -D sambaclient/sambaclient $(INSTALLDIR)/sambaclient/usr/sbin/sambaclient
	$(STRIP) $(INSTALLDIR)/sambaclient/usr/sbin/sambaclient
ifeq ($(RTCONFIG_BCMARM),y)
	install -D $(shell dirname $(shell which $(CXX)))/../arm-brcm-linux-uclibcgnueabi/lib/libstdc++.so.6 $(INSTALLDIR)/sambaclient/usr/lib/libstdc++.so.6
else
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/sambaclient/usr/lib/libstdc++.so.6
endif

sambaclient-clean:
	-@$(MAKE) -C sambaclient clean
	rm -rf sambaclient/stamp-h1

#usbclient
usbclient/stamp-h1: 
	touch $@

usbclient: usbclient/stamp-h1
	@$(MAKE) -C usbclient

usbclient-install:
	@$(SEP)
	install -D usbclient/usbclient $(INSTALLDIR)/usbclient/usr/sbin/usbclient
	$(STRIP) $(INSTALLDIR)/usbclient/usr/sbin/usbclient
ifeq ($(RTCONFIG_BCMARM),y)
	install -D $(shell dirname $(shell which $(CXX)))/../arm-brcm-linux-uclibcgnueabi/lib/libstdc++.so.6 $(INSTALLDIR)/usbclient/usr/lib/libstdc++.so.6
else
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/usbclient/usr/lib/libstdc++.so.6
endif

usbclient-clean:
	-@$(MAKE) -C usbclient clean
	rm -rf usbclient/stamp-h1

##strongswan/IPSEC
strongswan-5.2.1/stamp-h1:
	cd strongswan-5.2.1 && \
	touch configure.ac aclocal.m4 configure Makefile.am Makefile.in  && \
	$(CONFIGURE) CC=$(CC) \
	--prefix=/usr/ --sysconfdir=/etc/ --localstatedir=/var/ \
	--bindir=/usr/sbin --libdir=/usr/lib \
	--libexecdir=/usr/lib --with-ipsecdir=/usr/lib/ipsec --with-user=admin \
	--disable-gmp --enable-openssl --enable-agent --enable-eap-peap \
	--enable-eap-md5 --enable-eap-mschapv2 --enable-eap-identity \
	--with-strongswan-conf=/etc/strongswan.conf --enable-md4 --enable-acert \
	--enable-cmd --enable-eap-tls --enable-libipsec \
	CFLAGS='-I$(STAGEDIR)/usr/include -I$(TOP)/openssl/include' \
	LDFLAGS='-L$(STAGEDIR)/usr/lib -L$(TOP)/openssl' \
	LIBS='-lssl -lcrypto -lpthread -ldl -lm' \
	touch $@

strongswan-5.2.1: strongswan-5.2.1/stamp-h1
	$(MAKE) -C strongswan-5.2.1

strongswan-5.2.1-install: strongswan-5.2.1
	@$(SEP)
	@$(MAKE) -C strongswan-5.2.1 install DESTDIR=$(INSTALLDIR)/strongswan-5.2.1/
	mv $(INSTALLDIR)/strongswan-5.2.1/etc $(INSTALLDIR)/strongswan-5.2.1/usr/

strongswan-5.2.1-clean:
	-@$(MAKE) -C strongswan-5.2.1 clean
	@rm -f strongswan-5.2.1/stamp-h1
	@rm -rf $(INSTALLDIR)/strongswan-5.2.1/
	@rm -rf $(TARGETDIR)/usr/sbin/strongswan-5.2.1/*
	@rm -rf $(TARGETDIR)/usr/lib/strongswan-5.2.1/*


#add by alan 
#add install neon 
neon/stamp-h1 neon/config.h:
	cd neon && \
	$(CONFIGURE) CC=$(CC) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		--enable-shared --disable-static --disable-nls --with-zlib --with-ssl=openssl \
		--with-libs=$(TOP)/openssl:$(TOP)/libxml2 \
		CFLAGS='-I$(STAGEDIR)/usr/include -I$(TOP)/openssl/include' \
		LDFLAGS='-L$(TOP)/libxml2/.libs -L$(STAGEDIR)/usr/lib -L$(TOP)/openssl' \
		LIBS='-lxml2 -lssl -lcrypto -lpthread -ldl -lm' \
		XML2_CONFIG='$(TOP)/libxml2/xml2-config'
	cp -f neon/config.h neon/src/config.h
	touch $@

neon: libxml2 neon/stamp-h1 neon/config.h
	$(MAKE) -C neon && $(MAKE) $@-stage

neon-install:
	@$(SEP)
	install -D neon/src/.libs/libneon.so.27.2.6 $(INSTALLDIR)/neon/usr/lib/libneon.so.27.2.6
	$(STRIP) $(INSTALLDIR)/neon/usr/lib/libneon.so.27.2.6
	cd $(INSTALLDIR)/neon/usr/lib && ln -sf libneon.so.27.2.6 libneon.so && ln -sf libneon.so.27.2.6 libneon.so.27

neon-clean:
	-@$(MAKE) -C neon clean
	@rm -f neon/stamp-h1

neon-stage:
	@$(MAKE) -C neon install DESTDIR=$(STAGEDIR)
	@cp -f neon/src/webdav_base.h $(STAGEDIR)/usr/include

#webdav_client
webdav_client$(BCMEX)/stamp-h1:
	touch $@
webdav_client$(BCMEX): webdav_client$(BCMEX)/stamp-h1 nvram$(BCMEX) zlib libxml2 neon
	@$(MAKE) -C webdav_client$(BCMEX)

webdav_client$(BCMEX)-install:
	@$(SEP)
	install -D webdav_client$(BCMEX)/webdav_client $(INSTALLDIR)/webdav_client$(BCMEX)/usr/sbin/webdav_client
	$(STRIP) $(INSTALLDIR)/webdav_client$(BCMEX)/usr/sbin/webdav_client

webdav_client$(BCMEX)-clean:
	-@$(MAKE) -C webdav_client$(BCMEX) clean
	rm -rf webdav_client$(BCMEX)/stamp-h1

#fbwifi
fb_wifi/stamp-h1:
	touch $@

fb_wifi: curl-7.21.7 fb_wifi/stamp-h1
	@$(MAKE) -C fb_wifi

fb_wifi-install: fb_wifi
	@$(SEP)
	install -D fb_wifi/fb_wifi_check $(INSTALLDIR)/fb_wifi/usr/sbin/fb_wifi_check
	install -D fb_wifi/fb_wifi_register $(INSTALLDIR)/fb_wifi/usr/sbin/fb_wifi_register
	install -D fb_wifi/libfbwifi.so $(INSTALLDIR)/fb_wifi/usr/lib/libfbwifi.so
	$(STRIP) $(INSTALLDIR)/fb_wifi/usr/sbin/fb_wifi_check
	$(STRIP) $(INSTALLDIR)/fb_wifi/usr/sbin/fb_wifi_register
	$(STRIP) $(INSTALLDIR)/fb_wifi/usr/lib/libfbwifi.so

fb_wifi-clean:
	-@$(MAKE) -C fb_wifi clean
	rm -rf fb_wifi/stamp-h1

#add by gauss
#add install libcurl
curl: curl/Makefile
	@$(MAKE) -C $@ $(PARALLEL_BUILD) && $(MAKE) $@-stage

curl/Makefile: curl/configure
	@cd curl && $(CONFIGURE) CC=$(CC) \
		CFLAGS="-Os -Wall -ffunction-sections -fdata-sections" \
		--prefix=/usr --bindir=/usr/sbin --libdir=/usr/lib \
		--enable-http --with-ssl=$(TOP)/openssl/ssl \
		$(if $(RTCONFIG_IPV6),--enable-ipv6) \
		--disable-gopher --disable-dict --disable-telnet \
		--disable-proxy --disable-manual --disable-libcurl-option \
		CPPFLAGS='-I$(TOP)/openssl/include' \
		LDFLAGS='$(LDFLAGS) -L$(TOP)/openssl' LIBS='-lcrypto -lssl -ldl' \
		--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt

curl-install:
	@$(SEP)
	install -D curl/lib/.libs/libcurl.so.4.4.0 $(INSTALLDIR)/curl/usr/lib/libcurl.so.4.4.0
	$(STRIP) $(INSTALLDIR)/curl/usr/lib/libcurl.so.4.4.0
	cd $(INSTALLDIR)/curl/usr/lib && ln -sf libcurl.so.4.4.0 libcurl.so && ln -sf libcurl.so.4.4.0 libcurl.so.4
#ifeq ($(RTCONFIG_SPEEDTEST),y)
	# for speedtest
	install -D curl/src/.libs/curl $(INSTALLDIR)/curl/usr/sbin/curl
	$(STRIP) $(INSTALLDIR)/curl/usr/sbin/curl
#endif

curl-clean:
	[ ! -f curl/Makefile ] || $(MAKE) -C curl clean
	@rm -f curl/Makefile

#snmp
net-snmp-5.7.2/stamp-h1:
	cd $(TOP)/net-snmp-5.7.2 && $(CONFIGURE) --prefix=$(INSTALLDIR)/net-snmp-5.7.2/usr --disable-embedded-perl --with-perl-modules=no --disable-debugging --enable-mini-agent --with-transports=UDP --without-kmem-usage --disable-applications --disable-mib-loading --disable-scripts --disable-mibs --disable-manuals --with-openssl=$(TOP)/openssl --with-ldflags="-L$(TOP)/openssl" --with-libs="$(EXTRA_FLAG)" --with-cflags="$(SNMPD_CFLAGS)" --with-default-snmp-version="3" --with-sys-location=Unknown --with-logfile=/var/log/snmpd.log --with-persistent-directory=/var/net-snmp --with-sys-contact=root@localhost --enable-static=no --with-mib-modules="\
	host/hr_device,\
	host/hr_disk,\
	host/hr_filesys,\
	host/hr_network,\
	host/hr_partition,\
	host/hr_proc,\
	host/hr_storage,\
	host/hr_system,\
	if-mib/ifXTable,\
	mibII/at,\
	mibII/icmp,\
	mibII/ifTable,\
	mibII/ip,\
	mibII/snmp_mib,\
	mibII/sysORTable,\
	mibII/system_mib,\
	mibII/tcp,\
	mibII/udp,\
	mibII/vacm_context,\
	mibII/vacm_vars,\
	tunnel,\
	ucd-snmp/disk,\
	ucd-snmp/dlmod,\
	ucd-snmp/extensible,\
	ucd-snmp/loadave,\
	ucd-snmp/memory,\
	ucd-snmp/pass,\
	ucd-snmp/proc,\
	ucd-snmp/vmstat,\
	util_funcs"
	@touch net-snmp-5.7.2/stamp-h1

net-snmp-5.7.2: net-snmp-5.7.2/stamp-h1
	@$(MAKE) -C net-snmp-5.7.2

net-snmp-5.7.2-install:
	@$(MAKE) -C net-snmp-5.7.2 install

net-snmp-5.7.2-clean:
	@[ ! -f net-snmp-5.7.2/Makefile ] || $(MAKE) -C net-snmp-5.7.2 clean
	@rm -f net-snmp-5.7.2/stamp-h1

#asuswebstorage
asuswebstorage$(BCMEX)/stamp-h1:
	touch $@

asuswebstorage$(BCMEX): asuswebstorage$(BCMEX)/stamp-h1
	@$(MAKE) -C asuswebstorage$(BCMEX)

asuswebstorage$(BCMEX)-install: asuswebstorage$(BCMEX)
	@$(SEP)
	install -D asuswebstorage$(BCMEX)/asuswebstorage $(INSTALLDIR)/asuswebstorage$(BCMEX)/usr/sbin/asuswebstorage
	$(STRIP) $(INSTALLDIR)/asuswebstorage$(BCMEX)/usr/sbin/asuswebstorage

asuswebstorage$(BCMEX)-clean:
	-@$(MAKE) -C asuswebstorage$(BCMEX) clean
	rm -rf asuswebstorage/stamp-h1

#dropbox_client
dropbox_client/stamp-h1:
	touch $@

dropbox_client: dropbox_client/stamp-h1
	@$(MAKE) -C dropbox_client

dropbox_client-install:
	@$(SEP)
	install -D dropbox_client/dropbox_client $(INSTALLDIR)/dropbox_client/usr/sbin/dropbox_client
	$(STRIP) $(INSTALLDIR)/dropbox_client/usr/sbin/dropbox_client

dropbox_client-clean:
	-@$(MAKE) -C dropbox_client clean
	rm -rf dropbox_client/stamp-h1

#inotify
inotify:
	@$(MAKE) -C inotify

inotify-install: inotify
	@$(SEP)
	install -D inotify/inotify $(INSTALLDIR)/inotify/usr/sbin/inotify
	$(STRIP) $(INSTALLDIR)/inotify/usr/sbin/inotify

inotify-clean:
	-@$(MAKE) -C inotify clean

wl$(BCMEX)$(EX7):
	-@$(MAKE) -C wl$(BCMEX)$(EX7)

wl$(BCMEX)$(EX7)-install:
	@$(SEP)

rtl_bin:
	-@$(MAKE) -C rtl_bin

rtl_bin-install:
	@$(SEP)

#libgpg-error-1.10
libgpg-error-1.10/stamp-h1:
	cd $(TOP)/libgpg-error-1.10 && \
	$(CONFIGURE) --prefix=$(INSTALLDIR)/libgpg-error-1.10/usr
	touch $@

libgpg-error-1.10: libgpg-error-1.10/stamp-h1
	@$(SEP)
	@$(MAKE) -C libgpg-error-1.10

libgpg-error-1.10-install:
	install -D libgpg-error-1.10/src/.libs/libgpg-error.so.0.8.0 $(INSTALLDIR)/libgpg-error-1.10/usr/lib/libgpg-error.so.0.8.0
	$(STRIP) $(INSTALLDIR)/libgpg-error-1.10/usr/lib/libgpg-error.so.0.8.0
	cd $(INSTALLDIR)/libgpg-error-1.10/usr/lib && ln -sf libgpg-error.so.0.8.0 libgpg-error.so.0

libgpg-error-1.10-clean:
	-@$(MAKE) -C libgpg-error-1.10 clean
	@rm -f libgpg-error-1.10/stamp-h1

#libgcrypt-1.5.1
libgcrypt-1.5.1/stamp-h1: libgpg-error-1.10
	cd $(TOP)/libgcrypt-1.5.1 && \
	$(CONFIGURE) --prefix=$(INSTALLDIR)/libgcrypt-1.5.1/usr --with-gpg-error-prefix=$(TOP)/libgpg-error-1.10/src \
	LDFLAGS="-L$(TOP)/libgpg-error-1.10/src/.libs"
	touch $@

libgcrypt-1.5.1: libgcrypt-1.5.1/stamp-h1
	@$(SEP)
	@$(MAKE) -C libgcrypt-1.5.1

libgcrypt-1.5.1-install:
	install -D libgcrypt-1.5.1/src/.libs/libgcrypt.so.11.8.0 $(INSTALLDIR)/libgcrypt-1.5.1/usr/lib/libgcrypt.so.11.8.0
	$(STRIP) $(INSTALLDIR)/libgcrypt-1.5.1/usr/lib/libgcrypt.so.11.8.0
	cd $(INSTALLDIR)/libgcrypt-1.5.1/usr/lib && ln -sf libgcrypt.so.11.8.0 libgcrypt.so.11

libgcrypt-1.5.1-clean:
	-@$(MAKE) -C libgcrypt-1.5.1 clean
	@rm -f libgcrypt-1.5.1/stamp-h1

#db-4.8.30
db-4.8.30/build_unix/stamp-h1:
	cd $(TOP)/db-4.8.30/build_unix && \
	../dist/$(CONFIGURE) --prefix=$(INSTALLDIR)/db-4.8.30/usr \
	--enable-cxx CFLAGS="-Os" \
	--disable-cryptography \
	--disable-hash \
	--disable-queue \
	--disable-replication \
	--disable-statistics \
	--disable-verify \
	--disable-compat185 \
	--disable-cxx \
	--disable-diagnostic \
	--disable-dump185 \
	--disable-java \
	--disable-mingw \
	--disable-o_direct \
	--enable-posixmutexes \
	--disable-smallbuild \
	--disable-tcl \
	--disable-test \
	--disable-uimutexes \
	--enable-umrw \
	--disable-static \
	--disable-libtool-lock
	touch $@

##	--enable-compat185 --enable-dbm --enable-cxx CFLAGS="-Os" --disable-mutexsupport

db-4.8.30: db-4.8.30/build_unix/stamp-h1
	@$(SEP)
	@$(MAKE) -C db-4.8.30/build_unix $(PARALLEL_BUILD)

db-4.8.30-install:
	install -D db-4.8.30/build_unix/.libs/libdb-4.8.so $(INSTALLDIR)/db-4.8.30/usr/lib/libdb-4.8.so
	$(STRIP) $(INSTALLDIR)/db-4.8.30/usr/lib/libdb-4.8.so
	cd $(INSTALLDIR)/db-4.8.30/usr/lib && ln -sf libdb-4.8.so libdb.so
	cd $(INSTALLDIR)/db-4.8.30/usr/lib && ln -sf libdb-4.8.so libdb-4.so

#	@$(MAKE) -C db-4.8.30/build_unix install
	
db-4.8.30-clean:
	-@$(MAKE) -C db-4.8.30 clean
	-@$(MAKE) -C db-4.8.30/build_unix clean
	@rm -f db-4.8.30/build_unix/stamp-h1

#netatalk-3.0.5
netatalk-3.0.5/stamp-h1: openssl libgcrypt-1.5.1 db-4.8.30
	cd $(TOP)/netatalk-3.0.5 && \
	$(CONFIGURE) CPPFLAGS="-I$(TOP)/db-4.8.30/build_unix -I$(TOP)/libgcrypt-1.5.1/src" \
	LDFLAGS="-L$(TOP)/db-4.8.30/build_unix/.libs -L$(TOP)/libgcrypt-1.5.1/src/.libs" \
	LD_LIBRARY_PATH="$(SRCBASE)/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3/lib" \
	LIBS="-L$(TOP)/libgpg-error-1.10/src/.libs -lgpg-error -L$(TOP)/libgcrypt-1.5.1/src/.libs -lgcrypt" \
	LIBGCRYPT_CONFIG="$(TOP)/libgcrypt-1.5.1/src/libgcrypt-config" \
	ac_cv_header_sys_xattr_h=yes ac_cv_search_getxattr='none required' \
	ac_cv_func_getxattr=yes ac_cv_func_lgetxattr=yes ac_cv_func_fgetxattr=yes \
	ac_cv_func_listxattr=yes ac_cv_func_llistxattr=yes ac_cv_func_flistxattr=yes \
	ac_cv_func_removexattr=yes ac_cv_func_lremovexattr=yes ac_cv_func_fremovexattr=yes \
	ac_cv_func_setxattr=yes ac_cv_func_lsetxattr=yes ac_cv_func_fsetxattr=yes \
	ac_cv_func_sendfile=yes ac_cv_header_crypt_h=yes ac_cv_lib_crypt_main=yes \
	ac_cv_path_LIBGCRYPT_CONFIG=$(TOP)/libgcrypt-1.5.1/src/libgcrypt-config \
	--without-kerberos \
	--with-bdb=$(TOP)/db-4.8.30/build_unix \
	--prefix=$(INSTALLDIR)/netatalk-3.0.5/usr \
	--with-dtrace=no \
	--program-prefix= \
	--with-ssl-dir=$(TOP)/openssl \
	--disable-static \
	--with-libgcrypt-dir=$(TOP)/libgcrypt-1.5.1 \
	--without-dtrace
	touch $@

netatalk-3.0.5: netatalk-3.0.5/stamp-h1
	@$(SEP)
	@touch netatalk-3.0.5/configure
	@$(MAKE) -C netatalk-3.0.5 $(PARALLEL_BUILD)

netatalk-3.0.5-install:
	install -D netatalk-3.0.5/etc/afpd/.libs/afpd $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/afpd
	install -D netatalk-3.0.5/etc/cnid_dbd/.libs/cnid_dbd $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/cnid_dbd
	install -D netatalk-3.0.5/etc/cnid_dbd/.libs/cnid_metad $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/cnid_metad
	install -D netatalk-3.0.5/bin/ad/.libs/ad $(INSTALLDIR)/netatalk-3.0.5/usr/bin/ad
	install -D netatalk-3.0.5/bin/afppasswd/.libs/afppasswd $(INSTALLDIR)/netatalk-3.0.5/usr/bin/afppasswd
	install -D netatalk-3.0.5/etc/cnid_dbd/.libs/dbd $(INSTALLDIR)/netatalk-3.0.5/usr/bin/dbd
	install -D netatalk-3.0.5/bin/uniconv/.libs/uniconv $(INSTALLDIR)/netatalk-3.0.5/usr/bin/uniconv
	install -D netatalk-3.0.5/libatalk/.libs/libatalk.so.6.0.0 $(INSTALLDIR)/netatalk-3.0.5/usr/lib/libatalk.so.6.0.0
	install -D netatalk-3.0.5/etc/uams/.libs/uams_randnum.so $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_randnum.so
	install -D netatalk-3.0.5/etc/uams/.libs/uams_passwd.so $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_passwd.so
	install -D netatalk-3.0.5/etc/uams/.libs/uams_guest.so $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_guest.so
	install -D netatalk-3.0.5/etc/uams/.libs/uams_dhx_passwd.so $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_dhx_passwd.so
	install -D netatalk-3.0.5/etc/uams/.libs/uams_dhx2_passwd.so $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_dhx2_passwd.so
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/afpd
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/cnid_dbd
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/sbin/cnid_metad
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/bin/ad
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/bin/afppasswd
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/bin/dbd
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/bin/uniconv
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/libatalk.so.6.0.0
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_randnum.so
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_passwd.so
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_guest.so
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_dhx_passwd.so
	$(STRIP) $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk/uams_dhx2_passwd.so
	cd $(INSTALLDIR)/netatalk-3.0.5/usr/lib && ln -sf libatalk.so.6.0.0 libatalk.so.6
	cd $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk && ln -sf uams_passwd.so uams_clrtxt.so
	cd $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk && ln -sf uams_dhx_passwd.so uams_dhx.so
	cd $(INSTALLDIR)/netatalk-3.0.5/usr/lib/netatalk && ln -sf uams_dhx2_passwd.so uams_dhx2.so

netatalk-3.0.5-clean:
	-@$(MAKE) -C netatalk-3.0.5 distclean
	@rm -f netatalk-3.0.5/stamp-h1

#expat-2.0.1
expat-2.0.1/stamp-h1:
	cd $(TOP)/expat-2.0.1 && \
	$(CONFIGURE) --build=i686-linux --prefix=$(INSTALLDIR)/expat-2.0.1/usr
	touch $@

expat-2.0.1: expat-2.0.1/stamp-h1
	@$(SEP)
	@$(MAKE) -C expat-2.0.1 $(PARALLEL_BUILD) && $(MAKE) $@-stage

expat-2.0.1-install:
	install -D expat-2.0.1/.libs/libexpat.so.1.5.2 $(INSTALLDIR)/expat-2.0.1/usr/lib/libexpat.so.1.5.2
	$(STRIP) $(INSTALLDIR)/expat-2.0.1/usr/lib/libexpat.so.1.5.2
	cd $(INSTALLDIR)/expat-2.0.1/usr/lib && ln -sf libexpat.so.1.5.2 libexpat.so.1

expat-2.0.1-clean:
	-@$(MAKE) -C expat-2.0.1 clean
	@rm -f expat-2.0.1/stamp-h1

#avahi-0.6.31
avahi-0.6.31: shared nvram$(BCMEX) expat-2.0.1 libdaemon avahi-0.6.31/Makefile
	@$(SEP)
	@$(MAKE) -C avahi-0.6.31 $(PARALLEL_BUILD)
 
avahi-0.6.31/Makefile:
	$(MAKE) avahi-0.6.31-configure

avahi-0.6.31-configure:
	( cd avahi-0.6.31 ; $(CONFIGURE) LDFLAGS="-L$(TOP)/expat-2.0.1/.libs -ldl -lpthread" \
		CFLAGS="$(CFLAGS) -I$(TOP)/expat-2.0.1/lib -I$(TOP)/shared -I$(SRCBASE)/include" \
		--prefix=$(INSTALLDIR)/avahi-0.6.31/usr --with-distro=archlinux \
		--disable-glib --disable-gobject --disable-qt3 --disable-qt4 --disable-gtk \
		--disable-dbus --disable-expat --disable-gdbm --disable-python \
		--disable-pygtk --disable-python-dbus --disable-mono --disable-monodoc \
		--disable-gtk3 --with-xml=none \
		LIBDAEMON_LIBS="-L$(TOP)/libdaemon/libdaemon/.libs -ldaemon -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client -lqcsapi_client)" \
		LIBDAEMON_CFLAGS="-I$(TOP)/libdaemon" --disable-autoipd --with-xml=expat --disable-stack-protector --with-avahi-user="admin" --with-avahi-group="root" \
	)

avahi-0.6.31-install:
	install -D avahi-0.6.31/avahi-daemon/.libs/avahi-daemon $(INSTALLDIR)/avahi-0.6.31/usr/sbin/avahi-daemon
	install -D avahi-0.6.31/avahi-common/.libs/libavahi-common.so.3.5.3 $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-common.so.3.5.3
	install -D avahi-0.6.31/avahi-core/.libs/libavahi-core.so.7.0.2 $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-core.so.7.0.2
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/sbin/avahi-daemon
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-common.so.3.5.3
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-core.so.7.0.2
	cd $(INSTALLDIR)/avahi-0.6.31/usr/lib && ln -sf libavahi-common.so.3.5.3 libavahi-common.so.3
	cd $(INSTALLDIR)/avahi-0.6.31/usr/lib && ln -sf libavahi-core.so.7.0.2 libavahi-core.so.7

avahi-0.6.31-clean:
	[ ! -f avahi-0.6.31/Makefile ] || $(MAKE) -C avahi-0.6.31 clean
	@rm -f avahi-0.6.31/Makefile

madwimax-0.1.1/stamp-h1:
	cd $(TOP)/madwimax-0.1.1 && \
	LDFLAGS="$(LDFLAGS) -ldl" $(CONFIGURE) --prefix=$(TOP)/madwimax-0.1.1/build --with-script=wl500g --without-man-pages --with-udev-dir=$(TOP)/madwimax-0.1.1/build libusb1_CFLAGS=-I$(TOP)/libusb10/libusb libusb1_LIBS="-L$(TOP)/libusb10/libusb/.libs -lusb-1.0"
	touch $@

madwimax-0.1.1: libusb10 madwimax-0.1.1/stamp-h1
	@$(SEP)
	@$(MAKE) -C madwimax-0.1.1 all

madwimax-0.1.1-install: madwimax-0.1.1
	#install -D madwimax-0.1.1/scripts/events/event.sh $(INSTALLDIR)/madwimax-0.1.1/usr/sbin/event.sh
	install -D madwimax-0.1.1/src/madwimax $(INSTALLDIR)/madwimax-0.1.1/usr/sbin/madwimax
	$(STRIP) $(INSTALLDIR)/madwimax-0.1.1/usr/sbin/madwimax

madwimax-0.1.1-clean:
	-@$(MAKE) -C madwimax-0.1.1 clean
	@cd $(TOP)/madwimax-0.1.1 && rm -f config.log config.status
	@cd $(TOP)/madwimax-0.1.1 && rm -f `find -name Makefile`
	@cd $(TOP)/madwimax-0.1.1 && rm -rf include/config.h scripts/udev/z60_madwimax.rules src/.deps
	@rm -f madwimax-0.1.1/stamp-h1

ufsd: kernel_header kernel
	@$(MAKE) -C ufsd all

ufsd-install: ufsd
	@$(MAKE) -C ufsd install INSTALLDIR=$(INSTALLDIR)/ufsd

netstat-nat-clean:
	-@$(MAKE) -C netstat-nat clean
	@rm -f netstat-nat/Makefile
	@rm -f netstat-nat/stamp-h1

netstat-nat-install: netstat-nat
	install -D netstat-nat/netstat-nat $(INSTALLDIR)/netstat-nat/usr/sbin/netstat-nat
	$(STRIP) -s $(INSTALLDIR)/netstat-nat/usr/sbin/netstat-nat

netstat-nat/stamp-h1:
	cd netstat-nat && \
	CC=$(CC) CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
		$(CONFIGURE)
	@touch $@

netstat-nat: netstat-nat/stamp-h1

netstat-nat: netstat-nat/Makefile
	$(MAKE) -C netstat-nat

sd-idle: sd-idle/sd-idle-2.6
	cd sd-idle && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	$(CC) sd-idle-2.6.c -o sd-idle-2.6

sd-idle-install: sd-idle-2.6
	install -D sd-idle/sd-idle-2.6 $(INSTALLDIR)/sd-idle/usr/sbin/sd-idle-2.6
	$(STRIP) -s $(INSTALLDIR)/sd-idle/usr/sbin/sd-idle-2.6

nfs-utils/stamp-h1:
	cd nfs-utils && CPPFLAGS="$(EXTRACFLAGS) -Os" knfsd_cv_bsd_signals=no \
	CC_FOR_BUILD=$(CC) $(CONFIGURE) --enable-nfsv3 --disable-nfsv4 --disable-ipv6 \
	--disable-uuid --disable-gss --disable-mount --without-tcp-wrappers
	@touch $@

nfs-utils: nfs-utils/stamp-h1
	$(MAKE) -C $@ $(PARALLEL_BUILD)

nfs-utils-clean:
	-@$(MAKE) -C nfs-utils distclean
	@rm -f nfs-utils/stamp-h1

nfs-utils-install:
	install -D nfs-utils/utils/nfsd/nfsd $(INSTALLDIR)/nfs-utils/usr/sbin/nfsd
	install -D nfs-utils/utils/mountd/mountd $(INSTALLDIR)/nfs-utils/usr/sbin/mountd
	install -D nfs-utils/utils/exportfs/exportfs $(INSTALLDIR)/nfs-utils/usr/sbin/exportfs
	install -D nfs-utils/utils/showmount/showmount $(INSTALLDIR)/nfs-utils/usr/sbin/showmount
	install -D nfs-utils/utils/statd/statd $(INSTALLDIR)/nfs-utils/usr/sbin/statd
	install -D nfs-utils/support/lib/libnfs.so $(INSTALLDIR)/nfs-utils/usr/lib/libnfs.so
	$(STRIP) $(INSTALLDIR)/nfs-utils/usr/sbin/* $(INSTALLDIR)/nfs-utils/usr/lib/libnfs.so

portmap: portmap/Makefile
	$(MAKE) -C $@  EXTRACFLAGS="$(EXTRACFLAGS) -Os" NO_TCP_WRAPPER=y RPCUSER=nobody

portmap-install:
	install -D portmap/portmap $(INSTALLDIR)/portmap/usr/sbin/portmap
	$(STRIP) $(INSTALLDIR)/portmap/usr/sbin/portmap

ipset:
	$(MAKE) -C ipset binaries $(PARALLEL_BUILD)

ipset-install:
	install -D ipset/ipset $(INSTALLDIR)/ipset/usr/sbin/ipset
	install -d $(INSTALLDIR)/ipset/usr/lib/
	install ipset/*.so $(INSTALLDIR)/ipset/usr/lib/
	$(STRIP) $(INSTALLDIR)/ipset/usr/lib/*.so
	$(STRIP) $(INSTALLDIR)/ipset/usr/sbin/ipset

ncurses/Makefile:
	cd ncurses && CFLAGS="$(EXTRACFLAGS) -Os -I$(STAGEDIR)/usr/include -ffunction-sections -fdata-sections -fPIC" \
	$(CONFIGURE) --prefix=/usr --without-cxx --without-cxx-binding \
	LDFLAGS="$(LDFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(STAGEDIR)/usr/lib" \
	--enable-echo --enable-const --enable-overwrite --disable-rpath --without-ada \
	$(if $(RTCONFIG_BCMARM),--enable-widec,) \
	--without-debug --without-manpages --without-profile --without-progs --without-tests \
	--disable-home-terminfo --with-normal --with-shared --with-build-cppflags=-D_GNU_SOURCE \
	--enable-pc-files \
	--disable-termcap --disable-database --with-fallbacks="xterm,vt100,vt200,linux,ansi,xterm-256color"

#	--with-default-terminfo-dir=/usr/share/terminfo --with-terminfo-dirs=/usr/share/terminfo

ncurses: ncurses/Makefile
	@$(SEP)
	cd ncurses && $(MAKE) -C ncurses && $(MAKE) -C misc pc-files

ncurses-clean:
	-@$(MAKE) -C ncurses distclean
	@rm -f ncurses/Makefile

ncurses-install:
ifeq ($(RTCONFIG_BCMARM),y)
	install -D ncurses/lib/libncursesw.so.6.0 $(INSTALLDIR)/ncurses/usr/lib/libncursesw.so.6.0
else
	install -D ncurses/lib/libncurses.so.6.0 $(INSTALLDIR)/ncurses/usr/lib/libncurses.so.6.0
endif

	$(STRIP) $(INSTALLDIR)/ncurses/usr/lib/*.so.*

ifeq ($(RTCONFIG_BCMARM),y)
	cd $(INSTALLDIR)/ncurses/usr/lib && ln -sf libncursesw.so.6.0 libncursesw.so.6
else
	cd $(INSTALLDIR)/ncurses/usr/lib && ln -sf libncurses.so.6.0 libncurses.so.6
endif


nano/Makefile:
	cd nano && \
	autoreconf -i -f && $(CONFIGURE) --prefix=$(INSTALLDIR)/nano/usr \
		CFLAGS="$(EXTRACFLAGS) -Os -I$(TOP)/ncurses/include -ffunction-sections -fdata-sections" \
		LDFLAGS="$(LDFLAGS) -L$(TOP)/ncurses/lib -ffunction-sections -fdata-sections -Wl,--gc-sections" \
		PKG_CONFIG_LIBDIR="$(TOP)/ncurses/misc" PKG_CONFIG_PATH="$(TOP)/ncurses/misc" \
		$(if $(RTCONFIG_BCMARM),ac_cv_lib_ncursesw_get_wch=yes,ac_cv_lib_ncursesw_get_wch=no) \
		$(if $(RTCONFIG_BCMARM),--enable-utf8,--enable-tiny) \
		--disable-speller --disable-extra --disable-tabcomp --enable-wrapping --disable-glibtest \
		--disable-libmagic --disable-nls

nano-clean:
	-@$(MAKE) -C nano distclean
	@rm -f nano/Makefile

nano-install:
	@$(MAKE) -C nano install-exec INSTALLDIR=$(INSTALLDIR)/nano/usr/bin/nano
	$(STRIP) $(INSTALLDIR)/nano/usr/bin/nano

nano: ncurses nano/Makefile
	@$(SEP)
	$(MAKE) -C $@

libmnl/stamp-h1:
	cd $(TOP)/libmnl && CC=$(CC) STRIP=$(STRIP) \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --prefix=/usr
	touch libmnl/stamp-h1

libmnl: libmnl/stamp-h1
	$(MAKE) -C libmnl
	$(MAKE) -C libmnl DESTDIR=$(TOP)/libmnl/staged install

libmnl-install: libmnl
	install -d $(INSTALLDIR)/libmnl/usr/lib/
	install libmnl/src/.libs/libmnl.so.0.1.0 $(INSTALLDIR)/libmnl/usr/lib/libmnl.so.0.1.0
	$(STRIP) $(INSTALLDIR)/libmnl/usr/lib/libmnl.so.0.1.0
	cd $(INSTALLDIR)/libmnl/usr/lib/ && \
		ln -sf libmnl.so.0.1.0 libmnl.so.0 && \
		ln -sf libmnl.so.0.1.0 libmnl.so

libmnl-clean:
	-@$(MAKE) -C libmnl clean
	-@rm -rf libmnl/staged

ipset_arm/stamp-h1: libmnl
	cd $(TOP)/ipset_arm && CC=$(CC) STRIP=$(STRIP) \
	autoreconf -i -f && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	libmnl_CFLAGS="-I$(TOP)/libmnl/staged/usr/include" \
	libmnl_LIBS="-L$(TOP)/libmnl/staged/usr/lib -lmnl" \
	$(CONFIGURE) --prefix=/usr --with-kmod=yes --with-kbuild=$(LINUXDIR)
	touch ipset_arm/stamp-h1

ipset_arm: ipset_arm/stamp-h1
	$(MAKE) -C ipset_arm
	$(MAKE) -C ipset_arm modules

ipset_arm-install:
	install -D ipset_arm/src/ipset $(INSTALLDIR)/ipset_arm/usr/sbin/ipset
	install -d $(INSTALLDIR)/ipset_arm/usr/lib/
	install ipset_arm/lib/.libs/libipset.so.3.6.0 $(INSTALLDIR)/ipset_arm/usr/lib/libipset.so.3.6.0
	$(STRIP) $(INSTALLDIR)/ipset_arm/usr/lib/libipset.so.3.6.0
	$(STRIP) $(INSTALLDIR)/ipset_arm/usr/sbin/ipset
	cd $(INSTALLDIR)/ipset_arm/usr/lib/ && \
		ln -sf libipset.so.3.6.0 libipset.so.3 && \
		ln -sf libipset.so.3.6.0 libipset.so

ipset_arm-clean:
	-@$(MAKE) -C ipset_arm clean
	-@$(MAKE) -C ipset_arm modules_clean
	-@rm -rf ipset_arm/Makefile ipset_arm/stamp-h1

tftp:
	cd tftp && CC=$(CC) && $(MAKE)

tftp-clean:
	cd tftp && $(MAKE) clean

tftp-install:
	# TPTP server
	install -D tftp/tftpd $(INSTALLDIR)/tftp/usr/sbin/tftpd
	$(STRIP) -s $(INSTALLDIR)/tftp/usr/sbin/tftpd
	# TFTP client
	#install -D tftp/tftpc $(INSTALLDIR)/tftp/usr/sbin/tftpc
	#$(STRIP) -s $(INSTALLDIR)/tftp/usr/sbin/tftpc

bwdpi: nvram$(BCMEX)
	cd bwdpi && CC=$(CC) && $(MAKE)

bwdpi_sqlite: bwdpi sqlite
	cd bwdpi_sqlite && CC=$(CC) && $(MAKE)

bwdpi_sqlite-clean:
	cd bwdpi_sqlite && $(MAKE) clean

bwdpi_sqlite-install: bwdpi_sqlite

traffic_limiter: sqlite
	cd traffic_limiter && CC=$(CC) && $(MAKE)

traffic_limiter-clean:
	cd traffic_limiter && $(MAKE) clean

traffic_limiter-install: traffic_limiter

#chillispot
chillispot/stamp-h1:
	cd chillispot && \
	$(CONFIGURE) CC=$(CC) \
	CFLAGS="-g -Os -pipe -funit-at-a-time -Drpl_malloc=malloc -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared" \
	LDFLAGS="-lgcc_s -L$(TOP)/nvram${BCMEX} -L$(TOP)/shared -lshared" \
	LIBS="-lnvram"
	#$(CONFIGURE) CC=$(CC) CFLAGS="-g -Os -pipe -funit-at-a-time -Drpl_malloc=malloc"
	#touch $@

chillispot: chillispot/stamp-h1
	@$(MAKE) -C chillispot

chillispot-install: chillispot
	@$(SEP)
	install -D chillispot/src/chilli $(INSTALLDIR)/chillispot/usr/sbin/chilli
	$(STRIP) $(INSTALLDIR)/chillispot/usr/sbin/chilli

chillispot-clean:
	-@$(MAKE) -C chillispot clean 

#coovachilli
coovachilli/stamp-h1:
	cd coovachilli && autoreconf -fi && \
	$(CONFIGURE) CC=$(CC) --prefix="" --exec_prefix=/usr --enable-shared=no --with-openssl --without-ipv6 \
	CFLAGS="-g -Os -pipe -funit-at-a-time -Drpl_malloc=malloc -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared -I$(SRCBASE)/router/openssl/include" \
	LDFLAGS="-lgcc_s -L$(TOP)/nvram${BCMEX} -L$(TOP)/shared -lshared -L$(SRCBASE)/router/openssl" \
	LIBS="-lnvram -lcrypto -lssl $(if $(RTCONFIG_QCA), -ldl)"

	#$(CONFIGURE) CC=$(CC) CFLAGS="-g -Os -pipe -funit-at-a-time -Drpl_malloc=malloc" --prefix="" --exec_prefix=/usr --enable-shared=no --with-openssl --without-ipv6


coovachilli:coovachilli/stamp-h1
	@$(MAKE) -C coovachilli

coovachilli-install:coovachilli
	@$(SEP)
	install -D coovachilli/src/chilli $(INSTALLDIR)/coovachilli/usr/sbin/chilli
	install -D coovachilli/src/chilli_opt $(INSTALLDIR)/coovachilli/usr/sbin/chilli_opt
	install -D coovachilli/src/chilli_query $(INSTALLDIR)/coovachilli/usr/sbin/chilli_query
	install -D coovachilli/src/chilli_radconfig $(INSTALLDIR)/coovachilli/usr/sbin/chilli_radconfig
	install -D coovachilli/src/chilli_response $(INSTALLDIR)/coovachilli/usr/sbin/chilli_response
	$(STRIP) $(INSTALLDIR)/coovachilli/usr/sbin/*

coovachilli-clean:
	-@$(MAKE) -C coova-chilli-1.3.0 clean

 #

#talloc
talloc/stamp-h1:
	cd talloc-2.1.1 && \
	CC=$(CC) ./configure --without-gettext  --cross-compile --cross-answers=arm1.txt
	#touch $@

talloc: talloc/stamp-h1
	@$(MAKE) -C talloc-2.1.1
	cd $(TOP)/talloc-2.1.1/bin/default && ln -sf libtalloc.so libtalloc.so.2

talloc-install:talloc
	@$(SEP)
	install -D talloc-2.1.1/bin/default/libtalloc.so $(INSTALLDIR)/talloc/usr/lib/libtalloc.so
	$(STRIP) $(INSTALLDIR)/talloc/usr/lib/libtalloc.so
	cd $(INSTALLDIR)/talloc/usr/lib && ln -sf libtalloc.so libtalloc.so.2

talloc-clean:
	-@$(MAKE) -C talloc-2.1.1 clean

#freeradius
freeradius/stamp-h1:
	cd freeradius-server-3.0.0 && \
	ln -sf /usr/share/libtool/config/config.guess config.guess && \
	ln -sf /usr/share/libtool/config/config.sub config.sub && \
	$(CONFIGURE) CC=$(CC)  CFLAGS="-I$(SRCBASE) -I$(SRCBASE)/include -I$(TOP)/pcrc-8.31" \
	LDFLAGS="-L$(TOP)/pcre-8.31/.libs -lpcre -L$(TOP)/talloc-2.1.1/bin/default -Wl,-rpath=$(TOP)/talloc-2.1.1/bin/default -lpthread -ldl" \
	--prefix=/usr/freeradius --datarootdir=/usr/freeradius/share --with-raddbdir=/usr/freeradius/raddb \
	--with-openssl-includes=$(TOP)/openssl/include --with-openssl-libraries=$(TOP)/openssl \
	--with-talloc-lib-dir=$(TOP)/talloc-2.1.1/bin/default --with-talloc-include-dir=$(TOP)/talloc-2.1.1 \
	--with-sqlite-lib-dir=$(TOP)/sqlite/.libs --with-sqlite-include-dir=$(TOP)/sqlite \
	--with-pcre-lib-dir=$(TOP)/pcre-8.31/.libs  --with-pcre-include-dir=$(TOP)/pcrc-8.31
	#touch $@
freeradius:freeradius/stamp-h1
	@$(MAKE) -C freeradius-server-3.0.0

freeradius-install:
	@$(SEP)
	install -D freeradius-server-3.0.0/build/bin/radiusd $(INSTALLDIR)/freeradius/usr/sbin/radiusd
	$(STRIP) $(INSTALLDIR)/freeradius/usr/sbin/radiusd
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/lib
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/share/freeradius && mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/certs
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/attr_filter
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/preprocess
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/files
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/cui/sqlite
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/ippool/sqlite
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/ippool-dhcp/sqlite
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/main/sqlite
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-enabled
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/policy.d
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/sites-available
	mkdir -p $(INSTALLDIR)/freeradius/usr/freeradius/raddb/sites-enabled
	install -D freeradius-server-3.0.0/build/lib/.libs/libfreeradius-eap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-eap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/libfreeradius-radius.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-radius.so
	install -D freeradius-server-3.0.0/build/lib/.libs/libfreeradius-server.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-server.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_always.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_always.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_attr_filter.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_attr_filter.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_cache.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_cache.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_chap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_chap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_detail.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_detail.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_dhcp.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_dhcp.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_digest.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_digest.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_dynamic_clients.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_dynamic_clients.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_gtc.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_gtc.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_leap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_leap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_md5.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_md5.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_mschapv2.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_mschapv2.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_peap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_peap.so
#	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_pwd.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_pwd.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_sim.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_sim.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_tls.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_tls.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_eap_ttls.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_ttls.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_exec.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_exec.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_expiration.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_expiration.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_expr.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_expr.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_files.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_files.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_linelog.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_linelog.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_logintime.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_logintime.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_mschap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_mschap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_pap.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_pap.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_passwd.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_passwd.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_preprocess.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_preprocess.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_radutmp.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_radutmp.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_realm.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_realm.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_replicate.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_replicate.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_soh.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_soh.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_sql.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sql.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_sqlcounter.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sqlcounter.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_sqlippool.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sqlippool.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_sql_sqlite.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sql_sqlite.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_unix.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_unix.so
	install -D freeradius-server-3.0.0/build/lib/.libs/rlm_utf8.so $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_utf8.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-eap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-radius.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/libfreeradius-server.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_always.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_attr_filter.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_cache.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_chap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_detail.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_dhcp.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_digest.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_dynamic_clients.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_gtc.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_leap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_md5.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_mschapv2.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_peap.so
#	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_pwd.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_sim.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_tls.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_eap_ttls.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_exec.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_expiration.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_expr.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_files.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_linelog.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_logintime.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_mschap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_pap.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_passwd.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_preprocess.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_radutmp.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_realm.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_replicate.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_soh.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sql.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sqlcounter.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sqlippool.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_sql_sqlite.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_unix.so
	$(STRIP) $(INSTALLDIR)/freeradius/usr/freeradius/lib/rlm_utf8.so
	cp -rf freeradius-server-3.0.0/share/* $(INSTALLDIR)/freeradius/usr/freeradius/share/freeradius
	cp -f freeradius-server-3.0.0/raddb/dictionary $(INSTALLDIR)/freeradius/usr/freeradius/raddb/dictionary
	cp -f freeradius-server-3.0.0/raddb/radiusd.conf $(INSTALLDIR)/freeradius/usr/freeradius/raddb/radiusd.conf
	cp -f freeradius-server-3.0.0/raddb/test_user.sql $(INSTALLDIR)/freeradius/usr/freeradius/raddb/test_user.sql
	install -D freeradius-server-3.0.0/raddb/mods-available/always $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/always
	install -D freeradius-server-3.0.0/raddb/mods-available/attr_filter $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/attr_filter
	install -D freeradius-server-3.0.0/raddb/mods-available/cache_eap $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/cache_eap
	install -D freeradius-server-3.0.0/raddb/mods-available/chap $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/chap
	install -D freeradius-server-3.0.0/raddb/mods-available/detail $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/detail
	install -D freeradius-server-3.0.0/raddb/mods-available/detail.log $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/detail.log
	install -D freeradius-server-3.0.0/raddb/mods-available/dhcp $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/dhcp
	install -D freeradius-server-3.0.0/raddb/mods-available/digest $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/digest
	install -D freeradius-server-3.0.0/raddb/mods-available/dynamic_clients $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/dynamic_clients
	install -D freeradius-server-3.0.0/raddb/mods-available/eap $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/eap
	install -D freeradius-server-3.0.0/raddb/mods-available/echo $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/echo
	install -D freeradius-server-3.0.0/raddb/mods-available/exec $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/exec
	install -D freeradius-server-3.0.0/raddb/mods-available/expiration $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/expiration
	install -D freeradius-server-3.0.0/raddb/mods-available/expr $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/expr
	install -D freeradius-server-3.0.0/raddb/mods-available/files $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/files
	install -D freeradius-server-3.0.0/raddb/mods-available/linelog $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/linelog
	install -D freeradius-server-3.0.0/raddb/mods-available/logintime $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/logintime
	install -D freeradius-server-3.0.0/raddb/mods-available/mschap $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/mschap
	install -D freeradius-server-3.0.0/raddb/mods-available/ntlm_auth $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/ntlm_auth
	install -D freeradius-server-3.0.0/raddb/mods-available/pap $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/pap
	install -D freeradius-server-3.0.0/raddb/mods-available/passwd $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/passwd
	install -D freeradius-server-3.0.0/raddb/mods-available/preprocess $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/preprocess
	install -D freeradius-server-3.0.0/raddb/mods-available/radutmp $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/radutmp
	install -D freeradius-server-3.0.0/raddb/mods-available/realm $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/realm
	install -D freeradius-server-3.0.0/raddb/mods-available/replicate $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/replicate
	install -D freeradius-server-3.0.0/raddb/mods-available/soh $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/soh
	install -D freeradius-server-3.0.0/raddb/mods-available/sql $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/sql
	install -D freeradius-server-3.0.0/raddb/mods-available/sradutmp $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/sradutmp
	install -D freeradius-server-3.0.0/raddb/mods-available/unix $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/unix
	install -D freeradius-server-3.0.0/raddb/mods-available/utf8 $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-available/utf8
	cp -rf freeradius-server-3.0.0/raddb/mods-config/attr_filter/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/attr_filter/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/preprocess/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/preprocess/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/files/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/files/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/sql/cui/sqlite/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/cui/sqlite/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/sql/ippool/sqlite/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/ippool/sqlite/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/sql/ippool-dhcp/sqlite/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/ippool-dhcp/sqlite/
	cp -rf freeradius-server-3.0.0/raddb/mods-config/sql/main/sqlite/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-config/sql/main/sqlite/
	cp -rf freeradius-server-3.0.0/raddb/mods-enabled/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-enabled/
	cp -rf freeradius-server-3.0.0/raddb/policy.d/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/policy.d/
	cp -rf freeradius-server-3.0.0/raddb/sites-available/default $(INSTALLDIR)/freeradius/usr/freeradius/raddb/sites-available/
	cp -rf freeradius-server-3.0.0/raddb/sites-available/inner-tunnel $(INSTALLDIR)/freeradius/usr/freeradius/raddb/sites-available/
	cp -rf freeradius-server-3.0.0/raddb/sites-enabled/* $(INSTALLDIR)/freeradius/usr/freeradius/raddb/sites-enabled/	
	cd $(TOP)/freeradius-server-3.0.0/raddb/certs && make
	cp -f $(TOP)/freeradius-server-3.0.0/raddb/certs/server.pem $(INSTALLDIR)/freeradius/usr/freeradius/raddb/certs/
	cp -f $(TOP)/freeradius-server-3.0.0/raddb/certs/ca.pem $(INSTALLDIR)/freeradius/usr/freeradius/raddb/certs/
	cp -f $(TOP)/freeradius-server-3.0.0/raddb/certs/dh $(INSTALLDIR)/freeradius/usr/freeradius/raddb/certs/
	cd $(INSTALLDIR)/freeradius/usr/freeradius/raddb/mods-enabled && ln -sf ../mods-available/sql sql 
	
freeradius-clean:
	-@$(MAKE) -C freeradius-server-3.0.0 distclean

NORTON_DIR := $(SRCBASE)/router/norton${BCMEX}

.PHONY: norton${BCMEX} norton${BCMEX}-install norton${BCMEX}-clean


norton${BCMEX}:
	+$(MAKE) -C $(NORTON_DIR)

norton${BCMEX}-install:
	+$(MAKE) -C $(NORTON_DIR) install INSTALLDIR=$(INSTALLDIR)/norton${BCMEX}

norton${BCMEX}-clean:
	[ ! -f $(NORTEON_DIR)/Makefile ] || -@$(MAKE) -C $(NORTON_DIR) clean

.PHONY : email-3.1.3 email-3.1.3-clean

ifeq ($(RTCONFIG_NOTIFICATION_CENTER),y)
email-3.1.3/stamp-h1: openssl
	cd $(TOP)/email-3.1.3 && \
	$(CONFIGURE) --with-ssl --sysconfdir=/etc \
		CFLAGS="-I$(SRCBASE)/router/openssl/include -I$(TOP)/nt_center/lib -I$(TOP)/sqlite -DRTCONFIG_NOTIFICATION_CENTER" \
		LDFLAGS="-L$(SRCBASE)/router/openssl -L$(TOP)/nt_center/lib -L$(TOP)/sqlite/.libs" \
		LIBS="-lnt -lsqlite3 -lpthread -lcrypto -lssl $(if $(RTCONFIG_QCA), -ldl)"
	touch $@
else
email-3.1.3/stamp-h1: openssl
	cd $(TOP)/email-3.1.3 && \
	$(CONFIGURE) --with-ssl --sysconfdir=/etc \
		CFLAGS=-I$(SRCBASE)/router/openssl/include \
		LDFLAGS="-L$(SRCBASE)/router/openssl" \
		LIBS="-lcrypto -lssl $(if $(RTCONFIG_QCA), -ldl)"
	touch $@
endif

email-3.1.3: email-3.1.3/stamp-h1
	$(MAKE) -C email-3.1.3

email-3.1.3-clean:
	-@$(MAKE) -C email-3.1.3 clean
	@rm -f email-3.1.3/stamp-h1

email-3.1.3-install:
	install -D email-3.1.3/src/email $(INSTALLDIR)/email-3.1.3/usr/sbin/email
	$(STRIP) $(INSTALLDIR)/email-3.1.3/usr/sbin/email

#diskdev_cmds-332.14
diskdev_cmds-332.14: openssl
	@$(SEP)
	cd $(TOP)/diskdev_cmds-332.14 && \
	make -f Makefile.lnx

diskdev_cmds-332.14-install: diskdev_cmds-332.14
	install -D diskdev_cmds-332.14/newfs_hfs.tproj/newfs_hfs $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfsplus
	install -D diskdev_cmds-332.14/fsck_hfs.tproj/fsck_hfs $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfsplus
	$(STRIP) $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfsplus
	$(STRIP) $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfsplus
	cd $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin && \
	rm -f mkfs.hfs && \
	rm -f fsck.hfs && \
	ln -s mkfs.hfsplus mkfs.hfs && \
	ln -s fsck.hfsplus fsck.hfs

diskdev_cmds-332.14-clean:
	cd $(TOP)/diskdev_cmds-332.14 && \
	make -f Makefile.lnx clean
	rm -f $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfs 
	rm -f $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfs 

GeoIP-1.6.2/stamp-h1:
	cd GeoIP-1.6.2 && autoreconf -i -f && $(CONFIGURE) \
	--bindir=/sbin ac_cv_func_malloc_0_nonnull=yes \
	ac_cv_func_realloc_0_nonnull=yes --disable-dependency-tracking \
	--datadir=/usr/share
	touch $@

GeoIP-1.6.2: GeoIP-1.6.2/stamp-h1
	$(MAKE) -C $@

GeoIP-1.6.2-configure:
	( cd GeoIP-1.6.2 && $(CONFIGURE) \
	--bindir=/sbin ac_cv_func_malloc_0_nonnull=yes \
	ac_cv_func_realloc_0_nonnull=yes --disable-dependency-tracking \
	--datadir=/usr/share)

GeoIP-1.6.2-install: GeoIP-1.6.2
#	install -D GeoIP-1.6.2/apps/.libs/geoiplookup $(INSTALLDIR)/GeoIP-1.6.2/usr/sbin/geoiplookup
	install -D GeoIP-1.6.2/data/GeoIP.dat $(INSTALLDIR)/GeoIP-1.6.2/usr/share/GeoIP/GeoIP.dat
	install -D GeoIP-1.6.2/libGeoIP/.libs/libGeoIP.so.1.6.2 $(INSTALLDIR)/GeoIP-1.6.2/usr/lib/libGeoIP.so.1.6.2
#	$(STRIP) $(INSTALLDIR)/GeoIP-1.6.2/usr/sbin/geoiplookup
	$(STRIP) $(INSTALLDIR)/GeoIP-1.6.2/usr/lib/*.so.*
	cd $(INSTALLDIR)/GeoIP-1.6.2/usr/lib && \
		ln -sf libGeoIP.so.1.6.2 libGeoIP.so && \
		ln -sf libGeoIP.so.1.6.2 libGeoIP.so.1

GeoIP-1.6.2-clean:
	-@[ ! -f GeoIP-1.6.2/Makefile ] || $(MAKE) -C GeoIP-1.6.2 distclean
	@rm -f GeoIP-1.6.2/stamp-h1

Transmission-configure:
	( cd Transmission && ./autogen.sh && \
		$(CONFIGURE) --prefix=/usr --bindir=/usr/sbin --libdir=/usr/lib \
			CFLAGS="$(CFLAGS) -I$(STAGEDIR)/usr/include" \
			LDFLAGS="$(LDFLAGS) -L$(STAGEDIR)/usr/lib" \
			--disable-nls --disable-gtk \
	)

Transmission/Makefile:
	$(MAKE) Transmission-configure

Transmission: curl libevent-2.0.21 Transmission/Makefile
	@$(SEP)
	$(MAKE) -C $@

Transmission-install: Transmission
	install -D $</daemon/transmission-daemon $(INSTALLDIR)/$</usr/sbin/transmission-daemon
	install -D $</daemon/transmission-remote $(INSTALLDIR)/$</usr/sbin/transmission-remote
	$(STRIP) $(INSTALLDIR)/$</usr/sbin/*

Transmission-clean:
	[ ! -f Transmission/Makefile ] || $(MAKE) -C distclean
	@rm -f Transmission/Makefile

wget/Makefile.in: wget/Makefile.am
	cd wget && autoreconf -fi

wget/Makefile: wget/Makefile.in
	cd wget && $(CONFIGURE) \
		--with-ssl=openssl --with-libssl-prefix=$(TOP)/openssl \
		--sysconfdir=/etc --disable-opie --disable-ntlm --disable-debug --disable-nls --disable-rpath \
		$(if $(RTCONFIG_IPV6),,--disable-ipv6) --disable-iri --without-included-regex  \
		--disable-dependency-tracking \
		CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections -I$(TOP)/openssl/include -I$(TOP)/zlib" \
		LDFLAGS="$(EXTRALDFLAGS) -Wl,--gc-sections -L$(TOP)/openssl -lssl -lcrypto -L$(TOP)/zlib -lz"

wget: openssl zlib wget/Makefile
	$(MAKE) -C $@

wget-clean:
	[ ! -f wget/Makefile ] || $(MAKE) -C wget distclean
	@rm -f wget/Makefile

wget-install:
	install -D wget/src/wget $(INSTALLDIR)/wget/usr/sbin/wget
	$(STRIP) $(INSTALLDIR)/wget/usr/sbin/wget

gmp:
	$(MAKE) -C $@ && $(MAKE) $@-stage

gmp-install: gmp
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

gmp-clean:
	$(MAKE) -C gmp clean

gmp-stage:
	$(MAKE) -C gmp stage

libqcacommon:
	$(MAKE) -C $@ && $(MAKE) $@-stage

libqcacommon-install: libqcacommon
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

libqcacommon-clean:
	$(MAKE) -C libqcacommon clean

libqcacommon-stage:
	$(MAKE) -C libqcacommon stage

openswan-nss: gmp
	$(MAKE) -C $@ && $(MAKE) $@-stage

openswan-nss-install: openswan-nss
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

openswan-nss-clean:
	$(MAKE) -C openswan-nss clean

openswan-nss-stage:
	$(MAKE) -C openswan-nss stage

qca-whc30-lbd: libqcacommon
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-wifi:
	$(MAKE) -C $@ TARGET_CROSS=$(CROSS_COMPILE) LINUX_KARCH=$(ARCH) LINUX_VERSION=$(LINUX_KERNEL) LINUX_DIR=$(LINUXDIR) && $(MAKE) $@-stage

qca-wifi-install: qca-wifi
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< LINUX_VERSION=$(LINUX_KERNEL) install

qca-wifi-clean:
	$(MAKE) -C qca-wifi clean KERNELPATH=$(LINUXDIR)
	$(RM) -f qca-wifi/lsdk_flags

qca-wifi-stage:
	$(MAKE) -C qca-wifi stage

#qca-wifi.ipq40xx: qca-nss-drv.ipq40xx iproute2-3.x
qca-wifi.ipq40xx: iproute2-3.x
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-wifi.ipq40xx-install: qca-wifi.ipq40xx
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

qca-wifi.ipq40xx-clean:
	$(MAKE) -C qca-wifi.ipq40xx clean
	$(RM) -f qca-wifi.ipq40xx/source/lsdk_flags

qca-wifi.ipq40xx-stage:
	$(MAKE) -C qca-wifi.ipq40xx stage

qca-wifi-qca9990: qca-nss-drv
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-wifi-qca9990-install: qca-wifi-qca9990
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

qca-wifi-qca9990-clean:
	$(MAKE) -C qca-wifi-qca9990 clean
	$(RM) -f qca-wifi-qca9990/lsdk_flags

qca-wifi-qca9990-stage:
	$(MAKE) -C qca-wifi-qca9990 stage

qca-wifi-qca9994: qca-nss30-drv iproute2-3.x
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-wifi-qca9994-install: qca-wifi-qca9994
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

qca-wifi-qca9994-clean:
	$(MAKE) -C qca-wifi-qca9994 clean
	$(RM) -f qca-wifi-qca9994/lsdk_flags

qca-wifi-qca9994-stage:
	$(MAKE) -C qca-wifi-qca9994 stage

qca-hostap: qca-wifi
	$(MAKE) -C $@

qca-hostap.ipq40xx: qca-wifi.ipq40xx libnl-bf openssl
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-drv.ipq40xx: qca-nss-gmac.ipq40xx
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-drv.ipq40xx-stage:
	$(MAKE) -C qca-nss-drv.ipq40xx stage

qca-nss-gmac.ipq40xx:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-gmac.ipq40xx-stage:
	$(MAKE) -C qca-nss-gmac.ipq40xx && $(MAKE) -C qca-nss-gmac.ipq40xx stage

qca-rfs.ipq40xx:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-rfs.ipq40xx-stage:
	$(MAKE) -C qca-rfs.ipq40xx stage

qca-ssdk.ipq40xx:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-ssdk.ipq40xx-stage:
	$(MAKE) -C qca-ssdk.ipq40xx stage

qca-ssdk-shell.ipq40xx:
	$(MAKE) -C $@

qca-hyfi-qdisc.ipq40xx:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-hyfi-qdisc.ipq40xx-stage:
	$(MAKE) -C qca-hyfi-qdisc.ipq40xx && $(MAKE) -C qca-hyfi-qdisc.ipq40xx stage

qca-hyfi-qdisc.ipq40xx-install: qca-hyfi-qdisc.ipq40xx
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

qca-hyfi-bridge.ipq40xx:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-hyfi-bridge.ipq40xx-stage:
	$(MAKE) -C qca-hyfi-bridge.ipq40xx && $(MAKE) -C qca-hyfi-bridge.ipq40xx stage

qca-hyfi-bridge.ipq40xx-install: qca-hyfi-bridge.ipq40xx
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

libnl-bf:
	$(MAKE) -C $@ && $(MAKE) $@-stage

shortcut-fe.ipq40xx: libnl-bf
	$(MAKE) -C $@ && $(MAKE) $@-stage

shortcut-fe.ipq40xx-stage:
	$(MAKE) -C shortcut-fe.ipq40xx stage

qca-hostap-qca9990: qca-wifi-qca9990
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-hostap-qca9994: qca-wifi-qca9994 libnl-bf openssl
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-mcs30-lkm:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-mcs30-lkm-stage:
	$(MAKE) -C qca-mcs30-lkm stage

qca-nss-clients: qca-nss-drv
	$(MAKE) -C $@

qca-nss30-clients: qca-nss30-drv qca-nss30-crypto
	$(MAKE) -C $@

qca-nss-drv: qca-nss-gmac
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-drv-stage:
	$(MAKE) -C qca-nss-drv stage

qca-nss30-drv: qca-nss30-gmac
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss30-drv-stage:
	$(MAKE) -C qca-nss30-drv stage

qca-nss-ecm: qca-nss-drv
	$(MAKE) -C $@

qca-nss30-ecm: qca-nss30-drv $(if $(CONFIG_SWITCH_QCA8337N),qca-mcs30-lkm) shortcut-fe30
	$(MAKE) -C $@

qca-nss-gmac:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-gmac-stage:
	$(MAKE) -C qca-nss-gmac && $(MAKE) -C qca-nss-gmac stage

qca-nss30-gmac:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss30-gmac-stage:
	$(MAKE) -C qca-nss30-gmac && $(MAKE) -C qca-nss30-gmac stage

qca-nss-crypto: qca-nss-drv
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-crypto-stage:
	$(MAKE) -C qca-nss-crypto stage

qca-nss30-crypto: qca-nss30-drv
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss30-crypto-stage:
	$(MAKE) -C qca-nss30-crypto stage

qca-nss-cfi: qca-nss-crypto
	$(MAKE) -C $@

qca-nss30-cfi: qca-nss30-crypto
	$(MAKE) -C $@

qca-nss-macsec: qca-nss-gmac
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-nss-macsec-stage:
	$(MAKE) -C qca-nss-macsec stage

qca-ssdk:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-ssdk-stage:
	$(MAKE) -C qca-ssdk stage

qca-ssdk30:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-ssdk30-stage:
	$(MAKE) -C qca-ssdk30 stage

ethtool-3.7/configure:
	cd ethtool-3.7 && ./autogen.sh

ethtool-3.7/Makefile: ethtool-3.7/configure
	cd ethtool-3.7 && $(CONFIGURE) \
		--with-kernel=$(LINUXDIR)

ethtool-3.7: ethtool-3.7/Makefile
	$(MAKE) -C $@ KERNEL_DIR=$(LINUXDIR)

ethtool-3.7-clean:
	$(MAKE) -C ethtool-3.7 distclean

ethtool-3.7-install: ethtool-3.7
	install -D ethtool-3.7/ethtool $(INSTALLDIR)/ethtool-3.7/usr/bin/ethtool
	$(STRIP) $(INSTALLDIR)/ethtool-3.7/usr/bin/ethtool

shortcut-fe30: libnl-bf
	$(MAKE) -C $@ && $(MAKE) $@-stage

shortcut-fe30-stage:
	$(MAKE) -C shortcut-fe30 stage

LinuxART2-install: LinuxART2
ifneq ($(ART2_INSTALLDIR),)
	$(MAKE) -C $< install INSTALLDIR=$(ART2_INSTALLDIR)/$<
else
	$(MAKE) -C $< install INSTALLDIR=$(INSTALLDIR)/$<
endif

stage-tags: dummy
	ctags -aR -f $(SRCBASE)/tags $(STAGEDIR)/usr/include

sysstat-10.0.3-install:
	$(MAKE) -C sysstat-10.0.3 install_base DESTDIR=$(INSTALLDIR)/sysstat-10.0.3 INSTALL_DOC=n
	mv $(INSTALLDIR)/sysstat-10.0.3/etc $(INSTALLDIR)/sysstat-10.0.3/etc_sysstat
	rm -rf $(INSTALLDIR)/sysstat-10.0.3/var
#
# tunnel related 
#

asusnatnl: openssl
	$(MAKE) -C $@

asusnatnl-clean:
	$(MAKE) -C asusnatnl clean

asusnatnl-install: asusnatnl
	install -d $(INSTALLDIR)/asusnatnl/usr/lib/
	install -D asusnatnl/natnl/libasusnatnl.so $(INSTALLDIR)/asusnatnl/usr/lib/libasusnatnl.so
	$(STRIP) -s $(INSTALLDIR)/asusnatnl/usr/lib/libasusnatnl.so

mastiff: shared nvram${BCMEX} wb
	cd aaews && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \
	make mastiff

mastiff-install:
	install -d $(INSTALLDIR)/wb/usr/lib/
	install -D aaews/mastiff $(INSTALLDIR)/wb/usr/sbin/mastiff
	$(STRIP) -s $(INSTALLDIR)/wb/usr/sbin/mastiff

mastiff-clean:
	cd aaews && make clean 
	echo $(INSTALLDIR)


#aaews: wb

#wb: shared nvram curl openssl libxml2 asusnatnl 
aaews: wb
	cd aaews && \
	CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \
	make router
#make static-lib && \

#$(MAKE)
#CC=mipsel-uclibc-g++ AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \

aaews-install:
	install -d $(INSTALLDIR)/wb/usr/lib/
	install -D wb/libws.so $(INSTALLDIR)/wb/usr/lib/libws.so
	$(STRIP) -s $(INSTALLDIR)/wb/usr/lib/libws.so
	install -D aaews/aaews $(INSTALLDIR)/wb/usr/sbin/aaews
	$(STRIP) -s $(INSTALLDIR)/wb/usr/sbin/aaews
ifeq ($(RTCONFIG_BCMARM),y)
	install -D $(shell dirname $(shell which $(CXX)))/../arm-brcm-linux-uclibcgnueabi/lib/libstdc++.so.6 $(INSTALLDIR)/wb/usr/lib/libstdc++.so.6
else
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/wb/usr/lib/libstdc++.so.6
endif

aaews-clean:
	cd aaews && make clean

#json-c-0.12
json-c/stamp-h1:
	cd $(TOP)/json-c && \
	$(CONFIGURE) --enable-shared --disable-static \
	ac_cv_func_realloc_0_nonnull=yes ac_cv_func_malloc_0_nonnull=yes \
	CFLAGS="-Os -Wno-error $(EXTRACFLAGS)" \
	LDFLAGS="$(EXTRALDFLAGS)"
	touch $@

json-c: json-c/stamp-h1
	$(MAKE) -C $@

json-c-install:
	install -D json-c/.libs/libjson-c.so.2.0.1 $(INSTALLDIR)/json-c/usr/lib/libjson-c.so.2.0.1
	$(STRIP) $(INSTALLDIR)/json-c/usr/lib/libjson-c.so.2.0.1
	cd $(INSTALLDIR)/json-c/usr/lib && ln -sf libjson-c.so.2.0.1 libjson-c.so
	cd $(INSTALLDIR)/json-c/usr/lib && ln -sf libjson-c.so.2.0.1 libjson-c.so.2

json-c-clean:
	-@[ ! -f json-c/Makefile ] || $(MAKE) -C json-c clean
	-@rm -f json-c/stamp-h1

phddns/stamp-h1:
	cd phddns && $(CONFIGURE) --prefix="" \
		CFLAGS="-Os -I$(SRCBASE)/include -I$(TOP)/shared $(EXTRACFLAGS)" \
		LDFLAGS="-L$(TOP)/nvram$(BCMEX) -L$(TOP)/shared $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client) $(EXTRALDFLAGS)" \
		LIBS="-lnvram -lshared $(if $(RTCONFIG_QTN),-lqcsapi_client)"
	@touch $@

phddns: phddns/stamp-h1
	$(MAKE) -C phddns

phddns-install:
	install -D phddns/src/phddns $(INSTALLDIR)/phddns/usr/sbin/phddns
	$(STRIP) $(INSTALLDIR)/phddns/usr/sbin/phddns

phddns-clean:
	@[ ! -f phddns/Makefile ] || $(MAKE) -C phddns clean
	@rm -f phddns/stamp-h1

quicksec-6.0-fips:
	@$(SEP)
	@$(MAKE) KDIR=$(LINUXDIR) CAPTURE=IP -C quicksec-6.0-fips/build/linux/usermode-ue all
	@$(MAKE) KDIR=$(LINUXDIR) CAPTURE=IP -C quicksec-6.0-fips/gplinterceptor/build/linux

quicksec-6.0-fips-install:
	@install -D quicksec-6.0-fips/gplinterceptor/build/linux/interceptor-gpl/quicksec_usermode.ko $(INSTALLDIR)/quicksec-6.0-fips/lib/modules/3.4.103/kernel/net/quicksec_usermode.ko
	@install -D quicksec-6.0-fips/build/linux/usermode-ue/quicksecpm_usermode $(INSTALLDIR)/quicksec-6.0-fips/usr/sbin/quicksecpm_usermode
	@chmod 664 $(INSTALLDIR)/quicksec-6.0-fips/lib/modules/3.4.103/kernel/net/quicksec_usermode.ko
	@chmod 755 $(INSTALLDIR)/quicksec-6.0-fips/usr/sbin/quicksecpm_usermode
	@$(STRIP) $(INSTALLDIR)/quicksec-6.0-fips/usr/sbin/quicksecpm_usermode
ifneq ($(wildcard quicksec-6.0-fips/config/*.xml),)
	@mkdir -p $(TARGETDIR)/rom/etc/quicksec
	@cp -a quicksec-6.0-fips/config/*.xml $(TARGETDIR)/rom/etc/quicksec
	@chmod 644 $(TARGETDIR)/rom/etc/quicksec/*.xml
endif

quicksec-6.0-fips-clean:
	-@$(MAKE) KDIR=$(LINUXDIR) -C quicksec-6.0-fips/build/linux clean
	-@$(MAKE) KDIR=$(LINUXDIR) -C quicksec-6.0-fips/gplinterceptor/build/linux clean

#
# Generic rules
#

%:
	@[ ! -d $* ] || [ -f $*/Makefile ] || $(MAKE) $*-configure
	@[ ! -d $* ] || ( $(SEP); $(MAKE) -C $* )


%-clean:
	-@[ ! -d $* ] || $(MAKE) -C $* clean

%-install: %
	@echo $*
	@[ ! -d $* ] || $(MAKE) -C $* install INSTALLDIR=$(INSTALLDIR)/$*

%-stage:
	@echo $*
	@[ ! -d $* ] || $(MAKE) -C $* install DESTDIR=$(STAGEDIR)

%-build:
	$(MAKE) $*-clean $*

%/Makefile:
	[ ! -d $* ] || $(MAKE) $*-configure

%-configure:
	@[ ! -d $* ] || ( cd $* ; \
		$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
	)


$(obj-y) $(obj-n) $(obj-clean) $(obj-install): dummy

.PHONY: all clean distclean mrproper install package image
.PHONY: conf mconf oldconf kconf kmconf config menuconfig oldconfig
.PHONY: dummy libnet libpcap


