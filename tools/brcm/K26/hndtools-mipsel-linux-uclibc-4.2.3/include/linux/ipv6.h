#ifndef _IPV6_H
#define _IPV6_H

#include <linux/in6.h>
#include <asm/byteorder.h>

/* The latest drafts declared increase in minimal mtu up to 1280. */

#define IPV6_MIN_MTU	1280

/*
 *	Advanced API
 *	source interface/address selection, source routing, etc...
 *	*under construction*
 */


struct in6_pktinfo {
	struct in6_addr	ipi6_addr;
	int		ipi6_ifindex;
};


struct in6_ifreq {
	struct in6_addr	ifr6_addr;
	__u32		ifr6_prefixlen;
	int		ifr6_ifindex; 
};

#define IPV6_SRCRT_STRICT	0x01	/* this hop must be a neighbor	*/
#define IPV6_SRCRT_TYPE_0	0	/* IPv6 type 0 Routing Header	*/
#define IPV6_SRCRT_TYPE_2	2	/* IPv6 type 2 Routing Header	*/

/*
 *	routing header
 */
struct ipv6_rt_hdr {
	__u8		nexthdr;
	__u8		hdrlen;
	__u8		type;
	__u8		segments_left;

	/*
	 *	type specific data
	 *	variable length field
	 */
};


struct ipv6_opt_hdr {
	__u8 		nexthdr;
	__u8 		hdrlen;
	/* 
	 * TLV encoded option data follows.
	 */
};

#define ipv6_destopt_hdr ipv6_opt_hdr
#define ipv6_hopopt_hdr  ipv6_opt_hdr


/*
 *	routing header type 0 (used in cmsghdr struct)
 */

struct rt0_hdr {
	struct ipv6_rt_hdr	rt_hdr;
	__u32			reserved;
	struct in6_addr		addr[0];

#define rt0_type		rt_hdr.type
};

/*
 *	routing header type 2
 */

struct rt2_hdr {
	struct ipv6_rt_hdr	rt_hdr;
	__u32			reserved;
	struct in6_addr		addr;

#define rt2_type		rt_hdr.type
};

/*
 *	home address option in destination options header
 */

struct ipv6_destopt_hao {
	__u8			type;
	__u8			length;
	struct in6_addr		addr;
} __attribute__ ((__packed__));

struct ipv6_auth_hdr {
	__u8  nexthdr;
	__u8  hdrlen;           /* This one is measured in 32 bit units! */
	__be16 reserved;
	__be32 spi;
	__be32 seq_no;           /* Sequence number */
	__u8  auth_data[0];     /* Length variable but >=4. Mind the 64 bit alignment! */
};

struct ipv6_esp_hdr {
	__be32 spi;
	__be32 seq_no;           /* Sequence number */
	__u8  enc_data[0];      /* Length variable but >=8. Mind the 64 bit alignment! */
};

struct ipv6_comp_hdr {
	__u8 nexthdr;
	__u8 flags;
	__be16 cpi;
};

/*
 *	IPv6 fixed header
 *
 *	BEWARE, it is incorrect. The first 4 bits of flow_lbl
 *	are glued to priority now, forming "class".
 */

struct ipv6hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8			priority:4,
				version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8			version:4,
				priority:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8			flow_lbl[3];

	__be16			payload_len;
	__u8			nexthdr;
	__u8			hop_limit;

	struct	in6_addr	saddr;
	struct	in6_addr	daddr;
};

/*
 * This structure contains configuration options per IPv6 link.
 */
struct ipv6_devconf {
	__s32		forwarding;
	__s32		hop_limit;
	__s32		mtu6;
	__s32		accept_ra;
	__s32		accept_redirects;
	__s32		autoconf;
	__s32		dad_transmits;
	__s32		rtr_solicits;
	__s32		rtr_solicit_interval;
	__s32		rtr_solicit_delay;
	__s32		force_mld_version;
#ifdef CONFIG_IPV6_PRIVACY
	__s32		use_tempaddr;
	__s32		temp_valid_lft;
	__s32		temp_prefered_lft;
	__s32		regen_max_retry;
	__s32		max_desync_factor;
#endif
	__s32		max_addresses;
	__s32		accept_ra_defrtr;
	__s32		accept_ra_pinfo;
#ifdef CONFIG_IPV6_ROUTER_PREF
	__s32		accept_ra_rtr_pref;
	__s32		rtr_probe_interval;
#ifdef CONFIG_IPV6_ROUTE_INFO
	__s32		accept_ra_rt_info_max_plen;
#endif
#endif
	__s32		proxy_ndp;
	__s32		accept_source_route;
#ifdef CONFIG_IPV6_OPTIMISTIC_DAD
	__s32		optimistic_dad;
#endif
	void		*sysctl;
};

/* index values for the variables in ipv6_devconf */
enum {
	DEVCONF_FORWARDING = 0,
	DEVCONF_HOPLIMIT,
	DEVCONF_MTU6,
	DEVCONF_ACCEPT_RA,
	DEVCONF_ACCEPT_REDIRECTS,
	DEVCONF_AUTOCONF,
	DEVCONF_DAD_TRANSMITS,
	DEVCONF_RTR_SOLICITS,
	DEVCONF_RTR_SOLICIT_INTERVAL,
	DEVCONF_RTR_SOLICIT_DELAY,
	DEVCONF_USE_TEMPADDR,
	DEVCONF_TEMP_VALID_LFT,
	DEVCONF_TEMP_PREFERED_LFT,
	DEVCONF_REGEN_MAX_RETRY,
	DEVCONF_MAX_DESYNC_FACTOR,
	DEVCONF_MAX_ADDRESSES,
	DEVCONF_FORCE_MLD_VERSION,
	DEVCONF_ACCEPT_RA_DEFRTR,
	DEVCONF_ACCEPT_RA_PINFO,
	DEVCONF_ACCEPT_RA_RTR_PREF,
	DEVCONF_RTR_PROBE_INTERVAL,
	DEVCONF_ACCEPT_RA_RT_INFO_MAX_PLEN,
	DEVCONF_PROXY_NDP,
	DEVCONF_OPTIMISTIC_DAD,
	DEVCONF_ACCEPT_SOURCE_ROUTE,
	DEVCONF_MAX
};


#endif /* _IPV6_H */
