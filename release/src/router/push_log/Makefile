include ../common.mak

OBJS = push_log.o
PROGS = libpush_log.so push_log

CFLAGS += -fPIC $(EXTRACFLAGS) -Wall
CFLAGS += -I. -I$(LINUXDIR)/include -I$(TOP)/shared

LDFLAGS = -L. -lpush_log
LDFLAGS += -L$(TOP)/nvram$(BCMEX) -lnvram -L$(TOP)/shared -lshared
ifeq ($(RTCONFIG_BCMARM),y)
LDFLAGS += -lgcc_s
endif

all: $(PROGS)

libpush_log.so: push_log.o
	$(LD) -shared -o $@ $^

push_log: libpush_log.so main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: all
	$(STRIP) $(PROGS)
	install -D libpush_log.so $(INSTALLDIR)/lib/libpush_log.so
	install -D push_log $(INSTALLDIR)/sbin/push_log

clean:
	rm -f *.o $(PROGS)
