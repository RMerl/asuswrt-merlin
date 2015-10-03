include ../common.mak
include $(SRCBASE)/.config

DSTDIR = $(INSTALLDIR)/lib/modules/$(LINUX_KERNEL)/kernel/fs

ifeq ($(RTCONFIG_RALINK),y)
# Ralink/MTK platform

ifeq ($(CONFIG_LINUX30),y)
SRCFILE = ralink_3.0.0
else
SRCFILE = ralink_2.6.2219
endif

else ifeq ($(RTCONFIG_QCA),y)
# QCA platform
SRCFILE = qca
ifeq ($(RT4GAC55U),y)
SRCFILE = qca_4gac55u
endif

else
# Broadcom platform

ifeq ($(RTCONFIG_BCMARM),y)
ifeq ($(RTCONFIG_BCM7),y)
SRCFILE = broadcom_arm_7
else
ifeq ($(RTCONFIG_BCM_7114),y)
SRCFILE = broadcom_arm_7114
else
ifeq ($(ARMCPUSMP),up)
SRCFILE = broadcom_arm_up
else
ifeq ($(RTCONFIG_RECVFILE),y)
SRCFILE = broadcom_arm.recvfile
else
SRCFILE = broadcom_arm
endif
endif # ARMCPUSMP
endif # RTCONFIG_BCM_7114
endif # RTCONFIG_BCM7
DSTDIR = $(INSTALLDIR)/lib/modules/2.6.36.4brcmarm/kernel/fs
else # RTCONFIG_BCMARM
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
	install -D $(SRCFILE)/fat/tools/fatfsck $(INSTALLDIR)/usr/sbin/fatfsck
ifneq ($(NO_MKTOOLS),y)
	install -D $(SRCFILE)/fat/tools/mkfatfs $(INSTALLDIR)/usr/sbin/mkfatfs
	#install -D $(SRCFILE)/fat/tools/fatdebug $(INSTALLDIR)/usr/sbin/fatdebug
	#install -D $(SRCFILE)/fat/tools/fatinfo $(INSTALLDIR)/usr/sbin/fatinfo
	#install -D $(SRCFILE)/fat/tools/fatlabel $(INSTALLDIR)/usr/sbin/fatlabel
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
	#install -D $(SRCFILE)/hfsplus/tools/thfslabel $(INSTALLDIR)/usr/sbin/thfslabel
endif
endif
ifeq ($(or $(RTCONFIG_TFAT),$(RTCONFIG_TUXERA_NTFS),$(RTCONFIG_TUXERA_HFS)),y)
	install  -m 0644 -D $(SRCFILE)/VERSION-INFO $(INSTALLDIR)/rom/Tuxera-VERSION-INFO
#ifeq ($(and $(RTCONFIG_BCMARM),$(RTCONFIG_SAMBA3)),y)
#	install -D $(SRCFILE)/samba/lib/libstdc++.so.6.0.14 $(INSTALLDIR)/usr/lib/libstdc++.so.6.0.14
#	cd $(INSTALLDIR)/usr/lib && ln -sf libstdc++.so.6.0.14 libstdc++.so.6 && ln -sf libstdc++.so.6.0.14 libstdc++.so
#	install -D $(SRCFILE)/samba/smbd $(INSTALLDIR)/usr/bin/smbd
#endif
endif

.phony: all clean install

