include ../common.mak

#############################################################
# Makefile for WSC upnp module
#############################################################

#CROSS_COMPILE = mipsel-uclibc-
#CC = $(CROSS_COMPILE)gcc
#LD = $(CROSS_COMPILE)ld
#STRIP = $(CROSS_COMPILE)strip

LIBUPNP_SRC_PATH = ../libupnp-1.3.1
#LIB_TOOLCHAIN = ../../../../tools/brcm/hndtools-mipsel-linux/lib

INCLUDE_PATH += -I. 
INCLUDE_PATH += -I$(LIBUPNP_SRC_PATH)
INCLUDE_PATH += -I$(LIBUPNP_SRC_PATH)/upnp/inc -I$(LIBUPNP_SRC_PATH)/upnp/src/inc
INCLUDE_PATH += -I$(LIBUPNP_SRC_PATH)/ixml/inc -I$(LIBUPNP_SRC_PATH)/ixml/src/inc  
INCLUDE_PATH += -I$(LIBUPNP_SRC_PATH)/threadutil/inc
INCLUDE_PATH += -idirafter$(LINUXDIR)/include
INCLUDE_PATH += -I$(TOP)/wireless_tools

#INCLUDE_LIBS += -L$(LIBUPNP_SRC_PATH)/upnp/.libs -L$(LIBUPNP_SRC_PATH)/ixml/.libs -L$(LIBUPNP_SRC_PATH)/threadutil/.libs
#INCLUDE_LIBS += -lupnp -lixml -lthreadutil
INCLUDE_LIBS += -lpthread
STATIC_LIBS += $(LIBUPNP_SRC_PATH)/upnp/.libs/libupnp.a $(LIBUPNP_SRC_PATH)/ixml/.libs/libixml.a $(LIBUPNP_SRC_PATH)/threadutil/.libs/libthreadutil.a
#STATIC_LIBS += $(LIB_TOOLCHAIN)/libpthread.a

OBJS = wsc_main.o wsc_common.o wsc_ioctl.o wsc_netlink.o wsc_upnp_device.o wsc_upnp_cp.o sample_util.o wsc_msg.o

EXEC = wscd

CFLAGS += -O2 -Wall -D_REENTRANT
CFLAGS += $(INCLUDE_PATH)
#CFLAGS += -DRT_DEBUG
CFLAGS += -DMULTIPLE_CARD_SUPPORT

all: $(EXEC) $(STATIC_LIBS)

$(EXEC): $(OBJS) $(STATIC_LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(STATIC_LIBS) $(INCLUDE_LIBS)
	$(STRIP) -s $(EXEC)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o *~ *.bak $(EXEC)

cleanall: clean
	make -C $(LIBUPNP_SRC_PATH) clean
	rm -f *.a
	rm -f libupnp-1.3.1/Makefile

install: all
	install -d $(INSTALLDIR)/bin
	install wscd $(INSTALLDIR)/bin
	$(STRIP) $(INSTALLDIR)/bin/wscd
	install -d $(INSTALLDIR)/etc_ro/xml
	install xml/WFADeviceDesc.xml $(INSTALLDIR)/etc_ro/xml/WFADeviceDesc.xml
	install xml/WFAWLANConfigSCPD.xml $(INSTALLDIR)/etc_ro/xml/WFAWLANConfigSCPD.xml

