include ../.config

EXEC = networkmap
OBJS = function.o networkmap.o

CFLAGS  += -Wall -DASUS -DBCMNVRAM -I. -I$(TOP)/shared -I$(TOP)/kernel_header/include -I$(SRCBASE)/include
CFLAGS  += -s -O2 -DNO_PARALLEL

ifeq ($(RTCONFIG_BWDPI),y)
CFLAGS += -I$(TOP)/bwdpi/ -I$(TOP)/sqlite -I$(TOP)/httpd
endif

LDFLAGS += -L$(TOP)/shared -lshared -L$(TOP)/nvram${BCMEX} -lnvram 

ifeq ($(RTCONFIG_BCMARM),y)
LDFLAGS += -lgcc_s
endif
ifeq ($(RTCONFIG_BWDPI),y)
LDFLAGS += -L$(TOP)/bwdpi -lbwdpi
endif
ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

ifeq ($(RTCONFIG_NOTIFICATION_CENTER),y)
CFLAGS  += -I$(TOP)/sqlite
CFLAGS  += -I$(TOP)/nt_center/lib
CFLAGS  += -DASUSWRT_SDK
LDFLAGS += -L$(TOP)/nt_center/lib/ -lnt
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3
LDFLAGS += -lpthread
endif

include $(SRCBASE)/.config

#CFLAGS	+= -DDEBUG #-DDEBUG_MORE #-DDEBUG_FUNCTION
CFLAGS += -DNMP_DB
CFLAGS += -DBTN_SETUP

all: $(EXEC) asusdiscovery arpstorm

$(EXEC): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

arpstorm: ../shared/shutils.o arpstorm.o
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

asusdiscovery: ../shared/shutils.o packet.o ASUS_Discovery.o ASUS_Discovery_Debug.o
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

install:
	install -D $(EXEC) $(INSTALLDIR)/usr/sbin/$(EXEC)
	$(STRIP) $(INSTALLDIR)/usr/sbin/$(EXEC)
	install -D arpstorm $(INSTALLDIR)/usr/sbin/arpstorm
	$(STRIP) $(INSTALLDIR)/usr/sbin/arpstorm
	install -D asusdiscovery $(INSTALLDIR)/usr/sbin/asusdiscovery
	$(STRIP) $(INSTALLDIR)/usr/sbin/asusdiscovery

clean: 
	rm -rf *.o $(EXEC) *~ arpstorm asusdiscovery
