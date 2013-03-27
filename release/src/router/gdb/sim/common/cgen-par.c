/* Simulator parallel routines for CGEN simulators (and maybe others).
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

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-par.h"

/* Functions required by the cgen interface.  These functions add various
   kinds of writes to the write queue.  */
void sim_queue_bi_write (SIM_CPU *cpu, BI *target, BI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_BI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.bi_write.target = target;
  element->kinds.bi_write.value  = value;
}

void sim_queue_qi_write (SIM_CPU *cpu, UQI *target, UQI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_QI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.qi_write.target = target;
  element->kinds.qi_write.value  = value;
}

void sim_queue_si_write (SIM_CPU *cpu, SI *target, SI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_SI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.si_write.target = target;
  element->kinds.si_write.value  = value;
}

void sim_queue_sf_write (SIM_CPU *cpu, SI *target, SF value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_SF_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.sf_write.target = target;
  element->kinds.sf_write.value  = value;
}

void sim_queue_pc_write (SIM_CPU *cpu, USI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_PC_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.pc_write.value = value;
}

void sim_queue_fn_hi_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, UHI),
  UINT regno,
  UHI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_HI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_hi_write.function = write_function;
  element->kinds.fn_hi_write.regno = regno;
  element->kinds.fn_hi_write.value = value;
}

void sim_queue_fn_si_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, USI),
  UINT regno,
  USI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_SI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_si_write.function = write_function;
  element->kinds.fn_si_write.regno = regno;
  element->kinds.fn_si_write.value = value;
}

void sim_queue_fn_sf_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, SF),
  UINT regno,
  SF value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_SF_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_sf_write.function = write_function;
  element->kinds.fn_sf_write.regno = regno;
  element->kinds.fn_sf_write.value = value;
}

void sim_queue_fn_di_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, DI),
  UINT regno,
  DI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_DI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_di_write.function = write_function;
  element->kinds.fn_di_write.regno = regno;
  element->kinds.fn_di_write.value = value;
}

void sim_queue_fn_xi_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, SI *),
  UINT regno,
  SI *value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_XI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_xi_write.function = write_function;
  element->kinds.fn_xi_write.regno = regno;
  element->kinds.fn_xi_write.value[0] = value[0];
  element->kinds.fn_xi_write.value[1] = value[1];
  element->kinds.fn_xi_write.value[2] = value[2];
  element->kinds.fn_xi_write.value[3] = value[3];
}

void sim_queue_fn_df_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, UINT, DF),
  UINT regno,
  DF value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_DF_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_df_write.function = write_function;
  element->kinds.fn_df_write.regno = regno;
  element->kinds.fn_df_write.value = value;
}

void sim_queue_fn_pc_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, USI),
  USI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_PC_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_pc_write.function = write_function;
  element->kinds.fn_pc_write.value = value;
}

void sim_queue_mem_qi_write (SIM_CPU *cpu, SI address, QI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_QI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_qi_write.address = address;
  element->kinds.mem_qi_write.value   = value;
}

void sim_queue_mem_hi_write (SIM_CPU *cpu, SI address, HI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_HI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_hi_write.address = address;
  element->kinds.mem_hi_write.value   = value;
}

void sim_queue_mem_si_write (SIM_CPU *cpu, SI address, SI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_SI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_si_write.address = address;
  element->kinds.mem_si_write.value   = value;
}

void sim_queue_mem_di_write (SIM_CPU *cpu, SI address, DI value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_DI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_di_write.address = address;
  element->kinds.mem_di_write.value   = value;
}

void sim_queue_mem_df_write (SIM_CPU *cpu, SI address, DF value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_DF_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_df_write.address = address;
  element->kinds.mem_df_write.value   = value;
}

void sim_queue_mem_xi_write (SIM_CPU *cpu, SI address, SI *value)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_MEM_XI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.mem_xi_write.address = address;
  element->kinds.mem_xi_write.value[0] = value[0];
  element->kinds.mem_xi_write.value[1] = value[1];
  element->kinds.mem_xi_write.value[2] = value[2];
  element->kinds.mem_xi_write.value[3] = value[3];
}

