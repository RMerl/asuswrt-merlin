#
# Toplevel Makefile for the BCM947xx Linux Router release
#
# Copyright 2005, Broadcom Corporation
# All Rights Reserved.
#
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
# $Id: Makefile,v 1.53 2005/04/25 03:54:37 tallest Exp $
#

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

# To rebuild everything and all configurations:
#  make distclean
#  make libc (usually doesn't need to be done ???)
#  make V1=whatever V2=sub-whatever VPN=vpn3.6 a b c d m
# The 1st "whatever" would be the build number, the sub-whatever would
#	be the update to the version.
#
# Example:
# make V1=8516 V2="-jffs.1" a b c d m s

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#
# To rebuild a part of code and generate a new firmware file:
#

# Example: rebuild rc (clean rc, compile rc, install rc to INSTALL directory, generate TARGET tree, generate Firmware file)
#  make rc-clean mk-rc rc-install gen_target image
#
# Note: the "rc-clean mk-rc" may skip if the dependency in Makefile is okay.
#

# Example: rebuild kernel and modules
#  make kernel gen_target image
#

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!



export SRCBASE := $(shell pwd)
export SRCBASEDIR := $(shell pwd | sed 's/.*release\///g')
RELEASEDIR := $(shell (cd $(SRCBASE)/.. && pwd -P))
PATH := $(RELEASEDIR)/tools:$(SRCBASE)/ctools:$(PATH)

ifeq ($(ASUSWRTTARGETMAKDIR),)
include ./target.mak
else
include $(ASUSWRTTARGETMAKDIR)/target.mak
endif

ifeq ($(ASUSWRTVERSIONCONFDIR),)
include ./version.conf
else
include $(ASUSWRTVERSIONCONFDIR)/version.conf
endif

export BRANCH := $(shell git branch)

#ifneq ($(BRANCH),) 
#include ./router/extendno.conf
#else
#export EXTENDNO := $(shell git log --pretty=oneline asuswrt_$(KERNEL_VER).$(FS_VER).$(SERIALNO)..HEAD | wc -l)-g$(shell git log --pretty=format:'%h' -n 1)
#endif

#ifeq ($(EXTENDNO),)
#export EXTENDNO := 0-g$(shell git log --pretty=format:'%h' -n 1)
#endif

export SWPJNAME := $(shell git branch | grep "*" | grep "swpj-" | sed 's/.*swpj-//g')

ifneq ($(SWPJNAME),)
export SWPJVERNO := $(shell cat $(SRCBASE)/router/APP-IPK/$(SWPJNAME)/CONTROL/control | grep "Version:" | sed 's/Version: //g')
export SWPJEXTENDNO := $(shell git describe --match swpj_$(SWPJNAME)_$(SWPJVERNO) | sed 's/swpj_$(SWPJNAME)_$(SWPJVERNO)-//g')
ifneq ($(SWPJVERNO),)
export SWPJVER := $(SWPJNAME)_$(SWPJVERNO)_
export SWPJEXTENDNO := $(shell git describe --match swpj_$(SWPJNAME)_$(SWPJVER) | sed 's/swpj_$(SWPJNAME)_$(SWPJVER)-//g')
endif

export SWPJEXTENDNO := $(shell git describe --match swpj_$(SWPJNAME)_$(SWPJVERNO) | sed 's/swpj_$(SWPJNAME)_$(SWPJVERNO)-//g')

ifneq ($(SWPJEXTENDNO),)
export EXTENDNO := $(SWPJEXTENDNO)
else
export EXTENDNO := 0-g$(shell git log --pretty=format:'%h' -n 1)
endif
endif

include ./platform.mak
-include .config
-include ./dsl.mak

ifeq ($(or $(PLATFORM),$(KERNEL_BINARY),$(LINUXDIR)),)
$(error Define Platform-specific definitions in platform.mak)
endif
export PLATFORMDIR := $(SRCBASE)/router/$(PLATFORM)

EXTRA_KERNEL_YES_CONFIGS_1 := $(filter %=y %=Y,$(EXTRA_KERNEL_CONFIGS))
EXTRA_KERNEL_NO_CONFIGS_1 := $(filter %=n %=N,$(EXTRA_KERNEL_CONFIGS))
EXTRA_KERNEL_MOD_CONFIGS_1 := $(filter %=m %=M,$(EXTRA_KERNEL_CONFIGS))
EXTRA_KERNEL_VAL_CONFIGS := $(filter-out $(EXTRA_KERNEL_YES_CONFIGS_1) $(EXTRA_KERNEL_NO_CONFIGS_1) $(EXTRA_KERNEL_MOD_CONFIGS_1),$(EXTRA_KERNEL_CONFIGS))

EXTRA_KERNEL_YES_CONFIGS := $(subst =y,,$(subst =Y,,$(EXTRA_KERNEL_YES_CONFIGS_1)))
EXTRA_KERNEL_NO_CONFIGS := $(subst =n,,$(subst =N,,$(EXTRA_KERNEL_NO_CONFIGS_1)))
EXTRA_KERNEL_MOD_CONFIGS := $(subst =m,,$(subst =M,,$(EXTRA_KERNEL_MOD_CONFIGS_1)))

ifeq ($(NVRAM_SIZE),)
NVRAM_SIZE = 0
endif

CTAGS_EXCLUDE_OPT := --exclude=kernel_header --exclude=$(PLATFORM)
CTAGS_DEFAULT_DIRS := $(SRCBASE)/../src/router/rc $(SRCBASE)/../src/router/httpd $(SRCBASE)/../src/router/shared $(SRCBASE)/../src/router/www

uppercase_N = $(shell echo $(N) | tr a-z  A-Z)
lowercase_N = $(shell echo $(N) | tr A-Z a-z)
uppercase_B = $(shell echo $(BUILD_NAME) | tr a-z  A-Z)
lowercase_B = $(shell echo $(BUILD_NAME) | tr A-Z a-z)
BUILD_TIME := $(shell LC_ALL=C date -u)
BUILD_USER ?= $(shell whoami)
BUILD_INFO := $(shell git log --pretty="%h" -n 1)

ifeq ($(CONFIG_LINUX26),y)
mips_rev = $(if $(filter $(MIPS32),r2),MIPSR2,MIPSR1)
KERN_SIZE_OPT ?= n
else
mips_rev =
KERN_SIZE_OPT ?= y
endif

export IMGNAME := $(BUILD_NAME)_$(KERNEL_VER).$(FS_VER)_$(SERIALNO)_$(SWPJVER)$(EXTENDNO)
export PBDIR   := ../../../asuswrt.prebuilt/$(BUILD_NAME).$(KERNEL_VER).$(FS_VER).$(SERIALNO).$(EXTENDNO)


default:
	@if [ -f .config -a "$(BUILD_NAME)" != "" ] ; then \
		$(MAKE) bin ; \
	else \
		echo "Source tree is not configured.  Run make with model name." ; \
	fi

rt_ver:
	@echo '#ifndef RTVERSION' > router/shared/version.h
	@echo '#define RT_MAJOR_VERSION "$(KERNEL_VER)"' >> router/shared/version.h
	@echo '#define RT_MINOR_VERSION "$(FS_VER)"' >> router/shared/version.h
	@echo '#define RT_VERSION "$(KERNEL_VER).$(FS_VER)"' >> router/shared/version.h
	@echo '#define RT_SERIALNO "$(SERIALNO)"' >> router/shared/version.h
	@echo '#define RT_EXTENDNO "$(EXTENDNO)"' >> router/shared/version.h
	@echo '#define RT_SWPJVERNO "$(SWPJVERNO)"' >> router/shared/version.h
	@echo '#define RT_BUILD_NAME "$(BUILD_NAME)"' >> router/shared/version.h
	@echo '#define RT_BUILD_INFO "$(BUILD_TIME) $(BUILD_USER)@$(BUILD_INFO)"' >> router/shared/version.h
	@echo '#endif' >> router/shared/version.h
	@echo '$(BUILD_NAME)_$(KERNEL_VER).$(FS_VER) $(BUILD_TIME)' > router/shared/version
	@echo 'EXTENDNO=$(EXTENDNO)' > router/extendno.conf

rt_ver_ntools:
	-@rm -f ntools/version
	-@echo 'KERNEL_IMAGE = $(BUILD_NAME)_$(KERNEL_VER).$(FS_VER)_$(SERIALNO)_$(EXTENDNO).trx' >> ntools/version

all: rt_ver rt_ver_ntools
	@echo ""
	@echo "Building $(BUILD_NAME)_$(KERNEL_VER).$(FS_VER)_$(SERIALNO).trx"
	@echo ""
	@echo ""

	@-mkdir image
	@$(MAKE) -C router all
	@$(MAKE) -C router install
	@$(MAKE) image

image:
	@if [ -z "$(BUILD_NAME)" ]; then \
		echo "No BUILD_NAME is assigned"; \
		exit 1; \
	fi

	@rm -f image/$(BUILD_NAME)_$(KERNEL_VER).$(FS_VER)_$(SERIALNO).trx
ifeq ($(ARM),)
	@$(MAKE) -C router image
endif

ifeq ($(CONFIG_RALINK),y)
# MTK/Ralink platform
	# generate kernel part
	@rm -rf $(PLATFORMDIR)/zImage.lzma ; \
	mipsel-linux-objcopy -O binary $(LINUXDIR)/vmlinux $(PLATFORMDIR)/vmlinus ; \
	asustools/lzma -9 -f -c $(PLATFORMDIR)/vmlinus > $(PLATFORMDIR)/zImage.lzma ; \
	cp -f $(PLATFORMDIR)/zImage.lzma $(PLATFORMDIR)/zImage.img ; \
	# padded kernel image size
	@SIZE=`wc -c $(PLATFORMDIR)/zImage.img | awk '{print $$1}'`; \
	if [ "`grep -c \"^CONFIG_ROOTFS_IN_FLASH_NO_PADDING\>\" $(LINUXDIR)/.config`" -eq 0 ] ; then \
		CONFIG_MTD_KERNEL_PART_SIZ=`grep "^CONFIG_MTD_KERNEL_PART_SIZ\>" $(LINUXDIR)/.config|sed -e "s,[^=]*=,," -e "s,^\(0x\)*,0x,"` ; \
		MTD_KRN_PART_SIZE=`printf "%d" $${CONFIG_MTD_KERNEL_PART_SIZ}` ; \
		PAD=`expr $${MTD_KRN_PART_SIZE} - 64 - $${SIZE}` ; \
		echo "S: $$SIZE $${MTD_KRN_PART_SIZE} $${PAD}" ; \
		if [ "$${PAD}" -le "0" ] ; then \
			echo "CONFIG_MTD_KERNEL_PART_SIZ $${CONFIG_MTD_KERNEL_PART_SIZ} is smaller than " \
				"`wc -c $(PLATFORMDIR)/zImage.img|awk '{printf "0x%x",$$1}'`. Increase it!" ; \
			ls -l $(PLATFORMDIR)/zImage.img ; \
			exit 1 ; \
		fi ; \
		dd if=/dev/zero count=1 bs=$${PAD} 2> /dev/null | tr \\000 \\377 >> $(PLATFORMDIR)/zImage.img ; \
	else  \
		PAD=`expr 64 - $${SIZE} % 64` ; \
		dd if=/dev/zero count=1 bs=$${PAD} 2> /dev/null | tr \\000 \\377 >> $(PLATFORMDIR)/zImage.img ; \
	fi ; \

	cat $(PLATFORMDIR)/target.image >> $(PLATFORMDIR)/zImage.img ; \
	#generate ASUS Image
	@ENTRY=`LANG=en_US readelf -h $(ROOTDIR)/$(LINUXDIR)/vmlinux | grep "Entry" | awk '{print $$4}'` ; \
	ISIZE=`wc -c $(PLATFORMDIR)/zImage.img | awk '{print $$1}'` ; \
	KSIZE=`wc -c $(PLATFORMDIR)/zImage.lzma | awk '{print $$1}'` ; \
	RSIZE=`wc -c $(PLATFORMDIR)/target.image | awk '{print $$1}'` ; \
	PAD2=`expr $${ISIZE} - $${KSIZE} - $${RSIZE}` ; \
	RFSOFFSET=`expr 64 + $${KSIZE} + $${PAD2}` ; \
	echo "PAD2: $${PAD2}" ; \
	if [ "$(BUILD_NAME)" = "RT-N56UB1" ]; then \
		asustools/mkimage -A mips -O linux -T kernel -C lzma -a 80001000 -e $${ENTRY} -r $${RFSOFFSET} \
		-n $(BUILD_NAME) -V "$(KERNEL_VER)" "$(FS_VER)" "0" "0" "0" "0" "0" "0" "0" "0" \
		-d $(PLATFORMDIR)/zImage.img image/$(IMGNAME).trx ; \
	else \
		asustools/mkimage -A mips -O linux -T kernel -C lzma -a 80000000 -e $${ENTRY} -r $${RFSOFFSET} \
		-n $(BUILD_NAME) -V "$(KERNEL_VER)" "$(FS_VER)" "0" "0" "0" "0" "0" "0" "0" "0" \
		-d $(PLATFORMDIR)/zImage.img image/$(IMGNAME).trx ; \
	fi ; \
	if [ "`grep -c \"^CONFIG_RALINK_MT7620\>\" $(LINUXDIR)/.config`" -gt 0 ]; then \
		echo -n "PA/LNA: " ; \
		if [ "`grep -c \"^CONFIG_INTERNAL_PA_INTERNAL_LNA\>\" $(LINUXDIR)/.config`" -gt 0 ] ; then \
			echo "Internal PA + Internal LNA" ; \
			if [ "$(BUILD_NAME)" = "RT-N14U" ] || [ "$(BUILD_NAME)" = "RT-AC51U" ] || [ "$(BUILD_NAME)" = "RT-N11P" ]  || [ "$(BUILD_NAME)" = "RT-N54U" ] || [ "$(BUILD_NAME)" = "RT-AC54U" ] ; then \
				echo "Check PA/LNA: OK" ; \
			else \
				mv -f image/$(IMGNAME).trx image/$(IMGNAME)_int_PA_int_LNA.trx ; \
			fi ; \
		elif [ "`grep -c \"^CONFIG_INTERNAL_PA_EXTERNAL_LNA\>\" $(LINUXDIR)/.config`" -gt 0 ] ; then \
			echo "Internal PA + External LNA" ; \
			if [ "$(BUILD_NAME)" = "RT-AC1200HP" ]; then \
				echo "Check PA/LNA: OK" ; \
			else \
				mv -f image/$(IMGNAME).trx image/$(IMGNAME)_int_PA_ext_LNA.trx ; \
			fi ; \
		elif [ "`grep -c \"^CONFIG_EXTERNAL_PA_EXTERNAL_LNA\>\" $(LINUXDIR)/.config`" -gt 0 ] ; then \
			echo "External PA + External LNA" ; \
			if [ "$(BUILD_NAME)" = "RT-AC52U" ]; then \
				echo "Check PA/LNA: OK" ; \
			else \
				mv -f image/$(IMGNAME).trx image/$(IMGNAME)_ext_PA_ext_LNA.trx ; \
			fi ; \
		else \
			echo "UNKNOWN PA/LNA" ; \
		fi ; \
	fi
