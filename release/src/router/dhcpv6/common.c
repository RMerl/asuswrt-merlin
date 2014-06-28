/*	$KAME: common.c,v 1.129 2005/09/16 11:30:13 suz Exp $	*/
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/stat.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <net/if.h>
#include <netinet/in.h>
#ifdef __KAME__
#include <net/if_types.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <net/if_dl.h>
#endif
#ifdef __linux__
#include <linux/if_packet.h>
#endif
#include <net/if_arp.h>
#ifdef __sun__
#include <sys/sockio.h>
#include <sys/dlpi.h>
#include <stropts.h>
#include <fcntl.h>
#include <libdevinfo.h>
#endif

#ifdef __KAME__
#include <netinet6/in6_var.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <dhcp6.h>
#include <config.h>
#include <common.h>
#include <timer.h>

#ifdef __linux__
/* from /usr/include/linux/ipv6.h */

struct in6_ifreq {
	struct in6_addr ifr6_addr;
	u_int32_t ifr6_prefixlen;
	unsigned int ifr6_ifindex;
};
#endif

#define MAXDNAME 255

int foreground;
int debug_thresh;
int duid_type = 1;

static int dhcp6_count_list __P((struct dhcp6_list *));
static int in6_matchflags __P((struct sockaddr *, char *, int));
static ssize_t dnsencode __P((const char *, char *, size_t));
static char *dnsdecode __P((u_char **, u_char *, char *, size_t));
static int copyout_option __P((char *, char *, struct dhcp6_listval *));
static int copyin_option __P((int, struct dhcp6opt *, struct dhcp6opt *,
    struct dhcp6_list *));
static int copy_option __P((u_int16_t, u_int16_t, void *, struct dhcp6opt **,
    struct dhcp6opt *, int *));
static ssize_t gethwid __P((char *, int, const char *, u_int16_t *));
static char *sprint_uint64 __P((char *, int, u_int64_t));
static char *sprint_auth __P((struct dhcp6_optinfo *));

int
dhcp6_copy_list(dst, src)
	struct dhcp6_list *dst, *src;
{
	struct dhcp6_listval *ent;

	for (ent = TAILQ_FIRST(src); ent; ent = TAILQ_NEXT(ent, link)) {
		if (dhcp6_add_listval(dst, ent->type,
		    &ent->uv, &ent->sublist) == NULL)
			goto fail;
	}

	return (0);

  fail:
	dhcp6_clear_list(dst);
	return (-1);
}

void
dhcp6_move_list(dst, src)
	struct dhcp6_list *dst, *src;
{
	struct dhcp6_listval *v;

	while ((v = TAILQ_FIRST(src)) != NULL) {
		TAILQ_REMOVE(src, v, link);
		TAILQ_INSERT_TAIL(dst, v, link);
	}
}

void
dhcp6_clear_list(head)
	struct dhcp6_list *head;
{
	struct dhcp6_listval *v;

	while ((v = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, v, link);
		dhcp6_clear_listval(v);
	}

	return;
}

static int
dhcp6_count_list(head)
	struct dhcp6_list *head;
{
	struct dhcp6_listval *v;
	int i;

	for (i = 0, v = TAILQ_FIRST(head); v; v = TAILQ_NEXT(v, link))
		i++;

	return (i);
}

void
dhcp6_clear_listval(lv)
	struct dhcp6_listval *lv;
{
	dhcp6_clear_list(&lv->sublist);
	switch (lv->type) {
	case DHCP6_LISTVAL_VBUF:
		dhcp6_vbuf_free(&lv->val_vbuf);
		break;
	default:		/* nothing to do */
		break;
	}
	free(lv);
}

/*
 * Note: this function only searches for the first entry that matches
 * VAL.  It also does not care about sublists.
 */
struct dhcp6_listval *
dhcp6_find_listval(head, type, val, option)
	struct dhcp6_list *head;
	dhcp6_listval_type_t type;
	void *val;
	int option;
{
	struct dhcp6_listval *lv;

	for (lv = TAILQ_FIRST(head); lv; lv = TAILQ_NEXT(lv, link)) {
		if (lv->type != type)
			continue;

		switch(type) {
		case DHCP6_LISTVAL_NUM:
			if (lv->val_num == *(int *)val)
				return (lv);
			break;
		case DHCP6_LISTVAL_STCODE:
			if (lv->val_num16 == *(u_int16_t *)val)
				return (lv);
			break;
		case DHCP6_LISTVAL_ADDR6:
			if (IN6_ARE_ADDR_EQUAL(&lv->val_addr6,
			    (struct in6_addr *)val)) {
				return (lv);
			}
			break;
		case DHCP6_LISTVAL_PREFIX6:
			if ((option & MATCHLIST_PREFIXLEN) &&
			    lv->val_prefix6.plen ==
			    ((struct dhcp6_prefix *)val)->plen) {
				return (lv);
			} else if (IN6_ARE_ADDR_EQUAL(&lv->val_prefix6.addr,
			    &((struct dhcp6_prefix *)val)->addr) &&
			    lv->val_prefix6.plen ==
			    ((struct dhcp6_prefix *)val)->plen) {
				return (lv);
			}
			break;
		case DHCP6_LISTVAL_STATEFULADDR6:
			if (IN6_ARE_ADDR_EQUAL(&lv->val_statefuladdr6.addr,
			    &((struct dhcp6_prefix *)val)->addr)) {
				return (lv);
			}
			break;
		case DHCP6_LISTVAL_IAPD:
		case DHCP6_LISTVAL_IANA:
			if (lv->val_ia.iaid ==
			    ((struct dhcp6_ia *)val)->iaid) {
				return (lv);
			}
			break;
		case DHCP6_LISTVAL_VBUF:
			if (dhcp6_vbuf_cmp(&lv->val_vbuf,
			    (struct dhcp6_vbuf *)val) == 0) {
				return (lv);
			}
			break;
		}
	}

	return (NULL);
}

struct dhcp6_listval *
dhcp6_add_listval(head, type, val, sublist)
	struct dhcp6_list *head, *sublist;
	dhcp6_listval_type_t type;
	void *val;
{
	struct dhcp6_listval *lv = NULL;

	if ((lv = malloc(sizeof(*lv))) == NULL) {
		dprintf(LOG_ERR, FNAME,
		    "failed to allocate memory for list entry");
		goto fail;
	}
	memset(lv, 0, sizeof(*lv));
	lv->type = type;
	TAILQ_INIT(&lv->sublist);

	switch(type) {
	case DHCP6_LISTVAL_NUM:
		lv->val_num = *(int *)val;
		break;
	case DHCP6_LISTVAL_STCODE:
		lv->val_num16 = *(u_int16_t *)val;
		break;
	case DHCP6_LISTVAL_ADDR6:
		lv->val_addr6 = *(struct in6_addr *)val;
		break;
	case DHCP6_LISTVAL_PREFIX6:
		lv->val_prefix6 = *(struct dhcp6_prefix *)val;
		break;
	case DHCP6_LISTVAL_STATEFULADDR6:
		lv->val_statefuladdr6 = *(struct dhcp6_statefuladdr *)val;
		break;
	case DHCP6_LISTVAL_IAPD:
	case DHCP6_LISTVAL_IANA:
		lv->val_ia = *(struct dhcp6_ia *)val;
		break;
	case DHCP6_LISTVAL_VBUF:
		if (dhcp6_vbuf_copy(&lv->val_vbuf, (struct dhcp6_vbuf *)val))
			goto fail;
		break;
	default:
		dprintf(LOG_ERR, FNAME,
		    "unexpected list value type (%d)", type);
		goto fail;
	}

	if (sublist && dhcp6_copy_list(&lv->sublist, sublist))
		goto fail;

	TAILQ_INSERT_TAIL(head, lv, link);

	return (lv);

  fail:
	if (lv)
		free(lv);

	return (NULL);
}

int
dhcp6_vbuf_copy(dst, src)
	struct dhcp6_vbuf *dst, *src;
{
	dst->dv_buf = malloc(src->dv_len);
	if (dst->dv_buf == NULL)
		return (-1);

	dst->dv_len = src->dv_len;
	memcpy(dst->dv_buf, src->dv_buf, dst->dv_len);

	return (0);
}

void
dhcp6_vbuf_free(vbuf)
	struct dhcp6_vbuf *vbuf;
{
	free(vbuf->dv_buf);

	vbuf->dv_len = 0;
	vbuf->dv_buf = NULL;
}

int
dhcp6_vbuf_cmp(vb1, vb2)
	struct dhcp6_vbuf *vb1, *vb2;
{
	if (vb1->dv_len != vb2->dv_len)
		return (vb1->dv_len - vb2->dv_len);

	return (memcmp(vb1->dv_buf, vb2->dv_buf, vb1->dv_len));
}

static int
dhcp6_get_addr(optlen, cp, type, list)
	int optlen;
	void *cp;
	dhcp6_listval_type_t type;
	struct dhcp6_list *list;
{
	void *val;

	if (optlen % sizeof(struct in6_addr) || optlen == 0) {
		dprintf(LOG_INFO, FNAME,
		    "malformed DHCP option: type %d, len %d", type, optlen);
		return -1;
	}
	for (val = cp; val < cp + optlen; val += sizeof(struct in6_addr)) {
		struct in6_addr valaddr;

		memcpy(&valaddr, val, sizeof(valaddr));
		if (dhcp6_find_listval(list,
		    DHCP6_LISTVAL_ADDR6, &valaddr, 0)) {
			dprintf(LOG_INFO, FNAME, "duplicated %s address (%s)",
			    dhcp6optstr(type), in6addr2str(&valaddr, 0));
			continue;
		}

		if (dhcp6_add_listval(list, DHCP6_LISTVAL_ADDR6,
		    &valaddr, NULL) == NULL) {
			dprintf(LOG_ERR, FNAME,
			    "failed to copy %s address", dhcp6optstr(type));
			return -1;
		}
	}

	return 0;
}

static int
dhcp6_set_addr(type, list, p, optep, len)
	dhcp6_listval_type_t type;
	struct dhcp6_list *list;
	struct dhcp6opt **p, *optep;
	int *len;
{
	struct in6_addr *in6;
	char *tmpbuf;
	struct dhcp6_listval *d;
	int optlen;

	if (TAILQ_EMPTY(list))
		return 0;

	tmpbuf = NULL;
	optlen = dhcp6_count_list(list) * sizeof(struct in6_addr);
	if ((tmpbuf = malloc(optlen)) == NULL) {
		dprintf(LOG_ERR, FNAME,
		    "memory allocation failed for %s options",
		    dhcp6optstr(type));
		return -1;
	}
	in6 = (struct in6_addr *)tmpbuf;
	for (d = TAILQ_FIRST(list); d; d = TAILQ_NEXT(d, link), in6++)
		memcpy(in6, &d->val_addr6, sizeof(*in6));
	if (copy_option(type, optlen, tmpbuf, p, optep, len) != 0) {
		free(tmpbuf);
		return -1;
	}

	free(tmpbuf);
	return 0;
}

static int
dhcp6_get_domain(optlen, cp, type, list)
	int optlen;
	void *cp;
	dhcp6_listval_type_t type;
	struct dhcp6_list *list;
{
	void *val;

	val = cp;
	while (val < cp + optlen) {
		struct dhcp6_vbuf vb;
		char name[MAXDNAME + 1];

		if (dnsdecode((u_char **)(void *)&val,
		    (u_char *)(cp + optlen), name, sizeof(name)) == NULL) {
			dprintf(LOG_INFO, FNAME, "failed to "
			    "decode a %s domain name",
			    dhcp6optstr(type));
			dprintf(LOG_INFO, FNAME,
			    "malformed DHCP option: type %d, len %d",
			     type, optlen);
			return -1;
		}

		vb.dv_len = strlen(name) + 1;
		vb.dv_buf = name;

		if (dhcp6_add_listval(list,
		    DHCP6_LISTVAL_VBUF, &vb, NULL) == NULL) {
			dprintf(LOG_ERR, FNAME, "failed to "
			    "copy a %s domain name", dhcp6optstr(type));
			return -1;
		}
	}

	return 0;
}

static int
dhcp6_set_domain(type, list, p, optep, len)
	dhcp6_listval_type_t type;
	struct dhcp6_list *list;
	struct dhcp6opt **p, *optep;
	int *len;
{
	int optlen = 0;
	struct dhcp6_listval *d;
	char *tmpbuf;
	char name[MAXDNAME], *cp, *ep;

	if (TAILQ_EMPTY(list))
		return 0;

	for (d = TAILQ_FIRST(list); d; d = TAILQ_NEXT(d, link))
		optlen += (d->val_vbuf.dv_len + 1);

	if (optlen == 0) {
		return 0;
	}

	tmpbuf = NULL;
	if ((tmpbuf = malloc(optlen)) == NULL) {
		dprintf(LOG_ERR, FNAME, "memory allocation failed for "
		    "%s domain options", dhcp6optstr(type));
		return -1;
	}
	cp = tmpbuf;
	ep = cp + optlen;
	for (d = TAILQ_FIRST(list); d; d = TAILQ_NEXT(d, link)) {
		int nlen;

		nlen = dnsencode((const char *)d->val_vbuf.dv_buf,
		    name, sizeof (name));
		if (nlen < 0) {
			dprintf(LOG_ERR, FNAME,
			    "failed to encode a %s domain name",
			    dhcp6optstr(type));
			free(tmpbuf);
			return -1;
		}
		if (ep - cp < nlen) {
			dprintf(LOG_ERR, FNAME,
			    "buffer length for %s domain name is too short",
			    dhcp6optstr(type));
			free(tmpbuf);
			return -1;
		}
		memcpy(cp, name, nlen);
		cp += nlen;
	}
	if (copy_option(type, cp - tmpbuf, tmpbuf, p, optep, len) != 0) {
		free(tmpbuf);
		return -1;
	}
	free(tmpbuf);

	return 0;
}

