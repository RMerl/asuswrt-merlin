/* PPC GNU/Linux native support.

   Copyright (C) 1988, 1989, 1991, 1992, 1994, 1996, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "target.h"
#include "linux-nat.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include "gdb_wait.h"
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"
#include "ppc-tdep.h"

/* Glibc's headers don't define PTRACE_GETVRREGS so we cannot use a
   configure time check.  Some older glibc's (for instance 2.2.1)
   don't have a specific powerpc version of ptrace.h, and fall back on
   a generic one.  In such cases, sys/ptrace.h defines
   PTRACE_GETFPXREGS and PTRACE_SETFPXREGS to the same numbers that
   ppc kernel's asm/ptrace.h defines PTRACE_GETVRREGS and
   PTRACE_SETVRREGS to be.  This also makes a configury check pretty
   much useless.  */

/* These definitions should really come from the glibc header files,
   but Glibc doesn't know about the vrregs yet.  */
#ifndef PTRACE_GETVRREGS
#define PTRACE_GETVRREGS 18
#define PTRACE_SETVRREGS 19
#endif


/* Similarly for the ptrace requests for getting / setting the SPE
   registers (ev0 -- ev31, acc, and spefscr).  See the description of
   gdb_evrregset_t for details.  */
#ifndef PTRACE_GETEVRREGS
#define PTRACE_GETEVRREGS 20
#define PTRACE_SETEVRREGS 21
#endif

/* Similarly for the hardware watchpoint support.  */
#ifndef PTRACE_GET_DEBUGREG
#define PTRACE_GET_DEBUGREG    25
#endif
#ifndef PTRACE_SET_DEBUGREG
#define PTRACE_SET_DEBUGREG    26
#endif
#ifndef PTRACE_GETSIGINFO
#define PTRACE_GETSIGINFO    0x4202
#endif

/* This oddity is because the Linux kernel defines elf_vrregset_t as
   an array of 33 16 bytes long elements.  I.e. it leaves out vrsave.
   However the PTRACE_GETVRREGS and PTRACE_SETVRREGS requests return
   the vrsave as an extra 4 bytes at the end.  I opted for creating a
   flat array of chars, so that it is easier to manipulate for gdb.

   There are 32 vector registers 16 bytes longs, plus a VSCR register
   which is only 4 bytes long, but is fetched as a 16 bytes
   quantity. Up to here we have the elf_vrregset_t structure.
   Appended to this there is space for the VRSAVE register: 4 bytes.
   Even though this vrsave register is not included in the regset
   typedef, it is handled by the ptrace requests.

   Note that GNU/Linux doesn't support little endian PPC hardware,
   therefore the offset at which the real value of the VSCR register
   is located will be always 12 bytes.

   The layout is like this (where x is the actual value of the vscr reg): */

/* *INDENT-OFF* */
/*
   |.|.|.|.|.....|.|.|.|.||.|.|.|x||.|
   <------->     <-------><-------><->
     VR0           VR31     VSCR    VRSAVE
*/
/* *INDENT-ON* */

#define SIZEOF_VRREGS 33*16+4

typedef char gdb_vrregset_t[SIZEOF_VRREGS];


/* On PPC processors that support the the Signal Processing Extension
   (SPE) APU, the general-purpose registers are 64 bits long.
   However, the ordinary Linux kernel PTRACE_PEEKUSER / PTRACE_POKEUSER
   ptrace calls only access the lower half of each register, to allow
   them to behave the same way they do on non-SPE systems.  There's a
   separate pair of calls, PTRACE_GETEVRREGS / PTRACE_SETEVRREGS, that
   read and write the top halves of all the general-purpose registers
   at once, along with some SPE-specific registers.

   GDB itself continues to claim the general-purpose registers are 32
   bits long.  It has unnamed raw registers that hold the upper halves
   of the gprs, and the the full 64-bit SIMD views of the registers,
   'ev0' -- 'ev31', are pseudo-registers that splice the top and
   bottom halves together.

   This is the structure filled in by PTRACE_GETEVRREGS and written to
   the inferior's registers by PTRACE_SETEVRREGS.  */
