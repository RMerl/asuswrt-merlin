# uncomment this to get a fully statically linked version
# NO_SHARED_LIBS = 1

ifeq ($(RTCONFIG_IPV6),y)
DO_IPV6 = 1
else
DO_IPV6 = 0
endif

# uncomment this to disable IPv6 support
# DO_IPV6 = 0

DO_MULTI = 1

DO_SAVE = 1

######################################################################
# YOU SHOULD NOT NEED TO TOUCH ANYTHING BELOW THIS LINE
######################################################################

# Standard part of Makefile for topdir.
TOPLEVEL_INCLUDED=YES

ifndef KERNEL_DIR
KERNEL_DIR="/lib/modules/$(shell uname -r)/build"
endif
IPTABLES_VERSION:=1.3.8
OLD_IPTABLES_VERSION:=1.3.7

PREFIX:=/usr/local
LIBDIR:=$(PREFIX)/lib
BINDIR:=$(PREFIX)/sbin
MANDIR:=$(PREFIX)/man
INCDIR:=$(PREFIX)/include

# directory for new iptables releases
RELEASE_DIR:=/tmp

# Need libc6 for this.  FIXME: Should covert to autoconf.
ifndef DO_IPV6
ifeq ($(shell [ -f /usr/include/netinet/ip6.h ] && echo YES), YES)
DO_IPV6:=1
endif
endif

# Enable linking to libselinux via enviornment 'DO_SELINUX=1'
ifndef DO_SELINUX
DO_SELINUX=0
endif

#COPT_FLAGS:=-Os #-mips32
CFLAGS:=$(COPT_FLAGS) -Wall -Wunused -I$(KERNEL_DIR)/include -Iinclude/ -DIPTABLES_VERSION=\"$(IPTABLES_VERSION)\" #-g -DDEBUG #-pg # -DIPTC_DEBUG

ifdef NO_SHARED_LIBS
CFLAGS += -DNO_SHARED_LIBS=1
endif

EXTRAS+=iptables iptables.o iptables.8
EXTRA_INSTALLS+=$(DESTDIR)$(BINDIR)/iptables $(DESTDIR)$(MANDIR)/man8/iptables.8

# No longer experimental.
ifneq ($(DO_MULTI), 1)
EXTRAS+=iptables-save iptables-restore iptables-xml
endif
EXTRA_INSTALLS+=$(DESTDIR)$(BINDIR)/iptables-save $(DESTDIR)$(BINDIR)/iptables-restore $(DESTDIR)$(BINDIR)/iptables-xml $(DESTDIR)$(MANDIR)/man8/iptables-restore.8 $(DESTDIR)$(MANDIR)/man8/iptables-save.8

ifeq ($(DO_IPV6), 1)
EXTRAS+=ip6tables ip6tables.o ip6tables.8
EXTRA_INSTALLS+=$(DESTDIR)$(BINDIR)/ip6tables $(DESTDIR)$(MANDIR)/man8/ip6tables.8
ifneq ($(DO_MULTI), 1)
EXTRAS_+=ip6tables-save ip6tables-restore
endif
EXTRA_INSTALLS+=$(DESTDIR)$(BINDIR)/ip6tables-save $(DESTDIR)$(BINDIR)/ip6tables-restore # $(DESTDIR)$(MANDIR)/man8/iptables-restore.8 $(DESTDIR)$(MANDIR)/man8/iptables-save.8 $(DESTDIR)$(MANDIR)/man8/ip6tables-save.8 $(DESTDIR)$(MANDIR)/man8/ip6tables-restore.8
endif

# Sparc64 hack
#	ifeq ($(shell uname -m),sparc64)
#		POINTERTEST:=1
#		32bituser := $(shell echo -e "\#include <stdio.h>\n\#if !defined(__sparcv9) && !defined(__arch64__) && !defined(_LP64)\nuserspace_is_32bit\n\#endif" | $(CC) $(CFLAGS) -E - | grep userspace_is_32bit)
#		ifdef 32bituser
#			# The kernel is 64-bit, even though userspace is 32.
#			CFLAGS+=-DIPT_MIN_ALIGN=8 -DKERNEL_64_USERSPACE_32
#		else
#			EXT_LDFLAGS+=-Wl,-m,elf64_sparc
#		endif
#	endif

