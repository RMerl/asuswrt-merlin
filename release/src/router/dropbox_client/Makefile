#***********************************************************************
#
# Makefile
#
#***********************************************************************

CFLAGS = -I$(TOP)/openssl/include -I$(TOP)/curl-7.21.7/include
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared -D_FILE_OFFSET_BITS=64 -DNVRAM_

LDFLAGS = -L$(TOP)/curl-7.21.7/lib/.libs -L$(TOP)/openssl -L$(TOP)/nvram${BCMEX} -L$(TOP)/shared -lshared

LIBS = -lpthread -lnvram -lshared -lcrypto -lssl -lcurl -lm

include ../common.mak
ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client
LIBS += -lqcsapi_client
endif
ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
endif

all: dropbox_client

dropbox_client:dropbox.o function.o list.o cJSON.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $^ -o $@
.PHONY:clean
clean:
	rm -rf *.o dropbox_client

