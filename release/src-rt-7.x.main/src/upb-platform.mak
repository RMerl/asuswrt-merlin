include upb.inc
include .config
-include $(TOPPREFIX)/.config
-include $(LINUXDIR)/.config
-include $(SRCBASE)/router/.config

ifeq ($(RTCONFIG_BCMARM),y)
export  BCMEX := _arm
ifeq ($(RTCONFIG_BCM7),y)
export  EX7 := _7
endif
endif

pb-$(CONFIG_LIBBCM)		+= libbcm
pb-$(CONFIG_LIBUPNP)		+= libupnp$(BCMEX)$(EX7)
ifeq ($(RTCONFIG_BCMARM),y)
pb-y				+= acsd_arm$(EX7)
pb-y				+= eapd_arm$(EX7)/linux
pb-y				+= nas_arm$(EX7)/nas
pb-y				+= wps_arm$(EX7)
pb-y				+= utils_arm$(EX7)
pb-y				+= emf_arm$(EX7)/emfconf
pb-y				+= emf_arm$(EX7)/igsconf
pb-y				+= emf_arm$(EX7)
pb-y				+= wl_arm$(EX7)
pb-$(RTCONFIG_BCM7)		+= dhd
pb-$(RTCONFIG_HSPOT)		+= hspot_ap$(BCMEX)$(EX7)
pb-$(RTCONFIG_BCM7)		+= toad
pb-$(RTCONFIG_BCM7)		+= bsd
pb-$(RTCONFIG_BCMSSD)		+= ssd
pb-$(RTCONFIG_BWDPI)		+= bwdpi
pb-$(RTCONFIG_BWDPI)		+= bwdpi_sqlite
pb-$(RTCONFIG_PARAGON_NTFS)	+= ufsd
pb-$(RTCONFIG_PARAGON_HFS)	+= ufsd
pb-$(RTCONFIG_RTL8365MB)	+= rtl_bin
pb-y				+= wlconf_arm$(EX7)
else
pb-$(RTCONFIG_BCMWL6)		+= acsd
pb-y				+= eapd/linux
pb-y				+= nas/nas
pb-y				+= wps
pb-y				+= utils
pb-$(RTCONFIG_EMF)		+= emf/emfconf
pb-$(RTCONFIG_EMF)		+= emf/igsconf
pb-$(RTCONFIG_EMF)		+= emf
pb-y				+= wlconf
pb-$(RTCONFIG_BCMWL6)		+= igmp
endif
pb-$(RTCONFIG_TMOBILE)		+= radpd
pb-$(RTCONFIG_SNMPD)		+= net-snmp-5.7.2
pb-$(RTCONFIG_BCMWL6)		+= libnmp

all: $(pb-y) $(pb-m)

############################################################################
# Generate short variable for destination directory.
# NOTE: Only one variable can be defined in one target.
############################################################################
$(pb-y) $(pb-m): S=$(TOPPREFIX)/$@
$(pb-y) $(pb-m): D=$(PBTOPDIR)/$@/prebuilt


############################################################################
# Define special S or D variable here.
# NOTE: Only one variable can be defined in one target.
############################################################################
utils: D=$(PBTOPDIR)/$@/../../../$(SRCBASEDIR)/wl/exe/prebuilt
utils_arm$(EX7): D=$(PBTOPDIR)/$@/../../../$(SRCBASEDIR)/wl/exe/prebuilt
emf: S=$(LINUXDIR)/drivers/net/
emf_arm$(EX7): S=$(LINUXDIR)/drivers/net/
wl_arm$(EX7): D=$(PBTOPDIR)/$@/prebuilt
rtl_bin: D=$(PBTOPDIR)/$@/
dhd: D=$(PBTOPDIR)/$@/prebuilt

############################################################################
# Copy binary
############################################################################
acsd:
	$(call inst,$(S),$(D),acsd)

acsd_arm$(EX7):
	$(call inst,$(S),$(D),acsd)

