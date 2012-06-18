nlstestscripts = check-localedir.sh # check-codeset.sh

codeset_tests = test-codeset-default test-codeset-latin1 test-codeset-utf-8

TESTS = $(nlstestscripts)

check_SCRIPTS = $(nlstestscripts)

check_PROGRAMS = print-localedir # $(codeset_tests)

# test_codeset_default_SOURCES = test-codeset.c
# test_codeset_default_CPPFLAGS =	\
# 	$(AM_CPPFLAGS) $(CPPFLAGS)	\
# 	-DCODESET_DEFAULT

# test_codeset_utf_8_SOURCES = test-codeset.c
# test_codeset_utf_8_CPPFLAGS =	\
# 	$(AM_CPPFLAGS) $(CPPFLAGS)	\
# 	-DCODESET_UTF_8

# test_codeset_latin1_SOURCES = test-codeset.c
# test_codeset_latin1_CPPFLAGS =	\
# 	$(AM_CPPFLAGS) $(CPPFLAGS)	\
# 	-DCODESET_LATIN1

CLEANFILES = $(check_SCRIPTS)

# test_nls_LDADD = $(top_builddir)/libexif/libexif.la $(INTLLIBS)

EXTRA_DIST = check-localedir.in check-nls.in test-nls.c test-codeset.c

.in.sh: Makefile
	sed 's|@top_builddir\@|$(top_builddir)|g;s|@localedir\@|$(localedir)|g;s|@PRINT_LOCALEDIR\@|./print-localedir$(EXEEXT)|g;s|@build_alias\@|$(build_alias)|g;s|@host_alias\@|$(host_alias)|g;s|@codeset_tests\@|$(codeset_tests)|g;s|@DESTDIR\@|$(DESTDIR)|g' < $< > $@
	chmod +x $@
