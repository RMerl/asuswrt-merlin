/*
 * 25-Jul-1998 Major changes to allow for ip chain table
 *
 * 3-Jan-2000 Named tables to allow packet selection for different uses.
 */

/*
 * 	Format of an IP firewall descriptor
 *
 * 	src, dst, src_mask, dst_mask are always stored in network byte order.
 * 	flags are stored in host byte order (of course).
 * 	Port numbers are stored in HOST byte order.
 */

#ifndef _IPTABLES_H
#define _IPTABLES_H

#include <linux/netfilter_ipv4.h>

#define IPT_FUNCTION_MAXNAMELEN 30
#define IPT_TABLE_MAXNAMELEN 32

/* Yes, Virginia, you have to zero the padding. */
struct ipt_ip {
	/* Source and destination IP addr */
	struct in_addr src, dst;
	/* Mask for src and dest IP addr */
	struct in_addr smsk, dmsk;
	char iniface[IFNAMSIZ], outiface[IFNAMSIZ];
	unsigned char iniface_mask[IFNAMSIZ], outiface_mask[IFNAMSIZ];

	/* Protocol, 0 = ANY */
	u_int16_t proto;

	/* Flags word */
	u_int8_t flags;
	/* Inverse flags */
	u_int8_t invflags;
};

struct ipt_entry_match
{
	union {
		struct {
			u_int16_t match_size;

			/* Used by userspace */
			char name[IPT_FUNCTION_MAXNAMELEN-1];

			u_int8_t revision;
		} user;
		struct {
			u_int16_t match_size;

			/* Used inside the kernel */
			struct ipt_match *match;
		} kernel;

		/* Total length */
		u_int16_t match_size;
	} u;

	unsigned char data[0];
};

struct ipt_entry_target
{
	union {
		struct {
			u_int16_t target_size;

			/* Used by userspace */
			char name[IPT_FUNCTION_MAXNAMELEN-1];

			u_int8_t revision;
		} user;
		struct {
			u_int16_t target_size;

			/* Used inside the kernel */
			struct ipt_target *target;
		} kernel;

		/* Total length */
		u_int16_t target_size;
	} u;

	unsigned char data[0];
};

struct ipt_standard_target
{
	struct ipt_entry_target target;
	int verdict;
};

struct ipt_counters
{
	u_int64_t pcnt, bcnt;			/* Packet and byte counters */
};

/* Values for "flag" field in struct ipt_ip (general ip structure). */
#define IPT_F_FRAG		0x01	/* Set if rule is a fragment rule */
#define IPT_F_MASK		0x01	/* All possible flag bits mask. */

/* Values for "inv" field in struct ipt_ip. */
#define IPT_INV_VIA_IN		0x01	/* Invert the sense of IN IFACE. */
#define IPT_INV_VIA_OUT		0x02	/* Invert the sense of OUT IFACE */
#define IPT_INV_TOS		0x04	/* Invert the sense of TOS. */
#define IPT_INV_SRCIP		0x08	/* Invert the sense of SRC IP. */
#define IPT_INV_DSTIP		0x10	/* Invert the sense of DST OP. */
#define IPT_INV_FRAG		0x20	/* Invert the sense of FRAG. */
#define IPT_INV_PROTO		0x40	/* Invert the sense of PROTO. */
#define IPT_INV_MASK		0x7F	/* All possible flag bits mask. */

/* This structure defines each of the firewall rules.  Consists of 3
   parts which are 1) general IP header stuff 2) match specific
   stuff 3) the target to perform if the rule matches */
struct ipt_entry
{
	struct ipt_ip ip;

	/* Mark with fields that we care about. */
	unsigned int nfcache;

	/* Size of ipt_entry + matches */
	u_int16_t target_offset;
	/* Size of ipt_entry + matches + target */
	u_int16_t next_offset;

	/* Back pointer */
	unsigned int comefrom;

	/* Packet and byte counters. */
	struct ipt_counters counters;

	/* The matches (if any), then the target. */
	unsigned char elems[0];
};

/*
 * New IP firewall options for [gs]etsockopt at the RAW IP level.
 * Unlike BSD Linux inherits IP options so you don't have to use a raw
 * socket for this. Instead we check rights in the calls. */
#define IPT_BASE_CTL		64	/* base for firewall socket options */

#define IPT_SO_SET_REPLACE	(IPT_BASE_CTL)
#define IPT_SO_SET_ADD_COUNTERS	(IPT_BASE_CTL + 1)
#define IPT_SO_SET_MAX		IPT_SO_SET_ADD_COUNTERS

#define IPT_SO_GET_INFO			(IPT_BASE_CTL)
#define IPT_SO_GET_ENTRIES		(IPT_BASE_CTL + 1)
#define IPT_SO_GET_REVISION_MATCH	(IPT_BASE_CTL + 2)
#define IPT_SO_GET_REVISION_TARGET	(IPT_BASE_CTL + 3)
#define IPT_SO_GET_MAX			IPT_SO_GET_REVISION_TARGET

/* CONTINUE verdict for targets */
#define IPT_CONTINUE 0xFFFFFFFF

/* For standard target */
#define IPT_RETURN (-NF_MAX_VERDICT - 1)

/* TCP matching stuff */
struct ipt_tcp
{
	u_int16_t spts[2];			/* Source port range. */
	u_int16_t dpts[2];			/* Destination port range. */
	u_int8_t option;			/* TCP Option iff non-zero*/
	u_int8_t flg_mask;			/* TCP flags mask byte */
	u_int8_t flg_cmp;			/* TCP flags compare byte */
	u_int8_t invflags;			/* Inverse flags */
};

