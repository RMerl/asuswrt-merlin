include ../../common.mak

ifeq ($(RTCONFIG_DSL),y)
CFLAGS += -DRTCONFIG_DSL
endif

ifeq ($(ANNEX_B),y)
CFLAGS += -DRTCONFIG_DSL_ANNEX_B
endif

CFLAGS += -Wall -O

CFLAGS += -DASUS
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram
INSTALL = install

all: auto_det

auto_det: auto_det auto_det.c discover.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: auto_det
	$(STRIP) auto_det
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) auto_det $(INSTALLDIR)/usr/sbin

clean:
	rm -f auto_det *.o




