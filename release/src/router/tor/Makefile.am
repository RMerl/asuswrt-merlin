# Copyright (c) 2001-2004, Roger Dingledine
# Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
# Copyright (c) 2007-2015, The Tor Project, Inc.
# See LICENSE for licensing information

ACLOCAL_AMFLAGS = -I m4

noinst_LIBRARIES=
EXTRA_DIST=
noinst_HEADERS=
bin_PROGRAMS=
CLEANFILES=
TESTS=
noinst_PROGRAMS=
DISTCLEANFILES=
bin_SCRIPTS=
AM_CPPFLAGS=
AM_CFLAGS=@TOR_SYSTEMD_CFLAGS@ @CFLAGS_BUGTRAP@
SHELL=@SHELL@

if COVERAGE_ENABLED
TESTING_TOR_BINARY=$(top_builddir)/src/or/tor-cov$(EXEEXT)
else
TESTING_TOR_BINARY=$(top_builddir)/src/or/tor$(EXEEXT)
endif

include src/include.am
include doc/include.am
include contrib/include.am

EXTRA_DIST+= \
	ChangeLog					\
	INSTALL						\
	LICENSE						\
	Makefile.nmake					\
	README						\
	ReleaseNotes

## This tells etags how to find mockable function definitions.
AM_ETAGSFLAGS=--regex='{c}/MOCK_IMPL([^,]+,\W*\([a-zA-Z0-9_]+\)\W*,/\1/s'

if COVERAGE_ENABLED
TEST_CFLAGS=-fno-inline -fprofile-arcs -ftest-coverage
if DISABLE_ASSERTS_IN_UNIT_TESTS
TEST_CPPFLAGS=-DTOR_UNIT_TESTS -DTOR_COVERAGE -DDISABLE_ASSERTS_IN_UNIT_TESTS
else
TEST_CPPFLAGS=-DTOR_UNIT_TESTS -DTOR_COVERAGE
endif
TEST_NETWORK_FLAGS=--coverage --hs-multi-client 1
else
TEST_CFLAGS=
TEST_CPPFLAGS=-DTOR_UNIT_TESTS
TEST_NETWORK_FLAGS=--hs-multi-client 1
endif

TEST_NETWORK_ALL_LOG_DIR=$(top_builddir)/test_network_log
TEST_NETWORK_ALL_DRIVER_FLAGS=--color-tests yes

#install-data-local:
#	$(INSTALL) -m 755 -d $(LOCALSTATEDIR)/lib/tor

# Allows to override rpmbuild with rpmbuild-md5 from fedora-packager so that
# building for EL5 won't fail on https://bugzilla.redhat.com/show_bug.cgi?id=490613
RPMBUILD ?= rpmbuild

