/*
 * MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2009 Jardel Weyrich
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>

//
// This is a workaround for <sys/uio.h> troubles on FreeBSD, HPUX, OpenBSD.
// Needed here because on some systems <sys/uio.h> gets included by things
// like <sys/socket.h>
//
#ifndef _KERNEL
#  define ADD_KERNEL
#  define _KERNEL
#  define KERNEL
#endif
#ifdef __OpenBSD__
struct file;
#endif
#include <sys/uio.h>
#ifdef ADD_KERNEL
#  undef _KERNEL
#  undef KERNEL
#endif

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
#  include <net/if_var.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip_fw.h>
#include "ipfwaux.h"

#include "../config.h"
#include "../upnpglobalvars.h"


int init_redirect(void) {
	ipfw_exec(IP_FW_INIT, NULL, 0);
	return 0;
}

void shutdown_redirect(void) {
	ipfw_exec(IP_FW_TERM, NULL, 0);
}

int add_redirect_rule2(
	const char * ifname,
	unsigned short eport,
	const char * iaddr,
	unsigned short iport,
	int proto,
	const char * desc)
{
	struct ip_fw rule;

	if (ipfw_validate_protocol(proto) < 0)
		return -1;
	if (ipfw_validate_ifname(ifname) < 0)
		return -1;
	
	memset(&rule, 0, sizeof(struct ip_fw));
	rule.version = IP_FW_CURRENT_API_VERSION;
	//rule.fw_number = 1000; // rule number
	rule.context = (void *)desc; // TODO keep this?
	rule.fw_prot = proto; // protocol
	rule.fw_flg |= IP_FW_F_IIFACE; // interfaces to check
	rule.fw_flg |= IP_FW_F_IIFNAME; // interfaces to check by name
	rule.fw_flg |= (IP_FW_F_IN | IP_FW_F_OUT); // packet direction
	rule.fw_flg |= IP_FW_F_FWD; // forward action
#ifdef USE_IFNAME_IN_RULES
	if (ifname != NULL) {
		strcpy(rule.fw_in_if.fu_via_if.name, ifname); // src interface
		rule.fw_in_if.fu_via_if.unit = -1;
	}
#endif
	if (inet_aton(iaddr, &rule.fw_out_if.fu_via_ip) == 0) {
		syslog(LOG_ERR, "inet_aton(): %m");
		return -1;
	}	
	memcpy(&rule.fw_dst,  &rule.fw_out_if.fu_via_ip, sizeof(struct in_addr));
	memcpy(&rule.fw_fwd_ip.sin_addr, &rule.fw_out_if.fu_via_ip, sizeof(struct in_addr));
	rule.fw_dmsk.s_addr = INADDR_BROADCAST;
	IP_FW_SETNDSTP(&rule, 1); // number of external ports
	rule.fw_uar.fw_pts[0] = eport; // external port
	rule.fw_fwd_ip.sin_port = iport; // internal port

	return ipfw_exec(IP_FW_ADD, &rule, sizeof(rule));
}

/* get_redirect_rule()
 * return value : 0 success (found)
 * -1 = error or rule not found */
int get_redirect_rule(
	const char * ifname,
	unsigned short eport,
	int proto,
	char * iaddr, 
	int iaddrlen, 
	unsigned short * iport,
	char * desc, 
	int desclen,
	u_int64_t * packets,
	u_int64_t * bytes)
{
	int i, count_rules, total_rules = 0;
	struct ip_fw * rules = NULL;
	
	if (ipfw_validate_protocol(proto) < 0)
		return -1;
	if (ipfw_validate_ifname(ifname) < 0)
		return -1;
	
	do {
		count_rules = ipfw_fetch_ruleset(&rules, &total_rules, 10);
		if (count_rules < 0)
			goto error;
	} while (count_rules == 10);
	
	for (i=0; i<total_rules-1; i++) {
		const struct ip_fw const * ptr = &rules[i];
		if (proto == ptr->fw_prot && eport == ptr->fw_uar.fw_pts[0]) {
			if (packets != NULL)
				*packets = ptr->fw_pcnt;
			if (bytes != NULL)
				*bytes = ptr->fw_bcnt;
			if (iport != NULL)
				*iport = ptr->fw_fwd_ip.sin_port;
			if (desc != NULL && desclen > 0)
				strlcpy(desc, "", desclen); // TODO should we copy ptr->context?
			if (iaddr != NULL && iaddrlen > 0) {
				if (inet_ntop(AF_INET, &ptr->fw_out_if.fu_via_ip, iaddr, iaddrlen) == NULL) {
					syslog(LOG_ERR, "inet_ntop(): %m");
					goto error;
				}			
			}
			// And what if we found more than 1 matching rule?
			ipfw_free_ruleset(&rules);
			return 0;
		}
	}

error:
	if (rules != NULL)
		ipfw_free_ruleset(&rules);	
	return -1;
}