struct dhcp6_event *
dhcp6_create_event(ifp, state)
	struct dhcp6_if *ifp;
	int state;
{
	struct dhcp6_event *ev;

	if ((ev = malloc(sizeof(*ev))) == NULL) {
		dprintf(LOG_ERR, FNAME,
		    "failed to allocate memory for an event");
		return (NULL);
	}
	memset(ev, 0, sizeof(*ev));
	ev->ifp = ifp;
	ev->state = state;
	TAILQ_INIT(&ev->data_list);

	return (ev);
}

void
dhcp6_remove_event(ev)
	struct dhcp6_event *ev;
{
	struct dhcp6_serverinfo *sp, *sp_next;

	dprintf(LOG_DEBUG, FNAME, "removing an event on %s, state=%s",
	    ev->ifp->ifname, dhcp6_event_statestr(ev));

	dhcp6_remove_evdata(ev);

	duidfree(&ev->serverid);

	if (ev->timer)
		dhcp6_remove_timer(&ev->timer);
	TAILQ_REMOVE(&ev->ifp->event_list, ev, link);

	for (sp = ev->servers; sp; sp = sp_next) {
		sp_next = sp->next;

		dprintf(LOG_DEBUG, FNAME, "removing server (ID: %s)",
		    duidstr(&sp->optinfo.serverID));
		dhcp6_clear_options(&sp->optinfo);
		if (sp->authparam != NULL)
			free(sp->authparam);
		free(sp);
	}

	if (ev->authparam != NULL)
		free(ev->authparam);

	free(ev);
}

void
dhcp6_remove_evdata(ev)
	struct dhcp6_event *ev;
{
	struct dhcp6_eventdata *evd;

	while ((evd = TAILQ_FIRST(&ev->data_list)) != NULL) {
		TAILQ_REMOVE(&ev->data_list, evd, link);
		if (evd->destructor)
			(*evd->destructor)(evd);
		free(evd);
	}
}

struct authparam *
new_authparam(proto, alg, rdm)
	int proto, alg, rdm;
{
	struct authparam *authparam;

	if ((authparam = malloc(sizeof(*authparam))) == NULL)
		return (NULL);

	memset(authparam, 0, sizeof(*authparam));

	authparam->authproto = proto;
	authparam->authalgorithm = alg;
	authparam->authrdm = rdm;
	authparam->key = NULL;
	authparam->flags |= AUTHPARAM_FLAGS_NOPREVRD;
	authparam->prevrd = 0;

	return (authparam);
}

struct authparam *
copy_authparam(authparam)
	struct authparam *authparam;
{
	struct authparam *dst;

	if ((dst = malloc(sizeof(*dst))) == NULL)
		return (NULL);

	memcpy(dst, authparam, sizeof(*dst));

	return (dst);
}

/*
 * Home-brew function of a 64-bit version of ntohl.
 * XXX: is there any standard for this?
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
static __inline u_int64_t
ntohq(u_int64_t x)
{
	return (u_int64_t)ntohl((u_int32_t)(x >> 32)) |
	    (int64_t)ntohl((u_int32_t)(x & 0xffffffff)) << 32;
}
#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#define ntohq(x) (x)
#endif

int
dhcp6_auth_replaycheck(method, prev, current)
	int method;
	u_int64_t prev, current;
{
	char bufprev[] = "ffff ffff ffff ffff";
	char bufcurrent[] = "ffff ffff ffff ffff";

	if (method != DHCP6_AUTHRDM_MONOCOUNTER) {
		dprintf(LOG_ERR, FNAME, "unsupported replay detection "
		    "method (%d)", method);
		return (-1);
	}

	(void)sprint_uint64(bufprev, sizeof(bufprev), prev);
	(void)sprint_uint64(bufcurrent, sizeof(bufcurrent), current);
	dprintf(LOG_DEBUG, FNAME, "previous: %s, current: %s",
	    bufprev, bufcurrent);

	prev = ntohq(prev);
	current = ntohq(current);

	/*
	 * we call the singular point guilty, since we cannot guess
	 * whether the serial number is increasing or not.
	 */
        if (prev == (current ^ 0x8000000000000000ULL)) {
		dprintf(LOG_INFO, FNAME, "detected a singular point");
		return (1);
	}

	return (((int64_t)(current - prev) > 0) ? 0 : 1);
}

int
getifaddr(addr, ifnam, prefix, plen, strong, ignoreflags)
	struct in6_addr *addr;
	char *ifnam;
	struct in6_addr *prefix;
	int plen;
	int strong;		/* if strong host model is required or not */
	int ignoreflags;
{
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_in6 sin6;
	int error = -1;

	if (getifaddrs(&ifap) != 0) {
		dprintf(LOG_WARNING, FNAME,
			"getifaddrs failed: %s", strerror(errno));
		return (-1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		int s1, s2;

		if (strong && strcmp(ifnam, ifa->ifa_name) != 0)
			continue;

		/* in any case, ignore interfaces in different scope zones. */
		if ((s1 = in6_addrscopebyif(prefix, ifnam)) < 0 ||
		    (s2 = in6_addrscopebyif(prefix, ifa->ifa_name)) < 0 ||
		     s1 != s2)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
#ifdef HAVE_SA_LEN
		if (ifa->ifa_addr->sa_len > sizeof(sin6))
			continue;
#endif

		if (in6_matchflags(ifa->ifa_addr, ifa->ifa_name, ignoreflags))
			continue;

		memcpy(&sin6, ifa->ifa_addr, sysdep_sa_len(ifa->ifa_addr));
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr)) {
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (plen % 8 == 0) {
			if (memcmp(&sin6.sin6_addr, prefix, plen / 8) != 0)
				continue;
		} else {
			struct in6_addr a, m;
			int i;

			memcpy(&a, &sin6.sin6_addr, sizeof(sin6.sin6_addr));
			memset(&m, 0, sizeof(m));
			memset(&m, 0xff, plen / 8);
			m.s6_addr[plen / 8] = (0xff00 >> (plen % 8)) & 0xff;
			for (i = 0; i < sizeof(a); i++)
				a.s6_addr[i] &= m.s6_addr[i];

			if (memcmp(&a, prefix, plen / 8) != 0 ||
			    a.s6_addr[plen / 8] !=
			    (prefix->s6_addr[plen / 8] & m.s6_addr[plen / 8]))
				continue;
		}
		memcpy(addr, &sin6.sin6_addr, sizeof(sin6.sin6_addr));
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(addr))
			addr->s6_addr[2] = addr->s6_addr[3] = 0; 
#endif
		error = 0;
		break;
	}

	freeifaddrs(ifap);
	return (error);
}

int
getifidfromaddr(addr, ifidp)
	struct in6_addr *addr;
	unsigned int *ifidp;
{
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_in6 *sa6;
	unsigned int ifid;
	int retval = -1;

	if (getifaddrs(&ifap) != 0) {
		dprintf(LOG_WARNING, FNAME,
			"getifaddrs failed: %s", strerror(errno));
		return (-1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_ARE_ADDR_EQUAL(addr, &sa6->sin6_addr))
			break;
	}

	if (ifa != NULL) {
		if ((ifid = if_nametoindex(ifa->ifa_name)) == 0) {
			dprintf(LOG_ERR, FNAME,
			    "if_nametoindex failed for %s", ifa->ifa_name);
			goto end;
		}
		retval = 0;
		*ifidp = ifid;
	}

  end:
	freeifaddrs(ifap);
	return (retval);
}

int
in6_addrscopebyif(addr, ifnam)
	struct in6_addr *addr;
	char *ifnam;
{
	u_int ifindex; 

	if ((ifindex = if_nametoindex(ifnam)) == 0)
		return (-1);

	if (IN6_IS_ADDR_LINKLOCAL(addr) || IN6_IS_ADDR_MC_LINKLOCAL(addr))
		return (ifindex);

	if (IN6_IS_ADDR_SITELOCAL(addr) || IN6_IS_ADDR_MC_SITELOCAL(addr))
		return (1);	/* XXX */

	if (IN6_IS_ADDR_MC_ORGLOCAL(addr))
		return (1);	/* XXX */

	return (1);		/* treat it as global */
}

int
transmit_sa(s, sa, buf, len)
	int s;
	struct sockaddr *sa;
	char *buf;
	size_t len;
{
	int error;

	error = sendto(s, buf, len, 0, sa, sysdep_sa_len(sa));

	return (error != len) ? -1 : 0;
}

long
random_between(x, y)
	long x;
	long y;
{
	long ratio;

	ratio = 1 << 16;
	while ((y - x) * ratio < (y - x))
		ratio = ratio / 2;
	return (x + ((y - x) * (ratio - 1) / random() & (ratio - 1)));
}

int
prefix6_mask(in6, plen)
	struct in6_addr *in6;
	int plen;
{
	struct sockaddr_in6 mask6;
	int i;

	if (sa6_plen2mask(&mask6, plen))
		return (-1);

	for (i = 0; i < 16; i++)
		in6->s6_addr[i] &= mask6.sin6_addr.s6_addr[i];

	return (0);
}

int
sa6_plen2mask(sa6, plen)
	struct sockaddr_in6 *sa6;
	int plen;
{
	u_char *cp;

	if (plen < 0 || plen > 128)
		return (-1);

	memset(sa6, 0, sizeof(*sa6));
	sa6->sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	sa6->sin6_len = sizeof(*sa6);
#endif

	for (cp = (u_char *)&sa6->sin6_addr; plen > 7; plen -= 8)
		*cp++ = 0xff;
	*cp = 0xff << (8 - plen);

	return (0);
}

char *
addr2str(sa)
	struct sockaddr *sa;
{
	static char addrbuf[8][NI_MAXHOST];
	static int round = 0;
	char *cp;

	round = (round + 1) & 7;
	cp = addrbuf[round];

	getnameinfo(sa, sysdep_sa_len(sa), cp, NI_MAXHOST,
	    NULL, 0, NI_NUMERICHOST);

	return (cp);
}

char *
in6addr2str(in6, scopeid)
	struct in6_addr *in6;
	int scopeid;
{
	struct sockaddr_in6 sa6;

	memset(&sa6, 0, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	sa6.sin6_len = sizeof(sa6);
#endif
	sa6.sin6_addr = *in6;
	sa6.sin6_scope_id = scopeid;

	return (addr2str((struct sockaddr *)&sa6));
}

/* return IPv6 address scope type. caller assumes that smaller is narrower. */
int
in6_scope(addr)
	struct in6_addr *addr;
{
	int scope;

	if (addr->s6_addr[0] == 0xfe) {
		scope = addr->s6_addr[1] & 0xc0;

		switch (scope) {
		case 0x80:
			return (2); /* link-local */
			break;
		case 0xc0:
			return (5); /* site-local */
			break;
		default:
			return (14); /* global: just in case */
			break;
		}
	}

	/* multicast scope. just return the scope field */
	if (addr->s6_addr[0] == 0xff)
		return (addr->s6_addr[1] & 0x0f);

	if (bcmp(&in6addr_loopback, addr, sizeof(addr) - 1) == 0) {
		if (addr->s6_addr[15] == 1) /* loopback */
			return (1);
		if (addr->s6_addr[15] == 0) /* unspecified */
			return (0); /* XXX: good value? */
	}

	return (14);		/* global */
}

static int
in6_matchflags(addr, ifnam, flags)
	struct sockaddr *addr;
	char *ifnam;
	int flags;
{
#ifdef __KAME__
	int s;
	struct in6_ifreq ifr6;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warn("in6_matchflags: socket(DGRAM6)");
		return (-1);
	}
	memset(&ifr6, 0, sizeof(ifr6));
	strncpy(ifr6.ifr_name, ifnam, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = *(struct sockaddr_in6 *)addr;

	if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		warn("in6_matchflags: ioctl(SIOCGIFAFLAG_IN6, %s)",
		     addr2str(addr));
		close(s);
		return (-1);
	}

	close(s);

	return (ifr6.ifr_ifru.ifru_flags6 & flags);
#else
	return (0);
#endif
}

int
get_duid(idfile, duid)
	char *idfile;
	struct duid *duid;
{
	FILE *fp = NULL;
	u_int16_t len = 0, hwtype;
	int hwlen = 0;
	char tmpbuf[256];	/* HWID should be no more than 256 bytes */

	if ((fp = fopen(idfile, "r")) == NULL && errno != ENOENT)
		dprintf(LOG_NOTICE, FNAME, "failed to open DUID file: %s",
		    idfile);

	memset(duid, 0, sizeof(*duid));
	if (fp) {
		/* decode length */
		if (fread(&len, sizeof(len), 1, fp) != 1) {
			dprintf(LOG_ERR, FNAME, "DUID file corrupted");
			goto fail;
		}
		duid->duid_len = len;
	} else {
		if ((hwlen = gethwid(tmpbuf, sizeof(tmpbuf), NULL, &hwtype)) < 0) {
			dprintf(LOG_INFO, FNAME,
			    "failed to get a hardware address");
			goto fail;
		}
		len = hwlen + sizeof(union dhcp6opt_duid_type);
	}

