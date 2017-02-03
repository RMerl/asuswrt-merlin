# ebtables Makefile

PROGNAME:=ebtables
PROGRELEASE:=4
PROGVERSION_:=2.0.10
PROGVERSION:=$(PROGVERSION_)-$(PROGRELEASE)
PROGDATE:=December\ 2011
LOCKFILE?=/var/lib/ebtables/lock
LOCKDIR:=$(shell echo $(LOCKFILE) | sed 's/\(.*\)\/.*/\1/')/

# default paths
LIBDIR:=/usr/lib
MANDIR:=/usr/local/man
BINDIR:=/usr/local/sbin
ETCDIR:=/etc
INITDIR:=/etc/rc.d/init.d
SYSCONFIGDIR:=/etc/sysconfig
DESTDIR:=

CFLAGS:=-Wall -Wunused -Werror
CFLAGS_SH_LIB:=-fPIC -O3
CC:=gcc

ifeq ($(shell uname -m),sparc64)
CFLAGS+=-DEBT_MIN_ALIGN=8 -DKERNEL_64_USERSPACE_32
endif

include extensions/Makefile

OBJECTS2:=getethertype.o communication.o libebtc.o \
useful_functions.o ebtables.o

OBJECTS:=$(OBJECTS2) $(EXT_OBJS) $(EXT_LIBS)

KERNEL_INCLUDES?=include/

ETHERTYPESPATH?=$(ETCDIR)
ETHERTYPESFILE:=$(ETHERTYPESPATH)/ethertypes

PIPE_DIR?=/tmp/$(PROGNAME)-v$(PROGVERSION)
PIPE=$(PIPE_DIR)/ebtablesd_pipe
EBTD_CMDLINE_MAXLN?=2048
EBTD_ARGC_MAX?=50

PROGSPECS:=-DPROGVERSION=\"$(PROGVERSION)\" \
	-DPROGNAME=\"$(PROGNAME)\" \
	-DPROGDATE=\"$(PROGDATE)\" \
	-D_PATH_ETHERTYPES=\"$(ETHERTYPESFILE)\" \
	-DEBTD_ARGC_MAX=$(EBTD_ARGC_MAX) \
	-DEBTD_CMDLINE_MAXLN=$(EBTD_CMDLINE_MAXLN) \
	-DLOCKFILE=\"$(LOCKFILE)\" \
	-DLOCKDIR=\"$(LOCKDIR)\"

# You can probably ignore this, ebtables{u,d} are normally not used
PROGSPECSD:=-DPROGVERSION=\"$(PROGVERSION)\" \
	-DPROGNAME=\"$(PROGNAME)\" \
	-DPROGDATE=\"$(PROGDATE)\" \
	-D_PATH_ETHERTYPES=\"$(ETHERTYPESFILE)\" \
	-DEBTD_CMDLINE_MAXLN=$(EBTD_CMDLINE_MAXLN) \
	-DEBTD_ARGC_MAX=$(EBTD_ARGC_MAX) \
	-DEBTD_PIPE=\"$(PIPE)\" \
	-DEBTD_PIPE_DIR=\"$(PIPE_DIR)\"

# Uncomment for debugging (slower)
#PROGSPECS+=-DEBT_DEBUG
#PROGSPECSD+=-DEBT_DEBUG
#CFLAGS+=-ggdb

all: ebtables ebtables-restore

communication.o: communication.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)

libebtc.o: libebtc.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)

useful_functions.o: useful_functions.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)

getethertype.o: getethertype.c include/ethernetdb.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c -o $@ $< -Iinclude/

ebtables.o: ebtables.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)

ebtables-standalone.o: ebtables-standalone.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(PROGSPECS) -c $< -o $@ -I$(KERNEL_INCLUDES)

libebtc.so: $(OBJECTS2)
	$(CC) -shared $(LDFLAGS) -Wl,-soname,libebtc.so -o libebtc.so -lc $(OBJECTS2)

ebtables: $(OBJECTS) ebtables-standalone.o libebtc.so
	$(CC) $(CFLAGS) $(CFLAGS_SH_LIB) $(LDFLAGS) -o $@ ebtables-standalone.o -I$(KERNEL_INCLUDES) -L. -Lextensions -lebtc $(EXT_LIBSI) \
	-Wl,-rpath,$(LIBDIR)

ebtablesu: ebtablesu.c
	$(CC) $(CFLAGS) $(PROGSPECSD) $< -o $@

