/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

/* Get 64-bit stuff if on a GNU system.  */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>

/* This test was written for >2GB core files on 32-bit systems.  On
   current 64-bit systems, generating a >4EB (2 ** 63) core file is
   not practical, and getting as close as we can takes a lot of
   useless CPU time.  So limit ourselves to a bit bigger than
   32-bit, which is still a useful test.  */
#define RLIMIT_CAP (1ULL << 34)

/* Print routines:

   The following are so that printf et.al. can be avoided.  Those
   might try to use malloc() and that, for this code, would be a
   disaster.  */

#define printf do not use

const char digit[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static void
print_char (char c)
{
  write (1, &c, sizeof (c));
}

static void
print_unsigned (unsigned long long u)
{
  if (u >= 10)
    print_unsigned (u / 10);
  print_char (digit[u % 10]);
}

static void
print_hex (unsigned long long u)
{
  if (u >= 16)
    print_hex (u / 16);
  print_char (digit[u % 16]);
}

static void
print_string (const char *s)
{
  for (; (*s) != '\0'; s++)
    print_char ((*s));
}

static void
print_address (const void *a)
{
  print_string ("0x");
  print_hex ((unsigned long) a);
}

static void
print_byte_count (unsigned long long u)
{
  print_unsigned (u);
  print_string (" (");
  print_string ("0x");
  print_hex (u);
  print_string (") bytes");
}

/* Print the current values of RESOURCE.  */

static void
print_rlimit (int resource)
{
  struct rlimit rl;
  getrlimit (resource, &rl);
  print_string ("cur=0x");
  print_hex (rl.rlim_cur);
  print_string (" max=0x");
  print_hex (rl.rlim_max);
}

static void
maximize_rlimit (int resource, const char *prefix)
{
  struct rlimit rl;
  print_string ("  ");
  print_string (prefix);
  print_string (": ");
  print_rlimit (resource);
  getrlimit (resource, &rl);
  rl.rlim_cur = rl.rlim_max;
  if (sizeof (rl.rlim_cur) >= sizeof (RLIMIT_CAP))
    rl.rlim_cur = (rlim_t) RLIMIT_CAP;
  setrlimit (resource, &rl);
  print_string (" -> ");
  print_rlimit (resource);
  print_string ("\n");
}

/* Maintain a doublely linked list.  */
struct list
{
  struct list *next;
  struct list *prev;
  size_t size;
};

/* Put the "heap" in the DATA section.  That way it is more likely
   that the variable will occur early in the core file (an address
   before the heap) and hence more likely that GDB will at least get
   its value right.

   To simplify the list append logic, start the heap out with one
   entry (that lives in the BSS section).  */

static struct list dummy;
static struct list heap = { &dummy, &dummy };

static unsigned long bytes_allocated;

#ifdef O_LARGEFILE
#define large_off_t off64_t
#define large_lseek lseek64
#else
#define large_off_t off_t
#define O_LARGEFILE 0
#define large_lseek lseek
#endif

int
main ()
{
  size_t max_chunk_size;
  large_off_t max_core_size;

  /* Try to expand all the resource limits beyond the point of sanity
     - we're after the biggest possible core file.  */

  print_string ("Maximize resource limits ...\n");
#ifdef RLIMIT_CORE
  maximize_rlimit (RLIMIT_CORE, "core");
#endif
#ifdef RLIMIT_DATA
  maximize_rlimit (RLIMIT_DATA, "data");
#endif
#ifdef RLIMIT_STACK
  maximize_rlimit (RLIMIT_STACK, "stack");
#endif
#ifdef RLIMIT_AS
  maximize_rlimit (RLIMIT_AS, "stack");
#endif

  print_string ("Maximize allocation limits ...\n");

  /* Compute the largest possible corefile size.  No point in trying
     to create a corefile larger than the largest file supported by
     the file system.  What about 64-bit lseek64?  */
  {
    int fd;
    large_off_t tmp;
    unlink ("bigcore.corefile");
    fd = open ("bigcore.corefile", O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE,
	       0666);
    for (tmp = 1; tmp > 0; tmp <<= 1)
      {
	if (large_lseek (fd, tmp, SEEK_SET) > 0)
	  max_core_size = tmp;
      }
    close (fd);
  }
  
  /* Compute an initial chunk size.  The math is dodgy but it works
     for the moment.  Perhaphs there's a constant around somewhere.
     Limit this to max_core_size bytes - no point in trying to
     allocate more than can be written to the corefile.  */
  {
    size_t tmp;
    for (tmp = 1; tmp > 0 && tmp < max_core_size; tmp <<= 1)
      max_chunk_size = tmp;
  }

  print_string ("  core: ");
  print_byte_count (max_core_size);
  print_string ("\n");
  print_string ("  chunk: ");
  print_byte_count (max_chunk_size);
  print_string ("\n");
  print_string ("  large? ");
  if (O_LARGEFILE)
    print_string ("yes\n");
  else
    print_string ("no\n");

  /* Allocate as much memory as possible creating a linked list of
     each section.  The linking ensures that some, but not all, the
     memory is allocated.  NB: Some kernels handle this efficiently -
     only allocating and writing out referenced pages leaving holes in
     the file for unmodified pages - while others handle this poorly -
     writing out all pages including those that weren't modified.  */

  print_string ("Alocating the entire heap ...\n");
  {
    size_t chunk_size;
    unsigned long chunks_allocated = 0;
    /* Create a linked list of memory chunks.  Start with
       MAX_CHUNK_SIZE blocks of memory and then try allocating smaller
       and smaller amounts until all (well at least most) memory has
       been allocated.  */
    for (chunk_size = max_chunk_size;
	 chunk_size >= sizeof (struct list);
	 chunk_size >>= 1)
      {
	unsigned long count = 0;
	print_string ("  ");
	print_byte_count (chunk_size);
	print_string (" ... ");
	while (bytes_allocated + (1 + count) * chunk_size
	       < max_core_size)
	  {
	    struct list *chunk = malloc (chunk_size);
	    if (chunk == NULL)
	      break;
	    chunk->size = chunk_size;
	    /* Link it in.  */
	    chunk->next = NULL;
	    chunk->prev = heap.prev;
	    heap.prev->next = chunk;
	    heap.prev = chunk;
	    count++;
	  }
	print_unsigned (count);
	print_string (" chunks\n");
	chunks_allocated += count;
	bytes_allocated += chunk_size * count;
      }
    print_string ("Total of ");
    print_byte_count (bytes_allocated);
    print_string (" bytes ");
    print_unsigned (chunks_allocated);
    print_string (" chunks\n");
  }

  /* Push everything out to disk.  */

  print_string ("Dump core ....\n");
  *(char*)0 = 0;
}
