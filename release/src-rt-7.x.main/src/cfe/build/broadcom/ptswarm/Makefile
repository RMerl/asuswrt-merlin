
CFG_MLONG64 ?= 1
CFG_LITTLE ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "PTSWARM"
CFG_PCI ?= 1
CFG_LDT_REV_017 ?= 1
CFG_FLASHPROG ?= 1
CFG_ZLIB ?= 1
CFG_UNICPU ?= 0

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = ptswarm

include ${TOP}/main/cfe.mk

ifeq ($(strip ${CFG_FLASHPROG}),1)
CFLAGS += -D_FLASHPROG_
endif

ifeq ($(strip ${CFG_UNICPU}),1)
CFLAGS += -D_UNICPU_
endif

CFLAGS += -DSERIAL_PORT_LEDS -D_PTSWARM_CFG_

BSPOBJS = ptswarm_init.o ptswarm_devs.o dev_ptflash.o

ifeq ($(strip ${CFG_PCI}),1)
BSPOBJS += ptswarm_pci.o
endif

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
