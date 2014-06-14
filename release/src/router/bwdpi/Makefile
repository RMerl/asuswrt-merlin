include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/rc

LDFLAGS = -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared
LDFLAGS += -ffunction-sections -fdata-sections

OBJS += iqos.o wrs.o stat.o dpi.o

ifeq ($(RTCONFIG_BCMARM),y)
vpath %.c $(SRCBASE)/shared
endif

all: libbwdpi.so

libbwdpi.so: $(OBJS)
	@echo " [bwdpi] LD $@"
	@$(LD) $(LDFLAGS) -shared -o $@ $^

install: all
	@echo "[bwdpi] Installing..."
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libbwdpi.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libbwdpi.so

%.o: %.c
	@echo " [bwdpi] CC $@"
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.so *.o
