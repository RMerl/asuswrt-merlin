TCOBJ= tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o \
       m_police.o m_estimator.o m_action.o m_ematch.o \
       emp_ematch.yacc.o emp_ematch.lex.o

include ../Config

TCMODULES :=
TCMODULES += q_fifo.o
TCMODULES += q_sfq.o
TCMODULES += q_esfq.o
#	TCMODULES += q_red.o
TCMODULES += q_prio.o
TCMODULES += q_tbf.o
TCMODULES += q_cbq.o
#	TCMODULES += f_rsvp.o
TCMODULES += f_u32.o
TCMODULES += f_route.o
TCMODULES += f_fw.o
TCMODULES += f_basic.o
#	TCMODULES += q_dsmark.o
#	TCMODULES += q_gred.o
#	TCMODULES += f_tcindex.o
TCMODULES += q_ingress.o
TCMODULES += q_hfsc.o
TCMODULES += q_htb.o
TCMODULES += m_gact.o
TCMODULES += m_mirred.o
TCMODULES += m_ipt.o
TCMODULES += m_pedit.o
TCMODULES += p_ip.o
TCMODULES += p_icmp.o
TCMODULES += p_tcp.o
TCMODULES += p_udp.o
TCMODULES += em_nbyte.o
TCMODULES += em_cmp.o
TCMODULES += em_u32.o
TCMODULES += em_meta.o

TCOBJ += $(TCMODULES)

TCLIB := tc_core.o
TCLIB += tc_red.o
TCLIB += tc_cbq.o
TCLIB += tc_estimator.o

CFLAGS += -DCONFIG_GACT -DCONFIG_GACT_PROB

TCSO :=
TCSO += q_netem.so
ifeq ($(TC_CONFIG_ATM),y)
  TCSO += q_atm.so
endif

LDLIBS += -L. -ltc -lm -ldl

LDFLAGS += -Wl,-export-dynamic

YACC := bison
LEX := flex

%.so: %.c
	$(CC) $(CFLAGS) -shared -fpic $< -o $@


all: libtc.a tc $(TCSO)

tc: $(TCOBJ) $(LIBNETLINK) $(LIBUTIL) $(TCLIB)

libtc.a: $(TCLIB)
	$(AR) rcs $@ $(TCLIB)

install: all
	mkdir -p $(DESTDIR)/usr/lib/tc
	install -m 0755 -s tc $(DESTDIR)$(SBINDIR)
	for i in $(TCSO); \
	do install -m 755 -s $$i $(DESTDIR)/usr/lib/tc; \
	done

clean:
	rm -f $(TCOBJ) $(TCLIB) libtc.a tc *.so emp_ematch.yacc.h; \
	rm -f emp_ematch.yacc.output

q_atm.so: q_atm.c
	$(CC) $(CFLAGS) -shared -fpic -o q_atm.so q_atm.c -latm

%.yacc.c: %.y
	$(YACC) $(YACCFLAGS) -o $@ $<

%.lex.c: %.l
	$(LEX) $(LEXFLAGS) -o$@ $<
