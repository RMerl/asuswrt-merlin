# $Id: Makefile,v 1.4 2012/04/18 20:45:33 nanard Exp $
# made for GNU Make (and BSD make)
CFLAGS = -Wall -g -DTEST
CFLAGS += -Wextra
EXECUTABLES = testobsdrdr testpfpinhole

all:	$(EXECUTABLES)

clean:
	rm -f *.o $(EXECUTABLES)

testobsdrdr:	testobsdrdr.o obsdrdr.o
	$(CC) $(CFLAGS) -o $@ $>

testpfpinhole:	testpfpinhole.o obsdrdr.o pfpinhole.o
	$(CC) $(CFLAGS) -o $@ $>

obsdrdr.o:	obsdrdr.c obsdrdr.h

pfpinhole.o:	pfpinhole.c pfpinhole.h

testobsdrdr.o:	testobsdrdr.c obsdrdr.h

testpfpinhole.o:	testpfpinhole.c pfpinhole.h

