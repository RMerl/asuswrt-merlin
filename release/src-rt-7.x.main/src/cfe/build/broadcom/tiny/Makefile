
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0 
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "TINY"
CFG_PCI = 0

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
#
# This port doesn't really have a board directory - it does not
# need one.  So, don't set BOARD.
#BOARD = swarm
#CFLAGS += -D_FUNCSIM_ -D_FASTEMUL_
include ${TOP}/main/cfe.mk

BSPOBJS = tiny_init.o tiny_devs.o 

ALL : cfe
	echo done

include ${TOP}/main/cfe_link.mk
