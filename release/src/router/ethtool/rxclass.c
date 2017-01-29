/*
 * Copyright (C) 2008 Sun Microsystems, Inc. All rights reserved.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <linux/sockios.h>
#include <arpa/inet.h>
#include "internal.h"

static void invert_flow_mask(struct ethtool_rx_flow_spec *fsp)
{
	size_t i;

	for (i = 0; i < sizeof(fsp->m_u); i++)
		fsp->m_u.hdata[i] ^= 0xFF;
}

static void rxclass_print_ipv4_rule(__be32 sip, __be32 sipm, __be32 dip,
				    __be32 dipm, u8 tos, u8 tosm)
{
	char sip_str[INET_ADDRSTRLEN];
	char sipm_str[INET_ADDRSTRLEN];
	char dip_str[INET_ADDRSTRLEN];
	char dipm_str[INET_ADDRSTRLEN];

	fprintf(stdout,
		"\tSrc IP addr: %s mask: %s\n"
		"\tDest IP addr: %s mask: %s\n"
		"\tTOS: 0x%x mask: 0x%x\n",
		inet_ntop(AF_INET, &sip, sip_str, INET_ADDRSTRLEN),
		inet_ntop(AF_INET, &sipm, sipm_str, INET_ADDRSTRLEN),
		inet_ntop(AF_INET, &dip, dip_str, INET_ADDRSTRLEN),
		inet_ntop(AF_INET, &dipm, dipm_str, INET_ADDRSTRLEN),
		tos, tosm);
}

static void rxclass_print_ipv6_rule(__be32 *sip, __be32 *sipm, __be32 *dip,
				    __be32 *dipm, u8 tclass, u8 tclassm)
{
	char sip_str[INET6_ADDRSTRLEN];
	char sipm_str[INET6_ADDRSTRLEN];
	char dip_str[INET6_ADDRSTRLEN];
	char dipm_str[INET6_ADDRSTRLEN];

	fprintf(stdout,
		"\tSrc IP addr: %s mask: %s\n"
		"\tDest IP addr: %s mask: %s\n"
		"\tTraffic Class: 0x%x mask: 0x%x\n",
		inet_ntop(AF_INET6, sip, sip_str, INET6_ADDRSTRLEN),
		inet_ntop(AF_INET6, sipm, sipm_str, INET6_ADDRSTRLEN),
		inet_ntop(AF_INET6, dip, dip_str, INET6_ADDRSTRLEN),
		inet_ntop(AF_INET6, dipm, dipm_str, INET6_ADDRSTRLEN),
		tclass, tclassm);
}

static void rxclass_print_nfc_spec_ext(struct ethtool_rx_flow_spec *fsp)
{
	if (fsp->flow_type & FLOW_EXT) {
		u64 data, datam;
		__u16 etype, etypem, tci, tcim;
		etype = ntohs(fsp->h_ext.vlan_etype);
		etypem = ntohs(~fsp->m_ext.vlan_etype);
		tci = ntohs(fsp->h_ext.vlan_tci);
		tcim = ntohs(~fsp->m_ext.vlan_tci);
		data = (u64)ntohl(fsp->h_ext.data[0]) << 32;
		data |= (u64)ntohl(fsp->h_ext.data[1]);
		datam = (u64)ntohl(~fsp->m_ext.data[0]) << 32;
		datam |= (u64)ntohl(~fsp->m_ext.data[1]);

		fprintf(stdout,
			"\tVLAN EtherType: 0x%x mask: 0x%x\n"
			"\tVLAN: 0x%x mask: 0x%x\n"
			"\tUser-defined: 0x%llx mask: 0x%llx\n",
			etype, etypem, tci, tcim, data, datam);
	}

	if (fsp->flow_type & FLOW_MAC_EXT) {
		unsigned char *dmac, *dmacm;

		dmac = fsp->h_ext.h_dest;
		dmacm = fsp->m_ext.h_dest;

		fprintf(stdout,
			"\tDest MAC addr: %02X:%02X:%02X:%02X:%02X:%02X"
			" mask: %02X:%02X:%02X:%02X:%02X:%02X\n",
			dmac[0], dmac[1], dmac[2], dmac[3], dmac[4],
			dmac[5], dmacm[0], dmacm[1], dmacm[2], dmacm[3],
			dmacm[4], dmacm[5]);
	}
}

static void rxclass_print_nfc_rule(struct ethtool_rx_flow_spec *fsp)
{
	unsigned char	*smac, *smacm, *dmac, *dmacm;
	__u32		flow_type;

	fprintf(stdout,	"Filter: %d\n", fsp->location);

	flow_type = fsp->flow_type & ~(FLOW_EXT | FLOW_MAC_EXT);

	invert_flow_mask(fsp);

	switch (flow_type) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
		if (flow_type == TCP_V4_FLOW)
			fprintf(stdout, "\tRule Type: TCP over IPv4\n");
		else if (flow_type == UDP_V4_FLOW)
			fprintf(stdout, "\tRule Type: UDP over IPv4\n");
		else
			fprintf(stdout, "\tRule Type: SCTP over IPv4\n");
		rxclass_print_ipv4_rule(fsp->h_u.tcp_ip4_spec.ip4src,
				     fsp->m_u.tcp_ip4_spec.ip4src,
				     fsp->h_u.tcp_ip4_spec.ip4dst,
				     fsp->m_u.tcp_ip4_spec.ip4dst,
				     fsp->h_u.tcp_ip4_spec.tos,
				     fsp->m_u.tcp_ip4_spec.tos);
		fprintf(stdout,
			"\tSrc port: %d mask: 0x%x\n"
			"\tDest port: %d mask: 0x%x\n",
			ntohs(fsp->h_u.tcp_ip4_spec.psrc),
			ntohs(fsp->m_u.tcp_ip4_spec.psrc),
			ntohs(fsp->h_u.tcp_ip4_spec.pdst),
			ntohs(fsp->m_u.tcp_ip4_spec.pdst));
		break;
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
		if (flow_type == AH_V4_FLOW)
			fprintf(stdout, "\tRule Type: IPSEC AH over IPv4\n");
		else
			fprintf(stdout, "\tRule Type: IPSEC ESP over IPv4\n");
		rxclass_print_ipv4_rule(fsp->h_u.ah_ip4_spec.ip4src,
				     fsp->m_u.ah_ip4_spec.ip4src,
				     fsp->h_u.ah_ip4_spec.ip4dst,
				     fsp->m_u.ah_ip4_spec.ip4dst,
				     fsp->h_u.ah_ip4_spec.tos,
				     fsp->m_u.ah_ip4_spec.tos);
		fprintf(stdout,
			"\tSPI: %d mask: 0x%x\n",
			ntohl(fsp->h_u.esp_ip4_spec.spi),
			ntohl(fsp->m_u.esp_ip4_spec.spi));
		break;
	case IPV4_USER_FLOW:
		fprintf(stdout, "\tRule Type: Raw IPv4\n");
		rxclass_print_ipv4_rule(fsp->h_u.usr_ip4_spec.ip4src,
				     fsp->m_u.usr_ip4_spec.ip4src,
				     fsp->h_u.usr_ip4_spec.ip4dst,
				     fsp->m_u.usr_ip4_spec.ip4dst,
				     fsp->h_u.usr_ip4_spec.tos,
				     fsp->m_u.usr_ip4_spec.tos);
		fprintf(stdout,
			"\tProtocol: %d mask: 0x%x\n"
			"\tL4 bytes: 0x%x mask: 0x%x\n",
			fsp->h_u.usr_ip4_spec.proto,
			fsp->m_u.usr_ip4_spec.proto,
			ntohl(fsp->h_u.usr_ip4_spec.l4_4_bytes),
			ntohl(fsp->m_u.usr_ip4_spec.l4_4_bytes));
		break;
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
		if (flow_type == TCP_V6_FLOW)
			fprintf(stdout, "\tRule Type: TCP over IPv6\n");
		else if (flow_type == UDP_V6_FLOW)
			fprintf(stdout, "\tRule Type: UDP over IPv6\n");
		else
			fprintf(stdout, "\tRule Type: SCTP over IPv6\n");
		rxclass_print_ipv6_rule(fsp->h_u.tcp_ip6_spec.ip6src,
				     fsp->m_u.tcp_ip6_spec.ip6src,
				     fsp->h_u.tcp_ip6_spec.ip6dst,
				     fsp->m_u.tcp_ip6_spec.ip6dst,
				     fsp->h_u.tcp_ip6_spec.tclass,
				     fsp->m_u.tcp_ip6_spec.tclass);
		fprintf(stdout,
			"\tSrc port: %d mask: 0x%x\n"
			"\tDest port: %d mask: 0x%x\n",
			ntohs(fsp->h_u.tcp_ip6_spec.psrc),
			ntohs(fsp->m_u.tcp_ip6_spec.psrc),
			ntohs(fsp->h_u.tcp_ip6_spec.pdst),
			ntohs(fsp->m_u.tcp_ip6_spec.pdst));
		break;
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
		if (flow_type == AH_V6_FLOW)
			fprintf(stdout, "\tRule Type: IPSEC AH over IPv6\n");
		else
			fprintf(stdout, "\tRule Type: IPSEC ESP over IPv6\n");
		rxclass_print_ipv6_rule(fsp->h_u.ah_ip6_spec.ip6src,
				     fsp->m_u.ah_ip6_spec.ip6src,
				     fsp->h_u.ah_ip6_spec.ip6dst,
				     fsp->m_u.ah_ip6_spec.ip6dst,
				     fsp->h_u.ah_ip6_spec.tclass,
				     fsp->m_u.ah_ip6_spec.tclass);
		fprintf(stdout,
			"\tSPI: %d mask: 0x%x\n",
			ntohl(fsp->h_u.esp_ip6_spec.spi),
			ntohl(fsp->m_u.esp_ip6_spec.spi));
		break;
	case IPV6_USER_FLOW:
		fprintf(stdout, "\tRule Type: Raw IPv6\n");
		rxclass_print_ipv6_rule(fsp->h_u.usr_ip6_spec.ip6src,
				     fsp->m_u.usr_ip6_spec.ip6src,
				     fsp->h_u.usr_ip6_spec.ip6dst,
				     fsp->m_u.usr_ip6_spec.ip6dst,
				     fsp->h_u.usr_ip6_spec.tclass,
				     fsp->m_u.usr_ip6_spec.tclass);
		fprintf(stdout,
			"\tProtocol: %d mask: 0x%x\n"
			"\tL4 bytes: 0x%x mask: 0x%x\n",
			fsp->h_u.usr_ip6_spec.l4_proto,
			fsp->m_u.usr_ip6_spec.l4_proto,
			ntohl(fsp->h_u.usr_ip6_spec.l4_4_bytes),
			ntohl(fsp->m_u.usr_ip6_spec.l4_4_bytes));
		break;
	case ETHER_FLOW:
		dmac = fsp->h_u.ether_spec.h_dest;
		dmacm = fsp->m_u.ether_spec.h_dest;
		smac = fsp->h_u.ether_spec.h_source;
		smacm = fsp->m_u.ether_spec.h_source;

		fprintf(stdout,
			"\tFlow Type: Raw Ethernet\n"
			"\tSrc MAC addr: %02X:%02X:%02X:%02X:%02X:%02X"
			" mask: %02X:%02X:%02X:%02X:%02X:%02X\n"
			"\tDest MAC addr: %02X:%02X:%02X:%02X:%02X:%02X"
			" mask: %02X:%02X:%02X:%02X:%02X:%02X\n"
			"\tEthertype: 0x%X mask: 0x%X\n",
			smac[0], smac[1], smac[2], smac[3], smac[4], smac[5],
			smacm[0], smacm[1], smacm[2], smacm[3], smacm[4],
			smacm[5], dmac[0], dmac[1], dmac[2], dmac[3], dmac[4],
			dmac[5], dmacm[0], dmacm[1], dmacm[2], dmacm[3],
			dmacm[4], dmacm[5],
			ntohs(fsp->h_u.ether_spec.h_proto),
			ntohs(fsp->m_u.ether_spec.h_proto));
		break;
	default:
		fprintf(stdout,
			"\tUnknown Flow type: %d\n", flow_type);
		break;
	}

	rxclass_print_nfc_spec_ext(fsp);

	if (fsp->ring_cookie != RX_CLS_FLOW_DISC)
		fprintf(stdout, "\tAction: Direct to queue %llu\n",
			fsp->ring_cookie);
	else
		fprintf(stdout, "\tAction: Drop\n");

	fprintf(stdout, "\n");
}

static void rxclass_print_rule(struct ethtool_rx_flow_spec *fsp)
{
	/* print the rule in this location */
	switch (fsp->flow_type & ~(FLOW_EXT | FLOW_MAC_EXT)) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_USER_FLOW:
	case ETHER_FLOW:
		rxclass_print_nfc_rule(fsp);
		break;
	case IPV4_USER_FLOW:
		if (fsp->h_u.usr_ip4_spec.ip_ver == ETH_RX_NFC_IP4)
			rxclass_print_nfc_rule(fsp);
		else /* IPv6 uses IPV6_USER_FLOW */
			fprintf(stderr, "IPV4_USER_FLOW with wrong ip_ver\n");
		break;
	default:
		fprintf(stderr, "rxclass: Unknown flow type\n");
		break;
	}
}

