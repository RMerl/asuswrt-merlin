
#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS"
#

BSPOBJS += bcm91125e_init.o bcm91125e_devs.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += bcm91125e_pci.o
endif

ALLOBJS += ui_bcm91125e.o bcm91125e_tests.o