int delete_redirect_rule(
	const char * ifname,
	unsigned short eport,
	int proto) 
{
	int i, count_rules, total_rules = 0;
	struct ip_fw * rules = NULL;
	
	if (ipfw_validate_protocol(proto) < 0)
		return -1;
	if (ipfw_validate_ifname(ifname) < 0)
		return -1;
	
	do {
		count_rules = ipfw_fetch_ruleset(&rules, &total_rules, 10);
		if (count_rules < 0)
			goto error;
	} while (count_rules == 10);
	
	for (i=0; i<total_rules-1; i++) {
		const struct ip_fw const * ptr = &rules[i];
		if (proto == ptr->fw_prot && eport == ptr->fw_uar.fw_pts[0]) {
			if (ipfw_exec(IP_FW_DEL, (struct ip_fw *)ptr, sizeof(*ptr)) < 0)
				goto error;
			// And what if we found more than 1 matching rule?
			ipfw_free_ruleset(&rules);
			return 0;
		}
	}
	
error:
	if (rules != NULL)
		ipfw_free_ruleset(&rules);	
	return -1;
}

int add_filter_rule2(
	const char * ifname, 
	const char * iaddr,
	unsigned short eport, 
	unsigned short iport,
	int proto, 
	const char * desc)
{
	return -1;
}

int delete_filter_rule(
	const char * ifname, 
	unsigned short eport, 
	int proto) 
{
	return -1;
}

int get_redirect_rule_by_index(
	int index,
	char * ifname, 
	unsigned short * eport,
	char * iaddr, 
	int iaddrlen, 
	unsigned short * iport,
	int * proto, 
	char * desc, 
	int desclen,
	u_int64_t * packets, 
	u_int64_t * bytes)
{
	int total_rules = 0;
	struct ip_fw * rules = NULL;

	if (index < 0) // TODO shouldn't we also validate the maximum?
		return -1;

	ipfw_fetch_ruleset(&rules, &total_rules, index + 1);

	if (total_rules == index + 1) {
		const struct ip_fw const * ptr = &rules[index];
		if (proto != NULL)
			*proto = ptr->fw_prot;
		if (eport != NULL)
			*eport = ptr->fw_uar.fw_pts[0];
		if (iport != NULL)
			*iport = ptr->fw_fwd_ip.sin_port;
		if (ifname != NULL)
			strlcpy(ifname, ptr->fw_in_if.fu_via_if.name, IFNAMSIZ);
		if (packets != NULL)
			*packets = ptr->fw_pcnt;
		if (bytes != NULL)
			*bytes = ptr->fw_bcnt;
		if (iport != NULL)
			*iport = ptr->fw_fwd_ip.sin_port;
		if (desc != NULL && desclen > 0)
			strlcpy(desc, "", desclen); // TODO should we copy ptr->context?
		if (iaddr != NULL && iaddrlen > 0) {
			if (inet_ntop(AF_INET, &ptr->fw_out_if.fu_via_ip, iaddr, iaddrlen) == NULL) {
				syslog(LOG_ERR, "inet_ntop(): %m");
				goto error;
			}			
		}
		ipfw_free_ruleset(&rules);
		return 0;
	}

error:
	if (rules != NULL)
		ipfw_free_ruleset(&rules);	
	return -1;	
}
