/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: fuzzel $
 $Id: mld.h,v 1.4 2004/10/07 09:28:21 fuzzel Exp $
 $Date: 2004/10/07 09:28:21 $
**************************************/

/* Wrappers so we don't have to change the copied stuff ;) */
#define __u8 uint8_t
#define __u16 uint16_t

/* Determine Endianness */
#if BYTE_ORDER == LITTLE_ENDIAN
	/* 1234 machines */
	#define __LITTLE_ENDIAN_BITFIELD 1
#elif BYTE_ORDER == BIG_ENDIAN
	/* 4321 machines */
	#define __BIG_ENDIAN_BITFIELD 1
# define WORDS_BIGENDIAN 1
#elif BYTE_ORDER == PDP_ENDIAN
	/* 3412 machines */
#error PDP endianness not supported yet!
#else
#error unknown endianness!
#endif

/* Per RFC */
struct mld1 {
        __u8		type;
        __u8		code;
        __u16		csum;
        __u16		mrc;
        __u16		resv1;
        struct in6_addr	mca;
};

/* MLDv2 Report */
#ifndef ICMP6_V2_MEMBERSHIP_REPORT
#define ICMP6_V2_MEMBERSHIP_REPORT	143
#endif
/* MLDv2 Report - Experimental Code */
#ifndef ICMP6_V2_MEMBERSHIP_REPORT_EXP
#define ICMP6_V2_MEMBERSHIP_REPORT_EXP	206
#endif

/* MLDv2 Exclude/Include */

#ifndef MLD2_MODE_IS_INCLUDE
#define MLD2_MODE_IS_INCLUDE		1
#endif
#ifndef MLD2_MODE_IS_EXCLUDE
#define MLD2_MODE_IS_EXCLUDE		2
#endif
#ifndef MLD2_CHANGE_TO_INCLUDE
#define MLD2_CHANGE_TO_INCLUDE		3
#endif
#ifndef MLD2_CHANGE_TO_EXCLUDE
#define MLD2_CHANGE_TO_EXCLUDE		4
#endif
#ifndef MLD2_ALLOW_NEW_SOURCES
#define MLD2_ALLOW_NEW_SOURCES		5
#endif
#ifndef MLD2_BLOCK_OLD_SOURCES
#define MLD2_BLOCK_OLD_SOURCES		6
#endif

#ifndef MLD2_ALL_MCR_INIT
#define MLD2_ALL_MCR_INIT { { { 0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x16 } } }
#endif

#ifndef ICMP6_ROUTER_RENUMBERING
#define ICMP6_ROUTER_RENUMBERING	138	/* router renumbering */
#endif
#ifndef ICMP6_NI_QUERY
#define ICMP6_NI_QUERY			139	/* node information request */
#endif
#ifndef ICMP6_NI_REPLY
#define ICMP6_NI_REPLY			140	/* node information reply */
#endif
#ifndef MLD_MTRACE_RESP
#define	MLD_MTRACE_RESP			200	/* Mtrace response (to sender) */
#endif
#ifndef MLD_MTRACE
#define	MLD_MTRACE			201	/* Mtrace messages */
#endif

#ifndef ICMP6_DST_UNREACH_BEYONDSCOPE
#define ICMP6_DST_UNREACH_BEYONDSCOPE	2	/* Beyond scope of source address */
#endif

#ifndef ICMP6_NI_SUCCESS
#define ICMP6_NI_SUCCESS	0		/* node information successful reply */
#endif
#ifndef ICMP6_NI_REFUSED
#define ICMP6_NI_REFUSED	1		/* node information request is refused */
#endif
#ifndef  ICMP6_NI_UNKNOWN
#define ICMP6_NI_UNKNOWN	2		/* unknown Qtype */
#endif

#ifndef ICMP6_ROUTER_RENUMBERING_COMMAND
#define ICMP6_ROUTER_RENUMBERING_COMMAND	0	/* rr command */
#endif
#ifndef ICMP6_ROUTER_RENUMBERING_RESULT
#define ICMP6_ROUTER_RENUMBERING_RESULT		1	/* rr result */
#endif
#ifndef ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET
#define ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET	255	/* rr seq num reset */
#endif


/* From linux/net/ipv6/mcast.c */

/*
 *  These header formats should be in a separate include file, but icmpv6.h
 *  doesn't have in6_addr defined in all cases, there is no __u128, and no
 *  other files reference these.
 *
 *                      +-DLS 4/14/03
 *
 * Multicast Listener Discovery version 2 headers
 * Modified as they are where not ANSI C compliant
 */
struct mld2_grec {
	__u8			grec_type;
	__u8			grec_auxwords;
	__u16			grec_nsrcs;
	struct in6_addr		grec_mca;
/*	struct in6_addr		grec_src[0]; */
};

struct mld2_report {
	__u8			type;
	__u8			resv1;
	__u16			csum;
	__u16			resv2;
	__u16			ngrec;
/*	struct mld2_grec	grec[0]; */
};

struct mld2_query {
	__u8			type;
	__u8			code;
	__u16			csum;
	__u16			mrc;
	__u16			resv1;
	struct in6_addr		mca;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint32_t		qrv:3,
				suppress:1,
				resv2:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint32_t		resv2:4,
				suppress:1,
				qrv:3;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	__u8			qqic;
	__u16			nsrcs;
/*	struct in6_addr		srcs[0]; */
};

#define IGMP6_UNSOLICITED_IVAL	(10*HZ)
#define MLD_QRV_DEFAULT		2

#define MLD_V1_SEEN(idev) ((idev)->mc_v1_seen && \
	time_before(jiffies, (idev)->mc_v1_seen))

#define MLDV2_MASK(value, nb) ((nb)>=32 ? (value) : ((1<<(nb))-1) & (value))
#define MLDV2_EXP(thresh, nbmant, nbexp, value) \
	((value) < (thresh) ? (value) : \
	((MLDV2_MASK(value, nbmant) | (1<<(nbmant+nbexp))) << \
	(MLDV2_MASK((value) >> (nbmant), nbexp) + (nbexp))))

#define MLDV2_QQIC(value) MLDV2_EXP(0x80, 4, 3, value)
#define MLDV2_MRC(value) MLDV2_EXP(0x8000, 12, 3, value)

