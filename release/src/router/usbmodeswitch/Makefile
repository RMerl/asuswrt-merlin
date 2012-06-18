PROG        = usb_modeswitch
VERS        = 1.1.8
CC          = gcc
CFLAGS      += -Wall
LIBS        = -l usb
RM          = /bin/rm -f
OBJS        = usb_modeswitch.c
PREFIX      = $(DESTDIR)/usr
ETCDIR      = $(DESTDIR)/etc
UDEVDIR     = $(DESTDIR)/lib/udev
SBINDIR     = $(PREFIX)/sbin
MANDIR      = $(PREFIX)/share/man/man1

.PHONY:    clean install uninstall

all:        $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(CFLAGS) $(LIBS) $(LDFLAGS)
	$(STRIP) $@

clean:
	$(RM) usb_modeswitch *.o

install: all
	install -D -s --mode=755 usb_modeswitch $(SBINDIR)/usb_modeswitch
	install -D --mode=755 usb_modeswitch.tcl $(SBINDIR)/usb_modeswitch_dispatcher
	install -D --mode=755 usb_modeswitch.sh $(UDEVDIR)/usb_modeswitch
	install -D --mode=644 usb_modeswitch.conf $(ETCDIR)/usb_modeswitch.conf
	install -D --mode=644 usb_modeswitch.1 $(MANDIR)/usb_modeswitch.1
	install -d $(DESTDIR)/var/lib/usb_modeswitch

uninstall:
	$(RM) $(SBINDIR)/usb_modeswitch
	$(RM) $(UDEVDIR)/usb_modeswitch
	$(RM) $(ETCDIR)/usb_modeswitch.conf
	$(RM) $(MANDIR)/usb_modeswitch.1
	$(RM) -R $(DESTDIR)/var/lib/usb_modeswitch
