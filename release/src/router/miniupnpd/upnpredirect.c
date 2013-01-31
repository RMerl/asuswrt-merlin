/* $Id: upnpredirect.c,v 1.60 2011/06/22 20:34:39 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "config.h"
#include "upnpredirect.h"
#include "upnpglobalvars.h"
#include "upnpevents.h"
#if defined(USE_NETFILTER)
#include "netfilter/iptcrdr.h"
#endif
#if defined(USE_PF)
#include "pf/obsdrdr.h"
#endif
#if defined(USE_IPF)
#include "ipf/ipfrdr.h"
#endif
#if defined(USE_IPFW)
#include "ipfw/ipfwrdr.h"
#endif
#ifdef USE_MINIUPNPDCTL
#include <stdio.h>
#include <unistd.h>
#endif
#ifdef ENABLE_LEASEFILE
#include <sys/stat.h>
#endif

/* from <inttypes.h> */
#ifndef PRIu64
#define PRIu64 "llu"
#endif

/* proto_atoi() 
 * convert the string "UDP" or "TCP" to IPPROTO_UDP and IPPROTO_UDP */
static int
proto_atoi(const char * protocol)
{
	int proto = IPPROTO_TCP;
	if(strcmp(protocol, "UDP") == 0)
		proto = IPPROTO_UDP;
	return proto;
}

#ifdef ENABLE_LEASEFILE
static int
lease_file_add(unsigned short eport,
               const char * iaddr,
               unsigned short iport,
               int proto,
               const char * desc,
               unsigned int timestamp)
{
	FILE * fd;

	if (lease_file == NULL) return 0;

	fd = fopen( lease_file, "a");
	if (fd==NULL) {
		syslog(LOG_ERR, "could not open lease file: %s", lease_file);
		return -1;
	}

	fprintf(fd, "%s:%hu:%s:%hu:%u:%s\n",
	        ((proto==IPPROTO_TCP)?"TCP":"UDP"), eport, iaddr, iport,
	        timestamp, desc);
	fclose(fd);
	
	return 0;
}

