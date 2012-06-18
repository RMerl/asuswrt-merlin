#
#	MatrixSSL example applications makefile
#   'make' builds debug (Default).
#   'make gold' builds optimized.
#
#	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved
#

CC 	= gcc
O 	= .o
SO  = .so
A   = .a
E	=

LIBRARIES	= ../src/libmatrixssl$(SO)
STATICS		= ../src/libmatrixsslstatic$(A)

REF_EXE 	= httpsReflector$(E)
REF_OBJS 	= httpsReflector$(O) sslSocket$(O)

CLIENT_EXE	= httpsClient$(E)
CLIENT_OBJS = httpsClient$(O) sslSocket$(O)

#
# This is set by the debug target below
#
ifdef MATRIXSSL_DEBUG
DFLAGS  = -g -Wall -DDEBUG
else
#DFLAGS	= -Os
DFLAGS	= -O3
endif

ifdef MATRIXSSL_PROFILE
DFLAGS	+= -g -pg
endif

debug:
	@$(MAKE) compile "MATRIXSSL_DEBUG = 1"

profile:
	@$(MAKE) compile "MATRIXSSL_PROFILE = 1"

gold:
	@$(MAKE) compile

default: debug

#
#   Override variables for compilation on x86-64 (AMD64, Nacona, etc)
#
ifeq ($(shell uname -m),x86_64)
DFLAGS	+= -march=k8 -fPIC -DFP_64BIT -DTFM_X86_64 -DRDTSC
endif

#
#	Compile options
#
CFLAGS  = $(DFLAGS) -DLINUX
LDFLAGS = -lc

#
#   Override variables for compilation on Mac OS X (Darwin)
#
ifeq ($(shell uname),Darwin)
CC  = cc
SO  = .dylib
CFLAGS += -DOSX -isystem -I/usr/include
LDFLAGS =
MACLINK = ln -f $(LIBRARIES) .
endif

#
#   Override variables for compilation on uClinux (example only)
#
ifdef UCLINUX
CC		= /opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-uclibc-gcc
STRIP	= /opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-uclibc-strip
endif

all: compile

compile: $(REF_OBJS) $(REF_EXE) $(CLIENT_OBJS) $(CLIENT_EXE)

#
# Manual dependencies
#
*.o: ../matrixSsl.h Makefile

$(REF_EXE):$(REF_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBRARIES)
	$(MACLINK)

$(CLIENT_EXE):$(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBRARIES)

#
#	Clean up all generated files
#
clean:
	rm -f $(REF_EXE) $(REF_OBJS) \
		$(CLIENT_EXE) $(CLIENT_OBJS) \
		*$(SO)
