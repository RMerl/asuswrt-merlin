# build/os-auto.mak.  Generated from os-auto.mak.in by configure.

export OS_CFLAGS   := $(CC_DEF)PJ_AUTOCONF=1 -I/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl/include -g -O2 -fPIC -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1

export OS_CXXFLAGS := $(CC_DEF)PJ_AUTOCONF=1 -I/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl/include -g -O2 -fPIC 

export OS_LDFLAGS  := -L/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl -lstdc++ -lm -lnsl -lrt -lpthread  -lssl -lcrypto  -lcrypto -lssl

export OS_SOURCES  := 


