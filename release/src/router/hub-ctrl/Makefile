
CFLAGS += -s -I$(SRCBASE)/include

CFLAGS  += -Os
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections

UTILS := hub-ctrl

all: $(UTILS)

hub-ctrl: hub-ctrl.o
	$(CC) $(LDFLAGS) -o $@ -L$(TOP)/libusb10/libusb/.libs -lusb-1.0 -lpthread -ldl $<

hub-ctrl.o: hub-ctrl.c
	$(CC) -c $(CFLAGS) -o $@ -I$(TOP)/libusb10/libusb -I$(TOP)/libusb10/ $<

clean:
	rm -f *.o *~ $(UTILS)

install: all
	install -d $(INSTALLDIR)/usr/sbin
	install $(UTILS) $(INSTALLDIR)/usr/sbin
	$(STRIP) $(foreach file,$(UTILS),$(INSTALLDIR)/usr/sbin/$(file))
