/*
 * Hash Table Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 *
 * Free Software License:
 *
 * All rights are reserved by the author, with the following exceptions:
 * Permission is granted to freely reproduce and distribute this software,
 * possibly in exchange for a fee, provided that this copyright notice appears
 * intact. Permission is also granted to adapt this software to produce
 * derivative works, as long as the modified versions carry this copyright
 * notice and additional notices stating that the work has been modified.
 * This source code may be translated into executable form and incorporated
 * into proprietary software; there is no requirement for such software to
 * contain a copyright notice related to this source.
 *
 * $Name:  $
 */

#ifndef HASH_H
#define HASH_H

#include <limits.h>
#include <atalk/hash.h>

extern hash_t *hash_create(hashcount_t, hash_comp_t, hash_fun_t);
extern void hash_set_allocator(hash_t *, hnode_alloc_t, hnode_free_t, void *);
extern void hash_destroy(hash_t *);
extern void hash_free_nodes(hash_t *);
extern void hash_free(hash_t *);
extern hash_t *hash_init(hash_t *, hashcount_t, hash_comp_t,
	hash_fun_t, hnode_t **, hashcount_t);
extern void hash_insert(hash_t *, hnode_t *, const void *);
extern hnode_t *hash_lookup(hash_t *, const void *);
extern hnode_t *hash_delete(hash_t *, hnode_t *);
extern int hash_alloc_insert(hash_t *, const void *, void *);
extern void hash_delete_free(hash_t *, hnode_t *);

extern void hnode_put(hnode_t *, void *);
extern void *hnode_get(hnode_t *);
extern const void *hnode_getkey(hnode_t *);
extern hashcount_t hash_count(hash_t *);
extern hashcount_t hash_size(hash_t *);

extern int hash_isfull(hash_t *);
extern int hash_isempty(hash_t *);

extern void hash_scan_begin(hscan_t *, hash_t *);
extern hnode_t *hash_scan_next(hscan_t *);
extern hnode_t *hash_scan_delete(hash_t *, hnode_t *);
extern void hash_scan_delfree(hash_t *, hnode_t *);

extern int hash_verify(hash_t *);

extern hnode_t *hnode_create(void *);
extern hnode_t *hnode_init(hnode_t *, void *);
extern void hnode_destroy(hnode_t *);

#if defined(HASH_IMPLEMENTATION) || !defined(KAZLIB_OPAQUE_DEBUG)
#ifdef KAZLIB_SIDEEFFECT_DEBUG
#define hash_isfull(H) (SFX_CHECK(H)->hash_nodecount == (H)->hash_maxcount)
#else
#define hash_isfull(H) ((H)->hash_nodecount == (H)->hash_maxcount)
#endif
#define hash_isempty(H) ((H)->hash_nodecount == 0)
#define hash_count(H) ((H)->hash_nodecount)
#define hash_size(H) ((H)->hash_nchains)
#define hnode_get(N) ((N)->hash_data)
#define hnode_getkey(N) ((N)->hash_key)
#define hnode_put(N, V) ((N)->hash_data = (V))
#endif

#endif