static int rxclass_get_dev_info(struct cmd_context *ctx, __u32 *count,
				int *driver_select)
{
	struct ethtool_rxnfc nfccmd;
	int err;

	nfccmd.cmd = ETHTOOL_GRXCLSRLCNT;
	nfccmd.data = 0;
	err = send_ioctl(ctx, &nfccmd);
	*count = nfccmd.rule_cnt;
	if (driver_select)
		*driver_select = !!(nfccmd.data & RX_CLS_LOC_SPECIAL);
	if (err < 0)
		perror("rxclass: Cannot get RX class rule count");

	return err;
}

int rxclass_rule_get(struct cmd_context *ctx, __u32 loc)
{
	struct ethtool_rxnfc nfccmd;
	int err;

	/* fetch rule from netdev */
	nfccmd.cmd = ETHTOOL_GRXCLSRULE;
	memset(&nfccmd.fs, 0, sizeof(struct ethtool_rx_flow_spec));
	nfccmd.fs.location = loc;
	err = send_ioctl(ctx, &nfccmd);
	if (err < 0) {
		perror("rxclass: Cannot get RX class rule");
		return err;
	}

	/* display rule */
	rxclass_print_rule(&nfccmd.fs);
	return err;
}

int rxclass_rule_getall(struct cmd_context *ctx)
{
	struct ethtool_rxnfc *nfccmd;
	__u32 *rule_locs;
	int err, i;
	__u32 count;

	/* determine rule count */
	err = rxclass_get_dev_info(ctx, &count, NULL);
	if (err < 0)
		return err;

	fprintf(stdout, "Total %d rules\n\n", count);

	/* alloc memory for request of location list */
	nfccmd = calloc(1, sizeof(*nfccmd) + (count * sizeof(__u32)));
	if (!nfccmd) {
		perror("rxclass: Cannot allocate memory for"
		       " RX class rule locations");
		return -ENOMEM;
	}

	/* request location list */
	nfccmd->cmd = ETHTOOL_GRXCLSRLALL;
	nfccmd->rule_cnt = count;
	err = send_ioctl(ctx, nfccmd);
	if (err < 0) {
		perror("rxclass: Cannot get RX class rules");
		free(nfccmd);
		return err;
	}

	/* write locations to bitmap */
	rule_locs = nfccmd->rule_locs;
	for (i = 0; i < count; i++) {
		err = rxclass_rule_get(ctx, rule_locs[i]);
		if (err < 0)
			break;
	}

	/* free memory and set flag to avoid reinit */
	free(nfccmd);

	return err;
}

