# Under Solaris, you need to 
#    CFLAGS += -xO2 -Xc
#    LDLIBS += -lnsl -lsocket
# Some versions of Linux may need
#    CFLAGS += -D_GNU_SOURCE
# To cross-compile
#    CC = arm-linux-gcc

CFLAGS += -Wall -O

CFLAGS += -DASUS
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram
INSTALL = install

all: ntpclient

ntpclient: ntpclient.o phaselock.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: ntpclient
	$(STRIP) ntpclient
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) ntpclient $(INSTALLDIR)/usr/sbin

clean:
	rm -f ntpclient *.o
