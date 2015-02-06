/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
 * Copyright (c) 2007-2013, The Tor Project, Inc.
 */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "compat.h"
#include "../common/util.h"
#include "address.h"
#include "../common/torlog.h"
#include "sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* Must be included before sys/stat.h for Ultrix */
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef _WIN32
#if defined(_MSC_VER) && (_MSC_VER <= 1300)
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif

#define RESPONSE_LEN_4 8
#define log_sock_error(act, _s)                                         \
  STMT_BEGIN log_fn(LOG_ERR, LD_NET, "Error while %s: %s", act,         \
              tor_socket_strerror(tor_socket_errno(_s))); STMT_END

static void usage(void) ATTR_NORETURN;

/** Set *<b>out</b> to a newly allocated SOCKS4a resolve request with
 * <b>username</b> and <b>hostname</b> as provided.  Return the number
 * of bytes in the request. */
static ssize_t
build_socks_resolve_request(char **out,
                            const char *username,
                            const char *hostname,
                            int reverse,
                            int version)
{
  size_t len = 0;
  tor_assert(out);
  tor_assert(username);
  tor_assert(hostname);

  if (version == 4) {
    len = 8 + strlen(username) + 1 + strlen(hostname) + 1;
    *out = tor_malloc(len);
    (*out)[0] = 4;      /* SOCKS version 4 */
    (*out)[1] = '\xF0'; /* Command: resolve. */
    set_uint16((*out)+2, htons(0)); /* port: 0. */
    set_uint32((*out)+4, htonl(0x00000001u)); /* addr: 0.0.0.1 */
    memcpy((*out)+8, username, strlen(username)+1);
    memcpy((*out)+8+strlen(username)+1, hostname, strlen(hostname)+1);
  } else if (version == 5) {
    int is_ip_address;
    tor_addr_t addr;
    size_t addrlen;
    int ipv6;
    is_ip_address = tor_addr_parse(&addr, hostname) != -1;
    if (!is_ip_address && reverse) {
      log_err(LD_GENERAL, "Tried to do a reverse lookup on a non-IP!");
      return -1;
    }
    ipv6 = reverse && tor_addr_family(&addr) == AF_INET6;
    addrlen = reverse ? (ipv6 ? 16 : 4) : 1 + strlen(hostname);
    len = 6 + addrlen;
    *out = tor_malloc(len);
    (*out)[0] = 5; /* SOCKS version 5 */
    (*out)[1] = reverse ? '\xF1' : '\xF0'; /* RESOLVE_PTR or RESOLVE */
    (*out)[2] = 0; /* reserved. */
    if (reverse) {
      (*out)[3] = ipv6 ? 4 : 1;
      if (ipv6)
        memcpy((*out)+4, tor_addr_to_in6_addr8(&addr), 16);
      else
        set_uint32((*out)+4, tor_addr_to_ipv4n(&addr));
    } else {
      (*out)[3] = 3;
      (*out)[4] = (char)(uint8_t)(addrlen - 1);
      memcpy((*out)+5, hostname, addrlen - 1);
    }
    set_uint16((*out)+4+addrlen, 0); /* port */
  } else {
    tor_assert(0);
  }

  return len;
}

/** Given a <b>len</b>-byte SOCKS4a response in <b>response</b>, set
 * *<b>addr_out</b> to the address it contains (in host order).
 * Return 0 on success, -1 on error.
 */
static int
parse_socks4a_resolve_response(const char *hostname,
                               const char *response, size_t len,
                               tor_addr_t *addr_out)
{
  uint8_t status;
  tor_assert(response);
  tor_assert(addr_out);

  if (len < RESPONSE_LEN_4) {
    log_warn(LD_PROTOCOL,"Truncated socks response.");
    return -1;
  }
  if (((uint8_t)response[0])!=0) { /* version: 0 */
    log_warn(LD_PROTOCOL,"Nonzero version in socks response: bad format.");
    return -1;
  }
  status = (uint8_t)response[1];
  if (get_uint16(response+2)!=0) { /* port: 0 */
    log_warn(LD_PROTOCOL,"Nonzero port in socks response: bad format.");
    return -1;
  }
  if (status != 90) {
    log_warn(LD_NET,"Got status response '%d': socks request failed.", status);
    if (!strcasecmpend(hostname, ".onion")) {
      log_warn(LD_NET,
        "%s is a hidden service; those don't have IP addresses. "
        "To connect to a hidden service, you need to send the hostname "
        "to Tor; we suggest an application that uses SOCKS 4a.",hostname);
      return -1;
    }
    return -1;
  }

  tor_addr_from_ipv4n(addr_out, get_uint32(response+4));
  return 0;
}