/* Values for "inv" field in struct ipt_tcp. */
#define IPT_TCP_INV_SRCPT	0x01	/* Invert the sense of source ports. */
#define IPT_TCP_INV_DSTPT	0x02	/* Invert the sense of dest ports. */
#define IPT_TCP_INV_FLAGS	0x04	/* Invert the sense of TCP flags. */
#define IPT_TCP_INV_OPTION	0x08	/* Invert the sense of option test. */
#define IPT_TCP_INV_MASK	0x0F	/* All possible flags. */

/* UDP matching stuff */
struct ipt_udp
{
	u_int16_t spts[2];			/* Source port range. */
	u_int16_t dpts[2];			/* Destination port range. */
	u_int8_t invflags;			/* Inverse flags */
};

/* Values for "invflags" field in struct ipt_udp. */
#define IPT_UDP_INV_SRCPT	0x01	/* Invert the sense of source ports. */
#define IPT_UDP_INV_DSTPT	0x02	/* Invert the sense of dest ports. */
#define IPT_UDP_INV_MASK	0x03	/* All possible flags. */

/* ICMP matching stuff */
struct ipt_icmp
{
	u_int8_t type;				/* type to match */
	u_int8_t code[2];			/* range of code */
	u_int8_t invflags;			/* Inverse flags */
};

/* Values for "inv" field for struct ipt_icmp. */
#define IPT_ICMP_INV	0x01	/* Invert the sense of type/code test */

/* The argument to IPT_SO_GET_INFO */
struct ipt_getinfo
{
	/* Which table: caller fills this in. */
	char name[IPT_TABLE_MAXNAMELEN];

	/* Kernel fills these in. */
	/* Which hook entry points are valid: bitmask */
	unsigned int valid_hooks;

	/* Hook entry points: one per netfilter hook. */
	unsigned int hook_entry[NF_IP_NUMHOOKS];

	/* Underflow points. */
	unsigned int underflow[NF_IP_NUMHOOKS];

	/* Number of entries */
	unsigned int num_entries;

	/* Size of entries. */
	unsigned int size;
};

/* The argument to IPT_SO_SET_REPLACE. */
struct ipt_replace
{
	/* Which table. */
	char name[IPT_TABLE_MAXNAMELEN];

	/* Which hook entry points are valid: bitmask.  You can't
           change this. */
	unsigned int valid_hooks;

	/* Number of entries */
	unsigned int num_entries;

	/* Total size of new entries */
	unsigned int size;

	/* Hook entry points. */
	unsigned int hook_entry[NF_IP_NUMHOOKS];

	/* Underflow points. */
	unsigned int underflow[NF_IP_NUMHOOKS];

	/* Information about old entries: */
	/* Number of counters (must be equal to current number of entries). */
	unsigned int num_counters;

	/* The old entries' counters. */
	struct ipt_counters  *counters;

	/* The entries (hang off end: not really an array). */
	struct ipt_entry entries[0];
};

/* The argument to IPT_SO_ADD_COUNTERS. */
struct ipt_counters_info
{
	/* Which table. */
	char name[IPT_TABLE_MAXNAMELEN];

	unsigned int num_counters;

	/* The counters (actually `number' of these). */
	struct ipt_counters counters[0];
};

/* The argument to IPT_SO_GET_ENTRIES. */
struct ipt_get_entries
{
	/* Which table: user fills this in. */
	char name[IPT_TABLE_MAXNAMELEN];

	/* User fills this in: total entry size. */
	unsigned int size;

	/* The entries. */
	struct ipt_entry entrytable[0];
};

/* The argument to IPT_SO_GET_REVISION_*.  Returns highest revision
 * kernel supports, if >= revision. */
struct ipt_get_revision
{
	char name[IPT_FUNCTION_MAXNAMELEN-1];

	u_int8_t revision;
};

/* Standard return verdict, or do jump. */
#define IPT_STANDARD_TARGET ""
/* Error verdict. */
#define IPT_ERROR_TARGET "ERROR"

/* Helper functions */
static __inline__ struct ipt_entry_target *
ipt_get_target(struct ipt_entry *e)
{
	return (void *)e + e->target_offset;
}

/* fn returns 0 to continue iteration */
#define IPT_MATCH_ITERATE(e, fn, args...)	\
({						\
	unsigned int __i;			\
	int __ret = 0;				\
	struct ipt_entry_match *__match;	\
						\
	for (__i = sizeof(struct ipt_entry);	\
	     __i < (e)->target_offset;		\
	     __i += __match->u.match_size) {	\
		__match = (void *)(e) + __i;	\
						\
		__ret = fn(__match , ## args);	\
		if (__ret != 0)			\
			break;			\
	}					\
	__ret;					\
})

/* fn returns 0 to continue iteration */
#define IPT_ENTRY_ITERATE(entries, size, fn, args...)		\
({								\
	unsigned int __i;					\
	int __ret = 0;						\
	struct ipt_entry *__entry;				\
								\
	for (__i = 0; __i < (size); __i += __entry->next_offset) { \
		__entry = (void *)(entries) + __i;		\
								\
		__ret = fn(__entry , ## args);			\
		if (__ret != 0)					\
			break;					\
	}							\
	__ret;							\
})

/*
 *	Main firewall chains definitions and global var's definitions.
 */
#endif /* _IPTABLES_H */
