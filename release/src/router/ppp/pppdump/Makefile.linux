CFLAGS= -O -I../include/net
OBJS = pppdump.o bsd-comp.o deflate.o zlib.o

INSTALL= install

all:	pppdump

pppdump: $(OBJS)
	$(CC) -o pppdump $(OBJS)

clean:
	rm -f pppdump $(OBJS) *~

install:
	mkdir -p $(BINDIR) $(MANDIR)/man8
	$(INSTALL) -s -c pppdump $(BINDIR)
	$(INSTALL) -c -m 444 pppdump.8 $(MANDIR)/man8
