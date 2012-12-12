include ../common.mak

CFLAGS = -Wall -Os $(EXTRACFLAGS)
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -idirafter$(LINUXDIR)/include
CFLAGS += -I$(TOP)/openssl/include
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DDLM -DASUS_DDNS -DASUS_EXT -DBTN_SETUP -DWEB_REDIRECT

CFLAGS += -DOVERWRITE_DNS -DDEBUG_RCTEST

ifeq ($(RTCONFIG_WPS),y)
CFLAGS += -I$(SRCBASE)/wps/brcm_apps/include
endif

ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -I$(TOP)/shared/sysdeps/ralink -I$(TOP)/wireless_tools
endif

LDFLAGS = -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared -lcrypt -L$(TOP)/libbcmcrypto -lbcmcrypto

ifeq ($(RTCONFIG_USB),y)
CFLAGS += -I$(TOP)/libdisk
CFLAGS += -DU2EC
LDFLAGS += -L$(TOP)/libdisk -ldisk
endif

ifeq ($(RTAC66U), y)
CFLAGS += -DRTAC66U
#CFLAGS += -DACS_ONCE
endif

#CFLAGS += -DRC_BUILDTIME=$(shell date -d "now" +"%s")

LDFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections
ifeq ($(STATIC),1)
LDFLAGS += -static
endif

OBJS := rc.o init.o interface.o lan.o wireless.o wan.o pppd.o auth.o services.o mtd.o
OBJS += firewall.o ppp.o services.o usdsvr.o common.o
OBJS += watchdog.o ntp.o btnsetup.o qos.o udhcpc.o ate.o cpu.o
ifeq ($(RTCONFIG_FANCTRL),y)
OBJS += phy_tempsense.o
endif

ifeq ($(RTCONFIG_RALINK),y)
OBJS += init-ralink.o wps-ralink.o ralink.o wpsfix.o
ifeq ($(RTCONFIG_DSL),y)
OBJS += init-ralink-dsl.o 
endif
else
OBJS += init-broadcom.o wps-broadcom.o broadcom.o
endif

# TODO, speedtest

OBJS += wanduck.o
OBJS += tcpcheck.o

ifeq ($(RTCONFIG_USB),y)
OBJS += usb.o
OBJS += usbled.o
OBJS += usb_devices.o
endif

ifeq ($(RTCONFIG_JFFS2),y)
OBJS += jffs2.o
endif


ifeq ($(RTCONFIG_CIFS),y)
OBJS += cifs.o
endif

ifeq ($(RTCONFIG_SSH),y)
OBJS += ssh.o
endif

ifeq ($(RTCONFIG_PPTPD),y)
OBJS += vpn.o
endif

ifeq ($(RTCONFIG_ACCEL_PPTPD),y)
OBJS += vpn.o
endif

ifeq ($(RTCONFIG_OPENVPN),y)
OBJS += openvpn.o
endif

ifeq ($(RTCONFIG_OLD_PARENTALCTRL),y)
OBJS += parental_ctrl.o
endif

ifeq ($(RTCONFIG_PARENTALCTRL),y)
OBJS += pc.o
endif

ifeq ($(RTCONFIG_DSL),y)
OBJS += dsl.o
endif

ifeq ($(RTCONFIG_IPV6),y)
OBJS += ipv6aide.o
endif

ifeq ($(RTCONFIG_BCMWL6),y)
ifeq ($(RTCONFIG_PROXYSTA),y)
OBJS += psta_monitor.o
endif
endif

ifeq ($(RTCONFIG_WPS),y)
OBJS += wpsaide.o
endif

ifeq ($(RTCONFIG_SHP),y)
export CFLAGS += -DMEDIA_REVIEW
endif

vpath %.c sysdeps sysdeps/broadcom sysdeps/ralink

all: rc

rc: $(OBJS)
	@echo " [rc] CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

	$(SIZECHECK)
	$(CPTMP)

install: all
	@echo "[rc] Installing..."
	@install -D rc $(INSTALLDIR)/sbin/rc
	@$(STRIP) $(INSTALLDIR)/sbin/rc
	@chmod 0500 $(INSTALLDIR)/sbin/rc

	@cd $(INSTALLDIR)/sbin && ln -sf rc init
	@cd $(INSTALLDIR)/sbin && ln -sf rc console
	@cd $(INSTALLDIR)/sbin && ln -sf rc hotplug
	@cd $(INSTALLDIR)/sbin && ln -sf rc service
	@cd $(INSTALLDIR)/sbin && ln -sf rc rcheck
	@cd $(INSTALLDIR)/sbin && ln -sf rc radio
	@cd $(INSTALLDIR)/sbin && ln -sf rc led_ctrl
	@cd $(INSTALLDIR)/sbin && ln -sf rc reboot
	@cd $(INSTALLDIR)/sbin && ln -sf rc halt
