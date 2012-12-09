#! /usr/bin/make

# WARNING:
# only add extensions here that are either present in the kernel, or whose
# header files are present in the include/linux directory of this iptables
# package (HW)
#
#PF_EXT_SLIB:=ah addrtype comment connlimit connmark conntrack ecn esp hashlimit helper icmp iprange length limit mac mark multiport owner physdev pkttype policy realm rpc sctp standard state tcp tcpmss tos ttl udp unclean CLASSIFY CONNMARK DNAT DSCP ECN LOG MARK MASQUERADE MIRROR NETMAP NFQUEUE NOTRACK REDIRECT REJECT SAME SNAT TARPIT TCPMSS TOS TRACE TTL ULOG
#PF6_EXT_SLIB:=connmark eui64 hl icmp6 length limit mac mark multiport owner physdev policy standard state tcp udp CONNMARK HL LOG NFQUEUE MARK TRACE


PF_EXT_SLIB:=CLASSIFY CONNMARK DNAT DSCP LOG MARK MASQUERADE REDIRECT REJECT
PF_EXT_SLIB+=ROUTE SNAT TCPMSS TOS TRIGGER TTL NETMAP
PF_EXT_SLIB+=condition connlimit connmark geoip icmp iprange 
PF_EXT_SLIB+=length limit mac mark multiport recent standard state quota
PF_EXT_SLIB+=tcp tcpmss time tos u32 udp web dscp connbytes webstr string
PF_EXT_SLIB+=IMQ ipp2p
PF_EXT_SLIB+=conntrack
PF_EXT_SLIB+=layer7
ifeq ($(CONFIG_LINUX26),y)
PF_EXT_SLIB+=hashlimit NFQUEUE
endif
PF_EXT_SLIB+=account

ifeq ($(DO_IPV6), 1)
PF6_EXT_SLIB:=connmark icmp6 length limit mac mark multiport standard state rt hl
PF6_EXT_SLIB+=tcp udp CONNMARK LOG MARK IMQ TCPMSS
ifeq ($(CONFIG_LINUX26),y)
PF6_EXT_SLIB+=connlimit hashlimit recent NFQUEUE ROUTE HL connbytes
endif
endif

ifeq ($(DO_SELINUX), 1)
PF_EXT_SE_SLIB:=SECMARK CONNSECMARK
PF6_EXT_SE_SLIB:=SECMARK CONNSECMARK
endif

# Optionals
#PF_EXT_SLIB_OPTS:=$(foreach T,$(wildcard extensions/.*-test),$(shell KERNEL_DIR=$(KERNEL_DIR) $(T)))
#PF6_EXT_SLIB_OPTS:=$(foreach T,$(wildcard extensions/.*-test6),$(shell KERNEL_DIR=$(KERNEL_DIR) $(T)))

PF_EXT_ALL_SLIB:=$(patsubst extensions/libipt_%.c, %, $(wildcard extensions/libipt_*.c))
PF6_EXT_ALL_SLIB:=$(patsubst extensions/libip6t_%.c, %, $(wildcard extensions/libip6t_*.c))

PF_EXT_MAN_ALL_MATCHES:=$(foreach T,$(PF_EXT_ALL_SLIB),$(shell test -f extensions/libipt_$(T).man && grep -q register_match extensions/libipt_$(T).c  && echo $(T)))
PF_EXT_MAN_ALL_TARGETS:=$(foreach T,$(PF_EXT_ALL_SLIB),$(shell test -f extensions/libipt_$(T).man && grep -q register_target extensions/libipt_$(T).c && echo $(T)))
PF6_EXT_MAN_ALL_MATCHES:=$(foreach T,$(PF6_EXT_ALL_SLIB),$(shell test -f extensions/libip6t_$(T).man && grep -q register_match6 extensions/libip6t_$(T).c  && echo $(T)))
PF6_EXT_MAN_ALL_TARGETS:=$(foreach T,$(PF6_EXT_ALL_SLIB),$(shell test -f extensions/libip6t_$(T).man && grep -q register_target6 extensions/libip6t_$(T).c && echo $(T)))

