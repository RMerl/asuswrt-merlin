/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file address.c
 * \brief Functions to use and manipulate the tor_addr_t structure.
 **/

#define ADDRESS_PRIVATE

#include "orconfig.h"

#ifdef _WIN32
/* For access to structs needed by GetAdaptersAddresses */
#ifndef WIN32_LEAN_AND_MEAN
#error "orconfig.h didn't define WIN32_LEAN_AND_MEAN"
#endif
#ifndef WINVER
#error "orconfig.h didn't define WINVER"
#endif
#ifndef _WIN32_WINNT
#error "orconfig.h didn't define _WIN32_WINNT"
#endif
#if WINVER < 0x0501
#error "winver too low"
#endif
#if _WIN32_WINNT < 0x0501
#error "winver too low"
#endif
#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <iphlpapi.h>
#endif

#include "compat.h"
#include "util.h"
#include "util_format.h"
#include "address.h"
#include "torlog.h"
#include "container.h"
#include "sandbox.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h> /* FreeBSD needs this to know what version it is */
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* tor_addr_is_null() and maybe other functions rely on AF_UNSPEC being 0 to
 * work correctly. Bail out here if we've found a platform where AF_UNSPEC
 * isn't 0. */
#if AF_UNSPEC != 0
#error We rely on AF_UNSPEC being 0. Let us know about your platform, please!
#endif

/** Convert the tor_addr_t in <b>a</b>, with port in <b>port</b>, into a
 * sockaddr object in *<b>sa_out</b> of object size <b>len</b>.  If not enough
 * room is available in sa_out, or on error, return 0.  On success, return
 * the length of the sockaddr.
 *
 * Interface note: ordinarily, we return -1 for error.  We can't do that here,
 * since socklen_t is unsigned on some platforms.
 **/
socklen_t
tor_addr_to_sockaddr(const tor_addr_t *a,
                     uint16_t port,
                     struct sockaddr *sa_out,
                     socklen_t len)
{
  memset(sa_out, 0, len);

  sa_family_t family = tor_addr_family(a);
  if (family == AF_INET) {
    struct sockaddr_in *sin;
    if (len < (int)sizeof(struct sockaddr_in))
      return 0;
    sin = (struct sockaddr_in *)sa_out;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sin->sin_len = sizeof(struct sockaddr_in);
#endif
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = tor_addr_to_ipv4n(a);
    return sizeof(struct sockaddr_in);
  } else if (family == AF_INET6) {
    struct sockaddr_in6 *sin6;
    if (len < (int)sizeof(struct sockaddr_in6))
      return 0;
    sin6 = (struct sockaddr_in6 *)sa_out;
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
    sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    memcpy(&sin6->sin6_addr, tor_addr_to_in6_assert(a),
           sizeof(struct in6_addr));
    return sizeof(struct sockaddr_in6);
  } else {
    return 0;
  }
}

/** Set address <b>a</b> to zero.  This address belongs to
 * the AF_UNIX family. */
static void
tor_addr_make_af_unix(tor_addr_t *a)
{
  memset(a, 0, sizeof(*a));
  a->family = AF_UNIX;
}

/** Set the tor_addr_t in <b>a</b> to contain the socket address contained in
 * <b>sa</b>.  IF <b>port_out</b> is non-NULL and <b>sa</b> contains a port,
 * set *<b>port_out</b> to that port. Return 0 on success and -1 on
 * failure. */
int
tor_addr_from_sockaddr(tor_addr_t *a, const struct sockaddr *sa,
                       uint16_t *port_out)
{
  tor_assert(a);
  tor_assert(sa);

  memset(a, 0, sizeof(*a));

  if (sa->sa_family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *) sa;
    tor_addr_from_ipv4n(a, sin->sin_addr.s_addr);
    if (port_out)
      *port_out = ntohs(sin->sin_port);
  } else if (sa->sa_family == AF_INET6) {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
    tor_addr_from_in6(a, &sin6->sin6_addr);
    if (port_out)
      *port_out = ntohs(sin6->sin6_port);
  } else if (sa->sa_family == AF_UNIX) {
    tor_addr_make_af_unix(a);
    return 0;
  } else {
    tor_addr_make_unspec(a);
    return -1;
  }
  return 0;
}

/** Return a newly allocated string holding the address described in
 * <b>sa</b>.  AF_UNIX, AF_UNSPEC, AF_INET, and AF_INET6 are supported. */
char *
tor_sockaddr_to_str(const struct sockaddr *sa)
{
  char address[TOR_ADDR_BUF_LEN];
  char *result;
  tor_addr_t addr;
  uint16_t port;
#ifdef HAVE_SYS_UN_H
  if (sa->sa_family == AF_UNIX) {
    struct sockaddr_un *s_un = (struct sockaddr_un *)sa;
    tor_asprintf(&result, "unix:%s", s_un->sun_path);
    return result;
  }
#endif
  if (sa->sa_family == AF_UNSPEC)
    return tor_strdup("unspec");

  if (tor_addr_from_sockaddr(&addr, sa, &port) < 0)
    return NULL;
  if (! tor_addr_to_str(address, &addr, sizeof(address), 1))
    return NULL;
  tor_asprintf(&result, "%s:%d", address, (int)port);
  return result;
}

/** Set address <b>a</b> to the unspecified address.  This address belongs to
 * no family. */
void
tor_addr_make_unspec(tor_addr_t *a)
{
  memset(a, 0, sizeof(*a));
  a->family = AF_UNSPEC;
}

/** Set address <b>a</b> to the null address in address family <b>family</b>.
 * The null address for AF_INET is 0.0.0.0.  The null address for AF_INET6 is
 * [::].  AF_UNSPEC is all null. */
void
tor_addr_make_null(tor_addr_t *a, sa_family_t family)
{
  memset(a, 0, sizeof(*a));
  a->family = family;
}

/** Similar behavior to Unix gethostbyname: resolve <b>name</b>, and set
 * *<b>addr</b> to the proper IP address and family. The <b>family</b>
 * argument (which must be AF_INET, AF_INET6, or AF_UNSPEC) declares a
 * <i>preferred</i> family, though another one may be returned if only one
 * family is implemented for this address.
 *
 * Return 0 on success, -1 on failure; 1 on transient failure.
 */
int
tor_addr_lookup(const char *name, uint16_t family, tor_addr_t *addr)
{
  /* Perhaps eventually this should be replaced by a tor_getaddrinfo or
   * something.
   */
  struct in_addr iaddr;
  struct in6_addr iaddr6;
  tor_assert(name);
  tor_assert(addr);
  tor_assert(family == AF_INET || family == AF_INET6 || family == AF_UNSPEC);
  if (!*name) {
    /* Empty address is an error. */
    return -1;
  } else if (tor_inet_pton(AF_INET, name, &iaddr)) {
    /* It's an IPv4 IP. */
    if (family == AF_INET6)
      return -1;
    tor_addr_from_in(addr, &iaddr);
    return 0;
  } else if (tor_inet_pton(AF_INET6, name, &iaddr6)) {
    if (family == AF_INET)
      return -1;
    tor_addr_from_in6(addr, &iaddr6);
    return 0;
  } else {
#ifdef HAVE_GETADDRINFO
    int err;
    struct addrinfo *res=NULL, *res_p;
    struct addrinfo *best=NULL;
    struct addrinfo hints;
    int result = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    err = sandbox_getaddrinfo(name, NULL, &hints, &res);
    /* The check for 'res' here shouldn't be necessary, but it makes static
     * analysis tools happy. */
    if (!err && res) {
      best = NULL;
      for (res_p = res; res_p; res_p = res_p->ai_next) {
        if (family == AF_UNSPEC) {
          if (res_p->ai_family == AF_INET) {
            best = res_p;
            break;
          } else if (res_p->ai_family == AF_INET6 && !best) {
            best = res_p;
          }
        } else if (family == res_p->ai_family) {
          best = res_p;
          break;
        }
      }
      if (!best)
        best = res;
      if (best->ai_family == AF_INET) {
        tor_addr_from_in(addr,
                         &((struct sockaddr_in*)best->ai_addr)->sin_addr);
        result = 0;
      } else if (best->ai_family == AF_INET6) {
        tor_addr_from_in6(addr,
                          &((struct sockaddr_in6*)best->ai_addr)->sin6_addr);
        result = 0;
      }
      sandbox_freeaddrinfo(res);
      return result;
    }
    return (err == EAI_AGAIN) ? 1 : -1;
#else
    struct hostent *ent;
    int err;
#ifdef HAVE_GETHOSTBYNAME_R_6_ARG
    char buf[2048];
    struct hostent hostent;
    int r;
    r = gethostbyname_r(name, &hostent, buf, sizeof(buf), &ent, &err);
#elif defined(HAVE_GETHOSTBYNAME_R_5_ARG)
    char buf[2048];
    struct hostent hostent;
    ent = gethostbyname_r(name, &hostent, buf, sizeof(buf), &err);
#elif defined(HAVE_GETHOSTBYNAME_R_3_ARG)
    struct hostent_data data;
    struct hostent hent;
    memset(&data, 0, sizeof(data));
    err = gethostbyname_r(name, &hent, &data);
    ent = err ? NULL : &hent;
#else
    ent = gethostbyname(name);
#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = h_errno;
#endif
#endif /* endif HAVE_GETHOSTBYNAME_R_6_ARG. */
    if (ent) {
      if (ent->h_addrtype == AF_INET) {
        tor_addr_from_in(addr, (struct in_addr*) ent->h_addr);
      } else if (ent->h_addrtype == AF_INET6) {
        tor_addr_from_in6(addr, (struct in6_addr*) ent->h_addr);
      } else {
        tor_assert(0); // LCOV_EXCL_LINE: gethostbyname() returned bizarre type
      }
      return 0;
    }
#ifdef _WIN32
    return (err == WSATRY_AGAIN) ? 1 : -1;
#else
    return (err == TRY_AGAIN) ? 1 : -1;
#endif
#endif
  }
}

