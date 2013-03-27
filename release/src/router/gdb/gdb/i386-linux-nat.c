/* Native-dependent code for GNU/Linux i386.

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "target.h"
#include "linux-nat.h"

#include "gdb_assert.h"
#include "gdb_string.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/procfs.h>

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#ifndef ORIG_EAX
#define ORIG_EAX -1
#endif

#ifdef HAVE_SYS_DEBUGREG_H
#include <sys/debugreg.h>
#endif

#ifndef DR_FIRSTADDR
#define DR_FIRSTADDR 0
#endif

#ifndef DR_LASTADDR
#define DR_LASTADDR 3
#endif

#ifndef DR_STATUS
#define DR_STATUS 6
#endif

#ifndef DR_CONTROL
#define DR_CONTROL 7
#endif

/* Prototypes for supply_gregset etc.  */
#include "gregset.h"

#include "i387-tdep.h"
#include "i386-tdep.h"
#include "i386-linux-tdep.h"

/* Defines ps_err_e, struct ps_prochandle.  */
#include "gdb_proc_service.h"


/* The register sets used in GNU/Linux ELF core-dumps are identical to
   the register sets in `struct user' that is used for a.out
   core-dumps, and is also used by `ptrace'.  The corresponding types
   are `elf_gregset_t' for the general-purpose registers (with
   `elf_greg_t' the type of a single GP register) and `elf_fpregset_t'
   for the floating-point registers.

   Those types used to be available under the names `gregset_t' and
   `fpregset_t' too, and this file used those names in the past.  But
   those names are now used for the register sets used in the
   `mcontext_t' type, and have a different size and layout.  */

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register array layout.  */
static int regmap[] = 
{
  EAX, ECX, EDX, EBX,
  UESP, EBP, ESI, EDI,
  EIP, EFL, CS, SS,
  DS, ES, FS, GS,
  -1, -1, -1, -1,		/* st0, st1, st2, st3 */
  -1, -1, -1, -1,		/* st4, st5, st6, st7 */
  -1, -1, -1, -1,		/* fctrl, fstat, ftag, fiseg */
  -1, -1, -1, -1,		/* fioff, foseg, fooff, fop */
  -1, -1, -1, -1,		/* xmm0, xmm1, xmm2, xmm3 */
  -1, -1, -1, -1,		/* xmm4, xmm5, xmm6, xmm6 */
  -1,				/* mxcsr */
  ORIG_EAX
};

/* Which ptrace request retrieves which registers?
   These apply to the corresponding SET requests as well.  */

#define GETREGS_SUPPLIES(regno) \
  ((0 <= (regno) && (regno) <= 15) || (regno) == I386_LINUX_ORIG_EAX_REGNUM)

#define GETFPXREGS_SUPPLIES(regno) \
  (I386_ST0_REGNUM <= (regno) && (regno) < I386_SSE_NUM_REGS)

/* Does the current host support the GETREGS request?  */
int have_ptrace_getregs =
#ifdef HAVE_PTRACE_GETREGS
  1
#else
  0
#endif
;

/* Does the current host support the GETFPXREGS request?  The header
   file may or may not define it, and even if it is defined, the
   kernel will return EIO if it's running on a pre-SSE processor.

   My instinct is to attach this to some architecture- or
   target-specific data structure, but really, a particular GDB
   process can only run on top of one kernel at a time.  So it's okay
   for this to be a simple variable.  */
int have_ptrace_getfpxregs =
#ifdef HAVE_PTRACE_GETFPXREGS
  1
#else
  0
#endif
;


/* Accessing registers through the U area, one at a time.  */

/* Fetch one register.  */

static void
fetch_register (struct regcache *regcache, int regno)
{
  int tid;
  int val;

  gdb_assert (!have_ptrace_getregs);
  if (regmap[regno] == -1)
    {
      regcache_raw_supply (regcache, regno, NULL);
      return;
    }

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid); /* Not a threaded program.  */

  errno = 0;
  val = ptrace (PTRACE_PEEKUSER, tid, 4 * regmap[regno], 0);
  if (errno != 0)
    error (_("Couldn't read register %s (#%d): %s."), 
	   gdbarch_register_name (current_gdbarch, regno),
	   regno, safe_strerror (errno));

  regcache_raw_supply (regcache, regno, &val);
}

/* Store one register. */

