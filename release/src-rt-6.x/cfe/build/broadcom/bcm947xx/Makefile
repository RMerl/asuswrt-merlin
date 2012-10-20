#
# Makefile for Broadcom BCM947XX boards
#
# Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
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
# $Id: Makefile 321505 2012-03-15 16:06:17Z $
#

# Standard options
CFG_MLONG64 := 0
CFG_LITTLE ?= 1
CFG_RELOC := 0
CFG_UNCACHED := 0
CFG_VAPI := 0 
CFG_BOARDNAME := "BCM947XX"
CFG_PCI ?= 0
CFG_ZLIB := 1
CFG_LZMA := 1
CFG_MINIMAL_SIZE := 1
CFG_CFLASH := 1
CFG_XIP := 0
CFG_ATE := 0

# BCM947XX options
CFG_BCMHND74K ?= 0
CFG_EMBEDDED_NVRAM := 1
CFG_FLASH := 1   # if CFG_XIP is set then CFG_FLASH must be set to 0
CFG_SFLASH := 1
CFG_NFLASH := 1
ifeq ($(strip $(CFG_NFLASH)),1)
CFG_FLASH_ERASE := 1
endif
CFG_ET := 1
CFG_GMAC ?= 1	# CFG_GMAC requires CFG_ET
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
# CFG_DUAL_IMAGE :=1	# Default to off in compile time

# Additional options
COMPRESSED_CFE := 1
RESCUE_MODE := 1	# disable for tmp
RTN66U := 1
DUAL_TRX :=1

# read sdram params from nvram in internal builds                               
CFG_SDRAM_PARAM_FROM_EMBEDDED_NVRAM := 1


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
    CFG_TEXT_START := 0x80700000
  else
    CFG_DATA_START := 0x80001000
  endif
endif
# To generating ATE traces
ifeq ($(strip $(CFG_ATE)),1)
CFLAGS += -DBCM_ATE
endif
ifeq ($(strip $(RESCUE_MODE)),1)
CFLAGS += -DRESCUE_MODE
endif
ifeq ($(strip $(COMPRESSED_CFE)),1)
CFLAGS += -DCOMPRESSED_CFE
endif
ifeq ($(strip $(RTN66U)),1)
CFLAGS += -DRTN66U
endif
ifeq ($(strip $(DUAL_TRX)),1)
CFLAGS += -DDUAL_TRX
endif

ifeq ($(strip $(CFE_DUAL_IMAGE)),1)
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


# Toolchain prefix 
ifeq ($(CFG_LITTLE), 1)
  ifeq ($(LINUX_VERSION),2_6)
    TOOLPREFIX := mipsel-uclibc-linux26-
  else
    TOOLPREFIX := mipsel-linux-
  endif
else
  TOOLPREFIX := mips-wrs-vxworks-
endif

#PATH := $(PATH):/projects/hnd/tools/linux/bin:/projects/hnd/tools/linux/hndtools-mips-wrs-vxworks-3.2.3/bin

CFLAGS += -Ulinux -pipe

# Disable 4710A0 compiler workarounds if any
check_gcc = $(shell if $(GCC) $(1) -c -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)";  fi)
CFLAGS += $(call check_gcc, -m4710a0kern, )

# Disable PIC
CFLAGS += -G 0 -mno-abicalls -fno-pic

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

ARCH := mips
CPU := bcmcore
BOARD := bcm947xx


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