/** Return true iff <b>ip</b> is an IP reserved to localhost or local networks
 * in RFC1918 or RFC4193 or RFC4291. (fec0::/10, deprecated by RFC3879, is
 * also treated as internal for now.)
 */
int
tor_addr_is_internal_(const tor_addr_t *addr, int for_listening,
                      const char *filename, int lineno)
{
  uint32_t iph4 = 0;
  uint32_t iph6[4];

  tor_assert(addr);
  sa_family_t v_family = tor_addr_family(addr);

  if (v_family == AF_INET) {
    iph4 = tor_addr_to_ipv4h(addr);
  } else if (v_family == AF_INET6) {
    if (tor_addr_is_v4(addr)) { /* v4-mapped */
      uint32_t *addr32 = NULL;
      v_family = AF_INET;
      // Work around an incorrect NULL pointer dereference warning in
      // "clang --analyze" due to limited analysis depth
      addr32 = tor_addr_to_in6_addr32(addr);
      // To improve performance, wrap this assertion in:
      // #if !defined(__clang_analyzer__) || PARANOIA
      tor_assert(addr32);
      iph4 = ntohl(addr32[3]);
    }
  }

  if (v_family == AF_INET6) {
    const uint32_t *a32 = tor_addr_to_in6_addr32(addr);
    iph6[0] = ntohl(a32[0]);
    iph6[1] = ntohl(a32[1]);
    iph6[2] = ntohl(a32[2]);
    iph6[3] = ntohl(a32[3]);
    if (for_listening && !iph6[0] && !iph6[1] && !iph6[2] && !iph6[3]) /* :: */
      return 0;

    if (((iph6[0] & 0xfe000000) == 0xfc000000) || /* fc00/7  - RFC4193 */
        ((iph6[0] & 0xffc00000) == 0xfe800000) || /* fe80/10 - RFC4291 */
        ((iph6[0] & 0xffc00000) == 0xfec00000))   /* fec0/10 D- RFC3879 */
      return 1;

    if (!iph6[0] && !iph6[1] && !iph6[2] &&
        ((iph6[3] & 0xfffffffe) == 0x00000000))  /* ::/127 */
      return 1;

    return 0;
  } else if (v_family == AF_INET) {
    if (for_listening && !iph4) /* special case for binding to 0.0.0.0 */
      return 0;
    if (((iph4 & 0xff000000) == 0x0a000000) || /*       10/8 */
        ((iph4 & 0xff000000) == 0x00000000) || /*        0/8 */
        ((iph4 & 0xff000000) == 0x7f000000) || /*      127/8 */
        ((iph4 & 0xffff0000) == 0xa9fe0000) || /* 169.254/16 */
        ((iph4 & 0xfff00000) == 0xac100000) || /*  172.16/12 */
        ((iph4 & 0xffff0000) == 0xc0a80000))   /* 192.168/16 */
      return 1;
    return 0;
  }

  /* unknown address family... assume it's not safe for external use */
  /* rather than tor_assert(0) */
  log_warn(LD_BUG, "tor_addr_is_internal() called from %s:%d with a "
           "non-IP address of type %d", filename, lineno, (int)v_family);
  tor_fragile_assert();
  return 1;
}

/** Convert a tor_addr_t <b>addr</b> into a string, and store it in
 *  <b>dest</b> of size <b>len</b>.  Returns a pointer to dest on success,
 *  or NULL on failure.  If <b>decorate</b>, surround IPv6 addresses with
 *  brackets.
 */
const char *
tor_addr_to_str(char *dest, const tor_addr_t *addr, size_t len, int decorate)
{
  const char *ptr;
  tor_assert(addr && dest);

  switch (tor_addr_family(addr)) {
    case AF_INET:
      /* Shortest addr x.x.x.x + \0 */
      if (len < 8)
        return NULL;
      ptr = tor_inet_ntop(AF_INET, &addr->addr.in_addr, dest, len);
      break;
    case AF_INET6:
      /* Shortest addr [ :: ] + \0 */
      if (len < (3 + (decorate ? 2 : 0)))
        return NULL;

      if (decorate)
        ptr = tor_inet_ntop(AF_INET6, &addr->addr.in6_addr, dest+1, len-2);
      else
        ptr = tor_inet_ntop(AF_INET6, &addr->addr.in6_addr, dest, len);

      if (ptr && decorate) {
        *dest = '[';
        memcpy(dest+strlen(dest), "]", 2);
        tor_assert(ptr == dest+1);
        ptr = dest;
      }
      break;
    case AF_UNIX:
      tor_snprintf(dest, len, "AF_UNIX");
      ptr = dest;
      break;
    default:
      return NULL;
  }
  return ptr;
}

/** Parse an .in-addr.arpa or .ip6.arpa address from <b>address</b>.  Return 0
 * if this is not an .in-addr.arpa address or an .ip6.arpa address.  Return -1
 * if this is an ill-formed .in-addr.arpa address or an .ip6.arpa address.
 * Also return -1 if <b>family</b> is not AF_UNSPEC, and the parsed address
 * family does not match <b>family</b>.  On success, return 1, and store the
 * result, if any, into <b>result</b>, if provided.
 *
 * If <b>accept_regular</b> is set and the address is in neither recognized
 * reverse lookup hostname format, try parsing the address as a regular
 * IPv4 or IPv6 address too.
 */
int
tor_addr_parse_PTR_name(tor_addr_t *result, const char *address,
                                   int family, int accept_regular)
{
  if (!strcasecmpend(address, ".in-addr.arpa")) {
    /* We have an in-addr.arpa address. */
    char buf[INET_NTOA_BUF_LEN];
    size_t len;
    struct in_addr inaddr;
    if (family == AF_INET6)
      return -1;

    len = strlen(address) - strlen(".in-addr.arpa");
    if (len >= INET_NTOA_BUF_LEN)
      return -1; /* Too long. */

    memcpy(buf, address, len);
    buf[len] = '\0';
    if (tor_inet_aton(buf, &inaddr) == 0)
      return -1; /* malformed. */

    /* reverse the bytes */
    inaddr.s_addr = (uint32_t)
      (((inaddr.s_addr & 0x000000ff) << 24)
       |((inaddr.s_addr & 0x0000ff00) << 8)
       |((inaddr.s_addr & 0x00ff0000) >> 8)
       |((inaddr.s_addr & 0xff000000) >> 24));

    if (result) {
      tor_addr_from_in(result, &inaddr);
    }
    return 1;
  }

  if (!strcasecmpend(address, ".ip6.arpa")) {
    const char *cp;
    int n0, n1;
    struct in6_addr in6;

    if (family == AF_INET)
      return -1;

    cp = address;
    for (int i = 0; i < 16; ++i) {
      n0 = hex_decode_digit(*cp++); /* The low-order nybble appears first. */
      if (*cp++ != '.') return -1;  /* Then a dot. */
      n1 = hex_decode_digit(*cp++); /* The high-order nybble appears first. */
      if (*cp++ != '.') return -1;  /* Then another dot. */
      if (n0<0 || n1 < 0) /* Both nybbles must be hex. */
        return -1;

      /* We don't check the length of the string in here.  But that's okay,
       * since we already know that the string ends with ".ip6.arpa", and
       * there is no way to frameshift .ip6.arpa so it fits into the pattern
       * of hexdigit, period, hexdigit, period that we enforce above.
       */

      /* Assign from low-byte to high-byte. */
      in6.s6_addr[15-i] = n0 | (n1 << 4);
    }
    if (strcasecmp(cp, "ip6.arpa"))
      return -1;

    if (result) {
      tor_addr_from_in6(result, &in6);
    }
    return 1;
  }

  if (accept_regular) {
    tor_addr_t tmp;
    int r = tor_addr_parse(&tmp, address);
    if (r < 0)
      return 0;
    if (r != family && family != AF_UNSPEC)
      return -1;

    if (result)
      memcpy(result, &tmp, sizeof(tor_addr_t));

    return 1;
  }

  return 0;
}

/** Convert <b>addr</b> to an in-addr.arpa name or a .ip6.arpa name,
 * and store the result in the <b>outlen</b>-byte buffer at
 * <b>out</b>.  Return the number of chars written to <b>out</b>, not
 * including the trailing \0, on success. Returns -1 on failure. */
