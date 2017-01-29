/*
 * ethtool.c: Linux ethernet device configuration tool.
 *
 * Copyright (C) 1998 David S. Miller (davem@dm.cobaltmicro.com)
 * Portions Copyright 2001 Sun Microsystems
 * Kernel 2.4 update Copyright 2001 Jeff Garzik <jgarzik@mandrakesoft.com>
 * Wake-on-LAN,natsemi,misc support by Tim Hockin <thockin@sun.com>
 * Portions Copyright 2002 Intel
 * Portions Copyright (C) Sun Microsystems 2008
 * do_test support by Eli Kupermann <eli.kupermann@intel.com>
 * ETHTOOL_PHYS_ID support by Chris Leech <christopher.leech@intel.com>
 * e1000 support by Scott Feldman <scott.feldman@intel.com>
 * e100 support by Wen Tao <wen-hwa.tao@intel.com>
 * ixgb support by Nicholas Nunley <Nicholas.d.nunley@intel.com>
 * amd8111e support by Reeja John <reeja.john@amd.com>
 * long arguments by Andi Kleen.
 * SMSC LAN911x support by Steve Glendinning <steve.glendinning@smsc.com>
 * Rx Network Flow Control configuration support <santwona.behera@sun.com>
 * Various features by Ben Hutchings <bhutchings@solarflare.com>;
 *	Copyright 2009, 2010 Solarflare Communications
 * MDI-X set support by Jesse Brandeburg <jesse.brandeburg@intel.com>
 *	Copyright 2012 Intel Corporation
 * vmxnet3 support by Shrikrishna Khare <skhare@vmware.com>
 * Various features by Ben Hutchings <ben@decadent.org.uk>;
 *	Copyright 2008-2010, 2013-2016 Ben Hutchings
 * QSFP+/QSFP28 DOM support by Vidya Sagar Ravipati <vidya@cumulusnetworks.com>
 *
 * TODO:
 *   * show settings for all devices
 */

#include "internal.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/utsname.h>
#include <limits.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/sockios.h>
#include <linux/netlink.h>

#ifndef MAX_ADDR_LEN
#define MAX_ADDR_LEN	32
#endif

#ifndef HAVE_NETIF_MSG
enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};
#endif

#ifndef NETLINK_GENERIC
#define NETLINK_GENERIC	16
#endif

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

static void exit_bad_args(void) __attribute__((noreturn));

static void exit_bad_args(void)
{
	fprintf(stderr,
		"ethtool: bad command line argument(s)\n"
		"For more information run ethtool -h\n");
	exit(1);
}

typedef enum {
	CMDL_NONE,
	CMDL_BOOL,
	CMDL_S32,
	CMDL_U8,
	CMDL_U16,
	CMDL_U32,
	CMDL_U64,
	CMDL_BE16,
	CMDL_IP4,
	CMDL_STR,
	CMDL_FLAG,
	CMDL_MAC,
} cmdline_type_t;

struct cmdline_info {
	const char *name;
	cmdline_type_t type;
	/* Points to int (BOOL), s32, u16, u32 (U32/FLAG/IP4), u64,
	 * char * (STR) or u8[6] (MAC).  For FLAG, the value accumulates
	 * all flags to be set. */
	void *wanted_val;
	void *ioctl_val;
	/* For FLAG, the flag value to be set/cleared */
	u32 flag_val;
	/* For FLAG, points to u32 and accumulates all flags seen.
	 * For anything else, points to int and is set if the option is
	 * seen. */
	void *seen_val;
};

struct flag_info {
	const char *name;
	u32 value;
};

static const struct flag_info flags_msglvl[] = {
	{ "drv",	NETIF_MSG_DRV },
	{ "probe",	NETIF_MSG_PROBE },
	{ "link",	NETIF_MSG_LINK },
	{ "timer",	NETIF_MSG_TIMER },
	{ "ifdown",	NETIF_MSG_IFDOWN },
	{ "ifup",	NETIF_MSG_IFUP },
	{ "rx_err",	NETIF_MSG_RX_ERR },
	{ "tx_err",	NETIF_MSG_TX_ERR },
	{ "tx_queued",	NETIF_MSG_TX_QUEUED },
	{ "intr",	NETIF_MSG_INTR },
	{ "tx_done",	NETIF_MSG_TX_DONE },
	{ "rx_status",	NETIF_MSG_RX_STATUS },
	{ "pktdata",	NETIF_MSG_PKTDATA },
	{ "hw",		NETIF_MSG_HW },
	{ "wol",	NETIF_MSG_WOL },
};

struct off_flag_def {
	const char *short_name;
	const char *long_name;
	const char *kernel_name;
	u32 get_cmd, set_cmd;
	u32 value;
	/* For features exposed through ETHTOOL_GFLAGS, the oldest
	 * kernel version for which we can trust the result.  Where
	 * the flag was added at the same time the kernel started
	 * supporting the feature, this is 0 (to allow for backports).
	 * Where the feature was supported before the flag was added,
	 * it is the version that introduced the flag.
	 */
	u32 min_kernel_ver;
};
static const struct off_flag_def off_flag_def[] = {
	{ "rx",     "rx-checksumming",		    "rx-checksum",
	  ETHTOOL_GRXCSUM, ETHTOOL_SRXCSUM, ETH_FLAG_RXCSUM,	0 },
	{ "tx",     "tx-checksumming",		    "tx-checksum-*",
	  ETHTOOL_GTXCSUM, ETHTOOL_STXCSUM, ETH_FLAG_TXCSUM,	0 },
	{ "sg",     "scatter-gather",		    "tx-scatter-gather*",
	  ETHTOOL_GSG,	   ETHTOOL_SSG,     ETH_FLAG_SG,	0 },
	{ "tso",    "tcp-segmentation-offload",	    "tx-tcp*-segmentation",
	  ETHTOOL_GTSO,	   ETHTOOL_STSO,    ETH_FLAG_TSO,	0 },
	{ "ufo",    "udp-fragmentation-offload",    "tx-udp-fragmentation",
	  ETHTOOL_GUFO,	   ETHTOOL_SUFO,    ETH_FLAG_UFO,	0 },
	{ "gso",    "generic-segmentation-offload", "tx-generic-segmentation",
	  ETHTOOL_GGSO,	   ETHTOOL_SGSO,    ETH_FLAG_GSO,	0 },
	{ "gro",    "generic-receive-offload",	    "rx-gro",
	  ETHTOOL_GGRO,	   ETHTOOL_SGRO,    ETH_FLAG_GRO,	0 },
	{ "lro",    "large-receive-offload",	    "rx-lro",
	  0,		   0,		    ETH_FLAG_LRO,
	  KERNEL_VERSION(2,6,24) },
	{ "rxvlan", "rx-vlan-offload",		    "rx-vlan-hw-parse",
	  0,		   0,		    ETH_FLAG_RXVLAN,
	  KERNEL_VERSION(2,6,37) },
	{ "txvlan", "tx-vlan-offload",		    "tx-vlan-hw-insert",
	  0,		   0,		    ETH_FLAG_TXVLAN,
	  KERNEL_VERSION(2,6,37) },
	{ "ntuple", "ntuple-filters",		    "rx-ntuple-filter",
	  0,		   0,		    ETH_FLAG_NTUPLE,	0 },
	{ "rxhash", "receive-hashing",		    "rx-hashing",
	  0,		   0,		    ETH_FLAG_RXHASH,	0 },
};

struct feature_def {
	char name[ETH_GSTRING_LEN];
	int off_flag_index; /* index in off_flag_def; negative if none match */
};

struct feature_defs {
	size_t n_features;
	/* Number of features each offload flag is associated with */
	unsigned int off_flag_matched[ARRAY_SIZE(off_flag_def)];
	/* Name and offload flag index for each feature */
	struct feature_def def[0];
};

#define FEATURE_BITS_TO_BLOCKS(n_bits)		DIV_ROUND_UP(n_bits, 32U)
#define FEATURE_WORD(blocks, index, field)	((blocks)[(index) / 32U].field)
#define FEATURE_FIELD_FLAG(index)		(1U << (index) % 32U)
#define FEATURE_BIT_SET(blocks, index, field)			\
	(FEATURE_WORD(blocks, index, field) |= FEATURE_FIELD_FLAG(index))
#define FEATURE_BIT_CLEAR(blocks, index, field)			\
	(FEATURE_WORD(blocks, index, filed) &= ~FEATURE_FIELD_FLAG(index))
#define FEATURE_BIT_IS_SET(blocks, index, field)		\
	(FEATURE_WORD(blocks, index, field) & FEATURE_FIELD_FLAG(index))

static long long
get_int_range(char *str, int base, long long min, long long max)
{
	long long v;
	char *endp;

	if (!str)
		exit_bad_args();
	errno = 0;
	v = strtoll(str, &endp, base);
	if (errno || *endp || v < min || v > max)
		exit_bad_args();
	return v;
}

static unsigned long long
get_uint_range(char *str, int base, unsigned long long max)
{
	unsigned long long v;
	char *endp;

	if (!str)
		exit_bad_args();
	errno = 0;
	v = strtoull(str, &endp, base);
	if ( errno || *endp || v > max)
		exit_bad_args();
	return v;
}

static int get_int(char *str, int base)
{
	return get_int_range(str, base, INT_MIN, INT_MAX);
}

static u32 get_u32(char *str, int base)
{
	return get_uint_range(str, base, 0xffffffff);
}

static void get_mac_addr(char *src, unsigned char *dest)
{
	int count;
	int i;
	int buf[ETH_ALEN];

	count = sscanf(src, "%2x:%2x:%2x:%2x:%2x:%2x",
		&buf[0], &buf[1], &buf[2], &buf[3], &buf[4], &buf[5]);
	if (count != ETH_ALEN)
		exit_bad_args();

	for (i = 0; i < count; i++) {
		dest[i] = buf[i];
	}
}

static int parse_hex_u32_bitmap(const char *s,
				unsigned int nbits, u32 *result)
{
	const unsigned int nwords = __KERNEL_DIV_ROUND_UP(nbits, 32);
	size_t slen = strlen(s);
	size_t i;

	/* ignore optional '0x' prefix */
	if ((slen > 2) && (strncasecmp(s, "0x", 2) == 0)) {
		slen -= 2;
		s += 2;
	}

	if (slen > 8 * nwords)  /* up to 2 digits per byte */
		return -1;

	memset(result, 0, 4 * nwords);
	for (i = 0; i < slen; ++i) {
		const unsigned int shift = (slen - 1 - i) * 4;
		u32 *dest = &result[shift / 32];
		u32 nibble;

		if ('a' <= s[i] && s[i] <= 'f')
			nibble = 0xa + (s[i] - 'a');
		else if ('A' <= s[i] && s[i] <= 'F')
			nibble = 0xa + (s[i] - 'A');
		else if ('0' <= s[i] && s[i] <= '9')
			nibble = (s[i] - '0');
		else
			return -1;

		*dest |= (nibble << (shift % 32));
	}

	return 0;
}

static void parse_generic_cmdline(struct cmd_context *ctx,
				  int *changed,
				  struct cmdline_info *info,
				  unsigned int n_info)
{
	int argc = ctx->argc;
	char **argp = ctx->argp;
	int i, idx;
	int found;

	for (i = 0; i < argc; i++) {
		found = 0;
		for (idx = 0; idx < n_info; idx++) {
			if (!strcmp(info[idx].name, argp[i])) {
				found = 1;
				*changed = 1;
				if (info[idx].type != CMDL_FLAG &&
				    info[idx].seen_val)
					*(int *)info[idx].seen_val = 1;
				i += 1;
				if (i >= argc)
					exit_bad_args();
				switch (info[idx].type) {
				case CMDL_BOOL: {
					int *p = info[idx].wanted_val;
					if (!strcmp(argp[i], "on"))
						*p = 1;
					else if (!strcmp(argp[i], "off"))
						*p = 0;
					else
						exit_bad_args();
					break;
				}
				case CMDL_S32: {
					s32 *p = info[idx].wanted_val;
					*p = get_int_range(argp[i], 0,
							   -0x80000000LL,
							   0x7fffffff);
					break;
				}
				case CMDL_U8: {
					u8 *p = info[idx].wanted_val;
					*p = get_uint_range(argp[i], 0, 0xff);
					break;
				}
				case CMDL_U16: {
					u16 *p = info[idx].wanted_val;
					*p = get_uint_range(argp[i], 0, 0xffff);
					break;
				}
				case CMDL_U32: {
					u32 *p = info[idx].wanted_val;
					*p = get_uint_range(argp[i], 0,
							    0xffffffff);
					break;
				}
				case CMDL_U64: {
					u64 *p = info[idx].wanted_val;
					*p = get_uint_range(
						argp[i], 0,
						0xffffffffffffffffLL);
					break;
				}
				case CMDL_BE16: {
					u16 *p = info[idx].wanted_val;
					*p = cpu_to_be16(
						get_uint_range(argp[i], 0,
							       0xffff));
					break;
				}
				case CMDL_IP4: {
					u32 *p = info[idx].wanted_val;
					struct in_addr in;
					if (!inet_aton(argp[i], &in))
						exit_bad_args();
					*p = in.s_addr;
					break;
				}
				case CMDL_MAC:
					get_mac_addr(argp[i],
						     info[idx].wanted_val);
					break;
				case CMDL_FLAG: {
					u32 *p;
					p = info[idx].seen_val;
					*p |= info[idx].flag_val;
					if (!strcmp(argp[i], "on")) {
						p = info[idx].wanted_val;
						*p |= info[idx].flag_val;
					} else if (strcmp(argp[i], "off")) {
						exit_bad_args();
					}
					break;
				}
				case CMDL_STR: {
					char **s = info[idx].wanted_val;
					*s = strdup(argp[i]);
					break;
				}
				default:
					exit_bad_args();
				}
				break;
			}
		}
		if( !found)
			exit_bad_args();
	}
}

static void flag_to_cmdline_info(const char *name, u32 value,
				 u32 *wanted, u32 *mask,
				 struct cmdline_info *cli)
{
	memset(cli, 0, sizeof(*cli));
	cli->name = name;
	cli->type = CMDL_FLAG;
	cli->flag_val = value;
	cli->wanted_val = wanted;
	cli->seen_val = mask;
}

static void
print_flags(const struct flag_info *info, unsigned int n_info, u32 value)
{
	const char *sep = "";

	while (n_info) {
		if (value & info->value) {
			printf("%s%s", sep, info->name);
			sep = " ";
			value &= ~info->value;
		}
		++info;
		--n_info;
	}

	/* Print any unrecognised flags in hex */
	if (value)
		printf("%s%#x", sep, value);
}

static int rxflow_str_to_type(const char *str)
{
	int flow_type = 0;

	if (!strcmp(str, "tcp4"))
		flow_type = TCP_V4_FLOW;
	else if (!strcmp(str, "udp4"))
		flow_type = UDP_V4_FLOW;
	else if (!strcmp(str, "ah4") || !strcmp(str, "esp4"))
		flow_type = AH_ESP_V4_FLOW;
	else if (!strcmp(str, "sctp4"))
		flow_type = SCTP_V4_FLOW;
	else if (!strcmp(str, "tcp6"))
		flow_type = TCP_V6_FLOW;
	else if (!strcmp(str, "udp6"))
		flow_type = UDP_V6_FLOW;
	else if (!strcmp(str, "ah6") || !strcmp(str, "esp6"))
		flow_type = AH_ESP_V6_FLOW;
	else if (!strcmp(str, "sctp6"))
		flow_type = SCTP_V6_FLOW;
	else if (!strcmp(str, "ether"))
		flow_type = ETHER_FLOW;

	return flow_type;
}

static int do_version(struct cmd_context *ctx)
{
	fprintf(stdout,
		PACKAGE " version " VERSION
#ifndef ETHTOOL_ENABLE_PRETTY_DUMP
		" (pretty dumps disabled)"
#endif
		"\n");
	return 0;
}

/* link mode routines */

static ETHTOOL_DECLARE_LINK_MODE_MASK(all_advertised_modes);
static ETHTOOL_DECLARE_LINK_MODE_MASK(all_advertised_flags);

static void init_global_link_mode_masks(void)
{
	static const enum ethtool_link_mode_bit_indices
		all_advertised_modes_bits[] = {
		ETHTOOL_LINK_MODE_10baseT_Half_BIT,
		ETHTOOL_LINK_MODE_10baseT_Full_BIT,
		ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		ETHTOOL_LINK_MODE_100baseT_Full_BIT,
		ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
		ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
		ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseR_FEC_BIT,
		ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT,
		ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT,
		ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT,
		ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT,
		ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT,
		ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT,
		ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT,
		ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT,
		ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT,
		ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT,
		ETHTOOL_LINK_MODE_25000baseCR_Full_BIT,
		ETHTOOL_LINK_MODE_25000baseKR_Full_BIT,
		ETHTOOL_LINK_MODE_25000baseSR_Full_BIT,
		ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT,
		ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT,
		ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT,
		ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT,
		ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT,
		ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
		ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT,
		ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseCR_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
	};
	static const enum ethtool_link_mode_bit_indices
		additional_advertised_flags_bits[] = {
		ETHTOOL_LINK_MODE_Autoneg_BIT,
		ETHTOOL_LINK_MODE_TP_BIT,
		ETHTOOL_LINK_MODE_AUI_BIT,
		ETHTOOL_LINK_MODE_MII_BIT,
		ETHTOOL_LINK_MODE_FIBRE_BIT,
		ETHTOOL_LINK_MODE_BNC_BIT,
		ETHTOOL_LINK_MODE_Pause_BIT,
		ETHTOOL_LINK_MODE_Asym_Pause_BIT,
		ETHTOOL_LINK_MODE_Backplane_BIT,
	};
	unsigned int i;

	ethtool_link_mode_zero(all_advertised_modes);
	ethtool_link_mode_zero(all_advertised_flags);
	for (i = 0; i < ARRAY_SIZE(all_advertised_modes_bits); ++i) {
		ethtool_link_mode_set_bit(all_advertised_modes_bits[i],
					  all_advertised_modes);
		ethtool_link_mode_set_bit(all_advertised_modes_bits[i],
					  all_advertised_flags);
	}

	for (i = 0; i < ARRAY_SIZE(additional_advertised_flags_bits); ++i) {
		ethtool_link_mode_set_bit(
			additional_advertised_flags_bits[i],
			all_advertised_flags);
	}
}

static void dump_link_caps(const char *prefix, const char *an_prefix,
			   const u32 *mask, int link_mode_only);

