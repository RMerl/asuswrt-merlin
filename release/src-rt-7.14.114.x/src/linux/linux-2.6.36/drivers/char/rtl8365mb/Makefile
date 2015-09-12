#
# drivers/char/rtl8365mb/Makefile
#
# Makefile for RTL8365MB drivers.
#

obj-$(CONFIG_RTL8365MB) += rtl8365mb.o

RTL8365MB_SRC := $(srctree)/drivers/char/rtl8365mb
RTL8365MB_PRE := ../../../../../router/rtl_bin/prebuilt
BCM_INC := ../../../../../include

KBUILD_CFLAGS += -I$(srctree)/drivers/char/rtl8365mb -I$(BCM_INC)

ifneq (,$(wildcard $(RTL8365MB_SRC)/rtk_switch.c))
	rtl8365mb-objs := acl.o cpu.o dot1x.o eee.o igmp.o interrupt.o l2.o leaky.o led.o mirror.o oam.o port.o ptp.o \
			qos.o rate.o rldp.o rtk_switch.o rtl8367c_asicdrv_acl.o rtl8367c_asicdrv.o \
			rtl8367c_asicdrv_cputag.o rtl8367c_asicdrv_dot1x.o rtl8367c_asicdrv_eav.o rtl8367c_asicdrv_eee.o \
			rtl8367c_asicdrv_fc.o rtl8367c_asicdrv_green.o rtl8367c_asicdrv_hsb.o rtl8367c_asicdrv_igmp.o \
			rtl8367c_asicdrv_inbwctrl.o rtl8367c_asicdrv_interrupt.o rtl8367c_asicdrv_led.o \
			rtl8367c_asicdrv_lut.o rtl8367c_asicdrv_meter.o rtl8367c_asicdrv_mib.o rtl8367c_asicdrv_mirror.o \
			rtl8367c_asicdrv_misc.o rtl8367c_asicdrv_oam.o rtl8367c_asicdrv_phy.o rtl8367c_asicdrv_port.o \
			rtl8367c_asicdrv_portIsolation.o rtl8367c_asicdrv_qos.o rtl8367c_asicdrv_rldp.o \
			rtl8367c_asicdrv_rma.o rtl8367c_asicdrv_scheduling.o rtl8367c_asicdrv_storm.o \
			rtl8367c_asicdrv_svlan.o rtl8367c_asicdrv_trunking.o rtl8367c_asicdrv_unknownMulticast.o \
			rtl8367c_asicdrv_vlan.o smi.o stat.o storm.o svlan.o trap.o trunk.o vlan.o rtl8367c_drv.o
else
	rtl8365mb-objs := $(RTL8365MB_PRE)/rtl8365mb.o
endif

clean-files += $(RTL8365MB_SRC)/*.o $(RTL8365MB_SRC)/*.o.cmd
