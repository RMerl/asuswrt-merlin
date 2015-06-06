include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/sqlite -I$(TOP)/rc -I$(TOP)/httpd
CFLAGS += -I$(TOP)/util-linux/include
CFLAGS += -idirafter$(LINUXDIR)/arch/arm/include -idirafter$(LINUXDIR)/include

LDFLAGS += -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

# for QCA toolchain compile issue, xxxx-ld has no pthead and dl in openwrt-toolchain
ifeq ($(RTCONFIG_QCA),y)
EXTRACC += -lpthread -ldl
EXTRALD =
else
EXTRACC += -lpthread
EXTRALD += -lpthread
endif

LD1 += $(LDFLAGS) $(EXTRACC)
LD2 += $(LDFLAGS) $(EXTRALD)

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
	@$(CC) -o $@ $^ $(LD1)
	@$(STRIP) traffic_control

libtraffic_control.so: traffic_control_hook.o
	@echo " [traffic_control] LD $@"
	@$(LD) $(LD2) -shared -o $@ $^

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
