/* Support code for various pieces of CGEN simulators.
   Copyright (C) 1996, 1997, 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

#include "bfd.h"
#include "sim-main.h"
#include "dis-asm.h"

#define MEMOPS_DEFINE_INLINE
#include "cgen-mem.h"

#define SEMOPS_DEFINE_INLINE
#include "cgen-ops.h"

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

const char *mode_names[] = {
  "VOID",
  "BI",
  "QI",
  "HI",
  "SI",
  "DI",
  "UQI",
  "UHI",
  "USI",
  "UDI",
  "SF",
  "DF",
  "XF",
  "TF",
  0, /* MODE_TARGET_MAX */
  "INT",
  "UINT",
  "PTR"
};

/* Opcode table for virtual insns used by the simulator.  */

#define V CGEN_ATTR_MASK (CGEN_INSN_VIRTUAL)

static const CGEN_IBASE virtual_insn_entries[] =
{
  {
    VIRTUAL_INSN_X_INVALID, "--invalid--", NULL, 0, { V, { 0 } }
  },
  {
    VIRTUAL_INSN_X_BEFORE, "--before--", NULL, 0, { V, { 0 } }
  },
  {
    VIRTUAL_INSN_X_AFTER, "--after--", NULL, 0, { V, { 0 } }
  },
  {
    VIRTUAL_INSN_X_BEGIN, "--begin--", NULL, 0, { V, { 0 } }
  },
  {
    VIRTUAL_INSN_X_CHAIN, "--chain--", NULL, 0, { V, { 0 } }
  },
  {
    VIRTUAL_INSN_X_CTI_CHAIN, "--cti-chain--", NULL, 0, { V, { 0 } }
  }
};

#undef V

const CGEN_INSN cgen_virtual_insn_table[] =
{
  { & virtual_insn_entries[0] },
  { & virtual_insn_entries[1] },
  { & virtual_insn_entries[2] },
  { & virtual_insn_entries[3] },
  { & virtual_insn_entries[4] },
  { & virtual_insn_entries[5] }
};

/* Initialize cgen things.
   This is called after sim_post_argv_init.  */

void
cgen_init (SIM_DESC sd)
{
  int i, c;

  /* If no profiling or tracing has been enabled, run in fast mode.  */
  {
    int run_fast_p = 1;

    for (c = 0; c < MAX_NR_PROCESSORS; ++c)
      {
	SIM_CPU *cpu = STATE_CPU (sd, c);

	for (i = 0; i < MAX_PROFILE_VALUES; ++i)
	  if (CPU_PROFILE_FLAGS (cpu) [i])
	    {
	      run_fast_p = 0;
	      break;
	    }
	for (i = 0; i < MAX_TRACE_VALUES; ++i)
	  if (CPU_TRACE_FLAGS (cpu) [i])
	    {
	      run_fast_p = 0;
	      break;
	    }
	if (! run_fast_p)
	  break;
      }
    STATE_RUN_FAST_P (sd) = run_fast_p;
  }
}

/* Return the name of insn number I.  */

const char *
cgen_insn_name (SIM_CPU *cpu, int i)
{
  return CGEN_INSN_NAME ((* CPU_GET_IDATA (cpu)) ((cpu), (i)));
}

/* Return the maximum number of extra bytes required for a SIM_CPU struct.  */

int
cgen_cpu_max_extra_bytes (void)
{
  int i;
  int extra = 0;

  for (i = 0; sim_machs[i] != 0; ++i)
    {
      int size = IMP_PROPS_SIM_CPU_SIZE (MACH_IMP_PROPS (sim_machs[i]));
      if (size > extra)
	extra = size;
    }
  return extra;
}

#ifdef DI_FN_SUPPORT

DI
make_struct_di (hi, lo)
     SI hi, lo;
{
  DI result;

  result.hi = hi;
  result.lo = lo;
  return result;
}

DI
ANDDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  SI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  SI blo = GETLODI (b);
  return MAKEDI (ahi & bhi, alo & blo);
}

DI
ORDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  SI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  SI blo = GETLODI (b);
  return MAKEDI (ahi | bhi, alo | blo);
}

DI
ADDDI (a, b)
     DI a, b;
{
  USI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  USI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  USI x = alo + blo;
  return MAKEDI (ahi + bhi + (x < alo), x);
}

