/* frv simulator machine independent profiling code.

   Copyright (C) 1998, 1999, 2000, 2001, 2003, 2007
   Free Software Foundation, Inc.
   Contributed by Red Hat

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
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#define WANT_CPU
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "bfd.h"

#if WITH_PROFILE_MODEL_P

#include "profile.h"
#include "profile-fr400.h"
#include "profile-fr500.h"
#include "profile-fr550.h"

static void
reset_gr_flags (SIM_CPU *cpu, INT gr)
{
  SIM_DESC sd = CPU_STATE (cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400
      || STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr450)
    fr400_reset_gr_flags (cpu, gr);
  /* Other machines have no gr flags right now.  */
}

static void
reset_fr_flags (SIM_CPU *cpu, INT fr)
{
  SIM_DESC sd = CPU_STATE (cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400
      || STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr450)
    fr400_reset_fr_flags (cpu, fr);
  else if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr500)
    fr500_reset_fr_flags (cpu, fr);
}

static void
reset_acc_flags (SIM_CPU *cpu, INT acc)
{
  SIM_DESC sd = CPU_STATE (cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400
      || STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr450)
    fr400_reset_acc_flags (cpu, acc);
  /* Other machines have no acc flags right now.  */
}

static void
reset_cc_flags (SIM_CPU *cpu, INT cc)
{
  SIM_DESC sd = CPU_STATE (cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr500)
    fr500_reset_cc_flags (cpu, cc);
  /* Other machines have no cc flags.  */
}

void
set_use_is_gr_complex (SIM_CPU *cpu, INT gr)
{
  if (gr != -1)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      reset_gr_flags (cpu, gr);
      ps->cur_gr_complex |= (((DI)1) << gr);
    }
}

void
set_use_not_gr_complex (SIM_CPU *cpu, INT gr)
{
  if (gr != -1)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      ps->cur_gr_complex &= ~(((DI)1) << gr);
    }
}

int
use_is_gr_complex (SIM_CPU *cpu, INT gr)
{
  if (gr != -1)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      return ps->cur_gr_complex & (((DI)1) << gr);
    }
  return 0;
}

/* Globals flag indicates whether this insn is being modeled.  */
enum FRV_INSN_MODELING model_insn = FRV_INSN_NO_MODELING;

/* static buffer for the name of the currently most restrictive hazard.  */
static char hazard_name[100] = "";

/* Print information about the wait applied to an entire VLIW insn.  */
FRV_INSN_FETCH_BUFFER frv_insn_fetch_buffer[]
= {
  {1, NO_REQNO}, {1, NO_REQNO} /* init with impossible address.  */
};

enum cache_request
{
  cache_load,
  cache_invalidate,
  cache_flush,
  cache_preload,
  cache_unlock
};

/* A queue of load requests from the data cache. Use to keep track of loads
   which are still pending.  */
/* TODO -- some of these are mutually exclusive and can use a union.  */
typedef struct
{
  FRV_CACHE *cache;
  unsigned reqno;
  SI address;
  int length;
  int is_signed;
  int regnum;
  int cycles;
  int regtype;
  int lock;
  int all;
  int slot;
  int active;
  enum cache_request request;
} CACHE_QUEUE_ELEMENT;

#define CACHE_QUEUE_SIZE 64 /* TODO -- make queue dynamic */
struct
{
  unsigned reqno;
  int ix;
  CACHE_QUEUE_ELEMENT q[CACHE_QUEUE_SIZE];
} cache_queue = {0, 0};

/* Queue a request for a load from the cache. The load will be queued as
   'inactive' and will be requested after the given number
   of cycles have passed from the point the load is activated.  */
