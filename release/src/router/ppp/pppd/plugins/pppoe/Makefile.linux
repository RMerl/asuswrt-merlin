CC	= gcc
CFLAGS	= -g  -I.. -I../.. -I../../../include -D_linux_=1 -fPIC
LDFLAGS	= -shared

all: pppoe.so pppoed


PLUGINDIR = $(LIBDIR)/plugins
PLUGINSRCS= pppoe.c libpppoe.c utils.c pppoehash.c pppoe_client.c \
		pppoe_relay.c pppoe_server.c pppd_utils.c
#
# include dependancies if present and backup if as a header file
ifeq (.depend,$(wildcard .depend))
include .depend
endif



pppoefwd: pppoefwd.o libpppoe.a 
	$(CC) -o $@ $^

pppoed: pppoed.o pppd_utils.o libpppoe.a 
	$(CC) -o $@ $^

libpppoe.a: pppoehash.o pppoe_client.o pppoe_relay.o pppoe_server.o \
	utils.o libpppoe.o 
	ar -rc $@ $^

pppoe.so: pppoe.o libpppoe.a
	$(CC) -o $@ $(LDFLAGS) $^

%.so: %.c
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $^

clean: 
	rm -f *.o *.so *.a pppoefwd pppoed


$(PLUGINDIR):
	$(INSTALL) -d -m 755 $@

depend:
	$(CPP) -M $(CFLAGS) $(PLUGINSRCS) >.depend