/*
 * This is a simple rule manager implementation for ordering rx flow
 * classification rules based on newest rules being first in the list.
 * The assumption is that this rule manager is the only one adding rules to
 * the device's hardware classifier.
 */

struct rmgr_ctrl {
	/* flag for device/driver that can select locations itself */
	int			driver_select;
	/* slot contains a bitmap indicating which filters are valid */
	unsigned long		*slot;
	__u32			n_rules;
	__u32			size;
};

static int rmgr_ins(struct rmgr_ctrl *rmgr, __u32 loc)
{
	/* verify location is in rule manager range */
	if (loc >= rmgr->size) {
		fprintf(stderr, "rmgr: Location out of range\n");
		return -1;
	}

	/* set bit for the rule */
	set_bit(loc, rmgr->slot);

	return 0;
}

static int rmgr_find_empty_slot(struct rmgr_ctrl *rmgr,
				struct ethtool_rx_flow_spec *fsp)
{
	__u32 loc;
	__u32 slot_num;

	/* leave this to the driver if possible */
	if (rmgr->driver_select)
		return 0;

	/* start at the end of the list since it is lowest priority */
	loc = rmgr->size - 1;

	/* locate the first slot a rule can be placed in */
	slot_num = loc / BITS_PER_LONG;

	/*
	 * Avoid testing individual bits by inverting the word and checking
	 * to see if any bits are left set, if so there are empty spots.  By
	 * moving 1 + loc % BITS_PER_LONG we align ourselves to the last bit
	 * in the previous word.
	 *
	 * If loc rolls over it should be greater than or equal to rmgr->size
	 * and as such we know we have reached the end of the list.
	 */
	if (!~(rmgr->slot[slot_num] | (~1UL << rmgr->size % BITS_PER_LONG))) {
		loc -= 1 + (loc % BITS_PER_LONG);
		slot_num--;
	}

