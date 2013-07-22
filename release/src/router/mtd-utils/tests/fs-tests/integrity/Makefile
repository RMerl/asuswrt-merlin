
ifeq ($(origin CC),default)
CC = gcc
endif

COMMON_HEADERS_DIR := ../../../include
LIBUBI_PATH = ../../../ubi-utils/
LIBUBI_HEADER_PATH = $(LIBUBI_PATH)/include

CFLAGS := $(CFLAGS) -Wall -g -O2 -I$(COMMON_HEADERS_DIR) -I$(LIBUBI_HEADER_PATH)

LDFLAGS := $(LDFLAGS)

TARGETS = integck

all: $(TARGETS)

# Compile ubilib
libubi.a:
	$(CC) $(CFLAGS) -c $(LIBUBI_PATH)/libubi.c -o libubi.o
	ar cr libubi.a libubi.o

$(TARGETS): libubi.a

# Disable optimizations to make it possible to use gdb comfortably
# Use -rdynamic to have stack backtraces
debug: libubi.a
	gcc $(CFLAGS) -O0 -D INTEGCK_DEBUG -rdynamic integck.c libubi.a -o integck

clean:
	rm -f *.o $(TARGETS) libubi.a