static void dump_supported(const struct ethtool_link_usettings *link_usettings)
{
	fprintf(stdout, "	Supported ports: [ ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_TP_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "TP ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_AUI_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "AUI ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_BNC_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "BNC ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_MII_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "MII ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_FIBRE_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "FIBRE ");
	if (ethtool_link_mode_test_bit(
		    ETHTOOL_LINK_MODE_Backplane_BIT,
		    link_usettings->link_modes.supported))
		fprintf(stdout, "Backplane ");
	fprintf(stdout, "]\n");

	dump_link_caps("Supported", "Supports",
		       link_usettings->link_modes.supported, 0);
}

/* Print link capability flags (supported, advertised or lp_advertised).
 * Assumes that the corresponding SUPPORTED and ADVERTISED flags are equal.
 */
static void dump_link_caps(const char *prefix, const char *an_prefix,
			   const u32 *mask, int link_mode_only)
{
	static const struct {
		int same_line; /* print on same line as previous */
		unsigned int bit_index;
		const char *name;
	} mode_defs[] = {
		{ 0, ETHTOOL_LINK_MODE_10baseT_Half_BIT,
		  "10baseT/Half" },
		{ 1, ETHTOOL_LINK_MODE_10baseT_Full_BIT,
		  "10baseT/Full" },
		{ 0, ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		  "100baseT/Half" },
		{ 1, ETHTOOL_LINK_MODE_100baseT_Full_BIT,
		  "100baseT/Full" },
		{ 0, ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
		  "1000baseT/Half" },
		{ 1, ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		  "1000baseT/Full" },
		{ 0, ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
		  "1000baseKX/Full" },
		{ 0, ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
		  "2500baseX/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
		  "10000baseT/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
		  "10000baseKX4/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
		  "10000baseKR/Full" },
		{ 0, ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT,
		  "20000baseMLD2/Full" },
		{ 0, ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT,
		  "20000baseKR2/Full" },
		{ 0, ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT,
		  "40000baseKR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT,
		  "40000baseCR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT,
		  "40000baseSR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT,
		  "40000baseLR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT,
		  "56000baseKR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT,
		  "56000baseCR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT,
		  "56000baseSR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT,
		  "56000baseLR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_25000baseCR_Full_BIT,
		  "25000baseCR/Full" },
		{ 0, ETHTOOL_LINK_MODE_25000baseKR_Full_BIT,
		  "25000baseKR/Full" },
		{ 0, ETHTOOL_LINK_MODE_25000baseSR_Full_BIT,
		  "25000baseSR/Full" },
		{ 0, ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT,
		  "50000baseCR2/Full" },
		{ 0, ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT,
		  "50000baseKR2/Full" },
		{ 0, ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT,
		  "100000baseKR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT,
		  "100000baseSR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT,
		  "100000baseCR4/Full" },
		{ 0, ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
		  "100000baseLR4_ER4/Full" },
		{ 0, ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT,
		  "50000baseSR2/Full" },
		{ 0, ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
		  "1000baseX/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseCR_Full_BIT,
		  "10000baseCR/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
		  "10000baseSR/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
		  "10000baseLR/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
		  "10000baseLRM/Full" },
		{ 0, ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
		  "10000baseER/Full" },
	};
	int indent;
	int did1, new_line_pend, i;

	/* Indent just like the separate functions used to */
	indent = strlen(prefix) + 14;
	if (indent < 24)
		indent = 24;

	fprintf(stdout, "	%s link modes:%*s", prefix,
		indent - (int)strlen(prefix) - 12, "");
	did1 = 0;
	new_line_pend = 0;
	for (i = 0; i < ARRAY_SIZE(mode_defs); i++) {
		if (did1 && !mode_defs[i].same_line)
			new_line_pend = 1;
		if (ethtool_link_mode_test_bit(mode_defs[i].bit_index,
					       mask)) {
			if (new_line_pend) {
				fprintf(stdout, "\n");
				fprintf(stdout, "	%*s", indent, "");
				new_line_pend = 0;
			}
			did1++;
			fprintf(stdout, "%s ", mode_defs[i].name);
		}
	}
	if (did1 == 0)
		 fprintf(stdout, "Not reported");
	fprintf(stdout, "\n");

	if (!link_mode_only) {
		fprintf(stdout, "	%s pause frame use: ", prefix);
		if (ethtool_link_mode_test_bit(
			    ETHTOOL_LINK_MODE_Pause_BIT, mask)) {
			fprintf(stdout, "Symmetric");
			if (ethtool_link_mode_test_bit(
				    ETHTOOL_LINK_MODE_Asym_Pause_BIT, mask))
				fprintf(stdout, " Receive-only");
			fprintf(stdout, "\n");
		} else {
			if (ethtool_link_mode_test_bit(
				    ETHTOOL_LINK_MODE_Asym_Pause_BIT, mask))
				fprintf(stdout, "Transmit-only\n");
			else
				fprintf(stdout, "No\n");
		}

		fprintf(stdout, "	%s auto-negotiation: ", an_prefix);
		if (ethtool_link_mode_test_bit(
			    ETHTOOL_LINK_MODE_Autoneg_BIT, mask))
			fprintf(stdout, "Yes\n");
		else
			fprintf(stdout, "No\n");
	}
}

static int
dump_link_usettings(const struct ethtool_link_usettings *link_usettings)
{
	dump_supported(link_usettings);
	dump_link_caps("Advertised", "Advertised",
		       link_usettings->link_modes.advertising, 0);
	if (!ethtool_link_mode_is_empty(
		    link_usettings->link_modes.lp_advertising))
		dump_link_caps("Link partner advertised",
			       "Link partner advertised",
			       link_usettings->link_modes.lp_advertising, 0);

	fprintf(stdout, "	Speed: ");
	if (link_usettings->base.speed == 0
	    || link_usettings->base.speed == (u16)(-1)
	    || link_usettings->base.speed == (u32)(-1))
		fprintf(stdout, "Unknown!\n");
	else
		fprintf(stdout, "%uMb/s\n", link_usettings->base.speed);

	fprintf(stdout, "	Duplex: ");
	switch (link_usettings->base.duplex) {
	case DUPLEX_HALF:
		fprintf(stdout, "Half\n");
		break;
	case DUPLEX_FULL:
		fprintf(stdout, "Full\n");
		break;
	default:
		fprintf(stdout, "Unknown! (%i)\n", link_usettings->base.duplex);
		break;
	};

	fprintf(stdout, "	Port: ");
	switch (link_usettings->base.port) {
	case PORT_TP:
		fprintf(stdout, "Twisted Pair\n");
		break;
	case PORT_AUI:
		fprintf(stdout, "AUI\n");
		break;
	case PORT_BNC:
		fprintf(stdout, "BNC\n");
		break;
	case PORT_MII:
		fprintf(stdout, "MII\n");
		break;
	case PORT_FIBRE:
		fprintf(stdout, "FIBRE\n");
		break;
	case PORT_DA:
		fprintf(stdout, "Direct Attach Copper\n");
		break;
	case PORT_NONE:
		fprintf(stdout, "None\n");
		break;
	case PORT_OTHER:
		fprintf(stdout, "Other\n");
		break;
	default:
		fprintf(stdout, "Unknown! (%i)\n", link_usettings->base.port);
		break;
	};

	fprintf(stdout, "	PHYAD: %d\n", link_usettings->base.phy_address);
	fprintf(stdout, "	Transceiver: ");
	switch (link_usettings->deprecated.transceiver) {
	case XCVR_INTERNAL:
		fprintf(stdout, "internal\n");
		break;
	case XCVR_EXTERNAL:
		fprintf(stdout, "external\n");
		break;
	default:
		fprintf(stdout, "Unknown!\n");
		break;
	};

	fprintf(stdout, "	Auto-negotiation: %s\n",
		(link_usettings->base.autoneg == AUTONEG_DISABLE) ?
		"off" : "on");

	if (link_usettings->base.port == PORT_TP) {
		fprintf(stdout, "	MDI-X: ");
		if (link_usettings->base.eth_tp_mdix_ctrl == ETH_TP_MDI) {
			fprintf(stdout, "off (forced)\n");
		} else if (link_usettings->base.eth_tp_mdix_ctrl
			   == ETH_TP_MDI_X) {
			fprintf(stdout, "on (forced)\n");
		} else {
			switch (link_usettings->base.eth_tp_mdix) {
			case ETH_TP_MDI:
				fprintf(stdout, "off");
				break;
			case ETH_TP_MDI_X:
				fprintf(stdout, "on");
				break;
			default:
				fprintf(stdout, "Unknown");
				break;
			}
			if (link_usettings->base.eth_tp_mdix_ctrl
			    == ETH_TP_MDI_AUTO)
				fprintf(stdout, " (auto)");
			fprintf(stdout, "\n");
		}
	}

	return 0;
}

static int dump_drvinfo(struct ethtool_drvinfo *info)
{
	fprintf(stdout,
		"driver: %.*s\n"
		"version: %.*s\n"
		"firmware-version: %.*s\n"
		"expansion-rom-version: %.*s\n"
		"bus-info: %.*s\n"
		"supports-statistics: %s\n"
		"supports-test: %s\n"
		"supports-eeprom-access: %s\n"
		"supports-register-dump: %s\n"
		"supports-priv-flags: %s\n",
		(int)sizeof(info->driver), info->driver,
		(int)sizeof(info->version), info->version,
		(int)sizeof(info->fw_version), info->fw_version,
		(int)sizeof(info->erom_version), info->erom_version,
		(int)sizeof(info->bus_info), info->bus_info,
		info->n_stats ? "yes" : "no",
		info->testinfo_len ? "yes" : "no",
		info->eedump_len ? "yes" : "no",
		info->regdump_len ? "yes" : "no",
		info->n_priv_flags ? "yes" : "no");

	return 0;
}

static int parse_wolopts(char *optstr, u32 *data)
{
	*data = 0;
	while (*optstr) {
		switch (*optstr) {
			case 'p':
				*data |= WAKE_PHY;
				break;
			case 'u':
				*data |= WAKE_UCAST;
				break;
			case 'm':
				*data |= WAKE_MCAST;
				break;
			case 'b':
				*data |= WAKE_BCAST;
				break;
			case 'a':
				*data |= WAKE_ARP;
				break;
			case 'g':
				*data |= WAKE_MAGIC;
				break;
			case 's':
				*data |= WAKE_MAGICSECURE;
				break;
			case 'd':
				*data = 0;
				break;
			default:
				return -1;
		}
		optstr++;
	}
	return 0;
}

static char *unparse_wolopts(int wolopts)
{
	static char buf[16];
	char *p = buf;

	memset(buf, 0, sizeof(buf));

	if (wolopts) {
		if (wolopts & WAKE_PHY)
			*p++ = 'p';
		if (wolopts & WAKE_UCAST)
			*p++ = 'u';
		if (wolopts & WAKE_MCAST)
			*p++ = 'm';
		if (wolopts & WAKE_BCAST)
			*p++ = 'b';
		if (wolopts & WAKE_ARP)
			*p++ = 'a';
		if (wolopts & WAKE_MAGIC)
			*p++ = 'g';
		if (wolopts & WAKE_MAGICSECURE)
			*p++ = 's';
	} else {
		*p = 'd';
	}

	return buf;
}

static int dump_wol(struct ethtool_wolinfo *wol)
{
	fprintf(stdout, "	Supports Wake-on: %s\n",
		unparse_wolopts(wol->supported));
	fprintf(stdout, "	Wake-on: %s\n",
		unparse_wolopts(wol->wolopts));
	if (wol->supported & WAKE_MAGICSECURE) {
		int i;
		int delim = 0;
		fprintf(stdout, "        SecureOn password: ");
		for (i = 0; i < SOPASS_MAX; i++) {
			fprintf(stdout, "%s%02x", delim?":":"", wol->sopass[i]);
			delim=1;
		}
		fprintf(stdout, "\n");
	}

	return 0;
}

static int parse_rxfhashopts(char *optstr, u32 *data)
{
	*data = 0;
	while (*optstr) {
		switch (*optstr) {
			case 'm':
				*data |= RXH_L2DA;
				break;
			case 'v':
				*data |= RXH_VLAN;
				break;
			case 't':
				*data |= RXH_L3_PROTO;
				break;
			case 's':
				*data |= RXH_IP_SRC;
				break;
			case 'd':
				*data |= RXH_IP_DST;
				break;
			case 'f':
				*data |= RXH_L4_B_0_1;
				break;
			case 'n':
				*data |= RXH_L4_B_2_3;
				break;
			case 'r':
				*data |= RXH_DISCARD;
				break;
			default:
				return -1;
		}
		optstr++;
	}
	return 0;
}

static char *unparse_rxfhashopts(u64 opts)
{
	static char buf[300];

	memset(buf, 0, sizeof(buf));

	if (opts) {
		if (opts & RXH_L2DA) {
			strcat(buf, "L2DA\n");
		}
		if (opts & RXH_VLAN) {
			strcat(buf, "VLAN tag\n");
		}
		if (opts & RXH_L3_PROTO) {
			strcat(buf, "L3 proto\n");
		}
		if (opts & RXH_IP_SRC) {
			strcat(buf, "IP SA\n");
		}
		if (opts & RXH_IP_DST) {
			strcat(buf, "IP DA\n");
		}
		if (opts & RXH_L4_B_0_1) {
			strcat(buf, "L4 bytes 0 & 1 [TCP/UDP src port]\n");
		}
		if (opts & RXH_L4_B_2_3) {
			strcat(buf, "L4 bytes 2 & 3 [TCP/UDP dst port]\n");
		}
	} else {
		sprintf(buf, "None");
	}

	return buf;
}

static int convert_string_to_hashkey(char *rss_hkey, u32 key_size,
				     const char *rss_hkey_string)
{
	u32 i = 0;
	int hex_byte, len;

	do {
		if (i > (key_size - 1)) {
			fprintf(stderr,
				"Key is too long for device (%u > %u)\n",
				i + 1, key_size);
			goto err;
		}

		if (sscanf(rss_hkey_string, "%2x%n", &hex_byte, &len) < 1 ||
		    len != 2) {
			fprintf(stderr, "Invalid RSS hash key format\n");
			goto err;
		}

		rss_hkey[i++] = hex_byte;
		rss_hkey_string += 2;

		if (*rss_hkey_string == ':') {
			rss_hkey_string++;
		} else if (*rss_hkey_string != '\0') {
			fprintf(stderr, "Invalid RSS hash key format\n");
			goto err;
		}

	} while (*rss_hkey_string);

	if (i != key_size) {
		fprintf(stderr, "Key is too short for device (%u < %u)\n",
			i, key_size);
		goto err;
	}

	return 0;
err:
	return 2;
}

static int parse_hkey(char **rss_hkey, u32 key_size,
		      const char *rss_hkey_string)
{
	if (!key_size) {
		fprintf(stderr,
			"Cannot set RX flow hash configuration:\n"
			" Hash key setting not supported\n");
		return 1;
	}

	*rss_hkey = malloc(key_size);
	if (!(*rss_hkey)) {
		perror("Cannot allocate memory for RSS hash key");
		return 1;
	}

	if (convert_string_to_hashkey(*rss_hkey, key_size,
				      rss_hkey_string)) {
		free(*rss_hkey);
		*rss_hkey = NULL;
		return 2;
	}
	return 0;
}

static const struct {
	const char *name;
	int (*func)(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

} driver_list[] = {
#ifdef ETHTOOL_ENABLE_PRETTY_DUMP
	{ "8139cp", realtek_dump_regs },
	{ "8139too", realtek_dump_regs },
	{ "r8169", realtek_dump_regs },
	{ "de2104x", de2104x_dump_regs },
	{ "e1000", e1000_dump_regs },
	{ "e1000e", e1000_dump_regs },
	{ "igb", igb_dump_regs },
	{ "ixgb", ixgb_dump_regs },
	{ "ixgbe", ixgbe_dump_regs },
	{ "ixgbevf", ixgbevf_dump_regs },
	{ "natsemi", natsemi_dump_regs },
	{ "e100", e100_dump_regs },
	{ "amd8111e", amd8111e_dump_regs },
	{ "pcnet32", pcnet32_dump_regs },
	{ "fec_8xx", fec_8xx_dump_regs },
	{ "ibm_emac", ibm_emac_dump_regs },
	{ "tg3", tg3_dump_regs },
	{ "skge", skge_dump_regs },
	{ "sky2", sky2_dump_regs },
        { "vioc", vioc_dump_regs },
        { "smsc911x", smsc911x_dump_regs },
        { "at76c50x-usb", at76c50x_usb_dump_regs },
        { "sfc", sfc_dump_regs },
	{ "st_mac100", st_mac100_dump_regs },
	{ "st_gmac", st_gmac_dump_regs },
	{ "et131x", et131x_dump_regs },
	{ "altera_tse", altera_tse_dump_regs },
	{ "vmxnet3", vmxnet3_dump_regs },
#endif
};

void dump_hex(FILE *file, const u8 *data, int len, int offset)
{
	int i;

	fprintf(file, "Offset\t\tValues\n");
	fprintf(file, "------\t\t------");
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			fprintf(file, "\n0x%04x:\t\t", i + offset);
		fprintf(file, "%02x ", data[i]);
	}
	fprintf(file, "\n");
}

static int dump_regs(int gregs_dump_raw, int gregs_dump_hex,
		     struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	int i;

	if (gregs_dump_raw) {
		fwrite(regs->data, regs->len, 1, stdout);
		return 0;
	}

	if (!gregs_dump_hex)
		for (i = 0; i < ARRAY_SIZE(driver_list); i++)
			if (!strncmp(driver_list[i].name, info->driver,
				     ETHTOOL_BUSINFO_LEN)) {
				if (driver_list[i].func(info, regs) == 0)
					return 0;
				/* This version (or some other
				 * variation in the dump format) is
				 * not handled; fall back to hex
				 */
				break;
			}

	dump_hex(stdout, regs->data, regs->len, 0);

	return 0;
}

static int dump_eeprom(int geeprom_dump_raw, struct ethtool_drvinfo *info,
		       struct ethtool_eeprom *ee)
{
	if (geeprom_dump_raw) {
		fwrite(ee->data, 1, ee->len, stdout);
		return 0;
	}
#ifdef ETHTOOL_ENABLE_PRETTY_DUMP
	if (!strncmp("natsemi", info->driver, ETHTOOL_BUSINFO_LEN)) {
		return natsemi_dump_eeprom(info, ee);
	} else if (!strncmp("tg3", info->driver, ETHTOOL_BUSINFO_LEN)) {
		return tg3_dump_eeprom(info, ee);
	}
#endif
	dump_hex(stdout, ee->data, ee->len, ee->offset);

	return 0;
}

static int dump_test(struct ethtool_test *test,
		     struct ethtool_gstrings *strings)
{
	int i, rc;

	rc = test->flags & ETH_TEST_FL_FAILED;
	fprintf(stdout, "The test result is %s\n", rc ? "FAIL" : "PASS");

	if (test->flags & ETH_TEST_FL_EXTERNAL_LB)
		fprintf(stdout, "External loopback test was %sexecuted\n",
			(test->flags & ETH_TEST_FL_EXTERNAL_LB_DONE) ?
			"" : "not ");

	if (strings->len)
		fprintf(stdout, "The test extra info:\n");

	for (i = 0; i < strings->len; i++) {
		fprintf(stdout, "%s\t %d\n",
			(char *)(strings->data + i * ETH_GSTRING_LEN),
			(u32) test->data[i]);
	}

	fprintf(stdout, "\n");
	return rc;
}

static int dump_pause(const struct ethtool_pauseparam *epause,
		      u32 advertising, u32 lp_advertising)
{
	fprintf(stdout,
		"Autonegotiate:	%s\n"
		"RX:		%s\n"
		"TX:		%s\n",
		epause->autoneg ? "on" : "off",
		epause->rx_pause ? "on" : "off",
		epause->tx_pause ? "on" : "off");

	if (lp_advertising) {
		int an_rx = 0, an_tx = 0;

		/* Work out negotiated pause frame usage per
		 * IEEE 802.3-2005 table 28B-3.
		 */
		if (advertising & lp_advertising & ADVERTISED_Pause) {
			an_tx = 1;
			an_rx = 1;
		} else if (advertising & lp_advertising &
			   ADVERTISED_Asym_Pause) {
			if (advertising & ADVERTISED_Pause)
				an_rx = 1;
			else if (lp_advertising & ADVERTISED_Pause)
				an_tx = 1;
		}

		fprintf(stdout,
			"RX negotiated:	%s\n"
			"TX negotiated:	%s\n",
			an_rx ? "on" : "off",
			an_tx ? "on" : "off");
	}

	fprintf(stdout, "\n");
	return 0;
}

static int dump_ring(const struct ethtool_ringparam *ering)
{
	fprintf(stdout,
		"Pre-set maximums:\n"
		"RX:		%u\n"
		"RX Mini:	%u\n"
		"RX Jumbo:	%u\n"
		"TX:		%u\n",
		ering->rx_max_pending,
		ering->rx_mini_max_pending,
		ering->rx_jumbo_max_pending,
		ering->tx_max_pending);

	fprintf(stdout,
		"Current hardware settings:\n"
		"RX:		%u\n"
		"RX Mini:	%u\n"
		"RX Jumbo:	%u\n"
		"TX:		%u\n",
		ering->rx_pending,
		ering->rx_mini_pending,
		ering->rx_jumbo_pending,
		ering->tx_pending);

	fprintf(stdout, "\n");
	return 0;
}

static int dump_channels(const struct ethtool_channels *echannels)
{
	fprintf(stdout,
		"Pre-set maximums:\n"
		"RX:		%u\n"
		"TX:		%u\n"
		"Other:		%u\n"
		"Combined:	%u\n",
		echannels->max_rx, echannels->max_tx,
		echannels->max_other,
		echannels->max_combined);

	fprintf(stdout,
		"Current hardware settings:\n"
		"RX:		%u\n"
		"TX:		%u\n"
		"Other:		%u\n"
		"Combined:	%u\n",
		echannels->rx_count, echannels->tx_count,
		echannels->other_count,
		echannels->combined_count);

	fprintf(stdout, "\n");
	return 0;
}

static int dump_coalesce(const struct ethtool_coalesce *ecoal)
{
	fprintf(stdout, "Adaptive RX: %s  TX: %s\n",
		ecoal->use_adaptive_rx_coalesce ? "on" : "off",
		ecoal->use_adaptive_tx_coalesce ? "on" : "off");

	fprintf(stdout,
		"stats-block-usecs: %u\n"
		"sample-interval: %u\n"
		"pkt-rate-low: %u\n"
		"pkt-rate-high: %u\n"
		"\n"
		"rx-usecs: %u\n"
		"rx-frames: %u\n"
		"rx-usecs-irq: %u\n"
		"rx-frames-irq: %u\n"
		"\n"
		"tx-usecs: %u\n"
		"tx-frames: %u\n"
		"tx-usecs-irq: %u\n"
		"tx-frames-irq: %u\n"
		"\n"
		"rx-usecs-low: %u\n"
		"rx-frame-low: %u\n"
		"tx-usecs-low: %u\n"
		"tx-frame-low: %u\n"
		"\n"
		"rx-usecs-high: %u\n"
		"rx-frame-high: %u\n"
		"tx-usecs-high: %u\n"
		"tx-frame-high: %u\n"
		"\n",
		ecoal->stats_block_coalesce_usecs,
		ecoal->rate_sample_interval,
		ecoal->pkt_rate_low,
		ecoal->pkt_rate_high,

		ecoal->rx_coalesce_usecs,
		ecoal->rx_max_coalesced_frames,
		ecoal->rx_coalesce_usecs_irq,
		ecoal->rx_max_coalesced_frames_irq,

		ecoal->tx_coalesce_usecs,
		ecoal->tx_max_coalesced_frames,
		ecoal->tx_coalesce_usecs_irq,
		ecoal->tx_max_coalesced_frames_irq,

		ecoal->rx_coalesce_usecs_low,
		ecoal->rx_max_coalesced_frames_low,
		ecoal->tx_coalesce_usecs_low,
		ecoal->tx_max_coalesced_frames_low,

		ecoal->rx_coalesce_usecs_high,
		ecoal->rx_max_coalesced_frames_high,
		ecoal->tx_coalesce_usecs_high,
		ecoal->tx_max_coalesced_frames_high);

	return 0;
}

struct feature_state {
	u32 off_flags;
	struct ethtool_gfeatures features;
};

static void dump_one_feature(const char *indent, const char *name,
			     const struct feature_state *state,
			     const struct feature_state *ref_state,
			     u32 index)
{
	if (ref_state &&
	    !(FEATURE_BIT_IS_SET(state->features.features, index, active) ^
	      FEATURE_BIT_IS_SET(ref_state->features.features, index, active)))
		return;

	printf("%s%s: %s%s\n",
	       indent, name,
	       FEATURE_BIT_IS_SET(state->features.features, index, active) ?
	       "on" : "off",
	       (!FEATURE_BIT_IS_SET(state->features.features, index, available)
		|| FEATURE_BIT_IS_SET(state->features.features, index,
				      never_changed))
	       ? " [fixed]"
	       : (FEATURE_BIT_IS_SET(state->features.features, index, requested)
		  ^ FEATURE_BIT_IS_SET(state->features.features, index, active))
	       ? (FEATURE_BIT_IS_SET(state->features.features, index, requested)
		  ? " [requested on]" : " [requested off]")
	       : "");
}

static int linux_version_code(void)
{
	struct utsname utsname;
	unsigned version, patchlevel, sublevel = 0;

	if (uname(&utsname))
		return -1;
	if (sscanf(utsname.release, "%u.%u.%u", &version, &patchlevel, &sublevel) < 2)
		return -1;
	return KERNEL_VERSION(version, patchlevel, sublevel);
}

static void dump_features(const struct feature_defs *defs,
			  const struct feature_state *state,
			  const struct feature_state *ref_state)
{
	int kernel_ver = linux_version_code();
	u32 value;
	int indent;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(off_flag_def); i++) {
		/* Don't show features whose state is unknown on this
		 * kernel version
		 */
		if (defs->off_flag_matched[i] == 0 &&
		    off_flag_def[i].get_cmd == 0 &&
		    kernel_ver < off_flag_def[i].min_kernel_ver)
			continue;

		value = off_flag_def[i].value;

		/* If this offload flag matches exactly one generic
		 * feature then it's redundant to show the flag and
		 * feature states separately.  Otherwise, show the
		 * flag state first.
		 */
		if (defs->off_flag_matched[i] != 1 &&
		    (!ref_state ||
		     (state->off_flags ^ ref_state->off_flags) & value)) {
			printf("%s: %s\n",
			       off_flag_def[i].long_name,
			       (state->off_flags & value) ? "on" : "off");
			indent = 1;
		} else {
			indent = 0;
		}

		/* Show matching features */
		for (j = 0; j < defs->n_features; j++) {
			if (defs->def[j].off_flag_index != i)
				continue;
			if (defs->off_flag_matched[i] != 1)
				/* Show all matching feature states */
				dump_one_feature(indent ? "\t" : "",
						 defs->def[j].name,
						 state, ref_state, j);
			else
				/* Show full state with the old flag name */
				dump_one_feature("", off_flag_def[i].long_name,
						 state, ref_state, j);
		}
	}

	/* Show all unmatched features that have non-null names */
	for (j = 0; j < defs->n_features; j++)
		if (defs->def[j].off_flag_index < 0 && defs->def[j].name[0])
			dump_one_feature("", defs->def[j].name,
					 state, ref_state, j);
}

