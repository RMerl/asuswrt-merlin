include ../common.mak

ifeq ($(RTCONFIG_RALINK),y)
SRCFILE = ralink
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.2219/kernel/fs
else
SRCFILE = broadcom
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.22.19/kernel/fs
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
#	@chmod 775 $(INSTALLDIR)/usr/sbin/mkntfs
	@chmod 775 $(INSTALLDIR)/usr/sbin/chkntfs
	@chmod 664 $(DSTDIR)/ufsd.ko
