include ../../common.mak

ifeq ($(RTCONFIG_DSL),y)
CFLAGS += -DRTCONFIG_DSL
endif

ifeq ($(ANNEX_B),y)
CFLAGS += -DRTCONFIG_DSL_ANNEX_B
endif

CFLAGS += -Wall -O
CFLAGS += -DASUS -DASUS_NVRAM
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include
CFLAGS += -I$(SRCBASE)/linux/linux-2.6.21.x/drivers/net/raeth.dsl
CFLAGS += -I$(SRCBASE)/linux/linux-2.6.21.x/include
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram
INSTALL = install

all: adslate

adslate: adslate adslate.c ra_reg_rw_ate.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: adslate
	$(STRIP) adslate
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) adslate $(INSTALLDIR)/usr/sbin

clean:
	rm -f adslate *.o




