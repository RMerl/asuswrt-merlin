# vim:set sw=8 nosta:

BINS=generate_alias hotplug2-coldplug-2.4 hotplug2-modwrap
SUBDIRS=
DESTDIR=


all: $(BINS)

install:
	$(INSTALL_BIN) hotplug2-coldplug-2.4 hotplug2-modwrap $(DESTDIR)/sbin/
	$(INSTALL_BIN) generate_alias $(DESTDIR)/usr/sbin/


hotplug2-coldplug-2.4: hotplug2-coldplug-2.4.o ../parser_utils.o ../filemap_utils.o ../mem_utils.o ../hotplug2_utils.o
hotplug2-modwrap: hotplug2-modwrap.o ../parser_utils.o ../filemap_utils.o ../mem_utils.o
generate_alias: generate_alias.o ../parser_utils.o ../filemap_utils.o ../mem_utils.o


include ../common.mak
