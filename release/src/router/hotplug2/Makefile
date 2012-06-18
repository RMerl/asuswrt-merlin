# vim:set sw=8 nosta:

BINS=hotplug2
SUBDIRS=examples


all: $(BINS)

install:
	$(INSTALL_DIR) $(PREFIX)/sbin/
	$(INSTALL_BIN) $(BINS) $(PREFIX)/sbin/
	@for i in $(BINS); do $(STRIP) $(PREFIX)/sbin/$$i ; done


hotplug2: hotplug2.o hotplug2_utils.o childlist.o mem_utils.o rules.o filemap_utils.o
hotplug2-dnode: hotplug2-dnode.o hotplug2_utils.o mem_utils.o parser_utils.o


include common.mak
