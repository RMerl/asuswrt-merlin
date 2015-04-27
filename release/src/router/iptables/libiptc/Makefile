#! /usr/bin/make

EXTRAS+=libiptc/libiptc.a

CFLAGS+=-fPIC

# libiptc is not a public interface and therefore not installed
# DEVEL_LIBS+=libiptc/libiptc.a

ifndef TOPLEVEL_INCLUDED
local:
	cd .. && $(MAKE) $(KERN_TARGETS) $(SHARED_LIBS) $(EXTRAS)

else
EXTRA_DEPENDS+=libiptc/libip4tc.d

libiptc/libiptc.a: libiptc/libiptc.a(libiptc/libip4tc.o)

ifeq ($(DO_IPV6), 1)
EXTRA_DEPENDS+= libiptc/libip6tc.d
libiptc/libiptc.a: libiptc/libiptc.a(libiptc/libip6tc.o)
endif

libiptc/libip4tc.d libiptc/libip6tc.d: %.d: %.c
	@-$(CC) -M -MG $(CFLAGS) $< | sed -e 's@^.*\.o:@$*.d libiptc/libiptc.a($*.o):@' > $@
endif
