/* Cache support for the FRV simulator
   Copyright (C) 1999, 2000, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU Simulators.

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

#ifndef CACHE_H
#define CACHE_H

/* A representation of a set-associative cache with LRU replacement,
   cache line locking, non-blocking support and multiple read ports.  */

/* An enumeration of cache pipeline request kinds.  */
typedef enum
{
  req_load,
  req_store,
  req_invalidate,
  req_flush,
  req_preload,
  req_unlock,
  req_WAR
} FRV_CACHE_REQUEST_KIND;

/* The cache pipeline requests.  */
typedef struct {
  int preload;
  int lock;
} FRV_CACHE_WAR_REQUEST;

typedef struct {
  char *data;
  int length;
} FRV_CACHE_STORE_REQUEST;

typedef struct {
  int flush;
  int all;
} FRV_CACHE_INVALIDATE_REQUEST;

typedef struct {
  int lock;
  int length;
} FRV_CACHE_PRELOAD_REQUEST;

/* A cache pipeline request.  */
typedef struct frv_cache_request
{
  struct frv_cache_request *next;
  struct frv_cache_request *prev;
  FRV_CACHE_REQUEST_KIND kind;
  unsigned reqno;
  unsigned priority;
  SI address;
  union {
    FRV_CACHE_STORE_REQUEST store;
    FRV_CACHE_INVALIDATE_REQUEST invalidate;
    FRV_CACHE_PRELOAD_REQUEST preload;
    FRV_CACHE_WAR_REQUEST WAR;
  } u;
} FRV_CACHE_REQUEST;

/* The buffer for returning data to the caller.  */
typedef struct {
  unsigned reqno;
  SI address;
  char *data;
  int valid;
} FRV_CACHE_RETURN_BUFFER;

/* The status of flush requests.  */
typedef struct {
  unsigned reqno;
  SI address;
  int valid;
} FRV_CACHE_FLUSH_STATUS;

/* Communicate status of requests to the caller.  */
typedef struct {
  FRV_CACHE_FLUSH_STATUS  flush;
  FRV_CACHE_RETURN_BUFFER return_buffer;
} FRV_CACHE_STATUS;

/* A cache pipeline stage.  */
typedef struct {
  FRV_CACHE_REQUEST *request;
} FRV_CACHE_STAGE;

enum {
  FIRST_STAGE,
  A_STAGE = FIRST_STAGE, /* Addressing stage */
  I_STAGE,               /* Interference stage */
  LAST_STAGE = I_STAGE,
  FRV_CACHE_STAGES
};

/* Representation of the WAR register.  */
typedef struct {
  unsigned reqno;
  unsigned priority;
  SI address;
  int preload;
  int lock;
  int latency;
  int valid;
} FRV_CACHE_WAR;

/* A cache pipeline.  */
#define NUM_WARS 2
typedef struct {
  FRV_CACHE_REQUEST      *requests;
  FRV_CACHE_STAGE         stages[FRV_CACHE_STAGES];
  FRV_CACHE_WAR           WAR[NUM_WARS];
  FRV_CACHE_STATUS        status;
} FRV_CACHE_PIPELINE;

enum {LS, LD, FRV_CACHE_PIPELINES};

/* Representation of the xARS registers.  */
typedef struct {
  int pipe;
  unsigned reqno;
  unsigned priority;
  SI address;
  int preload;
  int lock;
  int valid;
} FRV_CACHE_ARS;

/* A cache tag.  */
typedef struct {
  USI   tag;    /* Address tag.  */
  int   lru;    /* Lower values indicates less recently used.  */
  char *line;   /* Points to storage for line in data_storage.  */
  char  dirty;  /* line has been written to since last stored?  */
  char  locked; /* line is locked?  */
  char  valid;  /* tag is valid?  */
} FRV_CACHE_TAG;

/* Cache statistics.  */
typedef struct {
  unsigned long accesses;   /* number of cache accesses.  */
  unsigned long hits;       /* number of cache hits.  */
} FRV_CACHE_STATISTICS;

