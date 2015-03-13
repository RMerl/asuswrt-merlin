/* Hash tables.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
   2009, 2010, 2011 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

/* With -DSTANDALONE, this file can be compiled outside Wget source
   tree.  To test, also use -DTEST.  */

#ifndef STANDALONE
# include "wget.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#ifndef STANDALONE
/* Get Wget's utility headers. */
# include "utils.h"
#else
/* Make do without them. */
# define xnew(x) xmalloc (sizeof (x))
# define xnew_array(type, x) xmalloc (sizeof (type) * (x))
# define xmalloc malloc
# define xfree free
# ifndef countof
#  define countof(x) (sizeof (x) / sizeof ((x)[0]))
# endif
# include <ctype.h>
# define c_tolower(x) tolower ((unsigned char) (x))
# include <stdint.h>
#endif

#include "hash.h"

/* INTERFACE:

   Hash tables are a technique used to implement mapping between
   objects with near-constant-time access and storage.  The table
   associates keys to values, and a value can be very quickly
   retrieved by providing the key.  Fast lookup tables are typically
   implemented as hash tables.

   The entry points are
     hash_table_new       -- creates the table.
     hash_table_destroy   -- destroys the table.
     hash_table_put       -- establishes or updates key->value mapping.
     hash_table_get       -- retrieves value of key.
     hash_table_get_pair  -- get key/value pair for key.
     hash_table_contains  -- test whether the table contains key.
     hash_table_remove    -- remove key->value mapping for given key.
     hash_table_for_each  -- call function for each table entry.
     hash_table_iterate   -- iterate over entries in hash table.
     hash_table_iter_next -- return next element during iteration.
     hash_table_clear     -- clear hash table contents.
     hash_table_count     -- return the number of entries in the table.

   The hash table grows internally as new entries are added and is not
   limited in size, except by available memory.  The table doubles
   with each resize, which ensures that the amortized time per
   operation remains constant.

   If not instructed otherwise, tables created by hash_table_new
   consider the keys to be equal if their pointer values are the same.
   You can use make_string_hash_table to create tables whose keys are
   considered equal if their string contents are the same.  In the
   general case, the criterion of equality used to compare keys is
   specified at table creation time with two callback functions,
   "hash" and "test".  The hash function transforms the key into an
   arbitrary number that must be the same for two equal keys.  The
   test function accepts two keys and returns non-zero if they are to
   be considered equal.

   Note that neither keys nor values are copied when inserted into the
   hash table, so they must exist for the lifetime of the table.  This
   means that e.g. the use of static strings is OK, but objects with a
   shorter life-time probably need to be copied (with strdup() or the
   like in the case of strings) before being inserted.  */

/* IMPLEMENTATION:

   The hash table is implemented as an open-addressed table with
   linear probing collision resolution.

   The above means that all the cells (each cell containing a key and
   a value pointer) are stored in a contiguous array.  Array position
   of each cell is determined by the hash value of its key and the
   size of the table: location := hash(key) % size.  If two different
   keys end up on the same position (collide), the one that came
   second is stored in the first unoccupied cell that follows it.
   This collision resolution technique is called "linear probing".

   There are more advanced collision resolution methods (quadratic
   probing, double hashing), but we don't use them because they incur
   more non-sequential access to the array, which results in worse CPU
   cache behavior.  Linear probing works well as long as the
   count/size ratio (fullness) is kept below 75%.  We make sure to
   grow and rehash the table whenever this threshold is exceeded.

   Collisions complicate deletion because simply clearing a cell
   followed by previously collided entries would cause those neighbors
   to not be picked up by find_cell later.  One solution is to leave a
   "tombstone" marker instead of clearing the cell, and another is to
   recalculate the positions of adjacent cells.  We take the latter
   approach because it results in less bookkeeping garbage and faster
   retrieval at the (slight) expense of deletion.  */

/* Maximum allowed fullness: when hash table's fullness exceeds this
   value, the table is resized.  */
#define HASH_MAX_FULLNESS 0.75

