#
# Makefile for the Broadcom wl driver
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
#
# $Id: Makefile,v 1.13 2010-12-13 20:23:49 $
#

SRCBASE_OFFSET := ../../../../../../src

TARGET := wl
WLCONF_H := wlconf.h
WLCONF_O := wlconf.o

ifeq ($(CONFIG_BUZZZ_FUNC),y)
subdir-ccflags-y += -O0 -fno-inline -finstrument-functions
endif # CONFIG_BUZZZ_FUNC

# Generate list of supported variants
VLIST_BASE := AP STA APSTA
ifeq ($(CONFIG_WL_USE_AP_SDSTD),y)
  VLIST_BASE   += AP_SDSTD
endif

VLIST          += $(VLIST_BASE)

ifeq ($(CONFIG_WL_USBAP),y)
# Do variants because we need two drivers for dual band operation
WL_VARIANTS	:= 1
VLIST		+= HIGH
# Note that we don't use CONFIG_WL_USE_HIGH because we need to leave WL_SUFFIX for
# the other driver. HIGH driver is always called wl_high and loaded upon hotplut
# event.
endif

ifeq ($(BUILD_WL_ONCHIP_G),true)
  VLIST        += $(foreach base,$(VLIST_BASE),$(base)_ONCHIP_G)
endif # BUILD_WL_ONCHIP_G

# Set suffix and config based on the selected variant
WL_SUFFIX := $(strip $(foreach v,$(VLIST),$(if $(CONFIG_WL_USE_$(v)),$(v))))
CONFIG_WL_CONF := $(CONFIG_WL_$(WL_SUFFIX))

# Module file names use lowercase of the suffix
wl_suffix := $(shell echo "$(WL_SUFFIX)" | tr A-Z a-z)


# For PREBUILD and postbuild w/WLSRC, compile variants in subdirs
# Subdir names use lowercase versions of the variant suffixes
#wlsubdirs := $(addprefix wl_,$(foreach v,$(VLIST),$(shell echo "$(v)/" | tr A-Z a-z)))
wlsubdirs := $(addprefix wl_,$(foreach v,$(VLIST),$(shell echo "$(v)" | tr A-Z a-z)))


#subdir-y += $(wlsubdirs)
#subdir-m += $(wlsubdirs)

.NOTPARALLEL:
include $(src)/wl_generic.mk
