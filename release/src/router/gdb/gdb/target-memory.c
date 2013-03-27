/* Parts of target interface that deal with accessing memory and memory-like
   objects.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "vec.h"
#include "target.h"
#include "memory-map.h"

#include "gdb_assert.h"

#include <stdio.h>
#include <sys/time.h>

static int
compare_block_starting_address (const void *a, const void *b)
{
  const struct memory_write_request *a_req = a;
  const struct memory_write_request *b_req = b;

  if (a_req->begin < b_req->begin)
    return -1;
  else if (a_req->begin == b_req->begin)
    return 0;
  else
    return 1;
}

/* Adds to RESULT all memory write requests from BLOCK that are
   in [BEGIN, END) range.

   If any memory request is only partially in the specified range,
   that part of the memory request will be added.  */

static void
claim_memory (VEC(memory_write_request_s) *blocks,
	      VEC(memory_write_request_s) **result,
	      ULONGEST begin,
	      ULONGEST end)
{
  int i;
  ULONGEST claimed_begin;
  ULONGEST claimed_end;
  struct memory_write_request *r;

  for (i = 0; VEC_iterate (memory_write_request_s, blocks, i, r); ++i)
    {
      /* If the request doesn't overlap [BEGIN, END), skip it.  We
	 must handle END == 0 meaning the top of memory; we don't yet
	 check for R->end == 0, which would also mean the top of
	 memory, but there's an assertion in
	 target_write_memory_blocks which checks for that.  */

      if (begin >= r->end)
	continue;
      if (end != 0 && end <= r->begin)
	continue;

      claimed_begin = max (begin, r->begin);
      if (end == 0)
	claimed_end = r->end;
      else
	claimed_end = min (end, r->end);

      if (claimed_begin == r->begin && claimed_end == r->end)
	VEC_safe_push (memory_write_request_s, *result, r);
      else
	{
	  struct memory_write_request *n =
	    VEC_safe_push (memory_write_request_s, *result, NULL);
	  memset (n, 0, sizeof (struct memory_write_request));
	  n->begin = claimed_begin;
	  n->end = claimed_end;
	  n->data = r->data + (claimed_begin - r->begin);
	}
    }
}

/* Given a vector of struct memory_write_request objects in BLOCKS,
   add memory requests for flash memory into FLASH_BLOCKS, and for
   regular memory to REGULAR_BLOCKS.  */

static void
split_regular_and_flash_blocks (VEC(memory_write_request_s) *blocks,
				VEC(memory_write_request_s) **regular_blocks,
				VEC(memory_write_request_s) **flash_blocks)
{
  struct mem_region *region;
  CORE_ADDR cur_address;

  /* This implementation runs in O(length(regions)*length(blocks)) time.
     However, in most cases the number of blocks will be small, so this does
     not matter.

     Note also that it's extremely unlikely that a memory write request
     will span more than one memory region, however for safety we handle
     such situations.  */

  cur_address = 0;
  while (1)
    {
      VEC(memory_write_request_s) **r;
      region = lookup_mem_region (cur_address);

      r = region->attrib.mode == MEM_FLASH ? flash_blocks : regular_blocks;
      cur_address = region->hi;
      claim_memory (blocks, r, region->lo, region->hi);

      if (cur_address == 0)
	break;
    }
}

/* Given an ADDRESS, if BEGIN is non-NULL this function sets *BEGIN
   to the start of the flash block containing the address.  Similarly,
   if END is non-NULL *END will be set to the address one past the end
   of the block containing the address.  */

static void
block_boundaries (CORE_ADDR address, CORE_ADDR *begin, CORE_ADDR *end)
{
  struct mem_region *region;
  unsigned blocksize;

  region = lookup_mem_region (address);
  gdb_assert (region->attrib.mode == MEM_FLASH);
  blocksize = region->attrib.blocksize;
  if (begin)
    *begin = address / blocksize * blocksize;
  if (end)
    *end = (address + blocksize - 1) / blocksize * blocksize;
}

/* Given the list of memory requests to be WRITTEN, this function
   returns write requests covering each group of flash blocks which must
   be erased.  */

