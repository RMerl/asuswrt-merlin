#!/bin/sh
/bin/rm -f mttest
cc -DSOLARIS -I../../include -g mttest.c -o mttest -L../.. -lthread  -lssl-1.0.0q -lcrypto-1.0.0q -lnsl -lsocket

