export LINUXDIR := $(SRCBASE)/linux/linux-2.6

EXTRA_CFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe

export CONFIG_LINUX26=y
export CONFIG_BCMWL5=y


define platformRouterOptions
endef

define platformBusyboxOptions
endef

define platformKernelConfig
# prepare config_base
# prepare prebuilt kernel binary
	@( \
	if [ "$(BCMNAND)" = "y" ]; then \
		sed -i "/CONFIG_MTD_NFLASH/d" $(1); \
		echo "CONFIG_MTD_NFLASH=y" >>$(1); \
		sed -i "/CONFIG_MTD_NAND/d" $(1); \
		echo "CONFIG_MTD_NAND=y" >>$(1); \
		echo "CONFIG_MTD_NAND_IDS=y" >>$(1); \
		echo "# CONFIG_MTD_NAND_VERIFY_WRITE is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_ECC_SMC is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_MUSEUM_IDS is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_DISKONCHIP is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_CAFE is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_NANDSIM is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_PLATFORM is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_ONENAND is not set" >>$(1); \
		sed -i "/CONFIG_MTD_BRCMNAND/d" $(1); \
		echo "CONFIG_MTD_BRCMNAND=y" >>$(1); \
	fi; \
	if [ "$(SFPRAM16M)" = "y" ]; then \
		sed -i "/CONFIG_WL_USE_AP/d" $(1); \
		echo "CONFIG_WL_USE_APSTA=y" >>$(1); \
		echo "# CONFIG_WL_USE_AP is not set" >>$(1); \
		echo "# CONFIG_WL_USE_AP_SDSTD is not set" >>$(1); \
		echo "# CONFIG_WL_USE_AP_ONCHIP_G is not set" >>$(1); \
		echo "# CONFIG_WL_USE_APSTA_ONCHIP_G is not set" >>$(1); \
		sed -i "/CONFIG_INET_GRO/d" $(1); \
		echo "# CONFIG_INET_GRO is not set" >> $(1); \
		sed -i "/CONFIG_INET_GSO/d" $(1); \
		echo "# CONFIG_INET_GSO is not set" >> $(1); \
		sed -i "/CONFIG_NET_SCH_HFSC/d" $(1); \
		echo "# CONFIG_NET_SCH_HFSC is not set" >> $(1); \
		sed -i "/CONFIG_NET_SCH_ESFQ/d" $(1); \
		echo "# CONFIG_NET_SCH_ESFQ is not set" >> $(1); \
		sed -i "/CONFIG_NET_SCH_TBF/d" $(1); \
		echo "# CONFIG_NET_SCH_TBF is not set" >> $(1); \
		sed -i "/CONFIG_NLS/d" $(1); \
		echo "# CONFIG_NLS is not set" >>$(1); \
		echo "# CONFIG_NLS_DEFAULT=\"iso8859-1\"">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_437 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_737 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_775 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_850 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_852 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_855 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_857 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_860 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_861 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_862 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_863 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_864 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_865 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_866 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_869 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_936 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_950 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_932 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_949 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_874 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_8 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_1250 is not set">>$(1); \
		echo "# CONFIG_NLS_CODEPAGE_1251 is not set">>$(1); \
		echo "# CONFIG_NLS_ASCII is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_1 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_2 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_3 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_4 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_5 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_6 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_7 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_9 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_13 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_14 is not set">>$(1); \
		echo "# CONFIG_NLS_ISO8859_15 is not set">>$(1); \
		echo "# CONFIG_NLS_KOI8_R is not set">>$(1); \
		echo "# CONFIG_NLS_KOI8_U is not set">>$(1); \
		echo "# CONFIG_NLS_UTF8 is not set">>$(1); \
		sed -i "/CONFIG_USB/d" $(1); \
		echo "# CONFIG_USB_SUPPORT is not set" >> $(1); \
		sed -i "/CONFIG_SCSI/d" $(1); \
		echo "# CONFIG_SCSI is not set" >> $(1); \
		sed -i "/CONFIG_LBD/d" $(1); \
		echo "# CONFIG_LBD is not set" >> $(1); \
		sed -i "/CONFIG_BLK_DEV_SD/d" $(1); \
		sed -i "/CONFIG_BLK_DEV_SR/d" $(1); \
		sed -i "/CONFIG_CHR_DEV_SG/d" $(1); \
		sed -i "/CONFIG_VIDEO/d" $(1); \
		echo "# CONFIG_VIDEO_DEV is not set" >> $(1); \
		sed -i "/CONFIG_V4L_USB_DRIVERS/d" $(1); \
		sed -i "/CONFIG_SOUND/d" $(1); \
		echo "# CONFIG_SOUND is not set" >> $(1); \
		sed -i "/CONFIG_SND/d" $(1); \
		sed -i "/CONFIG_HID/d" $(1); \
		echo "# CONFIG_HID is not set" >> $(1); \
		sed -i "/CONFIG_MMC/d" $(1); \
		echo "# CONFIG_MMC is not set" >> $(1); \
		sed -i "/CONFIG_PARTITION_ADVANCED/d" $(1); \
		echo "# CONFIG_PARTITION_ADVANCED is not set" >> $(1); \
		sed -i "/CONFIG_TRACE_IRQFLAGS_SUPPORT/d" $(1); \
		sed -i "/CONFIG_SYS_SUPPORTS_KGDB/d" $(1); \
		sed -i "/CONFIG_EXT2_FS/d" $(1); \
		echo "# CONFIG_EXT2_FS is not set" >> $(1); \
		sed -i "/CONFIG_EXT3_FS/d" $(1); \
		echo "# CONFIG_EXT3_FS is not set" >> $(1); \
		sed -i "/CONFIG_JBD/d" $(1); \
		echo "# CONFIG_JBD is not set" >> $(1); \
		sed -i "/CONFIG_REISERFS_FS/d" $(1); \
		echo "# CONFIG_REISERFS_FS is not set" >> $(1); \
		sed -i "/CONFIG_FAT_FS/d" $(1); \
		echo "# CONFIG_FAT_FS is not set" >> $(1); \
		sed -i "/CONFIG_VFAT_FS/d" $(1); \
		echo "# CONFIG_VFAT_FS is not set" >> $(1); \
		sed -i "/CONFIG_NFS_FS/d" $(1); \
		echo "# CONFIG_NFS_FS is not set" >> $(1); \
		sed -i "/CONFIG_NFSD/d" $(1); \
		echo "# CONFIG_NFSD is not set" >> $(1); \
		sed -i "/CONFIG_FUSE_FS/d" $(1); \
		echo "# CONFIG_FUSE_FS is not set" >> $(1); \
		sed -i "/CONFIG_CIFS/d" $(1); \
		echo "# CONFIG_CIFS is not set" >> $(1); \
		sed -i "/CONFIG_FAT/d" $(1); \
		sed -i "/CONFIG_INOTIFY/d" $(1); \
		echo "# CONFIG_INOTIFY is not set" >> $(1); \
		sed -i "/CONFIG_DNOTIFY/d" $(1); \
		echo "# CONFIG_DNOTIFY is not set" >> $(1); \
		sed -i "/CONFIG_CRYPTO_BLKCIPHER/d" $(1); \
		sed -i "/CONFIG_CRYPTO_HASH/d" $(1); \
		sed -i "/CONFIG_CRYPTO_MANAGER/d" $(1); \
		echo "# CONFIG_CRYPTO_MANAGER is not set" >> $(1); \
		sed -i "/CONFIG_CRYPTO_HMAC/d" $(1); \
		echo "# CONFIG_CRYPTO_HMAC is not set" >> $(1); \
		sed -i "/CONFIG_CRYPTO_ECB/d" $(1); \
		echo "# CONFIG_CRYPTO_ECB is not set" >> $(1); \
		sed -i "/CONFIG_CRYPTO_CBC/d" $(1); \
		echo "# CONFIG_CRYPTO_CBC is not set" >> $(1); \
	fi; \
	if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME) ]; then \
		cp -rf $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/linux $(SRCBASE)/wl/. ; \
	else \
		cp -rf $(SRCBASE)/wl/sysdeps/default/linux $(SRCBASE)/wl/. ; \
	fi; \
	)
endef