	if ((duid->duid_id = (char *)malloc(len)) == NULL) {
		dprintf(LOG_ERR, FNAME, "failed to allocate memory");
		goto fail;
	}

	/* copy (and fill) the ID */
	if (fp) {
		if (fread(duid->duid_id, len, 1, fp) != 1) {
			dprintf(LOG_ERR, FNAME, "DUID file corrupted");
			goto fail;
		}

		dprintf(LOG_DEBUG, FNAME,
		    "extracted an existing DUID from %s: %s",
		    idfile, duidstr(duid));
	} else {
		/* we only support the types 1,3 DUID */
		switch (duid_type) {
			case 1: {
				u_int64_t t64;
				struct dhcp6opt_duid_type1 *dp;

				duid->duid_len = hwlen + sizeof(struct dhcp6opt_duid_type1);
				dp = (struct dhcp6opt_duid_type1 *)duid->duid_id;
				dp->dh6_duid1_type = htons(1); /* type 1 */
				dp->dh6_duid1_hwtype = htons(hwtype);
				/* time is Jan 1, 2000 (UTC), modulo 2^32 */
				t64 = (u_int64_t)(time(NULL) - 946684800);
				dp->dh6_duid1_time = htonl((u_long)(t64 & 0xffffffff));
				memcpy((void *)(dp + 1), tmpbuf, hwlen);
				break;
			}
			case 3: {
				struct dhcp6opt_duid_type3 *dp;

				duid->duid_len = hwlen + sizeof(struct dhcp6opt_duid_type3);
				dp = (struct dhcp6opt_duid_type3 *)duid->duid_id;
				dp->dh6_duid3_type = htons(3); /* type 3 */
				dp->dh6_duid3_hwtype = htons(hwtype);
				memcpy((void *)(dp + 1), tmpbuf, hwlen);
				break;
			}
		}

		dprintf(LOG_DEBUG, FNAME, "generated a new DUID: %s",
		    duidstr(duid));
	}

	/* save the (new) ID to the file for next time */
	if (!fp) {
		if ((fp = fopen(idfile, "w+")) == NULL) {
			dprintf(LOG_ERR, FNAME,
			    "failed to open DUID file for save");
			goto fail;
		}
		len = duid->duid_len;
		if ((fwrite(&len, sizeof(len), 1, fp)) != 1) {
			dprintf(LOG_ERR, FNAME, "failed to save DUID");
			goto fail;
		}
		if ((fwrite(duid->duid_id, len, 1, fp)) != 1) {
			dprintf(LOG_ERR, FNAME, "failed to save DUID");
			goto fail;
		}

		dprintf(LOG_DEBUG, FNAME, "saved generated DUID to %s",
		    idfile);
	}

	if (fp)
		fclose(fp);
	return (0);

  fail:
	if (fp)
		fclose(fp);
	duidfree(duid);
	return (-1);
}

#ifdef __sun__
struct hwparms {
	char *buf;
	u_int16_t *hwtypep;
	ssize_t retval;
};

static ssize_t
getifhwaddr(const char *ifname, char *buf, u_int16_t *hwtypep, int ppa)
{
	int fd, flags;
	char fname[MAXPATHLEN], *cp;
	struct strbuf putctl;
	struct strbuf getctl;
	long getbuf[1024];
	dl_info_req_t dlir;
	dl_phys_addr_req_t dlpar;
	dl_phys_addr_ack_t *dlpaa;

	dprintf(LOG_DEBUG, FNAME, "trying %s ppa %d", ifname, ppa);

	if (ifname[0] == '\0')
		return (-1);
	if (ppa >= 0 && !isdigit(ifname[strlen(ifname) - 1]))
		(void) snprintf(fname, sizeof (fname), "/dev/%s%d", ifname,
		    ppa);
	else
		(void) snprintf(fname, sizeof (fname), "/dev/%s", ifname);
	getctl.maxlen = sizeof (getbuf);
	getctl.buf = (char *)getbuf;
	if ((fd = open(fname, O_RDWR)) == -1) {
		dl_attach_req_t dlar;

		cp = fname + strlen(fname) - 1;
		if (!isdigit(*cp))
			return (-1);
		while (cp > fname) {
			if (!isdigit(*cp))
				break;
			cp--;
		}
		if (cp == fname)
			return (-1);
		cp++;
		dlar.dl_ppa = atoi(cp);
		*cp = '\0';
		if ((fd = open(fname, O_RDWR)) == -1)
			return (-1);
		dlar.dl_primitive = DL_ATTACH_REQ;
		putctl.len = sizeof (dlar);
		putctl.buf = (char *)&dlar;
		if (putmsg(fd, &putctl, NULL, 0) == -1) {
			(void) close(fd);
			return (-1);
		}
		flags = 0;
		if (getmsg(fd, &getctl, NULL, &flags) == -1) {
			(void) close(fd);
			return (-1);
		}
		if (getbuf[0] != DL_OK_ACK) {
			(void) close(fd);
			return (-1);
		}
	}
	dlir.dl_primitive = DL_INFO_REQ;
	putctl.len = sizeof (dlir);
	putctl.buf = (char *)&dlir;
	if (putmsg(fd, &putctl, NULL, 0) == -1) {
		(void) close(fd);
		return (-1);
	}
	flags = 0;
	if (getmsg(fd, &getctl, NULL, &flags) == -1) {
		(void) close(fd);
		return (-1);
	}
	if (getbuf[0] != DL_INFO_ACK) {
		(void) close(fd);
		return (-1);
	}
	switch (((dl_info_ack_t *)getbuf)->dl_mac_type) {
	case DL_CSMACD:
	case DL_ETHER:
	case DL_100VG:
	case DL_ETH_CSMA:
	case DL_100BT:
		*hwtypep = ARPHRD_ETHER;
		break;
	default:
		(void) close(fd);
		return (-1);
	}
	dlpar.dl_primitive = DL_PHYS_ADDR_REQ;
	dlpar.dl_addr_type = DL_CURR_PHYS_ADDR;
	putctl.len = sizeof (dlpar);
	putctl.buf = (char *)&dlpar;
	if (putmsg(fd, &putctl, NULL, 0) == -1) {
		(void) close(fd);
		return (-1);
	}
	flags = 0;
	if (getmsg(fd, &getctl, NULL, &flags) == -1) {
		(void) close(fd);
		return (-1);
	}
	if (getbuf[0] != DL_PHYS_ADDR_ACK) {
		(void) close(fd);
		return (-1);
	}
	dlpaa = (dl_phys_addr_ack_t *)getbuf;
	if (dlpaa->dl_addr_length != 6) {
		(void) close(fd);
		return (-1);
	}
	(void) memcpy(buf, (char *)getbuf + dlpaa->dl_addr_offset,
	    dlpaa->dl_addr_length);
	return (dlpaa->dl_addr_length);
}

static int
devfs_handler(di_node_t node, di_minor_t minor, void *arg)
{
	struct hwparms *parms = arg;

	parms->retval = getifhwaddr(di_minor_name(minor), parms->buf,
	    parms->hwtypep, di_instance(node));
	return (parms->retval == -1 ? DI_WALK_CONTINUE : DI_WALK_TERMINATE);
}
#endif

static ssize_t
gethwid(buf, len, ifname, hwtypep)
	char *buf;
	int len;
	const char *ifname;
	u_int16_t *hwtypep;
{
	struct ifaddrs *ifa, *ifap;
#ifdef __KAME__
	struct sockaddr_dl *sdl;
#endif
#ifdef __linux__
	struct sockaddr_ll *sll;
#endif
	ssize_t l;

#ifdef __sun__
	if (ifname == NULL) {
		di_node_t root;
		struct hwparms parms;

		if ((root = di_init("/", DINFOSUBTREE | DINFOMINOR |
		    DINFOPROP)) == DI_NODE_NIL) {
			dprintf(LOG_INFO, FNAME, "di_init failed");
			return (-1);
		}
		parms.buf = buf;
		parms.hwtypep = hwtypep;
		parms.retval = -1;
		(void) di_walk_minor(root, DDI_NT_NET, DI_CHECK_ALIAS, &parms,
		    devfs_handler);
		di_fini(root);
		return (parms.retval);
	} else {
		return (getifhwaddr(ifname, buf, hwtypep, -1));
	}
#endif

	if (getifaddrs(&ifap) < 0)
		return (-1);

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifname && strcmp(ifa->ifa_name, ifname) != 0)
			continue;
		if (ifa->ifa_addr == NULL)
			continue;
#ifdef __KAME__
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (len < 2 + sdl->sdl_alen)
			goto fail;
		/*
		 * translate interface type to hardware type based on
		 * http://www.iana.org/assignments/arp-parameters
		 */
		switch(sdl->sdl_type) {
		case IFT_ETHER:
#ifdef IFT_IEEE80211
		case IFT_IEEE80211:
#endif
			*hwtypep = ARPHRD_ETHER;
			break;
		default:
			continue; /* XXX */
		}
		dprintf(LOG_DEBUG, FNAME, "found an interface %s for DUID",
		    ifa->ifa_name);
		memcpy(buf, LLADDR(sdl), sdl->sdl_alen);
		l = sdl->sdl_alen; /* sdl will soon be freed */
#endif
#ifdef __linux__
		if (ifa->ifa_addr->sa_family != AF_PACKET)
			continue;

		sll = (struct sockaddr_ll *)ifa->ifa_addr;
		if (sll->sll_hatype != ARPHRD_ETHER)
			continue;
		*hwtypep = ARPHRD_ETHER;
		dprintf(LOG_DEBUG, FNAME, "found an interface %s for DUID",
		    ifa->ifa_name);
		memcpy(buf, sll->sll_addr, sll->sll_halen);
		l = sll->sll_halen; /* sll will soon be freed */
#endif
		freeifaddrs(ifap);
		return (l);
	}

#ifdef __KAME__
  fail:
#endif
	freeifaddrs(ifap);
	return (-1);
}

void
dhcp6_init_options(optinfo)
	struct dhcp6_optinfo *optinfo;
{
	memset(optinfo, 0, sizeof(*optinfo));

	optinfo->pref = DH6OPT_PREF_UNDEF;
	optinfo->elapsed_time = DH6OPT_ELAPSED_TIME_UNDEF;
	optinfo->refreshtime = DH6OPT_REFRESHTIME_UNDEF;

	TAILQ_INIT(&optinfo->iapd_list);
	TAILQ_INIT(&optinfo->iana_list);
	TAILQ_INIT(&optinfo->reqopt_list);
	TAILQ_INIT(&optinfo->stcode_list);
	TAILQ_INIT(&optinfo->sip_list);
	TAILQ_INIT(&optinfo->sipname_list);
	TAILQ_INIT(&optinfo->dns_list);
	TAILQ_INIT(&optinfo->dnsname_list);
	TAILQ_INIT(&optinfo->ntp_list);
	TAILQ_INIT(&optinfo->prefix_list);
	TAILQ_INIT(&optinfo->nis_list);
	TAILQ_INIT(&optinfo->nisname_list);
	TAILQ_INIT(&optinfo->nisp_list);
	TAILQ_INIT(&optinfo->nispname_list);
	TAILQ_INIT(&optinfo->bcmcs_list);
	TAILQ_INIT(&optinfo->bcmcsname_list);

	optinfo->authproto = DHCP6_AUTHPROTO_UNDEF;
	optinfo->authalgorithm = DHCP6_AUTHALG_UNDEF;
	optinfo->authrdm = DHCP6_AUTHRDM_UNDEF;
}

void
dhcp6_clear_options(optinfo)
	struct dhcp6_optinfo *optinfo;
{
	switch (optinfo->authproto) {
	case DHCP6_AUTHPROTO_DELAYED:
		if (optinfo->delayedauth_realmval != NULL) {
			free(optinfo->delayedauth_realmval);
		}
		break;
	}

	duidfree(&optinfo->clientID);
	duidfree(&optinfo->serverID);

	dhcp6_clear_list(&optinfo->iapd_list);
	dhcp6_clear_list(&optinfo->iana_list);
	dhcp6_clear_list(&optinfo->reqopt_list);
	dhcp6_clear_list(&optinfo->stcode_list);
	dhcp6_clear_list(&optinfo->sip_list);
	dhcp6_clear_list(&optinfo->sipname_list);
	dhcp6_clear_list(&optinfo->dns_list);
	dhcp6_clear_list(&optinfo->dnsname_list);
	dhcp6_clear_list(&optinfo->ntp_list);
	dhcp6_clear_list(&optinfo->prefix_list);
	dhcp6_clear_list(&optinfo->nis_list);
	dhcp6_clear_list(&optinfo->nisname_list);
	dhcp6_clear_list(&optinfo->nisp_list);
	dhcp6_clear_list(&optinfo->nispname_list);
	dhcp6_clear_list(&optinfo->bcmcs_list);
	dhcp6_clear_list(&optinfo->bcmcsname_list);

	if (optinfo->relaymsg_msg != NULL)
		free(optinfo->relaymsg_msg);

	if (optinfo->ifidopt_id != NULL)
		free(optinfo->ifidopt_id);

	dhcp6_init_options(optinfo);
}