PF_EXT_MAN_MATCHES:=$(filter $(PF_EXT_ALL_SLIB), $(PF_EXT_MAN_ALL_MATCHES))
PF_EXT_MAN_TARGETS:=$(filter $(PF_EXT_ALL_SLIB), $(PF_EXT_MAN_ALL_TARGETS))
PF_EXT_MAN_EXTRA_MATCHES:=$(filter-out $(PF_EXT_MAN_MATCHES), $(PF_EXT_MAN_ALL_MATCHES))
PF_EXT_MAN_EXTRA_TARGETS:=$(filter-out $(PF_EXT_MAN_TARGETS), $(PF_EXT_MAN_ALL_TARGETS))
PF6_EXT_MAN_MATCHES:=$(filter $(PF6_EXT_ALL_SLIB), $(PF6_EXT_MAN_ALL_MATCHES))
PF6_EXT_MAN_TARGETS:=$(filter $(PF6_EXT_ALL_SLIB), $(PF6_EXT_MAN_ALL_TARGETS))
PF6_EXT_MAN_EXTRA_MATCHES:=$(filter-out $(PF6_EXT_MAN_MATCHES), $(PF6_EXT_MAN_ALL_MATCHES))
PF6_EXT_MAN_EXTRA_TARGETS:=$(filter-out $(PF6_EXT_MAN_TARGETS), $(PF6_EXT_MAN_ALL_TARGETS))


allman:
	@echo ALL_SLIB: $(PF_EXT_ALL_SLIB)
	@echo ALL_MATCH: $(PF_EXT_MAN_ALL_MATCHES)
	@echo ALL_TARGET: $(PF_EXT_MAN_ALL_TARGETS)

PF_EXT_SLIB+=$(PF_EXT_SLIB_OPTS)
PF6_EXT_SLIB+=$(PF6_EXT_SLIB_OPTS)

OPTIONALS+=$(patsubst %,IPv4:%,$(PF_EXT_SLIB_OPTS))

ifndef NO_SHARED_LIBS
SHARED_LIBS+=$(foreach T,$(PF_EXT_SLIB),extensions/libipt_$(T).so)
SHARED_SE_LIBS+=$(foreach T,$(PF_EXT_SE_SLIB),extensions/libipt_$(T).so)
EXTRA_INSTALLS+=$(foreach T, $(PF_EXT_SLIB), $(DESTDIR)$(LIBDIR)/iptables/libipt_$(T).so)
EXTRA_INSTALLS+=$(foreach T, $(PF_EXT_SE_SLIB), $(DESTDIR)$(LIBDIR)/iptables/libipt_$(T).so)

ifeq ($(DO_IPV6), 1)
OPTIONALS+=$(patsubst %,IPv6:%,$(PF6_EXT_SLIB_OPTS))
SHARED_LIBS+=$(foreach T,$(PF6_EXT_SLIB),extensions/libip6t_$(T).so)
SHARED_SE_LIBS+=$(foreach T,$(PF6_EXT_SE_SLIB),extensions/libip6t_$(T).so)
EXTRA_INSTALLS+=$(foreach T, $(PF6_EXT_SLIB), $(DESTDIR)$(LIBDIR)/iptables/libip6t_$(T).so)
EXTRA_INSTALLS+=$(foreach T, $(PF6_EXT_SE_SLIB), $(DESTDIR)$(LIBDIR)/iptables/libip6t_$(T).so)
endif
else 	# NO_SHARED_LIBS
EXT_OBJS+=$(foreach T,$(PF_EXT_SLIB),extensions/libipt_$(T).o)
EXT_OBJS+=$(foreach T,$(PF_EXT_SE_SLIB),extensions/libipt_$(T).o)
EXT_FUNC+=$(foreach T,$(PF_EXT_SLIB),ipt_$(T))
EXT_FUNC+=$(foreach T,$(PF_EXT_SE_SLIB),ipt_$(T))
EXT_OBJS+= extensions/initext.o
ifeq ($(DO_IPV6), 1)
EXT6_OBJS+=$(foreach T,$(PF6_EXT_SLIB),extensions/libip6t_$(T).o)
EXT6_OBJS+=$(foreach T,$(PF6_EXT_SE_SLIB),extensions/libip6t_$(T).o)
EXT6_FUNC+=$(foreach T,$(PF6_EXT_SLIB),ip6t_$(T))
EXT6_FUNC+=$(foreach T,$(PF6_EXT_SE_SLIB),ip6t_$(T))
EXT6_OBJS+= extensions/initext6.o
endif	# DO_IPV6
endif	# NO_SHARED_LIBS

ifndef TOPLEVEL_INCLUDED
local:
	cd .. && $(MAKE) $(SHARED_LIBS) $(SHARED_SE_LIBS)
endif

ifdef NO_SHARED_LIBS
extensions/libext.a: $(EXT_OBJS)
	rm -f $@; ar crv $@ $(EXT_OBJS)

extensions/libext6.a: $(EXT6_OBJS)
	rm -f $@; ar crv $@ $(EXT6_OBJS)

extensions/initext.o: extensions/initext.c
extensions/initext6.o: extensions/initext6.c