static int
lease_file_remove(unsigned short eport, int proto)
{
	FILE* fd, *fdt;
	int tmp;
	char buf[512];
	char str[32];
	char tmpfilename[128];
	int str_size, buf_size;


	if (lease_file == NULL) return 0;

	if (strlen( lease_file) + 7 > sizeof(tmpfilename)) {
		syslog(LOG_ERR, "Lease filename is too long");
		return -1;
	}

	strncpy( tmpfilename, lease_file, sizeof(tmpfilename) );
	strncat( tmpfilename, "XXXXXX", sizeof(tmpfilename) - strlen(tmpfilename));

	fd = fopen( lease_file, "r");
	if (fd==NULL) {
		return 0;
	}

	snprintf( str, sizeof(str), "%s:%u", ((proto==IPPROTO_TCP)?"TCP":"UDP"), eport);
	str_size = strlen(str);

	tmp = mkstemp(tmpfilename);
	if (tmp==-1) {
		fclose(fd);
		syslog(LOG_ERR, "could not open temporary lease file");
		return -1;
	}
	fchmod(tmp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	fdt = fdopen(tmp, "a");

	buf[sizeof(buf)-1] = 0;
	while( fgets(buf, sizeof(buf)-1, fd) != NULL) {
		buf_size = strlen(buf);

		if (buf_size < str_size || strncmp(str, buf, str_size)!=0) {
			fwrite(buf, buf_size, 1, fdt);
		}
	}
	fclose(fdt);
	fclose(fd);
	
	if (rename(tmpfilename, lease_file) < 0) {
		syslog(LOG_ERR, "could not rename temporary lease file to %s", lease_file);
		remove(tmpfilename);
	}
	
	return 0;
	
}

/* reload_from_lease_file()
 * read lease_file and add the rules contained
 */
int reload_from_lease_file()
{
	FILE * fd;
	char * p;
	unsigned short eport, iport;
	char * proto;
	char * iaddr;
	char * desc;
	char * rhost;
	unsigned int leaseduration;
	unsigned int timestamp;
	time_t current_time;
	char line[128];
	int r;

	if(!lease_file) return -1;
	fd = fopen( lease_file, "r");
	if (fd==NULL) {
		syslog(LOG_ERR, "could not open lease file: %s", lease_file);
		return -1;
	}
	if(unlink(lease_file) < 0) {
		syslog(LOG_WARNING, "could not unlink file %s : %m", lease_file);
	}

	current_time = time(NULL);
	while(fgets(line, sizeof(line), fd)) {
		syslog(LOG_DEBUG, "parsing lease file line '%s'", line);
		proto = line;
		p = strchr(line, ':');
		if(!p) {
			syslog(LOG_ERR, "unrecognized data in lease file");
			continue;
		}
		*(p++) = '\0';
		iaddr = strchr(p, ':');
		if(!iaddr) {
			syslog(LOG_ERR, "unrecognized data in lease file");
			continue;
		}
		*(iaddr++) = '\0';
		eport = (unsigned short)atoi(p);
		p = strchr(iaddr, ':');
		if(!p) {
			syslog(LOG_ERR, "unrecognized data in lease file");
			continue;
		}
		*(p++) = '\0';
		timestamp = (unsigned int)atoi(p);
		p = strchr(p, ':');
		if(!p) {
			syslog(LOG_ERR, "unrecognized data in lease file");
			continue;
		}
		*(p++) = '\0';
		desc = strchr(p, ':');
		if(!desc) {
			syslog(LOG_ERR, "unrecognized data in lease file");
			continue;
		}
		*(desc++) = '\0';
		iport = (unsigned short)atoi(p);
		/* trim description */
		while(isspace(*desc))
			desc++;
		p = desc;
		while(*(p+1))
			p++;
		while(isspace(*p) && (p > desc))
			*(p--) = '\0';

		if(timestamp > 0) {
			if(timestamp <= current_time) {
				syslog(LOG_NOTICE, "already expired lease in lease file");
				continue;
			} else {
				leaseduration = current_time - timestamp;
			}
		} else {
			leaseduration = 0;	/* default value */
		}
		rhost = NULL;
		r = upnp_redirect(rhost, eport, iaddr, iport, proto, desc, leaseduration);
		if(r == -1) {
			syslog(LOG_ERR, "Failed to redirect %hu -> %s:%hu protocol %s",
			       eport, iaddr, iport, proto);
		} else if(r == -2) {
			/* Add the redirection again to the lease file */
			lease_file_add(eport, iaddr, iport, proto_atoi(proto),
			               desc, timestamp);
		}
	}
	fclose(fd);
	
	return 0;
}
#endif

/* upnp_redirect() 
 * calls OS/fw dependant implementation of the redirection.
 * protocol should be the string "TCP" or "UDP"
 * returns: 0 on success
 *          -1 failed to redirect
 *          -2 already redirected
 *          -3 permission check failed
 */
int
upnp_redirect(const char * rhost, unsigned short eport, 
              const char * iaddr, unsigned short iport,
              const char * protocol, const char * desc,
              unsigned int leaseduration)
{
	int proto, r;
	char iaddr_old[32];
	unsigned short iport_old;
	struct in_addr address;
	unsigned int timestamp;

	proto = proto_atoi(protocol);
	if(inet_aton(iaddr, &address) < 0) {
		syslog(LOG_ERR, "inet_aton(%s) : %m", iaddr);
		return -1;
	}

	if(!check_upnp_rule_against_permissions(upnppermlist, num_upnpperm,
	                                        eport, address, iport)) {
		syslog(LOG_INFO, "redirection permission check failed for "
		                 "%hu->%s:%hu %s", eport, iaddr, iport, protocol);
		return -3;
	}
	r = get_redirect_rule(ext_if_name, eport, proto,
	                      iaddr_old, sizeof(iaddr_old), &iport_old, 0, 0,
	                      0, 0,
	                      &timestamp, 0, 0);
	if(r == 0) {
		/* if existing redirect rule matches redirect request return success
		 * xbox 360 does not keep track of the port it redirects and will
		 * redirect another port when receiving ConflictInMappingEntry */
		if(strcmp(iaddr, iaddr_old)==0 && iport==iport_old) {
			/* redirection allready exists */
			syslog(LOG_INFO, "port %hu %s already redirected to %s:%hu, replacing", eport, (proto==IPPROTO_TCP)?"tcp":"udp", iaddr_old, iport_old);
			/* remove and then add again */
			if(_upnp_delete_redir(eport, proto) < 0) {
				syslog(LOG_ERR, "failed to remove port mapping");
				return 0;
			}
		} else {
			syslog(LOG_INFO, "port %hu protocol %s already redirected to %s:%hu",
				eport, protocol, iaddr_old, iport_old);
			return -2;
		}
 	} else {
		timestamp = (leaseduration > 0) ? time(NULL) + leaseduration : 0;
 		syslog(LOG_INFO, "redirecting port %hu to %s:%hu protocol %s for: %s",
 			eport, iaddr, iport, protocol, desc);
		return upnp_redirect_internal(rhost, eport, iaddr, iport, proto,
		                              desc, timestamp);
 	}
	return 0;
}

int
upnp_redirect_internal(const char * rhost, unsigned short eport,
                       const char * iaddr, unsigned short iport,
                       int proto, const char * desc,
                       unsigned int timestamp)
{
	/*syslog(LOG_INFO, "redirecting port %hu to %s:%hu protocol %s for: %s",
		eport, iaddr, iport, protocol, desc);			*/
	if(add_redirect_rule2(ext_if_name, rhost, eport, iaddr, iport, proto,
	                      desc, timestamp) < 0) {
		return -1;
	}

#ifdef ENABLE_LEASEFILE
	lease_file_add( eport, iaddr, iport, proto, desc, timestamp);
#endif
/*	syslog(LOG_INFO, "creating pass rule to %s:%hu protocol %s for: %s",
		iaddr, iport, protocol, desc);*/
	if(add_filter_rule2(ext_if_name, rhost, iaddr, eport, iport, proto, desc) < 0) {
		/* clean up the redirect rule */
#if !defined(__linux__)
		delete_redirect_rule(ext_if_name, eport, proto);
#endif
		return -1;
	}
	if(timestamp > 0) {
		if(!nextruletoclean_timestamp || (timestamp < nextruletoclean_timestamp))
			nextruletoclean_timestamp = timestamp;
	}

#ifdef ENABLE_EVENTS
	/* the number of port mappings changed, we must
	 * inform the subscribers */
	upnp_event_var_change_notify(EWanIPC);
#endif
	return 0;
}



/* Firewall independant code which call the FW dependant code. */
int
upnp_get_redirection_infos(unsigned short eport, const char * protocol,
                           unsigned short * iport,
                           char * iaddr, int iaddrlen,
                           char * desc, int desclen,
                           char * rhost, int rhostlen,
                           unsigned int * leaseduration)
{
	int r;
	unsigned int timestamp;
	time_t current_time;

	if(desc && (desclen > 0))
		desc[0] = '\0';
	if(rhost && (rhostlen > 0))
		rhost[0] = '\0';
	r = get_redirect_rule(ext_if_name, eport, proto_atoi(protocol),
	                      iaddr, iaddrlen, iport, desc, desclen,
	                      rhost, rhostlen, &timestamp,
	                      0, 0);
	if(r == 0 && timestamp > 0 && timestamp > (current_time = time(NULL))) {
		*leaseduration = timestamp - current_time;
	} else {
		*leaseduration = 0;
	}
	return r;
}

int
upnp_get_redirection_infos_by_index(int index,
                                    unsigned short * eport, char * protocol,
                                    unsigned short * iport, 
                                    char * iaddr, int iaddrlen,
                                    char * desc, int desclen,
                                    char * rhost, int rhostlen,
                                    unsigned int * leaseduration)
{
	/*char ifname[IFNAMSIZ];*/
	int proto = 0;
	unsigned int timestamp;
	time_t current_time;

	if(desc && (desclen > 0))
		desc[0] = '\0';
	if(rhost && (rhost > 0))
		rhost[0] = '\0';
	if(get_redirect_rule_by_index(index, 0/*ifname*/, eport, iaddr, iaddrlen,
	                              iport, &proto, desc, desclen,
	                              rhost, rhostlen, &timestamp,
	                              0, 0) < 0)
		return -1;
	else
	{
		current_time = time(NULL);
		*leaseduration = (timestamp > current_time)
		                 ? (timestamp - current_time)
		                 : 0;
		if(proto == IPPROTO_TCP)
			memcpy(protocol, "TCP", 4);
		else
			memcpy(protocol, "UDP", 4);
		return 0;
	}
}

/* called from natpmp.c too */
int
_upnp_delete_redir(unsigned short eport, int proto)
{
	int r;
#if defined(__linux__)
	r = delete_redirect_and_filter_rules(eport, proto);
#else
	r = delete_redirect_rule(ext_if_name, eport, proto);
	delete_filter_rule(ext_if_name, eport, proto);
#endif
#ifdef ENABLE_LEASEFILE
	lease_file_remove( eport, proto);
#endif

#ifdef ENABLE_EVENTS
	upnp_event_var_change_notify(EWanIPC);
#endif
	return r;
}

int
upnp_delete_redirection(unsigned short eport, const char * protocol)
{
	syslog(LOG_INFO, "removing redirect rule port %hu %s", eport, protocol);
	return _upnp_delete_redir(eport, proto_atoi(protocol));
}

/* upnp_get_portmapping_number_of_entries()
 * TODO: improve this code. */
int
upnp_get_portmapping_number_of_entries()
{
	int n = 0, r = 0;
	unsigned short eport, iport;
	char protocol[4], iaddr[32], desc[64], rhost[32];
	unsigned int leaseduration;
	do {
		protocol[0] = '\0'; iaddr[0] = '\0'; desc[0] = '\0';
		r = upnp_get_redirection_infos_by_index(n, &eport, protocol, &iport,
		                                        iaddr, sizeof(iaddr),
		                                        desc, sizeof(desc),
		                                        rhost, sizeof(rhost),
		                                        &leaseduration);
		n++;
	} while(r==0);
	return (n-1);
}

/* functions used to remove unused rules
 * As a side effect, delete expired rules (based on LeaseDuration) */
struct rule_state *
get_upnp_rules_state_list(int max_rules_number_target)
{
	/*char ifname[IFNAMSIZ];*/
	int proto;
	unsigned short iport;
	unsigned int timestamp;
	struct rule_state * tmp;
	struct rule_state * list = 0;
	struct rule_state * * p;
	int i = 0;
	time_t current_time;

	/*ifname[0] = '\0';*/
	tmp = malloc(sizeof(struct rule_state));
	if(!tmp)
		return 0;
	current_time = time(NULL);
	nextruletoclean_timestamp = 0;
	while(get_redirect_rule_by_index(i, /*ifname*/0, &tmp->eport, 0, 0,
	                              &iport, &proto, 0, 0, 0,0, &timestamp,
								  &tmp->packets, &tmp->bytes) >= 0)
	{
		tmp->to_remove = 0;
		if(timestamp > 0) {
			/* need to remove this port mapping ? */
			if(timestamp <= current_time)
				tmp->to_remove = 1;
			else if((nextruletoclean_timestamp <= current_time)
			       || (timestamp < nextruletoclean_timestamp))
				nextruletoclean_timestamp = timestamp;
		}
		tmp->proto = (short)proto;
		/* add tmp to list */
		tmp->next = list;
		list = tmp;
		/* prepare next iteration */
		i++;
		tmp = malloc(sizeof(struct rule_state));
		if(!tmp)
			break;
	}
	free(tmp);
	/* remove the redirections that need to be removed */
	for(p = &list, tmp = list; tmp; tmp = *p)
	{
		if(tmp->to_remove)
		{
			syslog(LOG_NOTICE, "remove port mapping %hu %s because it has expired",
			       tmp->eport, (tmp->proto==IPPROTO_TCP)?"TCP":"UDP");
			_upnp_delete_redir(tmp->eport, tmp->proto);
			*p = tmp->next;
			free(tmp);
			i--;
		} else {
			p = &(tmp->next);
		}
	}
	/* return empty list if not enough redirections */
	if(i<=max_rules_number_target)
		while(list)
		{
			tmp = list;
			list = tmp->next;
			free(tmp);
		}
	/* return list */
	return list;
}

void
remove_unused_rules(struct rule_state * list)
{
	char ifname[IFNAMSIZ];
	unsigned short iport;
	struct rule_state * tmp;
	u_int64_t packets;
	u_int64_t bytes;
	unsigned int timestamp;
	int n = 0;

	while(list)
	{
		/* remove the rule if no traffic has used it */
		if(get_redirect_rule(ifname, list->eport, list->proto,
	                         0, 0, &iport, 0, 0, 0, 0, &timestamp,
		                     &packets, &bytes) >= 0)
		{
			if(packets == list->packets && bytes == list->bytes)
			{
				if(_upnp_delete_redir(list->eport, list->proto) >= 0)
					n++;
			}
		}
		tmp = list;
		list = tmp->next;
		free(tmp);
	}
	if(n>0)
		syslog(LOG_NOTICE, "removed %d unused rules", n);
}

/* upnp_get_portmappings_in_range()
 * return a list of all "external" ports for which a port
 * mapping exists */
unsigned short *
upnp_get_portmappings_in_range(unsigned short startport,
                               unsigned short endport,
                               const char * protocol,
                               unsigned int * number)
{
	int proto;
	proto = proto_atoi(protocol);
	if(!number)
		return NULL;
	return get_portmappings_in_range(startport, endport, proto, number);
}

#ifdef ENABLE_6FC_SERVICE
int
upnp_check_outbound_pinhole(int proto, int * timeout)
{
#if 0
	int s, tmptimeout, tmptime_out;
	switch(proto)
	{
		case IPPROTO_UDP:
			s = retrieve_timeout("udp_timeout", timeout);
			return s;
			break;
		case IPPROTO_UDPLITE:
			s = retrieve_timeout("udp_timeout_stream", timeout);
			return s;
			break;
		case IPPROTO_TCP:
			s = retrieve_timeout("tcp_timeout_established", timeout);
			return s;
			break;
		case 65535:
			s = retrieve_timeout("udp_timeout", timeout);
			s = retrieve_timeout("udp_timeout_stream", &tmptimeout);
			s = retrieve_timeout("tcp_timeout_established", &tmptime_out);
			if(tmptimeout<tmptime_out)
			{
				if(tmptimeout<*timeout)
					*timeout = tmptimeout;
			}
			else
			{
				if(tmptime_out<*timeout)
					*timeout = tmptimeout;
			}
			return s;
			break;
		default:
			return -5;
			break;
	}
#endif
	return 0;
}

/* upnp_add_inboundpinhole()
 * returns: 0 on success
 *          -1 failed to add pinhole
 *          -2 already created
 *          -3 inbound pinhole disabled
 */
int
upnp_add_inboundpinhole(const char * raddr,
                        unsigned short rport,
                        const char * iaddr,
                        unsigned short iport,
                        const char * protocol,
                        const char * leaseTime,
                        int * uid)
{
	int r, s, t, lt=0;
	char iaddr_old[40]="", proto[6]="", idfound[5]="", leaseTmp[12]; // IPv6 Modification
	snprintf(proto, sizeof(proto), "%.5d", atoi(protocol));
	unsigned short iport_old = 0;
	time_t current = time(NULL);
	/*struct in6_addr address; // IPv6 Modification
	if(inet_pton(AF_INET6, iaddr, &address) < 0) // IPv6 Modification
	{
		syslog(LOG_ERR, "inet_pton(%s) : %m", iaddr);
		return 0;
	}*/

#if 0
	r = get_rule_from_file(raddr, rport, iaddr_old, &iport_old, proto, 0, 0, idfound);
#endif
	r = 0;

	lt = (int) current + atoi(leaseTime);
	snprintf(leaseTmp, sizeof(leaseTmp), "%d", lt);
	printf("LeaseTime: %d / %d -> %s\n", atoi(leaseTime), (int)current, leaseTmp);

	printf("\tCompare addr: %s // port: %d\n\t     to addr: %s // port: %d\n", iaddr, iport, iaddr_old, iport_old);
	if(r == 1 && strcmp(iaddr, iaddr_old)==0 && iport==iport_old)
	{
		syslog(LOG_INFO, "Pinhole for inbound traffic from [%s]:%hu to [%s]:%hu with protocol %s already done. Updating it.", raddr, rport, iaddr_old, iport_old, protocol);
		t = upnp_update_inboundpinhole(idfound, leaseTime);
		*uid = atoi(idfound);
		return t;
	}
	else
	{
		syslog(LOG_INFO, "Adding pinhole for inbound traffic from [%s]:%hu to [%s]:%hu with protocol %s and %s lease time.", raddr, rport, iaddr, iport, protocol, leaseTime);
		s = upnp_add_inboundpinhole_internal(raddr, rport, iaddr, iport, protocol, uid);
#if 0
		if(rule_file_add(raddr, rport, iaddr, iport, protocol, leaseTmp, uid)<0)
			return -8;
		else
		{
			if(nextpinholetoclean_timestamp == 0 || (atoi(leaseTmp) <= nextpinholetoclean_timestamp))
			{
				printf("Initializing the nextpinholetoclean variables. uid = %d\n", *uid);
				snprintf(nextpinholetoclean_uid, 5, "%.4d", *uid);
				nextpinholetoclean_timestamp = atoi(leaseTmp);
			}
			return s;
		}
#endif
	}
return 0;
}

int
upnp_add_inboundpinhole_internal(const char * raddr, unsigned short rport,
                       const char * iaddr, unsigned short iport,
                       const char * proto, int * uid)
{
	int c = 9999;
	char cmd[256], cmd_raw[256], cuid[42];
#if 0
	static const char cmdval_full_udptcp[] = "ip6tables -I %s %d -p %s -i %s -s %s --sport %hu -d %s --dport %hu -j ACCEPT";
	static const char cmdval_udptcp[] = "ip6tables -I %s %d -p %s -i %s --sport %hu -d %s --dport %hu -j ACCEPT";
	static const char cmdval_full_udplite[] = "ip6tables -I %s %d -p %s -i %s -s %s -d %s -j ACCEPT";
	static const char cmdval_udplite[] = "ip6tables -I %s %d -p %s -i %s -d %s -j ACCEPT";
	// raw table command
	static const char cmdval_full_udptcp_raw[] = "ip6tables -t raw -I PREROUTING %d -p %s -i %s -s %s --sport %hu -d %s --dport %hu -j TRACE";
	static const char cmdval_udptcp_raw[] = "ip6tables -t raw -I PREROUTING %d -p %s -i %s --sport %hu -d %s --dport %hu -j TRACE";
	static const char cmdval_full_udplite_raw[] = "ip6tables -t raw -I PREROUTING %d -p %s -i %s -s %s -d %s -j TRACE";
	static const char cmdval_udplite_raw[] = "ip6tables -t raw -I PREROUTING %d -p %s -i %s -d %s -j TRACE";
#endif
	//printf("%s\n", raddr);
	if(raddr!=NULL)
	{
#ifdef IPPROTO_UDPLITE
		if(atoi(proto) == IPPROTO_UDPLITE)
		{
	/*		snprintf(cmd, sizeof(cmd), cmdval_full_udplite, miniupnpd_forward_chain, line_number, proto, ext_if_name, raddr, iaddr);
			snprintf(cmd_raw, sizeof(cmd_raw), cmdval_full_udplite_raw, line_number, proto, ext_if_name, raddr, iaddr);*/
		}
		else
#endif
		{
	/*		snprintf(cmd, sizeof(cmd), cmdval_full_udptcp, miniupnpd_forward_chain, line_number, proto, ext_if_name, raddr, rport, iaddr, iport);
			snprintf(cmd_raw, sizeof(cmd_raw), cmdval_full_udptcp_raw, line_number, proto, ext_if_name, raddr, rport, iaddr, iport);*/
		}
	}
	else
	{
#ifdef IPPROTO_UDPLITE
		if(atoi(proto) == IPPROTO_UDPLITE)
		{
			/*snprintf(cmd, sizeof(cmd), cmdval_udplite, miniupnpd_forward_chain, line_number, proto, ext_if_name, iaddr);
			snprintf(cmd_raw, sizeof(cmd_raw), cmdval_udplite_raw, line_number, proto, ext_if_name, iaddr);*/
		}
		else
#endif
		{
			/*snprintf(cmd, sizeof(cmd), cmdval_udptcp, miniupnpd_forward_chain, line_number, proto, ext_if_name, rport, iaddr, iport);
			snprintf(cmd_raw, sizeof(cmd_raw), cmdval_udptcp_raw, line_number, proto, ext_if_name, rport, iaddr, iport);
*/
		}
	}
#ifdef DEBUG
	syslog(LOG_INFO, "Adding following ip6tables rule:");
	syslog(LOG_INFO, "  -> %s", cmd);
	syslog(LOG_INFO, "  -> %s", cmd_raw);
#endif
	// TODO Add a better checking error.
	if(system(cmd) < 0 || system(cmd_raw) < 0)
	{
		return 0;
	}
	srand(time(NULL));
	snprintf(cuid, sizeof(cuid), "%.4d", rand()%c);
	*uid = atoi(cuid);
	printf("\t_add_ uid: %s\n", cuid);
	return 1;
}

int
upnp_get_pinhole_info(const char * raddr,
                      unsigned short rport,
                      char * iaddr,
                      unsigned short * iport,
                      char * proto,
                      const char * uid,
                      char * lt)
{
	/* TODO : to be done
	 * Call Firewall specific code to get IPv6 pinhole infos */
	return 0;
}

int
upnp_update_inboundpinhole(const char * uid, const char * leasetime)
{
	/* TODO : to be implemented */
#if 0
	int r, n;
	syslog(LOG_INFO, "Updating pinhole for inbound traffic with ID: %s", uid);
	r = check_rule_from_file(uid, 0);
	if(r < 0)
		return r;
	else
	{
		n = rule_file_update(uid, leasetime);
		upnp_update_expiredpinhole();
		return n;
	}
#else
	return -1;
#endif
}

int
upnp_delete_inboundpinhole(const char * uid)
{
	/* TODO : to be implemented */
#if 0
	/* this is a alpha implementation calling ip6tables via system(), 
	 * it can be usefull as an example to code the netfilter version */
	int r, s, linenum=0;
	char cmd[256], cmd_raw[256];
	syslog(LOG_INFO, "Removing pinhole for inbound traffic with ID: %s", uid);
	r = check_rule_from_file(uid, &linenum);
	if(r > 0)
	{
		s = rule_file_remove(uid, linenum);
		if(s < 0)
			return s;
		else
		{
			snprintf(cmd, sizeof(cmd), "ip6tables -t filter -D %s %d", miniupnpd_forward_chain, linenum);
			snprintf(cmd_raw, sizeof(cmd_raw), "ip6tables -t raw -D PREROUTING %d", linenum -1);
#ifdef DEBUG
			syslog(LOG_INFO, "Deleting ip6tables rule:");
			syslog(LOG_INFO, "  -> %s", cmd);
			syslog(LOG_INFO, "  -> %s", cmd_raw);
#endif
			// TODO Add a better checking error.
			if(system(cmd) < 0 || system(cmd_raw) < 0)
			{
				return 0;
			}
		}
	}
	upnp_update_expiredpinhole();
	return r;
#else
return -1;
#endif
}

/*
 * Result:
 * 	 1: Found Result
 * 	-4: No result
 * 	-5: Result in another table
 * 	-6: Result in another chain
 * 	-7: Result in a chain not a rule
*/
int
upnp_check_pinhole_working(const char * uid,
                           char * eaddr,
                           char * iaddr,
                           unsigned short * eport,
                           unsigned short * iport,
                           char * protocol,
                           int * rulenum_used)
{
	/* TODO : to be implemented */
#if 0
	FILE * fd;
	time_t expire = time(NULL);
	char buf[1024], filename[] = "/var/log/kern.log", expire_time[32]="";
	int res = -4, str_len;

	str_len = strftime(expire_time, sizeof(expire_time), "%b %d %H:%M:%S", localtime(&expire));

	fd = fopen(filename, "r");
	if (fd==NULL)
	{
		syslog(LOG_ERR, "Get_rule: could not open file: %s", filename);
		return -1;
	}

	syslog(LOG_INFO, "Get_rule: Starting getting info in file %s for %s\n", filename, uid);
	buf[sizeof(buf)-1] = 0;
	while(fgets(buf, sizeof(buf)-1, fd) != NULL && res != 1)
	{
		//printf("line: %s\n", buf);
		char * r, * t, * c, * p;
		// looking for something like filter:FORWARD:rule: or filter:MINIUPNPD:rule:
		r = strstr(buf, ":rule:");
		p = strstr(buf, ":policy:");
		t = strstr(buf, "TRACE:"); // table pointeur
		t += 7;
		c = t + 7; // chain pointeur
		if(r)
		{
			printf("\t** Found %.*s\n", 24 ,t);
			char * src, * dst, * sport, * dport, * proto, * line;
			char time[15]="", src_addr[40], dst_addr[40], proto_tmp[8];
			int proto_int;
			strncpy(time, buf, sizeof(time));
			/*if(compare_time(time, expire_time)<0)
			{
				printf("\t\tNot corresponding time\n");
				continue;
			}*/

			line = r + 6;
			printf("\trule line = %d\n", atoi(line));

			src = strstr(buf, "SRC=");
			src += 4;
			snprintf(src_addr, sizeof(src_addr), "%.*s", 39, src);
#if 0
			del_char(src_addr);
			add_char(src_addr);
#endif

			dst = strstr(buf, "DST=");
			dst += 4;
			snprintf(dst_addr, sizeof(dst_addr), "%.*s", 39, dst);
#if 0
			del_char(dst_addr);
			add_char(dst_addr);
#endif

			proto = strstr(buf, "PROTO=");
			proto += 6;
			proto_int = atoi(protocol);
			if(proto_int == IPPROTO_UDP)
				strcpy(proto_tmp, "UDP");
			else if(proto_int == IPPROTO_TCP)
				strcpy(proto_tmp, "TCP");
#ifdef IPPROTO_UDPLITE
			else if(proto_int == IPPROTO_UDPLITE)
				strcpy(proto_tmp, "UDPLITE");
#endif
			else
				strcpy(proto_tmp, "UnsupportedProto");

	//		printf("\tCompare eaddr: %s // protocol: %s\n\t     to  addr: %s // protocol: %.*s\n", eaddr, proto_tmp, src_addr, strlen(proto_tmp), proto);
	//		printf("\tCompare iaddr: %s // protocol: %s\n\t     to  addr: %s // protocol: %.*s\n", iaddr, proto_tmp, dst_addr, strlen(proto_tmp), proto);
			// TODO Check time
			// Check that the paquet found in trace correspond to the one we are looking for
			if( /*(strcmp(eaddr, src_addr) == 0) &&*/ (strcmp(iaddr, dst_addr) == 0) && (strncmp(proto_tmp, proto, strlen(proto_tmp))==0))
			{
				sport = strstr(buf, "SPT=");
				sport += 4;
				dport = strstr(buf, "DPT=");
				dport += 4;
				printf("\tCompare eport: %hu\n\t     to   port: %d\n", *eport, atoi(sport));
				printf("\tCompare iport: %hu\n\t     to   port: %d\n", *iport, atoi(dport));
				if(/*eport != atoi(sport) &&*/ *iport != atoi(dport))
				{
					printf("\t\tPort not corresponding\n");
					continue;
				}
				printf("\ttable found: %.*s\n", 6, t);
				printf("\tchain found: %.*s\n", 9, c);
				// Check that the table correspond to the filter table
				if(strncmp(t, "filter", 6)==0)
				{
					// Check that the table correspond to the MINIUPNP table
					if(strncmp(c, "MINIUPNPD", 9)==0)
					{
						*rulenum_used = atoi(line);
						res = 1;
					}
					else
					{
						res = -6;
						continue;
					}
				}
				else
				{
					res = -5;
					continue;
				}
			}
			else
			{
				printf("Packet information not corresponding\n");
				continue;
			}
		}
		if(!r && p)
		{
			printf("\t** Policy case\n");
			char * src, * dst, * sport, * dport, * proto, * line;
			char time[15], src_addr[40], dst_addr[40], proto_tmp[8];
			int proto_int;
			strncpy(time, buf, sizeof(time));
			/*if(compare_time(time, expire_time)<0)
			{
				printf("\t\tNot corresponding time\n");
				continue;
			}*/

			line = p + 8;
			printf("\trule line = %d\n", atoi(line));

			src = strstr(buf, "SRC=");
			src += 4;
			snprintf(src_addr, sizeof(src_addr), "%.*s", 39, src);
#if 0
			del_char(src_addr);
			add_char(src_addr);
#endif

			dst = strstr(buf, "DST=");
			dst += 4;
			snprintf(dst_addr, sizeof(dst_addr), "%.*s", 39, dst);
#if 0
			del_char(dst_addr);
			add_char(dst_addr);
#endif

			proto = strstr(buf, "PROTO=");
			proto += 6;
			proto_int = atoi(protocol);
			if(proto_int == IPPROTO_UDP)
				strcpy(proto_tmp, "UDP");
			else if(proto_int == IPPROTO_TCP)
				strcpy(proto_tmp, "TCP");
#ifdef IPPROTO_UDPLITE
			else if(proto_int == IPPROTO_UDPLITE)
				strcpy(proto_tmp, "UDPLITE");
#endif
			else
				strcpy(proto_tmp, "UnsupportedProto");

	//		printf("\tCompare eaddr: %s // protocol: %s\n\t     to  addr: %s // protocol: %.*s\n", eaddr, proto_tmp, src_addr, strlen(proto_tmp), proto);
	//		printf("\tCompare iaddr: %s // protocol: %s\n\t     to  addr: %s // protocol: %.*s\n", iaddr, proto_tmp, dst_addr, strlen(proto_tmp), proto);
			// Check that the paquet found in trace correspond to the one we are looking for
			if( (strcmp(eaddr, src_addr) == 0) && (strcmp(iaddr, dst_addr) == 0) && (strncmp(proto_tmp, proto, 5)==0))
			{
				sport = strstr(buf, "SPT=");
				sport += 4;
				dport = strstr(buf, "DPT=");
				dport += 4;
				printf("\tCompare eport: %hu\n\t     to   port: %d\n", *eport, atoi(sport));
				printf("\tCompare iport: %hu\n\t     to   port: %d\n", *iport, atoi(dport));
				if(*eport != atoi(sport) && *iport != atoi(dport))
				{
					printf("\t\tPort not corresponding\n");
					continue;
				}
				else
				{
					printf("Find a corresponding policy trace in the chain: %.*s\n", 10, c);
					res = -7;
					continue;
				}
			}
			else
				continue;
		}
	}
	fclose(fd);
	return res;
#else
	return -4;
#endif
}

int
upnp_get_pinhole_packets(const char * uid, int * packets)
{
	/* TODO : to be implemented */
#if 0
	int line=0, r;
	char cmd[256];
	r = check_rule_from_file(uid, &line);
	if(r < 0)
		return r;
	else
	{
		snprintf(cmd, sizeof(cmd), "ip6tables -L MINIUPNPD %d -v", line);
		return retrieve_packets(cmd, &line, packets);
	}
#else
	return 0;
#endif
}

int
upnp_update_expiredpinhole(void)
{
#if 0
	int r;
	char uid[5], leaseTime[12];

	r = get_rule_from_leasetime(uid, leaseTime);
	if(r<0)
		return r;
	else
	{
		strcpy(nextpinholetoclean_uid, uid);
		nextpinholetoclean_timestamp = atoi(leaseTime);
		return 1;
	}
#endif
	return 0;
}

int
upnp_clean_expiredpinhole()
{
#if 0
	upnp_delete_inboundpinhole(nextpinholetoclean_uid);

	return upnp_update_expiredpinhole();
#endif
	return 0;
}
#endif

/* stuff for miniupnpdctl */
#ifdef USE_MINIUPNPDCTL
void
write_ruleset_details(int s)
{
	int proto = 0;
	unsigned short eport, iport;
	char desc[64];
	char iaddr[32];
	char rhost[32];
	unsigned int timestamp;
	u_int64_t packets;
	u_int64_t bytes;
	int i = 0;
	char buffer[256];
	int n;

	write(s, "Ruleset :\n", 10);
	while(get_redirect_rule_by_index(i, 0/*ifname*/, &eport, iaddr, sizeof(iaddr),
	                                 &iport, &proto, desc, sizeof(desc),
	                                 rhost, sizeof(rhost),
	                                 &timestamp,
	                                 &packets, &bytes) >= 0)
	{
		n = snprintf(buffer, sizeof(buffer),
		             "%2d %s %s:%hu->%s:%hu "
		             "'%s' %u %" PRIu64 " %" PRIu64 "\n",
		             /*"'%s' %llu %llu\n",*/
		             i, proto==IPPROTO_TCP?"TCP":"UDP", rhost,
		             eport, iaddr, iport, desc, timestamp, packets, bytes);
		write(s, buffer, n);
		i++;
	}
}
#endif