int
dhcp6_copy_options(dst, src)
	struct dhcp6_optinfo *dst, *src;
{
	if (duidcpy(&dst->clientID, &src->clientID))
		goto fail;
	if (duidcpy(&dst->serverID, &src->serverID))
		goto fail;
	dst->rapidcommit = src->rapidcommit;

	if (dhcp6_copy_list(&dst->iapd_list, &src->iapd_list))
		goto fail;
	if (dhcp6_copy_list(&dst->iana_list, &src->iana_list))
		goto fail;
	if (dhcp6_copy_list(&dst->reqopt_list, &src->reqopt_list))
		goto fail;
	if (dhcp6_copy_list(&dst->stcode_list, &src->stcode_list))
		goto fail;
	if (dhcp6_copy_list(&dst->sip_list, &src->sip_list))
		goto fail;
	if (dhcp6_copy_list(&dst->sipname_list, &src->sipname_list))
		goto fail;
	if (dhcp6_copy_list(&dst->dns_list, &src->dns_list))
		goto fail;
	if (dhcp6_copy_list(&dst->dnsname_list, &src->dnsname_list))
		goto fail;
	if (dhcp6_copy_list(&dst->ntp_list, &src->ntp_list))
		goto fail;
	if (dhcp6_copy_list(&dst->prefix_list, &src->prefix_list))
		goto fail;
	if (dhcp6_copy_list(&dst->nis_list, &src->nis_list))
		goto fail;
	if (dhcp6_copy_list(&dst->nisname_list, &src->nisname_list))
		goto fail;
	if (dhcp6_copy_list(&dst->nisp_list, &src->nisp_list))
		goto fail;
	if (dhcp6_copy_list(&dst->nispname_list, &src->nispname_list))
		goto fail;
	if (dhcp6_copy_list(&dst->bcmcs_list, &src->bcmcs_list))
		goto fail;
	if (dhcp6_copy_list(&dst->bcmcsname_list, &src->bcmcsname_list))
		goto fail;
	dst->elapsed_time = src->elapsed_time;
	dst->refreshtime = src->refreshtime;
	dst->pref = src->pref;

	if (src->relaymsg_msg != NULL) {
		if ((dst->relaymsg_msg = malloc(src->relaymsg_len)) == NULL)
			goto fail;
		dst->relaymsg_len = src->relaymsg_len;
		memcpy(dst->relaymsg_msg, src->relaymsg_msg,
		    src->relaymsg_len);
	}

	if (src->ifidopt_id != NULL) {
		if ((dst->ifidopt_id = malloc(src->ifidopt_len)) == NULL)
			goto fail;
		dst->ifidopt_len = src->ifidopt_len;
		memcpy(dst->ifidopt_id, src->ifidopt_id, src->ifidopt_len);
	}

	dst->authflags = src->authflags;
	dst->authproto = src->authproto;
	dst->authalgorithm = src->authalgorithm;
	dst->authrdm = src->authrdm;
	dst->authrd = src->authrd;

	switch (src->authproto) {
	case DHCP6_AUTHPROTO_DELAYED:
		dst->delayedauth_keyid = src->delayedauth_keyid;
		dst->delayedauth_offset = src->delayedauth_offset;
		dst->delayedauth_realmlen = src->delayedauth_realmlen;
		if (src->delayedauth_realmval != NULL) {
			if ((dst->delayedauth_realmval =
			    malloc(src->delayedauth_realmlen)) == NULL) {
				goto fail;
			}
			memcpy(dst->delayedauth_realmval,
			    src->delayedauth_realmval,
			    src->delayedauth_realmlen);
		}
		break;
	case DHCP6_AUTHPROTO_RECONFIG:
		dst->reconfigauth_type = src->reconfigauth_type;
		dst->reconfigauth_offset = src->reconfigauth_offset;
		memcpy(dst->reconfigauth_val, src->reconfigauth_val,
		    sizeof(dst->reconfigauth_val));
		break;
	}

	return (0);

  fail:
	/* cleanup temporary resources */
	dhcp6_clear_options(dst);
	return (-1);
}

int
dhcp6_get_options(p, ep, optinfo)
	struct dhcp6opt *p, *ep;
	struct dhcp6_optinfo *optinfo;
{
	struct dhcp6opt *np, opth;
	int i, opt, optlen, reqopts, num;
	u_int16_t num16;
	char *bp, *cp, *val;
	u_int16_t val16;
	u_int32_t val32;
	struct dhcp6opt_ia optia;
	struct dhcp6_ia ia;
	struct dhcp6_list sublist;
	int authinfolen;

	bp = (char *)p;
	for (; p + 1 <= ep; p = np) {
		struct duid duid0;

		/*
		 * get the option header.  XXX: since there is no guarantee
		 * about the header alignment, we need to make a local copy.
		 */
		memcpy(&opth, p, sizeof(opth));
		optlen = ntohs(opth.dh6opt_len);
		opt = ntohs(opth.dh6opt_type);

		cp = (char *)(p + 1);
		np = (struct dhcp6opt *)(cp + optlen);

		dprintf(LOG_DEBUG, FNAME, "get DHCP option %s, len %d",
		    dhcp6optstr(opt), optlen);

		/* option length field overrun */
		if (np > ep) {
			dprintf(LOG_INFO, FNAME, "malformed DHCP options");
			goto fail;
		}

		switch (opt) {
		case DH6OPT_CLIENTID:
			if (optlen == 0)
				goto malformed;
			duid0.duid_len = optlen;
			duid0.duid_id = cp;
			dprintf(LOG_DEBUG, "",
				"  DUID: %s", duidstr(&duid0));
			if (duidcpy(&optinfo->clientID, &duid0)) {
				dprintf(LOG_ERR, FNAME, "failed to copy DUID");
				goto fail;
			}
			break;
		case DH6OPT_SERVERID:
			if (optlen == 0)
				goto malformed;
			duid0.duid_len = optlen;
			duid0.duid_id = cp;
			dprintf(LOG_DEBUG, "", "  DUID: %s", duidstr(&duid0));
			if (duidcpy(&optinfo->serverID, &duid0)) {
				dprintf(LOG_ERR, FNAME, "failed to copy DUID");
				goto fail;
			}
			break;
		case DH6OPT_STATUS_CODE:
			if (optlen < sizeof(u_int16_t))
				goto malformed;
			memcpy(&val16, cp, sizeof(val16));
			num16 = ntohs(val16);
			dprintf(LOG_DEBUG, "", "  status code: %s",
			    dhcp6_stcodestr(num16));

			/* need to check duplication? */

			if (dhcp6_add_listval(&optinfo->stcode_list,
			    DHCP6_LISTVAL_STCODE, &num16, NULL) == NULL) {
				dprintf(LOG_ERR, FNAME, "failed to copy "
				    "status code");
				goto fail;
			}

			break;
		case DH6OPT_ORO:
			if ((optlen % 2) != 0 || optlen == 0)
				goto malformed;
			reqopts = optlen / 2;
			for (i = 0, val = cp; i < reqopts;
			     i++, val += sizeof(u_int16_t)) {
				u_int16_t opttype;

				memcpy(&opttype, val, sizeof(u_int16_t));
				num = (int)ntohs(opttype);

				dprintf(LOG_DEBUG, "",
					"  requested option: %s",
					dhcp6optstr(num));

				if (dhcp6_find_listval(&optinfo->reqopt_list,
				    DHCP6_LISTVAL_NUM, &num, 0)) {
					dprintf(LOG_INFO, FNAME, "duplicated "
					    "option type (%s)",
					    dhcp6optstr(opttype));
					goto nextoption;
				}

				if (dhcp6_add_listval(&optinfo->reqopt_list,
				    DHCP6_LISTVAL_NUM, &num, NULL) == NULL) {
					dprintf(LOG_ERR, FNAME,
					    "failed to copy requested option");
					goto fail;
				}
			  nextoption:
				;
			}
			break;
		case DH6OPT_PREFERENCE:
			if (optlen != 1)
				goto malformed;
			dprintf(LOG_DEBUG, "", "  preference: %d",
			    (int)*(u_char *)cp);
			if (optinfo->pref != DH6OPT_PREF_UNDEF) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated preference option");
			} else
				optinfo->pref = (int)*(u_char *)cp;
			break;
		case DH6OPT_ELAPSED_TIME:
			if (optlen != 2)
				goto malformed;
			memcpy(&val16, cp, sizeof(val16));
			val16 = ntohs(val16);
			dprintf(LOG_DEBUG, "", "  elapsed time: %lu",
			    (u_int32_t)val16);
			if (optinfo->elapsed_time !=
			    DH6OPT_ELAPSED_TIME_UNDEF) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated elapsed time option");
			} else
				optinfo->elapsed_time = val16;
			break;
		case DH6OPT_RELAY_MSG:
			if ((optinfo->relaymsg_msg = malloc(optlen)) == NULL)
				goto fail;
			memcpy(optinfo->relaymsg_msg, cp, optlen);
			optinfo->relaymsg_len = optlen;
			break;
		case DH6OPT_AUTH:
			if (optlen < sizeof(struct dhcp6opt_auth) - 4)
				goto malformed;

			/*
			 * Any DHCP message that includes more than one
			 * authentication option MUST be discarded.
			 * [RFC3315 Section 21.4.2]
			 */
			if (optinfo->authproto != DHCP6_AUTHPROTO_UNDEF) {
				dprintf(LOG_INFO, FNAME, "found more than one "
				    "authentication option");
				goto fail;
			}

			optinfo->authproto = *cp++;
			optinfo->authalgorithm = *cp++;
			optinfo->authrdm = *cp++;
			memcpy(&optinfo->authrd, cp, sizeof(optinfo->authrd));
			cp += sizeof(optinfo->authrd);

			dprintf(LOG_DEBUG, "", "  %s", sprint_auth(optinfo));

