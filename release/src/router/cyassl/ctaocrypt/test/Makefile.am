INCLUDES = -I../include -I../../include
bin_PROGRAMS = test
test_SOURCES = test.c
test_LDFLAGS = -L../src
test_LDADD        = ../../src/libcyassl.la -lm
test_DEPENDENCIES = ../../src/libcyassl.la
EXTRA_DIST =  test.dsp test.dsw test.sln test.vcproj
