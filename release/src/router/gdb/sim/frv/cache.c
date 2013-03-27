/* frv cache model.
   Copyright (C) 1999, 2000, 2001, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU simulators.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "libiberty.h"
#include "sim-main.h"
#include "cache.h"
#include "bfd.h"

void
frv_cache_init (SIM_CPU *cpu, FRV_CACHE *cache)
{
  int elements;
  int i, j;
  SIM_DESC sd;

  /* Set defaults for fields which are not initialized.  */
  sd = CPU_STATE (cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      if (cache->configured_sets == 0)
	cache->configured_sets = 512;
      if (cache->configured_ways == 0)
	cache->configured_ways = 2;
      if (cache->line_size == 0)
	cache->line_size = 32;
      if (cache->memory_latency == 0)
	cache->memory_latency = 20;
      break;
    case bfd_mach_fr550:
      if (cache->configured_sets == 0)
	cache->configured_sets = 128;
      if (cache->configured_ways == 0)
	cache->configured_ways = 4;
      if (cache->line_size == 0)
	cache->line_size = 64;
      if (cache->memory_latency == 0)
	cache->memory_latency = 20;
      break;
    default:
      if (cache->configured_sets == 0)
	cache->configured_sets = 64;
      if (cache->configured_ways == 0)
	cache->configured_ways = 4;
      if (cache->line_size == 0)
	cache->line_size = 64;
      if (cache->memory_latency == 0)
	cache->memory_latency = 20;
      break;
    }

  frv_cache_reconfigure (cpu, cache);

  /* First allocate the cache storage based on the given dimensions.  */
  elements = cache->sets * cache->ways;
  cache->tag_storage = (FRV_CACHE_TAG *)
    zalloc (elements * sizeof (*cache->tag_storage));
  cache->data_storage = (char *) xmalloc (elements * cache->line_size);

  /* Initialize the pipelines and status buffers.  */
  for (i = LS; i < FRV_CACHE_PIPELINES; ++i)
    {
      cache->pipeline[i].requests = NULL;
      cache->pipeline[i].status.flush.valid = 0;
      cache->pipeline[i].status.return_buffer.valid = 0;
      cache->pipeline[i].status.return_buffer.data
	= (char *) xmalloc (cache->line_size);
      for (j = FIRST_STAGE; j < FRV_CACHE_STAGES; ++j)
	cache->pipeline[i].stages[j].request = NULL;
    }
  cache->BARS.valid = 0;
  cache->NARS.valid = 0;

  /* Now set the cache state.  */
  cache->cpu = cpu;
  cache->statistics.accesses = 0;
  cache->statistics.hits = 0;
}

void
frv_cache_term (FRV_CACHE *cache)
{
  /* Free the cache storage.  */
  free (cache->tag_storage);
  free (cache->data_storage);
  free (cache->pipeline[LS].status.return_buffer.data);
  free (cache->pipeline[LD].status.return_buffer.data);
}

/* Reset the cache configuration based on registers in the cpu.  */
void
frv_cache_reconfigure (SIM_CPU *current_cpu, FRV_CACHE *cache)
{
  int ihsr8;
  int icdm;
  SIM_DESC sd;

  /* Set defaults for fields which are not initialized.  */
  sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr550:
      if (cache == CPU_INSN_CACHE (current_cpu))
	{
	  ihsr8 = GET_IHSR8 ();
	  icdm = GET_IHSR8_ICDM (ihsr8);
	  /* If IHSR8.ICDM is set, then the cache becomes a one way cache.  */
	  if (icdm)
	    {
	      cache->sets = cache->sets * cache->ways;
	      cache->ways = 1;
	      break;
	    }
	}
      /* fall through */
    default:
      /* Set the cache to its original settings.  */
      cache->sets = cache->configured_sets;
      cache->ways = cache->configured_ways;
      break;
    }
}

/* Determine whether the given cache is enabled.  */
int
frv_cache_enabled (FRV_CACHE *cache)
{
  SIM_CPU *current_cpu = cache->cpu;
  int hsr0 = GET_HSR0 ();
  if (GET_HSR0_ICE (hsr0) && cache == CPU_INSN_CACHE (current_cpu))
    return 1;
  if (GET_HSR0_DCE (hsr0) && cache == CPU_DATA_CACHE (current_cpu))
    return 1;
  return 0;
}

/* Determine whether the given address is RAM access, assuming that HSR0.RME
   is set.  */
static int
ram_access (FRV_CACHE *cache, USI address) 
{
  int ihsr8;
  int cwe;
  USI start, end, way_size;
  SIM_CPU *current_cpu = cache->cpu;
  SIM_DESC sd = CPU_STATE (current_cpu);

  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr550:
      /* IHSR8.DCWE or IHSR8.ICWE deternines which ways get RAM access.  */
      ihsr8 = GET_IHSR8 ();
      if (cache == CPU_INSN_CACHE (current_cpu))
	{
	  start = 0xfe000000;
	  end = 0xfe008000;
	  cwe = GET_IHSR8_ICWE (ihsr8);
	}
      else
	{
	  start = 0xfe400000;
	  end = 0xfe408000;
	  cwe = GET_IHSR8_DCWE (ihsr8);
	}
      way_size = (end - start) / 4;
      end -= way_size * cwe;
      return address >= start && address < end;
    default:
      break;
    }

  return 1; /* RAM access */
}

/* Determine whether the given address should be accessed without using
   the cache.  */
static int
non_cache_access (FRV_CACHE *cache, USI address) 
{
  int hsr0;
  SIM_DESC sd;
  SIM_CPU *current_cpu = cache->cpu;

  sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      if (address >= 0xff000000
	  || address >= 0xfe000000 && address <= 0xfeffffff)
	return 1; /* non-cache access */
      break;
    case bfd_mach_fr550:
      if (address >= 0xff000000
	  || address >= 0xfeff0000 && address <= 0xfeffffff)
	return 1; /* non-cache access */
      if (cache == CPU_INSN_CACHE (current_cpu))
	{
	  if (address >= 0xfe000000 && address <= 0xfe007fff)
	    return 1; /* non-cache access */
	}
      else if (address >= 0xfe400000 && address <= 0xfe407fff)
	return 1; /* non-cache access */
      break;
    default:
      if (address >= 0xff000000
	  || address >= 0xfeff0000 && address <= 0xfeffffff)
	return 1; /* non-cache access */
      if (cache == CPU_INSN_CACHE (current_cpu))
	{
	  if (address >= 0xfe000000 && address <= 0xfe003fff)
	    return 1; /* non-cache access */
	}
      else if (address >= 0xfe400000 && address <= 0xfe403fff)
	return 1; /* non-cache access */
      break;
    }

  hsr0 = GET_HSR0 ();
  if (GET_HSR0_RME (hsr0))
    return ram_access (cache, address);

  return 0; /* cache-access */
}

