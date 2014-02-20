include ../common.mak

ifeq ($(RTCONFIG_RALINK),y)
ifeq ($(CONFIG_LINUX30),y)
SRCFILE = ralink_3.0.0
else
SRCFILE = ralink_2.6.2219
UFSD_PREBUILT = 1
NO_MKNTFS = 1
endif
DSTDIR = $(INSTALLDIR)/lib/modules/$(LINUX_KERNEL)/kernel/fs
else
ifeq ($(RTCONFIG_BCMARM),y)
SRCFILE = broadcom_arm
UFSD_PREBUILT = 1
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.36.4brcmarm/kernel/fs
else
SRCFILE = broadcom
UFSD_PREBUILT = 1
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.22.19/kernel/fs
endif
endif

ifeq ($(RTCONFIG_UFSD_DEBUG),y)
DEBUG=y
endif

export DSTDIR
export DEBUG

vpath broadcom ralink

all:

clean:

install:
	@echo "[ufsd] Installing..."
	@install -d $(INSTALLDIR)/usr/sbin
	@install -d $(DSTDIR)
ifeq ($(RTCONFIG_NTFS),y)
ifeq ($(UFSD_PREBUILT),1)
	@cp -f $(SRCFILE)/ufsd.ko $(DSTDIR)
endif
ifneq ($(NO_MKNTFS),1)
	@cp -f $(SRCFILE)/mkntfs $(INSTALLDIR)/usr/sbin
	@chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
endif
	@cp -f $(SRCFILE)/chkntfs $(INSTALLDIR)/usr/sbin
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
endif
ifeq ($(RTCONFIG_HFS),y)
	@cp -f $(SRCFILE)/jnl.ko $(DSTDIR)
	@[ ! -f $(SRCFILE)/mkhfs ] || cp -f $(SRCFILE)/mkhfs $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/mkhfs
	@[ ! -f $(SRCFILE)/chkhfs ] || cp -f $(SRCFILE)/chkhfs $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/chkhfs
	@[ ! -f $(SRCFILE)/memalloc ] || cp -f $(SRCFILE)/memalloc $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/memalloc
	@[ ! -f $(SRCFILE)/test_system ] || cp -f $(SRCFILE)/test_system $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/test_system
endif
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

.phony: all clean install

