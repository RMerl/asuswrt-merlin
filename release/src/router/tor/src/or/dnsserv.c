/* Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dnsserv.c \brief Implements client-side DNS proxy server code.  Note:
 * this is the DNS Server code, not the Server DNS code.  Confused?  This code
 * runs on client-side, and acts as a DNS server.  The code in dns.c, on the
 * other hand, runs on Tor servers, and acts as a DNS client.
 **/

#include "or.h"
#include "dnsserv.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "control.h"
#include "main.h"
#include "policies.h"
#ifdef HAVE_EVENT2_DNS_H
#include <event2/dns.h>
#include <event2/dns_compat.h>
/* XXXX this implies we want an improved evdns  */
#include <event2/dns_struct.h>
#else
#include "eventdns.h"
#endif

/** Helper function: called by evdns whenever the client sends a request to our
 * DNSPort.  We need to eventually answer the request <b>req</b>.
 */
static void
evdns_server_callback(struct evdns_server_request *req, void *data_)
{
  const listener_connection_t *listener = data_;
  entry_connection_t *entry_conn;
  edge_connection_t *conn;
  int i = 0;
  struct evdns_server_question *q = NULL, *supported_q = NULL;
  struct sockaddr_storage addr;
  struct sockaddr *sa;
  int addrlen;
  tor_addr_t tor_addr;
  uint16_t port;
  int err = DNS_ERR_NONE;
  char *q_name;

  tor_assert(req);

  log_info(LD_APP, "Got a new DNS request!");

  req->flags |= 0x80; /* set RA */

  /* First, check whether the requesting address matches our SOCKSPolicy. */
  if ((addrlen = evdns_server_request_get_requesting_addr(req,
                      (struct sockaddr*)&addr, (socklen_t)sizeof(addr))) < 0) {
    log_warn(LD_APP, "Couldn't get requesting address.");
    evdns_server_request_respond(req, DNS_ERR_SERVERFAILED);
    return;
  }
  (void) addrlen;
  sa = (struct sockaddr*) &addr;
  if (tor_addr_from_sockaddr(&tor_addr, sa, &port)<0) {
    log_warn(LD_APP, "Requesting address wasn't recognized.");
    evdns_server_request_respond(req, DNS_ERR_SERVERFAILED);
    return;
  }

  if (!socks_policy_permits_address(&tor_addr)) {
    log_warn(LD_APP, "Rejecting DNS request from disallowed IP.");
    evdns_server_request_respond(req, DNS_ERR_REFUSED);
    return;
  }

  /* Now, let's find the first actual question of a type we can answer in this
   * DNS request.  It makes us a little noncompliant to act like this; we
   * should fix that eventually if it turns out to make a difference for
   * anybody. */
  if (req->nquestions == 0) {
    log_info(LD_APP, "No questions in DNS request; sending back nil reply.");
    evdns_server_request_respond(req, 0);
    return;
  }
  if (req->nquestions > 1) {
    log_info(LD_APP, "Got a DNS request with more than one question; I only "
             "handle one question at a time for now.  Skipping the extras.");
  }
  for (i = 0; i < req->nquestions; ++i) {
    if (req->questions[i]->dns_question_class != EVDNS_CLASS_INET)
      continue;
    if (! q)
      q = req->questions[i];
    switch (req->questions[i]->type) {
      case EVDNS_TYPE_A:
      case EVDNS_TYPE_AAAA:
      case EVDNS_TYPE_PTR:
        /* We always pick the first one of these questions, if there is
           one. */
        if (! supported_q)
          supported_q = q;
        break;
      default:
        break;
      }
  }
  if (supported_q)
    q = supported_q;
  if (!q) {
    log_info(LD_APP, "None of the questions we got were ones we're willing "
             "to support. Sending NOTIMPL.");
    evdns_server_request_respond(req, DNS_ERR_NOTIMPL);
    return;
  }

  /* Make sure the name isn't too long: This should be impossible, I think. */
  if (err == DNS_ERR_NONE && strlen(q->name) > MAX_SOCKS_ADDR_LEN-1)
    err = DNS_ERR_FORMAT;

  if (err != DNS_ERR_NONE || !supported_q) {
    /* We got an error?  There's no question we're willing to answer? Then
     * send back an answer immediately; we're done. */
    evdns_server_request_respond(req, err);
    return;
  }

  /* Make a new dummy AP connection, and attach the request to it. */
  entry_conn = entry_connection_new(CONN_TYPE_AP, AF_INET);
  conn = ENTRY_TO_EDGE_CONN(entry_conn);
  TO_CONN(conn)->state = AP_CONN_STATE_RESOLVE_WAIT;
  conn->is_dns_request = 1;

  tor_addr_copy(&TO_CONN(conn)->addr, &tor_addr);
  TO_CONN(conn)->port = port;
  TO_CONN(conn)->address = tor_dup_addr(&tor_addr);

  if (q->type == EVDNS_TYPE_A || q->type == EVDNS_TYPE_AAAA ||
      q->type == EVDNS_QTYPE_ALL) {
    entry_conn->socks_request->command = SOCKS_COMMAND_RESOLVE;
  } else {
    tor_assert(q->type == EVDNS_TYPE_PTR);
    entry_conn->socks_request->command = SOCKS_COMMAND_RESOLVE_PTR;
  }

  if (q->type == EVDNS_TYPE_A || q->type == EVDNS_QTYPE_ALL) {
    entry_conn->entry_cfg.ipv4_traffic = 1;
    entry_conn->entry_cfg.ipv6_traffic = 0;
    entry_conn->entry_cfg.prefer_ipv6 = 0;
  } else if (q->type == EVDNS_TYPE_AAAA) {
    entry_conn->entry_cfg.ipv4_traffic = 0;
    entry_conn->entry_cfg.ipv6_traffic = 1;
    entry_conn->entry_cfg.prefer_ipv6 = 1;
  }

  strlcpy(entry_conn->socks_request->address, q->name,
          sizeof(entry_conn->socks_request->address));

  entry_conn->socks_request->listener_type = listener->base_.type;
  entry_conn->dns_server_request = req;
  entry_conn->entry_cfg.isolation_flags = listener->entry_cfg.isolation_flags;
  entry_conn->entry_cfg.session_group = listener->entry_cfg.session_group;
  entry_conn->nym_epoch = get_signewnym_epoch();

  if (connection_add(ENTRY_TO_CONN(entry_conn)) < 0) {
    log_warn(LD_APP, "Couldn't register dummy connection for DNS request");
    evdns_server_request_respond(req, DNS_ERR_SERVERFAILED);
    connection_free(ENTRY_TO_CONN(entry_conn));
    return;
  }

  control_event_stream_status(entry_conn, STREAM_EVENT_NEW_RESOLVE, 0);

  /* Now, unless a controller asked us to leave streams unattached,
  * throw the connection over to get rewritten (which will
  * answer it immediately if it's in the cache, or completely bogus, or
  * automapped), and then attached to a circuit. */
  log_info(LD_APP, "Passing request for %s to rewrite_and_attach.",
           escaped_safe_str_client(q->name));
  q_name = tor_strdup(q->name); /* q could be freed in rewrite_and_attach */
  connection_ap_rewrite_and_attach_if_allowed(entry_conn, NULL, NULL);
  /* Now, the connection is marked if it was bad. */

  log_info(LD_APP, "Passed request for %s to rewrite_and_attach_if_allowed.",
           escaped_safe_str_client(q_name));
  tor_free(q_name);
}

