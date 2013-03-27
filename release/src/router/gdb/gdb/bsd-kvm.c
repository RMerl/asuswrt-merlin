/* BSD Kernel Data Access Library (libkvm) interface.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

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

#include "defs.h"
#include "cli/cli-cmds.h"
#include "command.h"
#include "frame.h"
#include "regcache.h"
#include "target.h"
#include "value.h"
#include "gdbcore.h"		/* for get_exec_file */

#include "gdb_assert.h"
#include <fcntl.h>
#include <kvm.h>
#ifdef HAVE_NLIST_H
#include <nlist.h>
#endif
#include <paths.h>
#include "readline/readline.h"
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>

#include "bsd-kvm.h"

/* Kernel memory device file.  */
static const char *bsd_kvm_corefile;

/* Kernel memory interface descriptor.  */
static kvm_t *core_kd;

/* Address of process control block.  */
static struct pcb *bsd_kvm_paddr;

/* Pointer to architecture-specific function that reconstructs the
   register state from PCB and supplies it to REGCACHE.  */
static int (*bsd_kvm_supply_pcb)(struct regcache *regcache, struct pcb *pcb);

/* Target ops for libkvm interface.  */
static struct target_ops bsd_kvm_ops;

static void
bsd_kvm_open (char *filename, int from_tty)
{
  char errbuf[_POSIX2_LINE_MAX];
  char *execfile = NULL;
  kvm_t *temp_kd;

  target_preopen (from_tty);

  if (filename)
    {
      char *temp;

      filename = tilde_expand (filename);
      if (filename[0] != '/')
	{
	  temp = concat (current_directory, "/", filename, (char *)NULL);
	  xfree (filename);
	  filename = temp;
	}
    }

  execfile = get_exec_file (0);
  temp_kd = kvm_openfiles (execfile, filename, NULL,
			   write_files ? O_RDWR : O_RDONLY, errbuf);
  if (temp_kd == NULL)
    error (("%s"), errbuf);

  bsd_kvm_corefile = filename;
  unpush_target (&bsd_kvm_ops);
  core_kd = temp_kd;
  push_target (&bsd_kvm_ops);

  target_fetch_registers (get_current_regcache (), -1);

  reinit_frame_cache ();
  print_stack_frame (get_selected_frame (NULL), -1, 1);
}

static void
bsd_kvm_close (int quitting)
{
  if (core_kd)
    {
      if (kvm_close (core_kd) == -1)
	warning (("%s"), kvm_geterr(core_kd));
      core_kd = NULL;
    }
}

static LONGEST
bsd_kvm_xfer_memory (CORE_ADDR addr, ULONGEST len,
		     gdb_byte *readbuf, const gdb_byte *writebuf)
{
  ssize_t nbytes = len;

  if (readbuf)
    nbytes = kvm_read (core_kd, addr, readbuf, nbytes);
  if (writebuf && nbytes > 0)
    nbytes = kvm_write (core_kd, addr, writebuf, nbytes);
  return nbytes;
}

static LONGEST
bsd_kvm_xfer_partial (struct target_ops *ops, enum target_object object,
		      const char *annex, gdb_byte *readbuf,
		      const gdb_byte *writebuf,
		      ULONGEST offset, LONGEST len)
{
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
      return bsd_kvm_xfer_memory (offset, len, readbuf, writebuf);

    default:
      return -1;
    }
}

static void
bsd_kvm_files_info (struct target_ops *ops)
{
  if (bsd_kvm_corefile && strcmp (bsd_kvm_corefile, _PATH_MEM) != 0)
    printf_filtered (_("\tUsing the kernel crash dump %s.\n"),
		     bsd_kvm_corefile);
  else
    printf_filtered (_("\tUsing the currently running kernel.\n"));
}

/* Fetch process control block at address PADDR.  */

static int
bsd_kvm_fetch_pcb (struct regcache *regcache, struct pcb *paddr)
{
  struct pcb pcb;

  if (kvm_read (core_kd, (unsigned long) paddr, &pcb, sizeof pcb) == -1)
    error (("%s"), kvm_geterr (core_kd));

  gdb_assert (bsd_kvm_supply_pcb);
  return bsd_kvm_supply_pcb (regcache, &pcb);
}

