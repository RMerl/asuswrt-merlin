/*
 * support/export/client.c
 *
 * Maintain list of nfsd clients.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include "xmalloc.h"
#include "misc.h"
#include "nfslib.h"
#include "exportfs.h"

/* netgroup stuff never seems to be defined in any header file. Linux is
 * not alone in this.
 */
#if !defined(__GLIBC__) || __GLIBC__ < 2
extern int	innetgr(char *netgr, char *host, char *, char *);
#endif
static void	client_init(nfs_client *clp, const char *hname,
					struct hostent *hp);
static int	client_checkaddr(nfs_client *clp, struct in_addr addr);

nfs_client	*clientlist[MCL_MAXTYPES] = { NULL, };


/* if canonical is set, then we *know* this is already a canonical name
 * so hostname lookup is avoided.
 * This is used when reading /proc/fs/nfs/exports
 */
nfs_client *
client_lookup(char *hname, int canonical)
{
	nfs_client	*clp = NULL;
	int		htype;
	struct hostent	*hp = NULL;

	htype = client_gettype(hname);

	if (htype == MCL_FQDN && !canonical) {
		struct hostent *hp2;
		hp = gethostbyname(hname);
		if (hp == NULL || hp->h_addrtype != AF_INET) {
			xlog(L_ERROR, "%s has non-inet addr", hname);
			return NULL;
		}
		/* make sure we have canonical name */
		hp2 = hostent_dup(hp);
		hp = gethostbyaddr(hp2->h_addr, hp2->h_length,
				   hp2->h_addrtype);
		if (hp) {
			hp = hostent_dup(hp);
			/* but now we might not have all addresses... */
			if (hp2->h_addr_list[1]) {
				struct hostent *hp3 =
					gethostbyname(hp->h_name);
				if (hp3) {
					free(hp);
					hp = hostent_dup(hp3);
				}
			}
			free(hp2);
		} else
			hp = hp2;

		hname = (char *) hp->h_name;

		for (clp = clientlist[htype]; clp; clp = clp->m_next) {
			if (client_check(clp, hp))
				break;
		}
	} else {
		for (clp = clientlist[htype]; clp; clp = clp->m_next) {
			if (strcasecmp(hname, clp->m_hostname)==0)
				break;
		}
	}

	if (!clp) {
		clp = (nfs_client *) xmalloc(sizeof(*clp));
		memset(clp, 0, sizeof(*clp));
		clp->m_type = htype;
		client_init(clp, hname, NULL);
		client_add(clp);
	}

	if (htype == MCL_FQDN && clp->m_naddr == 0 && hp != NULL) {
		char	**ap = hp->h_addr_list;
		int	i;

		for (i = 0; *ap && i < NFSCLNT_ADDRMAX; i++, ap++)
			clp->m_addrlist[i] = *(struct in_addr *)*ap;
		clp->m_naddr = i;
	}

	if (hp)
		free (hp);

	return clp;
}

nfs_client *
client_dup(nfs_client *clp, struct hostent *hp)
{
	nfs_client		*new;

	new = (nfs_client *) xmalloc(sizeof(*new));
	memcpy(new, clp, sizeof(*new));
	new->m_type = MCL_FQDN;
	new->m_hostname = NULL;

	client_init(new, (char *) hp->h_name, hp);
	client_add(new);
	return new;
}

static void
client_init(nfs_client *clp, const char *hname, struct hostent *hp)
{
	xfree(clp->m_hostname);
	if (hp)
		clp->m_hostname = xstrdup(hp->h_name);
	else
		clp->m_hostname = xstrdup(hname);

	clp->m_exported = 0;
	clp->m_count = 0;

	if (clp->m_type == MCL_SUBNETWORK) {
		char	*cp = strchr(clp->m_hostname, '/');
		static char slash32[] = "/32";

		if(!cp) cp = slash32;
		*cp = '\0';
		clp->m_addrlist[0].s_addr = inet_addr(clp->m_hostname);
		if (strchr(cp + 1, '.')) {
			clp->m_addrlist[1].s_addr = inet_addr(cp+1);
		}
		else {
			int netmask = atoi(cp + 1);
			if (0 < netmask && netmask <= 32) {
				clp->m_addrlist[1].s_addr =
					htonl ((uint32_t) ~0 << (32 - netmask));
			}
			else {
				xlog(L_FATAL, "invalid netmask `%s' for %s",
				     cp + 1, clp->m_hostname);
			}
		}
		*cp = '/';
		clp->m_naddr = 0;
	} else if (!hp) {
		clp->m_naddr = 0;
	} else {
		char	**ap = hp->h_addr_list;
		int	i;

		for (i = 0; *ap && i < NFSCLNT_ADDRMAX; i++, ap++) {
			clp->m_addrlist[i] = *(struct in_addr *)*ap;
		}
		clp->m_naddr = i;
	}
}