void
request_cache_load (SIM_CPU *cpu, INT regnum, int regtype, int cycles)
{
  CACHE_QUEUE_ELEMENT *q;
  FRV_VLIW *vliw;
  int slot;

  /* For a conditional load which was not executed, CPU_LOAD_LENGTH will be
     zero.  */
  if (CPU_LOAD_LENGTH (cpu) == 0)
    return;

  if (cache_queue.ix >= CACHE_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  q = & cache_queue.q[cache_queue.ix];
  ++cache_queue.ix;

  q->reqno = cache_queue.reqno++;
  q->request = cache_load;
  q->cache = CPU_DATA_CACHE (cpu);
  q->address = CPU_LOAD_ADDRESS (cpu);
  q->length = CPU_LOAD_LENGTH (cpu);
  q->is_signed = CPU_LOAD_SIGNED (cpu);
  q->regnum = regnum;
  q->regtype = regtype;
  q->cycles = cycles;
  q->active = 0;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  q->slot = (*vliw->current_vliw)[slot];

  CPU_LOAD_LENGTH (cpu) = 0;
}

/* Queue a request to flush the cache. The request will be queued as
   'inactive' and will be requested after the given number
   of cycles have passed from the point the request is activated.  */
void
request_cache_flush (SIM_CPU *cpu, FRV_CACHE *cache, int cycles)
{
  CACHE_QUEUE_ELEMENT *q;
  FRV_VLIW *vliw;
  int slot;

  if (cache_queue.ix >= CACHE_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  q = & cache_queue.q[cache_queue.ix];
  ++cache_queue.ix;

  q->reqno = cache_queue.reqno++;
  q->request = cache_flush;
  q->cache = cache;
  q->address = CPU_LOAD_ADDRESS (cpu);
  q->all = CPU_PROFILE_STATE (cpu)->all_cache_entries;
  q->cycles = cycles;
  q->active = 0;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  q->slot = (*vliw->current_vliw)[slot];
}

/* Queue a request to invalidate the cache. The request will be queued as
   'inactive' and will be requested after the given number
   of cycles have passed from the point the request is activated.  */
void
request_cache_invalidate (SIM_CPU *cpu, FRV_CACHE *cache, int cycles)
{
  CACHE_QUEUE_ELEMENT *q;
  FRV_VLIW *vliw;
  int slot;

  if (cache_queue.ix >= CACHE_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  q = & cache_queue.q[cache_queue.ix];
  ++cache_queue.ix;

  q->reqno = cache_queue.reqno++;
  q->request = cache_invalidate;
  q->cache = cache;
  q->address = CPU_LOAD_ADDRESS (cpu);
  q->all = CPU_PROFILE_STATE (cpu)->all_cache_entries;
  q->cycles = cycles;
  q->active = 0;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  q->slot = (*vliw->current_vliw)[slot];
}

/* Queue a request to preload the cache. The request will be queued as
   'inactive' and will be requested after the given number
   of cycles have passed from the point the request is activated.  */
void
request_cache_preload (SIM_CPU *cpu, FRV_CACHE *cache, int cycles)
{
  CACHE_QUEUE_ELEMENT *q;
  FRV_VLIW *vliw;
  int slot;

  if (cache_queue.ix >= CACHE_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  q = & cache_queue.q[cache_queue.ix];
  ++cache_queue.ix;

  q->reqno = cache_queue.reqno++;
  q->request = cache_preload;
  q->cache = cache;
  q->address = CPU_LOAD_ADDRESS (cpu);
  q->length = CPU_LOAD_LENGTH (cpu);
  q->lock = CPU_LOAD_LOCK (cpu);
  q->cycles = cycles;
  q->active = 0;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  q->slot = (*vliw->current_vliw)[slot];

  CPU_LOAD_LENGTH (cpu) = 0;
}

/* Queue a request to unlock the cache. The request will be queued as
   'inactive' and will be requested after the given number
   of cycles have passed from the point the request is activated.  */
void
request_cache_unlock (SIM_CPU *cpu, FRV_CACHE *cache, int cycles)
{
  CACHE_QUEUE_ELEMENT *q;
  FRV_VLIW *vliw;
  int slot;

  if (cache_queue.ix >= CACHE_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  q = & cache_queue.q[cache_queue.ix];
  ++cache_queue.ix;

  q->reqno = cache_queue.reqno++;
  q->request = cache_unlock;
  q->cache = cache;
  q->address = CPU_LOAD_ADDRESS (cpu);
  q->cycles = cycles;
  q->active = 0;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  q->slot = (*vliw->current_vliw)[slot];
}

static void
submit_cache_request (CACHE_QUEUE_ELEMENT *q)
{
  switch (q->request)
    {
    case cache_load:
      frv_cache_request_load (q->cache, q->reqno, q->address, q->slot);
      break;
    case cache_flush:
      frv_cache_request_invalidate (q->cache, q->reqno, q->address, q->slot,
				    q->all, 1/*flush*/);
      break;
    case cache_invalidate:
      frv_cache_request_invalidate (q->cache, q->reqno, q->address, q->slot,
				   q->all, 0/*flush*/);
      break;
    case cache_preload:
      frv_cache_request_preload (q->cache, q->address, q->slot,
				 q->length, q->lock);
      break;
    case cache_unlock:
      frv_cache_request_unlock (q->cache, q->address, q->slot);
      break;
    default:
      abort ();
    }
}

/* Activate all inactive load requests.  */
static void
activate_cache_requests (SIM_CPU *cpu)
{
  int i;
  for (i = 0; i < cache_queue.ix; ++i)
    {
      CACHE_QUEUE_ELEMENT *q = & cache_queue.q[i];
      if (! q->active)
	{
	  q->active = 1;
	  /* Submit the request now if the cycle count is zero.  */
	  if (q->cycles == 0)
	    submit_cache_request (q);
	}
    }
}

/* Check to see if a load is pending which affects the given register(s).
 */
int
load_pending_for_register (SIM_CPU *cpu, int regnum, int words, int regtype)
{
  int i;
  for (i = 0; i < cache_queue.ix; ++i)
    {
      CACHE_QUEUE_ELEMENT *q = & cache_queue.q[i];

      /* Must be the same kind of register.  */
      if (! q->active || q->request != cache_load || q->regtype != regtype)
	continue;

      /* If the registers numbers are equal, then we have a match.  */
      if (q->regnum == regnum)
	return 1; /* load pending */

      /* Check for overlap of a load with a multi-word register.  */
      if (regnum < q->regnum)
	{
	  if (regnum + words > q->regnum)
	    return 1;
	}
      /* Check for overlap of a multi-word load with the register.  */
      else
	{
	  int data_words = (q->length + sizeof (SI) - 1) / sizeof (SI);
	  if (q->regnum + data_words > regnum)
	    return 1;
	}
    }

  return 0; /* no load pending */
}

/* Check to see if a cache flush pending which affects the given address.  */
static int
flush_pending_for_address (SIM_CPU *cpu, SI address)
{
  int line_mask = ~(CPU_DATA_CACHE (cpu)->line_size - 1);
  int i;
  for (i = 0; i < cache_queue.ix; ++i)
    {
      CACHE_QUEUE_ELEMENT *q = & cache_queue.q[i];

      /* Must be the same kind of request and active.  */
      if (! q->active || q->request != cache_flush)
	continue;

      /* If the addresses are equal, then we have a match.  */
      if ((q->address & line_mask) == (address & line_mask))
	return 1; /* flush pending */
    }

  return 0; /* no flush pending */
}

static void
remove_cache_queue_element (SIM_CPU *cpu, int i)
{
  /* If we are removing the load of a FR register, then remember which one(s).
   */
  CACHE_QUEUE_ELEMENT q = cache_queue.q[i];

  for (--cache_queue.ix; i < cache_queue.ix; ++i)
    cache_queue.q[i] = cache_queue.q[i + 1];

  /* If we removed a load of a FR register, check to see if any other loads
     of that register is still queued. If not, then apply the queued post
     processing time of that register to its latency.  Also apply
     1 extra cycle of latency to the register since it was a floating point
     load.  */
  if (q.request == cache_load && q.regtype != REGTYPE_NONE)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int data_words = (q.length + sizeof (SI) - 1) / sizeof (SI);
      int j;
      for (j = 0; j < data_words; ++j)
	{
	  int regnum = q.regnum + j;
	  if (! load_pending_for_register (cpu, regnum, 1, q.regtype))
	    {
	      if (q.regtype == REGTYPE_FR)
		{
		  int *fr = ps->fr_busy;
		  fr[regnum] += 1 + ps->fr_ptime[regnum];
		  ps->fr_ptime[regnum] = 0;
		}
	    }
	}
    }
}

/* Copy data from the cache buffer to the target register(s).  */
static void
copy_load_data (SIM_CPU *current_cpu, FRV_CACHE *cache, int slot,
		CACHE_QUEUE_ELEMENT *q)
{
  switch (q->length)
    {
    case 1:
      if (q->regtype == REGTYPE_FR)
	{
	  if (q->is_signed)
	    {
	      QI value = CACHE_RETURN_DATA (cache, slot, q->address, QI, 1);
	      SET_H_FR (q->regnum, value);
	    }
	  else
	    {
	      UQI value = CACHE_RETURN_DATA (cache, slot, q->address, UQI, 1);
	      SET_H_FR (q->regnum, value);
	    }
	}
      else
	{
	  if (q->is_signed)
	    {
	      QI value = CACHE_RETURN_DATA (cache, slot, q->address, QI, 1);
	      SET_H_GR (q->regnum, value);
	    }
	  else
	    {
	      UQI value = CACHE_RETURN_DATA (cache, slot, q->address, UQI, 1);
	      SET_H_GR (q->regnum, value);
	    }
	}
      break;
    case 2:
      if (q->regtype == REGTYPE_FR)
	{
	  if (q->is_signed)
	    {
	      HI value = CACHE_RETURN_DATA (cache, slot, q->address, HI, 2);
	      SET_H_FR (q->regnum, value);
	    }
	  else
	    {
	      UHI value = CACHE_RETURN_DATA (cache, slot, q->address, UHI, 2);
	      SET_H_FR (q->regnum, value);
	    }
	}
      else
	{
	  if (q->is_signed)
	    {
	      HI value = CACHE_RETURN_DATA (cache, slot, q->address, HI, 2);
	      SET_H_GR (q->regnum, value);
	    }
	  else
	    {
	      UHI value = CACHE_RETURN_DATA (cache, slot, q->address, UHI, 2);
	      SET_H_GR (q->regnum, value);
	    }
	}
      break;
    case 4:
      if (q->regtype == REGTYPE_FR)
	{
	  SET_H_FR (q->regnum,
		    CACHE_RETURN_DATA (cache, slot, q->address, SF, 4));
	}
      else
	{
	  SET_H_GR (q->regnum,
		    CACHE_RETURN_DATA (cache, slot, q->address, SI, 4));
	}
      break;
    case 8:
      if (q->regtype == REGTYPE_FR)
	{
	  SET_H_FR_DOUBLE (q->regnum,
			   CACHE_RETURN_DATA (cache, slot, q->address, DF, 8));
	}
      else
	{
	  SET_H_GR_DOUBLE (q->regnum,
			   CACHE_RETURN_DATA (cache, slot, q->address, DI, 8));
	}
      break;
    case 16:
      if (q->regtype == REGTYPE_FR)
	frvbf_h_fr_quad_set_handler (current_cpu, q->regnum,
				     CACHE_RETURN_DATA_ADDRESS (cache, slot,
								q->address,
								16));
      else
	frvbf_h_gr_quad_set_handler (current_cpu, q->regnum,
				     CACHE_RETURN_DATA_ADDRESS (cache, slot,
								q->address,
								16));
      break;
    default:
      abort ();
    }
}

static int
request_complete (SIM_CPU *cpu, CACHE_QUEUE_ELEMENT *q)
{
  FRV_CACHE* cache;
  if (! q->active || q->cycles > 0)
    return 0;

  cache = CPU_DATA_CACHE (cpu);
  switch (q->request)
    {
    case cache_load:
      /* For loads, we must wait until the data is returned from the cache.  */
      if (frv_cache_data_in_buffer (cache, 0, q->address, q->reqno))
	{
	  copy_load_data (cpu, cache, 0, q);
	  return 1;
	}
      if (frv_cache_data_in_buffer (cache, 1, q->address, q->reqno))
	{
	  copy_load_data (cpu, cache, 1, q);
	  return 1;
	}
      break;

    case cache_flush:
      /* We must wait until the data is flushed.  */
      if (frv_cache_data_flushed (cache, 0, q->address, q->reqno))
	return 1;
      if (frv_cache_data_flushed (cache, 1, q->address, q->reqno))
	return 1;
      break;

    default:
      /* All other requests are complete once they've been made.  */
      return 1;
    }

  return 0;
}

/* Run the insn and data caches through the given number of cycles, taking
   note of load requests which are fullfilled as a result.  */
static void
run_caches (SIM_CPU *cpu, int cycles)
{
  FRV_CACHE* data_cache = CPU_DATA_CACHE (cpu);
  FRV_CACHE* insn_cache = CPU_INSN_CACHE (cpu);
  int i;
  /* For each cycle, run the caches, noting which requests have been fullfilled
     and submitting new requests on their designated cycles.  */
  for (i = 0; i < cycles; ++i)
    {
      int j;
      /* Run the caches through 1 cycle.  */
      frv_cache_run (data_cache, 1);
      frv_cache_run (insn_cache, 1);

      /* Note whether prefetched insn data has been loaded yet.  */
      for (j = LS; j < FRV_CACHE_PIPELINES; ++j)
	{
	  if (frv_insn_fetch_buffer[j].reqno != NO_REQNO
	      && frv_cache_data_in_buffer (insn_cache, j,
					   frv_insn_fetch_buffer[j].address,
					   frv_insn_fetch_buffer[j].reqno))
	    frv_insn_fetch_buffer[j].reqno = NO_REQNO;
	}

      /* Check to see which requests have been satisfied and which should
	 be submitted now.  */
      for (j = 0; j < cache_queue.ix; ++j)
	{
	  CACHE_QUEUE_ELEMENT *q = & cache_queue.q[j];
	  if (! q->active)
	    continue;

	  /* If a load has been satisfied, complete the operation and remove it
	     from the queue.  */
	  if (request_complete (cpu, q))
	    {
	      remove_cache_queue_element (cpu, j);
	      --j;
	      continue;
	    }

	  /* Decrease the cycle count of each queued request.
	     Submit a request for each queued request whose cycle count has
	     become zero.  */
	  --q->cycles;
	  if (q->cycles == 0)
	    submit_cache_request (q);
	}
    }
}

static void
apply_latency_adjustments (SIM_CPU *cpu)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int i;
  /* update the latencies of the registers.  */
  int *fr  = ps->fr_busy;
  int *acc = ps->acc_busy;
  for (i = 0; i < 64; ++i)
    {
      if (ps->fr_busy_adjust[i] > 0)
	*fr -= ps->fr_busy_adjust[i]; /* OK if it goes negative.  */
      if (ps->acc_busy_adjust[i] > 0)
	*acc -= ps->acc_busy_adjust[i]; /* OK if it goes negative.  */
      ++fr;
      ++acc;
    }
}

/* Account for the number of cycles which have just passed in the latency of
   various system elements.  Works for negative cycles too so that latency
   can be extended in the case of insn fetch latency.
   If negative or zero, then no adjustment is necessary.  */
static void
update_latencies (SIM_CPU *cpu, int cycles)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int i;
  /* update the latencies of the registers.  */
  int *fdiv;
  int *fsqrt;
  int *idiv;
  int *flt;
  int *media;
  int *ccr;
  int *gr  = ps->gr_busy;
  int *fr  = ps->fr_busy;
  int *acc = ps->acc_busy;
  int *spr;
  /* This loop handles GR, FR and ACC registers.  */
  for (i = 0; i < 64; ++i)
    {
      if (*gr <= cycles)
	{
	  *gr = 0;
	  reset_gr_flags (cpu, i);
	}
      else
	*gr -= cycles;
      /* If the busy drops to 0, then mark the register as
	 "not in use".  */
      if (*fr <= cycles)
	{
	  int *fr_lat = ps->fr_latency + i;
	  *fr = 0;
	  ps->fr_busy_adjust[i] = 0;
	  /* Only clear flags if this register has no target latency.  */
	  if (*fr_lat == 0)
	    reset_fr_flags (cpu, i);
	}
      else
	*fr -= cycles;
      /* If the busy drops to 0, then mark the register as
	 "not in use".  */
      if (*acc <= cycles)
	{
	  int *acc_lat = ps->acc_latency + i;
	  *acc = 0;
	  ps->acc_busy_adjust[i] = 0;
	  /* Only clear flags if this register has no target latency.  */
	  if (*acc_lat == 0)
	    reset_acc_flags (cpu, i);
	}
      else
	*acc -= cycles;
      ++gr;
      ++fr;
      ++acc;
    }
  /* This loop handles CCR registers.  */
  ccr = ps->ccr_busy;
  for (i = 0; i < 8; ++i)
    {
      if (*ccr <= cycles)
	{
	  *ccr = 0;
	  reset_cc_flags (cpu, i);
	}
      else
	*ccr -= cycles;
      ++ccr;
    }
  /* This loop handles SPR registers.  */
  spr = ps->spr_busy;
  for (i = 0; i < 4096; ++i)
    {
      if (*spr <= cycles)
	*spr = 0;
      else
	*spr -= cycles;
      ++spr;
    }
  /* This loop handles resources.  */
  idiv = ps->idiv_busy;
  fdiv = ps->fdiv_busy;
  fsqrt = ps->fsqrt_busy;
  for (i = 0; i < 2; ++i)
    {
      *idiv = (*idiv <= cycles) ? 0 : (*idiv - cycles);
      *fdiv = (*fdiv <= cycles) ? 0 : (*fdiv - cycles);
      *fsqrt = (*fsqrt <= cycles) ? 0 : (*fsqrt - cycles);
      ++idiv;
      ++fdiv;
      ++fsqrt;
    }
  /* Float and media units can occur in 4 slots on some machines.  */
  flt = ps->float_busy;
  media = ps->media_busy;
  for (i = 0; i < 4; ++i)
    {
      *flt = (*flt <= cycles) ? 0 : (*flt - cycles);
      *media = (*media <= cycles) ? 0 : (*media - cycles);
      ++flt;
      ++media;
    }
}

/* Print information about the wait for the given number of cycles.  */
void
frv_model_trace_wait_cycles (SIM_CPU *cpu, int cycles, const char *hazard_name)
{
  if (TRACE_INSN_P (cpu) && cycles > 0)
    {
      SIM_DESC sd = CPU_STATE (cpu);
      trace_printf (sd, cpu, "**** %s wait %d cycles ***\n",
		    hazard_name, cycles);
    }
}

void
trace_vliw_wait_cycles (SIM_CPU *cpu)
{
  if (TRACE_INSN_P (cpu))
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      frv_model_trace_wait_cycles (cpu, ps->vliw_wait, hazard_name);
    }
}

/* Wait for the given number of cycles.  */
void
frv_model_advance_cycles (SIM_CPU *cpu, int cycles)
{
  PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);
  update_latencies (cpu, cycles);
  run_caches (cpu, cycles);
  PROFILE_MODEL_TOTAL_CYCLES (p) += cycles;
}

