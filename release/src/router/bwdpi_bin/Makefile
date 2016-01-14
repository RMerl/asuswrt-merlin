include ../common.mak

install:
ifeq ($(or $(RTCONFIG_BCM7),$(RTCONFIG_BCM_7114)),y)
ifeq ($(RTCONFIG_BCM7),y)
	cp $(TOP)/bwdpi_bin/bwdpi_bcm7/* $(TOP)/bwdpi_bin/
else
	cp $(TOP)/bwdpi_bin/bwdpi_bcm7114/* $(TOP)/bwdpi_bin/
endif
else
ifeq ($(RTCONFIG_BCMSMP),y)
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_smp/* $(TOP)/bwdpi_bin/
else
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_up/* $(TOP)/bwdpi_bin/
endif
endif
clean:
