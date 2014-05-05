#
# Broadcom Linux Router Makefile
#
# Copyright 2005, Broadcom Corporation
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

ifneq ($(RTCONFIG_RALINK),y)
export CFLAGS += -DBCMWPA2
ifeq ($(RTCONFIG_BCMWL6),y)
export CFLAGS += -DBCMQOS
export CFLAGS += -DBCM_DCS
export CFLAGS += -DEXT_ACS
export CFLAGS += -DD11AC_IOTYPES
export CFLAGS += -DNAS_GTK_PER_STA
export CFLAGS += -DPHYMON
export CFLAGS += -DPROXYARP
export CFLAGS += -DTRAFFIC_MGMT
export CFLAGS += -DTRAFFIC_MGMT_RSSI_POLICY
ifeq ($(RTCONFIG_BCMARM),y)
export CFLAGS += -DMFP
endif
endif
export CFLAGS += $(EXTRACFLAGS)
endif

ifeq ($(RTCONFIG_BCMARM),y)

LINUX_VERSION=2_6_36
LINUX_KERNEL=2.6.36
export PLATFORM LIBDIR USRLIBDIR LINUX_VERSION

WLAN_ComponentsInUse := bcmwifi clm ppr olpc
include ../../src-rt-6.x.4708/makefiles/WLAN_Common.mk
export BASEDIR := $(WLAN_TreeBaseA)
export EXTRALDFLAGS = -lgcc_s
export EXTRALDFLAGS2 = -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared

export LD_LIBRARY_PATH := $(SRCBASE)/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3/lib
ifeq (2_6_36,$(LINUX_VERSION))
export  LINUXDIR := $(BASEDIR)/src-rt-6.x.4708/linux/linux-2.6.36
export  KBUILD_VERBOSE := 1
export  BUILD_MFG := 0
endif
SUBMAKE_SETTINGS = SRCBASE=$(SRCBASE) BASEDIR=$(BASEDIR)
SUBMAKE_SETTINGS += ARCH=$(ARCH)
export CFLAGS += -O2
export OPTCFLAGS = -O2
WLCFGDIR=$(SRCBASE)/wl/config

ROOT_IMG := target.squashfs
CRAMFSDIR := cramfs

export MKSYM := $(shell which $(TOP)/misc/mksym.pl)
endif

ifeq ($(RTCONFIG_BCMWL6),$(RTAC53U),y)
ifeq ($(RTCONFIG_BCMWL6), y)
WLAN_ComponentsInUse := bcmwifi clm ppr
else ifeq ($(RTAC53U),y)
WLAN_ComponentsInUse := bcmwifi clm ppr olpc
endif
include ../../$(SRCBASE)/makefiles/WLAN_Common.mk
endif

ifneq ($(RTCONFIG_BCMARM),y)
KDIR=$(TOP)/kernel_header
else
KDIR=$(LINUXDIR)
endif

export CFLAGS += -D__CONFIG_EMF__

ifeq ($(RTCONFIG_WPS),y)
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
#
#
#
SEP=echo "\033[41;1m   $@   \033[0m"

#
# standard packages
#
obj-$(RTCONFIG_QTN) += libqcsapi_client
obj-$(RTCONFIG_DMALLOC) += dmalloc

ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P),y)
obj-y += reg
endif
ifneq ($(RTCONFIG_BCMARM),y)
obj-y += lzma-loader
endif
obj-y += shared
obj-y += nvram$(BCMEX)
ifneq ($(RTCONFIG_RALINK),y)
else
obj-y += wireless_tools
ifneq ($(findstring linux-3.x,$(LINUXDIR)),)
obj-y += rt2880_app
else
obj-y += ated
ifneq ($(RTN11P),y)
obj-y += flash
endif
ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P),y)
else
obj-y += hw_nat
endif
endif
endif
ifeq ($(RTCONFIG_DSL),y)
obj-y += dsl_drv_tool
endif
ifneq ($(or $(findstring linux-2.6.36,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR))),)
obj-y += iptables-1.4.x
obj-y += iproute2-3.x
else
obj-y += iptables
obj-y += iproute2
endif

ifeq ($(RTCONFIG_BWDPI),y)
obj-y += bwdpi
endif
obj-y += rc
ifeq ($(RTCONFIG_USB),y)
obj-y += rom
endif
obj-y += others
obj-y += busybox
obj-y += httpd
obj-y += www
obj-y += bridge
ifeq ($(RTCONFIG_DNSMASQ),y)
obj-y += dnsmasq
else
obj-y += dproxy
obj-y += udhcpd
endif
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
ifneq ($(RTCONFIG_RALINK),y)
#obj-y += et
obj-y += wlconf$(BCMEX)
obj-y += utils$(BCMEX)
obj-y += nas$(BCMEX)
obj-y += eapd$(BCMEX)/linux
obj-$(CONFIG_LIBBCM) += libbcm
obj-$(CONFIG_LIBUPNP) += libupnp
obj-$(RTCONFIG_WPS) += wps$(BCMEX)
obj-$(RTCONFIG_EMF) += emf
obj-$(RTCONFIG_EMF) += igs
endif

obj-y += libbcmcrypto
#obj-y += cyassl
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
obj-y += hub-ctrl
obj-$(RTCONFIG_USB_PRINTER) += u2ec
obj-$(RTCONFIG_USB_PRINTER) += LPRng

obj-$(RTCONFIG_USB_MODEM) += sdparm-1.02
obj-$(RTCONFIG_USB_MODEM) += comgt-0.32
obj-$(RTCONFIG_USB_MODEM) += uqmi

ifneq ($(or $(findstring linux-2.6.36.x,$(LINUXDIR)),$(findstring linux-2.6.36,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR))),)
#obj-$(RTCONFIG_USB_MODEM) += usbmodeswitch-1.2.3
obj-$(RTCONFIG_USB_MODEM) += usb-modeswitch-2.1.1
else
obj-$(RTCONFIG_USB_MODEM) += usbmodeswitch
endif

obj-$(RTCONFIG_USB_BECEEM) += madwimax-0.1.1 # for Samsung WiMAX
obj-$(RTCONFIG_USB_BECEEM) += openssl # for Beceem, GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += Beceem_BCMS250$(BCMEX) # for Beceem WiMAX
obj-$(RTCONFIG_USB_BECEEM) += wpa_supplicant-0.7.3 # for GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += zlib # for GCT WiMAX
obj-$(RTCONFIG_USB_BECEEM) += gctwimax-0.0.3rc4 # for GCT WiMAX

obj-y += libdisk

obj-y += e2fsprogs
obj-$(RTCONFIG_NFS) += nfs-utils
obj-$(RTCONFIG_NFS) += portmap

#obj-y += dosfstools
#obj-$(RTCONFIG_NTFS) += linux-ntfs-2.0.0
obj-$(RTCONFIG_FTP) += vsftpd

ifeq ($(CONFIG_LINUX26),y)
ifeq ($(RTCONFIG_SAMBASRV),y)
NEED_EX_NLS = y
endif
ifeq ($(RTCONFIG_USB_EXTRAS),y)
NEED_EX_USB = y
endif
endif

ifeq ($(RTCONFIG_SAMBASRV),y)
ifeq ($(RTCONFIG_SAMBA3),y)
NEED_SAMBA3 = y
else
NEED_SAMBA2 = y
endif
endif
endif

ifeq ($(RTCONFIG_IPV6),y)
export RTCONFIG_IPV6 := y
IPTABLES_IPV6_OPT := --enable-ipv6
else
RTCONFIG_IPV6 :=
IPTABLES_IPV6_OPT := --disable-ipv6
endif
# install before samba3 for not overwriting some binaries
obj-$(RTCONFIG_WEBDAV) += samba-3.5.8
obj-$(NEED_SAMBA2) += samba
ifeq ($(RTCONFIG_BCMARM),y)
obj-$(NEED_SAMBA3) += samba-3.0.25b
else
obj-$(NEED_SAMBA3) += samba3
endif
ifeq ($(RTCONFIG_NTFS3G),y)
obj-$(RTCONFIG_NTFS) += ntfs-3g
else
ifeq ($(RTCONFIG_TUXERA),y)
obj-$(RTCONFIG_NTFS) += tuxera
else
obj-$(RTCONFIG_NTFS) += ufsd
endif
endif
ifeq ($(RTCONFIG_HFS),y)
obj-$(RTCONFIG_PARAGON_HFS) += ufsd
obj-$(RTCONFIG_TUXERA_HFS) += tuxera
obj-$(RTCONFIG_KERNEL_HFSPLUS) += diskdev_cmds-332.14
endif
obj-$(RTCONFIG_EBTABLES) += ebtables
obj-$(RTCONFIG_IPV6) += libdaemon
obj-$(RTCONFIG_IPV6) += radvd
obj-$(RTCONFIG_IPV6) += dhcpv6
#obj-$(RTCONFIG_IPV6) += ecmh
obj-$(RTCONFIG_IPV6) += ndisc6

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
#obj-$(RTCONFIG_MEDIA_SERVER) += mt-daapd-svn-1696
#obj-$(RTCONFIG_MEDIA_SERVER) += mDNSResponder

MEDIA_SERVER_STATIC=y

#add by gauss for cloudsync
obj-$(RTCONFIG_CLOUDSYNC) += openssl
obj-$(RTCONFIG_CLOUDSYNC) += libxml2
obj-$(RTCONFIG_CLOUDSYNC) += curl-7.21.7
obj-$(RTCONFIG_CLOUDSYNC) += asuswebstorage$(BCMEX)
obj-$(RTCONFIG_CLOUDSYNC) += inotify

obj-$(RTCONFIG_CLOUDSYNC) += zlib # for neon
obj-$(RTCONFIG_CLOUDSYNC) += neon
obj-$(RTCONFIG_CLOUDSYNC) += webdav_client$(BCMEX)

#For timemachine
obj-$(RTCONFIG_TIMEMACHINE) += openssl
obj-$(RTCONFIG_TIMEMACHINE) += libgpg-error-1.10
obj-$(RTCONFIG_TIMEMACHINE) += libgcrypt-1.5.1
obj-$(RTCONFIG_TIMEMACHINE) += db-4.8.30
obj-$(RTCONFIG_TIMEMACHINE) += netatalk-3.0.5
ifeq ($(or $(RTCONFIG_TIMEMACHINE),$(RTCONFIG_MDNS)),y)
obj-$(RTCONFIG_TIMEMACHINE) += avahi-0.6.31
obj-$(RTCONFIG_TIMEMACHINE) += expat-2.0.1
else
obj-$(RTCONFIG_MEDIA_SERVER) += avahi-0.6.31
obj-$(RTCONFIG_MEDIA_SERVER) += expat-2.0.1
endif