void
handle_resource_wait (SIM_CPU *cpu)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  if (ps->vliw_wait != 0)
    frv_model_advance_cycles (cpu, ps->vliw_wait);
  if (ps->vliw_load_stall > ps->vliw_wait)
    ps->vliw_load_stall -= ps->vliw_wait;
  else
    ps->vliw_load_stall = 0;
}

/* Account for the number of cycles until these resources will be available
   again.  */
static void
update_target_latencies (SIM_CPU *cpu)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int i;
  /* update the latencies of the registers.  */
  int *ccr_lat;
  int *gr_lat  = ps->gr_latency;
  int *fr_lat  = ps->fr_latency;
  int *acc_lat = ps->acc_latency;
  int *spr_lat;
  int *ccr;
  int *gr = ps->gr_busy;
  int  *fr = ps->fr_busy;
  int  *acc = ps->acc_busy;
  int *spr;
  /* This loop handles GR, FR and ACC registers.  */
  for (i = 0; i < 64; ++i)
    {
      if (*gr_lat)
	{
	  *gr = *gr_lat;
	  *gr_lat = 0;
	}
      if (*fr_lat)
	{
	  *fr = *fr_lat;
	  *fr_lat = 0;
	}
      if (*acc_lat)
	{
	  *acc = *acc_lat;
	  *acc_lat = 0;
	}
      ++gr; ++gr_lat;
      ++fr; ++fr_lat;
      ++acc; ++acc_lat;
    }
  /* This loop handles CCR registers.  */
  ccr = ps->ccr_busy;
  ccr_lat = ps->ccr_latency;
  for (i = 0; i < 8; ++i)
    {
      if (*ccr_lat)
	{
	  *ccr = *ccr_lat;
	  *ccr_lat = 0;
	}
      ++ccr; ++ccr_lat;
    }
  /* This loop handles SPR registers.  */
  spr = ps->spr_busy;
  spr_lat = ps->spr_latency;
  for (i = 0; i < 4096; ++i)
    {
      if (*spr_lat)
	{
	  *spr = *spr_lat;
	  *spr_lat = 0;
	}
      ++spr; ++spr_lat;
    }
}

