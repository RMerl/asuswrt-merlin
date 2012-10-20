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

all: tp_init

tp_init: tp_init tp_init_main.c ra_reg_rw.c tp_msg.c scanner.c respipc.c nvram.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install:
	$(STRIP) tp_init
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) tp_init $(INSTALLDIR)/usr/sbin

clean:
	rm -f tp_init *.o




