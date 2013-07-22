export LINUXDIR := $(SRCBASE)/linux/linux-2.6

EXTRA_CFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DTTEST

export CONFIG_LINUX26=y
export CONFIG_BCMWL5=y

export PARALLEL_BUILD :=
#export PARALLEL_BUILD := -j`grep -c '^processor' /proc/cpuinfo`

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
		echo "# CONFIG_MTD_NAND_DENALI is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_RICOH is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_DISKONCHIP is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_CAFE is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_NANDSIM is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_PLATFORM is not set" >>$(1); \
		echo "# CONFIG_MTD_NAND_ONENAND is not set" >>$(1); \
		sed -i "/CONFIG_MTD_BRCMNAND/d" $(1); \
		echo "CONFIG_MTD_BRCMNAND=y" >>$(1); \
	fi; \
	if [ "$(ARMCPUSMP)" = "up" ]; then \
		cp -f $(SRCBASE)/router/ctf_arm/up/linux/ctf.* $(SRCBASE)/router/ctf_arm/linux/;\
		cp -f $(SRCBASE)/router/ufsd/broadcom_arm_up/ufsd.ko.46_up router/ufsd/broadcom_arm/ufsd.ko; \
	fi; \
	if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME) ]; then \
		if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/linux ]; then \
                	cp -rf $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/linux $(SRCBASE)/wl/. ; \
		 fi; \
   		 if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/clm ]; then \
                	cp -f $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/clm/src/wlc_clm_data.c $(SRCBASE)/wl/clm/src/. ; \
		 fi; \
        else \
		if [ -d $(SRCBASE)/wl/sysdeps/default/linux ]; then \
                      	cp -rf $(SRCBASE)/wl/sysdeps/default/linux $(SRCBASE)/wl/. ; \
                fi; \
                if [ -d $(SRCBASE)/wl/sysdeps/default/clm ]; then \
                        cp -f $(SRCBASE)/wl/sysdeps/default/clm/src/wlc_clm_data.c $(SRCBASE)/wl/clm/src/. ; \
                fi; \
        fi; \
	)
endef


