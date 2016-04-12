/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dircollate.c
 *
 * \brief Collation code for figuring out which identities to vote for in
 *   the directory voting process.
 */

#define DIRCOLLATE_PRIVATE
#include "dircollate.h"
#include "dirvote.h"

static void dircollator_collate_by_rsa(dircollator_t *dc);
static void dircollator_collate_by_ed25519(dircollator_t *dc);

typedef struct ddmap_entry_s {
  HT_ENTRY(ddmap_entry_s) node;
  uint8_t d[DIGEST_LEN + DIGEST256_LEN];
  vote_routerstatus_t *vrs_lst[FLEXIBLE_ARRAY_MEMBER];
} ddmap_entry_t;

double_digest_map_t *by_both_ids;

static void
ddmap_entry_free(ddmap_entry_t *e)
{
  tor_free(e);
}

static ddmap_entry_t *
ddmap_entry_new(int n_votes)
{
  return tor_malloc_zero(STRUCT_OFFSET(ddmap_entry_t, vrs_lst) +
                         sizeof(vote_routerstatus_t *) * n_votes);
}

static unsigned
ddmap_entry_hash(const ddmap_entry_t *ent)
{
  return (unsigned) siphash24g(ent->d, sizeof(ent->d));
}

static unsigned
ddmap_entry_eq(const ddmap_entry_t *a, const ddmap_entry_t *b)
{
  return fast_memeq(a->d, b->d, sizeof(a->d));
}

static void
ddmap_entry_set_digests(ddmap_entry_t *ent,
                        const uint8_t *rsa_sha1,
                        const uint8_t *ed25519)
{
  memcpy(ent->d, rsa_sha1, DIGEST_LEN);
  memcpy(ent->d + DIGEST_LEN, ed25519, DIGEST256_LEN);
}

HT_PROTOTYPE(double_digest_map, ddmap_entry_s, node, ddmap_entry_hash,
             ddmap_entry_eq);
HT_GENERATE2(double_digest_map, ddmap_entry_s, node, ddmap_entry_hash,
             ddmap_entry_eq, 0.6, tor_reallocarray, tor_free_);
static void
dircollator_add_routerstatus(dircollator_t *dc,
                             int vote_num,
                             networkstatus_t *vote,
                             vote_routerstatus_t *vrs)
{
  const char *id = vrs->status.identity_digest;

  (void) vote;
  vote_routerstatus_t **vrs_lst = digestmap_get(dc->by_rsa_sha1, id);
  if (NULL == vrs_lst) {
    vrs_lst = tor_calloc(sizeof(vote_routerstatus_t *), dc->n_votes);
    digestmap_set(dc->by_rsa_sha1, id, vrs_lst);
  }
  tor_assert(vrs_lst[vote_num] == NULL);
  vrs_lst[vote_num] = vrs;

  const uint8_t *ed = vrs->ed25519_id;

  if (tor_mem_is_zero((char*)ed, DIGEST256_LEN))
    return;

  ddmap_entry_t search, *found;
  memset(&search, 0, sizeof(search));
  ddmap_entry_set_digests(&search, (const uint8_t *)id, ed);
  found = HT_FIND(double_digest_map, &dc->by_both_ids, &search);
  if (NULL == found) {
    found = ddmap_entry_new(dc->n_votes);
    ddmap_entry_set_digests(found, (const uint8_t *)id, ed);
    HT_INSERT(double_digest_map, &dc->by_both_ids, found);
  }
  vrs_lst = found->vrs_lst;
  tor_assert(vrs_lst[vote_num] == NULL);
  vrs_lst[vote_num] = vrs;
}

dircollator_t *
dircollator_new(int n_votes, int n_authorities)
{
  dircollator_t *dc = tor_malloc_zero(sizeof(dircollator_t));

  tor_assert(n_votes <= n_authorities);

  dc->n_votes = n_votes;
  dc->n_authorities = n_authorities;

  dc->by_rsa_sha1 = digestmap_new();
  HT_INIT(double_digest_map, &dc->by_both_ids);

  return dc;
}

void
dircollator_free(dircollator_t *dc)
{
  if (!dc)
    return;

  if (dc->by_collated_rsa_sha1 != dc->by_rsa_sha1)
    digestmap_free(dc->by_collated_rsa_sha1, NULL);

  digestmap_free(dc->by_rsa_sha1, tor_free_);
  smartlist_free(dc->all_rsa_sha1_lst);

  ddmap_entry_t **e, **next, *this;
  for (e = HT_START(double_digest_map, &dc->by_both_ids);
       e != NULL; e = next) {
    this = *e;
    next = HT_NEXT_RMV(double_digest_map, &dc->by_both_ids, e);
    ddmap_entry_free(this);
  }
  HT_CLEAR(double_digest_map, &dc->by_both_ids);

  tor_free(dc);
}

