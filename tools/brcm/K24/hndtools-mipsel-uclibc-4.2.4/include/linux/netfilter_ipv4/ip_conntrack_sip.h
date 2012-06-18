#ifndef _IP_CONNTRACK_SIP_H
#define _IP_CONNTRACK_SIP_H
/* SIP tracking. */

#ifdef __KERNEL__

#include <linux/netfilter_ipv4/lockhelp.h>

/* Protects sip part of conntracks */
DECLARE_LOCK_EXTERN(ip_sip_lock);

#define SIP_PORT	5060	/* UDP */
#define SIP_EXPIRES 	3600	/* seconds */
#define RTP_TIMEOUT	180	/* seconds */

#endif /* __KERNEL__ */

/* SIP Request */
#define SIP_INVITE		0x01
#define SIP_ACK			0x02
#define SIP_BYE			0x04
/* SIP Response */
#define SIP_100			0x10
#define SIP_200			0x20
#define SIP_200_BYE		0x40
/* SIP session direction */
#define SIP_OUTGOING		0
#define SIP_INCOMING		1

enum ip_ct_conntype
{
	CONN_SIP,
	CONN_RTP,
	CONN_RTCP,
};

/* This structure is per expected connection */
struct ip_ct_sip_expect
{
	u_int16_t port; 		/* TCP port that was to be used */

	enum ip_ct_conntype type;
	int nated;
};

/* This structure exists only once per master */
struct ip_ct_sip_master {
	int mangled;
	u_int16_t rtpport;
};

extern u_int16_t find_sdp_audio_port(const char *data, size_t dlen,
		unsigned int *numoff, unsigned int *numlen);
extern int find_sdp_rtp_addr(const char *data, size_t dlen,
			unsigned int *numoff, unsigned int *numlen, u_int32_t *addr);
#endif /* _IP_CONNTRACK_SIP_H */
