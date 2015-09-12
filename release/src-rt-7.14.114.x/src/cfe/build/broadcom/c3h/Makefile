
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "C3H"
CFG_PCI ?= 1
CFG_LDT_REV_017 ?= 1

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = c3

#
# Note: the c3_init and c3_devs files are added in the board-specific
# directory, since they are identical among the various flavors of C3 boards.
#

include ${TOP}/main/cfe.mk

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
