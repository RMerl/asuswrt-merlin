/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929: resolve addresses
 */

#include "libbb.h"
#include "utils.h"
#include "inet_common.h"

unsigned get_unsigned(char *arg, const char *errmsg)
{
	unsigned long res;
	char *ptr;

	if (*arg) {
		res = strtoul(arg, &ptr, 0);
//FIXME: "" will be accepted too, is it correct?!
		if (!*ptr && res <= UINT_MAX) {
			return res;
		}
	}
	invarg(arg, errmsg); /* does not return */
}

uint32_t get_u32(char *arg, const char *errmsg)
{
	unsigned long res;
	char *ptr;

	if (*arg) {
		res = strtoul(arg, &ptr, 0);
//FIXME: "" will be accepted too, is it correct?!
		if (!*ptr && res <= 0xFFFFFFFFUL) {
			return res;
		}
	}
	invarg(arg, errmsg); /* does not return */
}

uint16_t get_u16(char *arg, const char *errmsg)
{
	unsigned long res;
	char *ptr;

	if (*arg) {
		res = strtoul(arg, &ptr, 0);
//FIXME: "" will be accepted too, is it correct?!
		if (!*ptr && res <= 0xFFFF) {
			return res;
		}
	}
	invarg(arg, errmsg); /* does not return */
}

int get_addr_1(inet_prefix *addr, char *name, int family)
{
	memset(addr, 0, sizeof(*addr));

	if (strcmp(name, "default") == 0
	 || strcmp(name, "all") == 0
	 || strcmp(name, "any") == 0
	) {
		addr->family = family;
		addr->bytelen = (family == AF_INET6 ? 16 : 4);
		addr->bitlen = -1;
		return 0;
	}

	if (strchr(name, ':')) {
		addr->family = AF_INET6;
		if (family != AF_UNSPEC && family != AF_INET6)
			return -1;
		if (inet_pton(AF_INET6, name, addr->data) <= 0)
			return -1;
		addr->bytelen = 16;
		addr->bitlen = -1;
		return 0;
	}

	if (family != AF_UNSPEC && family != AF_INET)
		return -1;

	/* Try to parse it as IPv4 */
	addr->family = AF_INET;
#if 0 /* Doesn't handle e.g. "10.10", for example, "ip r l root 10.10/16" */
	if (inet_pton(AF_INET, name, addr->data) <= 0)
		return -1;
#else
	{
		unsigned i = 0;
		unsigned n = 0;
		const char *cp = name - 1;
		while (*++cp) {
			if ((unsigned char)(*cp - '0') <= 9) {
				n = 10 * n + (unsigned char)(*cp - '0');
				if (n >= 256)
					return -1;
				((uint8_t*)addr->data)[i] = n;
				continue;
			}
			if (*cp == '.' && ++i <= 3) {
				n = 0;
				continue;
			}
			return -1;
		}
	}
#endif
	addr->bytelen = 4;
	addr->bitlen = -1;

	return 0;
}

static void get_prefix_1(inet_prefix *dst, char *arg, int family)
{
	char *slash;

	memset(dst, 0, sizeof(*dst));

	if (strcmp(arg, "default") == 0
	 || strcmp(arg, "all") == 0
	 || strcmp(arg, "any") == 0
	) {
		dst->family = family;
		/*dst->bytelen = 0; - done by memset */
		/*dst->bitlen = 0;*/
		return;
	}

	slash = strchr(arg, '/');
	if (slash)
		*slash = '\0';

	if (get_addr_1(dst, arg, family) == 0) {
		dst->bitlen = (dst->family == AF_INET6) ? 128 : 32;
		if (slash) {
			unsigned plen;
			inet_prefix netmask_pfx;

			netmask_pfx.family = AF_UNSPEC;
			plen = bb_strtou(slash + 1, NULL, 0);
			if ((errno || plen > dst->bitlen)
			 && get_addr_1(&netmask_pfx, slash + 1, family) != 0
			) {
				goto bad;
			}
			if (netmask_pfx.family == AF_INET) {
				/* fill in prefix length of dotted quad */
				uint32_t mask = ntohl(netmask_pfx.data[0]);
				uint32_t host = ~mask;

				/* a valid netmask must be 2^n - 1 */
				if (host & (host + 1))
					goto bad;

				for (plen = 0; mask; mask <<= 1)
					++plen;
				if (plen > dst->bitlen)
					goto bad;
				/* dst->flags |= PREFIXLEN_SPECIFIED; */
			}
			dst->bitlen = plen;
		}
	}

	if (slash)
		*slash = '/';
	return;
 bad:
	bb_error_msg_and_die("an %s %s is expected rather than \"%s\"", "inet", "prefix", arg);
}

