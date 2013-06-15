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

export DSTDIR

vpath broadcom ralink

all:
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

clean:
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@

install:
	@echo "[ufsd] Installing..."
	@install -d $(INSTALLDIR)/usr/sbin
	@install -d $(DSTDIR)
#	@[ ! -d $(SRCFILE) ] || cp $(SRCFILE)/mkntfs $(INSTALLDIR)/usr/sbin
	@[ ! -d $(SRCFILE) ] || cp $(SRCFILE)/chkntfs $(INSTALLDIR)/usr/sbin
	@[ ! -d $(SRCFILE) ] || cp $(SRCFILE)/ufsd.ko $(DSTDIR)
	@[ ! -f $(SRCFILE)/Makefile ] || $(MAKE) -C $(SRCFILE) $@
#	@[ ! -d $(SRCFILE) ] || chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
	@[ ! -d $(SRCFILE) ] || chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
	@[ ! -d $(SRCFILE) ] || chmod 664 $(DSTDIR)/ufsd.ko

.phony: all clean install