DI
MULDI (a, b)
     DI a, b;
{
  USI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  USI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  USI rhi,rlo;
  USI x0, x1, x2, x3;

  x0 = alo * blo;
  x1 = alo * bhi;
  x2 = ahi * blo;
  x3 = ahi * bhi;

#define SI_TYPE_SIZE 32
#define BITS4 (SI_TYPE_SIZE / 4)
#define ll_B (1L << (SI_TYPE_SIZE / 2))
#define ll_lowpart(t) ((USI) (t) % ll_B)
#define ll_highpart(t) ((USI) (t) / ll_B)
  x1 += ll_highpart (x0);	/* this can't give carry */
  x1 += x2;			/* but this indeed can */
  if (x1 < x2)			/* did we get it? */
    x3 += ll_B;			/* yes, add it in the proper pos. */

  rhi = x3 + ll_highpart (x1);
  rlo = ll_lowpart (x1) * ll_B + ll_lowpart (x0);
  return MAKEDI (rhi + (alo * bhi) + (ahi * blo), rlo);
}

DI
SHLDI (val, shift)
     DI val;
     SI shift;
{
  USI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI ((hi << shift) | (lo >> (32 - shift)), lo << shift);
}

DI
SLADI (val, shift)
     DI val;
     SI shift;
{
  SI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI ((hi << shift) | (lo >> (32 - shift)), lo << shift);
}

DI
SRADI (val, shift)
     DI val;
     SI shift;
{
  SI hi = GETHIDI (val);
  USI lo = GETLODI (val);
  /* We use SRASI because the result is implementation defined if hi < 0.  */
  /* FIXME: Need to worry about shift < 0 || shift >= 32.  */
  return MAKEDI (SRASI (hi, shift), (hi << (32 - shift)) | (lo >> shift));
}

int
GEDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  if (ahi > bhi)
    return 1;
  if (ahi == bhi)
    return alo >= blo;
  return 0;
}

int
LEDI (a, b)
     DI a, b;
{
  SI ahi = GETHIDI (a);
  USI alo = GETLODI (a);
  SI bhi = GETHIDI (b);
  USI blo = GETLODI (b);
  if (ahi < bhi)
    return 1;
  if (ahi == bhi)
    return alo <= blo;
  return 0;
}

DI
CONVHIDI (val)
     HI val;
{
  if (val < 0)
    return MAKEDI (-1, val);
  else
    return MAKEDI (0, val);
}

DI
CONVSIDI (val)
     SI val;
{
  if (val < 0)
    return MAKEDI (-1, val);
  else
    return MAKEDI (0, val);
}

SI
CONVDISI (val)
     DI val;
{
  return GETLODI (val);
}

#endif /* DI_FN_SUPPORT */

QI
RORQI (val, shift)
     QI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 8 - shift;
      int mask = (1 << shift) - 1;
      QI result = (val & mask) << remain;
      mask = (1 << remain) - 1;
      result |= (val >> shift) & mask;
      return result;
    }
  return val;
}

QI
ROLQI (val, shift)
     QI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 8 - shift;
      int mask = (1 << remain) - 1;
      QI result = (val & mask) << shift;
      mask = (1 << shift) - 1;
      result |= (val >> remain) & mask;
      return result;
    }
  return val;
}

HI
RORHI (val, shift)
     HI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 16 - shift;
      int mask = (1 << shift) - 1;
      HI result = (val & mask) << remain;
      mask = (1 << remain) - 1;
      result |= (val >> shift) & mask;
      return result;
    }
  return val;
}

HI
ROLHI (val, shift)
     HI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 16 - shift;
      int mask = (1 << remain) - 1;
      HI result = (val & mask) << shift;
      mask = (1 << shift) - 1;
      result |= (val >> remain) & mask;
      return result;
    }
  return val;
}

SI
RORSI (val, shift)
     SI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 32 - shift;
      int mask = (1 << shift) - 1;
      SI result = (val & mask) << remain;
      mask = (1 << remain) - 1;
      result |= (val >> shift) & mask;
      return result;
    }
  return val;
}

SI
ROLSI (val, shift)
     SI  val;
     int shift;
{
  if (shift != 0)
    {
      int remain = 32 - shift;
      int mask = (1 << remain) - 1;
      SI result = (val & mask) << shift;
      mask = (1 << shift) - 1;
      result |= (val >> remain) & mask;
      return result;
    }

  return val;
}

/* Emit an error message from CGEN RTL.  */

void
cgen_rtx_error (SIM_CPU *cpu, const char * msg)
{
  SIM_DESC sd = CPU_STATE (cpu);

  sim_io_printf (sd, msg);
  sim_io_printf (sd, "\n");

  sim_engine_halt (sd, cpu, NULL, CIA_GET (cpu), sim_stopped, SIM_SIGTRAP);
}
