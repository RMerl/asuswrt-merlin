/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2005, The PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this paragraph
and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS
TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*/

/*-------------------------------------------------------------------------
 *
 * getaddrinfo.c
 *	  Support getaddrinfo() on platforms that don't have it.
 *
 * We also supply getnameinfo() here, assuming that the platform will have
 * it if and only if it has getaddrinfo().	If this proves false on some
 * platform, we'll need to split this file and provide a separate configure
 * test for getnameinfo().
 *
 * Copyright (c) 2003-2007, PostgreSQL Global Development Group
 *
 * Copyright (C) 2007 Jeremy Allison.
 * Modified to return multiple IPv4 addresses for Samba.
 *
 *-------------------------------------------------------------------------
 */

#include "replace.h"
#include "system/network.h"

#ifndef SMB_MALLOC
#define SMB_MALLOC(s) malloc(s)
#endif

#ifndef SMB_STRDUP
#define SMB_STRDUP(s) strdup(s)
#endif

static int check_hostent_err(struct hostent *hp)
{
	if (!hp) {
		switch (h_errno) {
			case HOST_NOT_FOUND:
			case NO_DATA:
				return EAI_NONAME;
			case TRY_AGAIN:
				return EAI_AGAIN;
			case NO_RECOVERY:
			default:
				return EAI_FAIL;
		}
	}
	if (!hp->h_name || hp->h_addrtype != AF_INET) {
		return EAI_FAIL;
	}
	return 0;
}

static char *canon_name_from_hostent(struct hostent *hp,
				int *perr)
{
	char *ret = NULL;

	*perr = check_hostent_err(hp);
	if (*perr) {
		return NULL;
	}
	ret = SMB_STRDUP(hp->h_name);
	if (!ret) {
		*perr = EAI_MEMORY;
	}
	return ret;
}

static char *get_my_canon_name(int *perr)
{
	char name[HOST_NAME_MAX+1];

	if (gethostname(name, HOST_NAME_MAX) == -1) {
		*perr = EAI_FAIL;
		return NULL;
	}
	/* Ensure null termination. */
	name[HOST_NAME_MAX] = '\0';
	return canon_name_from_hostent(gethostbyname(name), perr);
}

static char *get_canon_name_from_addr(struct in_addr ip,
				int *perr)
{
	return canon_name_from_hostent(
			gethostbyaddr(&ip, sizeof(ip), AF_INET),
			perr);
}

static struct addrinfo *alloc_entry(const struct addrinfo *hints,
				struct in_addr ip,
				unsigned short port)
{
	struct sockaddr_in *psin = NULL;
	struct addrinfo *ai = SMB_MALLOC(sizeof(*ai));

	if (!ai) {
		return NULL;
	}
	memset(ai, '\0', sizeof(*ai));

	psin = SMB_MALLOC(sizeof(*psin));
	if (!psin) {
		free(ai);
		return NULL;
	}

	memset(psin, '\0', sizeof(*psin));

	psin->sin_family = AF_INET;
	psin->sin_port = htons(port);
	psin->sin_addr = ip;

	ai->ai_flags = 0;
	ai->ai_family = AF_INET;
	ai->ai_socktype = hints->ai_socktype;
	ai->ai_protocol = hints->ai_protocol;
	ai->ai_addrlen = sizeof(*psin);
	ai->ai_addr = (struct sockaddr *) psin;
	ai->ai_canonname = NULL;
	ai->ai_next = NULL;

	return ai;
}

/*
 * get address info for a single ipv4 address.
 *
 *	Bugs:	- servname can only be a number, not text.
 */

static int getaddr_info_single_addr(const char *service,
				uint32_t addr,
				const struct addrinfo *hints,
				struct addrinfo **res)
{

	struct addrinfo *ai = NULL;
	struct in_addr ip;
	unsigned short port = 0;

	if (service) {
		port = (unsigned short)atoi(service);
	}
	ip.s_addr = htonl(addr);

	ai = alloc_entry(hints, ip, port);
	if (!ai) {
		return EAI_MEMORY;
	}

