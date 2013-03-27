/* interrupts.c -- 68HC11 Interrupts Emulation
   Copyright 1999, 2000, 2001, 2002, 2003, 2007 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

This file is part of GDB, GAS, and the GNU binutils.

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
#include "sim-options.h"

static const char *interrupt_names[] = {
  "R1",
  "R2",
  "R3",
  "R4",
  "R5",
  "R6",
  "R7",
  "R8",
  "R9",
  "R10",
  "R11",

  "SCI",
  "SPI",
  "AINPUT",
  "AOVERFLOW",
  "TOVERFLOW",
  "OUT5",
  "OUT4",
  "OUT3",
  "OUT2",
  "OUT1",
  "INC3",
  "INC2",
  "INC1",
  "RT",
  "IRQ",
  "XIRQ",
  "SWI",
  "ILL",
  "COPRESET",
  "COPFAIL",
  "RESET"
};

struct interrupt_def idefs[] = {
  /* Serial interrupts.  */
  { M6811_INT_SCI,      M6811_SCSR,   M6811_TDRE,  M6811_SCCR2,  M6811_TIE },
  { M6811_INT_SCI,      M6811_SCSR,   M6811_TC,    M6811_SCCR2,  M6811_TCIE },
  { M6811_INT_SCI,      M6811_SCSR,   M6811_RDRF,  M6811_SCCR2,  M6811_RIE },
  { M6811_INT_SCI,      M6811_SCSR,   M6811_IDLE,  M6811_SCCR2,  M6811_ILIE },

  /* SPI interrupts.  */
  { M6811_INT_SPI,      M6811_SPSR,   M6811_SPIF,  M6811_SPCR,   M6811_SPIE },

  /* Realtime interrupts.  */
  { M6811_INT_TCTN,     M6811_TFLG2,  M6811_TOF,   M6811_TMSK2,  M6811_TOI },
  { M6811_INT_RT,       M6811_TFLG2,  M6811_RTIF,  M6811_TMSK2,  M6811_RTII },

  /* Output compare interrupts.  */
  { M6811_INT_OUTCMP1,  M6811_TFLG1,  M6811_OC1F,  M6811_TMSK1,  M6811_OC1I },
  { M6811_INT_OUTCMP2,  M6811_TFLG1,  M6811_OC2F,  M6811_TMSK1,  M6811_OC2I },
  { M6811_INT_OUTCMP3,  M6811_TFLG1,  M6811_OC3F,  M6811_TMSK1,  M6811_OC3I },
  { M6811_INT_OUTCMP4,  M6811_TFLG1,  M6811_OC4F,  M6811_TMSK1,  M6811_OC4I },
  { M6811_INT_OUTCMP5,  M6811_TFLG1,  M6811_OC5F,  M6811_TMSK1,  M6811_OC5I },

  /* Input compare interrupts.  */
  { M6811_INT_INCMP1,   M6811_TFLG1,  M6811_IC1F,  M6811_TMSK1,  M6811_IC1I },
  { M6811_INT_INCMP2,   M6811_TFLG1,  M6811_IC2F,  M6811_TMSK1,  M6811_IC2I },
  { M6811_INT_INCMP3,   M6811_TFLG1,  M6811_IC3F,  M6811_TMSK1,  M6811_IC3I },

  /* Pulse accumulator.  */
  { M6811_INT_AINPUT,   M6811_TFLG2,  M6811_PAIF,  M6811_TMSK2,  M6811_PAII },
  { M6811_INT_AOVERFLOW,M6811_TFLG2,  M6811_PAOVF, M6811_TMSK2,  M6811_PAOVI},
#if 0
  { M6811_INT_COPRESET, M6811_CONFIG, M6811_NOCOP, 0,            0 },
  { M6811_INT_COPFAIL,  M6811_CONFIG, M6811_NOCOP, 0,            0 }
#endif
};

#define TableSize(X) (sizeof X / sizeof(X[0]))
#define CYCLES_MAX ((((signed64) 1) << 62) - 1)

enum
{
  OPTION_INTERRUPT_INFO = OPTION_START,
  OPTION_INTERRUPT_CATCH,
  OPTION_INTERRUPT_CLEAR
};

static DECLARE_OPTION_HANDLER (interrupt_option_handler);