void sim_queue_fn_mem_qi_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, QI),
  SI address,
  QI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_QI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_qi_write.function = write_function;
  element->kinds.fn_mem_qi_write.address = address;
  element->kinds.fn_mem_qi_write.value   = value;
}

void sim_queue_fn_mem_hi_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, HI),
  SI address,
  HI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_HI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_hi_write.function = write_function;
  element->kinds.fn_mem_hi_write.address = address;
  element->kinds.fn_mem_hi_write.value   = value;
}

void sim_queue_fn_mem_si_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, SI),
  SI address,
  SI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_SI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_si_write.function = write_function;
  element->kinds.fn_mem_si_write.address = address;
  element->kinds.fn_mem_si_write.value   = value;
}

void sim_queue_fn_mem_di_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, DI),
  SI address,
  DI value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_DI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_di_write.function = write_function;
  element->kinds.fn_mem_di_write.address = address;
  element->kinds.fn_mem_di_write.value   = value;
}

void sim_queue_fn_mem_df_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, DF),
  SI address,
  DF value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_DF_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_df_write.function = write_function;
  element->kinds.fn_mem_df_write.address = address;
  element->kinds.fn_mem_df_write.value   = value;
}

void sim_queue_fn_mem_xi_write (
  SIM_CPU *cpu,
  void (*write_function)(SIM_CPU *cpu, IADDR, SI, SI *),
  SI address,
  SI *value
)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (cpu);
  CGEN_WRITE_QUEUE_ELEMENT *element = CGEN_WRITE_QUEUE_NEXT (q);
  element->kind = CGEN_FN_MEM_XI_WRITE;
  element->insn_address = CPU_PC_GET (cpu);
  element->kinds.fn_mem_xi_write.function = write_function;
  element->kinds.fn_mem_xi_write.address = address;
  element->kinds.fn_mem_xi_write.value[0] = value[0];
  element->kinds.fn_mem_xi_write.value[1] = value[1];
  element->kinds.fn_mem_xi_write.value[2] = value[2];
  element->kinds.fn_mem_xi_write.value[3] = value[3];
}