/* It would be nice to let someone know what SOCKS5 issue a user may have */
static const char *
socks5_reason_to_string(char reason)
{
  switch (reason) {
    case SOCKS5_SUCCEEDED:
      return "succeeded";
    case SOCKS5_GENERAL_ERROR:
      return "general error";
    case SOCKS5_NOT_ALLOWED:
      return "not allowed";
    case SOCKS5_NET_UNREACHABLE:
      return "network is unreachable";
    case SOCKS5_HOST_UNREACHABLE:
      return "host is unreachable";
    case SOCKS5_CONNECTION_REFUSED:
      return "connection refused";
    case SOCKS5_TTL_EXPIRED:
      return "ttl expired";
    case SOCKS5_COMMAND_NOT_SUPPORTED:
      return "command not supported";
    case SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED:
      return "address type not supported";
    default:
      return "unknown SOCKS5 code";
  }
}

/** Send a resolve request for <b>hostname</b> to the Tor listening on
 * <b>sockshost</b>:<b>socksport</b>.  Store the resulting IPv4
 * address (in host order) into *<b>result_addr</b>.
 */
static int
do_resolve(const char *hostname, uint32_t sockshost, uint16_t socksport,
           int reverse, int version,
           tor_addr_t *result_addr, char **result_hostname)
{
  int s = -1;
  struct sockaddr_in socksaddr;
  char *req = NULL;
  ssize_t len = 0;

  tor_assert(hostname);
  tor_assert(result_addr);
  tor_assert(version == 4 || version == 5);

  tor_addr_make_unspec(result_addr);
  *result_hostname = NULL;

  s = tor_open_socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (s<0) {
    log_sock_error("creating_socket", -1);
    return -1;
  }

  memset(&socksaddr, 0, sizeof(socksaddr));
  socksaddr.sin_family = AF_INET;
  socksaddr.sin_port = htons(socksport);
  socksaddr.sin_addr.s_addr = htonl(sockshost);
  if (connect(s, (struct sockaddr*)&socksaddr, sizeof(socksaddr))) {
    log_sock_error("connecting to SOCKS host", s);
    goto err;
  }

  if (version == 5) {
    char method_buf[2];
    if (write_all(s, "\x05\x01\x00", 3, 1) != 3) {
      log_err(LD_NET, "Error sending SOCKS5 method list.");
      goto err;
    }
    if (read_all(s, method_buf, 2, 1) != 2) {
      log_err(LD_NET, "Error reading SOCKS5 methods.");
      goto err;
    }
    if (method_buf[0] != '\x05') {
      log_err(LD_NET, "Unrecognized socks version: %u",
              (unsigned)method_buf[0]);
      goto err;
    }
    if (method_buf[1] != '\x00') {
      log_err(LD_NET, "Unrecognized socks authentication method: %u",
              (unsigned)method_buf[1]);
      goto err;
    }
  }

  if ((len = build_socks_resolve_request(&req, "", hostname, reverse,
                                         version))<0) {
    log_err(LD_BUG,"Error generating SOCKS request");
    tor_assert(!req);
    goto err;
  }
  if (write_all(s, req, len, 1) != len) {
    log_sock_error("sending SOCKS request", s);
    tor_free(req);
    goto err;
  }
  tor_free(req);

  if (version == 4) {
    char reply_buf[RESPONSE_LEN_4];
    if (read_all(s, reply_buf, RESPONSE_LEN_4, 1) != RESPONSE_LEN_4) {
      log_err(LD_NET, "Error reading SOCKS4 response.");
      goto err;
    }
    if (parse_socks4a_resolve_response(hostname,
                                       reply_buf, RESPONSE_LEN_4,
                                       result_addr)<0) {
      goto err;
    }
  } else {
    char reply_buf[16];
    if (read_all(s, reply_buf, 4, 1) != 4) {
      log_err(LD_NET, "Error reading SOCKS5 response.");
      goto err;
    }
    if (reply_buf[0] != 5) {
      log_err(LD_NET, "Bad SOCKS5 reply version.");
      goto err;
    }
    /* Give a user some useful feedback about SOCKS5 errors */
    if (reply_buf[1] != 0) {
      log_warn(LD_NET,"Got SOCKS5 status response '%u': %s",
               (unsigned)reply_buf[1],
               socks5_reason_to_string(reply_buf[1]));
      if (reply_buf[1] == 4 && !strcasecmpend(hostname, ".onion")) {
        log_warn(LD_NET,
            "%s is a hidden service; those don't have IP addresses. "
            "To connect to a hidden service, you need to send the hostname "
            "to Tor; we suggest an application that uses SOCKS 4a.",
            hostname);
      }
      goto err;
    }
    if (reply_buf[3] == 1) {
      /* IPv4 address */
      if (read_all(s, reply_buf, 4, 1) != 4) {
        log_err(LD_NET, "Error reading address in socks5 response.");
        goto err;
      }
      tor_addr_from_ipv4n(result_addr, get_uint32(reply_buf));
    } else if (reply_buf[3] == 4) {
      /* IPv6 address */
      if (read_all(s, reply_buf, 16, 1) != 16) {
        log_err(LD_NET, "Error reading address in socks5 response.");
        goto err;
      }
      tor_addr_from_ipv6_bytes(result_addr, reply_buf);
    } else if (reply_buf[3] == 3) {
      /* Domain name */
      size_t result_len;
      if (read_all(s, reply_buf, 1, 1) != 1) {
        log_err(LD_NET, "Error reading address_length in socks5 response.");
        goto err;
      }
      result_len = *(uint8_t*)(reply_buf);
      *result_hostname = tor_malloc(result_len+1);
      if (read_all(s, *result_hostname, result_len, 1) != (int) result_len) {
        log_err(LD_NET, "Error reading hostname in socks5 response.");
        goto err;
      }
      (*result_hostname)[result_len] = '\0';
    }
  }

  tor_close_socket(s);
  return 0;
 err:
  tor_close_socket(s);
  return -1;
}

