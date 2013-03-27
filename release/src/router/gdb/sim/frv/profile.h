/* Profiling definitions for the FRV simulator
   Copyright (C) 1998, 1999, 2000, 2001, 2003, 2007
   Free Software Foundation, Inc.
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

#ifndef PROFILE_H
#define PROFILE_H

#include "frv-desc.h"

/* This struct defines the state of profiling.  All fields are of general
   use to all machines.  */
typedef struct
{
  long vliw_insns; /* total number of VLIW insns.  */
  long vliw_wait;  /* number of cycles that the current VLIW insn must wait.  */
  long post_wait;  /* number of cycles that post processing in the current
                      VLIW insn must wait.  */
  long vliw_cycles;/* number of cycles used by current VLIW insn.  */

  int  past_first_p; /* Not the first insns in the VLIW */

  /* Register latencies.  Must be signed since they can be temporarily
     negative.  */
  int gr_busy[64];       /* Cycles until GR is available.  */
  int fr_busy[64];       /* Cycles until FR is available.  */
  int acc_busy[64];      /* Cycles until FR is available.  */
  int ccr_busy[8];       /* Cycles until ICC/FCC is available.  */
  int spr_busy[4096];    /* Cycles until spr is available.  */
  int idiv_busy[2];      /* Cycles until integer division unit is available.  */
  int fdiv_busy[2];      /* Cycles until float division unit is available.  */
  int fsqrt_busy[2];     /* Cycles until square root unit is available.  */
  int float_busy[4];     /* Cycles until floating point unit is available.  */
  int media_busy[4];     /* Cycles until media unit is available.  */
  int branch_penalty;    /* Cycles until branch is complete.  */

  int gr_latency[64];    /* Cycles until target GR is available.  */
  int fr_latency[64];    /* Cycles until target FR is available.  */
  int acc_latency[64];   /* Cycles until target FR is available.  */
  int ccr_latency[8];    /* Cycles until target ICC/FCC is available.  */
  int spr_latency[4096]; /* Cycles until target spr is available.  */

  /* Some registers are busy for a shorter number of cycles than normal
     depending on how they are used next. the xxx_busy_adjust arrays keep track
     of how many cycles to adjust down.
  */
  int fr_busy_adjust[64];
  int acc_busy_adjust[64];

  /* Register flags.  Each bit represents one register.  */
  DI cur_gr_complex;
  DI prev_gr_complex;

  /* Keep track of the total queued post-processing time required before a
     resource is available.  This is applied to the resource's latency once all
     pending loads for the resource are completed.  */
  int fr_ptime[64];

  int branch_hint;       /* hint field from branch insn.  */
  USI branch_address;    /* Address of predicted branch.  */
  USI insn_fetch_address;/* Address of sequential insns fetched.  */
  int mclracc_acc;       /* ACC number of register cleared by mclracc.  */
  int mclracc_A;         /* A field of mclracc.  */

  /* We need to know when the first branch of a vliw insn is taken, so that
     we don't consider the remaining branches in the vliw insn.  */
  int vliw_branch_taken;

  /* Keep track of the maximum load stall for each VLIW insn.  */
  int vliw_load_stall;

  /* Need to know if all cache entries are affected by various cache
     operations.  */
  int all_cache_entries;
} FRV_PROFILE_STATE;

#define DUAL_REG(reg) ((reg) >= 0 && (reg) < 63 ? (reg) + 1 : -1)
#define DUAL_DOUBLE(reg) ((reg) >= 0 && (reg) < 61 ? (reg) + 2 : -1)

/* Return the GNER register associated with the given GR register.
   There is no GNER associated with gr0.  */
#define GNER_FOR_GR(gr) ((gr) > 63 ? -1 : \
                         (gr) > 31 ? H_SPR_GNER0 : \
                         (gr) >  0 ? H_SPR_GNER1 : \
                         -1)
/* Return the GNER register associated with the given GR register.
   There is no GNER associated with gr0.  */
#define FNER_FOR_FR(fr) ((fr) > 63 ? -1 : \
                         (fr) > 31 ? H_SPR_FNER0 : \
                         (fr) >  0 ? H_SPR_FNER1 : \
                         -1)

