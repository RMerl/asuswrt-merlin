#ifndef UBI_CACHE_H
#define UBI_CACHE_H
/* ========================================================================== **
 *                                 ubi_Cache.h
 *
 *  Copyright (C) 1997 by Christopher R. Hertel
 *
 *  Email: crh@ubiqx.mn.org
 * -------------------------------------------------------------------------- **
 *
 *  This module implements a generic cache.
 *
 * -------------------------------------------------------------------------- **
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- **
 *
 *  This module uses a splay tree to implement a simple cache.  The cache
 *  module adds a thin layer of functionality to the splay tree.  In
 *  particular:
 *
 *    - The tree (cache) may be limited in size by the number of
 *      entries permitted or the amount of memory used.  When either
 *      limit is exceeded cache entries are removed until the cache
 *      conforms.
 *    - Some statistical information is kept so that an approximate
 *      "hit ratio" can be calculated.
 *    - There are several functions available that provide access to
 *      and management of cache size limits, hit ratio, and tree
 *      trimming.
 *
 *  The splay tree is used because recently accessed items tend toward the
 *  top of the tree and less recently accessed items tend toward the bottom.
 *  This makes it easy to purge less recently used items should the cache
 *  exceed its limits.
 *
 *  To use this module, you will need to supply a comparison function of
 *  type ubi_trCompFunc and a node-freeing function of type
 *  ubi_trKillNodeTrn.  See ubi_BinTree.h for more information on
 *  these.  (This is all basic ubiqx tree management stuff.)
 *
 *  Notes:
 *
 *  - Cache performance will start to suffer dramatically if the
 *    cache becomes large enough to force the OS to start swapping
 *    memory to disk.  This is because the nodes of the underlying tree
 *    will be scattered across memory in an order that is completely
 *    unrelated to their traversal order.  As more and more of the
 *    cache is placed into swap space, more and more swaps will be
 *    required for a simple traversal (...and then there's the splay
 *    operation).
 *
 *    In one simple test under Linux, the load and dump of a cache of
 *    400,000 entries took only 1min, 40sec of real time.  The same
 *    test with 450,000 records took 2 *hours* and eight minutes.
 *
 *  - In an effort to save memory, I considered using an unsigned
 *    short to save the per-entry entry size.  I would have tucked this
 *    value into some unused space in the tree node structure.  On
 *    32-bit word aligned systems this would have saved an additional
 *    four bytes per entry.  I may revisit this issue, but for now I've
 *    decided against it.
 *
 *    Using an unsigned short would limit the size of an entry to 64K
 *    bytes.  That's probably more than enough for most applications.
 *    The key word in that last sentence, however, is "probably".  I
 *    really dislike imposing such limits on things.
 *
 *  - Each entry keeps track of the amount of memory it used and the
 *    cache header keeps the total.  This information is provided via
 *    the EntrySize parameter in ubi_cachePut(), so it is up to you to
 *    make sure that the numbers are accurate.  (The numbers don't even
 *    have to represent bytes used.)
 *
 *    As you consider this, note that the strdup() function--as an
 *    example--will call malloc().  The latter generally allocates a
 *    multiple of the system word size, which may be more than the
 *    number of bytes needed to store the string.
 *
 * -------------------------------------------------------------------------- **
 *
 *  Log: ubi_Cache.h,v 
 *  Revision 0.3  1998/06/03 18:00:15  crh
 *  Further fiddling with sys_include.h, which is no longer explicitly
 *  included by this module since it is inherited from ubi_BinTree.h.
 *
 *  Revision 0.2  1998/06/02 01:36:18  crh
 *  Changed include name from ubi_null.h to sys_include.h to make it
 *  more generic.
 *
 *  Revision 0.1  1998/05/20 04:36:02  crh
 *  The C file now includes ubi_null.h.  See ubi_null.h for more info.
 *
 *  Revision 0.0  1997/12/18 06:25:23  crh
 *  Initial Revision.
 *
 * ========================================================================== **
 */

#include "ubi_SplayTree.h"

