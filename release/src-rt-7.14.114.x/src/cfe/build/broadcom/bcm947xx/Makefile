#
# Makefile for Broadcom BCM947XX boards
#
# Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
#
# $Id: Makefile 398971 2013-04-26 22:39:49Z $
#

include ../../../../.config

ARCH ?= arm
CPU ?= bcmcore
BOARD := bcm947xx
CFE_NAME := $(shell echo $(BUILD_NAME)|tr A-Z a-z)

# Standard options
CFG_MLONG64 := 0
CFG_LITTLE ?= 1
CFG_RELOC := 0
CFG_UNCACHED := 0
CFG_VAPI := 0 
CFG_BOARDNAME := "BCM947XX"
CFG_PCI ?= 0
CFG_ZLIB := 0
CFG_LZMA := 1
CFG_MINIMAL_SIZE := 1
CFG_CFLASH := 1
CFG_XIP := 0
CFG_ATE := 0
CFG_LDR_SREC := 0
CFG_LDR_ELF := 0
CFG_ROMBOOT ?= 0
CFG_SHMOO := 1

# BCM947XX options
CFG_BCMHND74K ?= 0
CFG_EMBEDDED_NVRAM := 1
CFG_OTPNVRAM := 0
CFG_FLASH := 1   # if CFG_XIP is set then CFG_FLASH must be set to 0
CFG_SFLASH := 1
CFG_NFLASH := 1
ifeq ($(strip $(CFG_NFLASH)),1)
CFG_FLASH_ERASE := 1
endif
CFG_ET := 1
CFG_GMAC ?= 0	# CFG_GMAC requires CFG_ET
CFG_WL := 0
CFG_WLU := 0
CFG_SIM := 0	# build for Quickturn/Simulator
CFG_SIM_CONSOLE := 0	# with console
ifeq ($(strip $(CFG_PCI)),1)
CFG_BCM57XX := 1
CFG_ET := 0
else
CFG_BCM57XX := 0
endif
CFG_FAILSAFE_UPGRADE ?= 1   # Default on, can be turned off with CFG_FAILSAFE_UPGRADE=0
# CFG_DUAL_IMAGE :=1	# Default to off in compile time

# read sdram params from nvram in internal builds                               
CFG_SDRAM_PARAM_FROM_EMBEDDED_NVRAM := 1

# Additional options
COMPRESSED_CFE := 1
RESCUE_MODE := 1
DSLAC68U := 0
RTAC68U := 0
RTAC56U := 0
RTAC3200 := 0
PATYPE := 0
MTDSIZE ?= $(LINUX_MTD)
MTDSIZE ?= 32
DUAL_TRX := 0
TMO := 0

# HND source base
SRCBASE := $(shell cd ../../../.. && pwd)
TOP := $(SRCBASE)/cfe/cfe

ifeq ($(strip $(CFG_RELOC)),0)

# Link binary in RAM
CFG_BOOTRAM := 1
ifeq ($(strip ${CFG_UNCACHED}),1)
  ifeq ($(strip $(CFG_XIP)),0)
    CFG_TEXT_START := 0xa0700000
  else
    CFG_DATA_START := 0xa0001000
  endif
else
  ifeq ($(strip $(CFG_XIP)),0)
    CFG_TEXT_START := 0x80e00000
  else
    CFG_DATA_START := 0x80001000
  endif
endif
# To generating ATE traces
ifeq ($(strip $(CFG_ATE)),1)
CFLAGS += -DBCM_ATE
endif

ifeq ($(strip $(CFG_DUAL_IMAGE)),1)
CFLAGS +=-DDUAL_IMAGE
endif
ifeq ($(strip $(CFG_FAILSAFE_UPGRADE)),1)
CFLAGS +=-DFAILSAFE_UPGRADE
endif
ifeq ($(strip $(CFG_FLASH_ERASE)),1)
CFLAGS +=-DCFE_FLASH_ERASE_FLASH_ENABLED=1
endif
ifeq ($(strip $(CFG_FLASH_FILL)),1)
CFLAGS +=-DFLASH_PARTITION_FILL_ENABLED=1
endif
ifeq ($(strip $(CFG_SDRAM_PARAM_FROM_EMBEDDED_NVRAM)),1)
CFLAGS +=-DSDRAM_PARAM_FROM_EMBEDDED_NVRAM
endif

ifeq ($(strip $(CFG_BCM_DEVINFO)),1)
CFLAGS += -DBCM_DEVINFO
endif

ifeq ($(strip $(RESCUE_MODE)),1)
CFLAGS += -DRESCUE_MODE
endif

ifeq ($(strip $(RTAC3200)),1)
MTDSIZE := 64
CFLAGS += -DRTAC3200
endif

ifeq ($(strip $(DUAL_TRX)),1)
CFLAGS += -DDUAL_TRX
endif

ifeq ($(strip $(RTAC68U)),1)
ifeq ($(strip $(TMO)),1)
MTDSIZE := 48
CFLAGS += -DDUAL_TRX
else
MTDSIZE := 64
endif
CFLAGS += -DRTAC68U
endif