# Use automake's dist-gzip target to build the tarball
dist-rpm: dist-gzip
	TIMESTAMP=$$(date +"%Y-%m-%d_%H.%M.%S"); 				\
	RPM_BUILD_DIR=$$(mktemp -d "/tmp/tor-rpm-build-$$TIMESTAMP-XXXX");	\
	mkdir -p "$$RPM_BUILD_DIR"/{BUILD,RPMS,SOURCES/"tor-$(VERSION)",SPECS,SRPMS}; \
	cp -fa "$(distdir).tar.gz" "$$RPM_BUILD_DIR"/SOURCES/;			\
	LIBS=-lrt $(RPMBUILD) -ba --define "_topdir $$RPM_BUILD_DIR" tor.spec; 	\
	cp -fa "$$RPM_BUILD_DIR"/SRPMS/* .;                           		\
	cp -fa "$$RPM_BUILD_DIR"/RPMS/* .;                            		\
	rm -rf "$$RPM_BUILD_DIR";						\
	echo "RPM build finished";						\
	#end of dist-rpm

doxygen:
	doxygen && cd doc/doxygen/latex && make

test: all
	$(top_builddir)/src/test/test

need-chutney-path:
	@if test ! -d "$$CHUTNEY_PATH"; then \
		echo '$$CHUTNEY_PATH was not set.'; \
		if test -d $(top_srcdir)/../chutney -a -x $(top_srcdir)/../chutney/chutney; then \
			echo "Assuming test-network.sh will find" $(top_srcdir)/../chutney; \
		else \
			echo; \
			echo "To run these tests, git clone https://git.torproject.org/chutney.git ; export CHUTNEY_PATH=\`pwd\`/chutney"; \
			exit 1; \
		fi \
	fi

# Note that test-network requires a copy of Chutney in $CHUTNEY_PATH.
# Chutney can be cloned from https://git.torproject.org/chutney.git .
test-network: need-chutney-path $(TESTING_TOR_BINARY) src/tools/tor-gencert
	$(top_srcdir)/src/test/test-network.sh $(TEST_NETWORK_FLAGS)

# Run all available tests using automake's test-driver
# only run IPv6 tests if we can ping6 ::1 (localhost)
# some IPv6 tests will fail without an IPv6 DNS server (see #16971 and #17011)
# only run mixed tests if we have a tor-stable binary
# Try both the BSD and the Linux ping6 syntax, because they're incompatible
test-network-all: need-chutney-path test-driver $(TESTING_TOR_BINARY) src/tools/tor-gencert
	mkdir -p $(TEST_NETWORK_ALL_LOG_DIR)
	@flavors="$(TEST_CHUTNEY_FLAVORS)"; \
	if ping6 -q -c 1 -o ::1 >/dev/null 2>&1 || ping6 -q -c 1 -W 1 ::1 >/dev/null 2>&1; then \
		echo "ping6 ::1 succeeded, running IPv6 flavors: $(TEST_CHUTNEY_FLAVORS_IPV6)."; \
		flavors="$$flavors $(TEST_CHUTNEY_FLAVORS_IPV6)"; \
	else \
		echo "ping6 ::1 failed, skipping IPv6 flavors: $(TEST_CHUTNEY_FLAVORS_IPV6)."; \
		skip_flavors="$$skip_flavors $(TEST_CHUTNEY_FLAVORS_IPV6)"; \
	fi; \
	if command -v tor-stable >/dev/null 2>&1; then \
		echo "tor-stable found, running mixed flavors: $(TEST_CHUTNEY_FLAVORS_MIXED)."; \
		flavors="$$flavors $(TEST_CHUTNEY_FLAVORS_MIXED)"; \
	else \
		echo "tor-stable not found, skipping mixed flavors: $(TEST_CHUTNEY_FLAVORS_MIXED)."; \
		skip_flavors="$$skip_flavors $(TEST_CHUTNEY_FLAVORS_MIXED)"; \
	fi; \
	for f in $$skip_flavors; do \
		echo "SKIP: $$f"; \
	done; \
	for f in $$flavors; do \
		$(SHELL) $(top_srcdir)/test-driver --test-name $$f --log-file $(TEST_NETWORK_ALL_LOG_DIR)/$$f.log --trs-file $(TEST_NETWORK_ALL_LOG_DIR)/$$f.trs $(TEST_NETWORK_ALL_DRIVER_FLAGS) $(top_srcdir)/src/test/test-network.sh --flavor $$f $(TEST_NETWORK_FLAGS); \
	done; \
	echo "Log and result files are available in $(TEST_NETWORK_ALL_LOG_DIR)."; \
	! grep -q FAIL test_network_log/*.trs

need-stem-path:
	@if test ! -d "$$STEM_SOURCE_DIR"; then \
		echo '$$STEM_SOURCE_DIR was not set.'; echo; \
		echo "To run these tests, git clone https://git.torproject.org/stem.git/ ; export STEM_SOURCE_DIR=\`pwd\`/stem"; \
		exit 1; \
	fi

test-stem: need-stem-path $(TESTING_TOR_BINARY)
	@$(PYTHON) "$$STEM_SOURCE_DIR"/run_tests.py --tor "$(TESTING_TOR_BINARY)" --all --log notice --target RUN_ALL;

test-stem-full: need-stem-path $(TESTING_TOR_BINARY)
	@$(PYTHON) "$$STEM_SOURCE_DIR"/run_tests.py --tor "$(TESTING_TOR_BINARY)" --all --log notice --target RUN_ALL,ONLINE -v;

test-full: need-stem-path need-chutney-path check test-network test-stem

test-full-online: need-stem-path need-chutney-path check test-network test-stem-full

reset-gcov:
	rm -f $(top_builddir)/src/*/*.gcda $(top_builddir)/src/*/*/*.gcda

