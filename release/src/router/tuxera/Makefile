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
	@echo "[tuxera] Installing..."
	@install -d $(DSTDIR)
	@install -d $(INSTALLDIR)/usr/sbin
ifeq ($(RTCONFIG_NTFS),y)
	@cp -f $(SRCFILE)/ntfs/kernel-module/tntfs.ko $(DSTDIR)
	install -D $(SRCFILE)/ntfs/tools/mkntfs $(INSTALLDIR)/usr/sbin/mkntfs
	install -D $(SRCFILE)/ntfs/tools/ntfsck $(INSTALLDIR)/usr/sbin/ntfsck
	install -D $(SRCFILE)/ntfs/tools/ntfsdebug $(INSTALLDIR)/usr/sbin/ntfsdebug
	install -D $(SRCFILE)/ntfs/tools/ntfsinfo $(INSTALLDIR)/usr/sbin/ntfsinfo
	install -D $(SRCFILE)/ntfs/tools/ntfslabel $(INSTALLDIR)/usr/sbin/ntfslabel
endif
ifeq ($(RTCONFIG_HFS),y)
	cp -f $(SRCFILE)/hfsplus/kernel-module/thfsplus.ko $(DSTDIR)
	install -D $(SRCFILE)/hfsplus/tools/fsck_hfs $(INSTALLDIR)/usr/sbin/fsck_hfs
	install -D $(SRCFILE)/hfsplus/tools/newfs_hfs $(INSTALLDIR)/usr/sbin/newfs_hfs
endif
ifeq ($(RTCONFIG_EXFAT),y)
	cp -f $(SRCFILE)/exfat/kernel-module/texfat.ko $(DSTDIR)
	install -D $(SRCFILE)/exfat/tools/mkexfat $(INSTALLDIR)/usr/sbin/mkexfat
	install -D $(SRCFILE)/exfat/tools/exfatck $(INSTALLDIR)/usr/sbin/exfatck
	install -D $(SRCFILE)/exfat/tools/exfatdebug $(INSTALLDIR)/usr/sbin/exfatdebug
	install -D $(SRCFILE)/exfat/tools/exfatinfo $(INSTALLDIR)/usr/sbin/exfatinfo
	install -D $(SRCFILE)/exfat/tools/exfatlabel $(INSTALLDIR)/usr/sbin/exfatlabel
endif

.phony: all clean install

