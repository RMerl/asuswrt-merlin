include ../common.mak
include $(SRCBASE)/.config

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include
ifneq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(TOP)/kernel_header/include
else
CFLAGS += -I$(LINUXDIR)/include
CFLAGS += -D__EXPORTED_HEADERS__
endif

ifeq ($(RTCONFIG_SHP),y)
CFLAGS += -DSHP
endif

ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -I./sysdeps/ralink -I$(TOP)/wireless_tools/ -DWE_NOLIBM 
endif

ifeq ($(RTCONFIG_QCA),y)
CFLAGS += -I./sysdeps/qca -I$(TOP)/wireless_tools/ -DWE_NOLIBM
endif

ifeq ($(RTCONFIG_BCMWL6),y)
CFLAGS += -I$(SRCBASE)/shared/bcmwifi/include
CFLAGS += -I$(SRCBASE)/common/include
ifeq ($(RTCONFIG_BCM7)$(RTCONFIG_HSPOT),y)
CFLAGS += ${WLAN_StdIncPathA}
endif
else
CFLAGS += -I$(SRCBASE)/shared
endif

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client -I$(TOP)/httpd -I./sysdeps/qtn
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

ifeq ($(RTCONFIG_BCMSMP),y)
CFLAGS += -DSMP
endif

ifeq ($(RTCONFIG_5301X),y)
CFLAGS += -DBCM5301X
endif

include $(SRCBASE)/.config

ifeq ($(RTCONFIG_4352_5G),y)
CFLAGS += -DBCM4352
endif

ifeq (2_6_36,$(LINUX_VERSION))
CFLAGS += -DAUTOFW_PORT_DEPRECATED
endif

CFLAGS += -DTRAFFIC_MGMT

ifeq ($(RTCONFIG_BCM7)$(RTCONFIG_HSPOT),y)
CFLAGS += -DD11AC_IOTYPES
endif

ifeq ($(or $(RTAC68A),$(RT4GAC68U)),y)
CFLAGS += -DRTAC68U
endif

OBJS = shutils.o linux_timer.o defaults.o model.o rtstate.o boardapi.o
OBJS += misc.o version.o files.o strings.o process.o 
OBJS += bin_sem_asus.o semaphore.o pids.o notify_rc.o discover.o
OBJS += base64.o
OBJS += nvparse.o
ifeq ($(RTCONFIG_BCM7),y)
OBJS += et_linux.o bcmwifi_channels.o
else ifeq ($(RTCONFIG_HSPOT),y)
OBJS += bcmwifi_channels.o
endif
ifeq ($(RTCONFIG_BCM_7114),y)
OBJS += confmtd_utils.o
ifeq ($(RTCONFIG_GMAC3),y)
OBJS += et_linux.o
endif
endif

ifeq ($(RTCONFIG_RALINK),y)
OBJS += $(TOP)/wireless_tools/iwlib.o
OBJS += flash_mtd.o api-ralink.o
ifeq ($(RTCONFIG_RALINK_RT3052),y)
OBJS += ra3052.o
endif
ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P)$(RTN300)$(RTN54U)$(RTAC1200HP)$(RTN56UB1)$(RTN56UB2)$(RTAC54U),y)
CFLAGS += -DRA_ESW
OBJS += mt7620.o
else
OBJS += rtkswitch.o
endif

else ifeq ($(RTCONFIG_QCA),y)
# Qualcomm Atheros
OBJS += $(TOP)/wireless_tools/iwlib.o
OBJS += flash_mtd.o api-qca.o qca8337.o
else
# Broadcom
OBJS += wl.o wl_linux.o
OBJS += broadcom.o api-broadcom.o
ifeq ($(RTCONFIG_DSL_TCLINUX),y)
OBJS += api-tclinux.o
endif
ifeq ($(RTCONFIG_EXT_RTL8365MB),y)
CFLAGS += -I$(LINUXDIR)/drivers/char/rtl8365mb
OBJS += rtl_8365mb.o
endif
ifeq ($(RTCONFIG_EXT_RTL8370MB),y)
CFLAGS += -I$(LINUXDIR)/drivers/char/rtl8365mb
OBJS += rtl_8365mb.o
endif
endif

ifeq ($(RTCONFIG_USB),y)
OBJS += usb.o

