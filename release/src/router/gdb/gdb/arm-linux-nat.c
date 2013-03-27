/* GNU/Linux on ARM native support.
   Copyright (C) 1999, 2000, 2001, 2002, 2004, 2005, 2006, 2007
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
#include "gdb_string.h"
#include "regcache.h"
#include "target.h"
#include "linux-nat.h"
#include "target-descriptions.h"
#include "xml-support.h"

#include "arm-tdep.h"
#include "arm-linux-tdep.h"

#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/utsname.h>
#include <sys/procfs.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

/* Defines ps_err_e, struct ps_prochandle.  */
#include "gdb_proc_service.h"

#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA 22
#endif

#ifndef PTRACE_GETWMMXREGS
#define PTRACE_GETWMMXREGS 18
#define PTRACE_SETWMMXREGS 19
#endif

/* A flag for whether the WMMX registers are available.  */
static int arm_linux_has_wmmx_registers;

extern int arm_apcs_32;

/* The following variables are used to determine the version of the
   underlying GNU/Linux operating system.  Examples:

   GNU/Linux 2.0.35             GNU/Linux 2.2.12
   os_version = 0x00020023      os_version = 0x0002020c
   os_major = 2                 os_major = 2
   os_minor = 0                 os_minor = 2
   os_release = 35              os_release = 12

   Note: os_version = (os_major << 16) | (os_minor << 8) | os_release

   These are initialized using get_linux_version() from
   _initialize_arm_linux_nat().  */

static unsigned int os_version, os_major, os_minor, os_release;

/* On GNU/Linux, threads are implemented as pseudo-processes, in which
   case we may be tracing more than one process at a time.  In that
   case, inferior_ptid will contain the main process ID and the
   individual thread (process) ID.  get_thread_id () is used to get
   the thread id if it's available, and the process id otherwise.  */

int
get_thread_id (ptid_t ptid)
{
  int tid = TIDGET (ptid);
  if (0 == tid)
    tid = PIDGET (ptid);
  return tid;
}
#define GET_THREAD_ID(PTID)	get_thread_id (PTID)

/* Get the value of a particular register from the floating point
   state of the process and store it into regcache.  */

static void
fetch_fpregister (struct regcache *regcache, int regno)
{
  int ret, tid;
  gdb_byte fp[ARM_LINUX_SIZEOF_NWFPE];
  
  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);

  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to fetch floating point register."));
      return;
    }

  /* Fetch fpsr.  */
  if (ARM_FPS_REGNUM == regno)
    regcache_raw_supply (regcache, ARM_FPS_REGNUM,
			 fp + NWFPE_FPSR_OFFSET);

  /* Fetch the floating point register.  */
  if (regno >= ARM_F0_REGNUM && regno <= ARM_F7_REGNUM)
    supply_nwfpe_register (regcache, regno, fp);
}

/* Get the whole floating point state of the process and store it
   into regcache.  */

static void
fetch_fpregs (struct regcache *regcache)
{
  int ret, regno, tid;
  gdb_byte fp[ARM_LINUX_SIZEOF_NWFPE];

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to fetch the floating point registers."));
      return;
    }

  /* Fetch fpsr.  */
  regcache_raw_supply (regcache, ARM_FPS_REGNUM,
		       fp + NWFPE_FPSR_OFFSET);

  /* Fetch the floating point registers.  */
  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    supply_nwfpe_register (regcache, regno, fp);
}

/* Save a particular register into the floating point state of the
   process using the contents from regcache.  */

static void
store_fpregister (const struct regcache *regcache, int regno)
{
  int ret, tid;
  gdb_byte fp[ARM_LINUX_SIZEOF_NWFPE];

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to fetch the floating point registers."));
      return;
    }

  /* Store fpsr.  */
  if (ARM_FPS_REGNUM == regno && regcache_valid_p (regcache, ARM_FPS_REGNUM))
    regcache_raw_collect (regcache, ARM_FPS_REGNUM, fp + NWFPE_FPSR_OFFSET);

  /* Store the floating point register.  */
  if (regno >= ARM_F0_REGNUM && regno <= ARM_F7_REGNUM)
    collect_nwfpe_register (regcache, regno, fp);

  ret = ptrace (PTRACE_SETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to store floating point register."));
      return;
    }
}

/* Save the whole floating point state of the process using
   the contents from regcache.  */

