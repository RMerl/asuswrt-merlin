/*
 * Frontend command-line utility for Linux network configuration layer
 * Toy ifconfig/route/iptables implementations
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: main.c 241182 2011-02-17 21:50:03Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <netconf.h>

#define valid_ports(min, max) ( \
	!((min) == 0 && (max) == 0) && \
	!((min) == 0 && (max) == 0xffff) \
)

static void
print_fw(netconf_fw_t *fw)
{
	netconf_filter_t *filter;
	netconf_nat_t *nat;
	netconf_app_t *app;
	const char *target_name[] = { "DROP", "ACCEPT", "logdrop", "logaccept", "SNAT", "DNAT", "MASQUERADE", "autofw" };
	const char *filter_dir_name[] = { "IN", "FORWARD", "OUT" };

	/* Target name */
	printf("%s", target_name[fw->target]);

	/* Filter direction */
	if (netconf_valid_filter(fw->target)) {
		filter = (netconf_filter_t *) fw;
		printf(" %s", filter_dir_name[filter->dir]);
	}

	/* IP protocol */
	if (fw->match.ipproto == IPPROTO_TCP)
		printf(" tcp");
	else if (fw->match.ipproto == IPPROTO_UDP)
		printf(" udp");

	/* Match source IP address */
	if (fw->match.src.ipaddr.s_addr & fw->match.src.netmask.s_addr) {
		printf(" src %s%s",
		       (fw->match.flags & NETCONF_INV_SRCIP) ? "!" : "",
		       inet_ntoa(fw->match.src.ipaddr));
		if (fw->match.src.netmask.s_addr != 0xffffffff)
			printf("/%s", inet_ntoa(fw->match.src.netmask));
	} else
		printf(" src anywhere");

	/* Match source TCP/UDP port range */
	if (fw->match.ipproto && valid_ports(fw->match.src.ports[0], fw->match.src.ports[1])) {
		printf(":%s%d",
		       (fw->match.flags & NETCONF_INV_SRCPT) ? "!" : "",
		       ntohs(fw->match.src.ports[0]));
		if (fw->match.src.ports[1] != fw->match.src.ports[0])
			printf("-%d", ntohs(fw->match.src.ports[1]));
	}

	/* Match destination IP address */
	if (fw->match.dst.ipaddr.s_addr & fw->match.dst.netmask.s_addr) {
		printf(" dst %s%s",
		       (fw->match.flags & NETCONF_INV_DSTIP) ? "!" : "",
		       inet_ntoa(fw->match.dst.ipaddr));
		if (fw->match.dst.netmask.s_addr != 0xffffffff)
			printf("/%s", inet_ntoa(fw->match.dst.netmask));
	} else
		printf(" dst anywhere");

	/* Match destination TCP/UDP port range */
	if (fw->match.ipproto && valid_ports(fw->match.dst.ports[0], fw->match.dst.ports[1] != 0xffff)) {
		printf(":%s%d",
		       (fw->match.flags & NETCONF_INV_DSTPT) ? "!" : "",
		       ntohs(fw->match.dst.ports[0]));
		if (fw->match.dst.ports[1] != fw->match.dst.ports[0])
			printf("-%d", ntohs(fw->match.dst.ports[1]));
	}

	/* Match MAC address */
	if (!ETHER_ISNULLADDR(fw->match.mac.octet)) {
		printf(" mac %s%02X:%02X:%02X:%02X:%02X:%02X",
		       (fw->match.flags & NETCONF_INV_MAC) ? "! " : "",
		       fw->match.mac.octet[0] & 0xff,
		       fw->match.mac.octet[1] & 0xff,
		       fw->match.mac.octet[2] & 0xff,
		       fw->match.mac.octet[3] & 0xff,
		       fw->match.mac.octet[4] & 0xff,
		       fw->match.mac.octet[5] & 0xff);
	}

	/* Match packet state */
	if (fw->match.state) {
		char *sep = "";
		printf(" state ");
		if (fw->match.state & NETCONF_INVALID) {
			printf("%sINVALID", sep);
			sep = ",";
		}
		if (fw->match.state & NETCONF_ESTABLISHED) {
			printf("%sESTABLISHED", sep);
			sep = ",";
		}
		if (fw->match.state & NETCONF_RELATED) {
			printf("%sRELATED", sep);
			sep = ",";
		}
		if (fw->match.state & NETCONF_NEW) {
			printf("%sNEW", sep);
			sep = ",";
		}
	}

	/* Match local time */
	if (fw->match.secs[0] || fw->match.secs[1]) {
		char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		if (fw->match.days[0] < 7 && fw->match.days[1] < 7)
			printf(" %s-%s",
			       days[fw->match.days[0]], days[fw->match.days[1]]);
		if (fw->match.secs[0] < (24 * 60 * 60) && fw->match.days[1] < (24 * 60 * 60))
			printf(" %02d:%02d:%02d-%02d:%02d:%02d",
			       fw->match.secs[0] / (60 * 60), (fw->match.secs[0] / 60) % 60, fw->match.secs[0] % 60,
			       fw->match.secs[1] / (60 * 60), (fw->match.secs[1] / 60) % 60, fw->match.secs[1] % 60);
	}

	/* Match interface name */
	if (strlen(fw->match.in.name))
		printf(" in %s%s", 
		       (fw->match.flags & NETCONF_INV_IN) ? "!" : "",
		       fw->match.in.name);
	if (strlen(fw->match.out.name))
		printf(" out %s%s", 
		       (fw->match.flags & NETCONF_INV_OUT) ? "!" : "",
		       fw->match.out.name);

	/* NAT target parameters */
	if (fw->target == NETCONF_SNAT || fw->target == NETCONF_DNAT) {
		nat = (netconf_nat_t *) fw;
		printf(" to %s", inet_ntoa(nat->ipaddr));
		if (valid_ports(nat->ports[0], nat->ports[1])) {
			printf(":%d", ntohs(nat->ports[0]));
			if (nat->ports[1] != nat->ports[0])
				printf("-%d", ntohs(nat->ports[1]));
		}
	}

	/* Application specific port forward parameters */
	if (fw->target == NETCONF_APP) {
		app = (netconf_app_t *) fw;
		printf(" autofw ");
		if (app->proto == IPPROTO_TCP)
			printf("tcp ");
		else if (app->proto == IPPROTO_UDP)
			printf("udp ");
		printf("dpt:%hu", ntohs(app->dport[0]));
		if (ntohs(app->dport[1]) > ntohs(app->dport[0]))
			printf("-%hu", ntohs(app->dport[1]));
		printf(" ");
		printf("to:%hu", ntohs(app->to[0]));
		if (ntohs(app->to[1]) > ntohs(app->to[0]))
			printf("-%hu", ntohs(app->to[1]));
		printf(" ");
	}

	printf("\n");
}

int
main(int argc, char **argv)
{
	netconf_fw_t *fw, fw_list;
	int ret;

	if ((ret = netconf_get_fw(&fw_list)))
		return ret;

	netconf_list_for_each(fw, &fw_list) {
		assert(netconf_fw_exists(fw));
		print_fw(fw);
	}

	netconf_list_free(&fw_list);

	return 0;
}
