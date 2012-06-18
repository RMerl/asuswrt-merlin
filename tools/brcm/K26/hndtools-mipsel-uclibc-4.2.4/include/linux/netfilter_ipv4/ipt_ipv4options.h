#ifndef __ipt_ipv4options_h_included__
#define __ipt_ipv4options_h_included__

#define IPT_IPV4OPTION_MATCH_SSRR		0x01  /* For strict source routing */
#define IPT_IPV4OPTION_MATCH_LSRR		0x02  /* For loose source routing */
#define IPT_IPV4OPTION_DONT_MATCH_SRR		0x04  /* any source routing */
#define IPT_IPV4OPTION_MATCH_RR			0x08  /* For Record route */
#define IPT_IPV4OPTION_DONT_MATCH_RR		0x10
#define IPT_IPV4OPTION_MATCH_TIMESTAMP		0x20  /* For timestamp request */
#define IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP	0x40
#define IPT_IPV4OPTION_MATCH_ROUTER_ALERT	0x80  /* For router-alert */
#define IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT	0x100
#define IPT_IPV4OPTION_MATCH_ANY_OPT		0x200 /* match packet with any option */
#define IPT_IPV4OPTION_DONT_MATCH_ANY_OPT	0x400 /* match packet with no option */

struct ipt_ipv4options_info {
	u_int16_t options;
};


#endif /* __ipt_ipv4options_h_included__ */