	/*
	 * Now that we are aligned with the last bit in each long we can just
	 * go though and eliminate all the longs with no free bits
	 */
	while (loc < rmgr->size && !~(rmgr->slot[slot_num])) {
		loc -= BITS_PER_LONG;
		slot_num--;
	}

	/*
	 * If we still are inside the range, test individual bits as one is
	 * likely available for our use.
	 */
	while (loc < rmgr->size && test_bit(loc, rmgr->slot))
		loc--;

	/* location found, insert rule */
	if (loc < rmgr->size) {
		fsp->location = loc;
		return rmgr_ins(rmgr, loc);
	}

	/* No space to add this rule */
	fprintf(stderr, "rmgr: Cannot find appropriate slot to insert rule\n");

	return -1;
}

static int rmgr_init(struct cmd_context *ctx, struct rmgr_ctrl *rmgr)
{
	struct ethtool_rxnfc *nfccmd;
	int err, i;
	__u32 *rule_locs;

	/* clear rule manager settings */
	memset(rmgr, 0, sizeof(*rmgr));

	/* request device/driver information */
	err = rxclass_get_dev_info(ctx, &rmgr->n_rules, &rmgr->driver_select);
	if (err < 0)
		return err;

	/* do not get the table if the device/driver can select locations */
	if (rmgr->driver_select)
		return 0;

	/* alloc memory for request of location list */
	nfccmd = calloc(1, sizeof(*nfccmd) + (rmgr->n_rules * sizeof(__u32)));
	if (!nfccmd) {
		perror("rmgr: Cannot allocate memory for"
		       " RX class rule locations");
		return -1;
	}

	/* request location list */
	nfccmd->cmd = ETHTOOL_GRXCLSRLALL;
	nfccmd->rule_cnt = rmgr->n_rules;
	err = send_ioctl(ctx, nfccmd);
	if (err < 0) {
		perror("rmgr: Cannot get RX class rules");
		free(nfccmd);
		return err;
	}

	/* make certain the table size is valid */
	rmgr->size = nfccmd->data;
	if (rmgr->size == 0 || rmgr->size < rmgr->n_rules) {
		perror("rmgr: Invalid RX class rules table size");
		return -1;
	}

	/* initialize bitmap for storage of valid locations */
	rmgr->slot = calloc(1, BITS_TO_LONGS(rmgr->size) * sizeof(long));
	if (!rmgr->slot) {
		perror("rmgr: Cannot allocate memory for RX class rules");
		return -1;
	}

	/* write locations to bitmap */
	rule_locs = nfccmd->rule_locs;
	for (i = 0; i < rmgr->n_rules; i++) {
		err = rmgr_ins(rmgr, rule_locs[i]);
		if (err < 0)
			break;
	}

	free(nfccmd);

	return err;
}

static void rmgr_cleanup(struct rmgr_ctrl *rmgr)
{
	free(rmgr->slot);
	rmgr->slot = NULL;
	rmgr->size = 0;
}

