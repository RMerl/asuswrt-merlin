#!/bin/sh
/bin/rm -f mttest
cc -p -DSOLARIS -I../../include -g mttest.c -o mttest -L/usr/lib/libc -ldl -L../.. -lthread  -lssl-1.0.0q -lcrypto-1.0.0q -lnsl -lsocket

