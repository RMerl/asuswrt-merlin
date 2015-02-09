# $Id: Makefile 241205 2011-02-17 21:57:41Z $

CFG_MLONG64 ?= 0
CFG_LITTLE ?= 1
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_PCI ?= 0

include ../bcm91120c/Makefile

# Special diag configuration: console on PromICE, etc.
CFLAGS += -D_BCM91120C_DIAG_CFG_ -DCFG_DRAM_ECC=0
