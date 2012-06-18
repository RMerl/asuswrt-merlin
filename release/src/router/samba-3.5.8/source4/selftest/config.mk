TEST_FORMAT = plain

SELFTEST = $(LD_LIBPATH_OVERRIDE) PYTHON=$(PYTHON) \
    $(PERL) $(selftestdir)/selftest.pl --prefix=${selftest_prefix} \
    --builddir=$(builddir) --srcdir=$(srcdir) \
    --exclude=$(srcdir)/selftest/skip --testlist="./selftest/tests.sh|" \
    $(TEST_OPTIONS)
# this strange code is to handle the fact that the bash pipefail option is 
# not portable. When we run selftest we use a pipeline, and the first element
# of that pipeline might abort. We need to catch this and mark the test
# as having failed
ST_RM = ( rm -f $(selftest_prefix)/st_done && 
ST_TOUCH = && touch $(selftest_prefix)/st_done ) 
ST_DONE_TEST = @test -f $(selftest_prefix)/st_done || { echo "SELFTEST FAILED"; exit 1; }

SELFTEST_NOSLOW_OPTS = --exclude=$(srcdir)/selftest/slow
SELFTEST_QUICK_OPTS = $(SELFTEST_NOSLOW_OPTS) --quick --include=$(srcdir)/selftest/quick
FILTER_XFAIL = $(PERL) $(selftestdir)/filter-subunit.pl --expected-failures=$(srcdir)/selftest/knownfail
FORMAT_TEST_OUTPUT = $(FILTER_XFAIL) | $(PERL) $(selftestdir)/format-subunit.pl --format=$(TEST_FORMAT)

test-subunit:: everything
	$(SELFTEST) --socket-wrapper $(TESTS) $(ST_TOUCH)
	$(ST_DONE_TEST)

slowtest:: everything
	$(SELFTEST) $(DEFAULT_TEST_OPTIONS) $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

ifeq ($(RUN_FROM_BUILD_FARM),yes)
test:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) $(TESTS) $(ST_TOUCH) | $(FILTER_XFAIL) --strip-passed-output
	$(ST_DONE_TEST)
else
test:: 
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)
endif

kvmtest:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) \
		--target=kvm --image=$(KVM_IMAGE) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

kvmquicktest:: everything
	$(ST_RM) $(SELFTEST) $(DEFAULT_TEST_OPTIONS) \
		$(SELFTEST_QUICK_OPTS) --target=kvm --image=$(KVM_IMAGE) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

testone:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) --one $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT)
	$(ST_DONE_TEST)

test-swrap:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

test-swrap-pcap:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper-pcap $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

test-swrap-keep-pcap:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper-keep-pcap $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

test-noswrap:: everything
	$(ST_RM) $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

quicktest:: all
	$(ST_RM) $(SELFTEST) $(SELFTEST_QUICK_OPTS) --socket-wrapper $(TESTS) $(ST_TOUCH) | $(FORMAT_TEST_OUTPUT) --immediate 
	$(ST_DONE_TEST)

quicktest-subunit:: all
	$(ST_RM) $(SELFTEST) $(SELFTEST_QUICK_OPTS) --socket-wrapper $(TESTS) $(ST_TOUCH)
	$(ST_DONE_TEST)

quicktestone:: all
	$(ST_RM) $(SELFTEST) $(SELFTEST_QUICK_OPTS) --socket-wrapper --one $(TESTS) | $(FORMAT_TEST_OUTPUT)
	$(ST_DONE_TEST)

testenv:: everything
	$(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper --testenv

testenv-%:: everything
	SELFTEST_TESTENV=$* $(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper --testenv

test-%:: 
	$(MAKE) test TESTS=$*

valgrindtest:: valgrindtest-all

valgrindtest-quick:: all
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) $(SELFTEST_QUICK_OPTS) --socket-wrapper $(TESTS) | $(FORMAT_TEST_OUTPUT) --immediate 

valgrindtest-all:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper $(TESTS) | $(FORMAT_TEST_OUTPUT) --immediate 

valgrindtest-env:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper --testenv

gdbtest:: gdbtest-all

gdbtest-quick:: all
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST) $(SELFTEST_QUICK_OPTS) --socket-wrapper $(TESTS) | $(FORMAT_TEST_OUTPUT) --immediate 

gdbtest-all:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper $(TESTS) | $(FORMAT_TEST_OUTPUT) --immediate 

gdbtest-env:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST) $(SELFTEST_NOSLOW_OPTS) --socket-wrapper --testenv

