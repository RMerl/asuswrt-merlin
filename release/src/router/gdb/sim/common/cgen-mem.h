/* Memory ops header for CGEN-based simulators.
   Copyright (C) 1996, 1997, 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

#ifndef CGEN_MEM_H
#define CGEN_MEM_H

#ifdef MEMOPS_DEFINE_INLINE
#define MEMOPS_INLINE
#else
#define MEMOPS_INLINE extern inline
#endif

/* Integer memory read support.

   There is no floating point support.  In this context there are no
   floating point modes, only floating point operations (whose arguments
   and results are arrays of bits that we treat as integer modes).  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_GETMEM(mode, size) \
MEMOPS_INLINE mode \
XCONCAT2 (GETMEM,mode) (SIM_CPU *cpu, IADDR pc, ADDR a) \
{ \
  PROFILE_COUNT_READ (cpu, a, XCONCAT2 (MODE_,mode)); \
  /* Don't read anything into "unaligned" here.  Bad name choice.  */\
  return XCONCAT2 (sim_core_read_unaligned_,size) (cpu, pc, read_map, a); \
}
#else
#define DECLARE_GETMEM(mode, size) \
extern mode XCONCAT2 (GETMEM,mode) (SIM_CPU *, IADDR, ADDR);
#endif

DECLARE_GETMEM (QI, 1)  /* TAGS: GETMEMQI */
DECLARE_GETMEM (UQI, 1) /* TAGS: GETMEMUQI */
DECLARE_GETMEM (HI, 2)  /* TAGS: GETMEMHI */
DECLARE_GETMEM (UHI, 2) /* TAGS: GETMEMUHI */
DECLARE_GETMEM (SI, 4)  /* TAGS: GETMEMSI */
DECLARE_GETMEM (USI, 4) /* TAGS: GETMEMUSI */
DECLARE_GETMEM (DI, 8)  /* TAGS: GETMEMDI */
DECLARE_GETMEM (UDI, 8) /* TAGS: GETMEMUDI */

#undef DECLARE_GETMEM

/* Integer memory write support.  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_SETMEM(mode, size) \
MEMOPS_INLINE void \
XCONCAT2 (SETMEM,mode) (SIM_CPU *cpu, IADDR pc, ADDR a, mode val) \
{ \
  PROFILE_COUNT_WRITE (cpu, a, XCONCAT2 (MODE_,mode)); \
  /* Don't read anything into "unaligned" here.  Bad name choice.  */ \
  XCONCAT2 (sim_core_write_unaligned_,size) (cpu, pc, write_map, a, val); \
}
#else
#define DECLARE_SETMEM(mode, size) \
extern void XCONCAT2 (SETMEM,mode) (SIM_CPU *, IADDR, ADDR, mode);
#endif

DECLARE_SETMEM (QI, 1)  /* TAGS: SETMEMQI */
DECLARE_SETMEM (UQI, 1) /* TAGS: SETMEMUQI */
DECLARE_SETMEM (HI, 2)  /* TAGS: SETMEMHI */
DECLARE_SETMEM (UHI, 2) /* TAGS: SETMEMUHI */
DECLARE_SETMEM (SI, 4)  /* TAGS: SETMEMSI */
DECLARE_SETMEM (USI, 4) /* TAGS: SETMEMUSI */
DECLARE_SETMEM (DI, 8)  /* TAGS: SETMEMDI */
DECLARE_SETMEM (UDI, 8) /* TAGS: SETMEMUDI */

#undef DECLARE_SETMEM

/* Instruction read support.  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_GETIMEM(mode, size) \
MEMOPS_INLINE mode \
XCONCAT2 (GETIMEM,mode) (SIM_CPU *cpu, IADDR a) \
{ \
  /*PROFILE_COUNT_READ (cpu, a, XCONCAT2 (MODE_,mode));*/ \
  /* Don't read anything into "unaligned" here.  Bad name choice.  */\
  return XCONCAT2 (sim_core_read_unaligned_,size) (cpu, a, exec_map, a); \
}
#else
#define DECLARE_GETIMEM(mode, size) \
extern mode XCONCAT2 (GETIMEM,mode) (SIM_CPU *, ADDR);
#endif

DECLARE_GETIMEM (UQI, 1) /* TAGS: GETIMEMUQI */
DECLARE_GETIMEM (UHI, 2) /* TAGS: GETIMEMUHI */
DECLARE_GETIMEM (USI, 4) /* TAGS: GETIMEMUSI */
DECLARE_GETIMEM (UDI, 8) /* TAGS: GETIMEMUDI */

#undef DECLARE_GETIMEM