# Alpha only has 64bit userspace and fails the test below
#	ifeq ($(shell uname -m), alpha)
#		POINTERTEST:=1
#	endif

# Generic test if arch wasn't found above
#	ifneq ($(POINTERTEST),1)
#		# Try to determine if kernel is 64bit and we are compiling for 32bit
#		ifeq ($(shell [ -d $(KERNEL_DIR)/include/asm ] && echo YES), YES)
#			64bitkernel := $(shell echo -e "\#include <asm/types.h>\n\#if BITS_PER_LONG == 64\nkernel_is_64bits\n\#endif" | $(CC) $(CFLAGS) -D__KERNEL__ -E - | grep kernel_is_64bits)
#			ifdef 64bitkernel
#				32bituser := $(shell echo -e "\#include <stdio.h>\n\#if !defined(__arch64__) && !defined(_LP64)\nuserspace_is_32bit\n\#endif" | $(CC) $(CFLAGS) -E - | grep userspace_is_32bit)
#				ifdef 32bituser
#					CFLAGS+=-DIPT_MIN_ALIGN=8 -DKERNEL_64_USERSPACE_32
#				endif
#			endif
#		else
#			CFLAGS+=-D_UNKNOWN_KERNEL_POINTER_SIZE
#		endif
#	endif

ifndef IPT_LIBDIR
IPT_LIBDIR:=$(LIBDIR)/iptables
endif

ifndef NO_SHARED_LIBS
DEPFILES = $(SHARED_LIBS:%.so=%.d)
DEPFILES += $(SHARED_SE_LIBS:%.so=%.d)
SH_CFLAGS:=$(CFLAGS) -fPIC
STATIC_LIBS  =
STATIC6_LIBS =
LDFLAGS      = -rdynamic
LDLIBS       = -ldl
ifeq ($(DO_SELINUX), 1)
LDLIBS       += -lselinux
endif
else
DEPFILES = $(EXT_OBJS:%.o=%.d)
STATIC_LIBS  = extensions/libext.a
STATIC6_LIBS = extensions/libext6.a
LDFLAGS      = -static
LDLIBS	     =
ifeq ($(DO_SELINUX), 1)
LDLIBS       += -lselinux
endif
endif

STATIC6_LIBS += libiptc/libip6tc.a
LDLIBS       += -L. -liptc

.PHONY: default
default: libiptc.so all

.PHONY: print-extensions
print-extensions:
	@[ -n "$(OPTIONALS)" ] && echo Extensions found: $(OPTIONALS)

iptables.o: iptables.c
	$(CC) $(CFLAGS) -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" -c -o $@ $<

libiptc.so: libiptc/libiptc.a
	$(LD) -shared -o $@ --whole-archive -Llibiptc -liptc


ifeq ($(DO_MULTI), 1)

ifeq ($(DO_SAVE), 1)
iptables: iptables-multi.c iptables-save.c iptables-restore.c iptables-standalone.c iptables.o $(STATIC_LIBS)
	$(CC) $(CFLAGS) -DIPTABLES_MULTI -DIPTABLES_SAVE -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
else
iptables: iptables-multi.c iptables-restore.c iptables-standalone.c iptables.o $(STATIC_LIBS)
	$(CC) $(CFLAGS) -DIPTABLES_MULTI -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
endif

#	iptables: iptables-multi.c iptables-save.c iptables-restore.c iptables-xml.c iptables-standalone.c iptables.o $(STATIC_LIBS)
#		$(CC) $(CFLAGS) -DIPTABLES_MULTI -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)

else
iptables: iptables-standalone.c iptables.o $(STATIC_LIBS)
	$(CC) $(CFLAGS) -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