struct gdb_evrregset_t
{
  unsigned long evr[32];
  unsigned long long acc;
  unsigned long spefscr;
};


/* Non-zero if our kernel may support the PTRACE_GETVRREGS and
   PTRACE_SETVRREGS requests, for reading and writing the Altivec
   registers.  Zero if we've tried one of them and gotten an
   error.  */
int have_ptrace_getvrregs = 1;

static CORE_ADDR last_stopped_data_address = 0;

/* Non-zero if our kernel may support the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests, for reading and writing the SPE
   registers.  Zero if we've tried one of them and gotten an
   error.  */
int have_ptrace_getsetevrregs = 1;

/* *INDENT-OFF* */
/* registers layout, as presented by the ptrace interface:
PT_R0, PT_R1, PT_R2, PT_R3, PT_R4, PT_R5, PT_R6, PT_R7,
PT_R8, PT_R9, PT_R10, PT_R11, PT_R12, PT_R13, PT_R14, PT_R15,
PT_R16, PT_R17, PT_R18, PT_R19, PT_R20, PT_R21, PT_R22, PT_R23,
PT_R24, PT_R25, PT_R26, PT_R27, PT_R28, PT_R29, PT_R30, PT_R31,
PT_FPR0, PT_FPR0 + 2, PT_FPR0 + 4, PT_FPR0 + 6, PT_FPR0 + 8, PT_FPR0 + 10, PT_FPR0 + 12, PT_FPR0 + 14,
PT_FPR0 + 16, PT_FPR0 + 18, PT_FPR0 + 20, PT_FPR0 + 22, PT_FPR0 + 24, PT_FPR0 + 26, PT_FPR0 + 28, PT_FPR0 + 30,
PT_FPR0 + 32, PT_FPR0 + 34, PT_FPR0 + 36, PT_FPR0 + 38, PT_FPR0 + 40, PT_FPR0 + 42, PT_FPR0 + 44, PT_FPR0 + 46,
PT_FPR0 + 48, PT_FPR0 + 50, PT_FPR0 + 52, PT_FPR0 + 54, PT_FPR0 + 56, PT_FPR0 + 58, PT_FPR0 + 60, PT_FPR0 + 62,
PT_NIP, PT_MSR, PT_CCR, PT_LNK, PT_CTR, PT_XER, PT_MQ */
/* *INDENT_ON * */

