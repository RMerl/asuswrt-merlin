AUTOMAKE_OPTIONS    = dejagnu
DEJATOOL = libzebra

SUBDIRS = \
	bgpd.tests \
	libzebra.tests

EXTRA_DIST = \
	config/unix.exp \
	lib/bgpd.exp \
	lib/libzebra.exp \
	global-conf.exp \
	testcommands.in \
	testcommands.refout

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ $(LOCAL_OPTS) -DSYSCONFDIR=\"$(sysconfdir)/\"

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

if BGPD
TESTS_BGPD = aspathtest testbgpcap ecommtest testbgpmpattr testbgpmpath
DEJATOOL += bgpd
else
TESTS_BGPD =
endif

check_PROGRAMS = testsig testsegv testbuffer testmemory heavy heavywq heavythread \
		testprivs teststream testchecksum tabletest testnexthopiter \
		testcommands test-timer-correctness test-timer-performance \
		$(TESTS_BGPD)

../vtysh/vtysh_cmd.c:
	$(MAKE) -C ../vtysh vtysh_cmd.c

test-commands-defun.c: ../vtysh/vtysh_cmd.c
	sed \
		-e '/"vtysh.h"/d' \
		-e 's/vtysh_init_cmd/test_init_cmd/' \
		-e 's/VTYSH_[A-Z][A-Z_0-9]*/0/g' \
		< ../vtysh/vtysh_cmd.c \
		> test-commands-defun.c

BUILT_SOURCES = test-commands-defun.c
noinst_HEADERS = prng.h

testsig_SOURCES = test-sig.c
testsegv_SOURCES = test-segv.c
testbuffer_SOURCES = test-buffer.c
testmemory_SOURCES = test-memory.c
testprivs_SOURCES = test-privs.c
teststream_SOURCES = test-stream.c
heavy_SOURCES = heavy.c main.c
heavywq_SOURCES = heavy-wq.c main.c
heavythread_SOURCES = heavy-thread.c main.c
aspathtest_SOURCES = aspath_test.c
testbgpcap_SOURCES = bgp_capability_test.c
ecommtest_SOURCES = ecommunity_test.c
testbgpmpattr_SOURCES =  bgp_mp_attr_test.c
testchecksum_SOURCES = test-checksum.c
testbgpmpath_SOURCES = bgp_mpath_test.c
tabletest_SOURCES = table_test.c
testnexthopiter_SOURCES = test-nexthop-iter.c prng.c
testcommands_SOURCES = test-commands-defun.c test-commands.c prng.c
test_timer_correctness_SOURCES = test-timer-correctness.c prng.c
test_timer_performance_SOURCES = test-timer-performance.c prng.c

testsig_LDADD = ../lib/libzebra.la @LIBCAP@
testsegv_LDADD = ../lib/libzebra.la @LIBCAP@
testbuffer_LDADD = ../lib/libzebra.la @LIBCAP@
testmemory_LDADD = ../lib/libzebra.la @LIBCAP@
testprivs_LDADD = ../lib/libzebra.la @LIBCAP@
teststream_LDADD = ../lib/libzebra.la @LIBCAP@
heavy_LDADD = ../lib/libzebra.la @LIBCAP@ -lm
heavywq_LDADD = ../lib/libzebra.la @LIBCAP@ -lm
heavythread_LDADD = ../lib/libzebra.la @LIBCAP@ -lm
aspathtest_LDADD = ../bgpd/libbgp.a ../lib/libzebra.la @LIBCAP@ -lm
testbgpcap_LDADD = ../bgpd/libbgp.a ../lib/libzebra.la @LIBCAP@ -lm
ecommtest_LDADD = ../bgpd/libbgp.a ../lib/libzebra.la @LIBCAP@ -lm
testbgpmpattr_LDADD = ../bgpd/libbgp.a ../lib/libzebra.la @LIBCAP@ -lm
testchecksum_LDADD = ../lib/libzebra.la @LIBCAP@ 
testbgpmpath_LDADD = ../bgpd/libbgp.a ../lib/libzebra.la @LIBCAP@ -lm
tabletest_LDADD = ../lib/libzebra.la @LIBCAP@ -lm
testnexthopiter_LDADD = ../lib/libzebra.la @LIBCAP@
testcommands_LDADD = ../lib/libzebra.la @LIBCAP@
test_timer_correctness_LDADD = ../lib/libzebra.la @LIBCAP@
test_timer_performance_LDADD = ../lib/libzebra.la @LIBCAP@
