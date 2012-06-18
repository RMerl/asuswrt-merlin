SUBDIRS = nls

# Notes about tests:
#  - Add "small" tests and stuff here.
#  - Add "big"   tests and stuff to explicitly test for (fixed) bugs
#    to libexif-testsuite:
#    * We don't want to force people to download dozens of test images
#      just for one small library.
#    * Proper testing is done with the "exif" program in libexif-testsuite.
#      And this is just the lib - we don't have the program available
#      here yet.

TESTS = test-mem test-value test-integers test-parse test-tagtable test-sorted

TEST_IMAGES = $(top_srcdir)/daniel-andrews-sample.jpg
export TEST_IMAGES

check_PROGRAMS = test-mem test-mnote test-value test-integers test-parse \
	test-tagtable test-sorted

LDADD = $(top_builddir)/libexif/libexif.la $(LTLIBINTL)