/** Helper function: called whenever the client sends a resolve request to our
 * controller.  We need to eventually answer the request <b>req</b>.
 * Returns 0 if the controller will be getting (or has gotten) an event in
 * response; -1 if we couldn't launch the request.
 */
int
dnsserv_launch_request(const char *name, int reverse,
                       control_connection_t *control_conn)
{
  entry_connection_t *entry_conn;
  edge_connection_t *conn;
  char *q_name;

  /* Make a new dummy AP connection, and attach the request to it. */
  entry_conn = entry_connection_new(CONN_TYPE_AP, AF_INET);
  conn = ENTRY_TO_EDGE_CONN(entry_conn);
  conn->base_.state = AP_CONN_STATE_RESOLVE_WAIT;

  tor_addr_copy(&TO_CONN(conn)->addr, &control_conn->base_.addr);
#ifdef AF_UNIX
  /*
   * The control connection can be AF_UNIX and if so tor_dup_addr will
   * unhelpfully say "<unknown address type>"; say "(Tor_internal)"
   * instead.
   */
  if (control_conn->base_.socket_family == AF_UNIX) {
    TO_CONN(conn)->port = 0;
    TO_CONN(conn)->address = tor_strdup("(Tor_internal)");
  } else {
    TO_CONN(conn)->port = control_conn->base_.port;
    TO_CONN(conn)->address = tor_dup_addr(&control_conn->base_.addr);
  }
#else
  TO_CONN(conn)->port = control_conn->base_.port;
  TO_CONN(conn)->address = tor_dup_addr(&control_conn->base_.addr);
#endif

  if (reverse)
    entry_conn->socks_request->command = SOCKS_COMMAND_RESOLVE_PTR;
  else
    entry_conn->socks_request->command = SOCKS_COMMAND_RESOLVE;

  conn->is_dns_request = 1;

  strlcpy(entry_conn->socks_request->address, name,
          sizeof(entry_conn->socks_request->address));

  entry_conn->socks_request->listener_type = CONN_TYPE_CONTROL_LISTENER;
  entry_conn->original_dest_address = tor_strdup(name);
  entry_conn->entry_cfg.session_group = SESSION_GROUP_CONTROL_RESOLVE;
  entry_conn->nym_epoch = get_signewnym_epoch();
  entry_conn->entry_cfg.isolation_flags = ISO_DEFAULT;

  if (connection_add(TO_CONN(conn))<0) {
    log_warn(LD_APP, "Couldn't register dummy connection for RESOLVE request");
    connection_free(TO_CONN(conn));
    return -1;
  }

  control_event_stream_status(entry_conn, STREAM_EVENT_NEW_RESOLVE, 0);

  /* Now, unless a controller asked us to leave streams unattached,
  * throw the connection over to get rewritten (which will
  * answer it immediately if it's in the cache, or completely bogus, or
  * automapped), and then attached to a circuit. */
  log_info(LD_APP, "Passing request for %s to rewrite_and_attach.",
           escaped_safe_str_client(name));
  q_name = tor_strdup(name); /* q could be freed in rewrite_and_attach */
  connection_ap_rewrite_and_attach_if_allowed(entry_conn, NULL, NULL);
  /* Now, the connection is marked if it was bad. */

  log_info(LD_APP, "Passed request for %s to rewrite_and_attach_if_allowed.",
           escaped_safe_str_client(q_name));
  tor_free(q_name);
  return 0;
}

