/* vi: set sw=4 ts=4: */
/*
 * stolen from net-tools-1.59 and stripped down for busybox by
 *                      Erik Andersen <andersen@codepoet.org>
 *
 * Heavily modified by Manuel Novoa III       Mar 12, 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "inet_common.h"

int FAST_FUNC INET_resolve(const char *name, struct sockaddr_in *s_in, int hostfirst)
{
	struct hostent *hp;
#if ENABLE_FEATURE_ETC_NETWORKS
	struct netent *np;
#endif

	/* Grmpf. -FvK */
	s_in->sin_family = AF_INET;
	s_in->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, bb_str_default)) {
		s_in->sin_addr.s_addr = INADDR_ANY;
		return 1;
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &s_in->sin_addr)) {
		return 0;
	}
	/* If we expect this to be a hostname, try hostname database first */
#ifdef DEBUG
	if (hostfirst) {
		bb_error_msg("gethostbyname(%s)", name);
	}
#endif
	if (hostfirst) {
		hp = gethostbyname(name);
		if (hp != NULL) {
			memcpy(&s_in->sin_addr, hp->h_addr_list[0],
				sizeof(struct in_addr));
			return 0;
		}
	}
#if ENABLE_FEATURE_ETC_NETWORKS
	/* Try the NETWORKS database to see if this is a known network. */
#ifdef DEBUG
	bb_error_msg("getnetbyname(%s)", name);
#endif
	np = getnetbyname(name);
	if (np != NULL) {
		s_in->sin_addr.s_addr = htonl(np->n_net);
		return 1;
	}
#endif
	if (hostfirst) {
		/* Don't try again */
		return -1;
	}
#ifdef DEBUG
	res_init();
	_res.options |= RES_DEBUG;
	bb_error_msg("gethostbyname(%s)", name);
#endif
	hp = gethostbyname(name);
	if (hp == NULL) {
		return -1;
	}
	memcpy(&s_in->sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
	return 0;
}


/* numeric: & 0x8000: default instead of *,
 *          & 0x4000: host instead of net,
 *          & 0x0fff: don't resolve
 */
char* FAST_FUNC INET_rresolve(struct sockaddr_in *s_in, int numeric, uint32_t netmask)
{
	/* addr-to-name cache */
	struct addr {
		struct addr *next;
		struct sockaddr_in addr;
		int host;
		char name[1];
	};
	static struct addr *cache = NULL;

	struct addr *pn;
	char *name;
	uint32_t ad, host_ad;
	int host = 0;

	if (s_in->sin_family != AF_INET) {
#ifdef DEBUG
		bb_error_msg("rresolve: unsupported address family %d!",
				  s_in->sin_family);
#endif
		errno = EAFNOSUPPORT;
		return NULL;
	}
	ad = s_in->sin_addr.s_addr;
#ifdef DEBUG
	bb_error_msg("rresolve: %08x, mask %08x, num %08x", (unsigned)ad, netmask, numeric);
#endif
	if (ad == INADDR_ANY) {
		if ((numeric & 0x0FFF) == 0) {
			if (numeric & 0x8000)
				return xstrdup(bb_str_default);
			return xstrdup("*");
		}
	}
	if (numeric & 0x0FFF)
		return xstrdup(inet_ntoa(s_in->sin_addr));

	if ((ad & (~netmask)) != 0 || (numeric & 0x4000))
		host = 1;
	pn = cache;
	while (pn) {
		if (pn->addr.sin_addr.s_addr == ad && pn->host == host) {
#ifdef DEBUG
			bb_error_msg("rresolve: found %s %08x in cache",
					  (host ? "host" : "net"), (unsigned)ad);
#endif
			return xstrdup(pn->name);
		}
		pn = pn->next;
	}

	host_ad = ntohl(ad);
	name = NULL;
	if (host) {
		struct hostent *ent;
#ifdef DEBUG
		bb_error_msg("gethostbyaddr (%08x)", (unsigned)ad);
#endif
		ent = gethostbyaddr((char *) &ad, 4, AF_INET);
		if (ent)
			name = xstrdup(ent->h_name);
	} else if (ENABLE_FEATURE_ETC_NETWORKS) {
		struct netent *np;
#ifdef DEBUG
		bb_error_msg("getnetbyaddr (%08x)", (unsigned)host_ad);
#endif
		np = getnetbyaddr(host_ad, AF_INET);
		if (np)
			name = xstrdup(np->n_name);
	}
	if (!name)
		name = xstrdup(inet_ntoa(s_in->sin_addr));
	pn = xmalloc(sizeof(*pn) + strlen(name)); /* no '+ 1', it's already accounted for */
	pn->next = cache;
	pn->addr = *s_in;
	pn->host = host;
	strcpy(pn->name, name);
	cache = pn;
	return name;
}

#if ENABLE_FEATURE_IPV6

int FAST_FUNC INET6_resolve(const char *name, struct sockaddr_in6 *sin6)
{
	struct addrinfo req, *ai;
	int s;

	memset(&req, '\0', sizeof req);
	req.ai_family = AF_INET6;
	s = getaddrinfo(name, NULL, &req, &ai);
	if (s) {
		bb_error_msg("getaddrinfo: %s: %d", name, s);
		return -1;
	}
	memcpy(sin6, ai->ai_addr, sizeof(struct sockaddr_in6));
	freeaddrinfo(ai);
	return 0;
}

#ifndef IN6_IS_ADDR_UNSPECIFIED
# define IN6_IS_ADDR_UNSPECIFIED(a) \
	(((uint32_t *) (a))[0] == 0 && ((uint32_t *) (a))[1] == 0 && \
	 ((uint32_t *) (a))[2] == 0 && ((uint32_t *) (a))[3] == 0)
#endif


char* FAST_FUNC INET6_rresolve(struct sockaddr_in6 *sin6, int numeric)
{
	char name[128];
	int s;

	if (sin6->sin6_family != AF_INET6) {
#ifdef DEBUG
		bb_error_msg("rresolve: unsupported address family %d!",
				  sin6->sin6_family);
#endif
		errno = EAFNOSUPPORT;
		return NULL;
	}
	if (numeric & 0x7FFF) {
		inet_ntop(AF_INET6, &sin6->sin6_addr, name, sizeof(name));
		return xstrdup(name);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (numeric & 0x8000)
			return xstrdup(bb_str_default);
		return xstrdup("*");
	}

	s = getnameinfo((struct sockaddr *) sin6, sizeof(struct sockaddr_in6),
				name, sizeof(name), NULL, 0, 0);
	if (s) {
		bb_error_msg("getnameinfo failed");
		return NULL;
	}
	return xstrdup(name);
}

#endif		/* CONFIG_FEATURE_IPV6 */
