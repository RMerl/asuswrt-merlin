#!/usr/bin/make

######################################################################
# YOU SHOULD NOT NEED TO TOUCH ANYTHING BELOW THIS LINE
######################################################################

ifndef KERNEL_DIR
KERNEL_DIR=/lib/modules/`uname -r`/build
endif
ifndef KBUILD_OUTPUT
KBUILD_OUTPUT=$(KERNEL_DIR)
endif
ifndef IP_NF_SET_MAX
IP_NF_SET_MAX=256
endif
ifndef IP_NF_SET_HASHSIZE
IP_NF_SET_HASHSIZE=1024
endif
ifndef V
V=0
endif

IPSET_VERSION:=4.5

PREFIX:=/usr
LIBDIR:=$(PREFIX)/lib
BINDIR:=$(PREFIX)/sbin
MANDIR:=$(PREFIX)/man
INCDIR:=$(PREFIX)/include
IPSET_LIB_DIR:=$(LIBDIR)/ipset

# directory for new iptables releases
RELEASE_DIR:=/tmp

COPT_FLAGS:=-O2
WARN_FLAGS:=-Wall
EXTRA_WARN_FLAGS:=\
	-Wextra \
	-Waggregate-return \
	-Wbad-function-cast \
	-Wcast-align \
	-Wformat=2 \
	-Wfloat-equal \
	-Winit-self \
	-Winline \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wold-style-definition \
	-Wpacked \
	-Wredundant-decls \
	-Wshadow \
	-Wsign-compare \
	-Wstrict-prototypes \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef \
	-Wwrite-strings \
	-Wno-missing-field-initializers \
	-Werror

ifndef NO_EXTRA_WARN_FLAGS
    WARN_FLAGS+=$(EXTRA_WARN_FLAGS)
endif

ABI_FLAGS:=
CFLAGS:=$(ABI_FLAGS) $(COPT_FLAGS) $(WARN_FLAGS) -Ikernel/include -I. # -g -DIPSET_DEBUG
CFLAGS+=-DIP_NF_SET_HASHSIZE=$(IP_NF_SET_HASHSIZE)
SH_CFLAGS:=$(CFLAGS) -fPIC
LDFLAGS:=$(ABI_FLAGS)
SETTYPES:=ipmap portmap macipmap
SETTYPES+=iptree iptreemap
SETTYPES+=iphash nethash ipporthash ipportiphash ipportnethash
SETTYPES+=setlist

PROGRAMS=ipset
SHARED_LIBS=$(foreach T, $(SETTYPES),libipset_$(T).so)
INSTALL=$(DESTDIR)$(BINDIR)/ipset $(DESTDIR)$(MANDIR)/man8/ipset.8
INSTALL+=$(foreach T, $(SETTYPES), $(DESTDIR)$(LIBDIR)/ipset/libipset_$(T).so)

all: binaries modules

.PHONY: tests

tests:
	cd tests; ./runtest.sh

binaries: $(PROGRAMS) $(SHARED_LIBS)

binaries_install: binaries $(INSTALL)

patch_kernel:
	cd kernel; ./patch_kernel $(KERNEL_DIR)

modules:
	@[ ! -f $(KERNEL_DIR)/net/ipv4/netfilter/Config.in ] || (echo "Error: The directory '$(KERNEL_DIR)' looks like a Linux 2.4.x kernel source tree, you have to patch it by 'make patch_kernel'." && exit 1)
	@[ -f $(KERNEL_DIR)/net/ipv4/netfilter/Kconfig ] || (echo "Error: The directory '$(KERNEL_DIR)' doesn't look like a Linux 2.6.x kernel source tree." && exit 1)
	@[ -f $(KBUILD_OUTPUT)/.config ] || (echo "Error: The kernel source in '$(KERNEL_DIR)' must be configured" && exit 1)
	@[ -f $(KBUILD_OUTPUT)/Module.symvers ] || echo "Warning: You should run 'make modules' in '$(KERNEL_DIR)' beforehand"
	cd kernel; make -C $(KBUILD_OUTPUT) M=`pwd` V=$V IP_NF_SET_MAX=$(IP_NF_SET_MAX) IP_NF_SET_HASHSIZE=$(IP_NF_SET_HASHSIZE) modules

modules_install: modules
	cd kernel; make -C $(KBUILD_OUTPUT) M=`pwd` modules_install

install: binaries_install modules_install

clean: $(EXTRA_CLEANS)
	rm -rf $(PROGRAMS) $(SHARED_LIBS) *.o *~ tests/*~
	[ -f $(KERNEL_DIR)/net/ipv4/netfilter/Config.in ] || (cd kernel; make -C $(KERNEL_DIR) M=`pwd` clean)

release: clean
	cp -a . /tmp/ipset-$(IPSET_VERSION)
	tar cjf ../ipset-$(IPSET_VERSION).tar.bz2 -C /tmp --exclude=.git ipset-$(IPSET_VERSION)
	rm -rf /tmp/ipset-$(IPSET_VERSION)

#The ipset(8) self
ipset.o: ipset.c ipset.h
	$(CC) $(CFLAGS) -DIPSET_VERSION=\"$(IPSET_VERSION)\" -DIPSET_LIB_DIR=\"$(IPSET_LIB_DIR)\" -c -o $@ $<

ipset: ipset.o
	$(CC) $(CFLAGS) $(LDFLAGS) -rdynamic -o $@ $^ -ldl

#Pooltypes
ipset_%.o: ipset_%.c ipset.h
	$(CC) $(SH_CFLAGS) -o $@ -c $<

libipset_%.so: ipset_%.o
	$(CC) -shared $(LDFLAGS) -o $@ $<

$(DESTDIR)$(LIBDIR)/ipset/libipset_%.so: libipset_%.so
	@[ -d $(DESTDIR)$(LIBDIR)/ipset ] || mkdir -p $(DESTDIR)$(LIBDIR)/ipset
	cp $< $@

$(DESTDIR)$(BINDIR)/ipset: ipset
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	cp $< $@

$(DESTDIR)$(MANDIR)/man8/ipset.8: ipset.8
	@[ -d $(DESTDIR)$(MANDIR)/man8 ] || mkdir -p $(DESTDIR)$(MANDIR)/man8
	cp $< $@