/* Top up the latency of the given GR by the given number of cycles.  */
void update_GR_latency (SIM_CPU *, INT, int);
void update_GRdouble_latency (SIM_CPU *, INT, int);
void update_GR_latency_for_load (SIM_CPU *, INT, int);
void update_GRdouble_latency_for_load (SIM_CPU *, INT, int);
void update_GR_latency_for_swap (SIM_CPU *, INT, int);
void update_FR_latency (SIM_CPU *, INT, int);
void update_FRdouble_latency (SIM_CPU *, INT, int);
void update_FR_latency_for_load (SIM_CPU *, INT, int);
void update_FRdouble_latency_for_load (SIM_CPU *, INT, int);
void update_FR_ptime (SIM_CPU *, INT, int);
void update_FRdouble_ptime (SIM_CPU *, INT, int);
void decrease_ACC_busy (SIM_CPU *, INT, int);
void decrease_FR_busy (SIM_CPU *, INT, int);
void decrease_GR_busy (SIM_CPU *, INT, int);
void increase_FR_busy (SIM_CPU *, INT, int);
void increase_ACC_busy (SIM_CPU *, INT, int);
void update_ACC_latency (SIM_CPU *, INT, int);
void update_CCR_latency (SIM_CPU *, INT, int);
void update_SPR_latency (SIM_CPU *, INT, int);
void update_idiv_resource_latency (SIM_CPU *, INT, int);
void update_fdiv_resource_latency (SIM_CPU *, INT, int);
void update_fsqrt_resource_latency (SIM_CPU *, INT, int);
void update_float_resource_latency (SIM_CPU *, INT, int);
void update_media_resource_latency (SIM_CPU *, INT, int);
void update_branch_penalty (SIM_CPU *, int);
void update_ACC_ptime (SIM_CPU *, INT, int);
void update_SPR_ptime (SIM_CPU *, INT, int);
void vliw_wait_for_GR (SIM_CPU *, INT);
void vliw_wait_for_GRdouble (SIM_CPU *, INT);
void vliw_wait_for_FR (SIM_CPU *, INT);
void vliw_wait_for_FRdouble (SIM_CPU *, INT);
void vliw_wait_for_CCR (SIM_CPU *, INT);
void vliw_wait_for_ACC (SIM_CPU *, INT);
void vliw_wait_for_SPR (SIM_CPU *, INT);
void vliw_wait_for_idiv_resource (SIM_CPU *, INT);
void vliw_wait_for_fdiv_resource (SIM_CPU *, INT);
void vliw_wait_for_fsqrt_resource (SIM_CPU *, INT);
void vliw_wait_for_float_resource (SIM_CPU *, INT);
void vliw_wait_for_media_resource (SIM_CPU *, INT);
void load_wait_for_GR (SIM_CPU *, INT);
void load_wait_for_FR (SIM_CPU *, INT);
void load_wait_for_GRdouble (SIM_CPU *, INT);
void load_wait_for_FRdouble (SIM_CPU *, INT);
void enforce_full_fr_latency (SIM_CPU *, INT);
void enforce_full_acc_latency (SIM_CPU *, INT);
int post_wait_for_FR (SIM_CPU *, INT);
int post_wait_for_FRdouble (SIM_CPU *, INT);
int post_wait_for_ACC (SIM_CPU *, INT);
int post_wait_for_CCR (SIM_CPU *, INT);
int post_wait_for_SPR (SIM_CPU *, INT);
int post_wait_for_fdiv (SIM_CPU *, INT);
int post_wait_for_fsqrt (SIM_CPU *, INT);
int post_wait_for_float (SIM_CPU *, INT);
int post_wait_for_media (SIM_CPU *, INT);

void trace_vliw_wait_cycles (SIM_CPU *);
void handle_resource_wait (SIM_CPU *);

void request_cache_load (SIM_CPU *, INT, int, int);
void request_cache_flush (SIM_CPU *, FRV_CACHE *, int);
void request_cache_invalidate (SIM_CPU *, FRV_CACHE *, int);
void request_cache_preload (SIM_CPU *, FRV_CACHE *, int);
void request_cache_unlock (SIM_CPU *, FRV_CACHE *, int);
int  load_pending_for_register (SIM_CPU *, int, int, int);

void set_use_is_gr_complex (SIM_CPU *, INT);
void set_use_not_gr_complex (SIM_CPU *, INT);
int  use_is_gr_complex (SIM_CPU *, INT);

typedef struct
{
  SI address;
  unsigned reqno;
} FRV_INSN_FETCH_BUFFER;

extern FRV_INSN_FETCH_BUFFER frv_insn_fetch_buffer[];

PROFILE_INFO_CPU_CALLBACK_FN frv_profile_info;

enum {
  /* Simulator specific profile bits begin here.  */
  /* Profile caches.  */
  PROFILE_CACHE_IDX = PROFILE_NEXT_IDX,
  /* Profile parallelization.  */
  PROFILE_PARALLEL_IDX
};

/* Masks so WITH_PROFILE can have symbolic values.
   The case choice here is on purpose.  The lowercase parts are args to
   --with-profile.  */
#define PROFILE_cache    (1 << PROFILE_INSN_IDX)
#define PROFILE_parallel (1 << PROFILE_INSN_IDX)

/* Preprocessor macros to simplify tests of WITH_PROFILE.  */
#define WITH_PROFILE_CACHE_P    (WITH_PROFILE & PROFILE_insn)
#define WITH_PROFILE_PARALLEL_P (WITH_PROFILE & PROFILE_insn)

#define FRV_COUNT_CYCLES(cpu, condition) \
  ((PROFILE_MODEL_P (cpu) && (condition)) || frv_interrupt_state.timer.enabled)

/* Modelling support.  */
extern int frv_save_profile_model_p;

extern enum FRV_INSN_MODELING {
  FRV_INSN_NO_MODELING = 0,
  FRV_INSN_MODEL_PASS_1,
  FRV_INSN_MODEL_PASS_2,
  FRV_INSN_MODEL_WRITEBACK
} model_insn;

void
frv_model_advance_cycles (SIM_CPU *, int);
void
frv_model_trace_wait_cycles (SIM_CPU *, int, const char *);

/* Register types for queued load requests.  */
#define REGTYPE_NONE 0
#define REGTYPE_FR   1
#define REGTYPE_ACC  2

#endif /* PROFILE_H */
