PREFIX   = /usr/local
BINDIR   = $(PREFIX)/sbin
BUILDDIR = $(SRC)
DESTDIR  = 
CFLAGS   = -g -std=gnu99 -D_GNU_SOURCE -Wall -Wextra -pedantic -idirafter. $(EXT_CFLAGS)
LDFLAGS  = $(EXT_LDFLAGS)
LIBS     = -Wl,--as-needed -lresolv

INSTALL  = install
SRC      = src

top ?= $(CURDIR)

OBJS := 6relayd.o router.o dhcpv6.o ndp.o md5.o dhcpv6-ia.o stubs.o
HDRS := 6relayd.h router.h dhcpv6.h ndp.h md5.h list.h stubs.h

all: $(BUILDDIR) dummy
	@cd $(BUILDDIR) && $(MAKE) top="$(top)" -f $(top)/Makefile 6relayd

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/6relayd

install: all install-common

install-common :
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(BUILDDIR)/6relayd $(DESTDIR)$(BINDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJS:.o=.c) $(HDRS):
	ln -s $(SRC)/$@ .

.c.o:
	$(CC) $(CFLAGS) -c $<

6relayd: $(HDRS) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) 

.PHONY : all clean install install-common dummy
