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
ifeq ($(RTCONFIG_CTF_PPTP_L2TP),y)
	# SMP with PPTP_L2TP
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_smp_pptp/* $(TOP)/bwdpi_bin/
else
	# SMP without PPTP_L2TP
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_smp/* $(TOP)/bwdpi_bin/
endif
else
ifeq ($(RTCONFIG_CTF_PPTP_L2TP),y)
	# UP with PPTP_L2TP
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_up_pptp/* $(TOP)/bwdpi_bin/
else
	# UP without PPTP_L2TP
	cp $(TOP)/bwdpi_bin/bwdpi_bcm6_up/* $(TOP)/bwdpi_bin/
endif
endif
endif
clean:
