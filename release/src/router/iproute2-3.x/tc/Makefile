TCOBJ= tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o \
       tc_monitor.o m_police.o m_estimator.o m_action.o \
       m_ematch.o emp_ematch.yacc.o emp_ematch.lex.o

include ../Config
SHARED_LIBS ?= y

TCMODULES :=
TCMODULES += q_fifo.o
TCMODULES += q_sfq.o
TCMODULES += q_esfq.o
#TCMODULES += q_red.o
TCMODULES += q_prio.o
TCMODULES += q_tbf.o
TCMODULES += q_cbq.o
TCMODULES += q_rr.o
TCMODULES += q_multiq.o
TCMODULES += q_netem.o
TCMODULES += q_choke.o
TCMODULES += q_sfb.o
#TCMODULES += f_rsvp.o
TCMODULES += f_u32.o
TCMODULES += f_route.o
TCMODULES += f_fw.o
TCMODULES += f_basic.o
TCMODULES += f_flow.o
TCMODULES += f_cgroup.o
#TCMODULES += q_dsmark.o
#TCMODULES += q_gred.o
#TCMODULES += f_tcindex.o
TCMODULES += q_ingress.o
TCMODULES += q_hfsc.o
TCMODULES += q_htb.o
TCMODULES += q_drr.o
TCMODULES += q_qfq.o
TCMODULES += m_gact.o
TCMODULES += m_mirred.o
TCMODULES += m_nat.o
TCMODULES += m_pedit.o
TCMODULES += m_skbedit.o
TCMODULES += m_csum.o
TCMODULES += p_ip.o
TCMODULES += p_icmp.o
TCMODULES += p_tcp.o
TCMODULES += p_udp.o
TCMODULES += em_nbyte.o
TCMODULES += em_cmp.o
TCMODULES += em_u32.o
TCMODULES += em_meta.o
TCMODULES += q_mqprio.o

TCSO :=
ifeq ($(TC_CONFIG_ATM),y)
  TCSO += q_atm.so
endif

ifeq ($(TC_CONFIG_XT),y)
  TCSO += m_xt.so
else
  ifeq ($(TC_CONFIG_XT_OLD),y)
    TCSO += m_xt_old.so
  else
    ifeq ($(TC_CONFIG_XT_OLD_H),y)
	CFLAGS += -DTC_CONFIG_XT_H
	TCSO += m_xt_old.so
    else
      TCMODULES += m_ipt.o
    endif
  endif
endif

TCOBJ += $(TCMODULES)
LDLIBS += -L. -ltc -lm

ifeq ($(SHARED_LIBS),y)
LDLIBS += -ldl
LDFLAGS += -Wl,-export-dynamic
endif

TCLIB := tc_core.o
TCLIB += tc_red.o
TCLIB += tc_cbq.o
TCLIB += tc_estimator.o
TCLIB += tc_stab.o

CFLAGS += -DCONFIG_GACT -DCONFIG_GACT_PROB
ifneq ($(IPT_LIB_DIR),)
	CFLAGS += -DIPT_LIB_DIR=\"$(IPT_LIB_DIR)\" -DXT_LIB_DIR=\"$(IPT_LIB_DIR)\"
endif

YACC := bison
LEX := flex

MODDESTDIR := $(DESTDIR)$(patsubst /usr%,%,$(LIBDIR))/tc

%.so: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fpic $< -o $@


all: libtc.a tc $(TCSO)

tc: $(TCOBJ) $(LIBNETLINK) $(LIBUTIL) $(TCLIB)

libtc.a: $(TCLIB)
	$(AR) rcs $@ $(TCLIB)

install: all
	mkdir -p $(MODDESTDIR)
	install -m 0755 tc $(DESTDIR)$(SBINDIR)
	for i in $(TCSO); \
	do install -m 755 $$i $(MODDESTDIR); \
	done
	if [ ! -f $(MODDESTDIR)/m_ipt.so ]; then \
	if [ -f $(MODDESTDIR)/m_xt.so ]; \
		then ln -s m_xt.so $(MODDESTDIR)/m_ipt.so ; \
	elif [ -f $(MODDESTDIR)/m_xt_old.so ]; \
		then ln -s m_xt_old.so $(MODDESTDIR)/m_ipt.so ; \
	fi; \
	fi

clean:
	rm -f $(TCOBJ) $(TCLIB) libtc.a tc *.so emp_ematch.yacc.h; \
	rm -f emp_ematch.yacc.output

q_atm.so: q_atm.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fpic -o q_atm.so q_atm.c -latm

m_xt.so: m_xt.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(IPTL) -shared -fpic -o m_xt.so m_xt.c -lxtables

m_xt_old.so: m_xt_old.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(IPTL) -shared -fpic -o m_xt_old.so m_xt_old.c -lxtables

%.yacc.c: %.y
	$(YACC) $(YACCFLAGS) -o $@ $<

%.lex.c: %.l
	$(LEX) $(LEXFLAGS) -o$@ $<

ifneq ($(SHARED_LIBS),y)

tc: static-syms.o
static-syms.o: static-syms.h
static-syms.h: $(wildcard *.c)
	files="$^" ; \
	for s in `grep -B 3 '\<dlsym' $$files | sed -n '/snprintf/{s:.*"\([^"]*\)".*:\1:;s:%s::;p}'` ; do \
		sed -n '/'$$s'[^ ]* =/{s:.* \([^ ]*'$$s'[^ ]*\) .*:extern char \1[] __attribute__((weak)); if (!strcmp(sym, "\1")) return \1;:;p}' $$files ; \
	done > $@

endif
