INCLUDES = -I../../include
bin_PROGRAMS    = echoclient
echoclient_SOURCES  = echoclient.c
echoclient_CFLAGS = -Wall -Wno-unused
echoclient_LDFLAGS     = -L../../src 
echoclient_LDADD       = ../../src/libcyassl.la -lm
echoclient_DEPENDENCIES= ../../src/libcyassl.la
EXTRA_DIST = echoclient.dsp input quit echoclient.sln echoclient.vcproj