static void
store_fpregs (const struct regcache *regcache)
{
  int ret, regno, tid;
  gdb_byte fp[ARM_LINUX_SIZEOF_NWFPE];

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to fetch the floating point registers."));
      return;
    }

  /* Store fpsr.  */
  if (regcache_valid_p (regcache, ARM_FPS_REGNUM))
    regcache_raw_collect (regcache, ARM_FPS_REGNUM, fp + NWFPE_FPSR_OFFSET);

  /* Store the floating point registers.  */
  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    if (regcache_valid_p (regcache, regno))
      collect_nwfpe_register (regcache, regno, fp);

  ret = ptrace (PTRACE_SETFPREGS, tid, 0, fp);
  if (ret < 0)
    {
      warning (_("Unable to store floating point registers."));
      return;
    }
}

/* Fetch a general register of the process and store into
   regcache.  */

static void
fetch_register (struct regcache *regcache, int regno)
{
  int ret, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning (_("Unable to fetch general register."));
      return;
    }

  if (regno >= ARM_A1_REGNUM && regno < ARM_PC_REGNUM)
    regcache_raw_supply (regcache, regno, (char *) &regs[regno]);

  if (ARM_PS_REGNUM == regno)
    {
      if (arm_apcs_32)
        regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     (char *) &regs[ARM_CPSR_REGNUM]);
      else
        regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     (char *) &regs[ARM_PC_REGNUM]);
    }
    
  if (ARM_PC_REGNUM == regno)
    { 
      regs[ARM_PC_REGNUM] = gdbarch_addr_bits_remove
			      (current_gdbarch, regs[ARM_PC_REGNUM]);
      regcache_raw_supply (regcache, ARM_PC_REGNUM,
			   (char *) &regs[ARM_PC_REGNUM]);
    }
}

/* Fetch all general registers of the process and store into
   regcache.  */

static void
fetch_regs (struct regcache *regcache)
{
  int ret, regno, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning (_("Unable to fetch general registers."));
      return;
    }

  for (regno = ARM_A1_REGNUM; regno < ARM_PC_REGNUM; regno++)
    regcache_raw_supply (regcache, regno, (char *) &regs[regno]);

  if (arm_apcs_32)
    regcache_raw_supply (regcache, ARM_PS_REGNUM,
			 (char *) &regs[ARM_CPSR_REGNUM]);
  else
    regcache_raw_supply (regcache, ARM_PS_REGNUM,
			 (char *) &regs[ARM_PC_REGNUM]);

  regs[ARM_PC_REGNUM] = gdbarch_addr_bits_remove
			  (current_gdbarch, regs[ARM_PC_REGNUM]);
  regcache_raw_supply (regcache, ARM_PC_REGNUM,
		       (char *) &regs[ARM_PC_REGNUM]);
}

/* Store all general registers of the process from the values in
   regcache.  */

static void
store_register (const struct regcache *regcache, int regno)
{
  int ret, tid;
  elf_gregset_t regs;
  
  if (!regcache_valid_p (regcache, regno))
    return;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Get the general registers from the process.  */
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning (_("Unable to fetch general registers."));
      return;
    }

  if (regno >= ARM_A1_REGNUM && regno <= ARM_PC_REGNUM)
    regcache_raw_collect (regcache, regno, (char *) &regs[regno]);
  else if (arm_apcs_32 && regno == ARM_PS_REGNUM)
    regcache_raw_collect (regcache, regno,
			 (char *) &regs[ARM_CPSR_REGNUM]);
  else if (!arm_apcs_32 && regno == ARM_PS_REGNUM)
    regcache_raw_collect (regcache, ARM_PC_REGNUM,
			 (char *) &regs[ARM_PC_REGNUM]);

  ret = ptrace (PTRACE_SETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning (_("Unable to store general register."));
      return;
    }
}

static void
store_regs (const struct regcache *regcache)
{
  int ret, regno, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Fetch the general registers.  */
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning (_("Unable to fetch general registers."));
      return;
    }

  for (regno = ARM_A1_REGNUM; regno <= ARM_PC_REGNUM; regno++)
    {
      if (regcache_valid_p (regcache, regno))
	regcache_raw_collect (regcache, regno, (char *) &regs[regno]);
    }

  if (arm_apcs_32 && regcache_valid_p (regcache, ARM_PS_REGNUM))
    regcache_raw_collect (regcache, ARM_PS_REGNUM,
			 (char *) &regs[ARM_CPSR_REGNUM]);

  ret = ptrace (PTRACE_SETREGS, tid, 0, &regs);

  if (ret < 0)
    {
      warning (_("Unable to store general registers."));
      return;
    }
}