HTML_COVER_DIR=$(top_builddir)/coverage_html
coverage-html: all
if COVERAGE_ENABLED
	test -e "`which lcov`" || (echo "lcov must be installed. See <http://ltp.sourceforge.net/coverage/lcov.php>." && false)
	test -d "$(HTML_COVER_DIR)" || $(MKDIR_P) "$(HTML_COVER_DIR)"
	lcov --rc lcov_branch_coverage=1 --directory $(top_builddir)/src --zerocounters
	$(MAKE) reset-gcov
	$(MAKE) check
	lcov --capture --rc lcov_branch_coverage=1 --no-external --directory $(top_builddir) --base-directory $(top_srcdir) --output-file "$(HTML_COVER_DIR)/lcov.tmp"
	lcov --remove "$(HTML_COVER_DIR)/lcov.tmp" --rc lcov_branch_coverage=1 'test/*' 'ext/tinytest*' '/usr/*' --output-file "$(HTML_COVER_DIR)/lcov.info"
	genhtml --branch-coverage -o "$(HTML_COVER_DIR)" "$(HTML_COVER_DIR)/lcov.info"
else
	@printf "Not configured with --enable-coverage, run ./configure --enable-coverage\n"
endif

coverage-html-full: all
	test -e "`which lcov`" || (echo "lcov must be installed. See <http://ltp.sourceforge.net/coverage/lcov.php>." && false)
	test -d "$(HTML_COVER_DIR)" || mkdir -p "$(HTML_COVER_DIR)"
	lcov --rc lcov_branch_coverage=1 --directory ./src --zerocounters
	$(MAKE) reset-gcov
	$(MAKE) check
	$(MAKE) test-stem-full
	CHUTNEY_TOR=tor-cov CHUTNEY_TOR_GENCERT=tor-cov-gencert $(top_srcdir)/src/test/test-network.sh
	CHUTNEY_TOR=tor-cov CHUTNEY_TOR_GENCERT=tor-cov-gencert $(top_srcdir)/src/test/test-network.sh --flavor hs
	lcov --capture --rc lcov_branch_coverage=1 --no-external --directory . --output-file "$(HTML_COVER_DIR)/lcov.tmp"
	lcov --remove "$(HTML_COVER_DIR)/lcov.tmp" --rc lcov_branch_coverage=1 'test/*' 'ext/tinytest*' '/usr/*' --output-file "$(HTML_COVER_DIR)/lcov.info"
	genhtml --branch-coverage -o "$(HTML_COVER_DIR)" "$(HTML_COVER_DIR)/lcov.info"

# Avoid strlcpy.c, strlcat.c, aes.c, OpenBSD_malloc_Linux.c, sha256.c,
# tinytest*.[ch]
check-spaces:
	$(top_srcdir)/scripts/maint/checkSpace.pl -C \
		$(top_srcdir)/src/common/*.[ch] \
		$(top_srcdir)/src/or/*.[ch] \
		$(top_srcdir)/src/test/*.[ch] \
		$(top_srcdir)/src/tools/*.[ch]

check-docs: all
	$(PERL) $(top_builddir)/scripts/maint/checkOptionDocs.pl

check-logs:
	$(top_srcdir)/scripts/maint/checkLogs.pl \
		$(top_srcdir)/src/*/*.[ch] | sort -n

.PHONY: check-changes
check-changes:
	@if test -d "$(top_srcdir)/changes"; then \
		$(PYTHON) $(top_srcdir)/scripts/maint/lintChanges.py $(top_srcdir)/changes/*; \
		fi

.PHONY: update-versions
update-versions:
	$(PERL) $(top_builddir)/scripts/maint/updateVersions.pl

version:
	@echo "Tor @VERSION@"
	@if test -d "$(top_srcdir)/.git" && test -x "`which git 2>&1;true`"; then \
	   echo -n "git: " ;\
	   (cd "$(top_srcdir)" && git rev-parse --short=16 HEAD); \
	fi

mostlyclean-local:
	rm -f $(top_builddir)/src/*/*.gc{da,no} $(top_builddir)/src/*/*/*.gc{da,no}
	rm -rf $(HTML_COVER_DIR)
	rm -rf $(top_builddir)/doc/doxygen
	rm -rf $(TEST_NETWORK_ALL_LOG_DIR)