/* Find the cache line corresponding to the given address.
   If it is found then 'return_tag' is set to point to the tag for that line
   and 1 is returned.
   If it is not found, 'return_tag' is set to point to the tag for the least
   recently used line and 0 is returned.
*/
static int
get_tag (FRV_CACHE *cache, SI address, FRV_CACHE_TAG **return_tag)
{
  int set;
  int way;
  int bits;
  USI tag;
  FRV_CACHE_TAG *found;
  FRV_CACHE_TAG *available;

  ++cache->statistics.accesses;

  /* First calculate which set this address will fall into. Do this by
     shifting out the bits representing the offset within the line and
     then keeping enough bits to index the set.  */
  set = address & ~(cache->line_size - 1);
  for (bits = cache->line_size - 1; bits != 0; bits >>= 1)
    set >>= 1;
  set &= (cache->sets - 1);
  
  /* Now search the set for a valid tag which matches this address.  At the
     same time make note of the least recently used tag, which we will return
     if no match is found.  */
  available = NULL;
  tag = CACHE_ADDRESS_TAG (cache, address);
  for (way = 0; way < cache->ways; ++way)
    {
      found = CACHE_TAG (cache, set, way);
      /* This tag is available as the least recently used if it is the
	 least recently used seen so far and it is not locked.  */
      if (! found->locked && (available == NULL || available->lru > found->lru))
	available = found;
      if (found->valid && found->tag == tag)
	{
	  *return_tag = found;
	  ++cache->statistics.hits;
	  return 1; /* found it */
	}
    }

  *return_tag = available;
  return 0; /* not found */
}

/* Write the given data out to memory.  */
static void
write_data_to_memory (FRV_CACHE *cache, SI address, char *data, int length)
{
  SIM_CPU *cpu = cache->cpu;
  IADDR pc = CPU_PC_GET (cpu);
  int write_index = 0;

  switch (length)
    {
    case 1:
    default:
      PROFILE_COUNT_WRITE (cpu, address, MODE_QI);
      break;
    case 2:
      PROFILE_COUNT_WRITE (cpu, address, MODE_HI);
      break;
    case 4:
      PROFILE_COUNT_WRITE (cpu, address, MODE_SI);
      break;
    case 8:
      PROFILE_COUNT_WRITE (cpu, address, MODE_DI);
      break;
    }

  for (write_index = 0; write_index < length; ++write_index)
    {
      /* TODO: Better way to copy memory than a byte at a time?  */
      sim_core_write_unaligned_1 (cpu, pc, write_map, address + write_index,
				  data[write_index]);
    }
}

/* Write a cache line out to memory.  */
static void
write_line_to_memory (FRV_CACHE *cache, FRV_CACHE_TAG *tag)
{
  SI address = tag->tag;
  int set = CACHE_TAG_SET_NUMBER (cache, tag);
  int bits;
  for (bits = cache->line_size - 1; bits != 0; bits >>= 1)
    set <<= 1;
  address |= set;
  write_data_to_memory (cache, address, tag->line, cache->line_size);
}

static void
read_data_from_memory (SIM_CPU *current_cpu, SI address, char *buffer,
		       int length)
{
  PCADDR pc = CPU_PC_GET (current_cpu);
  int i;
  PROFILE_COUNT_READ (current_cpu, address, MODE_QI);
  for (i = 0; i < length; ++i)
    {
      /* TODO: Better way to copy memory than a byte at a time?  */
      buffer[i] = sim_core_read_unaligned_1 (current_cpu, pc, read_map,
					     address + i);
    }
}

/* Fill the given cache line from memory.  */
static void
fill_line_from_memory (FRV_CACHE *cache, FRV_CACHE_TAG *tag, SI address)
{
  PCADDR pc;
  int line_alignment;
  SI read_address;
  SIM_CPU *current_cpu = cache->cpu;

  /* If this line is already valid and the cache is in copy-back mode, then
     write this line to memory before refilling it.
     Check the dirty bit first, since it is less likely to be set.  */
  if (tag->dirty && tag->valid)
    {
      int hsr0 = GET_HSR0 ();
      if (GET_HSR0_CBM (hsr0))
	write_line_to_memory (cache, tag);
    }
  else if (tag->line == NULL)
    {
      int line_index = tag - cache->tag_storage;
      tag->line = cache->data_storage + (line_index * cache->line_size);
    }

  pc = CPU_PC_GET (current_cpu);
  line_alignment = cache->line_size - 1;
  read_address = address & ~line_alignment;
  read_data_from_memory (current_cpu, read_address, tag->line,
			 cache->line_size);
  tag->tag = CACHE_ADDRESS_TAG (cache, address);
  tag->valid = 1;
}

/* Update the LRU information for the tags in the same set as the given tag.  */
static void
set_most_recently_used (FRV_CACHE *cache, FRV_CACHE_TAG *tag)
{
  /* All tags in the same set are contiguous, so find the beginning of the
     set by aligning to the size of a set.  */
  FRV_CACHE_TAG *item = cache->tag_storage + CACHE_TAG_SET_START (cache, tag);
  FRV_CACHE_TAG *limit = item + cache->ways;

  while (item < limit)
    {
      if (item->lru > tag->lru)
	--item->lru;
      ++item;
    }
  tag->lru = cache->ways; /* Mark as most recently used.  */
}

/* Update the LRU information for the tags in the same set as the given tag.  */
static void
set_least_recently_used (FRV_CACHE *cache, FRV_CACHE_TAG *tag)
{
  /* All tags in the same set are contiguous, so find the beginning of the
     set by aligning to the size of a set.  */
  FRV_CACHE_TAG *item = cache->tag_storage + CACHE_TAG_SET_START (cache, tag);
  FRV_CACHE_TAG *limit = item + cache->ways;

  while (item < limit)
    {
      if (item->lru != 0 && item->lru < tag->lru)
	++item->lru;
      ++item;
    }
  tag->lru = 0; /* Mark as least recently used.  */
}

