/* Simulator header for cgen parallel support.
   Copyright (C) 1999, 2000, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This file is part of the GNU instruction set simulator.

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

#ifndef CGEN_PAR_H
#define CGEN_PAR_H

/* Kinds of writes stored on the write queue.  */
enum cgen_write_queue_kind {
  CGEN_BI_WRITE, CGEN_QI_WRITE, CGEN_SI_WRITE, CGEN_SF_WRITE,
  CGEN_PC_WRITE,
  CGEN_FN_HI_WRITE, CGEN_FN_SI_WRITE, CGEN_FN_SF_WRITE,
  CGEN_FN_DI_WRITE, CGEN_FN_DF_WRITE,
  CGEN_FN_XI_WRITE, CGEN_FN_PC_WRITE,
  CGEN_MEM_QI_WRITE, CGEN_MEM_HI_WRITE, CGEN_MEM_SI_WRITE, CGEN_MEM_DI_WRITE,
  CGEN_MEM_DF_WRITE, CGEN_MEM_XI_WRITE,
  CGEN_FN_MEM_QI_WRITE, CGEN_FN_MEM_HI_WRITE, CGEN_FN_MEM_SI_WRITE,
  CGEN_FN_MEM_DI_WRITE, CGEN_FN_MEM_DF_WRITE, CGEN_FN_MEM_XI_WRITE,
  CGEN_NUM_WRITE_KINDS
};

/* Element of the write queue.  */
typedef struct {
  enum cgen_write_queue_kind kind; /* Used to select union member below.  */
  IADDR insn_address;       /* Address of the insn performing the write.  */
  unsigned32 flags;         /* Target specific flags.  */
  long       word1;         /* Target specific field.  */
  union {
    struct {
      BI  *target;
      BI   value;
    } bi_write;
    struct {
      UQI *target;
      QI   value;
    } qi_write;
    struct {
      SI *target;
      SI  value;
    } si_write;
    struct {
      SI *target;
      SF  value;
    } sf_write;
    struct {
      USI value;
    } pc_write;
    struct {
      UINT regno;
      UHI   value;
      void (*function)(SIM_CPU *, UINT, UHI);
    } fn_hi_write;
    struct {
      UINT regno;
      SI   value;
      void (*function)(SIM_CPU *, UINT, USI);
    } fn_si_write;
    struct {
      UINT regno;
      SF   value;
      void (*function)(SIM_CPU *, UINT, SF);
    } fn_sf_write;
    struct {
      UINT regno;
      DI   value;
      void (*function)(SIM_CPU *, UINT, DI);
    } fn_di_write;
    struct {
      UINT regno;
      DF   value;
      void (*function)(SIM_CPU *, UINT, DF);
    } fn_df_write;
    struct {
      UINT regno;
      SI   value[4];
      void (*function)(SIM_CPU *, UINT, SI *);
    } fn_xi_write;
    struct {
      USI  value;
      void (*function)(SIM_CPU *, USI);
    } fn_pc_write;
    struct {
      SI   address;
      QI   value;
    } mem_qi_write;
    struct {
      SI   address;
      HI   value;
    } mem_hi_write;
    struct {
      SI   address;
      SI   value;
    } mem_si_write;
    struct {
      SI   address;
      DI   value;
    } mem_di_write;
    struct {
      SI   address;
      DF   value;
    } mem_df_write;
    struct {
      SI   address;
      SI   value[4];
    } mem_xi_write;
    struct {
      SI   address;
      QI   value;
      void (*function)(SIM_CPU *, IADDR, SI, QI);
    } fn_mem_qi_write;
    struct {
      SI   address;
      HI   value;
      void (*function)(SIM_CPU *, IADDR, SI, HI);
    } fn_mem_hi_write;
    struct {
      SI   address;
      SI   value;
      void (*function)(SIM_CPU *, IADDR, SI, SI);
    } fn_mem_si_write;
    struct {
      SI   address;
      DI   value;
      void (*function)(SIM_CPU *, IADDR, SI, DI);
    } fn_mem_di_write;
    struct {
      SI   address;
      DF   value;
      void (*function)(SIM_CPU *, IADDR, SI, DF);
    } fn_mem_df_write;
    struct {
      SI   address;
      SI   value[4];
      void (*function)(SIM_CPU *, IADDR, SI, SI *);
    } fn_mem_xi_write;
  } kinds;
} CGEN_WRITE_QUEUE_ELEMENT;