/* Run the caches until all pending cache flushes are complete.  */
static void
wait_for_flush (SIM_CPU *cpu)
{
  SI address = CPU_LOAD_ADDRESS (cpu);
  int wait = 0;
  while (flush_pending_for_address (cpu, address))
    {
      frv_model_advance_cycles (cpu, 1);
      ++wait;
    }
  if (TRACE_INSN_P (cpu) && wait)
    {
      sprintf (hazard_name, "Data cache flush address %p:", address);
      frv_model_trace_wait_cycles (cpu, wait, hazard_name);
    }
}

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */
void
frvbf_model_insn_before (SIM_CPU *cpu, int first_p)
{
  SIM_DESC sd = CPU_STATE (cpu);
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);

  ps->vliw_wait = 0;
  ps->post_wait = 0;
  memset (ps->fr_busy_adjust, 0, sizeof (ps->fr_busy_adjust));
  memset (ps->acc_busy_adjust, 0, sizeof (ps->acc_busy_adjust));

  if (first_p)
    {
      ps->vliw_insns++;
      ps->vliw_cycles = 0;
      ps->vliw_branch_taken = 0;
      ps->vliw_load_stall = 0;
    }

  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      fr400_model_insn_before (cpu, first_p);
      break;
    case bfd_mach_fr500:
      fr500_model_insn_before (cpu, first_p);
      break;
    case bfd_mach_fr550:
      fr550_model_insn_before (cpu, first_p);
      break;
    default:
      break;
    }

  if (first_p)
    wait_for_flush (cpu);
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */

