EXTRA_DIST =  \
	.bzrignore \
	Apache-2.0 \
	BSD \
	INSTALL \
	Makefile.am \
	NEWS \
	README \
	c++/README \
	c/README \
	c/check-subunit-0.9.3.patch \
	c/check-subunit-0.9.5.patch \
	c/check-subunit-0.9.6.patch \
	perl/Makefile.PL.in \
	perl/lib/Subunit.pm \
	perl/lib/Subunit/Diff.pm \
	perl/subunit-diff \
	python/iso8601/LICENSE \
	python/iso8601/README \
	python/iso8601/README.subunit \
	python/iso8601/setup.py \
	python/iso8601/test_iso8601.py \
	python/subunit/tests/TestUtil.py \
	python/subunit/tests/__init__.py \
	python/subunit/tests/sample-script.py \
	python/subunit/tests/sample-two-script.py \
	python/subunit/tests/test_chunked.py \
	python/subunit/tests/test_details.py \
	python/subunit/tests/test_progress_model.py \
	python/subunit/tests/test_subunit_filter.py \
	python/subunit/tests/test_subunit_stats.py \
	python/subunit/tests/test_subunit_tags.py \
	python/subunit/tests/test_tap2subunit.py \
	python/subunit/tests/test_test_protocol.py \
	python/subunit/tests/test_test_results.py \
	runtests.py \
	shell/README \
	shell/share/subunit.sh \
	shell/subunit-ui.patch \
	shell/tests/test_function_output.sh \
	shell/tests/test_source_library.sh

ACLOCAL_AMFLAGS = -I m4

include_subunitdir = $(includedir)/subunit

dist_bin_SCRIPTS = \
	filters/subunit-filter \
	filters/subunit-ls \
	filters/subunit-stats \
	filters/subunit-tags \
	filters/subunit2gtk \
	filters/subunit2junitxml \
	filters/subunit2pyunit \
	filters/tap2subunit

TESTS_ENVIRONMENT = SHELL_SHARE='$(top_srcdir)/shell/share/' PYTHONPATH='$(abs_top_srcdir)/python':${PYTHONPATH}
TESTS = runtests.py $(check_PROGRAMS)

## install libsubunit.pc
pcdatadir = $(libdir)/pkgconfig
pcdata_DATA = \
	libsubunit.pc \
	libcppunit_subunit.pc

pkgpython_PYTHON = \
	python/subunit/__init__.py \
	python/subunit/chunked.py \
	python/subunit/details.py \
	python/subunit/iso8601.py \
	python/subunit/progress_model.py \
	python/subunit/run.py \
	python/subunit/test_results.py

lib_LTLIBRARIES = libsubunit.la
lib_LTLIBRARIES +=  libcppunit_subunit.la

include_subunit_HEADERS = \
	c/include/subunit/child.h \
	c++/SubunitTestProgressListener.h

check_PROGRAMS = \
	c/tests/test_child

check_SCRIPTS = \
	runtests.py

libsubunit_la_SOURCES = \
	c/lib/child.c \
	c/include/subunit/child.h

libcppunit_subunit_la_SOURCES = \
	c++/SubunitTestProgressListener.cpp \
	c++/SubunitTestProgressListener.h

tests_LDADD = @CHECK_LIBS@ $(top_builddir)/libsubunit.la
c_tests_test_child_CFLAGS = -I$(top_srcdir)/c/include $(SUBUNIT_CFLAGS) @CHECK_CFLAGS@
c_tests_test_child_LDADD = $(tests_LDADD)


all-local: perl/Makefile
	$(MAKE) -C perl all

check-local: perl/Makefile
	$(MAKE) -C perl check

clean-local:
	find . -type f -name "*.pyc" -exec rm {} ';'
	rm -f perl/Makefile

# Remove perl dir for VPATH builds.
distclean-local:
	-rmdir perl > /dev/null
	-rm perl/Makefile.PL > /dev/null

install-exec-local: perl/Makefile
	$(MAKE) -C perl install

mostlyclean-local:
	rm -rf perl/blib
	rm -rf perl/pm_to_blib

# 'uninstall' perl files during distcheck
uninstall-local:
	if [ "_inst" = `basename ${prefix}` ]; then \
	  $(MAKE) -C perl uninstall_distcheck; \
	    rm -f "$(DESTDIR)$(bindir)"/subunit-diff; \
	fi

# The default for MakeMaker; can be overridden by exporting
INSTALLDIRS ?= site

perl/Makefile: perl/Makefile.PL
	mkdir -p perl
	cd perl && perl Makefile.PL INSTALLDIRS=${INSTALLDIRS}
	-rm perl/Makefile.old > /dev/null
