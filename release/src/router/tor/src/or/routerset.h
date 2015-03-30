/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerlist.h
 * \brief Header file for routerset.c
 **/

#ifndef TOR_ROUTERSET_H
#define TOR_ROUTERSET_H

routerset_t *routerset_new(void);
void routerset_refresh_countries(routerset_t *rs);
int routerset_parse(routerset_t *target, const char *s,
                    const char *description);
void routerset_union(routerset_t *target, const routerset_t *source);
int routerset_is_list(const routerset_t *set);
int routerset_needs_geoip(const routerset_t *set);
int routerset_is_empty(const routerset_t *set);
int routerset_contains_router(const routerset_t *set, const routerinfo_t *ri,
                              country_t country);
int routerset_contains_routerstatus(const routerset_t *set,
                                    const routerstatus_t *rs,
                                    country_t country);
int routerset_contains_extendinfo(const routerset_t *set,
                                  const extend_info_t *ei);

int routerset_contains_node(const routerset_t *set, const node_t *node);
void routerset_get_all_nodes(smartlist_t *out, const routerset_t *routerset,
                             const routerset_t *excludeset,
                             int running_only);
int routerset_add_unknown_ccs(routerset_t **setp, int only_if_some_cc_set);
void routerset_subtract_nodes(smartlist_t *out,
                                const routerset_t *routerset);

char *routerset_to_string(const routerset_t *routerset);
int routerset_equal(const routerset_t *old, const routerset_t *new);
void routerset_free(routerset_t *routerset);

#endif

