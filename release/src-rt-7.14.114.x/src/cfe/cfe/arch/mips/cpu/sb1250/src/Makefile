
#
# This is just a Makefile fragment -- it is included by the master
# makefile, cfe.mk
#
# This file should just append object file names to "ALLOBJS"
#

ALLOBJS += sb1250_cpu.o sb1_cpuinit.o sb1250_altcpu.o sb1250_l1cache.o \
           sb1250_l2cache.o sb1250_draminit.o

ALLOBJS += sb1250_arena.o

ALLOBJS += dev_sb1250_uart.o dev_sb1250_ethernet.o dev_sb1250_x1240eeprom.o \
	  dev_sb1250_24lc128eeprom.o dev_sb1250_pcmcia.o dev_jtag.o \
	  dev_sb1250_x1241clock.o dev_sb1250_m41t81clock.o \
	  dev_sb1250_at24c02eeprom.o

ALLOBJS += dev_sb1250_pcihost.o cfe_device_ldr.o

ALLOBJS += ui_soccmds.o ui_corecmds.o ui_tempsensor.o ui_toyclock.o

ALLOBJS += cfe_tests.o 

ALLOBJS += ui_reset.o

ALLOBJS += ui_phycmds.o

ALLOBJS += ui_cpuinfo.o

ALLOBJS += ui_spdcmds.o

ifeq ($(strip ${CFG_PCI}),1)
ALLOBJS += sb1250_pci_machdep.o
endif

ALLOBJS += diag_l2cache.o
#
# Deal with pass1's little quirks
#

CFLAGS += -mcpu=sb1 -mips4
CFLAGS += -D_SB1250_PASS1_WORKAROUNDS_