static void
store_register (const struct regcache *regcache, int regno)
{
  int tid;
  int val;

  gdb_assert (!have_ptrace_getregs);
  if (regmap[regno] == -1)
    return;

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid); /* Not a threaded program.  */

  errno = 0;
  regcache_raw_collect (regcache, regno, &val);
  ptrace (PTRACE_POKEUSER, tid, 4 * regmap[regno], val);
  if (errno != 0)
    error (_("Couldn't write register %s (#%d): %s."),
	   gdbarch_register_name (current_gdbarch, regno),
	   regno, safe_strerror (errno));
}


/* Transfering the general-purpose registers between GDB, inferiors
   and core files.  */

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (struct regcache *regcache, const elf_gregset_t *gregsetp)
{
  const elf_greg_t *regp = (const elf_greg_t *) gregsetp;
  int i;

  for (i = 0; i < I386_NUM_GREGS; i++)
    regcache_raw_supply (regcache, i, regp + regmap[i]);

  if (I386_LINUX_ORIG_EAX_REGNUM < gdbarch_num_regs (current_gdbarch))
    regcache_raw_supply (regcache, I386_LINUX_ORIG_EAX_REGNUM,
			 regp + ORIG_EAX);
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_gregset (const struct regcache *regcache,
	      elf_gregset_t *gregsetp, int regno)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = 0; i < I386_NUM_GREGS; i++)
    if (regno == -1 || regno == i)
      regcache_raw_collect (regcache, i, regp + regmap[i]);

  if ((regno == -1 || regno == I386_LINUX_ORIG_EAX_REGNUM)
      && I386_LINUX_ORIG_EAX_REGNUM < gdbarch_num_regs (current_gdbarch))
    regcache_raw_collect (regcache, I386_LINUX_ORIG_EAX_REGNUM,
			  regp + ORIG_EAX);
}

#ifdef HAVE_PTRACE_GETREGS

/* Fetch all general-purpose registers from process/thread TID and
   store their values in GDB's register array.  */

static void
fetch_regs (struct regcache *regcache, int tid)
{
  elf_gregset_t regs;
  elf_gregset_t *regs_p = &regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    {
      if (errno == EIO)
	{
	  /* The kernel we're running on doesn't support the GETREGS
             request.  Reset `have_ptrace_getregs'.  */
	  have_ptrace_getregs = 0;
	  return;
	}

      perror_with_name (_("Couldn't get registers"));
    }

  supply_gregset (regcache, (const elf_gregset_t *) regs_p);
}

/* Store all valid general-purpose registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_regs (const struct regcache *regcache, int tid, int regno)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't get registers"));

  fill_gregset (regcache, &regs, regno);
  
  if (ptrace (PTRACE_SETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't write registers"));
}

#else

static void fetch_regs (struct regcache *regcache, int tid) {}
static void store_regs (const struct regcache *regcache, int tid, int regno) {}

#endif


/* Transfering floating-point registers between GDB, inferiors and cores.  */

/* Fill GDB's register array with the floating-point register values in
   *FPREGSETP.  */

void 
supply_fpregset (struct regcache *regcache, const elf_fpregset_t *fpregsetp)
{
  i387_supply_fsave (regcache, -1, fpregsetp);
}

/* Fill register REGNO (if it is a floating-point register) in
   *FPREGSETP with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_fpregset (const struct regcache *regcache,
	       elf_fpregset_t *fpregsetp, int regno)
{
  i387_collect_fsave (regcache, regno, fpregsetp);
}

#ifdef HAVE_PTRACE_GETREGS

/* Fetch all floating-point registers from process/thread TID and store
   thier values in GDB's register array.  */

static void
fetch_fpregs (struct regcache *regcache, int tid)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't get floating point status"));

  supply_fpregset (regcache, (const elf_fpregset_t *) &fpregs);
}

/* Store all valid floating-point registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_fpregs (const struct regcache *regcache, int tid, int regno)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't get floating point status"));

  fill_fpregset (regcache, &fpregs, regno);

  if (ptrace (PTRACE_SETFPREGS, tid, 0, (int) &fpregs) < 0)
    perror_with_name (_("Couldn't write floating point status"));
}

#else

static void fetch_fpregs (struct regcache *regcache, int tid) {}
static void store_fpregs (const struct regcache *regcache, int tid, int regno) {}

#endif


/* Transfering floating-point and SSE registers to and from GDB.  */

#ifdef HAVE_PTRACE_GETFPXREGS

/* Fill GDB's register array with the floating-point and SSE register
   values in *FPXREGSETP.  */

