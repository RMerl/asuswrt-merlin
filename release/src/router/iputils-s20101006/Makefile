# Path to parent kernel include files directory
LIBC_INCLUDE=/usr/include

DEFINES= 

#options if you have a bind>=4.9.4 libresolv (or, maybe, glibc)
LDLIBS=
ADDLIB=

#options if you compile with libc5, and without a bind>=4.9.4 libresolv
# NOT AVAILABLE. Please, use libresolv.

CC=gcc
# What a pity, all new gccs are buggy and -Werror does not work. Sigh.
#CCOPT=-D_GNU_SOURCE -O2 -Wstrict-prototypes -Wall -g -Werror
CCOPT=-D_GNU_SOURCE -O2 -Wstrict-prototypes -Wall -g
CFLAGS =$(CCOPT) $(GLIBCFIX) $(DEFINES) -I$(KERNEL_INCLUDE) -I../include 

LDFLAGS = -L$(LIBDIR) -L$(TOP)/libbcmcrypto
CFLAGS += $(LDFLAGS)

#IPV4_TARGETS=tracepath ping clockdiff rdisc arping tftpd rarpd
IPV4_TARGETS=tracepath clockdiff
IPV6_TARGETS=tracepath6 traceroute6 ping6
TARGETS=$(IPV4_TARGETS) $(IPV6_TARGETS)
#TARGETS=$(IPV6_TARGETS)

LASTTAG:=`git describe HEAD | sed -e 's/-.*//'`
TAG:=`date +s%Y%m%d`

all:$(TARGETS)

tftpd: tftpd.o tftpsubs.o
arping: arping.o -lsysfs
ping: ping.o ping_common.o
#ping6: ping6.o ping_common.o -lresolv -lcrypto
ping.o ping6.o ping_common.o: ping_common.h
tftpd.o tftpsubs.o: tftp.h

rdisc_srv: rdisc_srv.o

rdisc_srv.o: rdisc.c
	$(CC) $(CFLAGS) -DRDISC_SERVER -o rdisc_srv.o rdisc.c

ping6:	ping6.o ping_common.o
	$(CC) $(LDFLAGS) -o ping6 ping6.o ping_common.o -lresolv -lcrypto 


check-kernel:
ifeq ($(KERNEL_INCLUDE),)
	@echo "Please, set correct KERNEL_INCLUDE"; false
else ifneq (2_6_36,$(LINUX_VERSION)) 
	@set -e; \
	if [ ! -r $(KERNEL_INCLUDE)/linux/autoconf.h ]; then \
		echo "Please, set correct KERNEL_INCLUDE"; false; fi
endif

modules: check-kernel
	$(MAKE) KERNEL_INCLUDE=$(KERNEL_INCLUDE) -C Modules

man:
	$(MAKE) -C doc man

html:
	$(MAKE) -C doc html

clean:
	@rm -f *.o $(TARGETS)
	@$(MAKE) -C Modules clean
	@$(MAKE) -C doc clean

snapshot:
	@if [ "`uname -n`" != "takos" ]; then echo "Not authorized to advance snapshot"; exit 1; fi
	@date "+[$(TAG)]" > RELNOTES.NEW
	@echo >>RELNOTES.NEW
	@git log $(LASTTAG).. | git shortlog >> RELNOTES.NEW
	@echo >> RELNOTES.NEW
	@cat RELNOTES >> RELNOTES.NEW
	@mv RELNOTES.NEW RELNOTES
	@date "+static char SNAPSHOT[] = \"$(TAG)\";" > SNAPSHOT.h
	@$(MAKE) -C doc snapshot
	@$(MAKE) man
	@git commit -a -m "iputils-$(TAG)"
	@git tag -s -m "iputils-$(TAG)" $(TAG)
	@git archive --format=tar --prefix=iputils-$(TAG)/ $(TAG) | bzip2 -9 > ../iputils-$(TAG).tar.bz2
