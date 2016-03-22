#!/bin/sh
#
# build using pthreads where it's already built into the system
#
/bin/rm -f mttest
gcc -DPTHREADS -I../../include -g mttest.c -o mttest -L../.. -lssl-1.0.0q -lcrypto-1.0.0q -lpthread

