/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file protover.h
 * \brief Headers and type declarations for protover.c
 **/

#ifndef TOR_PROTOVER_H
#define TOR_PROTOVER_H

#include "container.h"

/** The first version of Tor that included "proto" entries in its
 * descriptors.  Authorities should use this to decide whether to
 * guess proto lines. */
/* This is a guess. */
#define FIRST_TOR_VERSION_TO_ADVERTISE_PROTOCOLS "0.2.9.3-alpha"

/** List of recognized subprotocols. */
typedef enum protocol_type_t {
  PRT_LINK,
  PRT_LINKAUTH,
  PRT_RELAY,
  PRT_DIRCACHE,
  PRT_HSDIR,
  PRT_HSINTRO,
  PRT_HSREND,
  PRT_DESC,
  PRT_MICRODESC,
  PRT_CONS,
} protocol_type_t;

int protover_all_supported(const char *s, char **missing);
int protover_is_supported_here(protocol_type_t pr, uint32_t ver);
const char *protover_get_supported_protocols(void);

char *protover_compute_vote(const smartlist_t *list_of_proto_strings,
                            int threshold);
const char *protover_compute_for_old_tor(const char *version);
int protocol_list_supports_protocol(const char *list, protocol_type_t tp,
                                    uint32_t version);

void protover_free_all(void);

#ifdef PROTOVER_PRIVATE
/** Represents a range of subprotocols of a given type. All subprotocols
 * between <b>low</b> and <b>high</b> inclusive are included. */
typedef struct proto_range_t {
  uint32_t low;
  uint32_t high;
} proto_range_t;

/** Represents a set of ranges of subprotocols of a given type. */
typedef struct proto_entry_t {
  /** The name of the protocol.
   *
   * (This needs to handle voting on protocols which
   * we don't recognize yet, so it's a char* rather than a protocol_type_t.)
   */
  char *name;
  /** Smartlist of proto_range_t */
  smartlist_t *ranges;
} proto_entry_t;

STATIC smartlist_t *parse_protocol_list(const char *s);
STATIC void proto_entry_free(proto_entry_t *entry);
STATIC char *encode_protocol_list(const smartlist_t *sl);
STATIC const char *protocol_type_to_str(protocol_type_t pr);
STATIC int str_to_protocol_type(const char *s, protocol_type_t *pr_out);
#endif

#endif

