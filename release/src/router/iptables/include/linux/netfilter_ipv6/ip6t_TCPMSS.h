#ifndef _IP6T_TCPMSS_H
#define _IP6T_TCPMSS_H

struct ip6t_tcpmss_info {
	u_int16_t mss;
};

#define IP6T_TCPMSS_CLAMP_PMTU 0xffff

#endif /*_IP6T_TCPMSS_H*/