#
# configurable packages
#
RTCONFIG_PPP_BASE = y
obj-$(RTCONFIG_PPP_BASE) += pppd rp-pppoe
obj-$(RTCONFIG_L2TP) += rp-l2tp
obj-$(RTCONFIG_PPTP) += accel-pptp
obj-$(RTCONFIG_EAPOL) += wpa_supplicant
obj-$(RTCONFIG_HTTPS) += openssl
obj-$(RTCONFIG_SSH) += dropbear
#obj-$(RTCONFIG_ZEBRA) += zebra
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
obj-$(RTCONFIG_WEBDAV) += lighttpd-1.4.29
obj-$(RTCONFIG_WEBDAV) += libexif

obj-$(RTCONFIG_IXIAEP) += ixia_endpoint$(BCMEX)

# Add for ASUS Components
obj-y += miniupnpd
obj-y += igmpproxy
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
obj-y += acsd$(BCMEX)
endif
else
obj-y += lldt
endif
obj-y += networkmap
obj-y += infosvr
obj-y += ez-ipupdate

obj-$(RTCONFIG_STRACE) += strace

obj-$(RTCONFIG_GDB) += gdb
obj-$(RTCONFIG_LLDP) += openlldp

obj-n += lsof
obj-$(RTCONFIG_IPERF) += iperf

obj-$(RTCONFIG_PUSH_EMAIL) += zlib # for libxml2, curl.
obj-$(RTCONFIG_PUSH_EMAIL) += openssl
obj-$(RTCONFIG_PUSH_EMAIL) += libxml2
obj-$(RTCONFIG_PUSH_EMAIL) += curl-7.21.7
obj-$(RTCONFIG_PUSH_EMAIL) += wb
obj-$(RTCONFIG_PUSH_EMAIL) += email-3.1.3
obj-$(RTCONFIG_PUSH_EMAIL) += push_log
ifeq ($(RTCONFIG_PUSH_EMAIL),y)
PUSH_LOG=push_log
endif

obj-$(RTCONFIG_NORTON) += norton${BCMEX}

#obj-y += lighttpd-1.4.28
#obj-$(RTCONFIG_USB_WEBDAV) += lighttpd-1.4.29

obj-$(UBI) += mtd-utils
obj-$(UBIFS) += mtd-utils
obj-$(UBIFS) += util-linux lzo zlib
ifeq ($(RTCONFIG_RALINK),y)
obj-$(RA_SKU) += ra_SingleSKU
endif
obj-$(RTCONFIG_BCMARM) += wl$(BCMEX)

#obj-y += bonnie

ifeq ($(RTN65U), y)
obj-y += asm1042
endif

ifeq ($(RTCONFIG_BCMARM),y)
obj-prelibs =$(filter nvram$(BCMEX) libbcmcrypto shared netconf libupnp libz libbcm, $(obj-y))
obj-postlibs := $(filter-out $(obj-prelibs), $(obj-y))
endif

ifneq ($(RTCONFIG_4M_SFP),y)
obj-y += netstat-nat
endif

obj-$(RTCONFIG_TCPDUMP) += libpcap
obj-$(RTCONFIG_TCPDUMP) += tcpdump-4.4.0

# Merlin addons
obj-n += wol
obj-y += ipset

obj-clean := $(foreach obj, $(obj-y) $(obj-n) $(obj-), $(obj)-clean)
obj-install := $(foreach obj,$(obj-y),$(obj)-install)

# Specify iptables path and target for miniupnpd
ifneq ($(findstring iptables-1.4.x,$(obj-y)),)
IPTABLESPATH=$(TOP)/iptables-1.4.x
export IPTABLESPATH
IPTABLES_TARGET = iptables-1.4.x
else
IPTABLES_TARGET = iptables
endif

ifneq ($(or $(findstring linux-2.6.36.x,$(LINUXDIR)),$(findstring linux-3.x,$(LINUXDIR))),)
MKSQUASHFS_TARGET = mksquashfs
else
MKSQUASHFS_TARGET = mksquashfs-lzma
endif
MKSQUASHFS = $(MKSQUASHFS_TARGET)

ifneq ($(findstring linux-3.x,$(LINUXDIR)),)
LINUX_ARCH_ASM_INCL_DIR=$(LINUXDIR)/arch/mips/include/asm
else
LINUX_ARCH_ASM_INCL_DIR=$(LINUXDIR)/include/asm-mips
endif

#
# Basic rules
#

all: clean-build kernel_header libc version obj_prelibs kernel $(obj-y)

ifeq ($(RTCONFIG_BCMARM),y)
	+$(MAKE) $(MAKE_ARGS) post_preplibs
endif

ifeq ($(RTCONFIG_BCMARM),y)
obj_prelibs:
	+$(MAKE) parallel=true $(MAKE_ARGS) $(obj-prelibs)

obj_postlibs:
	+$(MAKE) parallel=true $(MAKE_ARGS) $(obj-postlibs)

post_preplibs:  obj_postlibs
	# flip MFP on all config including apdef
	-cp $(WLCFGDIR)/wlconfig_apdef $(WLCFGDIR)/wlconfig_apdef.tmp
	-sed -e "s/MFP=1/MFP=0/g" $(WLCFGDIR)/wlconfig_apdef.tmp > $(WLCFGDIR)/wlconfig_apdef;
	-rm -f $(WLCFGDIR)/wlconfig_apdef.tmp
	# flip NPS on all config including apdef
	-cp $(WLCFGDIR)/wlconfig_apdef $(WLCFGDIR)/wlconfig_apdef.tmp
	-sed -e "s/NPS=1/NPS=0/g" $(WLCFGDIR)/wlconfig_apdef.tmp > $(WLCFGDIR)/wlconfig_apdef;
	-rm -f $(WLCFGDIR)/wlconfig_apdef.tmp
	# support for MFP_TEST
	-cp $(WLCFGDIR)/wlconfig_apdef $(WLCFGDIR)/wlconfig_apdef.tmp
	-sed -e "s/MFP_TEST=1/MFP_TEST=0/g" $(WLCFGDIR)/wlconfig_apdef.tmp > $(WLCFGDIR)/wlconfig_apdef;
	-rm -f $(WLCFGDIR)/wlconfig_apdef.tmp
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
	$(MAKE) -C $(LINUXDIR) prepare scripts
	find $(LINUXDIR)/include -name autoconf.h -exec cp {} $@/include/linux/ \;
	cp $(LINUXDIR)/Makefile $@/
else
kernel_header: $(LINUXDIR)/.config

endif

kernel: $(LINUXDIR)/.config
	@$(SEP)

ifneq ($(RTCONFIG_BCMARM),y)
ifeq ($(RTCONFIG_RALINK),y)
	@if ! grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) vmlinux CC=$(KERNELCC) LD=$(KERNELLD) $(PARALLEL_BUILD); \
	fi
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) LD=$(KERNELLD) $(PARALLEL_BUILD); \
	fi
else
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
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/bcm57xx/bcm57xx.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/emf/emf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/igs/igs.ko)
endif   # RTCONFIG_BCMARM

ifneq ($(RTCONFIG_BCMARM),y)
lzma-loader:
	$(MAKE) -C $(SRCBASE)/lzma-loader CROSS_COMPILE=$(CROSS_COMPILE) LD=$(LD)

lzma-loader-install: lzma-loader
	@$(SEP)
endif

kmod: dummy
	$(MAKE) -C $(LINUXDIR) modules CC=$(KERNELCC) $(PARALLEL_BUILD)

testfind:
	cd $(TARGETDIR)/lib/modules/* && find -name "*.o" -exec mv -i {} . \; || true
	cd $(TARGETDIR)/lib/modules/* && find -type d -delete || true

install package: $(obj-install) $(LINUXDIR)/.config gen_target

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
	find $(TARGETDIR)/lib/modules -name *.o -exec $(STRIP) --strip-debug {} \;
	find $(TARGETDIR)/lib/modules -name *.ko -exec $(STRIP) --strip-debug {} \;

#	-cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv diag/* . && rm -rf diag

# nice and clean
ifeq ($(RTCONFIG_RALINK),y)
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

	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv emf$(BCMEX)/* . && rm -rf emf$(BCMEX) || true
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv igs$(BCMEX)/* . && rm -rf igs$(BCMEX) || true
ifeq ($(RTAC53U),y)
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv ctf_5358/* . && rm -rf ctf_5358 || true
else
	cd $(TARGETDIR)/lib/modules/*/kernel/drivers/net && mv ctf$(BCMEX)/* . && rm -rf ctf$(BCMEX) || true
endif
	cd $(TARGETDIR)/lib/modules && rm -f */source || true

# misc
	for dir in $(wildcard $(patsubst %,$(INSTALLDIR)/%,$(obj-y))) ; do \
	    (cd $${dir} && tar cpf - .) | (cd $(TARGETDIR) && tar xpf -) \
	done

ifneq ($(RTCONFIG_L7),y)
	rm -f $(TARGETDIR)/usr/lib/iptables/libipt_layer7.so
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

ifeq ($(RTCONFIG_OPTIMIZE_SHARED_LIBS),y)
ifneq ($(RTCONFIG_BCMARM),y)
	@$(SRCBASE)/btools/libfoo.pl
endif
else
	@$(SRCBASE)/btools/libfoo.pl --noopt
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
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/usbnet.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/asix.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_ether.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/rndis_host.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_ncm.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_wdm.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/qmi_wwan.*o $(PLATFORMDIR)/extras/ || true
	$(if $(RTCONFIG_USB_MODEM),@cp -f,@mv) $(TARGETDIR)/lib/modules/*/kernel/drivers/net/usb/cdc_mbim.*o $(PLATFORMDIR)/extras/ || true
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
ifneq ($(RTCONFIG_BCMARM),y)
	mv $(TARGETDIR)/lib/modules/$(LINUX_KERNEL)/modules.dep $(TARGETDIR)/lib/modules/
else
	mv $(TARGETDIR)/lib/modules/*/modules.dep $(TARGETDIR)/lib/modules/
endif

ifeq ($(RTCONFIG_QTN),y)
	# install qtn boot-code and firmware
	install -D qtnimage/topaz-linux.lzma.img $(TARGETDIR)/rom/qtn/topaz-linux.lzma.img
	install -D qtnimage/u-boot.bin $(TARGETDIR)/rom/qtn/u-boot.bin
endif
ifeq ($(RTCONFIG_BWDPI),y)
	install -d $(TARGETDIR)/usr/bwdpi/
	cp -f bwdpi_bin/*.so $(TARGETDIR)/usr/bwdpi/
	cp -f bwdpi_bin/*.ko $(TARGETDIR)/usr/bwdpi/
	cp -f bwdpi_bin/*.trf $(TARGETDIR)/usr/bwdpi/
	install -D bwdpi_bin/qosd $(TARGETDIR)/usr/sbin/qosd
	install -D bwdpi_bin/wred $(TARGETDIR)/usr/sbin/wred
	install -D bwdpi_bin/bwdpi-rule-agent $(TARGETDIR)/usr/sbin/bwdpi-rule-agent
endif
	@echo ---

	@rm -f $(TARGETDIR)/lib/modules/*/build

