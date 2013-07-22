# $Id: Makefile 241205 2011-02-17 21:57:41Z $

CFG_MLONG64 ?= 1
CFG_LITTLE ?= 1
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_VAPI ?= 1
CFG_BOOTRAM ?= 0
CFG_PCI ?= 0

include ../cswarm/Makefile

VPATH += ../cswarm
CFLAGS += -I../cswarm

# disable most devices, console on Prom-ICE
CFLAGS += -D_CSWARM_DIAG_CFG_
