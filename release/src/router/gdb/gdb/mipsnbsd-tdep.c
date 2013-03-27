/* Target-dependent code for NetBSD/mips.

   Copyright (C) 2002, 2003, 2004, 2006, 2007 Free Software Foundation, Inc.

   Contributed by Wasabi Systems, Inc.

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
#include "gdbcore.h"
#include "regcache.h"
#include "regset.h"
#include "target.h"
#include "value.h"
#include "osabi.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "nbsd-tdep.h"
#include "mipsnbsd-tdep.h"
#include "mips-tdep.h"

#include "solib-svr4.h"

/* Shorthand for some register numbers used below.  */
#define MIPS_PC_REGNUM  MIPS_EMBED_PC_REGNUM
#define MIPS_FP0_REGNUM MIPS_EMBED_FP0_REGNUM
#define MIPS_FSR_REGNUM MIPS_EMBED_FP0_REGNUM + 32

/* Core file support.  */

/* Number of registers in `struct reg' from <machine/reg.h>.  */
#define MIPSNBSD_NUM_GREGS	38

/* Number of registers in `struct fpreg' from <machine/reg.h>.  */
#define MIPSNBSD_NUM_FPREGS	33

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
mipsnbsd_supply_fpregset (const struct regset *regset,
			  struct regcache *regcache,
			  int regnum, const void *fpregs, size_t len)
{
  size_t regsize = mips_isa_regsize (get_regcache_arch (regcache));
  const char *regs = fpregs;
  int i;

  gdb_assert (len >= MIPSNBSD_NUM_FPREGS * regsize);

  for (i = MIPS_FP0_REGNUM; i <= MIPS_FSR_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i,
			     regs + (i - MIPS_FP0_REGNUM) * regsize);
    }
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
mipsnbsd_supply_gregset (const struct regset *regset,
			 struct regcache *regcache, int regnum,
			 const void *gregs, size_t len)
{
  size_t regsize = mips_isa_regsize (get_regcache_arch (regcache));
  const char *regs = gregs;
  int i;

  gdb_assert (len >= MIPSNBSD_NUM_GREGS * regsize);

  for (i = 0; i <= MIPS_PC_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * regsize);
    }

  if (len >= (MIPSNBSD_NUM_GREGS + MIPSNBSD_NUM_FPREGS) * regsize)
    {
      regs += MIPSNBSD_NUM_GREGS * regsize;
      len -= MIPSNBSD_NUM_GREGS * regsize;
      mipsnbsd_supply_fpregset (regset, regcache, regnum, regs, len);
    }
}

/* NetBSD/mips register sets.  */

static struct regset mipsnbsd_gregset =
{
  NULL,
  mipsnbsd_supply_gregset
};

