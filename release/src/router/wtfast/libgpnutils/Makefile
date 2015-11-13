#
# This is the Makefile for the GPN utilities library.
#
# Copyright (C) 2014-2015, AAA Internet Publishing, Inc. (DBA WTFast)
#

LIB_VER_MAJOR = 1
LIB_VER_MINOR = 1
LIB_VER_MICRO = 0
LIB_VER_STRING = "\"1.1.0\""
CFLAGS += -DLIB_VER_MAJOR=$(LIB_VER_MAJOR) -DLIB_VER_MINOR=$(LIB_VER_MINOR) -DLIB_VER_MICRO=$(LIB_VER_MICRO) -DLIB_VER_STRING=$(LIB_VER_STRING)
SONAME = $(LIB_NAME).$(LIB_VER_MAJOR)

CFLAGS += --std=c99 -Wall -Wextra -pedantic-errors -O -g -fpic
LDFLAGS += -Wl,-soname,$(SONAME)

LIB_SRCS = kvp.c log.c net.c misc.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_NAME = libgpnutils.so

TEST_SRCS_1 = test_libgpnutils.c
TEST_OBJS_1 = $(TEST_SRCS_1:.c=.o)
TEST_SRCS_2 = test_gpn_tcp_listen.c
TEST_OBJS_2 = $(TEST_SRCS_2:.c=.o)

UPSTREAM_NAME = libgpnutils_$(LIB_VER_MAJOR).$(LIB_VER_MINOR).$(LIB_VER_MICRO)

LIB_TARGET = $(LIB_NAME).$(LIB_VER_MAJOR).$(LIB_VER_MINOR).$(LIB_VER_MICRO)
TEST_TARGETS = test_libgpnutils test_gpn_tcp_listen

OBJS = $(LIB_OBJS) $(TEST_OBJS_1) $(TEST_OBJS_2)
TARGETS = $(LIB_TARGET)

LIB = $(DESTDIR)/usr/lib
INC = $(DESTDIR)/usr/include

.PHONY: all clean tags install test

all: $(TARGETS)

$(LIB_TARGET): $(LIB_OBJS)
	$(CC) -shared $(LDFLAGS) -o $@ $(LIB_OBJS)

test_libgpnutils: $(TEST_OBJS_1) $(LIB_OBJS)
ifneq (,$(filter nocheck,$(DEB_BUILD_OPTIONS)))
	$(CC) -o $@ $(TEST_OBJS_1) $(LIB_OBJS)
endif

test_gpn_tcp_listen: $(TEST_OBJS_2) $(LIB_OBJS)
ifneq (,$(filter nocheck,$(DEB_BUILD_OPTIONS)))
	$(CC) -o $@ $(TEST_OBJS_2) $(LIB_OBJS)
endif

clean:
	rm -f $(TARGETS) $(OBJS)

tags:
	ctags -t $(LIB_SRCS) $(LIB_HDRS)

test: $(TEST_TARGETS)
ifneq (,$(filter nocheck,$(DEB_BUILD_OPTIONS)))
	./test_harness $(VERBOSE) $(TEST_PORT)
endif
	
install:
	install -d -o 0 -g 0 -m 755 $(LIB)
	install -d -o 0 -g 0 -m 755 $(INC)
	install -o 0 -g 0 -m 644 $(LIB_TARGET) $(LIB)
	install -o 0 -g 0 -m 644 libgpnutils.h $(INC)
	ln -sf ./$(LIB_TARGET) $(LIB)/$(SONAME)
	ln -sf ./$(LIB_TARGET) $(LIB)/$(LIB_NAME)

dist: clean
	tar -zcvf ../$(UPSTREAM_NAME).orig.tar.gz .

deb: dist
	git-buildpackage -uc -us -b

#
# Tribute to a long-forgotten hack...
#
love:
	@echo Not war?
