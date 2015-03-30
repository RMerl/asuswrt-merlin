/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-fw-helper-natpmp.c
  * \brief The implementation of our NAT-PMP firewall helper.
  **/

#include "orconfig.h"
#ifdef NAT_PMP
#ifdef _WIN32
#define STATICLIB
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

// debugging stuff
#include <assert.h>

#include "compat.h"

#include "tor-fw-helper.h"
#include "tor-fw-helper-natpmp.h"

/** This hooks NAT-PMP into our multi-backend API. */
static tor_fw_backend_t tor_natpmp_backend = {
  "natpmp",
  sizeof(struct natpmp_state_t),
  tor_natpmp_init,
  tor_natpmp_cleanup,
  tor_natpmp_fetch_public_ip,
  tor_natpmp_add_tcp_mapping
};

/** Return the backend for NAT-PMP. */
const tor_fw_backend_t *
tor_fw_get_natpmp_backend(void)
{
  return &tor_natpmp_backend;
}

/** Initialize the NAT-PMP backend and store the results in
 * <b>backend_state</b>.*/
int
tor_natpmp_init(tor_fw_options_t *tor_fw_options, void *backend_state)
{
  natpmp_state_t *state = (natpmp_state_t *) backend_state;
  int r = 0;

  memset(&(state->natpmp), 0, sizeof(natpmp_t));
  memset(&(state->response), 0, sizeof(natpmpresp_t));
  state->init = 0;
  state->protocol = NATPMP_PROTOCOL_TCP;
  state->lease = NATPMP_DEFAULT_LEASE;

  if (tor_fw_options->verbose)
    fprintf(stderr, "V: natpmp init...\n");

  r = initnatpmp(&(state->natpmp), 0, 0);
  if (r == 0) {
    state->init = 1;
    fprintf(stderr, "V: natpmp initialized...\n");
    return r;
  } else {
    fprintf(stderr, "V: natpmp failed to initialize...\n");
    return r;
  }
}

/** Tear down the NAT-PMP connection stored in <b>backend_state</b>.*/
int
tor_natpmp_cleanup(tor_fw_options_t *tor_fw_options, void *backend_state)
{
  natpmp_state_t *state = (natpmp_state_t *) backend_state;
  int r = 0;
  if (tor_fw_options->verbose)
    fprintf(stderr, "V: natpmp cleanup...\n");
  r = closenatpmp(&(state->natpmp));
  if (tor_fw_options->verbose)
    fprintf(stderr, "V: closing natpmp socket: %d\n", r);
  return r;
}

/** Use select() to wait until we can read on fd. */
static int
wait_until_fd_readable(tor_socket_t fd, struct timeval *timeout)
{
  int r;
  fd_set fds;

#ifndef WIN32
  if (fd >= FD_SETSIZE) {
    fprintf(stderr, "E: NAT-PMP FD_SETSIZE error %d\n", fd);
    return -1;
  }
#endif

  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  r = select(fd+1, &fds, NULL, NULL, timeout);
  if (r == -1) {
    fprintf(stderr, "V: select failed in wait_until_fd_readable: %s\n",
            tor_socket_strerror(tor_socket_errno(fd)));
    return -1;
  }
  /* XXXX we should really check to see whether fd was readable, or we timed
     out. */
  return 0;
}

