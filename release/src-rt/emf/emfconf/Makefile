#
# $Id: Makefile,v 1.3 2008-08-12 17:43:47 Exp $
#

CROSS =
CC = $(CROSS)gcc
AR = $(CROSS)ar
STRIP = $(CROSS)strip

ALL_OBJS = emfu.o emfu_linux.o

CCFLAGS = -Os -Wall -I../../../include -I../../shared -I../../../include/emf/emf

ifneq ($(wildcard ./prebuilt),)
all:
	-cp prebuilt/emf emf
else
all: emf
endif

emf: $(ALL_OBJS)
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LDLIBS)
	$(STRIP) $@

$(ALL_OBJS): %.o: %.c
	@echo " "
	@echo "Making $<"
	$(CC) $(CCFLAGS) -c $<

install: all
	install -d $(INSTALLDIR)/emf/usr/sbin
	install -m 755 emf $(INSTALLDIR)/emf/usr/sbin

.PHONY:	clean
clean:
	-rm -f *.o *~ core emf