/* Find the line containing the given address and load it if it is not
   already loaded.
   Returns the tag of the requested line.  */
static FRV_CACHE_TAG *
find_or_retrieve_cache_line (FRV_CACHE *cache, SI address)
{
  /* See if this data is already in the cache.  */
  FRV_CACHE_TAG *tag;
  int found = get_tag (cache, address, &tag);

  /* Fill the line from memory, if it is not valid.  */
  if (! found)
    {
      /* The tag could be NULL is all ways in the set were used and locked.  */
      if (tag == NULL)
	return tag;

      fill_line_from_memory (cache, tag, address);
      tag->dirty = 0;
    }

  /* Update the LRU information for the tags in this set.  */
  set_most_recently_used (cache, tag);

  return tag;
}

static void
copy_line_to_return_buffer (FRV_CACHE *cache, int pipe, FRV_CACHE_TAG *tag,
			    SI address)
{
  /* A cache line was available for the data.
     Copy the data from the cache line to the output buffer.  */
  memcpy (cache->pipeline[pipe].status.return_buffer.data,
	  tag->line, cache->line_size);
  cache->pipeline[pipe].status.return_buffer.address
    = address & ~(cache->line_size - 1);
  cache->pipeline[pipe].status.return_buffer.valid = 1;
}

static void
copy_memory_to_return_buffer (FRV_CACHE *cache, int pipe, SI address)
{
  address &= ~(cache->line_size - 1);
  read_data_from_memory (cache->cpu, address,
			 cache->pipeline[pipe].status.return_buffer.data,
			 cache->line_size);
  cache->pipeline[pipe].status.return_buffer.address = address;
  cache->pipeline[pipe].status.return_buffer.valid = 1;
}

static void
set_return_buffer_reqno (FRV_CACHE *cache, int pipe, unsigned reqno)
{
  cache->pipeline[pipe].status.return_buffer.reqno = reqno;
}

/* Read data from the given cache.
   Returns the number of cycles required to obtain the data.  */
int
frv_cache_read (FRV_CACHE *cache, int pipe, SI address)
{
  FRV_CACHE_TAG *tag;

  if (non_cache_access (cache, address))
    {
      copy_memory_to_return_buffer (cache, pipe, address);
      return 1;
    }
	
  tag = find_or_retrieve_cache_line (cache, address);

  if (tag == NULL)
    return 0; /* Indicate non-cache-access.  */

  /* A cache line was available for the data.
     Copy the data from the cache line to the output buffer.  */
  copy_line_to_return_buffer (cache, pipe, tag, address);

  return 1; /* TODO - number of cycles unknown */
}

/* Writes data through the given cache.
   The data is assumed to be in target endian order.
   Returns the number of cycles required to write the data.  */
int
frv_cache_write (FRV_CACHE *cache, SI address, char *data, unsigned length)
{
  int copy_back;

  /* See if this data is already in the cache.  */
  SIM_CPU *current_cpu = cache->cpu;
  USI hsr0 = GET_HSR0 ();
  FRV_CACHE_TAG *tag;
  int found;

  if (non_cache_access (cache, address))
    {
      write_data_to_memory (cache, address, data, length);
      return 1;
    }

  found = get_tag (cache, address, &tag);

  /* Write the data to the cache line if one was available and if it is
     either a hit or a miss in copy-back mode.
     The tag may be NULL if all ways were in use and locked on a miss.
  */
  copy_back = GET_HSR0_CBM (GET_HSR0 ());
  if (tag != NULL && (found || copy_back))
    {
      int line_offset;
      /* Load the line from memory first, if it was a miss.  */
      if (! found)
	fill_line_from_memory (cache, tag, address);
      line_offset = address & (cache->line_size - 1);
      memcpy (tag->line + line_offset, data, length);
      tag->dirty = 1;

      /* Update the LRU information for the tags in this set.  */
      set_most_recently_used (cache, tag);
    }

  /* Write the data to memory if there was no line available or we are in
     write-through (not copy-back mode).  */
  if (tag == NULL || ! copy_back)
    {
      write_data_to_memory (cache, address, data, length);
      if (tag != NULL)
	tag->dirty = 0;
    }

  return 1; /* TODO - number of cycles unknown */
}

/* Preload the cache line containing the given address. Lock the
   data if requested.
   Returns the number of cycles required to write the data.  */
int
frv_cache_preload (FRV_CACHE *cache, SI address, USI length, int lock)
{
  int offset;
  int lines;

  if (non_cache_access (cache, address))
    return 1;

  /* preload at least 1 line.  */
  if (length == 0)
    length = 1;

  offset = address & (cache->line_size - 1);
  lines = 1 + (offset + length - 1) / cache->line_size;

  /* Careful with this loop -- length is unsigned.  */
  for (/**/; lines > 0; --lines)
    {
      FRV_CACHE_TAG *tag = find_or_retrieve_cache_line (cache, address);
      if (lock && tag != NULL)
	tag->locked = 1;
      address += cache->line_size;
    }

  return 1; /* TODO - number of cycles unknown */
}

/* Unlock the cache line containing the given address.
   Returns the number of cycles required to unlock the line.  */
int
frv_cache_unlock (FRV_CACHE *cache, SI address)
{
  FRV_CACHE_TAG *tag;
  int found;

  if (non_cache_access (cache, address))
    return 1;

  found = get_tag (cache, address, &tag);

  if (found)
    tag->locked = 0;

  return 1; /* TODO - number of cycles unknown */
}

static void
invalidate_return_buffer (FRV_CACHE *cache, SI address)
{
  /* If this address is in one of the return buffers, then invalidate that
     return buffer.  */
  address &= ~(cache->line_size - 1);
  if (address == cache->pipeline[LS].status.return_buffer.address)
    cache->pipeline[LS].status.return_buffer.valid = 0;
  if (address == cache->pipeline[LD].status.return_buffer.address)
    cache->pipeline[LD].status.return_buffer.valid = 0;
}

/* Invalidate the cache line containing the given address. Flush the
   data if requested.
   Returns the number of cycles required to write the data.  */
