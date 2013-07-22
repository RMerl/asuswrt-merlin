
CFG_MLONG64 ?= 1
CFG_LITTLE ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 1
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "C3F"
CFG_PCI = 0

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
