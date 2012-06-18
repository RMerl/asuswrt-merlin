# $Id: Makefile,v 1.2 2010/03/08 17:10:37 nanard Exp $
# made for GNU Make (and BSD make)
CFLAGS = -Wall -g -DTEST
EXECUTABLES = testobsdrdr

all:	$(EXECUTABLES)

clean:
	rm -f *.o $(EXECUTABLES)

testobsdrdr:	testobsdrdr.o obsdrdr.o
	$(CC) $(CFLAGS) -o $@ $>

obsdrdr.o:	obsdrdr.c obsdrdr.h

testobsdrdr.o:	testobsdrdr.c obsdrdr.h