else ifeq ($(QCA),y)
# Qualcomm Atheros platform
	# generate kernel part
	@rm -rf $(PLATFORMDIR)/zImage.lzma ; \
		mips-openwrt-linux-objcopy -O binary $(LINUXDIR)/vmlinux $(PLATFORMDIR)/vmlinus ; \
		asustools/lzma -9 -f -c $(PLATFORMDIR)/vmlinus > $(PLATFORMDIR)/zImage.lzma ; \
		cp -f $(PLATFORMDIR)/zImage.lzma $(PLATFORMDIR)/zImage.img ; \
	# padded kernel image size
	@SIZE=`wc -c $(PLATFORMDIR)/zImage.img | awk '{print $$1}'`; \
		PAD=`expr 64 - $${SIZE} % 64` ; \
		dd if=/dev/zero count=1 bs=$${PAD} 2> /dev/null | tr \\000 \\377 >> $(PLATFORMDIR)/zImage.img
	@cat $(PLATFORMDIR)/target.image >> $(PLATFORMDIR)/zImage.img ; \
	#generate ASUS Image
	@ISIZE=`wc -c $(PLATFORMDIR)/zImage.img | awk '{print $$1}'` ; \
		KSIZE=`wc -c $(PLATFORMDIR)/zImage.lzma | awk '{print $$1}'` ; \
		RSIZE=`wc -c $(PLATFORMDIR)/target.image | awk '{print $$1}'` ; \
		PAD2=`expr $${ISIZE} - $${KSIZE} - $${RSIZE}` ; \
		RFSOFFSET=`expr 64 + $${KSIZE} + $${PAD2}` ; \
		echo "PAD2: $${PAD2}" ; \
		asustools/mkimage -A mips -O linux -T kernel -C lzma -a 80060000 -e 80060000 -r $${RFSOFFSET} \
			-n $(BUILD_NAME) -V "$(KERNEL_VER)" "$(FS_VER)" "0" "0" "0" "0" "0" "0" "0" "0" \
			-d $(PLATFORMDIR)/zImage.img image/$(IMGNAME).trx
else
# Broadcom ARM/MIPS platform
	$(MAKE) -C ctools clean
	$(MAKE) -C ctools
	# Create generic TRX image
ifeq ($(ARM),y)
	ctools/objcopy -O binary -R .note -R .note.gnu.build-id -R .comment -S $(LINUXDIR)/vmlinux ctools/piggy
else
	ctools/objcopy -O binary -R .reginfo -R .note -R .comment -R .mdebug -S $(LINUXDIR)/vmlinux ctools/piggy
endif
	ctools/lzma_4k e ctools/piggy  ctools/vmlinuz-lzma
	echo $(PLATFORMDIR)
	echo $(PLATFORM)
	ctools/mksquashfs $(PLATFORMDIR)/target ctools/target.squashfs -noappend -all-root
ifneq (,$(filter y,$(BCMWL6) $(BCMWL6A) $(BOOTLZMA)))
	ctools/trx -o image/linux-lzma.trx ctools/vmlinuz-lzma ctools/target.squashfs
else
	ctools/trx -o image/linux-lzma.trx lzma-loader/loader.gz ctools/vmlinuz-lzma ctools/target.squashfs
endif
	ctools/trx_asus -i image/linux-lzma.trx -r $(BUILD_NAME),$(KERNEL_VER).$(FS_VER),image/$(IMGNAME).trx
	@rm -f image/linux-lzma.trx
endif
	
#	md5sum image/$(IMGNAME).trx > image/$(IMGNAME).md5

ifeq ($(DSL_TCLINUX),y)
	cat image/$(IMGNAME).trx tc_fw/tclinux.bin > image/$(IMGNAME)_DSL.trx
	md5sum image/$(IMGNAME)_DSL.trx > image/$(IMGNAME)_DSL.md5
ifeq ($(HTTPS),y)
ifneq ("$(wildcard $(SRCBASE)/../../../buildtools/private.pem)","")
	openssl sha1 -sign $(SRCBASE)/../../buildtools/private.pem -out image/$(IMGNAME)_DSL_rsa.zip image/$(IMGNAME)_DSL.trx
endif
endif
endif

#general rsasign file
ifeq ($(HTTPS),y)
ifeq ($(BCM7),y)
ifneq ("$(wildcard $(SRCBASE)/../../../buildtools/private.pem)","")
	openssl sha1 -sign $(SRCBASE)/../../../buildtools/private.pem -out image/$(IMGNAME)_rsa.zip image/$(IMGNAME).trx
endif
else
ifneq ("$(wildcard $(SRCBASE)/../../../buildtools/private.pem)","")
	openssl sha1 -sign $(SRCBASE)/../../buildtools/private.pem -out image/$(IMGNAME)_rsa.zip image/$(IMGNAME).trx
endif
endif
endif
#	ln -sf $(IMGNAME).trx image/$(BUILD_NAME).trx
	@echo ""

bin_file:
	[ ! -e .bin/$(BUILD_NAME)/Makefile ] || $(MAKE) -C .bin/$(BUILD_NAME) O=$(SRCBASE)/image FW_FN=$(IMGNAME).trx

clean:
	@touch router/.config
	@rm -f router/config_[a-z]
	@rm -f router/busybox/config_[a-z]
	@$(MAKE) -C router $@
	@-rm -rf $(PLATFORMDIR)
	@$(MAKE) -C btools clean

