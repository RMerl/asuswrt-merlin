INSTALL=install
INSTALLDIR=install -m 0755 -d
INSTALLMAN=install -m 0644

SUBDIRS = man3 man7 man8

all clean install:
	@for subdir in $(SUBDIRS); do $(MAKE) -C $$subdir $@ || exit $$?; done

distclean: clean

.PHONY: install clean distclean

.EXPORT_ALL_VARIABLES:
