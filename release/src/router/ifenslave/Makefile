include ../common.mak
include $(SRCBASE)/.config

OBJS = ifenslave.o
PROGS = ifenslave

CFLAGS += $(EXTRACFLAGS)
CFLAGS += -Wall -Wstrict-prototypes -Os -I$(LINUXDIR)/include


all: $(PROGS)


$(PROGS): ifenslave.c
	$(CC) $(CFLAGS) $^ -o $@

install: all
	$(STRIP) $(PROGS)
	install -D $(PROGS) $(INSTALLDIR)/bin/$(PROGS)

clean:
	rm -f *.o $(PROGS)
