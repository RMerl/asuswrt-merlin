INCLUDES = -I../../include
bin_PROGRAMS    = client
client_SOURCES  = client.c
client_CFLAGS = -Wall -Wno-unused
client_LDFLAGS      = -L../../src
client_LDADD        = ../../src/libcyassl.la -lm
client_DEPENDENCIES = ../../src/libcyassl.la
EXTRA_DIST = client.dsp client.sln client.vcproj
