/*
 *
 *   Authors:
 *    Pedro Roque		<roque@di.fc.ul.pt>
 *    Lars Fenneberg		<lf@elemental.net>
 *
 *   This software is Copyright 1996,1997 by the above mentioned author(s),
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <pekkas@netcore.fi>.
 *
 */

#include "config.h"
#include "includes.h"
#include "radvd.h"

static void process_rs(struct Interface *, unsigned char *msg,
		       int len, struct sockaddr_in6 *);
static void process_ra(struct Interface *, unsigned char *msg, int len,
	struct sockaddr_in6 *);
static int  addr_match(struct in6_addr *a1, struct in6_addr *a2,
	int prefixlen);

void
process(struct Interface *ifacel, unsigned char *msg, int len,
	struct sockaddr_in6 *addr, struct in6_pktinfo *pkt_info, int hoplimit)
{
	struct Interface *iface;
	struct icmp6_hdr *icmph;
	char addr_str[INET6_ADDRSTRLEN];

	print_addr(&addr->sin6_addr, addr_str);

	if ( ! pkt_info )
	{
		flog(LOG_WARNING, "received packet with no pkt_info from %s!", addr_str );
		return;
	}

	/*
	 * can this happen?
	 */

	if (len < sizeof(struct icmp6_hdr))
	{
		flog(LOG_WARNING, "received icmpv6 packet with invalid length (%d) from %s",
			len, addr_str);
		return;
	}

	icmph = (struct icmp6_hdr *) msg;

	if (icmph->icmp6_type != ND_ROUTER_SOLICIT &&
	    icmph->icmp6_type != ND_ROUTER_ADVERT)
	{
		/*
		 *	We just want to listen to RSs and RAs
		 */

		flog(LOG_ERR, "icmpv6 filter failed");
		return;
	}

	if (icmph->icmp6_type == ND_ROUTER_ADVERT)
	{
		if (len < sizeof(struct nd_router_advert)) {
			flog(LOG_WARNING, "received icmpv6 RA packet with invalid length (%d) from %s",
				len, addr_str);
			return;
		}

		if (!IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)) {
			flog(LOG_WARNING, "received icmpv6 RA packet with non-linklocal source address from %s", addr_str);
			return;
		}
	}

	if (icmph->icmp6_type == ND_ROUTER_SOLICIT)
	{
		if (len < sizeof(struct nd_router_solicit)) {
			flog(LOG_WARNING, "received icmpv6 RS packet with invalid length (%d) from %s",
				len, addr_str);
			return;
		}
	}

	if (icmph->icmp6_code != 0)
	{
		flog(LOG_WARNING, "received icmpv6 RS/RA packet with invalid code (%d) from %s",
			icmph->icmp6_code, addr_str);
		return;
	}

	dlog(LOG_DEBUG, 4, "if_index %u", pkt_info->ipi6_ifindex);

	/* get iface by received if_index */

	for (iface = ifacel; iface; iface=iface->next)
	{
		if (iface->if_index == pkt_info->ipi6_ifindex)
		{
			break;
		}
	}

	if (iface == NULL)
	{
		dlog(LOG_DEBUG, 2, "received packet from unknown interface: %d",
			pkt_info->ipi6_ifindex);
		return;
	}

	if (hoplimit != 255)
	{
		print_addr(&addr->sin6_addr, addr_str);
		flog(LOG_WARNING, "received RS or RA with invalid hoplimit %d from %s",
			hoplimit, addr_str);
		return;
	}

	if (!iface->AdvSendAdvert)
	{
		dlog(LOG_DEBUG, 2, "AdvSendAdvert is off for %s", iface->Name);
		return;
	}

	dlog(LOG_DEBUG, 4, "found Interface: %s", iface->Name);

	if (icmph->icmp6_type == ND_ROUTER_SOLICIT)
	{
		dlog(LOG_DEBUG, 4, "received RS from %s", addr_str);
		process_rs(iface, msg, len, addr);
	}
	else if (icmph->icmp6_type == ND_ROUTER_ADVERT)
	{
		dlog(LOG_DEBUG, 4, "received RA from %s", addr_str);
		process_ra(iface, msg, len, addr);
	}
}

static void
process_rs(struct Interface *iface, unsigned char *msg, int len,
	struct sockaddr_in6 *addr)
{
	double delay;
	double next;
	struct timeval tv;
	uint8_t *opt_str;

	/* validation */
	len -= sizeof(struct nd_router_solicit);

	opt_str = (uint8_t *)(msg + sizeof(struct nd_router_solicit));

	while (len > 0)
	{
		int optlen;

		if (len < 2)
		{
			flog(LOG_WARNING, "trailing garbage in RS");
			return;
		}

		optlen = (opt_str[1] << 3);

		if (optlen == 0)
		{
			flog(LOG_WARNING, "zero length option in RS");
			return;
		}
		else if (optlen > len)
		{
			flog(LOG_WARNING, "option length greater than total length in RS");
			return;
		}

		if (*opt_str == ND_OPT_SOURCE_LINKADDR &&
		    IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr)) {
			flog(LOG_WARNING, "received icmpv6 RS packet with unspecified source address and there is a lladdr option");
			return;
		}

