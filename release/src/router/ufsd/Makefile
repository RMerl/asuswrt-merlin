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
#	@cp $(SRCFILE)/mkntfs $(INSTALLDIR)/usr/sbin
	@cp $(SRCFILE)/chkntfs $(INSTALLDIR)/usr/sbin
	@cp $(SRCFILE)/ufsd.ko $(DSTDIR)
	@cp $(SRCFILE)/jnl.ko $(DSTDIR)
#	@chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
	@chmod 664 $(DSTDIR)/ufsd.ko
	@chmod 664 $(DSTDIR)/jnl.ko

.phony: all clean install