int
tor_addr_to_PTR_name(char *out, size_t outlen,
                     const tor_addr_t *addr)
{
  tor_assert(out);
  tor_assert(addr);

  if (addr->family == AF_INET) {
    uint32_t a = tor_addr_to_ipv4h(addr);

    return tor_snprintf(out, outlen, "%d.%d.%d.%d.in-addr.arpa",
                        (int)(uint8_t)((a    )&0xff),
                        (int)(uint8_t)((a>>8 )&0xff),
                        (int)(uint8_t)((a>>16)&0xff),
                        (int)(uint8_t)((a>>24)&0xff));
  } else if (addr->family == AF_INET6) {
    int i;
    char *cp = out;
    const uint8_t *bytes = tor_addr_to_in6_addr8(addr);
    if (outlen < REVERSE_LOOKUP_NAME_BUF_LEN)
      return -1;
    for (i = 15; i >= 0; --i) {
      uint8_t byte = bytes[i];
      *cp++ = "0123456789abcdef"[byte & 0x0f];
      *cp++ = '.';
      *cp++ = "0123456789abcdef"[byte >> 4];
      *cp++ = '.';
    }
    memcpy(cp, "ip6.arpa", 9); /* 8 characters plus NUL */
    return 32 * 2 + 8;
  }
  return -1;
}

/** Parse a string <b>s</b> containing an IPv4/IPv6 address, and possibly
 *  a mask and port or port range.  Store the parsed address in
 *  <b>addr_out</b>, a mask (if any) in <b>mask_out</b>, and port(s) (if any)
 *  in <b>port_min_out</b> and <b>port_max_out</b>.
 *
 * The syntax is:
 *   Address OptMask OptPortRange
 *   Address ::= IPv4Address / "[" IPv6Address "]" / "*"
 *   OptMask ::= "/" Integer /
 *   OptPortRange ::= ":*" / ":" Integer / ":" Integer "-" Integer /
 *
 *  - If mask, minport, or maxport are NULL, we do not want these
 *    options to be set; treat them as an error if present.
 *  - If the string has no mask, the mask is set to /32 (IPv4) or /128 (IPv6).
 *  - If the string has one port, it is placed in both min and max port
 *    variables.
 *  - If the string has no port(s), port_(min|max)_out are set to 1 and 65535.
 *
 *  Return an address family on success, or -1 if an invalid address string is
 *  provided.
 *
 *  If 'flags & TAPMP_EXTENDED_STAR' is false, then the wildcard address '*'
 *  yield an IPv4 wildcard.
 *
 *  If 'flags & TAPMP_EXTENDED_STAR' is true, then the wildcard address '*'
 *  yields an AF_UNSPEC wildcard address, which expands to corresponding
 *  wildcard IPv4 and IPv6 rules, and the following change is made
 *  in the grammar above:
 *   Address ::= IPv4Address / "[" IPv6Address "]" / "*" / "*4" / "*6"
 *  with the new "*4" and "*6" productions creating a wildcard to match
 *  IPv4 or IPv6 addresses.
 *
 *  If 'flags & TAPMP_EXTENDED_STAR' and 'flags & TAPMP_STAR_IPV4_ONLY' are
 *  both true, then the wildcard address '*' yields an IPv4 wildcard.
 *
 *  If 'flags & TAPMP_EXTENDED_STAR' and 'flags & TAPMP_STAR_IPV6_ONLY' are
 *  both true, then the wildcard address '*' yields an IPv6 wildcard.
 *
 * TAPMP_STAR_IPV4_ONLY and TAPMP_STAR_IPV6_ONLY are mutually exclusive. */
