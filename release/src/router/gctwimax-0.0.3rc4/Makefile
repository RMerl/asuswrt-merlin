-include $(TOP)/.config

ALL=gctwimax

all: $(ALL)

ifndef CC
CC=gcc
endif

ifndef LDO
LDO=$(CC)
endif

CONFDIR = /usr/share/gctwimax

FLAGS = -MMD -O2 -g -Wall

FLAGS += -I.
FLAGS += -I$(TOP)/wpa_supplicant-0.7.3/src
FLAGS += -I$(TOP)/libusb10
FLAGS += -I$(TOP)/openssl/include
FLAGS += -I$(TOP)/zlib
#FLAGS += -I/usr/include/dbus-1.0
#FLAGS += -I/usr/lib/dbus-1.0/include
 
FLAGS += -DCONFDIR="$(CONFDIR)"

FLAGS += -DIEEE8021X_EAPOL
#FLAGS += -DWITH_DBUS

override CFLAGS += $(FLAGS)

LDFLAGS += -L$(TOP)/wpa_supplicant-0.7.3/src/eap_peer -leap
ifeq ($(RTCONFIG_BCMARM),y)
# for libusb10
LDFLAGS += -lpthread
endif
LDFLAGS += -L$(TOP)/libusb10/libusb/.libs -lusb-1.0
LDFLAGS += -L$(TOP)/openssl-1.0.0q -lcrypto-1.0.0q -lssl-1.0.0q -lpthread -ldl
LDFLAGS += -L$(TOP)/zlib -lz
#LDFLAGS += -ldbus-1


OBJS_ex = src/wimax.o src/protocol.o src/logging.o src/tap_dev.o src/eap_auth.o src/config.o

gctwimax: $(OBJS_ex)
	$(LDO) $(LDFLAGS) -o gctwimax $(OBJS_ex) $(CFLAGS)

install:
	cp ./gctwimax /sbin/gctwimax

	if [ ! -d $(CONFDIR) ] ; then mkdir $(CONFDIR) ; fi
	cp ./src/event.sh $(CONFDIR)/event.sh
	cp ./src/gctwimax.conf $(CONFDIR)/gctwimax.conf

uninstall:
	rm /sbin/gctwimax
	rm $(CONFDIR)/event.sh
	rm $(CONFDIR)/gctwimax.conf

.PHONY: clean
clean:
	rm -f core src/*~ src/*.o src/*.d $(ALL)

-include $(OBJS:%.o=%.d)