static struct regset mipsnbsd_fpregset =
{
  NULL,
  mipsnbsd_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
mipsnbsd_regset_from_core_section (struct gdbarch *gdbarch,
				   const char *sect_name, size_t sect_size)
{
  size_t regsize = mips_isa_regsize (gdbarch);
  
  if (strcmp (sect_name, ".reg") == 0
      && sect_size >= MIPSNBSD_NUM_GREGS * regsize)
    return &mipsnbsd_gregset;

  if (strcmp (sect_name, ".reg2") == 0
      && sect_size >= MIPSNBSD_NUM_FPREGS * regsize)
    return &mipsnbsd_fpregset;

  return NULL;
}


/* Conveniently, GDB uses the same register numbering as the
   ptrace register structure used by NetBSD/mips.  */

void
mipsnbsd_supply_reg (struct regcache *regcache, const char *regs, int regno)
{
  int i;

  for (i = 0; i <= gdbarch_pc_regnum (current_gdbarch); i++)
    {
      if (regno == i || regno == -1)
	{
	  if (gdbarch_cannot_fetch_register (current_gdbarch, i))
	    regcache_raw_supply (regcache, i, NULL);
	  else
            regcache_raw_supply (regcache, i,
				 regs + (i * mips_isa_regsize (current_gdbarch)));
        }
    }
}

void
mipsnbsd_fill_reg (const struct regcache *regcache, char *regs, int regno)
{
  int i;

  for (i = 0; i <= gdbarch_pc_regnum (current_gdbarch); i++)
    if ((regno == i || regno == -1)
	&& ! gdbarch_cannot_store_register (current_gdbarch, i))
      regcache_raw_collect (regcache, i,
			    regs + (i * mips_isa_regsize (current_gdbarch)));
}

void
mipsnbsd_supply_fpreg (struct regcache *regcache, const char *fpregs, int regno)
{
  int i;

  for (i = gdbarch_fp0_regnum (current_gdbarch);
       i <= mips_regnum (current_gdbarch)->fp_implementation_revision;
       i++)
    {
      if (regno == i || regno == -1)
	{
	  if (gdbarch_cannot_fetch_register (current_gdbarch, i))
	    regcache_raw_supply (regcache, i, NULL);
	  else
            regcache_raw_supply (regcache, i,
				 fpregs 
				 + ((i - gdbarch_fp0_regnum (current_gdbarch))
				    * mips_isa_regsize (current_gdbarch)));
	}
    }
}

void
mipsnbsd_fill_fpreg (const struct regcache *regcache, char *fpregs, int regno)
{
  int i;

  for (i = gdbarch_fp0_regnum (current_gdbarch);
       i <= mips_regnum (current_gdbarch)->fp_control_status;
       i++)
    if ((regno == i || regno == -1) 
	&& ! gdbarch_cannot_store_register (current_gdbarch, i))
      regcache_raw_collect (regcache, i,
			    fpregs + ((i - gdbarch_fp0_regnum
					     (current_gdbarch))
			      * mips_isa_regsize (current_gdbarch)));
}

/* Under NetBSD/mips, signal handler invocations can be identified by the
   designated code sequence that is used to return from a signal handler.
   In particular, the return address of a signal handler points to the
   following code sequence:

	addu	a0, sp, 16
	li	v0, 295			# __sigreturn14
	syscall
   
   Each instruction has a unique encoding, so we simply attempt to match
   the instruction the PC is pointing to with any of the above instructions.
   If there is a hit, we know the offset to the start of the designated
   sequence and can then check whether we really are executing in the
   signal trampoline.  If not, -1 is returned, otherwise the offset from the
   start of the return sequence is returned.  */

#define RETCODE_NWORDS	3
#define RETCODE_SIZE	(RETCODE_NWORDS * 4)

static const unsigned char sigtramp_retcode_mipsel[RETCODE_SIZE] =
{
  0x10, 0x00, 0xa4, 0x27,	/* addu a0, sp, 16 */
  0x27, 0x01, 0x02, 0x24,	/* li v0, 295 */
  0x0c, 0x00, 0x00, 0x00,	/* syscall */
};

static const unsigned char sigtramp_retcode_mipseb[RETCODE_SIZE] =
{
  0x27, 0xa4, 0x00, 0x10,	/* addu a0, sp, 16 */
  0x24, 0x02, 0x01, 0x27,	/* li v0, 295 */
  0x00, 0x00, 0x00, 0x0c,	/* syscall */
};

static LONGEST
mipsnbsd_sigtramp_offset (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  const char *retcode = gdbarch_byte_order (current_gdbarch)
			== BFD_ENDIAN_BIG ? sigtramp_retcode_mipseb :
			sigtramp_retcode_mipsel;
  unsigned char ret[RETCODE_SIZE], w[4];
  LONGEST off;
  int i;

  if (!safe_frame_unwind_memory (next_frame, pc, w, sizeof (w)))
    return -1;

  for (i = 0; i < RETCODE_NWORDS; i++)
    {
      if (memcmp (w, retcode + (i * 4), 4) == 0)
	break;
    }
  if (i == RETCODE_NWORDS)
    return -1;

  off = i * 4;
  pc -= off;

  if (!safe_frame_unwind_memory (next_frame, pc, ret, sizeof (ret)))
    return -1;

  if (memcmp (ret, retcode, RETCODE_SIZE) == 0)
    return off;

  return -1;
}

/* Figure out where the longjmp will land.  We expect that we have
   just entered longjmp and haven't yet setup the stack frame, so the
   args are still in the argument regs.  MIPS_A0_REGNUM points at the
   jmp_buf structure from which we extract the PC that we will land
   at.  The PC is copied into *pc.  This routine returns true on
   success.  */

#define NBSD_MIPS_JB_PC			(2 * 4)
#define NBSD_MIPS_JB_ELEMENT_SIZE	mips_isa_regsize (current_gdbarch)
#define NBSD_MIPS_JB_OFFSET		(NBSD_MIPS_JB_PC * \
					 NBSD_MIPS_JB_ELEMENT_SIZE)