int
frv_cache_invalidate (FRV_CACHE *cache, SI address, int flush)
{
  /* See if this data is already in the cache.  */
  FRV_CACHE_TAG *tag;
  int found;

  /* Check for non-cache access.  This operation is still perfromed even if
     the cache is not currently enabled.  */
  if (non_cache_access (cache, address))
    return 1;

  /* If the line is found, invalidate it. If a flush is requested, then flush
     it if it is dirty.  */
  found = get_tag (cache, address, &tag);
  if (found)
    {
      SIM_CPU *cpu;
      /* If a flush is requested, then flush it if it is dirty.  */
      if (tag->dirty && flush)
	write_line_to_memory (cache, tag);
      set_least_recently_used (cache, tag);
      tag->valid = 0;
      tag->locked = 0;

      /* If this is the insn cache, then flush the cpu's scache as well.  */
      cpu = cache->cpu;
      if (cache == CPU_INSN_CACHE (cpu))
	scache_flush_cpu (cpu);
    }

  invalidate_return_buffer (cache, address);

  return 1; /* TODO - number of cycles unknown */
}

/* Invalidate the entire cache. Flush the data if requested.  */
int
frv_cache_invalidate_all (FRV_CACHE *cache, int flush)
{
  /* See if this data is already in the cache.  */
  int elements = cache->sets * cache->ways;
  FRV_CACHE_TAG *tag = cache->tag_storage;
  SIM_CPU *cpu;
  int i;

  for(i = 0; i < elements; ++i, ++tag)
    {
      /* If a flush is requested, then flush it if it is dirty.  */
      if (tag->valid && tag->dirty && flush)
	write_line_to_memory (cache, tag);
      tag->valid = 0;
      tag->locked = 0;
    }


  /* If this is the insn cache, then flush the cpu's scache as well.  */
  cpu = cache->cpu;
  if (cache == CPU_INSN_CACHE (cpu))
    scache_flush_cpu (cpu);

  /* Invalidate both return buffers.  */
  cache->pipeline[LS].status.return_buffer.valid = 0;
  cache->pipeline[LD].status.return_buffer.valid = 0;

  return 1; /* TODO - number of cycles unknown */
}

/* ---------------------------------------------------------------------------
   Functions for operating the cache in cycle accurate mode.
   -------------------------------------------------------------------------  */
/* Convert a VLIW slot to a cache pipeline index.  */
static int
convert_slot_to_index (int slot)
{
  switch (slot)
    {
    case UNIT_I0:
    case UNIT_C:
      return LS;
    case UNIT_I1:
      return LD;
    default:
      abort ();
    }
  return 0;
}

/* Allocate free chains of cache requests.  */
#define FREE_CHAIN_SIZE 16
static FRV_CACHE_REQUEST *frv_cache_request_free_chain = NULL;
static FRV_CACHE_REQUEST *frv_store_request_free_chain = NULL;

static void
allocate_new_cache_requests (void)
{
  int i;
  frv_cache_request_free_chain = xmalloc (FREE_CHAIN_SIZE
					  * sizeof (FRV_CACHE_REQUEST));
  for (i = 0; i < FREE_CHAIN_SIZE - 1; ++i)
    {
      frv_cache_request_free_chain[i].next
	= & frv_cache_request_free_chain[i + 1]; 
    }

  frv_cache_request_free_chain[FREE_CHAIN_SIZE - 1].next = NULL;
}

/* Return the next free request in the queue for the given cache pipeline.  */
static FRV_CACHE_REQUEST *
new_cache_request (void)
{
  FRV_CACHE_REQUEST *req;

  /* Allocate new elements for the free chain if necessary.  */
  if (frv_cache_request_free_chain == NULL)
    allocate_new_cache_requests ();

  req = frv_cache_request_free_chain;
  frv_cache_request_free_chain = req->next;

  return req;
}

/* Return the given cache request to the free chain.  */
static void
free_cache_request (FRV_CACHE_REQUEST *req)
{
  if (req->kind == req_store)
    {
      req->next = frv_store_request_free_chain;
      frv_store_request_free_chain = req;
    }
  else
    {
      req->next = frv_cache_request_free_chain;
      frv_cache_request_free_chain = req;
    }
}

/* Search the free chain for an existing store request with a buffer that's
   large enough.  */
static FRV_CACHE_REQUEST *
new_store_request (int length)
{
  FRV_CACHE_REQUEST *prev = NULL;
  FRV_CACHE_REQUEST *req;
  for (req = frv_store_request_free_chain; req != NULL; req = req->next)
    {
      if (req->u.store.length == length)
	break;
      prev = req;
    }
  if (req != NULL)
    {
      if (prev == NULL)
	frv_store_request_free_chain = req->next;
      else
	prev->next = req->next;
      return req;
    }

  /* No existing request buffer was found, so make a new one.  */
  req = new_cache_request ();
  req->kind = req_store;
  req->u.store.data = xmalloc (length);
  req->u.store.length = length;
  return req;
}

/* Remove the given request from the given pipeline.  */
static void
pipeline_remove_request (FRV_CACHE_PIPELINE *p, FRV_CACHE_REQUEST *request)
{
  FRV_CACHE_REQUEST *next = request->next;
  FRV_CACHE_REQUEST *prev = request->prev;

  if (prev == NULL)
    p->requests = next;
  else
    prev->next = next;

  if (next != NULL)
    next->prev = prev;
}

/* Add the given request to the given pipeline.  */
static void
pipeline_add_request (FRV_CACHE_PIPELINE *p, FRV_CACHE_REQUEST *request)
{
  FRV_CACHE_REQUEST *prev = NULL;
  FRV_CACHE_REQUEST *item;

  /* Add the request in priority order.  0 is the highest priority.  */
  for (item = p->requests; item != NULL; item = item->next)
    {
      if (item->priority > request->priority)
	break;
      prev = item;
    }

  request->next = item;
  request->prev = prev;
  if (prev == NULL)
    p->requests = request;
  else
    prev->next = request;
  if (item != NULL)
    item->prev = request;
}

/* Requeu the given request from the last of the given pipeline.  */
static void
pipeline_requeue_request (FRV_CACHE_PIPELINE *p)
{
  FRV_CACHE_STAGE *stage = & p->stages[LAST_STAGE];
  FRV_CACHE_REQUEST *req = stage->request;
  stage->request = NULL;
  pipeline_add_request (p, req);
}