static int
ppc_register_u_addr (int regno)
{
  int u_addr = -1;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* NOTE: cagney/2003-11-25: This is the word size used by the ptrace
     interface, and not the wordsize of the program's ABI.  */
  int wordsize = sizeof (long);

  /* General purpose registers occupy 1 slot each in the buffer */
  if (regno >= tdep->ppc_gp0_regnum 
      && regno < tdep->ppc_gp0_regnum + ppc_num_gprs)
    u_addr = ((regno - tdep->ppc_gp0_regnum + PT_R0) * wordsize);

  /* Floating point regs: eight bytes each in both 32- and 64-bit
     ptrace interfaces.  Thus, two slots each in 32-bit interface, one
     slot each in 64-bit interface.  */
  if (tdep->ppc_fp0_regnum >= 0
      && regno >= tdep->ppc_fp0_regnum
      && regno < tdep->ppc_fp0_regnum + ppc_num_fprs)
    u_addr = (PT_FPR0 * wordsize) + ((regno - tdep->ppc_fp0_regnum) * 8);

  /* UISA special purpose registers: 1 slot each */
  if (regno == gdbarch_pc_regnum (current_gdbarch))
    u_addr = PT_NIP * wordsize;
  if (regno == tdep->ppc_lr_regnum)
    u_addr = PT_LNK * wordsize;
  if (regno == tdep->ppc_cr_regnum)
    u_addr = PT_CCR * wordsize;
  if (regno == tdep->ppc_xer_regnum)
    u_addr = PT_XER * wordsize;
  if (regno == tdep->ppc_ctr_regnum)
    u_addr = PT_CTR * wordsize;
#ifdef PT_MQ
  if (regno == tdep->ppc_mq_regnum)
    u_addr = PT_MQ * wordsize;
#endif
  if (regno == tdep->ppc_ps_regnum)
    u_addr = PT_MSR * wordsize;
  if (tdep->ppc_fpscr_regnum >= 0
      && regno == tdep->ppc_fpscr_regnum)
    {
      /* NOTE: cagney/2005-02-08: On some 64-bit GNU/Linux systems the
	 kernel headers incorrectly contained the 32-bit definition of
	 PT_FPSCR.  For the 32-bit definition, floating-point
	 registers occupy two 32-bit "slots", and the FPSCR lives in
	 the secondhalf of such a slot-pair (hence +1).  For 64-bit,
	 the FPSCR instead occupies the full 64-bit 2-word-slot and
	 hence no adjustment is necessary.  Hack around this.  */
      if (wordsize == 8 && PT_FPSCR == (48 + 32 + 1))
	u_addr = (48 + 32) * wordsize;
      else
	u_addr = PT_FPSCR * wordsize;
    }
  return u_addr;
}

/* The Linux kernel ptrace interface for AltiVec registers uses the
   registers set mechanism, as opposed to the interface for all the
   other registers, that stores/fetches each register individually.  */
static void
fetch_altivec_register (struct regcache *regcache, int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name (_("Unable to fetch AltiVec register"));
    }
 
  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  We deal only with the lower 4 bytes of the
     vector.  VRSAVE is at the end of the array in a 4 bytes slot, so
     there is no need to define an offset for it.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);
  
  regcache_raw_supply (regcache, regno,
		       regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);
}

/* Fetch the top 32 bits of TID's general-purpose registers and the
   SPE-specific registers, and place the results in EVRREGSET.  If we
   don't support PTRACE_GETEVRREGS, then just fill EVRREGSET with
   zeros.

   All the logic to deal with whether or not the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests are supported is isolated here, and in
   set_spe_registers.  */
static void
get_spe_registers (int tid, struct gdb_evrregset_t *evrregset)
{
  if (have_ptrace_getsetevrregs)
    {
      if (ptrace (PTRACE_GETEVRREGS, tid, 0, evrregset) >= 0)
        return;
      else
        {
          /* EIO means that the PTRACE_GETEVRREGS request isn't supported;
             we just return zeros.  */
          if (errno == EIO)
            have_ptrace_getsetevrregs = 0;
          else
            /* Anything else needs to be reported.  */
            perror_with_name (_("Unable to fetch SPE registers"));
        }
    }

  memset (evrregset, 0, sizeof (*evrregset));
}

/* Supply values from TID for SPE-specific raw registers: the upper
   halves of the GPRs, the accumulator, and the spefscr.  REGNO must
   be the number of an upper half register, acc, spefscr, or -1 to
   supply the values of all registers.  */