void
frvbf_model_insn_after (SIM_CPU *cpu, int last_p, int cycles)
{
  PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  SIM_DESC sd = CPU_STATE (cpu);

  PROFILE_MODEL_CUR_INSN_CYCLES (p) = cycles;

  /* The number of cycles for a VLIW insn is the maximum number of cycles
     used by any individual insn within it.  */
  if (cycles > ps->vliw_cycles)
    ps->vliw_cycles = cycles;

  if (last_p)
    {
      /*  This is the last insn in a VLIW insn.  */
      struct frv_interrupt_timer *timer = & frv_interrupt_state.timer;

      activate_cache_requests (cpu); /* before advancing cycles.  */
      apply_latency_adjustments (cpu); /* must go first.  */ 
      update_target_latencies (cpu); /* must go next.  */ 
      frv_model_advance_cycles (cpu, ps->vliw_cycles);

      PROFILE_MODEL_LOAD_STALL_CYCLES (p) += ps->vliw_load_stall;

      /* Check the interrupt timer.  cycles contains the total cycle count.  */
      if (timer->enabled)
	{
	  cycles = PROFILE_MODEL_TOTAL_CYCLES (p);
	  if (timer->current % timer->value
	      + (cycles - timer->current) >= timer->value)
	    frv_queue_external_interrupt (cpu, timer->interrupt);
	  timer->current = cycles;
	}

      ps->past_first_p = 0; /* Next one will be the first in a new VLIW.  */
      ps->branch_address = -1;
    }
  else
    ps->past_first_p = 1;

  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      fr400_model_insn_after (cpu, last_p, cycles);
      break;
    case bfd_mach_fr500:
      fr500_model_insn_after (cpu, last_p, cycles);
      break;
    case bfd_mach_fr550:
      fr550_model_insn_after (cpu, last_p, cycles);
      break;
    default:
      break;
    }
}

USI
frvbf_model_branch (SIM_CPU *current_cpu, PCADDR target, int hint)
{
  /* Record the hint and branch address for use in profiling.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);
  ps->branch_hint = hint;
  ps->branch_address = target;
}

/* Top up the latency of the given GR by the given number of cycles.  */
void
update_GR_latency (SIM_CPU *cpu, INT out_GR, int cycles)
{
  if (out_GR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *gr = ps->gr_latency;
      if (gr[out_GR] < cycles)
	gr[out_GR] = cycles;
    }
}

void
decrease_GR_busy (SIM_CPU *cpu, INT in_GR, int cycles)
{
  if (in_GR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *gr = ps->gr_busy;
      gr[in_GR] -= cycles;
    }
}

/* Top up the latency of the given double GR by the number of cycles.  */
void
update_GRdouble_latency (SIM_CPU *cpu, INT out_GR, int cycles)
{
  if (out_GR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *gr = ps->gr_latency;
      if (gr[out_GR] < cycles)
	gr[out_GR] = cycles;
      if (out_GR < 63 && gr[out_GR + 1] < cycles)
	gr[out_GR + 1] = cycles;
    }
}

void
update_GR_latency_for_load (SIM_CPU *cpu, INT out_GR, int cycles)
{
  if (out_GR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *gr = ps->gr_latency;

      /* The latency of the GR will be at least the number of cycles used
	 by the insn.  */
      if (gr[out_GR] < cycles)
	gr[out_GR] = cycles;

      /* The latency will also depend on how long it takes to retrieve the
	 data from the cache or memory.  Assume that the load is issued
	 after the last cycle of the insn.  */
      request_cache_load (cpu, out_GR, REGTYPE_NONE, cycles);
    }
}

void
update_GRdouble_latency_for_load (SIM_CPU *cpu, INT out_GR, int cycles)
{
  if (out_GR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *gr = ps->gr_latency;

      /* The latency of the GR will be at least the number of cycles used
	 by the insn.  */
      if (gr[out_GR] < cycles)
	gr[out_GR] = cycles;
      if (out_GR < 63 && gr[out_GR + 1] < cycles)
	gr[out_GR + 1] = cycles;

      /* The latency will also depend on how long it takes to retrieve the
	 data from the cache or memory.  Assume that the load is issued
	 after the last cycle of the insn.  */
      request_cache_load (cpu, out_GR, REGTYPE_NONE, cycles);
    }
}

void
update_GR_latency_for_swap (SIM_CPU *cpu, INT out_GR, int cycles)
{
  update_GR_latency_for_load (cpu, out_GR, cycles);
}

/* Top up the latency of the given FR by the given number of cycles.  */
void
update_FR_latency (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_latency;
      if (fr[out_FR] < cycles)
	fr[out_FR] = cycles;
    }
}

/* Top up the latency of the given double FR by the number of cycles.  */
void
update_FRdouble_latency (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_latency;
      if (fr[out_FR] < cycles)
	fr[out_FR] = cycles;
      if (out_FR < 63 && fr[out_FR + 1] < cycles)
	fr[out_FR + 1] = cycles;
    }
}

void
update_FR_latency_for_load (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_latency;

      /* The latency of the FR will be at least the number of cycles used
	 by the insn.  */
      if (fr[out_FR] < cycles)
	fr[out_FR] = cycles;

      /* The latency will also depend on how long it takes to retrieve the
	 data from the cache or memory.  Assume that the load is issued
	 after the last cycle of the insn.  */
      request_cache_load (cpu, out_FR, REGTYPE_FR, cycles);
    }
}

void
update_FRdouble_latency_for_load (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_latency;
	  
      /* The latency of the FR will be at least the number of cycles used
	 by the insn.  */
      if (fr[out_FR] < cycles)
	fr[out_FR] = cycles;
      if (out_FR < 63 && fr[out_FR + 1] < cycles)
	fr[out_FR + 1] = cycles;

      /* The latency will also depend on how long it takes to retrieve the
	 data from the cache or memory.  Assume that the load is issued
	 after the last cycle of the insn.  */
      request_cache_load (cpu, out_FR, REGTYPE_FR, cycles);
    }
}

/* Top up the post-processing time of the given FR by the given number of
   cycles.  */
void
update_FR_ptime (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      /* If a load is pending on this register, then add the cycles to
	 the post processing time for this register. Otherwise apply it
	 directly to the latency of the register.  */
      if (! load_pending_for_register (cpu, out_FR, 1, REGTYPE_FR))
	{
	  int *fr = ps->fr_latency;
	  fr[out_FR] += cycles;
	}
      else
	ps->fr_ptime[out_FR] += cycles;
    }
}

void
update_FRdouble_ptime (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      /* If a load is pending on this register, then add the cycles to
	 the post processing time for this register. Otherwise apply it
	 directly to the latency of the register.  */
      if (! load_pending_for_register (cpu, out_FR, 2, REGTYPE_FR))
	{
	  int *fr = ps->fr_latency;
	  fr[out_FR] += cycles;
	  if (out_FR < 63)
	    fr[out_FR + 1] += cycles;
	}
      else
	{
	  ps->fr_ptime[out_FR] += cycles;
	  if (out_FR < 63)
	    ps->fr_ptime[out_FR + 1] += cycles;
	}
    }
}

/* Top up the post-processing time of the given ACC by the given number of
   cycles.  */
void
update_ACC_ptime (SIM_CPU *cpu, INT out_ACC, int cycles)
{
  if (out_ACC >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      /* No load can be pending on this register. Apply the cycles
	 directly to the latency of the register.  */
      int *acc = ps->acc_latency;
      acc[out_ACC] += cycles;
    }
}

/* Top up the post-processing time of the given SPR by the given number of
   cycles.  */
void
update_SPR_ptime (SIM_CPU *cpu, INT out_SPR, int cycles)
{
  if (out_SPR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      /* No load can be pending on this register. Apply the cycles
	 directly to the latency of the register.  */
      int *spr = ps->spr_latency;
      spr[out_SPR] += cycles;
    }
}