ifeq ($(BUILD_NAME),$(filter $(BUILD_NAME),RT-AC88U RT-AC5300 RT-AC3100))
MODELFLAG = $(shell echo $(BUILD_NAME) | sed -n "s/-//p")
CFLAGS += -D$(MODELFLAG)
ifeq ($(RGMII_BRCM5301X),y) 
ifeq ($(SWITCH2), RTL8365MB)
CFLAGS += -DRTL8365MB
endif
endif
CFLAGS += -L /opt/hndtools-arm-linux-2.6.36-uclibc-4.5.3/bin/../lib/gcc/arm-brcm-linux-uclibcgnueabi/4.5.3 -lgcc
endif

ifeq ($(strip $(DSLAC68U)),1)
CFLAGS += -DDSLAC68U
endif

ifeq ($(strip $(RTAC87U)),1)
MTDSIZE := 128
CFLAGS += -DRTAC87U
endif

ifeq ($(strip $(RTN18U)),1)
CFLAGS += -DRTN18U
endif

ifeq ($(strip $(COMPRESSED_CFE)),1)
CFLAGS += -DCOMPRESSED_CFE
endif
ifeq ($(MTDSIZE),32)
CFLAGS += -D"CONFIG_LINUX_MTD=32"
endif
ifeq ($(MTDSIZE),48)
CFLAGS += -D"CONFIG_LINUX_MTD=48"
endif
ifeq ($(MTDSIZE),64)
CFLAGS += -D"CONFIG_LINUX_MTD=64"
endif
ifeq ($(MTDSIZE),128)
CFLAGS += -D"CONFIG_LINUX_MTD=128"
endif

# Toolchain prefix 
LINUX_VERSION ?= 2_6
ifeq ($(strip $(LINUX_VERSION)),2_6)
    TOOLPREFIX := mipsel-uclibc-linux26-
else
    TOOLPREFIX := mipsel-linux-
endif

#PATH := $(PATH):/projects/hnd/tools/linux/bin:/projects/hnd/tools/linux/hndtools-mips-wrs-vxworks-3.2.3/bin

CFLAGS += -Ulinux -pipe

# Disable 4710A0 compiler workarounds if any
check_gcc = $(shell if $(GCC) $(1) -c -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)";  fi)
CFLAGS += $(call check_gcc, -m4710a0kern, )

# Disable PIC
CFLAGS-mips = -G 0 -mno-abicalls -fno-pic
CFLAGS-arm = -g -DSTANDALONE -O0 -D__KERNEL__
CFLAGS += $(CFLAGS-$(ARCH))

# Discard unused sections if supported
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -static --gc-sections

else

# Locate data in RAM
CFG_BOOTRAM := 0
ifeq ($(strip ${CFG_UNCACHED}),1)
  CFG_DATA_START := 0xa0001000
else
  CFG_DATA_START := 0x80001000
endif

endif

# ARM override
ifeq ($(ARCH), arm)
CPU := armv7
ifeq ($(NVSIZE), 128)
CFG_NVRAM_SPACE := 131072
else
CFG_NVRAM_SPACE := 65536
endif
CFG_GMAC := 1
CFG_CFLASH := 0
CFG_FLASH := 0   # if CFG_XIP is set then CFG_FLASH must be set to 0
CFG_SFLASH := 1
CFG_TEXT_START := 0x00f00000

TOOLPREFIX := arm-uclibc-linux-2.6.36-

ASFLAGS := -D_LANGUAGE_ASSEMBLY
else
CFG_NVRAM_SPACE := 32768
endif

CFLAGS += -DMAX_NVRAM_SPACE=$(CFG_NVRAM_SPACE)

# handle "undefined reference to otp_read_word" problem
ifeq ($CFG_OTPNVRAM), 1)
CFLAGS +=  -DLOADADDR=$(CFG_TEXT_START)
endif

# define WL_CONFFILE for arch/mips/board/bcm947xx/src/Makefile
ifeq ($(strip $(CFG_WL)),1)
WLCONFFILE ?= wlconfig_cfe_apsta
WLTUNEFILE ?= wltunable_cfe.h
WLCFGDIR   := $(SRCBASE)/wl/config
UPDATESH   := $(WLCFGDIR)/diffupdate.sh
endif

include $(TOP)/main/cfe.mk

#
# Main target for normal builds
#
ifneq ($(strip $(CFG_WL)),1)
ALL: cfe.bin
	$(MAKE)  -C compressed

else # CFG_WL = 1

CLEANOBJS += wlconf.h

ALL: version wlconf.h cfe.bin
	$(MAKE) -C compressed

wlconf.h: $(WLCFGDIR)/$(WLTUNEFILE) FORCE
	[ ! -f $@ ] || chmod +w $@
	@echo "check and update config file"
	cp $< wltemp
	$(UPDATESH) wltemp $@

FORCE:

endif


cfe1M.bin:	cfe.bin
	dd conv=sync if=cfe.bin of=cfe1M.bin bs=1M

CLEANOBJS += cfe1M.bin cfe.qt

CLEAN:
	$(MAKE) -C compressed clean

clean: CLEAN


.EXPORT_ALL_VARIABLES:

#
# Linker script
#

include $(TOP)/main/cfe_link.mk