static int
mipsnbsd_get_longjmp_target (struct frame_info *frame, CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  char *buf;

  buf = alloca (NBSD_MIPS_JB_ELEMENT_SIZE);

  jb_addr = get_frame_register_unsigned (frame, MIPS_A0_REGNUM);

  if (target_read_memory (jb_addr + NBSD_MIPS_JB_OFFSET, buf,
  			  NBSD_MIPS_JB_ELEMENT_SIZE))
    return 0;

  *pc = extract_unsigned_integer (buf, NBSD_MIPS_JB_ELEMENT_SIZE);

  return 1;
}

static int
mipsnbsd_cannot_fetch_register (int regno)
{
  return (regno == MIPS_ZERO_REGNUM
	  || regno == mips_regnum (current_gdbarch)->fp_implementation_revision);
}

static int
mipsnbsd_cannot_store_register (int regno)
{
  return (regno == MIPS_ZERO_REGNUM
	  || regno == mips_regnum (current_gdbarch)->fp_implementation_revision);
}

/* Shared library support.  */

/* NetBSD/mips uses a slightly different `struct link_map' than the
   other NetBSD platforms.  */

static struct link_map_offsets *
mipsnbsd_ilp32_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL) 
    {
      lmp = &lmo;

      lmo.r_version_offset = 0;
      lmo.r_version_size = 4;
      lmo.r_map_offset = 4;
      lmo.r_ldsomap_offset = -1;

      /* Everything we need is in the first 24 bytes.  */
      lmo.link_map_size = 24;
      lmo.l_addr_offset = 4;
      lmo.l_name_offset = 8;
      lmo.l_ld_offset = 12;
      lmo.l_next_offset = 16;
      lmo.l_prev_offset = 20;
    }

  return lmp;
}

static struct link_map_offsets *
mipsnbsd_lp64_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    {
      lmp = &lmo;

      lmo.r_version_offset = 0;
      lmo.r_version_size = 4;
      lmo.r_map_offset = 8;
      lmo.r_ldsomap_offset = -1;

      /* Everything we need is in the first 40 bytes.  */
      lmo.link_map_size = 48;
      lmo.l_addr_offset = 0;
      lmo.l_name_offset = 16; 
      lmo.l_ld_offset = 24;
      lmo.l_next_offset = 32;
      lmo.l_prev_offset = 40;
    }

  return lmp;
}


static void
mipsnbsd_init_abi (struct gdbarch_info info,
                   struct gdbarch *gdbarch)
{
  set_gdbarch_regset_from_core_section
    (gdbarch, mipsnbsd_regset_from_core_section);

  set_gdbarch_get_longjmp_target (gdbarch, mipsnbsd_get_longjmp_target);

  set_gdbarch_cannot_fetch_register (gdbarch, mipsnbsd_cannot_fetch_register);
  set_gdbarch_cannot_store_register (gdbarch, mipsnbsd_cannot_store_register);

  set_gdbarch_software_single_step (gdbarch, mips_software_single_step);

  /* NetBSD/mips has SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, (gdbarch_ptr_bit (gdbarch) == 32 ?
	       mipsnbsd_ilp32_fetch_link_map_offsets :
	       mipsnbsd_lp64_fetch_link_map_offsets));
}


static enum gdb_osabi
mipsnbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_ELF;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_mipsnbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_mips, 0, GDB_OSABI_NETBSD_ELF,
			  mipsnbsd_init_abi);
}
