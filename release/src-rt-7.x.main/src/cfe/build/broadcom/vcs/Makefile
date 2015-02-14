
CFG_MLONG64 ?= 0
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "VCS"
CFG_PCI = 0

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = vcs

include ${TOP}/main/cfe.mk

BSPOBJS = vcs_init.o vcs_devs.o diag_null.o

#
# These settings speed up the cache init by not doing too much of it.
# This helps boot time in the sim.
# They also adjust timers and other hardware stuff 
# for running in the functional simulator (mostly changes CPU speed to 
# 500Khz).  Remove this before using on real hardware.
#
CFLAGS += -D_FASTINIT_ -D_FASTEMUL_ -D_FUNCSIM_
CFLAGS += -U_SB1250_PASS1_WORKAROUNDS_

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
