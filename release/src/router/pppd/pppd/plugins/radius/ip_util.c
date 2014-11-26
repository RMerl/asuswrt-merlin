/*
 * $Id: ip_util.c,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <includes.h>
#include <radiusclient.h>

/*
 * Function: rc_get_ipaddr
 *
 * Purpose: return an IP address in host long notation from a host
 *          name or address in dot notation.
 *
 * Returns: 0 on failure
 */

UINT4 rc_get_ipaddr (char *host)
{
	struct hostent *hp;

	if (rc_good_ipaddr (host) == 0)
	{
		return ntohl(inet_addr (host));
	}
	else if ((hp = gethostbyname (host)) == (struct hostent *) NULL)
	{
		error("rc_get_ipaddr: couldn't resolve hostname: %s", host);
		return ((UINT4) 0);
	}
	return ntohl((*(UINT4 *) hp->h_addr));
}

/*
 * Function: rc_good_ipaddr
 *
 * Purpose: check for valid IP address in standard dot notation.
 *
 * Returns: 0 on success, -1 when failure
 *
 */

int rc_good_ipaddr (char *addr)
{
	int             dot_count;
	int             digit_count;

	if (addr == NULL)
		return (-1);

	dot_count = 0;
	digit_count = 0;
	while (*addr != '\0' && *addr != ' ')
	{
		if (*addr == '.')
		{
			dot_count++;
			digit_count = 0;
		}
		else if (!isdigit (*addr))
		{
			dot_count = 5;
		}
		else
		{
			digit_count++;
			if (digit_count > 3)
			{
				dot_count = 5;
			}
		}
		addr++;
	}
	if (dot_count != 3)
	{
		return (-1);
	}
	else
	{
		return (0);
	}
}

/*
 * Function: rc_ip_hostname
 *
 * Purpose: Return a printable host name (or IP address in dot notation)
 *	    for the supplied IP address.
 *
 */

const char *rc_ip_hostname (UINT4 h_ipaddr)
{
	struct hostent  *hp;
	UINT4           n_ipaddr = htonl (h_ipaddr);

	if ((hp = gethostbyaddr ((char *) &n_ipaddr, sizeof (struct in_addr),
			    AF_INET)) == NULL) {
		error("rc_ip_hostname: couldn't look up host by addr: %08lX", h_ipaddr);
	}

	return ((hp==NULL)?"unknown":hp->h_name);
}

/*
 * Function: rc_own_ipaddress
 *
 * Purpose: get the IP address of this host in host order
 *
 * Returns: IP address on success, 0 on failure
 *
 */

UINT4 rc_own_ipaddress(void)
{
	static UINT4 this_host_ipaddr = 0;

	if (!this_host_ipaddr) {
		if ((this_host_ipaddr = rc_get_ipaddr (hostname)) == 0) {
			error("rc_own_ipaddress: couldn't get own IP address");
			return 0;
		}
	}

	return this_host_ipaddr;
}

/*
 * Function: rc_own_bind_ipaddress
 *
 * Purpose: get the IP address to be used as a source address
 *          for sending requests in host order
 *
 * Returns: IP address
 *
 */

UINT4 rc_own_bind_ipaddress(void)
{
	char *bindaddr;
	UINT4 rval = 0;

	if ((bindaddr = rc_conf_str("bindaddr")) == NULL ||
	    strcmp(rc_conf_str("bindaddr"), "*") == 0) {
		rval = INADDR_ANY;
	} else {
		if ((rval = rc_get_ipaddr(bindaddr)) == 0) {
			error("rc_own_bind_ipaddress: couldn't get IP address from bindaddr");
			rval = INADDR_ANY;
		}
	}

	return rval;
}