endif


$(DESTDIR)$(BINDIR)/iptables: iptables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@

iptables-save: iptables-save.c iptables.o $(STATIC_LIBS)
	$(CC) $(CFLAGS) -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(DO_MULTI), 1)
$(DESTDIR)$(BINDIR)/iptables-save: iptables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	ln -sf $< $@
else
$(DESTDIR)$(BINDIR)/iptables-save: iptables-save
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@
endif

iptables-restore: iptables-restore.c iptables.o $(STATIC_LIBS)
	$(CC) $(CFLAGS) -DIPT_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(DO_MULTI), 1)
$(DESTDIR)$(BINDIR)/iptables-restore: iptables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	ln -sf $< $@
else
$(DESTDIR)$(BINDIR)/iptables-restore: iptables-restore
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@
endif

iptables-xml: iptables-xml.c #iptables.o # $(STATIC_LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(DO_MULTI), 1)
$(DESTDIR)$(BINDIR)/iptables-xml: iptables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	ln -sf $< $@
else
$(DESTDIR)$(BINDIR)/iptables-xml: iptables-xml
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@
endif

ip6tables.o: ip6tables.c
	$(CC) $(CFLAGS) -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" -c -o $@ $<

ifeq ($(DO_MULTI), 1)
ifeq ($(DO_SAVE), 1)
ip6tables: ip6tables-multi.c ip6tables-save.c ip6tables-restore.c ip6tables-standalone.c ip6tables.o $(STATIC6_LIBS)
	$(CC) $(CFLAGS) -DIPTABLES_MULTI -DIPTABLES_SAVE -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
else
ip6tables: ip6tables-multi.c ip6tables-restore.c ip6tables-standalone.c ip6tables.o $(STATIC6_LIBS)
	$(CC) $(CFLAGS) -DIPTABLES_MULTI -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
endif
else
ip6tables: ip6tables-standalone.c ip6tables.o $(STATIC6_LIBS)
	$(CC) $(CFLAGS) -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)
endif

$(DESTDIR)$(BINDIR)/ip6tables: ip6tables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@

ip6tables-save: ip6tables-save.c ip6tables.o $(STATIC6_LIBS)
	$(CC) $(CFLAGS) -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(DO_MULTI), 1)
$(DESTDIR)$(BINDIR)/ip6tables-save: ip6tables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	ln -sf $< $@
else
$(DESTDIR)$(BINDIR)/ip6tables-save: ip6tables-save
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@
endif

ip6tables-restore: ip6tables-restore.c ip6tables.o $(STATIC6_LIBS)
	$(CC) $(CFLAGS) -DIP6T_LIB_DIR=\"$(IPT_LIBDIR)\" $(LDFLAGS) -o $@ $^ $(LDLIBS)

ifeq ($(DO_MULTI), 1)
$(DESTDIR)$(BINDIR)/ip6tables-restore: ip6tables
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	ln -sf $< $@
else
$(DESTDIR)$(BINDIR)/ip6tables-restore: ip6tables-restore
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@
endif

$(DESTDIR)$(MANDIR)/man8/%.8: %.8
	@[ -d $(DESTDIR)$(MANDIR)/man8 ] || mkdir -p $(DESTDIR)$(MANDIR)/man8
	cp $< $@

EXTRA_DEPENDS+=iptables-standalone.d iptables.d

iptables-standalone.d iptables.d: %.d: %.c
	@-$(CC) -M -MG $(CFLAGS) $< | sed -e 's@^.*\.o:@$*.d $*.o:@' > $@

iptables.8: iptables.8.in extensions/libipt_matches.man extensions/libipt_targets.man
	sed -e '/@MATCH@/ r extensions/libipt_matches.man' -e '/@TARGET@/ r extensions/libipt_targets.man' iptables.8.in >iptables.8

ip6tables.8: ip6tables.8.in extensions/libip6t_matches.man extensions/libip6t_targets.man
	sed -e '/@MATCH@/ r extensions/libip6t_matches.man' -e '/@TARGET@/ r extensions/libip6t_targets.man' ip6tables.8.in >ip6tables.8

