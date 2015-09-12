#
# Makefile for Broadcom Home Networking Division (HND) shared driver code
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
# $Id: Makefile,v 1.5 2008-05-02 22:49:54 $
#

SHARED   := ../../../../../../src/shared
ROUTER   := ../../../../../../src/router
SRC      := ../../../../../../src
WLSHARED := wlconfig_lx_shared
WLCFGDIR := $(src)/../../../../../../src/wl/config
WLSYSDIR := $(src)/../../../../../../src/wl/sys

ifeq ($(CONFIG_BUZZZ_FUNC),y)
subdir-ccflags-y += -O0 -fno-inline -finstrument-functions
endif # CONFIG_BUZZZ_FUNC

ifneq ($(wildcard $(WLCFGDIR)),)
include $(WLCFGDIR)/$(WLSHARED)
include $(WLCFGDIR)/wl.mk
endif

hnd-objs := $(addprefix $(SHARED)/, $(sort $(WLFILES:.c=.o)))
HND_OBJS := $(addprefix $(src)/$(SHARED)/, $(sort $(WLFILES:.c=.o)))

EXTRA_CFLAGS += $(WLFLAGS) -DBCM5354 -DLINUX_HYBRID_POSTPROCESSING_REMOVAL

# For the ARM platform...
ifneq ($(CONFIG_MACH_BRCM_NS),)
EXTRA_CFLAGS    += -DROBO_SRAB -DBCMDMASGLISTOSL
endif

ifneq ($(CONFIG_BCM947XX),y)
HND_OBJS += $(src)/$(SHARED)/nvramstubs.o
hnd-objs += $(SHARED)/nvramstubs.o
endif

ifeq ($(CONFIG_ET_47XX),y)
HND_OBJS += $(src)/$(SHARED)/bcmrobo.o
hnd-objs += $(SHARED)/bcmrobo.o
else
ifeq ($(CONFIG_BCM57XX),y)
HND_OBJS += $(src)/$(SHARED)/bcmrobo.o
hnd-objs += $(SHARED)/bcmrobo.o
endif
endif

ifeq ($(CONFIG_BCM_GMAC3),y)
HND_OBJS += $(src)/$(SHARED)/hndfwd.o
hnd-objs += $(SHARED)/hndfwd.o
endif

ifdef CONFIG_USB_RNDIS
EXTRA_CFLAGS += -DCONFIG_USBRNDIS_RETAIL
endif                                  

ifeq ($(BCMDBG_MEM),1)
EXTRA_CFLAGS += -DBCMDBG_MEM
endif

obj-$(CONFIG_HND) := hnd.o shared_ksyms.o

ifeq ($(wildcard $(src)/$(SHARED)/bcmutils.c),)
$(src)/$(SHARED)/bcmutils.o: $(src)/$(SHARED)/linux/bcmutils.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/bcmrobo.c),)
$(src)/$(SHARED)/bcmrobo.o: $(src)/$(SHARED)/linux/bcmrobo.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/hnddma.c),)
$(src)/$(SHARED)/hnddma.o: $(src)/$(SHARED)/linux/hnddma.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/hndfwd.c),)
$(src)/$(SHARED)/hndfwd.o: $(src)/$(SHARED)/linux/hndfwd.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/linux_osl.c),)
$(src)/$(SHARED)/linux_osl.o: $(src)/$(SHARED)/linux/linux_osl.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/sbutils.c),)
$(src)/$(SHARED)/sbutils.o: $(src)/$(SHARED)/linux/sbutils.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/hndpmu.c),)
$(src)/$(SHARED)/hndpmu.o: $(src)/$(SHARED)/linux/hndpmu.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/bcmsrom.c),)
$(src)/$(SHARED)/bcmsrom.o: $(src)/$(SHARED)/linux/bcmsrom.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/nvramstubs.c),)
$(src)/$(SHARED)/nvramstubs.o: $(src)/$(SHARED)/linux/nvramstubs.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/bcmwpa.c),)
$(src)/$(SHARED)/bcmwpa.o: $(src)/$(SHARED)/linux/bcmwpa.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/bcmotp.c),)
$(src)/$(SHARED)/bcmotp.o: $(src)/$(SHARED)/linux/bcmotp.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/nicpci.c),)
$(src)/$(SHARED)/nicpci.o: $(src)/$(SHARED)/linux/nicpci.o
	cp $< $@
endif

ifeq ($(wildcard $(src)/$(SHARED)/pcie_core.c),)
$(src)/$(SHARED)/pcie_core.o: $(src)/$(SHARED)/linux/pcie_core.o
	cp $< $@
endif

$(src)/shared_ksyms.c: $(src)/shared_ksyms.sh $(HND_OBJS)
	sh -e $< $(HND_OBJS) > $@

clean-files += $(SHARED)/*.o $(SHARED)/.*.o.cmd