/* Floating point support.

   ??? One can specify that the integer memory ops should be used instead,
   and treat fp values as just a series of bits.  One might even bubble
   that notion up into the description language.  However, that departs from
   gcc.  One could cross over from gcc's notion and a "series of bits" notion
   between there and here, and thus still not require these routines.  However,
   that's a complication of its own (not that having these fns isn't).
   But for now, we do things this way.  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_GETMEM(mode, size) \
MEMOPS_INLINE mode \
XCONCAT2 (GETMEM,mode) (SIM_CPU *cpu, IADDR pc, ADDR a) \
{ \
  PROFILE_COUNT_READ (cpu, a, XCONCAT2 (MODE_,mode)); \
  /* Don't read anything into "unaligned" here.  Bad name choice.  */\
  return XCONCAT2 (sim_core_read_unaligned_,size) (cpu, pc, read_map, a); \
}
#else
#define DECLARE_GETMEM(mode, size) \
extern mode XCONCAT2 (GETMEM,mode) (SIM_CPU *, IADDR, ADDR);
#endif

DECLARE_GETMEM (SF, 4) /* TAGS: GETMEMSF */
DECLARE_GETMEM (DF, 8) /* TAGS: GETMEMDF */

#undef DECLARE_GETMEM

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_SETMEM(mode, size) \
MEMOPS_INLINE void \
XCONCAT2 (SETMEM,mode) (SIM_CPU *cpu, IADDR pc, ADDR a, mode val) \
{ \
  PROFILE_COUNT_WRITE (cpu, a, XCONCAT2 (MODE_,mode)); \
  /* Don't read anything into "unaligned" here.  Bad name choice.  */ \
  XCONCAT2 (sim_core_write_unaligned_,size) (cpu, pc, write_map, a, val); \
}
#else
#define DECLARE_SETMEM(mode, size) \
extern void XCONCAT2 (SETMEM,mode) (SIM_CPU *, IADDR, ADDR, mode);
#endif

DECLARE_SETMEM (SF, 4) /* TAGS: SETMEMSF */
DECLARE_SETMEM (DF, 8) /* TAGS: SETMEMDF */

#undef DECLARE_SETMEM

/* GETT<mode>: translate target value at P to host value.
   This needn't be very efficient (i.e. can call memcpy) as this is
   only used when interfacing with the outside world (e.g. gdb).  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_GETT(mode, size) \
MEMOPS_INLINE mode \
XCONCAT2 (GETT,mode) (unsigned char *p) \
{ \
  mode tmp; \
  memcpy (&tmp, p, sizeof (mode)); \
  return XCONCAT2 (T2H_,size) (tmp); \
}
#else
#define DECLARE_GETT(mode, size) \
extern mode XCONCAT2 (GETT,mode) (unsigned char *);
#endif

DECLARE_GETT (QI, 1)  /* TAGS: GETTQI */
DECLARE_GETT (UQI, 1) /* TAGS: GETTUQI */
DECLARE_GETT (HI, 2)  /* TAGS: GETTHI */
DECLARE_GETT (UHI, 2) /* TAGS: GETTUHI */
DECLARE_GETT (SI, 4)  /* TAGS: GETTSI */
DECLARE_GETT (USI, 4) /* TAGS: GETTUSI */
DECLARE_GETT (DI, 8)  /* TAGS: GETTDI */
DECLARE_GETT (UDI, 8) /* TAGS: GETTUDI */

#if 0 /* ??? defered until necessary */
DECLARE_GETT (SF, 4)  /* TAGS: GETTSF */
DECLARE_GETT (DF, 8)  /* TAGS: GETTDF */
DECLARE_GETT (TF, 16) /* TAGS: GETTTF */
#endif

#undef DECLARE_GETT

/* SETT<mode>: translate host value to target value and store at P.
   This needn't be very efficient (i.e. can call memcpy) as this is
   only used when interfacing with the outside world (e.g. gdb).  */

#if defined (__GNUC__) || defined (MEMOPS_DEFINE_INLINE)
#define DECLARE_SETT(mode, size) \
MEMOPS_INLINE void \
XCONCAT2 (SETT,mode) (unsigned char *buf, mode val) \
{ \
  mode tmp; \
  tmp = XCONCAT2 (H2T_,size) (val); \
  memcpy (buf, &tmp, sizeof (mode)); \
}
#else
#define DECLARE_SETT(mode, size) \
extern mode XCONCAT2 (GETT,mode) (unsigned char *, mode);
#endif

DECLARE_SETT (QI, 1)  /* TAGS: SETTQI */
DECLARE_SETT (UQI, 1) /* TAGS: SETTUQI */
DECLARE_SETT (HI, 2)  /* TAGS: SETTHI */
DECLARE_SETT (UHI, 2) /* TAGS: SETTUHI */
DECLARE_SETT (SI, 4)  /* TAGS: SETTSI */
DECLARE_SETT (USI, 4) /* TAGS: SETTUSI */
DECLARE_SETT (DI, 8)  /* TAGS: SETTDI */
DECLARE_SETT (UDI, 8) /* TAGS: SETTUDI */

#if 0 /* ??? defered until necessary */
DECLARE_SETT (SF, 4)  /* TAGS: SETTSF */
DECLARE_SETT (DF, 8)  /* TAGS: SETTDF */
DECLARE_SETT (TF, 16) /* TAGS: SETTTF */
#endif

#undef DECLARE_SETT

#endif /* CGEN_MEM_H */