void
dircollator_add_vote(dircollator_t *dc, networkstatus_t *v)
{
  tor_assert(v->type == NS_TYPE_VOTE);
  tor_assert(dc->next_vote_num < dc->n_votes);
  tor_assert(!dc->is_collated);

  const int votenum = dc->next_vote_num++;

  SMARTLIST_FOREACH_BEGIN(v->routerstatus_list, vote_routerstatus_t *, vrs) {
    dircollator_add_routerstatus(dc, votenum, v, vrs);
  } SMARTLIST_FOREACH_END(vrs);
}

void
dircollator_collate(dircollator_t *dc, int consensus_method)
{
  tor_assert(!dc->is_collated);
  dc->all_rsa_sha1_lst = smartlist_new();

  if (consensus_method < MIN_METHOD_FOR_ED25519_ID_VOTING + 10/*XXX*/)
    dircollator_collate_by_rsa(dc);
  else
    dircollator_collate_by_ed25519(dc);

  smartlist_sort_digests(dc->all_rsa_sha1_lst);
  dc->is_collated = 1;
}

static void
dircollator_collate_by_rsa(dircollator_t *dc)
{
  const int total_authorities = dc->n_authorities;

  DIGESTMAP_FOREACH(dc->by_rsa_sha1, k, vote_routerstatus_t **, vrs_lst) {
    int n = 0, i;
    for (i = 0; i < dc->n_votes; ++i) {
      if (vrs_lst[i] != NULL)
        ++n;
    }

    if (n <= total_authorities / 2)
      continue;

    smartlist_add(dc->all_rsa_sha1_lst, (char *)k);
  } DIGESTMAP_FOREACH_END;

  dc->by_collated_rsa_sha1 = dc->by_rsa_sha1;
}

static void
dircollator_collate_by_ed25519(dircollator_t *dc)
{
  const int total_authorities = dc->n_authorities;
  digestmap_t *rsa_digests = digestmap_new();

  ddmap_entry_t **iter;

  HT_FOREACH(iter, double_digest_map, &dc->by_both_ids) {
    ddmap_entry_t *ent = *iter;
    int n = 0, i;
    for (i = 0; i < dc->n_votes; ++i) {
      if (ent->vrs_lst[i] != NULL)
        ++n;
    }

    if (n <= total_authorities / 2)
      continue;

    vote_routerstatus_t **vrs_lst2 = digestmap_get(dc->by_rsa_sha1,
                                                   (char*)ent->d);
    tor_assert(vrs_lst2);

    for (i = 0; i < dc->n_votes; ++i) {
      if (ent->vrs_lst[i] != NULL) {
        ent->vrs_lst[i]->ed25519_reflects_consensus = 1;
      } else if (vrs_lst2[i] && ! vrs_lst2[i]->has_ed25519_listing) {
        ent->vrs_lst[i] = vrs_lst2[i];
      }
    }

    digestmap_set(rsa_digests, (char*)ent->d, ent->vrs_lst);
    smartlist_add(dc->all_rsa_sha1_lst, ent->d);
  }

  DIGESTMAP_FOREACH(dc->by_rsa_sha1, k, vote_routerstatus_t **, vrs_lst) {
    if (digestmap_get(rsa_digests, k) != NULL)
      continue;

    int n = 0, i;
    for (i = 0; i < dc->n_votes; ++i) {
      if (vrs_lst[i] != NULL)
        ++n;
    }

    if (n <= total_authorities / 2)
      continue;

    digestmap_set(rsa_digests, k, vrs_lst);
    smartlist_add(dc->all_rsa_sha1_lst, (char *)k);
  } DIGESTMAP_FOREACH_END;

  dc->by_collated_rsa_sha1 = rsa_digests;
}

int
dircollator_n_routers(dircollator_t *dc)
{
  return smartlist_len(dc->all_rsa_sha1_lst);
}

vote_routerstatus_t **
dircollator_get_votes_for_router(dircollator_t *dc, int idx)
{
  tor_assert(idx < smartlist_len(dc->all_rsa_sha1_lst));
  return digestmap_get(dc->by_collated_rsa_sha1,
                       smartlist_get(dc->all_rsa_sha1_lst, idx));
}

