include ../common.mak

CFLAGS += -O3 -Wall $(EXTRACFLAGS)
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared -I$(TOP)/zlib
CFLAGS += -I$(TOP)/push_log
CFLAGS += -DASUS_DDNS -DTRANSLATE_ON_FLY -DFLASH_EMULATOR -DLinux -DWSC 

ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -DRalink_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/ralink -I$(TOP)/wireless_tools
endif

ifeq ($(RTCONFIG_QCA),y)
CFLAGS += -DQCA_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/qca -I$(TOP)/wireless_tools
endif

include $(SRCBASE)/.config

ifeq ($(RTN11P)$(RTN300),y)
CFLAGS += -DBAND_2G_ONLY
endif

ifeq ($(RTN14U),y)
CFLAGS += -DBAND_2G_ONLY
endif

ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P)$(RTN300)$(RTN54U)$(RTAC1200HP)$(RTN56UB1)$(RTAC54U),y)
CFLAGS += -DRA_ESW
endif

ifeq ($(RTCONFIG_USB),y)
CFLAGS += -I$(TOP)/libdisk	
endif

ifeq ($(RTCONFIG_DNSMASQ),y)
CFLAGS += -DDNSMASQ
endif

ifeq ($(RTCONFIG_BWDPI),y)
CFLAGS += -I$(TOP)/bwdpi/ -I$(TOP)/sqlite -I$(TOP)/bwdpi_sqlite
endif

ifeq ($(RTCONFIG_TRAFFIC_CONTROL),y)
CFLAGS += -I$(TOP)/traffic_control
CFLAGS += -I$(TOP)/sqlite
endif

OBJS = httpd.o cgi.o ej.o 
OBJS += web.o common.o nvram_f.o 
OBJS += aspbw.o initial_web_hook.o
OBJS += apps.o
OBJS += sysinfo.o data_arrays.o

ifeq ($(CONFIG_RALINK),y)
OBJS += web-ralink.o
else ifeq ($(CONFIG_QCA),y)
OBJS += web-qca.o
else
ifeq ($(RTCONFIG_BCMWL6),y)
OBJS += web-broadcom-wl6.o
ifeq ($(RTCONFIG_QTN),y)
OBJS += find_host_addr.o
endif
else
OBJS += web-broadcom.o
endif
endif

ifeq ($(RTCONFIG_DSL),y)
OBJS += web-dsl.o
ifeq ($(RTCONFIG_RALINK),y)
OBJS += web-dsl-upg.o
endif
endif

ifeq ($(RTCONFIG_GEOIP),y)
OBJS += geoiplookup.o
endif

ifeq ($(RTCONFIG_HTTPS),y)
OBJS += pwenc.o
endif

LIBS = -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared
# for ceil()
LIBS += -lm

ifeq ($(RTCONFIG_PUSH_EMAIL),y)
LIBS += -L$(TOP)/openssl -lcrypto -lssl -ldl
LIBS += -L$(TOP)/curl-7.21.7/lib/.libs -lcurl
LIBS += -L$(TOP)/libxml2/.libs -lxml2
LIBS += -L$(TOP)/wb -lws
LIBS += -L$(TOP)/push_log -lpush_log
endif

ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/shared/bcmwifi/include
CFLAGS += -I$(SRCBASE)/common/include
LIBS += -lgcc_s
ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client -I$(TOP)/shared/sysdeps/qtn
LIBS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif
endif

ifeq ($(RTCONFIG_USB),y)
LIBS += -L$(TOP)/libdisk -ldisk
endif

ifeq ($(RTCONFIG_HTTPS),y)
CFLAGS += -I$(TOP)/mssl
CFLAGS += -I$(TOP)/openssl/include
LIBS += -L$(TOP)/mssl -lmssl
LIBS += -L$(TOP)/openssl -lssl -lcrypto -ldl
endif

ifeq ($(RTCONFIG_OPENVPN),y)
OBJS += openvpn_options.o
LIBS += -L$(TOP)/openssl -lssl -lcrypto -ldl
endif

CFLAGS += -I$(TOP)/json-c
LIBS += -L$(TOP)/json-c/.libs -ljson-c

ifeq ($(RTCONFIG_BWDPI),y)
LIBS += -L$(TOP)/bwdpi -lbwdpi
LIBS += -L$(TOP)/bwdpi_sqlite -lbwdpi_sql
LIBS += -L$(TOP)/sqlite/.libs -lsqlite3
LIBS += -lpthread
endif

ifeq ($(RTCONFIG_TRAFFIC_CONTROL),y)
LIBS += -L$(TOP)/traffic_control -ltraffic_control
LIBS += -L$(TOP)/sqlite/.libs -lsqlite3
endif

ifeq ($(RTCONFIG_GEOIP),y)
CFLAGS += -I$(TOP)/GeoIP-1.6.2/libGeoIP
LIBS += -L$(TOP)/GeoIP-1.6.2/libGeoIP/.libs -lGeoIP
endif

ifeq ($(RTCONFIG_EXT_RTL8365MB),y)
CFLAGS += -I$(LINUXDIR)/drivers/char/rtl8365mb
endif

ifeq ($(RTCONFIG_QCA),y)
LIBS += -lpthread -ldl
endif

export CFLAGS
ifeq ($(RTCONFIG_QTN),y)
vpath %.c sysdeps $(TOP)/libqcsapi_client/qcsapi_rpc_common/client $(TOP)/shared/sysdeps/qtn
else
vpath %.c sysdeps
endif

all: httpd #test_apps

#test_apps: apps.c
#	$(CC) -DAPPS $(CFLAGS) $(LIBS) $^ -o $@

httpd: $(OBJS)
	@echo " [httpd] CC $@"
	$(CC) -o $@ $(OBJS) $(LIBS)

	$(SIZECHECK)
	$(CPTMP)


install: all
	@echo " [httpd] Installing to $(INSTALLDIR)"
	@install -D httpd $(INSTALLDIR)/usr/sbin/httpd
	@$(STRIP) $(INSTALLDIR)/usr/sbin/httpd
	@chmod 0500 $(INSTALLDIR)/usr/sbin/httpd
	#@install -m 0755 test_apps $(INSTALLDIR)/usr/sbin
	#@$(STRIP) $(INSTALLDIR)/usr/sbin/test_apps

clean:
	rm -f httpd *.o .*.depend test_apps

size: httpd
	mipsel-uclibc-nm --print-size --size-sort httpd

# handle prebuilt object here
ifneq ($(wildcard ./prebuild/),)
ifeq ($(RTCONFIG_HTTPS),y)
pwenc.o:
	@-cp -f ./prebuild/pwenc$(BCMEX).o pwenc.o
endif
endif

%.o: %.c .%.depend
	@echo " [httpd] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
