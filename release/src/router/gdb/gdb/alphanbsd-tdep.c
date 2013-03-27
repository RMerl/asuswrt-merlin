/* Target-dependent code for NetBSD/alpha.

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
#include "frame.h"
#include "gdbcore.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "value.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "alpha-tdep.h"
#include "alphabsd-tdep.h"
#include "nbsd-tdep.h"
#include "solib-svr4.h"

/* Core file support.  */

/* Even though NetBSD/alpha used ELF since day one, it used the
   traditional a.out-style core dump format before NetBSD 1.6.  */

/* Sizeof `struct reg' in <machine/reg.h>.  */
#define ALPHANBSD_SIZEOF_GREGS	(32 * 8)

/* Sizeof `struct fpreg' in <machine/reg.h.  */
#define ALPHANBSD_SIZEOF_FPREGS	((32 * 8) + 8)

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
alphanbsd_supply_fpregset (const struct regset *regset,
			   struct regcache *regcache,
			   int regnum, const void *fpregs, size_t len)
{
  const gdb_byte *regs = fpregs;
  int i;

  gdb_assert (len >= ALPHANBSD_SIZEOF_FPREGS);

  for (i = ALPHA_FP0_REGNUM; i < ALPHA_FP0_REGNUM + 31; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + (i - ALPHA_FP0_REGNUM) * 8);
    }

  if (regnum == ALPHA_FPCR_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_FPCR_REGNUM, regs + 32 * 8);
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
alphanbsd_supply_gregset (const struct regset *regset,
			  struct regcache *regcache,
			  int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  gdb_assert (len >= ALPHANBSD_SIZEOF_GREGS);

  for (i = 0; i < ALPHA_ZERO_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 8);
    }

  if (regnum == ALPHA_PC_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_PC_REGNUM, regs + 31 * 8);
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
alphanbsd_aout_supply_gregset (const struct regset *regset,
			       struct regcache *regcache,
			       int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  /* Table to map a GDB register number to a trapframe register index.  */
  static const int regmap[] =
  {
     0,   1,   2,   3,
     4,   5,   6,   7,
     8,   9,  10,  11,
    12,  13,  14,  15, 
    30,  31,  32,  16, 
    17,  18,  19,  20,
    21,  22,  23,  24,
    25,  29,  26
  };

  gdb_assert (len >= ALPHANBSD_SIZEOF_GREGS);

  for (i = 0; i < ARRAY_SIZE(regmap); i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + regmap[i] * 8);
    }

  if (regnum == ALPHA_PC_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_PC_REGNUM, regs + 31 * 8);

  if (len >= ALPHANBSD_SIZEOF_GREGS + ALPHANBSD_SIZEOF_FPREGS)
    {
      regs += ALPHANBSD_SIZEOF_GREGS;
      len -= ALPHANBSD_SIZEOF_GREGS;
      alphanbsd_supply_fpregset (regset, regcache, regnum, regs, len);
    }
}

/* NetBSD/alpha register sets.  */

static struct regset alphanbsd_gregset =
{
  NULL,
  alphanbsd_supply_gregset
};

static struct regset alphanbsd_fpregset =
{
  NULL,
  alphanbsd_supply_fpregset
};

static struct regset alphanbsd_aout_gregset =
{
  NULL,
  alphanbsd_aout_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

const struct regset *
alphanbsd_regset_from_core_section (struct gdbarch *gdbarch,
				    const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= ALPHANBSD_SIZEOF_GREGS)
    {
      if (sect_size >= ALPHANBSD_SIZEOF_GREGS + ALPHANBSD_SIZEOF_FPREGS)
	return &alphanbsd_aout_gregset;
      else
	return &alphanbsd_gregset;
    }

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= ALPHANBSD_SIZEOF_FPREGS)
    return &alphanbsd_fpregset;

  return NULL;
}


/* Signal trampolines.  */

/* Under NetBSD/alpha, signal handler invocations can be identified by the
   designated code sequence that is used to return from a signal handler.
   In particular, the return address of a signal handler points to the
   following code sequence:

	ldq	a0, 0(sp)
	lda	sp, 16(sp)
	lda	v0, 295(zero)	# __sigreturn14
	call_pal callsys

   Each instruction has a unique encoding, so we simply attempt to match
   the instruction the PC is pointing to with any of the above instructions.
   If there is a hit, we know the offset to the start of the designated
   sequence and can then check whether we really are executing in the
   signal trampoline.  If not, -1 is returned, otherwise the offset from the
   start of the return sequence is returned.  */
static const unsigned char sigtramp_retcode[] =
{
  0x00, 0x00, 0x1e, 0xa6,	/* ldq a0, 0(sp) */
  0x10, 0x00, 0xde, 0x23,	/* lda sp, 16(sp) */
  0x27, 0x01, 0x1f, 0x20,	/* lda v0, 295(zero) */
  0x83, 0x00, 0x00, 0x00,	/* call_pal callsys */
};
#define RETCODE_NWORDS		4
#define RETCODE_SIZE		(RETCODE_NWORDS * 4)

static LONGEST
alphanbsd_sigtramp_offset (CORE_ADDR pc)
{
  unsigned char ret[RETCODE_SIZE], w[4];
  LONGEST off;
  int i;

  if (read_memory_nobpt (pc, (char *) w, 4) != 0)
    return -1;

  for (i = 0; i < RETCODE_NWORDS; i++)
    {
      if (memcmp (w, sigtramp_retcode + (i * 4), 4) == 0)
	break;
    }
  if (i == RETCODE_NWORDS)
    return (-1);

  off = i * 4;
  pc -= off;

  if (read_memory_nobpt (pc, (char *) ret, sizeof (ret)) != 0)
    return -1;

  if (memcmp (ret, sigtramp_retcode, RETCODE_SIZE) == 0)
    return off;

  return -1;
}

static int
alphanbsd_pc_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  return (nbsd_pc_in_sigtramp (pc, func_name)
	  || alphanbsd_sigtramp_offset (pc) >= 0);
}

static CORE_ADDR
alphanbsd_sigcontext_addr (struct frame_info *frame)
{
  /* FIXME: This is not correct for all versions of NetBSD/alpha.
     We will probably need to disassemble the trampoline to figure
     out which trampoline frame type we have.  */
  return get_frame_base (frame);
}


static void
alphanbsd_init_abi (struct gdbarch_info info,
                    struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Hook into the DWARF CFI frame unwinder.  */
  alpha_dwarf2_init_abi (info, gdbarch);

  /* Hook into the MDEBUG frame unwinder.  */
  alpha_mdebug_init_abi (info, gdbarch);

  /* NetBSD/alpha does not provide single step support via ptrace(2); we
     must use software single-stepping.  */
  set_gdbarch_software_single_step (gdbarch, alpha_software_single_step);

  /* NetBSD/alpha has SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);

  tdep->dynamic_sigtramp_offset = alphanbsd_sigtramp_offset;
  tdep->pc_in_sigtramp = alphanbsd_pc_in_sigtramp;
  tdep->sigcontext_addr = alphanbsd_sigcontext_addr;

  tdep->jb_pc = 2;
  tdep->jb_elt_size = 8;

  set_gdbarch_regset_from_core_section
    (gdbarch, alphanbsd_regset_from_core_section);
}


static enum gdb_osabi
alphanbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_ELF;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_alphanbsd_tdep (void);

void
_initialize_alphanbsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_alpha, bfd_target_unknown_flavour,
                                  alphanbsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_alpha, 0, GDB_OSABI_NETBSD_ELF,
                          alphanbsd_init_abi);
}