bwdpi:
	$(call inst,$(S),$(D),libbwdpi.so)
	$(call inst,$(S),$(D),wrs)
	$(call inst,$(S),$(D),bwdpi.h)

bwdpi_sqlite:
	$(call inst,$(S),$(D),bwdpi_sqlite)
	$(call inst,$(S),$(D),libbwdpi_sql.so)

eapd/linux:
	$(call inst,$(S),$(D),eapd)

eapd_arm$(EX7)/linux:
	$(call inst,$(S),$(D),eapd)

libbcm:
	$(call inst,$(S),$(D),libbcm.so)

emf/emfconf:
	$(call inst,$(S),$(D),emf)

emf_arm$(EX7)/emfconf:
	$(call inst,$(S),$(D),emf)

emf/igsconf:
	echo ${S}
	$(call inst,$(S),$(D),igs)

emf_arm$(EX7)/igsconf:
	echo ${S}
	$(call inst,$(S),$(D),igs)

emf:
	$(call inst,$(S)/emf,$(D)/..,emf.o)
	$(call inst,$(S)/igs,$(D)/..,igs.o)

emf_arm$(EX7):
	$(call inst,$(S)/emf,$(D)/..,emf.o)
	$(call inst,$(S)/igs,$(D)/..,igs.o)

libupnp:
	echo ${S} ${D}
	$(call inst,$(S),$(D),libupnp.so)

nas/nas:
	$(call inst,$(S),$(D),nas)

nas_arm$(EX7)/nas:
	$(call inst,$(S),$(D),nas)

wlconf:
	$(call inst,$(S),$(D),wlconf)

wlconf_arm$(EX7):
	$(call inst,$(S),$(D),wlconf)

igmp:
	$(call inst,$(S),$(D),igmp)

radpd:
	echo ${S} ${D}
	$(call inst,$(S),$(D),radpd)

wps:
	echo ${S} ${D}
	$(call inst,$(S),$(D),wps_monitor)

wps_arm$(EX7):
	echo ${S} ${D}
	$(call inst,$(S),$(D),wps_monitor)

utils:
	echo ${S} ${D}
	$(call inst,$(S),$(D),wl)

utils_arm$(EX7):
	echo ${S} ${D}
	$(call inst,$(S),$(D),wl)

wl_arm$(EX7):
	$(call inst,$(S),$(D),wl_apsta.o)

rtl_bin:
	$(call inst,$(S),$(D),rtl8365mb.o)

dhd:
	$(call inst,$(S),$(D),dhd.o)

hspot_ap$(BCMEX)$(EX7):
	$(call inst,$(S),$(D),hspotap)
	$(call inst,$(S),$(D)/..,hspotap.c)

toad:
	$(call inst,$(S),$(D),toad)
	$(call inst,$(S),$(D),toast)

bsd:
	$(call inst,$(S),$(D),bsd)

ssd:
	$(call inst,$(S),$(D),ssd)

ufsd:
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,ufsd.ko)
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,jnl.ko)
ifeq ($(RTCONFIG_PARAGON_NTFS),y)
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,chkntfs)
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,mkntfs)
endif
ifeq ($(RTCONFIG_PARAGON_HFS),y)
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,chkhfs)
	$(call inst,$(S)/broadcom_arm,$(D)/../broadcom_arm,mkhfs)
endif

net-snmp-5.7.2:
	$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME),$(D)/../mibs,$(BUILD_NAME)-MIB.txt)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,administration.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,administration.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,guestNetwork.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,guestNetwork.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,lan.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,lan.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,quickInternetSetup.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,quickInternetSetup.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,trafficManager.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,trafficManager.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wan.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wan.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,firewall.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,firewall.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,ipv6.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,ipv6.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,parentalControl.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,parentalControl.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,systemLog.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,systemLog.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,vpn.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,vpn.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wireless.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wireless.h)

libnmp:
	$(call inst,$(S),$(D),libnmp.so)

.PHONY: all $(pb-y) $(pb-m)