static void
fetch_spe_register (struct regcache *regcache, int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct gdb_evrregset_t evrregs;

  gdb_assert (sizeof (evrregs.evr[0])
              == register_size (current_gdbarch, tdep->ppc_ev0_upper_regnum));
  gdb_assert (sizeof (evrregs.acc)
              == register_size (current_gdbarch, tdep->ppc_acc_regnum));
  gdb_assert (sizeof (evrregs.spefscr)
              == register_size (current_gdbarch, tdep->ppc_spefscr_regnum));

  get_spe_registers (tid, &evrregs);

  if (regno == -1)
    {
      int i;

      for (i = 0; i < ppc_num_gprs; i++)
        regcache_raw_supply (regcache, tdep->ppc_ev0_upper_regnum + i,
                             &evrregs.evr[i]);
    }
  else if (tdep->ppc_ev0_upper_regnum <= regno
           && regno < tdep->ppc_ev0_upper_regnum + ppc_num_gprs)
    regcache_raw_supply (regcache, regno,
                         &evrregs.evr[regno - tdep->ppc_ev0_upper_regnum]);

  if (regno == -1
      || regno == tdep->ppc_acc_regnum)
    regcache_raw_supply (regcache, tdep->ppc_acc_regnum, &evrregs.acc);

  if (regno == -1
      || regno == tdep->ppc_spefscr_regnum)
    regcache_raw_supply (regcache, tdep->ppc_spefscr_regnum,
                         &evrregs.spefscr);
}

static void
fetch_register (struct regcache *regcache, int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr = ppc_register_u_addr (regno);
  int bytes_transferred;
  unsigned int offset;         /* Offset of registers within the u area. */
  char buf[MAX_REGISTER_SIZE];

  if (altivec_register_p (regno))
    {
      /* If this is the first time through, or if it is not the first
         time through, and we have comfirmed that there is kernel
         support for such a ptrace request, then go and fetch the
         register.  */
      if (have_ptrace_getvrregs)
       {
         fetch_altivec_register (regcache, tid, regno);
         return;
       }
     /* If we have discovered that there is no ptrace support for
        AltiVec registers, fall through and return zeroes, because
        regaddr will be -1 in this case.  */
    }
  else if (spe_register_p (regno))
    {
      fetch_spe_register (regcache, tid, regno);
      return;
    }

  if (regaddr == -1)
    {
      memset (buf, '\0', register_size (current_gdbarch, regno));   /* Supply zeroes */
      regcache_raw_supply (regcache, regno, buf);
      return;
    }

  /* Read the raw register using sizeof(long) sized chunks.  On a
     32-bit platform, 64-bit floating-point registers will require two
     transfers.  */
  for (bytes_transferred = 0;
       bytes_transferred < register_size (current_gdbarch, regno);
       bytes_transferred += sizeof (long))
    {
      errno = 0;
      *(long *) &buf[bytes_transferred]
        = ptrace (PTRACE_PEEKUSER, tid, (PTRACE_TYPE_ARG3) regaddr, 0);
      regaddr += sizeof (long);
      if (errno != 0)
	{
          char message[128];
	  sprintf (message, "reading register %s (#%d)", 
		   gdbarch_register_name (current_gdbarch, regno), regno);
	  perror_with_name (message);
	}
    }

  /* Now supply the register.  Keep in mind that the regcache's idea
     of the register's size may not be a multiple of sizeof
     (long).  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
    {
      /* Little-endian values are always found at the left end of the
         bytes transferred.  */
      regcache_raw_supply (regcache, regno, buf);
    }
  else if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      /* Big-endian values are found at the right end of the bytes
         transferred.  */
      size_t padding = (bytes_transferred
                        - register_size (current_gdbarch, regno));
      regcache_raw_supply (regcache, regno, buf + padding);
    }
  else 
    internal_error (__FILE__, __LINE__,
                    _("fetch_register: unexpected byte order: %d"),
                    gdbarch_byte_order (current_gdbarch));
}

static void
supply_vrregset (struct regcache *regcache, gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);
  int offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128.  However an offset is necessary only for VSCR because it
         occupies a whole vector, while VRSAVE occupies a full 4 bytes
         slot.  */
      if (i == (num_of_vrregs - 2))
        regcache_raw_supply (regcache, tdep->ppc_vr0_regnum + i,
			     *vrregsetp + i * vrregsize + offset);
      else
        regcache_raw_supply (regcache, tdep->ppc_vr0_regnum + i,
			     *vrregsetp + i * vrregsize);
    }
}