/* -------------------------------------------------------------------------- **
 * Typedefs...
 *
 *  ubi_cacheRoot     - Cache header structure, which consists of a binary
 *                      tree root and other required housekeeping fields, as
 *                      listed below.
 *  ubi_cacheRootPtr  - Pointer to a Cache.
 *
 *  ubi_cacheEntry    - A cache Entry, which consists of a tree node
 *                      structure and the size (in bytes) of the entry
 *                      data.  The entry size should be supplied via
 *                      the EntrySize parameter of the ubi_cachePut()
 *                      function.
 *
 *  ubi_cacheEntryPtr - Pointer to a ubi_cacheEntry.
 *
 */

typedef struct
  {
  ubi_trRoot        root;         /* Splay tree control structure.      */
  ubi_trKillNodeRtn free_func;    /* Function used to free entries.     */
  unsigned long     max_entries;  /* Max cache entries.  0 == unlimited */
  unsigned long     max_memory;   /* Max memory to use.  0 == unlimited */
  unsigned long     mem_used;     /* Memory currently in use (bytes).   */
  unsigned short    cache_hits;   /* Incremented on succesful find.     */
  unsigned short    cache_trys;   /* Incremented on cache lookup.       */
  } ubi_cacheRoot;

typedef ubi_cacheRoot *ubi_cacheRootPtr;


typedef struct
  {
  ubi_trNode    node;           /* Tree node structure. */
  unsigned long entry_size;     /* Entry size.  Used when managing
                                 * caches with maximum memory limits.
                                 */
  } ubi_cacheEntry;

typedef ubi_cacheEntry *ubi_cacheEntryPtr;


/* -------------------------------------------------------------------------- **
 * Macros...
 *
 *  ubi_cacheGetMaxEntries()  - Report the current maximum number of entries
 *                              allowed in the cache.  Zero indicates no
 *                              maximum.
 *  ubi_cacheGetMaxMemory()   - Report the current maximum amount of memory
 *                              that may be used in the cache.  Zero
 *                              indicates no maximum.
 *  ubi_cacheGetEntryCount()  - Report the current number of entries in the
 *                              cache.
 *  ubi_cacheGetMemUsed()     - Report the amount of memory currently in use
 *                              by the cache.
 */

#define ubi_cacheGetMaxEntries( Cptr ) (((ubi_cacheRootPtr)(Cptr))->max_entries)
#define ubi_cacheGetMaxMemory(  Cptr ) (((ubi_cacheRootPtr)(Cptr))->max_memory)

#define ubi_cacheGetEntryCount( Cptr ) (((ubi_cacheRootPtr)(Cptr))->root.count)
#define ubi_cacheGetMemUsed( Cptr ) (((ubi_cacheRootPtr)(Cptr))->mem_used)

/* -------------------------------------------------------------------------- **
 * Prototypes...
 */

ubi_cacheRootPtr ubi_cacheInit( ubi_cacheRootPtr  CachePtr,
                                ubi_trCompFunc    CompFunc,
                                ubi_trKillNodeRtn FreeFunc,
                                unsigned long     MaxEntries,
                                unsigned long     MaxMemory );
  /* ------------------------------------------------------------------------ **
   * Initialize a cache header structure.
   *
   *  Input:  CachePtr    - A pointer to a ubi_cacheRoot structure that is
   *                        to be initialized.
   *          CompFunc    - A pointer to the function that will be called
   *                        to compare two cache values.  See the module
   *                        comments, above, for more information.
   *          FreeFunc    - A pointer to a function that will be called
   *                        to free a cache entry.  If you allocated
   *                        the cache entry using malloc(), then this
   *                        will likely be free().  If you are allocating
   *                        cache entries from a free list, then this will
   *                        likely be a function that returns memory to the
   *                        free list, etc.
   *          MaxEntries  - The maximum number of entries that will be
   *                        allowed to exist in the cache.  If this limit
   *                        is exceeded, then existing entries will be
   *                        removed from the cache.  A value of zero
   *                        indicates that there is no limit on the number
   *                        of cache entries.  See ubi_cachePut().
   *          MaxMemory   - The maximum amount of memory, in bytes, to be
   *                        allocated to the cache (excluding the cache
   *                        header).  If this is exceeded, existing entries
   *                        in the cache will be removed until enough memory
   *                        has been freed to meet the condition.  See
   *                        ubi_cachePut().
   *
   *  Output: A pointer to the initialized cache (i.e., the same as CachePtr).
   *
   *  Notes:  Both MaxEntries and MaxMemory may be changed after the cache
   *          has been created.  See
   *            ubi_cacheSetMaxEntries() 
   *            ubi_cacheSetMaxMemory()
   *            ubi_cacheGetMaxEntries()
   *            ubi_cacheGetMaxMemory()    (the latter two are macros).
   *
   *        - Memory is allocated in multiples of the word size.  The
   *          return value of the strlen() function does not reflect
   *          this; it will allways be less than or equal to the amount
   *          of memory actually allocated.  Keep this in mind when
   *          choosing a value for MaxMemory.
   *
   * ------------------------------------------------------------------------ **
   */