			authinfolen =
			    optlen - (sizeof(struct dhcp6opt_auth) - 4);
			switch (optinfo->authproto) {
			case DHCP6_AUTHPROTO_DELAYED:
				if (authinfolen == 0) {
					optinfo->authflags |=
					    DHCP6OPT_AUTHFLAG_NOINFO;
					break;
				}
				/* XXX: should we reject an empty realm? */
				if (authinfolen <
				    sizeof(optinfo->delayedauth_keyid) + 16) {
					goto malformed;
				}

				optinfo->delayedauth_realmlen = authinfolen -
				    (sizeof(optinfo->delayedauth_keyid) + 16);
				optinfo->delayedauth_realmval =
				    malloc(optinfo->delayedauth_realmlen);
				if (optinfo->delayedauth_realmval == NULL) {
					dprintf(LOG_WARNING, FNAME, "failed "
					    "allocate memory for auth realm");
					goto fail;
				}
				memcpy(optinfo->delayedauth_realmval, cp,
				    optinfo->delayedauth_realmlen);
				cp += optinfo->delayedauth_realmlen;

				memcpy(&optinfo->delayedauth_keyid, cp,
				    sizeof(optinfo->delayedauth_keyid));
				optinfo->delayedauth_keyid =
				    ntohl(optinfo->delayedauth_keyid);
				cp += sizeof(optinfo->delayedauth_keyid);

				optinfo->delayedauth_offset = cp - bp;
				cp += 16;

				dprintf(LOG_DEBUG, "", "  auth key ID: %x, "
				    "offset=%d, realmlen=%d",
				    optinfo->delayedauth_keyid,
				    optinfo->delayedauth_offset,
				    optinfo->delayedauth_realmlen);
				break;
#ifdef notyet
			case DHCP6_AUTHPROTO_RECONFIG:
				break;
#endif
			default:
				dprintf(LOG_INFO, FNAME,
				    "unsupported authentication protocol: %d",
				    *cp);
				goto fail;
			}
			break;
		case DH6OPT_RAPID_COMMIT:
			if (optlen != 0)
				goto malformed;
			optinfo->rapidcommit = 1;
			break;
		case DH6OPT_INTERFACE_ID:
			if ((optinfo->ifidopt_id = malloc(optlen)) == NULL)
				goto fail;
			memcpy(optinfo->ifidopt_id, cp, optlen);
			optinfo->ifidopt_len = optlen;
			break;
		case DH6OPT_SIP_SERVER_D:
			if (dhcp6_get_domain(optlen, cp, opt,
			    &optinfo->sipname_list) == -1)
				goto fail;
			break;
		case DH6OPT_DNSNAME:
			if (dhcp6_get_domain(optlen, cp, opt,
			    &optinfo->dnsname_list) == -1)
				goto fail;
			break;
		case DH6OPT_NIS_DOMAIN_NAME:
			if (dhcp6_get_domain(optlen, cp, opt,
			    &optinfo->nisname_list) == -1)
				goto fail;
			break;
		case DH6OPT_NISP_DOMAIN_NAME:
			if (dhcp6_get_domain(optlen, cp, opt,
			    &optinfo->nispname_list) == -1)
				goto fail;
			break;
		case DH6OPT_BCMCS_SERVER_D:
			if (dhcp6_get_domain(optlen, cp, opt,
			    &optinfo->bcmcsname_list) == -1)
				goto fail;
			break;
		case DH6OPT_SIP_SERVER_A:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->sip_list) == -1)
				goto fail;
			break;
		case DH6OPT_DNS:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->dns_list) == -1)
				goto fail;
			break;
		case DH6OPT_NIS_SERVERS:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->nis_list) == -1)
				goto fail;
			break;
		case DH6OPT_NISP_SERVERS:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->nisp_list) == -1)
				goto fail;
			break;
		case DH6OPT_BCMCS_SERVER_A:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->bcmcs_list) == -1)
				goto fail;
			break;
		case DH6OPT_NTP:
			if (dhcp6_get_addr(optlen, cp, opt,
			    &optinfo->ntp_list) == -1)
				goto fail;
			break;
		case DH6OPT_IA_PD:
			if (optlen + sizeof(struct dhcp6opt) <
			    sizeof(optia))
				goto malformed;
			memcpy(&optia, p, sizeof(optia));
			ia.iaid = ntohl(optia.dh6_ia_iaid);
			ia.t1 = ntohl(optia.dh6_ia_t1);
			ia.t2 = ntohl(optia.dh6_ia_t2);

			dprintf(LOG_DEBUG, "",
			    "  IA_PD: ID=%lu, T1=%lu, T2=%lu",
			    ia.iaid, ia.t1, ia.t2);

			/* duplication check */
			if (dhcp6_find_listval(&optinfo->iapd_list,
			    DHCP6_LISTVAL_IAPD, &ia, 0)) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated IA_PD %lu", ia.iaid);
				break; /* ignore this IA_PD */
			}

			/* take care of sub-options */
			TAILQ_INIT(&sublist);
			if (copyin_option(opt,
			    (struct dhcp6opt *)((char *)p + sizeof(optia)),
			    (struct dhcp6opt *)(cp + optlen), &sublist)) {
				goto fail;
			}

			/* link this option set */
			if (dhcp6_add_listval(&optinfo->iapd_list,
			    DHCP6_LISTVAL_IAPD, &ia, &sublist) == NULL) {
				dhcp6_clear_list(&sublist);
				goto fail;
			}
			dhcp6_clear_list(&sublist);

			break;
		case DH6OPT_REFRESHTIME:
			if (optlen != 4)
				goto malformed;
			memcpy(&val32, cp, sizeof(val32));
			val32 = ntohl(val32);
			dprintf(LOG_DEBUG, "",
			    "   information refresh time: %lu", val32);
			if (val32 < DHCP6_IRT_MINIMUM) {
				/*
				 * A client MUST use the refresh time
				 * IRT_MINIMUM if it receives the option with a
				 * value less than IRT_MINIMUM.
				 * [draft-ietf-dhc-lifetime-02.txt,
				 *  Section 3.2]
				 */
				dprintf(LOG_INFO, FNAME,
				    "refresh time is too small (%d), adjusted",
				    val32);
				val32 = DHCP6_IRT_MINIMUM;
			}
			if (optinfo->refreshtime != DH6OPT_REFRESHTIME_UNDEF) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated refresh time option");
			} else
				optinfo->refreshtime = (int64_t)val32;
			break;
		case DH6OPT_IA_NA:
			if (optlen + sizeof(struct dhcp6opt) <
			    sizeof(optia))
				goto malformed;
			memcpy(&optia, p, sizeof(optia));
			ia.iaid = ntohl(optia.dh6_ia_iaid);
			ia.t1 = ntohl(optia.dh6_ia_t1);
			ia.t2 = ntohl(optia.dh6_ia_t2);

			dprintf(LOG_DEBUG, "",
			    "  IA_NA: ID=%lu, T1=%lu, T2=%lu",
			    ia.iaid, ia.t1, ia.t2);

			/* duplication check */
			if (dhcp6_find_listval(&optinfo->iana_list,
			    DHCP6_LISTVAL_IANA, &ia, 0)) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated IA_NA %lu", ia.iaid);
				break; /* ignore this IA_NA */
			}

			/* take care of sub-options */
			TAILQ_INIT(&sublist);
			if (copyin_option(opt,
			    (struct dhcp6opt *)((char *)p + sizeof(optia)),
			    (struct dhcp6opt *)(cp + optlen), &sublist)) {
				goto fail;
			}

			/* link this option set */
			if (dhcp6_add_listval(&optinfo->iana_list,
			    DHCP6_LISTVAL_IANA, &ia, &sublist) == NULL) {
				dhcp6_clear_list(&sublist);
				goto fail;
			}
			dhcp6_clear_list(&sublist);

			break;
		default:
			/* no option specific behavior */
			dprintf(LOG_INFO, FNAME,
			    "unknown or unexpected DHCP6 option %s, len %d",
			    dhcp6optstr(opt), optlen);
			break;
		}
	}

	return (0);

  malformed:
	dprintf(LOG_INFO, FNAME, "malformed DHCP option: type %d, len %d",
	    opt, optlen);
  fail:
	dhcp6_clear_options(optinfo);
	return (-1);
}

static char *
dnsdecode(sp, ep, buf, bufsiz)
	u_char **sp;
	u_char *ep;
	char *buf;
	size_t bufsiz;
{
	int i, l;
	u_char *cp;
	char tmpbuf[MAXDNAME + 1];

	cp = *sp;
	*buf = '\0';
	i = 0;			/* XXX: appease gcc */

	if (cp >= ep)
		return (NULL);
	while (cp < ep) {
		i = *cp;
		if (i == 0 || cp != *sp) {
			if (strlcat((char *)buf, ".", bufsiz) >= bufsiz)
				return (NULL);	/* result overrun */
		}
		if (i == 0)
			break;
		cp++;

		if (i > 0x3f)
			return (NULL); /* invalid label */

		if (i > ep - cp)
			return (NULL); /* source overrun */
		while (i-- > 0 && cp < ep) {
			if (!isprint(*cp)) /* we don't accept non-printables */
				return (NULL);
			l = snprintf(tmpbuf, sizeof(tmpbuf), "%c" , *cp);
			if (l >= sizeof(tmpbuf) || l < 0)
				return (NULL);
			if (strlcat(buf, tmpbuf, bufsiz) >= bufsiz)
				return (NULL); /* result overrun */
			cp++;
		}
	}
	if (i != 0)
		return (NULL);	/* not terminated */
	cp++;
	*sp = cp;
	return (buf);
}

static int
copyin_option(type, p, ep, list)
	int type;
	struct dhcp6opt *p, *ep;
	struct dhcp6_list *list;
{
	int opt, optlen;
	char *cp;
	struct dhcp6opt *np, opth;
	struct dhcp6opt_stcode opt_stcode;
	struct dhcp6opt_ia_pd_prefix opt_iapd_prefix;
	struct dhcp6_prefix iapd_prefix;
	struct dhcp6opt_ia_addr opt_ia_addr;
	struct dhcp6_prefix ia_addr;
	struct dhcp6_list sublist;

	TAILQ_INIT(&sublist);

	for (; p + 1 <= ep; p = np) {
		memcpy(&opth, p, sizeof(opth));
		optlen = ntohs(opth.dh6opt_len);
		opt = ntohs(opth.dh6opt_type);

		cp = (char *)(p + 1);
		np = (struct dhcp6opt *)(cp + optlen);

		dprintf(LOG_DEBUG, FNAME, "get DHCP option %s, len %d",
		    dhcp6optstr(opt), optlen);

		if (np > ep) {
			dprintf(LOG_INFO, FNAME, "malformed DHCP option");
			goto fail;
		}

		switch (opt) {
		case DH6OPT_IA_PD_PREFIX:
			/* check option context */
			if (type != DH6OPT_IA_PD) {
				dprintf(LOG_INFO, FNAME,
				    "%s is an invalid position for %s",
				    dhcp6optstr(type), dhcp6optstr(opt));
				goto fail;
			}
			/* check option length */
			if (optlen + sizeof(opth) < sizeof(opt_iapd_prefix))
				goto malformed;

			/* copy and convert option values */
			memcpy(&opt_iapd_prefix, p, sizeof(opt_iapd_prefix));
			if (opt_iapd_prefix.dh6_iapd_prefix_prefix_len > 128) {
				dprintf(LOG_INFO, FNAME,
				    "invalid prefix length (%d)",
				    opt_iapd_prefix.dh6_iapd_prefix_prefix_len);
				goto malformed;
			}
			iapd_prefix.pltime = ntohl(opt_iapd_prefix.dh6_iapd_prefix_preferred_time);
			iapd_prefix.vltime = ntohl(opt_iapd_prefix.dh6_iapd_prefix_valid_time);
			iapd_prefix.plen =
			    opt_iapd_prefix.dh6_iapd_prefix_prefix_len;
			memcpy(&iapd_prefix.addr,
			    &opt_iapd_prefix.dh6_iapd_prefix_prefix_addr,
			    sizeof(iapd_prefix.addr));
			/* clear padding bits in the prefix address */
			prefix6_mask(&iapd_prefix.addr, iapd_prefix.plen);

			dprintf(LOG_DEBUG, FNAME, "  IA_PD prefix: "
			    "%s/%d pltime=%lu vltime=%lu",
			    in6addr2str(&iapd_prefix.addr, 0),
			    iapd_prefix.plen,
			    iapd_prefix.pltime, iapd_prefix.vltime);

			if (dhcp6_find_listval(list, DHCP6_LISTVAL_PREFIX6,
			    &iapd_prefix, 0)) {
				dprintf(LOG_INFO, FNAME, 
				    "duplicated IA_PD prefix "
				    "%s/%d pltime=%lu vltime=%lu",
				    in6addr2str(&iapd_prefix.addr, 0),
				    iapd_prefix.plen,
				    iapd_prefix.pltime, iapd_prefix.vltime);
				goto nextoption;
			}

			/* take care of sub-options */
			TAILQ_INIT(&sublist);
			if (copyin_option(opt,
			    (struct dhcp6opt *)((char *)p +
			    sizeof(opt_iapd_prefix)), np, &sublist)) {
				goto fail;
			}

			if (dhcp6_add_listval(list, DHCP6_LISTVAL_PREFIX6,
			    &iapd_prefix, &sublist) == NULL) {
				dhcp6_clear_list(&sublist);
				goto fail;
			}
			dhcp6_clear_list(&sublist);
			break;
		case DH6OPT_IAADDR:
			/* check option context */
			if (type != DH6OPT_IA_NA) {
				dprintf(LOG_INFO, FNAME,
				    "%s is an invalid position for %s",
				    dhcp6optstr(type), dhcp6optstr(opt));
				goto fail;
			}
			/* check option length */
			if (optlen + sizeof(opth) < sizeof(opt_ia_addr))
				goto malformed;

			/* copy and convert option values */
			memcpy(&opt_ia_addr, p, sizeof(opt_ia_addr));
			ia_addr.pltime = ntohl(opt_ia_addr.dh6_ia_addr_preferred_time);
			ia_addr.vltime = ntohl(opt_ia_addr.dh6_ia_addr_valid_time);
			memcpy(&ia_addr.addr, &opt_ia_addr.dh6_ia_addr_addr,
			    sizeof(ia_addr.addr));

			dprintf(LOG_DEBUG, FNAME, "  IA_NA address: "
			    "%s pltime=%lu vltime=%lu",
			    in6addr2str(&ia_addr.addr, 0),
			    ia_addr.pltime, ia_addr.vltime);

			if (dhcp6_find_listval(list,
			    DHCP6_LISTVAL_STATEFULADDR6, &ia_addr, 0)) {
				dprintf(LOG_INFO, FNAME, 
				    "duplicated IA_NA address"
				    "%s pltime=%lu vltime=%lu",
				    in6addr2str(&ia_addr.addr, 0),
				    ia_addr.pltime, ia_addr.vltime);
				goto nextoption;
			}

			/* take care of sub-options */
			TAILQ_INIT(&sublist);
			if (copyin_option(opt,
			    (struct dhcp6opt *)((char *)p +
			    sizeof(opt_ia_addr)), np, &sublist)) {
				goto fail;
			}

			if (dhcp6_add_listval(list, DHCP6_LISTVAL_STATEFULADDR6,
			    &ia_addr, &sublist) == NULL) {
				dhcp6_clear_list(&sublist);
				goto fail;
			}
			dhcp6_clear_list(&sublist);
			break;
		case DH6OPT_STATUS_CODE:
			/* check option context */
			if (type != DH6OPT_IA_PD &&
			    type != DH6OPT_IA_PD_PREFIX &&
			    type != DH6OPT_IA_NA &&
			    type != DH6OPT_IAADDR) {
				dprintf(LOG_INFO, FNAME,
				    "%s is an invalid position for %s",
				    dhcp6optstr(type), dhcp6optstr(opt));
				goto nextoption; /* or discard the message? */
			}
			/* check option length */
			if (optlen + sizeof(opth) < sizeof(opt_stcode))
				goto malformed;

			/* copy and convert option values */
			memcpy(&opt_stcode, p, sizeof(opt_stcode));
			opt_stcode.dh6_stcode_code =
			    ntohs(opt_stcode.dh6_stcode_code);

			dprintf(LOG_DEBUG, "", "  status code: %s",
			    dhcp6_stcodestr(opt_stcode.dh6_stcode_code));

			/* duplication check */
			if (dhcp6_find_listval(list, DHCP6_LISTVAL_STCODE,
			    &opt_stcode.dh6_stcode_code, 0)) {
				dprintf(LOG_INFO, FNAME,
				    "duplicated status code (%d)",
				    opt_stcode.dh6_stcode_code);
				goto nextoption;
			}

			/* copy-in the code value */
			if (dhcp6_add_listval(list, DHCP6_LISTVAL_STCODE,
			    &opt_stcode.dh6_stcode_code, NULL) == NULL)
				goto fail;

			break;
		}
	  nextoption:
		;
	}

