include ../common.mak

ifeq ($(RTCONFIG_RALINK),y)
ifeq ($(CONFIG_LINUX30),y)
SRCFILE = ralink_3.0.0
else
SRCFILE = ralink_2.6.2219
endif
DSTDIR = $(INSTALLDIR)/lib/modules/$(LINUX_KERNEL)/kernel/fs
else
ifeq ($(RTCONFIG_BCMARM),y)
SRCFILE = broadcom_arm
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.36.4brcmarm/kernel/fs
else
SRCFILE = broadcom
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.22.19/kernel/fs
endif
endif

export DSTDIR

vpath broadcom ralink

all:
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

clean:
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

install:
	echo "[ufsd] Installing..."
	@install -d $(INSTALLDIR)/usr/sbin
	@install -d $(DSTDIR)
ifeq ($(RTCONFIG_NTFS),y)
#       @cp -f $(SRCFILE)/mkntfs $(INSTALLDIR)/usr/sbin
#       @chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
	@cp -f $(SRCFILE)/chkntfs $(INSTALLDIR)/usr/sbin
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
endif
ifeq ($(RTCONFIG_HFS),y)
	@[ ! -f $(SRCFILE)/mkhfs ] || cp -f $(SRCFILE)/mkhfs $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/mkhfs
	@[ ! -f $(SRCFILE)/chkhfs ] || cp -f $(SRCFILE)/chkhfs $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/chkhfs
	@[ ! -f $(SRCFILE)/memalloc ] || cp -f $(SRCFILE)/memalloc $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/memalloc
	@[ ! -f $(SRCFILE)/test_system ] || cp -f $(SRCFILE)/test_system $(INSTALLDIR)/usr/sbin || chmod 775 $(INSTALLDIR)/usr/sbin/test_system
endif
	@cp -f $(SRCFILE)/ufsd.ko $(DSTDIR)
	@chmod 664 $(DSTDIR)/ufsd.ko
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

.phony: all clean install