void
decrease_ACC_busy (SIM_CPU *cpu, INT out_ACC, int cycles)
{
  if (out_ACC >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *acc = ps->acc_busy;
      acc[out_ACC] -= cycles;
      if (ps->acc_busy_adjust[out_ACC] >= 0
	  && cycles > ps->acc_busy_adjust[out_ACC])
	ps->acc_busy_adjust[out_ACC] = cycles;
    }
}

void
increase_ACC_busy (SIM_CPU *cpu, INT out_ACC, int cycles)
{
  if (out_ACC >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *acc = ps->acc_busy;
      acc[out_ACC] += cycles;
    }
}

void
enforce_full_acc_latency (SIM_CPU *cpu, INT in_ACC)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  ps->acc_busy_adjust [in_ACC] = -1;
}

void
decrease_FR_busy (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_busy;
      fr[out_FR] -= cycles;
      if (ps->fr_busy_adjust[out_FR] >= 0
	  && cycles > ps->fr_busy_adjust[out_FR])
	ps->fr_busy_adjust[out_FR] = cycles;
    }
}

void
increase_FR_busy (SIM_CPU *cpu, INT out_FR, int cycles)
{
  if (out_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr = ps->fr_busy;
      fr[out_FR] += cycles;
    }
}

/* Top up the latency of the given ACC by the given number of cycles.  */
void
update_ACC_latency (SIM_CPU *cpu, INT out_ACC, int cycles)
{
  if (out_ACC >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *acc = ps->acc_latency;
      if (acc[out_ACC] < cycles)
	acc[out_ACC] = cycles;
    }
}

/* Top up the latency of the given CCR by the given number of cycles.  */
void
update_CCR_latency (SIM_CPU *cpu, INT out_CCR, int cycles)
{
  if (out_CCR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *ccr = ps->ccr_latency;
      if (ccr[out_CCR] < cycles)
	ccr[out_CCR] = cycles;
    }
}

/* Top up the latency of the given SPR by the given number of cycles.  */
void
update_SPR_latency (SIM_CPU *cpu, INT out_SPR, int cycles)
{
  if (out_SPR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *spr = ps->spr_latency;
      if (spr[out_SPR] < cycles)
	spr[out_SPR] = cycles;
    }
}

/* Top up the latency of the given integer division resource by the given
   number of cycles.  */
void
update_idiv_resource_latency (SIM_CPU *cpu, INT in_resource, int cycles)
{
  /* operate directly on the busy cycles since each resource can only
     be used once in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->idiv_busy;
  r[in_resource] = cycles;
}

/* Set the latency of the given resource to the given number of cycles.  */
void
update_fdiv_resource_latency (SIM_CPU *cpu, INT in_resource, int cycles)
{
  /* operate directly on the busy cycles since each resource can only
     be used once in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->fdiv_busy;
  r[in_resource] = cycles;
}

/* Set the latency of the given resource to the given number of cycles.  */
void
update_fsqrt_resource_latency (SIM_CPU *cpu, INT in_resource, int cycles)
{
  /* operate directly on the busy cycles since each resource can only
     be used once in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->fsqrt_busy;
  r[in_resource] = cycles;
}

/* Set the latency of the given resource to the given number of cycles.  */
void
update_float_resource_latency (SIM_CPU *cpu, INT in_resource, int cycles)
{
  /* operate directly on the busy cycles since each resource can only
     be used once in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->float_busy;
  r[in_resource] = cycles;
}

void
update_media_resource_latency (SIM_CPU *cpu, INT in_resource, int cycles)
{
  /* operate directly on the busy cycles since each resource can only
     be used once in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->media_busy;
  r[in_resource] = cycles;
}

/* Set the branch penalty to the given number of cycles.  */
void
update_branch_penalty (SIM_CPU *cpu, int cycles)
{
  /* operate directly on the busy cycles since only one branch can occur
     in a VLIW insn.  */
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  ps->branch_penalty = cycles;
}

/* Check the availability of the given GR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_GR (SIM_CPU *cpu, INT in_GR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *gr = ps->gr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_GR >= 0 && gr[in_GR] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for gr%d:", in_GR);
      ps->vliw_wait = gr[in_GR];
    }
}

/* Check the availability of the given GR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_GRdouble (SIM_CPU *cpu, INT in_GR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *gr = ps->gr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_GR >= 0)
    {
      if (gr[in_GR] > ps->vliw_wait)
	{
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for gr%d:", in_GR);
	  ps->vliw_wait = gr[in_GR];
	}
      if (in_GR < 63 && gr[in_GR + 1] > ps->vliw_wait)
	{
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for gr%d:", in_GR + 1);
	  ps->vliw_wait = gr[in_GR + 1];
	}
    }
}

/* Check the availability of the given FR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_FR (SIM_CPU *cpu, INT in_FR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fr = ps->fr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_FR >= 0 && fr[in_FR] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
      ps->vliw_wait = fr[in_FR];
    }
}

/* Check the availability of the given GR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_FRdouble (SIM_CPU *cpu, INT in_FR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fr = ps->fr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_FR >= 0)
    {
      if (fr[in_FR] > ps->vliw_wait)
	{
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
	  ps->vliw_wait = fr[in_FR];
	}
      if (in_FR < 63 && fr[in_FR + 1] > ps->vliw_wait)
	{
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR + 1);
	  ps->vliw_wait = fr[in_FR + 1];
	}
    }
}

/* Check the availability of the given CCR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_CCR (SIM_CPU *cpu, INT in_CCR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *ccr = ps->ccr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_CCR >= 0 && ccr[in_CCR] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  if (in_CCR > 3)
	    sprintf (hazard_name, "Data hazard for icc%d:", in_CCR-4);
	  else
	    sprintf (hazard_name, "Data hazard for fcc%d:", in_CCR);
	}
      ps->vliw_wait = ccr[in_CCR];
    }
}

/* Check the availability of the given ACC register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_ACC (SIM_CPU *cpu, INT in_ACC)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *acc = ps->acc_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_ACC >= 0 && acc[in_ACC] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for acc%d:", in_ACC);
      ps->vliw_wait = acc[in_ACC];
    }
}

/* Check the availability of the given SPR register and update the number
   of cycles the current VLIW insn must wait until it is available.  */
void
vliw_wait_for_SPR (SIM_CPU *cpu, INT in_SPR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *spr = ps->spr_busy;
  /* If the latency of the register is greater than the current wait
     then update the current wait.  */
  if (in_SPR >= 0 && spr[in_SPR] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for spr %d:", in_SPR);
      ps->vliw_wait = spr[in_SPR];
    }
}

/* Check the availability of the given integer division resource and update
   the number of cycles the current VLIW insn must wait until it is available.
*/
void
vliw_wait_for_idiv_resource (SIM_CPU *cpu, INT in_resource)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->idiv_busy;
  /* If the latency of the resource is greater than the current wait
     then update the current wait.  */
  if (r[in_resource] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for integer division in slot I%d:", in_resource);
	}
      ps->vliw_wait = r[in_resource];
    }
}

/* Check the availability of the given float division resource and update
   the number of cycles the current VLIW insn must wait until it is available.
*/
void
vliw_wait_for_fdiv_resource (SIM_CPU *cpu, INT in_resource)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->fdiv_busy;
  /* If the latency of the resource is greater than the current wait
     then update the current wait.  */
  if (r[in_resource] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for floating point division in slot F%d:", in_resource);
	}
      ps->vliw_wait = r[in_resource];
    }
}

