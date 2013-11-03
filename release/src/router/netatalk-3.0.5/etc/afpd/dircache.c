/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/volume.h>
#include <atalk/directory.h>
#include <atalk/queue.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/globals.h>

#include "dircache.h"
#include "directory.h"
#include "hash.h"


/*
 * Directory Cache
 * ===============
 *
 * Cache files and directories in a LRU cache.
 *
 * The directory cache caches directories and files(!). The main reason for having the cache
 * is avoiding recursive walks up the path, querying the CNID database each time, when
 * we have to calculate the location of eg directory with CNID 30, which is located in a dir with
 * CNID 25, next CNID 20 and then CNID 2 (the volume root as per AFP spec).
 * If all these dirs where in the cache, each database look up can be avoided. Additionally there's
 * the element "fullpath" in struct dir, which is used to avoid the recursion in any case. Wheneveer
 * a struct dir is initialized, the fullpath to the directory is stored there.
 *
 * In order to speed up the CNID query for files too, which eg happens when a directory is enumerated,
 * files are stored too in the dircache. In order to differentiate between files and dirs, we set
 * the flag DIRF_ISFILE in struct dir.d_flags for files.
 *
 * The most frequent codepatch that leads to caching is directory enumeration (cf enumerate.c):
 * - if a element is a directory:
 *   (1) the cache is searched by dircache_search_by_name()
 *   (2) if it wasn't found a new struct dir is created and cached both from within dir_add()
 * - for files the caching happens a little bit down the call chain:
 *   (3) first getfilparams() is called, which calls
 *   (4) getmetadata() where the cache is searched with dircache_search_by_name()
 *   (5) if the element is not found
 *   (6) get_id() queries the CNID from the database
 *   (7) then a struct dir is initialized via dir_new() (note the fullpath arg is NULL)
 *   (8) finally added to the cache with dircache_add()
 * (2) of course does contain the steps 6,7 and 8.
 *
 * The dircache is a LRU cache, whenever it fills up we call dircache_evict internally which removes
 * DIRCACHE_FREE_QUANTUM elements from the cache.
 *
 * There is only one cache for all volumes, so of course we use the volume id in hashing calculations.
 *
 * In order to avoid cache poisoning, we store the cached entries st_ctime from stat in
 * struct dir.ctime_dircache. Later when we search the cache we compare the stored
 * value with the result of a fresh stat. If the times differ, we remove the cached
 * entry and return "no entry found in cache".
 * A elements ctime changes when
 *   1) the element is renamed
 *      (we loose the cached entry here, but it will expire when the cache fills)
 *   2) its a directory and an object has been created therein
 *   3) the element is deleted and recreated under the same name
 * Using ctime leads to cache eviction in case 2) where it wouldn't be necessary, because
 * the dir itself (name, CNID, ...) hasn't changed, but there's no other way.
 *
 * Indexes
 * =======
 *
 * The maximum dircache size is:
 * max(DEFAULT_MAX_DIRCACHE_SIZE, min(size, MAX_POSSIBLE_DIRCACHE_SIZE)).
 * It is a hashtable which we use to store "struct dir"s in. If the cache get full, oldest
 * entries are evicted in chunks of DIRCACHE_FREE.
 *
 * We have/need two indexes:
 * - a DID/name index on the main dircache, another hashtable
 * - a queue index on the dircache, for evicting the oldest entries
 *
 * Debugging
 * =========
 *
 * Sending SIGINT to a afpd child causes it to dump the dircache to a file "/tmp/dircache.PID".
 */

/********************************************************
 * Local funcs and variables
 ********************************************************/

/*****************************
 *       the dircache        */

static hash_t       *dircache;        /* The actual cache */
static unsigned int dircache_maxsize; /* cache maximum size */

static struct dircache_stat {
    unsigned long long lookups;
    unsigned long long hits;
    unsigned long long misses;
    unsigned long long added;
    unsigned long long removed;
    unsigned long long expunged;
    unsigned long long evicted;
} dircache_stat;

