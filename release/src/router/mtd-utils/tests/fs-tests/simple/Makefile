
ifeq ($(origin CC),default)
CC = gcc
endif

CFLAGS := $(CFLAGS) -Wall -g -O2 -I../lib

LDFLAGS := $(LDFLAGS)

TARGETS = test_1 \
	test_2 \
	ftrunc \
	orph \
	perf

all: $(TARGETS)

$(TARGETS): ../lib/tests.o

../lib/tests.o: ../lib/tests.h

clean:
	rm -f *.o $(TARGETS)

tests: all
	./test_1 --sync
	./test_2 --sync
	./ftrunc
	./orph --sync
	./perf
