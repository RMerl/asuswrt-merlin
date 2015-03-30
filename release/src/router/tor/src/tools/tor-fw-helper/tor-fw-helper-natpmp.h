/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-fw-helper-natpmp.h
  **/

#ifdef NAT_PMP
#ifndef TOR_TOR_FW_HELPER_NATPMP_H
#define TOR_TOR_FW_HELPER_NATPMP_H

#include <natpmp.h>

/** This is the default NAT-PMP lease time in seconds. */
#define NATPMP_DEFAULT_LEASE 3600
/** NAT-PMP has many codes for success; this is one of them. */
#define NATPMP_SUCCESS 0

/** This is our NAT-PMP meta structure - it holds our request data, responses,
 * various NAT-PMP parameters, and of course the status of the motion in the
 * NAT-PMP ocean. */
typedef struct natpmp_state_t {
  natpmp_t natpmp;
  natpmpresp_t response;
  int fetch_public_ip;
  int status;
  int init; /**< Have we been initialized? */
  int protocol; /**< This will only be TCP. */
  int lease;
} natpmp_state_t;

const tor_fw_backend_t *tor_fw_get_natpmp_backend(void);

int tor_natpmp_init(tor_fw_options_t *tor_fw_options, void *backend_state);

int tor_natpmp_cleanup(tor_fw_options_t *tor_fw_options, void *backend_state);

int tor_natpmp_add_tcp_mapping(uint16_t internal_port, uint16_t external_port,
                               int is_verbose, void *backend_state);

int tor_natpmp_fetch_public_ip(tor_fw_options_t *tor_fw_options,
                               void *backend_state);

#endif
#endif

