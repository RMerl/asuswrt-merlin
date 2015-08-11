# $Id: Makefile,v 1.81 2011/06/21 15:24:14 nanard Exp $
# MiniUPnP Project
# http://miniupnp.free.fr/
# (c) 2005-2011 Thomas Bernard
# to install use :
# $ PREFIX=/tmp/dummylocation make install
# or
# $ INSTALLPREFIX=/usr/local make install
# or 
# make install (will go to /usr/bin, /usr/lib, etc...)
PJBASE=../pjproject-1.12
OS = $(shell uname -s)
include $(PJBASE)/build.mak
#CC ?= gcc
#CC = arm-linux-androideabi-gcc
CC = $(PJ_CC)
#AR = $(PJ_AR)
#AR = ar
#CFLAGS = -O -Wall -g -DDEBUG
CFLAGS ?= -O -Wall -DNDEBUG -DMINIUPNPC_SET_SOCKET_TIMEOUT -Wstrict-prototypes
CFLAGS += $(PJ_CFLAGS)
LDFLAGS += $(PJ_LDFLAGS)
# -DNO_GETADDRINFO
INSTALL = install
SH = /bin/sh
JAVA = java
# see http://code.google.com/p/jnaerator/
JNAERATOR = jnaerator-0.9.3.jar
#following libs are needed on Solaris
#LDLIBS=-lsocket -lnsl -lresolv

# APIVERSION is used to build SONAME
#APIVERSION = 8

SRCS = igd_desc_parse.c miniupnpc.c minixml.c minisoap.c miniwget.c \
       upnpc.c upnpcommands.c upnpreplyparse.c testminixml.c \
	   minixmlvalid.c testupnpreplyparse.c minissdpc.c \
	   upnperrors.c testigddescparse.c testminiwget.c \
       connecthostport.c portlistringparse.c receivedata.c

LIBOBJS = miniwget.o minixml.o igd_desc_parse.o minisoap.o \
          miniupnpc.o upnpreplyparse.o upnpcommands.o upnperrors.o \
          connecthostport.o portlistingparse.o receivedata.o

ifneq ($(OS), AmigaOS)
CFLAGS := -fPIC $(CFLAGS)
LIBOBJS := $(LIBOBJS) minissdpc.o
endif

OBJS = $(patsubst %.c,%.o,$(SRCS))

# HEADERS to install
HEADERS = miniupnpc.h miniwget.h upnpcommands.h igd_desc_parse.h \
          upnpreplyparse.h upnperrors.h miniupnpctypes.h \
          portlistingparse.h \
          declspec.h

#
# library names
LIB_NAME=libminiupnpc
LIBRARY = $(LIB_NAME).a
ifeq ($(OS), Darwin)
	YYY = $(CFLAGS)
	XXX = $(findstring armv7s, $(YYY))
	ifeq ($(XXX),)
		XXX = $(findstring armv7, $(YYY))
		ifeq ($(XXX),)
			XXX = $(findstring i386, $(YYY))
			ifeq ($(XXX),)
				XXX = $(findstring arm64, $(YYY))
				ifeq ($(XXX),)
					XXX = $(findstring x86_64, $(YYY))
				endif
			endif
		endif
	endif
	ifeq ($(XXX),)
		LIBRARY = $(LIB_NAME).a
	else
		LIBRARY = $(LIB_NAME)-$(XXX).a
	endif
endif
ANDROID_ARM_GCC=arm-linux-androideabi-gcc
ifeq ($(OS), Darwin)
  SHAREDLIBRARY = libminiupnpc.dylib
  SONAME = $(basename $(SHAREDLIBRARY)).$(APIVERSION).dylib
  CFLAGS := -DMACOSX -D_DARWIN_C_SOURCE $(CFLAGS)
else 
  SHAREDLIBRARY = libminiupnpc.so
# SONAME = $(SHAREDLIBRARY).$(APIVERSION)
  SONAME = $(SHAREDLIBRARY)
endif

ifeq ($(PJ_CC),$(ANDROID_ARM_GCC))
AR=arm-linux-androideabi-ar
endif

ANDROID_I686_GCC=i686-linux-android-gcc
ifeq ($(PJ_CC),$(ANDROID_I686_GCC))
AR=i686-linux-android-ar
endif


EXECUTABLES = upnpc-static
EXECUTABLES_ADDTESTS = testminixml minixmlvalid testupnpreplyparse \
			  testigddescparse testminiwget

TESTMINIXMLOBJS = minixml.o igd_desc_parse.o testminixml.o

TESTMINIWGETOBJS = miniwget.o testminiwget.o connecthostport.o receivedata.o

TESTUPNPREPLYPARSE = testupnpreplyparse.o minixml.o upnpreplyparse.o

TESTIGDDESCPARSE = testigddescparse.o igd_desc_parse.o minixml.o \
                   miniupnpc.o miniwget.o upnpcommands.o upnpreplyparse.o \
                   minisoap.o connecthostport.o receivedata.o \
                   portlistingparse.o

ifneq ($(OS), AmigaOS)
EXECUTABLES := $(EXECUTABLES) upnpc-shared
TESTMINIWGETOBJS := $(TESTMINIWGETOBJS) minissdpc.o
TESTIGDDESCPARSE := $(TESTIGDDESCPARSE) minissdpc.o
endif

# install directories
INSTALLPREFIX ?= $(PREFIX)/usr
INSTALLDIRINC = $(INSTALLPREFIX)/include/miniupnpc
INSTALLDIRLIB = $(INSTALLPREFIX)/lib
INSTALLDIRBIN = $(INSTALLPREFIX)/bin