/* Check the availability of the given float square root resource and update
   the number of cycles the current VLIW insn must wait until it is available.
*/
void
vliw_wait_for_fsqrt_resource (SIM_CPU *cpu, INT in_resource)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->fsqrt_busy;
  /* If the latency of the resource is greater than the current wait
     then update the current wait.  */
  if (r[in_resource] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for square root in slot F%d:", in_resource);
	}
      ps->vliw_wait = r[in_resource];
    }
}

/* Check the availability of the given float unit resource and update
   the number of cycles the current VLIW insn must wait until it is available.
*/
void
vliw_wait_for_float_resource (SIM_CPU *cpu, INT in_resource)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->float_busy;
  /* If the latency of the resource is greater than the current wait
     then update the current wait.  */
  if (r[in_resource] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for floating point unit in slot F%d:", in_resource);
	}
      ps->vliw_wait = r[in_resource];
    }
}

/* Check the availability of the given media unit resource and update
   the number of cycles the current VLIW insn must wait until it is available.
*/
void
vliw_wait_for_media_resource (SIM_CPU *cpu, INT in_resource)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *r = ps->media_busy;
  /* If the latency of the resource is greater than the current wait
     then update the current wait.  */
  if (r[in_resource] > ps->vliw_wait)
    {
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for media unit in slot M%d:", in_resource);
	}
      ps->vliw_wait = r[in_resource];
    }
}

/* Run the caches until all requests for the given register(s) are satisfied. */
void
load_wait_for_GR (SIM_CPU *cpu, INT in_GR)
{
  if (in_GR >= 0)
    {
      int wait = 0;
      while (load_pending_for_register (cpu, in_GR, 1/*words*/, REGTYPE_NONE))
	{
	  frv_model_advance_cycles (cpu, 1);
	  ++wait;
	}
      if (wait)
	{
	  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
	  ps->vliw_wait += wait;
	  ps->vliw_load_stall += wait;
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for gr%d:", in_GR);
	}
    }
}

void
load_wait_for_FR (SIM_CPU *cpu, INT in_FR)
{
  if (in_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr;
      int wait = 0;
      while (load_pending_for_register (cpu, in_FR, 1/*words*/, REGTYPE_FR))
	{
	  frv_model_advance_cycles (cpu, 1);
	  ++wait;
	}
      /* Post processing time may have been added to the register's
	 latency after the loads were processed. Account for that too.
      */
      fr = ps->fr_busy;
      if (fr[in_FR])
	{
	  wait += fr[in_FR];
	  frv_model_advance_cycles (cpu, fr[in_FR]);
	}
      /* Update the vliw_wait with the number of cycles we waited for the
	 load and any post-processing.  */
      if (wait)
	{
	  ps->vliw_wait += wait;
	  ps->vliw_load_stall += wait;
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
	}
    }
}

void
load_wait_for_GRdouble (SIM_CPU *cpu, INT in_GR)
{
  if (in_GR >= 0)
    {
      int wait = 0;
      while (load_pending_for_register (cpu, in_GR, 2/*words*/, REGTYPE_NONE))
	{
	  frv_model_advance_cycles (cpu, 1);
	  ++wait;
	}
      if (wait)
	{
	  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
	  ps->vliw_wait += wait;
	  ps->vliw_load_stall += wait;
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for gr%d:", in_GR);
	}
    }
}

void
load_wait_for_FRdouble (SIM_CPU *cpu, INT in_FR)
{
  if (in_FR >= 0)
    {
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      int *fr;
      int wait = 0;
      while (load_pending_for_register (cpu, in_FR, 2/*words*/, REGTYPE_FR))
	{
	  frv_model_advance_cycles (cpu, 1);
	  ++wait;
	}
      /* Post processing time may have been added to the registers'
	 latencies after the loads were processed. Account for that too.
      */
      fr = ps->fr_busy;
      if (fr[in_FR])
	{
	  wait += fr[in_FR];
	  frv_model_advance_cycles (cpu, fr[in_FR]);
	}
      if (in_FR < 63)
	{
	  if (fr[in_FR + 1])
	    {
	      wait += fr[in_FR + 1];
	      frv_model_advance_cycles (cpu, fr[in_FR + 1]);
	    }
	}
      /* Update the vliw_wait with the number of cycles we waited for the
	 load and any post-processing.  */
      if (wait)
	{
	  ps->vliw_wait += wait;
	  ps->vliw_load_stall += wait;
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
	}
    }
}

void
enforce_full_fr_latency (SIM_CPU *cpu, INT in_FR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  ps->fr_busy_adjust [in_FR] = -1;
}

/* Calculate how long the post processing for a floating point insn must
   wait for resources to become available.  */
int
post_wait_for_FR (SIM_CPU *cpu, INT in_FR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fr = ps->fr_busy;

  if (in_FR >= 0 && fr[in_FR] > ps->post_wait)
    {
      ps->post_wait = fr[in_FR];
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
    }
}

/* Calculate how long the post processing for a floating point insn must
   wait for resources to become available.  */
int
post_wait_for_FRdouble (SIM_CPU *cpu, INT in_FR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fr = ps->fr_busy;

  if (in_FR >= 0)
    {
      if (fr[in_FR] > ps->post_wait)
	{
	  ps->post_wait = fr[in_FR];
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR);
	}
      if (in_FR < 63 && fr[in_FR + 1] > ps->post_wait)
	{
	  ps->post_wait = fr[in_FR + 1];
	  if (TRACE_INSN_P (cpu))
	    sprintf (hazard_name, "Data hazard for fr%d:", in_FR + 1);
	}
    }
}

int
post_wait_for_ACC (SIM_CPU *cpu, INT in_ACC)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *acc = ps->acc_busy;

  if (in_ACC >= 0 && acc[in_ACC] > ps->post_wait)
    {
      ps->post_wait = acc[in_ACC];
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for acc%d:", in_ACC);
    }
}

int
post_wait_for_CCR (SIM_CPU *cpu, INT in_CCR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *ccr = ps->ccr_busy;

  if (in_CCR >= 0 && ccr[in_CCR] > ps->post_wait)
    {
      ps->post_wait = ccr[in_CCR];
      if (TRACE_INSN_P (cpu))
	{
	  if (in_CCR > 3)
	    sprintf (hazard_name, "Data hazard for icc%d:", in_CCR - 4);
	  else
	    sprintf (hazard_name, "Data hazard for fcc%d:", in_CCR);
	}
    }
}

