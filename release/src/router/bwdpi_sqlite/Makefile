include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/sqlite -I$(TOP)/bwdpi -I$(TOP)/rc -I$(TOP)/httpd
CFLAGS += -I$(TOP)/util-linux/include
CFLAGS += -idirafter$(LINUXDIR)/arch/arm/include -idirafter$(LINUXDIR)/include

LDFLAGS += $(EXTRALDFLAGS2)
LDFLAGS += -L$(TOP)/bwdpi -lbwdpi
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3
#LDFLAGS += -lpthread

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

# ASUSWRT
OBJS += main.o table.o sqlite_stat.o

ifeq ($(RTCONFIG_BCMARM),y)
vpath %.c $(SRCBASE)/shared
endif

ifeq ($(wildcard $(SRCBASE)/router/bwdpi_sqlite/*.c),)
all:
	-cp -f prebuilt/bwdpi_sqlite bwdpi_sqlite
	-cp -f prebuilt/libbwdpi_sql.so libbwdpi_sql.so
else
all: bwdpi_sqlite libbwdpi_sql.so
endif

bwdpi_sqlite: $(OBJS)
	@echo " [bwdpi_sqlite] CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)
	@$(STRIP) bwdpi_sqlite

libbwdpi_sql.so: sqlite_stat.o
	@echo " [bwdpi] LD $@"
	@$(LD) $(LDFLAGS) -shared -o $@ $^

install: all
	@echo "[bwdpi_sqlite] Installing..."
	@install -D bwdpi_sqlite $(INSTALLDIR)/sbin/bwdpi_sqlite
	@$(STRIP) $(INSTALLDIR)/sbin/bwdpi_sqlite
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libbwdpi_sql.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libbwdpi_sql.so

%.o: %.c
	@echo " [bwdpi_sqlite] CC $@"
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o bwdpi_sqlite libbwdpi_sql.so