/* Fetch all WMMX registers of the process and store into
   regcache.  */

#define IWMMXT_REGS_SIZE (16 * 8 + 6 * 4)

static void
fetch_wmmx_regs (struct regcache *regcache)
{
  char regbuf[IWMMXT_REGS_SIZE];
  int ret, regno, tid;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);

  ret = ptrace (PTRACE_GETWMMXREGS, tid, 0, regbuf);
  if (ret < 0)
    {
      warning (_("Unable to fetch WMMX registers."));
      return;
    }

  for (regno = 0; regno < 16; regno++)
    regcache_raw_supply (regcache, regno + ARM_WR0_REGNUM,
			 &regbuf[regno * 8]);

  for (regno = 0; regno < 2; regno++)
    regcache_raw_supply (regcache, regno + ARM_WCSSF_REGNUM,
			 &regbuf[16 * 8 + regno * 4]);

  for (regno = 0; regno < 4; regno++)
    regcache_raw_supply (regcache, regno + ARM_WCGR0_REGNUM,
			 &regbuf[16 * 8 + 2 * 4 + regno * 4]);
}

static void
store_wmmx_regs (const struct regcache *regcache)
{
  char regbuf[IWMMXT_REGS_SIZE];
  int ret, regno, tid;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);

  ret = ptrace (PTRACE_GETWMMXREGS, tid, 0, regbuf);
  if (ret < 0)
    {
      warning (_("Unable to fetch WMMX registers."));
      return;
    }

  for (regno = 0; regno < 16; regno++)
    if (regcache_valid_p (regcache, regno + ARM_WR0_REGNUM))
      regcache_raw_collect (regcache, regno + ARM_WR0_REGNUM,
			    &regbuf[regno * 8]);

  for (regno = 0; regno < 2; regno++)
    if (regcache_valid_p (regcache, regno + ARM_WCSSF_REGNUM))
      regcache_raw_collect (regcache, regno + ARM_WCSSF_REGNUM,
			    &regbuf[16 * 8 + regno * 4]);

  for (regno = 0; regno < 4; regno++)
    if (regcache_valid_p (regcache, regno + ARM_WCGR0_REGNUM))
      regcache_raw_collect (regcache, regno + ARM_WCGR0_REGNUM,
			    &regbuf[16 * 8 + 2 * 4 + regno * 4]);

  ret = ptrace (PTRACE_SETWMMXREGS, tid, 0, regbuf);

  if (ret < 0)
    {
      warning (_("Unable to store WMMX registers."));
      return;
    }
}

/* Fetch registers from the child process.  Fetch all registers if
   regno == -1, otherwise fetch all general registers or all floating
   point registers depending upon the value of regno.  */

static void
arm_linux_fetch_inferior_registers (struct regcache *regcache, int regno)
{
  if (-1 == regno)
    {
      fetch_regs (regcache);
      fetch_fpregs (regcache);
      if (arm_linux_has_wmmx_registers)
	fetch_wmmx_regs (regcache);
    }
  else 
    {
      if (regno < ARM_F0_REGNUM || regno == ARM_PS_REGNUM)
        fetch_register (regcache, regno);
      else if (regno >= ARM_F0_REGNUM && regno <= ARM_FPS_REGNUM)
        fetch_fpregister (regcache, regno);
      else if (arm_linux_has_wmmx_registers
	       && regno >= ARM_WR0_REGNUM && regno <= ARM_WCGR7_REGNUM)
	fetch_wmmx_regs (regcache);
    }
}

/* Store registers back into the inferior.  Store all registers if
   regno == -1, otherwise store all general registers or all floating
   point registers depending upon the value of regno.  */

static void
arm_linux_store_inferior_registers (struct regcache *regcache, int regno)
{
  if (-1 == regno)
    {
      store_regs (regcache);
      store_fpregs (regcache);
      if (arm_linux_has_wmmx_registers)
	store_wmmx_regs (regcache);
    }
  else
    {
      if (regno < ARM_F0_REGNUM || regno == ARM_PS_REGNUM)
        store_register (regcache, regno);
      else if ((regno >= ARM_F0_REGNUM) && (regno <= ARM_FPS_REGNUM))
        store_fpregister (regcache, regno);
      else if (arm_linux_has_wmmx_registers
	       && regno >= ARM_WR0_REGNUM && regno <= ARM_WCGR7_REGNUM)
	store_wmmx_regs (regcache);
    }
}