static int dump_rxfhash(int fhash, u64 val)
{
	switch (fhash) {
	case TCP_V4_FLOW:
		fprintf(stdout, "TCP over IPV4 flows");
		break;
	case UDP_V4_FLOW:
		fprintf(stdout, "UDP over IPV4 flows");
		break;
	case SCTP_V4_FLOW:
		fprintf(stdout, "SCTP over IPV4 flows");
		break;
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
		fprintf(stdout, "IPSEC AH/ESP over IPV4 flows");
		break;
	case TCP_V6_FLOW:
		fprintf(stdout, "TCP over IPV6 flows");
		break;
	case UDP_V6_FLOW:
		fprintf(stdout, "UDP over IPV6 flows");
		break;
	case SCTP_V6_FLOW:
		fprintf(stdout, "SCTP over IPV6 flows");
		break;
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
		fprintf(stdout, "IPSEC AH/ESP over IPV6 flows");
		break;
	default:
		break;
	}

	if (val & RXH_DISCARD) {
		fprintf(stdout, " - All matching flows discarded on RX\n");
		return 0;
	}
	fprintf(stdout, " use these fields for computing Hash flow key:\n");

	fprintf(stdout, "%s\n", unparse_rxfhashopts(val));

	return 0;
}

static void dump_eeecmd(struct ethtool_eee *ep)
{
	ETHTOOL_DECLARE_LINK_MODE_MASK(link_mode);

	fprintf(stdout, "	EEE status: ");
	if (!ep->supported) {
		fprintf(stdout, "not supported\n");
		return;
	} else if (!ep->eee_enabled) {
		fprintf(stdout, "disabled\n");
	} else {
		fprintf(stdout, "enabled - ");
		if (ep->eee_active)
			fprintf(stdout, "active\n");
		else
			fprintf(stdout, "inactive\n");
	}

	fprintf(stdout, "	Tx LPI:");
	if (ep->tx_lpi_enabled)
		fprintf(stdout, " %d (us)\n", ep->tx_lpi_timer);
	else
		fprintf(stdout, " disabled\n");

	ethtool_link_mode_zero(link_mode);

	link_mode[0] = ep->supported;
	dump_link_caps("Supported EEE", "", link_mode, 1);

	link_mode[0] = ep->advertised;
	dump_link_caps("Advertised EEE", "", link_mode, 1);

	link_mode[0] = ep->lp_advertised;
	dump_link_caps("Link partner advertised EEE", "", link_mode, 1);
}

#define N_SOTS 7

static char *so_timestamping_labels[N_SOTS] = {
	"hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)",
	"software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)",
	"hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)",
	"software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)",
	"software-system-clock (SOF_TIMESTAMPING_SOFTWARE)",
	"hardware-legacy-clock (SOF_TIMESTAMPING_SYS_HARDWARE)",
	"hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)",
};

#define N_TX_TYPES (HWTSTAMP_TX_ONESTEP_SYNC + 1)

static char *tx_type_labels[N_TX_TYPES] = {
	"off                   (HWTSTAMP_TX_OFF)",
	"on                    (HWTSTAMP_TX_ON)",
	"one-step-sync         (HWTSTAMP_TX_ONESTEP_SYNC)",
};

#define N_RX_FILTERS (HWTSTAMP_FILTER_PTP_V2_DELAY_REQ + 1)

static char *rx_filter_labels[N_RX_FILTERS] = {
	"none                  (HWTSTAMP_FILTER_NONE)",
	"all                   (HWTSTAMP_FILTER_ALL)",
	"some                  (HWTSTAMP_FILTER_SOME)",
	"ptpv1-l4-event        (HWTSTAMP_FILTER_PTP_V1_L4_EVENT)",
	"ptpv1-l4-sync         (HWTSTAMP_FILTER_PTP_V1_L4_SYNC)",
	"ptpv1-l4-delay-req    (HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ)",
	"ptpv2-l4-event        (HWTSTAMP_FILTER_PTP_V2_L4_EVENT)",
	"ptpv2-l4-sync         (HWTSTAMP_FILTER_PTP_V2_L4_SYNC)",
	"ptpv2-l4-delay-req    (HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ)",
	"ptpv2-l2-event        (HWTSTAMP_FILTER_PTP_V2_L2_EVENT)",
	"ptpv2-l2-sync         (HWTSTAMP_FILTER_PTP_V2_L2_SYNC)",
	"ptpv2-l2-delay-req    (HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ)",
	"ptpv2-event           (HWTSTAMP_FILTER_PTP_V2_EVENT)",
	"ptpv2-sync            (HWTSTAMP_FILTER_PTP_V2_SYNC)",
	"ptpv2-delay-req       (HWTSTAMP_FILTER_PTP_V2_DELAY_REQ)",
};

static int dump_tsinfo(const struct ethtool_ts_info *info)
{
	int i;

	fprintf(stdout, "Capabilities:\n");

	for (i = 0; i < N_SOTS; i++) {
		if (info->so_timestamping & (1 << i))
			fprintf(stdout, "\t%s\n", so_timestamping_labels[i]);
	}

	fprintf(stdout, "PTP Hardware Clock: ");

	if (info->phc_index < 0)
		fprintf(stdout, "none\n");
	else
		fprintf(stdout, "%d\n", info->phc_index);

	fprintf(stdout, "Hardware Transmit Timestamp Modes:");

	if (!info->tx_types)
		fprintf(stdout, " none\n");
	else
		fprintf(stdout, "\n");

	for (i = 0; i < N_TX_TYPES; i++) {
		if (info->tx_types & (1 << i))
			fprintf(stdout,	"\t%s\n", tx_type_labels[i]);
	}

	fprintf(stdout, "Hardware Receive Filter Modes:");

	if (!info->rx_filters)
		fprintf(stdout, " none\n");
	else
		fprintf(stdout, "\n");

	for (i = 0; i < N_RX_FILTERS; i++) {
		if (info->rx_filters & (1 << i))
			fprintf(stdout, "\t%s\n", rx_filter_labels[i]);
	}

	return 0;
}

static struct ethtool_gstrings *
get_stringset(struct cmd_context *ctx, enum ethtool_stringset set_id,
	      ptrdiff_t drvinfo_offset, int null_terminate)
{
	struct {
		struct ethtool_sset_info hdr;
		u32 buf[1];
	} sset_info;
	struct ethtool_drvinfo drvinfo;
	u32 len, i;
	struct ethtool_gstrings *strings;

	sset_info.hdr.cmd = ETHTOOL_GSSET_INFO;
	sset_info.hdr.reserved = 0;
	sset_info.hdr.sset_mask = 1ULL << set_id;
	if (send_ioctl(ctx, &sset_info) == 0) {
		len = sset_info.hdr.sset_mask ? sset_info.hdr.data[0] : 0;
	} else if (errno == EOPNOTSUPP && drvinfo_offset != 0) {
		/* Fallback for old kernel versions */
		drvinfo.cmd = ETHTOOL_GDRVINFO;
		if (send_ioctl(ctx, &drvinfo))
			return NULL;
		len = *(u32 *)((char *)&drvinfo + drvinfo_offset);
	} else {
		return NULL;
	}

	strings = calloc(1, sizeof(*strings) + len * ETH_GSTRING_LEN);
	if (!strings)
		return NULL;

	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = set_id;
	strings->len = len;
	if (len != 0 && send_ioctl(ctx, strings)) {
		free(strings);
		return NULL;
	}

	if (null_terminate)
		for (i = 0; i < len; i++)
			strings->data[(i + 1) * ETH_GSTRING_LEN - 1] = 0;

	return strings;
}

static struct feature_defs *get_feature_defs(struct cmd_context *ctx)
{
	struct ethtool_gstrings *names;
	struct feature_defs *defs;
	u32 n_features;
	int i, j;

	names = get_stringset(ctx, ETH_SS_FEATURES, 0, 1);
	if (names) {
		n_features = names->len;
	} else if (errno == EOPNOTSUPP || errno == EINVAL) {
		/* Kernel doesn't support named features; not an error */
		n_features = 0;
	} else if (errno == EPERM) {
		/* Kernel bug: ETHTOOL_GSSET_INFO was privileged.
		 * Work around it. */
		n_features = 0;
	} else {
		return NULL;
	}

	defs = malloc(sizeof(*defs) + sizeof(defs->def[0]) * n_features);
	if (!defs) {
		free(names);
		return NULL;
	}

	defs->n_features = n_features;
	memset(defs->off_flag_matched, 0, sizeof(defs->off_flag_matched));

	/* Copy out feature names and find those associated with legacy flags */
	for (i = 0; i < defs->n_features; i++) {
		memcpy(defs->def[i].name, names->data + i * ETH_GSTRING_LEN,
		       ETH_GSTRING_LEN);
		defs->def[i].off_flag_index = -1;

		for (j = 0;
		     j < ARRAY_SIZE(off_flag_def) &&
			     defs->def[i].off_flag_index < 0;
		     j++) {
			const char *pattern =
				off_flag_def[j].kernel_name;
			const char *name = defs->def[i].name;
			for (;;) {
				if (*pattern == '*') {
					/* There is only one wildcard; so
					 * switch to a suffix comparison */
					size_t pattern_len =
						strlen(pattern + 1);
					size_t name_len = strlen(name);
					if (name_len < pattern_len)
						break; /* name is too short */
					name += name_len - pattern_len;
					++pattern;
				} else if (*pattern != *name) {
					break; /* mismatch */
				} else if (*pattern == 0) {
					defs->def[i].off_flag_index = j;
					defs->off_flag_matched[j]++;
					break;
				} else {
					++name;
					++pattern;
				}
			}
		}
	}

	free(names);
	return defs;
}

static int do_gdrv(struct cmd_context *ctx)
{
	int err;
	struct ethtool_drvinfo drvinfo;

	if (ctx->argc != 0)
		exit_bad_args();

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	err = send_ioctl(ctx, &drvinfo);
	if (err < 0) {
		perror("Cannot get driver information");
		return 71;
	}
	return dump_drvinfo(&drvinfo);
}

static int do_gpause(struct cmd_context *ctx)
{
	struct ethtool_pauseparam epause;
	struct ethtool_cmd ecmd;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Pause parameters for %s:\n", ctx->devname);

	epause.cmd = ETHTOOL_GPAUSEPARAM;
	err = send_ioctl(ctx, &epause);
	if (err) {
		perror("Cannot get device pause settings");
		return 76;
	}

	if (epause.autoneg) {
		ecmd.cmd = ETHTOOL_GSET;
		err = send_ioctl(ctx, &ecmd);
		if (err) {
			perror("Cannot get device settings");
			return 1;
		}
		dump_pause(&epause, ecmd.advertising, ecmd.lp_advertising);
	} else {
		dump_pause(&epause, 0, 0);
	}

	return 0;
}

static void do_generic_set1(struct cmdline_info *info, int *changed_out)
{
	int wanted, *v1, *v2;

	v1 = info->wanted_val;
	wanted = *v1;

	if (wanted < 0)
		return;

	v2 = info->ioctl_val;
	if (wanted == *v2) {
		fprintf(stderr, "%s unmodified, ignoring\n", info->name);
	} else {
		*v2 = wanted;
		*changed_out = 1;
	}
}

static void do_generic_set(struct cmdline_info *info,
			   unsigned int n_info,
			   int *changed_out)
{
	unsigned int i;

	for (i = 0; i < n_info; i++)
		do_generic_set1(&info[i], changed_out);
}

