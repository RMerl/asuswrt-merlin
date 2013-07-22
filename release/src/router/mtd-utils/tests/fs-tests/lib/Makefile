
ifeq ($(origin CC),default)
CC = gcc
endif

CFLAGS := $(CFLAGS) -Wall -g -O2

LDFLAGS := $(LDFLAGS)

all: tests.o

tests.o: tests.h

clean:
	rm -f *.o

tests:
	echo