static void
fetch_altivec_registers (struct regcache *regcache, int tid)
{
  int ret;
  gdb_vrregset_t regs;
  
  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
	{
          have_ptrace_getvrregs = 0;
	  return;
	}
      perror_with_name (_("Unable to fetch AltiVec registers"));
    }
  supply_vrregset (regcache, &regs);
}

static void 
fetch_ppc_registers (struct regcache *regcache, int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  for (i = 0; i < ppc_num_gprs; i++)
    fetch_register (regcache, tid, tdep->ppc_gp0_regnum + i);
  if (tdep->ppc_fp0_regnum >= 0)
    for (i = 0; i < ppc_num_fprs; i++)
      fetch_register (regcache, tid, tdep->ppc_fp0_regnum + i);
  fetch_register (regcache, tid, gdbarch_pc_regnum (current_gdbarch));
  if (tdep->ppc_ps_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_ps_regnum);
  if (tdep->ppc_cr_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_cr_regnum);
  if (tdep->ppc_lr_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_lr_regnum);
  if (tdep->ppc_ctr_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_ctr_regnum);
  if (tdep->ppc_xer_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_xer_regnum);
  if (tdep->ppc_mq_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_mq_regnum);
  if (tdep->ppc_fpscr_regnum != -1)
    fetch_register (regcache, tid, tdep->ppc_fpscr_regnum);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      fetch_altivec_registers (regcache, tid);
  if (tdep->ppc_ev0_upper_regnum >= 0)
    fetch_spe_register (regcache, tid, -1);
}

/* Fetch registers from the child process.  Fetch all registers if
   regno == -1, otherwise fetch all general registers or all floating
   point registers depending upon the value of regno.  */
static void
ppc_linux_fetch_inferior_registers (struct regcache *regcache, int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno == -1)
    fetch_ppc_registers (regcache, tid);
  else 
    fetch_register (regcache, tid, regno);
}

/* Store one register. */
static void
store_altivec_register (const struct regcache *regcache, int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name (_("Unable to fetch AltiVec register"));
    }

  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  regcache_raw_collect (regcache, regno,
			regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);

  ret = ptrace (PTRACE_SETVRREGS, tid, 0, &regs);
  if (ret < 0)
    perror_with_name (_("Unable to store AltiVec register"));
}

/* Assuming TID referrs to an SPE process, set the top halves of TID's
   general-purpose registers and its SPE-specific registers to the
   values in EVRREGSET.  If we don't support PTRACE_SETEVRREGS, do
   nothing.

   All the logic to deal with whether or not the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests are supported is isolated here, and in
   get_spe_registers.  */
static void
set_spe_registers (int tid, struct gdb_evrregset_t *evrregset)
{
  if (have_ptrace_getsetevrregs)
    {
      if (ptrace (PTRACE_SETEVRREGS, tid, 0, evrregset) >= 0)
        return;
      else
        {
          /* EIO means that the PTRACE_SETEVRREGS request isn't
             supported; we fail silently, and don't try the call
             again.  */
          if (errno == EIO)
            have_ptrace_getsetevrregs = 0;
          else
            /* Anything else needs to be reported.  */
            perror_with_name (_("Unable to set SPE registers"));
        }
    }
}

/* Write GDB's value for the SPE-specific raw register REGNO to TID.
   If REGNO is -1, write the values of all the SPE-specific
   registers.  */
