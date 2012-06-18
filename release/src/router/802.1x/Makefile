include ../common.mak

#PLATFORM = 5VT
#PLATFORM = PC
#PLATFORM = STAR
PLATFORM = RT2880

ifeq ($(PLATFORM),RT2880)
CROSS_COMPILE = mipsel-linux-
endif

ifeq ($(PLATFORM),5VT)
CROSS_COMPILE = /opt/crosstool/uClibc_v5te_le_gcc_4_1_1/bin/arm-linux-
endif

ifeq ($(PLATFORM),STAR)
CROSS_COMPILE = /opt/star/tools/arm-linux/bin/arm-linux-
endif

ifeq ($(PLATFORM),PC)
CROSS_COMPILE =
endif

CC = $(CROSS_COMPILE)gcc

ifndef CFLAGS
CFLAGS = -O2 -Wall -g
endif

# If you want to support multiple radius server for multiple bssid, add following line
#CFLAGS +=  -DMULTIPLE_RADIUS

# If you want to debug daemon, add following line
CFLAGS +=  -DDBG 

OBJS =	rtdot1x.o eloop.o eapol_sm.o radius.o md5.o  \
	config.o ieee802_1x.o  \
	sta_info.o   radius_client.o

all: rt2860apd

rt2860apd: $(OBJS)
	$(CC) -o rt2860apd $(OBJS)

clean:
	rm -f core *~ *.o rtdot1xd rt2860apd rtinicapd *.d

#
# add by YYHuang@Ralink
install: all
	install -D rt2860apd $(INSTALLDIR)/bin/rt2860apd
	$(STRIP) $(INSTALLDIR)/bin/rt2860apd
	cd $(INSTALLDIR)/bin && ln -sf rt2860apd rtinicapd

-include $(OBJS:%.o=%.d)