static void
bsd_kvm_fetch_registers (struct regcache *regcache, int regnum)
{
  struct nlist nl[2];

  if (bsd_kvm_paddr)
    {
      bsd_kvm_fetch_pcb (regcache, bsd_kvm_paddr);
      return;
    }

  /* On dumping core, BSD kernels store the faulting context (PCB)
     in the variable "dumppcb".  */
  memset (nl, 0, sizeof nl);
  nl[0].n_name = "_dumppcb";

  if (kvm_nlist (core_kd, nl) == -1)
    error (("%s"), kvm_geterr (core_kd));

  if (nl[0].n_value != 0)
    {
      /* Found dumppcb. If it contains a valid context, return
	 immediately.  */
      if (bsd_kvm_fetch_pcb (regcache, (struct pcb *) nl[0].n_value))
	return;
    }

  /* Traditional BSD kernels have a process proc0 that should always
     be present.  The address of proc0's PCB is stored in the variable
     "proc0paddr".  */

  memset (nl, 0, sizeof nl);
  nl[0].n_name = "_proc0paddr";

  if (kvm_nlist (core_kd, nl) == -1)
    error (("%s"), kvm_geterr (core_kd));

  if (nl[0].n_value != 0)
    {
      struct pcb *paddr;

      /* Found proc0paddr.  */
      if (kvm_read (core_kd, nl[0].n_value, &paddr, sizeof paddr) == -1)
	error (("%s"), kvm_geterr (core_kd));

      bsd_kvm_fetch_pcb (regcache, paddr);
      return;
    }

#ifdef HAVE_STRUCT_THREAD_TD_PCB
  /* In FreeBSD kernels for 5.0-RELEASE and later, the PCB no longer
     lives in `struct proc' but in `struct thread'.  The `struct
     thread' for the initial thread for proc0 can be found in the
     variable "thread0".  */

  memset (nl, 0, sizeof nl);
  nl[0].n_name = "_thread0";

  if (kvm_nlist (core_kd, nl) == -1)
    error (("%s"), kvm_geterr (core_kd));

  if (nl[0].n_value != 0)
    {
      struct pcb *paddr;

      /* Found thread0.  */
      nl[0].n_value += offsetof (struct thread, td_pcb);
      if (kvm_read (core_kd, nl[0].n_value, &paddr, sizeof paddr) == -1)
	error (("%s"), kvm_geterr (core_kd));

      bsd_kvm_fetch_pcb (regcache, paddr);
      return;
    }
#endif

  /* i18n: PCB == "Process Control Block" */
  error (_("Cannot find a valid PCB"));
}


/* Kernel memory interface commands.  */
struct cmd_list_element *bsd_kvm_cmdlist;

static void
bsd_kvm_cmd (char *arg, int fromtty)
{
  /* ??? Should this become an alias for "target kvm"?  */
}

#ifndef HAVE_STRUCT_THREAD_TD_PCB

static void
bsd_kvm_proc_cmd (char *arg, int fromtty)
{
  CORE_ADDR addr;

  if (arg == NULL)
    error_no_arg (_("proc address"));

  if (core_kd == NULL)
    error (_("No kernel memory image."));

  addr = parse_and_eval_address (arg);
#ifdef HAVE_STRUCT_LWP
  addr += offsetof (struct lwp, l_addr);
#else
  addr += offsetof (struct proc, p_addr);
#endif

  if (kvm_read (core_kd, addr, &bsd_kvm_paddr, sizeof bsd_kvm_paddr) == -1)
    error (("%s"), kvm_geterr (core_kd));

  target_fetch_registers (get_current_regcache (), -1);

  reinit_frame_cache ();
  print_stack_frame (get_selected_frame (NULL), -1, 1);
}

#endif

static void
bsd_kvm_pcb_cmd (char *arg, int fromtty)
{
  if (arg == NULL)
    /* i18n: PCB == "Process Control Block" */
    error_no_arg (_("pcb address"));

  if (core_kd == NULL)
    error (_("No kernel memory image."));

  bsd_kvm_paddr = (struct pcb *)(u_long) parse_and_eval_address (arg);

  target_fetch_registers (get_current_regcache (), -1);

  reinit_frame_cache ();
  print_stack_frame (get_selected_frame (NULL), -1, 1);
}

/* Add the libkvm interface to the list of all possible targets and
   register CUPPLY_PCB as the architecture-specific process control
   block interpreter.  */

void
bsd_kvm_add_target (int (*supply_pcb)(struct regcache *, struct pcb *))
{
  gdb_assert (bsd_kvm_supply_pcb == NULL);
  bsd_kvm_supply_pcb = supply_pcb;

  bsd_kvm_ops.to_shortname = "kvm";
  bsd_kvm_ops.to_longname = _("Kernel memory interface");
  bsd_kvm_ops.to_doc = _("Use a kernel virtual memory image as a target.\n\
Optionally specify the filename of a core dump.");
  bsd_kvm_ops.to_open = bsd_kvm_open;
  bsd_kvm_ops.to_close = bsd_kvm_close;
  bsd_kvm_ops.to_fetch_registers = bsd_kvm_fetch_registers;
  bsd_kvm_ops.to_xfer_partial = bsd_kvm_xfer_partial;
  bsd_kvm_ops.to_files_info = bsd_kvm_files_info;
  bsd_kvm_ops.to_stratum = process_stratum;
  bsd_kvm_ops.to_has_memory = 1;
  bsd_kvm_ops.to_has_stack = 1;
  bsd_kvm_ops.to_has_registers = 1;
  bsd_kvm_ops.to_magic = OPS_MAGIC;

  add_target (&bsd_kvm_ops);
  
  add_prefix_cmd ("kvm", class_obscure, bsd_kvm_cmd, _("\
Generic command for manipulating the kernel memory interface."),
		  &bsd_kvm_cmdlist, "kvm ", 0, &cmdlist);

#ifndef HAVE_STRUCT_THREAD_TD_PCB
  add_cmd ("proc", class_obscure, bsd_kvm_proc_cmd,
	   _("Set current context from proc address"), &bsd_kvm_cmdlist);
#endif
  add_cmd ("pcb", class_obscure, bsd_kvm_pcb_cmd,
	   /* i18n: PCB == "Process Control Block" */
	   _("Set current context from pcb address"), &bsd_kvm_cmdlist);
}
