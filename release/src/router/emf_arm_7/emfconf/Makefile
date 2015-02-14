#
# $Id: Makefile 403251 2013-05-20 05:15:55Z $
#

CROSS =
CC = $(CROSS)gcc
AR = $(CROSS)ar
STRIP = $(CROSS)strip

ALL_OBJS = emfu.o emfu_linux.o

CCFLAGS = -Os -Wall -I$(SRCBASE)/include ${WLAN_StdIncPathA} -I$(SRCBASE)/common/include -I$(SRCBASE)/shared -I$(SRCBASE)/include/emf/emf -I$(SRCBASE)/router/shared

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
	install -d $(INSTALLDIR)/emf$(BCMEX)$(EX7)/usr/sbin
	install -m 755 emf $(INSTALLDIR)/emf$(BCMEX)$(EX7)/usr/sbin

.PHONY:	clean
clean:
	-rm -f *.o *~ core emf