void
supply_fpxregset (struct regcache *regcache,
		  const elf_fpxregset_t *fpxregsetp)
{
  i387_supply_fxsave (regcache, -1, fpxregsetp);
}

/* Fill register REGNO (if it is a floating-point or SSE register) in
   *FPXREGSETP with the value in GDB's register array.  If REGNO is
   -1, do this for all registers.  */

void
fill_fpxregset (const struct regcache *regcache,
		elf_fpxregset_t *fpxregsetp, int regno)
{
  i387_collect_fxsave (regcache, regno, fpxregsetp);
}

/* Fetch all registers covered by the PTRACE_GETFPXREGS request from
   process/thread TID and store their values in GDB's register array.
   Return non-zero if successful, zero otherwise.  */

static int
fetch_fpxregs (struct regcache *regcache, int tid)
{
  elf_fpxregset_t fpxregs;

  if (! have_ptrace_getfpxregs)
    return 0;

  if (ptrace (PTRACE_GETFPXREGS, tid, 0, (int) &fpxregs) < 0)
    {
      if (errno == EIO)
	{
	  have_ptrace_getfpxregs = 0;
	  return 0;
	}

      perror_with_name (_("Couldn't read floating-point and SSE registers"));
    }

  supply_fpxregset (regcache, (const elf_fpxregset_t *) &fpxregs);
  return 1;
}

/* Store all valid registers in GDB's register array covered by the
   PTRACE_SETFPXREGS request into the process/thread specified by TID.
   Return non-zero if successful, zero otherwise.  */

static int
store_fpxregs (const struct regcache *regcache, int tid, int regno)
{
  elf_fpxregset_t fpxregs;

  if (! have_ptrace_getfpxregs)
    return 0;
  
  if (ptrace (PTRACE_GETFPXREGS, tid, 0, &fpxregs) == -1)
    {
      if (errno == EIO)
	{
	  have_ptrace_getfpxregs = 0;
	  return 0;
	}

      perror_with_name (_("Couldn't read floating-point and SSE registers"));
    }

  fill_fpxregset (regcache, &fpxregs, regno);

  if (ptrace (PTRACE_SETFPXREGS, tid, 0, &fpxregs) == -1)
    perror_with_name (_("Couldn't write floating-point and SSE registers"));

  return 1;
}

#else

static int fetch_fpxregs (struct regcache *regcache, int tid) { return 0; }
static int store_fpxregs (const struct regcache *regcache, int tid, int regno) { return 0; }

#endif /* HAVE_PTRACE_GETFPXREGS */


/* Transferring arbitrary registers between GDB and inferior.  */

/* Fetch register REGNO from the child process.  If REGNO is -1, do
   this for all registers (including the floating point and SSE
   registers).  */

static void
i386_linux_fetch_inferior_registers (struct regcache *regcache, int regno)
{
  int tid;

  /* Use the old method of peeking around in `struct user' if the
     GETREGS request isn't available.  */
  if (!have_ptrace_getregs)
    {
      int i;

      for (i = 0; i < gdbarch_num_regs (current_gdbarch); i++)
	if (regno == -1 || regno == i)
	  fetch_register (regcache, i);

      return;
    }

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid); /* Not a threaded program.  */

  /* Use the PTRACE_GETFPXREGS request whenever possible, since it
     transfers more registers in one system call, and we'll cache the
     results.  But remember that fetch_fpxregs can fail, and return
     zero.  */
  if (regno == -1)
    {
      fetch_regs (regcache, tid);

      /* The call above might reset `have_ptrace_getregs'.  */
      if (!have_ptrace_getregs)
	{
	  i386_linux_fetch_inferior_registers (regcache, regno);
	  return;
	}

      if (fetch_fpxregs (regcache, tid))
	return;
      fetch_fpregs (regcache, tid);
      return;
    }

  if (GETREGS_SUPPLIES (regno))
    {
      fetch_regs (regcache, tid);
      return;
    }

  if (GETFPXREGS_SUPPLIES (regno))
    {
      if (fetch_fpxregs (regcache, tid))
	return;

      /* Either our processor or our kernel doesn't support the SSE
	 registers, so read the FP registers in the traditional way,
	 and fill the SSE registers with dummy values.  It would be
	 more graceful to handle differences in the register set using
	 gdbarch.  Until then, this will at least make things work
	 plausibly.  */
      fetch_fpregs (regcache, tid);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request for bad register number %d."), regno);
}

