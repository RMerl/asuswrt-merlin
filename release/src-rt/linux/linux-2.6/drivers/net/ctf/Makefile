#
# Makefile for the Broadcom CTF
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
# $Id: Makefile,v 1.1 2009-10-27 01:27:44 $
#

obj-${CONFIG_BCM_CTF} := ctf.o

CTFSRC := ../../../../../router/ctf
CTFOBJ := $(CTFSRC)/hndctf.o $(CTFSRC)/hndctf_linux.o
SYMOBJ := $(CTFSRC)/hndctf.o

# Search for sources under src/router/ctf
ifneq (,$(wildcard $(src)/$(CTFSRC)/*.c))
  EXTRA_CFLAGS += -I$(src)/$(CTFSRC)/../../router/ctf
  ctf-objs := $(CTFOBJ) ctf_ksyms.o
else
  ctf-objs := $(CTFSRC)/linux/ctf.o
endif

ifeq (,${KERNELRELEASE})
  # Normal makefile
  KERNELDIR := ../../../../../linux/linux-2.6

  .PHONY: all clean
  all clean :
	  +$(MAKE) -C $(KERNELDIR) M=`pwd` $@
endif

PHONY += $(src)/ctf_ksyms.c

quiet_cmd_ksyms = CALL    $<
      cmd_ksyms = $(CONFIG_SHELL) -e $< $(SYMOBJ) > $@

$(src)/ctf_ksyms.c: $(src)/ctf_ksyms.sh $(src)/$(SYMOBJ) FORCE
	$(call cmd,ksyms)

clean-files += $(src)/$(CTFSRC)/*.o $(src)/$(CTFSRC)/.*.o.cmd
