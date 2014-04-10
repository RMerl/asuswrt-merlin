/* $Id: upnppermissions.c,v 1.18 2014/03/07 10:43:29 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "config.h"
#include "upnppermissions.h"

/* read_permission_line()
 * parse the a permission line which format is :
 * (deny|allow) [0-9]+(-[0-9]+) ip/mask [0-9]+(-[0-9]+)
 * ip/mask is either 192.168.1.1/24 or 192.168.1.1/255.255.255.0
 */
int
read_permission_line(struct upnpperm * perm,
                     char * p)
{
	char * q;
	int n_bits;
	int i;

	/* first token: (allow|deny) */
	while(isspace(*p))
		p++;
	if(0 == memcmp(p, "allow", 5))
	{
		perm->type = UPNPPERM_ALLOW;
		p += 5;
	}
	else if(0 == memcmp(p, "deny", 4))
	{
		perm->type = UPNPPERM_DENY;
		p += 4;
	}
	else
	{
		return -1;
	}
	while(isspace(*p))
		p++;

	/* second token: eport or eport_min-eport_max */
	if(!isdigit(*p))
		return -1;
	for(q = p; isdigit(*q); q++);
	if(*q=='-')
	{
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->eport_min = (u_short)i;
		q++;
		p = q;
		while(isdigit(*q))
			q++;
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->eport_max = (u_short)i;
		if(perm->eport_min > perm->eport_max)
			return -1;
	}
	else if(isspace(*q))
	{
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->eport_min = perm->eport_max = (u_short)i;
	}
	else
	{
		return -1;
	}
	p = q + 1;
	while(isspace(*p))
		p++;

	/* third token:  ip/mask */
	if(!isdigit(*p))
		return -1;
	for(q = p; isdigit(*q) || (*q == '.'); q++);
	if(*q=='/')
	{
		*q = '\0';
		if(!inet_aton(p, &perm->address))
			return -1;
		q++;
		p = q;
		while(isdigit(*q))
			q++;
		if(*q == '.')
		{
			while(*q == '.' || isdigit(*q))
				q++;
			if(!isspace(*q))
				return -1;
			*q = '\0';
			if(!inet_aton(p, &perm->mask))
				return -1;
		}
		else if(!isspace(*q))
			return -1;
		else
		{
			*q = '\0';
			n_bits = atoi(p);
			if(n_bits > 32)
				return -1;
			perm->mask.s_addr = htonl(n_bits ? (0xffffffffu << (32 - n_bits)) : 0);
		}
	}
	else if(isspace(*q))
	{
		*q = '\0';
		if(!inet_aton(p, &perm->address))
			return -1;
		perm->mask.s_addr = 0xffffffffu;
	}
	else
	{
		return -1;
	}
	p = q + 1;

	/* fourth token: iport or iport_min-iport_max */
	while(isspace(*p))
		p++;
	if(!isdigit(*p))
		return -1;
	for(q = p; isdigit(*q); q++);
	if(*q=='-')
	{
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->iport_min = (u_short)i;
		q++;
		p = q;
		while(isdigit(*q))
			q++;
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->iport_max = (u_short)i;
		if(perm->iport_min > perm->iport_max)
			return -1;
	}
	else if(isspace(*q) || *q == '\0')
	{
		*q = '\0';
		i = atoi(p);
		if(i > 65535)
			return -1;
		perm->iport_min = perm->iport_max = (u_short)i;
	}
	else
	{
		return -1;
	}
#ifdef DEBUG
	printf("perm rule added : %s %hu-%hu %08x/%08x %hu-%hu\n",
	       (perm->type==UPNPPERM_ALLOW)?"allow":"deny",
	       perm->eport_min, perm->eport_max, ntohl(perm->address.s_addr),
	       ntohl(perm->mask.s_addr), perm->iport_min, perm->iport_max);
#endif
	return 0;
}

#ifdef USE_MINIUPNPDCTL
void
write_permlist(int fd, const struct upnpperm * permary,
               int nperms)
{
	int l;
	const struct upnpperm * perm;
	int i;
	char buf[128];
	write(fd, "Permissions :\n", 14);
	for(i = 0; i<nperms; i++)
	{
		perm = permary + i;
		l = snprintf(buf, sizeof(buf), "%02d %s %hu-%hu %08x/%08x %hu-%hu\n",
	       i,
    	   (perm->type==UPNPPERM_ALLOW)?"allow":"deny",
	       perm->eport_min, perm->eport_max, ntohl(perm->address.s_addr),
	       ntohl(perm->mask.s_addr), perm->iport_min, perm->iport_max);
		if(l<0)
			return;
		write(fd, buf, l);
	}
}
#endif

/* match_permission()
 * returns: 1 if eport, address, iport matches the permission rule
 *          0 if no match */
static int
match_permission(const struct upnpperm * perm,
                 u_short eport, struct in_addr address, u_short iport)
{
	if( (eport < perm->eport_min) || (perm->eport_max < eport))
		return 0;
	if( (iport < perm->iport_min) || (perm->iport_max < iport))
		return 0;
	if( (address.s_addr & perm->mask.s_addr)
	   != (perm->address.s_addr & perm->mask.s_addr) )
		return 0;
	return 1;
}

#if 0
/* match_permission_internal()
 * returns: 1 if address, iport matches the permission rule
 *          0 if no match */
static int
match_permission_internal(const struct upnpperm * perm,
                          struct in_addr address, u_short iport)
{
	if( (iport < perm->iport_min) || (perm->iport_max < iport))
		return 0;
	if( (address.s_addr & perm->mask.s_addr)
	   != (perm->address.s_addr & perm->mask.s_addr) )
		return 0;
	return 1;
}
#endif

int
check_upnp_rule_against_permissions(const struct upnpperm * permary,
                                    int n_perms,
                                    u_short eport, struct in_addr address,
                                    u_short iport)
{
	int i;
	for(i=0; i<n_perms; i++)
	{
		if(match_permission(permary + i, eport, address, iport))
		{
			syslog(LOG_DEBUG,
			       "UPnP permission rule %d matched : port mapping %s",
			       i, (permary[i].type == UPNPPERM_ALLOW)?"accepted":"rejected"
			       );
			return (permary[i].type == UPNPPERM_ALLOW);
		}
	}
	syslog(LOG_DEBUG, "no permission rule matched : accept by default (n_perms=%d)", n_perms);
	return 1;	/* Default : accept */
}