/** If there is a pending request on <b>conn</b> that's waiting for an answer,
 * send back an error and free the request. */
void
dnsserv_reject_request(entry_connection_t *conn)
{
  if (conn->dns_server_request) {
    evdns_server_request_respond(conn->dns_server_request,
                                 DNS_ERR_SERVERFAILED);
    conn->dns_server_request = NULL;
  }
}

/** Look up the original name that corresponds to 'addr' in req.  We use this
 * to preserve case in order to facilitate people using 0x20-hacks to avoid
 * DNS poisoning. */
static const char *
evdns_get_orig_address(const struct evdns_server_request *req,
                       int rtype, const char *addr)
{
  int i, type;

  switch (rtype) {
  case RESOLVED_TYPE_IPV4:
    type = EVDNS_TYPE_A;
    break;
  case RESOLVED_TYPE_HOSTNAME:
    type = EVDNS_TYPE_PTR;
    break;
  case RESOLVED_TYPE_IPV6:
    type = EVDNS_TYPE_AAAA;
    break;
  default:
    tor_fragile_assert();
    return addr;
  }

  for (i = 0; i < req->nquestions; ++i) {
    const struct evdns_server_question *q = req->questions[i];
    if (q->type == type && !strcasecmp(q->name, addr))
      return q->name;
  }
  return addr;
}