static VEC(memory_write_request_s) *
blocks_to_erase (VEC(memory_write_request_s) *written)
{
  unsigned i;
  struct memory_write_request *ptr;

  VEC(memory_write_request_s) *result = NULL;

  for (i = 0; VEC_iterate (memory_write_request_s, written, i, ptr); ++i)
    {
      CORE_ADDR begin, end;

      block_boundaries (ptr->begin, &begin, 0);
      block_boundaries (ptr->end - 1, 0, &end);

      if (!VEC_empty (memory_write_request_s, result)
	  && VEC_last (memory_write_request_s, result)->end >= begin)
	{
	  VEC_last (memory_write_request_s, result)->end = end;
	}
      else
	{
	  struct memory_write_request *n =
	    VEC_safe_push (memory_write_request_s, result, NULL);
	  memset (n, 0, sizeof (struct memory_write_request));
	  n->begin = begin;
	  n->end = end;
	}
    }

  return result;
}

/* Given ERASED_BLOCKS, a list of blocks that will be erased with
   flash erase commands, and WRITTEN_BLOCKS, the list of memory
   addresses that will be written, compute the set of memory addresses
   that will be erased but not rewritten (e.g. padding within a block
   which is only partially filled by "load").  */

static VEC(memory_write_request_s) *
compute_garbled_blocks (VEC(memory_write_request_s) *erased_blocks,
			VEC(memory_write_request_s) *written_blocks)
{
  VEC(memory_write_request_s) *result = NULL;

  unsigned i, j;
  unsigned je = VEC_length (memory_write_request_s, written_blocks);
  struct memory_write_request *erased_p;

  /* Look at each erased memory_write_request in turn, and
     see what part of it is subsequently written to.

     This implementation is O(length(erased) * length(written)).  If
     the lists are sorted at this point it could be rewritten more
     efficiently, but the complexity is not generally worthwhile.  */

  for (i = 0;
       VEC_iterate (memory_write_request_s, erased_blocks, i, erased_p);
       ++i)
    {
      /* Make a deep copy -- it will be modified inside the loop, but
	 we don't want to modify original vector.  */
      struct memory_write_request erased = *erased_p;

      for (j = 0; j != je;)
	{
	  struct memory_write_request *written
	    = VEC_index (memory_write_request_s,
			 written_blocks, j);

	  /* Now try various cases.  */

	  /* If WRITTEN is fully to the left of ERASED, check the next
	     written memory_write_request.  */
	  if (written->end <= erased.begin)
	    {
	      ++j;
	      continue;
	    }

	  /* If WRITTEN is fully to the right of ERASED, then ERASED
	     is not written at all.  WRITTEN might affect other
	     blocks.  */
	  if (written->begin >= erased.end)
	    {
	      VEC_safe_push (memory_write_request_s, result, &erased);
	      goto next_erased;
	    }

	  /* If all of ERASED is completely written, we can move on to
	     the next erased region.  */
	  if (written->begin <= erased.begin
	      && written->end >= erased.end)
	    {
	      goto next_erased;
	    }

	  /* If there is an unwritten part at the beginning of ERASED,
	     then we should record that part and try this inner loop
	     again for the remainder.  */
	  if (written->begin > erased.begin)
	    {
	      struct memory_write_request *n =
		VEC_safe_push (memory_write_request_s, result, NULL);
	      memset (n, 0, sizeof (struct memory_write_request));
	      n->begin = erased.begin;
	      n->end = written->begin;
	      erased.begin = written->begin;
	      continue;
	    }

	  /* If there is an unwritten part at the end of ERASED, we
	     forget about the part that was written to and wait to see
	     if the next write request writes more of ERASED.  We can't
	     push it yet.  */
	  if (written->end < erased.end)
	    {
	      erased.begin = written->end;
	      ++j;
	      continue;
	    }
	}

      /* If we ran out of write requests without doing anything about
	 ERASED, then that means it's really erased.  */
      VEC_safe_push (memory_write_request_s, result, &erased);

    next_erased:
      ;
    }

  return result;
}

static void
cleanup_request_data (void *p)
{
  VEC(memory_write_request_s) **v = p;
  struct memory_write_request *r;
  int i;

  for (i = 0; VEC_iterate (memory_write_request_s, *v, i, r); ++i)
    xfree (r->data);
}

static void
cleanup_write_requests_vector (void *p)
{
  VEC(memory_write_request_s) **v = p;
  VEC_free (memory_write_request_s, *v);
}

