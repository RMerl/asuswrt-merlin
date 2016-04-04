#***********************************************************************
#
# Makefile for mipsel asuswrt
#
#***********************************************************************

#CC = mipsel-linux-uclibc-gcc

CFLAGS = -g -D_FILE_OFFSET_BITS=64 
#CFLAGS += -DNVRAM_ -D__DEBUG__ -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared
CFLAGS += -DNVRAM_  -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/shared
CFLAGS += -Wall

LDFLAGS =  -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared
LIBS = -lpthread -lgcc_s

include ../common.mak
ifeq ($(RTCONFIG_QTN),y)
LDFLAGS += -L$(TOP)/libqcsapi_client
LIBS += -lqcsapi_client
endif
ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/common/include
endif

objects = main.o list.o usb.o usb_api.o

all : usbclient

usbclient : $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $^

.PHONY : clean
clean:
	rm -rf *.o usbclient