/* The hash table size is multiplied by this factor (and then rounded
   to the next prime) with each resize.  This guarantees infrequent
   resizes.  */
#define HASH_RESIZE_FACTOR 2

struct cell {
  void *key;
  void *value;
};

typedef unsigned long (*hashfun_t) (const void *);
typedef int (*testfun_t) (const void *, const void *);

struct hash_table {
  hashfun_t hash_function;
  testfun_t test_function;

  struct cell *cells;           /* contiguous array of cells. */
  int size;                     /* size of the array. */

  int count;                    /* number of occupied entries. */
  int resize_threshold;         /* after size exceeds this number of
                                   entries, resize the table.  */
  int prime_offset;             /* the offset of the current prime in
                                   the prime table. */
};

/* We use the all-bits-set constant (INVALID_PTR) marker to mean that
   a cell is empty.  It is unaligned and therefore illegal as a
   pointer.  INVALID_PTR_CHAR (0xff) is the single-character constant
   used to initialize the entire cells array as empty.

   The all-bits-set value is a better choice than NULL because it
   allows the use of NULL/0 keys.  Since the keys are either integers
   or pointers, the only key that cannot be used is the integer value
   -1.  This is acceptable because it still allows the use of
   nonnegative integer keys.  */

#define INVALID_PTR ((void *) ~(uintptr_t) 0)
#ifndef UCHAR_MAX
# define UCHAR_MAX 0xff
#endif
#define INVALID_PTR_CHAR UCHAR_MAX

/* Whether the cell C is occupied (non-empty). */
#define CELL_OCCUPIED(c) ((c)->key != INVALID_PTR)

/* Clear the cell C, i.e. mark it as empty (unoccupied). */
#define CLEAR_CELL(c) ((c)->key = INVALID_PTR)

/* "Next" cell is the cell following C, but wrapping back to CELLS
   when C would reach CELLS+SIZE.  */
#define NEXT_CELL(c, cells, size) (c != cells + (size - 1) ? c + 1 : cells)

/* Loop over occupied cells starting at C, terminating the loop when
   an empty cell is encountered.  */
#define FOREACH_OCCUPIED_ADJACENT(c, cells, size)                               \
  for (; CELL_OCCUPIED (c); c = NEXT_CELL (c, cells, size))

/* Return the position of KEY in hash table SIZE large, hash function
   being HASHFUN.  */
#define HASH_POSITION(key, hashfun, size) ((hashfun) (key) % size)

/* Find a prime near, but greather than or equal to SIZE.  The primes
   are looked up from a table with a selection of primes convenient
   for this purpose.

   PRIME_OFFSET is a minor optimization: it specifies start position
   for the search for the large enough prime.  The final offset is
   stored in the same variable.  That way the list of primes does not
   have to be scanned from the beginning each time around.  */

static int
prime_size (int size, int *prime_offset)
{
  static const int primes[] = {
    13, 19, 29, 41, 59, 79, 107, 149, 197, 263, 347, 457, 599, 787, 1031,
    1361, 1777, 2333, 3037, 3967, 5167, 6719, 8737, 11369, 14783,
    19219, 24989, 32491, 42257, 54941, 71429, 92861, 120721, 156941,
    204047, 265271, 344857, 448321, 582821, 757693, 985003, 1280519,
    1664681, 2164111, 2813353, 3657361, 4754591, 6180989, 8035301,
    10445899, 13579681, 17653589, 22949669, 29834603, 38784989,
    50420551, 65546729, 85210757, 110774011, 144006217, 187208107,
    243370577, 316381771, 411296309, 534685237, 695090819, 903618083,
    1174703521, 1527114613, 1837299131, 2147483647
  };
  size_t i;

  for (i = *prime_offset; i < countof (primes); i++)
    if (primes[i] >= size)
      {
        /* Set the offset to the next prime.  That is safe because,
           next time we are called, it will be with a larger SIZE,
           which means we could never return the same prime anyway.
           (If that is not the case, the caller can simply reset
           *prime_offset.)  */
        *prime_offset = i + 1;
        return primes[i];
      }

  abort ();
}

