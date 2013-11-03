# procps Makefile
# Albert Cahalan, 2002-2004
#
# Recursive make is considered harmful:
# http://google.com/search?q=%22recursive+make+considered+harmful%22
#
# For now this Makefile uses explicit dependencies. The project
# hasn't grown big enough to need something complicated, and the
# dependency tracking files are an ugly annoyance.
#
# This file includes */module.mk files which add on to variables:
# FOO += bar/baz
#
#
# Set (or uncomment) SKIP if you wish to avoid something.
# For example, you may prefer the /bin/kill from util-linux or bsdutils.


VERSION      := 3
SUBVERSION   := 2
MINORVERSION := 8
TARVERSION   := $(VERSION).$(SUBVERSION).$(MINORVERSION)

############ vars

# so you can disable them or choose alternates
ldconfig := ldconfig
ln_f     := ln -f
ln_sf    := ln -sf
install  := install -D --owner 0 --group 0

# Lame x86-64 /lib64 and /usr/lib64 abomination:
lib64    := lib$(shell [ -d /lib64 ] && echo 64)

usr/bin                  := $(DESTDIR)/usr/bin/
bin                      := $(DESTDIR)/bin/
sbin                     := $(DESTDIR)/sbin/
usr/proc/bin             := $(DESTDIR)/usr/bin/
man1                     := $(DESTDIR)/usr/share/man/man1/
man5                     := $(DESTDIR)/usr/share/man/man5/
man8                     := $(DESTDIR)/usr/share/man/man8/
lib                      := $(DESTDIR)/$(lib64)/
usr/lib                  := $(DESTDIR)/usr/$(lib64)/
usr/include              := $(DESTDIR)/usr/include/

#SKIP     := $(bin)kill $(man1)kill.1

#BINFILES := $(usr/bin)uptime $(usr/bin)tload $(usr/bin)free $(usr/bin)w \
            $(usr/bin)top $(usr/bin)vmstat $(usr/bin)watch $(usr/bin)skill \
            $(usr/bin)snice $(bin)kill $(sbin)sysctl $(usr/bin)pmap \
            $(usr/proc/bin)pgrep $(usr/proc/bin)pkill $(usr/bin)slabtop \
            $(usr/proc/bin)pwdx
BINFILES := $(usr/bin)vmstat $(usr/bin)pmap

#MANFILES := $(man1)uptime.1 $(man1)tload.1 $(man1)free.1 $(man1)w.1 \
            $(man1)top.1 $(man1)watch.1 $(man1)skill.1 $(man1)kill.1 \
            $(man1)snice.1 $(man1)pgrep.1 $(man1)pkill.1 $(man1)pmap.1 \
            $(man5)sysctl.conf.5 $(man8)vmstat.8 $(man8)sysctl.8 \
            $(man1)slabtop.1 $(man1)pwdx.1

#TARFILES := AUTHORS BUGS NEWS README TODO COPYING COPYING.LIB \
            Makefile procps.lsm procps.spec v t README.top CodingStyle \
            sysctl.conf minimal.c $(notdir $(MANFILES)) dummy.c \
            uptime.c tload.c free.c w.c top.c vmstat.c watch.c skill.c \
            sysctl.c pgrep.c top.h pmap.c slabtop.c pwdx.c
TARFILES := vmstat.c pmap.c

# Stuff (tests, temporary hacks, etc.) left out of the standard tarball
# plus the top-level Makefile to make it work stand-alone.
_TARFILES := Makefile

CURSES := -lncurses

# This seems about right for the dynamic library stuff.
# Something like this is probably needed to make the SE Linux
# library loading not conflict with embedded systems stuff.
#
#ifeq ($(SHARED),1)
#ldl := -ldl
#LIBTYPE := -DSHAREDLIB
#else
#LIBTYPE := -DSTATICLIB
#endif

# Preprocessor flags.
PKG_CPPFLAGS := -D_GNU_SOURCE -I proc
CPPFLAGS     := -I/usr/include/ncurses
ALL_CPPFLAGS := $(PKG_CPPFLAGS) $(CPPFLAGS)

# Left out -Wconversion due to noise in glibc headers.
# Left out -Wunreachable-code and -Wdisabled-optimization
# because gcc spews many useless warnings with them.
#
# Since none of the PKG_CFLAGS things are truly required
# to compile procps, they might best be moved to CFLAGS.
# On the other hand, they aren't normal -O -g things either.
#
# Note that -O2 includes -fomit-frame-pointer only if the arch
# doesn't lose some debugging ability.
#
PKG_CFLAGS   := -fno-common -ffast-math \
  -W -Wall -Wshadow -Wcast-align -Wredundant-decls \
  -Wbad-function-cast -Wcast-qual -Wwrite-strings -Waggregate-return \
  -Wstrict-prototypes -Wmissing-prototypes
# Note that some stuff below is conditional on CFLAGS containing
# an option that starts with "-g". (-g, -g2, -g3, -ggdb, etc.)
CFLAGS       := -O2 -s
ALL_CFLAGS   := $(PKG_CFLAGS) $(CFLAGS)

PKG_LDFLAGS  := -Wl,-warn-common
LDFLAGS      :=
ALL_LDFLAGS  := $(PKG_LDFLAGS) $(LDFLAGS)

