
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "P5064"
CFG_PCI = 1

TOP = ../../../cfe
ARCH = mips
CPU = rm5200
BOARD = p5064

include ${TOP}/main/cfe.mk

BSPOBJS = p5064_init.o p5064_devs.o
CLEANOBJS += cfe.algsrec

ALL : cfe cfe.flash
	/home/mpl/bin/linux/algsrec cfe
	cp cfe.algsrec cfe.srec
	echo done

include ${TOP}/main/cfe_link.mk
