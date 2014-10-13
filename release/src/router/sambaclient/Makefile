#***********************************************************************
#
# Makefile for mipsel asuswrt
#
#***********************************************************************

include ../common.mak
ifeq ($(RTCONFIG_SAMBA_NEW),y)
SMBSRC = samba36
else
SMBSRC = samba-3.5.8
endif
#CC = mipsel-linux-uclibc-gcc

#CFLAGS = -g -D_FILE_OFFSET_BITS=64 -DHAVE_LIBSMBCLIENT_H -I/home/mars/asuswrt-dropbox/asuswrt/release/src-rt/router/$(SMBSRC)/source3/include
#LDFLAGS = -L/home/mars/asuswrt-dropbox/asuswrt/release/src-rt/router/$(SMBSRC)/source3/bin
#LIBS = -lsmbclient -lwbclient -lpthread

CFLAGS = -g -D_FILE_OFFSET_BITS=64 -DHAVE_LIBSMBCLIENT_H -I/$(TOP)/$(SMBSRC)/source3/include
CFLAGS += -DNVRAM_ -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared

LDFLAGS = -L/$(TOP)/$(SMBSRC)/source3/bin -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared
LIBS = -lpthread -lsmbclient -lwbclient

ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client
LIBS += -lqcsapi_client
endif
ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
endif

objects = main.o smb_api.o list.o usb.o

all : sambaclient

sambaclient : $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $^

.PHONY : clean
clean:
	rm -rf *.o sambaclient

