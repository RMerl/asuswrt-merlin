SHARED := ../../../../../../../src-rt-7.14.114.x/src/shared

AFLAGS_plat-head.o        := -DTEXT_OFFSET=$(TEXT_OFFSET)

ifeq ($(CONFIG_BUZZZ), y)
obj-$(CONFIG_BUZZZ) += buzzz.o
endif
obj-$(CONFIG_PLAT_MPCORE) += plat-head.o
obj-$(CONFIG_PLAT_MPCORE) += mpcore_core.o
obj-$(CONFIG_PLAT_MPCORE) += mpcore_gtimer.o
obj-$(CONFIG_PLAT_MPCORE) += mpcore_cache.o
obj-$(CONFIG_PLAT_MPCORE) += clock.o
obj-$(CONFIG_PLAT_MPCORE) += platsmp.o 
obj-$(CONFIG_PLAT_IPROC) += iproc_cru.o
obj-$(CONFIG_PLAT_BCM5301X) += bcm5301x.o bcm5301x_dmu.o
obj-$(CONFIG_PLAT_BCM5301X) += bcm5301x_pcie.o
obj-$(CONFIG_PLAT_NAND_JFFS2) += bcm5301x_nandc.o
obj-$(CONFIG_PLAT_BCM5301X) += nvram_linux.o
obj-$(CONFIG_PLAT_BCM5301X) += hndcoma.o
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndarm_ca9.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndsoc.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndchipc.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, nvram.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndpci.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndsflash.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, spiflash.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, ccsflash.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, nandcore.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, hndnand.o)
obj-$(CONFIG_PLAT_BCM5301X) += $(addprefix $(SHARED)/, linux_gpio.o)
obj-$(CONFIG_CACHE_L310) += cache-l310.o
ifeq ($(CONFIG_GENERIC_GPIO),y)
obj-$(CONFIG_GENERIC_GPIO) += gen_gpio.o
endif
