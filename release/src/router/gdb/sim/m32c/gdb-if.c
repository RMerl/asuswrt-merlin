/* gdb.c --- sim interface to GDB.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

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
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "ansidecl.h"
#include "gdb/callback.h"
#include "gdb/remote-sim.h"
#include "gdb/signals.h"
#include "gdb/sim-m32c.h"

#include "cpu.h"
#include "mem.h"
#include "load.h"
#include "syscalls.h"

/* I don't want to wrap up all the minisim's data structures in an
   object and pass that around.  That'd be a big change, and neither
   GDB nor run needs that ability.

   So we just have one instance, that lives in global variables, and
   each time we open it, we re-initialize it.  */
struct sim_state
{
  const char *message;
};

static struct sim_state the_minisim = {
  "This is the sole m32c minisim instance.  See libsim.a's global variables."
};

static int open;

SIM_DESC
sim_open (SIM_OPEN_KIND kind,
	  struct host_callback_struct *callback,
	  struct bfd *abfd, char **argv)
{
  if (open)
    fprintf (stderr, "m32c minisim: re-opened sim\n");

  /* The 'run' interface doesn't use this function, so we don't care
     about KIND; it's always SIM_OPEN_DEBUG.  */
  if (kind != SIM_OPEN_DEBUG)
    fprintf (stderr, "m32c minisim: sim_open KIND != SIM_OPEN_DEBUG: %d\n",
	     kind);

  if (abfd)
    m32c_set_mach (bfd_get_mach (abfd));

  /* We can use ABFD, if non-NULL to select the appropriate
     architecture.  But we only support the r8c right now.  */

  set_callbacks (callback);

  /* We don't expect any command-line arguments.  */

  init_mem ();
  init_regs ();

  open = 1;
  return &the_minisim;
}

static void
check_desc (SIM_DESC sd)
{
  if (sd != &the_minisim)
    fprintf (stderr, "m32c minisim: desc != &the_minisim\n");
}

void
sim_close (SIM_DESC sd, int quitting)
{
  check_desc (sd);

  /* Not much to do.  At least free up our memory.  */
  init_mem ();

  open = 0;
}

static bfd *
open_objfile (const char *filename)
{
  bfd *prog = bfd_openr (filename, 0);

  if (!prog)
    {
      fprintf (stderr, "Can't read %s\n", filename);
      return 0;
    }

  if (!bfd_check_format (prog, bfd_object))
    {
      fprintf (stderr, "%s not a m32c program\n", filename);
      return 0;
    }

  return prog;
}


SIM_RC
sim_load (SIM_DESC sd, char *prog, struct bfd *abfd, int from_tty)
{
  check_desc (sd);

  if (!abfd)
    abfd = open_objfile (prog);
  if (!abfd)
    return SIM_RC_FAIL;

  m32c_load (abfd);

  return SIM_RC_OK;
}

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *abfd, char **argv, char **env)
{
  check_desc (sd);

  if (abfd)
    m32c_load (abfd);

  return SIM_RC_OK;
}

int
sim_read (SIM_DESC sd, SIM_ADDR mem, unsigned char *buf, int length)
{
  check_desc (sd);

  if (mem == 0)
    return 0;

  mem_get_blk ((int) mem, buf, length);

  return length;
}

int
sim_write (SIM_DESC sd, SIM_ADDR mem, unsigned char *buf, int length)
{
  check_desc (sd);

  mem_put_blk ((int) mem, buf, length);

  return length;
}


/* Read the LENGTH bytes at BUF as an little-endian value.  */
static DI
get_le (unsigned char *buf, int length)
{
  DI acc = 0;
  while (--length >= 0)
    acc = (acc << 8) + buf[length];

  return acc;
}

/* Store VAL as a little-endian value in the LENGTH bytes at BUF.  */
static void
put_le (unsigned char *buf, int length, DI val)
{
  int i;

  for (i = 0; i < length; i++)
    {
      buf[i] = val & 0xff;
      val >>= 8;
    }
}

static int
check_regno (enum m32c_sim_reg regno)
{
  return 0 <= regno && regno < m32c_sim_reg_num_regs;
}

static size_t
mask_size (int addr_mask)
{
  switch (addr_mask)
    {
    case 0xffff:
      return 2;
    case 0xfffff:
    case 0xffffff:
      return 3;
    default:
      fprintf (stderr,
	       "m32c minisim: addr_mask_size: unexpected mask 0x%x\n",
	       addr_mask);
      return sizeof (addr_mask);
    }
}