	/* If we're asked for the canonical name,
	 * make sure it returns correctly. */
	if (!(hints->ai_flags & AI_NUMERICSERV) &&
			hints->ai_flags & AI_CANONNAME) {
		int err;
		if (addr == INADDR_LOOPBACK || addr == INADDR_ANY) {
			ai->ai_canonname = get_my_canon_name(&err);
		} else {
			ai->ai_canonname =
			get_canon_name_from_addr(ip,&err);
		}
		if (ai->ai_canonname == NULL) {
			freeaddrinfo(ai);
			return err;
		}
	}

	*res = ai;
	return 0;
}

/*
 * get address info for multiple ipv4 addresses.
 *
 *	Bugs:	- servname can only be a number, not text.
 */

static int getaddr_info_name(const char *node,
				const char *service,
				const struct addrinfo *hints,
				struct addrinfo **res)
{
	struct addrinfo *listp = NULL, *prevp = NULL;
	char **pptr = NULL;
	int err;
	struct hostent *hp = NULL;
	unsigned short port = 0;

	if (service) {
		port = (unsigned short)atoi(service);
	}

	hp = gethostbyname(node);
	err = check_hostent_err(hp);
	if (err) {
		return err;
	}

	for(pptr = hp->h_addr_list; *pptr; pptr++) {
		struct in_addr ip = *(struct in_addr *)*pptr;
		struct addrinfo *ai = alloc_entry(hints, ip, port);

		if (!ai) {
			freeaddrinfo(listp);
			return EAI_MEMORY;
		}

		if (!listp) {
			listp = ai;
			prevp = ai;
			ai->ai_canonname = SMB_STRDUP(hp->h_name);
			if (!ai->ai_canonname) {
				freeaddrinfo(listp);
				return EAI_MEMORY;
			}
		} else {
			prevp->ai_next = ai;
			prevp = ai;
		}
	}
	*res = listp;
	return 0;
}

/*
 * get address info for ipv4 sockets.
 *
 *	Bugs:	- servname can only be a number, not text.
 */

int rep_getaddrinfo(const char *node,
		const char *service,
		const struct addrinfo * hintp,
		struct addrinfo ** res)
{
	struct addrinfo hints;

	/* Setup the hints struct. */
	if (hintp == NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
	} else {
		memcpy(&hints, hintp, sizeof(hints));
	}

	if (hints.ai_family != AF_INET && hints.ai_family != AF_UNSPEC) {
		return EAI_FAMILY;
	}

	if (hints.ai_socktype == 0) {
		hints.ai_socktype = SOCK_STREAM;
	}

	if (!node && !service) {
		return EAI_NONAME;
	}

	if (node) {
		if (node[0] == '\0') {
			return getaddr_info_single_addr(service,
					INADDR_ANY,
					&hints,
					res);
		} else if (hints.ai_flags & AI_NUMERICHOST) {
			struct in_addr ip;
			if (!inet_aton(node, &ip)) {
				return EAI_FAIL;
			}
			return getaddr_info_single_addr(service,
					ntohl(ip.s_addr),
					&hints,
					res);
		} else {
			return getaddr_info_name(node,
						service,
						&hints,
						res);
		}
	} else if (hints.ai_flags & AI_PASSIVE) {
		return getaddr_info_single_addr(service,
					INADDR_ANY,
					&hints,
					res);
	}
	return getaddr_info_single_addr(service,
					INADDR_LOOPBACK,
					&hints,
					res);
}


void rep_freeaddrinfo(struct addrinfo *res)
{
	struct addrinfo *next = NULL;

	for (;res; res = next) {
		next = res->ai_next;
		if (res->ai_canonname) {
			free(res->ai_canonname);
		}
		if (res->ai_addr) {
			free(res->ai_addr);
		}
		free(res);
	}
}


