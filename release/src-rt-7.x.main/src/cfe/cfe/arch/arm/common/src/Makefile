#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS",
# but since it is mean to be linked *first*, it will append
# modules to "CRT0OBJS"
#

ifndef INIT_ARM
INIT_ARM = bootarm.o
endif

ifeq ($(strip ${CFG_UNCACHED_RAMAPP}),1)
#CRT0OBJS += init_ram.o exception.o
else
ifeq ($(strip ${CFG_RAMAPP}),1)
#CRT0OBJS += init_ram.o exception.o
else
CRT0OBJS += $(INIT_ARM) exception.o
endif
endif

ALLOBJS += lib_setjmp.o disasm.o arm_arena.o l1cache.o exchandler.o

#
# Use the relocatable self-write engine
#
ifneq ($(strip ${CFG_FLASH}),1)
#ALLOBJS += dev_flash_all.o dev_flashop_engine.o
endif

makereg : ${TOP}/hosttools/makereg.c
	gcc -o makereg ${TOP}/hosttools/makereg.c

%.inc : %.regdef makereg
	./makereg $< $@ 

ui_soccmds.o : ${CPU_SRC}/ui_soccmds.c ${CPU}_socregs.inc

vapi.o : ${TOP}/verif/vapi.S sb1250_socregs.inc
