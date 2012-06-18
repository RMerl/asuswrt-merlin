#
# Makefile ET
#

include ../common.mak

CFLAGS	+= -I. -I$(TOP)/shared -I$(TOP)/busybox -I$(SRCBASE)/include -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2 -DBCMWPA2
LDFLAGS	+= -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared

OBJS := et.o
vpath %.c $(SRCBASE)/et/linux

ifneq ($(wildcard ./prebuilt),)
all:
	-cp prebuilt/et et
else
all: et
endif

clean:
	rm -f *.o et .*.depend

install: all
	install -d $(INSTALLDIR)/usr/sbin
	install et $(INSTALLDIR)/usr/sbin
	$(STRIP) $(INSTALLDIR)/usr/sbin/et

et: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