FILESTOINSTALL = $(LIBRARY) $(EXECUTABLES)
ifneq ($(OS), AmigaOS)
FILESTOINSTALL := $(FILESTOINSTALL) $(SHAREDLIBRARY)
endif


.PHONY:	install clean depend all check everything \
	installpythonmodule
#	validateminixml validateminiwget

all:	$(LIBRARY) $(EXECUTABLES)

check:	validateminixml validateminiwget

everything:	all $(EXECUTABLES_ADDTESTS)

pythonmodule:	$(LIBRARY) miniupnpcmodule.c setup.py
	python setup.py build
	touch $@

installpythonmodule:	pythonmodule
	python setup.py install

validateminixml:	minixmlvalid
	@echo "minixml validation test"
	./minixmlvalid
	touch $@

validateminiwget:	testminiwget minihttptestserver testminiwget.sh
	@echo "miniwget validation test"
	./testminiwget.sh
	touch $@

clean:
	$(RM) $(LIBRARY) $(SHAREDLIBRARY) $(EXECUTABLES) $(OBJS) $(LIBOBJS) miniupnpcstrings.h
	# clean python stuff
	$(RM) pythonmodule validateminixml
	$(RM) -r build/ dist/
	#python setup.py clean

install:	$(FILESTOINSTALL)
	$(INSTALL) -d $(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(INSTALLDIRINC)
	$(INSTALL) -d $(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(INSTALLDIRLIB)
ifneq ($(OS), AmigaOS)
	$(INSTALL) -m 644 $(SHAREDLIBRARY) $(INSTALLDIRLIB)/$(SONAME)
	ln -fs $(SONAME) $(INSTALLDIRLIB)/$(SHAREDLIBRARY)
endif
	$(INSTALL) -d $(INSTALLDIRBIN)
ifeq ($(OS), AmigaOS)
	$(INSTALL) -m 755 upnpc-static $(INSTALLDIRBIN)/upnpc
else
	$(INSTALL) -m 755 upnpc-shared $(INSTALLDIRBIN)/upnpc
endif
	$(INSTALL) -m 755 external-ip.sh $(INSTALLDIRBIN)/external-ip

cleaninstall:
	$(RM) -r $(INSTALLDIRINC)
	$(RM) $(INSTALLDIRLIB)/$(LIBRARY)
	$(RM) $(INSTALLDIRLIB)/$(SHAREDLIBRARY)

depend:
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2>/dev/null

$(LIBRARY): $(LIBOBJS)
	$(AR) crs $@ $?

$(SHAREDLIBRARY):	$(LIBOBJS)
ifeq ($(OS), Darwin)
	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(SONAME) -o $@ $^ 
else
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$(SONAME) -o $@ $^
endif

upnpc-static:	upnpc.o $(LIBRARY) $(LDLIBS)
	$(CC) $(LDFLAGS) -o $@ $^

upnpc-shared:	upnpc.o $(SHAREDLIBRARY) $(LDLIBS)
	$(CC) $(LDFLAGS) -o $@ $^

testminixml:	$(TESTMINIXMLOBJS)

testminiwget:	$(TESTMINIWGETOBJS)

minixmlvalid:	minixml.o minixmlvalid.o

testupnpreplyparse:	$(TESTUPNPREPLYPARSE)

testigddescparse:	$(TESTIGDDESCPARSE)

miniupnpcstrings.h:	miniupnpcstrings.h.in updateminiupnpcstrings.sh
	$(SH) updateminiupnpcstrings.sh

jar:	$(SHAREDLIBRARY) 
	$(JAVA) -jar $(JNAERATOR) -library miniupnpc miniupnpc.h declspec.h upnpcommands.h upnpreplyparse.h igd_desc_parse.h miniwget.h upnperrors.h $(SHAREDLIBRARY) -package fr.free.miniupnp -o . -jar java/miniupnpc_$(OS).jar -v

minihttptestserver:	minihttptestserver.o

# DO NOT DELETE THIS LINE -- make depend depends on it.

igd_desc_parse.o: igd_desc_parse.h
miniupnpc.o: miniupnpc.h declspec.h igd_desc_parse.h minissdpc.h miniwget.h
miniupnpc.o: minisoap.h minixml.h upnpcommands.h upnpreplyparse.h
miniupnpc.o: portlistingparse.h miniupnpctypes.h connecthostport.h
miniupnpc.o: receivedata.h
minixml.o: minixml.h
minisoap.o: minisoap.h miniupnpcstrings.h
miniwget.o: miniupnpcstrings.h miniwget.h declspec.h connecthostport.h
miniwget.o: receivedata.h
upnpc.o: miniwget.h declspec.h miniupnpc.h igd_desc_parse.h upnpcommands.h
upnpc.o: upnpreplyparse.h portlistingparse.h miniupnpctypes.h upnperrors.h
upnpcommands.o: upnpcommands.h upnpreplyparse.h portlistingparse.h declspec.h
upnpcommands.o: miniupnpctypes.h miniupnpc.h igd_desc_parse.h
upnpreplyparse.o: upnpreplyparse.h minixml.h
testminixml.o: minixml.h igd_desc_parse.h
minixmlvalid.o: minixml.h
testupnpreplyparse.o: upnpreplyparse.h
minissdpc.o: minissdpc.h miniupnpc.h declspec.h igd_desc_parse.h codelength.h
upnperrors.o: upnperrors.h declspec.h upnpcommands.h upnpreplyparse.h
upnperrors.o: portlistingparse.h miniupnpctypes.h miniupnpc.h
upnperrors.o: igd_desc_parse.h
testigddescparse.o: igd_desc_parse.h minixml.h miniupnpc.h declspec.h
testminiwget.o: miniwget.h declspec.h
connecthostport.o: connecthostport.h
receivedata.o: receivedata.h