/* Return the priority lower than the lowest one in this cache pipeline.
   0 is the highest priority.  */
static int
next_priority (FRV_CACHE *cache, FRV_CACHE_PIPELINE *pipeline)
{
  int i, j;
  int pipe;
  int lowest = 0;
  FRV_CACHE_REQUEST *req;

  /* Check the priorities of any queued items.  */
  for (req = pipeline->requests; req != NULL; req = req->next)
    if (req->priority > lowest)
      lowest = req->priority;

  /* Check the priorities of items in the pipeline stages.  */
  for (i = FIRST_STAGE; i < FRV_CACHE_STAGES; ++i)
    {
      FRV_CACHE_STAGE *stage = & pipeline->stages[i];
      if (stage->request != NULL && stage->request->priority > lowest)
        lowest = stage->request->priority;
    }

  /* Check the priorities of load requests waiting in WAR.  These are one
     higher than the request that spawned them.  */
  for (i = 0; i < NUM_WARS; ++i)
    {
      FRV_CACHE_WAR *war = & pipeline->WAR[i];
      if (war->valid && war->priority > lowest)
	lowest = war->priority + 1;
    }

  /* Check the priorities of any BARS or NARS associated with this pipeline.
     These are one higher than the request that spawned them.  */
  pipe = pipeline - cache->pipeline;
  if (cache->BARS.valid && cache->BARS.pipe == pipe 
      && cache->BARS.priority > lowest)
    lowest = cache->BARS.priority + 1;
  if (cache->NARS.valid && cache->NARS.pipe == pipe 
      && cache->NARS.priority > lowest)
    lowest = cache->NARS.priority + 1;

  /* Return a priority 2 lower than the lowest found.  This allows a WAR
     request to be generated with a priority greater than this but less than
     the next higher priority request.  */
  return lowest + 2;
}

static void
add_WAR_request (FRV_CACHE_PIPELINE* pipeline, FRV_CACHE_WAR *war)
{
  /* Add the load request to the indexed pipeline.  */
  FRV_CACHE_REQUEST *req = new_cache_request ();
  req->kind = req_WAR;
  req->reqno = war->reqno;
  req->priority = war->priority;
  req->address = war->address;
  req->u.WAR.preload = war->preload;
  req->u.WAR.lock = war->lock;
  pipeline_add_request (pipeline, req);
}

/* Remove the next request from the given pipeline and return it.  */
static FRV_CACHE_REQUEST *
pipeline_next_request (FRV_CACHE_PIPELINE *p)
{
  FRV_CACHE_REQUEST *first = p->requests;
  if (first != NULL)
    pipeline_remove_request (p, first);
  return first;
}

/* Return the request which is at the given stage of the given pipeline.  */
static FRV_CACHE_REQUEST *
pipeline_stage_request (FRV_CACHE_PIPELINE *p, int stage)
{
  return p->stages[stage].request;
}

static void
advance_pipelines (FRV_CACHE *cache)
{
  int stage;
  int pipe;
  FRV_CACHE_PIPELINE *pipelines = cache->pipeline;

  /* Free the final stage requests.  */
  for (pipe = 0; pipe < FRV_CACHE_PIPELINES; ++pipe)
    {
      FRV_CACHE_REQUEST *req = pipelines[pipe].stages[LAST_STAGE].request;
      if (req != NULL)
	free_cache_request (req);
    }

  /* Shuffle the requests along the pipeline.  */
  for (stage = LAST_STAGE; stage > FIRST_STAGE; --stage)
    {
      for (pipe = 0; pipe < FRV_CACHE_PIPELINES; ++pipe)
	pipelines[pipe].stages[stage] = pipelines[pipe].stages[stage - 1];
    }

  /* Add a new request to the pipeline.  */
  for (pipe = 0; pipe < FRV_CACHE_PIPELINES; ++pipe)
    pipelines[pipe].stages[FIRST_STAGE].request
      = pipeline_next_request (& pipelines[pipe]);
}

/* Handle a request for a load from the given address.  */
void
frv_cache_request_load (FRV_CACHE *cache, unsigned reqno, SI address, int slot)
{
  FRV_CACHE_REQUEST *req;

  /* slot is a UNIT_*.  Convert it to a cache pipeline index.  */
  int pipe = convert_slot_to_index (slot);
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Add the load request to the indexed pipeline.  */
  req = new_cache_request ();
  req->kind = req_load;
  req->reqno = reqno;
  req->priority = next_priority (cache, pipeline);
  req->address = address;

  pipeline_add_request (pipeline, req);
}

void
frv_cache_request_store (FRV_CACHE *cache, SI address,
			 int slot, char *data, unsigned length)
{
  FRV_CACHE_REQUEST *req;

  /* slot is a UNIT_*.  Convert it to a cache pipeline index.  */
  int pipe = convert_slot_to_index (slot);
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Add the load request to the indexed pipeline.  */
  req = new_store_request (length);
  req->kind = req_store;
  req->reqno = NO_REQNO;
  req->priority = next_priority (cache, pipeline);
  req->address = address;
  req->u.store.length = length;
  memcpy (req->u.store.data, data, length);

  pipeline_add_request (pipeline, req);
  invalidate_return_buffer (cache, address);
}

/* Handle a request to invalidate the cache line containing the given address.
   Flush the data if requested.  */
void
frv_cache_request_invalidate (FRV_CACHE *cache, unsigned reqno, SI address,
			      int slot, int all, int flush)
{
  FRV_CACHE_REQUEST *req;

  /* slot is a UNIT_*.  Convert it to a cache pipeline index.  */
  int pipe = convert_slot_to_index (slot);
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Add the load request to the indexed pipeline.  */
  req = new_cache_request ();
  req->kind = req_invalidate;
  req->reqno = reqno;
  req->priority = next_priority (cache, pipeline);
  req->address = address;
  req->u.invalidate.all = all;
  req->u.invalidate.flush = flush;

  pipeline_add_request (pipeline, req);
}

