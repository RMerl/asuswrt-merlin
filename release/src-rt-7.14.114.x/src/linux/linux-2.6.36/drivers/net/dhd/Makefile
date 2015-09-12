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
TARGET := dhd
obj-$(CONFIG_DHDAP) := $(TARGET).o

ifndef SRCBASE_DHD
    SRCBASE_DHD := dhd/src
else
    SRCBASE_DHD := $(subst $(dir $(patsubst %/,%,$(dir $(SRCBASE_DHD)))),,$(SRCBASE_DHD))
endif

SRCBASE_OFFSET := ../../../../../../$(SRCBASE_DHD)
DHDMAKEFILE    := $(SRCBASE)/../$(SRCBASE_DHD)/dhd/config/dhd_router.mk

REBUILD_DHD_MODULE=$(shell if [ -d "$(SRCBASE)/../$(SRCBASE_DHD)/dhd/sys" ]; then echo 1; else echo 0; fi)

# If source directory (dhd/src/dhd/sys) exists then build dhd, otherwise use pre-builts
ifeq ($(REBUILD_DHD_MODULE),1)

    ifeq ($(CONFIG_BUZZZ_FUNC),y)
        subdir-ccflags-y += -O0 -fno-inline -finstrument-functions
    endif # CONFIG_BUZZZ_FUNC

    # Include master configuration from dhd makefile
    include $(DHDMAKEFILE)

    # Fix the include paths for DHD. DHD gets built from a
    # different source tree.
    KBUILD_CFLAGS_REMOVE := -I$(SRCBASE)/include
    KBUILD_CFLAGS_REMOVE += -I$(SRCBASE)/common/include
    KBUILD_CFLAGS_REMOVE += $(WLAN_ComponentIncPath)
    KBUILD_CFLAGS_REMOVE += $(WLAN_StdIncPathA)

    KBUILD_CFLAGS   := $(filter-out $(KBUILD_CFLAGS_REMOVE), $(KBUILD_CFLAGS))

    # dhd-objs is for linking to dhd.o
    $(TARGET)-objs := $(DHD_OBJS)

else # Use prebuilt modules

    prebuilt := dhd.o
    $(TARGET)-objs := $(SRCBASE_OFFSET)/dhd/linux/$(prebuilt)
    obj-$(CONFIG_DHDAP) := $(TARGET).o

endif