############ Add some extra flags if gcc allows

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),tar)  
ifneq ($(MAKECMDGOALS),extratar)
ifneq ($(MAKECMDGOALS),beta)

# Unlike the kernel one, this check_gcc goes all the way to
# producing an executable. There might be a -m64 that works
# until you go looking for a 64-bit curses library.
check_gcc = $(shell if $(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) dummy.c $(ALL_LDFLAGS) $(1) -o /dev/null $(CURSES) > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

# Be 64-bit if at all possible. In a cross-compiling situation, one may
# do "make m64=-m32 lib64=lib" to produce 32-bit executables. DO NOT
# attempt to use a 32-bit executable on a 64-bit kernel. Packagers MUST
# produce separate executables for ppc and ppc64, s390 and s390x,
# i386 and x86-64, mips and mips64, sparc and sparc64, and so on.
# Failure to do so will cause data corruption.
m64 := $(call check_gcc,-m64,$(call check_gcc,-mabi=64,))
ALL_CFLAGS += $(m64)

ALL_CFLAGS += $(call check_gcc,-Wdeclaration-after-statement,)
ALL_CFLAGS += $(call check_gcc,-Wpadded,)
ALL_CFLAGS += $(call check_gcc,-Wstrict-aliasing,)

# Adding -fno-gcse might be good for those files which
# use computed goto.
#ALL_CFLAGS += $(call check_gcc,-fno-gcse,)

# if not debugging, enable things that could confuse gdb
ifeq (,$(findstring -g,$(filter -g%,$(CFLAGS))))
ALL_CFLAGS += $(call check_gcc,-fweb,)
ALL_CFLAGS += $(call check_gcc,-frename-registers,)
ALL_CFLAGS += $(call check_gcc,-fomit-frame-pointer,)
endif

# in case -O3 is enabled, avoid bloat
ALL_CFLAGS += $(call check_gcc,-fno-inline-functions,)

endif
endif
endif
endif

############ misc.

# free.c pmap.c sysctl.c uptime.c vmstat.c watch.c pgrep.c skill.c tload.c top.c w.c
# utmp.c oldtop.c tmp-junk.c minimal.c

.SUFFIXES:
.SUFFIXES: .a .o .c .s .h

.PHONY: all clean do_all install tar extratar beta

ALL := $(notdir $(BINFILES))

CLEAN := $(notdir $(BINFILES))

DIRS :=

INSTALL := $(BINFILES) $(MANFILES)

# want this rule first, use := on ALL, and ALL not filled in yet
all: do_all

-include */module.mk

do_all:    $(ALL)

junk := DEADJOE *~ *.o core gmon.out

# Remove $(junk) from all $(DIRS)
CLEAN += $(junk) $(foreach dir,$(DIRS),$(addprefix $(dir), $(junk)))

##########
# not maintained because it isn't really needed:
#
#SRC :=
#OBJ := $(patsubst %.c,%.o, $(filter %.c,$(SRC)))
#
#ifneq ($(MAKECMDGOALS),clean)
#-include $(OBJ:.o=.d)
#endif
#
#%.d: %.c
#	depend.sh $(ALL_CPPFLAGS) $(ALL_CFLAGS) $< > $@
############

# don't want to type "make procps-$(TARVERSION).tar.gz"
tar: $(TARFILES)
	mkdir procps-$(TARVERSION)
	(tar cf - $(TARFILES)) | (cd procps-$(TARVERSION) && tar xf -)
	tar cf procps-$(TARVERSION).tar procps-$(TARVERSION)
	gzip -9 procps-$(TARVERSION).tar

extratar: $(_TARFILES)
	mkdir procps-$(TARVERSION)
	(tar cf - $(_TARFILES)) | (cd procps-$(TARVERSION) && tar xf -)
	tar cf extra-$(TARVERSION).tar procps-$(TARVERSION)
	gzip -9 extra-$(TARVERSION).tar

beta: $(TARFILES) $(_TARFILES)
	mkdir beta-$(TARVERSION)
	(tar cf - $(TARFILES) $(_TARFILES)) | (cd beta-$(TARVERSION) && tar xf -)
	tar cf beta-$(TARVERSION).tar beta-$(TARVERSION)
	gzip -9 beta-$(TARVERSION).tar

clean:
	rm -f $(CLEAN)

###### install

$(BINFILES) : all
	$(install) --mode a=rx $(notdir $@) $@

$(MANFILES) : all
	$(install) --mode a=r $(notdir $@) $@

install: $(filter-out $(SKIP) $(addprefix $(DESTDIR),$(SKIP)),$(INSTALL))
	cd $(usr/bin) && $(ln_f) skill snice
	cd $(usr/proc/bin) && $(ln_f) pgrep pkill

############ prog.c --> prog.o

top.o : top.h

%.o : %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

w.o:    w.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(W_SHOWFROM) -c $<

############ prog.o --> prog

pmap w uptime tload free sysctl vmstat utmp pgrep skill pwdx: % : %.o $(LIBPROC)
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@

slabtop top: % : %.o $(LIBPROC)
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@ $(CURSES)

watch: % : %.o
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@ $(CURSES)

############ progX --> progY

snice kill: skill
	$(ln_f) skill $@

pkill: pgrep
	$(ln_f) pgrep pkill
