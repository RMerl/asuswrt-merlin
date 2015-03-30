/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-fw-helper-upnp.h
  * \brief The main header for our firewall helper.
  **/

#ifdef MINIUPNPC
#ifndef TOR_TOR_FW_HELPER_UPNP_H
#define TOR_TOR_FW_HELPER_UPNP_H

#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

/** This is a magic number for miniupnpc lan address size. */
#define UPNP_LANADDR_SZ 64

/** This is our miniupnpc meta structure - it holds our request data,
 * responses, and various miniupnpc parameters. */
typedef struct miniupnpc_state_t {
  struct UPNPUrls urls;
  struct IGDdatas data;
  char lanaddr[UPNP_LANADDR_SZ];
  int init;
} miniupnpc_state_t;

const tor_fw_backend_t *tor_fw_get_miniupnp_backend(void);

int tor_upnp_init(tor_fw_options_t *options, void *backend_state);

int tor_upnp_cleanup(tor_fw_options_t *options, void *backend_state);

int tor_upnp_fetch_public_ip(tor_fw_options_t *options, void *backend_state);

int tor_upnp_add_tcp_mapping(uint16_t internal_port, uint16_t external_port,
                             int is_verbose, void *backend_state);

#endif
#endif

