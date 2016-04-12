/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dirvote.h
 * \brief Header file for dirvote.c.
 **/

#ifndef TOR_DIRCOLLATE_H
#define TOR_DIRCOLLATE_H

#include "testsupport.h"
#include "or.h"

typedef struct dircollator_s dircollator_t;

dircollator_t *dircollator_new(int n_votes, int n_authorities);
void dircollator_free(dircollator_t *obj);
void dircollator_add_vote(dircollator_t *dc, networkstatus_t *v);

void dircollator_collate(dircollator_t *dc, int consensus_method);

int dircollator_n_routers(dircollator_t *dc);
vote_routerstatus_t **dircollator_get_votes_for_router(dircollator_t *dc,
                                                       int idx);

#ifdef DIRCOLLATE_PRIVATE
struct ddmap_entry_s;
typedef HT_HEAD(double_digest_map, ddmap_entry_s) double_digest_map_t;
struct dircollator_s {
  /**DOCDOC */
  int is_collated;
  int n_votes;
  int n_authorities;

  int next_vote_num;
  digestmap_t *by_rsa_sha1;
  struct double_digest_map by_both_ids;

  digestmap_t *by_collated_rsa_sha1;

  smartlist_t *all_rsa_sha1_lst;
};
#endif

#endif