static void
store_spe_register (const struct regcache *regcache, int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct gdb_evrregset_t evrregs;

  gdb_assert (sizeof (evrregs.evr[0])
              == register_size (current_gdbarch, tdep->ppc_ev0_upper_regnum));
  gdb_assert (sizeof (evrregs.acc)
              == register_size (current_gdbarch, tdep->ppc_acc_regnum));
  gdb_assert (sizeof (evrregs.spefscr)
              == register_size (current_gdbarch, tdep->ppc_spefscr_regnum));

  if (regno == -1)
    /* Since we're going to write out every register, the code below
       should store to every field of evrregs; if that doesn't happen,
       make it obvious by initializing it with suspicious values.  */
    memset (&evrregs, 42, sizeof (evrregs));
  else
    /* We can only read and write the entire EVR register set at a
       time, so to write just a single register, we do a
       read-modify-write maneuver.  */
    get_spe_registers (tid, &evrregs);

  if (regno == -1)
    {
      int i;

      for (i = 0; i < ppc_num_gprs; i++)
        regcache_raw_collect (regcache,
                              tdep->ppc_ev0_upper_regnum + i,
                              &evrregs.evr[i]);
    }
  else if (tdep->ppc_ev0_upper_regnum <= regno
           && regno < tdep->ppc_ev0_upper_regnum + ppc_num_gprs)
    regcache_raw_collect (regcache, regno,
                          &evrregs.evr[regno - tdep->ppc_ev0_upper_regnum]);

  if (regno == -1
      || regno == tdep->ppc_acc_regnum)
    regcache_raw_collect (regcache,
                          tdep->ppc_acc_regnum,
                          &evrregs.acc);

  if (regno == -1
      || regno == tdep->ppc_spefscr_regnum)
    regcache_raw_collect (regcache,
                          tdep->ppc_spefscr_regnum,
                          &evrregs.spefscr);

  /* Write back the modified register set.  */
  set_spe_registers (tid, &evrregs);
}

static void
store_register (const struct regcache *regcache, int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr = ppc_register_u_addr (regno);
  int i;
  size_t bytes_to_transfer;
  char buf[MAX_REGISTER_SIZE];

  if (altivec_register_p (regno))
    {
      store_altivec_register (regcache, tid, regno);
      return;
    }
  else if (spe_register_p (regno))
    {
      store_spe_register (regcache, tid, regno);
      return;
    }

  if (regaddr == -1)
    return;

  /* First collect the register.  Keep in mind that the regcache's
     idea of the register's size may not be a multiple of sizeof
     (long).  */
  memset (buf, 0, sizeof buf);
  bytes_to_transfer = align_up (register_size (current_gdbarch, regno),
                                sizeof (long));
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
    {
      /* Little-endian values always sit at the left end of the buffer.  */
      regcache_raw_collect (regcache, regno, buf);
    }
  else if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      /* Big-endian values sit at the right end of the buffer.  */
      size_t padding = (bytes_to_transfer
                        - register_size (current_gdbarch, regno));
      regcache_raw_collect (regcache, regno, buf + padding);
    }

  for (i = 0; i < bytes_to_transfer; i += sizeof (long))
    {
      errno = 0;
      ptrace (PTRACE_POKEUSER, tid, (PTRACE_TYPE_ARG3) regaddr,
	      *(long *) &buf[i]);
      regaddr += sizeof (long);

      if (errno == EIO 
          && regno == tdep->ppc_fpscr_regnum)
	{
	  /* Some older kernel versions don't allow fpscr to be written.  */
	  continue;
	}

      if (errno != 0)
	{
          char message[128];
	  sprintf (message, "writing register %s (#%d)", 
		   gdbarch_register_name (current_gdbarch, regno), regno);
	  perror_with_name (message);
	}
    }
}

static void
fill_vrregset (const struct regcache *regcache, gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);
  int offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128, but only VSCR is fetched as a 16 bytes quantity.  */
      if (i == (num_of_vrregs - 2))
        regcache_raw_collect (regcache, tdep->ppc_vr0_regnum + i,
			      *vrregsetp + i * vrregsize + offset);
      else
        regcache_raw_collect (regcache, tdep->ppc_vr0_regnum + i,
			      *vrregsetp + i * vrregsize);
    }
}

