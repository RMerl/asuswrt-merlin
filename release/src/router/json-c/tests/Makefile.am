
include ../Makefile.am.inc
LDADD= $(LIBJSON_LA)

LIBJSON_LA=$(top_builddir)/libjson-c.la

TESTS=
TESTS+= test1.test
TESTS+= test2.test
TESTS+= test4.test
TESTS+= testReplaceExisting.test
TESTS+= test_parse_int64.test
TESTS+= test_null.test
TESTS+= test_cast.test
TESTS+= test_parse.test
TESTS+= test_locale.test
TESTS+= test_charcase.test
TESTS+= test_printbuf.test
TESTS+= test_set_serializer.test

check_PROGRAMS=
check_PROGRAMS += $(TESTS:.test=)

# Note: handled by test1.test
check_PROGRAMS += test1Formatted 
test1Formatted_SOURCES = test1.c parse_flags.c
test1Formatted_CPPFLAGS = -DTEST_FORMATTED

# Note: handled by test2.test
check_PROGRAMS += test2Formatted
test2Formatted_SOURCES = test2.c parse_flags.c
test2Formatted_CPPFLAGS = -DTEST_FORMATTED

EXTRA_DIST=
EXTRA_DIST += $(TESTS)

testsubdir=testSubDir
TESTS_ENVIRONMENT       = top_builddir=$(top_builddir)

distclean-local:
	-rm -rf $(testsubdir)