static int do_spause(struct cmd_context *ctx)
{
	struct ethtool_pauseparam epause;
	int gpause_changed = 0;
	int pause_autoneg_wanted = -1;
	int pause_rx_wanted = -1;
	int pause_tx_wanted = -1;
	struct cmdline_info cmdline_pause[] = {
		{ "autoneg", CMDL_BOOL, &pause_autoneg_wanted,
		  &epause.autoneg },
		{ "rx", CMDL_BOOL, &pause_rx_wanted, &epause.rx_pause },
		{ "tx", CMDL_BOOL, &pause_tx_wanted, &epause.tx_pause },
	};
	int err, changed = 0;

	parse_generic_cmdline(ctx, &gpause_changed,
			      cmdline_pause, ARRAY_SIZE(cmdline_pause));

	epause.cmd = ETHTOOL_GPAUSEPARAM;
	err = send_ioctl(ctx, &epause);
	if (err) {
		perror("Cannot get device pause settings");
		return 77;
	}

	do_generic_set(cmdline_pause, ARRAY_SIZE(cmdline_pause), &changed);

	if (!changed) {
		fprintf(stderr, "no pause parameters changed, aborting\n");
		return 78;
	}

	epause.cmd = ETHTOOL_SPAUSEPARAM;
	err = send_ioctl(ctx, &epause);
	if (err) {
		perror("Cannot set device pause parameters");
		return 79;
	}

	return 0;
}

static int do_sring(struct cmd_context *ctx)
{
	struct ethtool_ringparam ering;
	int gring_changed = 0;
	s32 ring_rx_wanted = -1;
	s32 ring_rx_mini_wanted = -1;
	s32 ring_rx_jumbo_wanted = -1;
	s32 ring_tx_wanted = -1;
	struct cmdline_info cmdline_ring[] = {
		{ "rx", CMDL_S32, &ring_rx_wanted, &ering.rx_pending },
		{ "rx-mini", CMDL_S32, &ring_rx_mini_wanted,
		  &ering.rx_mini_pending },
		{ "rx-jumbo", CMDL_S32, &ring_rx_jumbo_wanted,
		  &ering.rx_jumbo_pending },
		{ "tx", CMDL_S32, &ring_tx_wanted, &ering.tx_pending },
	};
	int err, changed = 0;

	parse_generic_cmdline(ctx, &gring_changed,
			      cmdline_ring, ARRAY_SIZE(cmdline_ring));

	ering.cmd = ETHTOOL_GRINGPARAM;
	err = send_ioctl(ctx, &ering);
	if (err) {
		perror("Cannot get device ring settings");
		return 76;
	}

	do_generic_set(cmdline_ring, ARRAY_SIZE(cmdline_ring), &changed);

	if (!changed) {
		fprintf(stderr, "no ring parameters changed, aborting\n");
		return 80;
	}

	ering.cmd = ETHTOOL_SRINGPARAM;
	err = send_ioctl(ctx, &ering);
	if (err) {
		perror("Cannot set device ring parameters");
		return 81;
	}

	return 0;
}

static int do_gring(struct cmd_context *ctx)
{
	struct ethtool_ringparam ering;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Ring parameters for %s:\n", ctx->devname);

	ering.cmd = ETHTOOL_GRINGPARAM;
	err = send_ioctl(ctx, &ering);
	if (err == 0) {
		err = dump_ring(&ering);
		if (err)
			return err;
	} else {
		perror("Cannot get device ring settings");
		return 76;
	}

	return 0;
}

static int do_schannels(struct cmd_context *ctx)
{
	struct ethtool_channels echannels;
	int gchannels_changed;
	s32 channels_rx_wanted = -1;
	s32 channels_tx_wanted = -1;
	s32 channels_other_wanted = -1;
	s32 channels_combined_wanted = -1;
	struct cmdline_info cmdline_channels[] = {
		{ "rx", CMDL_S32, &channels_rx_wanted, &echannels.rx_count },
		{ "tx", CMDL_S32, &channels_tx_wanted, &echannels.tx_count },
		{ "other", CMDL_S32, &channels_other_wanted,
		  &echannels.other_count },
		{ "combined", CMDL_S32, &channels_combined_wanted,
		  &echannels.combined_count },
	};
	int err, changed = 0;

	parse_generic_cmdline(ctx, &gchannels_changed,
			      cmdline_channels, ARRAY_SIZE(cmdline_channels));

	echannels.cmd = ETHTOOL_GCHANNELS;
	err = send_ioctl(ctx, &echannels);
	if (err) {
		perror("Cannot get device channel parameters");
		return 1;
	}

	do_generic_set(cmdline_channels, ARRAY_SIZE(cmdline_channels),
			&changed);

	if (!changed) {
		fprintf(stderr, "no channel parameters changed, aborting\n");
		fprintf(stderr, "current values: tx %u rx %u other %u"
			" combined %u\n", echannels.rx_count,
			echannels.tx_count, echannels.other_count,
			echannels.combined_count);
		return 1;
	}

	echannels.cmd = ETHTOOL_SCHANNELS;
	err = send_ioctl(ctx, &echannels);
	if (err) {
		perror("Cannot set device channel parameters");
		return 1;
	}

	return 0;
}

static int do_gchannels(struct cmd_context *ctx)
{
	struct ethtool_channels echannels;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Channel parameters for %s:\n", ctx->devname);

	echannels.cmd = ETHTOOL_GCHANNELS;
	err = send_ioctl(ctx, &echannels);
	if (err == 0) {
		err = dump_channels(&echannels);
		if (err)
			return err;
	} else {
		perror("Cannot get device channel parameters\n");
		return 1;
	}
	return 0;

}

static int do_gcoalesce(struct cmd_context *ctx)
{
	struct ethtool_coalesce ecoal;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Coalesce parameters for %s:\n", ctx->devname);

	ecoal.cmd = ETHTOOL_GCOALESCE;
	err = send_ioctl(ctx, &ecoal);
	if (err == 0) {
		err = dump_coalesce(&ecoal);
		if (err)
			return err;
	} else {
		perror("Cannot get device coalesce settings");
		return 82;
	}

	return 0;
}

static int do_scoalesce(struct cmd_context *ctx)
{
	struct ethtool_coalesce ecoal;
	int gcoalesce_changed = 0;
	s32 coal_stats_wanted = -1;
	int coal_adaptive_rx_wanted = -1;
	int coal_adaptive_tx_wanted = -1;
	s32 coal_sample_rate_wanted = -1;
	s32 coal_pkt_rate_low_wanted = -1;
	s32 coal_pkt_rate_high_wanted = -1;
	s32 coal_rx_usec_wanted = -1;
	s32 coal_rx_frames_wanted = -1;
	s32 coal_rx_usec_irq_wanted = -1;
	s32 coal_rx_frames_irq_wanted = -1;
	s32 coal_tx_usec_wanted = -1;
	s32 coal_tx_frames_wanted = -1;
	s32 coal_tx_usec_irq_wanted = -1;
	s32 coal_tx_frames_irq_wanted = -1;
	s32 coal_rx_usec_low_wanted = -1;
	s32 coal_rx_frames_low_wanted = -1;
	s32 coal_tx_usec_low_wanted = -1;
	s32 coal_tx_frames_low_wanted = -1;
	s32 coal_rx_usec_high_wanted = -1;
	s32 coal_rx_frames_high_wanted = -1;
	s32 coal_tx_usec_high_wanted = -1;
	s32 coal_tx_frames_high_wanted = -1;
	struct cmdline_info cmdline_coalesce[] = {
		{ "adaptive-rx", CMDL_BOOL, &coal_adaptive_rx_wanted,
		  &ecoal.use_adaptive_rx_coalesce },
		{ "adaptive-tx", CMDL_BOOL, &coal_adaptive_tx_wanted,
		  &ecoal.use_adaptive_tx_coalesce },
		{ "sample-interval", CMDL_S32, &coal_sample_rate_wanted,
		  &ecoal.rate_sample_interval },
		{ "stats-block-usecs", CMDL_S32, &coal_stats_wanted,
		  &ecoal.stats_block_coalesce_usecs },
		{ "pkt-rate-low", CMDL_S32, &coal_pkt_rate_low_wanted,
		  &ecoal.pkt_rate_low },
		{ "pkt-rate-high", CMDL_S32, &coal_pkt_rate_high_wanted,
		  &ecoal.pkt_rate_high },
		{ "rx-usecs", CMDL_S32, &coal_rx_usec_wanted,
		  &ecoal.rx_coalesce_usecs },
		{ "rx-frames", CMDL_S32, &coal_rx_frames_wanted,
		  &ecoal.rx_max_coalesced_frames },
		{ "rx-usecs-irq", CMDL_S32, &coal_rx_usec_irq_wanted,
		  &ecoal.rx_coalesce_usecs_irq },
		{ "rx-frames-irq", CMDL_S32, &coal_rx_frames_irq_wanted,
		  &ecoal.rx_max_coalesced_frames_irq },
		{ "tx-usecs", CMDL_S32, &coal_tx_usec_wanted,
		  &ecoal.tx_coalesce_usecs },
		{ "tx-frames", CMDL_S32, &coal_tx_frames_wanted,
		  &ecoal.tx_max_coalesced_frames },
		{ "tx-usecs-irq", CMDL_S32, &coal_tx_usec_irq_wanted,
		  &ecoal.tx_coalesce_usecs_irq },
		{ "tx-frames-irq", CMDL_S32, &coal_tx_frames_irq_wanted,
		  &ecoal.tx_max_coalesced_frames_irq },
		{ "rx-usecs-low", CMDL_S32, &coal_rx_usec_low_wanted,
		  &ecoal.rx_coalesce_usecs_low },
		{ "rx-frames-low", CMDL_S32, &coal_rx_frames_low_wanted,
		  &ecoal.rx_max_coalesced_frames_low },
		{ "tx-usecs-low", CMDL_S32, &coal_tx_usec_low_wanted,
		  &ecoal.tx_coalesce_usecs_low },
		{ "tx-frames-low", CMDL_S32, &coal_tx_frames_low_wanted,
		  &ecoal.tx_max_coalesced_frames_low },
		{ "rx-usecs-high", CMDL_S32, &coal_rx_usec_high_wanted,
		  &ecoal.rx_coalesce_usecs_high },
		{ "rx-frames-high", CMDL_S32, &coal_rx_frames_high_wanted,
		  &ecoal.rx_max_coalesced_frames_high },
		{ "tx-usecs-high", CMDL_S32, &coal_tx_usec_high_wanted,
		  &ecoal.tx_coalesce_usecs_high },
		{ "tx-frames-high", CMDL_S32, &coal_tx_frames_high_wanted,
		  &ecoal.tx_max_coalesced_frames_high },
	};
	int err, changed = 0;

	parse_generic_cmdline(ctx, &gcoalesce_changed,
			      cmdline_coalesce, ARRAY_SIZE(cmdline_coalesce));

	ecoal.cmd = ETHTOOL_GCOALESCE;
	err = send_ioctl(ctx, &ecoal);
	if (err) {
		perror("Cannot get device coalesce settings");
		return 76;
	}

	do_generic_set(cmdline_coalesce, ARRAY_SIZE(cmdline_coalesce),
		       &changed);

	if (!changed) {
		fprintf(stderr, "no coalesce parameters changed, aborting\n");
		return 80;
	}

	ecoal.cmd = ETHTOOL_SCOALESCE;
	err = send_ioctl(ctx, &ecoal);
	if (err) {
		perror("Cannot set device coalesce parameters");
		return 81;
	}

	return 0;
}

static struct feature_state *
get_features(struct cmd_context *ctx, const struct feature_defs *defs)
{
	struct feature_state *state;
	struct ethtool_value eval;
	int err, allfail = 1;
	u32 value;
	int i;

	state = malloc(sizeof(*state) +
		       FEATURE_BITS_TO_BLOCKS(defs->n_features) *
		       sizeof(state->features.features[0]));
	if (!state)
		return NULL;

	state->off_flags = 0;

	for (i = 0; i < ARRAY_SIZE(off_flag_def); i++) {
		value = off_flag_def[i].value;
		if (!off_flag_def[i].get_cmd)
			continue;
		eval.cmd = off_flag_def[i].get_cmd;
		err = send_ioctl(ctx, &eval);
		if (err) {
			fprintf(stderr,
				"Cannot get device %s settings: %m\n",
				off_flag_def[i].long_name);
		} else {
			if (eval.data)
				state->off_flags |= value;
			allfail = 0;
		}
	}

	eval.cmd = ETHTOOL_GFLAGS;
	err = send_ioctl(ctx, &eval);
	if (err) {
		perror("Cannot get device flags");
	} else {
		state->off_flags |= eval.data & ETH_FLAG_EXT_MASK;
		allfail = 0;
	}

	if (defs->n_features) {
		state->features.cmd = ETHTOOL_GFEATURES;
		state->features.size = FEATURE_BITS_TO_BLOCKS(defs->n_features);
		err = send_ioctl(ctx, &state->features);
		if (err)
			perror("Cannot get device generic features");
		else
			allfail = 0;
	}

	if (allfail) {
		free(state);
		return NULL;
	}

	return state;
}

static int do_gfeatures(struct cmd_context *ctx)
{
	struct feature_defs *defs;
	struct feature_state *features;

	if (ctx->argc != 0)
		exit_bad_args();

	defs = get_feature_defs(ctx);
	if (!defs) {
		perror("Cannot get device feature names");
		return 1;
	}

	fprintf(stdout, "Features for %s:\n", ctx->devname);

	features = get_features(ctx, defs);
	if (!features) {
		fprintf(stdout, "no feature info available\n");
		free(defs);
		return 1;
	}

	dump_features(defs, features, NULL);
	free(features);
	free(defs);
	return 0;
}

static int do_sfeatures(struct cmd_context *ctx)
{
	struct feature_defs *defs;
	int any_changed = 0, any_mismatch = 0;
	u32 off_flags_wanted = 0;
	u32 off_flags_mask = 0;
	struct ethtool_sfeatures *efeatures;
	struct cmdline_info *cmdline_features;
	struct feature_state *old_state, *new_state;
	struct ethtool_value eval;
	int err, rc;
	int i, j;

	defs = get_feature_defs(ctx);
	if (!defs) {
		perror("Cannot get device feature names");
		return 1;
	}
	if (defs->n_features) {
		efeatures = malloc(sizeof(*efeatures) +
				   FEATURE_BITS_TO_BLOCKS(defs->n_features) *
				   sizeof(efeatures->features[0]));
		if (!efeatures) {
			perror("Cannot parse arguments");
			rc = 1;
			goto err;
		}
		efeatures->cmd = ETHTOOL_SFEATURES;
		efeatures->size = FEATURE_BITS_TO_BLOCKS(defs->n_features);
		memset(efeatures->features, 0,
		       FEATURE_BITS_TO_BLOCKS(defs->n_features) *
		       sizeof(efeatures->features[0]));
	} else {
		efeatures = NULL;
	}

	/* Generate cmdline_info for legacy flags and kernel-named
	 * features, and parse our arguments.
	 */
	cmdline_features = calloc(ARRAY_SIZE(off_flag_def) + defs->n_features,
				  sizeof(cmdline_features[0]));
	if (!cmdline_features) {
		perror("Cannot parse arguments");
		rc = 1;
		goto err;
	}
	for (i = 0; i < ARRAY_SIZE(off_flag_def); i++)
		flag_to_cmdline_info(off_flag_def[i].short_name,
				     off_flag_def[i].value,
				     &off_flags_wanted, &off_flags_mask,
				     &cmdline_features[i]);
	for (i = 0; i < defs->n_features; i++)
		flag_to_cmdline_info(
			defs->def[i].name, FEATURE_FIELD_FLAG(i),
			&FEATURE_WORD(efeatures->features, i, requested),
			&FEATURE_WORD(efeatures->features, i, valid),
			&cmdline_features[ARRAY_SIZE(off_flag_def) + i]);
	parse_generic_cmdline(ctx, &any_changed, cmdline_features,
			      ARRAY_SIZE(off_flag_def) + defs->n_features);
	free(cmdline_features);

	if (!any_changed) {
		fprintf(stdout, "no features changed\n");
		rc = 0;
		goto err;
	}

	old_state = get_features(ctx, defs);
	if (!old_state) {
		rc = 1;
		goto err;
	}

	if (efeatures) {
		/* For each offload that the user specified, update any
		 * related features that the user did not specify and that
		 * are not fixed.  Warn if all related features are fixed.
		 */
		for (i = 0; i < ARRAY_SIZE(off_flag_def); i++) {
			int fixed = 1;

			if (!(off_flags_mask & off_flag_def[i].value))
				continue;

			for (j = 0; j < defs->n_features; j++) {
				if (defs->def[j].off_flag_index != i ||
				    !FEATURE_BIT_IS_SET(
					    old_state->features.features,
					    j, available) ||
				    FEATURE_BIT_IS_SET(
					    old_state->features.features,
					    j, never_changed))
					continue;

				fixed = 0;
				if (!FEATURE_BIT_IS_SET(efeatures->features,
							j, valid)) {
					FEATURE_BIT_SET(efeatures->features,
							j, valid);
					if (off_flags_wanted &
					    off_flag_def[i].value)
						FEATURE_BIT_SET(
							efeatures->features,
							j, requested);
				}
			}

			if (fixed)
				fprintf(stderr, "Cannot change %s\n",
					off_flag_def[i].long_name);
		}

		err = send_ioctl(ctx, efeatures);
		if (err < 0) {
			perror("Cannot set device feature settings");
			rc = 1;
			goto err;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(off_flag_def); i++) {
			if (!off_flag_def[i].set_cmd)
				continue;
			if (off_flags_mask & off_flag_def[i].value) {
				eval.cmd = off_flag_def[i].set_cmd;
				eval.data = !!(off_flags_wanted &
					       off_flag_def[i].value);
				err = send_ioctl(ctx, &eval);
				if (err) {
					fprintf(stderr,
						"Cannot set device %s settings: %m\n",
						off_flag_def[i].long_name);
					rc = 1;
					goto err;
				}
			}
		}

		if (off_flags_mask & ETH_FLAG_EXT_MASK) {
			eval.cmd = ETHTOOL_SFLAGS;
			eval.data = (old_state->off_flags & ~off_flags_mask &
				     ETH_FLAG_EXT_MASK);
			eval.data |= off_flags_wanted & ETH_FLAG_EXT_MASK;

			err = send_ioctl(ctx, &eval);
			if (err) {
				perror("Cannot set device flag settings");
				rc = 92;
				goto err;
			}
		}
	}

	/* Compare new state with requested state */
	new_state = get_features(ctx, defs);
	if (!new_state) {
		rc = 1;
		goto err;
	}
	any_changed = new_state->off_flags != old_state->off_flags;
	any_mismatch = (new_state->off_flags !=
			((old_state->off_flags & ~off_flags_mask) |
			 off_flags_wanted));
	for (i = 0; i < FEATURE_BITS_TO_BLOCKS(defs->n_features); i++) {
		if (new_state->features.features[i].active !=
		    old_state->features.features[i].active)
			any_changed = 1;
		if (new_state->features.features[i].active !=
		    ((old_state->features.features[i].active &
		      ~efeatures->features[i].valid) |
		     efeatures->features[i].requested))
			any_mismatch = 1;
	}
	if (any_mismatch) {
		if (!any_changed) {
			fprintf(stderr,
				"Could not change any device features\n");
			rc = 1;
			goto err;
		}
		printf("Actual changes:\n");
		dump_features(defs, new_state, old_state);
	}

	rc = 0;

err:
	free(defs);
	if (efeatures)
		free(efeatures);
	return rc;
}