	return (0);

  malformed:
	dprintf(LOG_INFO, "", "  malformed DHCP option: type %d", opt);

  fail:
	dhcp6_clear_list(&sublist);
	return (-1);
}

static char *
sprint_uint64(buf, buflen, i64)
	char *buf;
	int buflen;
	u_int64_t i64;
{
	u_int16_t rd0, rd1, rd2, rd3;
	u_int16_t *ptr = (u_int16_t *)(void *)&i64;

	rd0 = ntohs(*ptr++);
	rd1 = ntohs(*ptr++);
	rd2 = ntohs(*ptr++);
	rd3 = ntohs(*ptr);

	snprintf(buf, buflen, "%04x %04x %04x %04x", rd0, rd1, rd2, rd3);

	return (buf);
}

static char *
sprint_auth(optinfo)
	struct dhcp6_optinfo *optinfo;
{
	static char ret[1024];	/* XXX: thread unsafe */
	char *proto, proto0[] = "unknown(255)";
	char *alg, alg0[] = "unknown(255)";
	char *rdm, rdm0[] = "unknown(255)";
	char rd[] = "ffff ffff ffff ffff";

	switch (optinfo->authproto) {
	case DHCP6_AUTHPROTO_DELAYED:
		proto = "delayed";
		break;
	case DHCP6_AUTHPROTO_RECONFIG:
		proto = "reconfig";
		break;
	default:
		snprintf(proto0, sizeof(proto0), "unknown(%d)",
		    optinfo->authproto & 0xff);
		proto = proto0;
		break;
	}

	switch (optinfo->authalgorithm) {
	case DHCP6_AUTHALG_HMACMD5:
		alg = "HMAC-MD5";
		break;
	default:
		snprintf(alg0, sizeof(alg0), "unknown(%d)",
		    optinfo->authalgorithm & 0xff);
		alg = alg0;
		break;
	}

	switch (optinfo->authrdm) {
	case DHCP6_AUTHRDM_MONOCOUNTER:
		rdm = "mono counter";
		break;
	default:
		snprintf(rdm0, sizeof(rdm0), "unknown(%d)", optinfo->authrdm);
		rdm = rdm0;
	}

	(void)sprint_uint64(rd, sizeof(rd), optinfo->authrd);

	snprintf(ret, sizeof(ret), "proto: %s, alg: %s, RDM: %s, RD: %s",
	    proto, alg, rdm, rd);

	return (ret);
}

static int
copy_option(type, len, val, optp, ep, totallenp)
	u_int16_t type, len;
	void *val;
	struct dhcp6opt **optp, *ep;
	int *totallenp;
{
	struct dhcp6opt *opt = *optp, opth;

	if ((void *)ep - (void *)optp < len + sizeof(struct dhcp6opt)) {
		dprintf(LOG_INFO, FNAME,
		    "option buffer short for %s", dhcp6optstr(type));
		return (-1);
	}
	opth.dh6opt_type = htons(type);
	opth.dh6opt_len = htons(len);
	memcpy(opt, &opth, sizeof(opth));
	if (len != 0)
		memcpy(opt + 1, val, len);

	*optp = (struct dhcp6opt *)((char *)(opt + 1) + len);
 	*totallenp += sizeof(struct dhcp6opt) + len;
	dprintf(LOG_DEBUG, FNAME, "set %s (len %d)", dhcp6optstr(type), len);

	return (0);
}

int
dhcp6_set_options(type, optbp, optep, optinfo)
	int type;
	struct dhcp6opt *optbp, *optep;
	struct dhcp6_optinfo *optinfo;
{
	struct dhcp6opt *p = optbp;
	struct dhcp6_listval *stcode, *op;
	int len = 0, optlen;
	char *tmpbuf = NULL;

