
ifeq ($(origin CC),default)
CC = gcc
endif

CFLAGS := $(CFLAGS) -Wall -g -O2 -I../lib

LDFLAGS := $(LDFLAGS)

TARGETS = fstest_monitor free_space

all: $(TARGETS)

clean:
	rm -f *.o $(TARGETS)

tests: all
	./fstest_monitor
	./free_space > /dev/null