static struct ethtool_link_usettings *
do_ioctl_glinksettings(struct cmd_context *ctx)
{
	int err;
	struct {
		struct ethtool_link_settings req;
		__u32 link_mode_data[3 * ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32];
	} ecmd;
	struct ethtool_link_usettings *link_usettings;
	unsigned int u32_offs;

	/* Handshake with kernel to determine number of words for link
	 * mode bitmaps. When requested number of bitmap words is not
	 * the one expected by kernel, the latter returns the integer
	 * opposite of what it is expecting. We request length 0 below
	 * (aka. invalid bitmap length) to get this info.
	 */
	memset(&ecmd, 0, sizeof(ecmd));
	ecmd.req.cmd = ETHTOOL_GLINKSETTINGS;
	err = send_ioctl(ctx, &ecmd);
	if (err < 0)
		return NULL;

	/* see above: we expect a strictly negative value from kernel.
	 */
	if (ecmd.req.link_mode_masks_nwords >= 0
	    || ecmd.req.cmd != ETHTOOL_GLINKSETTINGS)
		return NULL;

	/* got the real ecmd.req.link_mode_masks_nwords,
	 * now send the real request
	 */
	ecmd.req.cmd = ETHTOOL_GLINKSETTINGS;
	ecmd.req.link_mode_masks_nwords = -ecmd.req.link_mode_masks_nwords;
	err = send_ioctl(ctx, &ecmd);
	if (err < 0)
		return NULL;

	if (ecmd.req.link_mode_masks_nwords <= 0
	    || ecmd.req.cmd != ETHTOOL_GLINKSETTINGS)
		return NULL;

	/* Convert to usettings struct */
	link_usettings = calloc(1, sizeof(*link_usettings));
	if (link_usettings == NULL)
		return NULL;

	/* keep transceiver 0 */
	memcpy(&link_usettings->base, &ecmd.req, sizeof(link_usettings->base));

	/* copy link mode bitmaps */
	u32_offs = 0;
	memcpy(link_usettings->link_modes.supported,
	       &ecmd.link_mode_data[u32_offs],
	       4 * ecmd.req.link_mode_masks_nwords);

	u32_offs += ecmd.req.link_mode_masks_nwords;
	memcpy(link_usettings->link_modes.advertising,
	       &ecmd.link_mode_data[u32_offs],
	       4 * ecmd.req.link_mode_masks_nwords);

	u32_offs += ecmd.req.link_mode_masks_nwords;
	memcpy(link_usettings->link_modes.lp_advertising,
	       &ecmd.link_mode_data[u32_offs],
	       4 * ecmd.req.link_mode_masks_nwords);

	return link_usettings;
}

static int
do_ioctl_slinksettings(struct cmd_context *ctx,
		       const struct ethtool_link_usettings *link_usettings)
{
	struct {
		struct ethtool_link_settings req;
		__u32 link_mode_data[3 * ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32];
	} ecmd;
	unsigned int u32_offs;

	/* refuse to send ETHTOOL_SLINKSETTINGS ioctl if
	 * link_usettings was retrieved with ETHTOOL_GSET
	 */
	if (link_usettings->base.cmd != ETHTOOL_GLINKSETTINGS)
		return -1;

	/* refuse to send ETHTOOL_SLINKSETTINGS ioctl if deprecated fields
	 * were set
	 */
	if (link_usettings->deprecated.transceiver)
		return -1;

	if (link_usettings->base.link_mode_masks_nwords <= 0)
		return -1;

	memcpy(&ecmd.req, &link_usettings->base, sizeof(ecmd.req));
	ecmd.req.cmd = ETHTOOL_SLINKSETTINGS;

	/* copy link mode bitmaps */
	u32_offs = 0;
	memcpy(&ecmd.link_mode_data[u32_offs],
	       link_usettings->link_modes.supported,
	       4 * ecmd.req.link_mode_masks_nwords);

	u32_offs += ecmd.req.link_mode_masks_nwords;
	memcpy(&ecmd.link_mode_data[u32_offs],
	       link_usettings->link_modes.advertising,
	       4 * ecmd.req.link_mode_masks_nwords);

	u32_offs += ecmd.req.link_mode_masks_nwords;
	memcpy(&ecmd.link_mode_data[u32_offs],
	       link_usettings->link_modes.lp_advertising,
	       4 * ecmd.req.link_mode_masks_nwords);

	return send_ioctl(ctx, &ecmd);
}

static struct ethtool_link_usettings *
do_ioctl_gset(struct cmd_context *ctx)
{
	int err;
	struct ethtool_cmd ecmd;
	struct ethtool_link_usettings *link_usettings;

	memset(&ecmd, 0, sizeof(ecmd));
	ecmd.cmd = ETHTOOL_GSET;
	err = send_ioctl(ctx, &ecmd);
	if (err < 0)
		return NULL;

	link_usettings = calloc(1, sizeof(*link_usettings));
	if (link_usettings == NULL)
		return NULL;

	/* remember that ETHTOOL_GSET was used */
	link_usettings->base.cmd = ETHTOOL_GSET;

	link_usettings->base.link_mode_masks_nwords = 1;
	link_usettings->link_modes.supported[0] = ecmd.supported;
	link_usettings->link_modes.advertising[0] = ecmd.advertising;
	link_usettings->link_modes.lp_advertising[0] = ecmd.lp_advertising;
	link_usettings->base.speed = ethtool_cmd_speed(&ecmd);
	link_usettings->base.duplex = ecmd.duplex;
	link_usettings->base.port = ecmd.port;
	link_usettings->base.phy_address = ecmd.phy_address;
	link_usettings->deprecated.transceiver = ecmd.transceiver;
	link_usettings->base.autoneg = ecmd.autoneg;
	link_usettings->base.mdio_support = ecmd.mdio_support;
	/* ignored (fully deprecated): maxrxpkt, maxtxpkt */
	link_usettings->base.eth_tp_mdix = ecmd.eth_tp_mdix;
	link_usettings->base.eth_tp_mdix_ctrl = ecmd.eth_tp_mdix_ctrl;

	return link_usettings;
}

static bool ethtool_link_mode_is_backward_compatible(const u32 *mask)
{
	unsigned int i;

	for (i = 1; i < ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32; ++i)
		if (mask[i])
			return false;

	return true;
}

static int
do_ioctl_sset(struct cmd_context *ctx,
	      const struct ethtool_link_usettings *link_usettings)
{
	struct ethtool_cmd ecmd;

	/* refuse to send ETHTOOL_SSET ioctl if link_usettings was
	 * retrieved with ETHTOOL_GLINKSETTINGS
	 */
	if (link_usettings->base.cmd != ETHTOOL_GSET)
		return -1;

	if (link_usettings->base.link_mode_masks_nwords <= 0)
		return -1;

	/* refuse to sset if any bit > 31 is set */
	if (!ethtool_link_mode_is_backward_compatible(
		    link_usettings->link_modes.supported))
		return -1;
	if (!ethtool_link_mode_is_backward_compatible(
		    link_usettings->link_modes.advertising))
		return -1;
	if (!ethtool_link_mode_is_backward_compatible(
		    link_usettings->link_modes.lp_advertising))
		return -1;

	memset(&ecmd, 0, sizeof(ecmd));
	ecmd.cmd = ETHTOOL_SSET;

	ecmd.supported = link_usettings->link_modes.supported[0];
	ecmd.advertising = link_usettings->link_modes.advertising[0];
	ecmd.lp_advertising = link_usettings->link_modes.lp_advertising[0];
	ethtool_cmd_speed_set(&ecmd, link_usettings->base.speed);
	ecmd.duplex = link_usettings->base.duplex;
	ecmd.port = link_usettings->base.port;
	ecmd.phy_address = link_usettings->base.phy_address;
	ecmd.transceiver = link_usettings->deprecated.transceiver;
	ecmd.autoneg = link_usettings->base.autoneg;
	ecmd.mdio_support = link_usettings->base.mdio_support;
	/* ignored (fully deprecated): maxrxpkt, maxtxpkt */
	ecmd.eth_tp_mdix = link_usettings->base.eth_tp_mdix;
	ecmd.eth_tp_mdix_ctrl = link_usettings->base.eth_tp_mdix_ctrl;
	return send_ioctl(ctx, &ecmd);
}

static int do_gset(struct cmd_context *ctx)
{
	int err;
	struct ethtool_link_usettings *link_usettings;
	struct ethtool_wolinfo wolinfo;
	struct ethtool_value edata;
	int allfail = 1;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Settings for %s:\n", ctx->devname);

	link_usettings = do_ioctl_glinksettings(ctx);
	if (link_usettings == NULL)
		link_usettings = do_ioctl_gset(ctx);
	if (link_usettings != NULL) {
		err = dump_link_usettings(link_usettings);
		free(link_usettings);
		if (err)
			return err;
		allfail = 0;
	} else if (errno != EOPNOTSUPP) {
		perror("Cannot get device settings");
	}

	wolinfo.cmd = ETHTOOL_GWOL;
	err = send_ioctl(ctx, &wolinfo);
	if (err == 0) {
		err = dump_wol(&wolinfo);
		if (err)
			return err;
		allfail = 0;
	} else if (errno != EOPNOTSUPP) {
		perror("Cannot get wake-on-lan settings");
	}

	edata.cmd = ETHTOOL_GMSGLVL;
	err = send_ioctl(ctx, &edata);
	if (err == 0) {
		fprintf(stdout, "	Current message level: 0x%08x (%d)\n"
			"			       ",
			edata.data, edata.data);
		print_flags(flags_msglvl, ARRAY_SIZE(flags_msglvl),
			    edata.data);
		fprintf(stdout, "\n");
		allfail = 0;
	} else if (errno != EOPNOTSUPP) {
		perror("Cannot get message level");
	}

	edata.cmd = ETHTOOL_GLINK;
	err = send_ioctl(ctx, &edata);
	if (err == 0) {
		fprintf(stdout, "	Link detected: %s\n",
			edata.data ? "yes":"no");
		allfail = 0;
	} else if (errno != EOPNOTSUPP) {
		perror("Cannot get link status");
	}

	if (allfail) {
		fprintf(stdout, "No data available\n");
		return 75;
	}
	return 0;
}

static int do_sset(struct cmd_context *ctx)
{
	int speed_wanted = -1;
	int duplex_wanted = -1;
	int port_wanted = -1;
	int mdix_wanted = -1;
	int autoneg_wanted = -1;
	int phyad_wanted = -1;
	int xcvr_wanted = -1;
	u32 *full_advertising_wanted = NULL;
	u32 *advertising_wanted = NULL;
	ETHTOOL_DECLARE_LINK_MODE_MASK(mask_full_advertising_wanted);
	ETHTOOL_DECLARE_LINK_MODE_MASK(mask_advertising_wanted);
	int gset_changed = 0; /* did anything in GSET change? */
	u32 wol_wanted = 0;
	int wol_change = 0;
	u8 sopass_wanted[SOPASS_MAX];
	int sopass_change = 0;
	int gwol_changed = 0; /* did anything in GWOL change? */
	int msglvl_changed = 0;
	u32 msglvl_wanted = 0;
	u32 msglvl_mask = 0;
	struct cmdline_info cmdline_msglvl[ARRAY_SIZE(flags_msglvl)];
	int argc = ctx->argc;
	char **argp = ctx->argp;
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(flags_msglvl); i++)
		flag_to_cmdline_info(flags_msglvl[i].name,
				     flags_msglvl[i].value,
				     &msglvl_wanted, &msglvl_mask,
				     &cmdline_msglvl[i]);

	for (i = 0; i < argc; i++) {
		if (!strcmp(argp[i], "speed")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			speed_wanted = get_int(argp[i],10);
		} else if (!strcmp(argp[i], "duplex")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (!strcmp(argp[i], "half"))
				duplex_wanted = DUPLEX_HALF;
			else if (!strcmp(argp[i], "full"))
				duplex_wanted = DUPLEX_FULL;
			else
				exit_bad_args();
		} else if (!strcmp(argp[i], "port")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (!strcmp(argp[i], "tp"))
				port_wanted = PORT_TP;
			else if (!strcmp(argp[i], "aui"))
				port_wanted = PORT_AUI;
			else if (!strcmp(argp[i], "bnc"))
				port_wanted = PORT_BNC;
			else if (!strcmp(argp[i], "mii"))
				port_wanted = PORT_MII;
			else if (!strcmp(argp[i], "fibre"))
				port_wanted = PORT_FIBRE;
			else
				exit_bad_args();
		} else if (!strcmp(argp[i], "mdix")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (!strcmp(argp[i], "auto"))
				mdix_wanted = ETH_TP_MDI_AUTO;
			else if (!strcmp(argp[i], "on"))
				mdix_wanted = ETH_TP_MDI_X;
			else if (!strcmp(argp[i], "off"))
				mdix_wanted = ETH_TP_MDI;
			else
				exit_bad_args();
		} else if (!strcmp(argp[i], "autoneg")) {
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (!strcmp(argp[i], "on")) {
				gset_changed = 1;
				autoneg_wanted = AUTONEG_ENABLE;
			} else if (!strcmp(argp[i], "off")) {
				gset_changed = 1;
				autoneg_wanted = AUTONEG_DISABLE;
			} else {
				exit_bad_args();
			}
		} else if (!strcmp(argp[i], "advertise")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (parse_hex_u32_bitmap(
				    argp[i],
				    ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBITS,
				    mask_full_advertising_wanted))
				exit_bad_args();
			full_advertising_wanted = mask_full_advertising_wanted;
		} else if (!strcmp(argp[i], "phyad")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			phyad_wanted = get_int(argp[i], 0);
		} else if (!strcmp(argp[i], "xcvr")) {
			gset_changed = 1;
			i += 1;
			if (i >= argc)
				exit_bad_args();
			if (!strcmp(argp[i], "internal"))
				xcvr_wanted = XCVR_INTERNAL;
			else if (!strcmp(argp[i], "external"))
				xcvr_wanted = XCVR_EXTERNAL;
			else
				exit_bad_args();
		} else if (!strcmp(argp[i], "wol")) {
			gwol_changed = 1;
			i++;
			if (i >= argc)
				exit_bad_args();
			if (parse_wolopts(argp[i], &wol_wanted) < 0)
				exit_bad_args();
			wol_change = 1;
		} else if (!strcmp(argp[i], "sopass")) {
			gwol_changed = 1;
			i++;
			if (i >= argc)
				exit_bad_args();
			get_mac_addr(argp[i], sopass_wanted);
			sopass_change = 1;
		} else if (!strcmp(argp[i], "msglvl")) {
			i++;
			if (i >= argc)
				exit_bad_args();
			if (isdigit((unsigned char)argp[i][0])) {
				msglvl_changed = 1;
				msglvl_mask = ~0;
				msglvl_wanted =
					get_uint_range(argp[i], 0,
						       0xffffffff);
			} else {
				ctx->argc -= i;
				ctx->argp += i;
				parse_generic_cmdline(
					ctx, &msglvl_changed,
					cmdline_msglvl,
					ARRAY_SIZE(cmdline_msglvl));
				break;
			}
		} else {
			exit_bad_args();
		}
	}

	if (full_advertising_wanted == NULL) {
		/* User didn't supply a full advertisement bitfield:
		 * construct one from the specified speed and duplex.
		 */
		int adv_bit = -1;

		if (speed_wanted == SPEED_10 && duplex_wanted == DUPLEX_HALF)
			adv_bit = ETHTOOL_LINK_MODE_10baseT_Half_BIT;
		else if (speed_wanted == SPEED_10 &&
			 duplex_wanted == DUPLEX_FULL)
			adv_bit = ETHTOOL_LINK_MODE_10baseT_Full_BIT;
		else if (speed_wanted == SPEED_100 &&
			 duplex_wanted == DUPLEX_HALF)
			adv_bit = ETHTOOL_LINK_MODE_100baseT_Half_BIT;
		else if (speed_wanted == SPEED_100 &&
			 duplex_wanted == DUPLEX_FULL)
			adv_bit = ETHTOOL_LINK_MODE_100baseT_Full_BIT;
		else if (speed_wanted == SPEED_1000 &&
			 duplex_wanted == DUPLEX_HALF)
			adv_bit = ETHTOOL_LINK_MODE_1000baseT_Half_BIT;
		else if (speed_wanted == SPEED_1000 &&
			 duplex_wanted == DUPLEX_FULL)
			adv_bit = ETHTOOL_LINK_MODE_1000baseT_Full_BIT;
		else if (speed_wanted == SPEED_2500 &&
			 duplex_wanted == DUPLEX_FULL)
			adv_bit = ETHTOOL_LINK_MODE_2500baseX_Full_BIT;
		else if (speed_wanted == SPEED_10000 &&
			 duplex_wanted == DUPLEX_FULL)
			adv_bit = ETHTOOL_LINK_MODE_10000baseT_Full_BIT;

		if (adv_bit >= 0) {
			advertising_wanted = mask_advertising_wanted;
			ethtool_link_mode_zero(advertising_wanted);
			ethtool_link_mode_set_bit(
				adv_bit, advertising_wanted);
		}
		/* otherwise: auto negotiate without forcing,
		 * all supported speed will be assigned below
		 */
	}

	if (gset_changed) {
		struct ethtool_link_usettings *link_usettings;

		link_usettings = do_ioctl_glinksettings(ctx);
		if (link_usettings == NULL)
			link_usettings = do_ioctl_gset(ctx);
		if (link_usettings == NULL) {
			perror("Cannot get current device settings");
			err = -1;
		} else {
			/* Change everything the user specified. */
			if (speed_wanted != -1)
				link_usettings->base.speed = speed_wanted;
			if (duplex_wanted != -1)
				link_usettings->base.duplex = duplex_wanted;
			if (port_wanted != -1)
				link_usettings->base.port = port_wanted;
			if (mdix_wanted != -1) {
				/* check driver supports MDI-X */
				if (link_usettings->base.eth_tp_mdix_ctrl
				    != ETH_TP_MDI_INVALID)
					link_usettings->base.eth_tp_mdix_ctrl
						= mdix_wanted;
				else
					fprintf(stderr,
						"setting MDI not supported\n");
			}
			if (autoneg_wanted != -1)
				link_usettings->base.autoneg = autoneg_wanted;
			if (phyad_wanted != -1)
				link_usettings->base.phy_address = phyad_wanted;
			if (xcvr_wanted != -1)
				link_usettings->deprecated.transceiver
					= xcvr_wanted;
			/* XXX If the user specified speed or duplex
			 * then we should mask the advertised modes
			 * accordingly.  For now, warn that we aren't
			 * doing that.
			 */
			if ((speed_wanted != -1 || duplex_wanted != -1) &&
			    link_usettings->base.autoneg &&
			    advertising_wanted == NULL) {
				fprintf(stderr, "Cannot advertise");
				if (speed_wanted >= 0)
					fprintf(stderr, " speed %d",
						speed_wanted);
				if (duplex_wanted >= 0)
					fprintf(stderr, " duplex %s",
						duplex_wanted ? 
						"full" : "half");
				fprintf(stderr,	"\n");
			}
			if (autoneg_wanted == AUTONEG_ENABLE &&
			    advertising_wanted == NULL) {
				unsigned int i;

				/* Auto negotiation enabled, but with
				 * unspecified speed and duplex: enable all
				 * supported speeds and duplexes.
				 */
				ethtool_link_mode_for_each_u32(i) {
					u32 sup = link_usettings->link_modes.supported[i];
					u32 *adv = link_usettings->link_modes.advertising + i;

					*adv = ((*adv & ~all_advertised_modes[i])
						| (sup & all_advertised_modes[i]));
				}

				/* If driver supports unknown flags, we cannot
				 * be sure that we enable all link modes.
				 */
				ethtool_link_mode_for_each_u32(i) {
					u32 sup = link_usettings->link_modes.supported[i];

					if ((sup & all_advertised_flags[i]) != sup) {
						fprintf(stderr, "Driver supports one or more unknown flags\n");
						break;
					}
				}
			} else if (advertising_wanted != NULL) {
				unsigned int i;

				/* Enable all requested modes */
				ethtool_link_mode_for_each_u32(i) {
					u32 *adv = link_usettings->link_modes.advertising + i;

					*adv = ((*adv & ~all_advertised_modes[i])
						| advertising_wanted[i]);
				}
			} else if (full_advertising_wanted != NULL) {
				ethtool_link_mode_copy(
					link_usettings->link_modes.advertising,
					full_advertising_wanted);
			}

			/* Try to perform the update. */
			if (link_usettings->base.cmd == ETHTOOL_GLINKSETTINGS)
				err = do_ioctl_slinksettings(ctx,
							     link_usettings);
			else
				err = do_ioctl_sset(ctx, link_usettings);
			free(link_usettings);
			if (err < 0)
				perror("Cannot set new settings");
		}
		if (err < 0) {
			if (speed_wanted != -1)
				fprintf(stderr, "  not setting speed\n");
			if (duplex_wanted != -1)
				fprintf(stderr, "  not setting duplex\n");
			if (port_wanted != -1)
				fprintf(stderr, "  not setting port\n");
			if (autoneg_wanted != -1)
				fprintf(stderr, "  not setting autoneg\n");
			if (phyad_wanted != -1)
				fprintf(stderr, "  not setting phy_address\n");
			if (xcvr_wanted != -1)
				fprintf(stderr, "  not setting transceiver\n");
			if (mdix_wanted != -1)
				fprintf(stderr, "  not setting mdix\n");
		}
	}

	if (gwol_changed) {
		struct ethtool_wolinfo wol;

		wol.cmd = ETHTOOL_GWOL;
		err = send_ioctl(ctx, &wol);
		if (err < 0) {
			perror("Cannot get current wake-on-lan settings");
		} else {
			/* Change everything the user specified. */
			if (wol_change) {
				wol.wolopts = wol_wanted;
			}
			if (sopass_change) {
				int i;
				for (i = 0; i < SOPASS_MAX; i++) {
					wol.sopass[i] = sopass_wanted[i];
				}
			}

			/* Try to perform the update. */
			wol.cmd = ETHTOOL_SWOL;
			err = send_ioctl(ctx, &wol);
			if (err < 0)
				perror("Cannot set new wake-on-lan settings");
		}
		if (err < 0) {
			if (wol_change)
				fprintf(stderr, "  not setting wol\n");
			if (sopass_change)
				fprintf(stderr, "  not setting sopass\n");
		}
	}

	if (msglvl_changed) {
		struct ethtool_value edata;

		edata.cmd = ETHTOOL_GMSGLVL;
		err = send_ioctl(ctx, &edata);
		if (err < 0) {
			perror("Cannot get msglvl");
		} else {
			edata.cmd = ETHTOOL_SMSGLVL;
			edata.data = ((edata.data & ~msglvl_mask) |
				      msglvl_wanted);
			err = send_ioctl(ctx, &edata);
			if (err < 0)
				perror("Cannot set new msglvl");
		}
	}

	return 0;
}