static int cmp_pointer (const void *, const void *);

/* Create a hash table with hash function HASH_FUNCTION and test
   function TEST_FUNCTION.  The table is empty (its count is 0), but
   pre-allocated to store at least ITEMS items.

   ITEMS is the number of items that the table can accept without
   needing to resize.  It is useful when creating a table that is to
   be immediately filled with a known number of items.  In that case,
   the regrows are a waste of time, and specifying ITEMS correctly
   will avoid them altogether.

   Note that hash tables grow dynamically regardless of ITEMS.  The
   only use of ITEMS is to preallocate the table and avoid unnecessary
   dynamic regrows.  Don't bother making ITEMS prime because it's not
   used as size unchanged.  To start with a small table that grows as
   needed, simply specify zero ITEMS.

   If hash and test callbacks are not specified, identity mapping is
   assumed, i.e. pointer values are used for key comparison.  (Common
   Lisp calls such tables EQ hash tables, and Java calls them
   IdentityHashMaps.)  If your keys require different comparison,
   specify hash and test functions.  For easy use of C strings as hash
   keys, you can use the convenience functions make_string_hash_table
   and make_nocase_string_hash_table.  */

struct hash_table *
hash_table_new (int items,
                unsigned long (*hash_function) (const void *),
                int (*test_function) (const void *, const void *))
{
  int size;
  struct hash_table *ht = xnew (struct hash_table);

  ht->hash_function = hash_function ? hash_function : hash_pointer;
  ht->test_function = test_function ? test_function : cmp_pointer;

  /* If the size of struct hash_table ever becomes a concern, this
     field can go.  (Wget doesn't create many hashes.)  */
  ht->prime_offset = 0;

  /* Calculate the size that ensures that the table will store at
     least ITEMS keys without the need to resize.  */
  size = 1 + items / HASH_MAX_FULLNESS;
  size = prime_size (size, &ht->prime_offset);
  ht->size = size;
  ht->resize_threshold = size * HASH_MAX_FULLNESS;
  /*assert (ht->resize_threshold >= items);*/

  ht->cells = xnew_array (struct cell, ht->size);

  /* Mark cells as empty.  We use 0xff rather than 0 to mark empty
     keys because it allows us to use NULL/0 as keys.  */
  memset (ht->cells, INVALID_PTR_CHAR, size * sizeof (struct cell));

  ht->count = 0;

  return ht;
}

/* Free the data associated with hash table HT. */

void
hash_table_destroy (struct hash_table *ht)
{
  xfree (ht->cells);
  xfree (ht);
}

/* The heart of most functions in this file -- find the cell whose
   KEY is equal to key, using linear probing.  Returns the cell
   that matches KEY, or the first empty cell if none matches.  */

static inline struct cell *
find_cell (const struct hash_table *ht, const void *key)
{
  struct cell *cells = ht->cells;
  int size = ht->size;
  struct cell *c = cells + HASH_POSITION (key, ht->hash_function, size);
  testfun_t equals = ht->test_function;

  FOREACH_OCCUPIED_ADJACENT (c, cells, size)
    if (equals (key, c->key))
      break;
  return c;
}

/* Get the value that corresponds to the key KEY in the hash table HT.
   If no value is found, return NULL.  Note that NULL is a legal value
   for value; if you are storing NULLs in your hash table, you can use
   hash_table_contains to be sure that a (possibly NULL) value exists
   in the table.  Or, you can use hash_table_get_pair instead of this
   function.  */

void *
hash_table_get (const struct hash_table *ht, const void *key)
{
  struct cell *c = find_cell (ht, key);
  if (CELL_OCCUPIED (c))
    return c->value;
  else
    return NULL;
}

/* Like hash_table_get, but writes out the pointers to both key and
   value.  Returns non-zero on success.  */

