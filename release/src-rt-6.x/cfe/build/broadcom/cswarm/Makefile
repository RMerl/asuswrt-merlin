
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "CSWARM"
CFG_PCI ?= 1
CFG_LDT_REV_017 ?= 1

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = cswarm

include ${TOP}/main/cfe.mk

BSPOBJS = cswarm_init.o cswarm_devs.o 

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += cswarm_pci.o
endif

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
