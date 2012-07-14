
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 1
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "P6064"
CFG_PCI ?= 1

TOP = ../../../cfe
ARCH = mips
CPU = rm7000
BOARD = p6064

include ${TOP}/main/cfe.mk

BSPOBJS = p6064_init.o p6064_devs.o
CLEANOBJS += cfe.algsrec

ALL : cfe cfe.flash
	/home/mpl/bin/linux/algsrec cfe
	#cp cfe.algsrec cfe.srec
	echo done

include ${TOP}/main/cfe_link.mk