int
tor_natpmp_add_tcp_mapping(uint16_t internal_port, uint16_t external_port,
                           int is_verbose, void *backend_state)
{
  int r = 0;
  int x = 0;
  int sav_errno;
  natpmp_state_t *state = (natpmp_state_t *) backend_state;

  struct timeval timeout;

  if (is_verbose)
    fprintf(stderr, "V: sending natpmp portmapping request...\n");
  r = sendnewportmappingrequest(&(state->natpmp), state->protocol,
                                internal_port,
                                external_port,
                                state->lease);
  if (is_verbose)
    fprintf(stderr, "tor-fw-helper: NAT-PMP sendnewportmappingrequest "
            "returned %d (%s)\n", r, r==12?"SUCCESS":"FAILED");

  do {
    getnatpmprequesttimeout(&(state->natpmp), &timeout);
    x = wait_until_fd_readable(state->natpmp.s, &timeout);
    if (x == -1)
      return -1;

    if (is_verbose)
      fprintf(stderr, "V: attempting to readnatpmpreponseorretry...\n");
    r = readnatpmpresponseorretry(&(state->natpmp), &(state->response));
    sav_errno = tor_socket_errno(state->natpmp.s);

    if (r<0 && r!=NATPMP_TRYAGAIN) {
      fprintf(stderr, "E: readnatpmpresponseorretry failed %d\n", r);
      fprintf(stderr, "E: errno=%d '%s'\n", sav_errno,
              tor_socket_strerror(sav_errno));
    }

  } while (r == NATPMP_TRYAGAIN);

  if (r != 0) {
    /* XXX TODO: NATPMP_* should be formatted into useful error strings */
    fprintf(stderr, "E: NAT-PMP It appears that something went wrong:"
            " %d\n", r);
    if (r == -51)
      fprintf(stderr, "E: NAT-PMP It appears that the request was "
              "unauthorized\n");
    return r;
  }

  if (r == NATPMP_SUCCESS) {
    fprintf(stderr, "tor-fw-helper: NAT-PMP mapped public port %hu to"
            " localport %hu liftime %u\n",
            (state->response).pnu.newportmapping.mappedpublicport,
            (state->response).pnu.newportmapping.privateport,
            (state->response).pnu.newportmapping.lifetime);
  }

  return (r == NATPMP_SUCCESS) ? 0 : -1;
}

/** Fetch our likely public IP from our upstream NAT-PMP enabled NAT device.
 * Use the connection context stored in <b>backend_state</b>. */
int
tor_natpmp_fetch_public_ip(tor_fw_options_t *tor_fw_options,
                           void *backend_state)
{
  int r = 0;
  int x = 0;
  int sav_errno;
  natpmp_state_t *state = (natpmp_state_t *) backend_state;

  struct timeval timeout;

  r = sendpublicaddressrequest(&(state->natpmp));
  fprintf(stderr, "tor-fw-helper: NAT-PMP sendpublicaddressrequest returned"
          " %d (%s)\n", r, r==2?"SUCCESS":"FAILED");

  do {
    getnatpmprequesttimeout(&(state->natpmp), &timeout);

    x = wait_until_fd_readable(state->natpmp.s, &timeout);
    if (x == -1)
      return -1;

    if (tor_fw_options->verbose)
      fprintf(stderr, "V: NAT-PMP attempting to read reponse...\n");
    r = readnatpmpresponseorretry(&(state->natpmp), &(state->response));
    sav_errno = tor_socket_errno(state->natpmp.s);

    if (tor_fw_options->verbose)
      fprintf(stderr, "V: NAT-PMP readnatpmpresponseorretry returned"
              " %d\n", r);

    if ( r < 0 && r != NATPMP_TRYAGAIN) {
      fprintf(stderr, "E: NAT-PMP readnatpmpresponseorretry failed %d\n",
              r);
      fprintf(stderr, "E: NAT-PMP errno=%d '%s'\n", sav_errno,
              tor_socket_strerror(sav_errno));
    }

  } while (r == NATPMP_TRYAGAIN );

  if (r != 0) {
    fprintf(stderr, "E: NAT-PMP It appears that something went wrong:"
            " %d\n", r);
    return r;
  }

  fprintf(stderr, "tor-fw-helper: ExternalIPAddress = %s\n",
          inet_ntoa((state->response).pnu.publicaddress.addr));
  tor_fw_options->public_ip_status = 1;

  if (tor_fw_options->verbose) {
    fprintf(stderr, "V: result = %u\n", r);
    fprintf(stderr, "V: type = %u\n", (state->response).type);
    fprintf(stderr, "V: resultcode = %u\n", (state->response).resultcode);
    fprintf(stderr, "V: epoch = %u\n", (state->response).epoch);
  }

  return r;
}
#endif

