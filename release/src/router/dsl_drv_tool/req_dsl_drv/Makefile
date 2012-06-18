CFLAGS += -Wall -O

CFLAGS += -DASUS -DASUS_NVRAM
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram
INSTALL = install

all: req_dsl_drv

req_dsl_drv: req_dsl_drv req_dsl_drv.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: req_dsl_drv
	$(STRIP) req_dsl_drv
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) req_dsl_drv $(INSTALLDIR)/usr/sbin

clean:
	rm -f req_dsl_drv *.o