/* Store register REGNO back into the child process.  If REGNO is -1,
   do this for all registers (including the floating point and SSE
   registers).  */
static void
i386_linux_store_inferior_registers (struct regcache *regcache, int regno)
{
  int tid;

  /* Use the old method of poking around in `struct user' if the
     SETREGS request isn't available.  */
  if (!have_ptrace_getregs)
    {
      int i;

      for (i = 0; i < gdbarch_num_regs (current_gdbarch); i++)
	if (regno == -1 || regno == i)
	  store_register (regcache, i);

      return;
    }

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid); /* Not a threaded program.  */

  /* Use the PTRACE_SETFPXREGS requests whenever possible, since it
     transfers more registers in one system call.  But remember that
     store_fpxregs can fail, and return zero.  */
  if (regno == -1)
    {
      store_regs (regcache, tid, regno);
      if (store_fpxregs (regcache, tid, regno))
	return;
      store_fpregs (regcache, tid, regno);
      return;
    }

  if (GETREGS_SUPPLIES (regno))
    {
      store_regs (regcache, tid, regno);
      return;
    }

  if (GETFPXREGS_SUPPLIES (regno))
    {
      if (store_fpxregs (regcache, tid, regno))
	return;

      /* Either our processor or our kernel doesn't support the SSE
	 registers, so just write the FP registers in the traditional
	 way.  */
      store_fpregs (regcache, tid, regno);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request to store bad register number %d."), regno);
}


/* Support for debug registers.  */

static unsigned long
i386_linux_dr_get (int regnum)
{
  int tid;
  unsigned long value;

  /* FIXME: kettenis/2001-01-29: It's not clear what we should do with
     multi-threaded processes here.  For now, pretend there is just
     one thread.  */
  tid = PIDGET (inferior_ptid);

  /* FIXME: kettenis/2001-03-27: Calling perror_with_name if the
     ptrace call fails breaks debugging remote targets.  The correct
     way to fix this is to add the hardware breakpoint and watchpoint
     stuff to the target vector.  For now, just return zero if the
     ptrace call fails.  */
  errno = 0;
  value = ptrace (PTRACE_PEEKUSER, tid,
		  offsetof (struct user, u_debugreg[regnum]), 0);
  if (errno != 0)
#if 0
    perror_with_name (_("Couldn't read debug register"));
#else
    return 0;
#endif

  return value;
}

static void
i386_linux_dr_set (int regnum, unsigned long value)
{
  int tid;

  /* FIXME: kettenis/2001-01-29: It's not clear what we should do with
     multi-threaded processes here.  For now, pretend there is just
     one thread.  */
  tid = PIDGET (inferior_ptid);

  errno = 0;
  ptrace (PTRACE_POKEUSER, tid,
	  offsetof (struct user, u_debugreg[regnum]), value);
  if (errno != 0)
    perror_with_name (_("Couldn't write debug register"));
}

void
i386_linux_dr_set_control (unsigned long control)
{
  i386_linux_dr_set (DR_CONTROL, control);
}

void
i386_linux_dr_set_addr (int regnum, CORE_ADDR addr)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  i386_linux_dr_set (DR_FIRSTADDR + regnum, addr);
}

void
i386_linux_dr_reset_addr (int regnum)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  i386_linux_dr_set (DR_FIRSTADDR + regnum, 0L);
}

unsigned long
i386_linux_dr_get_status (void)
{
  return i386_linux_dr_get (DR_STATUS);
}


/* Called by libthread_db.  Returns a pointer to the thread local
   storage (or its descriptor).  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph, 
		    lwpid_t lwpid, int idx, void **base)
{
  /* NOTE: cagney/2003-08-26: The definition of this buffer is found
     in the kernel header <asm-i386/ldt.h>.  It, after padding, is 4 x
     4 byte integers in size: `entry_number', `base_addr', `limit',
     and a bunch of status bits.

     The values returned by this ptrace call should be part of the
     regcache buffer, and ps_get_thread_area should channel its
     request through the regcache.  That way remote targets could
     provide the value using the remote protocol and not this direct
     call.

     Is this function needed?  I'm guessing that the `base' is the
     address of a a descriptor that libthread_db uses to find the
     thread local address base that GDB needs.  Perhaps that
     descriptor is defined by the ABI.  Anyway, given that
     libthread_db calls this function without prompting (gdb
     requesting tls base) I guess it needs info in there anyway.  */
  unsigned int desc[4];
  gdb_assert (sizeof (int) == 4);

