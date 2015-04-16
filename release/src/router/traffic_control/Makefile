include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/sqlite -I$(TOP)/rc -I$(TOP)/httpd
CFLAGS += -I$(TOP)/util-linux/include
CFLAGS += -idirafter$(LINUXDIR)/arch/arm/include -idirafter$(LINUXDIR)/include

LDFLAGS += $(EXTRALDFLAGS2)
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3
LDFLAGS += -lpthread

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

# ASUSWRT
OBJS += main.o traffic_control.o traffic_control_hook.o

ifeq ($(RTCONFIG_BCMARM),y)
vpath %.c $(SRCBASE)/shared
endif

ifeq ($(wildcard $(SRCBASE)/router/traffic_control/*.c),)
all:
	-cp -f prebuilt/traffic_control traffic_control
	-cp -f prebuilt/libtraffic_control.so libtraffic_control.so 
else
all: traffic_control libtraffic_control.so 
endif

traffic_control: $(OBJS)
	@echo " [traffic_control] CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)
	@$(STRIP) traffic_control

libtraffic_control.so: traffic_control_hook.o
	@echo " [traffic_control] LD $@"
	@$(LD) $(LDFLAGS) -shared -o $@ $^

install: all
	@echo "[traffic_control] Installing..."
	@install -D traffic_control $(INSTALLDIR)/sbin/traffic_control
	@$(STRIP) $(INSTALLDIR)/sbin/traffic_control
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libtraffic_control.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libtraffic_control.so

%.o: %.c
	@echo " [traffic_control] CC $@"
	@$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o traffic_control libtraffic_control.so
