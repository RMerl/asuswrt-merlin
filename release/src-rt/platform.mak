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
	if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME) ]; then \
		cp -rf $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/linux $(SRCBASE)/wl/. ; \
	else \
		cp -rf $(SRCBASE)/wl/sysdeps/default/linux $(SRCBASE)/wl/. ; \
	fi; \
	)
endef


