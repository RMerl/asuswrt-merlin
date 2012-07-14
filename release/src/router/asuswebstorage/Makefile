#***********************************************************************
#
# Makefile
#
#***********************************************************************

#C = $(CC)
#TOP = $(TOP)
CFLAGS = -I$(TOP)/curl-7.21.7/include  -I$(TOP)/libxml2/include  -I$(TOP)/openssl/include
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared
#CFLAGS +=  -I. -I$(TOP)/shared
#CFLAGS = -I/home/gauss/oleg/optware/staging/opt/include -I/home/gauss/optware_bulid/APP/DownloadMaster2/build/curl/include  -I/home/gauss/oleg/optware/staging/opt/include/libxml2  
#LDFLAGS =  -L$(TOP)/mipsel-uclibc/install/curl/usr/lib  -L$(TOP)/mipsel-uclibc/install/libxml2/usr/lib  -L$(TOP)/mipsel-uclibc/install/openssl/usr/lib
#LDFLAGS =  -L/home/gauss/optware_bulid/APP/DownloadMaster2/build/curl/lib  -L/home/gauss/oleg/optware/staging/opt/lib
#CFLAGS = -g -rdynamic
LDFLAGS =  -L$(TOP)/curl-7.21.7/lib/.libs  -L$(TOP)/libxml2/.libs  -L$(TOP)/openssl -L$(TOP)/nvram -L$(TOP)/shared -lshared
#LDFLAGS =  -L$(TOP)/curl-7.21.7/lib/.libs  -L$(TOP)/libxml2/.libs  -L$(TOP)/openssl
LIBS = -lcurl -lxml2 -lssl -lcrypto -lpthread -lnvram

ifeq ($(wildcard $(TOP)/asuswebstorage/*.c),)
all:
	-cp -f $(TOP)/asuswebstorage/prebuilt/asuswebstorage asuswebstorage
else
all: asuswebstorage
endif

asuswebstorage: asuswebstorage.o api.o function.o list.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS)  $^ -o $@ 


.PHONY: clean
clean:
	-rm -rf *.o asuswebstorage