image:
ifneq ($(RTCONFIG_BCMARM),y)
	@$(MAKE) -C $(LINUXDIR)/scripts/squashfs $(MKSQUASHFS_TARGET)
	@$(LINUXDIR)/scripts/squashfs/$(MKSQUASHFS) $(TARGETDIR) $(PLATFORMDIR)/target.image -all-root -noappend -nopad | tee target.info
else
	+$(MAKE) -C squashfs-4.2 mksquashfs
	squashfs-4.2/mksquashfs $(TARGETDIR) $(PLATFORMDIR)/$(ROOT_IMG) -noappend -all-root
endif
#	Package kernel and filesystem
#	if grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
#	    cp $(PLATFORMDIR)/target.image $(LINUXDIR)/arch/mips/ramdisk/$${CONFIG_EMBEDDED_RAMDISK_IMAGE} ; \
#	    $(MAKE) -C $(LINUXDIR) zImage ; \
#	else \
#	    cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/vmlinuz $(PLATFORMDIR)/ ; \
#	    trx -o $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/target.image ; \
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
	$(MAKE) dnsmasq-clean iproute2-clean
ifeq ($(RTCONFIG_BCMARM),y)
	$(MAKE) compressed-clean
endif

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

busybox: dummy
	@$(MAKE) -C busybox EXTRA_CFLAGS="-fPIC $(EXTRACFLAGS)" $(PARALLEL_BUILD)

# V=1

busybox-install:
	rm -rf $(INSTALLDIR)/busybox
	$(MAKE) -C busybox install EXTRA_CFLAGS="-fPIC $(EXTRACFLAGS)" CONFIG_PREFIX=$(INSTALLDIR)/busybox

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
	$(CONFIGURE) --prefix=/usr --sysconfdir=/etc --enable-elf-shlibs \
		--disable-tls --disable-rpath --disable-nls \
		--disable-debugfs --disable-imager --disable-resizer --disable-defrag --disable-uuidd
endif

e2fsprogs: e2fsprogs/Makefile
	$(MAKE) -C $@

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

httpd: shared nvram$(BCMEX) libdisk $(if $(RTCONFIG_HTTPS),mssl) $(PUSH_LOG)
	@$(SEP)
	@$(MAKE) -C httpd

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
	install -D $(shell dirname $(shell which $(CXX)))/../lib/libstdc++.so.6 $(INSTALLDIR)/bonnie/lib/libstdc++.so.6

bonnie-clean:
	[ ! -f bonnie/Makefile ] || $(MAKE) -C bonnie clean

www-install:
	@$(MAKE) -C www install INSTALLDIR=$(INSTALLDIR)/www TOMATO_EXPERIMENTAL=$(TOMATO_EXPERIMENTAL)

matrixssl:
	@$(SEP)
	@$(MAKE) -C matrixssl/src

matrixssl-install:
	@true

matrixssl-clean:
	-@$(MAKE) -C matrixssl/src clean

cyassl/stamp-h1:
	@cd cyassl && CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections \
	-DNO_MD4 -DNO_AES -DNO_ERROR_STRINGS -DNO_HC128 -DNO_RABBIT -DNO_PSK -DNO_DSA -DNO_DH -DNO_PWDBASED" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	PTHREAD_CFLAGS="-lpthread" PTHREAD_LIBS="-lpthread" \
	$(CONFIGURE) --with-libz=no
	@touch $@

cyassl: cyassl/stamp-h1
	@$(SEP)
	@$(MAKE) -C cyassl

cyassl-clean:
	-@$(MAKE) -C cyassl clean
	@rm -f cyassl/stamp-h1

cyassl-install:
	@true

ifeq ($(RTCONFIG_OPENVPN),y)
OPENSSL_CIPHERS:=enable-rc5
else
#OPENSSL_CIPHERS:=no-dh no-idea no-rc2 no-rc5 no-aes no-aes192 no-cast no-des no-modes no-tls1 no-tlsext
OPENSSL_CIPHERS:=
endif

#OPENSSL_CIPHERS:=enable-aes enable-tls1 enable-tlsext

openssl/stamp-h1:
	cd openssl && \
	./Configure $(HOSTCONFIG) --openssldir=/etc --cross-compile-prefix=' ' \
	-ffunction-sections -fdata-sections -Wl,--gc-sections \
	shared $(OPENSSL_CIPHERS) 
#	no-sha0 no-smime no-camellia no-krb5 no-rmd160 no-ripemd \
#	no-seed no-capieng no-cms no-gms no-gmp no-rfc3779 \
#	no-ec no-ecdh no-ecdsa no-err no-hw no-jpake no-threads \
#	no-zlib no-engine no-engines no-sse2 no-perlasm \
#	no-dtls1 no-store no-psk no-md2 no-mdc2 no-ts

	-@$(MAKE) -C openssl clean
	@touch $@

openssl: openssl/stamp-h1

openssl-clean:
	-@$(MAKE) -C openssl clean
	@rm -f openssl/stamp-h1

openssl-install: openssl
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


mssl:	openssl

mdu:	shared mssl

wb: openssl libxml2 curl-7.21.7

push_log: wb

rc: nvram$(BCMEX) shared libbcmcrypto libdisk $(PUSH_LOG) $(if $(RTCONFIG_QTN),libqcsapi_client) $(if $(CONFIG_LIBBCM),libbcm)

bridge/stamp-h1:
	cd bridge && CFLAGS="-Os -g $(EXTRACFLAGS)" \
	$(CONFIGURE) --prefix="" --with-linux-headers=$(LINUXDIR)/include
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

dmalloc: dmalloc/Makefile dmalloc/dmalloc.h.2 dmalloc/settings.h
	$(MAKE) -C dmalloc all shlib cxx

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

