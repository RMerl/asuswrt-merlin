#
# This is the Makefile for WTFast.
#
# Copyright (C) 2015, AAA Internet Publishing, Inc. (DBA WTFast)
#

.PHONY: libgpnutils libgpnclient libev-4.20 gpnrd libwtf wtfd wtfslhd 

#all: libgpnutils libgpnclient libev-4.20 gpnrd libwtf wtfd wtfslhd 
all: libev-4.20

libgpnutils:
	$(MAKE) CPPFLAGS="-DDEBUG" -C $@
	cd $@ && \
	ln -sf ./$@.so.1.1.0 $@.so.1 && \
	ln -sf ./$@.so.1.1.0 $@.so

libgpnclient:
	$(MAKE) CPPFLAGS="-I../../openssl/include -I../libgpnutils" LDFLAGS="-L../libgpnutils -DDEBUG" -C $@
	cd $@ && \
	ln -sf ./$@.so.1.1.0 $@.so.1 && \
	ln -sf ./$@.so.1.1.0 $@.so

libev-4.20/Makefile:
	cd libev-4.20 && \
	$(CONFIGURE) --host=arm-linux

libev-4.20: libev-4.20/Makefile
	$(MAKE) -C $@

libev-4.20-clean:
	[ ! -f libev-4.20/Makefile ] || $(MAKE) -C libev-4.20 clean
	@rm -f libev-4.20/Makefile

libwtf/Makefile:
	cd libwtf && \
	aclocal; autoreconf -i; $(CONFIGURE) --host=arm-linux

libwtf: libwtf/Makefile
	$(MAKE) CPPFLAGS="-I../../../curl-7.21.7/include" -C $@

libwtf-clean:
	[ ! -f libwtf/Makefile ] || $(MAKE) -C libwtf clean
	@rm -f libwtf/Makefile

gpnrd:
	$(MAKE) ROUTER_BUILD=y CPPFLAGS="-I../libgpnutils -I../libgpnclient -I../../../../src-rt-6.x.4708/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3/arm-brcm-linux-uclibcgnueabi/sysroot/usr/include" LDFLAGS="-L../libgpnutils -L../libgpnclient -DDEBUG" -C $@

wtfd:
	$(MAKE) ROUTER_BUILD=y CPPFLAGS="-I../libgpnutils -I../libev-4.20 -I../libwtf -I$(SRCBASE) -I$(SRCBASE)/include -I$(TOP)/shared" LDFLAGS="-L../libgpnutils -L../libev-4.20/.libs/ -L../../nvram_arm -L$(TOP)/shared" -C $@

wtfslhd:
	$(MAKE) ROUTER_BUILD=y CPPFLAGS="-I../libgpnutils -I../libev-4.20 -I../libwtf -I$(SRCBASE) -I$(SRCBASE)/include -I$(TOP)/shared -I$(TOP)/zlib -I$(SRCBASE)/shared/bcmwifi/include -I$(SRCBASE)/common/include" LDFLAGS="-L../libgpnutils -L../libev-4.20/.libs/ -L$(TOP)/openssl -L$(TOP)/curl-7.21.7/lib/.libs -L../libwtf/src/.libs -L$(TOP)/nvram_arm -L$(TOP)/shared" -C $@

clean: libev-4.20-clean
