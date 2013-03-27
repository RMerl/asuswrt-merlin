PROG        = usb_modeswitch
VERS        = 1.2.3
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
VPATH		= jimtcl

.PHONY:    clean install uninstall

all: $(PROG)

shared: $(PROG) dispatcher-dynamic

static: $(PROG) dispatcher-static

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(CFLAGS) $(LIBS) $(LDFLAGS)

dispatcher-static: dispatcher.c usb_modeswitch.tcl
	./make_static_dispatcher.sh

dispatcher-dynamic: usb_modeswitch_dispatcher

usb_modeswitch_dispatcher: dispatcher.c usb_modeswitch.string
	$(CC) $(CFLAGS) dispatcher.c -ljim -o $@

usb_modeswitch.string: usb_modeswitch.tcl
	jimsh make_string.tcl usb_modeswitch.tcl > $@

clean:
	$(RM) usb_modeswitch
	$(RM) usb_modeswitch_dispatcher
	$(RM) usb_modeswitch.string

distclean:
	$(RM) usb_modeswitch
	$(RM) usb_modeswitch_dispatcher
	$(RM) usb_modeswitch.string
	cd jim && $(MAKE) distclean

install-common:
	install -D -s --mode=755 usb_modeswitch $(SBINDIR)/usb_modeswitch
	install -D --mode=755 usb_modeswitch.sh $(UDEVDIR)/usb_modeswitch
	install -D --mode=644 usb_modeswitch.conf $(ETCDIR)/usb_modeswitch.conf
	install -D --mode=644 usb_modeswitch.1 $(MANDIR)/usb_modeswitch.1
	install -d $(DESTDIR)/var/lib/usb_modeswitch

install-script:
	@SHELL=`which tclsh 2>/dev/null`; \
	if [ -z $$SHELL ]; then \
		SHELL="`which jimsh 2>/dev/null`"; \
		if [ -z $$SHELL ]; then \
			echo "Warning: no Tcl interpreter found!"; \
			SHELL="/usr/bin/tclsh"; \
		fi \
	fi ; \
	sed 's_!/usr/bin/tclsh_!'"$$SHELL"'_' <usb_modeswitch.tcl >usb_modeswitch_dispatcher
	install -D --mode=755 usb_modeswitch_dispatcher $(SBINDIR)/usb_modeswitch_dispatcher

install-binary:
	install -D -s --mode=755 usb_modeswitch_dispatcher $(SBINDIR)/usb_modeswitch_dispatcher

install: all install-common install-script

install-shared: shared install-common install-binary

install-static: static install-common install-binary

uninstall:
	$(RM) $(SBINDIR)/usb_modeswitch
	$(RM) $(SBINDIR)/usb_modeswitch_dispatcher
	$(RM) $(UDEVDIR)/usb_modeswitch
	$(RM) $(ETCDIR)/usb_modeswitch.conf
	$(RM) $(MANDIR)/usb_modeswitch.1
	$(RM) -R $(DESTDIR)/var/lib/usb_modeswitch