ebtablesd.o: ebtablesd.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(PROGSPECSD) -c $< -o $@  -I$(KERNEL_INCLUDES)

ebtablesd: $(OBJECTS) ebtablesd.o libebtc.so
	$(CC) $(CFLAGS) -o $@ ebtablesd.o -I$(KERNEL_INCLUDES) -L. -Lextensions -lebtc $(EXT_LIBSI) \
	-Wl,-rpath,$(LIBDIR)

ebtables-restore.o: ebtables-restore.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(PROGSPECS) -c $< -o $@  -I$(KERNEL_INCLUDES)

ebtables-restore: $(OBJECTS) ebtables-restore.o libebtc.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ ebtables-restore.o -I$(KERNEL_INCLUDES) -L. -Lextensions -lebtc $(EXT_LIBSI) \
	-Wl,-rpath,$(LIBDIR)

.PHONY: daemon
daemon: ebtablesd ebtablesu

# a little scripting for a static binary, making one for ebtables-restore
# should be completely analogous
static: extensions/ebt_*.c extensions/ebtable_*.c ebtables.c communication.c ebtables-standalone.c getethertype.c libebtc.c useful_functions.c
	cp ebtables-standalone.c ebtables-standalone.c_ ; \
	cp include/ebtables_u.h include/ebtables_u.h_ ; \
	sed "s/ main(/ pseudomain(/" ebtables-standalone.c > ebtables-standalone.c__ ; \
	mv ebtables-standalone.c__ ebtables-standalone.c ; \
	printf "\nint main(int argc, char *argv[])\n{\n "  >> ebtables-standalone.c ; \
	for arg in $(EXT_FUNC) \
	; do \
	sed s/_init/_$${arg}_init/ extensions/ebt_$${arg}.c > extensions/ebt_$${arg}.c_ ; \
	mv extensions/ebt_$${arg}.c_ extensions/ebt_$${arg}.c ; \
	printf "\t%s();\n" _$${arg}_init >> ebtables-standalone.c ; \
	printf "extern void %s();\n" _$${arg}_init >> include/ebtables_u.h ; \
	done ; \
	for arg in $(EXT_TABLES) \
	; do \
	sed s/_init/_t_$${arg}_init/ extensions/ebtable_$${arg}.c > extensions/ebtable_$${arg}.c_ ; \
	mv extensions/ebtable_$${arg}.c_ extensions/ebtable_$${arg}.c ; \
	printf "\t%s();\n" _t_$${arg}_init >> ebtables-standalone.c ; \
	printf "extern void %s();\n" _t_$${arg}_init >> include/ebtables_u.h ; \
	done ; \
	printf "\n\tpseudomain(argc, argv);\n\treturn 0;\n}\n" >> ebtables-standalone.c ;\
	$(CC) $(CFLAGS) $(LDFLAGS) $(PROGSPECS) -o $@ $^ -I$(KERNEL_INCLUDES) -Iinclude ; \
	for arg in $(EXT_FUNC) \
	; do \
	sed "s/ .*_init/ _init/" extensions/ebt_$${arg}.c > extensions/ebt_$${arg}.c_ ; \
	mv extensions/ebt_$${arg}.c_ extensions/ebt_$${arg}.c ; \
	done ; \
	for arg in $(EXT_TABLES) \
	; do \
	sed "s/ .*_init/ _init/" extensions/ebtable_$${arg}.c > extensions/ebtable_$${arg}.c_ ; \
	mv extensions/ebtable_$${arg}.c_ extensions/ebtable_$${arg}.c ; \
	done ; \
	mv ebtables-standalone.c_ ebtables-standalone.c ; \
	mv include/ebtables_u.h_ include/ebtables_u.h