ubi_cacheRootPtr ubi_cacheClear( ubi_cacheRootPtr CachePtr );
  /* ------------------------------------------------------------------------ **
   * Remove and free all entries in an existing cache.
   *
   *  Input:  CachePtr  - A pointer to the cache that is to be cleared.
   *
   *  Output: A pointer to the cache header (i.e., the same as CachePtr).
   *          This function re-initializes the cache header.
   *
   * ------------------------------------------------------------------------ **
   */

void ubi_cachePut( ubi_cacheRootPtr  CachePtr,
                   unsigned long     EntrySize,
                   ubi_cacheEntryPtr EntryPtr,
                   ubi_trItemPtr     Key );
  /* ------------------------------------------------------------------------ **
   * Add an entry to the cache.
   *
   *  Input:  CachePtr  - A pointer to the cache into which the entry
   *                      will be added.
   *          EntrySize - The size, in bytes, of the memory block indicated
   *                      by EntryPtr.  This will be copied into the
   *                      EntryPtr->entry_size field.
   *          EntryPtr  - A pointer to a memory block that begins with a
   *                      ubi_cacheEntry structure.  The entry structure
   *                      should be followed immediately by the data to be
   *                      cached (even if that is a pointer to yet more data).
   *          Key       - Pointer used to identify the lookup key within the
   *                      Entry.
   *
   *  Output: None.
   *
   *  Notes:  After adding the new node, the cache is "trimmed".  This
   *          removes extra nodes if the tree has exceeded it's memory or
   *          entry count limits.  It is unlikely that the newly added node
   *          will be purged from the cache (assuming a reasonably large
   *          cache), since new nodes in a splay tree (which is what this
   *          module was designed to use) are moved to the top of the tree
   *          and the cache purge process removes nodes from the bottom of
   *          the tree.
   *        - The underlying splay tree is opened in OVERWRITE mode.  If
   *          the input key matches an existing key, the existing entry will
   *          be politely removed from the tree and freed.
   *        - Memory is allocated in multiples of the word size.  The
   *          return value of the strlen() function does not reflect
   *          this; it will allways be less than or equal to the amount
   *          of memory actually allocated.
   *
   * ------------------------------------------------------------------------ **
   */

ubi_cacheEntryPtr ubi_cacheGet( ubi_cacheRootPtr CachePtr,
                                ubi_trItemPtr    FindMe );
  /* ------------------------------------------------------------------------ **
   * Attempt to retrieve an entry from the cache.
   *
   *  Input:  CachePtr  - A ponter to the cache that is to be searched.
   *          FindMe    - A ubi_trItemPtr that indicates the key for which
   *                      to search.
   *
   *  Output: A pointer to the cache entry that was found, or NULL if no
   *          matching entry was found.
   *
   *  Notes:  This function also updates the hit ratio counters.
   *          The counters are unsigned short.  If the number of cache tries
   *          reaches 32768, then both the number of tries and the number of
   *          hits are divided by two.  This prevents the counters from
   *          overflowing.  See the comments in ubi_cacheHitRatio() for
   *          additional notes.
   *
   * ------------------------------------------------------------------------ **
   */