static int do_gregs(struct cmd_context *ctx)
{
	int gregs_changed = 0;
	int gregs_dump_raw = 0;
	int gregs_dump_hex = 0;
	char *gregs_dump_file = NULL;
	struct cmdline_info cmdline_gregs[] = {
		{ "raw", CMDL_BOOL, &gregs_dump_raw, NULL },
		{ "hex", CMDL_BOOL, &gregs_dump_hex, NULL },
		{ "file", CMDL_STR, &gregs_dump_file, NULL },
	};
	int err;
	struct ethtool_drvinfo drvinfo;
	struct ethtool_regs *regs;

	parse_generic_cmdline(ctx, &gregs_changed,
			      cmdline_gregs, ARRAY_SIZE(cmdline_gregs));

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	err = send_ioctl(ctx, &drvinfo);
	if (err < 0) {
		perror("Cannot get driver information");
		return 72;
	}

	regs = calloc(1, sizeof(*regs)+drvinfo.regdump_len);
	if (!regs) {
		perror("Cannot allocate memory for register dump");
		return 73;
	}
	regs->cmd = ETHTOOL_GREGS;
	regs->len = drvinfo.regdump_len;
	err = send_ioctl(ctx, regs);
	if (err < 0) {
		perror("Cannot get register dump");
		free(regs);
		return 74;
	}

	if (!gregs_dump_raw && gregs_dump_file != NULL) {
		/* overwrite reg values from file dump */
		FILE *f = fopen(gregs_dump_file, "r");
		struct stat st;
		size_t nread;

		if (!f || fstat(fileno(f), &st) < 0) {
			fprintf(stderr, "Can't open '%s': %s\n",
				gregs_dump_file, strerror(errno));
			free(regs);
			return 75;
		}

		regs = realloc(regs, sizeof(*regs) + st.st_size);
		regs->len = st.st_size;
		nread = fread(regs->data, regs->len, 1, f);
		fclose(f);
		if (nread != 1) {
			free(regs);
			return 75;
		}
        }

	if (dump_regs(gregs_dump_raw, gregs_dump_hex,
		      &drvinfo, regs) < 0) {
		fprintf(stderr, "Cannot dump registers\n");
		free(regs);
		return 75;
	}
	free(regs);

	return 0;
}

static int do_nway_rst(struct cmd_context *ctx)
{
	struct ethtool_value edata;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	edata.cmd = ETHTOOL_NWAY_RST;
	err = send_ioctl(ctx, &edata);
	if (err < 0)
		perror("Cannot restart autonegotiation");

	return err;
}

static int do_geeprom(struct cmd_context *ctx)
{
	int geeprom_changed = 0;
	int geeprom_dump_raw = 0;
	u32 geeprom_offset = 0;
	u32 geeprom_length = -1;
	struct cmdline_info cmdline_geeprom[] = {
		{ "offset", CMDL_U32, &geeprom_offset, NULL },
		{ "length", CMDL_U32, &geeprom_length, NULL },
		{ "raw", CMDL_BOOL, &geeprom_dump_raw, NULL },
	};
	int err;
	struct ethtool_drvinfo drvinfo;
	struct ethtool_eeprom *eeprom;

	parse_generic_cmdline(ctx, &geeprom_changed,
			      cmdline_geeprom, ARRAY_SIZE(cmdline_geeprom));

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	err = send_ioctl(ctx, &drvinfo);
	if (err < 0) {
		perror("Cannot get driver information");
		return 74;
	}

	if (geeprom_length == -1)
		geeprom_length = drvinfo.eedump_len;

	if (drvinfo.eedump_len < geeprom_offset + geeprom_length)
		geeprom_length = drvinfo.eedump_len - geeprom_offset;

	eeprom = calloc(1, sizeof(*eeprom)+geeprom_length);
	if (!eeprom) {
		perror("Cannot allocate memory for EEPROM data");
		return 75;
	}
	eeprom->cmd = ETHTOOL_GEEPROM;
	eeprom->len = geeprom_length;
	eeprom->offset = geeprom_offset;
	err = send_ioctl(ctx, eeprom);
	if (err < 0) {
		perror("Cannot get EEPROM data");
		free(eeprom);
		return 74;
	}
	err = dump_eeprom(geeprom_dump_raw, &drvinfo, eeprom);
	free(eeprom);

	return err;
}

static int do_seeprom(struct cmd_context *ctx)
{
	int seeprom_changed = 0;
	u32 seeprom_magic = 0;
	u32 seeprom_length = -1;
	u32 seeprom_offset = 0;
	u8 seeprom_value = 0;
	int seeprom_value_seen = 0;
	struct cmdline_info cmdline_seeprom[] = {
		{ "magic", CMDL_U32, &seeprom_magic, NULL },
		{ "offset", CMDL_U32, &seeprom_offset, NULL },
		{ "length", CMDL_U32, &seeprom_length, NULL },
		{ "value", CMDL_U8, &seeprom_value, NULL,
		  0, &seeprom_value_seen },
	};
	int err;
	struct ethtool_drvinfo drvinfo;
	struct ethtool_eeprom *eeprom;

	parse_generic_cmdline(ctx, &seeprom_changed,
			      cmdline_seeprom, ARRAY_SIZE(cmdline_seeprom));

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	err = send_ioctl(ctx, &drvinfo);
	if (err < 0) {
		perror("Cannot get driver information");
		return 74;
	}

	if (seeprom_value_seen)
		seeprom_length = 1;

	if (seeprom_length == -1)
		seeprom_length = drvinfo.eedump_len;

	if (drvinfo.eedump_len < seeprom_offset + seeprom_length) {
		fprintf(stderr, "offset & length out of bounds\n");
		return 1;
	}

	eeprom = calloc(1, sizeof(*eeprom)+seeprom_length);
	if (!eeprom) {
		perror("Cannot allocate memory for EEPROM data");
		return 75;
	}

	eeprom->cmd = ETHTOOL_SEEPROM;
	eeprom->len = seeprom_length;
	eeprom->offset = seeprom_offset;
	eeprom->magic = seeprom_magic;
	eeprom->data[0] = seeprom_value;

	/* Multi-byte write: read input from stdin */
	if (!seeprom_value_seen) {
		if (fread(eeprom->data, eeprom->len, 1, stdin) != 1) {
			fprintf(stderr, "not enough data from stdin\n");
			free(eeprom);
			return 75;
		}
		if ((fgetc(stdin) != EOF) || !feof(stdin)) {
			fprintf(stderr, "too much data from stdin\n");
			free(eeprom);
			return 75;
		}
	}

	err = send_ioctl(ctx, eeprom);
	if (err < 0) {
		perror("Cannot set EEPROM data");
		err = 87;
	}
	free(eeprom);

	return err;
}

static int do_test(struct cmd_context *ctx)
{
	enum {
		ONLINE=0,
		OFFLINE,
		EXTERNAL_LB,
	} test_type;
	int err;
	struct ethtool_test *test;
	struct ethtool_gstrings *strings;

	if (ctx->argc > 1)
		exit_bad_args();
	if (ctx->argc == 1) {
		if (!strcmp(ctx->argp[0], "online")) {
			test_type = ONLINE;
	 	} else if (!strcmp(*ctx->argp, "offline")) {
			test_type = OFFLINE;
		} else if (!strcmp(*ctx->argp, "external_lb")) {
			test_type = EXTERNAL_LB;
		} else {
			exit_bad_args();
		}
	} else {
		test_type = OFFLINE;
	}

	strings = get_stringset(ctx, ETH_SS_TEST,
				offsetof(struct ethtool_drvinfo, testinfo_len),
				1);
	if (!strings) {
		perror("Cannot get strings");
		return 74;
	}

	test = calloc(1, sizeof(*test) + strings->len * sizeof(u64));
	if (!test) {
		perror("Cannot allocate memory for test info");
		free(strings);
		return 73;
	}
	memset(test->data, 0, strings->len * sizeof(u64));
	test->cmd = ETHTOOL_TEST;
	test->len = strings->len;
	if (test_type == EXTERNAL_LB)
		test->flags = (ETH_TEST_FL_OFFLINE | ETH_TEST_FL_EXTERNAL_LB);
	else if (test_type == OFFLINE)
		test->flags = ETH_TEST_FL_OFFLINE;
	else
		test->flags = 0;
	err = send_ioctl(ctx, test);
	if (err < 0) {
		perror("Cannot test");
		free (test);
		free(strings);
		return 74;
	}

	err = dump_test(test, strings);
	free(test);
	free(strings);

	return err;
}

static int do_phys_id(struct cmd_context *ctx)
{
	int err;
	struct ethtool_value edata;
	int phys_id_time;

	if (ctx->argc > 1)
		exit_bad_args();
	if (ctx->argc == 1)
		phys_id_time = get_int(*ctx->argp, 0);
	else
		phys_id_time = 0;

	edata.cmd = ETHTOOL_PHYS_ID;
	edata.data = phys_id_time;
	err = send_ioctl(ctx, &edata);
	if (err < 0)
		perror("Cannot identify NIC");

	return err;
}

static int do_gstats(struct cmd_context *ctx, int cmd, int stringset,
		    const char *name)
{
	struct ethtool_gstrings *strings;
	struct ethtool_stats *stats;
	unsigned int n_stats, sz_stats, i;
	int err;

	if (ctx->argc != 0)
		exit_bad_args();

	strings = get_stringset(ctx, stringset,
				offsetof(struct ethtool_drvinfo, n_stats),
				0);
	if (!strings) {
		perror("Cannot get stats strings information");
		return 96;
	}

	n_stats = strings->len;
	if (n_stats < 1) {
		fprintf(stderr, "no stats available\n");
		free(strings);
		return 94;
	}

	sz_stats = n_stats * sizeof(u64);

	stats = calloc(1, sz_stats + sizeof(struct ethtool_stats));
	if (!stats) {
		fprintf(stderr, "no memory available\n");
		free(strings);
		return 95;
	}

	stats->cmd = cmd;
	stats->n_stats = n_stats;
	err = send_ioctl(ctx, stats);
	if (err < 0) {
		perror("Cannot get stats information");
		free(strings);
		free(stats);
		return 97;
	}

	/* todo - pretty-print the strings per-driver */
	fprintf(stdout, "%s statistics:\n", name);
	for (i = 0; i < n_stats; i++) {
		fprintf(stdout, "     %.*s: %llu\n",
			ETH_GSTRING_LEN,
			&strings->data[i * ETH_GSTRING_LEN],
			stats->data[i]);
	}
	free(strings);
	free(stats);

	return 0;
}

static int do_gnicstats(struct cmd_context *ctx)
{
	return do_gstats(ctx, ETHTOOL_GSTATS, ETH_SS_STATS, "NIC");
}

static int do_gphystats(struct cmd_context *ctx)
{
	return do_gstats(ctx, ETHTOOL_GPHYSTATS, ETH_SS_PHY_STATS, "PHY");
}

static int do_srxntuple(struct cmd_context *ctx,
			struct ethtool_rx_flow_spec *rx_rule_fs);

static int do_srxclass(struct cmd_context *ctx)
{
	int err;

	if (ctx->argc < 2)
		exit_bad_args();

	if (ctx->argc == 3 && !strcmp(ctx->argp[0], "rx-flow-hash")) {
		int rx_fhash_set;
		u32 rx_fhash_val;
		struct ethtool_rxnfc nfccmd;

		rx_fhash_set = rxflow_str_to_type(ctx->argp[1]);
		if (!rx_fhash_set)
			exit_bad_args();
		if (parse_rxfhashopts(ctx->argp[2], &rx_fhash_val) < 0)
			exit_bad_args();

		nfccmd.cmd = ETHTOOL_SRXFH;
		nfccmd.flow_type = rx_fhash_set;
		nfccmd.data = rx_fhash_val;

		err = send_ioctl(ctx, &nfccmd);
		if (err < 0)
			perror("Cannot change RX network flow hashing options");
	} else if (!strcmp(ctx->argp[0], "flow-type")) {	
		struct ethtool_rx_flow_spec rx_rule_fs;

		ctx->argc--;
		ctx->argp++;
		if (rxclass_parse_ruleopts(ctx, &rx_rule_fs) < 0)
			exit_bad_args();

		/* attempt to add rule via N-tuple specifier */
		err = do_srxntuple(ctx, &rx_rule_fs);
		if (!err)
			return 0;

		/* attempt to add rule via network flow classifier */
		err = rxclass_rule_ins(ctx, &rx_rule_fs);
		if (err < 0) {
			fprintf(stderr, "Cannot insert"
				" classification rule\n");
			return 1;
		}
	} else if (!strcmp(ctx->argp[0], "delete")) {
		int rx_class_rule_del =
			get_uint_range(ctx->argp[1], 0, INT_MAX);

		err = rxclass_rule_del(ctx, rx_class_rule_del);

		if (err < 0) {
			fprintf(stderr, "Cannot delete"
				" classification rule\n");
			return 1;
		}
	} else {
		exit_bad_args();
	}

	return 0;
}

static int do_grxclass(struct cmd_context *ctx)
{
	struct ethtool_rxnfc nfccmd;
	int err;

	if (ctx->argc == 2 && !strcmp(ctx->argp[0], "rx-flow-hash")) {
		int rx_fhash_get;

		rx_fhash_get = rxflow_str_to_type(ctx->argp[1]);
		if (!rx_fhash_get)
			exit_bad_args();

		nfccmd.cmd = ETHTOOL_GRXFH;
		nfccmd.flow_type = rx_fhash_get;
		err = send_ioctl(ctx, &nfccmd);
		if (err < 0)
			perror("Cannot get RX network flow hashing options");
		else
			dump_rxfhash(rx_fhash_get, nfccmd.data);
	} else if (ctx->argc == 2 && !strcmp(ctx->argp[0], "rule")) {
		int rx_class_rule_get =
			get_uint_range(ctx->argp[1], 0, INT_MAX);

		err = rxclass_rule_get(ctx, rx_class_rule_get);
		if (err < 0)
			fprintf(stderr, "Cannot get RX classification rule\n");
	} else if (ctx->argc == 0) {
		nfccmd.cmd = ETHTOOL_GRXRINGS;
		err = send_ioctl(ctx, &nfccmd);
		if (err < 0)
			perror("Cannot get RX rings");
		else
			fprintf(stdout, "%d RX rings available\n",
				(int)nfccmd.data);

		err = rxclass_rule_getall(ctx);
		if (err < 0)
			fprintf(stderr, "RX classification rule retrieval failed\n");

	} else {
		exit_bad_args();
	}

	return err ? 1 : 0;
}

static void print_indir_table(struct cmd_context *ctx,
			      struct ethtool_rxnfc *ring_count,
			      u32 indir_size, u32 *indir)
{
	u32 i;

	printf("RX flow hash indirection table for %s with %llu RX ring(s):\n",
	       ctx->devname, ring_count->data);

	if (!indir_size)
		printf("Operation not supported\n");

	for (i = 0; i < indir_size; i++) {
		if (i % 8 == 0)
			printf("%5u: ", i);
		printf(" %5u", indir[i]);
		if (i % 8 == 7 || i == indir_size - 1)
			fputc('\n', stdout);
	}
}

static int do_grxfhindir(struct cmd_context *ctx,
			 struct ethtool_rxnfc *ring_count)
{
	struct ethtool_rxfh_indir indir_head;
	struct ethtool_rxfh_indir *indir;
	int err;

	indir_head.cmd = ETHTOOL_GRXFHINDIR;
	indir_head.size = 0;
	err = send_ioctl(ctx, &indir_head);
	if (err < 0) {
		perror("Cannot get RX flow hash indirection table size");
		return 1;
	}

	indir = malloc(sizeof(*indir) +
		       indir_head.size * sizeof(*indir->ring_index));
	if (!indir) {
		perror("Cannot allocate memory for indirection table");
		return 1;
	}

