
#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS"
#

BSPOBJS += littlesur_init.o littlesur_devs.o cpu1cmds.o dev_ide.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += littlesur_pci.o
endif

ALLOBJS += ui_littlesur.o
