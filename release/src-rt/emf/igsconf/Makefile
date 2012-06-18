#
# $Id: Makefile,v 1.3 2008-08-12 17:43:48 Exp $
#

CROSS =
CC = $(CROSS)gcc
AR = $(CROSS)ar
STRIP = $(CROSS)strip

ALL_OBJS = igsu.o igsu_linux.o

CCFLAGS = -Os -Wall -I../../../include -I../../shared -I../../../include/emf/igs

ifneq ($(wildcard ./prebuilt),)
all:
	-cp prebuilt/igs igs
else
all: igs
endif

igs: $(ALL_OBJS)
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LDLIBS)
	$(STRIP) $@

$(ALL_OBJS): %.o: %.c
	@echo " "
	@echo "Making $<"
	$(CC) $(CCFLAGS) -c $<

install: all
	install -d $(INSTALLDIR)/igs/usr/sbin
	install -m 755 igs $(INSTALLDIR)/igs/usr/sbin

.PHONY:	clean
clean:
	-rm -f *.o *~ core igs