static size_t
reg_size (enum m32c_sim_reg regno)
{
  switch (regno)
    {
    case m32c_sim_reg_r0_bank0:
    case m32c_sim_reg_r1_bank0:
    case m32c_sim_reg_r2_bank0:
    case m32c_sim_reg_r3_bank0:
    case m32c_sim_reg_r0_bank1:
    case m32c_sim_reg_r1_bank1:
    case m32c_sim_reg_r2_bank1:
    case m32c_sim_reg_r3_bank1:
    case m32c_sim_reg_flg:
    case m32c_sim_reg_svf:
      return 2;

    case m32c_sim_reg_a0_bank0:
    case m32c_sim_reg_a1_bank0:
    case m32c_sim_reg_fb_bank0:
    case m32c_sim_reg_sb_bank0:
    case m32c_sim_reg_a0_bank1:
    case m32c_sim_reg_a1_bank1:
    case m32c_sim_reg_fb_bank1:
    case m32c_sim_reg_sb_bank1:
    case m32c_sim_reg_usp:
    case m32c_sim_reg_isp:
      return mask_size (addr_mask);

    case m32c_sim_reg_pc:
    case m32c_sim_reg_intb:
    case m32c_sim_reg_svp:
    case m32c_sim_reg_vct:
      return mask_size (membus_mask);

    case m32c_sim_reg_dmd0:
    case m32c_sim_reg_dmd1:
      return 1;

    case m32c_sim_reg_dct0:
    case m32c_sim_reg_dct1:
    case m32c_sim_reg_drc0:
    case m32c_sim_reg_drc1:
      return 2;

    case m32c_sim_reg_dma0:
    case m32c_sim_reg_dma1:
    case m32c_sim_reg_dsa0:
    case m32c_sim_reg_dsa1:
    case m32c_sim_reg_dra0:
    case m32c_sim_reg_dra1:
      return 3;

    default:
      fprintf (stderr, "m32c minisim: unrecognized register number: %d\n",
	       regno);
      return -1;
    }
}

int
sim_fetch_register (SIM_DESC sd, int regno, unsigned char *buf, int length)
{
  size_t size;

  check_desc (sd);

  if (!check_regno (regno))
    return 0;

  size = reg_size (regno);
  if (length == size)
    {
      DI val;

      switch (regno)
	{
	case m32c_sim_reg_r0_bank0:
	  val = regs.r[0].r_r0;
	  break;
	case m32c_sim_reg_r1_bank0:
	  val = regs.r[0].r_r1;
	  break;
	case m32c_sim_reg_r2_bank0:
	  val = regs.r[0].r_r2;
	  break;
	case m32c_sim_reg_r3_bank0:
	  val = regs.r[0].r_r3;
	  break;
	case m32c_sim_reg_a0_bank0:
	  val = regs.r[0].r_a0;
	  break;
	case m32c_sim_reg_a1_bank0:
	  val = regs.r[0].r_a1;
	  break;
	case m32c_sim_reg_fb_bank0:
	  val = regs.r[0].r_fb;
	  break;
	case m32c_sim_reg_sb_bank0:
	  val = regs.r[0].r_sb;
	  break;
	case m32c_sim_reg_r0_bank1:
	  val = regs.r[1].r_r0;
	  break;
	case m32c_sim_reg_r1_bank1:
	  val = regs.r[1].r_r1;
	  break;
	case m32c_sim_reg_r2_bank1:
	  val = regs.r[1].r_r2;
	  break;
	case m32c_sim_reg_r3_bank1:
	  val = regs.r[1].r_r3;
	  break;
	case m32c_sim_reg_a0_bank1:
	  val = regs.r[1].r_a0;
	  break;
	case m32c_sim_reg_a1_bank1:
	  val = regs.r[1].r_a1;
	  break;
	case m32c_sim_reg_fb_bank1:
	  val = regs.r[1].r_fb;
	  break;
	case m32c_sim_reg_sb_bank1:
	  val = regs.r[1].r_sb;
	  break;

	case m32c_sim_reg_usp:
	  val = regs.r_usp;
	  break;
	case m32c_sim_reg_isp:
	  val = regs.r_isp;
	  break;
	case m32c_sim_reg_pc:
	  val = regs.r_pc;
	  break;
	case m32c_sim_reg_intb:
	  val = regs.r_intbl * 65536 + regs.r_intbl;
	  break;
	case m32c_sim_reg_flg:
	  val = regs.r_flags;
	  break;

	  /* These registers aren't implemented by the minisim.  */
	case m32c_sim_reg_svf:
	case m32c_sim_reg_svp:
	case m32c_sim_reg_vct:
	case m32c_sim_reg_dmd0:
	case m32c_sim_reg_dmd1:
	case m32c_sim_reg_dct0:
	case m32c_sim_reg_dct1:
	case m32c_sim_reg_drc0:
	case m32c_sim_reg_drc1:
	case m32c_sim_reg_dma0:
	case m32c_sim_reg_dma1:
	case m32c_sim_reg_dsa0:
	case m32c_sim_reg_dsa1:
	case m32c_sim_reg_dra0:
	case m32c_sim_reg_dra1:
	  return 0;

	default:
	  fprintf (stderr, "m32c minisim: unrecognized register number: %d\n",
		   regno);
	  return -1;
	}

      put_le (buf, length, val);
    }

  return size;
}