int get_addr(inet_prefix *dst, char *arg, int family)
{
	if (family == AF_PACKET) {
		bb_error_msg_and_die("\"%s\" may be inet %s, but it is not allowed in this context", arg, "address");
	}
	if (get_addr_1(dst, arg, family)) {
		bb_error_msg_and_die("an %s %s is expected rather than \"%s\"", "inet", "address", arg);
	}
	return 0;
}

void get_prefix(inet_prefix *dst, char *arg, int family)
{
	if (family == AF_PACKET) {
		bb_error_msg_and_die("\"%s\" may be inet %s, but it is not allowed in this context", arg, "prefix");
	}
	get_prefix_1(dst, arg, family);
}

uint32_t get_addr32(char *name)
{
	inet_prefix addr;

	if (get_addr_1(&addr, name, AF_INET)) {
		bb_error_msg_and_die("an %s %s is expected rather than \"%s\"", "IP", "address", name);
	}
	return addr.data[0];
}

void incomplete_command(void)
{
	bb_error_msg_and_die("command line is not complete, try option \"help\"");
}

void invarg(const char *arg, const char *opt)
{
	bb_error_msg_and_die(bb_msg_invalid_arg, arg, opt);
}

void duparg(const char *key, const char *arg)
{
	bb_error_msg_and_die("duplicate \"%s\": \"%s\" is the second value", key, arg);
}

void duparg2(const char *key, const char *arg)
{
	bb_error_msg_and_die("either \"%s\" is duplicate, or \"%s\" is garbage", key, arg);
}

int inet_addr_match(const inet_prefix *a, const inet_prefix *b, int bits)
{
	const uint32_t *a1 = a->data;
	const uint32_t *a2 = b->data;
	int words = bits >> 5;

	bits &= 0x1f;

	if (words)
		if (memcmp(a1, a2, words << 2))
			return -1;

	if (bits) {
		uint32_t w1, w2;
		uint32_t mask;

		w1 = a1[words];
		w2 = a2[words];

		mask = htonl((0xffffffff) << (0x20 - bits));

		if ((w1 ^ w2) & mask)
			return 1;
	}

	return 0;
}

const char *rt_addr_n2a(int af,
		void *addr, char *buf, int buflen)
{
	switch (af) {
	case AF_INET:
	case AF_INET6:
		return inet_ntop(af, addr, buf, buflen);
	default:
		return "???";
	}
}

#ifdef RESOLVE_HOSTNAMES
const char *format_host(int af, int len, void *addr, char *buf, int buflen)
{
	if (resolve_hosts) {
		struct hostent *h_ent;

		if (len <= 0) {
			switch (af) {
			case AF_INET:
				len = 4;
				break;
			case AF_INET6:
				len = 16;
				break;
			default:;
			}
		}
		if (len > 0) {
			h_ent = gethostbyaddr(addr, len, af);
			if (h_ent != NULL) {
				safe_strncpy(buf, h_ent->h_name, buflen);
				return buf;
			}
		}
	}
	return rt_addr_n2a(af, addr, buf, buflen);
}
#endif
