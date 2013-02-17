## Process this file with automake to produce Makefile.in

CC=$(CC_FOR_BUILD)
LIBTOOL = @LIBTOOL@ --tag=CC

noinst_PROGRAMS = testlk
testlk_SOURCES = testlk.c
testlk_CFLAGS=$(CFLAGS_FOR_BUILD)
testlk_CPPFLAGS=$(CPPFLAGS_FOR_BUILD)
testlk_LDFLAGS=$(LDFLAGS_FOR_BUILD)

MAINTAINERCLEANFILES = Makefile.in
