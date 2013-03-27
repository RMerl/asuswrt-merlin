/*> cp1.h <*/
/* MIPS Simulator FPU (CoProcessor 1) definitions.
   Copyright (C) 1997, 1998, 2002, 2007 Free Software Foundation, Inc.
   Derived from sim-main.h contributed by Cygnus Solutions,
   modified substantially by Ed Satterthwaite of Broadcom Corporation
   (SiByte).

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

#ifndef CP1_H
#define CP1_H

/* See sim-main.h for allocation of registers FCR0 and FCR31 (FCSR) 
   in CPU state (struct sim_cpu), and for FPU functions.  */

#define fcsr_FCC_mask      (0xFE800000)
#define fcsr_FCC_shift     (23)
#define	fcsr_FCC_bit(cc)   ((cc) == 0 ? 23 : (24 + (cc)))
#define fcsr_FS            (1 << 24) /* MIPS III onwards : Flush to Zero */
#define fcsr_ZERO_mask     (0x007C0000)
#define fcsr_CAUSE_mask    (0x0003F000)
#define fcsr_CAUSE_shift   (12)
#define fcsr_ENABLES_mask  (0x00000F80)
#define fcsr_ENABLES_shift (7)
#define fcsr_FLAGS_mask    (0x0000007C)
#define fcsr_FLAGS_shift   (2)
#define fcsr_RM_mask       (0x00000003)
#define fcsr_RM_shift      (0)

#define fenr_FS            (0x00000004)

/* Macros to update and retrieve the FCSR condition-code bits.  This
   is complicated by the fact that there is a hole in the index range
   of the bits within the FCSR register.  (Note that the number of bits
   visible depends on the ISA in use, but that is handled elsewhere.)  */
#define SETFCC(cc,v) \
  do { \
    (FCSR = ((FCSR & ~(1 << fcsr_FCC_bit(cc))) | ((v) << fcsr_FCC_bit(cc)))); \
  } while (0)
#define GETFCC(cc) ((FCSR & (1 << fcsr_FCC_bit(cc))) != 0 ? 1 : 0)


/* Read flush-to-zero bit (not right-justified).  */
#define GETFS()            ((int)(FCSR & fcsr_FS))


/* FCSR flag bits definitions and access macros.  */
#define IR            0   /* I: Inexact Result */
#define UF            1   /* U: UnderFlow */
#define OF            2   /* O: OverFlow */
#define DZ            3   /* Z: Division by Zero */
#define IO            4   /* V: Invalid Operation */
#define UO            5   /* E: Unimplemented Operation (CAUSE field only) */

#define FP_FLAGS(b)   (1 << ((b) + fcsr_FLAGS_shift))
#define FP_ENABLE(b)  (1 << ((b) + fcsr_ENABLES_shift))
#define FP_CAUSE(b)   (1 << ((b) + fcsr_CAUSE_shift))


/* Rounding mode bit definitions and access macros.  */
#define FP_RM_NEAREST 0   /* Round to nearest (Round).  */
#define FP_RM_TOZERO  1   /* Round to zero (Trunc).  */
#define FP_RM_TOPINF  2   /* Round to Plus infinity (Ceil).  */
#define FP_RM_TOMINF  3   /* Round to Minus infinity (Floor).  */

#define GETRM()       ((FCSR >> fcsr_RM_shift) & fcsr_RM_mask)


#endif /* CP1_H */
