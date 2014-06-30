include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/sqlite -I$(TOP)/bwdpi -I$(TOP)/rc -I$(TOP)/httpd
CFLAGS += -I$(TOP)/util-linux/include
CFLAGS += -idirafter$(LINUXDIR)/arch/arm/include -idirafter$(LINUXDIR)/include

LDFLAGS += $(EXTRALDFLAGS2)
LDFLAGS += -L$(TOP)/bwdpi -lbwdpi
LDFLAGS += -lpthread $(TOP)/sqlite/.libs/libsqlite3.a

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

# ASUSWRT
OBJS += main.o table.o

ifeq ($(RTCONFIG_BCMARM),y)
vpath %.c $(SRCBASE)/shared
endif

ifeq ($(wildcard $(SRCBASE)/router/bwdpi_sqlite/*.c),)
all:
	-cp -f prebuilt/bwdpi_sqlite bwdpi_sqlite
else
all: bwdpi_sqlite
endif

bwdpi_sqlite: $(OBJS)
	@echo " [bwdpi_sqlite] CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)
	@$(STRIP) bwdpi_sqlite

install: all
	@echo "[bwdpi_sqlite] Installing..."
	@install -D bwdpi_sqlite $(INSTALLDIR)/sbin/bwdpi_sqlite
	@$(STRIP) $(INSTALLDIR)/sbin/bwdpi_sqlite

%.o: %.c
	@echo " [bwdpi_sqlite] CC $@"
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o bwdpi_sqlite
