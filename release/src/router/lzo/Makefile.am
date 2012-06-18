## Process this file with automake to create Makefile.in
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

AUTOMAKE_OPTIONS = 1.9.6 gnu

SUBDIRS = include src lzotest examples tests minilzo

EXTRA_DIST = BUGS


# /***********************************************************************
# // automake targets
# ************************************************************************/

dist-hook:
	-rm -rf "./$(distdir)/autoconf/"
	cp -pR $(srcdir)/B/ $(srcdir)/asm/ $(srcdir)/autoconf/ $(srcdir)/doc/ $(srcdir)/util/ "$(distdir)/"
	-rm -f `find "./$(distdir)/" -type l -print`
	-rm -f ./$(distdir)/util/*.dat

check-local: all-am
	./lzotest/lzotest -mlzo -n2 -q $(top_srcdir)/COPYING
	@echo
	@echo "All checks passed."
	@echo


# /***********************************************************************
# // special targets
# ************************************************************************/

basic-test: all
	./lzotest/lzotest -mlzo -n2 -q $(top_srcdir)/COPYING
	./tests/align
	./tests/chksum
	./examples/simple
	./minilzo/testmini

test: all
	./lzotest/lzotest -mavail -n10 -q $(top_srcdir)/COPYING
	$(SHELL) -e $(top_srcdir)/util/check.sh $(top_srcdir)
	./tests/align
	./tests/chksum
	./examples/simple
	./minilzo/testmini
	@echo
	@echo "All tests passed. Now you are ready to install LZO."
	@echo

@MAINT@-include $(top_srcdir)/Makefile.maint
