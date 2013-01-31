/* $Id: upnpredirect.h,v 1.24 2011/06/22 20:34:39 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __UPNPREDIRECT_H__
#define __UPNPREDIRECT_H__

/* for u_int64_t */
#include <sys/types.h>

#include "config.h"

#ifdef ENABLE_LEASEFILE
int reload_from_lease_file(void);
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
              unsigned int leaseduration);

/* upnp_redirect_internal()
 * same as upnp_redirect() without any check */
int
upnp_redirect_internal(const char * rhost, unsigned short eport,
                       const char * iaddr, unsigned short iport,
                       int proto, const char * desc,
                       unsigned int timestamp);

/* upnp_get_redirection_infos()
 * returns : 0 on success
 *           -1 failed to get the port mapping entry or no entry exists */
int
upnp_get_redirection_infos(unsigned short eport, const char * protocol,
                           unsigned short * iport, char * iaddr, int iaddrlen,
                           char * desc, int desclen,
                           char * rhost, int rhostlen,
                           unsigned int * leaseduration);

/* upnp_get_redirection_infos_by_index()
 * returns : 0 on success
 *           -1 failed to get the port mapping or index out of range */
int
upnp_get_redirection_infos_by_index(int index,
                                    unsigned short * eport, char * protocol,
                                    unsigned short * iport, 
                                    char * iaddr, int iaddrlen,
                                    char * desc, int desclen,
                                    char * rhost, int rhostlen,
                                    unsigned int * leaseduration);

/* upnp_delete_redirection()
 * returns: 0 on success
 *          -1 on failure*/
int
upnp_delete_redirection(unsigned short eport, const char * protocol);

/* _upnp_delete_redir()
 * same as above */
int
_upnp_delete_redir(unsigned short eport, int proto);

/* Periodic cleanup functions
 */
struct rule_state
{
	u_int64_t packets;
	u_int64_t bytes;
	struct rule_state * next;
	unsigned short eport;
	unsigned char proto;
	unsigned char to_remove; 
};

/* return a linked list of all rules
 * or an empty list if there are not enough
 * As a "side effect", delete rules which are expired */
struct rule_state *
get_upnp_rules_state_list(int max_rules_number_target);

/* return the number of port mapping entries */
int
upnp_get_portmapping_number_of_entries(void);

/* remove_unused_rules() :
 * also free the list */
void
remove_unused_rules(struct rule_state * list);

/* upnp_get_portmappings_in_range()
 * return a list of all "external" ports for which a port
 * mapping exists */
unsigned short *
upnp_get_portmappings_in_range(unsigned short startport,
                               unsigned short endport,
                               const char * protocol,
                               unsigned int * number);

#ifdef ENABLE_6FC_SERVICE
/* function to be used by WANIPv6_FirewallControl implementation */

/* retreive outbound pinhole timeout*/
int
upnp_check_outbound_pinhole(int proto, int * timeout);

/* add an inbound pinehole
 * return value :
 *  1 = success
 * -1 = Pinhole space exhausted
 * .. = error */
int
upnp_add_inboundpinhole(const char * raddr, unsigned short rport,
              const char * iaddr, unsigned short iport,
              const char * protocol, const char * leaseTime, int * uid);

int
upnp_add_inboundpinhole_internal(const char * raddr, unsigned short rport,
                       const char * iaddr, unsigned short iport,
                       const char * proto, int * uid);

/*
 * return values :
 *  -4 not found
 *  -5 in another table
 *  -6 in another chain
 *  -7 in a chain but not a rule. (chain policy)
 * */
int
upnp_get_pinhole_info(const char * raddr, unsigned short rport, char * iaddr, unsigned short * iport, char * proto, const char * uid, char * lt);

/* update the lease time */
int
upnp_update_inboundpinhole(const char * uid, const char * leasetime);

/* remove the inbound pinhole */
int
upnp_delete_inboundpinhole(const char * uid);

/* ... */
int
upnp_check_pinhole_working(const char * uid, char * eaddr, char * iaddr, unsigned short * eport, unsigned short * iport, char * protocol, int * rulenum_used);

/* number of packets that went through the pinhole */
int
upnp_get_pinhole_packets(const char * uid, int * packets);

/* ? */
int
upnp_clean_expiredpinhole(void);

#endif /* ENABLE_6FC_SERVICE */

/* stuff for responding to miniupnpdctl */
#ifdef USE_MINIUPNPDCTL
void
write_ruleset_details(int s);
#endif

#endif


