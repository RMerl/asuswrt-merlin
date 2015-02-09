# $Id: Makefile 241205 2011-02-17 21:57:41Z $

CFG_MLONG64 ?= 1
CFG_LITTLE ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 1
CFG_BOOTRAM ?= 0
CFG_PCI ?= 0

include ../bcm91120c/Makefile

# Special diag configuration: console on PromICE, etc.
CFLAGS += -D_BCM91120C_DIAG_CFG_ -DCFG_DRAM_ECC=0
