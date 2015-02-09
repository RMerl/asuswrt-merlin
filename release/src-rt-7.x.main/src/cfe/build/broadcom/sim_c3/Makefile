
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "SIM_C3"
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

#
# These settings speed up the DRAM init shortening the delays and
# by not doing too much of it.  This helps boot time in the sim.
# It also adjusts timers and other hardware stuff for running
# in the functional simulator (mostly changes CPU speed to 
# 500Khz).  Remove this before using on real hardware.
#
CFLAGS += -D_FASTEMUL_ -D_FUNCSIM_


ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