static int rmgr_set_location(struct cmd_context *ctx,
			     struct ethtool_rx_flow_spec *fsp)
{
	struct rmgr_ctrl rmgr;
	int err;

	/* init table of available rules */
	err = rmgr_init(ctx, &rmgr);
	if (err < 0)
		goto out;

	/* verify rule location */
	err = rmgr_find_empty_slot(&rmgr, fsp);

out:
	/* cleanup table and free resources */
	rmgr_cleanup(&rmgr);

	return err;
}

int rxclass_rule_ins(struct cmd_context *ctx,
		     struct ethtool_rx_flow_spec *fsp)
{
	struct ethtool_rxnfc nfccmd;
	__u32 loc = fsp->location;
	int err;

	/*
	 * if location is unspecified and driver cannot select locations, pull
	 * rules from device and allocate a free rule for our use
	 */
	if (loc & RX_CLS_LOC_SPECIAL) {
		err = rmgr_set_location(ctx, fsp);
		if (err < 0)
			return err;
	}

	/* notify netdev of new rule */
	nfccmd.cmd = ETHTOOL_SRXCLSRLINS;
	nfccmd.fs = *fsp;
	err = send_ioctl(ctx, &nfccmd);
	if (err < 0)
		perror("rmgr: Cannot insert RX class rule");
	else if (loc & RX_CLS_LOC_SPECIAL)
		printf("Added rule with ID %d\n", nfccmd.fs.location);

	return 0;
}

int rxclass_rule_del(struct cmd_context *ctx, __u32 loc)
{
	struct ethtool_rxnfc nfccmd;
	int err;

	/* notify netdev of rule removal */
	nfccmd.cmd = ETHTOOL_SRXCLSRLDEL;
	nfccmd.fs.location = loc;
	err = send_ioctl(ctx, &nfccmd);
	if (err < 0)
		perror("rmgr: Cannot delete RX class rule");

	return err;
}

typedef enum {
	OPT_NONE = 0,
	OPT_S32,
	OPT_U8,
	OPT_U16,
	OPT_U32,
	OPT_U64,
	OPT_BE16,
	OPT_BE32,
	OPT_BE64,
	OPT_IP4,
	OPT_IP6,
	OPT_MAC,
} rule_opt_type_t;

#define NFC_FLAG_RING		0x001
#define NFC_FLAG_LOC		0x002
#define NFC_FLAG_SADDR		0x004
#define NFC_FLAG_DADDR		0x008
#define NFC_FLAG_SPORT		0x010
#define NFC_FLAG_DPORT		0x020
#define NFC_FLAG_SPI		0x030
#define NFC_FLAG_TOS		0x040
#define NFC_FLAG_PROTO		0x080
#define NTUPLE_FLAG_VLAN	0x100
#define NTUPLE_FLAG_UDEF	0x200
#define NTUPLE_FLAG_VETH	0x400
#define NFC_FLAG_MAC_ADDR	0x800

struct rule_opts {
	const char	*name;
	rule_opt_type_t	type;
	u32		flag;
	int		offset;
	int		moffset;
};

static const struct rule_opts rule_nfc_tcp_ip4[] = {
	{ "src-ip", OPT_IP4, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip4_spec.ip4src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip4_spec.ip4src) },
	{ "dst-ip", OPT_IP4, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip4_spec.ip4dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip4_spec.ip4dst) },
	{ "tos", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip4_spec.tos),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip4_spec.tos) },
	{ "src-port", OPT_BE16, NFC_FLAG_SPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip4_spec.psrc),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip4_spec.psrc) },
	{ "dst-port", OPT_BE16, NFC_FLAG_DPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip4_spec.pdst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip4_spec.pdst) },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_esp_ip4[] = {
	{ "src-ip", OPT_IP4, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip4_spec.ip4src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip4_spec.ip4src) },
	{ "dst-ip", OPT_IP4, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip4_spec.ip4dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip4_spec.ip4dst) },
	{ "tos", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip4_spec.tos),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip4_spec.tos) },
	{ "spi", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip4_spec.spi),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip4_spec.spi) },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_usr_ip4[] = {
	{ "src-ip", OPT_IP4, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.ip4src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.ip4src) },
	{ "dst-ip", OPT_IP4, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.ip4dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.ip4dst) },
	{ "tos", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.tos),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.tos) },
	{ "l4proto", OPT_U8, NFC_FLAG_PROTO,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.proto),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.proto) },
	{ "l4data", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.l4_4_bytes) },
	{ "spi", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.l4_4_bytes) },
	{ "src-port", OPT_BE16, NFC_FLAG_SPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.l4_4_bytes) },
	{ "dst-port", OPT_BE16, NFC_FLAG_DPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip4_spec.l4_4_bytes) + 2,
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip4_spec.l4_4_bytes) + 2 },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_tcp_ip6[] = {
	{ "src-ip", OPT_IP6, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip6_spec.ip6src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip6_spec.ip6src) },
	{ "dst-ip", OPT_IP6, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip6_spec.ip6dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip6_spec.ip6dst) },
	{ "tclass", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip6_spec.tclass),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip6_spec.tclass) },
	{ "src-port", OPT_BE16, NFC_FLAG_SPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip6_spec.psrc),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip6_spec.psrc) },
	{ "dst-port", OPT_BE16, NFC_FLAG_DPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.tcp_ip6_spec.pdst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.tcp_ip6_spec.pdst) },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_esp_ip6[] = {
	{ "src-ip", OPT_IP6, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip6_spec.ip6src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip6_spec.ip6src) },
	{ "dst-ip", OPT_IP6, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip6_spec.ip6dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip6_spec.ip6dst) },
	{ "tclass", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip6_spec.tclass),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip6_spec.tclass) },
	{ "spi", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.esp_ip6_spec.spi),
	  offsetof(struct ethtool_rx_flow_spec, m_u.esp_ip6_spec.spi) },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_usr_ip6[] = {
	{ "src-ip", OPT_IP6, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.ip6src),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.ip6src) },
	{ "dst-ip", OPT_IP6, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.ip6dst),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.ip6dst) },
	{ "tclass", OPT_U8, NFC_FLAG_TOS,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.tclass),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.tclass) },
	{ "l4proto", OPT_U8, NFC_FLAG_PROTO,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.l4_proto),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.l4_proto) },
	{ "l4data", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.l4_4_bytes) },
	{ "spi", OPT_BE32, NFC_FLAG_SPI,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.l4_4_bytes) },
	{ "src-port", OPT_BE16, NFC_FLAG_SPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.l4_4_bytes),
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.l4_4_bytes) },
	{ "dst-port", OPT_BE16, NFC_FLAG_DPORT,
	  offsetof(struct ethtool_rx_flow_spec, h_u.usr_ip6_spec.l4_4_bytes) + 2,
	  offsetof(struct ethtool_rx_flow_spec, m_u.usr_ip6_spec.l4_4_bytes) + 2 },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
	{ "dst-mac", OPT_MAC, NFC_FLAG_MAC_ADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.h_dest) },
};

