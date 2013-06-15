include ../common.mak

CFLAGS = -Os -Wall $(EXTRACFLAGS)
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared -I$(TOP)/zlib
CFLAGS += -DASUS_DDNS -DTRANSLATE_ON_FLY -DFLASH_EMULATOR -DLinux -DWSC 


ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -DRalink_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/ralink -I$(TOP)/wireless_tools
endif

ifeq ($(RTN14U),y)
export CFLAGS += -DRTN14U 
endif

ifeq ($(RTCONFIG_HTTPS),y)
CFLAGS += -I$(TOP)/mssl
endif

ifeq ($(RTCONFIG_USB),y)
CFLAGS += -I$(TOP)/libdisk	
endif

ifeq ($(RTCONFIG_DNSMASQ),y)
CFLAGS += -DDNSMASQ
endif

OBJS = httpd.o cgi.o ej.o 
OBJS += web.o common.o nvram_f.o 
OBJS += aspbw.o initial_web_hook.o
OBJS += apps.o
OBJS += sysinfo.o
ifeq ($(CONFIG_RALINK),y)
OBJS += web-ralink.o
else
ifeq ($(RTCONFIG_BCMWL6),y)
OBJS += web-broadcom-wl6.o
else
OBJS += web-broadcom.o
endif
endif

ifeq ($(RTCONFIG_DSL),y)
OBJS += web-dsl.o
OBJS += web-dsl-upg.o
endif

LIBS = -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared

ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/shared/bcmwifi/include
LIBS += -lgcc_s
endif

ifeq ($(RTCONFIG_USB),y)
LIBS += -L$(TOP)/libdisk -ldisk
endif

ifeq ($(RTCONFIG_HTTPS),y)
LIBS += -L$(TOP)/mssl -lmssl
LIBS += -L$(TOP)/openssl -lssl -lcrypto
endif

ifeq ($(RTCONFIG_OPENVPN),y)
LIBS += -L$(TOP)/openssl -lssl -lcrypto
endif

vpath %.c sysdeps

all: httpd #test_apps

#test_apps: apps.c
#	$(CC) -DAPPS $(CFLAGS) $(LIBS) $^ -o $@

httpd: $(OBJS)
	@echo " [httpd] CC $@"
	@$(CC) -o $@ $(OBJS) $(LIBS)

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

%.o: %.c .%.depend
	@echo " [httpd] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
