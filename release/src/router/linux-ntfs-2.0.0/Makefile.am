
SUBDIRS = doc include libntfs ntfsprogs

EXTRA_DIST = AUTHORS CREDITS COPYING TODO.include TODO.libntfs ChangeLog \
		INSTALL NEWS README autogen.sh ntfsprogs.spec.in \
		TODO.ntfsprogs getgccver

AUTOMAKE_OPTIONS = gnu

MAINTAINERCLEANFILES = configure Makefile.in aclocal.m4 compile depcomp \
	install-sh ltmain.sh missing config.guess config.sub config.h.in INSTALL

dist-hook: ntfsprogs.spec
	cp ntfsprogs.spec $(distdir)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

strip:
	(cd ntfsprogs && $(MAKE) strip) || exit 1;

libs:
	(cd libntfs && $(MAKE) libs) || exit 1;

extra:	extras

extras: libs
	(cd ntfsprogs && $(MAKE) extras) || exit 1;

if ENABLE_TEST
test:	libs
	(cd test && $(MAKE) all test) || exit 1;
SUBDIRS += test
else
.PHONY: test
test:
	@echo The libntfs test code has been configured out of this release.
	@echo "./configure --enable-test" and rebuild.
endif

