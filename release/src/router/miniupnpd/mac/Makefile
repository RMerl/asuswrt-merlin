# made for GNU Make
CFLAGS = -Wall -g
EXECUTABLES = testgetifstats

all:	$(EXECUTABLES)

clean:
	rm -f *.o $(EXECUTABLES)

testmacrdr.o:	testmacrdr.c macrdr.h

testgetifstats:	testgetifstats.o getifstats.o

