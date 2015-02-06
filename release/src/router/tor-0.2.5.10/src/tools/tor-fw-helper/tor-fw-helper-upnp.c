/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-fw-helper-upnp.c
  * \brief The implementation of our UPnP firewall helper.
  **/

#include "orconfig.h"
#ifdef MINIUPNPC
#ifdef _WIN32
#define STATICLIB
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#include "compat.h"
#include "tor-fw-helper.h"
#include "tor-fw-helper-upnp.h"

/** UPnP timeout value. */
#define UPNP_DISCOVER_TIMEOUT 2000
/** Description of the port mapping in the UPnP table. */
#define UPNP_DESC "Tor relay"

/* XXX TODO: We should print these as a useful user string when we return the
 * number to a user */
/** Magic numbers as miniupnpc return codes. */
#define UPNP_ERR_SUCCESS 0
#define UPNP_ERR_NODEVICESFOUND 1
#define UPNP_ERR_NOIGDFOUND 2
#define UPNP_ERR_ADDPORTMAPPING 3
#define UPNP_ERR_GETPORTMAPPING 4
#define UPNP_ERR_DELPORTMAPPING 5
#define UPNP_ERR_GETEXTERNALIP 6
#define UPNP_ERR_INVAL 7
#define UPNP_ERR_OTHER 8
#define UPNP_SUCCESS 1

/** This hooks miniupnpc into our multi-backend API. */
static tor_fw_backend_t tor_miniupnp_backend = {
  "miniupnp",
  sizeof(struct miniupnpc_state_t),
  tor_upnp_init,
  tor_upnp_cleanup,
  tor_upnp_fetch_public_ip,
  tor_upnp_add_tcp_mapping
};

/** Return the backend for miniupnp. */
const tor_fw_backend_t *
tor_fw_get_miniupnp_backend(void)
{
  return &tor_miniupnp_backend;
}

/** Initialize the UPnP backend and store the results in
 * <b>backend_state</b>.*/
int
tor_upnp_init(tor_fw_options_t *options, void *backend_state)
{
  /*
    This leaks the user agent from the client to the router - perhaps we don't
    want to do that? eg:

    User-Agent: Ubuntu/10.04, UPnP/1.0, MiniUPnPc/1.4

  */
  miniupnpc_state_t *state = (miniupnpc_state_t *) backend_state;
  struct UPNPDev *devlist;
  int r;

  memset(&(state->urls), 0, sizeof(struct UPNPUrls));
  memset(&(state->data), 0, sizeof(struct IGDdatas));
  state->init = 0;

#ifdef MINIUPNPC15
  devlist = upnpDiscover(UPNP_DISCOVER_TIMEOUT, NULL, NULL, 0);
#else
  devlist = upnpDiscover(UPNP_DISCOVER_TIMEOUT, NULL, NULL, 0, 0, NULL);
#endif
  if (NULL == devlist) {
    fprintf(stderr, "E: upnpDiscover returned: NULL\n");
    return UPNP_ERR_NODEVICESFOUND;
  }

  assert(options);
  r = UPNP_GetValidIGD(devlist, &(state->urls), &(state->data),
                       state->lanaddr, UPNP_LANADDR_SZ);
  fprintf(stderr, "tor-fw-helper: UPnP GetValidIGD returned: %d (%s)\n", r,
          r==UPNP_SUCCESS?"SUCCESS":"FAILED");

  freeUPNPDevlist(devlist);

  if (r != 1 && r != 2)
    return UPNP_ERR_NOIGDFOUND;

  state->init = 1;
  return UPNP_ERR_SUCCESS;
}

/** Tear down the UPnP connection stored in <b>backend_state</b>.*/
int
tor_upnp_cleanup(tor_fw_options_t *options, void *backend_state)
{

  miniupnpc_state_t *state = (miniupnpc_state_t *) backend_state;
  assert(options);

  if (state->init)
    FreeUPNPUrls(&(state->urls));
  state->init = 0;

  return UPNP_ERR_SUCCESS;
}

/** Fetch our likely public IP from our upstream UPnP IGD enabled NAT device.
 * Use the connection context stored in <b>backend_state</b>. */
int
tor_upnp_fetch_public_ip(tor_fw_options_t *options, void *backend_state)
{
  miniupnpc_state_t *state = (miniupnpc_state_t *) backend_state;
  int r;
  char externalIPAddress[16];

  if (!state->init) {
    r = tor_upnp_init(options, state);
    if (r != UPNP_ERR_SUCCESS)
      return r;
  }

  r = UPNP_GetExternalIPAddress(state->urls.controlURL,
                                state->data.first.servicetype,
                                externalIPAddress);

  if (r != UPNPCOMMAND_SUCCESS)
    goto err;

  if (externalIPAddress[0]) {
    fprintf(stderr, "tor-fw-helper: ExternalIPAddress = %s\n",
            externalIPAddress); tor_upnp_cleanup(options, state);
    options->public_ip_status = 1;
    return UPNP_ERR_SUCCESS;
  } else {
    goto err;
  }

 err:
  tor_upnp_cleanup(options, state);
  return UPNP_ERR_GETEXTERNALIP;
}

int
tor_upnp_add_tcp_mapping(uint16_t internal_port, uint16_t external_port,
                         int is_verbose, void *backend_state)
{
  int retval;
  char internal_port_str[6];
  char external_port_str[6];
  miniupnpc_state_t *state = (miniupnpc_state_t *) backend_state;

  if (!state->init) {
    fprintf(stderr, "E: %s but state is not initialized.\n", __func__);
    return -1;
  }

  if (is_verbose)
    fprintf(stderr, "V: UPnP: internal port: %u, external port: %u\n",
            internal_port, external_port);

  tor_snprintf(internal_port_str, sizeof(internal_port_str),
               "%u", internal_port);
  tor_snprintf(external_port_str, sizeof(external_port_str),
               "%u", external_port);

  retval = UPNP_AddPortMapping(state->urls.controlURL,
                               state->data.first.servicetype,
                               external_port_str, internal_port_str,
#ifdef MINIUPNPC15
                               state->lanaddr, UPNP_DESC, "TCP", 0);
#else
                               state->lanaddr, UPNP_DESC, "TCP", 0, 0);
#endif

  return (retval == UPNP_ERR_SUCCESS) ? 0 : -1;
}

#endif

