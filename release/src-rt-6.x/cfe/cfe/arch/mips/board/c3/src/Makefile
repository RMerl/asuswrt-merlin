
#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS"
#

ALLOBJS += c3_init.o c3_devs.o ui_swarm.o c3_tests.o

ifeq ($(strip ${CFG_PCI}),1)
ALLOBJS += c3_pci.o
endif