static const OPTION interrupt_options[] =
{
  { {"interrupt-info", no_argument, NULL, OPTION_INTERRUPT_INFO },
      '\0', NULL, "Print information about interrupts",
      interrupt_option_handler },
  { {"interrupt-catch", required_argument, NULL, OPTION_INTERRUPT_CATCH },
      '\0', "NAME[,MODE]",
    "Catch interrupts when they are raised or taken\n"
    "NAME   Name of the interrupt\n"
    "MODE   Optional mode (`taken' or `raised')",
      interrupt_option_handler },
  { {"interrupt-clear", required_argument, NULL, OPTION_INTERRUPT_CLEAR },
      '\0', "NAME", "No longer catch the interrupt",
      interrupt_option_handler },
  
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

/* Initialize the interrupts module.  */
void
interrupts_initialize (SIM_DESC sd, struct _sim_cpu *proc)
{
  struct interrupts *interrupts = &proc->cpu_interrupts;
  
  interrupts->cpu          = proc;

  sim_add_option_table (sd, 0, interrupt_options);
}

/* Initialize the interrupts of the processor.  */
void
interrupts_reset (struct interrupts *interrupts)
{
  int i;
  
  interrupts->pending_mask = 0;
  if (interrupts->cpu->cpu_mode & M6811_SMOD)
    interrupts->vectors_addr = 0xbfc0;
  else
    interrupts->vectors_addr = 0xffc0;
  interrupts->nb_interrupts_raised = 0;
  interrupts->min_mask_cycles = CYCLES_MAX;
  interrupts->max_mask_cycles = 0;
  interrupts->last_mask_cycles = 0;
  interrupts->start_mask_cycle = -1;
  interrupts->xirq_start_mask_cycle = -1;
  interrupts->xirq_max_mask_cycles = 0;
  interrupts->xirq_min_mask_cycles = CYCLES_MAX;
  interrupts->xirq_last_mask_cycles = 0;
  
  for (i = 0; i < M6811_INT_NUMBER; i++)
    {
      interrupts->interrupt_order[i] = i;
    }

  /* Clear the interrupt history table.  */
  interrupts->history_index = 0;
  memset (interrupts->interrupts_history, 0,
          sizeof (interrupts->interrupts_history));

  memset (interrupts->interrupts, 0,
          sizeof (interrupts->interrupts));

  /* In bootstrap mode, initialize the vector table to point
     to the RAM location.  */
  if (interrupts->cpu->cpu_mode == M6811_SMOD)
    {
      bfd_vma addr = interrupts->vectors_addr;
      uint16 vector = 0x0100 - 3 * (M6811_INT_NUMBER - 1);
      for (i = 0; i < M6811_INT_NUMBER; i++)
        {
          memory_write16 (interrupts->cpu, addr, vector);
          addr += 2;
          vector += 3;
        }
    }
}

static int
find_interrupt (const char *name)
{
  int i;

  if (name)
    for (i = 0; i < M6811_INT_NUMBER; i++)
      if (strcasecmp (name, interrupt_names[i]) == 0)
        return i;

  return -1;
}

static SIM_RC
interrupt_option_handler (SIM_DESC sd, sim_cpu *cpu,
                          int opt, char *arg, int is_command)
{
  char *p;
  int mode;
  int id;
  struct interrupts *interrupts;

  if (cpu == 0)
    cpu = STATE_CPU (sd, 0);

  interrupts = &cpu->cpu_interrupts;
  switch (opt)
    {
    case OPTION_INTERRUPT_INFO:
      for (id = 0; id < M6811_INT_NUMBER; id++)
        {
          sim_io_eprintf (sd, "%-10.10s ", interrupt_names[id]);
          switch (interrupts->interrupts[id].stop_mode)
            {
            case SIM_STOP_WHEN_RAISED:
              sim_io_eprintf (sd, "catch raised ");
              break;

            case SIM_STOP_WHEN_TAKEN:
              sim_io_eprintf (sd, "catch taken  ");
              break;

            case SIM_STOP_WHEN_RAISED | SIM_STOP_WHEN_TAKEN:
              sim_io_eprintf (sd, "catch all    ");
              break;

            default:
              sim_io_eprintf (sd, "             ");
              break;
            }
          sim_io_eprintf (sd, "%ld\n",
                          interrupts->interrupts[id].raised_count);
        }
      break;

    case OPTION_INTERRUPT_CATCH:
      p = strchr (arg, ',');
      if (p)
        *p++ = 0;

      mode = SIM_STOP_WHEN_RAISED;
      id = find_interrupt (arg);
      if (id < 0)
        sim_io_eprintf (sd, "Interrupt name not recognized: %s\n", arg);

      if (p && strcasecmp (p, "raised") == 0)
        mode = SIM_STOP_WHEN_RAISED;
      else if (p && strcasecmp (p, "taken") == 0)
        mode = SIM_STOP_WHEN_TAKEN;
      else if (p && strcasecmp (p, "all") == 0)
        mode = SIM_STOP_WHEN_RAISED | SIM_STOP_WHEN_TAKEN;
      else if (p)
        {
          sim_io_eprintf (sd, "Invalid argument: %s\n", p);
          break;
        }
      if (id >= 0)
        interrupts->interrupts[id].stop_mode = mode;
      break;

    case OPTION_INTERRUPT_CLEAR:
      mode = SIM_STOP_WHEN_RAISED;
      id = find_interrupt (arg);
      if (id < 0)
        sim_io_eprintf (sd, "Interrupt name not recognized: %s\n", arg);
      else
        interrupts->interrupts[id].stop_mode = 0;      
      break;      
    }

  return SIM_RC_OK;
}

/* Update the mask of pending interrupts.  This operation must be called
   when the state of some 68HC11 IO register changes.  It looks the
   different registers that indicate a pending interrupt (timer, SCI, SPI,
   ...) and records the interrupt if it's there and enabled.  */
void
interrupts_update_pending (struct interrupts *interrupts)
{
  int i;
  uint8 *ioregs;
  unsigned long clear_mask;
  unsigned long set_mask;

  clear_mask = 0;
  set_mask = 0;
  ioregs = &interrupts->cpu->ios[0];
  
  for (i = 0; i < TableSize(idefs); i++)
    {
      struct interrupt_def *idef = &idefs[i];
      uint8 data;
      
      /* Look if the interrupt is enabled.  */
      if (idef->enable_paddr)
	{
	  data = ioregs[idef->enable_paddr];
	  if (!(data & idef->enabled_mask))
            {
              /* Disable it.  */
              clear_mask |= (1 << idef->int_number);
              continue;
            }
	}

      /* Interrupt is enabled, see if it's there.  */
      data = ioregs[idef->int_paddr];
      if (!(data & idef->int_mask))
        {
          /* Disable it.  */
          clear_mask |= (1 << idef->int_number);
          continue;
        }

      /* Ok, raise it.  */
      set_mask |= (1 << idef->int_number);
    }

  /* Some interrupts are shared (M6811_INT_SCI) so clear
     the interrupts before setting the new ones.  */
  interrupts->pending_mask &= ~clear_mask;
  interrupts->pending_mask |= set_mask;

  /* Keep track of when the interrupt is raised by the device.
     Also implements the breakpoint-on-interrupt.  */
  if (set_mask)
    {
      signed64 cycle = cpu_current_cycle (interrupts->cpu);
      int must_stop = 0;
      
      for (i = 0; i < M6811_INT_NUMBER; i++)
        {
          if (!(set_mask & (1 << i)))
            continue;

          interrupts->interrupts[i].cpu_cycle = cycle;
          if (interrupts->interrupts[i].stop_mode & SIM_STOP_WHEN_RAISED)
            {
              must_stop = 1;
              sim_io_printf (CPU_STATE (interrupts->cpu),
                             "Interrupt %s raised\n",
                             interrupt_names[i]);
            }
        }
      if (must_stop)
        sim_engine_halt (CPU_STATE (interrupts->cpu),
                         interrupts->cpu,
                         0, cpu_get_pc (interrupts->cpu),
                         sim_stopped,
                         SIM_SIGTRAP);
    }
}


/* Finds the current active and non-masked interrupt.
   Returns the interrupt number (index in the vector table) or -1
   if no interrupt can be serviced.  */
int
interrupts_get_current (struct interrupts *interrupts)
{
  int i;
  
  if (interrupts->pending_mask == 0)
    return -1;

  /* SWI and illegal instructions are simulated by an interrupt.
     They are not maskable.  */
  if (interrupts->pending_mask & (1 << M6811_INT_SWI))
    {
      interrupts->pending_mask &= ~(1 << M6811_INT_SWI);
      return M6811_INT_SWI;
    }
  if (interrupts->pending_mask & (1 << M6811_INT_ILLEGAL))
    {
      interrupts->pending_mask &= ~(1 << M6811_INT_ILLEGAL);
      return M6811_INT_ILLEGAL;
    }
  
  /* If there is a non maskable interrupt, go for it (unless we are masked
     by the X-bit.  */
  if (interrupts->pending_mask & (1 << M6811_INT_XIRQ))
    {
      if (cpu_get_ccr_X (interrupts->cpu) == 0)
	{
	  interrupts->pending_mask &= ~(1 << M6811_INT_XIRQ);
	  return M6811_INT_XIRQ;
	}
      return -1;
    }

  /* Interrupts are masked, do nothing.  */
  if (cpu_get_ccr_I (interrupts->cpu) == 1)
    {
      return -1;
    }

  /* Returns the first interrupt number which is pending.
     The interrupt priority is specified by the table `interrupt_order'.
     For these interrupts, the pending mask is cleared when the program
     performs some actions on the corresponding device.  If the device
     is not reset, the interrupt remains and will be re-raised when
     we return from the interrupt (see 68HC11 pink book).  */
  for (i = 0; i < M6811_INT_NUMBER; i++)
    {
      enum M6811_INT int_number = interrupts->interrupt_order[i];

      if (interrupts->pending_mask & (1 << int_number))
	{
	  return int_number;
	}
    }
  return -1;
}


/* Process the current interrupt if there is one.  This operation must
   be called after each instruction to handle the interrupts.  If interrupts
   are masked, it does nothing.  */
int
interrupts_process (struct interrupts *interrupts)
{
  int id;
  uint8 ccr;

  /* See if interrupts are enabled/disabled and keep track of the
     number of cycles the interrupts are masked.  Such information is
     then reported by the info command.  */
  ccr = cpu_get_ccr (interrupts->cpu);
  if (ccr & M6811_I_BIT)
    {
      if (interrupts->start_mask_cycle < 0)
        interrupts->start_mask_cycle = cpu_current_cycle (interrupts->cpu);
    }
  else if (interrupts->start_mask_cycle >= 0
           && (ccr & M6811_I_BIT) == 0)
    {
      signed64 t = cpu_current_cycle (interrupts->cpu);

      t -= interrupts->start_mask_cycle;
      if (t < interrupts->min_mask_cycles)
        interrupts->min_mask_cycles = t;
      if (t > interrupts->max_mask_cycles)
        interrupts->max_mask_cycles = t;
      interrupts->start_mask_cycle = -1;
      interrupts->last_mask_cycles = t;
    }
  if (ccr & M6811_X_BIT)
    {
      if (interrupts->xirq_start_mask_cycle < 0)
        interrupts->xirq_start_mask_cycle
	  = cpu_current_cycle (interrupts->cpu);
    }
  else if (interrupts->xirq_start_mask_cycle >= 0
           && (ccr & M6811_X_BIT) == 0)
    {
      signed64 t = cpu_current_cycle (interrupts->cpu);

      t -= interrupts->xirq_start_mask_cycle;
      if (t < interrupts->xirq_min_mask_cycles)
        interrupts->xirq_min_mask_cycles = t;
      if (t > interrupts->xirq_max_mask_cycles)
        interrupts->xirq_max_mask_cycles = t;
      interrupts->xirq_start_mask_cycle = -1;
      interrupts->xirq_last_mask_cycles = t;
    }

  id = interrupts_get_current (interrupts);
  if (id >= 0)
    {
      uint16 addr;
      struct interrupt_history *h;

      /* Implement the breakpoint-on-interrupt.  */
      if (interrupts->interrupts[id].stop_mode & SIM_STOP_WHEN_TAKEN)
        {
          sim_io_printf (CPU_STATE (interrupts->cpu),
                         "Interrupt %s will be handled\n",
                         interrupt_names[id]);
          sim_engine_halt (CPU_STATE (interrupts->cpu),
                           interrupts->cpu,
                           0, cpu_get_pc (interrupts->cpu),
                           sim_stopped,
                           SIM_SIGTRAP);
        }

      cpu_push_all (interrupts->cpu);
      addr = memory_read16 (interrupts->cpu,
                            interrupts->vectors_addr + id * 2);
      cpu_call (interrupts->cpu, addr);

      /* Now, protect from nested interrupts.  */
      if (id == M6811_INT_XIRQ)
	{
	  cpu_set_ccr_X (interrupts->cpu, 1);
	}
      else
	{
	  cpu_set_ccr_I (interrupts->cpu, 1);
	}

      /* Update the interrupt history table.  */
      h = &interrupts->interrupts_history[interrupts->history_index];
      h->type = id;
      h->taken_cycle = cpu_current_cycle (interrupts->cpu);
      h->raised_cycle = interrupts->interrupts[id].cpu_cycle;
      
      if (interrupts->history_index >= MAX_INT_HISTORY-1)
        interrupts->history_index = 0;
      else
        interrupts->history_index++;

      interrupts->nb_interrupts_raised++;
      cpu_add_cycles (interrupts->cpu, 14);
      return 1;
    }
  return 0;
}

void
interrupts_raise (struct interrupts *interrupts, enum M6811_INT number)
{
  interrupts->pending_mask |= (1 << number);
  interrupts->nb_interrupts_raised ++;
}

void
interrupts_info (SIM_DESC sd, struct interrupts *interrupts)
{
  signed64 t, prev_interrupt;
  int i;
  
  sim_io_printf (sd, "Interrupts Info:\n");
  sim_io_printf (sd, "  Interrupts raised: %lu\n",
                 interrupts->nb_interrupts_raised);

  if (interrupts->start_mask_cycle >= 0)
    {
      t = cpu_current_cycle (interrupts->cpu);

      t -= interrupts->start_mask_cycle;
      if (t > interrupts->max_mask_cycles)
        interrupts->max_mask_cycles = t;

      sim_io_printf (sd, "  Current interrupts masked sequence:   %s\n",
                     cycle_to_string (interrupts->cpu, t,
                                      PRINT_TIME | PRINT_CYCLE));
    }
  t = interrupts->min_mask_cycles == CYCLES_MAX ?
    interrupts->max_mask_cycles :
    interrupts->min_mask_cycles;
  sim_io_printf (sd, "  Shortest interrupts masked sequence:  %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));

  t = interrupts->max_mask_cycles;
  sim_io_printf (sd, "  Longest interrupts masked sequence:   %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));

  t = interrupts->last_mask_cycles;
  sim_io_printf (sd, "  Last interrupts masked sequence:      %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));
  
  if (interrupts->xirq_start_mask_cycle >= 0)
    {
      t = cpu_current_cycle (interrupts->cpu);

      t -= interrupts->xirq_start_mask_cycle;
      if (t > interrupts->xirq_max_mask_cycles)
        interrupts->xirq_max_mask_cycles = t;

      sim_io_printf (sd, "  XIRQ Current interrupts masked sequence: %s\n",
                     cycle_to_string (interrupts->cpu, t,
                                      PRINT_TIME | PRINT_CYCLE));
    }

  t = interrupts->xirq_min_mask_cycles == CYCLES_MAX ?
    interrupts->xirq_max_mask_cycles :
    interrupts->xirq_min_mask_cycles;
  sim_io_printf (sd, "  XIRQ Min interrupts masked sequence:  %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));

  t = interrupts->xirq_max_mask_cycles;
  sim_io_printf (sd, "  XIRQ Max interrupts masked sequence:  %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));

  t = interrupts->xirq_last_mask_cycles;
  sim_io_printf (sd, "  XIRQ Last interrupts masked sequence: %s\n",
                 cycle_to_string (interrupts->cpu, t,
                                  PRINT_TIME | PRINT_CYCLE));

  if (interrupts->pending_mask)
    {
      sim_io_printf (sd, "  Pending interrupts : ");
      for (i = 0; i < M6811_INT_NUMBER; i++)
        {
          enum M6811_INT int_number = interrupts->interrupt_order[i];
          
          if (interrupts->pending_mask & (1 << int_number))
            {
              sim_io_printf (sd, "%s ", interrupt_names[int_number]);
            }
        }
      sim_io_printf (sd, "\n");
    }

  prev_interrupt = 0;
  sim_io_printf (sd, "N  Interrupt     Cycle Taken         Latency"
                 "   Delta between interrupts\n");
  for (i = 0; i < MAX_INT_HISTORY; i++)
    {
      int which;
      struct interrupt_history *h;
      signed64 dt;

      which = interrupts->history_index - i - 1;
      if (which < 0)
        which += MAX_INT_HISTORY;
      h = &interrupts->interrupts_history[which];
      if (h->taken_cycle == 0)
        break;

      dt = h->taken_cycle - h->raised_cycle;
      sim_io_printf (sd, "%2d %-9.9s %15.15s ", i,
                     interrupt_names[h->type],
                     cycle_to_string (interrupts->cpu, h->taken_cycle, 0));
      sim_io_printf (sd, "%15.15s",
                     cycle_to_string (interrupts->cpu, dt, 0));
      if (prev_interrupt)
        {
          dt = prev_interrupt - h->taken_cycle;
          sim_io_printf (sd, " %s",
                         cycle_to_string (interrupts->cpu, dt, PRINT_TIME));
        }
      sim_io_printf (sd, "\n");
      prev_interrupt = h->taken_cycle;
    }
}