int
tor_addr_parse_mask_ports(const char *s,
                          unsigned flags,
                          tor_addr_t *addr_out,
                          maskbits_t *maskbits_out,
                          uint16_t *port_min_out, uint16_t *port_max_out)
{
  char *base = NULL, *address, *mask = NULL, *port = NULL, *rbracket = NULL;
  char *endptr;
  int any_flag=0, v4map=0;
  sa_family_t family;
  struct in6_addr in6_tmp;
  struct in_addr in_tmp = { .s_addr = 0 };

  tor_assert(s);
  tor_assert(addr_out);
  /* We can either only want an IPv4 address or only want an IPv6 address,
   * but we can't only want IPv4 & IPv6 at the same time. */
  tor_assert(!((flags & TAPMP_STAR_IPV4_ONLY)
               && (flags & TAPMP_STAR_IPV6_ONLY)));

  /** Longest possible length for an address, mask, and port-range combination.
   * Includes IP, [], /mask, :, ports */
#define MAX_ADDRESS_LENGTH (TOR_ADDR_BUF_LEN+2+(1+INET_NTOA_BUF_LEN)+12+1)

  if (strlen(s) > MAX_ADDRESS_LENGTH) {
    log_warn(LD_GENERAL, "Impossibly long IP %s; rejecting", escaped(s));
    goto err;
  }
  base = tor_strdup(s);

  /* Break 'base' into separate strings. */
  address = base;
  if (*address == '[') {  /* Probably IPv6 */
    address++;
    rbracket = strchr(address, ']');
    if (!rbracket) {
      log_warn(LD_GENERAL,
               "No closing IPv6 bracket in address pattern; rejecting.");
      goto err;
    }
  }
  mask = strchr((rbracket?rbracket:address),'/');
  port = strchr((mask?mask:(rbracket?rbracket:address)), ':');
  if (port)
    *port++ = '\0';
  if (mask)
    *mask++ = '\0';
  if (rbracket)
    *rbracket = '\0';
  if (port && mask)
    tor_assert(port > mask);
  if (mask && rbracket)
    tor_assert(mask > rbracket);

  /* Now "address" is the a.b.c.d|'*'|abcd::1 part...
   *     "mask" is the Mask|Maskbits part...
   * and "port" is the *|port|min-max part.
   */

  /* Process the address portion */
  memset(addr_out, 0, sizeof(tor_addr_t));

  if (!strcmp(address, "*")) {
    if (flags & TAPMP_EXTENDED_STAR) {
      if (flags & TAPMP_STAR_IPV4_ONLY) {
        family = AF_INET;
        tor_addr_from_ipv4h(addr_out, 0);
      } else if (flags & TAPMP_STAR_IPV6_ONLY) {
        static char nil_bytes[16] = { [0]=0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
        family = AF_INET6;
        tor_addr_from_ipv6_bytes(addr_out, nil_bytes);
      } else {
        family = AF_UNSPEC;
        tor_addr_make_unspec(addr_out);
        log_info(LD_GENERAL,
                 "'%s' expands into rules which apply to all IPv4 and IPv6 "
                 "addresses. (Use accept/reject *4:* for IPv4 or "
                 "accept[6]/reject[6] *6:* for IPv6.)", s);
      }
    } else {
      family = AF_INET;
      tor_addr_from_ipv4h(addr_out, 0);
    }
    any_flag = 1;
  } else if (!strcmp(address, "*4") && (flags & TAPMP_EXTENDED_STAR)) {
    family = AF_INET;
    tor_addr_from_ipv4h(addr_out, 0);
    any_flag = 1;
  } else if (!strcmp(address, "*6") && (flags & TAPMP_EXTENDED_STAR)) {
    static char nil_bytes[16] = { [0]=0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    family = AF_INET6;
    tor_addr_from_ipv6_bytes(addr_out, nil_bytes);
    any_flag = 1;
  } else if (tor_inet_pton(AF_INET6, address, &in6_tmp) > 0) {
    family = AF_INET6;
    tor_addr_from_in6(addr_out, &in6_tmp);
  } else if (tor_inet_pton(AF_INET, address, &in_tmp) > 0) {
    family = AF_INET;
    tor_addr_from_in(addr_out, &in_tmp);
  } else {
    log_warn(LD_GENERAL, "Malformed IP %s in address pattern; rejecting.",
             escaped(address));
    goto err;
  }

  v4map = tor_addr_is_v4(addr_out);

  /* Parse mask */
  if (maskbits_out) {
    int bits = 0;
    struct in_addr v4mask;

    if (mask) {  /* the caller (tried to) specify a mask */
      bits = (int) strtol(mask, &endptr, 10);
      if (!*endptr) {  /* strtol converted everything, so it was an integer */
        if ((bits<0 || bits>128) ||
            (family == AF_INET && bits > 32)) {
          log_warn(LD_GENERAL,
                   "Bad number of mask bits (%d) on address range; rejecting.",
                   bits);
          goto err;
        }
      } else {  /* mask might still be an address-style mask */
        if (tor_inet_pton(AF_INET, mask, &v4mask) > 0) {
          bits = addr_mask_get_bits(ntohl(v4mask.s_addr));
          if (bits < 0) {
            log_warn(LD_GENERAL,
                     "IPv4-style mask %s is not a prefix address; rejecting.",
                     escaped(mask));
            goto err;
          }
        } else { /* Not IPv4; we don't do address-style IPv6 masks. */
          log_warn(LD_GENERAL,
                   "Malformed mask on address range %s; rejecting.",
                   escaped(s));
          goto err;
        }
      }
      if (family == AF_INET6 && v4map) {
        if (bits > 32 && bits < 96) { /* Crazy */
          log_warn(LD_GENERAL,
                   "Bad mask bits %d for V4-mapped V6 address; rejecting.",
                   bits);
          goto err;
        }
        /* XXXX_IP6 is this really what we want? */
        bits = 96 + bits%32; /* map v4-mapped masks onto 96-128 bits */
      }
      if (any_flag) {
        log_warn(LD_GENERAL,
                 "Found bit prefix with wildcard address; rejecting");
        goto err;
      }
    } else { /* pick an appropriate mask, as none was given */
      if (any_flag)
        bits = 0;  /* This is okay whether it's V6 or V4 (FIX V4-mapped V6!) */
      else if (tor_addr_family(addr_out) == AF_INET)
        bits = 32;
      else if (tor_addr_family(addr_out) == AF_INET6)
        bits = 128;
    }
    *maskbits_out = (maskbits_t) bits;
  } else {
    if (mask) {
      log_warn(LD_GENERAL,
               "Unexpected mask in address %s; rejecting", escaped(s));
      goto err;
    }
  }

  /* Parse port(s) */
  if (port_min_out) {
    uint16_t port2;
    if (!port_max_out) /* caller specified one port; fake the second one */
      port_max_out = &port2;

    if (parse_port_range(port, port_min_out, port_max_out) < 0) {
      goto err;
    } else if ((*port_min_out != *port_max_out) && port_max_out == &port2) {
      log_warn(LD_GENERAL,
               "Wanted one port from address range, but there are two.");

      port_max_out = NULL;  /* caller specified one port, so set this back */
      goto err;
    }
  } else {
    if (port) {
      log_warn(LD_GENERAL,
               "Unexpected ports in address %s; rejecting", escaped(s));
      goto err;
    }
  }

  tor_free(base);
  return tor_addr_family(addr_out);
 err:
  tor_free(base);
  return -1;
}

/** Determine whether an address is IPv4, either native or IPv4-mapped IPv6.
 * Note that this is about representation only, as any decent stack will
 * reject IPv4-mapped addresses received on the wire (and won't use them
 * on the wire either).
 */
int
tor_addr_is_v4(const tor_addr_t *addr)
{
  tor_assert(addr);

  if (tor_addr_family(addr) == AF_INET)
    return 1;

  if (tor_addr_family(addr) == AF_INET6) {
    /* First two don't need to be ordered */
    uint32_t *a32 = tor_addr_to_in6_addr32(addr);
    if (a32[0] == 0 && a32[1] == 0 && ntohl(a32[2]) == 0x0000ffffu)
      return 1;
  }

  return 0; /* Not IPv4 - unknown family or a full-blood IPv6 address */
}

/** Determine whether an address <b>addr</b> is null, either all zeroes or
 *  belonging to family AF_UNSPEC.
 */
int
tor_addr_is_null(const tor_addr_t *addr)
{
  tor_assert(addr);

  switch (tor_addr_family(addr)) {
    case AF_INET6: {
      uint32_t *a32 = tor_addr_to_in6_addr32(addr);
      return (a32[0] == 0) && (a32[1] == 0) && (a32[2] == 0) && (a32[3] == 0);
    }
    case AF_INET:
      return (tor_addr_to_ipv4n(addr) == 0);
    case AF_UNIX:
      return 1;
    case AF_UNSPEC:
      return 1;
    default:
      log_warn(LD_BUG, "Called with unknown address family %d",
               (int)tor_addr_family(addr));
      return 0;
  }
  //return 1;
}

/** Return true iff <b>addr</b> is a loopback address */
int
tor_addr_is_loopback(const tor_addr_t *addr)
{
  tor_assert(addr);
  switch (tor_addr_family(addr)) {
    case AF_INET6: {
      /* ::1 */
      uint32_t *a32 = tor_addr_to_in6_addr32(addr);
      return (a32[0] == 0) && (a32[1] == 0) && (a32[2] == 0) &&
        (ntohl(a32[3]) == 1);
    }
    case AF_INET:
      /* 127.0.0.1 */
      return (tor_addr_to_ipv4h(addr) & 0xff000000) == 0x7f000000;
    case AF_UNSPEC:
      return 0;
    default:
      /* LCOV_EXCL_START */
      tor_fragile_assert();
      return 0;
      /* LCOV_EXCL_STOP */
  }
}

/* Is addr valid?
 * Checks that addr is non-NULL and not tor_addr_is_null().
 * If for_listening is true, IPv4 addr 0.0.0.0 is allowed.
 * It means "bind to all addresses on the local machine". */
int
tor_addr_is_valid(const tor_addr_t *addr, int for_listening)
{
  /* NULL addresses are invalid regardless of for_listening */
  if (addr == NULL) {
    return 0;
  }

  /* Only allow IPv4 0.0.0.0 for_listening. */
  if (for_listening && addr->family == AF_INET
      && tor_addr_to_ipv4h(addr) == 0) {
    return 1;
  }

  /* Otherwise, the address is valid if it's not tor_addr_is_null() */
  return !tor_addr_is_null(addr);
}

/* Is the network-order IPv4 address v4n_addr valid?
 * Checks that addr is not zero.
 * Except if for_listening is true, where IPv4 addr 0.0.0.0 is allowed. */
int
tor_addr_is_valid_ipv4n(uint32_t v4n_addr, int for_listening)
{
  /* Any IPv4 address is valid with for_listening. */
  if (for_listening) {
    return 1;
  }

  /* Otherwise, zero addresses are invalid. */
  return v4n_addr != 0;
}

/* Is port valid?
 * Checks that port is not 0.
 * Except if for_listening is true, where port 0 is allowed.
 * It means "OS chooses a port". */
int
tor_port_is_valid(uint16_t port, int for_listening)
{
  /* Any port value is valid with for_listening. */
  if (for_listening) {
    return 1;
  }

  /* Otherwise, zero ports are invalid. */
  return port != 0;
}

/** Set <b>dest</b> to equal the IPv4 address in <b>v4addr</b> (given in
 * network order). */
void
tor_addr_from_ipv4n(tor_addr_t *dest, uint32_t v4addr)
{
  tor_assert(dest);
  memset(dest, 0, sizeof(tor_addr_t));
  dest->family = AF_INET;
  dest->addr.in_addr.s_addr = v4addr;
}

/** Set <b>dest</b> to equal the IPv6 address in the 16 bytes at
 * <b>ipv6_bytes</b>. */
void
tor_addr_from_ipv6_bytes(tor_addr_t *dest, const char *ipv6_bytes)
{
  tor_assert(dest);
  tor_assert(ipv6_bytes);
  memset(dest, 0, sizeof(tor_addr_t));
  dest->family = AF_INET6;
  memcpy(dest->addr.in6_addr.s6_addr, ipv6_bytes, 16);
}

/** Set <b>dest</b> equal to the IPv6 address in the in6_addr <b>in6</b>. */
void
tor_addr_from_in6(tor_addr_t *dest, const struct in6_addr *in6)
{
  tor_addr_from_ipv6_bytes(dest, (const char*)in6->s6_addr);
}

/** Copy a tor_addr_t from <b>src</b> to <b>dest</b>.
 */
void
tor_addr_copy(tor_addr_t *dest, const tor_addr_t *src)
{
  if (src == dest)
    return;
  tor_assert(src);
  tor_assert(dest);
  memcpy(dest, src, sizeof(tor_addr_t));
}

/** Copy a tor_addr_t from <b>src</b> to <b>dest</b>, taking extra care to
 * copy only the well-defined portions. Used for computing hashes of
 * addresses.
 */
void
tor_addr_copy_tight(tor_addr_t *dest, const tor_addr_t *src)
{
  tor_assert(src != dest);
  tor_assert(src);
  tor_assert(dest);
  memset(dest, 0, sizeof(tor_addr_t));
  dest->family = src->family;
  switch (tor_addr_family(src))
    {
    case AF_INET:
      dest->addr.in_addr.s_addr = src->addr.in_addr.s_addr;
      break;
    case AF_INET6:
      memcpy(dest->addr.in6_addr.s6_addr, src->addr.in6_addr.s6_addr, 16);
    case AF_UNSPEC:
      break;
    default:
      tor_fragile_assert(); // LCOV_EXCL_LINE
    }
}

/** Given two addresses <b>addr1</b> and <b>addr2</b>, return 0 if the two
 * addresses are equivalent under the mask mbits, less than 0 if addr1
 * precedes addr2, and greater than 0 otherwise.
 *
 * Different address families (IPv4 vs IPv6) are always considered unequal if
 * <b>how</b> is CMP_EXACT; otherwise, IPv6-mapped IPv4 addresses are
 * considered equivalent to their IPv4 equivalents.
 *
 * As a special case, all pointer-wise distinct AF_UNIX addresses are always
 * considered unequal since tor_addr_t currently does not contain the
 * information required to make the comparison.
 */
int
tor_addr_compare(const tor_addr_t *addr1, const tor_addr_t *addr2,
                 tor_addr_comparison_t how)
{
  return tor_addr_compare_masked(addr1, addr2, 128, how);
}

/** As tor_addr_compare(), but only looks at the first <b>mask</b> bits of
 * the address.
 *
 * Reduce over-specific masks (>128 for ipv6, >32 for ipv4) to 128 or 32.
 *
 * The mask is interpreted relative to <b>addr1</b>, so that if a is
 * \::ffff:1.2.3.4, and b is 3.4.5.6,
 * tor_addr_compare_masked(a,b,100,CMP_SEMANTIC) is the same as
 * -tor_addr_compare_masked(b,a,4,CMP_SEMANTIC).
 *
 * We guarantee that the ordering from tor_addr_compare_masked is a total
 * order on addresses, but not that it is any particular order, or that it
 * will be the same from one version to the next.
 */
int
tor_addr_compare_masked(const tor_addr_t *addr1, const tor_addr_t *addr2,
                        maskbits_t mbits, tor_addr_comparison_t how)
{
  /** Helper: Evaluates to -1 if a is less than b, 0 if a equals b, or 1 if a
   * is greater than b.  May evaluate a and b more than once.  */
#define TRISTATE(a,b) (((a)<(b))?-1: (((a)==(b))?0:1))
  sa_family_t family1, family2, v_family1, v_family2;

  tor_assert(addr1 && addr2);

  v_family1 = family1 = tor_addr_family(addr1);
  v_family2 = family2 = tor_addr_family(addr2);

  if (family1==family2) {
    /* When the families are the same, there's only one way to do the
     * comparison: exactly. */
    int r;
    switch (family1) {
      case AF_UNSPEC:
        return 0; /* All unspecified addresses are equal */
      case AF_INET: {
        uint32_t a1 = tor_addr_to_ipv4h(addr1);
        uint32_t a2 = tor_addr_to_ipv4h(addr2);
        if (mbits <= 0)
          return 0;
        if (mbits > 32)
          mbits = 32;
        a1 >>= (32-mbits);
        a2 >>= (32-mbits);
        r = TRISTATE(a1, a2);
        return r;
      }
      case AF_INET6: {
        if (mbits > 128)
          mbits = 128;

        const uint8_t *a1 = tor_addr_to_in6_addr8(addr1);
        const uint8_t *a2 = tor_addr_to_in6_addr8(addr2);
        const int bytes = mbits >> 3;
        const int leftover_bits = mbits & 7;
        if (bytes && (r = tor_memcmp(a1, a2, bytes))) {
          return r;
        } else if (leftover_bits) {
          uint8_t b1 = a1[bytes] >> (8-leftover_bits);
          uint8_t b2 = a2[bytes] >> (8-leftover_bits);
          return TRISTATE(b1, b2);
        } else {
          return 0;
        }
      }
      case AF_UNIX:
        /* HACKHACKHACKHACKHACK:
         * tor_addr_t doesn't contain a copy of sun_path, so it's not
         * possible to comapre this at all.
         *
         * Since the only time we currently actually should be comparing
         * 2 AF_UNIX addresses is when dealing with ISO_CLIENTADDR (which
         * is disabled for AF_UNIX SocksPorts anyway), this just does
         * a pointer comparison.
         *
         * See: #20261.
         */
        if (addr1 < addr2)
          return -1;
        else if (addr1 == addr2)
          return 0;
        else
          return 1;
      default:
        /* LCOV_EXCL_START */
        tor_fragile_assert();
        return 0;
        /* LCOV_EXCL_STOP */
    }
  } else if (how == CMP_EXACT) {
    /* Unequal families and an exact comparison?  Stop now! */
    return TRISTATE(family1, family2);
  }

  if (mbits == 0)
    return 0;

  if (family1 == AF_INET6 && tor_addr_is_v4(addr1))
    v_family1 = AF_INET;
  if (family2 == AF_INET6 && tor_addr_is_v4(addr2))
    v_family2 = AF_INET;
  if (v_family1 == v_family2) {
    /* One or both addresses are a mapped ipv4 address. */
    uint32_t a1, a2;
    if (family1 == AF_INET6) {
      a1 = tor_addr_to_mapped_ipv4h(addr1);
      if (mbits <= 96)
        return 0;
      mbits -= 96; /* We just decided that the first 96 bits of a1 "match". */
    } else {
      a1 = tor_addr_to_ipv4h(addr1);
    }
    if (family2 == AF_INET6) {
      a2 = tor_addr_to_mapped_ipv4h(addr2);
    } else {
      a2 = tor_addr_to_ipv4h(addr2);
    }
    if (mbits > 32) mbits = 32;
    a1 >>= (32-mbits);
    a2 >>= (32-mbits);
    return TRISTATE(a1, a2);
  } else {
    /* Unequal families, and semantic comparison, and no semantic family
     * matches. */
    return TRISTATE(family1, family2);
  }
}

/** Return a hash code based on the address addr. DOCDOC extra */
uint64_t
tor_addr_hash(const tor_addr_t *addr)
{
  switch (tor_addr_family(addr)) {
  case AF_INET:
    return siphash24g(&addr->addr.in_addr.s_addr, 4);
  case AF_UNSPEC:
    return 0x4e4d5342;
  case AF_INET6:
    return siphash24g(&addr->addr.in6_addr.s6_addr, 16);
  default:
    /* LCOV_EXCL_START */
    tor_fragile_assert();
    return 0;
    /* LCOV_EXCL_END */
  }
}

/** Return a newly allocated string with a representation of <b>addr</b>. */
char *
tor_addr_to_str_dup(const tor_addr_t *addr)
{
  char buf[TOR_ADDR_BUF_LEN];
  if (tor_addr_to_str(buf, addr, sizeof(buf), 0)) {
    return tor_strdup(buf);
  } else {
    return tor_strdup("<unknown address type>");
  }
}

/** Return a string representing the address <b>addr</b>.  This string
 * is statically allocated, and must not be freed.  Each call to
 * <b>fmt_addr_impl</b> invalidates the last result of the function.
 * This function is not thread-safe. If <b>decorate</b> is set, add
 * brackets to IPv6 addresses.
 *
 * It's better to use the wrapper macros of this function:
 * <b>fmt_addr()</b> and <b>fmt_and_decorate_addr()</b>.
 */
const char *
fmt_addr_impl(const tor_addr_t *addr, int decorate)
{
  static char buf[TOR_ADDR_BUF_LEN];
  if (!addr) return "<null>";
  if (tor_addr_to_str(buf, addr, sizeof(buf), decorate))
    return buf;
  else
    return "???";
}

/** Return a string representing the pair <b>addr</b> and <b>port</b>.
 * This calls fmt_and_decorate_addr internally, so IPv6 addresses will
 * have brackets, and the caveats of fmt_addr_impl apply.
 */
const char *
fmt_addrport(const tor_addr_t *addr, uint16_t port)
{
  /* Add space for a colon and up to 5 digits. */
  static char buf[TOR_ADDR_BUF_LEN + 6];
  tor_snprintf(buf, sizeof(buf), "%s:%u", fmt_and_decorate_addr(addr), port);
  return buf;
}

/** Like fmt_addr(), but takes <b>addr</b> as a host-order IPv4
 * addresses. Also not thread-safe, also clobbers its return buffer on
 * repeated calls. */
const char *
fmt_addr32(uint32_t addr)
{
  static char buf[INET_NTOA_BUF_LEN];
  struct in_addr in;
  in.s_addr = htonl(addr);
  tor_inet_ntoa(&in, buf, sizeof(buf));
  return buf;
}

/** Convert the string in <b>src</b> to a tor_addr_t <b>addr</b>.  The string
 * may be an IPv4 address, an IPv6 address, or an IPv6 address surrounded by
 * square brackets.
 *
 *  Return an address family on success, or -1 if an invalid address string is
 *  provided. */
int
tor_addr_parse(tor_addr_t *addr, const char *src)
{
  /* Holds substring of IPv6 address after removing square brackets */
  char *tmp = NULL;
  int result;
  struct in_addr in_tmp;
  struct in6_addr in6_tmp;
  tor_assert(addr && src);
  if (src[0] == '[' && src[1])
    src = tmp = tor_strndup(src+1, strlen(src)-2);

  if (tor_inet_pton(AF_INET6, src, &in6_tmp) > 0) {
    result = AF_INET6;
    tor_addr_from_in6(addr, &in6_tmp);
  } else if (tor_inet_pton(AF_INET, src, &in_tmp) > 0) {
    result = AF_INET;
    tor_addr_from_in(addr, &in_tmp);
  } else {
    result = -1;
  }

  tor_free(tmp);
  return result;
}

/** Parse an address or address-port combination from <b>s</b>, resolve the
 * address as needed, and put the result in <b>addr_out</b> and (optionally)
 * <b>port_out</b>.  Return 0 on success, negative on failure. */
int
tor_addr_port_lookup(const char *s, tor_addr_t *addr_out, uint16_t *port_out)
{
  const char *port;
  tor_addr_t addr;
  uint16_t portval;
  char *tmp = NULL;

  tor_assert(s);
  tor_assert(addr_out);

  s = eat_whitespace(s);

  if (*s == '[') {
    port = strstr(s, "]");
    if (!port)
      goto err;
    tmp = tor_strndup(s+1, port-(s+1));
    port = port+1;
    if (*port == ':')
      port++;
    else
      port = NULL;
  } else {
    port = strchr(s, ':');
    if (port)
      tmp = tor_strndup(s, port-s);
    else
      tmp = tor_strdup(s);
    if (port)
      ++port;
  }

  if (tor_addr_lookup(tmp, AF_UNSPEC, &addr) != 0)
    goto err;
  tor_free(tmp);

  if (port) {
    portval = (int) tor_parse_long(port, 10, 1, 65535, NULL, NULL);
    if (!portval)
      goto err;
  } else {
    portval = 0;
  }

  if (port_out)
    *port_out = portval;
  tor_addr_copy(addr_out, &addr);

  return 0;
 err:
  tor_free(tmp);
  return -1;
}

#ifdef _WIN32
typedef ULONG (WINAPI *GetAdaptersAddresses_fn_t)(
              ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
#endif

#ifdef HAVE_IFADDRS_TO_SMARTLIST
/*
 * Convert a linked list consisting of <b>ifaddrs</b> structures
 * into smartlist of <b>tor_addr_t</b> structures.
 */
STATIC smartlist_t *
ifaddrs_to_smartlist(const struct ifaddrs *ifa, sa_family_t family)
{
  smartlist_t *result = smartlist_new();
  const struct ifaddrs *i;

  for (i = ifa; i; i = i->ifa_next) {
    tor_addr_t tmp;
    if ((i->ifa_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING))
      continue;
    if (!i->ifa_addr)
      continue;
    if (i->ifa_addr->sa_family != AF_INET &&
        i->ifa_addr->sa_family != AF_INET6)
      continue;
    if (family != AF_UNSPEC && i->ifa_addr->sa_family != family)
      continue;
    if (tor_addr_from_sockaddr(&tmp, i->ifa_addr, NULL) < 0)
      continue;
    smartlist_add(result, tor_memdup(&tmp, sizeof(tmp)));
  }

  return result;
}

/** Use getiffaddrs() function to get list of current machine
 * network interface addresses. Represent the result by smartlist of
 * <b>tor_addr_t</b> structures.
 */
STATIC smartlist_t *
get_interface_addresses_ifaddrs(int severity, sa_family_t family)
{

  /* Most free Unixy systems provide getifaddrs, which gives us a linked list
   * of struct ifaddrs. */
  struct ifaddrs *ifa = NULL;
  smartlist_t *result;
  if (getifaddrs(&ifa) < 0) {
    log_fn(severity, LD_NET, "Unable to call getifaddrs(): %s",
           strerror(errno));
    return NULL;
  }

  result = ifaddrs_to_smartlist(ifa, family);

  freeifaddrs(ifa);

  return result;
}
#endif

#ifdef HAVE_IP_ADAPTER_TO_SMARTLIST

/** Convert a Windows-specific <b>addresses</b> linked list into smartlist
 * of <b>tor_addr_t</b> structures.
 */

STATIC smartlist_t *
ip_adapter_addresses_to_smartlist(const IP_ADAPTER_ADDRESSES *addresses)
{
  smartlist_t *result = smartlist_new();
  const IP_ADAPTER_ADDRESSES *address;

  for (address = addresses; address; address = address->Next) {
    const IP_ADAPTER_UNICAST_ADDRESS *a;
    for (a = address->FirstUnicastAddress; a; a = a->Next) {
      /* Yes, it's a linked list inside a linked list */
      const struct sockaddr *sa = a->Address.lpSockaddr;
      tor_addr_t tmp;
      if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
        continue;
      if (tor_addr_from_sockaddr(&tmp, sa, NULL) < 0)
        continue;
      smartlist_add(result, tor_memdup(&tmp, sizeof(tmp)));
    }
  }

  return result;
}

/** Windows only: use GetAdaptersInfo() function to retrieve network interface
 * addresses of current machine and return them to caller as smartlist of
 * <b>tor_addr_t</b>  structures.
 */
STATIC smartlist_t *
get_interface_addresses_win32(int severity, sa_family_t family)
{

  /* Windows XP began to provide GetAdaptersAddresses. Windows 2000 had a
     "GetAdaptersInfo", but that's deprecated; let's just try
     GetAdaptersAddresses and fall back to connect+getsockname.
  */
  HANDLE lib = load_windows_system_library(TEXT("iphlpapi.dll"));
  smartlist_t *result = NULL;
  GetAdaptersAddresses_fn_t fn;
  ULONG size, res;
  IP_ADAPTER_ADDRESSES *addresses = NULL;

  (void) severity;

#define FLAGS (GAA_FLAG_SKIP_ANYCAST | \
               GAA_FLAG_SKIP_MULTICAST | \
               GAA_FLAG_SKIP_DNS_SERVER)

  if (!lib) {
    log_fn(severity, LD_NET, "Unable to load iphlpapi.dll");
    goto done;
  }

  if (!(fn = (GetAdaptersAddresses_fn_t)
                  GetProcAddress(lib, "GetAdaptersAddresses"))) {
    log_fn(severity, LD_NET, "Unable to obtain pointer to "
           "GetAdaptersAddresses");
    goto done;
  }

  /* Guess how much space we need. */
  size = 15*1024;
  addresses = tor_malloc(size);
  res = fn(family, FLAGS, NULL, addresses, &size);
  if (res == ERROR_BUFFER_OVERFLOW) {
    /* we didn't guess that we needed enough space; try again */
    tor_free(addresses);
    addresses = tor_malloc(size);
    res = fn(AF_UNSPEC, FLAGS, NULL, addresses, &size);
  }
  if (res != NO_ERROR) {
    log_fn(severity, LD_NET, "GetAdaptersAddresses failed (result: %lu)", res);
    goto done;
  }

  result = ip_adapter_addresses_to_smartlist(addresses);

 done:
  if (lib)
    FreeLibrary(lib);
  tor_free(addresses);
  return result;
}

#endif

#ifdef HAVE_IFCONF_TO_SMARTLIST

/* Guess how much space we need. There shouldn't be any struct ifreqs
 * larger than this, even on OS X where the struct's size is dynamic. */
#define IFREQ_SIZE 4096

/* This is defined on Mac OS X */
#ifndef _SIZEOF_ADDR_IFREQ
#define _SIZEOF_ADDR_IFREQ sizeof
#endif

/** Convert <b>*buf</b>, an ifreq structure array of size <b>buflen</b>,
 * into smartlist of <b>tor_addr_t</b> structures.
 */
STATIC smartlist_t *
ifreq_to_smartlist(char *buf, size_t buflen)
{
  smartlist_t *result = smartlist_new();
  char *end = buf + buflen;

  /* These acrobatics are due to alignment issues which trigger
   * undefined behaviour traps on OSX. */
  struct ifreq *r = tor_malloc(IFREQ_SIZE);

  while (buf < end) {
    /* Copy up to IFREQ_SIZE bytes into the struct ifreq, but don't overrun
     * buf. */
    memcpy(r, buf, end - buf < IFREQ_SIZE ? end - buf : IFREQ_SIZE);

    const struct sockaddr *sa = &r->ifr_addr;
    tor_addr_t tmp;
    int valid_sa_family = (sa->sa_family == AF_INET ||
                           sa->sa_family == AF_INET6);

    int conversion_success = (tor_addr_from_sockaddr(&tmp, sa, NULL) == 0);

    if (valid_sa_family && conversion_success)
      smartlist_add(result, tor_memdup(&tmp, sizeof(tmp)));

    buf += _SIZEOF_ADDR_IFREQ(*r);
  }

  tor_free(r);
  return result;
}

/** Use ioctl(.,SIOCGIFCONF,.) to get a list of current machine
 * network interface addresses. Represent the result by smartlist of
 * <b>tor_addr_t</b> structures.
 */
STATIC smartlist_t *
get_interface_addresses_ioctl(int severity, sa_family_t family)
{
  /* Some older unixy systems make us use ioctl(SIOCGIFCONF) */
  struct ifconf ifc;
  ifc.ifc_buf = NULL;
  int fd;
  smartlist_t *result = NULL;

  /* This interface, AFAICT, only supports AF_INET addresses,
   * except on AIX. For Solaris, we could use SIOCGLIFCONF. */

  /* Bail out if family is neither AF_INET nor AF_UNSPEC since
   * ioctl() technique supports non-IPv4 interface addresses on
   * a small number of niche systems only. If family is AF_UNSPEC,
   * fall back to getting AF_INET addresses only. */
  if (family == AF_UNSPEC)
    family = AF_INET;
  else if (family != AF_INET)
    return NULL;

  fd = socket(family, SOCK_DGRAM, 0);
  if (fd < 0) {
    tor_log(severity, LD_NET, "socket failed: %s", strerror(errno));
    goto done;
  }

  int mult = 1;
  do {
    mult *= 2;
    ifc.ifc_len = mult * IFREQ_SIZE;
    ifc.ifc_buf = tor_realloc(ifc.ifc_buf, ifc.ifc_len);

    tor_assert(ifc.ifc_buf);

    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
      tor_log(severity, LD_NET, "ioctl failed: %s", strerror(errno));
      goto done;
    }
    /* Ensure we have least IFREQ_SIZE bytes unused at the end. Otherwise, we
     * don't know if we got everything during ioctl. */
  } while (mult * IFREQ_SIZE - ifc.ifc_len <= IFREQ_SIZE);
  result = ifreq_to_smartlist(ifc.ifc_buf, ifc.ifc_len);

 done:
  if (fd >= 0)
    close(fd);
  tor_free(ifc.ifc_buf);
  return result;
}
#endif

/** Try to ask our network interfaces what addresses they are bound to.
 * Return a new smartlist of tor_addr_t on success, and NULL on failure.
 * (An empty smartlist indicates that we successfully learned that we have no
 * addresses.)  Log failure messages at <b>severity</b>. Only return the
 * interface addresses of requested <b>family</b> and ignore the addresses
 * of other address families. */
MOCK_IMPL(smartlist_t *,
get_interface_addresses_raw,(int severity, sa_family_t family))
{
  smartlist_t *result = NULL;
#if defined(HAVE_IFADDRS_TO_SMARTLIST)
  if ((result = get_interface_addresses_ifaddrs(severity, family)))
    return result;
#endif
#if defined(HAVE_IP_ADAPTER_TO_SMARTLIST)
  if ((result = get_interface_addresses_win32(severity, family)))
    return result;
#endif
#if defined(HAVE_IFCONF_TO_SMARTLIST)
  if ((result = get_interface_addresses_ioctl(severity, family)))
    return result;
#endif
  (void) severity;
  (void) result;
  return NULL;
}

/** Return true iff <b>a</b> is a multicast address.  */
int
tor_addr_is_multicast(const tor_addr_t *a)
{
  sa_family_t family = tor_addr_family(a);
  if (family == AF_INET) {
    uint32_t ipv4h = tor_addr_to_ipv4h(a);
    if ((ipv4h >> 24) == 0xe0)
      return 1; /* Multicast */
  } else if (family == AF_INET6) {
    const uint8_t *a32 = tor_addr_to_in6_addr8(a);
    if (a32[0] == 0xff)
      return 1;
  }
  return 0;
}

/** Attempt to retrieve IP address of current host by utilizing some
 * UDP socket trickery. Only look for address of given <b>family</b>
 * (only AF_INET and AF_INET6 are supported). Set result to *<b>addr</b>.
 * Return 0 on success, -1 on failure.
 */
MOCK_IMPL(int,
get_interface_address6_via_udp_socket_hack,(int severity,
                                            sa_family_t family,
                                            tor_addr_t *addr))
{
  struct sockaddr_storage my_addr, target_addr;
  int sock=-1, r=-1;
  socklen_t addr_len;

  memset(addr, 0, sizeof(tor_addr_t));
  memset(&target_addr, 0, sizeof(target_addr));

  /* Don't worry: no packets are sent. We just need to use a real address
   * on the actual Internet. */
  if (family == AF_INET6) {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&target_addr;
    /* Use the "discard" service port */
    sin6->sin6_port = htons(9);
    sock = tor_open_socket(PF_INET6,SOCK_DGRAM,IPPROTO_UDP);
    addr_len = (socklen_t)sizeof(struct sockaddr_in6);
    sin6->sin6_family = AF_INET6;
    S6_ADDR16(sin6->sin6_addr)[0] = htons(0x2002); /* 2002:: */
  } else if (family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in*)&target_addr;
    /* Use the "discard" service port */
    sin->sin_port = htons(9);
    sock = tor_open_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    addr_len = (socklen_t)sizeof(struct sockaddr_in);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(0x12000001); /* 18.0.0.1 */
  } else {
    return -1;
  }

  if (sock < 0) {
    int e = tor_socket_errno(-1);
    log_fn(severity, LD_NET, "unable to create socket: %s",
           tor_socket_strerror(e));
    goto err;
  }

  if (tor_connect_socket(sock,(struct sockaddr *)&target_addr,
                         addr_len) < 0) {
    int e = tor_socket_errno(sock);
    log_fn(severity, LD_NET, "connect() failed: %s", tor_socket_strerror(e));
    goto err;
  }

  if (tor_getsockname(sock,(struct sockaddr*)&my_addr, &addr_len)) {
    int e = tor_socket_errno(sock);
    log_fn(severity, LD_NET, "getsockname() to determine interface failed: %s",
           tor_socket_strerror(e));
    goto err;
  }

 if (tor_addr_from_sockaddr(addr, (struct sockaddr*)&my_addr, NULL) == 0) {
    if (tor_addr_is_loopback(addr) || tor_addr_is_multicast(addr)) {
      log_fn(severity, LD_NET, "Address that we determined via UDP socket"
                               " magic is unsuitable for public comms.");
    } else {
      r=0;
    }
 }

 err:
  if (sock >= 0)
    tor_close_socket(sock);
  if (r == -1)
    memset(addr, 0, sizeof(tor_addr_t));
  return r;
}

