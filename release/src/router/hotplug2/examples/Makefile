# vim:set sw=8 nosta:

BINS=
SUBDIRS=

KERNELVER ?= `uname -r`

all:

install:
	$(INSTALL_DIR) $(PREFIX)/etc
	case "$(KERNELVER)" in \
		2.6.*) \
		$(INSTALL) hotplug2.rules-2.6kernel $(PREFIX)/etc/hotplug2.rules \
		;; \
		*) \
		$(INSTALL) hotplug2.rules-2.4kernel $(PREFIX)/etc/hotplug2.rules \
		;; \
	esac
	# remove comments, compress whitespace
	@sed -i 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' $(PREFIX)/etc/hotplug2.rules

include ../common.mak
MAKEDEP:=
