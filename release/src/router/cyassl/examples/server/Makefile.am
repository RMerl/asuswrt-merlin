INCLUDES = -I../../include
bin_PROGRAMS    = server
server_SOURCES  = server.c
server_CFLAGS = -Wall
server_LDFLAGS      = -L../../src
server_LDADD        = ../../src/libcyassl.la -lm
server_DEPENDENCIES = ../../src/libcyassl.la
EXTRA_DIST = server.dsp server.sln server.vcproj