/** Set *<b>addr</b> to an arbitrary IP address (if any) of an interface that
 * connects to the Internet.  Prefer public IP addresses to internal IP
 * addresses.  This address should only be used in checking whether our
 * address has changed, as it may be an internal IP address.  Return 0 on
 * success, -1 on failure.
 * Prefer get_interface_address6_list for a list of all addresses on all
 * interfaces which connect to the Internet.
 */
MOCK_IMPL(int,
get_interface_address6,(int severity, sa_family_t family, tor_addr_t *addr))
{
  smartlist_t *addrs;
  int rv = -1;
  tor_assert(addr);

  memset(addr, 0, sizeof(tor_addr_t));

  /* Get a list of public or internal IPs in arbitrary order */
  addrs = get_interface_address6_list(severity, family, 1);

  /* Find the first non-internal address, or the last internal address
   * Ideally, we want the default route, see #12377 for details */
  SMARTLIST_FOREACH_BEGIN(addrs, tor_addr_t *, a) {
    tor_addr_copy(addr, a);
    rv = 0;

    /* If we found a non-internal address, declare success.  Otherwise,
     * keep looking. */
    if (!tor_addr_is_internal(a, 0))
      break;
  } SMARTLIST_FOREACH_END(a);

  free_interface_address6_list(addrs);
  return rv;
}