ID_CFLAGS += -I$(TOP)/busybox/include
ID_CFLAGS += -I$(TOP)/busybox/util-linux/volume_id
ID_CFLAGS += -include $(TOP)/busybox/include/autoconf.h
ID_CFLAGS += -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
#ID_CFLAGS += -fdata-sections -ffunction-sections
ID_OBJS  = $(TOP)/busybox/util-linux/volume_id/util.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/linux_swap.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/ext.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/fat.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/ntfs.o
ID_OBJS += $(if $(HFS),$(TOP)/busybox/util-linux/volume_id/hfs.o)
ID_OBJS += volume_id.o
#OBJS += volume_id.lib.o
OBJS += $(notdir $(ID_OBJS))

ifeq ($(RTCONFIG_USB_MODEM),y)
OBJS += at_cmd.o
endif
endif

ifeq ($(RTCONFIG_BCMARM),y)
OBJS += ifaddrs.o
endif

ifeq ($(RTCONFIG_QTN),y)
OBJS += web-qtn.o
OBJS += rpc_raw.o
OBJS += rpc_raw_clnt.o
#OBJS += find_host_addr.o
endif

ifeq ($(RTCONFIG_BLINK_LED),y)
CFLAGS	+= -I$(TOP)/bled
OBJS	+= bled.o
endif

ifeq ($(RTCONFIG_QCA_PLC_UTILS),y)
OBJS	+= $(if $(wildcard plc_utils.c),plc_utils.o,prebuild/plc_utils.o)
endif

ifeq ($(RTCONFIG_TCODE),y)
OBJS    += tcode.o
endif

ifeq ($(CONFIG_BCMWL5),y)
ifneq ($(RTCONFIG_DSL),y)
CFLAGS += -DTRX_NEW
endif
OBJS += private.o
OBJS += wlif_utils.o UdpLib.o bcmutils.o
OBJS += bcmwifi_channels.o
endif

vpath %.c sysdeps $(SRCBASE)/shared/ $(WLAN_ComponentSrcDirs)
ifeq ($(RTCONFIG_RALINK),y)
vpath %.c sysdeps/ralink
else ifeq ($(RTCONFIG_QCA),y)
vpath %.c sysdeps/qca
else ifeq ($(RTCONFIG_QTN),y)
vpath %.c sysdeps sysdeps/broadcom sysdeps/qtn $(SRCBASE)/shared/ $(SRCBASE)/shared/bcmwifi/src $(WLAN_StdSrcDirsA) $(WLAN_ComponentSrcDirs) $(TOP)/libqcsapi_client/qcsapi_rpc_common/client  $(TOP)/libqcsapi_client/qcsapi_rpc_common/common
else
vpath %.c sysdeps/broadcom  $(SRCBASE)/shared/bcmwifi/src
endif

all: libshared.so libshared.a


libshared.so: $(OBJS)
	@echo " [shared] LD $@"
	@$(LD) $(LDFLAGS) -shared -o $@ $^

	$(SIZECHECK)
	$(CPTMP)

libshared.a: $(OBJS)
	@echo " [shared] AR $@"
	@rm -f $@
	@$(AR) cru $@ $^

install: all
	@echo " [shared] Installing as $(INSTALLDIR)/usr/lib/libshared.so"
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libshared.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libshared.so

clean:
	rm -f *.o *.so *.a .*.depend *.prep sysdeps/*.o sysdeps/broadcom/*.o sysdeps/ralink/*.o sysdeps/qtn/*.o

%.o: prebuild/%.o
	@echo " [shared] cp $@"
	@cp -f $< $@

ifeq ($(RTCONFIG_USB),y)
vpath %.c $(sort $(dir $(ID_OBJS)))

$(notdir $(ID_OBJS)): %.o: %.c .%.depend
	@echo " [shared] CC $@"
	@$(CC) $(CFLAGS) $(ID_CFLAGS) -o $@ -c $<

$(patsubst %.o,.%.depend,$(notdir $(ID_OBJS))): .%.depend: %.c
	@$(CC) $(CFLAGS) $(ID_CFLAGS) -M $< > $@

# ld -r --gc-sections is broken, so don't use it for now
#volume_id.lib.o: $(notdir $(ID_OBJS))
#	@echo " [shared] LD $@"
#	@$(LD) -r -e find_label_or_uuid --gc-sections -o $@ $(notdir $(ID_OBJS))
endif

%.o: %.c .%.depend
	@echo " [shared] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

.defaults.depend .tcode.depend: .%.depend: %.c %.prep
	@$(CC) $(CFLAGS) -M $< > $@

%.prep: %.c
	@$(CC) $(CFLAGS) -E $< > $@

-include $(OBJS:%.o=.%.depend)