	indir->cmd = ETHTOOL_GRXFHINDIR;
	indir->size = indir_head.size;
	err = send_ioctl(ctx, indir);
	if (err < 0) {
		perror("Cannot get RX flow hash indirection table");
		free(indir);
		return 1;
	}

	print_indir_table(ctx, ring_count, indir->size, indir->ring_index);

	free(indir);
	return 0;
}

static int do_grxfh(struct cmd_context *ctx)
{
	struct ethtool_rxfh rss_head = {0};
	struct ethtool_rxnfc ring_count;
	struct ethtool_rxfh *rss;
	u32 i, indir_bytes;
	char *hkey;
	int err;

	ring_count.cmd = ETHTOOL_GRXRINGS;
	err = send_ioctl(ctx, &ring_count);
	if (err < 0) {
		perror("Cannot get RX ring count");
		return 1;
	}

	rss_head.cmd = ETHTOOL_GRSSH;
	err = send_ioctl(ctx, &rss_head);
	if (err < 0 && errno == EOPNOTSUPP) {
		return do_grxfhindir(ctx, &ring_count);
	} else if (err < 0) {
		perror("Cannot get RX flow hash indir size and/or key size");
		return 1;
	}

	rss = calloc(1, sizeof(*rss) +
			rss_head.indir_size * sizeof(rss_head.rss_config[0]) +
			rss_head.key_size);
	if (!rss) {
		perror("Cannot allocate memory for RX flow hash config");
		return 1;
	}

	rss->cmd = ETHTOOL_GRSSH;
	rss->indir_size = rss_head.indir_size;
	rss->key_size = rss_head.key_size;
	err = send_ioctl(ctx, rss);
	if (err < 0) {
		perror("Cannot get RX flow hash configuration");
		free(rss);
		return 1;
	}

	print_indir_table(ctx, &ring_count, rss->indir_size, rss->rss_config);

	indir_bytes = rss->indir_size * sizeof(rss->rss_config[0]);
	hkey = ((char *)rss->rss_config + indir_bytes);

	printf("RSS hash key:\n");
	if (!rss->key_size)
		printf("Operation not supported\n");

	for (i = 0; i < rss->key_size; i++) {
		if (i == (rss->key_size - 1))
			printf("%02x\n", (u8) hkey[i]);
		else
			printf("%02x:", (u8) hkey[i]);
	}

	free(rss);
	return 0;
}

static int fill_indir_table(u32 *indir_size, u32 *indir, int rxfhindir_default,
			    int rxfhindir_equal, char **rxfhindir_weight,
			    u32 num_weights)
{
	u32 i;
	if (rxfhindir_equal) {
		for (i = 0; i < *indir_size; i++)
			indir[i] = i % rxfhindir_equal;
	} else if (rxfhindir_weight) {
		u32 j, weight, sum = 0, partial = 0;

		for (j = 0; j < num_weights; j++) {
			weight = get_u32(rxfhindir_weight[j], 0);
			sum += weight;
		}

		if (sum == 0) {
			fprintf(stderr,
				"At least one weight must be non-zero\n");
			return 2;
		}

		if (sum > *indir_size) {
			fprintf(stderr,
				"Total weight exceeds the size of the "
				"indirection table\n");
			return 2;
		}

		j = -1;
		for (i = 0; i < *indir_size; i++) {
			while (i >= (*indir_size) * partial / sum) {
				j += 1;
				weight = get_u32(rxfhindir_weight[j], 0);
				partial += weight;
			}
			indir[i] = j;
		}
	} else if (rxfhindir_default) {
		/* "*indir_size == 0" ==> reset indir to default */
		*indir_size = 0;
	} else {
		*indir_size = ETH_RXFH_INDIR_NO_CHANGE;
	}

	return 0;
}

static int do_srxfhindir(struct cmd_context *ctx, int rxfhindir_default,
			 int rxfhindir_equal, char **rxfhindir_weight,
			 u32 num_weights)
{
	struct ethtool_rxfh_indir indir_head;
	struct ethtool_rxfh_indir *indir;
	int err;

	indir_head.cmd = ETHTOOL_GRXFHINDIR;
	indir_head.size = 0;
	err = send_ioctl(ctx, &indir_head);
	if (err < 0) {
		perror("Cannot get RX flow hash indirection table size");
		return 1;
	}

	indir = malloc(sizeof(*indir) +
		       indir_head.size * sizeof(*indir->ring_index));

	if (!indir) {
		perror("Cannot allocate memory for indirection table");
		return 1;
	}

	indir->cmd = ETHTOOL_SRXFHINDIR;
	indir->size = indir_head.size;

	if (fill_indir_table(&indir->size, indir->ring_index,
			     rxfhindir_default, rxfhindir_equal,
			     rxfhindir_weight, num_weights)) {
		free(indir);
		return 1;
	}

	err = send_ioctl(ctx, indir);
	if (err < 0) {
		perror("Cannot set RX flow hash indirection table");
		free(indir);
		return 1;
	}

	free(indir);
	return 0;
}

static int do_srxfh(struct cmd_context *ctx)
{
	struct ethtool_rxfh rss_head = {0};
	struct ethtool_rxfh *rss;
	struct ethtool_rxnfc ring_count;
	int rxfhindir_equal = 0, rxfhindir_default = 0;
	char **rxfhindir_weight = NULL;
	char *rxfhindir_key = NULL;
	char *hkey = NULL;
	int err = 0;
	u32 arg_num = 0, indir_bytes = 0;
	u32 entry_size = sizeof(rss_head.rss_config[0]);
	u32 num_weights = 0;

	if (ctx->argc < 1)
		exit_bad_args();

	while (arg_num < ctx->argc) {
		if (!strcmp(ctx->argp[arg_num], "equal")) {
			++arg_num;
			rxfhindir_equal = get_int_range(ctx->argp[arg_num],
							0, 1, INT_MAX);
			++arg_num;
		} else if (!strcmp(ctx->argp[arg_num], "weight")) {
			++arg_num;
			rxfhindir_weight = ctx->argp + arg_num;
			while (arg_num < ctx->argc &&
			       isdigit((unsigned char)ctx->argp[arg_num][0])) {
				++arg_num;
				++num_weights;
			}
			if (!num_weights)
				exit_bad_args();
		} else if (!strcmp(ctx->argp[arg_num], "hkey")) {
			++arg_num;
			rxfhindir_key = ctx->argp[arg_num];
			if (!rxfhindir_key)
				exit_bad_args();
			++arg_num;
		} else if (!strcmp(ctx->argp[arg_num], "default")) {
			++arg_num;
			rxfhindir_default = 1;
		} else {
			exit_bad_args();
		}
	}

	if (rxfhindir_equal && rxfhindir_weight) {
		fprintf(stderr,
			"Equal and weight options are mutually exclusive\n");
		return 1;
	}

	if (rxfhindir_equal && rxfhindir_default) {
		fprintf(stderr,
			"Equal and default options are mutually exclusive\n");
		return 1;
	}

	if (rxfhindir_weight && rxfhindir_default) {
		fprintf(stderr,
			"Weight and default options are mutually exclusive\n");
		return 1;
	}

	ring_count.cmd = ETHTOOL_GRXRINGS;
	err = send_ioctl(ctx, &ring_count);
	if (err < 0) {
		perror("Cannot get RX ring count");
		return 1;
	}

	rss_head.cmd = ETHTOOL_GRSSH;
	err = send_ioctl(ctx, &rss_head);
	if (err < 0 && errno == EOPNOTSUPP && !rxfhindir_key) {
		return do_srxfhindir(ctx, rxfhindir_default, rxfhindir_equal,
				     rxfhindir_weight, num_weights);
	} else if (err < 0) {
		perror("Cannot get RX flow hash indir size and key size");
		return 1;
	}

	if (rxfhindir_key) {
		err = parse_hkey(&hkey, rss_head.key_size,
				 rxfhindir_key);
		if (err)
			return err;
	}

	if (rxfhindir_equal || rxfhindir_weight)
		indir_bytes = rss_head.indir_size * entry_size;

	rss = calloc(1, sizeof(*rss) + indir_bytes + rss_head.key_size);
	if (!rss) {
		perror("Cannot allocate memory for RX flow hash config");
		return 1;
	}
	rss->cmd = ETHTOOL_SRSSH;
	rss->indir_size = rss_head.indir_size;
	rss->key_size = rss_head.key_size;

	if (fill_indir_table(&rss->indir_size, rss->rss_config, rxfhindir_default,
			     rxfhindir_equal, rxfhindir_weight, num_weights)) {
		err = 1;
		goto free;
	}

	if (hkey)
		memcpy((char *)rss->rss_config + indir_bytes,
		       hkey, rss->key_size);
	else
		rss->key_size = 0;

	err = send_ioctl(ctx, rss);
	if (err < 0) {
		perror("Cannot set RX flow hash configuration");
		err = 1;
	}

free:
	if (hkey)
		free(hkey);

	free(rss);
	return err;
}

static int do_flash(struct cmd_context *ctx)
{
	char *flash_file;
	int flash_region;
	struct ethtool_flash efl;
	int err;

	if (ctx->argc < 1 || ctx->argc > 2)
		exit_bad_args();
	flash_file = ctx->argp[0];
	if (ctx->argc == 2) {
		flash_region = strtol(ctx->argp[1], NULL, 0);
		if (flash_region < 0)
			exit_bad_args();
	} else {
		flash_region = -1;
	}

	if (strlen(flash_file) > ETHTOOL_FLASH_MAX_FILENAME - 1) {
		fprintf(stdout, "Filename too long\n");
		return 99;
	}

	efl.cmd = ETHTOOL_FLASHDEV;
	strcpy(efl.data, flash_file);

	if (flash_region < 0)
		efl.region = ETHTOOL_FLASH_ALL_REGIONS;
	else
		efl.region = flash_region;

	err = send_ioctl(ctx, &efl);
	if (err < 0)
		perror("Flashing failed");

	return err;
}

static int do_permaddr(struct cmd_context *ctx)
{
	int i, err;
	struct ethtool_perm_addr *epaddr;

	epaddr = malloc(sizeof(struct ethtool_perm_addr) + MAX_ADDR_LEN);
	if (!epaddr) {
		perror("Cannot allocate memory for operation");
		return 1;
	}

	epaddr->cmd = ETHTOOL_GPERMADDR;
	epaddr->size = MAX_ADDR_LEN;

	err = send_ioctl(ctx, epaddr);
	if (err < 0)
		perror("Cannot read permanent address");
	else {
		printf("Permanent address:");
		for (i = 0; i < epaddr->size; i++)
			printf("%c%02x", (i == 0) ? ' ' : ':',
			       epaddr->data[i]);
		printf("\n");
	}
	free(epaddr);

	return err;
}

static bool flow_type_is_ntuple_supported(__u32 flow_type)
{
	switch (flow_type) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_USER_FLOW:
	case ETHER_FLOW:
		return true;
	default:
		return false;
	}
}

static int flow_spec_to_ntuple(struct ethtool_rx_flow_spec *fsp,
			       struct ethtool_rx_ntuple_flow_spec *ntuple)
{
	size_t i;

	/* verify location is not specified */
	if (fsp->location != RX_CLS_LOC_ANY)
		return -1;

	/* destination MAC address in L3/L4 rules is not supported by ntuple */
	if (fsp->flow_type & FLOW_MAC_EXT)
		return -1;

	/* verify ring cookie can transfer to action */
	if (fsp->ring_cookie > INT_MAX && fsp->ring_cookie < (u64)(-2))
		return -1;

	/* verify only one field is setting data field */
	if ((fsp->flow_type & FLOW_EXT) &&
	    (fsp->m_ext.data[0] || fsp->m_ext.data[1]) &&
	    fsp->m_ext.vlan_etype)
		return -1;

	/* IPv6 flow types are not supported by ntuple */
	if (!flow_type_is_ntuple_supported(fsp->flow_type & ~FLOW_EXT))
		return -1;

	/* Set entire ntuple to ~0 to guarantee all masks are set */
	memset(ntuple, ~0, sizeof(*ntuple));

	/* set non-filter values */
	ntuple->flow_type = fsp->flow_type;
	ntuple->action = fsp->ring_cookie;

	/*
	 * Copy over header union, they are identical in layout however
	 * the ntuple union contains additional padding on the end
	 */
	memcpy(&ntuple->h_u, &fsp->h_u, sizeof(fsp->h_u));

	/*
	 * The same rule mentioned above applies to the mask union.  However,
	 * in addition we need to invert the mask bits to match the ntuple
	 * mask which is 1 for masked, versus 0 for masked as seen in nfc.
	 */
	memcpy(&ntuple->m_u, &fsp->m_u, sizeof(fsp->m_u));
	for (i = 0; i < sizeof(fsp->m_u); i++)
		ntuple->m_u.hdata[i] ^= 0xFF;

	/* copy extended fields */
	if (fsp->flow_type & FLOW_EXT) {
		ntuple->vlan_tag =
			ntohs(fsp->h_ext.vlan_tci);
		ntuple->vlan_tag_mask =
			~ntohs(fsp->m_ext.vlan_tci);
		if (fsp->m_ext.vlan_etype) {
			/*
			 * vlan_etype and user data are mutually exclusive
			 * in ntuple configuration as they occupy the same
			 * space.
			 */
			if (fsp->m_ext.data[0] || fsp->m_ext.data[1])
				return -1;
			ntuple->data =
				ntohl(fsp->h_ext.vlan_etype);
			ntuple->data_mask =
				~(u64)ntohl(fsp->m_ext.vlan_etype);
		} else {
			ntuple->data =
				(u64)ntohl(fsp->h_ext.data[0]) << 32;
			ntuple->data |=
				(u64)ntohl(fsp->h_ext.data[1]);
			ntuple->data_mask =
				(u64)ntohl(~fsp->m_ext.data[0]) << 32;
			ntuple->data_mask |=
				(u64)ntohl(~fsp->m_ext.data[1]);
		}
	}

	/* Mask out the extended bit, because ntuple does not know it! */
	ntuple->flow_type &= ~FLOW_EXT;

	return 0;
}

static int do_srxntuple(struct cmd_context *ctx,
			struct ethtool_rx_flow_spec *rx_rule_fs)
{
	struct ethtool_rx_ntuple ntuplecmd;
	struct ethtool_value eval;
	int err;

	/* attempt to convert the flow classifier to an ntuple classifier */
	err = flow_spec_to_ntuple(rx_rule_fs, &ntuplecmd.fs);
	if (err)
		return -1;

	/*
	 * Check to see if the flag is set for N-tuple, this allows
	 * us to avoid the possible EINVAL response for the N-tuple
	 * flag not being set on the device
	 */
	eval.cmd = ETHTOOL_GFLAGS;
	err = send_ioctl(ctx, &eval);
	if (err || !(eval.data & ETH_FLAG_NTUPLE))
		return -1;

	/* send rule via N-tuple */
	ntuplecmd.cmd = ETHTOOL_SRXNTUPLE;
	err = send_ioctl(ctx, &ntuplecmd);

	/*
	 * Display error only if response is something other than op not
	 * supported.  It is possible that the interface uses the network
	 * flow classifier interface instead of N-tuple. 
	 */ 
	if (err < 0) {
		if (errno != EOPNOTSUPP)
			perror("Cannot add new rule via N-tuple");
		return -1;
	}

	return 0;
}

static int do_writefwdump(struct ethtool_dump *dump, const char *dump_file)
{
	int err = 0;
	FILE *f;
	size_t bytes;

	f = fopen(dump_file, "wb+");

	if (!f) {
		fprintf(stderr, "Can't open file %s: %s\n",
			dump_file, strerror(errno));
		return 1;
	}
	bytes = fwrite(dump->data, 1, dump->len, f);
	if (bytes != dump->len) {
		fprintf(stderr, "Can not write all of dump data\n");
		err = 1;
	}
	if (fclose(f)) {
		fprintf(stderr, "Can't close file %s: %s\n",
			dump_file, strerror(errno));
		err = 1;
	}
	return err;
}

static int do_getfwdump(struct cmd_context *ctx)
{
	u32 dump_flag;
	char *dump_file;
	int err;
	struct ethtool_dump edata;
	struct ethtool_dump *data;

	if (ctx->argc == 2 && !strcmp(ctx->argp[0], "data")) {
		dump_flag = ETHTOOL_GET_DUMP_DATA;
		dump_file = ctx->argp[1];
	} else if (ctx->argc == 0) {
		dump_flag = 0;
		dump_file = NULL;
	} else {
		exit_bad_args();
	}

	edata.cmd = ETHTOOL_GET_DUMP_FLAG;

	err = send_ioctl(ctx, &edata);
	if (err < 0) {
		perror("Can not get dump level\n");
		return 1;
	}
	if (dump_flag != ETHTOOL_GET_DUMP_DATA) {
		fprintf(stdout, "flag: %u, version: %u, length: %u\n",
			edata.flag, edata.version, edata.len);
		return 0;
	}
	data = calloc(1, offsetof(struct ethtool_dump, data) + edata.len);
	if (!data) {
		perror("Can not allocate enough memory\n");
		return 1;
	}
	data->cmd = ETHTOOL_GET_DUMP_DATA;
	data->len = edata.len;
	err = send_ioctl(ctx, data);
	if (err < 0) {
		perror("Can not get dump data\n");
		err = 1;
		goto free;
	}
	err = do_writefwdump(data, dump_file);
free:
	free(data);
	return err;
}

static int do_setfwdump(struct cmd_context *ctx)
{
	u32 dump_flag;
	int err;
	struct ethtool_dump dump;

	if (ctx->argc != 1)
		exit_bad_args();
	dump_flag = get_u32(ctx->argp[0], 0);

	dump.cmd = ETHTOOL_SET_DUMP;
	dump.flag = dump_flag;
	err = send_ioctl(ctx, &dump);
	if (err < 0) {
		perror("Can not set dump level\n");
		return 1;
	}
	return 0;
}

static int do_gprivflags(struct cmd_context *ctx)
{
	struct ethtool_gstrings *strings;
	struct ethtool_value flags;
	unsigned int i;
	int max_len = 0, cur_len, rc;

	if (ctx->argc != 0)
		exit_bad_args();

	strings = get_stringset(ctx, ETH_SS_PRIV_FLAGS,
				offsetof(struct ethtool_drvinfo, n_priv_flags),
				1);
	if (!strings) {
		perror("Cannot get private flag names");
		return 1;
	}
	if (strings->len == 0) {
		fprintf(stderr, "No private flags defined\n");
		rc = 1;
		goto err;
	}
	if (strings->len > 32) {
		/* ETHTOOL_GPFLAGS can only cover 32 flags */
		fprintf(stderr, "Only showing first 32 private flags\n");
		strings->len = 32;
	}

	flags.cmd = ETHTOOL_GPFLAGS;
	if (send_ioctl(ctx, &flags)) {
		perror("Cannot get private flags");
		rc = 1;
		goto err;
	}

	/* Find longest string and align all strings accordingly */
	for (i = 0; i < strings->len; i++) {
		cur_len = strlen((const char*)strings->data +
				 i * ETH_GSTRING_LEN);
		if (cur_len > max_len)
			max_len = cur_len;
	}

	printf("Private flags for %s:\n", ctx->devname);
	for (i = 0; i < strings->len; i++)
		printf("%-*s: %s\n",
		       max_len,
		       (const char *)strings->data + i * ETH_GSTRING_LEN,
		       (flags.data & (1U << i)) ? "on" : "off");

	rc = 0;

err:
	free(strings);
	return rc;
}