/** Free a smartlist of IP addresses returned by get_interface_address6_list.
 */
void
free_interface_address6_list(smartlist_t *addrs)
{
  if (addrs != NULL) {
    SMARTLIST_FOREACH(addrs, tor_addr_t *, a, tor_free(a));
    smartlist_free(addrs);
  }
}

/** Return a smartlist of the IP addresses of type family from all interfaces
 * on the server. Excludes loopback and multicast addresses. Only includes
 * internal addresses if include_internal is true. (Note that a relay behind
 * NAT may use an internal address to connect to the Internet.)
 * An empty smartlist means that there are no addresses of the selected type
 * matching these criteria.
 * Returns NULL on failure.
 * Use free_interface_address6_list to free the returned list.
 */
MOCK_IMPL(smartlist_t *,get_interface_address6_list,(int severity,
                                                     sa_family_t family,
                                                     int include_internal))
{
  smartlist_t *addrs;
  tor_addr_t addr;

  /* Try to do this the smart way if possible. */
  if ((addrs = get_interface_addresses_raw(severity, family))) {
    SMARTLIST_FOREACH_BEGIN(addrs, tor_addr_t *, a)
    {
      if (tor_addr_is_loopback(a) ||
          tor_addr_is_multicast(a)) {
        SMARTLIST_DEL_CURRENT_KEEPORDER(addrs, a);
        tor_free(a);
        continue;
      }

      if (!include_internal && tor_addr_is_internal(a, 0)) {
        SMARTLIST_DEL_CURRENT_KEEPORDER(addrs, a);
        tor_free(a);
        continue;
      }
    } SMARTLIST_FOREACH_END(a);
  }

  if (addrs && smartlist_len(addrs) > 0) {
    return addrs;
  }

  /* if we removed all entries as unsuitable */
  if (addrs) {
    smartlist_free(addrs);
  }

  /* Okay, the smart way is out. */
  addrs = smartlist_new();

  if (family == AF_INET || family == AF_UNSPEC) {
    if (get_interface_address6_via_udp_socket_hack(severity,AF_INET,
                                                   &addr) == 0) {
      if (include_internal || !tor_addr_is_internal(&addr, 0)) {
        smartlist_add(addrs, tor_memdup(&addr, sizeof(addr)));
      }
    }
  }

  if (family == AF_INET6 || family == AF_UNSPEC) {
    if (get_interface_address6_via_udp_socket_hack(severity,AF_INET6,
                                                   &addr) == 0) {
      if (include_internal || !tor_addr_is_internal(&addr, 0)) {
        smartlist_add(addrs, tor_memdup(&addr, sizeof(addr)));
      }
    }
  }

  return addrs;
}