		len -= optlen;
		opt_str += optlen;
	}

	gettimeofday(&tv, NULL);

	delay = MAX_RA_DELAY_TIME * rand() / (RAND_MAX +1.0);

	if (iface->UnicastOnly) {
		send_ra_forall(iface, &addr->sin6_addr);
	}
	else if ( timevaldiff(&tv, &iface->last_multicast) / 1000.0 < iface->MinDelayBetweenRAs ) {
		/* last RA was sent only a few moments ago, don't send another immediately. */
		next = iface->MinDelayBetweenRAs - (tv.tv_sec + tv.tv_usec / 1000000.0) + (iface->last_multicast.tv_sec + iface->last_multicast.tv_usec / 1000000.0) + delay/1000.0;
		iface->next_multicast = next_timeval(next);
	}
	else {
		/* no RA sent in a while, send a multicast reply */
		send_ra_forall(iface, NULL);
		next = rand_between(iface->MinRtrAdvInterval, iface->MaxRtrAdvInterval);
		iface->next_multicast = next_timeval(next);
	}
}

/*
 * check router advertisements according to RFC 4861, 6.2.7
 */
static void
process_ra(struct Interface *iface, unsigned char *msg, int len,
	struct sockaddr_in6 *addr)
{
	struct nd_router_advert *radvert;
	char addr_str[INET6_ADDRSTRLEN];
	uint8_t *opt_str;

	print_addr(&addr->sin6_addr, addr_str);

	radvert = (struct nd_router_advert *) msg;