/* Execute a write stored on the write queue.  */
void
cgen_write_queue_element_execute (SIM_CPU *cpu, CGEN_WRITE_QUEUE_ELEMENT *item)
{
  IADDR pc;
  switch (CGEN_WRITE_QUEUE_ELEMENT_KIND (item))
    {
    case CGEN_BI_WRITE:
      *item->kinds.bi_write.target = item->kinds.bi_write.value;
      break;
    case CGEN_QI_WRITE:
      *item->kinds.qi_write.target = item->kinds.qi_write.value;
      break;
    case CGEN_SI_WRITE:
      *item->kinds.si_write.target = item->kinds.si_write.value;
      break;
    case CGEN_SF_WRITE:
      *item->kinds.sf_write.target = item->kinds.sf_write.value;
      break;
    case CGEN_PC_WRITE:
      CPU_PC_SET (cpu, item->kinds.pc_write.value);
      break;
    case CGEN_FN_HI_WRITE:
      item->kinds.fn_hi_write.function (cpu,
					item->kinds.fn_hi_write.regno,
					item->kinds.fn_hi_write.value);
      break;
    case CGEN_FN_SI_WRITE:
      item->kinds.fn_si_write.function (cpu,
					item->kinds.fn_si_write.regno,
					item->kinds.fn_si_write.value);
      break;
    case CGEN_FN_SF_WRITE:
      item->kinds.fn_sf_write.function (cpu,
					item->kinds.fn_sf_write.regno,
					item->kinds.fn_sf_write.value);
      break;
    case CGEN_FN_DI_WRITE:
      item->kinds.fn_di_write.function (cpu,
					item->kinds.fn_di_write.regno,
					item->kinds.fn_di_write.value);
      break;
    case CGEN_FN_DF_WRITE:
      item->kinds.fn_df_write.function (cpu,
					item->kinds.fn_df_write.regno,
					item->kinds.fn_df_write.value);
      break;
    case CGEN_FN_XI_WRITE:
      item->kinds.fn_xi_write.function (cpu,
					item->kinds.fn_xi_write.regno,
					item->kinds.fn_xi_write.value);
      break;
    case CGEN_FN_PC_WRITE:
      item->kinds.fn_pc_write.function (cpu, item->kinds.fn_pc_write.value);
      break;
    case CGEN_MEM_QI_WRITE:
      pc = item->insn_address;
      SETMEMQI (cpu, pc, item->kinds.mem_qi_write.address,
		item->kinds.mem_qi_write.value);
      break;
    case CGEN_MEM_HI_WRITE:
      pc = item->insn_address;
      SETMEMHI (cpu, pc, item->kinds.mem_hi_write.address,
		item->kinds.mem_hi_write.value);
      break;
    case CGEN_MEM_SI_WRITE:
      pc = item->insn_address;
      SETMEMSI (cpu, pc, item->kinds.mem_si_write.address,
		item->kinds.mem_si_write.value);
      break;
    case CGEN_MEM_DI_WRITE:
      pc = item->insn_address;
      SETMEMDI (cpu, pc, item->kinds.mem_di_write.address,
		item->kinds.mem_di_write.value);
      break;
    case CGEN_MEM_DF_WRITE:
      pc = item->insn_address;
      SETMEMDF (cpu, pc, item->kinds.mem_df_write.address,
		item->kinds.mem_df_write.value);
      break;
    case CGEN_MEM_XI_WRITE:
      pc = item->insn_address;
      SETMEMSI (cpu, pc, item->kinds.mem_xi_write.address,
		item->kinds.mem_xi_write.value[0]);
      SETMEMSI (cpu, pc, item->kinds.mem_xi_write.address + 4,
		item->kinds.mem_xi_write.value[1]);
      SETMEMSI (cpu, pc, item->kinds.mem_xi_write.address + 8,
		item->kinds.mem_xi_write.value[2]);
      SETMEMSI (cpu, pc, item->kinds.mem_xi_write.address + 12,
		item->kinds.mem_xi_write.value[3]);
      break;
    case CGEN_FN_MEM_QI_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_qi_write.function (cpu, pc,
					    item->kinds.fn_mem_qi_write.address,
					    item->kinds.fn_mem_qi_write.value);
      break;
    case CGEN_FN_MEM_HI_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_hi_write.function (cpu, pc,
					    item->kinds.fn_mem_hi_write.address,
					    item->kinds.fn_mem_hi_write.value);
      break;
    case CGEN_FN_MEM_SI_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_si_write.function (cpu, pc,
					    item->kinds.fn_mem_si_write.address,
					    item->kinds.fn_mem_si_write.value);
      break;
    case CGEN_FN_MEM_DI_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_di_write.function (cpu, pc,
					    item->kinds.fn_mem_di_write.address,
					    item->kinds.fn_mem_di_write.value);
      break;
    case CGEN_FN_MEM_DF_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_df_write.function (cpu, pc,
					    item->kinds.fn_mem_df_write.address,
					    item->kinds.fn_mem_df_write.value);
      break;
    case CGEN_FN_MEM_XI_WRITE:
      pc = item->insn_address;
      item->kinds.fn_mem_xi_write.function (cpu, pc,
					    item->kinds.fn_mem_xi_write.address,
					    item->kinds.fn_mem_xi_write.value);
      break;
    default:
      abort ();
      break; /* FIXME: for now....print message later.  */
    }
}

/* Utilities for the write queue.  */
CGEN_WRITE_QUEUE_ELEMENT *
cgen_write_queue_overflow (CGEN_WRITE_QUEUE *q)
{
  abort (); /* FIXME: for now....print message later.  */
  return 0;
}
