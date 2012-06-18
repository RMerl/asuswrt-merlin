/*
 * multilink.c - support routines for multilink.
 *
 * Copyright (c) 2000 Paul Mackerras.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms.  The name of the author may not be
 * used to endorse or promote products derived from this software
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "tdb.h"

bool endpoint_specified;	/* user gave explicit endpoint discriminator */
char *bundle_id;		/* identifier for our bundle */

extern TDB_CONTEXT *pppdb;
extern char db_key[];

static int get_default_epdisc __P((struct epdisc *));
static int parse_num __P((char *str, const char *key, int *valp));
static int owns_unit __P((TDB_DATA pid, int unit));

#define set_ip_epdisc(ep, addr) do {	\
	ep->length = 4;			\
	ep->value[0] = addr >> 24;	\
	ep->value[1] = addr >> 16;	\
	ep->value[2] = addr >> 8;	\
	ep->value[3] = addr;		\
} while (0)

#define LOCAL_IP_ADDR(addr)						  \
	(((addr) & 0xff000000) == 0x0a000000		/* 10.x.x.x */	  \
	 || ((addr) & 0xfff00000) == 0xac100000		/* 172.16.x.x */  \
	 || ((addr) & 0xffff0000) == 0xc0a80000)	/* 192.168.x.x */

#define process_exists(n)	(kill((n), 0) == 0 || errno != ESRCH)

void
mp_check_options()
{
	lcp_options *wo = &lcp_wantoptions[0];
	lcp_options *ao = &lcp_allowoptions[0];

	if (!multilink)
		return;
	/* if we're doing multilink, we have to negotiate MRRU */
	if (!wo->neg_mrru) {
		/* mrru not specified, default to mru */
		wo->mrru = wo->mru;
		wo->neg_mrru = 1;
	}
	ao->mrru = ao->mru;
	ao->neg_mrru = 1;

	if (!wo->neg_endpoint && !noendpoint) {
		/* get a default endpoint value */
		wo->neg_endpoint = get_default_epdisc(&wo->endpoint);
	}
}

/*
 * Make a new bundle or join us to an existing bundle
 * if we are doing multilink.
 */
int
mp_join_bundle()
{
	lcp_options *go = &lcp_gotoptions[0];
	lcp_options *ho = &lcp_hisoptions[0];
	lcp_options *ao = &lcp_allowoptions[0];
	int unit, pppd_pid;
	int l, mtu;
	char *p;
	TDB_DATA key, pid, rec;

	if (!go->neg_mrru || !ho->neg_mrru) {
		/* not doing multilink */
		if (go->neg_mrru)
			notice("oops, multilink negotiated only for receive");
		mtu = ho->neg_mru? ho->mru: PPP_MRU;
		if (mtu > ao->mru)
			mtu = ao->mru;
		if (demand) {
			/* already have a bundle */
			cfg_bundle(0, 0, 0, 0);
			netif_set_mtu(0, mtu);
			return 0;
		}
		make_new_bundle(0, 0, 0, 0);
		set_ifunit(1);
		netif_set_mtu(0, mtu);
		return 0;
	}

	/*
	 * Find the appropriate bundle or join a new one.
	 * First we make up a name for the bundle.
	 * The length estimate is worst-case assuming every
	 * character has to be quoted.
	 */
	l = 4 * strlen(peer_authname) + 10;
	if (ho->neg_endpoint)
		l += 3 * ho->endpoint.length + 8;
	if (bundle_name)
		l += 3 * strlen(bundle_name) + 2;
	bundle_id = malloc(l);
	if (bundle_id == 0)
		novm("bundle identifier");

	p = bundle_id;
	p += slprintf(p, l-1, "BUNDLE=\"%q\"", peer_authname);
	if (ho->neg_endpoint || bundle_name)
		*p++ = '/';
	if (ho->neg_endpoint)
		p += slprintf(p, bundle_id+l-p, "%s",
			      epdisc_to_str(&ho->endpoint));
	if (bundle_name)
		p += slprintf(p, bundle_id+l-p, "/%v", bundle_name);

	/*
	 * For demand mode, we only need to configure the bundle
	 * and attach the link.
	 */
	mtu = MIN(ho->mrru, ao->mru);
	if (demand) {
		cfg_bundle(go->mrru, ho->mrru, go->neg_ssnhf, ho->neg_ssnhf);
		netif_set_mtu(0, mtu);
		script_setenv("BUNDLE", bundle_id + 7, 1);
		return 0;
	}

	/*
	 * Check if the bundle ID is already in the database.
	 */
	unit = -1;
	tdb_writelock(pppdb);
	key.dptr = bundle_id;
	key.dsize = p - bundle_id;
	pid = tdb_fetch(pppdb, key);
	if (pid.dptr != NULL) {
		/* bundle ID exists, see if the pppd record exists */
		rec = tdb_fetch(pppdb, pid);
		if (rec.dptr != NULL) {
			/* it is, parse the interface number */
			parse_num(rec.dptr, "IFNAME=ppp", &unit);
			/* check the pid value */
			if (!parse_num(rec.dptr, "PPPD_PID=", &pppd_pid)
			    || !process_exists(pppd_pid)
			    || !owns_unit(pid, unit))
				unit = -1;
			free(rec.dptr);
		}
		free(pid.dptr);
	}

	if (unit >= 0) {
		/* attach to existing unit */
		if (bundle_attach(unit)) {
			set_ifunit(0);
			script_setenv("BUNDLE", bundle_id + 7, 0);
			tdb_writeunlock(pppdb);
			info("Link attached to %s", ifname);
			return 1;
		}
		/* attach failed because bundle doesn't exist */
	}

	/* we have to make a new bundle */
	make_new_bundle(go->mrru, ho->mrru, go->neg_ssnhf, ho->neg_ssnhf);
	set_ifunit(1);
	netif_set_mtu(0, mtu);
	script_setenv("BUNDLE", bundle_id + 7, 1);
	tdb_writeunlock(pppdb);
	info("New bundle %s created", ifname);
	return 0;
}