tmp1:=$(shell printf $(BINDIR) | sed 's/\//\\\//g')
tmp2:=$(shell printf $(SYSCONFIGDIR) | sed 's/\//\\\//g')
tmp3:=$(shell printf $(PIPE) | sed 's/\//\\\//g')
.PHONY: scripts
scripts: ebtables-save ebtables.sysv ebtables-config
	cat ebtables-save | sed 's/__EXEC_PATH__/$(tmp1)/g' > ebtables-save_
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 -o root -g root ebtables-save_ $(DESTDIR)$(BINDIR)/ebtables-save
	cat ebtables.sysv | sed 's/__EXEC_PATH__/$(tmp1)/g' | sed 's/__SYSCONFIG__/$(tmp2)/g' > ebtables.sysv_
	if [ "$(DESTDIR)" != "" ]; then mkdir -p $(DESTDIR)$(INITDIR); fi
	if test -d $(DESTDIR)$(INITDIR); then install -m 0755 -o root -g root ebtables.sysv_ $(DESTDIR)$(INITDIR)/ebtables; fi
	cat ebtables-config | sed 's/__SYSCONFIG__/$(tmp2)/g' > ebtables-config_
	if [ "$(DESTDIR)" != "" ]; then mkdir -p $(DESTDIR)$(SYSCONFIGDIR); fi
	if test -d $(DESTDIR)$(SYSCONFIGDIR); then install -m 0600 -o root -g root ebtables-config_ $(DESTDIR)$(SYSCONFIGDIR)/ebtables-config; fi
	rm -f ebtables-save_ ebtables.sysv_ ebtables-config_

tmp4:=$(shell printf $(LOCKFILE) | sed 's/\//\\\//g')
$(MANDIR)/man8/ebtables.8: ebtables.8
	mkdir -p $(DESTDIR)$(@D)
	sed -e 's/$$(VERSION)/$(PROGVERSION)/' -e 's/$$(DATE)/$(PROGDATE)/' -e 's/$$(LOCKFILE)/$(tmp4)/' ebtables.8 > ebtables.8_
	install -m 0644 -o root -g root ebtables.8_ $(DESTDIR)$@
	rm -f ebtables.8_

$(DESTDIR)$(ETHERTYPESFILE): ethertypes
	mkdir -p $(@D)
	install -m 0644 -o root -g root $< $@

.PHONY: exec
exec: ebtables ebtables-restore
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 -o root -g root $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)
	install -m 0755 -o root -g root ebtables-restore $(DESTDIR)$(BINDIR)/ebtables-restore

.PHONY: install
install: $(MANDIR)/man8/ebtables.8 $(DESTDIR)$(ETHERTYPESFILE) exec scripts
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 0755 extensions/*.so $(DESTDIR)$(LIBDIR)
	install -m 0755 *.so $(DESTDIR)$(LIBDIR)

.PHONY: clean
clean:
	rm -f ebtables ebtables-restore ebtablesd ebtablesu static
	rm -f *.o *~ *.so
	rm -f extensions/*.o extensions/*.c~ extensions/*.so include/*~

DIR:=$(PROGNAME)-v$(PROGVERSION)
CVSDIRS:=CVS extensions/CVS examples/CVS examples/perf_test/CVS \
examples/ulog/CVS include/CVS
# This is used to make a new userspace release, some files are altered so
# do this on a temporary version
.PHONY: release
release:
	rm -f extensions/ebt_inat.c
	rm -rf $(CVSDIRS)
	mkdir -p include/linux/netfilter_bridge
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/netfilter_bridge.h include/linux/
# To keep possible compile error complaints about undefined ETH_P_8021Q
# off my back
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/if_ether.h include/linux/
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/types.h include/linux/
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/netfilter_bridge/*.h \
		include/linux/netfilter_bridge/
	install -m 0644 -o root -g root \
		include/ebtables.h include/linux/netfilter_bridge/
	make clean
	touch *
	touch extensions/*
	touch include/*
	touch include/linux/*
	touch include/linux/netfilter_bridge/*
	sed -i -e 's/$$(VERSION)/$(PROGVERSION)/' -e 's/$$(DATE)/$(PROGDATE)/' -e 's/$$(LOCKFILE)/$(tmp4)/' ebtables.8
	sed -i -e 's/$$(VERSION)/$(PROGVERSION_)/' -e 's/$$(RELEASE)/$(PROGRELEASE)/' ebtables.spec
	cd ..;tar -c $(DIR) | gzip >$(DIR).tar.gz; cd -
	rm -rf include/linux

# This will make the rpm and put it in /usr/src/redhat/RPMS
# (do this as root after make release)
.PHONY: rpmbuild
rpmbuild:
	cp ../$(DIR).tar.gz /usr/src/redhat/SOURCES/
	rpmbuild --buildroot $(shell mktemp -td $(DIR)-XXXXX) -bb ebtables.spec

.PHONY: test_ulog
test_ulog: examples/ulog/test_ulog.c getethertype.o
	$(CC) $(CFLAGS)  $< -o test_ulog -I$(KERNEL_INCLUDES) -lc \
	getethertype.o
	mv test_ulog examples/ulog/

.PHONY: examples
examples: test_ulog
