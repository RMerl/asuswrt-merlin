CC	= gcc
CFLAGS	= -g -O2 -I.. -I../../include -fPIC
LDFLAGS	= -shared
INSTALL	= install

all:	minconn.so passprompt.so pppoe/pppoe.so

pppoe/pppoe.so:
	$(MAKE) -C pppoe $(MFLAGS) pppoe.so

minconn.so: minconn.c
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) minconn.c

passprompt.so: passprompt.c
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) passprompt.c

LIBDIR	= /usr/lib/pppd

install: minconn.so passprompt.so pppoe/pppoe.so
	version=`awk -F '"' '/VERSION/ { print $$2; }' ../patchlevel.h`; \
	$(INSTALL) -d $(LIBDIR)/$$version; \
	$(INSTALL) $? $(LIBDIR)/$$version

clean:
	rm -rf *.o *.so