cleanimage:
	@rm -f fpkg.log
	@rm -fr image/*
	@rm -f router/.config
	@touch router/.config 
	@-mkdir image

cleantools:
	@$(MAKE) -C $(LINUXDIR)/scripts/squashfs clean
	@$(MAKE) -C btools clean
ifeq ($(CONFIG_RALINK),y)
else ifeq ($(CONFIG_QCA),y)
else
	@$(MAKE) -C ctools clean
endif


cleankernel:
	@cd $(LINUXDIR) && \
	mv .config save-config && \
	$(MAKE) distclean || true; \
	cp -p save-config .config || true

kernel:
	@$(MAKE) -C router kernel $(PARALLEL_BUILD)
	@[ ! -e $(KERNEL_BINARY) ] || ls -l $(KERNEL_BINARY)

distclean: clean cleanimage cleankernel cleantools cleanlibc
ifneq ($(INSIDE_MAK),1)
	@$(MAKE) -C router $@ INSIDE_MAK=1
endif
	mv router/busybox/.config busybox-saved-config || true
	@$(MAKE) -C router/busybox distclean
	@rm -f router/busybox/config_current
	@cp -p busybox-saved-config router/busybox/.config || true
	@cp -p router/busybox/.config  router/busybox/config_current || true
	@rm -f router/config_current
	@rm -f router/.config.cmd router/.config.old router/.config
	@rm -f router/libfoo_xref.txt
	@-rm -f .config

prepk:
	@cd $(LINUXDIR) ; \
		rm -f config_current ; \
		ln -s config_base config_current ; \
		cp -f config_current .config
ifeq ($(CONFIG_LINUX26),y)
	$(MAKE) -C $(LINUXDIR) oldconfig prepare
else
	$(MAKE) -C $(LINUXDIR) oldconfig dep
endif

what:
	@echo ""
	@echo "$(current_BUILD_DESC)-$(current_BUILD_NAME)-$(TOMATO_PROFILE_NAME) Profile"
	@echo ""

# The methodology for making the different builds is to
# copy the "base" config file to the "target" config file in
# the appropriate directory, and then edit it by removing and
# inserting the desired configuration lines.
# You can't just delete the "whatever=y" line, you must have
# a "...is not set" line, or the make oldconfig will stop and ask
# what to do.

# Options for "make bin" :
# BUILD_DESC (Std|Lite|Ext|...)
# MIPS32 (r2|r1)
# KERN_SIZE_OPT
# USB ("USB"|"")
# JFFSv1 | NO_JFFS
# NO_CIFS, NO_SSH, NO_ZEBRA, NO_SAMBA, NO_LIBOPT
# SAMBA3, OPENVPN, IPV6SUPP, EBTABLES, NTFS, MEDIASRV, BBEXTRAS, USBEXTRAS, BCM57, SLIM, XHCI, PSISTLOG
# STRACE, GDB

define RouterOptions
	@( \
	if [ "$(CONFIG_LINUX26)" = "y" ] || [ "$(SAMBA3)" = "y" ]; then \
		sed -i "/RTCONFIG_SAMBA3/d" $(1); \
		echo "RTCONFIG_SAMBA3=y" >>$(1); \
	fi; \
	if [ "$(SAMBA36)" = "y" ]; then \
		sed -i "/RTCONFIG_SAMBA_MODERN/d" $(1); \
		echo "RTCONFIG_SAMBA_MODERN=y" >>$(1); \
	fi; \
	sed -i "/RTCONFIG_EMF/d" $(1); \
	if [ "$(CONFIG_LINUX26)" = "y" ]; then \
		if [ "$(SLIM)" = "y" ]; then \
			echo "# RTCONFIG_EMF is not set" >>$(1); \
		else \
			echo "RTCONFIG_EMF=y" >>$(1); \
		fi; \
	else \
		echo "# RTCONFIG_EMF is not set" >>$(1); \
	fi; \
	sed -i "/RTCONFIG_JFFSV1/d" $(1); \
	if [ "$(CONFIG_LINUX26)" = "y" ]; then \
		if [ "$(JFFSv1)" = "y" ]; then \
			echo "RTCONFIG_JFFSV1=y" >>$(1); \
		else \
			echo "# RTCONFIG_JFFSV1 is not set" >>$(1); \
		fi; \
	else \
		echo "RTCONFIG_JFFSV1=y" >>$(1); \
	fi; \
	if [ "$(UBIFS)" = "y" ]; then \
		sed -i "/RTCONFIG_UBIFS/d" $(1); \
		echo "RTCONFIG_UBIFS" >>$(1); \
		sed -i "/RTCONFIG_JFFS2/d" $(1); \
		echo "# RTCONFIG_JFFS2 is not set" >>$(1); \
		sed -i "/RTCONFIG_JFFSV1/d" $(1); \
		echo "# RTCONFIG_JFFSV1 is not set" >>$(1); \
	fi; \
	if [ "$(FANCTRL)" = "y" ]; then \
		sed -i "/RTCONFIG_FANCTRL/d" $(1); \
		echo "RTCONFIG_FANCTRL=y" >>$(1); \
	fi; \
	if [ "$(BCM5301X)" = "y" ]; then \
		sed -i "/RTCONFIG_5301X/d" $(1); \
		echo "RTCONFIG_5301X=y" >>$(1); \
	fi; \
	if [ "$(BCMWL6)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMWL6/d" $(1); \
		echo "RTCONFIG_BCMWL6=y" >>$(1); \
	fi; \
	if [ "$(BCMWL6A)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMWL6A/d" $(1); \
		echo "RTCONFIG_BCMWL6A=y" >>$(1); \
	fi; \
	if [ "$(ARM)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMARM/d" $(1); \
		echo "RTCONFIG_BCMARM=y" >>$(1); \
	fi; \
	if [ "$(BCMSMP)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMSMP/d" $(1); \
		echo "RTCONFIG_BCMSMP=y" >>$(1); \
	fi; \
	if [ "$(BCMFA)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMFA/d" $(1); \
		echo "RTCONFIG_BCMFA=y" >>$(1); \
	fi; \
	if [ "$(RGMII_BCM_FA)" = "y" ]; then \
		sed -i "/RTCONFIG_RGMII_BCM_FA/d" $(1); \
		echo "RTCONFIG_RGMII_BCM_FA=y" >>$(1); \
	fi; \
	if [ "$(COMA)" = "y" ]; then \
		sed -i "/RTCONFIG_COMA/d" $(1); \
		echo "RTCONFIG_COMA=y" >>$(1); \
	fi; \
	if [ "$(WIRELESSWAN)" = "y" ]; then \
		sed -i "/RTCONFIG_WIRELESSWAN/d" $(1); \
		echo "RTCONFIG_WIRELESSWAN=y" >>$(1); \
	fi; \
	if [ "$(PARENTAL)" = "y" ]; then \
		sed -i "/RTCONFIG_OLD_PARENTALCTRL/d" $(1); \
		echo "RTCONFIG_OLD_PARENTALCTRL=y" >>$(1); \
	fi; \
	if [ "$(PARENTAL2)" = "y" ]; then \
		sed -i "/RTCONFIG_PARENTALCTRL/d" $(1); \
		echo "RTCONFIG_PARENTALCTRL=y" >>$(1); \
	fi; \
	if [ "$(YANDEXDNS)" = "y" ]; then \
		sed -i "/RTCONFIG_YANDEXDNS/d" $(1); \
		echo "RTCONFIG_YANDEXDNS=y" >>$(1); \
	fi; \
	if [ "$(PPTPD)" = "y" ]; then \
		sed -i "/RTCONFIG_PPTPD/d" $(1); \
		echo "RTCONFIG_PPTPD=y" >>$(1); \
	fi; \
	if [ "$(REPEATER)" = "y" ]; then \
		sed -i "/RTCONFIG_WIRELESSREPEATER/d" $(1); \
		echo "RTCONFIG_WIRELESSREPEATER=y" >>$(1); \
	fi; \
	if [ "$(DISABLE_REPEATER_UI)" = "y" ] ; then \
		sed -i "/RTCONFIG_DISABLE_REPEATER_UI/d" $(1); \
		echo "RTCONFIG_DISABLE_REPEATER_UI=y" >>$(1); \
	fi; \
	if [ "$(PROXYSTA)" = "y" ]; then \
		sed -i "/RTCONFIG_PROXYSTA/d" $(1); \
		echo "RTCONFIG_PROXYSTA=y" >>$(1); \
	fi; \
	if [ "$(IXIAEP)" = "y" ]; then \
		sed -i "/RTCONFIG_IXIAEP/d" $(1); \
		echo "RTCONFIG_IXIAEP=y" >>$(1); \
	fi; \
	if [ "$(IPERF)" = "y" ]; then \
		sed -i "/RTCONFIG_IPERF/d" $(1); \
		echo "RTCONFIG_IPERF=y" >>$(1); \
	fi; \
	if [ "$(TCPDUMP)" = "y" ]; then \
		sed -i "/RTCONFIG_TCPDUMP/d" $(1); \
		echo "RTCONFIG_TCPDUMP=y" >>$(1); \
	fi; \
	if [ "$(DISKTEST)" = "y" ]; then \
		sed -i "/RTCONFIG_DISKTEST/d" $(1); \
		echo "RTCONFIG_DISKTEST=y" >>$(1); \
	fi; \
	if [ "$(LOCALE2012)" = "y" ]; then \
		sed -i "/RTCONFIG_LOCALE2012/d" $(1); \
		echo "RTCONFIG_LOCALE2012=y" >>$(1); \
	fi; \
	if [ "$(ODMPID)" = "y" ]; then \
		sed -i "/RTCONFIG_ODMPID/d" $(1); \
		echo "RTCONFIG_ODMPID=y" >>$(1); \
	fi; \
	if [ "$(MDNS)" = "y" ]; then \
		sed -i "/RTCONFIG_MDNS/d" $(1); \
		echo "RTCONFIG_MDNS=y" >>$(1); \
	fi; \
	if [ "$(FINDASUS)" = "y" ]; then \
		sed -i "/RTCONFIG_FINDASUS/d" $(1); \
		echo "RTCONFIG_FINDASUS=y" >>$(1); \
		sed -i "/RTCONFIG_MDNS/d" $(1); \
		echo "RTCONFIG_MDNS=y" >>$(1); \
	fi; \
	if [ "$(TIMEMACHINE)" = "y" ]; then \
		sed -i "/RTCONFIG_TIMEMACHINE/d" $(1); \
		echo "RTCONFIG_TIMEMACHINE=y" >>$(1); \
		sed -i "/RTCONFIG_MDNS/d" $(1); \
		echo "RTCONFIG_MDNS=y" >>$(1); \
	fi; \
	if [ "$(LED_ALL)" = "y" ]; then \
		sed -i "/RTCONFIG_LED_ALL/d" $(1); \
		echo "RTCONFIG_LED_ALL=y" >>$(1); \
	fi; \
	if [ "$(N56U_SR2)" = "y" ]; then \
		sed -i "/RTCONFIG_N56U_SR2/d" $(1); \
		echo "RTCONFIG_N56U_SR2=y" >>$(1); \
	fi; \
	if [ "$(AP_CARRIER_DETECTION)" = "y" ]; then \
		sed -i "/RTCONFIG_AP_CARRIER_DETECTION/d" $(1); \
		echo "RTCONFIG_AP_CARRIER_DETECTION=y" >>$(1); \
	fi; \
	if [ "$(SFP)" = "y" ]; then \
		sed -i "/RTCONFIG_SFP/d" $(1); \
		echo "RTCONFIG_SFP=y" >>$(1); \
	fi; \
	if [ "$(SFP4M)" = "y" ]; then \
		sed -i "/RTCONFIG_SFP/d" $(1); \
		echo "RTCONFIG_SFP=y" >>$(1); \
		sed -i "/RTCONFIG_4M_SFP/d" $(1); \
		echo "RTCONFIG_4M_SFP=y" >>$(1); \
	fi; \
	if [ "$(SFP8M)" = "y" ]; then \
		sed -i "/RTCONFIG_8M_SFP/d" $(1); \
		echo "RTCONFIG_8M_SFP=y" >>$(1); \
	fi; \
	if [ "$(SFPRAM16M)" = "y" ]; then \
		sed -i "/RTCONFIG_16M_RAM_SFP/d" $(1); \
		echo "RTCONFIG_16M_RAM_SFP=y" >>$(1); \
	fi; \
	if [ "$(AUTODICT)" = "y" ]; then \
		sed -i "/RTCONFIG_AUTODICT/d" $(1); \
		echo "RTCONFIG_AUTODICT=y" >>$(1); \
	fi; \
	if [ "$(ZIPLIVEUPDATE)" = "y" ]; then \
		sed -i "/RTCONFIG_AUTOLIVEUPDATE_ZIP/d" $(1); \
		echo "RTCONFIG_AUTOLIVEUPDATE_ZIP=y" >>$(1); \
	fi; \
	if [ "$(LANWAN_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_LANWAN_LED/d" $(1); \
		echo "RTCONFIG_LANWAN_LED=y" >>$(1); \
	fi; \
	if [ "$(WLAN_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_WLAN_LED/d" $(1); \
		echo "RTCONFIG_WLAN_LED=y" >>$(1); \
	fi; \
	if [ "$(EXT_LED_WPS)" = "y" ]; then \
		sed -i "/RTCONFIG_EXT_LED_WPS/d" $(1); \
		echo "RTCONFIG_EXT_LED_WPS=y" >>$(1); \
	fi; \
	if [ "$(LAN4WAN_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_LAN4WAN_LED/d" $(1); \
		echo "RTCONFIG_LAN4WAN_LED=y" >>$(1); \
	fi; \
	if [ "$(SWMODE_SWITCH)" = "y" ]; then \
		sed -i "/RTCONFIG_SWMODE_SWITCH/d" $(1); \
		echo "RTCONFIG_SWMODE_SWITCH=y" >>$(1); \
	fi; \
	if [ "$(WL_AUTO_CHANNEL)" = "y" ]; then \
		sed -i "/RTCONFIG_WL_AUTO_CHANNEL/d" $(1); \
		echo "RTCONFIG_WL_AUTO_CHANNEL=y" >>$(1); \
	fi; \
	if [ "$(SMALL_FW_UPDATE)" = "y" ]; then \
		sed -i "/RTCONFIG_SMALL_FW_UPDATE/d" $(1); \
		echo "RTCONFIG_SMALL_FW_UPDATE=y" >>$(1); \
	fi; \
	if [ "$(WIRELESS_SWITCH)" = "y" ]; then \
		sed -i "/RTCONFIG_WIRELESS_SWITCH/d" $(1); \
		echo "RTCONFIG_WIRELESS_SWITCH=y" >>$(1); \
	fi; \
	if [ "$(BTN_WIFITOG)" = "y" ]; then \
		sed -i "/RTCONFIG_WIFI_TOG_BTN/d" $(1); \
		echo "RTCONFIG_WIFI_TOG_BTN=y" >>$(1); \
	fi; \
	if [ "$(BTN_WPS_RST)" = "y" ]; then \
		sed -i "/RTCONFIG_WPS_RST_BTN/d" $(1); \
		echo "RTCONFIG_WPS_RST_BTN=y" >>$(1); \
	fi; \
	if [ "$(TURBO)" = "y" ]; then \
		sed -i "/RTCONFIG_TURBO/d" $(1); \
		echo "RTCONFIG_TURBO=y" >>$(1); \
	fi; \
	if [ "$(LED_BTN)" = "y" ]; then \
		sed -i "/RTCONFIG_LED_BTN/d" $(1); \
		echo "RTCONFIG_LED_BTN=y" >>$(1); \
	fi; \
	if [ "$(LED_BTN_MODE)" = "y" ]; then \
		sed -i "/RTCONFIG_LED_BTN_MODE/d" $(1); \
		echo "RTCONFIG_LED_BTN_MODE=y" >>$(1); \
	fi; \
	if [ "$(USBEJECT)" = "y" ]; then \
		sed -i "/RTCONFIG_USBEJECT/d" $(1); \
		echo "RTCONFIG_USBEJECT=y" >>$(1); \
	fi; \
	if [ "$(BCM4352_5G)" = "y" ]; then \
		sed -i "/RTCONFIG_4352_5G/d" $(1); \
		echo "RTCONFIG_4352_5G=y" >>$(1); \
	fi; \
	if [ "$(ACCEL_PPTPD)" = "y" ]; then \
		sed -i "/RTCONFIG_ACCEL_PPTPD/d" $(1); \
		echo "RTCONFIG_ACCEL_PPTPD=y" >>$(1); \
	fi; \
	if [ "$(SNMPD)" = "y" ]; then \
		sed -i "/RTCONFIG_SNMPD/d" $(1); \
		echo "RTCONFIG_SNMPD=y" >>$(1); \
	fi; \
	if [ "$(SHP)" = "y" ]; then \
		sed -i "/RTCONFIG_SHP/d" $(1); \
		echo "RTCONFIG_SHP=y" >>$(1); \
	fi; \
	if [ "$(GRO)" = "y" ]; then \
		sed -i "/RTCONFIG_GROCTRL/d" $(1); \
		echo "RTCONFIG_GROCTRL=y" >>$(1); \
	fi; \
	if [ "$(DSL)" = "y" ]; then \
		sed -i "/RTCONFIG_DSL/d" $(1); \
		echo "RTCONFIG_DSL=y" >>$(1); \
		if [ "$(ANNEX_B)" = "y" ]; then \
			echo "RTCONFIG_DSL_ANNEX_B=y" >>$(1); \
		else \
			echo "# RTCONFIG_DSL_ANNEX_B is not set" >>$(1); \
		fi; \
		if [ "$(DSL_TCLINUX)" = "y" ]; then \
			sed -i "/RTCONFIG_DSL_TCLINUX/d" $(1); \
			echo "RTCONFIG_DSL_TCLINUX=y" >>$(1); \
		else \
			echo "# RTCONFIG_DSL_TCLINUX is not set" >>$(1); \
		fi; \
		if [ "$(VDSL)" = "y" ]; then \
			sed -i "/RTCONFIG_VDSL/d" $(1); \
			echo "RTCONFIG_VDSL=y" >>$(1); \
		else \
			echo "# RTCONFIG_VDSL is not set" >>$(1); \
		fi; \
	fi; \
	if [ "$(DUALWAN)" = "y" ]; then \
		sed -i "/RTCONFIG_DUALWAN/d" $(1); \
		echo "RTCONFIG_DUALWAN=y" >>$(1); \
	fi; \
	if [ "$(EMAIL)" = "y" ]; then \
		sed -i "/RTCONFIG_PUSH_EMAIL/d" $(1); \
		echo "RTCONFIG_PUSH_EMAIL=y" >>$(1); \
	fi; \
	if [ "$(USER_LOW_RSSI)" = "y" ]; then \
		sed -i "/RTCONFIG_USER_LOW_RSSI/d" $(1); \
		echo "RTCONFIG_USER_LOW_RSSI=y" >>$(1); \
	fi; \
	if [ "$(USB)" = "USB" ]; then \
		sed -i "/RTCONFIG_USB is not set/d" $(1); \
		echo "RTCONFIG_USB=y" >>$(1); \
		if [ "$(USBEXTRAS)" = "y" ]; then \
			sed -i "/RTCONFIG_USB_EXTRAS/d" $(1); \
			echo "RTCONFIG_USB_EXTRAS=y" >>$(1); \
		fi; \
		if [ "$(TFAT)" = "y" ]; then \
			sed -i "/RTCONFIG_TFAT/d" $(1); \
			echo "RTCONFIG_TFAT=y" >>$(1); \
		fi; \
		if [ "$(NTFS)" != "" ]; then \
			sed -i "/RTCONFIG_NTFS/d" $(1); \
			echo "RTCONFIG_NTFS=y" >>$(1); \
			if [ "$(NTFS)" = "open" ]; then \
				sed -i "/RTCONFIG_OPEN_NTFS3G/d" $(1); \
				echo "RTCONFIG_OPEN_NTFS3G=y" >>$(1); \
			elif [ "$(NTFS)" = "paragon" ]; then \
				sed -i "/RTCONFIG_PARAGON_NTFS/d" $(1); \
				echo "RTCONFIG_PARAGON_NTFS=y" >>$(1); \
			elif [ "$(NTFS)" = "tuxera" ]; then \
				sed -i "/RTCONFIG_TUXERA_NTFS/d" $(1); \
				echo "RTCONFIG_TUXERA_NTFS=y" >>$(1); \
			fi; \
		fi; \
		if [ "$(HFS)" != "" ]; then \
			if [ "$(HFS)" = "open" ]; then \
				sed -i "/RTCONFIG_KERNEL_HFSPLUS/d" $(1); \
				echo "RTCONFIG_KERNEL_HFSPLUS=y" >>$(1); \
	                        sed -i "/RTCONFIG_HFS/d" $(1); \
	                        echo "RTCONFIG_HFS=y" >>$(1); \
			elif [ "$(HFS)" = "paragon" ]; then \
				sed -i "/RTCONFIG_PARAGON_HFS/d" $(1); \
				echo "RTCONFIG_PARAGON_HFS=y" >>$(1); \
	                        sed -i "/RTCONFIG_HFS/d" $(1); \
	                        echo "RTCONFIG_HFS=y" >>$(1); \
			elif [ "$(HFS)" = "tuxera" ]; then \
				sed -i "/RTCONFIG_TUXERA_HFS/d" $(1); \
				echo "RTCONFIG_TUXERA_HFS=y" >>$(1); \
	                        sed -i "/RTCONFIG_HFS/d" $(1); \
	                        echo "RTCONFIG_HFS=y" >>$(1); \
			fi; \
		fi; \
		if [ "$(UFSDDEBUG)" = "y" ]; then \
			sed -i "/RTCONFIG_UFSD_DEBUG/d" $(1); \
			echo "RTCONFIG_UFSD_DEBUG=y" >>$(1); \
		fi; \
		if [ "$(DISK_MONITOR)" = "y" ]; then \
			sed -i "/RTCONFIG_DISK_MONITOR/d" $(1); \
			echo "RTCONFIG_DISK_MONITOR=y" >>$(1); \
		fi; \
		if [ "$(MEDIASRV)" = "y" ]; then \
			sed -i "/RTCONFIG_MEDIA_SERVER/d" $(1); \
			echo "RTCONFIG_MEDIA_SERVER=y" >>$(1); \
		fi; \
		if [ "$(SWEBDAVCLIENT)" = "y" ]; then \
			sed -i "/RTCONFIG_SWEBDAVCLIENT/d" $(1); \
			echo "RTCONFIG_SWEBDAVCLIENT=y" >>$(1); \
		fi; \
		if [ "$(DROPBOXCLIENT)" = "y" ]; then \
			sed -i "/RTCONFIG_DROPBOXCLIENT/d" $(1); \
			echo "RTCONFIG_DROPBOXCLIENT=y" >>$(1); \
		fi; \
		if [ "$(FTPCLIENT)" = "y" ]; then \
			sed -i "/RTCONFIG_FTPCLIENT/d" $(1); \
			echo "RTCONFIG_FTPCLIENT=y" >>$(1); \
		fi; \
		if [ "$(SAMBACLIENT)" = "y" ]; then \
			sed -i "/RTCONFIG_SAMBACLIENT/d" $(1); \
			echo "RTCONFIG_SAMBACLIENT=y" >>$(1); \
		fi; \
		if [ "$(FLICKRCLIENT)" = "y" ]; then \
			sed -i "/RTCONFIG_FLICKRCLIENT/d" $(1); \
			echo "RTCONFIG_FLICKRCLIENT=y" >>$(1); \
		fi; \
		if [ "$(CLOUDSYNC)" = "y" ]; then \
			sed -i "/RTCONFIG_CLOUDSYNC/d" $(1); \
			echo "RTCONFIG_CLOUDSYNC=y" >>$(1); \
		fi; \
		if [ "$(MODEM)" = "y" ]; then \
			sed -i "/RTCONFIG_USB_MODEM/d" $(1); \
			echo "RTCONFIG_USB_MODEM=y" >>$(1); \
			if [ "$(MODEMPIN)" = "y" ]; then \
				echo "RTCONFIG_USB_MODEM_PIN=y" >>$(1); \
			else \
				echo "# RTCONFIG_USB_MODEM_PIN is not set" >>$(1); \
			fi; \
			if [ "$(BECEEM)" = "y" ]; then \
				sed -i "/RTCONFIG_USB_BECEEM/d" $(1); \
				echo "RTCONFIG_USB_BECEEM=y" >>$(1); \
			fi; \
			if [ "$(LESSMODEM)" = "y" ]; then \
				sed -i "/RTCONFIG_USB_LESSMODEM/d" $(1); \
				echo "RTCONFIG_USB_LESSMODEM=y" >>$(1); \
			fi; \
		fi; \
		if [ "$(PRINTER)" = "y" ]; then \
			sed -i "/RTCONFIG_USB_PRINTER/d" $(1); \
			echo "RTCONFIG_USB_PRINTER=y" >>$(1); \
		fi; \
		if [ "$(WEBDAV)" = "y" ]; then \
			sed -i "/RTCONFIG_WEBDAV/d" $(1); \
			echo "RTCONFIG_WEBDAV=y" >>$(1); \
		fi; \
		if [ "$(USBAP)" = "y" ]; then \
			sed -i "/RTCONFIG_BRCM_USBAP/d" $(1); \
			echo "RTCONFIG_BRCM_USBAP=y" >>$(1); \
			if [ "$(RTAC53U)" != "y" ]; then \
				sed -i "/EPI_VERSION_NUM/d" include/epivers.h; \
				sed -i "/#endif \/\* _epivers_h_ \*\//d" include/epivers.h; \
				echo "#define	EPI_VERSION_NUM		$(DONGLE_VER)" >>include/epivers.h; \
				echo "#endif /* _epivers_h_ */" >>include/epivers.h; \
			fi; \
		fi; \
		if [ "$(XHCI)" = "y" ]; then \
			sed -i "/RTCONFIG_USB_XHCI/d" $(1); \
			echo "RTCONFIG_USB_XHCI=y" >>$(1); \
		fi; \
		if [ "$(NFS)" = "y" ]; then \
			sed -i "/RTCONFIG_NFS/d" $(1); \
			echo "RTCONFIG_NFS=y" >>$(1); \
		fi; \
	else \
		sed -i "/RTCONFIG_USB\b/d" $(1); \
		echo "# RTCONFIG_USB is not set" >>$(1); \
	fi; \
	if [ "$(HTTPS)" = "y" ]; then \
		sed -i "/RTCONFIG_HTTPS/d" $(1); \
		echo "RTCONFIG_HTTPS=y" >>$(1); \
	fi; \
	if [ "$(USBRESET)" = "y" ]; then \
		sed -i "/RTCONFIG_USBRESET/d" $(1); \
		echo "RTCONFIG_USBRESET=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_USBRESET/d" $(1); \
		echo "# RTCONFIG_USBRESET is not set" >>$(1); \
	fi; \
	if [ "$(WIFIPWR)" = "y" ]; then \
		sed -i "/RTCONFIG_WIFIPWR/d" $(1); \
		echo "RTCONFIG_WIFIPWR=y" >>$(1); \
	fi; \
	if [ "$(LEDARRAY)" = "y" ]; then \
		sed -i "/RTCONFIG_LEDARRAY/d" $(1); \
		echo "RTCONFIG_LEDARRAY=y" >>$(1); \
	fi; \
	if [ "$(XHCIMODE)" = "y" ]; then \
		sed -i "/RTCONFIG_XHCIMODE/d" $(1); \
		echo "RTCONFIG_XHCIMODE=y" >>$(1); \
	fi; \
	if [ "$(NO_SAMBA)" = "y" ]; then \
		sed -i "/RTCONFIG_SAMBASRV/d" $(1); \
		echo "# RTCONFIG_SAMBASRV is not set" >>$(1); \
	fi; \
	if [ "$(NO_USBSTORAGE)" = "y" ]; then \
		sed -i "/RTCONFIG_SAMBASRV/d" $(1); \
		echo "# RTCONFIG_SAMBASRV is not set" >>$(1); \
		sed -i "/RTCONFIG_FTP/d" $(1); \
		echo "# RTCONFIG_FTP is not set" >>$(1); \
	fi; \
	if [ "$(ZEBRA)" = "y" ]; then \
		sed -i "/RTCONFIG_ZEBRA/d" $(1); \
		echo "RTCONFIG_ZEBRA=y" >>$(1); \
	fi; \
	if [ "$(JFFS2)" = "y" ]; then \
		sed -i "/RTCONFIG_JFFS2/d" $(1); \
		echo "RTCONFIG_JFFS2=y" >>$(1); \
	fi; \
	if [ "$(BRCM_NAND_JFFS2)" = "y" ]; then \
		sed -i "/RTCONFIG_BRCM_NAND_JFFS2/d" $(1); \
		echo "RTCONFIG_BRCM_NAND_JFFS2=y" >>$(1); \
	fi; \
	if [ "$(JFFS1)" = "y" ]; then \
		sed -i "/RTCONFIG_JFFSV1/d" $(1); \
		echo "RTCONFIG_JFFSV1=y" >>$(1); \
	fi; \
	if [ "$(CIFS)" = "y" ]; then \
		sed -i "/RTCONFIG_CIFS/d" $(1); \
		echo "RTCONFIG_CIFS=y" >>$(1); \
		sed -i "/RTCONFIG_AUTODICT/d" $(1); \
		echo "RTCONFIG_AUTODICT=n" >>$(1); \
	fi; \
	if [ "$(SSH)" = "y" ]; then \
		sed -i "/RTCONFIG_SSH/d" $(1); \
		echo "RTCONFIG_SSH=y" >>$(1); \
	fi; \
	if [ "$(NO_LIBOPT)" = "y" ]; then \
		sed -i "/RTCONFIG_OPTIMIZE_SHARED_LIBS/d" $(1); \
		echo "# RTCONFIG_OPTIMIZE_SHARED_LIBS is not set" >>$(1); \
	fi; \
	if [ "$(EBTABLES)" = "y" ]; then \
		sed -i "/RTCONFIG_EBTABLES/d" $(1); \
		echo "RTCONFIG_EBTABLES=y" >>$(1); \
	fi; \
	if [ "$(IPV6SUPP)" = "y" ]; then \
		sed -i "/RTCONFIG_IPV6/d" $(1); \
		echo "RTCONFIG_IPV6=y" >>$(1); \
	fi; \
	if [ "$(OPENVPN)" = "y" ]; then \
		sed -i "/RTCONFIG_LZO/d" $(1); \
		echo "RTCONFIG_LZO=y" >>$(1); \
		sed -i "/RTCONFIG_OPENVPN/d" $(1); \
		echo "RTCONFIG_OPENVPN=y" >>$(1); \
 	fi; \
	if [ "$(APP)" = "installed" ]; then \
		sed -i "/RTCONFIG_APP_PREINSTALLED/d" $(1); \
		echo "RTCONFIG_APP_PREINSTALLED=y" >>$(1); \
	elif [ "$(APP)" = "network" ]; then \
		sed -i "/RTCONFIG_APP_NETINSTALLED/d" $(1); \
		echo "RTCONFIG_APP_NETINSTALLED=y" >>$(1); \
        elif [ "$(APP)" = "nolocaldm" ] ; then \
                sed -i "/RTCONFIG_APP_NETINSTALLED/d" $(1); \
                echo "RTCONFIG_APP_NETINSTALLED=y" >>$(1); \
                sed -i "/RTCONFIG_APP_NOLOCALDM/d" $(1); \
                echo "RTCONFIG_APP_NOLOCALDM=y" >>$(1); \
	fi; \
	if [ "$(STRACE)" = "y" ] ; then \
		sed -i "/RTCONFIG_STRACE/d" $(1); \
		echo "RTCONFIG_STRACE=y" >>$(1); \
	fi; \
	if [ "$(DEBUG)" = "y" ] || [ "$(GDB)" = "y" ]; then \
                sed -i "/CONFIG_ELF_CORE/d" $(1); \
                echo "CONFIG_ELF_CORE=y" >>$(1); \
        	if [ "$(BCMWL6A)" = "y" ]; then \
                	sed -i "/CONFIG_CORE_DUMP_DEFAULT_ELF_HEADERS/d" $(1); \
                	echo "# CONFIG_CORE_DUMP_DEFAULT_ELF_HEADERS is not set" >>$(1); \
        	fi; \
        fi; \
        if [ "$(DUMP_OOPS_MSG)" = "y" ]; then \
                if [ "$(BUILD_NAME)" = "RT-AC66U" ] || [ "$(BUILD_NAME)" = "RT-N66U" ]; then \
                        echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
                        echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0x07FFE000" >>$(1); \
                        echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
                else \
                        echo "# CONFIG_DUMP_PREV_OOPS_MSG is not set" >>$(1); \
                fi; \
        else \
                echo "# CONFIG_DUMP_PREV_OOPS_MSG is not set" >>$(1); \
        fi; \
	if [ "$(DNSMASQ)" = "y" ]; then \
		sed -i "/RTCONFIG_DNSMASQ/d" $(1); \
		echo "RTCONFIG_DNSMASQ=y" >>$(1); \
	fi; \
	if [ "$(ISP_METER)" = "y" ]; then \
		sed -i "/RTCONFIG_ISP_METER/d" $(1); \
		echo "RTCONFIG_ISP_METER=y" >>$(1); \
	fi; \
	if [ "$(NVRAM_64K)" = "y" ]; then \
		sed -i "/RTCONFIG_NVRAM_64K/d" $(1); \
		echo "RTCONFIG_NVRAM_64K=y" >>$(1); \
	fi; \
	if [ "$(DUAL_TRX)" = "y" ]; then \
		sed -i "/RTCONFIG_DUAL_TRX/d" $(1); \
		echo "RTCONFIG_DUAL_TRX=y" >>$(1); \
	fi; \
	if [ "$(PSISTLOG)" = "y" ]; then \
		sed -i "/RTCONFIG_PSISTLOG/d" $(1); \
		echo "RTCONFIG_PSISTLOG=y" >>$(1); \
	fi; \
	if [ "$(UBI)" = "y" ]; then \
		sed -i "/RTCONFIG_UBI/d" $(1); \
		echo "RTCONFIG_UBI=y" >>$(1); \
		if [ "$(UBIFS)" = "y" ]; then \
			sed -i "/RTCONFIG_UBIFS/d" $(1); \
			echo "RTCONFIG_UBIFS=y" >>$(1); \
		else \
			sed -i "/RTCONFIG_UBIFS/d" $(1); \
			echo "# RTCONFIG_UBIFS is not set" >>$(1); \
		fi; \
	else \
		sed -i "/RTCONFIG_UBI/d" $(1); \
		echo "# RTCONFIG_UBI is not set" >>$(1); \
		sed -i "/RTCONFIG_UBIFS/d" $(1); \
		echo "# RTCONFIG_UBIFS is not set" >>$(1); \
	fi; \
	if [ "$(OPTIMIZE_XBOX)" = "y" ]; then \
		sed -i "/RTCONFIG_OPTIMIZE_XBOX/d" $(1); \
		echo "RTCONFIG_OPTIMIZE_XBOX=y" >>$(1); \
	fi; \
	if [ "$(NEW_RGDM)" = "y" ]; then \
		sed -i "/RTCONFIG_NEW_REGULATION_DOMAIN/d" $(1); \
		echo "RTCONFIG_NEW_REGULATION_DOMAIN=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_NEW_REGULATION_DOMAIN/d" $(1); \
		echo "# RTCONFIG_NEW_REGULATION_DOMAIN is not set" >>$(1); \
	fi; \
	if [ "$(DYN_DICT_NAME)" = "y" ]; then \
		sed -i "/RTCONFIG_DYN_DICT_NAME/d" $(1); \
		echo "RTCONFIG_DYN_DICT_NAME=y" >>$(1); \
	fi; \
	if [ "$(EXT4FS)" = "y" ]; then \
		sed -i "/RTCONFIG_EXT4FS/d" $(1); \
		echo "RTCONFIG_EXT4FS=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_EXT4FS/d" $(1); \
		echo "# RTCONFIG_EXT4FS is not set" >>$(1); \
	fi; \
	if [ "$(DMALLOC)" = "y" ]; then \
		sed -i "/RTCONFIG_DMALLOC/d" $(1); \
		echo "RTCONFIG_DMALLOC=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_DMALLOC/d" $(1); \
		echo "# RTCONFIG_DMALLOC is not set" >>$(1); \
	fi; \
	if [ "$(JFFS2ND_BACKUP)" = "y" ]; then \
		sed -i "/RTCONFIG_JFFS2ND_BACKUP/d" $(1); \
		echo "RTCONFIG_JFFS2ND_BACKUP=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_JFFS2ND_BACKUP/d" $(1); \
		echo "# RTCONFIG_JFFS2ND_BACKUP is not set" >>$(1); \
	fi; \
	if [ "$(TEMPROOTFS)" = "y" ]; then \
		sed -i "/RTCONFIG_TEMPROOTFS/d" $(1); \
		echo "RTCONFIG_TEMPROOTFS=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_TEMPROOTFS/d" $(1); \
		echo "# RTCONFIG_TEMPROOTFS is not set" >>$(1); \
	fi; \
	if [ "$(ATEUSB3_FORCE)" = "y" ]; then \
		sed -i "/RTCONFIG_ATEUSB3_FORCE/d" $(1); \
		echo "RTCONFIG_ATEUSB3_FORCE=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_ATEUSB3_FORCE/d" $(1); \
		echo "# RTCONFIG_ATEUSB3_FORCE is not set" >>$(1); \
	fi; \
	if [ "$(JFFS2LOG)" = "y" ]; then \
		sed -i "/RTCONFIG_JFFS2LOG/d" $(1); \
		echo "RTCONFIG_JFFS2LOG=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_JFFS2LOG/d" $(1); \
		echo "# RTCONFIG_JFFS2LOG is not set" >>$(1); \
	fi; \
	if [ "$(WPSMULTIBAND)" = "y" ]; then \
		sed -i "/RTCONFIG_WPSMULTIBAND/d" $(1); \
		echo "RTCONFIG_WPSMULTIBAND=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_WPSMULTIBAND/d" $(1); \
		echo "# RTCONFIG_WPSMULTIBAND is not set" >>$(1); \
	fi; \
	if [ "$(EM)" = "y" ]; then \
		sed -i "/RTCONFIG_ENGINEERING_MODE/d" $(1); \
		echo "RTCONFIG_ENGINEERING_MODE=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_ENGINEERING_MODE/d" $(1); \
		echo "# RTCONFIG_ENGINEERING_MODE is not set" >>$(1); \
	fi; \
	if [ "$(VPNC)" = "y" ]; then \
		sed -i "/RTCONFIG_VPNC/d" $(1); \
		echo "RTCONFIG_VPNC=y" >>$(1); \
	fi; \
	if [ "$(IPSEC)" = "y" ]; then \
		sed -i "/RTCONFIG_IPSEC/d" $(1); \
		echo "RTCONFIG_IPSEC=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_IPSEC/d" $(1); \
		echo "# RTCONFIG_IPSEC is not set" >>$(1); \
	fi; \
	if [ "$(KYIVSTAR)" = "y" ]; then \
		sed -i "/RTCONFIG_KYIVSTAR/d" $(1); \
		echo "RTCONFIG_KYIVSTAR=y" >>$(1); \
	fi; \
	if [ "$(ETRON_XHCI)" = "y" ]; then \
		sed -i "/RTCONFIG_ETRON_XHCI\>/d" $(1); \
		echo "RTCONFIG_ETRON_XHCI=y" >>$(1); \
		if [ "$(ETRON_XHCI_USB3_LED)" = "y" ]; then \
			sed -i "/RTCONFIG_ETRON_XHCI_USB3_LED/d" $(1); \
			echo "RTCONFIG_ETRON_XHCI_USB3_LED=y" >>$(1); \
		else \
			sed -i "/RTCONFIG_ETRON_XHCI_USB3_LED/d" $(1); \
			echo "# RTCONFIG_ETRON_XHCI_USB3_LED is not set" >>$(1); \
		fi; \
	fi; \
	if [ "$(WPS_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_WPS_LED/d" $(1); \
		echo "RTCONFIG_WPS_LED=y" >>$(1); \
	fi; \
	if [ "$(WANRED_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_WANRED_LED/d" $(1); \
		echo "RTCONFIG_WANRED_LED=y" >>$(1); \
	fi; \
	if [ "$(BLINK_LED)" = "y" ]; then \
		sed -i "/RTCONFIG_BLINK_LED/d" $(1); \
		echo "RTCONFIG_BLINK_LED=y" >>$(1); \
	fi; \
	if [ "$(CFE_NVRAM_CHK)" = "y" ]; then \
		sed -i "/RTCONFIG_CFE_NVRAM_CHK/d" $(1); \
		echo "RTCONFIG_CFE_NVRAM_CHK=y" >>$(1); \
	fi; \
	if [ "$(LLDP)" = "y" ]; then \
		sed -i "/RTCONFIG_LLDP/d" $(1); \
		echo "RTCONFIG_LLDP=y" >>$(1); \
	fi; \
	if [ "$(DEBUG)" = "y" ]; then \
		sed -i "/RTCONFIG_DEBUG/d" $(1); \
		echo "RTCONFIG_DEBUG=y" >>$(1); \
		sed -i "/RTCONFIG_GDB/d" $(1); \
		echo "RTCONFIG_GDB=y" >>$(1); \
        fi; \
	if [ "$(UIDEBUG)" = "y" ]; then \
		sed -i "/RTCONFIG_UIDEBUG/d" $(1); \
		echo "RTCONFIG_UIDEBUG=y" >>$(1); \
		sed -i "/RTCONFIG_CIFS/d" $(1); \
		echo "RTCONFIG_CIFS=y" >>$(1); \
		sed -i "/RTCONFIG_AUTODICT/d" $(1); \
		echo "# RTCONFIG_AUTODICT is not set" >>$(1); \
		sed -i "/RTCONFIG_TIMEMACHINE/d" $(1); \
		echo "# RTCONFIG_TIMEMACHINE is not set" >>$(1); \
		sed -i "/RTCONFIG_MDNS/d" $(1); \
		echo "# RTCONFIG_MDNS is not set" >>$(1); \
		sed -i "/RTCONFIG_JFFS2USERICON/d" $(1); \
		echo "RTCONFIG_JFFS2USERICON=y" >>$(1); \
	fi; \
	if [ "$(ROG)" = "y" ]; then \
		sed -i "/RTCONFIG_ROG/d" $(1); \
		echo "RTCONFIG_ROG=y" >>$(1); \
	fi; \
	if [ "$(GEOIP)" = "y" ]; then \
		sed -i "/RTCONFIG_GEOIP/d" $(1); \
		echo "RTCONFIG_GEOIP=y" >>$(1); \
	fi; \
	if [ "$(SINGLE_2G)" = "y" ]; then \
		sed -i "/RTCONFIG_HAS_5G/d" $(1); \
		echo "# RTCONFIG_HAS_5G is not set" >>$(1); \
	fi; \
	if [ "$(TFTP)" = "y" ]; then \
		sed -i "/RTCONFIG_TFTP/d" $(1); \
		echo "RTCONFIG_TFTP=y" >>$(1); \
	fi; \
	if [ "$(QTN)" = "y" ]; then \
		sed -i "/RTCONFIG_QTN/d" $(1); \
		echo "RTCONFIG_QTN=y" >>$(1); \
	fi; \
	if [ "$(RGMII_BRCM5301X)" = "y" ]; then \
		sed -i "/RTCONFIG_RGMII_BRCM5301X/d" $(1); \
		echo "RTCONFIG_RGMII_BRCM5301X=y" >>$(1); \
	fi; \
	if [ "$(DNSFILTER)" = "y" ]; then \
		sed -i "/RTCONFIG_DNSFILTER/d" $(1); \
		echo "RTCONFIG_DNSFILTER=y" >>$(1); \
	fi; \
	if [ "$(WPS_DUALBAND)" = "y" ]; then \
		sed -i "/RTCONFIG_WPS_DUALBAND/d" $(1); \
		echo "RTCONFIG_WPS_DUALBAND=y" >>$(1); \
	fi; \
	if [ "$(N18UTXBF)" = "y" ]; then \
		sed -i "/RTCONFIG_N18UTXBF/d" $(1); \
		echo "RTCONFIG_N18UTXBF=y" >>$(1); \
	fi; \
	if [ "$(BWDPI)" = "y" ]; then \
		sed -i "/RTCONFIG_BWDPI/d" $(1); \
		echo "RTCONFIG_BWDPI=y" >>$(1); \
		sed -i "/RTCONFIG_PUSH_EMAIL/d" $(1); \
		echo "RTCONFIG_PUSH_EMAIL=y" >>$(1); \
	fi; \
	if [ "$(ADBLOCK)" = "y" ]; then \
		sed -i "/RTCONFIG_ADBLOCK/d" $(1); \
		echo "RTCONFIG_ADBLOCK=y" >>$(1); \
	fi; \
	if [ "$(SPEEDTEST)" = "y" ]; then \
		sed -i "/RTCONFIG_SPEEDTEST/d" $(1); \
		echo "RTCONFIG_SPEEDTEST=y" >>$(1); \
	fi; \
	if [ "$(BCM7)" = "y" ]; then \
		sed -i "/RTCONFIG_BCM7/d" $(1); \
		echo "RTCONFIG_BCM7=y" >>$(1); \
		sed -i "/RTCONFIG_TOAD/d" $(1); \
		echo "RTCONFIG_TOAD=y" >>$(1); \
		sed -i "/RTCONFIG_BCMBSD/d" $(1); \
		echo "RTCONFIG_BCMBSD=y" >>$(1); \
		sed -i "/RTCONFIG_BCMDCS/d" $(1); \
		echo "RTCONFIG_BCMDCS=y" >>$(1); \
		sed -i "/RTCONFIG_TRAFFIC_MGMT_RSSI_POLICY/d" $(1); \
		echo "RTCONFIG_TRAFFIC_MGMT_RSSI_POLICY=y" >>$(1); \
		sed -i "/RTCONFIG_DPSTA/d" $(1); \
		echo "RTCONFIG_DPSTA=y" >>$(1); \
		sed -i "/RTCONFIG_DHDAP/d" $(1); \
		echo "RTCONFIG_DHDAP=y" >>$(1); \
		sed -i "/RTCONFIG_GMAC3/d" $(1); \
		echo "RTCONFIG_GMAC3=y" >>$(1); \
	fi; \
	if [ "$(BUILD_BCM7)" = "y" ]; then \
		sed -i "/RTCONFIG_BUILDBCM7/d" $(1); \
		echo "RTCONFIG_BUILDBCM7=y" >>$(1); \
	fi; \
	if [ "$(SSD)" = "y" ]; then \
		sed -i "/RTCONFIG_BCMSSD/d" $(1); \
		echo "RTCONFIG_BCMSSD=y" >>$(1); \
	fi; \
	if [ "$(HSPOT)" = "y" ]; then \
		sed -i "/RTCONFIG_HSPOT/d" $(1); \
		echo "RTCONFIG_HSPOT=y" >>$(1); \
	fi; \
	if [ "$(NVSIZE)" != "" ]; then \
		sed -i "/RTCONFIG_NV$(NVSIZE)/d" $(1); \
		echo "RTCONFIG_NV$(NVSIZE)=y" >>$(1); \
	fi; \
	if [ "$(BONDING)" = "y" ]; then \
		sed -i "/RTCONFIG_BONDING/d" $(1); \
		echo "RTCONFIG_BONDING=y" >>$(1); \
	fi; \
	if [ "$(NOWL)" = "y" ]; then \
		sed -i "/RTCONFIG_NOWL/d" $(1); \
		echo "RTCONFIG_NOWL=y" >>$(1); \
	fi; \
	if [ "$(WIFILOGO)" = "y" ]; then \
		sed -i "/RTCONFIG_WIFILOGO/d" $(1); \
		echo "RTCONFIG_WIFILOGO=y" >>$(1); \
	fi; \
	if [ "$(JFFS2USERICON)" = "y" ]; then \
		sed -i "/RTCONFIG_JFFS2USERICON/d" $(1); \
		echo "RTCONFIG_JFFS2USERICON=y" >>$(1); \
	else \
		sed -i "/RTCONFIG_JFFS2USERICON/d" $(1); \
		echo "# RTCONFIG_JFFS2USERICON is not set" >>$(1); \
	fi; \
	if [ "$(TOR)" = "y" ]; then \
                sed -i "/RTCONFIG_TOR/d" $(1); \
                echo "RTCONFIG_TOR=y" >>$(1); \
        fi; \
	if [ "$(TR069)" = "y" ]; then \
		sed -i "/RTCONFIG_TR069/d" $(1); \
		echo "RTCONFIG_TR069=y" >>$(1); \
	fi; \
	if [ "$(TR181)" = "y" ]; then \
		sed -i "/RTCONFIG_TR181/d" $(1); \
		echo "RTCONFIG_TR181=y" >>$(1); \
	fi; \
	)
	$(call platformRouterOptions, $(1))
endef

define BusyboxOptions
	@( \
	if [ "$(CONFIG_LINUX26)" = "y" ]; then \
		sed -i "/CONFIG_FEATURE_2_4_MODULES/d" $(1); \
		echo "# CONFIG_FEATURE_2_4_MODULES is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_LSMOD_PRETTY_2_6_OUTPUT/d" $(1); \
		echo "CONFIG_FEATURE_LSMOD_PRETTY_2_6_OUTPUT=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_DEVFS/d" $(1); \
		echo "# CONFIG_FEATURE_DEVFS is not set" >>$(1); \
		sed -i "/CONFIG_MKNOD/d" $(1); \
		echo "CONFIG_MKNOD=y" >>$(1); \
	fi; \
	if [ "$(NO_CIFS)" = "y" ]; then \
		sed -i "/CONFIG_FEATURE_MOUNT_CIFS/d" $(1); \
		echo "# CONFIG_FEATURE_MOUNT_CIFS is not set" >>$(1); \
	fi; \
	if [ "$(BBEXTRAS)" = "y" ]; then \
		sed -i "/CONFIG_FEATURE_SORT_BIG/d" $(1); \
		echo "CONFIG_FEATURE_SORT_BIG=y" >>$(1); \
		sed -i "/CONFIG_CLEAR/d" $(1); \
		echo "CONFIG_CLEAR=y" >>$(1); \
		sed -i "/CONFIG_SETCONSOLE/d" $(1); \
		echo "CONFIG_SETCONSOLE=y" >>$(1); \
		if [ "$(CONFIG_LINUX26)" = "y" ]; then \
			sed -i "/CONFIG_FEATURE_SYSLOGD_READ_BUFFER_SIZE/d" $(1); \
			echo "CONFIG_FEATURE_SYSLOGD_READ_BUFFER_SIZE=512" >>$(1); \
		fi; \
		if [ "$(DSL)" = "y" ]; then \
			sed -i "/CONFIG_TFTP/d" $(1); \
			echo "CONFIG_TFTP=y" >>$(1); \
			sed -i "/CONFIG_TFTPD/d" $(1); \
			echo "# CONFIG_TFTPD is not set" >>$(1); \
			sed -i "/CONFIG_FEATURE_TFTP_GET/d" $(1); \
			echo "CONFIG_FEATURE_TFTP_GET=y" >>$(1); \
			sed -i "/CONFIG_FEATURE_TFTP_PUT/d" $(1); \
			echo "CONFIG_FEATURE_TFTP_PUT=y" >>$(1); \
			sed -i "/CONFIG_FEATURE_TFTP_BLOCKSIZE/d" $(1); \
			echo "# CONFIG_FEATURE_TFTP_BLOCKSIZE is not set" >>$(1); \
			sed -i "/CONFIG_FEATURE_TFTP_PROGRESS_BAR/d" $(1); \
			echo "# CONFIG_FEATURE_TFTP_PROGRESS_BAR is not set" >>$(1); \
			sed -i "/CONFIG_TFTP_DEBUG/d" $(1); \
			echo "# CONFIG_TFTP_DEBUG is not set" >>$(1); \
			if [ "$(DSL_TCLINUX)" = "y" ]; then \
				sed -i "/CONFIG_TELNET/d" $(1); \
				echo "CONFIG_TELNET=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_TELNET_TTYPE/d" $(1); \
				echo "# CONFIG_FEATURE_TELNET_TTYPE is not set" >>$(1); \
				sed -i "/CONFIG_FEATURE_TELNET_AUTOLOGIN/d" $(1); \
				echo "# CONFIG_FEATURE_TELNET_AUTOLOGIN is not set" >>$(1); \
				sed -i "/CONFIG_TELNETD/d" $(1); \
				echo "CONFIG_TELNETD=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_TELNETD_STANDALONE/d" $(1); \
				echo "CONFIG_FEATURE_TELNETD_STANDALONE=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_TELNETD_INETD_WAIT/d" $(1); \
				echo "# CONFIG_FEATURE_TELNETD_INETD_WAIT is not set" >>$(1); \
				sed -i "/CONFIG_FTPGET/d" $(1); \
				echo "CONFIG_FTPGET=y" >>$(1); \
				sed -i "/CONFIG_FTPPUT/d" $(1); \
				echo "CONFIG_FTPPUT=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_FTPGETPUT_LONG_OPTIONS/d" $(1); \
				echo "CONFIG_FEATURE_FTPGETPUT_LONG_OPTIONS=y" >>$(1); \
				sed -i "/CONFIG_SPLIT/d" $(1); \
				echo "CONFIG_SPLIT=y" >>$(1); \
			fi;\
		fi; \
	fi; \
	if [ "$(USB)" = "USB" ]; then \
		if [ "$(NFS)" = "y" ]; then \
			sed -i "/CONFIG_FEATURE_MOUNT_NFS/d" $(1); \
	                echo "CONFIG_FEATURE_MOUNT_NFS=y" >>$(1); \
		fi; \
		if [ "$(USBEXTRAS)" = "y" ]; then \
			sed -i "/CONFIG_E2FSCK/d" $(1); \
			echo "CONFIG_E2FSCK=n" >>$(1); \
			sed -i "/CONFIG_FSCK_MINIX/d" $(1); \
			echo "CONFIG_FSCK_MINIX=n" >>$(1); \
			sed -i "/CONFIG_MKE2FS/d" $(1); \
			echo "CONFIG_MKE2FS=n" >>$(1); \
			sed -i "/CONFIG_FDISK/d" $(1); \
			echo "CONFIG_FDISK=y" >>$(1); \
			sed -i "/CONFIG_FEATURE_FDISK_WRITABLE/d" $(1); \
			echo "CONFIG_FEATURE_FDISK_WRITABLE=y" >>$(1); \
			sed -i "/CONFIG_MKSWAP/d" $(1); \
			echo "CONFIG_MKSWAP=y" >>$(1); \
			sed -i "/CONFIG_FLOCK/d" $(1); \
			echo "CONFIG_FLOCK=y" >>$(1); \
			sed -i "/CONFIG_FSYNC/d" $(1); \
			echo "CONFIG_FSYNC=y" >>$(1); \
			sed -i "/CONFIG_UNZIP/d" $(1); \
			echo "CONFIG_UNZIP=y" >>$(1); \
			if [ "$(CONFIG_LINUX26)" = "y" ]; then \
				sed -i "/CONFIG_LSUSB/d" $(1); \
				echo "CONFIG_LSUSB=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_WGET_STATUSBAR/d" $(1); \
				echo "CONFIG_FEATURE_WGET_STATUSBAR=y" >>$(1); \
				sed -i "/CONFIG_FEATURE_VERBOSE_USAGE/d" $(1); \
				echo "CONFIG_FEATURE_VERBOSE_USAGE=y" >>$(1); \
			fi; \
		fi; \
	else \
		sed -i "/CONFIG_FEATURE_MOUNT_LOOP/d" $(1); \
		echo "# CONFIG_FEATURE_MOUNT_LOOP is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_DEVFS/d" $(1); \
		echo "# CONFIG_FEATURE_DEVFS is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_MOUNT_LABEL/d" $(1); \
		echo "# CONFIG_FEATURE_MOUNT_LABEL is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_MOUNT_FSTAB/d" $(1); \
		echo "# CONFIG_FEATURE_MOUNT_FSTAB is not set" >>$(1); \
		sed -i "/CONFIG_VOLUMEID/d" $(1); \
		echo "# CONFIG_VOLUMEID is not set" >>$(1); \
		sed -i "/CONFIG_BLKID/d" $(1); \
		echo "# CONFIG_BLKID is not set" >>$(1); \
		sed -i "/CONFIG_SWAPONOFF/d" $(1); \
		echo "# CONFIG_SWAPONOFF is not set" >>$(1); \
		sed -i "/CONFIG_TRUE/d" $(1); \
		echo "# CONFIG_TRUE is not set" >>$(1); \
	fi; \
	if [ "$(IPV6SUPP)" = "y" ]; then \
		sed -i "/CONFIG_FEATURE_IPV6/d" $(1); \
		echo "CONFIG_FEATURE_IPV6=y" >>$(1); \
		sed -i "/CONFIG_PING6/d" $(1); \
		echo "CONFIG_PING6=y" >>$(1); \
		sed -i "/CONFIG_TRACEROUTE6/d" $(1); \
		echo "CONFIG_TRACEROUTE6=y" >>$(1); \
	fi; \
	if [ "$(SNMPD)" = "y" ]; then \
		sed -i "/CONFIG_TFTP/d" $(1); \
		echo "CONFIG_TFTP=y" >>$(1); \
		sed -i "/CONFIG_TFTPD/d" $(1); \
		echo "# CONFIG_TFTPD is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_TFTP_GET/d" $(1); \
		echo "CONFIG_FEATURE_TFTP_GET=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TFTP_PUT/d" $(1); \
		echo "CONFIG_FEATURE_TFTP_PUT=y" >>$(1); \
		sed -i "/CONFIG_TFTP_DEBUG/d" $(1); \
		echo "# CONFIG_TFTP_DEBUG is not set" >>$(1); \
	fi; \
	if [ "$(RTN11P)" = "y" ]; then \
		sed -i "/CONFIG_LESS/d" $(1); \
		echo "# CONFIG_LESS is not set" >>$(1); \
		sed -i "/CONFIG_DU\b/d" $(1); \
		echo "# CONFIG_DU is not set" >>$(1); \
		sed -i "/CONFIG_HEAD/d" $(1); \
		echo "# CONFIG_HEAD is not set" >>$(1); \
		sed -i "/CONFIG_TAIL/d" $(1); \
		echo "# CONFIG_TAIL is not set" >>$(1); \
		sed -i "/CONFIG_BASENAME/d" $(1); \
		echo "# CONFIG_BASENAME is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_DEVFS/d" $(1); \
		echo "# CONFIG_FEATURE_DEVFS is not set" >>$(1); \
		sed -i "/CONFIG_BLKID/d" $(1); \
		echo "# CONFIG_BLKID is not set" >>$(1); \
		sed -i "/CONFIG_TELNET\b/d" $(1); \
		echo "# CONFIG_TELNET is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_LS_COLOR\b/d" $(1); \
		echo "# CONFIG_FEATURE_LS_COLOR is not set" >>$(1); \
		sed -i "/CONFIG_CUT/d" $(1); \
		echo "# CONFIG_CUT is not set" >>$(1); \
		sed -i "/CONFIG_CROND/d" $(1); \
		echo "# CONFIG_CROND is not set" >>$(1); \
		sed -i "/CONFIG_MD5SUM/d" $(1); \
		echo "# CONFIG_MD5SUM is not set" >>$(1); \
		sed -i "/CONFIG_AWK/d" $(1); \
		echo "# CONFIG_AWK is not set" >>$(1); \
		sed -i "/CONFIG_WC/d" $(1); \
		echo "# CONFIG_WC is not set" >>$(1); \
	fi; \
	if [ "$(SLIM)" = "y" ]; then \
		sed -i "/CONFIG_AWK/d" $(1); \
		echo "# CONFIG_AWK is not set" >>$(1); \
		sed -i "/CONFIG_BASENAME/d" $(1); \
		echo "# CONFIG_BASENAME is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_DEVFS/d" $(1); \
		echo "# CONFIG_FEATURE_DEVFS is not set" >>$(1); \
		sed -i "/CONFIG_BLKID/d" $(1); \
		echo "# CONFIG_BLKID is not set" >>$(1); \
		sed -i "/CONFIG_TELNET\b/d" $(1); \
		echo "# CONFIG_TELNET is not set" >>$(1); \
		sed -i "/CONFIG_ARPING/d" $(1); \
		echo "# CONFIG_ARPING is not set" >>$(1); \
		sed -i "/CONFIG_FEATURE_LS_COLOR/d" $(1); \
		echo "# CONFIG_FEATURE_LS_COLOR is not set" >>$(1); \
	else \
		if [ "$(SFP)" = "y" ]; then \
			sed -i "/CONFIG_LESS/d" $(1); \
			echo "# CONFIG_LESS is not set" >>$(1); \
			sed -i "/CONFIG_GZIP/d" $(1); \
			echo "# CONFIG_GZIP is not set" >>$(1); \
			sed -i "/CONFIG_DU\b/d" $(1); \
			echo "# CONFIG_DU is not set" >>$(1); \
			sed -i "/CONFIG_HEAD/d" $(1); \
			echo "# CONFIG_HEAD is not set" >>$(1); \
			sed -i "/CONFIG_TAIL/d" $(1); \
			echo "# CONFIG_TAIL is not set" >>$(1); \
			sed -i "/CONFIG_BASENAME/d" $(1); \
			echo "# CONFIG_BASENAME is not set" >>$(1); \
			sed -i "/CONFIG_FEATURE_DEVFS/d" $(1); \
			echo "# CONFIG_FEATURE_DEVFS is not set" >>$(1); \
			sed -i "/CONFIG_BLKID/d" $(1); \
			echo "# CONFIG_BLKID is not set" >>$(1); \
			sed -i "/CONFIG_TELNET\b/d" $(1); \
			echo "# CONFIG_TELNET is not set" >>$(1); \
			sed -i "/CONFIG_ARPING/d" $(1); \
			echo "# CONFIG_ARPING is not set" >>$(1); \
			sed -i "/CONFIG_FEATURE_LS_COLOR\b/d" $(1); \
			echo "# CONFIG_FEATURE_LS_COLOR is not set" >>$(1); \
			if [ "$(SFP4M)" = "y" ]; then \
				sed -i "/CONFIG_TAR/d" $(1); \
				echo "# CONFIG_TAR is not set" >>$(1); \
				sed -i "/CONFIG_DD/d" $(1); \
				echo "# CONFIG_DD is not set" >>$(1); \
				sed -i "/CONFIG_CUT/d" $(1); \
				echo "# CONFIG_CUT is not set" >>$(1); \
				sed -i "/CONFIG_SORT/d" $(1); \
				echo "# CONFIG_SORT is not set" >>$(1); \
				sed -i "/CONFIG_DMESG/d" $(1); \
				echo "# CONFIG_DMESG is not set" >>$(1); \
				sed -i "/CONFIG_CROND/d" $(1); \
				echo "# CONFIG_CROND is not set" >>$(1); \
				sed -i "/CONFIG_EXPR_MATH_SUPPORT_64/d" $(1); \
				echo "# CONFIG_EXPR_MATH_SUPPORT_64 is not set" >>$(1); \
				sed -i "/CONFIG_HEAD/d" $(1); \
				echo "# CONFIG_HEAD is not set" >>$(1); \
				sed -i "/CONFIG_MD5SUM/d" $(1); \
				echo "# CONFIG_MD5SUM is not set" >>$(1); \
				sed -i "/CONFIG_TAIL/d" $(1); \
				echo "# CONFIG_TAIL is not set" >>$(1); \
				sed -i "/CONFIG_AWK/d" $(1); \
				echo "# CONFIG_AWK is not set" >>$(1); \
				sed -i "/CONFIG_WC/d" $(1); \
				echo "# CONFIG_WC is not set" >>$(1); \
				sed -i "/CONFIG_VI/d" $(1); \
				echo "# CONFIG_VI is not set" >>$(1); \
				sed -i "/CONFIG_FIND/d" $(1); \
				echo "# CONFIG_FIND is not set" >>$(1); \
				echo "# CONFIG_FINDFS is not set" >>$(1); \
			fi; \
		else \
			sed -i "/CONFIG_FEATURE_LS_COLOR\b/d" $(1); \
			echo "CONFIG_FEATURE_LS_COLOR=y" >>$(1); \
			sed -i "/CONFIG_SENDMAIL/d" $(1); \
			echo "CONFIG_SENDMAIL=y" >>$(1); \
		fi; \
	fi; \
	if [ "$(DISKTEST)" = "y" ]; then \
		sed -i "/CONFIG_HDPARM/d" $(1); \
		echo "CONFIG_HDPARM=y" >>$(1); \
	fi; \
	if [ "$(BCMSMP)" = "y" ]; then \
		sed -i "/CONFIG_FEATURE_TOP_SMP_CPU/d" $(1); \
		echo "CONFIG_FEATURE_TOP_SMP_CPU=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TOP_DECIMALS/d" $(1); \
		echo "CONFIG_FEATURE_TOP_DECIMALS=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TOP_SMP_PROCESS/d" $(1); \
		echo "CONFIG_FEATURE_TOP_SMP_PROCESS=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TOPMEM/d" $(1); \
		echo "CONFIG_FEATURE_TOPMEM=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_SHOW_THREADS/d" $(1); \
		echo "CONFIG_FEATURE_SHOW_THREADS=y" >>$(1); \
	fi; \
	if [ "$(WANRED_LED)" = "y" ]; then \
		sed -i "/CONFIG_ARPING/d" $(1); \
		echo "CONFIG_ARPING=y" >>$(1); \
	fi; \
	if [ "$(TR069)" = "y" ]; then \
		if [ "$(TR181)" = "y" ]; then \
			sed -i "/CONFIG_FEATURE_UDHCP_VIVSO/d" $(1); \
			echo "CONFIG_FEATURE_UDHCP_VIVSO=y" >>$(1); \
		fi; \
	fi; \
	)
	$(call platformBusyboxOptions, $(1))
endef

define extraKernelConfig
	@( \
	if [ ! -z "$(EXTRA_KERNEL_YES_CONFIGS)" ] ; then \
		for c in $(EXTRA_KERNEL_YES_CONFIGS) ; do \
			sed -i "/CONFIG_$${c}/d" $(1); \
			echo "CONFIG_$${c}=y" >>$(1); \
		done \
	fi; \
	if [ ! -z "$(EXTRA_KERNEL_NO_CONFIGS)" ] ; then \
		for c in $(EXTRA_KERNEL_NO_CONFIGS) ; do \
			sed -i "/CONFIG_$${c}/d" $(1); \
			echo "# CONFIG_$${c} is not set" >>$(1); \
		done \
	fi; \
	if [ ! -z "$(EXTRA_KERNEL_MOD_CONFIGS)" ] ; then \
		for c in $(EXTRA_KERNEL_MOD_CONFIGS) ; do \
			sed -i "/CONFIG_$${c}/d" $(1); \
			echo "CONFIG_$${c}=m" >>$(1); \
		done \
	fi; \
	if [ ! -z "$(EXTRA_KERNEL_VAL_CONFIGS)" ] ; then \
		for c in $(EXTRA_KERNEL_VAL_CONFIGS) ; do \
			sed -i "/CONFIG_$${c}/d" $(1); \
			echo "CONFIG_$${c}" >>$(1); \
		done \
	fi; \
	)
endef

define KernelConfig
	@( \
	if [ "$(TUNEK)" != "n" ]; then \
	sed -i "/CONFIG_NVRAM_SIZE/d" $(1); \
	echo "CONFIG_NVRAM_SIZE="$(NVRAM_SIZE) >>$(1); \
	sed -i "/CONFIG_CC_OPTIMIZE_FOR_SIZE/d" $(1); \
	if [ "$(KERN_SIZE_OPT)" = "y" ]; then \
		echo "CONFIG_CC_OPTIMIZE_FOR_SIZE=y" >>$(1); \
	else \
		echo "# CONFIG_CC_OPTIMIZE_FOR_SIZE is not set" >>$(1); \
	fi; \
	if [ "$(CONFIG_LINUX26)" = "y" ] && [ "$(MIPS32)" = "r2" ]; then \
		sed -i "/CONFIG_CPU_MIPS32_R1/d" $(1); \
		echo "# CONFIG_CPU_MIPS32_R1 is not set" >>$(1); \
		sed -i "/CONFIG_CPU_MIPS32_R2/d" $(1); \
		echo "CONFIG_CPU_MIPS32_R2=y" >>$(1); \
		sed -i "/CONFIG_CPU_MIPSR1/d" $(1); \
		echo "CONFIG_CPU_MIPSR2=y" >>$(1); \
	fi; \
	if [ "$(RTN11P)" = "y" ]; then \
		sed -i "/CONFIG_USB/d" $(1); \
		echo "# CONFIG_USB is not set" >>$(1); \
		sed -i "/CONFIG_USB_SUPPORT/d" $(1); \
		echo "# CONFIG_USB_SUPPORT is not set" >>$(1); \
		sed -i "/CONFIG_USB_ARCH_HAS_OHCI/d" $(1); \
		echo "# CONFIG_USB_ARCH_HAS_OHCI is not set" >>$(1); \
		sed -i "/CONFIG_USB_ARCH_HAS_EHCI/d" $(1); \
		echo "# CONFIG_USB_ARCH_HAS_EHCI is not set" >>$(1); \
		sed -i "/CONFIG_SWAP/d" $(1); \
		echo "# CONFIG_SWAP is not set" >>$(1); \
		sed -i "/CONFIG_RD_GZIP/d" $(1); \
		echo "# CONFIG_RD_GZIP is not set" >>$(1); \
		sed -i "/CONFIG_SCSI/d" $(1); \
		echo "# CONFIG_SCSI is not set" >>$(1); \
		sed -i "/CONFIG_EXT2_FS/d" $(1); \
		echo "# CONFIG_EXT2_FS is not set" >>$(1); \
		sed -i "/CONFIG_EXT3_FS/d" $(1); \
		echo "# CONFIG_EXT3_FS is not set" >>$(1); \
		sed -i "/CONFIG_FAT_FS/d" $(1); \
		echo "# CONFIG_FAT_FS is not set" >>$(1); \
		sed -i "/CONFIG_VFAT_FS/d" $(1); \
		echo "# CONFIG_VFAT_FS is not set" >>$(1); \
		sed -i "/CONFIG_REISERFS_FS/d" $(1); \
		echo "# CONFIG_REISERFS_FS is not set" >>$(1); \
		sed -i "/CONFIG_JFFS2_FS/d" $(1); \
		echo "# CONFIG_JFFS2_FS is not set" >>$(1); \
		sed -i "/CONFIG_FUSE_FS/d" $(1); \
		echo "# CONFIG_FUSE_FS is not set" >>$(1); \
		sed -i "/CONFIG_CONFIGFS_FS/d" $(1); \
		echo "# CONFIG_CONFIGFS_FS is not set" >>$(1); \
		sed -i "/CONFIG_SERIAL_NONSTANDARD/d" $(1); \
		echo "# CONFIG_SERIAL_NONSTANDARD is not set" >>$(1); \
		sed -i "/CONFIG_NETWORK_FILESYSTEMS/d" $(1); \
		echo "# CONFIG_NETWORK_FILESYSTEMS is not set" >>$(1); \
		sed -i "/CONFIG_CC_OPTIMIZE_FOR_SIZE/d" $(1); \
		echo "CONFIG_CC_OPTIMIZE_FOR_SIZE=y" >>$(1); \
		sed -i "/CONFIG_KALLSYMS/d" $(1); \
		echo "# CONFIG_KALLSYMS is not set" >>$(1); \
		sed -i "/CONFIG_RALINK_TIMER/d" $(1); \
		echo "# CONFIG_RALINK_TIMER is not set" >>$(1); \
		sed -i "/CONFIG_BUG/d" $(1); \
		echo "# CONFIG_BUG is not set" >>$(1); \
	fi; \
	if [ "$(USB)" = "" ]; then \
		sed -i "/CONFIG_MSDOS_PARTITION/d" $(1); \
		echo "# CONFIG_MSDOS_PARTITION is not set" >>$(1); \
		sed -i "/CONFIG_EFI_PARTITION/d" $(1); \
		echo "# CONFIG_EFI_PARTITION is not set" >>$(1); \
	elif [ "$(MODEM)" = "y" ]; then \
		if [ "$(BECEEM)" = "y" ]; then \
			sed -i "/CONFIG_USB_BECEEM/d" $(1); \
			echo "CONFIG_USB_BECEEM=m" >>$(1); \
		fi; \
		if [ "$(LESSMODEM)" = "y" ]; then \
			sed -i "/CONFIG_HSO/d" $(1); \
			echo "# CONFIG_HSO is not set" >>$(1); \
			sed -i "/CONFIG_USB_IPHETH/d" $(1); \
			echo "# CONFIG_USB_IPHETH is not set" >>$(1); \
		fi; \
	else \
		sed -i "/CONFIG_USB_SERIAL/d" $(1); \
		echo "# CONFIG_USB_SERIAL is not set" >>$(1); \
		sed -i "/CONFIG_USB_ACM/d" $(1); \
		echo "# CONFIG_USB_ACM is not set" >>$(1); \
		sed -i "/CONFIG_USB_USBNET/d" $(1); \
		echo "# CONFIG_USB_USBNET is not set" >>$(1); \
		sed -i "/CONFIG_USB_WDM/d" $(1); \
		echo "# CONFIG_USB_WDM is not set" >>$(1); \
		sed -i "/CONFIG_USB_IPHETH/d" $(1); \
		echo "# CONFIG_USB_IPHETH is not set" >>$(1); \
	fi; \
	if [ "$(AP_CARRIER_DETECTION)" = "y" ]; then \
		sed -i "/CONFIG_RALINK_TIMER_DFS/d" $(1); \
		echo "CONFIG_RALINK_TIMER_DFS=y" >>$(1); \
		sed -i "/CONFIG_RT2860V2_AP_DFS/d" $(1); \
		echo "CONFIG_RT2860V2_AP_DFS=y" >>$(1); \
		sed -i "/CONFIG_RT2860V2_AP_CARRIER/d" $(1); \
		echo "CONFIG_RT2860V2_AP_CARRIER=y" >>$(1); \
		sed -i "/CONFIG_RTPCI_AP_CARRIER/d" $(1); \
		echo "CONFIG_RTPCI_AP_CARRIER=y" >>$(1); \
	else \
		sed -i "/CONFIG_RALINK_TIMER_DFS/d" $(1); \
		echo "# CONFIG_RALINK_TIMER_DFS is not set" >>$(1); \
		sed -i "/CONFIG_RT2860V2_AP_DFS/d" $(1); \
		echo "# CONFIG_RT2860V2_AP_DFS is not set" >>$(1); \
		sed -i "/CONFIG_RT2860V2_AP_CARRIER/d" $(1); \
		echo "# CONFIG_RT2860V2_AP_CARRIER is not set" >>$(1); \
		sed -i "/CONFIG_RTPCI_AP_CARRIER/d" $(1); \
		echo "# CONFIG_RTPCI_AP_CARRIER is not set" >>$(1); \
	fi; \
	if [ "$(CONFIG_LINUX30)" = "y" ]; then \
		sed -i "/CONFIG_USB_XHCI_HCD/d" $(1); \
		echo "# CONFIG_USB_XHCI_HCD is not set" >>$(1); \
		if [ "$(USB)" = "USB" ]; then \
			if [ "$(XHCI)" = "y" ]; then \
				sed -i "/CONFIG_USB_XHCI_HCD/d" $(1); \
				echo "CONFIG_USB_XHCI_HCD=y" >>$(1); \
				sed -i "/CONFIG_USB_XHCI_HCD_DEBUGGING/d" $(1); \
				echo "# CONFIG_USB_XHCI_HCD_DEBUGGING is not set" >>$(1); \
			fi; \
		fi; \
	fi; \
	if [ "$(SHP)" = "y" ] || [ "$(LFP)" = "y" ]; then \
		sed -i "/CONFIG_IP_NF_LFP/d" $(1); \
		echo "CONFIG_IP_NF_LFP=y" >>$(1); \
 	fi; \
	if [ "$(DNSMQ)" = "y" ]; then \
		sed -i "/CONFIG_IP_NF_DNSMQ/d" $(1); \
		echo "CONFIG_IP_NF_DNSMQ=y" >>$(1); \
	fi; \
	if [ "$(IPV6SUPP)" = "y" ]; then \
		sed -i "/CONFIG_IPV6 is not set/d" $(1); \
		echo "CONFIG_IPV6=y" >>$(1); \
		sed -i "/CONFIG_IP6_NF_IPTABLES/d" $(1); \
		echo "CONFIG_IP6_NF_IPTABLES=y" >>$(1); \
		sed -i "/CONFIG_IP6_NF_MATCH_RT/d" $(1); \
		echo "CONFIG_IP6_NF_MATCH_RT=y" >>$(1); \
		sed -i "/CONFIG_IP6_NF_FILTER/d" $(1); \
		echo "CONFIG_IP6_NF_FILTER=m" >>$(1); \
		sed -i "/CONFIG_IP6_NF_TARGET_LOG/d" $(1); \
		echo "CONFIG_IP6_NF_TARGET_LOG=m" >>$(1); \
		if [ "$(CONFIG_BCMWL5)" = "y" ] && [ "$(ARM)" != "y" ]; then \
			sed -i "/CONFIG_IP6_NF_TARGET_SKIPLOG/d" $(1); \
			echo "CONFIG_IP6_NF_TARGET_SKIPLOG=m" >>$(1); \
		fi; \
		sed -i "/CONFIG_IP6_NF_TARGET_REJECT/d" $(1); \
		echo "CONFIG_IP6_NF_TARGET_REJECT=m" >>$(1); \
		sed -i "/CONFIG_IP6_NF_MANGLE/d" $(1); \
		echo "CONFIG_IP6_NF_MANGLE=m" >>$(1); \
		if [ "$(CONFIG_LINUX26)" = "y" ]; then \
			sed -i "/CONFIG_NF_CONNTRACK_IPV6/d" $(1); \
			echo "CONFIG_NF_CONNTRACK_IPV6=y" >>$(1); \
			sed -i "/CONFIG_IPV6_ROUTER_PREF/d" $(1); \
			echo "CONFIG_IPV6_ROUTER_PREF=y" >>$(1); \
			sed -i "/CONFIG_IPV6_SIT\b/d" $(1); \
			echo "CONFIG_IPV6_SIT=m" >>$(1); \
			sed -i "/CONFIG_IPV6_SIT_6RD/d" $(1); \
			echo "CONFIG_IPV6_SIT_6RD=y" >>$(1); \
			sed -i "/CONFIG_IPV6_MULTIPLE_TABLES/d" $(1); \
			echo "CONFIG_IPV6_MULTIPLE_TABLES=y" >>$(1); \
			sed -i "/CONFIG_IP6_NF_TARGET_ROUTE/d" $(1); \
			echo "CONFIG_IP6_NF_TARGET_ROUTE=m" >>$(1); \
			sed -i "/CONFIG_IPV6_MROUTE\b/d" $(1); \
			echo "CONFIG_IPV6_MROUTE=y" >>$(1); \
		fi; \
		if [ "$(CONFIG_LINUX30)" = "y" ]; then \
			sed -i "/CONFIG_IP6_NF_CONNTRACK/d" $(1); \
			echo "CONFIG_IP6_NF_CONNTRACK=m" >>$(1); \
			sed -i "/CONFIG_IPV6_ROUTER_PREF/d" $(1); \
			echo "CONFIG_IPV6_ROUTER_PREF=y" >>$(1); \
			sed -i "/CONFIG_IPV6_SIT\b/d" $(1); \
			echo "CONFIG_IPV6_SIT=m" >>$(1); \
			sed -i "/CONFIG_IPV6_SIT_6RD/d" $(1); \
			echo "CONFIG_IPV6_SIT_6RD=y" >>$(1); \
			sed -i "/CONFIG_IPV6_MULTIPLE_TABLES/d" $(1); \
			echo "CONFIG_IPV6_MULTIPLE_TABLES=y" >>$(1); \
			sed -i "/CONFIG_IP6_NF_FTP/d" $(1); \
			echo "CONFIG_IP6_NF_FTP=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_LIMIT/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_LIMIT=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_CONDITION/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_CONDITION=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_MAC/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_MAC=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_MULTIPORT/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_MULTIPORT=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_MARK/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_MARK=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_LENGTH/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_LENGTH=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_MATCH_STATE/d" $(1); \
			echo "CONFIG_IP6_NF_MATCH_STATE=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_TARGET_MARK/d" $(1); \
			echo "CONFIG_IP6_NF_TARGET_MARK=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_TARGET_TCPMSS/d" $(1); \
			echo "CONFIG_IP6_NF_TARGET_TCPMSS=m" >>$(1); \
			sed -i "/CONFIG_IP6_NF_TARGET_ROUTE/d" $(1); \
			echo "CONFIG_IP6_NF_TARGET_ROUTE=m" >>$(1); \
			sed -i "/CONFIG_IPV6_MROUTE\b/d" $(1); \
			echo "CONFIG_IPV6_MROUTE=y" >>$(1); \
		fi; \
	else \
		sed -i "/CONFIG_IPV6/d" $(1); \
		echo "# CONFIG_IPV6 is not set" >>$(1); \
		sed -i "/CONFIG_RA_HW_NAT_IPV6/d" $(1); \
		echo "# CONFIG_RA_HW_NAT_IPV6 is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_BCM57XX/d" $(1); \
	if [ "$(BCM57)" = "y" ]; then \
		sed -i "/CONFIG_ET_ALL_PASSIVE/d" $(1); \
		echo "CONFIG_BCM57XX=m" >>$(1); \
		echo "# CONFIG_ET_ALL_PASSIVE_ON is not set" >>$(1); \
		echo "CONFIG_ET_ALL_PASSIVE_RUNTIME=y" >>$(1); \
	else \
		echo "# CONFIG_BCM57XX is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_WL_USE_HIGH/d" $(1); \
	sed -i "/CONFIG_WL_USBAP/d" $(1); \
	if [ "$(USBAP)" = "y" ]; then \
		echo "CONFIG_WL_USE_HIGH=y" >> $(1); \
		echo "CONFIG_WL_USBAP=y" >>$(1); \
	else \
		echo "# CONFIG_WL_USE_HIGH is not set" >> $(1); \
		echo "# CONFIG_WL_USBAP is not set" >>$(1); \
	fi; \
	if [ "$(CONFIG_LINUX26)" = "y" ] && [ "$(EBTABLES)" = "y" ]; then \
		sed -i "/CONFIG_BRIDGE_NF_EBTABLES/d" $(1); \
		echo "CONFIG_BRIDGE_NF_EBTABLES=m" >>$(1); \
		if [ "$(IPV6SUPP)" = "y" ]; then \
			sed -i "/CONFIG_BRIDGE_EBT_IP6/d" $(1); \
			echo "CONFIG_BRIDGE_EBT_IP6=m" >>$(1); \
		fi; \
	fi; \
	sed -i "/CONFIG_NVRAM_64K/d" $(1); \
	if [ "$(NVRAM_64K)" = "y" ]; then \
		echo "CONFIG_NVRAM_64K=y" >>$(1); \
	else \
		echo "# CONFIG_NVRAM_64K is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_LOCALE2012/d" $(1); \
	if [ "$(LOCALE2012)" = "y" ]; then \
		echo "CONFIG_LOCALE2012=y" >>$(1); \
	else \
		echo "# CONFIG_LOCALE2012 is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_N56U_SR2/d" $(1); \
	if [ "$(N56U_SR2)" = "y" ]; then \
		echo "CONFIG_N56U_SR2=y" >>$(1); \
	else \
		echo "# CONFIG_N56U_SR2 is not set" >>$(1); \
	fi; \
	if [ "$(EXT4FS)" = "y" ]; then \
		sed -i "/CONFIG_EXT4_FS/d" $(1); \
		echo "CONFIG_EXT4_FS=m" >>$(1); \
		sed -i "/CONFIG_EXT4_FS_XATTR/d" $(1); \
		echo "CONFIG_EXT4_FS_XATTR=y" >>$(1); \
		sed -i "/CONFIG_EXT4_FS_POSIX_ACL/d" $(1); \
		echo "# CONFIG_EXT4_FS_POSIX_ACL is not set" >>$(1); \
		sed -i "/CONFIG_EXT4_FS_SECURITY/d" $(1); \
		echo "# CONFIG_EXT4_FS_SECURITY is not set" >>$(1); \
		sed -i "/CONFIG_EXT4_DEBUG/d" $(1); \
		echo "# CONFIG_EXT4_DEBUG is not set" >>$(1); \
	else \
		sed -i "/CONFIG_EXT4_FS/d" $(1); \
		echo "# CONFIG_EXT4_FS is not set" >>$(1); \
	fi; \
	fi; \
	if [ "$(ARMCPUSMP)" = "up" ]; then \
		sed -i "/CONFIG_GENERIC_CLOCKEVENTS_BROADCAST/d" $(1); \
		echo "CONFIG_HAVE_LATENCYTOP_SUPPORT=y" >>$(1); \
		sed -i "/CONFIG_GENERIC_LOCKBREAK/d" $(1); \
		echo "CONFIG_BROKEN_ON_SMP=y" >>$(1); \
		sed -i "/CONFIG_TREE_RCU=y/# CONFIG_TREE_RCU is not set/g" >>$(1); \
		echo "CONFIG_TREE_PREEMPT_RCU=y" >>$(1); \
		echo "# CONFIG_TINY_RCU is not set" >>$(1); \
		sed -i "/CONFIG_USE_GENERIC_SMP_HELPERS/d" $(1); \
		sed -i "/CONFIG_STOP_MACHINE/d" $(1); \
		sed -i "/CONFIG_MUTEX_SPIN_ON_OWNER/d" $(1); \
		echo "# CONFIG_MUTEX_SPIN_ON_OWNER is not set" >>$(1); \
		sed -i "/# CONFIG_ARM_ERRATA_742230 is not set/d" $(1); \
		sed -i "/# CONFIG_ARM_ERRATA_742231 is not set/d" $(1); \
		sed -i "/# CONFIG_ARM_ERRATA_720789 is not set/d" $(1); \
		sed -i "/CONFIG_SMP=y/d" $(1); \
		echo "# CONFIG_SMP is not set" >>$(1); \
		sed -i "/CONFIG_NR_CPUS=2/d" $(1); \
		sed -i "/# CONFIG_HOTPLUG_CPU is not set/d" $(1); \
		sed -i "/CONFIG_RPS=y/d" $(1); \
	fi; \
	if [ "$(DUALWAN)" = "y" ]; then \
		if [ "$(BCMWL6A)" = "y" ]; then \
			sed -i "/CONFIG_IP_ADVANCED_ROUTER/d" $(1); \
			echo "CONFIG_IP_ADVANCED_ROUTER=y" >>$(1); \
			echo "CONFIG_ASK_IP_FIB_HASH=y" >>$(1); \
			echo "# CONFIG_IP_FIB_TRIE is not set" >>$(1); \
			echo "CONFIG_IP_MULTIPLE_TABLES=y" >>$(1); \
			echo "CONFIG_IP_ROUTE_MULTIPATH=y" >>$(1); \
			echo "# CONFIG_IP_ROUTE_VERBOSE is not set" >>$(1); \
			echo "CONFIG_IP_MROUTE_MULTIPLE_TABLES=y" >>$(1); \
		fi; \
	fi; \
	if [ "$(IPSEC)" = "y" ]; then \
		sed -i "/CONFIG_XFRM_USER/d" $(1); \
		echo "CONFIG_XFRM_USER=m" >>$(1); \
		echo "# CONFIG_XFRM_SUB_POLICY is not set" >>$(1); \
		echo "# CONFIG_XFRM_MIGRATE is not set" >>$(1); \
		echo "# CONFIG_XFRM_STATISTICS is not set" >>$(1); \
		sed -i "/CONFIG_NET_KEY/d" $(1); \
		echo "CONFIG_NET_KEY=m" >>$(1); \
		echo "# CONFIG_NET_KEY_MIGRATE is not set" >>$(1); \
		sed -i "/CONFIG_INET_AH/d" $(1); \
		echo "CONFIG_INET_AH=m" >>$(1); \
		sed -i "/CONFIG_INET_ESP/d" $(1); \
		echo "CONFIG_INET_ESP=m" >>$(1); \
		sed -i "/CONFIG_INET_IPCOMP/d" $(1); \
		echo "CONFIG_INET_IPCOMP=m" >>$(1); \
		sed -i "/CONFIG_INET_XFRM_MODE_TRANSPORT/d" $(1); \
		echo "CONFIG_INET_XFRM_MODE_TRANSPORT=m" >>$(1); \
		sed -i "/CONFIG_INET_XFRM_MODE_TUNNEL/d" $(1); \
		echo "CONFIG_INET_XFRM_MODE_TUNNEL=m" >>$(1); \
		sed -i "/CONFIG_INET_XFRM_MODE_BEET/d" $(1); \
		echo "CONFIG_INET_XFRM_MODE_BEET=m" >>$(1); \
		sed -i "/CONFIG_NETFILTER_XT_MATCH_POLICY/d" $(1); \
		echo "CONFIG_NETFILTER_XT_MATCH_POLICY=m" >>$(1); \
		if [ "$(IPV6SUPP)" = "y" ]; then \
			sed -i "/CONFIG_INET6_AH/d" $(1); \
			echo "CONFIG_INET6_AH=m" >>$(1); \
			sed -i "/CONFIG_INET6_ESP/d" $(1); \
			echo "CONFIG_INET6_ESP=m" >>$(1); \
			sed -i "/CONFIG_INET6_IPCOMP/d" $(1); \
			echo "CONFIG_INET6_IPCOMP=m" >>$(1); \
			sed -i "/CONFIG_INET6_XFRM_MODE_TRANSPORT/d" $(1); \
			echo "CONFIG_INET6_XFRM_MODE_TRANSPORT=m" >>$(1); \
			sed -i "/CONFIG_INET6_XFRM_MODE_TUNNEL/d" $(1); \
			echo "CONFIG_INET6_XFRM_MODE_TUNNEL=m" >>$(1); \
			sed -i "/CONFIG_INET6_XFRM_MODE_BEET/d" $(1); \
			echo "CONFIG_INET6_XFRM_MODE_BEET=m" >>$(1); \
		fi; \
	fi; \
	sed -i "/CONFIG_CFE_NVRAM_CHK/d" $(1); \
	if [ "$(NVRAM_64K)" = "y" ]; then \
		echo "CONFIG_CFE_NVRAM_CHK=y" >>$(1); \
	else \
		echo "# CONFIG_CFE_NVRAM_CHK is not set" >>$(1); \
	fi; \
	if [ "$(DEBUG)" = "y" ] || [ "$(GDB)" = "y" ]; then \
		sed -i "/CONFIG_ELF_CORE/d" $(1); \
		echo "CONFIG_ELF_CORE=y" >>$(1); \
		if [ "$(BCMWL6A)" = "y" ]; then \
			sed -i "/CONFIG_CORE_DUMP_DEFAULT_ELF_HEADERS/d" $(1); \
			echo "# CONFIG_CORE_DUMP_DEFAULT_ELF_HEADERS is not set" >>$(1); \
		fi; \
	fi; \
	sed -i "/CONFIG_DUAL_TRX/d" $(1); \
	if [ "$(DUAL_TRX)" = "y" ]; then \
		echo "CONFIG_DUAL_TRX=y" >>$(1); \
	else \
		echo "# CONFIG_DUAL_TRX is not set" >>$(1); \
	fi; \
	if [ "$(DUMP_OOPS_MSG)" = "y" ]; then \
		if [ "$(BUILD_NAME)" = "RT-AC66U" ] || [ "$(BUILD_NAME)" = "RT-N66U" ]; then \
			echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0x07FFE000" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
		elif [ "$(BUILD_NAME)" = "RT-N65U" ]; then \
			echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0x01810000" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
		elif [ "$(BUILD_NAME)" = "RT-AC68U" ] || [ "$(BUILD_NAME)" = "TM-AC1900" ] || [ "$(BUILD_NAME)" = "RT-AC3200" ]; then \
			echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0xC0000000" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
		elif [ "$(RALINK)" = "y" -o "$(QCA)" = "y" ]; then \
			echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0x03300000" >>$(1); \
			echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
		else \
			echo "# CONFIG_DUMP_PREV_OOPS_MSG is not set" >>$(1); \
		fi; \
	else \
		echo "# CONFIG_DUMP_PREV_OOPS_MSG is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_RTAC3200/d" $(1); \
	if [ "$(RTAC3200)" = "y" ]; then \
		echo "CONFIG_RTAC3200=y" >>$(1); \
	else \
		echo "# CONFIG_RTAC3200 is not set" >>$(1); \
	fi; \
	if [ "$(NOWL)" = "y" ]; then \
		sed -i "/CONFIG_WL=m/d" $(1); \
		echo "# CONFIG_WL is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_APSTA/d" $(1); \
		echo "# CONFIG_WL_USE_APSTA is not set" >>$(1); \
		sed -i "/CONFIG_WL_ALL_PASSIVE_RUNTIME/d" $(1); \
		sed -i "/CONFIG_WAPI/d" $(1); \
		echo "# CONFIG_WAPI is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_AP/d" $(1); \
		echo "# CONFIG_WL_USE_AP is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_AP_SDSTD/d" $(1); \
		echo "# CONFIG_WL_USE_AP_SDSTD is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_STA/d" $(1); \
		echo "# CONFIG_WL_USE_STA is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_AP_ONCHIP_G/d" $(1); \
		echo "# CONFIG_WL_USE_AP_ONCHIP_G is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_STA_ONCHIP_G/d" $(1); \
		echo "# CONFIG_WL_USE_STA_ONCHIP_G is not set" >>$(1); \
		sed -i "/CONFIG_WL_USE_APSTA_ONCHIP_G/d" $(1); \
		echo "# CONFIG_WL_USE_APSTA_ONCHIP_G is not set" >>$(1); \
		sed -i "/CONFIG_WL_ALL_PASSIVE_ON/d" $(1); \
		echo "# CONFIG_WL_ALL_PASSIVE_ON is not set" >>$(1); \
		sed -i "/CONFIG_DPSTA/d" $(1); \
		echo "# CONFIG_DPSTA is not set" >>$(1); \
		sed -i "/CONFIG_PLC/d" $(1); \
		echo "# CONFIG_PLC is not set" >>$(1); \
	fi; \
	sed -i "/CONFIG_LINUX_MTD/d" $(1); \
	if [ "$(LINUX_MTD)" = "" ]; then \
		echo "CONFIG_LINUX_MTD=32" >>$(1); \
	else \
		echo "CONFIG_LINUX_MTD=$(LINUX_MTD)" >>$(1); \
	fi; \
	sed -i "/CONFIG_NF_CONNTRACK_EVENTS/d" $(1); \
	if [ "$(BWDPI)" = "y" ]; then \
		echo "CONFIG_NF_CONNTRACK_EVENTS=y" >>$(1); \
	else \
		echo "# CONFIG_NF_CONNTRACK_EVENTS is not set" >>$(1); \
	fi; \
	if [ "$(TFAT)" = "y" ]; then \
		sed -i "/CONFIG_MSDOS_FS/d" $(1); \
		echo "# CONFIG_MSDOS_FS is not set" >>$(1); \
		sed -i "/CONFIG_VFAT_FS/d" $(1); \
		echo "# CONFIG_VFAT_FS is not set" >>$(1); \
	fi; \
	if [ "$(HFS)" = "open" ]; then \
		sed -i "/CONFIG_HFS_FS/d" $(1); \
		echo "CONFIG_HFS_FS=y" >>$(1); \
		sed -i "/CONFIG_HFSPLUS_FS/d" $(1); \
		echo "CONFIG_HFSPLUS_FS=y" >>$(1); \
		sed -i "/CONFIG_MAC_PARTITION/d" $(1); \
		echo "CONFIG_MAC_PARTITION=y" >>$(1); \
	fi; \
	if [ "$(BLINK_LED)" = "y" ]; then \
		sed -i "/CONFIG_USB_BUS_STATS/d" $(1); \
		echo "CONFIG_USB_BUS_STATS=y" >>$(1); \
	fi; \
	if [ "$(DEBUGFS)" = "y" ]; then \
		sed -i "/CONFIG_DEBUG_FS/d" $(1); \
		echo "CONFIG_DEBUG_FS=y" >>$(1); \
		sed -i "/CONFIG_USB_MON/d" $(1); \
		echo "CONFIG_USB_MON=m" >>$(1); \
		if [ "$(ARM)" = "y" ]; then \
			sed -i "/CONFIG_GCOV_KERNEL/d" $(1); \
			echo "# CONFIG_GCOV_KERNEL is not set" >>$(1); \
			sed -i "/CONFIG_L2TP_DEBUGFS/d" $(1); \
			echo "# CONFIG_L2TP_DEBUGFS is not set" >>$(1); \
			sed -i "/CONFIG_JBD_DEBUG/d" $(1); \
			echo "# CONFIG_JBD_DEBUG is not set" >>$(1); \
			sed -i "/CONFIG_JBD2_DEBUG/d" $(1); \
			echo "# CONFIG_JBD2_DEBUG is not set" >>$(1); \
			sed -i "/CONFIG_LKDTM/d" $(1); \
			echo "# CONFIG_LKDTM is not set" >>$(1); \
			sed -i "/CONFIG_DYNAMIC_DEBUG/d" $(1); \
			echo "# CONFIG_DYNAMIC_DEBUG is not set" >>$(1); \
		fi; \
	fi; \
	if [ "$(BONDING)" = "y" ]; then \
		sed -i "/CONFIG_BONDING/d" $(1); \
		echo "CONFIG_BONDING=m" >>$(1); \
	fi; \
	if [ "$(BCM7)" = "y" ]; then \
		sed -i "/CONFIG_DPSTA/d" $(1); \
		echo "CONFIG_DPSTA=m" >>$(1); \
		sed -i "/CONFIG_DHDAP/d" $(1); \
		echo "CONFIG_DHDAP=m" >>$(1); \
	fi; \
	)
	$(call platformKernelConfig, $(1))
	$(call extraKernelConfig, $(1))
endef

mk-%:
	@$(MAKE) -C router $(shell echo $@ | sed s/mk-//)


bin:
ifeq ($(BUILD_NAME),)
	@echo $@" is not a valid target!"
	@false
endif

	@cp router/config_base router/config_$(lowercase_B)
	@cp router/busybox/config_base router/busybox/config_$(lowercase_B)
ifeq ($(BCMWL6A),y)
	@cp $(LINUXDIR)/config_base.6a $(LINUXDIR)/config_$(lowercase_B)
else
	@cp $(LINUXDIR)/config_base $(LINUXDIR)/config_$(lowercase_B)
	@if [ -f $(LINUXDIR)/config_base_$(lowercase_B) ]; then \
		cp $(LINUXDIR)/config_base_$(lowercase_B) $(LINUXDIR)/config_$(lowercase_B); \
	fi
endif
	$(call RouterOptions, router/config_$(lowercase_B))
	$(call KernelConfig, $(LINUXDIR)/config_$(lowercase_B))
	$(call BusyboxOptions, router/busybox/config_$(lowercase_B))

	@$(MAKE) setprofile
	@$(MAKE) all

define save_src_config
	@if [ -f .config ] ; then \
		if [ $(shell echo $(1) | grep -i "^RT4G-") ] ; then \
			NEW_BUILD_NAME=$(shell echo $(1) | tr a-z A-Z | sed 's/^RT//') ; \
		else \
			NEW_BUILD_NAME=$(shell echo $(1) | tr a-z A-Z) ; \
		fi ; \
		echo "CONFIGURED MODEL: $(BUILD_NAME)" ; \
		echo "SPECIFIED  MODEL: $${NEW_BUILD_NAME}" ; \
		echo "----------------------------------------------------------------------------" ; \
		if [ "$(BUILD_NAME)" != "$${NEW_BUILD_NAME}" ] ; then \
			echo "!!! MODEL NAME MISMATCH.  REMOVE .config AND MAKE AGAIN. !!!" ; \
			exit 1; \
		fi ; \
	fi ;
	@if [ -z '$($(shell echo $(1) | tr a-z A-Z))' ] ; then \
		echo NO THIS TARGET $(1) ; exit 1; \
	fi ;
	@if [ -f .config ] ; then \
			echo "Clean old model configuration"; \
			while read line ; do \
				var=`echo "$${line}"|sed -e "s,^export[       ]*,," -e "s,=.*$$,,"` ; \
				unset "$${var}" ; \
			done < .config; \
			echo "Update model configuration" ; \
			rm -f .config ; \
	fi ;
	@for var in $($(shell echo $(1) | tr a-z A-Z)) ; do \
		echo "export $${var}" >> .config ; \
		export $${var} ; \
	done ;
	@echo "";
endef

4g-% 4G-%:
	$(call save_src_config, RT$@)
	$(MAKE) bin

rt-% rp-% RT-% RP-% ea-% EA-% tm-% TM-%:
	$(call save_src_config, $@)
	$(MAKE) bin

dsl-% DSL-%:
	$(call dsl_genbintrx_prolog)
	$(call save_src_config, $@)
	@$(MAKE) bin
	$(call dsl_genbintrx_epilog)

setprofile:
	@echo ""
	@echo "Using $(N) profile, $(B) build config."
	@echo ""

	@cd $(LINUXDIR) ; \
		rm -f config_current ; \
		ln -s config_$(lowercase_B) config_current ; \
		cp -f config_current .config

	@cd router/busybox && \
		rm -f config_current ; \
		ln -s config_$(lowercase_B) config_current ; \
		cp config_current .config

	@cd router ; \
		rm -f config_current ; \
		ln -s config_$(lowercase_B) config_current ; \
		cp config_current .config

	@if grep -q "CONFIG_RT3352_INIC_MII=m" $(LINUXDIR)/.config ; then \
		sed -i "/RTCONFIG_WLMODULE_RT3352_INIC_MII/d" router/.config; \
		echo "RTCONFIG_WLMODULE_RT3352_INIC_MII=y" >> router/.config; \
	fi
	@if grep -q "CONFIG_RTPCI_AP=m" $(LINUXDIR)/.config ; then \
		sed -i "/RTCONFIG_WLMODULE_RT3090_AP/d" router/.config; \
		echo "RTCONFIG_WLMODULE_RT3090_AP=y" >> router/.config; \
	fi
	@if grep -q "CONFIG_MT7610_AP=m" $(LINUXDIR)/.config ; then \
		sed -i "/RTCONFIG_WLMODULE_MT7610_AP/d" router/.config; \
		echo "RTCONFIG_WLMODULE_MT7610_AP=y" >> router/.config; \
	fi

	@if grep -q "CONFIG_RLT_WIFI=m" $(LINUXDIR)/.config ; then \
		sed -i "/RTCONFIG_WLMODULE_RLT_WIFI/d" router/.config; \
		echo "RTCONFIG_WLMODULE_RLT_WIFI=y" >> router/.config; \
	fi
	@if grep -q "CONFIG_WIFI_MT7603E=m" $(LINUXDIR)/.config ; then \
		sed -i "/RTCONFIG_WLMODULE_MT7603E_AP/d" router/.config; \
		echo "RTCONFIG_WLMODULE_MT7603E_AP=y" >> router/.config; \
	fi

	@$(MAKE) -C router oldconfig

cleanlibc:
#	@$(MAKE) -C ../../tools-src/uClibc clean

libc: cleanlibc
#	@$(MAKE) -C ../../tools-src/uClibc
#	@$(MAKE) -C ../../tools-src/uClibc install

help:
	@echo "make [model id]"
	@echo "make mk-[package]"
	@echo "..etc..      other build configs"
	@echo "clean        -C router clean"
	@echo "cleanimage   rm -rf image"
	@echo "cleantools   clean btools, mksquashfs"
	@echo "cleankernel  -C Linux distclean (but preserves .config)"
	@echo "distclean    distclean of Linux & busybox (but preserve .configs)"
	@echo "prepk        -C Linux oldconfig dep"
	@echo "libc         -C uClibc clean, all, install"

kernel-tags: dummy
	$(MAKE) -C $(LINUXDIR) tags

tags: kernel-tags
	ctags -R $(CTAGS_EXCLUDE_OPT) $(CTAGS_DEFAULT_DIRS)

stage-tags:
	$(MAKE) -C router stage-tags

cscope: dummy
	cscope -bkR -s `echo "$(CTAGS_DEFAULT_DIRS)" | sed -e "s, , -s ,g"` -s $(SRCBASE)/../src/router

all-tags: kernel-tags
	ctags -R shared $(CTAGS_EXCLUDE_OPT) $(SRCBASE)/../src/router

clean-tags: dummy
	$(RM) -f $(LINUXDIR)/tags tags

clean-cscope: dummy
	$(RM) -f $(LINUXDIR)/cscope.* cscope.*

install gen_target:
	$(MAKE) -C router $@

gen_prebuilt:
	-mkdir -p $(PBDIR)
	$(MAKE) -f upb.mak PBDIR=${PBDIR}


#
# Generic rules
#

%: dummy
	@[ ! -d router/$* ] || $(MAKE) -C router $@


%-clean: dummy
	@-[ ! -d router/$* ] || $(MAKE) -C router $@

%-install: dummy
	@[ ! -d router/$* ] || $(MAKE) -C router $* $@

%-stage: dummy
	@[ ! -d router/$* ] || $(MAKE) -C router $* $@

%-build: dummy
	$(MAKE) $*-clean $*

%-tags: dummy
	@[ ! -d router/$* ] || ctags -a -R $(CTAGS_EXCLUDE_OPT) $(SRCBASE)/../src/router/$*

.PHONY: all clean distclean cleanimage cleantools cleankernel prepk what setprofile libc help image default bin_file
.PHONY: a b c d m Makefile allversions
.PHONY: tags
.PHONY: dummy