# Development Targets
.PHONY: install-devel-man3
install-devel-man3: $(DEVEL_MAN3)
	@[ -d $(DESTDIR)$(MANDIR)/man3 ] || mkdir -p $(DESTDIR)$(MANDIR)/man3
	@cp -v $(DEVEL_MAN3) $(DESTDIR)$(MANDIR)/man3

.PHONY: install-devel-headers
install-devel-headers: $(DEVEL_HEADERS)
	@[ -d $(DESTDIR)$(INCDIR) ] || mkdir -p $(DESTDIR)$(INCDIR)
	@cp -v $(DEVEL_HEADERS) $(DESTDIR)$(INCDIR)

.PHONY: install-devel-libs
install-devel-libs: $(DEVEL_LIBS)
	@[ -d $(DESTDIR)$(LIBDIR) ] || mkdir -p $(DESTDIR)$(LIBDIR)
	@cp -v $(DEVEL_LIBS) $(DESTDIR)$(LIBDIR)

.PHONY: install-devel
install-devel: all install-devel-man3 install-devel-headers install-devel-libs

.PHONY: distclean
distclean: clean
	@rm -f TAGS `find . -name '*~' -o -name '.*~'` `find . -name '*.rej'` `find . -name '*.d'` .makefirst

# Rusty's distro magic.
.PHONY: distrib
distrib: check distclean delrelease $(RELEASE_DIR)/iptables-$(IPTABLES_VERSION).tar.bz2 diff md5sums # nowhitespace

# Makefile must not define:
# -g -pg -DIPTC_DEBUG
.PHONY: check
check:
	@if echo $(CFLAGS) | egrep -e '(^|[[:space:]])(-g|-pg|-DIPTC_DEBUG)([[:space:]]|$)' >/dev/null; then echo Remove debugging flags; exit 1; else exit 0; fi

.PHONY: nowhitespace
nowhitespace:
	@if grep -n '[ 	]$$' `find . -name 'Makefile' -o -name '*.[ch]'`; then exit 1; else exit 0; fi

.PHONY: delrelease
delrelease:
	rm -f $(RELEASE_DIR)/iptables-$(IPTABLES_VERSION).tar.bz2

$(RELEASE_DIR)/iptables-$(IPTABLES_VERSION).tar.bz2:
	cd .. && ln -sf iptables iptables-$(IPTABLES_VERSION) && tar cvf - --exclude .svn iptables-$(IPTABLES_VERSION)/. | bzip2 -9 > $@ && rm iptables-$(IPTABLES_VERSION)

.PHONY: diff
diff: $(RELEASE_DIR)/iptables-$(IPTABLES_VERSION).tar.bz2
	@mkdir /tmp/diffdir
	@cd /tmp/diffdir && tar -x --bzip2 -f $(RELEASE_DIR)/iptables-$(IPTABLES_VERSION).tar.bz2
	@set -e; cd /tmp/diffdir; tar -x --bzip2 -f $(RELEASE_DIR)/iptables-$(OLD_IPTABLES_VERSION).tar.bz2; echo Creating patch-iptables-$(OLD_IPTABLES_VERSION)-$(IPTABLES_VERSION).bz2; diff -urN iptables-$(OLD_IPTABLES_VERSION) iptables-$(IPTABLES_VERSION) | bzip2 -9 > $(RELEASE_DIR)/patch-iptables-$(OLD_IPTABLES_VERSION)-$(IPTABLES_VERSION).bz2
	@rm -rf /tmp/diffdir

.PHONY: md5sums
md5sums:
	cd $(RELEASE_DIR)/ && md5sum patch-iptables-*-$(IPTABLES_VERSION).bz2 iptables-$(IPTABLES_VERSION).tar.bz2

# $(wildcard) fails wierdly with make v.3.78.1.
include $(shell echo */Makefile)
include Rules.make