/* FNV 1a */
static hash_val_t hash_vid_did(const void *key)
{
    const struct dir *k = (const struct dir *)key;
    hash_val_t hash = 2166136261;

    hash ^= k->d_vid >> 8;
    hash *= 16777619;
    hash ^= k->d_vid;
    hash *= 16777619;

    hash ^= k->d_did >> 24;
    hash *= 16777619;
    hash ^= (k->d_did >> 16) & 0xff;
    hash *= 16777619;
    hash ^= (k->d_did >> 8) & 0xff;
    hash *= 16777619;
    hash ^= (k->d_did >> 0) & 0xff;
    hash *= 16777619;

    return hash;
}

static int hash_comp_vid_did(const void *key1, const void *key2)
{
    const struct dir *k1 = key1;
    const struct dir *k2 = key2;

    return !(k1->d_did == k2->d_did && k1->d_vid == k2->d_vid);
}

/**************************************************
 * DID/name index on dircache (another hashtable) */

static hash_t *index_didname;

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__)    \
    || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)    \
                      +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

static hash_val_t hash_didname(const void *p)
{
    const struct dir *key = (const struct dir *)p;
    const unsigned char *data = key->d_u_name->data;
    int len = key->d_u_name->slen;
    hash_val_t hash = key->d_pdid + key->d_vid;
    hash_val_t tmp;

    int rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
    case 3: hash += get16bits (data);
        hash ^= hash << 16;
        hash ^= data[sizeof (uint16_t)] << 18;
        hash += hash >> 11;
        break;
    case 2: hash += get16bits (data);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;
    case 1: hash += *data;
        hash ^= hash << 10;
        hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

static int hash_comp_didname(const void *k1, const void *k2)
{
    const struct dir *key1 = (const struct dir *)k1;
    const struct dir *key2 = (const struct dir *)k2;

    return ! (key1->d_vid == key2->d_vid
              && key1->d_pdid == key2->d_pdid
              && (bstrcmp(key1->d_u_name, key2->d_u_name) == 0) );
}

/***************************
 * queue index on dircache */

static q_t *index_queue;    /* the index itself */
static unsigned long queue_count;

/*!
 * @brief Remove a fixed number of (oldest) entries from the cache and indexes
 *
 * The default is to remove the 256 oldest entries from the cache.
 * 1. Get the oldest entry
 * 2. If it's in use ie open forks reference it or it's curdir requeue it,
 *    dont remove it
 * 3. Remove the dir from the main cache and the didname index
 * 4. Free the struct dir structure and all its members
 */
static void dircache_evict(void)
{
    int i = DIRCACHE_FREE_QUANTUM;
    struct dir *dir;

    LOG(log_debug, logtype_afpd, "dircache: {starting cache eviction}");

    while (i--) {
        if ((dir = (struct dir *)dequeue(index_queue)) == NULL) { /* 1 */
            dircache_dump();
            AFP_PANIC("dircache_evict");
        }
        queue_count--;

        if (curdir == dir) {                          /* 2 */
            if ((dir->qidx_node = enqueue(index_queue, dir)) == NULL) {
                dircache_dump();
                AFP_PANIC("dircache_evict");
            }
            queue_count++;
            continue;
        }

        dircache_remove(NULL, dir, DIRCACHE | DIDNAME_INDEX); /* 3 */
        dir_free(dir);                                        /* 4 */
    }

    AFP_ASSERT(queue_count == dircache->hash_nodecount);
    dircache_stat.evicted += DIRCACHE_FREE_QUANTUM;
    LOG(log_debug, logtype_afpd, "dircache: {finished cache eviction}");
}


/********************************************************
 * Interface
 ********************************************************/

/*!
 * @brief Search the dircache via a CNID for a directory
 *
 * Found cache entries are expunged if both the parent directory st_ctime and the objects
 * st_ctime are modified.
 * This func builds on the fact, that all our code only ever needs to and does search
 * the dircache by CNID expecting directories to be returned, but not files.
 * Thus
 * (1) if we find a file for a given CNID we
 *     (1a) remove it from the cache
 *     (1b) return NULL indicating nothing found
 * (2) we can then use d_fullpath to stat the directory
 *
 * @param vol      (r) pointer to struct vol
 * @param cnid     (r) CNID of the directory to search
 *
 * @returns            Pointer to struct dir if found, else NULL
 */
struct dir *dircache_search_by_did(const struct vol *vol, cnid_t cnid)
{
    struct dir *cdir = NULL;
    struct dir key;
    struct stat st;
    hnode_t *hn;

    AFP_ASSERT(vol);
    AFP_ASSERT(ntohl(cnid) >= CNID_START);

    dircache_stat.lookups++;
    key.d_vid = vol->v_vid;
    key.d_did = cnid;
    if ((hn = hash_lookup(dircache, &key)))
        cdir = hnode_get(hn);

    if (cdir) {
        if (cdir->d_flags & DIRF_ISFILE) { /* (1) */
            LOG(log_debug, logtype_afpd, "dircache(cnid:%u): {not a directory:\"%s\"}",
                ntohl(cnid), cfrombstr(cdir->d_u_name));
            (void)dir_remove(vol, cdir); /* (1a) */
            dircache_stat.expunged++;
            return NULL;        /* (1b) */

        }
        if (ostat(cfrombstr(cdir->d_fullpath), &st, vol_syml_opt(vol)) != 0) {
            LOG(log_debug, logtype_afpd, "dircache(cnid:%u): {missing:\"%s\"}",
                ntohl(cnid), cfrombstr(cdir->d_fullpath));
            (void)dir_remove(vol, cdir);
            dircache_stat.expunged++;
            return NULL;
        }
        if ((cdir->dcache_ctime != st.st_ctime) || (cdir->dcache_ino != st.st_ino)) {
            LOG(log_debug, logtype_afpd, "dircache(cnid:%u): {modified:\"%s\"}",
                ntohl(cnid), cfrombstr(cdir->d_u_name));
            (void)dir_remove(vol, cdir);
            dircache_stat.expunged++;
            return NULL;
        }
        LOG(log_debug, logtype_afpd, "dircache(cnid:%u): {cached: path:\"%s\"}",
            ntohl(cnid), cfrombstr(cdir->d_fullpath));
        dircache_stat.hits++;
    } else {
        LOG(log_debug, logtype_afpd, "dircache(cnid:%u): {not in cache}", ntohl(cnid));
        dircache_stat.misses++;
    }
    
    return cdir;
}

/*!
 * @brief Search the cache via did/name hashtable
 *
 * Found cache entries are expunged if both the parent directory st_ctime and the objects
 * st_ctime are modified.
 *
 * @param vol      (r) volume
 * @param dir      (r) directory
 * @param name     (r) name (server side encoding)
 * @parma len      (r) strlen of name
 *
 * @returns pointer to struct dir if found in cache, else NULL
 */
struct dir *dircache_search_by_name(const struct vol *vol,
                                    const struct dir *dir,
                                    char *name,
                                    int len)
{
    struct dir *cdir = NULL;
    struct dir key;
    struct stat st;

    hnode_t *hn;
    static_bstring uname = {-1, len, (unsigned char *)name};

    AFP_ASSERT(vol);
    AFP_ASSERT(dir);
    AFP_ASSERT(name);
    AFP_ASSERT(len == strlen(name));
    AFP_ASSERT(len < 256);

    dircache_stat.lookups++;
    LOG(log_debug, logtype_afpd, "dircache_search_by_name(did:%u, \"%s\")",
        ntohl(dir->d_did), name);

    if (dir->d_did != DIRDID_ROOT_PARENT) {
        key.d_vid = vol->v_vid;
        key.d_pdid = dir->d_did;
        key.d_u_name = &uname;

        if ((hn = hash_lookup(index_didname, &key)))
            cdir = hnode_get(hn);
    }

    if (cdir) {
        if (ostat(cfrombstr(cdir->d_fullpath), &st, vol_syml_opt(vol)) != 0) {
            LOG(log_debug, logtype_afpd, "dircache(did:%u,\"%s\"): {missing:\"%s\"}",
                ntohl(dir->d_did), name, cfrombstr(cdir->d_fullpath));
            (void)dir_remove(vol, cdir);
            dircache_stat.expunged++;
            return NULL;
        }

        /* Remove modified directories and files */
        if ((cdir->dcache_ctime != st.st_ctime) || (cdir->dcache_ino != st.st_ino)) {
            LOG(log_debug, logtype_afpd, "dircache(did:%u,\"%s\"): {modified}",
                ntohl(dir->d_did), name);
            (void)dir_remove(vol, cdir);
            dircache_stat.expunged++;
            return NULL;
        }
        LOG(log_debug, logtype_afpd, "dircache(did:%u,\"%s\"): {found in cache}",
            ntohl(dir->d_did), name);
        dircache_stat.hits++;
    } else {
        LOG(log_debug, logtype_afpd, "dircache(did:%u,\"%s\"): {not in cache}",
            ntohl(dir->d_did), name);
        dircache_stat.misses++;
    }

    return cdir;
}

/*!
 * @brief create struct dir from struct path
 *
 * Add a struct dir to the cache and its indexes.
 *
 * @param dir   (r) pointer to parrent directory
 *
 * @returns 0 on success, -1 on error which should result in an abort
 */
int dircache_add(const struct vol *vol,
                 struct dir *dir)
{
    struct dir key;
    hnode_t *hn;

    AFP_ASSERT(dir);
    AFP_ASSERT(ntohl(dir->d_pdid) >= 2);
    AFP_ASSERT(ntohl(dir->d_did) >= CNID_START);
    AFP_ASSERT(dir->d_u_name);
    AFP_ASSERT(dir->d_vid);
    AFP_ASSERT(dircache->hash_nodecount <= dircache_maxsize);

    /* Check if cache is full */
    if (dircache->hash_nodecount == dircache_maxsize)
        dircache_evict();

    /* 
     * Make sure we don't add duplicates
     */

    /* Search primary cache by CNID */
    key.d_vid = dir->d_vid;
    key.d_did = dir->d_did;
    if ((hn = hash_lookup(dircache, &key))) {
        /* Found an entry with the same CNID, delete it */
        dir_remove(vol, hnode_get(hn));
        dircache_stat.expunged++;
    }
    key.d_vid = vol->v_vid;
    key.d_pdid = dir->d_pdid;
    key.d_u_name = dir->d_u_name;
    if ((hn = hash_lookup(index_didname, &key))) {
        /* Found an entry with the same DID/name, delete it */
        dir_remove(vol, hnode_get(hn));
        dircache_stat.expunged++;
    }

    /* Add it to the main dircache */
    if (hash_alloc_insert(dircache, dir, dir) == 0) {
        dircache_dump();
        exit(EXITERR_SYS);
    }

    /* Add it to the did/name index */
    if (hash_alloc_insert(index_didname, dir, dir) == 0) {
        dircache_dump();
        exit(EXITERR_SYS);
    }

    /* Add it to the fifo queue index */
    if ((dir->qidx_node = enqueue(index_queue, dir)) == NULL) {
        dircache_dump();
        exit(EXITERR_SYS);
    } else {
        queue_count++;
    }

    dircache_stat.added++;
    LOG(log_debug, logtype_afpd, "dircache(did:%u,'%s'): {added}",
        ntohl(dir->d_did), cfrombstr(dir->d_u_name));

   AFP_ASSERT(queue_count == index_didname->hash_nodecount 
           && queue_count == dircache->hash_nodecount);

    return 0;
}

/*!
  * @brief Remove an entry from the dircache
  *
  * Callers outside of dircache.c should call this with
  * flags = QUEUE_INDEX | DIDNAME_INDEX | DIRCACHE.
  */
void dircache_remove(const struct vol *vol _U_, struct dir *dir, int flags)
{
    hnode_t *hn;

    AFP_ASSERT(dir);
    AFP_ASSERT((flags & ~(QUEUE_INDEX | DIDNAME_INDEX | DIRCACHE)) == 0);

    if (flags & QUEUE_INDEX) {
        /* remove it from the queue index */
        dequeue(dir->qidx_node->prev); /* this effectively deletes the dequeued node */
        queue_count--;
    }

    if (flags & DIDNAME_INDEX) {
        if ((hn = hash_lookup(index_didname, dir)) == NULL) {
            LOG(log_error, logtype_afpd, "dircache_remove(%u,\"%s\"): not in didname index", 
                ntohl(dir->d_did), cfrombstr(dir->d_u_name));
            dircache_dump();
            AFP_PANIC("dircache_remove");
        }
        hash_delete_free(index_didname, hn);
    }

    if (flags & DIRCACHE) {
        if ((hn = hash_lookup(dircache, dir)) == NULL) {
            LOG(log_error, logtype_afpd, "dircache_remove(%u,\"%s\"): not in dircache", 
                ntohl(dir->d_did), cfrombstr(dir->d_u_name));
            dircache_dump();
            AFP_PANIC("dircache_remove");
        }
        hash_delete_free(dircache, hn);
    }

    LOG(log_debug, logtype_afpd, "dircache(did:%u,\"%s\"): {removed}",
        ntohl(dir->d_did), cfrombstr(dir->d_u_name));

    dircache_stat.removed++;
    AFP_ASSERT(queue_count == index_didname->hash_nodecount 
               && queue_count == dircache->hash_nodecount);
}

/*!
 * @brief Initialize the dircache and indexes
 *
 * This is called in child afpd initialisation. The maximum cache size will be
 * max(DEFAULT_MAX_DIRCACHE_SIZE, min(size, MAX_POSSIBLE_DIRCACHE_SIZE)).
 * It initializes a hashtable which we use to store a directory cache in.
 * It also initializes two indexes:
 * - a DID/name index on the main dircache
 * - a queue index on the dircache
 *
 * @param size   (r) requested maximum size from afp.conf
 *
 * @return 0 on success, -1 on error
 */
int dircache_init(int reqsize)
{
    dircache_maxsize = DEFAULT_MAX_DIRCACHE_SIZE;

    /* Initialize the main dircache */
    if (reqsize > DEFAULT_MAX_DIRCACHE_SIZE && reqsize < MAX_POSSIBLE_DIRCACHE_SIZE) {
        while ((dircache_maxsize < MAX_POSSIBLE_DIRCACHE_SIZE) && (dircache_maxsize < reqsize))
               dircache_maxsize *= 2;
    }
    if ((dircache = hash_create(dircache_maxsize, hash_comp_vid_did, hash_vid_did)) == NULL)
        return -1;
    
    LOG(log_debug, logtype_afpd, "dircache_init: done. max dircache size: %u", dircache_maxsize);

    /* Initialize did/name index hashtable */
    if ((index_didname = hash_create(dircache_maxsize, hash_comp_didname, hash_didname)) == NULL)
        return -1;

    /* Initialize index queue */
    if ((index_queue = queue_init()) == NULL)
        return -1;
    else
        queue_count = 0;

    /* Initialize index queue */
    if ((invalid_dircache_entries = queue_init()) == NULL)
        return -1;

    /* As long as directory.c hasn't got its own initializer call, we do it for it */
    rootParent.d_did = DIRDID_ROOT_PARENT;
    rootParent.d_fullpath = bfromcstr("ROOT_PARENT");
    rootParent.d_m_name = bfromcstr("ROOT_PARENT");
    rootParent.d_u_name = rootParent.d_m_name;
    rootParent.d_rights_cache = 0xffffffff;

    return 0;
}

/*!
 * Log dircache statistics
 */
void log_dircache_stat(void)
{
    LOG(log_info, logtype_afpd, "dircache statistics: "
        "entries: %lu, lookups: %llu, hits: %llu, misses: %llu, added: %llu, removed: %llu, expunged: %llu, evicted: %llu",
        queue_count,
        dircache_stat.lookups,
        dircache_stat.hits,
        dircache_stat.misses,
        dircache_stat.added,
        dircache_stat.removed,
        dircache_stat.expunged,
        dircache_stat.evicted);
}

/*!
 * @brief Dump dircache to /tmp/dircache.PID
 */
void dircache_dump(void)
{
    char tmpnam[64];
    FILE *dump;
    qnode_t *n = index_queue->next;
    hnode_t *hn;
    hscan_t hs;
    const struct dir *dir;
    int i;

    LOG(log_warning, logtype_afpd, "Dumping directory cache...");

    sprintf(tmpnam, "/tmp/dircache.%u", getpid());
    if ((dump = fopen(tmpnam, "w+")) == NULL) {
        LOG(log_error, logtype_afpd, "dircache_dump: %s", strerror(errno));
        return;
    }
    setbuf(dump, NULL);

    fprintf(dump, "Number of cache entries in LRU queue: %lu\n", queue_count);
    fprintf(dump, "Configured maximum cache size: %u\n\n", dircache_maxsize);

    fprintf(dump, "Primary CNID index:\n");
    fprintf(dump, "       VID     DID    CNID STAT PATH\n");
    fprintf(dump, "====================================================================\n");
    hash_scan_begin(&hs, dircache);
    i = 1;
    while ((hn = hash_scan_next(&hs))) {
        dir = hnode_get(hn);
        fprintf(dump, "%05u: %3u  %6u  %6u %s    %s\n",
                i++,
                ntohs(dir->d_vid),
                ntohl(dir->d_pdid),
                ntohl(dir->d_did),
                dir->d_flags & DIRF_ISFILE ? "f" : "d",
                cfrombstr(dir->d_fullpath));
    }

    fprintf(dump, "\nSecondary DID/name index:\n");
    fprintf(dump, "       VID     DID    CNID STAT PATH\n");
    fprintf(dump, "====================================================================\n");
    hash_scan_begin(&hs, index_didname);
    i = 1;
    while ((hn = hash_scan_next(&hs))) {
        dir = hnode_get(hn);
        fprintf(dump, "%05u: %3u  %6u  %6u %s    %s\n",
                i++,
                ntohs(dir->d_vid),
                ntohl(dir->d_pdid),
                ntohl(dir->d_did),
                dir->d_flags & DIRF_ISFILE ? "f" : "d",
                cfrombstr(dir->d_fullpath));
    }

    fprintf(dump, "\nLRU Queue:\n");
    fprintf(dump, "       VID     DID    CNID STAT PATH\n");
    fprintf(dump, "====================================================================\n");

    for (i = 1; i <= queue_count; i++) {
        if (n == index_queue)
            break;
        dir = (struct dir *)n->data;
        fprintf(dump, "%05u: %3u  %6u  %6u %s    %s\n",
                i,
                ntohs(dir->d_vid),
                ntohl(dir->d_pdid),
                ntohl(dir->d_did),
                dir->d_flags & DIRF_ISFILE ? "f" : "d",
                cfrombstr(dir->d_fullpath));
        n = n->next;
    }

    fprintf(dump, "\n");
    fflush(dump);
    fclose(dump);
    return;
}