static const struct rule_opts rule_nfc_ether[] = {
	{ "src", OPT_MAC, NFC_FLAG_SADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.ether_spec.h_source),
	  offsetof(struct ethtool_rx_flow_spec, m_u.ether_spec.h_source) },
	{ "dst", OPT_MAC, NFC_FLAG_DADDR,
	  offsetof(struct ethtool_rx_flow_spec, h_u.ether_spec.h_dest),
	  offsetof(struct ethtool_rx_flow_spec, m_u.ether_spec.h_dest) },
	{ "proto", OPT_BE16, NFC_FLAG_PROTO,
	  offsetof(struct ethtool_rx_flow_spec, h_u.ether_spec.h_proto),
	  offsetof(struct ethtool_rx_flow_spec, m_u.ether_spec.h_proto) },
	{ "action", OPT_U64, NFC_FLAG_RING,
	  offsetof(struct ethtool_rx_flow_spec, ring_cookie), -1 },
	{ "loc", OPT_U32, NFC_FLAG_LOC,
	  offsetof(struct ethtool_rx_flow_spec, location), -1 },
	{ "vlan-etype", OPT_BE16, NTUPLE_FLAG_VETH,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_etype),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_etype) },
	{ "vlan", OPT_BE16, NTUPLE_FLAG_VLAN,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.vlan_tci),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.vlan_tci) },
	{ "user-def", OPT_BE64, NTUPLE_FLAG_UDEF,
	  offsetof(struct ethtool_rx_flow_spec, h_ext.data),
	  offsetof(struct ethtool_rx_flow_spec, m_ext.data) },
};

static int rxclass_get_long(char *str, long long *val, int size)
{
	long long max = ~0ULL >> (65 - size);
	char *endp;

	errno = 0;

	*val = strtoll(str, &endp, 0);

	if (*endp || errno || (*val > max) || (*val < ~max))
		return -1;

	return 0;
}

static int rxclass_get_ulong(char *str, unsigned long long *val, int size)
{
	long long max = ~0ULL >> (64 - size);
	char *endp;

	errno = 0;

	*val = strtoull(str, &endp, 0);

	if (*endp || errno || (*val > max))
		return -1;

	return 0;
}

static int rxclass_get_ipv4(char *str, __be32 *val)
{
	if (!inet_pton(AF_INET, str, val))
		return -1;

	return 0;
}

static int rxclass_get_ipv6(char *str, __be32 *val)
{
	if (!inet_pton(AF_INET6, str, val))
		return -1;

	return 0;
}

static int rxclass_get_ether(char *str, unsigned char *val)
{
	unsigned int buf[ETH_ALEN];
	int count;

	if (!strchr(str, ':'))
		return -1;

	count = sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
		       &buf[0], &buf[1], &buf[2],
		       &buf[3], &buf[4], &buf[5]);

	if (count != ETH_ALEN)
		return -1;

	do {
		count--;
		val[count] = buf[count];
	} while (count);

	return 0;
}