#	@cd $(INSTALLDIR)/sbin && ln -sf rc gpio

	@cd $(INSTALLDIR)/sbin && ln -sf rc mtd-write
	@cd $(INSTALLDIR)/sbin && ln -sf rc mtd-erase
	@cd $(INSTALLDIR)/sbin && ln -sf rc mtd-unlock
	@cd $(INSTALLDIR)/sbin && ln -sf rc ots
	@cd $(INSTALLDIR)/sbin && ln -sf rc watchdog
	@cd $(INSTALLDIR)/sbin && ln -sf rc wpsfix
ifeq ($(RTCONFIG_FANCTRL),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc phy_tempsense
endif
ifeq ($(RTCONFIG_BCMWL6),y)
ifeq ($(RTCONFIG_PROXYSTA),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc psta_monitor
endif
endif
	@cd $(INSTALLDIR)/sbin && ln -sf rc usbled
	@cd $(INSTALLDIR)/sbin && ln -sf rc ntp
	@cd $(INSTALLDIR)/sbin && ln -sf rc autodet
	@cd $(INSTALLDIR)/sbin && ln -sf rc ddns_updated

#	@cd $(INSTALLDIR)/sbin && ln -sf rc wldist

ifeq ($(RTCONFIG_IPV6),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc dhcp6c-state
	@cd $(INSTALLDIR)/sbin && ln -sf rc ipv6aide
endif
ifeq ($(RTCONFIG_WPS),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc wpsaide
endif

#	ifeq ($(RTCONFIG_HEARTBEAT),y)
#		@cd $(INSTALLDIR)/sbin && ln -sf rc hb_connect
#		@cd $(INSTALLDIR)/sbin && ln -sf rc hb_disconnect
#		@cd $(INSTALLDIR)/sbin && ln -sf rc hb_idle
#	endif

ifeq ($(RTCONFIG_CIFS),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc mount-cifs
endif

ifeq ($(RTCONFIG_RALINK),y)
ifeq ($(RTCONFIG_DSL),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc 8367r
	@cd $(INSTALLDIR)/sbin && ln -sf rc convert_dsl_wan_settings
else
	@cd $(INSTALLDIR)/sbin && ln -sf rc 8367m
endif
endif

	@cd $(INSTALLDIR)/sbin && ln -sf rc wanduck
	@cd $(INSTALLDIR)/sbin && ln -sf rc tcpcheck
	@cd $(INSTALLDIR)/sbin && ln -sf rc auto_macclone
ifeq ($(RTCONFIG_USB),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc get_apps_name
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_sd
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_lp
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_sg
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_sr
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_tty
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_usbbcm
	@cd $(INSTALLDIR)/sbin && ln -sf rc asus_usb_interface
	@cd $(INSTALLDIR)/sbin && ln -sf rc ejusb
ifeq ($(RTCONFIG_DISK_MONITOR),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc disk_monitor
endif
endif
	@cd $(INSTALLDIR)/sbin && ln -sf rc run_app_script 
	@cd $(INSTALLDIR)/sbin && ln -sf rc restart_wireless
	@cd $(INSTALLDIR)/sbin && ln -sf rc ATE
ifeq ($(RTCONFIG_DSL),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc gen_ralink_config
endif
	@cd $(INSTALLDIR)/sbin && ln -sf rc run_telnetd 
	@cd $(INSTALLDIR)/sbin && ln -sf rc run_pptpd

ifeq ($(RTCONFIG_OPENVPN),y)
	@mkdir -p $(INSTALLDIR)/rom/openvpn
	@cp openvpn-updown.sh $(INSTALLDIR)/rom/openvpn/updown.sh
	@chmod 0755 $(INSTALLDIR)/rom/openvpn/updown.sh
endif

ifeq ($(RTCONFIG_PARENTALCTRL),y)
	@cd $(INSTALLDIR)/sbin && ln -sf rc pc
endif
	@cd $(INSTALLDIR)/sbin && ln -sf rc wlcscan
	@cd $(INSTALLDIR)/sbin && ln -sf rc wlcconnect
ifeq ($(RTAC66U), y)
#	@cd $(INSTALLDIR)/sbin && ln -sf rc acsd_restart_wl
endif
	@cd $(INSTALLDIR)/sbin && ln -sf rc setup_dnsmq
	@cd $(INSTALLDIR)/sbin && ln -sf rc add_multi_routes

	@cd $(INSTALLDIR)/sbin && ln -sf rc free_caches
	@cd $(INSTALLDIR)/sbin && ln -sf rc get_phy_status
	@cd $(INSTALLDIR)/sbin && ln -sf rc get_phy_speed

clean:
	rm -f rc *.o .*.depend

size: rc
	mipsel-uclibc-nm --print-size --size-sort rc

%.o: %.c .%.depend
	@echo " [rc] CC $@"
	@$(CC) $(CFLAGS) -c $<

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)



pswatch: pswatch.c
	$(CC) -o $@ $^
	$(STRIP) $@
	$(CPTMP)