void
client_add(nfs_client *clp)
{
	nfs_client	**cpp;

	if (clp->m_type < 0 || clp->m_type >= MCL_MAXTYPES)
		xlog(L_FATAL, "unknown client type in client_add");
	cpp = clientlist + clp->m_type;
	while (*cpp)
		cpp = &((*cpp)->m_next);
	clp->m_next = NULL;
	*cpp = clp;
}

void
client_release(nfs_client *clp)
{
	if (clp->m_count <= 0)
		xlog(L_FATAL, "client_free: m_count <= 0!");
	clp->m_count--;
}

void
client_freeall(void)
{
	nfs_client	*clp, **head;
	int		i;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		head = clientlist + i;
		while (*head) {
			*head = (clp = *head)->m_next;
			xfree(clp->m_hostname);
			xfree(clp);
		}
	}
}

nfs_client *
client_find(struct hostent *hp)
{
	nfs_client	*clp;
	int		i;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (clp = clientlist[i]; clp; clp = clp->m_next) {
			if (!client_check(clp, hp))
				continue;
#ifdef notdef
			if (clp->m_type == MCL_FQDN)
				return clp;
			return client_dup(clp, hp);
#else
			return clp;
#endif
		}
	}
	return NULL;
}

struct hostent *
client_resolve(struct in_addr addr)
{
	struct hostent *he = NULL;

	if (clientlist[MCL_WILDCARD] || clientlist[MCL_NETGROUP])
		he = get_reliable_hostbyaddr((const char*)&addr, sizeof(addr), AF_INET);
	if (he == NULL)
		he = get_hostent((const char*)&addr, sizeof(addr), AF_INET);

	return he;
}

/*
 * Find client name given an IP address
 * This is found by gathering all known names that match that IP address,
 * sorting them and joining them with '+'
 *
 */
static char *add_name(char *old, char *add);

char *
client_compose(struct hostent *he)
{
	char *name = NULL;
	int i;

	for (i = 0 ; i < MCL_MAXTYPES; i++) {
		nfs_client	*clp;
		for (clp = clientlist[i]; clp ; clp = clp->m_next) {
			if (!client_check(clp, he))
				continue;
			name = add_name(name, clp->m_hostname);
		}
	}
	return name;
}

int
client_member(char *client, char *name)
{
	/* check if "client" (a ',' separated list of names)
	 * contains 'name' as a member
	 */
	int l = strlen(name);
	while (*client) {
		if (strncmp(client, name, l) == 0 &&
		    (client[l] == ',' || client[l] == '\0'))
			return 1;
		client = strchr(client, ',');
		if (client == NULL)
			return 0;
		client++;
	}
	return 0;
}


int
name_cmp(char *a, char *b)
{
	/* compare strings a and b, but only upto ',' in a */
	while (*a && *b && *a != ',' && *a == *b)
		a++, b++;
	if (!*b && (!*a || !a == ',') )
		return 0;
	if (!*b) return 1;
	if (!*a || *a == ',') return -1;
	return *a - *b;
}

static char *
add_name(char *old, char *add)
{
	int len = strlen(add)+2;
	char *new;
	char *cp;
	if (old) len += strlen(old);
	
	new = malloc(len);
	if (!new) {
		free(old);
		return NULL;
	}
	cp = old;
	while (cp && *cp && name_cmp(cp, add) < 0) {
		/* step cp forward over a name */
		char *e = strchr(cp, ',');
		if (e)
			cp = e+1;
		else
			cp = cp + strlen(cp);
	}
	strncpy(new, old, cp-old);
	new[cp-old] = 0;
	if (cp != old && !*cp)
		strcat(new, ",");
	strcat(new, add);
	if (cp && *cp) {
		strcat(new, ",");
		strcat(new, cp);
	}
	free(old);
	return new;
}