/** Tell the dns request waiting for an answer on <b>conn</b> that we have an
 * answer of type <b>answer_type</b> (RESOLVE_TYPE_IPV4/IPV6/ERR), of length
 * <b>answer_len</b>, in <b>answer</b>, with TTL <b>ttl</b>.  Doesn't do
 * any caching; that's handled elsewhere. */
void
dnsserv_resolved(entry_connection_t *conn,
                 int answer_type,
                 size_t answer_len,
                 const char *answer,
                 int ttl)
{
  struct evdns_server_request *req = conn->dns_server_request;
  const char *name;
  int err = DNS_ERR_NONE;
  if (!req)
    return;
  name = evdns_get_orig_address(req, answer_type,
                                conn->socks_request->address);

  /* XXXX Re-do; this is dumb. */
  if (ttl < 60)
    ttl = 60;

  /* The evdns interface is: add a bunch of reply items (corresponding to one
   * or more of the questions in the request); then, call
   * evdns_server_request_respond. */
  if (answer_type == RESOLVED_TYPE_IPV6) {
    evdns_server_request_add_aaaa_reply(req,
                                        name,
                                        1, answer, ttl);
  } else if (answer_type == RESOLVED_TYPE_IPV4 && answer_len == 4 &&
             conn->socks_request->command == SOCKS_COMMAND_RESOLVE) {
    evdns_server_request_add_a_reply(req,
                                     name,
                                     1, answer, ttl);
  } else if (answer_type == RESOLVED_TYPE_HOSTNAME &&
             answer_len < 256 &&
             conn->socks_request->command == SOCKS_COMMAND_RESOLVE_PTR) {
    char *ans = tor_strndup(answer, answer_len);
    evdns_server_request_add_ptr_reply(req, NULL,
                                       name,
                                       ans, ttl);
    tor_free(ans);
  } else if (answer_type == RESOLVED_TYPE_ERROR) {
    err = DNS_ERR_NOTEXIST;
  } else { /* answer_type == RESOLVED_TYPE_ERROR_TRANSIENT */
    err = DNS_ERR_SERVERFAILED;
  }

  evdns_server_request_respond(req, err);

  conn->dns_server_request = NULL;
}

/** Set up the evdns server port for the UDP socket on <b>conn</b>, which
 * must be an AP_DNS_LISTENER */
void
dnsserv_configure_listener(connection_t *conn)
{
  listener_connection_t *listener_conn;
  tor_assert(conn);
  tor_assert(SOCKET_OK(conn->s));
  tor_assert(conn->type == CONN_TYPE_AP_DNS_LISTENER);

  listener_conn = TO_LISTENER_CONN(conn);
  listener_conn->dns_server_port =
    tor_evdns_add_server_port(conn->s, 0, evdns_server_callback,
                              listener_conn);
}

/** Free the evdns server port for <b>conn</b>, which must be an
 * AP_DNS_LISTENER. */
void
dnsserv_close_listener(connection_t *conn)
{
  listener_connection_t *listener_conn;
  tor_assert(conn);
  tor_assert(conn->type == CONN_TYPE_AP_DNS_LISTENER);

  listener_conn = TO_LISTENER_CONN(conn);

  if (listener_conn->dns_server_port) {
    evdns_close_server_port(listener_conn->dns_server_port);
    listener_conn->dns_server_port = NULL;
  }
}