int
post_wait_for_SPR (SIM_CPU *cpu, INT in_SPR)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *spr = ps->spr_busy;

  if (in_SPR >= 0 && spr[in_SPR] > ps->post_wait)
    {
      ps->post_wait = spr[in_SPR];
      if (TRACE_INSN_P (cpu))
	sprintf (hazard_name, "Data hazard for spr[%d]:", in_SPR);
    }
}

int
post_wait_for_fdiv (SIM_CPU *cpu, INT slot)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fdiv = ps->fdiv_busy;

  /* Multiple floating point divisions in the same slot need only wait 1
     extra cycle.  */
  if (fdiv[slot] > 0 && 1 > ps->post_wait)
    {
      ps->post_wait = 1;
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for floating point division in slot F%d:", slot);
	}
    }
}

int
post_wait_for_fsqrt (SIM_CPU *cpu, INT slot)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *fsqrt = ps->fsqrt_busy;

  /* Multiple floating point square roots in the same slot need only wait 1
     extra cycle.  */
  if (fsqrt[slot] > 0 && 1 > ps->post_wait)
    {
      ps->post_wait = 1;
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for square root in slot F%d:", slot);
	}
    }
}

int
post_wait_for_float (SIM_CPU *cpu, INT slot)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *flt = ps->float_busy;

  /* Multiple floating point square roots in the same slot need only wait 1
     extra cycle.  */
  if (flt[slot] > ps->post_wait)
    {
      ps->post_wait = flt[slot];
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for floating point unit in slot F%d:", slot);
	}
    }
}

int
post_wait_for_media (SIM_CPU *cpu, INT slot)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  int *media = ps->media_busy;

  /* Multiple floating point square roots in the same slot need only wait 1
     extra cycle.  */
  if (media[slot] > ps->post_wait)
    {
      ps->post_wait = media[slot];
      if (TRACE_INSN_P (cpu))
	{
	  sprintf (hazard_name, "Resource hazard for media unit in slot M%d:", slot);
	}
    }
}

/* Print cpu-specific profile information.  */
#define COMMAS(n) sim_add_commas (comma_buf, sizeof (comma_buf), (n))

static void
print_cache (SIM_CPU *cpu, FRV_CACHE *cache, const char *cache_name)
{
  SIM_DESC sd = CPU_STATE (cpu);

  if (cache != NULL)
    {
      char comma_buf[20];
      unsigned accesses;

      sim_io_printf (sd, "  %s Cache\n\n", cache_name);
      accesses = cache->statistics.accesses;
      sim_io_printf (sd, "    Total accesses:  %s\n", COMMAS (accesses));
      if (accesses != 0)
	{
	  float rate;
	  unsigned hits = cache->statistics.hits;
	  sim_io_printf (sd, "    Hits:            %s\n", COMMAS (hits));
	  rate = (float)hits / accesses;
	  sim_io_printf (sd, "    Hit rate:        %.2f%%\n", rate * 100);
	}
    }
  else
    sim_io_printf (sd, "  Model %s has no %s cache\n",
		   MODEL_NAME (CPU_MODEL (cpu)), cache_name);

  sim_io_printf (sd, "\n");
}

/* This table must correspond to the UNIT_ATTR table in
   opcodes/frv-desc.h. Only the units up to UNIT_C need be
   listed since the others cannot occur after mapping.  */
static char *
slot_names[] =
{
  "none",
  "I0", "I1", "I01", "I2", "I3", "IALL",
  "FM0", "FM1", "FM01", "FM2", "FM3", "FMALL", "FMLOW",
  "B0", "B1", "B01",
  "C"
};

static void
print_parallel (SIM_CPU *cpu, int verbose)
{
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
  unsigned total, vliw;
  char comma_buf[20];
  float average;

  sim_io_printf (sd, "Model %s Parallelization\n\n",
		 MODEL_NAME (CPU_MODEL (cpu)));

  total = PROFILE_TOTAL_INSN_COUNT (p);
  sim_io_printf (sd, "  Total instructions:           %s\n", COMMAS (total));
  vliw = ps->vliw_insns;
  sim_io_printf (sd, "  VLIW instructions:            %s\n", COMMAS (vliw));
  average = (float)total / vliw;
  sim_io_printf (sd, "  Average VLIW length:          %.2f\n", average);
  average = (float)PROFILE_MODEL_TOTAL_CYCLES (p) / vliw;
  sim_io_printf (sd, "  Cycles per VLIW instruction:  %.2f\n", average);
  average = (float)total / PROFILE_MODEL_TOTAL_CYCLES (p);
  sim_io_printf (sd, "  Instructions per cycle:       %.2f\n", average);

  if (verbose)
    {
      int i;
      int max_val = 0;
      int max_name_len = 0;
      for (i = UNIT_NIL + 1; i < UNIT_NUM_UNITS; ++i)
	{
	  if (INSNS_IN_SLOT (i))
	    {
	      int len;
	      if (INSNS_IN_SLOT (i) > max_val)
		max_val = INSNS_IN_SLOT (i);
	      len = strlen (slot_names[i]);
	      if (len > max_name_len)
		max_name_len = len;
	    }
	}
      if (max_val > 0)
	{
	  sim_io_printf (sd, "\n");
	  sim_io_printf (sd, "  Instructions per slot:\n");
	  sim_io_printf (sd, "\n");
	  for (i = UNIT_NIL + 1; i < UNIT_NUM_UNITS; ++i)
	    {
	      if (INSNS_IN_SLOT (i) != 0)
		{
		  sim_io_printf (sd, "  %*s: %*s: ",
				 max_name_len, slot_names[i],
				 max_val < 10000 ? 5 : 10,
				 COMMAS (INSNS_IN_SLOT (i)));
		  sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
					 INSNS_IN_SLOT (i),
					 max_val);
		  sim_io_printf (sd, "\n");
		}
	    }
	} /* details to print */
    } /* verbose */

  sim_io_printf (sd, "\n");
}

void
frv_profile_info (SIM_CPU *cpu, int verbose)
{
  /* FIXME: Need to add smp support.  */
  PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);

#if WITH_PROFILE_PARALLEL_P
  if (PROFILE_FLAGS (p) [PROFILE_PARALLEL_IDX])
    print_parallel (cpu, verbose);
#endif

#if WITH_PROFILE_CACHE_P
  if (PROFILE_FLAGS (p) [PROFILE_CACHE_IDX])
    {
      SIM_DESC sd = CPU_STATE (cpu);
      sim_io_printf (sd, "Model %s Cache Statistics\n\n",
		     MODEL_NAME (CPU_MODEL (cpu)));
      print_cache (cpu, CPU_INSN_CACHE (cpu), "Instruction");
      print_cache (cpu, CPU_DATA_CACHE (cpu), "Data");
    }
#endif /* WITH_PROFILE_CACHE_P */
}

/* A hack to get registers referenced for profiling.  */
SI frv_ref_SI (SI ref) {return ref;}
#endif /* WITH_PROFILE_MODEL_P */