/* Wrapper functions for the standard regset handling, used by
   thread debugging.  */

void
fill_gregset (const struct regcache *regcache,	
	      gdb_gregset_t *gregsetp, int regno)
{
  arm_linux_collect_gregset (NULL, regcache, regno, gregsetp, 0);
}

void
supply_gregset (struct regcache *regcache, const gdb_gregset_t *gregsetp)
{
  arm_linux_supply_gregset (NULL, regcache, -1, gregsetp, 0);
}

void
fill_fpregset (const struct regcache *regcache,
	       gdb_fpregset_t *fpregsetp, int regno)
{
  arm_linux_collect_nwfpe (NULL, regcache, regno, fpregsetp, 0);
}

/* Fill GDB's register array with the floating-point register values
   in *fpregsetp.  */

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t *fpregsetp)
{
  arm_linux_supply_nwfpe (NULL, regcache, -1, fpregsetp, 0);
}

/* Fetch the thread-local storage pointer for libthread_db.  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
  if (ptrace (PTRACE_GET_THREAD_AREA, lwpid, NULL, base) != 0)
    return PS_ERR;

  /* IDX is the bias from the thread pointer to the beginning of the
     thread descriptor.  It has to be subtracted due to implementation
     quirks in libthread_db.  */
  *base = (void *) ((char *)*base - idx);

  return PS_OK;
}

static unsigned int
get_linux_version (unsigned int *vmajor,
		   unsigned int *vminor,
		   unsigned int *vrelease)
{
  struct utsname info;
  char *pmajor, *pminor, *prelease, *tail;

  if (-1 == uname (&info))
    {
      warning (_("Unable to determine GNU/Linux version."));
      return -1;
    }

  pmajor = strtok (info.release, ".");
  pminor = strtok (NULL, ".");
  prelease = strtok (NULL, ".");

  *vmajor = (unsigned int) strtoul (pmajor, &tail, 0);
  *vminor = (unsigned int) strtoul (pminor, &tail, 0);
  *vrelease = (unsigned int) strtoul (prelease, &tail, 0);

  return ((*vmajor << 16) | (*vminor << 8) | *vrelease);
}

static LONGEST (*super_xfer_partial) (struct target_ops *, enum target_object,
				      const char *, gdb_byte *, const gdb_byte *,
				      ULONGEST, LONGEST);

static LONGEST
arm_linux_xfer_partial (struct target_ops *ops,
			 enum target_object object,
			 const char *annex,
			 gdb_byte *readbuf, const gdb_byte *writebuf,
			 ULONGEST offset, LONGEST len)
{
  if (object == TARGET_OBJECT_AVAILABLE_FEATURES)
    {
      if (annex != NULL && strcmp (annex, "target.xml") == 0)
	{
	  int ret;
	  char regbuf[IWMMXT_REGS_SIZE];

	  ret = ptrace (PTRACE_GETWMMXREGS, GET_THREAD_ID (inferior_ptid),
			0, regbuf);
	  if (ret < 0)
	    arm_linux_has_wmmx_registers = 0;
	  else
	    arm_linux_has_wmmx_registers = 1;

	  if (arm_linux_has_wmmx_registers)
	    annex = "arm-with-iwmmxt.xml";
	  else
	    return -1;
	}

      return xml_builtin_xfer_partial (annex, readbuf, writebuf, offset, len);
    }

  return super_xfer_partial (ops, object, annex, readbuf, writebuf,
			     offset, len);
}

void _initialize_arm_linux_nat (void);

void
_initialize_arm_linux_nat (void)
{
  struct target_ops *t;

  os_version = get_linux_version (&os_major, &os_minor, &os_release);

  /* Fill in the generic GNU/Linux methods.  */
  t = linux_target ();

  /* Add our register access methods.  */
  t->to_fetch_registers = arm_linux_fetch_inferior_registers;
  t->to_store_registers = arm_linux_store_inferior_registers;

  /* Override the default to_xfer_partial.  */
  super_xfer_partial = t->to_xfer_partial;
  t->to_xfer_partial = arm_linux_xfer_partial;

  /* Register the target.  */
  linux_nat_add_target (t);
}