ubi_trBool ubi_cacheDelete( ubi_cacheRootPtr CachePtr, ubi_trItemPtr DeleteMe );
  /* ------------------------------------------------------------------------ **
   * Find and delete the specified cache entry.
   *
   *  Input:  CachePtr  - A pointer to the cache.
   *          DeleteMe  - The key of the entry to be deleted.
   *
   *  Output: TRUE if the entry was found & freed, else FALSE.
   *
   * ------------------------------------------------------------------------ **
   */

ubi_trBool ubi_cacheReduce( ubi_cacheRootPtr CachePtr, unsigned long count );
  /* ------------------------------------------------------------------------ **
   * Remove <count> entries from the bottom of the cache.
   *
   *  Input:  CachePtr  - A pointer to the cache which is to be reduced in
   *                      size.
   *          count     - The number of entries to remove.
   *
   *  Output: The function will return TRUE if <count> entries were removed,
   *          else FALSE.  A return value of FALSE should indicate that
   *          there were less than <count> entries in the cache, and that the
   *          cache is now empty.
   *
   *  Notes:  This function forces a reduction in the number of cache entries
   *          without requiring that the MaxMemory or MaxEntries values be
   *          changed.
   *
   * ------------------------------------------------------------------------ **
   */

unsigned long ubi_cacheSetMaxEntries( ubi_cacheRootPtr CachePtr,
                                      unsigned long    NewSize );
  /* ------------------------------------------------------------------------ **
   * Change the maximum number of entries allowed to exist in the cache.
   *
   *  Input:  CachePtr  - A pointer to the cache to be modified.
   *          NewSize   - The new maximum number of cache entries.
   *
   *  Output: The maximum number of entries previously allowed to exist in
   *          the cache.
   *
   *  Notes:  If the new size is less than the old size, this function will
   *          trim the cache (remove excess entries).
   *        - A value of zero indicates an unlimited number of entries.
   *
   * ------------------------------------------------------------------------ **
   */

unsigned long ubi_cacheSetMaxMemory( ubi_cacheRootPtr CachePtr,
                                     unsigned long    NewSize );
  /* ------------------------------------------------------------------------ **
   * Change the maximum amount of memory to be used for storing cache
   * entries.
   *
   *  Input:  CachePtr  - A pointer to the cache to be modified.
   *          NewSize   - The new cache memory size.
   *
   *  Output: The previous maximum memory size.
   *
   *  Notes:  If the new size is less than the old size, this function will
   *          trim the cache (remove excess entries).
   *        - A value of zero indicates that the cache has no memory limit.
   *
   * ------------------------------------------------------------------------ **
   */

int ubi_cacheHitRatio( ubi_cacheRootPtr CachePtr );
  /* ------------------------------------------------------------------------ **
   * Returns a value that is 10,000 times the slightly weighted average hit
   * ratio for the cache.
   *
   *  Input:  CachePtr  - Pointer to the cache to be queried.
   *
   *  Output: An integer that is 10,000 times the number of successful
   *          cache hits divided by the number of cache lookups, or:
   *            (10000 * hits) / trys
   *          You can easily convert this to a float, or do something
   *          like this (where i is the return value of this function):
   *
   *            printf( "Hit rate   : %d.%02d%%\n", (i/100), (i%100) );
   *
   *  Notes:  I say "slightly-weighted", because the numerator and
   *          denominator are both accumulated in locations of type
   *          'unsigned short'.  If the number of cache trys becomes
   *          large enough, both are divided  by two.  (See function
   *          ubi_cacheGet().) 
   *          Dividing both numerator and denominator by two does not
   *          change the ratio (much...it is an integer divide), but it
   *          does mean that subsequent increments to either counter will
   *          have twice as much significance as previous ones. 
   *
   *        - The value returned by this function will be in the range
   *          [0..10000] because ( 0 <= cache_hits <= cache_trys ) will
   *          always be true.
   *
   * ------------------------------------------------------------------------ **
   */

/* -------------------------------------------------------------------------- */
#endif /* ubi_CACHE_H */