#define CGEN_WRITE_QUEUE_ELEMENT_KIND(element) ((element)->kind)
#define CGEN_WRITE_QUEUE_ELEMENT_IADDR(element) ((element)->insn_address)
#define CGEN_WRITE_QUEUE_ELEMENT_FLAGS(element) ((element)->flags)
#define CGEN_WRITE_QUEUE_ELEMENT_WORD1(element) ((element)->word1)

extern void cgen_write_queue_element_execute (
  SIM_CPU *, CGEN_WRITE_QUEUE_ELEMENT *
);

/* Instance of the queue for parallel write-after support.  */
/* FIXME: Should be dynamic?  */
#define CGEN_WRITE_QUEUE_SIZE (64 * 4) /* 64 writes x 4 insns -- for now.  */

typedef struct {
  int index;
  CGEN_WRITE_QUEUE_ELEMENT q[CGEN_WRITE_QUEUE_SIZE];
} CGEN_WRITE_QUEUE;

#define CGEN_WRITE_QUEUE_CLEAR(queue)       ((queue)->index = 0)
#define CGEN_WRITE_QUEUE_INDEX(queue)       ((queue)->index)
#define CGEN_WRITE_QUEUE_ELEMENT(queue, ix) (&(queue)->q[(ix)])

#define CGEN_WRITE_QUEUE_NEXT(queue) (   \
  (queue)->index < CGEN_WRITE_QUEUE_SIZE \
    ? &(queue)->q[(queue)->index++]      \
    : cgen_write_queue_overflow (queue)  \
)

extern CGEN_WRITE_QUEUE_ELEMENT *cgen_write_queue_overflow (CGEN_WRITE_QUEUE *);

/* Functions for queuing writes.  Used by semantic code.  */
extern void sim_queue_bi_write (SIM_CPU *, BI *, BI);
extern void sim_queue_qi_write (SIM_CPU *, UQI *, UQI);
extern void sim_queue_si_write (SIM_CPU *, SI *, SI);
extern void sim_queue_sf_write (SIM_CPU *, SI *, SF);

extern void sim_queue_pc_write (SIM_CPU *, USI);

extern void sim_queue_fn_hi_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, UHI), UINT, UHI);
extern void sim_queue_fn_si_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, USI), UINT, USI);
extern void sim_queue_fn_sf_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, SF), UINT, SF);
extern void sim_queue_fn_di_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, DI), UINT, DI);
extern void sim_queue_fn_df_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, DF), UINT, DF);
extern void sim_queue_fn_xi_write (SIM_CPU *, void (*)(SIM_CPU *, UINT, SI *), UINT, SI *);
extern void sim_queue_fn_pc_write (SIM_CPU *, void (*)(SIM_CPU *, USI), USI);

extern void sim_queue_mem_qi_write (SIM_CPU *, SI, QI);
extern void sim_queue_mem_hi_write (SIM_CPU *, SI, HI);
extern void sim_queue_mem_si_write (SIM_CPU *, SI, SI);
extern void sim_queue_mem_di_write (SIM_CPU *, SI, DI);
extern void sim_queue_mem_df_write (SIM_CPU *, SI, DF);
extern void sim_queue_mem_xi_write (SIM_CPU *, SI, SI *);

extern void sim_queue_fn_mem_qi_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, QI), SI, QI);
extern void sim_queue_fn_mem_hi_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, HI), SI, HI);
extern void sim_queue_fn_mem_si_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, SI), SI, SI);
extern void sim_queue_fn_mem_di_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, DI), SI, DI);
extern void sim_queue_fn_mem_df_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, DF), SI, DF);
extern void sim_queue_fn_mem_xi_write (SIM_CPU *, void (*)(SIM_CPU *, IADDR, SI, SI *), SI, SI *);

#endif /* CGEN_PAR_H */
