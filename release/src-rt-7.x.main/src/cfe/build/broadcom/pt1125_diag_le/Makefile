# $Id: Makefile 241205 2011-02-17 21:57:41Z $

CFG_MLONG64 ?= 1
CFG_LITTLE ?= 1
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 1
CFG_BOOTRAM ?= 0
CFG_PCI ?= 1

include ../pt1125/Makefile

VPATH += ../pt1125
CFLAGS += -I../pt1125

# disable most devices, console on Prom-ICE
CFLAGS += -D_PT1125_DIAG_CFG_  -DCFG_DRAM_ECC=0