int
hash_table_get_pair (const struct hash_table *ht, const void *lookup_key,
                     void *orig_key, void *value)
{
  struct cell *c = find_cell (ht, lookup_key);
  if (CELL_OCCUPIED (c))
    {
      if (orig_key)
        *(void **)orig_key = c->key;
      if (value)
        *(void **)value = c->value;
      return 1;
    }
  else
    return 0;
}

/* Return 1 if HT contains KEY, 0 otherwise. */

int
hash_table_contains (const struct hash_table *ht, const void *key)
{
  struct cell *c = find_cell (ht, key);
  return CELL_OCCUPIED (c);
}

/* Grow hash table HT as necessary, and rehash all the key-value
   mappings.  */

static void
grow_hash_table (struct hash_table *ht)
{
  hashfun_t hasher = ht->hash_function;
  struct cell *old_cells = ht->cells;
  struct cell *old_end   = ht->cells + ht->size;
  struct cell *c, *cells;
  int newsize;

  newsize = prime_size (ht->size * HASH_RESIZE_FACTOR, &ht->prime_offset);
#if 0
  printf ("growing from %d to %d; fullness %.2f%% to %.2f%%\n",
          ht->size, newsize,
          100.0 * ht->count / ht->size,
          100.0 * ht->count / newsize);
#endif

  ht->size = newsize;
  ht->resize_threshold = newsize * HASH_MAX_FULLNESS;

  cells = xnew_array (struct cell, newsize);
  memset (cells, INVALID_PTR_CHAR, newsize * sizeof (struct cell));
  ht->cells = cells;

  for (c = old_cells; c < old_end; c++)
    if (CELL_OCCUPIED (c))
      {
        struct cell *new_c;
        /* We don't need to test for uniqueness of keys because they
           come from the hash table and are therefore known to be
           unique.  */
        new_c = cells + HASH_POSITION (c->key, hasher, newsize);
        FOREACH_OCCUPIED_ADJACENT (new_c, cells, newsize)
          ;
        *new_c = *c;
      }

  xfree (old_cells);
}

/* Put VALUE in the hash table HT under the key KEY.  This regrows the
   table if necessary.  */

void
hash_table_put (struct hash_table *ht, const void *key, const void *value)
{
  struct cell *c = find_cell (ht, key);
  if (CELL_OCCUPIED (c))
    {
      /* update existing item */
      c->key   = (void *)key; /* const? */
      c->value = (void *)value;
      return;
    }

  /* If adding the item would make the table exceed max. fullness,
     grow the table first.  */
  if (ht->count >= ht->resize_threshold)
    {
      grow_hash_table (ht);
      c = find_cell (ht, key);
    }

  /* add new item */
  ++ht->count;
  c->key   = (void *)key;       /* const? */
  c->value = (void *)value;
}

/* Remove KEY->value mapping from HT.  Return 0 if there was no such
   entry; return 1 if an entry was removed.  */

int
hash_table_remove (struct hash_table *ht, const void *key)
{
  struct cell *c = find_cell (ht, key);
  if (!CELL_OCCUPIED (c))
    return 0;
  else
    {
      int size = ht->size;
      struct cell *cells = ht->cells;
      hashfun_t hasher = ht->hash_function;

      CLEAR_CELL (c);
      --ht->count;

      /* Rehash all the entries following C.  The alternative
         approach is to mark the entry as deleted, i.e. create a
         "tombstone".  That speeds up removal, but leaves a lot of
         garbage and slows down hash_table_get and hash_table_put.  */

      c = NEXT_CELL (c, cells, size);
      FOREACH_OCCUPIED_ADJACENT (c, cells, size)
        {
          const void *key2 = c->key;
          struct cell *c_new;

          /* Find the new location for the key. */
          c_new = cells + HASH_POSITION (key2, hasher, size);
          FOREACH_OCCUPIED_ADJACENT (c_new, cells, size)
            if (key2 == c_new->key)
              /* The cell C (key2) is already where we want it (in
                 C_NEW's "chain" of keys.)  */
              goto next_rehash;

          *c_new = *c;
          CLEAR_CELL (c);

        next_rehash:
          ;
        }
      return 1;
    }
}

