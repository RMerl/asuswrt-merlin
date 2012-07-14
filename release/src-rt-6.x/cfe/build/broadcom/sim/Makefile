
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "SIM_SWARM"
CFG_PCI = 1
CFG_LDT_REV_017 = 1

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = swarm

include ${TOP}/main/cfe.mk

BSPOBJS = swarm_init.o swarm_devs.o diag_null.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += swarm_pci.o
endif

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
