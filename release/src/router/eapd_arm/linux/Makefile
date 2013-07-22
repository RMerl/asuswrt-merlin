#Linux Makefile

include $(SRCBASE)/router/.config

INCLUDE += -I. -I.. -I$(SRCBASE)/include -I$(SRCBASE)/shared -I$(SRCBASE)/router/shared -I$(SRCBASE)/router/nas

CFLAGS += $(INCLUDE)
CFLAGS += $(WLAN_ComponentIncPath)
CFLAGS += -DEAPD_WKSP_AUTO_CONFIG
#CFLAGS += -DNAS_GTK_PER_STA
CFLAGS += -Wall -DEAPDDUMP

CFLAGS  += -s


ifneq ($(LINUXDIR),)
CFLAGS += -Dlinux
endif

LDFLAGS += -L$(TOP)/nvram${BCMEX} -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared
LDFLAGS += -lgcc_s

vpath %.c $(SRCBASE)/router/eapd $(SRCBASE)/shared

OBJS = eapd.o eapd_linux.o
OBJS += nas_eap.o wps_eap.o ses_eap.o


#ifdef BCM_DCS
#ifeq ($(CONFIG_BCMDCS),y)
OBJS += dcs_eap.o
CFLAGS += -DBCM_DCS
#endif
#endif

#ifdef BCM_MEVENT
ifeq ($(CONFIG_BCM_MEVENT),y)
OBJS += mevent_eap.o
CFLAGS += -DBCM_MEVENT
endif
#endif

ifeq ($(wildcard $(SRCBASE)/router/eapd/*.c),)
all:
	-cp prebuild/eapd eapd
else
all: eapd
endif

eapd: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

install: all
	install -d $(INSTALLDIR)/bin
	install eapd $(INSTALLDIR)/bin
	$(STRIP) $(INSTALLDIR)/bin/eapd

clean:
	rm -f *.o eapd
