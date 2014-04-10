/* $Id: upnppermissions.h,v 1.10 2014/03/07 10:43:29 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef UPNPPERMISSIONS_H_INCLUDED
#define UPNPPERMISSIONS_H_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config.h"

/* UPnP permission rule samples:
 * allow 1024-65535 192.168.3.0/24 1024-65535
 * deny 0-65535 192.168.1.125/32 0-65535 */
struct upnpperm {
	enum {UPNPPERM_ALLOW=1, UPNPPERM_DENY=2 } type;
				/* is it an allow or deny permission rule ? */
	u_short eport_min, eport_max;	/* external port range */
	struct in_addr address, mask;	/* ip/mask */
	u_short iport_min, iport_max;	/* internal port range */
};

/* read_permission_line()
 * returns: 0 line read okay
 *          -1 error reading line
 *
 * line sample :
 *  allow 1024-65535 192.168.3.0/24 1024-65535
 *  allow 22 192.168.4.33/32 22
 *  deny 0-65535 0.0.0.0/0 0-65535 */
int
read_permission_line(struct upnpperm * perm,
                     char * p);

/* check_upnp_rule_against_permissions()
 * returns: 0 if the upnp rule should be rejected,
 *          1 if it could be accepted */
int
check_upnp_rule_against_permissions(const struct upnpperm * permary,
                                    int n_perms,
                                    u_short eport, struct in_addr address,
                                    u_short iport);

#ifdef USE_MINIUPNPDCTL
void
write_permlist(int fd, const struct upnpperm * permary,
               int nperms);
#endif

#endif

