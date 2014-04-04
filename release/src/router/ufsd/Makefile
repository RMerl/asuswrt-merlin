include ../common.mak

ifeq ($(RTCONFIG_RALINK),y)
ifeq ($(CONFIG_LINUX30),y)
SRCFILE = ralink_3.0.0
DSTDIR = $(INSTALLDIR)/lib/modules/3.0.0/kernel/fs
else
SRCFILE = ralink_2.6.2219
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.2219/kernel/fs
endif
else
ifeq ($(RTCONFIG_BCMARM),y)
SRCFILE = broadcom_arm
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.36.4brcmarm/kernel/fs
else
SRCFILE = broadcom
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.22.19/kernel/fs
endif
endif

vpath broadcom ralink

all:

clean:

install:
	@echo "[ufsd] Installing..."
	@install -d $(INSTALLDIR)/usr/sbin
	@install -d $(DSTDIR)
	@cp $(SRCFILE)/mkntfs $(INSTALLDIR)/usr/sbin
	@cp $(SRCFILE)/chkntfs $(INSTALLDIR)/usr/sbin
	@cp $(SRCFILE)/ufsd.ko $(DSTDIR)
ifeq ($(RTCONFIG_HFS),y)
	@cp $(SRCFILE)/jnl.ko $(DSTDIR)
	@cp $(SRCFILE)/chkhfs $(INSTALLDIR)/usr/sbin/
	@cp $(SRCFILE)/mkhfs $(INSTALLDIR)/usr/sbin
endif
	@cp $(SRCFILE)/memalloc $(INSTALLDIR)/usr/sbin
	@chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
ifeq ($(RTCONFIG_HFS),y)
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkhfs
	@chmod 775 $(INSTALLDIR)/usr/sbin/mkhfs
endif
	@chmod 664 $(DSTDIR)/ufsd.ko
ifeq ($(RTCONFIG_HFS),y)
	@chmod 664 $(DSTDIR)/jnl.ko
endif

.phony: all clean install
