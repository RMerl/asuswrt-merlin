#
# Makefile for the Broadcom PROXYARP
#
# Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
# $Id: Makefile,v 1.1 2009-10-27 01:27:44 $
#

obj-${CONFIG_PROXYARP} := proxyarp.o

PROXYARPOSRC := ../../../../../../src-rt-6.x.4708/router/proxyarp
PROXYARPCSRC := ../../../../../../src-rt-6.x.4708/proxyarp
PROXYARPOBJ := $(PROXYARPCSRC)/proxyarp_common.o $(PROXYARPOSRC)/proxyarp_linux.o
SYMOBJ := $(PROXYARPCSRC)/proxyarp_common.o

# Search for sources under src/router/proxyarp
ifneq ($(wildcard $(src)/$(PROXYARPCSRC)/*.c),)
  proxyarp-objs := $(PROXYARPOBJ) proxyarp_ksyms.o
  EXTRA_CFLAGS += -Werror
else
  proxyarp-objs := $(PROXYARPOSRC)/proxyarp.o
endif

ifeq (,${KERNELRELEASE})
# Normal makefile
  KERNELDIR := $(BASEDIR)/components/opensource/linux/linux-2.6.36

  .PHONY: all clean
  all clean :
	  +$(MAKE) -C $(KERNELDIR) M=`pwd` $@
endif

PHONY += $(src)/proxyarp_ksyms.c

quiet_cmd_ksyms = CALL    $<
      cmd_ksyms = $(CONFIG_SHELL) -e $< $(src)/$(SYMOBJ) > $@

$(src)/proxyarp_ksyms.c: $(src)/proxyarp_ksyms.sh $(src)/$(SYMOBJ) FORCE
	$(call cmd,ksyms)

clean-files += $(PROXYARPCSRC)/*.o $(PROXYARPCSRC)/.*.o.cmd $(PROXYARPOSRC)/*.o $(PROXYARPOSRC)/.*.o.cmd