static int do_sprivflags(struct cmd_context *ctx)
{
	struct ethtool_gstrings *strings;
	struct cmdline_info *cmdline;
	struct ethtool_value flags;
	u32 wanted_flags = 0, seen_flags = 0;
	int any_changed, rc;
	unsigned int i;

	strings = get_stringset(ctx, ETH_SS_PRIV_FLAGS,
				offsetof(struct ethtool_drvinfo, n_priv_flags),
				1);
	if (!strings) {
		perror("Cannot get private flag names");
		return 1;
	}
	if (strings->len == 0) {
		fprintf(stderr, "No private flags defined\n");
		rc = 1;
		goto err;
	}
	if (strings->len > 32) {
		/* ETHTOOL_{G,S}PFLAGS can only cover 32 flags */
		fprintf(stderr, "Only setting first 32 private flags\n");
		strings->len = 32;
	}

	cmdline = calloc(strings->len, sizeof(*cmdline));
	if (!cmdline) {
		perror("Cannot parse arguments");
		rc = 1;
		goto err;
	}
	for (i = 0; i < strings->len; i++) {
		cmdline[i].name = ((const char *)strings->data +
				   i * ETH_GSTRING_LEN);
		cmdline[i].type = CMDL_FLAG;
		cmdline[i].wanted_val = &wanted_flags;
		cmdline[i].flag_val = 1U << i;
		cmdline[i].seen_val = &seen_flags;
	}
	parse_generic_cmdline(ctx, &any_changed, cmdline, strings->len);
	free(cmdline);

	flags.cmd = ETHTOOL_GPFLAGS;
	if (send_ioctl(ctx, &flags)) {
		perror("Cannot get private flags");
		rc = 1;
		goto err;
	}

	flags.cmd = ETHTOOL_SPFLAGS;
	flags.data = (flags.data & ~seen_flags) | wanted_flags;
	if (send_ioctl(ctx, &flags)) {
		perror("Cannot set private flags");
		rc = 1;
		goto err;
	}

	rc = 0;
err:
	free(strings);
	return rc;
}

static int do_tsinfo(struct cmd_context *ctx)
{
	struct ethtool_ts_info info;

	if (ctx->argc != 0)
		exit_bad_args();

	fprintf(stdout, "Time stamping parameters for %s:\n", ctx->devname);
	info.cmd = ETHTOOL_GET_TS_INFO;
	if (send_ioctl(ctx, &info)) {
		perror("Cannot get device time stamping settings");
		return -1;
	}
	dump_tsinfo(&info);
	return 0;
}

static int do_getmodule(struct cmd_context *ctx)
{
	struct ethtool_modinfo modinfo;
	struct ethtool_eeprom *eeprom;
	u32 geeprom_offset = 0;
	u32 geeprom_length = -1;
	int geeprom_changed = 0;
	int geeprom_dump_raw = 0;
	int geeprom_dump_hex = 0;
	int err;

	struct cmdline_info cmdline_geeprom[] = {
		{ "offset", CMDL_U32, &geeprom_offset, NULL },
		{ "length", CMDL_U32, &geeprom_length, NULL },
		{ "raw", CMDL_BOOL, &geeprom_dump_raw, NULL },
		{ "hex", CMDL_BOOL, &geeprom_dump_hex, NULL },
	};

	parse_generic_cmdline(ctx, &geeprom_changed,
			      cmdline_geeprom, ARRAY_SIZE(cmdline_geeprom));

	if (geeprom_dump_raw && geeprom_dump_hex) {
		printf("Hex and raw dump cannot be specified together\n");
		return 1;
	}

	modinfo.cmd = ETHTOOL_GMODULEINFO;
	err = send_ioctl(ctx, &modinfo);
	if (err < 0) {
		perror("Cannot get module EEPROM information");
		return 1;
	}

	if (geeprom_length == -1)
		geeprom_length = modinfo.eeprom_len;

	if (modinfo.eeprom_len < geeprom_offset + geeprom_length)
		geeprom_length = modinfo.eeprom_len - geeprom_offset;

	eeprom = calloc(1, sizeof(*eeprom)+geeprom_length);
	if (!eeprom) {
		perror("Cannot allocate memory for Module EEPROM data");
		return 1;
	}

	eeprom->cmd = ETHTOOL_GMODULEEEPROM;
	eeprom->len = geeprom_length;
	eeprom->offset = geeprom_offset;
	err = send_ioctl(ctx, eeprom);
	if (err < 0) {
		perror("Cannot get Module EEPROM data");
		free(eeprom);
		return 1;
	}

	/*
	 * SFF-8079 EEPROM layout contains the memory available at A0 address on
	 * the PHY EEPROM.
	 * SFF-8472 defines a virtual extension of the EEPROM, where the
	 * microcontroller on the SFP/SFP+ generates a page at the A2 address,
	 * which contains data relative to optical diagnostics.
	 * The current kernel implementation returns a blob, which contains:
	 *  - ETH_MODULE_SFF_8079 => The A0 page only.
	 *  - ETH_MODULE_SFF_8472 => The A0 and A2 page concatenated.
	 */
	if (geeprom_dump_raw) {
		fwrite(eeprom->data, 1, eeprom->len, stdout);
	} else {
		if (eeprom->offset != 0  ||
		    (eeprom->len != modinfo.eeprom_len)) {
			geeprom_dump_hex = 1;
		} else if (!geeprom_dump_hex) {
			switch (modinfo.type) {
#ifdef ETHTOOL_ENABLE_PRETTY_DUMP
			case ETH_MODULE_SFF_8079:
				sff8079_show_all(eeprom->data);
				break;
			case ETH_MODULE_SFF_8472:
				sff8079_show_all(eeprom->data);
				sff8472_show_all(eeprom->data);
				break;
			case ETH_MODULE_SFF_8436:
			case ETH_MODULE_SFF_8636:
				sff8636_show_all(eeprom->data,
						 modinfo.eeprom_len);
				break;
#endif
			default:
				geeprom_dump_hex = 1;
				break;
			}
		}
		if (geeprom_dump_hex)
			dump_hex(stdout, eeprom->data,
				 eeprom->len, eeprom->offset);
	}

	free(eeprom);

	return 0;
}

static int do_geee(struct cmd_context *ctx)
{
	struct ethtool_eee eeecmd;

	if (ctx->argc != 0)
		exit_bad_args();

	eeecmd.cmd = ETHTOOL_GEEE;
	if (send_ioctl(ctx, &eeecmd)) {
		perror("Cannot get EEE settings");
		return 1;
	}

	fprintf(stdout, "EEE Settings for %s:\n", ctx->devname);
	dump_eeecmd(&eeecmd);

	return 0;
}

static int do_seee(struct cmd_context *ctx)
{
	int adv_c = -1, lpi_c = -1, lpi_time_c = -1, eee_c = -1;
	int change = -1, change2 = 0;
	struct ethtool_eee eeecmd;
	struct cmdline_info cmdline_eee[] = {
		{ "advertise",    CMDL_U32,  &adv_c,       &eeecmd.advertised },
		{ "tx-lpi",       CMDL_BOOL, &lpi_c,   &eeecmd.tx_lpi_enabled },
		{ "tx-timer",	  CMDL_U32,  &lpi_time_c, &eeecmd.tx_lpi_timer},
		{ "eee",	  CMDL_BOOL, &eee_c,	   &eeecmd.eee_enabled},
	};

	if (ctx->argc == 0)
		exit_bad_args();

	parse_generic_cmdline(ctx, &change, cmdline_eee,
			      ARRAY_SIZE(cmdline_eee));

	eeecmd.cmd = ETHTOOL_GEEE;
	if (send_ioctl(ctx, &eeecmd)) {
		perror("Cannot get EEE settings");
		return 1;
	}

	do_generic_set(cmdline_eee, ARRAY_SIZE(cmdline_eee), &change2);

	if (change2) {

		eeecmd.cmd = ETHTOOL_SEEE;
		if (send_ioctl(ctx, &eeecmd)) {
			perror("Cannot set EEE settings");
			return 1;
		}
	}

	return 0;
}

#ifndef TEST_ETHTOOL
int send_ioctl(struct cmd_context *ctx, void *cmd)
{
	ctx->ifr.ifr_data = cmd;
	return ioctl(ctx->fd, SIOCETHTOOL, &ctx->ifr);
}
#endif

static int show_usage(struct cmd_context *ctx);

static const struct option {
	const char *opts;
	int want_device;
	int (*func)(struct cmd_context *);
	char *help;
	char *opthelp;
} args[] = {
	{ "-s|--change", 1, do_sset, "Change generic options",
	  "		[ speed %d ]\n"
	  "		[ duplex half|full ]\n"
	  "		[ port tp|aui|bnc|mii|fibre ]\n"
	  "		[ mdix auto|on|off ]\n"
	  "		[ autoneg on|off ]\n"
	  "		[ advertise %x ]\n"
	  "		[ phyad %d ]\n"
	  "		[ xcvr internal|external ]\n"
	  "		[ wol p|u|m|b|a|g|s|d... ]\n"
	  "		[ sopass %x:%x:%x:%x:%x:%x ]\n"
	  "		[ msglvl %d | msglvl type on|off ... ]\n" },
	{ "-a|--show-pause", 1, do_gpause, "Show pause options" },
	{ "-A|--pause", 1, do_spause, "Set pause options",
	  "		[ autoneg on|off ]\n"
	  "		[ rx on|off ]\n"
	  "		[ tx on|off ]\n" },
	{ "-c|--show-coalesce", 1, do_gcoalesce, "Show coalesce options" },
	{ "-C|--coalesce", 1, do_scoalesce, "Set coalesce options",
	  "		[adaptive-rx on|off]\n"
	  "		[adaptive-tx on|off]\n"
	  "		[rx-usecs N]\n"
	  "		[rx-frames N]\n"
	  "		[rx-usecs-irq N]\n"
	  "		[rx-frames-irq N]\n"
	  "		[tx-usecs N]\n"
	  "		[tx-frames N]\n"
	  "		[tx-usecs-irq N]\n"
	  "		[tx-frames-irq N]\n"
	  "		[stats-block-usecs N]\n"
	  "		[pkt-rate-low N]\n"
	  "		[rx-usecs-low N]\n"
	  "		[rx-frames-low N]\n"
	  "		[tx-usecs-low N]\n"
	  "		[tx-frames-low N]\n"
	  "		[pkt-rate-high N]\n"
	  "		[rx-usecs-high N]\n"
	  "		[rx-frames-high N]\n"
	  "		[tx-usecs-high N]\n"
	  "		[tx-frames-high N]\n"
	  "		[sample-interval N]\n" },
	{ "-g|--show-ring", 1, do_gring, "Query RX/TX ring parameters" },
	{ "-G|--set-ring", 1, do_sring, "Set RX/TX ring parameters",
	  "		[ rx N ]\n"
	  "		[ rx-mini N ]\n"
	  "		[ rx-jumbo N ]\n"
	  "		[ tx N ]\n" },
	{ "-k|--show-features|--show-offload", 1, do_gfeatures,
	  "Get state of protocol offload and other features" },
	{ "-K|--features|--offload", 1, do_sfeatures,
	  "Set protocol offload and other features",
	  "		FEATURE on|off ...\n" },
	{ "-i|--driver", 1, do_gdrv, "Show driver information" },
	{ "-d|--register-dump", 1, do_gregs, "Do a register dump",
	  "		[ raw on|off ]\n"
	  "		[ file FILENAME ]\n" },
	{ "-e|--eeprom-dump", 1, do_geeprom, "Do a EEPROM dump",
	  "		[ raw on|off ]\n"
	  "		[ offset N ]\n"
	  "		[ length N ]\n" },
	{ "-E|--change-eeprom", 1, do_seeprom,
	  "Change bytes in device EEPROM",
	  "		[ magic N ]\n"
	  "		[ offset N ]\n"
	  "		[ length N ]\n"
	  "		[ value N ]\n" },
	{ "-r|--negotiate", 1, do_nway_rst, "Restart N-WAY negotiation" },
	{ "-p|--identify", 1, do_phys_id,
	  "Show visible port identification (e.g. blinking)",
	  "               [ TIME-IN-SECONDS ]\n" },
	{ "-t|--test", 1, do_test, "Execute adapter self test",
	  "               [ online | offline | external_lb ]\n" },
	{ "-S|--statistics", 1, do_gnicstats, "Show adapter statistics" },
	{ "--phy-statistics", 1, do_gphystats,
	  "Show phy statistics" },
	{ "-n|-u|--show-nfc|--show-ntuple", 1, do_grxclass,
	  "Show Rx network flow classification options or rules",
	  "		[ rx-flow-hash tcp4|udp4|ah4|esp4|sctp4|"
	  "tcp6|udp6|ah6|esp6|sctp6 |\n"
	  "		  rule %d ]\n" },
	{ "-N|-U|--config-nfc|--config-ntuple", 1, do_srxclass,
	  "Configure Rx network flow classification options or rules",
	  "		rx-flow-hash tcp4|udp4|ah4|esp4|sctp4|"
	  "tcp6|udp6|ah6|esp6|sctp6 m|v|t|s|d|f|n|r... |\n"
	  "		flow-type ether|ip4|tcp4|udp4|sctp4|ah4|esp4|"
	  "ip6|tcp6|udp6|ah6|esp6|sctp6\n"
	  "			[ src %x:%x:%x:%x:%x:%x [m %x:%x:%x:%x:%x:%x] ]\n"
	  "			[ dst %x:%x:%x:%x:%x:%x [m %x:%x:%x:%x:%x:%x] ]\n"
	  "			[ proto %d [m %x] ]\n"
	  "			[ src-ip IP-ADDRESS [m IP-ADDRESS] ]\n"
	  "			[ dst-ip IP-ADDRESS [m IP-ADDRESS] ]\n"
	  "			[ tos %d [m %x] ]\n"
	  "			[ tclass %d [m %x] ]\n"
	  "			[ l4proto %d [m %x] ]\n"
	  "			[ src-port %d [m %x] ]\n"
	  "			[ dst-port %d [m %x] ]\n"
	  "			[ spi %d [m %x] ]\n"
	  "			[ vlan-etype %x [m %x] ]\n"
	  "			[ vlan %x [m %x] ]\n"
	  "			[ user-def %x [m %x] ]\n"
	  "			[ dst-mac %x:%x:%x:%x:%x:%x [m %x:%x:%x:%x:%x:%x] ]\n"
	  "			[ action %d ]\n"
	  "			[ loc %d]] |\n"
	  "		delete %d\n" },
	{ "-T|--show-time-stamping", 1, do_tsinfo,
	  "Show time stamping capabilities" },
	{ "-x|--show-rxfh-indir|--show-rxfh", 1, do_grxfh,
	  "Show Rx flow hash indirection and/or hash key" },
	{ "-X|--set-rxfh-indir|--rxfh", 1, do_srxfh,
	  "Set Rx flow hash indirection and/or hash key",
	  "		[ equal N | weight W0 W1 ... ]\n"
	  "		[ hkey %x:%x:%x:%x:%x:.... ]\n" },
	{ "-f|--flash", 1, do_flash,
	  "Flash firmware image from the specified file to a region on the device",
	  "               FILENAME [ REGION-NUMBER-TO-FLASH ]\n" },
	{ "-P|--show-permaddr", 1, do_permaddr,
	  "Show permanent hardware address" },
	{ "-w|--get-dump", 1, do_getfwdump,
	  "Get dump flag, data",
	  "		[ data FILENAME ]\n" },
	{ "-W|--set-dump", 1, do_setfwdump,
	  "Set dump flag of the device",
	  "		N\n"},
	{ "-l|--show-channels", 1, do_gchannels, "Query Channels" },
	{ "-L|--set-channels", 1, do_schannels, "Set Channels",
	  "               [ rx N ]\n"
	  "               [ tx N ]\n"
	  "               [ other N ]\n"
	  "               [ combined N ]\n" },
	{ "--show-priv-flags" , 1, do_gprivflags, "Query private flags" },
	{ "--set-priv-flags", 1, do_sprivflags, "Set private flags",
	  "		FLAG on|off ...\n" },
	{ "-m|--dump-module-eeprom|--module-info", 1, do_getmodule,
	  "Query/Decode Module EEPROM information and optical diagnostics if available",
	  "		[ raw on|off ]\n"
	  "		[ hex on|off ]\n"
	  "		[ offset N ]\n"
	  "		[ length N ]\n" },
	{ "--show-eee", 1, do_geee, "Show EEE settings"},
	{ "--set-eee", 1, do_seee, "Set EEE settings",
	  "		[ eee on|off ]\n"
	  "		[ advertise %x ]\n"
	  "		[ tx-lpi on|off ]\n"
	  "		[ tx-timer %d ]\n"},
	{ "-h|--help", 0, show_usage, "Show this help" },
	{ "--version", 0, do_version, "Show version number" },
	{}
};

static int show_usage(struct cmd_context *ctx)
{
	int i;

	/* ethtool -h */
	fprintf(stdout, PACKAGE " version " VERSION "\n");
	fprintf(stdout,
		"Usage:\n"
		"        ethtool DEVNAME\t"
		"Display standard information about device\n");
	for (i = 0; args[i].opts; i++) {
		fputs("        ethtool ", stdout);
		fprintf(stdout, "%s %s\t%s\n",
			args[i].opts,
			args[i].want_device ? "DEVNAME" : "\t",
			args[i].help);
		if (args[i].opthelp)
			fputs(args[i].opthelp, stdout);
	}

	return 0;
}

int main(int argc, char **argp)
{
	int (*func)(struct cmd_context *);
	int want_device;
	struct cmd_context ctx;
	int k;

	init_global_link_mode_masks();

	/* Skip command name */
	argp++;
	argc--;

	/* First argument must be either a valid option or a device
	 * name to get settings for (which we don't expect to begin
	 * with '-').
	 */
	if (argc == 0)
		exit_bad_args();
	for (k = 0; args[k].opts; k++) {
		const char *opt;
		size_t len;
		opt = args[k].opts;
		for (;;) {
			len = strcspn(opt, "|");
			if (strncmp(*argp, opt, len) == 0 &&
			    (*argp)[len] == 0) {
				argp++;
				argc--;
				func = args[k].func;
				want_device = args[k].want_device;
				goto opt_found;
			}
			if (opt[len] == 0)
				break;
			opt += len + 1;
		}
	}
	if ((*argp)[0] == '-')
		exit_bad_args();
	func = do_gset;
	want_device = 1;

opt_found:
	if (want_device) {
		ctx.devname = *argp++;
		argc--;

		if (ctx.devname == NULL)
			exit_bad_args();
		if (strlen(ctx.devname) >= IFNAMSIZ)
			exit_bad_args();

		/* Setup our control structures. */
		memset(&ctx.ifr, 0, sizeof(ctx.ifr));
		strcpy(ctx.ifr.ifr_name, ctx.devname);

		/* Open control socket. */
		ctx.fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (ctx.fd < 0)
			ctx.fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
		if (ctx.fd < 0) {
			perror("Cannot get control socket");
			return 70;
		}
	} else {
		ctx.fd = -1;
	}

	ctx.argc = argc;
	ctx.argp = argp;

	return func(&ctx);
}
