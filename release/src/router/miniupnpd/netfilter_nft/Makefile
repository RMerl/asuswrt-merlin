CFLAGS?=-Wall -g -D_GNU_SOURCE -DDEBUG -Wstrict-prototypes -Wdeclaration-after-statement
CC = gcc

LIBS = -lnftnl -lmnl

ARCH := $(shell uname -m | grep -q "x86_64" && echo 64)

all:	test_nfct_get testnftnlrdr 

clean:
	$(RM) *.o testnftnlcrdr testnftnlpinhole testnftnlrdr_peer \
		test_nfct_get testnftnlrdr 

testnftnlrdr:	nftnlrdr.o nftnlrdr_misc.o testnftnlrdr.o upnpglobalvars.o $(LIBS)

testiptpinhole:	testiptpinhole.o iptpinhole.o upnpglobalvars.o $(LIBS)

test_nfct_get:	test_nfct_get.o test_nfct_get.o -lmnl -lnetfilter_conntrack

test_nfct_get.o:	test_nfct_get.c

testnftnlrdr_peer.o:	testnftnlrdr_peer.c

testnftnlrdr_dscp.o:	testnftnlrdr_dscp.c

nftnlrdr.o:		nftnlrdr.c nftnlrdr.h 

nftnlrdr_misc.o:	nftnlrdr_misc.c

iptpinhole.o:		iptpinhole.c iptpinhole.h

upnpglobalvars.o:	../upnpglobalvars.c ../upnpglobalvars.h
	$(CC) -c -o $@ $<