	if (optinfo->clientID.duid_len) {
		if (copy_option(DH6OPT_CLIENTID, optinfo->clientID.duid_len,
		    optinfo->clientID.duid_id, &p, optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->serverID.duid_len) {
		if (copy_option(DH6OPT_SERVERID, optinfo->serverID.duid_len,
		    optinfo->serverID.duid_id, &p, optep, &len) != 0) {
			goto fail;
		}
	}

	for (op = TAILQ_FIRST(&optinfo->iana_list); op;
	    op = TAILQ_NEXT(op, link)) {
		int optlen;

		tmpbuf = NULL;
		if ((optlen = copyout_option(NULL, NULL, op)) < 0) {
			dprintf(LOG_INFO, FNAME,
			    "failed to count option length");
			goto fail;
		}
		if ((void *)optep - (void *)p < optlen) {
			dprintf(LOG_INFO, FNAME, "short buffer");
			goto fail;
		}
		if ((tmpbuf = malloc(optlen)) == NULL) {
			dprintf(LOG_NOTICE, FNAME,
			    "memory allocation failed for IA_NA options");
			goto fail;
		}
		if (copyout_option(tmpbuf, tmpbuf + optlen, op) < 0) {
			dprintf(LOG_ERR, FNAME,
			    "failed to construct an IA_NA option");
			goto fail;
		}
		memcpy(p, tmpbuf, optlen);
		free(tmpbuf);
		tmpbuf = NULL;
		p = (struct dhcp6opt *)((char *)p + optlen);
		len += optlen;
	}

	if (optinfo->rapidcommit) {
		if (copy_option(DH6OPT_RAPID_COMMIT, 0, NULL, &p,
		    optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->pref != DH6OPT_PREF_UNDEF) {
		u_int8_t p8 = (u_int8_t)optinfo->pref;

		if (copy_option(DH6OPT_PREFERENCE, sizeof(p8), &p8, &p,
		    optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->elapsed_time != DH6OPT_ELAPSED_TIME_UNDEF) {
		u_int16_t p16 = (u_int16_t)optinfo->elapsed_time;

		p16 = htons(p16);
		if (copy_option(DH6OPT_ELAPSED_TIME, sizeof(p16), &p16, &p,
		    optep, &len) != 0) {
			goto fail;
		}
	}

	for (stcode = TAILQ_FIRST(&optinfo->stcode_list); stcode;
	     stcode = TAILQ_NEXT(stcode, link)) {
		u_int16_t code;

		code = htons(stcode->val_num16);
		if (copy_option(DH6OPT_STATUS_CODE, sizeof(code), &code, &p,
		    optep, &len) != 0) {
			goto fail;
		}
	}

	if (!TAILQ_EMPTY(&optinfo->reqopt_list)) {
		struct dhcp6_listval *opt;
		u_int16_t *valp;
		int buflen;

		tmpbuf = NULL;
		buflen = dhcp6_count_list(&optinfo->reqopt_list) *
			sizeof(u_int16_t);
		if ((tmpbuf = malloc(buflen)) == NULL) {
			dprintf(LOG_ERR, FNAME,
			    "memory allocation failed for options");
			goto fail;
		}
		optlen = 0;
		valp = (u_int16_t *)tmpbuf;
		for (opt = TAILQ_FIRST(&optinfo->reqopt_list); opt;
		     opt = TAILQ_NEXT(opt, link)) {
			/*
			 * Information request option can only be specified
			 * in information-request messages.
			 * [draft-ietf-dhc-lifetime-02.txt, Section 3.2]
			 */
			if (opt->val_num == DH6OPT_REFRESHTIME &&
			    type != DH6_INFORM_REQ) {
				dprintf(LOG_DEBUG, FNAME,
				    "refresh time option is not requested "
				    "for %s", dhcp6msgstr(type));
			}

			*valp = htons((u_int16_t)opt->val_num);
			valp++;
			optlen += sizeof(u_int16_t);
		}
		if (optlen > 0 &&
		    copy_option(DH6OPT_ORO, optlen, tmpbuf, &p,
		    optep, &len) != 0) {
			goto fail;
		}
		free(tmpbuf);
		tmpbuf = NULL;
	}

	if (dhcp6_set_domain(DH6OPT_SIP_SERVER_D, &optinfo->sipname_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_SIP_SERVER_A, &optinfo->sip_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_DNS, &optinfo->dns_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_domain(DH6OPT_DNSNAME, &optinfo->dnsname_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_NIS_SERVERS, &optinfo->nis_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_NISP_SERVERS, &optinfo->nisp_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_domain(DH6OPT_NIS_DOMAIN_NAME, &optinfo->nisname_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_domain(DH6OPT_NISP_DOMAIN_NAME, &optinfo->nispname_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_NTP, &optinfo->ntp_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_domain(DH6OPT_BCMCS_SERVER_D, &optinfo->bcmcsname_list,
	    &p, optep, &len) != 0)
		goto fail;

	if (dhcp6_set_addr(DH6OPT_BCMCS_SERVER_A, &optinfo->bcmcs_list,
	    &p, optep, &len) != 0)
		goto fail;

	for (op = TAILQ_FIRST(&optinfo->iapd_list); op;
	    op = TAILQ_NEXT(op, link)) {
		int optlen;

		tmpbuf = NULL;
		if ((optlen = copyout_option(NULL, NULL, op)) < 0) {
			dprintf(LOG_INFO, FNAME,
			    "failed to count option length");
			goto fail;
		}
		if ((void *)optep - (void *)p < optlen) {
			dprintf(LOG_INFO, FNAME, "short buffer");
			goto fail;
		}
		if ((tmpbuf = malloc(optlen)) == NULL) {
			dprintf(LOG_NOTICE, FNAME,
			    "memory allocation failed for IA_PD options");
			goto fail;
		}
		if (copyout_option(tmpbuf, tmpbuf + optlen, op) < 0) {
			dprintf(LOG_ERR, FNAME,
			    "failed to construct an IA_PD option");
			goto fail;
		}
		memcpy(p, tmpbuf, optlen);
		free(tmpbuf);
		tmpbuf = NULL;
		p = (struct dhcp6opt *)((char *)p + optlen);
		len += optlen;
	}

	if (optinfo->relaymsg_len) {
		if (copy_option(DH6OPT_RELAY_MSG, optinfo->relaymsg_len,
		    optinfo->relaymsg_msg, &p, optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->ifidopt_id) {
		if (copy_option(DH6OPT_INTERFACE_ID, optinfo->ifidopt_len,
		    optinfo->ifidopt_id, &p, optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->refreshtime != DH6OPT_REFRESHTIME_UNDEF) {
		u_int32_t p32 = (u_int32_t)optinfo->refreshtime;

		p32 = htonl(p32);
		if (copy_option(DH6OPT_REFRESHTIME, sizeof(p32), &p32, &p,
		    optep, &len) != 0) {
			goto fail;
		}
	}

	if (optinfo->authproto != DHCP6_AUTHPROTO_UNDEF) {
		struct dhcp6opt_auth *auth;
		int authlen;
		char *authinfo;

		authlen = sizeof(*auth);
		if (!(optinfo->authflags & DHCP6OPT_AUTHFLAG_NOINFO)) {
			switch (optinfo->authproto) {
			case DHCP6_AUTHPROTO_DELAYED:
				/* Realm + key ID + HMAC-MD5 */
				authlen += optinfo->delayedauth_realmlen +
				    sizeof(optinfo->delayedauth_keyid) + 16;
				break;
#ifdef notyet
			case DHCP6_AUTHPROTO_RECONFIG:
				/* type + key-or-HAMC */
				authlen += 17;
				break;
#endif
			default:
				dprintf(LOG_ERR, FNAME,
				    "unexpected authentication protocol");
				goto fail;
			}
		}
		if ((auth = malloc(authlen)) == NULL) {
			dprintf(LOG_WARNING, FNAME, "failed to allocate "
			    "memory for authentication information");
			goto fail;
		}

		memset(auth, 0, authlen);
		/* copy_option will take care of type and len later */
		auth->dh6_auth_proto = (u_int8_t)optinfo->authproto;
		auth->dh6_auth_alg = (u_int8_t)optinfo->authalgorithm;
		auth->dh6_auth_rdm = (u_int8_t)optinfo->authrdm;
		memcpy(auth->dh6_auth_rdinfo, &optinfo->authrd,
		    sizeof(auth->dh6_auth_rdinfo));

		if (!(optinfo->authflags & DHCP6OPT_AUTHFLAG_NOINFO)) {
			u_int32_t p32;

			switch (optinfo->authproto) {
			case DHCP6_AUTHPROTO_DELAYED:
				authinfo = (char *)(auth + 1);

				/* copy realm */
				memcpy(authinfo, optinfo->delayedauth_realmval,
				    optinfo->delayedauth_realmlen);
				authinfo += optinfo->delayedauth_realmlen;

				/* copy key ID (need memcpy for alignment) */
				p32 = htonl(optinfo->delayedauth_keyid);
				memcpy(authinfo, &p32, sizeof(p32));

				/*
				 * Set the offset so that the caller can
				 * calculate the HMAC.
				 */
				optinfo->delayedauth_offset =
				    ((char *)p - (char *)optbp) + authlen - 16;

				dprintf(LOG_DEBUG, FNAME,
				    "key ID %x, offset %d",
				    optinfo->delayedauth_keyid,
				    optinfo->delayedauth_offset); 
				break;
#ifdef notyet
			case DHCP6_AUTHPROTO_RECONFIG:
#endif
			default:
				dprintf(LOG_ERR, FNAME,
				    "unexpected authentication protocol");
				free(auth);
				goto fail;
			}
		}

		if (copy_option(DH6OPT_AUTH, authlen - 4,
		    &auth->dh6_auth_proto, &p, optep, &len) != 0) {
			goto fail;
		}
		free(auth);
	}

	return (len);

  fail:
	if (tmpbuf)
		free(tmpbuf);
	return (-1);
}
#undef COPY_OPTION

static ssize_t
dnsencode(name, buf, buflen)
	const char *name;
	char *buf;
	size_t buflen;
{
	char *cp, *ep;
	const char *p, *q;
	int i;
	int namelen = strlen(name);

	cp = buf;
	ep = cp + buflen;

	/* if not certain about my name, return an empty buffer */
	if (namelen == 0)
		return (0);

	p = name;
	while (cp < ep && p < name + namelen) {
		i = 0;
		for (q = p; q < name + namelen && *q && *q != '.'; q++)
			i++;
		/* result does not fit into buf */
		if (cp + i + 1 >= ep)
			goto fail;
		/*
		 * DNS label length restriction, RFC1035 page 8.
		 * "i == 0" case is included here to avoid returning
		 * 0-length label on "foo..bar".
		 */
		if (i <= 0 || i >= 64)
			goto fail;
		*cp++ = i;
		if (!isalpha(p[0]) || !isalnum(p[i - 1]))
			goto fail;
		while (i > 0) {
			if (!isalnum(*p) && *p != '-')
				goto fail;
			if (isupper(*p))
				*cp++ = tolower(*p++);
			else
				*cp++ = *p++;
			i--;
		}
		p = q;
		if (p < name + namelen && *p == '.')
			p++;
	}
	/* termination */
	if (cp + 1 >= ep)
		goto fail;
	*cp++ = '\0';
	return (cp - buf);

 fail:
	return (-1);
}

/*
 * Construct a DHCPv6 option along with sub-options in the wire format.
 * If the packet buffer is NULL, just calculate the length of the option
 * (and sub-options) so that the caller can allocate a buffer to store the
 * option(s).
 * This function basically assumes that the caller prepares enough buffer to
 * store all the options.  However, it also takes the buffer end and checks
 * the possibility of overrun for safety.
 */
static int
copyout_option(p, ep, optval)
	char *p, *ep;
	struct dhcp6_listval *optval;
{
	struct dhcp6opt *opt;
	struct dhcp6opt_stcode stcodeopt;
	struct dhcp6opt_ia ia;
	struct dhcp6opt_ia_pd_prefix pd_prefix;
	struct dhcp6opt_ia_addr ia_addr;
	char *subp;
	struct dhcp6_listval *subov;
	int optlen, headlen, sublen, opttype;

	/* check invariant for safety */
	if (p && ep <= p)
		return (-1);

	/* first, detect the length of the option head */
	switch(optval->type) {
	case DHCP6_LISTVAL_IAPD:
		memset(&ia, 0, sizeof(ia));
		headlen = sizeof(ia);
		opttype = DH6OPT_IA_PD;
		opt = (struct dhcp6opt *)(void *)&ia;
		break;
	case DHCP6_LISTVAL_IANA:
		memset(&ia, 0, sizeof(ia));
		headlen = sizeof(ia);
		opttype = DH6OPT_IA_NA;
		opt = (struct dhcp6opt *)(void *)&ia;
		break;
	case DHCP6_LISTVAL_ADDR6:
		memset(&pd_prefix, 0, sizeof(pd_prefix));
		headlen = sizeof(pd_prefix);
		opttype = DH6OPT_IA_PD_PREFIX;
		opt = (struct dhcp6opt *)&pd_prefix;
		break;
	case DHCP6_LISTVAL_PREFIX6:
		memset(&pd_prefix, 0, sizeof(pd_prefix));
		headlen = sizeof(pd_prefix);
		opttype = DH6OPT_IA_PD_PREFIX;
		opt = (struct dhcp6opt *)&pd_prefix;
		break;
	case DHCP6_LISTVAL_STATEFULADDR6:
		memset(&ia_addr, 0, sizeof(ia_addr));
		headlen = sizeof(ia_addr);
		opttype = DH6OPT_IAADDR;
		opt = (struct dhcp6opt *)&ia_addr;
		break;
	case DHCP6_LISTVAL_STCODE:
		memset(&stcodeopt, 0, sizeof(stcodeopt));
		headlen = sizeof(stcodeopt);
		opttype = DH6OPT_STATUS_CODE;
		opt = (struct dhcp6opt *)(void *)&stcodeopt;
		break;
	default:
		/*
		 * we encounter an unknown option.  this should be an internal
		 * error.
		 */
		dprintf(LOG_ERR, FNAME, "unknown option: code %d",
		    optval->type);
		return (-1);
	}

	/* then, calculate the length of and/or fill in the sub-options */
	subp = NULL;
	sublen = 0;
	if (p)
		subp = p + headlen;
	for (subov = TAILQ_FIRST(&optval->sublist); subov;
	    subov = TAILQ_NEXT(subov, link)) {
		int s;

		if ((s = copyout_option(subp, ep, subov)) < 0)
			return (-1);
		if (p)
			subp += s;
		sublen += s;
	}

	/* finally, deal with the head part again */
	optlen = headlen + sublen;
	if (!p)
		return(optlen);

	dprintf(LOG_DEBUG, FNAME, "set %s", dhcp6optstr(opttype));
	if (ep - p < headlen) /* check it just in case */
		return (-1);

	/* fill in the common part */
	opt->dh6opt_type = htons(opttype);
	opt->dh6opt_len = htons(optlen - sizeof(struct dhcp6opt));

	/* fill in type specific fields */
	switch(optval->type) {
	case DHCP6_LISTVAL_IAPD:
		ia.dh6_ia_iaid = htonl(optval->val_ia.iaid);
		ia.dh6_ia_t1 = htonl(optval->val_ia.t1);
		ia.dh6_ia_t2 = htonl(optval->val_ia.t2);
		break;
	case DHCP6_LISTVAL_IANA:
		ia.dh6_ia_iaid = htonl(optval->val_ia.iaid);
		ia.dh6_ia_t1 = htonl(optval->val_ia.t1);
		ia.dh6_ia_t2 = htonl(optval->val_ia.t2);
		break;
	case DHCP6_LISTVAL_PREFIX6:
		pd_prefix.dh6_iapd_prefix_preferred_time =
		    htonl(optval->val_prefix6.pltime);
		pd_prefix.dh6_iapd_prefix_valid_time =
		    htonl(optval->val_prefix6.vltime);
		pd_prefix.dh6_iapd_prefix_prefix_len =
		    optval->val_prefix6.plen;
		/* XXX: prefix_addr is badly aligned, so we need memcpy */
		memcpy(&pd_prefix.dh6_iapd_prefix_prefix_addr,
		    &optval->val_prefix6.addr, sizeof(struct in6_addr));
		break;
	case DHCP6_LISTVAL_STATEFULADDR6:
		ia_addr.dh6_ia_addr_preferred_time =
		    htonl(optval->val_statefuladdr6.pltime);
		ia_addr.dh6_ia_addr_valid_time =
		    htonl(optval->val_statefuladdr6.vltime);
		ia_addr.dh6_ia_addr_addr = optval->val_statefuladdr6.addr;
		break;
	case DHCP6_LISTVAL_STCODE:
		stcodeopt.dh6_stcode_code = htons(optval->val_num16);
		break;
	default:
		/*
		 * XXX: this case should be rejected at the beginning of this
		 * function.
		 */
		return (-1);
	}

	/* copyout the data (p must be non NULL at this point) */
	memcpy(p, opt, headlen);
	return (optlen);
}

void
dhcp6_set_timeoparam(ev)
	struct dhcp6_event *ev;
{
	ev->retrans = 0;
	ev->init_retrans = 0;
	ev->max_retrans_cnt = 0;
	ev->max_retrans_dur = 0;
	ev->max_retrans_time = 0;

	switch(ev->state) {
	case DHCP6S_SOLICIT:
		ev->init_retrans = SOL_TIMEOUT;
		ev->max_retrans_time = SOL_MAX_RT;
		break;
	case DHCP6S_INFOREQ:
		ev->init_retrans = INF_TIMEOUT;
		ev->max_retrans_time = INF_MAX_RT;
		break;
	case DHCP6S_REQUEST:
		ev->init_retrans = REQ_TIMEOUT;
		ev->max_retrans_time = REQ_MAX_RT;
		ev->max_retrans_cnt = REQ_MAX_RC;
		break;
	case DHCP6S_RENEW:
		ev->init_retrans = REN_TIMEOUT;
		ev->max_retrans_time = REN_MAX_RT;
		break;
	case DHCP6S_REBIND:
		ev->init_retrans = REB_TIMEOUT;
		ev->max_retrans_time = REB_MAX_RT;
		break;
	case DHCP6S_RELEASE:
		ev->init_retrans = REL_TIMEOUT;
		ev->max_retrans_cnt = REL_MAX_RC;
		break;
	default:
		dprintf(LOG_ERR, FNAME, "unexpected event state %d on %s",
		    ev->state, ev->ifp->ifname);
		exit(1);
	}
}

void
dhcp6_reset_timer(ev)
	struct dhcp6_event *ev;
{
	double n, r;
	char *statestr;
	struct timeval interval;

	switch(ev->state) {
	case DHCP6S_INIT:
		/*
		 * The first Solicit message from the client on the interface
		 * MUST be delayed by a random amount of time between
		 * 0 and SOL_MAX_DELAY.
		 * [RFC3315 17.1.2]
		 * XXX: a random delay is also necessary before the first
		 * information-request message.  Fortunately, the parameters
		 * and the algorithm for these two cases are the same.
		 * [RFC3315 18.1.5]
		 */
		ev->retrans = (random() % (SOL_MAX_DELAY));
		break;
	default:
		if (ev->state == DHCP6S_SOLICIT && ev->timeouts == 0) {
			/*
			 * The first RT MUST be selected to be strictly
			 * greater than IRT by choosing RAND to be strictly
			 * greater than 0.
			 * [RFC3315 17.1.2]
			 */
			r = (double)((random() % 1000) + 1) / 10000;
			n = ev->init_retrans + r * ev->init_retrans;
		} else {
			r = (double)((random() % 2000) - 1000) / 10000;

			if (ev->timeouts == 0) {
				n = ev->init_retrans + r * ev->init_retrans;
			} else
				n = 2 * ev->retrans + r * ev->retrans;
		}
		if (ev->max_retrans_time && n > ev->max_retrans_time)
			n = ev->max_retrans_time + r * ev->max_retrans_time;
		ev->retrans = (long)n;
		break;
	}

	interval.tv_sec = (ev->retrans * 1000) / 1000000;
	interval.tv_usec = (ev->retrans * 1000) % 1000000;
	dhcp6_set_timer(&interval, ev->timer);

	statestr = dhcp6_event_statestr(ev);

	dprintf(LOG_DEBUG, FNAME, "reset a timer on %s, "
		"state=%s, timeo=%d, retrans=%d",
		ev->ifp->ifname, statestr, ev->timeouts, ev->retrans);
}

int
duidcpy(dd, ds)
	struct duid *dd, *ds;
{
	dd->duid_len = ds->duid_len;
	if ((dd->duid_id = malloc(dd->duid_len)) == NULL) {
		dprintf(LOG_ERR, FNAME, "memory allocation failed");
		return (-1);
	}
	memcpy(dd->duid_id, ds->duid_id, dd->duid_len);

	return (0);
}

int
duidcmp(d1, d2)
	struct duid *d1, *d2;
{
	if (d1->duid_len == d2->duid_len) {
		return (memcmp(d1->duid_id, d2->duid_id, d1->duid_len));
	} else
		return (-1);
}

void
duidfree(duid)
	struct duid *duid;
{
	if (duid->duid_id)
		free(duid->duid_id);
	duid->duid_id = NULL;
	duid->duid_len = 0;
}

/*
 * Provide an NTP-format timestamp as a replay detection counter
 * as mentioned in RFC3315.
 */
#define JAN_1970        2208988800UL        /* 1970 - 1900 in seconds */
int
get_rdvalue(rdm, rdvalue, rdsize)
	int rdm;
	void *rdvalue;
	size_t rdsize;
{
#if defined(HAVE_CLOCK_GETTIME)
	struct timespec tp;
	double nsec;
#else
	struct timeval tv;
#endif
	u_int32_t u32, l32;

	if (rdm != DHCP6_AUTHRDM_MONOCOUNTER) {
		dprintf(LOG_INFO, FNAME, "unsupported RDM (%d)", rdm);
		return (-1);
	}
	if (rdsize != sizeof(u_int64_t)) {
		dprintf(LOG_INFO, FNAME, "unsupported RD size (%d)", rdsize);
		return (-1);
	}

#if defined(HAVE_CLOCK_GETTIME)
	if (clock_gettime(CLOCK_REALTIME, &tp)) {
		dprintf(LOG_WARNING, FNAME, "clock_gettime failed: %s",
		    strerror(errno));
		return (-1);
	}

	u32 = (u_int32_t)tp.tv_sec;
	u32 += JAN_1970;

	nsec = (double)tp.tv_nsec / 1e9 * 0x100000000ULL;
	/* nsec should be smaller than 2^32 */
	l32 = (u_int32_t)nsec;
#else
	if (gettimeofday(&tv, NULL) != 0) {
		dprintf(LOG_WARNING, FNAME, "gettimeofday failed: %s",
		    strerror(errno));
		return (-1);
	}
	u32 = (u_int32_t)tv.tv_sec;
	u32 += JAN_1970;
	l32 = (u_int32_t)tv.tv_usec;
#endif /* HAVE_CLOCK_GETTIME */

	u32 = htonl(u32);
	l32 = htonl(l32);

	memcpy(rdvalue, &u32, sizeof(u32));
	memcpy((char *)rdvalue + sizeof(u32), &l32, sizeof(l32));

	return (0);
}

char *
dhcp6optstr(type)
	int type;
{
	static char genstr[sizeof("opt_65535") + 1]; /* XXX thread unsafe */

	if (type > 65535)
		return ("INVALID option");

	switch(type) {
	case DH6OPT_CLIENTID:
		return ("client ID");
	case DH6OPT_SERVERID:
		return ("server ID");
	case DH6OPT_IA_NA:
		return ("identity association");
	case DH6OPT_IA_TA:
		return ("IA for temporary");
	case DH6OPT_IAADDR:
		return ("IA address");
	case DH6OPT_ORO:
		return ("option request");
	case DH6OPT_PREFERENCE:
		return ("preference");
	case DH6OPT_ELAPSED_TIME:
		return ("elapsed time");
	case DH6OPT_RELAY_MSG:
		return ("relay message");
	case DH6OPT_AUTH:
		return ("authentication");
	case DH6OPT_UNICAST:
		return ("server unicast");
	case DH6OPT_STATUS_CODE:
		return ("status code");
	case DH6OPT_RAPID_COMMIT:
		return ("rapid commit");
	case DH6OPT_USER_CLASS:
		return ("user class");
	case DH6OPT_VENDOR_CLASS:
		return ("vendor class");
	case DH6OPT_VENDOR_OPTS:
		return ("vendor specific info");
	case DH6OPT_INTERFACE_ID:
		return ("interface ID");
	case DH6OPT_RECONF_MSG:
		return ("reconfigure message");
	case DH6OPT_SIP_SERVER_D:
		return ("SIP domain name");
	case DH6OPT_SIP_SERVER_A:
		return ("SIP server address");
	case DH6OPT_DNS:
		return ("DNS");
	case DH6OPT_DNSNAME:
		return ("domain search list");
	case DH6OPT_NTP:
		return ("NTP server");
	case DH6OPT_IA_PD:
		return ("IA_PD");
	case DH6OPT_IA_PD_PREFIX:
		return ("IA_PD prefix");
	case DH6OPT_REFRESHTIME:
		return ("information refresh time");
	case DH6OPT_NIS_SERVERS:
		return ("NIS servers");
	case DH6OPT_NISP_SERVERS:
		return ("NIS+ servers");
	case DH6OPT_NIS_DOMAIN_NAME:
		return ("NIS domain name");
	case DH6OPT_NISP_DOMAIN_NAME:
		return ("NIS+ domain name");
	case DH6OPT_BCMCS_SERVER_D:
		return ("BCMCS domain name");
	case DH6OPT_BCMCS_SERVER_A:
		return ("BCMCS server address");
	case DH6OPT_GEOCONF_CIVIC:
		return ("Geoconf Civic");
	case DH6OPT_REMOTE_ID:
		return ("remote ID");
	case DH6OPT_SUBSCRIBER_ID:
		return ("subscriber ID");
	case DH6OPT_CLIENT_FQDN:
		return ("client FQDN");
	default:
		snprintf(genstr, sizeof(genstr), "opt_%d", type);
		return (genstr);
	}
}

char *
dhcp6msgstr(type)
	int type;
{
	static char genstr[sizeof("msg255") + 1]; /* XXX thread unsafe */

	if (type > 255)
		return ("INVALID msg");

	switch(type) {
	case DH6_SOLICIT:
		return ("solicit");
	case DH6_ADVERTISE:
		return ("advertise");
	case DH6_REQUEST:
		return ("request");
	case DH6_CONFIRM:
		return ("confirm");
	case DH6_RENEW:
		return ("renew");
	case DH6_REBIND:
		return ("rebind");
	case DH6_REPLY:
		return ("reply");
	case DH6_RELEASE:
		return ("release");
	case DH6_DECLINE:
		return ("decline");
	case DH6_RECONFIGURE:
		return ("reconfigure");
	case DH6_INFORM_REQ:
		return ("information request");
	case DH6_RELAY_FORW:
		return ("relay-forward");
	case DH6_RELAY_REPLY:
		return ("relay-reply");
	default:
		snprintf(genstr, sizeof(genstr), "msg%d", type);
		return (genstr);
	}
}

char *
dhcp6_stcodestr(code)
	u_int16_t code;
{
	static char genstr[sizeof("code255") + 1]; /* XXX thread unsafe */

	if (code > 255)
		return ("INVALID code");

	switch(code) {
	case DH6OPT_STCODE_SUCCESS:
		return ("success");
	case DH6OPT_STCODE_UNSPECFAIL:
		return ("unspec failure");
	case DH6OPT_STCODE_NOADDRSAVAIL:
		return ("no addresses");
	case DH6OPT_STCODE_NOBINDING:
		return ("no binding");
	case DH6OPT_STCODE_NOTONLINK:
		return ("not on-link");
	case DH6OPT_STCODE_USEMULTICAST:
		return ("use multicast");
	case DH6OPT_STCODE_NOPREFIXAVAIL:
		return ("no prefixes");
	default:
		snprintf(genstr, sizeof(genstr), "code%d", code);
		return (genstr);
	}
}

char *
duidstr(duid)
	struct duid *duid;
{
	int i, n;
	char *cp, *ep;
	static char duidstr[sizeof("xx:") * 128 + sizeof("...")];

	cp = duidstr;
	ep = duidstr + sizeof(duidstr);
	for (i = 0; i < duid->duid_len && i <= 128; i++) {
		n = snprintf(cp, ep - cp, "%s%02x", i == 0 ? "" : ":",
		    duid->duid_id[i] & 0xff);
		if (n < 0)
			return NULL;
		cp += n;
	}
	if (i < duid->duid_len)
		snprintf(cp, ep - cp, "%s", "...");

	return (duidstr);
}

char *dhcp6_event_statestr(ev)
	struct dhcp6_event *ev;
{
	return dhcp6_statestr(ev->state);
}

char *dhcp6_statestr(state)
	int state;
{
	switch(state) {
	case DHCP6S_INIT:
		return ("INIT");
	case DHCP6S_SOLICIT:
		return ("SOLICIT");
	case DHCP6S_INFOREQ:
		return ("INFOREQ");
	case DHCP6S_REQUEST:
		return ("REQUEST");
	case DHCP6S_RENEW:
		return ("RENEW");
	case DHCP6S_REBIND:
		return ("REBIND");
	case DHCP6S_RELEASE:
		return ("RELEASE");
	case DHCP6S_IDLE:
		return ("IDLE");
	default:
		return ("???"); /* XXX */
	}
}

void
setloglevel(debuglevel)
	int debuglevel;
{
	if (foreground) {
		switch(debuglevel) {
		case 0:
			debug_thresh = LOG_ERR;
			break;
		case 1:
			debug_thresh = LOG_INFO;
			break;
		default:
			debug_thresh = LOG_DEBUG;
			break;
		}
	} else {
		switch(debuglevel) {
		case 0:
			setlogmask(LOG_UPTO(LOG_ERR));
			break;
		case 1:
			setlogmask(LOG_UPTO(LOG_INFO));
			break;
		}
	}
}

void
my_dprintf(int level, const char *fname, const char *fmt, ...)
{
	va_list ap;
	char logbuf[LINE_MAX];
	int printfname = 1;

	va_start(ap, fmt);
	vsnprintf(logbuf, sizeof(logbuf), fmt, ap);

	if (*fname == '\0')
		printfname = 0;

	if (foreground && debug_thresh >= level) {
		time_t now;
		struct tm *tm_now;
		const char *month[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
		};

		if ((now = time(NULL)) < 0)
			exit(1); /* XXX */
		tm_now = localtime(&now);
		fprintf(stderr, "%3s/%02d/%04d %02d:%02d:%02d: %s%s%s\n",
		    month[tm_now->tm_mon], tm_now->tm_mday,
		    tm_now->tm_year + 1900,
		    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
		    fname, printfname ? ": " : "",
		    logbuf);
	} else
#if 0
		syslog(level, "%s%s%s", fname, printfname ? ": " : "", logbuf);
#else
		syslog(level > LOG_NOTICE ? LOG_NOTICE : level, "%s%s%s", fname, printfname ? ": " : "", logbuf);
#endif
}

int
ifaddrconf(cmd, ifname, addr, plen, pltime, vltime)
	ifaddrconf_cmd_t cmd;
	char *ifname;
	struct sockaddr_in6 *addr;
	int plen;
	int pltime;
	int vltime;
{
#ifdef __KAME__
	struct in6_aliasreq req;
#endif
#ifdef __linux__
	struct in6_ifreq req;
	struct ifreq ifr;
#endif
#ifdef __sun__
	struct lifreq req;
#endif
	unsigned long ioctl_cmd;
	char *cmdstr;
	int s;			/* XXX overhead */

	switch(cmd) {
	case IFADDRCONF_ADD:
		cmdstr = "add";
#ifdef __KAME__
		ioctl_cmd = SIOCAIFADDR_IN6;
#endif
#ifdef __linux__
		ioctl_cmd = SIOCSIFADDR;
#endif
#ifdef __sun__
		ioctl_cmd = SIOCLIFADDIF;
#endif
		break;
	case IFADDRCONF_REMOVE:
		cmdstr = "remove";
#ifdef __KAME__
		ioctl_cmd = SIOCDIFADDR_IN6;
#endif
#ifdef __linux__
		ioctl_cmd = SIOCDIFADDR;
#endif
#ifdef __sun__
		ioctl_cmd = SIOCLIFREMOVEIF;
#endif
		break;
	default:
		return (-1);
	}

	if ((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		dprintf(LOG_ERR, FNAME, "can't open a temporary socket: %s",
		    strerror(errno));
		return (-1);
	}

	memset(&req, 0, sizeof(req));
#ifdef __KAME__
	req.ifra_addr = *addr;
	memcpy(req.ifra_name, ifname, sizeof(req.ifra_name));
	(void)sa6_plen2mask(&req.ifra_prefixmask, plen);
	/* XXX: should lifetimes be calculated based on the lease duration? */
	req.ifra_lifetime.ia6t_vltime = vltime;
	req.ifra_lifetime.ia6t_pltime = pltime;
#endif
#ifdef __linux__
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(s, SIOGIFINDEX, &ifr) < 0) {
		dprintf(LOG_NOTICE, FNAME, "failed to get the index of %s: %s",
		    ifname, strerror(errno));
		close(s); 
		return (-1); 
	}
	memcpy(&req.ifr6_addr, &addr->sin6_addr, sizeof(struct in6_addr));
	req.ifr6_prefixlen = plen;
	req.ifr6_ifindex = ifr.ifr_ifindex;
#endif
#ifdef __sun__
	strncpy(req.lifr_name, ifname, sizeof (req.lifr_name));
#endif

	if (ioctl(s, ioctl_cmd, &req)) {
		dprintf(LOG_NOTICE, FNAME, "failed to %s an address on %s: %s",
		    cmdstr, ifname, strerror(errno));
		close(s);
		return (-1);
	}

#ifdef __sun__
	memcpy(&req.lifr_addr, addr, sizeof (*addr));
	if (ioctl(s, SIOCSLIFADDR, &req) == -1) {
		dprintf(LOG_NOTICE, FNAME, "failed to %s new address on %s: %s",
		    cmdstr, ifname, strerror(errno));
		close(s);
		return (-1);
	}
#endif

	dprintf(LOG_DEBUG, FNAME, "%s an address %s/%d on %s", cmdstr,
	    addr2str((struct sockaddr *)addr), plen, ifname);

	close(s);
	return (0);
}

int
safefile(path)
	const char *path;
{
	struct stat s;
	uid_t myuid;

	/* no setuid */
	if (getuid() != geteuid()) {
		dprintf(LOG_NOTICE, FNAME,
		    "setuid'ed execution not allowed");
		return (-1);
	}

	if (lstat(path, &s) != 0) {
		dprintf(LOG_NOTICE, FNAME, "lstat failed: %s",
		    strerror(errno));
		return (-1);
	}

	/* the file must be owned by the running uid */
	myuid = getuid();
	if (s.st_uid != myuid) {
		dprintf(LOG_NOTICE, FNAME, "%s has invalid owner uid", path);
		return (-1);
	}

	switch (s.st_mode & S_IFMT) {
	case S_IFREG:
	case S_IFLNK:
		break;
	default:
		dprintf(LOG_NOTICE, FNAME, "%s is an invalid file type 0x%o",
		    path, (s.st_mode & S_IFMT));
		return (-1);
	}

	return (0);
}

int
dumpfile(path)
	const char *path;
{
	FILE *f = NULL;
	char buf[256];

	if (path == NULL)
		return -1;

	if ((f = fopen(path, "r")) == NULL) {
		dprintf(LOG_ERR, FNAME, "%s: %s: %s", strerror(errno), path, __FUNCTION__);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f))
	{
		dprintf(LOG_ERR, FNAME, "%s", buf);
	}

	fclose(f);

	return 0;
}
