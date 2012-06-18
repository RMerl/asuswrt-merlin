/* $Id: upnpredirect.h,v 1.15 2009/02/14 11:01:14 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __UPNPREDIRECT_H__
#define __UPNPREDIRECT_H__

#include "config.h"

#ifdef ENABLE_LEASEFILE
int reload_from_lease_file();
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
upnp_redirect(unsigned short eport, 
              const char * iaddr, unsigned short iport,
              const char * protocol, const char * desc);

/* upnp_redirect_internal()
 * same as upnp_redirect() without any check */
int
upnp_redirect_internal(unsigned short eport,
                       const char * iaddr, unsigned short iport,
                       int proto, const char * desc);

/* upnp_get_redirection_infos() */
int
upnp_get_redirection_infos(unsigned short eport, const char * protocol,
                           unsigned short * iport, char * iaddr, int iaddrlen,
                           char * desc, int desclen);

/* upnp_get_redirection_infos_by_index */
int
upnp_get_redirection_infos_by_index(int index,
                                    unsigned short * eport, char * protocol,
                                    unsigned short * iport, 
                                    char * iaddr, int iaddrlen,
                                    char * desc, int desclen);

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
	short proto;
};

struct rule_state *
get_upnp_rules_state_list(int max_rules_number_target);

int
upnp_get_portmapping_number_of_entries();

/* remove_unused_rules() :
 * also free the list */
void
remove_unused_rules(struct rule_state * list);

/* stuff for responding to miniupnpdctl */
#ifdef USE_MINIUPNPDCTL
void
write_ruleset_details(int s);
#endif

#endif


