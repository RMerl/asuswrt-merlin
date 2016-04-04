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
OBJS += main.o traffic_limiter.o traffic_limiter_hook.o

ifeq ($(RTCONFIG_BCMARM),y)
vpath %.c $(SRCBASE)/shared
endif

ifeq ($(wildcard $(SRCBASE)/router/traffic_limiter/*.c),)
all:
	-cp -f prebuilt/traffic_limiter traffic_limiter
	-cp -f prebuilt/libtraffic_limiter.so libtraffic_limiter.so 
else
all: traffic_limiter libtraffic_limiter.so 
endif

traffic_limiter: $(OBJS)
	@echo " [traffic_limiter] CC $@"
	@$(CC) -o $@ $^ $(LD1)
	@$(STRIP) traffic_limiter

libtraffic_limiter.so: traffic_limiter_hook.o
	@echo " [traffic_limiter] LD $@"
	@$(LD) $(LD2) -shared -o $@ $^

install: all
	@echo "[traffic_limiter] Installing..."
	@install -D traffic_limiter $(INSTALLDIR)/sbin/traffic_limiter
	@$(STRIP) $(INSTALLDIR)/sbin/traffic_limiter
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libtraffic_limiter.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libtraffic_limiter.so

%.o: %.c
	@echo " [traffic_limiter] CC $@"
	@$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o traffic_limiter libtraffic_limiter.so