static int
parse_num(str, key, valp)
     char *str;
     const char *key;
     int *valp;
{
	char *p, *endp;
	int i;

	p = strstr(str, key);
	if (p != 0) {
		p += strlen(key);
		i = strtol(p, &endp, 10);
		if (endp != p && (*endp == 0 || *endp == ';')) {
			*valp = i;
			return 1;
		}
	}
	return 0;
}

/*
 * Check whether the pppd identified by `key' still owns ppp unit `unit'.
 */
static int
owns_unit(key, unit)
     TDB_DATA key;
     int unit;
{
	char ifkey[32];
	TDB_DATA kd, vd;
	int ret = 0;

	slprintf(ifkey, sizeof(ifkey), "IFNAME=ppp%d", unit);
	kd.dptr = ifkey;
	kd.dsize = strlen(ifkey);
	vd = tdb_fetch(pppdb, kd);
	if (vd.dptr != NULL) {
		ret = vd.dsize == key.dsize
			&& memcmp(vd.dptr, key.dptr, vd.dsize) == 0;
		free(vd.dptr);
	}
	return ret;
}

static int
get_default_epdisc(ep)
     struct epdisc *ep;
{
	char *p;
	struct hostent *hp;
	u_int32_t addr;

	/* First try for an ethernet MAC address */
	p = get_first_ethernet();
	if (p != 0 && get_if_hwaddr(ep->value, p) >= 0) {
		ep->class = EPD_MAC;
		ep->length = 6;
		return 1;
	}

	/* see if our hostname corresponds to a reasonable IP address */
	hp = gethostbyname(hostname);
	if (hp != NULL) {
		addr = *(u_int32_t *)hp->h_addr;
		if (!bad_ip_adrs(addr)) {
			addr = ntohl(addr);
			if (!LOCAL_IP_ADDR(addr)) {
				ep->class = EPD_IP;
				set_ip_epdisc(ep, addr);
				return 1;
			}
		}
	}

	return 0;
}

/*
 * epdisc_to_str - make a printable string from an endpoint discriminator.
 */

static char *endp_class_names[] = {
    "null", "local", "IP", "MAC", "magic", "phone"
};

char *
epdisc_to_str(ep)
     struct epdisc *ep;
{
	static char str[MAX_ENDP_LEN*3+8];
	u_char *p = ep->value;
	int i, mask = 0;
	char *q, c, c2;

	if (ep->class == EPD_NULL && ep->length == 0)
		return "null";
	if (ep->class == EPD_IP && ep->length == 4) {
		u_int32_t addr;

		GETLONG(addr, p);
		slprintf(str, sizeof(str), "IP:%I", htonl(addr));
		return str;
	}

	c = ':';
	c2 = '.';
	if (ep->class == EPD_MAC && ep->length == 6)
		c2 = ':';
	else if (ep->class == EPD_MAGIC && (ep->length % 4) == 0)
		mask = 3;
	q = str;
	if (ep->class <= EPD_PHONENUM)
		q += slprintf(q, sizeof(str)-1, "%s",
			      endp_class_names[ep->class]);
	else
		q += slprintf(q, sizeof(str)-1, "%d", ep->class);
	c = ':';
	for (i = 0; i < ep->length && i < MAX_ENDP_LEN; ++i) {
		if ((i & mask) == 0) {
			*q++ = c;
			c = c2;
		}
		q += slprintf(q, str + sizeof(str) - q, "%.2x", ep->value[i]);
	}
	return str;
}

static int hexc_val(int c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	if (c >= 'A')
		return c - 'A' + 10;
	return c - '0';
}

int
str_to_epdisc(ep, str)
     struct epdisc *ep;
     char *str;
{
	int i, l;
	char *p, *endp;

	for (i = EPD_NULL; i <= EPD_PHONENUM; ++i) {
		int sl = strlen(endp_class_names[i]);
		if (strncasecmp(str, endp_class_names[i], sl) == 0) {
			str += sl;
			break;
		}
	}
	if (i > EPD_PHONENUM) {
		/* not a class name, try a decimal class number */
		i = strtol(str, &endp, 10);
		if (endp == str)
			return 0;	/* can't parse class number */
		str = endp;
	}
	ep->class = i;
	if (*str == 0) {
		ep->length = 0;
		return 1;
	}
	if (*str != ':' && *str != '.')
		return 0;
	++str;

	if (i == EPD_IP) {
		u_int32_t addr;
		i = parse_dotted_ip(str, &addr);
		if (i == 0 || str[i] != 0)
			return 0;
		set_ip_epdisc(ep, addr);
		return 1;
	}
	if (i == EPD_MAC && get_if_hwaddr(ep->value, str) >= 0) {
		ep->length = 6;
		return 1;
	}

	p = str;
	for (l = 0; l < MAX_ENDP_LEN; ++l) {
		if (*str == 0)
			break;
		if (p <= str)
			for (p = str; isxdigit(*p); ++p)
				;
		i = p - str;
		if (i == 0)
			return 0;
		ep->value[l] = hexc_val(*str++);
		if ((i & 1) == 0)
			ep->value[l] = (ep->value[l] << 4) + hexc_val(*str++);
		if (*str == ':' || *str == '.')
			++str;
	}
	if (*str != 0 || (ep->class == EPD_MAC && l != 6))
		return 0;
	ep->length = l;
	return 1;
}

