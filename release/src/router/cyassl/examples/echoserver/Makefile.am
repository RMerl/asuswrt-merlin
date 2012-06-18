INCLUDES = -I../../include
bin_PROGRAMS    = echoserver
echoserver_SOURCES  = echoserver.c
echoserver_CFLAGS = -Wall
echoserver_LDFLAGS  = -L../../src
echoserver_LDADD       = ../../src/libcyassl.la -lm
echoserver_DEPENDENCIES= ../../src/libcyassl.la
EXTRA_DIST = echoserver.dsp echoserver.sln echoserver.vcproj