dmalloc-install: dmalloc-stage
	install -D $(STAGEDIR)/usr/sbin/dmalloc $(INSTALLDIR)/dmalloc/usr/sbin/dmalloc
	install -d $(INSTALLDIR)/dmalloc/usr/lib
	install -D $(STAGEDIR)/usr/lib/*.a $(INSTALLDIR)/dmalloc/usr/lib
	install -D $(STAGEDIR)/usr/lib/*.so* $(INSTALLDIR)/dmalloc/usr/lib
	$(STRIP) $(INSTALLDIR)/dmalloc/usr/sbin/dmalloc
	$(STRIP) $(INSTALLDIR)/dmalloc/usr/lib/*.so*

dmalloc-clean:
	[ ! -f dmalloc/Makefile ] || $(MAKE) -C dmalloc clean
	-rm -f dmalloc/{conf.h,dmalloc.h,dmalloc.h.2,settings.h,Makefile}

dnsmasq:
	@$(SEP)
	@$(MAKE) -C dnsmasq \
	COPTS="-DHAVE_BROKEN_RTC -DHAVE_LEASEFILE_EXPIRE -DNO_AUTH -DNO_IPSET \
	    $(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6) \
	    $(if $(RTCONFIG_USB_EXTRAS),,-DNO_SCRIPT -DNO_TFTP)" \
	CFLAGS="-Os -ffunction-sections -fdata-sections $(EXTRACFLAGS)" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections"

dnsmasq-install:
	install -D dnsmasq/src/dnsmasq $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq
	$(STRIP) $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq

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

iptables-1.4.x: iptables-1.4.x/Makefile
	@$(SEP)
	$(MAKE) -C $@ KERNEL_DIR=$(LINUXDIR) COPT_FLAGS="-Os $(EXTRACFLAGS) -U CONFIG_NVRAM_SIZE"

iptables-1.4.x/Makefile: iptables-1.4.x/configure
	$(MAKE) iptables-1.4.x-configure

iptables-1.4.x/configure:
	( cd iptables-1.4.x ; ./autogen.sh )

iptables-1.4.x-configure:
	( cd iptables-1.4.x ; \
		$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/sbin \
		--libdir=/usr/lib \
		$(IPTABLES_IPV6_OPT) \
		--with-kernel=$(LINUXDIR) \
	)

iptables-1.4.x-install:
	install -D iptables-1.4.x/iptables/.libs/xtables-multi $(INSTALLDIR)/iptables-1.4.x/usr/sbin/xtables-multi
	cd $(INSTALLDIR)/iptables-1.4.x/usr/sbin && \
		ln -sf xtables-multi iptables-save && \
		ln -sf xtables-multi iptables-restore && \
		ln -sf xtables-multi iptables
	install -d $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables
	install -D iptables-1.4.x/libiptc/.libs/lib*.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/
	cd $(INSTALLDIR)/iptables-1.4.x/usr/lib && \
		ln -sf libip4tc.so libip4tc.so.0 && \
		ln -sf libip4tc.so libip4tc.so.0.0.0 && \
		ln -sf libip6tc.so libip6tc.so.0 && \
		ln -sf libip6tc.so libip6tc.so.0.0.0 && \
		ln -sf libiptc.so  libiptc.so.0 && \
		ln -sf libiptc.so  libiptc.so.0.0.0
	install -D iptables-1.4.x/libxtables/.libs/lib*.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/
	cd $(INSTALLDIR)/iptables-1.4.x/usr/lib && \
		ln -sf libxtables.so libxtables.so.7 && \
		ln -sf libxtables.so libxtables.so.7.0.0
	install -D iptables-1.4.x/extensions/*.so $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables

ifeq ($(RTCONFIG_IPV6),y)
	cd $(INSTALLDIR)/iptables-1.4.x/usr/sbin && \
		ln -sf xtables-multi ip6tables-restore && \
		ln -sf xtables-multi ip6tables
endif

	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/sbin/xtables-multi
	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/lib/*.so*
	$(STRIP) $(INSTALLDIR)/iptables-1.4.x/usr/lib/xtables/*.so*

iptables-1.4.x-clean:
	[ ! -f iptables-1.4.x/Makefile ] || $(MAKE) -C iptables-1.4.x KERNEL_DIR=$(LINUXDIR) distclean

upnp: nvram$(BCMEX) shared iptables

miniupnpd/config.h:
	cd miniupnpd && ./genconfig.sh --leasefile --vendorcfg

miniupnpd: $(IPTABLES_TARGET) miniupnpd/config.h
	@$(SEP)
	cp -f ./shared/version.h miniupnpd/.
	$(MAKE) -C miniupnpd -f Makefile.linux $(PARALLEL_BUILD)

miniupnpd-clean:
	-@$(MAKE) -C miniupnpd -f Makefile.linux clean

miniupnpd-install:
	install -D miniupnpd/miniupnpd $(INSTALLDIR)/miniupnpd/usr/sbin/miniupnpd
	$(STRIP) $(INSTALLDIR)/miniupnpd/usr/sbin/miniupnpd

# !!TB
shared: busybox $(if $(RTCONFIG_QTN),libqcsapi_client)
ifeq ($(RTCONFIG_RALINK),y)
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
	$(MAKE) -C vsftpd

vsftpd-install: vsftpd
	install -D vsftpd/vsftpd $(INSTALLDIR)/vsftpd/usr/sbin/vsftpd
	$(STRIP) -s $(INSTALLDIR)/vsftpd/usr/sbin/vsftpd

ntfs-3g/stamp-h1:
	cd ntfs-3g && \
	CC=$(CC) CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
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
	cd libusb10 && CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="-lpthread" \
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
		LIBUSB_1_0_LIBS="-L$(TOP)/libusb10/libusb/.libs -lusb-1.0 -lpthread \
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

#usbmodeswitch: libusb10
#	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) -DLIBUSB10 \
#	    -Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread \
#	    -I$(TOP)/libusb10/libusb -L$(TOP)/libusb10/libusb/.libs -lusb-1.0"

usbmodeswitch: libusb-0.1.12
	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) \
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread \
		-I$(TOP)/libusb-0.1.12 -L$(TOP)/libusb-0.1.12/.libs -lusb"

usbmodeswitch-install: usbmodeswitch usbmodeswitch-install
	install -D usbmodeswitch/usb_modeswitch $(INSTALLDIR)/usbmodeswitch/usr/sbin/usb_modeswitch
	$(STRIP) -s $(INSTALLDIR)/usbmodeswitch/usr/sbin/usb_modeswitch
	@mkdir -p $(TARGETDIR)/rom/etc
	@sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < $(TOP)/usbmodeswitch/usb_modeswitch.conf > $(TARGETDIR)/rom/etc/usb_modeswitch.conf

usbmodeswitch-1.2.3: libusb-0.1.12
	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) \
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread \
		-I$(TOP)/libusb-0.1.12 -L$(TOP)/libusb-0.1.12/.libs -lusb"

usbmodeswitch-1.2.3-install: usbmodeswitch-1.2.3
	install -D usbmodeswitch-1.2.3/usb_modeswitch $(INSTALLDIR)/usbmodeswitch-1.2.3/usr/sbin/usb_modeswitch
	$(STRIP) -s $(INSTALLDIR)/usbmodeswitch-1.2.3/usr/sbin/usb_modeswitch
	@mkdir -p $(TARGETDIR)/rom/etc
	@sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < $(TOP)/usbmodeswitch-1.2.3/usb_modeswitch.conf > $(TARGETDIR)/rom/etc/usb_modeswitch.conf

usb-modeswitch-2.1.1: libusb10
	$(MAKE) -C $@ CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) \
		-Wl,-R/lib:/usr/lib:/opt/usr/lib:/usr/local/share -lpthread \
		-I$(TOP)/libusb10/libusb -L$(TOP)/libusb10/libusb/.libs -lusb-1.0"

usb-modeswitch-2.1.1-install: usb-modeswitch-2.1.1
	install -D usb-modeswitch-2.1.1/usb_modeswitch $(INSTALLDIR)/usb-modeswitch-2.1.1/usr/sbin/usb_modeswitch
	$(STRIP) -s $(INSTALLDIR)/usb-modeswitch-2.1.1/usr/sbin/usb_modeswitch
	@mkdir -p $(TARGETDIR)/rom/etc
	@sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < $(TOP)/usb-modeswitch-2.1.1/usb_modeswitch.conf > $(TARGETDIR)/rom/etc/usb_modeswitch.conf

libusb-0.1.12/stamp-h1:
	cd libusb-0.1.12 && CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="-lpthread" \
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
	$(MAKE) -C $@

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

mtd-utils: util-linux-stage lzo-stage zlib-stage
	$(MAKE) CPPFLAGS="-I$(STAGEDIR)/usr/include" \
		CFLAGS="-I$(TOP)/mtd-utils/include  -I$(TOP)/mtd-utils/ubi-utils/include" \
		LDFLAGS="-L$(STAGEDIR)/usr/lib" \
		WITHOUT_XATTR=1 -C $@

mtd-utils-install:
	$(MAKE) WITHOUT_XATTR=1 WITHOUT_LZO=1 DESTDIR=$(INSTALLDIR)/mtd-utils -C mtd-utils install

mtd-utils-clean:
	$(MAKE) -C mtd-utils clean

util-linux: util-linux/Makefile
	$(MAKE) -C $@/libuuid/src

util-linux/Makefile: util-linux/configure
	$(MAKE) util-linux-configure

util-linux/configure:
	( cd util-linux ; ./autogen.sh )

util-linux-configure:
	( cd util-linux ; \
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

util-linux-stage: util-linux
	$(MAKE) -C util-linux/libuuid/src DESTDIR=$(STAGEDIR) \
		install-usrlib_execLTLIBRARIES install-uuidincHEADERS

util-linux-install: util-linux-stage
	install -D $(STAGEDIR)/usr/lib/libuuid.so.1 $(INSTALLDIR)/util-linux/usr/lib/libuuid.so.1
	$(STRIP) $(INSTALLDIR)/util-linux/usr/lib/*.so*

util-linux-clean:
	[ ! -f util-linux/Makefile ] || $(MAKE) -C util-linux distclean

radvd/stamp-h1:
	cd radvd && CFLAGS="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections $(EXTRALDFLAGS)" \
	$(CONFIGURE) --prefix="" --disable-dependency-tracking \
	DAEMON_CFLAGS="-I$(TOP)/libdaemon" \
	DAEMON_LIBS="-ldaemon -L$(TOP)/libdaemon/libdaemon/.libs $(EXTRALDFLAGS2) $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client -lqcsapi_client)"
	-@$(MAKE) -C radvd clean
	@touch $@

radvd: libdaemon radvd/stamp-h1
	make -C $@ $(PARALLEL_BUILD)

radvd-clean:
	-@ [ ! -f radvd/Makefile ] || $(MAKE) -C radvd distclean
	@rm -f radvd/stamp-h1

radvd-install: radvd
	install -D radvd/radvd $(INSTALLDIR)/radvd/usr/sbin/radvd
	$(STRIP) $(INSTALLDIR)/radvd/usr/sbin/radvd

ndisc6/stamp-h1:
ifeq ($(RTCONFIG_BCMARM),y)
	cd ndisc6 && CFLAGS="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections -I$(SRCBASE)/include -I$(TOP)/shared" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --prefix="" --disable-dependency-tracking
else
	cd ndisc6 && CFLAGS="-Os -ffunction-sections -fdata-sections -I$(SRCBASE)/include -I$(TOP)/shared" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -L$(TOP)/nvram$(BCMEX) -L$(TOP)/shared" \
	$(CONFIGURE) --prefix="" LIBS='-lnvram -lshared' --disable-dependency-tracking
endif
	-@$(MAKE) -C ndisc6 clean
	@touch $@

ndisc6: ndisc6/stamp-h1
	$(MAKE) -C $@

ndisc6-clean:
	-@[ ! -f ndisc6/Makefile ] || $(MAKE) -C ndisc6 clean
	-@[ ! -f ndisc6/Makefile ] || $(MAKE) -C ndisc6 distclean
	-@[ -f ndisc6/stamp-h1 ] || rm -f ndisc6/stamp-h1

ndisc6-install: ndisc6
	install -D ndisc6/rdnssd/rdnssd $(INSTALLDIR)/ndisc6/usr/sbin/rdnssd
	$(STRIP) $(INSTALLDIR)/ndisc6/usr/sbin/rdnssd
ifneq ($(RTCONFIG_BCMWL6),y)
	install -D $(shell dirname $(shell which $(CXX)))/../lib/librt.so.0 $(TARGETDIR)/lib/librt.so.0
endif

dhcpv6/stamp-h1:
	@cd dhcpv6 && \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -D_GNU_SOURCE -ffunction-sections -fdata-sections -DUSE_DHCP6SRV" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -fPIC" \
	ac_cv_func_setpgrp_void=yes \
	$(CONFIGURE) --prefix= --with-localdbdir=/var --with-sysconfdir=/etc
	-@$(MAKE) -C dhcpv6 clean
	@touch $@

dhcpv6: dhcpv6/stamp-h1
	@$(SEP)
	@$(MAKE) -C dhcpv6 dhcp6c dhcp6s

dhcpv6-install: dhcpv6
	install -D dhcpv6/dhcp6c $(INSTALLDIR)/dhcpv6/usr/sbin/dhcp6c
	$(STRIP) $(INSTALLDIR)/dhcpv6/usr/sbin/dhcp6c
	install -D dhcpv6/dhcp6s $(INSTALLDIR)/dhcpv6/usr/sbin/dhcp6s
	$(STRIP) $(INSTALLDIR)/dhcpv6/usr/sbin/dhcp6s

dhcpv6-clean:
	-@$(MAKE) -C dhcpv6 clean
	@rm -f dhcpv6/stamp-h1 

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
samba-3.5.8:
	@$(MAKE) -C samba-3.5.8 $(PARALLEL_BUILD)

#samba-3.5.8-install:
#    install -D samba-3.5.8/source3/bin/libsmbclient.so $(INSTALLDIR)/samba-3.5.8/usr/lib/libsmbclient.so
#    $(STRIP) $(INSTALLDIR)/samba-3.5.8/usr/lib/libsmbclient.so



nvram$(BCMEX): shared

prebuilt: shared

vlan:
	@$(SEP)
	@$(MAKE) -C vlan CROSS=$(CROSS_COMPILE)	# STRIPTOOL=$(STRIP)

vlan-install:
	$(MAKE) -C vlan CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install	# STRIPTOOL=$(STRIP)
	$(STRIP) $(INSTALLDIR)/vlan/usr/sbin/vconfig

accel-pptpd: kernel_header pppd
ifeq ($(RTCONFIG_BCMARM),y)
	@$(MAKE) -C accel-pptpd server KDIR=$(KDIR)
else
	@$(MAKE) -C accel-pptpd server KDIR=$(TOP)/kernel_header
endif

accel-pptpd-clean:
	-@$(MAKE) -C accel-pptpd distclean

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
	cd rp-pppoe/src && CFLAGS="-g -O2 $(EXTRACFLAGS)" \
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
	cd rp-l2tp && CFLAGS="-O2 -Wall -I$(TOP)/kernel_header/include $(EXTRACFLAGS) $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)" \
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
ifeq ($(RTCONFIG_BCMARM),y)
	cd accel-pptp && CFLAGS="-g -O2 $(EXTRACFLAGS) $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)" \
	$(CONFIGURE) --prefix=/usr KDIR=$(KDIR) PPPDIR=$(TOP)/pppd
else
	cd accel-pptp && CFLAGS="-g -O2 $(EXTRACFLAGS) $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)" \
	$(CONFIGURE) --prefix=/usr KDIR=$(TOP)/kernel_header PPPDIR=$(TOP)/pppd
endif
	touch $@

accel-pptp: pppd accel-pptp/stamp-h1
	@$(MAKE) -C accel-pptp

accel-pptp-clean:
	-@$(MAKE) -C accel-pptp clean
	@rm -f accel-pptp/stamp-h1

accel-pptp-install: accel-pptp
	install -D accel-pptp/src/.libs/pptp.so $(INSTALLDIR)/accel-pptp/usr/lib/pppd/pptp.so
	$(STRIP) $(INSTALLDIR)/accel-pptp/usr/lib/pppd/pptp.so

pppd/stamp-h1:
	cd pppd && $(CONFIGURE) --prefix=/usr --sysconfdir=/tmp
	touch $@

pppd: pppd/stamp-h1
	@$(SEP)
	@$(MAKE) -C pppd MFLAGS='$(if $(RTCONFIG_IPV6),HAVE_INET6=y,) EXTRACFLAGS="$(EXTRACFLAGS) $(if $(RTCONFIG_VPNC), -DRTCONFIG_VPNC,)"'

pppd-clean:
	-@$(MAKE) -C pppd clean
	@rm -f pppd/stamp-h1

pppd-install: pppd
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
	$(MAKE) -C $@/wpa_supplicant EXTRACFLAGS="-Os $(EXTRACFLAGS)"

wpa_supplicant-install: wpa_supplicant
	install -D wpa_supplicant/wpa_supplicant/wpa_supplicant $(INSTALLDIR)/wpa_supplicant/usr/sbin/wpa_supplicant
	install -D wpa_supplicant/wpa_supplicant/wpa_cli $(INSTALLDIR)/wpa_supplicant/usr/sbin/wpa_cli
	$(STRIP) $(INSTALLDIR)/wpa_supplicant/usr/sbin/*

wpa_supplicant-clean:
	-@$(MAKE) -C wpa_supplicant/wpa_supplicant clean

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

iproute2:
	@$(SEP)
	@$(MAKE) -C $@ KERNEL_INCLUDE=$(LINUXDIR)/include EXTRACFLAGS="$(EXTRACFLAGS) $(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6)" $(PARALLEL_BUILD)
	
iproute2-install: iproute2
	install -D iproute2/tc/tc $(INSTALLDIR)/iproute2/usr/sbin/tc
	$(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/tc
	install -D iproute2/ip/ip $(INSTALLDIR)/iproute2/usr/sbin/ip
	$(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/ip

iproute2-3.x: kernel_header iptables-1.4.x
	@$(SEP)
	@$(MAKE) -C $@ IPTABLES_DIR=$(TOP)/iptables-1.4.x KERNEL_INCLUDE=$(TOP)/kernel_header/include EXTRACFLAGS="$(EXTRACFLAGS) $(if $(RTCONFIG_IPV6),-DUSE_IPV6,-DNO_IPV6)"

iproute2-3.x-install: iproute2-3.x
	install -D iproute2-3.x/tc/tc $(INSTALLDIR)/iproute2-3.x/usr/sbin/tc
	$(STRIP) $(INSTALLDIR)/iproute2-3.x/usr/sbin/tc
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

dropbear: dropbear/config.h
	@$(SEP)
	@$(MAKE) -C dropbear PROGRAMS="dropbear dbclient dropbearkey scp" MULTI=1

dropbear-install:
	install -D dropbear/dropbearmulti $(INSTALLDIR)/dropbear/usr/bin/dropbearmulti
	$(STRIP) $(INSTALLDIR)/dropbear/usr/bin/dropbearmulti
	cd $(INSTALLDIR)/dropbear/usr/bin && \
	ln -sf dropbearmulti dropbear && \
	ln -sf dropbearmulti dropbearconvert && \
	ln -sf dropbearmulti dropbearkey && \
	ln -sf dropbearmulti dbclient && \
	ln -sf dropbearmulti ssh && \
	ln -sf dropbearmulti scp

dropbear-clean:
	-@$(MAKE) -C dropbear clean
	@rm -f dropbear/config.h

dropbear/config.h:
	cd dropbear && CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
		ac_cv_func_logout=no ac_cv_func_logwtmp=no \
		$(CONFIGURE) --disable-zlib --enable-syslog --disable-lastlog --disable-utmp \
		--disable-utmpx --disable-wtmp --disable-wtmpx --disable-pututline \
		--disable-pututxline --disable-loginfunc --disable-pam --enable-openpty
	-@$(MAKE) -C dropbear clean

# Media libraries

sqlite/stamp-h1:
	cd sqlite && \
	CC=$(CC) CFLAGS="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections -fPIC" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
		$(CONFIGURE) --prefix=/usr --enable-shared --enable-static \
		--disable-readline --disable-dynamic-extensions --enable-threadsafe \
		--disable-dependency-tracking
	touch $@

sqlite: sqlite/stamp-h1
	@$(MAKE) -C sqlite all

sqlite-clean:
	-@$(MAKE) -C sqlite clean
	@rm -f sqlite/stamp-h1

sqlite-install: sqlite
	@$(SEP)

# commented out for mt-daapd-svn-1696
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D sqlite/.libs/libsqlite3.so.0 $(INSTALLDIR)/sqlite/usr/lib/libsqlite3.so.0
	$(STRIP) $(INSTALLDIR)/sqlite/usr/lib/libsqlite3.so.0
endif

FFMPEG_FILTER_CONFIG= $(foreach c, $(2), --$(1)="$(c)")

FFMPEG_DECODERS:=aac ac3 atrac3 h264 jpegls mp3 mpeg1video mpeg2video mpeg4 mpeg4aac mpegvideo png wmav1 wmav2
FFMPEG_CONFIGURE_DECODERS:=$(call FFMPEG_FILTER_CONFIG,enable-decoder,$(FFMPEG_DECODERS))

FFMPEG_PARSERS:=aac ac3 h264 mpeg4video mpegaudio mpegvideo
FFMPEG_CONFIGURE_PARSERS:=$(call FFMPEG_FILTER_CONFIG,enable-parser,$(FFMPEG_PARSERS))

FFMPEG_PROTOCOLS:=file
FFMPEG_CONFIGURE_PROTOCOLS:=$(call FFMPEG_FILTER_CONFIG,enable-protocol,$(FFMPEG_PROTOCOLS))

FFMPEG_DISABLED_DEMUXERS:=amr apc ape ass bethsoftvid bfi c93 daud dnxhd dsicin dxa ffm gsm gxf idcin iff image2 image2pipe ingenient ipmovie lmlm4 mm mmf msnwc_tcp mtv mxf nsv nut oma pva rawvideo rl2 roq rpl segafilm shorten siff smacker sol str thp tiertexseq tta txd vmd voc wc3 wsaud wsvqa xa yuv4mpegpipe
FFMPEG_CONFIGURE_DEMUXERS:=$(call FFMPEG_FILTER_CONFIG,disable-demuxer,$(FFMPEG_DISABLED_DEMUXERS))

ffmpeg/stamp-h1: zlib
	cd ffmpeg && symver_asm_label=no symver_gnu_asm=no symver=no CC=$(CC) \
		./configure --enable-cross-compile --arch=$(ARCH) --target_os=linux \
		--cross-prefix=$(CROSS_COMPILE) --enable-shared --enable-gpl --disable-doc \
		--enable-pthreads --enable-small --disable-encoders --disable-filters \
		--disable-muxers --disable-devices --disable-ffmpeg --disable-ffplay \
		--disable-ffserver --disable-ffprobe --disable-avdevice --disable-swscale \
		--disable-hwaccels --disable-network --disable-bsfs --disable-mpegaudio-hp \
		--enable-demuxers $(FFMPEG_CONFIGURE_DEMUXERS) \
		--disable-decoders $(FFMPEG_CONFIGURE_DECODERS) \
		--disable-parsers $(FFMPEG_CONFIGURE_PARSERS) \
		--disable-protocols $(FFMPEG_CONFIGURE_PROTOCOLS) \
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

ffmpeg-install: ffmpeg
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

libogg-install: libogg
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libogg/src/.libs/libogg.so.0 $(INSTALLDIR)/libogg/usr/lib/libogg.so.0
	$(STRIP) $(INSTALLDIR)/libogg/usr/lib/libogg.so.0
endif

flac/stamp-h1: libogg
	cd flac && \
	CFLAGS="-Os $(EXTRACFLAGS) -fPIC -ffunction-sections -fdata-sections" \
	CPPFLAGS="-I$(TOP)/libogg/include" \
	LDFLAGS="-L$(TOP)/libogg/src/.libs -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --enable-shared --enable-static --prefix='' --disable-rpath \
		--disable-doxygen-docs --disable-xmms-plugin --disable-cpplibs \
		--without-libiconv-prefix --disable-altivec --disable-3dnow --disable-sse
	touch $@

flac: flac/stamp-h1 libogg
	@$(MAKE) -C flac/src/libFLAC all $(PARALLEL_BUILD)

flac-clean:
	-@$(MAKE) -C flac clean
	@rm -f flac/stamp-h1

flac-install: flac
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D flac/src/libFLAC/.libs/libFLAC.so.8 $(INSTALLDIR)/flac/usr/lib/libFLAC.so.8
	$(STRIP) $(INSTALLDIR)/flac/usr/lib/libFLAC.so.8
endif

jpeg/stamp-h1:
	cd jpeg && \
	CFLAGS="-Os $(EXTRACFLAGS) -fPIC" CC=$(CC) AR2="touch" $(CONFIGURE) --enable-shared --enable-static --prefix=''
	touch $@

jpeg: jpeg/stamp-h1
	@$(MAKE) -C jpeg LIBTOOL="" O=o A=a CC=$(CC) AR2="touch" libjpeg.a libjpeg.so $(PARALLEL_BUILD)

jpeg-clean:
	-@$(MAKE) -C jpeg clean
	@rm -f jpeg/stamp-h1

jpeg-install: jpeg
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

libexif-install: libexif
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
	@$(MAKE) -C zlib CC=$(CC) AR="ar rc" RANLIB=$(RANLIB) LD=$(LD) all $(PARALLEL_BUILD)

zlib-clean:
	-@$(MAKE) -C zlib clean
	@rm -f zlib/stamp-h1

zlib-install: zlib
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

libid3tag-install: libid3tag
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
	LDFLAGS="-L$(TOP)/libogg/src/.libs -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections" \
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

libvorbis-install: libvorbis
	@$(SEP)
ifneq ($(MEDIA_SERVER_STATIC),y)
	install -D libvorbis/lib/.libs/libvorbis.so.0 $(INSTALLDIR)/libvorbis/usr/lib/libvorbis.so.0
	$(STRIP) $(INSTALLDIR)/libvorbis/usr/lib/libvorbis.so.0
endif

minidlna: zlib sqlite ffmpeg libogg flac jpeg libexif libid3tag libvorbis
	@$(SEP)
	@$(MAKE) -C minidlna CC=$(CC) $(if $(MEDIA_SERVER_STATIC),STATIC=1,) minidlna $(PARALLEL_BUILD)

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

libgdbm-install: libgdbm
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
	@$(MAKE) -C mt-daapd CC=$(CC) $(if $(MEDIA_SERVER_STATIC),STATIC=1,) all $(PARALLEL_BUILD)

mt-daapd-svn-1696/stamp-h1: zlib libid3tag sqlite
	cd mt-daapd-svn-1696 && $(CONFIGURE) --prefix=/usr \
	CC=$(CC) \
	LDFLAGS="-L$(TOP)/sqlite -L$(TOP)/libid3tag -L$(TOP)/zlib -ldl" \
	CFLAGS="-I$(TOP)/zlib" \
	--with-id3tag=$(TOP)/libid3tag \
	--enable-sqlite3 --with-sqlite3-includes=$(TOP)/sqlite \
	--disable-rpath --disable-dependency-tracking \
	ac_cv_func_setpgrp_void=yes ac_cv_lib_id3tag_id3_file_open=yes ac_cv_lib_sqlite3_sqlite3_open=yes
	touch $@

mt-daapd-svn-1696: mt-daapd-svn-1696/stamp-h1
	@$(SEP)
	@$(MAKE) -C mt-daapd-svn-1696 all $(PARALLEL_BUILD)

mt-daapd-svn-1696-clean:
	-@$(MAKE) -C mt-daapd-svn-1696 distclean
	@rm -f mt-daapd-svn-1696/stamp-h1

mt-daapd-svn-1696-install:
	rm -rf $(INSTALLDIR)/rom/mt-daapd/admin-root
	rm -rf $(INSTALLDIR)/rom/mt-daapd/plugins
	install -d $(INSTALLDIR)/mt-daapd-svn-1696/rom/mt-daapd/plugins
	cp -rf mt-daapd-svn-1696/admin-root $(INSTALLDIR)/mt-daapd-svn-1696/rom/mt-daapd
	cd $(INSTALLDIR)/mt-daapd-svn-1696/rom/mt-daapd/admin-root && rm -rf `find -name Makefile` && rm -rf `find -name Makefile.in` && rm -rf `find -name Makefile.am`
	cp -rf mt-daapd-svn-1696/src/plugins/.libs/*.so $(INSTALLDIR)/mt-daapd-svn-1696/rom/mt-daapd/plugins
	install -D mt-daapd-svn-1696/src/.libs/mt-daapd $(INSTALLDIR)/mt-daapd-svn-1696/usr/sbin/mt-daapd
	$(STRIP) $(INSTALLDIR)/mt-daapd-svn-1696/usr/sbin/mt-daapd

mDNSResponder: 
	@$(SEP)
	@$(MAKE) -C mDNSResponder CC=$(CC) all $(PARALLEL_BUILD)


igmpproxy/stamp-h1:
	cd igmpproxy && CFLAGS="-O2 -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections" \
	$(CONFIGURE) --prefix=/usr --disable-dependency-tracking
	#touch $@ # will be generated by configure.

igmpproxy: igmpproxy/stamp-h1
	@$(SEP)
	@$(MAKE) -C igmpproxy/src

igmpproxy-install: igmpproxy
	install -D igmpproxy/src/igmpproxy $(INSTALLDIR)/igmpproxy/usr/sbin/igmpproxy
	$(STRIP) $(INSTALLDIR)/igmpproxy/usr/sbin/igmpproxy

igmpproxy-clean:
	-@$(MAKE) -C igmpproxy/src clean
	@rm -f igmpproxy/stamp-h1

wps$(BCMEX): nvram$(BCMEX) shared
ifeq ($(RTCONFIG_WPS),y)
	-rm -rf $(SRCBASE)/../src/wps
	-ln -s $(SRCBASE)/../$(SRCBASEDIR)/wps $(SRCBASE)/../src/wps
	[ ! -f wps$(BCMEX)/Makefile ] || $(MAKE) -C wps$(BCMEX) EXTRA_LDFLAGS=$(EXTRALDFLAGS)
else
	# Prevent to use generic rules"
	@true
endif

wps$(BCMEX)-install:
ifeq ($(RTCONFIG_WPS),y)
	[ ! -f wps$(BCMEX)/Makefile ] || $(MAKE) -C wps$(BCMEX) INSTALLDIR=$(INSTALLDIR) install
else
	# Prevent to use generic rules"
	@true
endif

wps$(BCMEX)-clean:
ifeq ($(RTCONFIG_WPS),y)
	[ ! -f wps$(BCMEX)/Makefile ] || $(MAKE) -C wps$(BCMEX) clean
else
	# Prevent to use generic rules"
	@true
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

emf:
	$(MAKE) -C emf$(BCMEX)/emfconf CROSS=$(CROSS_COMPILE)

emf-install:
ifeq ($(RTCONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf$(BCMEX)/emfconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

emf-clean:
	-@$(MAKE) -C emf$(BCMEX)/emfconf clean

igs:
	$(MAKE) -C emf$(BCMEX)/igsconf CROSS=$(CROSS_COMPILE)

igs-install:
ifeq ($(RTCONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf$(BCMEX)/igsconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

igs-clean:
	-@$(MAKE) -C emf$(BCMEX)/igsconf clean

ebtables: kernel_header dummy
	$(MAKE) -C ebtables CC=$(CC) LD=$(LD) \
	CFLAGS="-Os $(EXTRACFLAGS) -Wall -Wunused" $(PARALLEL_BUILD) \
	BINDIR="/usr/sbin" LIBDIR="/usr/lib/ebtables" KERNEL_INCLUDES=$(TOP)/kernel_header/include $(if $(RTCONFIG_IPV6),DO_IPV6=1,)

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
	@$(MAKE) -C lzo $(PARALLEL_BUILD)

lzo-clean:
	-@$(MAKE) -C lzo clean
	@rm -f lzo/stamp-h1

lzo-install: lzo
	install -D lzo/src/.libs/liblzo2.so $(INSTALLDIR)/lzo/usr/lib/liblzo2.so.2
	$(STRIP) $(INSTALLDIR)/lzo/usr/lib/liblzo2.so.2
	cd $(INSTALLDIR)/lzo/usr/lib && ln -sf liblzo2.so.2 liblzo2.so

openpam/stamp-h1:
	cd openpam && ./autogen.sh && \
	$(CONFIGURE) --without-doc --with-pam-unix
	touch $@

openpam: openpam/stamp-h1
	$(MAKE) -C openpam $(PARALLEL_BUILD)

openpam-install: openpam
	install -D openpam/lib/libpam/.libs/libpam.so.2.0.0 $(INSTALLDIR)/openpam/usr/lib/libpam.so.2
	$(STRIP) -s $(INSTALLDIR)/openpam/usr/lib/libpam.so.2
	install -D openpam/modules/pam_unix/.libs/pam_unix.so.2.0.0 $(INSTALLDIR)/openpam/usr/lib/pam_unix.so
	$(STRIP) -s $(INSTALLDIR)/openpam/usr/lib/pam_unix.so

openpam-clean:
	@[ ! -f openpam/Makefile ] || $(MAKE) -C openpam clean
	@rm -f openpam/stamp-h1

openvpn/stamp-h1:
	cd openvpn && ./autogen.sh && \
	export OPENSSL_CRYPTO_CFLAGS="-I$(TOP)/openssl/include"; \
	export OPENSSL_CRYPTO_LIBS="-L$(TOP)/openssl -lcrypto"; \
	export OPENSSL_SSL_CFLAGS="-I$(TOP)/openssl/include"; \
	export OPENSSL_SSL_LIBS="-L$(TOP)/openssl -lssl"; \
	CFLAGS="-O3 -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	LDFLAGS="$(EXTRALDFLAGS) -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client -lqcsapi_client) -L$(TOP)/zlib -lz -L$(TOP)/lzo/src/.libs -L$(TOP)/openpam/lib/libpam/.libs -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	CPPFLAGS="-I$(TOP)/lzo/include -I$(TOP)/openpam/include -I$(TOP)/shared -I$(SRCBASE)/include" \
	IPROUTE="/usr/sbin/ip" \
	$(CONFIGURE) --prefix=  --enable-iproute2 \
	--disable-debug --enable-management --disable-small \
	--disable-selinux --disable-socks --enable-password-save \
	--enable-plugin-auth-pam \
	ac_cv_lib_resolv_gethostbyname=no
	touch $@

openvpn: openvpn/stamp-h1 shared openssl lzo openpam zlib
	$(MAKE) -C $@

openvpn-clean:
	@[ ! -f openvpn/Makefile ] || $(MAKE) -C openvpn clean
	@rm -f openvpn/stamp-h1

openvpn-install: openvpn
	install -D openvpn/src/openvpn/.libs/openvpn $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	$(STRIP) -s $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	chmod 0500 $(INSTALLDIR)/openvpn/usr/sbin/openvpn
	install -D openvpn/src/plugins/auth-pam/.libs/openvpn-plugin-auth-pam.so $(INSTALLDIR)/openvpn/usr/lib/openvpn-plugin-auth-pam.so
	$(STRIP) -s $(INSTALLDIR)/openvpn/usr/lib/openvpn-plugin-auth-pam.so
	@mkdir -p $(TARGETDIR)/rom/easy-rsa
	install openvpn/easy-rsa/2.0/* $(TARGETDIR)/rom/easy-rsa

sdparm-1.02/stamp-h1:
	cd $(TOP)/sdparm-1.02 && $(CONFIGURE) --prefix=$(INSTALLDIR)/sdparm-1.02
	touch $@

sdparm-1.02: sdparm-1.02/stamp-h1

sdparm-1.02-install: sdparm-1.02
	@echo sdparm-1.02
	@[ ! -d sdparm-1.02 ] || $(MAKE) -C sdparm-1.02 install INSTALLDIR=$(INSTALLDIR)/sdparm-1.02
	@$(STRIP) $(INSTALLDIR)/sdparm-1.02/bin/sdparm

sdparm-1.02-clean:
	-@make -C sdparm-1.02 clean
	@rm -rf sdparm-1.02/stamp-h1 sdparm-1.02/src/.deps

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
	@[ ! -f strace/Makefile ] || $(MAKE) -C strace distclean

.PHONY: termcap
termcap: termcap/Makefile
	$(MAKE) -C $@

termcap/Makefile:
	( cd termcap && $(CONFIGURE) && touch termcap.info)

termcap-install:
	# We don't need to install termcap to target filesystem.

termcap-clean:
	$(MAKE) -C termcap distclean

.PHONY: gdb
gdb: termcap gdb/Makefile
	$(MAKE) -C $@

gdb/Makefile:
	( cd gdb && \
	  TUI_LIBRARY=$(TOP)/termcap/libtermcap.a \
	  LDFLAGS=-L$(TOP)/termcap \
	  CFLAGS_FOR_BUILD= \
	  $(CONFIGURE) --bindir=/sbin \
	)

gdb-install:
	@install -D gdb/gdb/gdb $(INSTALLDIR)/gdb/sbin/gdb
	@install -D gdb/gdb/gdbserver/gdbserver $(INSTALLDIR)/gdb/sbin/gdbserver
	@$(STRIP) $(INSTALLDIR)/gdb/sbin/gdb
	@$(STRIP) $(INSTALLDIR)/gdb/sbin/gdbserver

gdb-clean:
	@[ ! -f gdb/Makefile ] || $(MAKE) -C gdb distclean

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
	@[ ! -f openlldp/Makefile ] || $(MAKE) -C openlldp distclean


nas$(BCMEX): nvram$(BCMEX) shared

eapd$(BCMEX)-clean:
	-@cd eapd$(BCMEX)/linux && make clean

lighttpd-1.4.29/stamp-h1:
ifneq ($(RTCONFIG_QTN),y)
	cd lighttpd-1.4.29 && ./preconfigure-script touch $@
else
	cd lighttpd-1.4.29 && ./preconfigure-script-qtn touch $@
endif

lighttpd-1.4.29: shared nvram$(BCMEX) libdisk samba-3.5.8 pcre-8.31 libxml2 sqlite libexif openssl wb push_log lighttpd-1.4.29/stamp-h1
	$(MAKE) -C lighttpd-1.4.29 $(PARALLEL_BUILD)
ifeq ($(RTCONFIG_BCMARM),y)
	$(MAKE) -C lighttpd-1.4.29/src copy-prebuild-arm
else
	$(MAKE) -C lighttpd-1.4.29/src copy-prebuild
endif

lighttpd-1.4.29-install: lighttpd-1.4.29
	install -D lighttpd-1.4.29/src/.libs/lighttpd $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd
	install -D lighttpd-1.4.29/src/.libs/lighttpd-monitor $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd-monitor
	install -D lighttpd-1.4.29/src/.libs/lighttpd-arpping $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd-arpping
	$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd
	$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd-monitor
	$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.29/usr/sbin/lighttpd-arpping
	
	install -d $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/
	install -D lighttpd-1.4.29/src/.libs/*.so $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/
	install -D samba-3.5.8/source3/bin/libsmbclient.so.0 $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/libsmbclient.so.0
	install -D sqlite/.libs/libsqlite3.so.0 $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/libsqlite3.so.0
	install -D libexif/libexif/.libs/libexif.so.12 $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/libexif.so.12

	mkdir -p $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd
	mkdir -p $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd/css & mkdir -p $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd/js
	cp -rf  lighttpd-1.4.29/external_file/css/ $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd
	cp -rf  lighttpd-1.4.29/external_file/js/ $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd
	cp -f APP-IPK/AiCloud-ipk/CONTROL/control $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd
	cp -f APP-IPK/SmartSync-ipk/CONTROL/control $(INSTALLDIR)/lighttpd-1.4.29/usr/lighttpd/smartsync_control

	$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/*.so
	$(STRIP) -s $(INSTALLDIR)/lighttpd-1.4.29/usr/lib/*.so.0

lighttpd-1.4.29-clean:
	-@$(MAKE) -C lighttpd-1.4.29 clean
	@rm -f lighttpd-1.4.29/stamp-h1

pcre-8.12/stamp-h1:
	cd pcre-8.12 && \
	CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \
	./$(CONFIGURE) --prefix=/usr --disable-dependency-tracking
	touch $@
	[ -d pcre-8.12/m4 ] || mkdir pcre-8.12/m4

pcre-8.12: pcre-8.12/stamp-h1
	@$(MAKE) -C pcre-8.12 $(PARALLEL_BUILD)

pcre-8.12-install: pcre-8.12
	@$(SEP)
	install -D pcre-8.12/.libs/libpcre.so.0.0.1 $(INSTALLDIR)/pcre-8.12/usr/lib/libpcre.so.0
	$(STRIP) -s $(INSTALLDIR)/pcre-8.12/usr/lib/libpcre.so.0

pcre-8.12-clean:
	-@$(MAKE) -C pcre-8.12 clean
	@rm -f pcre-8.12/stamp-h1

pcre-8.31/stamp-h1:
	cd pcre-8.31 && \
	CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" LIBS="$(EXTRALDFLAGS)" \
	./$(CONFIGURE) --prefix=/usr --disable-dependency-tracking
	touch $@
	[ -d pcre-8.31/m4 ] || mkdir pcre-8.31/m4

pcre-8.31: pcre-8.31/stamp-h1

pcre-8.31-install: pcre-8.31
	@$(SEP)
	install -D pcre-8.31/.libs/libpcre.so.1.0.1 $(INSTALLDIR)/pcre-8.31/usr/lib/libpcre.so.1.0.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcre.so.1.0.1 libpcre.so.1
	cd $(INSTALLDIR)/pcre-8.31/usr/lib && ln -sf libpcre.so.1.0.1 libpcre.so
	$(STRIP) -s $(INSTALLDIR)/pcre-8.31/usr/lib/libpcre.so.1.0.1

pcre-8.31-clean:
	-@$(MAKE) -C pcre-8.31 clean
	@rm -f pcre-8.31/stamp-h1

libxml2/stamp-h1:
	cd libxml2 && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \
	./$(CONFIGURE) --prefix=/usr --without-python --disable-dependency-tracking
	touch $@

libxml2: libxml2/stamp-h1
	@$(MAKE) -C libxml2 all $(PARALLEL_BUILD)

libxml2-install: libxml2
	@$(SEP)
	install -D libxml2/.libs/libxml2.so.2 $(INSTALLDIR)/libxml2/usr/lib/libxml2.so.2
	$(STRIP) $(INSTALLDIR)/libxml2/usr/lib/libxml2.so.2 
	cd $(INSTALLDIR)/libxml2/usr/lib && ln -sf libxml2.so.2 libxml2.so
	
libxml2-clean:
	-@$(MAKE) -C libxml2 clean
	@rm -f libxml2/stamp-h1

#add by alan 
#add install neon 
neon/stamp-h1 neon/config.h:
	cd neon && \
	$(CONFIGURE) CC=$(CC) --enable-shared --disable-static --disable-nls --with-zlib --with-ssl=openssl --with-libs=$(TOP)/openssl:$(TOP)/libxml2 CFLAGS='-I$(TOP)/zlib -I$(TOP)/libxml2/include -I$(TOP)/openssl/include' LDFLAGS='-L$(TOP)/libxml2/.libs -L$(TOP)/openssl -L$(TOP)/zlib' LIBS='-lxml2 -lssl -lcrypto -lz' XML2_CONFIG='$(TOP)/libxml2/xml2-config'
	cp -f neon/config.h neon/src/config.h
	touch $@

neon: neon/stamp-h1 neon/config.h
	@$(MAKE) -C neon

neon-install: neon
	@$(SEP)
	install -D neon/src/.libs/libneon.so.27.2.6 $(INSTALLDIR)/neon/usr/lib/libneon.so.27.2.6
	$(STRIP) $(INSTALLDIR)/neon/usr/lib/libneon.so.27.2.6
	cd $(INSTALLDIR)/neon/usr/lib && ln -sf libneon.so.27.2.6 libneon.so && ln -sf libneon.so.27.2.6 libneon.so.27

neon-clean:
	-@$(MAKE) -C neon clean
	@rm -f neon/stamp-h1

#webdav_client
webdav_client$(BCMEX): 
	@$(MAKE) -C webdav_client$(BCMEX)

webdav_client$(BCMEX)-install: webdav_client$(BCMEX)
	@$(SEP)
	install -D webdav_client$(BCMEX)/webdav_client $(INSTALLDIR)/webdav_client$(BCMEX)/usr/sbin/webdav_client
	$(STRIP) $(INSTALLDIR)/webdav_client$(BCMEX)/usr/sbin/webdav_client

webdav_client$(BCMEX)-clean:
	-@$(MAKE) -C webdav_client$(BCMEX) clean

#add by gauss
#add install libcurl
curl-7.21.7/stamp-h1:
	cd curl-7.21.7 && \
	$(CONFIGURE) CC=$(CC) --enable-http --with-ssl=$(TOP)/openssl
	touch $@

curl-7.21.7: curl-7.21.7/stamp-h1
	@$(MAKE) -C curl-7.21.7 $(PARALLEL_BUILD)

curl-7.21.7-install: curl-7.21.7
	@$(SEP)
	install -D curl-7.21.7/lib/.libs/libcurl.so.4.2.0 $(INSTALLDIR)/curl-7.21.7/usr/lib/libcurl.so.4.2.0
	$(STRIP) $(INSTALLDIR)/curl-7.21.7/usr/lib/libcurl.so.4.2.0
	cd $(INSTALLDIR)/curl-7.21.7/usr/lib && ln -sf libcurl.so.4.2.0 libcurl.so && ln -sf libcurl.so.4.2.0 libcurl.so.4
	
curl-7.21.7-clean:
	-@$(MAKE) -C curl-7.21.7 clean
	@rm -f curl-7.21.7/stamp-h1

#asuswebstorage
asuswebstorage$(BCMEX):
	@$(MAKE) -C asuswebstorage$(BCMEX)

asuswebstorage$(BCMEX)-install: asuswebstorage$(BCMEX)
	@$(SEP)
	install -D asuswebstorage$(BCMEX)/asuswebstorage $(INSTALLDIR)/asuswebstorage$(BCMEX)/usr/sbin/asuswebstorage
	$(STRIP) $(INSTALLDIR)/asuswebstorage$(BCMEX)/usr/sbin/asuswebstorage

asuswebstorage$(BCMEX)-clean:
	-@$(MAKE) -C asuswebstorage$(BCMEX) clean

#inotify
inotify: 
	@$(MAKE) -C inotify

inotify-install: inotify
	@$(SEP)
	install -D inotify/inotify $(INSTALLDIR)/inotify/usr/sbin/inotify
	$(STRIP) $(INSTALLDIR)/inotify/usr/sbin/inotify

inotify-clean:
	-@$(MAKE) -C inotify clean

wl$(BCMEX):
	-@$(MAKE) -C wl_arm
	
wl$(BCMEX)-install:
	@$(SEP)

#libgpg-error-1.10
libgpg-error-1.10/stamp-h1:
	cd $(TOP)/libgpg-error-1.10 && \
	$(CONFIGURE) --prefix=$(INSTALLDIR)/libgpg-error-1.10/usr
	touch $@

libgpg-error-1.10: libgpg-error-1.10/stamp-h1
	@$(SEP)
	@$(MAKE) -C libgpg-error-1.10

libgpg-error-1.10-install: libgpg-error-1.10
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

libgcrypt-1.5.1-install: libgcrypt-1.5.1
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
	@$(MAKE) -C db-4.8.30/build_unix

db-4.8.30-install: db-4.8.30
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
	LIBS="-L$(TOP)/libgpg-error-1.10/src/.libs -lgpg-error -L$(TOP)/libgcrypt-1.5.1/src/.libs -lgcrypt" \
	LD_LIBRARY_PATH="$(SRCBASE)/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3/lib" \
	LIBGCRYPT_CONFIG="$(TOP)/libgcrypt-1.5.1/src/libgcrypt-config" \
	--without-kerberos \
	--with-bdb=$(TOP)/db-4.8.30/build_unix \
	--with-cnid-cdb-backend \
	--prefix=$(INSTALLDIR)/netatalk-3.0.5/usr \
	--program-prefix= \
	--with-ssl-dir=$(TOP)/openssl \
	--disable-static \
	--with-libgcrypt-dir=$(TOP)/libgcrypt-1.5.1 \
	--without-dtrace
	touch $@

netatalk-3.0.5: netatalk-3.0.5/stamp-h1
	@$(SEP)
	@$(MAKE) -C netatalk-3.0.5 $(PARALLEL_BUILD)

netatalk-3.0.5-install: netatalk-3.0.5
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
	-@$(MAKE) -C netatalk-3.0.5 clean
	@rm -f netatalk-3.0.5/stamp-h1

#expat-2.0.1
expat-2.0.1/stamp-h1:
	cd $(TOP)/expat-2.0.1 && \
	$(CONFIGURE) --build=i686-linux --prefix=$(INSTALLDIR)/expat-2.0.1/usr
	touch $@

expat-2.0.1: expat-2.0.1/stamp-h1
	@$(SEP)
	@$(MAKE) -C expat-2.0.1 $(PARALLEL_BUILD)

expat-2.0.1-install: expat-2.0.1
	install -D expat-2.0.1/.libs/libexpat.so.1.5.2 $(INSTALLDIR)/expat-2.0.1/usr/lib/libexpat.so.1.5.2
	$(STRIP) $(INSTALLDIR)/expat-2.0.1/usr/lib/libexpat.so.1.5.2
	cd $(INSTALLDIR)/expat-2.0.1/usr/lib && ln -sf libexpat.so.1.5.2 libexpat.so.1

expat-2.0.1-clean:
	-@$(MAKE) -C expat-2.0.1 clean
	@rm -f expat-2.0.1/stamp-h1

#avahi-0.6.31
avahi-0.6.31/stamp-h1: shared nvram$(BCMEX) expat-2.0.1 libdaemon
	cd $(TOP)/avahi-0.6.31 && \
	$(CONFIGURE) LDFLAGS="-L$(TOP)/expat-2.0.1/.libs" CFLAGS="-I$(TOP)/expat-2.0.1/lib -I$(TOP)/shared -I$(SRCBASE)/include" --prefix=$(INSTALLDIR)/avahi-0.6.31/usr --with-distro=archlinux --disable-glib --disable-gobject --disable-qt3 --disable-qt4 --disable-gtk --disable-dbus --disable-expat --disable-gdbm --disable-python --disable-pygtk --disable-python-dbus --disable-mono --disable-monodoc --disable-gtk3 --with-xml=none  LIBDAEMON_LIBS="-L$(TOP)/libdaemon/libdaemon/.libs -ldaemon -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared $(if $(RTCONFIG_QTN),-L$(TOP)/libqcsapi_client -lqcsapi_client)" LIBDAEMON_CFLAGS="-I$(TOP)/libdaemon" --disable-autoipd --with-xml=expat --disable-stack-protector --with-avahi-user="admin" --with-avahi-group="root"
	touch $@

avahi-0.6.31: avahi-0.6.31/stamp-h1
	@$(SEP)
	@$(MAKE) -C avahi-0.6.31 $(PARALLEL_BUILD)

avahi-0.6.31-install: avahi-0.6.31
	install -D avahi-0.6.31/avahi-daemon/.libs/avahi-daemon $(INSTALLDIR)/avahi-0.6.31/usr/sbin/avahi-daemon
	install -D avahi-0.6.31/avahi-common/.libs/libavahi-common.so.3.5.3 $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-common.so.3.5.3
	install -D avahi-0.6.31/avahi-core/.libs/libavahi-core.so.7.0.2 $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-core.so.7.0.2
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/sbin/avahi-daemon
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-common.so.3.5.3
	$(STRIP) $(INSTALLDIR)/avahi-0.6.31/usr/lib/libavahi-core.so.7.0.2
	cd $(INSTALLDIR)/avahi-0.6.31/usr/lib && ln -sf libavahi-common.so.3.5.3 libavahi-common.so.3
	cd $(INSTALLDIR)/avahi-0.6.31/usr/lib && ln -sf libavahi-core.so.7.0.2 libavahi-core.so.7

avahi-0.6.31-clean:
	-@$(MAKE) -C avahi-0.6.31 clean
	@rm -f avahi-0.6.31/stamp-h1

madwimax-0.1.1/stamp-h1:
	cd $(TOP)/madwimax-0.1.1 && \
	$(CONFIGURE) --prefix=$(TOP)/madwimax-0.1.1/build --with-script=wl500g --without-man-pages --with-udev-dir=$(TOP)/madwimax-0.1.1/build libusb1_CFLAGS=-I$(TOP)/libusb10/libusb libusb1_LIBS="-L$(TOP)/libusb10/libusb/.libs -lusb-1.0"
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

wol/stamp-h1:
	cd $(TOP)/wol && jm_cv_func_working_malloc=yes \
	CFLAGS="-Os -Wall $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
	$(CONFIGURE) --host=mips --prefix=$(INSTALLDIR)/wol CC=mipsel-uclibc-gcc STRIP=mipsel-uclibc-strip \
	AR=mipsel-uclibc-ar LD=mipsel-uclibc-ld NM=mipsel-uclibc-nm RANLIB=mipsel-uclibc-ranlib
	@touch $@

wol: wol/stamp-h1
	@$(SEP)
	jm_cv_func_working_malloc=yes $(MAKE) -C $@

wol-clean:
	-@$(MAKE) -C wol distclean
	@rm -f wol/stamp-h1

wol-install: wol
	install -D wol/src/wol $(INSTALLDIR)/wol/usr/sbin/wol
	$(STRIP) $(INSTALLDIR)/wol/usr/sbin/wol

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

tftp:
	cd tftp-1.0 && CC=$(CC) && $(MAKE)

tftp-clean:
	cd tftp-1.0 && $(MAKE) clean

tftp-install:
	# TPTP server
	install -D tftp-1.0/tftpd $(INSTALLDIR)/tftp/usr/sbin/tftpd
	$(STRIP) -s $(INSTALLDIR)/tftp/usr/sbin/tftpd
	# TFTP client
	#install -D tftp-1.0/tftpc $(INSTALLDIR)/tftp/usr/sbin/tftpc
	#$(STRIP) -s $(INSTALLDIR)/tftp/usr/sbin/tftpc

bwdpi:
	cd bwdpi && CC=$(CC) && $(MAKE)

bwdpi-clean:
	cd bwdpi && $(MAKE) clean

bwdpi-install: bwdpi
	install -D bwdpi/libbwdpi.so $(INSTALLDIR)/bwdpi/usr/lib/libbwdpi.so
	$(STRIP) $(INSTALLDIR)/bwdpi/usr/lib/libbwdpi.so

NORTON_DIR := $(SRCBASE)/router/norton${BCMEX}

.PHONY: norton${BCMEX} norton${BCMEX}-install norton${BCMEX}-clean


norton${BCMEX}:
	+$(MAKE) -C $(NORTON_DIR)

norton${BCMEX}-install:
	+$(MAKE) -C $(NORTON_DIR) install INSTALLDIR=$(INSTALLDIR)/norton${BCMEX}

norton${BCMEX}-clean:
	[ ! -f $(NORTEON_DIR)/Makefile ] || $(MAKE) -C $(NORTON_DIR) clean

.PHONY : email-3.1.3 email-3.1.3-clean
email-3.1.3:
	echo "Build emailaaa"
	cd email-3.1.3 && \
	./configure --with-ssl --sysconfdir=/etc --host=mipsel-uclibc CFLAGS=-I$(SRCBASE)/router/openssl/include LDFLAGS="-L$(SRCBASE)/router/openssl"  LIBS="-lcrypto -lssl"
	echo "Build emailbbb"
	$(MAKE) -C email-3.1.3

email-3.1.3-clean:
	-@$(MAKE) -C email-3.1.3 clean

email-3.1.3-install: email-3.1.3
	install -D email-3.1.3/src/email $(INSTALLDIR)/email-3.1.3/usr/sbin/email
	$(STRIP) $(INSTALLDIR)/email-3.1.3/usr/sbin/email

#diskdev_cmds-332.14
diskdev_cmds-332.14:
	@$(SEP)
	cd $(TOP)/diskdev_cmds-332.14 && \
	make -f Makefile.lnx

diskdev_cmds-332.14-install: diskdev_cmds-332.14
	install -D diskdev_cmds-332.14/newfs_hfs.tproj/newfs_hfs $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfsplus
	install -D diskdev_cmds-332.14/fsck_hfs.tproj/fsck_hfs $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfsplus
	$(STRIP) $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfsplus
	$(STRIP) $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfsplus
	cd $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin && \
	ln -s mkfs.hfsplus mkfs.hfs && \
	ln -s fsck.hfsplus fsck.hfs

diskdev_cmds-332.14-clean:
	cd $(TOP)/diskdev_cmds-332.14 && \
	make -f Makefile.lnx clean
	rm -f $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/mkfs.hfs 
	rm -f $(INSTALLDIR)/diskdev_cmds-332.14/usr/sbin/fsck.hfs 
	

#
# Generic rules
#

%:
	@[ ! -d $* ] || ( $(SEP); $(MAKE) -C $* )


%-clean:
	-@[ ! -d $* ] || $(MAKE) -C $* clean

%-install: %
	@echo $*
	@[ ! -d $* ] || $(MAKE) -C $* install INSTALLDIR=$(INSTALLDIR)/$*

%-stage: %
	@echo $*
	@[ ! -d $* ] || $(MAKE) -C $* install DESTDIR=$(STAGEDIR)

%-build:
	$(MAKE) $*-clean $*

$(obj-y) $(obj-n) $(obj-clean) $(obj-install): dummy

.PHONY: all clean distclean mrproper install package image
.PHONY: conf mconf oldconf kconf kmconf config menuconfig oldconfig
.PHONY: dummy libnet libpcap