/* ======
 * IPv4 helpers
 * XXXX IPv6 deprecate some of these.
 */

/** Given an address of the form "ip:port", try to divide it into its
 * ip and port portions, setting *<b>address_out</b> to a newly
 * allocated string holding the address portion and *<b>port_out</b>
 * to the port.
 *
 * Don't do DNS lookups and don't allow domain names in the "ip" field.
 *
 * If <b>default_port</b> is less than 0, don't accept <b>addrport</b> of the
 * form "ip" or "ip:0".  Otherwise, accept those forms, and set
 * *<b>port_out</b> to <b>default_port</b>.
 *
 * Return 0 on success, -1 on failure. */
int
tor_addr_port_parse(int severity, const char *addrport,
                    tor_addr_t *address_out, uint16_t *port_out,
                    int default_port)
{
  int retval = -1;
  int r;
  char *addr_tmp = NULL;

  tor_assert(addrport);
  tor_assert(address_out);
  tor_assert(port_out);

  r = tor_addr_port_split(severity, addrport, &addr_tmp, port_out);
  if (r < 0)
    goto done;

  if (!*port_out) {
    if (default_port >= 0)
      *port_out = default_port;
    else
      goto done;
  }

  /* make sure that address_out is an IP address */
  if (tor_addr_parse(address_out, addr_tmp) < 0)
    goto done;

  retval = 0;

 done:
  tor_free(addr_tmp);
  return retval;
}