int
sim_store_register (SIM_DESC sd, int regno, unsigned char *buf, int length)
{
  size_t size;

  check_desc (sd);

  if (!check_regno (regno))
    return 0;

  size = reg_size (regno);

  if (length == size)
    {
      DI val = get_le (buf, length);

      switch (regno)
	{
	case m32c_sim_reg_r0_bank0:
	  regs.r[0].r_r0 = val & 0xffff;
	  break;
	case m32c_sim_reg_r1_bank0:
	  regs.r[0].r_r1 = val & 0xffff;
	  break;
	case m32c_sim_reg_r2_bank0:
	  regs.r[0].r_r2 = val & 0xffff;
	  break;
	case m32c_sim_reg_r3_bank0:
	  regs.r[0].r_r3 = val & 0xffff;
	  break;
	case m32c_sim_reg_a0_bank0:
	  regs.r[0].r_a0 = val & addr_mask;
	  break;
	case m32c_sim_reg_a1_bank0:
	  regs.r[0].r_a1 = val & addr_mask;
	  break;
	case m32c_sim_reg_fb_bank0:
	  regs.r[0].r_fb = val & addr_mask;
	  break;
	case m32c_sim_reg_sb_bank0:
	  regs.r[0].r_sb = val & addr_mask;
	  break;
	case m32c_sim_reg_r0_bank1:
	  regs.r[1].r_r0 = val & 0xffff;
	  break;
	case m32c_sim_reg_r1_bank1:
	  regs.r[1].r_r1 = val & 0xffff;
	  break;
	case m32c_sim_reg_r2_bank1:
	  regs.r[1].r_r2 = val & 0xffff;
	  break;
	case m32c_sim_reg_r3_bank1:
	  regs.r[1].r_r3 = val & 0xffff;
	  break;
	case m32c_sim_reg_a0_bank1:
	  regs.r[1].r_a0 = val & addr_mask;
	  break;
	case m32c_sim_reg_a1_bank1:
	  regs.r[1].r_a1 = val & addr_mask;
	  break;
	case m32c_sim_reg_fb_bank1:
	  regs.r[1].r_fb = val & addr_mask;
	  break;
	case m32c_sim_reg_sb_bank1:
	  regs.r[1].r_sb = val & addr_mask;
	  break;

	case m32c_sim_reg_usp:
	  regs.r_usp = val & addr_mask;
	  break;
	case m32c_sim_reg_isp:
	  regs.r_isp = val & addr_mask;
	  break;
	case m32c_sim_reg_pc:
	  regs.r_pc = val & membus_mask;
	  break;
	case m32c_sim_reg_intb:
	  regs.r_intbl = (val & membus_mask) & 0xffff;
	  regs.r_intbh = (val & membus_mask) >> 16;
	  break;
	case m32c_sim_reg_flg:
	  regs.r_flags = val & 0xffff;
	  break;

	  /* These registers aren't implemented by the minisim.  */
	case m32c_sim_reg_svf:
	case m32c_sim_reg_svp:
	case m32c_sim_reg_vct:
	case m32c_sim_reg_dmd0:
	case m32c_sim_reg_dmd1:
	case m32c_sim_reg_dct0:
	case m32c_sim_reg_dct1:
	case m32c_sim_reg_drc0:
	case m32c_sim_reg_drc1:
	case m32c_sim_reg_dma0:
	case m32c_sim_reg_dma1:
	case m32c_sim_reg_dsa0:
	case m32c_sim_reg_dsa1:
	case m32c_sim_reg_dra0:
	case m32c_sim_reg_dra1:
	  return 0;

	default:
	  fprintf (stderr, "m32c minisim: unrecognized register number: %d\n",
		   regno);
	  return -1;
	}
    }

  return size;
}

