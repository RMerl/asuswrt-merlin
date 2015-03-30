/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-fw-helper.h
  * \brief The main header for our firewall helper.
  **/

#ifndef TOR_TOR_FW_HELPER_H
#define TOR_TOR_FW_HELPER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

/** The current version of tor-fw-helper. */
#define tor_fw_version "0.2"

/** This is an arbitrary hard limit - We currently have two (NAT-PMP and UPnP).
 We're likely going to add the Intel UPnP library but nothing else comes to
 mind at the moment. */
#define MAX_BACKENDS 23

/** Forward traffic received in port <b>external_port</b> in the
 *  external side of our NAT to <b>internal_port</b> in this host. */
typedef struct {
  uint16_t external_port;
  uint16_t internal_port;
} port_to_forward_t;

/** This is where we store parsed commandline options. */
typedef struct {
  int verbose;
  int help;
  int test_commandline;
  struct smartlist_t *ports_to_forward;
  int fetch_public_ip;
  int nat_pmp_status;
  int upnp_status;
  int public_ip_status;
} tor_fw_options_t;

/** This is our main structure that defines our backend helper API; each helper
 * must conform to these public methods if it expects to be handled in a
 * non-special way. */
typedef struct tor_fw_backend_t {
  const char *name;
  size_t state_len;
  int (*init)(tor_fw_options_t *options, void *backend_state);
  int (*cleanup)(tor_fw_options_t *options, void *backend_state);
  int (*fetch_public_ip)(tor_fw_options_t *options, void *backend_state);
  int (*add_tcp_mapping)(uint16_t internal_port, uint16_t external_port,
                         int is_verbose, void *backend_state);
} tor_fw_backend_t;
#endif