static void
store_altivec_registers (const struct regcache *regcache, int tid)
{
  int ret;
  gdb_vrregset_t regs;

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name (_("Couldn't get AltiVec registers"));
    }

  fill_vrregset (regcache, &regs);
  
  if (ptrace (PTRACE_SETVRREGS, tid, 0, &regs) < 0)
    perror_with_name (_("Couldn't write AltiVec registers"));
}

static void
store_ppc_registers (const struct regcache *regcache, int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  for (i = 0; i < ppc_num_gprs; i++)
    store_register (regcache, tid, tdep->ppc_gp0_regnum + i);
  if (tdep->ppc_fp0_regnum >= 0)
    for (i = 0; i < ppc_num_fprs; i++)
      store_register (regcache, tid, tdep->ppc_fp0_regnum + i);
  store_register (regcache, tid, gdbarch_pc_regnum (current_gdbarch));
  if (tdep->ppc_ps_regnum != -1)
    store_register (regcache, tid, tdep->ppc_ps_regnum);
  if (tdep->ppc_cr_regnum != -1)
    store_register (regcache, tid, tdep->ppc_cr_regnum);
  if (tdep->ppc_lr_regnum != -1)
    store_register (regcache, tid, tdep->ppc_lr_regnum);
  if (tdep->ppc_ctr_regnum != -1)
    store_register (regcache, tid, tdep->ppc_ctr_regnum);
  if (tdep->ppc_xer_regnum != -1)
    store_register (regcache, tid, tdep->ppc_xer_regnum);
  if (tdep->ppc_mq_regnum != -1)
    store_register (regcache, tid, tdep->ppc_mq_regnum);
  if (tdep->ppc_fpscr_regnum != -1)
    store_register (regcache, tid, tdep->ppc_fpscr_regnum);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      store_altivec_registers (regcache, tid);
  if (tdep->ppc_ev0_upper_regnum >= 0)
    store_spe_register (regcache, tid, -1);
}

static int
ppc_linux_check_watch_resources (int type, int cnt, int ot)
{
  int tid;
  ptid_t ptid = inferior_ptid;

  /* DABR (data address breakpoint register) is optional for PPC variants.
     Some variants have one DABR, others have none.  So CNT can't be larger
     than 1.  */
  if (cnt > 1)
    return 0;

  /* We need to know whether ptrace supports PTRACE_SET_DEBUGREG and whether
     the target has DABR.  If either answer is no, the ptrace call will
     return -1.  Fail in that case.  */
  tid = TIDGET (ptid);
  if (tid == 0)
    tid = PIDGET (ptid);

  if (ptrace (PTRACE_SET_DEBUGREG, tid, 0, 0) == -1)
    return 0;
  return 1;
}

static int
ppc_linux_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len)
{
  /* Handle sub-8-byte quantities.  */
  if (len <= 0)
    return 0;

  /* addr+len must fall in the 8 byte watchable region.  */
  if ((addr + len) > (addr & ~7) + 8)
    return 0;

  return 1;
}

/* Set a watchpoint of type TYPE at address ADDR.  */
static int
ppc_linux_insert_watchpoint (CORE_ADDR addr, int len, int rw)
{
  int tid;
  long dabr_value;
  ptid_t ptid = inferior_ptid;

  dabr_value = addr & ~7;
  switch (rw)
    {
    case hw_read:
      /* Set read and translate bits.  */
      dabr_value |= 5;
      break;
    case hw_write:
      /* Set write and translate bits.  */
      dabr_value |= 6;
      break;
    case hw_access:
      /* Set read, write and translate bits.  */
      dabr_value |= 7;
      break;
    }

  tid = TIDGET (ptid);
  if (tid == 0)
    tid = PIDGET (ptid);

  return ptrace (PTRACE_SET_DEBUGREG, tid, 0, dabr_value);
}

static int
ppc_linux_remove_watchpoint (CORE_ADDR addr, int len, int rw)
{
  int tid;
  ptid_t ptid = inferior_ptid;

  tid = TIDGET (ptid);
  if (tid == 0)
    tid = PIDGET (ptid);

  return ptrace (PTRACE_SET_DEBUGREG, tid, 0, 0);
}