	if ((radvert->nd_ra_curhoplimit && iface->AdvCurHopLimit) &&
	   (radvert->nd_ra_curhoplimit != iface->AdvCurHopLimit))
	{
		flog(LOG_WARNING, "our AdvCurHopLimit on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) && !iface->AdvManagedFlag)
	{
		flog(LOG_WARNING, "our AdvManagedFlag on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) && !iface->AdvOtherConfigFlag)
	{
		flog(LOG_WARNING, "our AdvOtherConfigFlag on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	/* note: we don't check the default router preference here, because they're likely different */

	if ((radvert->nd_ra_reachable && iface->AdvReachableTime) &&
	   (ntohl(radvert->nd_ra_reachable) != iface->AdvReachableTime))
	{
		flog(LOG_WARNING, "our AdvReachableTime on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_retransmit && iface->AdvRetransTimer) &&
	   (ntohl(radvert->nd_ra_retransmit) != iface->AdvRetransTimer))
	{
		flog(LOG_WARNING, "our AdvRetransTimer on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	len -= sizeof(struct nd_router_advert);

	if (len == 0)
		return;

	opt_str = (uint8_t *)(msg + sizeof(struct nd_router_advert));

	while (len > 0)
	{
		int optlen;
		struct nd_opt_prefix_info *pinfo;
		struct nd_opt_rdnss_info_local *rdnssinfo;
		struct nd_opt_dnssl_info_local *dnsslinfo;
		struct nd_opt_mtu *mtu;
		struct AdvPrefix *prefix;
		struct AdvRDNSS *rdnss;
		char prefix_str[INET6_ADDRSTRLEN];
		char rdnss_str[INET6_ADDRSTRLEN];
		char suffix[256];
		unsigned int offset, label_len;
		uint32_t preferred, valid, count;

		if (len < 2)
		{
			flog(LOG_ERR, "trailing garbage in RA on %s from %s",
				iface->Name, addr_str);
			break;
		}

		optlen = (opt_str[1] << 3);

		if (optlen == 0)
		{
			flog(LOG_ERR, "zero length option in RA on %s from %s",
				iface->Name, addr_str);
			break;
		}
		else if (optlen > len)
		{
			flog(LOG_ERR, "option length (%d) greater than total"
				" length (%d) in RA on %s from %s",
				optlen, len,
				iface->Name, addr_str);
			break;
		}

		switch (*opt_str)
		{
		case ND_OPT_MTU:
			mtu = (struct nd_opt_mtu *)opt_str;
			if (len < sizeof(*mtu))
				return;

			if (iface->AdvLinkMTU && (ntohl(mtu->nd_opt_mtu_mtu) != iface->AdvLinkMTU))
			{
				flog(LOG_WARNING, "our AdvLinkMTU on %s doesn't agree with %s",
					iface->Name, addr_str);
			}
			break;
		case ND_OPT_PREFIX_INFORMATION:
			pinfo = (struct nd_opt_prefix_info *) opt_str;
			if (len < sizeof(*pinfo))
				return;
			preferred = ntohl(pinfo->nd_opt_pi_preferred_time);
			valid = ntohl(pinfo->nd_opt_pi_valid_time);

			prefix = iface->AdvPrefixList;
			while (prefix)
			{
				if (prefix->enabled &&
				    (prefix->PrefixLen == pinfo->nd_opt_pi_prefix_len) &&
				    addr_match(&prefix->Prefix, &pinfo->nd_opt_pi_prefix,
				    	 prefix->PrefixLen))
				{
					print_addr(&prefix->Prefix, prefix_str);

					if (!prefix->DecrementLifetimesFlag && valid != prefix->AdvValidLifetime)
					{
						flog(LOG_WARNING, "our AdvValidLifetime on"
						 " %s for %s doesn't agree with %s",
						 iface->Name,
						 prefix_str,
						 addr_str
						 );
					}
					if (!prefix->DecrementLifetimesFlag && preferred != prefix->AdvPreferredLifetime)
					{
						flog(LOG_WARNING, "our AdvPreferredLifetime on"
						 " %s for %s doesn't agree with %s",
						 iface->Name,
						 prefix_str,
						 addr_str
						 );
					}
				}

				prefix = prefix->next;
			}
			break;
		case ND_OPT_ROUTE_INFORMATION:
			/* not checked: these will very likely vary a lot */
			break;
		case ND_OPT_SOURCE_LINKADDR:
			/* not checked */
			break;
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_REDIRECTED_HEADER:
			flog(LOG_ERR, "invalid option %d in RA on %s from %s",
				(int)*opt_str, iface->Name, addr_str);
			break;
		/* Mobile IPv6 extensions */
		case ND_OPT_RTR_ADV_INTERVAL:
		case ND_OPT_HOME_AGENT_INFO:
			/* not checked */
			break;
		case ND_OPT_RDNSS_INFORMATION:
			rdnssinfo = (struct nd_opt_rdnss_info_local *) opt_str;
			if (len < sizeof(*rdnssinfo))
				return;
			count = rdnssinfo->nd_opt_rdnssi_len;

			/* Check the RNDSS addresses received */
			switch (count) {
				case 7:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr3 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr3, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}
					/* FALLTHROUGH */
				case 5:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr2 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr2, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}
					/* FALLTHROUGH */
				case 3:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr1 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr1, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}

					break;
				default:
					flog(LOG_ERR, "invalid len %i in RDNSS option on %s from %s",
							count, iface->Name, addr_str);
			}

			break;
		case ND_OPT_DNSSL_INFORMATION:
			dnsslinfo = (struct nd_opt_dnssl_info_local *) opt_str;
			if (len < sizeof(*dnsslinfo))
				return;

			suffix[0] = '\0';
			for (offset = 0; offset < (dnsslinfo->nd_opt_dnssli_len-1)*8;) {
				if (&dnsslinfo->nd_opt_dnssli_suffixes[offset] - opt_str >= len)
					return;
				label_len = dnsslinfo->nd_opt_dnssli_suffixes[offset++];

				if (label_len == 0) {
					/*
					 * Ignore empty suffixes. They're
					 * probably just padding...
					 */
					if (suffix[0] == '\0')
						continue;

					if (!check_dnssl_presence(iface->AdvDNSSLList, suffix)) {
						flog(LOG_WARNING, "DNSSL suffix %s received on %s from %s is not advertised by us",
							suffix, iface->Name, addr_str);
					}

					suffix[0] = '\0';
					continue;
				}

				/*
				 * 1) must not overflow int: label + 2, offset + label_len
				 * 2) last byte of dnssli_suffix must not overflow opt_str + len
				 */
				if ((sizeof(suffix) - strlen(suffix)) < (label_len + 2) ||
				    label_len > label_len + 2 ||
				    &dnsslinfo->nd_opt_dnssli_suffixes[offset+label_len] - opt_str >= len ||
				    offset + label_len < offset) {
					flog(LOG_ERR, "oversized suffix in DNSSL option on %s from %s",
							iface->Name, addr_str);
					break;
				}

				if (suffix[0] != '\0')
					strcat(suffix, ".");
				strncat(suffix, (char*)&dnsslinfo->nd_opt_dnssli_suffixes[offset], label_len);
				offset += label_len;
			}
			break;
		default:
			dlog(LOG_DEBUG, 1, "unknown option %d in RA on %s from %s",
				(int)*opt_str, iface->Name, addr_str);
			break;
		}

		len -= optlen;
		opt_str += optlen;
	}
}

static int
addr_match(struct in6_addr *a1, struct in6_addr *a2, int prefixlen)
{
	unsigned int pdw;
	unsigned int pbi;

	pdw = prefixlen >> 0x05;  /* num of whole uint32_t in prefix */
	pbi = prefixlen &  0x1f;  /* num of bits in incomplete uint32_t in prefix */

	if (pdw)
	{
		if (memcmp(a1, a2, pdw << 2))
			return 0;
	}

	if (pbi)
	{
		uint32_t w1, w2;
		uint32_t mask;

		w1 = *((uint32_t *)a1 + pdw);
		w2 = *((uint32_t *)a2 + pdw);

		mask = htonl(((uint32_t) 0xffffffff) << (0x20 - pbi));

		if ((w1 ^ w2) & mask)
			return 0;
	}

	return 1;
}

