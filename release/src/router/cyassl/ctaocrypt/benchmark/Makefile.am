INCLUDES = -I../include
bin_PROGRAMS = benchmark
benchmark_SOURCES = benchmark.c
benchmark_LDFLAGS      = -L../src
benchmark_LDADD        = ../../src/libcyassl.la -lm
benchmark_DEPENDENCIES = ../../src/libcyassl.la
EXTRA_DIST =  benchmark.dsp *.der benchmark.sln benchmark.vcproj
