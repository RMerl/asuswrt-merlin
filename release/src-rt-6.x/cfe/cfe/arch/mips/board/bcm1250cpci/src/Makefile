
#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS"
#

BSPOBJS += bcm1250cpci_init.o bcm1250cpci_devs.o
ALLOBJS += dev_ide.o ui_bcm1250cpci.o cpu1test.o cpu1cmds.o sl11h.o ui_usb_scanlogic.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += bcm1250cpci_pci.o
endif
