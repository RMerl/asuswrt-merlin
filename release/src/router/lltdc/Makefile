include ../.config
#include src/common.mk

VPATH = src
LD= $(CC)

CFLAGS += -I$(TOP)/shared
CFLAGS += -I$(SRCBASE)/include

LDFLAGS += -L$(TOP)/shared -lshared -L$(TOP)/nvram${BCMEX} -lnvram

ifeq ($(RTCONFIG_BCMARM),y)
LDFLAGS += -lgcc_s
endif

ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

OS_LAYER = osl-linux.c

all: lld2c

include src/common.mk

depend:
	$(CC) $(CFLAGS) -M $(DCFILES) >.depend
	$(CC) $(CFLAGS) -M $(TCFILES) >>.depend

#clean:
#	rm -f *.o lld2c

install: all
	@ echo " [lld2c] Installing..."
	install -D lld2c $(INSTALLDIR)/usr/sbin/lld2c
	$(STRIP) $(INSTALLDIR)/usr/sbin/lld2c

#-include .depend