/** Print a usage message and exit. */
static void
usage(void)
{
  puts("Syntax: tor-resolve [-4] [-v] [-x] [-F] [-p port] "
       "hostname [sockshost:socksport]");
  exit(1);
}

/** Entry point to tor-resolve */
int
main(int argc, char **argv)
{
  uint32_t sockshost;
  uint16_t socksport = 0, port_option = 0;
  int isSocks4 = 0, isVerbose = 0, isReverse = 0;
  char **arg;
  int n_args;
  tor_addr_t result;
  char *result_hostname = NULL;
  log_severity_list_t *s = tor_malloc_zero(sizeof(log_severity_list_t));

  init_logging();
  sandbox_disable_getaddrinfo_cache();

  arg = &argv[1];
  n_args = argc-1;

  if (!n_args)
    usage();

  if (!strcmp(arg[0],"--version")) {
    printf("Tor version %s.\n",VERSION);
    return 0;
  }
  while (n_args && *arg[0] == '-') {
    if (!strcmp("-v", arg[0]))
      isVerbose = 1;
    else if (!strcmp("-4", arg[0]))
      isSocks4 = 1;
    else if (!strcmp("-5", arg[0]))
      isSocks4 = 0;
    else if (!strcmp("-x", arg[0]))
      isReverse = 1;
    else if (!strcmp("-p", arg[0])) {
      int p;
      if (n_args < 2) {
        fprintf(stderr, "No arguments given to -p\n");
        usage();
      }
      p = atoi(arg[1]);
      if (p<1 || p > 65535) {
        fprintf(stderr, "-p requires a number between 1 and 65535\n");
        usage();
      }
      port_option = (uint16_t) p;
      ++arg; /* skip the port */
      --n_args;
    } else {
      fprintf(stderr, "Unrecognized flag '%s'\n", arg[0]);
      usage();
    }
    ++arg;
    --n_args;
  }

  if (isSocks4 && isReverse) {
    fprintf(stderr, "Reverse lookups not supported with SOCKS4a\n");
    usage();
  }

  if (isVerbose)
    set_log_severity_config(LOG_DEBUG, LOG_ERR, s);
  else
    set_log_severity_config(LOG_WARN, LOG_ERR, s);
  add_stream_log(s, "<stderr>", fileno(stderr));

  if (n_args == 1) {
    log_debug(LD_CONFIG, "defaulting to localhost");
    sockshost = 0x7f000001u; /* localhost */
    if (port_option) {
      log_debug(LD_CONFIG, "Using port %d", (int)port_option);
      socksport = port_option;
    } else {
      log_debug(LD_CONFIG, "defaulting to port 9050");
      socksport = 9050; /* 9050 */
    }
  } else if (n_args == 2) {
    if (addr_port_lookup(LOG_WARN, arg[1], NULL, &sockshost, &socksport)<0) {
      fprintf(stderr, "Couldn't parse/resolve address %s", arg[1]);
      return 1;
    }
    if (socksport && port_option && socksport != port_option) {
      log_warn(LD_CONFIG, "Conflicting ports; using %d, not %d",
               (int)socksport, (int)port_option);
    } else if (port_option) {
      socksport = port_option;
    } else if (!socksport) {
      log_debug(LD_CONFIG, "defaulting to port 9050");
      socksport = 9050;
    }
  } else {
    usage();
  }

  if (network_init()<0) {
    log_err(LD_BUG,"Error initializing network; exiting.");
    return 1;
  }

  if (do_resolve(arg[0], sockshost, socksport, isReverse,
                 isSocks4 ? 4 : 5, &result,
                 &result_hostname))
    return 1;

  if (result_hostname) {
    printf("%s\n", result_hostname);
  } else {
    printf("%s\n", fmt_addr(&result));
  }
  return 0;
}