/* Handle a request to preload the cache line containing the given address.  */
void
frv_cache_request_preload (FRV_CACHE *cache, SI address,
			   int slot, int length, int lock)
{
  FRV_CACHE_REQUEST *req;

  /* slot is a UNIT_*.  Convert it to a cache pipeline index.  */
  int pipe = convert_slot_to_index (slot);
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Add the load request to the indexed pipeline.  */
  req = new_cache_request ();
  req->kind = req_preload;
  req->reqno = NO_REQNO;
  req->priority = next_priority (cache, pipeline);
  req->address = address;
  req->u.preload.length = length;
  req->u.preload.lock = lock;

  pipeline_add_request (pipeline, req);
  invalidate_return_buffer (cache, address);
}

/* Handle a request to unlock the cache line containing the given address.  */
void
frv_cache_request_unlock (FRV_CACHE *cache, SI address, int slot)
{
  FRV_CACHE_REQUEST *req;

  /* slot is a UNIT_*.  Convert it to a cache pipeline index.  */
  int pipe = convert_slot_to_index (slot);
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Add the load request to the indexed pipeline.  */
  req = new_cache_request ();
  req->kind = req_unlock;
  req->reqno = NO_REQNO;
  req->priority = next_priority (cache, pipeline);
  req->address = address;

  pipeline_add_request (pipeline, req);
}

/* Check whether this address interferes with a pending request of
   higher priority.  */
static int
address_interference (FRV_CACHE *cache, SI address, FRV_CACHE_REQUEST *req,
		      int pipe)
{
  int i, j;
  int line_mask = ~(cache->line_size - 1);
  int other_pipe;
  int priority = req->priority;
  FRV_CACHE_REQUEST *other_req;
  SI other_address;
  SI all_address;

  address &= line_mask;
  all_address = -1 & line_mask;

  /* Check for collisions in the queue for this pipeline.  */
  for (other_req = cache->pipeline[pipe].requests;
       other_req != NULL;
       other_req = other_req->next)
    {
      other_address = other_req->address & line_mask;
      if ((address == other_address || address == all_address)
	  && priority > other_req->priority)
	return 1;
    }

  /* Check for a collision in the the other pipeline.  */
  other_pipe = pipe ^ 1;
  other_req = cache->pipeline[other_pipe].stages[LAST_STAGE].request;
  if (other_req != NULL)
    {
      other_address = other_req->address & line_mask;
      if (address == other_address || address == all_address)
	return 1;
    }

  /* Check for a collision with load requests waiting in WAR.  */
  for (i = LS; i < FRV_CACHE_PIPELINES; ++i)
    {
      for (j = 0; j < NUM_WARS; ++j)
	{
	  FRV_CACHE_WAR *war = & cache->pipeline[i].WAR[j];
	  if (war->valid
	      && (address == (war->address & line_mask) 
		  || address == all_address)
	      && priority > war->priority)
	    return 1;
	}
      /* If this is not a WAR request, then yield to any WAR requests in
	 either pipeline or to a higher priority request in the same pipeline.
      */
      if (req->kind != req_WAR)
	{
	  for (j = FIRST_STAGE; j < FRV_CACHE_STAGES; ++j)
	    {
	      other_req = cache->pipeline[i].stages[j].request;
	      if (other_req != NULL)
		{
		  if (other_req->kind == req_WAR)
		    return 1;
		  if (i == pipe
		      && (address == (other_req->address & line_mask) 
			  || address == all_address)
		      && priority > other_req->priority)
		    return 1;
		}
	    }
	}
    }

  /* Check for a collision with load requests waiting in ARS.  */
  if (cache->BARS.valid
      && (address == (cache->BARS.address & line_mask)
	  || address == all_address)
      && priority > cache->BARS.priority)
    return 1;
  if (cache->NARS.valid
      && (address == (cache->NARS.address & line_mask)
	  || address == all_address)
      && priority > cache->NARS.priority)
    return 1;

  return 0;
}

/* Wait for a free WAR register in BARS or NARS.  */
static void
wait_for_WAR (FRV_CACHE* cache, int pipe, FRV_CACHE_REQUEST *req)
{
  FRV_CACHE_WAR war;
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  if (! cache->BARS.valid)
    {
      cache->BARS.pipe = pipe;
      cache->BARS.reqno = req->reqno;
      cache->BARS.address = req->address;
      cache->BARS.priority = req->priority - 1;
      switch (req->kind)
	{
	case req_load:
	  cache->BARS.preload = 0;
	  cache->BARS.lock = 0;
	  break;
	case req_store:
	  cache->BARS.preload = 1;
	  cache->BARS.lock = 0;
	  break;
	case req_preload:
	  cache->BARS.preload = 1;
	  cache->BARS.lock = req->u.preload.lock;
	  break;
	}
      cache->BARS.valid = 1;
      return;
    }
  if (! cache->NARS.valid)
    {
      cache->NARS.pipe = pipe;
      cache->NARS.reqno = req->reqno;
      cache->NARS.address = req->address;
      cache->NARS.priority = req->priority - 1;
      switch (req->kind)
	{
	case req_load:
	  cache->NARS.preload = 0;
	  cache->NARS.lock = 0;
	  break;
	case req_store:
	  cache->NARS.preload = 1;
	  cache->NARS.lock = 0;
	  break;
	case req_preload:
	  cache->NARS.preload = 1;
	  cache->NARS.lock = req->u.preload.lock;
	  break;
	}
      cache->NARS.valid = 1;
      return;
    }
  /* All wait registers are busy, so resubmit this request.  */
  pipeline_requeue_request (pipeline);
}

/* Find a free WAR register and wait for memory to fetch the data.  */
static void
wait_in_WAR (FRV_CACHE* cache, int pipe, FRV_CACHE_REQUEST *req)
{
  int war;
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];

  /* Find a valid WAR to  hold this request.  */
  for (war = 0; war < NUM_WARS; ++war)
    if (! pipeline->WAR[war].valid)
      break;
  if (war >= NUM_WARS)
    {
      wait_for_WAR (cache, pipe, req);
      return;
    }

  pipeline->WAR[war].address = req->address;
  pipeline->WAR[war].reqno = req->reqno;
  pipeline->WAR[war].priority = req->priority - 1;
  pipeline->WAR[war].latency = cache->memory_latency + 1;
  switch (req->kind)
    {
    case req_load:
      pipeline->WAR[war].preload = 0;
      pipeline->WAR[war].lock = 0;
      break;
    case req_store:
      pipeline->WAR[war].preload = 1;
      pipeline->WAR[war].lock = 0;
      break;
    case req_preload:
      pipeline->WAR[war].preload = 1;
      pipeline->WAR[war].lock = req->u.preload.lock;
      break;
    }
  pipeline->WAR[war].valid = 1;
}