/* The cache itself.
   Notes:
   - line_size must be a power of 2
   - sets must be a power of 2
   - ways must be a power of 2
*/
typedef struct {
  SIM_CPU *cpu;
  unsigned configured_ways;   /* Number of ways configured in each set.  */
  unsigned configured_sets;   /* Number of sets configured in the cache.  */
  unsigned ways;              /* Number of ways in each set.  */
  unsigned sets;              /* Number of sets in the cache.  */
  unsigned line_size;         /* Size of each cache line.  */
  unsigned memory_latency;    /* Latency of main memory in cycles.  */
  FRV_CACHE_TAG *tag_storage; /* Storage for tags.  */
  char *data_storage;         /* Storage for data (cache lines).  */
  FRV_CACHE_PIPELINE pipeline[2];  /* Cache pipelines.  */
  FRV_CACHE_ARS BARS;         /* BARS register.  */
  FRV_CACHE_ARS NARS;         /* BARS register.  */
  FRV_CACHE_STATISTICS statistics; /* Operation statistics.  */
} FRV_CACHE;

/* The tags are stored by ways within sets in order to make computations
   easier.  */
#define CACHE_TAG(cache, set, way) ( \
  & ((cache)->tag_storage[(set) * (cache)->ways + (way)]) \
)

/* Compute the address tag corresponding to the given address.  */
#define CACHE_ADDRESS_TAG(cache, address) ( \
  (address) & ~(((cache)->line_size * (cache)->sets) - 1) \
)

/* Determine the index at which the set containing this tag starts.  */
#define CACHE_TAG_SET_START(cache, tag) ( \
  ((tag) - (cache)->tag_storage) & ~((cache)->ways - 1) \
)

/* Determine the number of the set which this cache tag is in.  */
#define CACHE_TAG_SET_NUMBER(cache, tag) ( \
  CACHE_TAG_SET_START ((cache), (tag)) / (cache)->ways \
)

#define CACHE_RETURN_DATA(cache, slot, address, mode, N) (               \
  T2H_##N (*(mode *)(& (cache)->pipeline[slot].status.return_buffer.data \
		     [((address) & ((cache)->line_size - 1))]))          \
)
#define CACHE_RETURN_DATA_ADDRESS(cache, slot, address, N) (              \
  ((void *)& (cache)->pipeline[slot].status.return_buffer.data[(address)  \
			                       & ((cache)->line_size - 1)]) \
)

#define DATA_CROSSES_CACHE_LINE(cache, address, size) ( \
  ((address) & ((cache)->line_size - 1)) + (size) > (cache)->line_size \
)

#define CACHE_INITIALIZED(cache) ((cache)->data_storage != NULL)

/* These functions are used to initialize and terminate a cache.  */
void
frv_cache_init (SIM_CPU *, FRV_CACHE *);
void
frv_cache_term (FRV_CACHE *);
void
frv_cache_reconfigure (SIM_CPU *, FRV_CACHE *);
int
frv_cache_enabled (FRV_CACHE *);

/* These functions are used to operate the cache in non-cycle-accurate mode.
   Each request is handled individually and immediately using the current
   cache internal state.  */
int
frv_cache_read (FRV_CACHE *, int, SI);
int
frv_cache_write (FRV_CACHE *, SI, char *, unsigned);
int
frv_cache_preload (FRV_CACHE *, SI, USI, int);
int
frv_cache_invalidate (FRV_CACHE *, SI, int);
int
frv_cache_invalidate_all (FRV_CACHE *, int);

/* These functions are used to operate the cache in cycle-accurate mode.
   The internal operation of the cache is simulated down to the cycle level.  */
#define NO_REQNO 0xffffffff
void
frv_cache_request_load (FRV_CACHE *, unsigned, SI, int);
void
frv_cache_request_store (FRV_CACHE *, SI, int, char *, unsigned);
void
frv_cache_request_invalidate (FRV_CACHE *, unsigned, SI, int, int, int);
void
frv_cache_request_preload (FRV_CACHE *, SI, int, int, int);
void
frv_cache_request_unlock (FRV_CACHE *, SI, int);

void
frv_cache_run (FRV_CACHE *, int);

int
frv_cache_data_in_buffer (FRV_CACHE*, int, SI, unsigned);
int
frv_cache_data_flushed (FRV_CACHE*, int, SI, unsigned);

int
frv_cache_read_passive_SI (FRV_CACHE *, SI, SI *);

#endif /* CACHE_H */
