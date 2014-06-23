include ../common.mak

OBJS = push_log.o
PROGS = libpush_log.so push_log

CFLAGS += -fPIC $(EXTRACFLAGS) -Wall
CFLAGS += -I. -I$(LINUXDIR)/include -I$(TOP)/shared -I$(SRCBASE)/include -I$(TOP)/wb/ws_src

LDFLAGS = -L. -lpush_log
LDFLAGS += -L$(TOP)/nvram$(BCMEX) -lnvram
LDFLAGS += -L$(TOP)/shared -lshared
LDFLAGS += -L$(TOP)/openssl -lcrypto -lssl
LDFLAGS += -L$(TOP)/curl-7.21.7/lib/.libs -lcurl
LDFLAGS += -L$(TOP)/libxml2/.libs -lxml2
LDFLAGS += -L$(TOP)/wb -lws

ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
endif

ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
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
