
CFG_MLONG64 ?= 1
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "PT1120"
CFG_PCI ?= 0
CFG_FLASHPROG ?= 1
CFG_ZLIB ?= 1
CFG_UNICPU ?= 1

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = pt1120

include ${TOP}/main/cfe.mk

#
# For now, we do the BCM11xx magic at compile time, not at run-time
# So, define this symbol to tweak the MC code and other stuff
# to work properly.
#
CFLAGS += -D_SB11XX_

ifeq ($(strip ${CFG_FLASHPROG}),1)
CFLAGS += -D_FLASHPROG_
endif

ifeq ($(strip ${CFG_UNICPU}),1)
CFLAGS += -D_UNICPU_
endif

CFLAGS += -DSERIAL_PORT_LEDS -D_PT1120_CFG_

BSPOBJS = pt1120_init.o pt1120_devs.o dev_ptflash.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += pt1120_pci.o
endif

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
