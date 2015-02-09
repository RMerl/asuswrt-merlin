
CFG_MLONG64  ?= 0
CFG_LITTLE   ?= 1
CFG_RELOC    ?= 1
CFG_UNCACHED ?= 0
CFG_NEWRELOC ?= 0
CFG_VAPI     ?= 0
CFG_BOOTRAM  ?= 0
CFG_BOARDNAME = "BCM97115"
CFG_PCI       = 0


TOP   = ../../../cfe
ARCH  = mips
CPU   = bcmcore
BOARD = bcm97115

CFLAGS += -DBCM97115

CFLAGS += -DDEBUG
#	CFLAGS += -DDEBUG_ENV_SIM
#	CFLAGS += -DDEBUG_ENV_ICE
	CFLAGS += -DDEBUG_ENV_ROM

#CFLAGS += -DSERIALLEDS

include ${TOP}/main/cfe.mk

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