/* Clear HT of all entries.  After calling this function, the count
   and the fullness of the hash table will be zero.  The size will
   remain unchanged.  */

void
hash_table_clear (struct hash_table *ht)
{
  memset (ht->cells, INVALID_PTR_CHAR, ht->size * sizeof (struct cell));
  ht->count = 0;
}

/* Call FN for each entry in HT.  FN is called with three arguments:
   the key, the value, and ARG.  When FN returns a non-zero value, the
   mapping stops.

   It is undefined what happens if you add or remove entries in the
   hash table while hash_table_for_each is running.  The exception is
   the entry you're currently mapping over; you may call
   hash_table_put or hash_table_remove on that entry's key.  That is
   also the reason why this function cannot be implemented in terms of
   hash_table_iterate.  */

void
hash_table_for_each (struct hash_table *ht,
                     int (*fn) (void *, void *, void *), void *arg)
{
  struct cell *c = ht->cells;
  struct cell *end = ht->cells + ht->size;

  for (; c < end; c++)
    if (CELL_OCCUPIED (c))
      {
        void *key;
      repeat:
        key = c->key;
        if (fn (key, c->value, arg))
          return;
        /* hash_table_remove might have moved the adjacent cells. */
        if (c->key != key && CELL_OCCUPIED (c))
          goto repeat;
      }
}

/* Initiate iteration over HT.  Entries are obtained with
   hash_table_iter_next, a typical iteration loop looking like this:

       hash_table_iterator iter;
       for (hash_table_iterate (ht, &iter); hash_table_iter_next (&iter); )
         ... do something with iter.key and iter.value ...

   The iterator does not need to be deallocated after use.  The hash
   table must not be modified while being iterated over.  */

void
hash_table_iterate (struct hash_table *ht, hash_table_iterator *iter)
{
  iter->pos = ht->cells;
  iter->end = ht->cells + ht->size;
}

/* Get the next hash table entry.  ITER is an iterator object
   initialized using hash_table_iterate.  While there are more
   entries, the key and value pointers are stored to ITER->key and
   ITER->value respectively and 1 is returned.  When there are no more
   entries, 0 is returned.

   If the hash table is modified between calls to this function, the
   result is undefined.  */

int
hash_table_iter_next (hash_table_iterator *iter)
{
  struct cell *c = iter->pos;
  struct cell *end = iter->end;
  for (; c < end; c++)
    if (CELL_OCCUPIED (c))
      {
        iter->key = c->key;
        iter->value = c->value;
        iter->pos = c + 1;
        return 1;
      }
  return 0;
}

/* Return the number of elements in the hash table.  This is not the
   same as the physical size of the hash table, which is always
   greater than the number of elements.  */

int
hash_table_count (const struct hash_table *ht)
{
  return ht->count;
}

/* Functions from this point onward are meant for convenience and
   don't strictly belong to this file.  However, this is as good a
   place for them as any.  */

/* Guidelines for creating custom hash and test functions:

   - The test function returns non-zero for keys that are considered
     "equal", zero otherwise.

   - The hash function returns a number that represents the
     "distinctness" of the object.  In more precise terms, it means
     that for any two objects that test "equal" under the test
     function, the hash function MUST produce the same result.

     This does not mean that all different objects must produce
     different values (that would be "perfect" hashing), only that
     non-distinct objects must produce the same values!  For instance,
     a hash function that returns 0 for any given object is a
     perfectly valid (albeit extremely bad) hash function.  A hash
     function that hashes a string by adding up all its characters is
     another example of a valid (but still quite bad) hash function.

     It is not hard to make hash and test functions agree about
     equality.  For example, if the test function compares strings
     case-insensitively, the hash function can lower-case the
     characters when calculating the hash value.  That ensures that
     two strings differing only in case will hash the same.

   - To prevent performance degradation, choose a hash function with
     as good "spreading" as possible.  A good hash function will use
     all the bits of the input when calculating the hash, and will
     react to even small changes in input with a completely different
     output.  But don't make the hash function itself overly slow,
     because you'll be incurring a non-negligible overhead to all hash
     table operations.  */