#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA 25
#endif

  if (ptrace (PTRACE_GET_THREAD_AREA, lwpid,
	      (void *) idx, (unsigned long) &desc) < 0)
    return PS_ERR;

  *(int *)base = desc[1];
  return PS_OK;
}


/* The instruction for a GNU/Linux system call is:
       int $0x80
   or 0xcd 0x80.  */

static const unsigned char linux_syscall[] = { 0xcd, 0x80 };

#define LINUX_SYSCALL_LEN (sizeof linux_syscall)

/* The system call number is stored in the %eax register.  */
#define LINUX_SYSCALL_REGNUM I386_EAX_REGNUM

/* We are specifically interested in the sigreturn and rt_sigreturn
   system calls.  */

#ifndef SYS_sigreturn
#define SYS_sigreturn		0x77
#endif
#ifndef SYS_rt_sigreturn
#define SYS_rt_sigreturn	0xad
#endif

/* Offset to saved processor flags, from <asm/sigcontext.h>.  */
#define LINUX_SIGCONTEXT_EFLAGS_OFFSET (64)

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

static void
i386_linux_resume (ptid_t ptid, int step, enum target_signal signal)
{
  int pid = PIDGET (ptid);

  int request = PTRACE_CONT;

  if (pid == -1)
    /* Resume all threads.  */
    /* I think this only gets used in the non-threaded case, where "resume
       all threads" and "resume inferior_ptid" are the same.  */
    pid = PIDGET (inferior_ptid);

  if (step)
    {
      struct regcache *regcache = get_thread_regcache (pid_to_ptid (pid));
      ULONGEST pc;
      gdb_byte buf[LINUX_SYSCALL_LEN];

      request = PTRACE_SINGLESTEP;

      regcache_cooked_read_unsigned (regcache,
				     gdbarch_pc_regnum (current_gdbarch), &pc);

      /* Returning from a signal trampoline is done by calling a
         special system call (sigreturn or rt_sigreturn, see
         i386-linux-tdep.c for more information).  This system call
         restores the registers that were saved when the signal was
         raised, including %eflags.  That means that single-stepping
         won't work.  Instead, we'll have to modify the signal context
         that's about to be restored, and set the trace flag there.  */

      /* First check if PC is at a system call.  */
      if (read_memory_nobpt (pc, buf, LINUX_SYSCALL_LEN) == 0
	  && memcmp (buf, linux_syscall, LINUX_SYSCALL_LEN) == 0)
	{
	  ULONGEST syscall;
	  regcache_cooked_read_unsigned (regcache,
					 LINUX_SYSCALL_REGNUM, &syscall);

	  /* Then check the system call number.  */
	  if (syscall == SYS_sigreturn || syscall == SYS_rt_sigreturn)
	    {
	      ULONGEST sp, addr;
	      unsigned long int eflags;

	      regcache_cooked_read_unsigned (regcache, I386_ESP_REGNUM, &sp);
	      if (syscall == SYS_rt_sigreturn)
		addr = read_memory_integer (sp + 8, 4) + 20;
	      else
		addr = sp;

	      /* Set the trace flag in the context that's about to be
                 restored.  */
	      addr += LINUX_SIGCONTEXT_EFLAGS_OFFSET;
	      read_memory (addr, (gdb_byte *) &eflags, 4);
	      eflags |= 0x0100;
	      write_memory (addr, (gdb_byte *) &eflags, 4);
	    }
	}
    }

  if (ptrace (request, pid, 0, target_signal_to_host (signal)) == -1)
    perror_with_name (("ptrace"));
}

static void (*super_post_startup_inferior) (ptid_t ptid);

static void
i386_linux_child_post_startup_inferior (ptid_t ptid)
{
  i386_cleanup_dregs ();
  super_post_startup_inferior (ptid);
}

void
_initialize_i386_linux_nat (void)
{
  struct target_ops *t;

  /* Fill in the generic GNU/Linux methods.  */
  t = linux_target ();

  /* Override the default ptrace resume method.  */
  t->to_resume = i386_linux_resume;

  /* Override the GNU/Linux inferior startup hook.  */
  super_post_startup_inferior = t->to_post_startup_inferior;
  t->to_post_startup_inferior = i386_linux_child_post_startup_inferior;

  /* Add our register access methods.  */
  t->to_fetch_registers = i386_linux_fetch_inferior_registers;
  t->to_store_registers = i386_linux_store_inferior_registers;

  /* Register the target.  */
  linux_nat_add_target (t);
}
