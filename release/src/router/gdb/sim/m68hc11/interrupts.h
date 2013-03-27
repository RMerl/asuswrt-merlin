/* interrupts.h -- 68HC11 Interrupts Emulation
   Copyright 1999, 2000, 2001, 2002, 2007 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@worldnet.fr)

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

#ifndef _M6811_SIM_INTERRUPTS_H
#define _M6811_SIM_INTERRUPTS_H

/* Definition of 68HC11 interrupts.  These enum are used as an index
   in the interrupt table.  */
enum M6811_INT
{
  M6811_INT_RESERVED1 = 0,
  M6811_INT_RESERVED2,
  M6811_INT_RESERVED3,
  M6811_INT_RESERVED4,
  M6811_INT_RESERVED5,
  M6811_INT_RESERVED6,
  M6811_INT_RESERVED7,
  M6811_INT_RESERVED8,

  M6811_INT_RESERVED9,
  M6811_INT_RESERVED10,
  M6811_INT_RESERVED11,

  M6811_INT_SCI,
  M6811_INT_SPI,
  M6811_INT_AINPUT,
  M6811_INT_AOVERFLOW,
  M6811_INT_TCTN,

  M6811_INT_OUTCMP5,
  M6811_INT_OUTCMP4,
  M6811_INT_OUTCMP3,
  M6811_INT_OUTCMP2,
  M6811_INT_OUTCMP1,

  M6811_INT_INCMP3,
  M6811_INT_INCMP2,
  M6811_INT_INCMP1,

  M6811_INT_RT,
  M6811_INT_IRQ,
  M6811_INT_XIRQ,
  M6811_INT_SWI,
  M6811_INT_ILLEGAL,

  M6811_INT_COPRESET,
  M6811_INT_COPFAIL,

  M6811_INT_RESET,
  M6811_INT_NUMBER
};


/* Structure to describe how to recognize an interrupt in the
   68hc11 IO regs.  */
struct interrupt_def
{
  enum M6811_INT   int_number;
  unsigned char    int_paddr;
  unsigned char    int_mask;
  unsigned char    enable_paddr;
  unsigned char    enabled_mask;
};

#define MAX_INT_HISTORY 64

/* Structure used to keep track of interrupt history.
   This is used to understand in which order interrupts were
   raised and when.  */
struct interrupt_history
{
  enum M6811_INT   type;

  /* CPU cycle when interrupt handler is called.  */
  signed64         taken_cycle;   

  /* CPU cycle when the interrupt is first raised by the device.  */
  signed64         raised_cycle;
};

#define SIM_STOP_WHEN_RAISED 1
#define SIM_STOP_WHEN_TAKEN  2

/* Information and control of pending interrupts.  */
struct interrupt
{
  /* CPU cycle when the interrupt is raised by the device.  */
  signed64         cpu_cycle;

  /* Number of times the interrupt was raised.  */
  unsigned long    raised_count;

  /* Controls whether we must stop the simulator.  */
  int              stop_mode;
};


/* Management of 68HC11 interrupts:
    - We use a table of 'interrupt_def' to describe the interrupts that must be
      raised depending on IO register flags (enable and present flags).
    - We keep a mask of pending interrupts.  This mask is refreshed by
      calling 'interrupts_update_pending'.  It must be refreshed each time
      an IO register is changed.
    - 'interrupts_process' must be called after each insn. It has two purposes:
      first it maintains a min/max count of CPU cycles between which interrupts
      are masked; second it checks for pending interrupts and raise one if
      interrupts are enabled.  */
struct interrupts {
  struct _sim_cpu   *cpu;

  /* Mask of current pending interrupts.  */
  unsigned long     pending_mask;

  /* Address of vector table.  This is set depending on the
     68hc11 init mode.  */
  uint16            vectors_addr;

  /* Priority order of interrupts.  This is controlled by setting the HPRIO
     IO register.  */
  enum M6811_INT    interrupt_order[M6811_INT_NUMBER];
  struct interrupt  interrupts[M6811_INT_NUMBER];

  /* Simulator statistics to report useful debug information to users.  */

  /* - Max/Min number of CPU cycles executed with interrupts masked.  */
  signed64          start_mask_cycle;
  signed64          min_mask_cycles;
  signed64          max_mask_cycles;
  signed64          last_mask_cycles;

  /* - Same for XIRQ.  */
  signed64          xirq_start_mask_cycle;
  signed64          xirq_min_mask_cycles;
  signed64          xirq_max_mask_cycles;
  signed64          xirq_last_mask_cycles;

  /* - Total number of interrupts raised.  */
  unsigned long     nb_interrupts_raised;

  /* Interrupt history to help understand which interrupts
     were raised recently and in which order.  */
  int               history_index;
  struct interrupt_history interrupts_history[MAX_INT_HISTORY];
};

extern void interrupts_initialize     (SIM_DESC sd, struct _sim_cpu* cpu);
extern void interrupts_reset          (struct interrupts* interrupts);
extern void interrupts_update_pending (struct interrupts* interrupts);
extern int  interrupts_get_current    (struct interrupts* interrupts);
extern int  interrupts_process        (struct interrupts* interrupts);
extern void interrupts_raise          (struct interrupts* interrupts,
                                       enum M6811_INT number);

extern void interrupts_info           (SIM_DESC sd,
                                       struct interrupts* interrupts);

#endif