/*
 * Support for hash tables whose keys are strings.
 *
 */

/* Base 31 hash function.  Taken from Gnome's glib, modified to use
   standard C types.

   We used to use the popular hash function from the Dragon Book, but
   this one seems to perform much better, both by being faster and by
   generating less collisions.  */

static unsigned long
hash_string (const void *key)
{
  const char *p = key;
  unsigned int h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

/* Frontend for strcmp usable for hash tables. */

static int
cmp_string (const void *s1, const void *s2)
{
  return !strcmp ((const char *)s1, (const char *)s2);
}

/* Return a hash table of preallocated to store at least ITEMS items
   suitable to use strings as keys.  */

struct hash_table *
make_string_hash_table (int items)
{
  return hash_table_new (items, hash_string, cmp_string);
}

/*
 * Support for hash tables whose keys are strings, but which are
 * compared case-insensitively.
 *
 */

/* Like hash_string, but produce the same hash regardless of the case. */

static unsigned long
hash_string_nocase (const void *key)
{
  const char *p = key;
  unsigned int h = c_tolower (*p);

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + c_tolower (*p);

  return h;
}

/* Like string_cmp, but doing case-insensitive compareison. */

static int
string_cmp_nocase (const void *s1, const void *s2)
{
  return !strcasecmp ((const char *)s1, (const char *)s2);
}

/* Like make_string_hash_table, but uses string_hash_nocase and
   string_cmp_nocase.  */

struct hash_table *
make_nocase_string_hash_table (int items)
{
  return hash_table_new (items, hash_string_nocase, string_cmp_nocase);
}

/* Hashing of numeric values, such as pointers and integers.

   This implementation is the Robert Jenkins' 32 bit Mix Function,
   with a simple adaptation for 64-bit values.  According to Jenkins
   it should offer excellent spreading of values.  Unlike the popular
   Knuth's multiplication hash, this function doesn't need to know the
   hash table size to work.  */

unsigned long
hash_pointer (const void *ptr)
{
  uintptr_t key = (uintptr_t) ptr;
  key += (key << 12);
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);
#if SIZEOF_VOID_P > 4
  key += (key << 44);
  key ^= (key >> 54);
  key += (key << 36);
  key ^= (key >> 41);
  key += (key << 42);
  key ^= (key >> 34);
  key += (key << 39);
  key ^= (key >> 44);
#endif
  return (unsigned long) key;
}

static int
cmp_pointer (const void *ptr1, const void *ptr2)
{
  return ptr1 == ptr2;
}

#ifdef TEST

#include <stdio.h>
#include <string.h>

void
print_hash (struct hash_table *sht)
{
  hash_table_iterator iter;
  int count = 0;

  for (hash_table_iterate (sht, &iter); hash_table_iter_next (&iter);
       ++count)
    printf ("%s: %s\n", iter.key, iter.value);
  assert (count == sht->count);
}

int
main (void)
{
  struct hash_table *ht = make_string_hash_table (0);
  char line[80];

#ifdef ENABLE_NLS
  /* Set the current locale.  */
  setlocale (LC_ALL, "");
  /* Set the text message domain.  */
  bindtextdomain ("wget", LOCALEDIR);
  textdomain ("wget");
#endif /* ENABLE_NLS */

  while ((fgets (line, sizeof (line), stdin)))
    {
      int len = strlen (line);
      if (len <= 1)
        continue;
      line[--len] = '\0';
      if (!hash_table_contains (ht, line))
        hash_table_put (ht, strdup (line), "here I am!");
#if 1
      if (len % 5 == 0)
        {
          char *line_copy;
          if (hash_table_get_pair (ht, line, &line_copy, NULL))
            {
              hash_table_remove (ht, line);
              xfree (line_copy);
            }
        }
#endif
    }
#if 0
  print_hash (ht);
#endif
#if 1
  printf ("%d %d\n", ht->count, ht->size);
#endif
  return 0;
}
#endif /* TEST */