/** Given an address of the form "host[:port]", try to divide it into its host
 * and port portions, setting *<b>address_out</b> to a newly allocated string
 * holding the address portion and *<b>port_out</b> to the port (or 0 if no
 * port is given).  Return 0 on success, -1 on failure. */
int
tor_addr_port_split(int severity, const char *addrport,
                    char **address_out, uint16_t *port_out)
{
  tor_addr_t a_tmp;
  tor_assert(addrport);
  tor_assert(address_out);
  tor_assert(port_out);
  /* We need to check for IPv6 manually because addr_port_lookup() doesn't
   * do a good job on IPv6 addresses that lack a port. */
  if (tor_addr_parse(&a_tmp, addrport) == AF_INET6) {
    *port_out = 0;
    *address_out = tor_strdup(addrport);
    return 0;
  }

  return addr_port_lookup(severity, addrport, address_out, NULL, port_out);
}

/** Parse a string of the form "host[:port]" from <b>addrport</b>.  If
 * <b>address</b> is provided, set *<b>address</b> to a copy of the
 * host portion of the string.  If <b>addr</b> is provided, try to
 * resolve the host portion of the string and store it into
 * *<b>addr</b> (in host byte order).  If <b>port_out</b> is provided,
 * store the port number into *<b>port_out</b>, or 0 if no port is given.
 * If <b>port_out</b> is NULL, then there must be no port number in
 * <b>addrport</b>.
 * Return 0 on success, -1 on failure.
 */
int
addr_port_lookup(int severity, const char *addrport, char **address,
                uint32_t *addr, uint16_t *port_out)
{
  const char *colon;
  char *address_ = NULL;
  int port_;
  int ok = 1;

  tor_assert(addrport);

  colon = strrchr(addrport, ':');
  if (colon) {
    address_ = tor_strndup(addrport, colon-addrport);
    port_ = (int) tor_parse_long(colon+1,10,1,65535,NULL,NULL);
    if (!port_) {
      log_fn(severity, LD_GENERAL, "Port %s out of range", escaped(colon+1));
      ok = 0;
    }
    if (!port_out) {
      char *esc_addrport = esc_for_log(addrport);
      log_fn(severity, LD_GENERAL,
             "Port %s given on %s when not required",
             escaped(colon+1), esc_addrport);
      tor_free(esc_addrport);
      ok = 0;
    }
  } else {
    address_ = tor_strdup(addrport);
    port_ = 0;
  }

  if (addr) {
    /* There's an addr pointer, so we need to resolve the hostname. */
    if (tor_lookup_hostname(address_,addr)) {
      log_fn(severity, LD_NET, "Couldn't look up %s", escaped(address_));
      ok = 0;
      *addr = 0;
    }
  }

  if (address && ok) {
    *address = address_;
  } else {
    if (address)
      *address = NULL;
    tor_free(address_);
  }
  if (port_out)
    *port_out = ok ? ((uint16_t) port_) : 0;

  return ok ? 0 : -1;
}

/** If <b>mask</b> is an address mask for a bit-prefix, return the number of
 * bits.  Otherwise, return -1. */
int
addr_mask_get_bits(uint32_t mask)
{
  int i;
  if (mask == 0)
    return 0;
  if (mask == 0xFFFFFFFFu)
    return 32;
  for (i=1; i<=32; ++i) {
    if (mask == (uint32_t) ~((1u<<(32-i))-1)) {
      return i;
    }
  }
  return -1;
}

/** Parse a string <b>s</b> in the format of (*|port(-maxport)?)?, setting the
 * various *out pointers as appropriate.  Return 0 on success, -1 on failure.
 */
int
parse_port_range(const char *port, uint16_t *port_min_out,
                 uint16_t *port_max_out)
{
  int port_min, port_max, ok;
  tor_assert(port_min_out);
  tor_assert(port_max_out);

  if (!port || *port == '\0' || strcmp(port, "*") == 0) {
    port_min = 1;
    port_max = 65535;
  } else {
    char *endptr = NULL;
    port_min = (int)tor_parse_long(port, 10, 0, 65535, &ok, &endptr);
    if (!ok) {
      log_warn(LD_GENERAL,
               "Malformed port %s on address range; rejecting.",
               escaped(port));
      return -1;
    } else if (endptr && *endptr == '-') {
      port = endptr+1;
      endptr = NULL;
      port_max = (int)tor_parse_long(port, 10, 1, 65535, &ok, &endptr);
      if (!ok) {
        log_warn(LD_GENERAL,
                 "Malformed port %s on address range; rejecting.",
                 escaped(port));
        return -1;
      }
    } else {
      port_max = port_min;
    }
    if (port_min > port_max) {
      log_warn(LD_GENERAL, "Insane port range on address policy; rejecting.");
      return -1;
    }
  }

  if (port_min < 1)
    port_min = 1;
  if (port_max > 65535)
    port_max = 65535;

  *port_min_out = (uint16_t) port_min;
  *port_max_out = (uint16_t) port_max;

  return 0;
}

/** Given an IPv4 in_addr struct *<b>in</b> (in network order, as usual),
 *  write it as a string into the <b>buf_len</b>-byte buffer in
 *  <b>buf</b>.
 */
int
tor_inet_ntoa(const struct in_addr *in, char *buf, size_t buf_len)
{
  uint32_t a = ntohl(in->s_addr);
  return tor_snprintf(buf, buf_len, "%d.%d.%d.%d",
                      (int)(uint8_t)((a>>24)&0xff),
                      (int)(uint8_t)((a>>16)&0xff),
                      (int)(uint8_t)((a>>8 )&0xff),
                      (int)(uint8_t)((a    )&0xff));
}

/** Given a host-order <b>addr</b>, call tor_inet_ntop() on it
 *  and return a strdup of the resulting address.
 */
char *
tor_dup_ip(uint32_t addr)
{
  char buf[TOR_ADDR_BUF_LEN];
  struct in_addr in;

  in.s_addr = htonl(addr);
  tor_inet_ntop(AF_INET, &in, buf, sizeof(buf));
  return tor_strdup(buf);
}

/**
 * Set *<b>addr</b> to a host-order IPv4 address (if any) of an
 * interface that connects to the Internet.  Prefer public IP addresses to
 * internal IP addresses.  This address should only be used in checking
 * whether our address has changed, as it may be an internal IPv4 address.
 * Return 0 on success, -1 on failure.
 * Prefer get_interface_address_list6 for a list of all IPv4 and IPv6
 * addresses on all interfaces which connect to the Internet.
 */
MOCK_IMPL(int,
get_interface_address,(int severity, uint32_t *addr))
{
  tor_addr_t local_addr;
  int r;

  memset(addr, 0, sizeof(uint32_t));

  r = get_interface_address6(severity, AF_INET, &local_addr);
  if (r>=0)
    *addr = tor_addr_to_ipv4h(&local_addr);
  return r;
}

/** Return true if we can tell that <b>name</b> is a canonical name for the
 * loopback address. */
int
tor_addr_hostname_is_local(const char *name)
{
  return !strcasecmp(name, "localhost") ||
    !strcasecmp(name, "local") ||
    !strcasecmpend(name, ".local");
}

/** Return a newly allocated tor_addr_port_t with <b>addr</b> and
    <b>port</b> filled in. */
tor_addr_port_t *
tor_addr_port_new(const tor_addr_t *addr, uint16_t port)
{
  tor_addr_port_t *ap = tor_malloc_zero(sizeof(tor_addr_port_t));
  if (addr)
    tor_addr_copy(&ap->addr, addr);
  ap->port = port;
  return ap;
}