static int
ppc_linux_stopped_data_address (struct target_ops *target, CORE_ADDR *addr_p)
{
  if (last_stopped_data_address)
    {
      *addr_p = last_stopped_data_address;
      last_stopped_data_address = 0;
      return 1;
    }
  return 0;
}

static int
ppc_linux_stopped_by_watchpoint (void)
{
  int tid;
  struct siginfo siginfo;
  ptid_t ptid = inferior_ptid;
  CORE_ADDR *addr_p;

  tid = TIDGET(ptid);
  if (tid == 0)
    tid = PIDGET (ptid);

  errno = 0;
  ptrace (PTRACE_GETSIGINFO, tid, (PTRACE_TYPE_ARG3) 0, &siginfo);

  if (errno != 0 || siginfo.si_signo != SIGTRAP ||
      (siginfo.si_code & 0xffff) != 0x0004)
    return 0;

  last_stopped_data_address = (uintptr_t) siginfo.si_addr;
  return 1;
}

static void
ppc_linux_store_inferior_registers (struct regcache *regcache, int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno >= 0)
    store_register (regcache, tid, regno);
  else
    store_ppc_registers (regcache, tid);
}

/* Functions for transferring registers between a gregset_t or fpregset_t
   (see sys/ucontext.h) and gdb's regcache.  The word size is that used
   by the ptrace interface, not the current program's ABI.  eg. If a
   powerpc64-linux gdb is being used to debug a powerpc32-linux app, we
   read or write 64-bit gregsets.  This is to suit the host libthread_db.  */

void
supply_gregset (struct regcache *regcache, const gdb_gregset_t *gregsetp)
{
  const struct regset *regset = ppc_linux_gregset (sizeof (long));

  ppc_supply_gregset (regset, regcache, -1, gregsetp, sizeof (*gregsetp));
}

void
fill_gregset (const struct regcache *regcache,
	      gdb_gregset_t *gregsetp, int regno)
{
  const struct regset *regset = ppc_linux_gregset (sizeof (long));

  if (regno == -1)
    memset (gregsetp, 0, sizeof (*gregsetp));
  ppc_collect_gregset (regset, regcache, regno, gregsetp, sizeof (*gregsetp));
}

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t * fpregsetp)
{
  const struct regset *regset = ppc_linux_fpregset ();

  ppc_supply_fpregset (regset, regcache, -1,
		       fpregsetp, sizeof (*fpregsetp));
}

void
fill_fpregset (const struct regcache *regcache,
	       gdb_fpregset_t *fpregsetp, int regno)
{
  const struct regset *regset = ppc_linux_fpregset ();

  ppc_collect_fpregset (regset, regcache, regno,
			fpregsetp, sizeof (*fpregsetp));
}

void _initialize_ppc_linux_nat (void);

void
_initialize_ppc_linux_nat (void)
{
  struct target_ops *t;

  /* Fill in the generic GNU/Linux methods.  */
  t = linux_target ();

  /* Add our register access methods.  */
  t->to_fetch_registers = ppc_linux_fetch_inferior_registers;
  t->to_store_registers = ppc_linux_store_inferior_registers;

  /* Add our watchpoint methods.  */
  t->to_can_use_hw_breakpoint = ppc_linux_check_watch_resources;
  t->to_region_ok_for_hw_watchpoint = ppc_linux_region_ok_for_hw_watchpoint;
  t->to_insert_watchpoint = ppc_linux_insert_watchpoint;
  t->to_remove_watchpoint = ppc_linux_remove_watchpoint;
  t->to_stopped_by_watchpoint = ppc_linux_stopped_by_watchpoint;
  t->to_stopped_data_address = ppc_linux_stopped_data_address;

  /* Register the target.  */
  linux_nat_add_target (t);
}