static void
handle_req_load (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  FRV_CACHE_TAG *tag;
  SI address = req->address;

  /* If this address interferes with an existing request, then requeue it.  */
  if (address_interference (cache, address, req, pipe))
    {
      pipeline_requeue_request (& cache->pipeline[pipe]);
      return;
    }

  if (frv_cache_enabled (cache) && ! non_cache_access (cache, address))
    {
      int found = get_tag (cache, address, &tag);

      /* If the data was found, return it to the caller.  */
      if (found)
	{
	  set_most_recently_used (cache, tag);
	  copy_line_to_return_buffer (cache, pipe, tag, address);
	  set_return_buffer_reqno (cache, pipe, req->reqno);
	  return;
	}
    }

  /* The data is not in the cache or this is a non-cache access.  We need to
     wait for the memory unit to fetch it.  Store this request in the WAR in
     the meantime.  */
  wait_in_WAR (cache, pipe, req);
}

static void
handle_req_preload (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  int found;
  FRV_CACHE_WAR war;
  FRV_CACHE_TAG *tag;
  int length;
  int lock;
  int offset;
  int lines;
  int line;
  SI address = req->address;
  SI cur_address;

  if (! frv_cache_enabled (cache) || non_cache_access (cache, address))
    return;

  /* preload at least 1 line.  */
  length = req->u.preload.length;
  if (length == 0)
    length = 1;

  /* Make sure that this request does not interfere with a pending request.  */
  offset = address & (cache->line_size - 1);
  lines = 1 + (offset + length - 1) / cache->line_size;
  cur_address = address & ~(cache->line_size - 1);
  for (line = 0; line < lines; ++line)
    {
      /* If this address interferes with an existing request,
	 then requeue it.  */
      if (address_interference (cache, cur_address, req, pipe))
	{
	  pipeline_requeue_request (& cache->pipeline[pipe]);
	  return;
	}
      cur_address += cache->line_size;
    }

  /* Now process each cache line.  */
  /* Careful with this loop -- length is unsigned.  */
  lock = req->u.preload.lock;
  cur_address = address & ~(cache->line_size - 1);
  for (line = 0; line < lines; ++line)
    {
      /* If the data was found, then lock it if requested.  */
      found = get_tag (cache, cur_address, &tag);
      if (found)
	{
	  if (lock)
	    tag->locked = 1;
	}
      else
	{
	  /* The data is not in the cache.  We need to wait for the memory
	     unit to fetch it.  Store this request in the WAR in the meantime.
	  */
	  wait_in_WAR (cache, pipe, req);
	}
      cur_address += cache->line_size;
    }
}

static void
handle_req_store (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  SIM_CPU *current_cpu;
  FRV_CACHE_TAG *tag;
  int found;
  int copy_back;
  SI address = req->address;
  char *data = req->u.store.data;
  int length = req->u.store.length;

  /* If this address interferes with an existing request, then requeue it.  */
  if (address_interference (cache, address, req, pipe))
    {
      pipeline_requeue_request (& cache->pipeline[pipe]);
      return;
    }

  /* Non-cache access. Write the data directly to memory.  */
  if (! frv_cache_enabled (cache) || non_cache_access (cache, address))
    {
      write_data_to_memory (cache, address, data, length);
      return;
    }

  /* See if the data is in the cache.  */
  found = get_tag (cache, address, &tag);

  /* Write the data to the cache line if one was available and if it is
     either a hit or a miss in copy-back mode.
     The tag may be NULL if all ways were in use and locked on a miss.
  */
  current_cpu = cache->cpu;
  copy_back = GET_HSR0_CBM (GET_HSR0 ());
  if (tag != NULL && (found || copy_back))
    {
      int line_offset;
      /* Load the line from memory first, if it was a miss.  */
      if (! found)
	{
	  /* We need to wait for the memory unit to fetch the data.  
	     Store this request in the WAR and requeue the store request.  */
	  wait_in_WAR (cache, pipe, req);
	  pipeline_requeue_request (& cache->pipeline[pipe]);
	  /* Decrement the counts of accesses and hits because when the requeued
	     request is processed again, it will appear to be a new access and
	     a hit.  */
	  --cache->statistics.accesses;
	  --cache->statistics.hits;
	  return;
	}
      line_offset = address & (cache->line_size - 1);
      memcpy (tag->line + line_offset, data, length);
      invalidate_return_buffer (cache, address);
      tag->dirty = 1;

      /* Update the LRU information for the tags in this set.  */
      set_most_recently_used (cache, tag);
    }

  /* Write the data to memory if there was no line available or we are in
     write-through (not copy-back mode).  */
  if (tag == NULL || ! copy_back)
    {
      write_data_to_memory (cache, address, data, length);
      if (tag != NULL)
	tag->dirty = 0;
    }
}

static void
handle_req_invalidate (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];
  SI address = req->address;
  SI interfere_address = req->u.invalidate.all ? -1 : address;

  /* If this address interferes with an existing request, then requeue it.  */
  if (address_interference (cache, interfere_address, req, pipe))
    {
      pipeline_requeue_request (pipeline);
      return;
    }

  /* Invalidate the cache line now.  This function already checks for
     non-cache access.  */
  if (req->u.invalidate.all)
    frv_cache_invalidate_all (cache, req->u.invalidate.flush);
  else
    frv_cache_invalidate (cache, address, req->u.invalidate.flush);
  if (req->u.invalidate.flush)
    {
      pipeline->status.flush.reqno = req->reqno;
      pipeline->status.flush.address = address;
      pipeline->status.flush.valid = 1;
    }
}

static void
handle_req_unlock (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];
  SI address = req->address;

  /* If this address interferes with an existing request, then requeue it.  */
  if (address_interference (cache, address, req, pipe))
    {
      pipeline_requeue_request (pipeline);
      return;
    }

  /* Unlock the cache line.  This function checks for non-cache access.  */
  frv_cache_unlock (cache, address);
}