extensions/initext.c: extensions/Makefile
	echo "" > $@
	for i in $(EXT_FUNC); do \
		echo "extern void $${i}_init(void);" >> $@; \
	done
	echo "void init_extensions(void) {" >> $@
	for i in $(EXT_FUNC); do \
		echo "	$${i}_init();" >> $@; \
	done
	echo "}" >> $@

extensions/initext6.c: extensions/Makefile
	echo "" > $@
	for i in $(EXT6_FUNC); do \
		echo "extern void $${i}_init(void);" >> $@; \
	done
	echo "void init_extensions(void) {" >> $@
	for i in $(EXT6_FUNC); do \
		echo "	$${i}_init();" >> $@; \
	done
	echo "}" >> $@

extensions/lib%.o: extensions/lib%.c
	$(CC) $(CFLAGS) -D_INIT=$*_init -c -o $@ $<

endif
 
EXTRAS += extensions/libipt_targets.man
extensions/libipt_targets.man: $(patsubst %,extensions/libipt_%.man,$(PF_EXT_MAN_ALL_TARGETS))
	@for ext in $(PF_EXT_MAN_TARGETS); do \
	    echo ".SS $$ext" ;\
	    cat extensions/libipt_$$ext.man ;\
	done >extensions/libipt_targets.man
	@if [ -n "$(PF_EXT_MAN_EXTRA_TARGETS)" ]; then \
	    extra=$(PF_EXT_MAN_EXTRA_TARGETS) ;\
	    for ext in $${extra:-""}; do \
		echo ".SS $$ext (not supported, see Patch-O-Matic)" ;\
		cat extensions/libipt_$$ext.man ;\
	    done ;\
       	fi >>extensions/libipt_targets.man

EXTRAS += extensions/libipt_matches.man
extensions/libipt_matches.man: $(patsubst %,extensions/libipt_%.man,$(PF_EXT_MAN_ALL_MATCHES))
	@for ext in $(PF_EXT_MAN_MATCHES); do \
	    echo ".SS $$ext" ;\
	    cat extensions/libipt_$$ext.man ;\
	done >extensions/libipt_matches.man
	@if [ -n "$(PF_EXT_MAN_EXTRA_MATCHES)" ]; then \
	    extra=$(PF_EXT_MAN_EXTRA_MATCHES) ;\
	    for ext in $${extra:-""}; do \
		echo ".SS $$ext (not supported, see Patch-O-Matic)" ;\
		cat extensions/libipt_$$ext.man ;\
	    done ;\
       	fi >>extensions/libipt_matches.man

EXTRAS += extensions/libip6t_targets.man
extensions/libip6t_targets.man: $(patsubst %, extensions/libip6t_%.man, $(PF6_EXT_MAN_ALL_TARGETS))
	@for ext in $(PF6_EXT_MAN_TARGETS); do \
	    echo ".SS $$ext" ;\
	    cat extensions/libip6t_$$ext.man ;\
	done >extensions/libip6t_targets.man
	@if [ -n "$(PF6_EXT_MAN_EXTRA_TARGETS)" ]; then \
	    extra=$(PF6_EXT_MAN_EXTRA_TARGETS) ;\
	    for ext in $${extra:-""}; do \
		echo ".SS $$ext (not supported, see Patch-O-Matic)" ;\
		cat extensions/libip6t_$$ext.man ;\
	    done ;\
       	fi >>extensions/libip6t_targets.man

EXTRAS += extensions/libip6t_matches.man
extensions/libip6t_matches.man: $(patsubst %, extensions/libip6t_%.man, $(PF6_EXT_MAN_ALL_MATCHES))
	@for ext in $(PF6_EXT_MAN_MATCHES); do \
	    echo ".SS $$ext" ;\
	    cat extensions/libip6t_$$ext.man ;\
	done >extensions/libip6t_matches.man
	@if [ -n "$(PF6_EXT_MAN_EXTRA_MATCHES)" ]; then \
	    extra=$(PF6_EXT_MAN_EXTRA_MATCHES) ;\
	    for ext in $${extra:-""}; do \
		echo ".SS $$ext (not supported, see Patch-O-Matic)" ;\
		cat extensions/libip6t_$$ext.man ;\
	    done ;\
       	fi >>extensions/libip6t_matches.man

$(DESTDIR)$(LIBDIR)/iptables/libipt_%.so: extensions/libipt_%.so
	@[ -d $(DESTDIR)$(LIBDIR)/iptables ] || mkdir -p $(DESTDIR)$(LIBDIR)/iptables
	cp $< $@

$(DESTDIR)$(LIBDIR)/iptables/libip6t_%.so: extensions/libip6t_%.so
	@[ -d $(DESTDIR)$(LIBDIR)/iptables ] || mkdir -p $(DESTDIR)$(LIBDIR)/iptables
	cp $< $@