static int rxclass_get_val(char *str, unsigned char *p, u32 *flags,
			   const struct rule_opts *opt)
{
	unsigned long long mask = ~0ULL;
	int err = 0;

	if (*flags & opt->flag)
		return -1;

	*flags |= opt->flag;

	switch (opt->type) {
	case OPT_S32: {
		long long val;
		err = rxclass_get_long(str, &val, 32);
		if (err)
			return -1;
		*(int *)&p[opt->offset] = (int)val;
		if (opt->moffset >= 0)
			*(int *)&p[opt->moffset] = (int)mask;
		break;
	}
	case OPT_U8: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 8);
		if (err)
			return -1;
		*(u8 *)&p[opt->offset] = (u8)val;
		if (opt->moffset >= 0)
			*(u8 *)&p[opt->moffset] = (u8)mask;
		break;
	}
	case OPT_U16: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 16);
		if (err)
			return -1;
		*(u16 *)&p[opt->offset] = (u16)val;
		if (opt->moffset >= 0)
			*(u16 *)&p[opt->moffset] = (u16)mask;
		break;
	}
	case OPT_U32: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 32);
		if (err)
			return -1;
		*(u32 *)&p[opt->offset] = (u32)val;
		if (opt->moffset >= 0)
			*(u32 *)&p[opt->moffset] = (u32)mask;
		break;
	}
	case OPT_U64: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 64);
		if (err)
			return -1;
		*(u64 *)&p[opt->offset] = (u64)val;
		if (opt->moffset >= 0)
			*(u64 *)&p[opt->moffset] = (u64)mask;
		break;
	}
	case OPT_BE16: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 16);
		if (err)
			return -1;
		*(__be16 *)&p[opt->offset] = htons((u16)val);
		if (opt->moffset >= 0)
			*(__be16 *)&p[opt->moffset] = (__be16)mask;
		break;
	}
	case OPT_BE32: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 32);
		if (err)
			return -1;
		*(__be32 *)&p[opt->offset] = htonl((u32)val);
		if (opt->moffset >= 0)
			*(__be32 *)&p[opt->moffset] = (__be32)mask;
		break;
	}
	case OPT_BE64: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 64);
		if (err)
			return -1;
		*(__be64 *)&p[opt->offset] = htonll((u64)val);
		if (opt->moffset >= 0)
			*(__be64 *)&p[opt->moffset] = (__be64)mask;
		break;
	}
	case OPT_IP4: {
		__be32 val;
		err = rxclass_get_ipv4(str, &val);
		if (err)
			return -1;
		*(__be32 *)&p[opt->offset] = val;
		if (opt->moffset >= 0)
			*(__be32 *)&p[opt->moffset] = (__be32)mask;
		break;
	}
	case OPT_IP6: {
		__be32 val[4];
		err = rxclass_get_ipv6(str, val);
		if (err)
			return -1;
		memcpy(&p[opt->offset], val, sizeof(val));
		if (opt->moffset >= 0)
			memset(&p[opt->moffset], mask, sizeof(val));
		break;
	}
	case OPT_MAC: {
		unsigned char val[ETH_ALEN];
		err = rxclass_get_ether(str, val);
		if (err)
			return -1;
		memcpy(&p[opt->offset], val, ETH_ALEN);
		if (opt->moffset >= 0)
			memcpy(&p[opt->moffset], &mask, ETH_ALEN);
		break;
	}
	case OPT_NONE:
	default:
		return -1;
	}

	return 0;
}

static int rxclass_get_mask(char *str, unsigned char *p,
			    const struct rule_opts *opt)
{
	int err = 0;

	if (opt->moffset < 0)
		return -1;

	switch (opt->type) {
	case OPT_S32: {
		long long val;
		err = rxclass_get_long(str, &val, 32);
		if (err)
			return -1;
		*(int *)&p[opt->moffset] = ~(int)val;
		break;
	}
	case OPT_U8: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 8);
		if (err)
			return -1;
		*(u8 *)&p[opt->moffset] = ~(u8)val;
		break;
	}
	case OPT_U16: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 16);
		if (err)
			return -1;
		*(u16 *)&p[opt->moffset] = ~(u16)val;
		break;
	}
	case OPT_U32: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 32);
		if (err)
			return -1;
		*(u32 *)&p[opt->moffset] = ~(u32)val;
		break;
	}
	case OPT_U64: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 64);
		if (err)
			return -1;
		*(u64 *)&p[opt->moffset] = ~(u64)val;
		break;
	}
	case OPT_BE16: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 16);
		if (err)
			return -1;
		*(__be16 *)&p[opt->moffset] = ~htons((u16)val);
		break;
	}
	case OPT_BE32: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 32);
		if (err)
			return -1;
		*(__be32 *)&p[opt->moffset] = ~htonl((u32)val);
		break;
	}
	case OPT_BE64: {
		unsigned long long val;
		err = rxclass_get_ulong(str, &val, 64);
		if (err)
			return -1;
		*(__be64 *)&p[opt->moffset] = ~htonll((u64)val);
		break;
	}
	case OPT_IP4: {
		__be32 val;
		err = rxclass_get_ipv4(str, &val);
		if (err)
			return -1;
		*(__be32 *)&p[opt->moffset] = ~val;
		break;
	}
	case OPT_IP6: {
		__be32 val[4], *field;
		int i;
		err = rxclass_get_ipv6(str, val);
		if (err)
			return -1;
		field = (__be32 *)&p[opt->moffset];
		for (i = 0; i < 4; i++)
			field[i] = ~val[i];
		break;
	}
	case OPT_MAC: {
		unsigned char val[ETH_ALEN];
		int i;
		err = rxclass_get_ether(str, val);
		if (err)
			return -1;

		for (i = 0; i < ETH_ALEN; i++)
			val[i] = ~val[i];

		memcpy(&p[opt->moffset], val, ETH_ALEN);
		break;
	}
	case OPT_NONE:
	default:
		return -1;
	}

	return 0;
}