static void
handle_req_WAR (FRV_CACHE *cache, int pipe, FRV_CACHE_REQUEST *req)
{
  char *buffer;
  FRV_CACHE_TAG *tag;
  SI address = req->address;

  if (frv_cache_enabled (cache) && ! non_cache_access (cache, address))
    {
      /* Look for the data in the cache.  The statistics of cache hit or
	 miss have already been recorded, so save and restore the stats before
	 and after obtaining the cache line.  */
      FRV_CACHE_STATISTICS save_stats = cache->statistics;
      tag = find_or_retrieve_cache_line (cache, address);
      cache->statistics = save_stats;
      if (tag != NULL)
	{
	  if (! req->u.WAR.preload)
	    {
	      copy_line_to_return_buffer (cache, pipe, tag, address);
	      set_return_buffer_reqno (cache, pipe, req->reqno);
	    }
	  else 
	    {
	      invalidate_return_buffer (cache, address);
	      if (req->u.WAR.lock)
		tag->locked = 1;
	    }
	  return;
	}
    }

  /* All cache lines in the set were locked, so just copy the data to the
     return buffer directly.  */
  if (! req->u.WAR.preload)
    {
      copy_memory_to_return_buffer (cache, pipe, address);
      set_return_buffer_reqno (cache, pipe, req->reqno);
    }
}

/* Resolve any conflicts and/or execute the given requests.  */
static void
arbitrate_requests (FRV_CACHE *cache)
{
  int pipe;
  /* Simply execute the requests in the final pipeline stages.  */
  for (pipe = LS; pipe < FRV_CACHE_PIPELINES; ++pipe)
    {
      FRV_CACHE_REQUEST *req
	= pipeline_stage_request (& cache->pipeline[pipe], LAST_STAGE);
      /* Make sure that there is a request to handle.  */
      if (req == NULL)
	continue;

      /* Handle the request.  */
      switch (req->kind)
	{
	case req_load:
	  handle_req_load (cache, pipe, req);
	  break;
	case req_store:
	  handle_req_store (cache, pipe, req);
	  break;
	case req_invalidate:
	  handle_req_invalidate (cache, pipe, req);
	  break;
	case req_preload:
	  handle_req_preload (cache, pipe, req);
	  break;
	case req_unlock:
	  handle_req_unlock (cache, pipe, req);
	  break;
	case req_WAR:
	  handle_req_WAR (cache, pipe, req);
	  break;
	default:
	  abort ();
	}
    }
}

/* Move a waiting ARS register to a free WAR register.  */
static void
move_ARS_to_WAR (FRV_CACHE *cache, int pipe, FRV_CACHE_WAR *war)
{
  /* If BARS is valid for this pipe, then move it to the given WAR. Move
     NARS to BARS if it is valid.  */
  if (cache->BARS.valid && cache->BARS.pipe == pipe)
    {
      war->address = cache->BARS.address;
      war->reqno = cache->BARS.reqno;
      war->priority = cache->BARS.priority;
      war->preload = cache->BARS.preload;
      war->lock = cache->BARS.lock;
      war->latency = cache->memory_latency + 1;
      war->valid = 1;
      if (cache->NARS.valid)
	{
	  cache->BARS = cache->NARS;
	  cache->NARS.valid = 0;
	}
      else
	cache->BARS.valid = 0;
      return;
    }
  /* If NARS is valid for this pipe, then move it to the given WAR.  */
  if (cache->NARS.valid && cache->NARS.pipe == pipe)
    {
      war->address = cache->NARS.address;
      war->reqno = cache->NARS.reqno;
      war->priority = cache->NARS.priority;
      war->preload = cache->NARS.preload;
      war->lock = cache->NARS.lock;
      war->latency = cache->memory_latency + 1;
      war->valid = 1;
      cache->NARS.valid = 0;
    }
}

/* Decrease the latencies of the various states in the cache.  */
static void
decrease_latencies (FRV_CACHE *cache)
{
  int pipe, j;
  /* Check the WAR registers.  */
  for (pipe = LS; pipe < FRV_CACHE_PIPELINES; ++pipe)
    {
      FRV_CACHE_PIPELINE *pipeline = & cache->pipeline[pipe];
      for (j = 0; j < NUM_WARS; ++j)
	{
	  FRV_CACHE_WAR *war = & pipeline->WAR[j];
	  if (war->valid)
	    {
	      --war->latency;
	      /* If the latency has expired, then submit a WAR request to the
		 pipeline.  */
	      if (war->latency <= 0)
		{
		  add_WAR_request (pipeline, war);
		  war->valid = 0;
		  move_ARS_to_WAR (cache, pipe, war);
		}
	    }
	}
    }
}

/* Run the cache for the given number of cycles.  */
void
frv_cache_run (FRV_CACHE *cache, int cycles)
{
  int i;
  for (i = 0; i < cycles; ++i)
    {
      advance_pipelines (cache);
      arbitrate_requests (cache);
      decrease_latencies (cache);
    }
}

int
frv_cache_read_passive_SI (FRV_CACHE *cache, SI address, SI *value)
{
  SI offset;
  FRV_CACHE_TAG *tag;

  if (non_cache_access (cache, address))
    return 0;

  {
    FRV_CACHE_STATISTICS save_stats = cache->statistics;
    int found = get_tag (cache, address, &tag);
    cache->statistics = save_stats;

    if (! found)
      return 0; /* Indicate non-cache-access.  */
  }

  /* A cache line was available for the data.
     Extract the target data from the line.  */
  offset = address & (cache->line_size - 1);
  *value = T2H_4 (*(SI *)(tag->line + offset));
  return 1;
}

/* Check the return buffers of the data cache to see if the requested data is
   available.  */
int
frv_cache_data_in_buffer (FRV_CACHE* cache, int pipe, SI address,
			  unsigned reqno)
{
  return cache->pipeline[pipe].status.return_buffer.valid
    && cache->pipeline[pipe].status.return_buffer.reqno == reqno
    && cache->pipeline[pipe].status.return_buffer.address <= address
    && cache->pipeline[pipe].status.return_buffer.address + cache->line_size
       > address;
}

/* Check to see if the requested data has been flushed.  */
int
frv_cache_data_flushed (FRV_CACHE* cache, int pipe, SI address, unsigned reqno)
{
  return cache->pipeline[pipe].status.flush.valid
    && cache->pipeline[pipe].status.flush.reqno == reqno
    && cache->pipeline[pipe].status.flush.address <= address
    && cache->pipeline[pipe].status.flush.address + cache->line_size
       > address;
}