const char *rep_gai_strerror(int errcode)
{
#ifdef HAVE_HSTRERROR
	int			hcode;

	switch (errcode)
	{
		case EAI_NONAME:
			hcode = HOST_NOT_FOUND;
			break;
		case EAI_AGAIN:
			hcode = TRY_AGAIN;
			break;
		case EAI_FAIL:
		default:
			hcode = NO_RECOVERY;
			break;
	}

	return hstrerror(hcode);
#else							/* !HAVE_HSTRERROR */

	switch (errcode)
	{
		case EAI_NONAME:
			return "Unknown host";
		case EAI_AGAIN:
			return "Host name lookup failure";
#ifdef EAI_BADFLAGS
		case EAI_BADFLAGS:
			return "Invalid argument";
#endif
#ifdef EAI_FAMILY
		case EAI_FAMILY:
			return "Address family not supported";
#endif
#ifdef EAI_MEMORY
		case EAI_MEMORY:
			return "Not enough memory";
#endif
#ifdef EAI_NODATA
		case EAI_NODATA:
			return "No host data of that type was found";
#endif
#ifdef EAI_SERVICE
		case EAI_SERVICE:
			return "Class type not found";
#endif
#ifdef EAI_SOCKTYPE
		case EAI_SOCKTYPE:
			return "Socket type not supported";
#endif
		default:
			return "Unknown server error";
	}
#endif   /* HAVE_HSTRERROR */
}

static int gethostnameinfo(const struct sockaddr *sa,
			char *node,
			size_t nodelen,
			int flags)
{
	int ret = -1;
	char *p = NULL;

	if (!(flags & NI_NUMERICHOST)) {
		struct hostent *hp = gethostbyaddr(
				&((struct sockaddr_in *)sa)->sin_addr,
				sizeof(struct in_addr),
				sa->sa_family);
		ret = check_hostent_err(hp);
		if (ret == 0) {
			/* Name looked up successfully. */
			ret = snprintf(node, nodelen, "%s", hp->h_name);
			if (ret < 0 || (size_t)ret >= nodelen) {
				return EAI_MEMORY;
			}
			if (flags & NI_NOFQDN) {
				p = strchr(node,'.');
				if (p) {
					*p = '\0';
				}
			}
			return 0;
		}

		if (flags & NI_NAMEREQD) {
			/* If we require a name and didn't get one,
			 * automatically fail. */
			return ret;
		}
		/* Otherwise just fall into the numeric host code... */
	}
	p = inet_ntoa(((struct sockaddr_in *)sa)->sin_addr);
	ret = snprintf(node, nodelen, "%s", p);
	if (ret < 0 || (size_t)ret >= nodelen) {
		return EAI_MEMORY;
	}
	return 0;
}

static int getservicenameinfo(const struct sockaddr *sa,
			char *service,
			size_t servicelen,
			int flags)
{
	int ret = -1;
	int port = ntohs(((struct sockaddr_in *)sa)->sin_port);

	if (!(flags & NI_NUMERICSERV)) {
		struct servent *se = getservbyport(
				port,
				(flags & NI_DGRAM) ? "udp" : "tcp");
		if (se && se->s_name) {
			/* Service name looked up successfully. */
			ret = snprintf(service, servicelen, "%s", se->s_name);
			if (ret < 0 || (size_t)ret >= servicelen) {
				return EAI_MEMORY;
			}
			return 0;
		}
		/* Otherwise just fall into the numeric service code... */
	}
	ret = snprintf(service, servicelen, "%d", port);
	if (ret < 0 || (size_t)ret >= servicelen) {
		return EAI_MEMORY;
	}
	return 0;
}

/*
 * Convert an ipv4 address to a hostname.
 *
 * Bugs:	- No IPv6 support.
 */
int rep_getnameinfo(const struct sockaddr *sa, socklen_t salen,
			char *node, size_t nodelen,
			char *service, size_t servicelen, int flags)
{

	/* Invalid arguments. */
	if (sa == NULL || (node == NULL && service == NULL)) {
		return EAI_FAIL;
	}

	if (sa->sa_family != AF_INET) {
		return EAI_FAIL;
	}

	if (salen < sizeof(struct sockaddr_in)) {
		return EAI_FAIL;
	}

	if (node) {
		return gethostnameinfo(sa, node, nodelen, flags);
	}

	if (service) {
		return getservicenameinfo(sa, service, servicelen, flags);
	}
	return 0;
}