int rxclass_parse_ruleopts(struct cmd_context *ctx,
			   struct ethtool_rx_flow_spec *fsp)
{
	const struct rule_opts *options;
	unsigned char *p = (unsigned char *)fsp;
	int i = 0, n_opts, err;
	u32 flags = 0;
	int flow_type;
	int argc = ctx->argc;
	char **argp = ctx->argp;

	if (argc < 1)
		goto syntax_err;

	if (!strcmp(argp[0], "tcp4"))
		flow_type = TCP_V4_FLOW;
	else if (!strcmp(argp[0], "udp4"))
		flow_type = UDP_V4_FLOW;
	else if (!strcmp(argp[0], "sctp4"))
		flow_type = SCTP_V4_FLOW;
	else if (!strcmp(argp[0], "ah4"))
		flow_type = AH_V4_FLOW;
	else if (!strcmp(argp[0], "esp4"))
		flow_type = ESP_V4_FLOW;
	else if (!strcmp(argp[0], "ip4"))
		flow_type = IPV4_USER_FLOW;
	else if (!strcmp(argp[0], "tcp6"))
		flow_type = TCP_V6_FLOW;
	else if (!strcmp(argp[0], "udp6"))
		flow_type = UDP_V6_FLOW;
	else if (!strcmp(argp[0], "sctp6"))
		flow_type = SCTP_V6_FLOW;
	else if (!strcmp(argp[0], "ah6"))
		flow_type = AH_V6_FLOW;
	else if (!strcmp(argp[0], "esp6"))
		flow_type = ESP_V6_FLOW;
	else if (!strcmp(argp[0], "ip6"))
		flow_type = IPV6_USER_FLOW;
	else if (!strcmp(argp[0], "ether"))
		flow_type = ETHER_FLOW;
	else
		goto syntax_err;

	switch (flow_type) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
		options = rule_nfc_tcp_ip4;
		n_opts = ARRAY_SIZE(rule_nfc_tcp_ip4);
		break;
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
		options = rule_nfc_esp_ip4;
		n_opts = ARRAY_SIZE(rule_nfc_esp_ip4);
		break;
	case IPV4_USER_FLOW:
		options = rule_nfc_usr_ip4;
		n_opts = ARRAY_SIZE(rule_nfc_usr_ip4);
		break;
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
		options = rule_nfc_tcp_ip6;
		n_opts = ARRAY_SIZE(rule_nfc_tcp_ip6);
		break;
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
		options = rule_nfc_esp_ip6;
		n_opts = ARRAY_SIZE(rule_nfc_esp_ip6);
		break;
	case IPV6_USER_FLOW:
		options = rule_nfc_usr_ip6;
		n_opts = ARRAY_SIZE(rule_nfc_usr_ip6);
		break;
	case ETHER_FLOW:
		options = rule_nfc_ether;
		n_opts = ARRAY_SIZE(rule_nfc_ether);
		break;
	default:
		fprintf(stderr, "Add rule, invalid rule type[%s]\n", argp[0]);
		return -1;
	}

	memset(p, 0, sizeof(*fsp));
	fsp->flow_type = flow_type;
	fsp->location = RX_CLS_LOC_ANY;

	for (i = 1; i < argc;) {
		const struct rule_opts *opt;
		int idx;
		for (opt = options, idx = 0; idx < n_opts; idx++, opt++) {
			char mask_name[16];

			if (strcmp(argp[i], opt->name))
				continue;

			i++;
			if (i >= argc)
				break;

			err = rxclass_get_val(argp[i], p, &flags, opt);
			if (err) {
				fprintf(stderr, "Invalid %s value[%s]\n",
					opt->name, argp[i]);
				return -1;
			}

			i++;
			if (i >= argc)
				break;

			sprintf(mask_name, "%s-mask", opt->name);
			if (strcmp(argp[i], "m") && strcmp(argp[i], mask_name))
				break;

			i++;
			if (i >= argc)
				goto syntax_err;

			err = rxclass_get_mask(argp[i], p, opt);
			if (err) {
				fprintf(stderr, "Invalid %s mask[%s]\n",
					opt->name, argp[i]);
				return -1;
			}

			i++;

			break;
		}
		if (idx == n_opts) {
			fprintf(stdout, "Add rule, unrecognized option[%s]\n",
				argp[i]);
			return -1;
		}
	}

	if (flow_type == IPV4_USER_FLOW)
		fsp->h_u.usr_ip4_spec.ip_ver = ETH_RX_NFC_IP4;
	if (flags & (NTUPLE_FLAG_VLAN | NTUPLE_FLAG_UDEF | NTUPLE_FLAG_VETH))
		fsp->flow_type |= FLOW_EXT;
	if (flags & NFC_FLAG_MAC_ADDR)
		fsp->flow_type |= FLOW_MAC_EXT;

	return 0;

syntax_err:
	fprintf(stderr, "Add rule, invalid syntax\n");
	return -1;
}
