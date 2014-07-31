#***********************************************************************
#
# Makefile
#
#***********************************************************************

#C = $(CC)
#TOP = $(TOP)

#CC = mipsel-uclibc-g++
CPPFLAGS = -I$(TOP)/libiconv-1.14/include -I$(TOP)/curl-7.21.7/include -I$(TOP)/openssl/include -I$(TOP)/libxml2/include
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DNVRAM_ -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared

LDFLAGS = -L$(TOP)/curl-7.21.7/lib/.libs -L$(TOP)/libxml2/.libs -L$(TOP)/libiconv-1.14/lib/.libs -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared -L$(TOP)/openssl

LIBS = -lpthread -liconv -lcurl -lcrypto -lssl -lxml2

include ../common.mak
ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client
LIBS += -lqcsapi_client
endif
ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
endif

objects = ftpclient.o api.o list.o
all: ftpclient

ftpclient: $(objects)
	$(CXX)  $(CPPFLAGS) $(LDFLAGS) $(LIBS) -o $@ $^

.PHONY: clean
clean:
	-rm -rf ftpclient $(objects)