int
target_write_memory_blocks (VEC(memory_write_request_s) *requests,
			    enum flash_preserve_mode preserve_flash_p,
			    void (*progress_cb) (ULONGEST, void *))
{
  struct cleanup *back_to = make_cleanup (null_cleanup, NULL);
  VEC(memory_write_request_s) *blocks = VEC_copy (memory_write_request_s,
						  requests);
  unsigned i;
  int err = 0;
  struct memory_write_request *r;
  VEC(memory_write_request_s) *regular = NULL;
  VEC(memory_write_request_s) *flash = NULL;
  VEC(memory_write_request_s) *erased, *garbled;

  /* END == 0 would represent wraparound: a write to the very last
     byte of the address space.  This file was not written with that
     possibility in mind.  This is fixable, but a lot of work for a
     rare problem; so for now, fail noisily here instead of obscurely
     later.  */
  for (i = 0; VEC_iterate (memory_write_request_s, requests, i, r); ++i)
    gdb_assert (r->end != 0);

  make_cleanup (cleanup_write_requests_vector, &blocks);

  /* Sort the blocks by their start address.  */
  qsort (VEC_address (memory_write_request_s, blocks),
	 VEC_length (memory_write_request_s, blocks),
	 sizeof (struct memory_write_request), compare_block_starting_address);

  /* Split blocks into list of regular memory blocks,
     and list of flash memory blocks. */
  make_cleanup (cleanup_write_requests_vector, &regular);
  make_cleanup (cleanup_write_requests_vector, &flash);
  split_regular_and_flash_blocks (blocks, &regular, &flash);

  /* If a variable is added to forbid flash write, even during "load",
     it should be checked here.  Similarly, if this function is used
     for other situations besides "load" in which writing to flash
     is undesirable, that should be checked here.  */

  /* Find flash blocks to erase.  */
  erased = blocks_to_erase (flash);
  make_cleanup (cleanup_write_requests_vector, &erased);

  /* Find what flash regions will be erased, and not overwritten; then
     either preserve or discard the old contents.  */
  garbled = compute_garbled_blocks (erased, flash);
  make_cleanup (cleanup_request_data, &garbled);
  make_cleanup (cleanup_write_requests_vector, &garbled);

  if (!VEC_empty (memory_write_request_s, garbled))
    {
      if (preserve_flash_p == flash_preserve)
	{
	  struct memory_write_request *r;

	  /* Read in regions that must be preserved and add them to
	     the list of blocks we read.  */
	  for (i = 0; VEC_iterate (memory_write_request_s, garbled, i, r); ++i)
	    {
	      gdb_assert (r->data == NULL);
	      r->data = xmalloc (r->end - r->begin);
	      err = target_read_memory (r->begin, r->data, r->end - r->begin);
	      if (err != 0)
		goto out;

	      VEC_safe_push (memory_write_request_s, flash, r);
	    }

	  qsort (VEC_address (memory_write_request_s, flash),
		 VEC_length (memory_write_request_s, flash),
		 sizeof (struct memory_write_request), compare_block_starting_address);
	}
    }

  /* We could coalesce adjacent memory blocks here, to reduce the
     number of write requests for small sections.  However, we would
     have to reallocate and copy the data pointers, which could be
     large; large sections are more common in loadable objects than
     large numbers of small sections (although the reverse can be true
     in object files).  So, we issue at least one write request per
     passed struct memory_write_request.  The remote stub will still
     have the opportunity to batch flash requests.  */

  /* Write regular blocks.  */
  for (i = 0; VEC_iterate (memory_write_request_s, regular, i, r); ++i)
    {
      LONGEST len;

      len = target_write_with_progress (&current_target,
					TARGET_OBJECT_MEMORY, NULL,
					r->data, r->begin, r->end - r->begin,
					progress_cb, r->baton);
      if (len < (LONGEST) (r->end - r->begin))
	{
	  /* Call error?  */
	  err = -1;
	  goto out;
	}
    }

  if (!VEC_empty (memory_write_request_s, erased))
    {
      /* Erase all pages.  */
      for (i = 0; VEC_iterate (memory_write_request_s, erased, i, r); ++i)
	target_flash_erase (r->begin, r->end - r->begin);

      /* Write flash data.  */
      for (i = 0; VEC_iterate (memory_write_request_s, flash, i, r); ++i)
	{
	  LONGEST len;

	  len = target_write_with_progress (&current_target,
					    TARGET_OBJECT_FLASH, NULL,
					    r->data, r->begin, r->end - r->begin,
					    progress_cb, r->baton);
	  if (len < (LONGEST) (r->end - r->begin))
	    error (_("Error writing data to flash"));
	}

      target_flash_done ();
    }

 out:
  do_cleanups (back_to);

  return err;
}