void
sim_info (SIM_DESC sd, int verbose)
{
  check_desc (sd);

  printf ("The m32c minisim doesn't collect any statistics.\n");
}

static volatile int stop;
static enum sim_stop reason;
int siggnal;


/* Given a signal number used by the M32C bsp (that is, newlib),
   return a host signal number.  (Oddly, the gdb/sim interface uses
   host signal numbers...)  */
int
m32c_signal_to_host (int m32c)
{
  switch (m32c)
    {
    case 4:
#ifdef SIGILL
      return SIGILL;
#else
      return SIGSEGV;
#endif

    case 5:
      return SIGTRAP;

    case 10:
#ifdef SIGBUS
      return SIGBUS;
#else
      return SIGSEGV;
#endif

    case 11:
      return SIGSEGV;

    case 24:
#ifdef SIGXCPU
      return SIGXCPU;
#else
      break;
#endif

    case 2:
      return SIGINT;

    case 8:
#ifdef SIGFPE
      return SIGFPE;
#else
      break;
#endif

    case 6:
      return SIGABRT;
    }

  return 0;
}


/* Take a step return code RC and set up the variables consulted by
   sim_stop_reason appropriately.  */
void
handle_step (int rc)
{
  if (M32C_STEPPED (rc) || M32C_HIT_BREAK (rc))
    {
      reason = sim_stopped;
      siggnal = TARGET_SIGNAL_TRAP;
    }
  else if (M32C_STOPPED (rc))
    {
      reason = sim_stopped;
      siggnal = m32c_signal_to_host (M32C_STOP_SIG (rc));
    }
  else
    {
      assert (M32C_EXITED (rc));
      reason = sim_exited;
      siggnal = M32C_EXIT_STATUS (rc);
    }
}


void
sim_resume (SIM_DESC sd, int step, int sig_to_deliver)
{
  check_desc (sd);

  if (sig_to_deliver != 0)
    {
      fprintf (stderr,
	       "Warning: the m32c minisim does not implement "
	       "signal delivery yet.\n" "Resuming with no signal.\n");
    }

  if (step)
    handle_step (decode_opcode ());
  else
    {
      /* We don't clear 'stop' here, because then we would miss
         interrupts that arrived on the way here.  Instead, we clear
         the flag in sim_stop_reason, after GDB has disabled the
         interrupt signal handler.  */
      for (;;)
	{
	  if (stop)
	    {
	      stop = 0;
	      reason = sim_stopped;
	      siggnal = TARGET_SIGNAL_INT;
	      break;
	    }

	  int rc = decode_opcode ();

	  if (!M32C_STEPPED (rc))
	    {
	      handle_step (rc);
	      break;
	    }
	}
    }
}

int
sim_stop (SIM_DESC sd)
{
  stop = 1;

  return 1;
}

void
sim_stop_reason (SIM_DESC sd, enum sim_stop *reason_p, int *sigrc_p)
{
  check_desc (sd);

  *reason_p = reason;
  *sigrc_p = siggnal;
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  check_desc (sd);

  char *p = cmd;

  /* Skip leading whitespace.  */
  while (isspace (*p))
    p++;

  /* Find the extent of the command word.  */
  for (p = cmd; *p; p++)
    if (isspace (*p))
      break;

  /* Null-terminate the command word, and record the start of any
     further arguments.  */
  char *args;
  if (*p)
    {
      *p = '\0';
      args = p + 1;
      while (isspace (*args))
	args++;
    }
  else
    args = p;

  if (strcmp (cmd, "trace") == 0)
    {
      if (strcmp (args, "on") == 0)
	trace = 1;
      else if (strcmp (args, "off") == 0)
	trace = 0;
      else
	printf ("The 'sim trace' command expects 'on' or 'off' "
		"as an argument.\n");
    }
  else if (strcmp (cmd, "verbose") == 0)
    {
      if (strcmp (args, "on") == 0)
	verbose = 1;
      else if (strcmp (args, "off") == 0)
	verbose = 0;
      else
	printf ("The 'sim verbose' command expects 'on' or 'off'"
		" as an argument.\n");
    }
  else
    printf ("The 'sim' command expects either 'trace' or 'verbose'"
	    " as a subcommand.\n");
}