/*
 * Match a host (given its hostent record) to a client record. This
 * is usually called from mountd.
 */
int
client_check(nfs_client *clp, struct hostent *hp)
{
	char	*hname = (char *) hp->h_name;
	char	*cname = clp->m_hostname;
	char	**ap;

	switch (clp->m_type) {
	case MCL_FQDN:
	case MCL_SUBNETWORK:
		for (ap = hp->h_addr_list; *ap; ap++) {
			if (client_checkaddr(clp, *(struct in_addr *) *ap))
				return 1;
		}
		return 0;
	case MCL_WILDCARD:
		if (wildmat(hname, cname))
			return 1;
		else {
			for (ap = hp->h_aliases; *ap; ap++)
				if (wildmat(*ap, cname))
					return 1;
		}
		return 0;
	case MCL_NETGROUP:
#ifdef HAVE_INNETGR
		{
			char	*dot;
			int	match;
			struct hostent *nhp = NULL;
			struct sockaddr_in addr;

			/* First, try to match the hostname without
			 * splitting off the domain */
			if (innetgr(cname+1, hname, NULL, NULL))
				return 1;

			/* If hname is ip address convert to FQDN */
			if (inet_aton(hname, &addr.sin_addr) &&
			   (nhp = gethostbyaddr((const char *)&(addr.sin_addr),
			    sizeof(addr.sin_addr), AF_INET))) {
				hname = (char *)nhp->h_name;
				if (innetgr(cname+1, hname, NULL, NULL))
					return 1;
			}

			/* Okay, strip off the domain (if we have one) */
			if ((dot = strchr(hname, '.')) == NULL)
				return 0;

			*dot = '\0';
			match = innetgr(cname+1, hname, NULL, NULL);
			*dot = '.';

			return match;
		}
#else
		return 0;
#endif
	case MCL_ANONYMOUS:
		return 1;
	case MCL_GSS:
		return 0;
	default:
		xlog(L_FATAL, "internal: bad client type %d", clp->m_type);
	}

	return 0;
}

static int
client_checkaddr(nfs_client *clp, struct in_addr addr)
{
	int	i;

	switch (clp->m_type) {
	case MCL_FQDN:
		for (i = 0; i < clp->m_naddr; i++) {
			if (clp->m_addrlist[i].s_addr == addr.s_addr)
				return 1;
		}
		return 0;
	case MCL_SUBNETWORK:
		return !((clp->m_addrlist[0].s_addr ^ addr.s_addr)
			& clp->m_addrlist[1].s_addr);
	}
	return 0;
}

int
client_gettype(char *ident)
{
	char	*sp;

	if (ident[0] == '\0' || strcmp(ident, "*")==0)
		return MCL_ANONYMOUS;
	if (strncmp(ident, "gss/", 4) == 0)
		return MCL_GSS;
	if (ident[0] == '@') {
#ifndef HAVE_INNETGR
		xlog(L_WARNING, "netgroup support not compiled in");
#endif
		return MCL_NETGROUP;
	}
	for (sp = ident; *sp; sp++) {
		if (*sp == '*' || *sp == '?' || *sp == '[')
			return MCL_WILDCARD;
		if (*sp == '/')
			return MCL_SUBNETWORK;
		if (*sp == '\\' && sp[1])
			sp++;
	}
	/* check for N.N.N.N */
	sp = ident;
	if(!isdigit(*sp) || strtoul(sp, &sp, 10) > 255 || *sp != '.') return MCL_FQDN;
	sp++; if(!isdigit(*sp) || strtoul(sp, &sp, 10) > 255 || *sp != '.') return MCL_FQDN;
	sp++; if(!isdigit(*sp) || strtoul(sp, &sp, 10) > 255 || *sp != '.') return MCL_FQDN;
	sp++; if(!isdigit(*sp) || strtoul(sp, &sp, 10) > 255 || *sp != '\0') return MCL_FQDN;
	/* we lie here a bit. but technically N.N.N.N == N.N.N.N/32 :) */
	return MCL_SUBNETWORK;
}
