include ../common.mak
include $(SRCBASE)/.config

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
ifeq ($(RTCONFIG_TFAT),y)
	@cp -f $(SRCFILE)/fat/kernel-module/tfat.ko $(DSTDIR)
	install -D $(SRCFILE)/fat/tools/fsck_msdos $(INSTALLDIR)/usr/sbin/fsck_msdos
ifneq ($(NO_MKTOOLS),y)
	install -D $(SRCFILE)/fat/tools/newfs_msdos $(INSTALLDIR)/usr/sbin/newfs_msdos
	#install -D $(SRCFILE)/fat/tools/fatdebug $(INSTALLDIR)/usr/sbin/fatdebug
	#install -D $(SRCFILE)/fat/tools/fatinfo $(INSTALLDIR)/usr/sbin/fatinfo
	#install -D $(SRCFILE)/fat/tools/dosfslabel $(INSTALLDIR)/usr/sbin/dosfslabel
	#install -D $(SRCFILE)/fat/tools/fatdump $(INSTALLDIR)/usr/sbin/fatdump
endif
endif
ifeq ($(RTCONFIG_TUXERA_NTFS),y)
	@cp -f $(SRCFILE)/ntfs/kernel-module/tntfs.ko $(DSTDIR)
	install -D $(SRCFILE)/ntfs/tools/ntfsck $(INSTALLDIR)/usr/sbin/ntfsck
ifneq ($(NO_MKTOOLS),y)
	install -D $(SRCFILE)/ntfs/tools/mkntfs $(INSTALLDIR)/usr/sbin/mkntfs
	#install -D $(SRCFILE)/ntfs/tools/ntfsdebug $(INSTALLDIR)/usr/sbin/ntfsdebug
	#install -D $(SRCFILE)/ntfs/tools/ntfsinfo $(INSTALLDIR)/usr/sbin/ntfsinfo
	#install -D $(SRCFILE)/ntfs/tools/ntfslabel $(INSTALLDIR)/usr/sbin/ntfslabel
endif
endif
ifeq ($(RTCONFIG_TUXERA_HFS),y)
	cp -f $(SRCFILE)/hfsplus/kernel-module/thfsplus.ko $(DSTDIR)
	install -D $(SRCFILE)/hfsplus/tools/fsck_hfs $(INSTALLDIR)/usr/sbin/fsck_hfs
ifneq ($(NO_MKTOOLS),y)
	install -D $(SRCFILE)/hfsplus/tools/newfs_hfs $(INSTALLDIR)/usr/sbin/newfs_hfs
endif
endif
ifeq ($(or $(RTCONFIG_TFAT),$(RTCONFIG_TUXERA_NTFS),$(RTCONFIG_TUXERA_HFS)),y)
#ifeq ($(and $(RTCONFIG_BCMARM),$(RTCONFIG_SAMBA3)),y)
#	install -D $(SRCFILE)/samba/lib/libstdc++.so.6.0.14 $(INSTALLDIR)/usr/lib/libstdc++.so.6.0.14
#	cd $(INSTALLDIR)/usr/lib && ln -sf libstdc++.so.6.0.14 libstdc++.so.6 && ln -sf libstdc++.so.6.0.14 libstdc++.so
#	install -D $(SRCFILE)/samba/bin/smbd $(INSTALLDIR)/usr/bin/smbd
#endif
endif

.phony: all clean install

