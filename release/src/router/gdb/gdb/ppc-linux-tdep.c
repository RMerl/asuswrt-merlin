/* Target-dependent code for GDB, the GNU debugger.

   Copyright (C) 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
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
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "objfiles.h"
#include "regcache.h"
#include "value.h"
#include "osabi.h"
#include "regset.h"
#include "solib-svr4.h"
#include "ppc-tdep.h"
#include "trad-frame.h"
#include "frame-unwind.h"
#include "tramp-frame.h"

static CORE_ADDR
ppc_linux_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  gdb_byte buf[4];
  struct obj_section *sect;
  struct objfile *objfile;
  unsigned long insn;
  CORE_ADDR plt_start = 0;
  CORE_ADDR symtab = 0;
  CORE_ADDR strtab = 0;
  int num_slots = -1;
  int reloc_index = -1;
  CORE_ADDR plt_table;
  CORE_ADDR reloc;
  CORE_ADDR sym;
  long symidx;
  char symname[1024];
  struct minimal_symbol *msymbol;

  /* Find the section pc is in; return if not in .plt */
  sect = find_pc_section (pc);
  if (!sect || strcmp (sect->the_bfd_section->name, ".plt") != 0)
    return 0;

  objfile = sect->objfile;

  /* Pick up the instruction at pc.  It had better be of the
     form
     li r11, IDX

     where IDX is an index into the plt_table.  */

  if (target_read_memory (pc, buf, 4) != 0)
    return 0;
  insn = extract_unsigned_integer (buf, 4);

  if ((insn & 0xffff0000) != 0x39600000 /* li r11, VAL */ )
    return 0;

  reloc_index = (insn << 16) >> 16;

  /* Find the objfile that pc is in and obtain the information
     necessary for finding the symbol name. */
  for (sect = objfile->sections; sect < objfile->sections_end; ++sect)
    {
      const char *secname = sect->the_bfd_section->name;
      if (strcmp (secname, ".plt") == 0)
	plt_start = sect->addr;
      else if (strcmp (secname, ".rela.plt") == 0)
	num_slots = ((int) sect->endaddr - (int) sect->addr) / 12;
      else if (strcmp (secname, ".dynsym") == 0)
	symtab = sect->addr;
      else if (strcmp (secname, ".dynstr") == 0)
	strtab = sect->addr;
    }

  /* Make sure we have all the information we need. */
  if (plt_start == 0 || num_slots == -1 || symtab == 0 || strtab == 0)
    return 0;

  /* Compute the value of the plt table */
  plt_table = plt_start + 72 + 8 * num_slots;

  /* Get address of the relocation entry (Elf32_Rela) */
  if (target_read_memory (plt_table + reloc_index, buf, 4) != 0)
    return 0;
  reloc = extract_unsigned_integer (buf, 4);

  sect = find_pc_section (reloc);
  if (!sect)
    return 0;

  if (strcmp (sect->the_bfd_section->name, ".text") == 0)
    return reloc;

  /* Now get the r_info field which is the relocation type and symbol
     index. */
  if (target_read_memory (reloc + 4, buf, 4) != 0)
    return 0;
  symidx = extract_unsigned_integer (buf, 4);

  /* Shift out the relocation type leaving just the symbol index */
  /* symidx = ELF32_R_SYM(symidx); */
  symidx = symidx >> 8;

  /* compute the address of the symbol */
  sym = symtab + symidx * 4;

  /* Fetch the string table index */
  if (target_read_memory (sym, buf, 4) != 0)
    return 0;
  symidx = extract_unsigned_integer (buf, 4);

  /* Fetch the string; we don't know how long it is.  Is it possible
     that the following will fail because we're trying to fetch too
     much? */
  if (target_read_memory (strtab + symidx, (gdb_byte *) symname,
			  sizeof (symname)) != 0)
    return 0;

  /* This might not work right if we have multiple symbols with the
     same name; the only way to really get it right is to perform
     the same sort of lookup as the dynamic linker. */
  msymbol = lookup_minimal_symbol_text (symname, NULL);
  if (!msymbol)
    return 0;

  return SYMBOL_VALUE_ADDRESS (msymbol);
}

/* ppc_linux_memory_remove_breakpoints attempts to remove a breakpoint
   in much the same fashion as memory_remove_breakpoint in mem-break.c,
   but is careful not to write back the previous contents if the code
   in question has changed in between inserting the breakpoint and
   removing it.

   Here is the problem that we're trying to solve...

   Once upon a time, before introducing this function to remove
   breakpoints from the inferior, setting a breakpoint on a shared
   library function prior to running the program would not work
   properly.  In order to understand the problem, it is first
   necessary to understand a little bit about dynamic linking on
   this platform.

   A call to a shared library function is accomplished via a bl
   (branch-and-link) instruction whose branch target is an entry
   in the procedure linkage table (PLT).  The PLT in the object
   file is uninitialized.  To gdb, prior to running the program, the
   entries in the PLT are all zeros.

   Once the program starts running, the shared libraries are loaded
   and the procedure linkage table is initialized, but the entries in
   the table are not (necessarily) resolved.  Once a function is
   actually called, the code in the PLT is hit and the function is
   resolved.  In order to better illustrate this, an example is in
   order; the following example is from the gdb testsuite.
	    
	We start the program shmain.

	    [kev@arroyo testsuite]$ ../gdb gdb.base/shmain
	    [...]

	We place two breakpoints, one on shr1 and the other on main.

	    (gdb) b shr1
	    Breakpoint 1 at 0x100409d4
	    (gdb) b main
	    Breakpoint 2 at 0x100006a0: file gdb.base/shmain.c, line 44.

	Examine the instruction (and the immediatly following instruction)
	upon which the breakpoint was placed.  Note that the PLT entry
	for shr1 contains zeros.

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      .long 0x0
	    0x100409d8 <shr1+4>:    .long 0x0

	Now run 'til main.

	    (gdb) r
	    Starting program: gdb.base/shmain 
	    Breakpoint 1 at 0xffaf790: file gdb.base/shr1.c, line 19.

	    Breakpoint 2, main ()
		at gdb.base/shmain.c:44
	    44        g = 1;

	Examine the PLT again.  Note that the loading of the shared
	library has initialized the PLT to code which loads a constant
	(which I think is an index into the GOT) into r11 and then
	branchs a short distance to the code which actually does the
	resolving.

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      li      r11,4
	    0x100409d8 <shr1+4>:    b       0x10040984 <sg+4>
	    (gdb) c
	    Continuing.

	    Breakpoint 1, shr1 (x=1)
		at gdb.base/shr1.c:19
	    19        l = 1;

	Now we've hit the breakpoint at shr1.  (The breakpoint was
	reset from the PLT entry to the actual shr1 function after the
	shared library was loaded.) Note that the PLT entry has been
	resolved to contain a branch that takes us directly to shr1. 
	(The real one, not the PLT entry.)

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      b       0xffaf76c <shr1>
	    0x100409d8 <shr1+4>:    b       0x10040984 <sg+4>

   The thing to note here is that the PLT entry for shr1 has been
   changed twice.

   Now the problem should be obvious.  GDB places a breakpoint (a
   trap instruction) on the zero value of the PLT entry for shr1. 
   Later on, after the shared library had been loaded and the PLT
   initialized, GDB gets a signal indicating this fact and attempts
   (as it always does when it stops) to remove all the breakpoints.

   The breakpoint removal was causing the former contents (a zero
   word) to be written back to the now initialized PLT entry thus
   destroying a portion of the initialization that had occurred only a
   short time ago.  When execution continued, the zero word would be
   executed as an instruction an an illegal instruction trap was
   generated instead.  (0 is not a legal instruction.)

   The fix for this problem was fairly straightforward.  The function
   memory_remove_breakpoint from mem-break.c was copied to this file,
   modified slightly, and renamed to ppc_linux_memory_remove_breakpoint.
   In tm-linux.h, MEMORY_REMOVE_BREAKPOINT is defined to call this new
   function.

   The differences between ppc_linux_memory_remove_breakpoint () and
   memory_remove_breakpoint () are minor.  All that the former does
   that the latter does not is check to make sure that the breakpoint
   location actually contains a breakpoint (trap instruction) prior
   to attempting to write back the old contents.  If it does contain
   a trap instruction, we allow the old contents to be written back. 
   Otherwise, we silently do nothing.

   The big question is whether memory_remove_breakpoint () should be
   changed to have the same functionality.  The downside is that more
   traffic is generated for remote targets since we'll have an extra
   fetch of a memory word each time a breakpoint is removed.

   For the time being, we'll leave this self-modifying-code-friendly
   version in ppc-linux-tdep.c, but it ought to be migrated somewhere
   else in the event that some other platform has similar needs with
   regard to removing breakpoints in some potentially self modifying
   code.  */
int
ppc_linux_memory_remove_breakpoint (struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  const unsigned char *bp;
  int val;
  int bplen;
  gdb_byte old_contents[BREAKPOINT_MAX];

  /* Determine appropriate breakpoint contents and size for this address.  */
  bp = gdbarch_breakpoint_from_pc (current_gdbarch, &addr, &bplen);
  if (bp == NULL)
    error (_("Software breakpoints not implemented for this target."));

  val = target_read_memory (addr, old_contents, bplen);

  /* If our breakpoint is no longer at the address, this means that the
     program modified the code on us, so it is wrong to put back the
     old value */
  if (val == 0 && memcmp (bp, old_contents, bplen) == 0)
    val = target_write_memory (addr, bp_tgt->shadow_contents, bplen);

  return val;
}

/* For historic reasons, PPC 32 GNU/Linux follows PowerOpen rather
   than the 32 bit SYSV R4 ABI structure return convention - all
   structures, no matter their size, are put in memory.  Vectors,
   which were added later, do get returned in a register though.  */

static enum return_value_convention
ppc_linux_return_value (struct gdbarch *gdbarch, struct type *valtype,
			struct regcache *regcache, gdb_byte *readbuf,
			const gdb_byte *writebuf)
{  
  if ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
       || TYPE_CODE (valtype) == TYPE_CODE_UNION)
      && !((TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 8)
	   && TYPE_VECTOR (valtype)))
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    return ppc_sysv_abi_return_value (gdbarch, valtype, regcache, readbuf,
				      writebuf);
}

/* Macros for matching instructions.  Note that, since all the
   operands are masked off before they're or-ed into the instruction,
   you can use -1 to make masks.  */

#define insn_d(opcd, rts, ra, d)                \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((ra) & 0x1f) << 16)                      \
   | ((d) & 0xffff))

#define insn_ds(opcd, rts, ra, d, xo)           \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((ra) & 0x1f) << 16)                      \
   | ((d) & 0xfffc)                             \
   | ((xo) & 0x3))

#define insn_xfx(opcd, rts, spr, xo)            \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((spr) & 0x1f) << 16)                     \
   | (((spr) & 0x3e0) << 6)                     \
   | (((xo) & 0x3ff) << 1))

/* Read a PPC instruction from memory.  PPC instructions are always
   big-endian, no matter what endianness the program is running in, so
   we can't use read_memory_integer or one of its friends here.  */
static unsigned int
read_insn (CORE_ADDR pc)
{
  unsigned char buf[4];

  read_memory (pc, buf, 4);
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}


/* An instruction to match.  */
struct insn_pattern
{
  unsigned int mask;            /* mask the insn with this... */
  unsigned int data;            /* ...and see if it matches this. */
  int optional;                 /* If non-zero, this insn may be absent.  */
};

/* Return non-zero if the instructions at PC match the series
   described in PATTERN, or zero otherwise.  PATTERN is an array of
   'struct insn_pattern' objects, terminated by an entry whose mask is
   zero.

   When the match is successful, fill INSN[i] with what PATTERN[i]
   matched.  If PATTERN[i] is optional, and the instruction wasn't
   present, set INSN[i] to 0 (which is not a valid PPC instruction).
   INSN should have as many elements as PATTERN.  Note that, if
   PATTERN contains optional instructions which aren't present in
   memory, then INSN will have holes, so INSN[i] isn't necessarily the
   i'th instruction in memory.  */
static int
insns_match_pattern (CORE_ADDR pc,
                     struct insn_pattern *pattern,
                     unsigned int *insn)
{
  int i;

  for (i = 0; pattern[i].mask; i++)
    {
      insn[i] = read_insn (pc);
      if ((insn[i] & pattern[i].mask) == pattern[i].data)
        pc += 4;
      else if (pattern[i].optional)
        insn[i] = 0;
      else
        return 0;
    }

  return 1;
}


/* Return the 'd' field of the d-form instruction INSN, properly
   sign-extended.  */
static CORE_ADDR
insn_d_field (unsigned int insn)
{
  return ((((CORE_ADDR) insn & 0xffff) ^ 0x8000) - 0x8000);
}


/* Return the 'ds' field of the ds-form instruction INSN, with the two
   zero bits concatenated at the right, and properly
   sign-extended.  */
static CORE_ADDR
insn_ds_field (unsigned int insn)
{
  return ((((CORE_ADDR) insn & 0xfffc) ^ 0x8000) - 0x8000);
}


/* If DESC is the address of a 64-bit PowerPC GNU/Linux function
   descriptor, return the descriptor's entry point.  */
static CORE_ADDR
ppc64_desc_entry_point (CORE_ADDR desc)
{
  /* The first word of the descriptor is the entry point.  */
  return (CORE_ADDR) read_memory_unsigned_integer (desc, 8);
}


/* Pattern for the standard linkage function.  These are built by
   build_plt_stub in elf64-ppc.c, whose GLINK argument is always
   zero.  */
static struct insn_pattern ppc64_standard_linkage[] =
  {
    /* addis r12, r2, <any> */
    { insn_d (-1, -1, -1, 0), insn_d (15, 12, 2, 0), 0 },

    /* std r2, 40(r1) */
    { -1, insn_ds (62, 2, 1, 40, 0), 0 },

    /* ld r11, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 11, 12, 0, 0), 0 },

    /* addis r12, r12, 1 <optional> */
    { insn_d (-1, -1, -1, -1), insn_d (15, 12, 2, 1), 1 },

    /* ld r2, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 2, 12, 0, 0), 0 },

    /* addis r12, r12, 1 <optional> */
    { insn_d (-1, -1, -1, -1), insn_d (15, 12, 2, 1), 1 },

    /* mtctr r11 */
    { insn_xfx (-1, -1, -1, -1), insn_xfx (31, 11, 9, 467),
      0 },

    /* ld r11, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 11, 12, 0, 0), 0 },
      
    /* bctr */
    { -1, 0x4e800420, 0 },

    { 0, 0, 0 }
  };
#define PPC64_STANDARD_LINKAGE_LEN \
  (sizeof (ppc64_standard_linkage) / sizeof (ppc64_standard_linkage[0]))

/* When the dynamic linker is doing lazy symbol resolution, the first
   call to a function in another object will go like this:

   - The user's function calls the linkage function:

     100007c4:	4b ff fc d5 	bl	10000498
     100007c8:	e8 41 00 28 	ld	r2,40(r1)

   - The linkage function loads the entry point (and other stuff) from
     the function descriptor in the PLT, and jumps to it:

     10000498:	3d 82 00 00 	addis	r12,r2,0
     1000049c:	f8 41 00 28 	std	r2,40(r1)
     100004a0:	e9 6c 80 98 	ld	r11,-32616(r12)
     100004a4:	e8 4c 80 a0 	ld	r2,-32608(r12)
     100004a8:	7d 69 03 a6 	mtctr	r11
     100004ac:	e9 6c 80 a8 	ld	r11,-32600(r12)
     100004b0:	4e 80 04 20 	bctr

   - But since this is the first time that PLT entry has been used, it
     sends control to its glink entry.  That loads the number of the
     PLT entry and jumps to the common glink0 code:

     10000c98:	38 00 00 00 	li	r0,0
     10000c9c:	4b ff ff dc 	b	10000c78

   - The common glink0 code then transfers control to the dynamic
     linker's fixup code:

     10000c78:	e8 41 00 28 	ld	r2,40(r1)
     10000c7c:	3d 82 00 00 	addis	r12,r2,0
     10000c80:	e9 6c 80 80 	ld	r11,-32640(r12)
     10000c84:	e8 4c 80 88 	ld	r2,-32632(r12)
     10000c88:	7d 69 03 a6 	mtctr	r11
     10000c8c:	e9 6c 80 90 	ld	r11,-32624(r12)
     10000c90:	4e 80 04 20 	bctr

   Eventually, this code will figure out how to skip all of this,
   including the dynamic linker.  At the moment, we just get through
   the linkage function.  */

/* If the current thread is about to execute a series of instructions
   at PC matching the ppc64_standard_linkage pattern, and INSN is the result
   from that pattern match, return the code address to which the
   standard linkage function will send them.  (This doesn't deal with
   dynamic linker lazy symbol resolution stubs.)  */
static CORE_ADDR
ppc64_standard_linkage_target (struct frame_info *frame,
			       CORE_ADDR pc, unsigned int *insn)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (frame));

  /* The address of the function descriptor this linkage function
     references.  */
  CORE_ADDR desc
    = ((CORE_ADDR) get_frame_register_unsigned (frame,
						tdep->ppc_gp0_regnum + 2)
       + (insn_d_field (insn[0]) << 16)
       + insn_ds_field (insn[2]));

  /* The first word of the descriptor is the entry point.  Return that.  */
  return ppc64_desc_entry_point (desc);
}


/* Given that we've begun executing a call trampoline at PC, return
   the entry point of the function the trampoline will go to.  */
static CORE_ADDR
ppc64_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  unsigned int ppc64_standard_linkage_insn[PPC64_STANDARD_LINKAGE_LEN];

  if (insns_match_pattern (pc, ppc64_standard_linkage,
                           ppc64_standard_linkage_insn))
    return ppc64_standard_linkage_target (frame, pc,
					  ppc64_standard_linkage_insn);
  else
    return 0;
}


/* Support for convert_from_func_ptr_addr (ARCH, ADDR, TARG) on PPC
   GNU/Linux.

   Usually a function pointer's representation is simply the address
   of the function.  On GNU/Linux on the PowerPC however, a function
   pointer may be a pointer to a function descriptor.

   For PPC64, a function descriptor is a TOC entry, in a data section,
   which contains three words: the first word is the address of the
   function, the second word is the TOC pointer (r2), and the third word
   is the static chain value.

   For PPC32, there are two kinds of function pointers: non-secure and
   secure.  Non-secure function pointers point directly to the
   function in a code section and thus need no translation.  Secure
   ones (from GCC's -msecure-plt option) are in a data section and
   contain one word: the address of the function.

   Throughout GDB it is currently assumed that a function pointer contains
   the address of the function, which is not easy to fix.  In addition, the
   conversion of a function address to a function pointer would
   require allocation of a TOC entry in the inferior's memory space,
   with all its drawbacks.  To be able to call C++ virtual methods in
   the inferior (which are called via function pointers),
   find_function_addr uses this function to get the function address
   from a function pointer.

   If ADDR points at what is clearly a function descriptor, transform
   it into the address of the corresponding function, if needed.  Be
   conservative, otherwise GDB will do the transformation on any
   random addresses such as occur when there is no symbol table.  */

static CORE_ADDR
ppc_linux_convert_from_func_ptr_addr (struct gdbarch *gdbarch,
				      CORE_ADDR addr,
				      struct target_ops *targ)
{
  struct gdbarch_tdep *tdep;
  struct section_table *s = target_section_by_addr (targ, addr);
  char *sect_name = NULL;

  if (!s)
    return addr;

  tdep = gdbarch_tdep (gdbarch);

  switch (tdep->wordsize)
    {
      case 4:
	sect_name = ".plt";
	break;
      case 8:
	sect_name = ".opd";
	break;
      default:
	internal_error (__FILE__, __LINE__,
			_("failed internal consistency check"));
    }

  /* Check if ADDR points to a function descriptor.  */

  /* NOTE: this depends on the coincidence that the address of a functions
     entry point is contained in the first word of its function descriptor
     for both PPC-64 and for PPC-32 with secure PLTs.  */
  if ((strcmp (s->the_bfd_section->name, sect_name) == 0)
      && s->the_bfd_section->flags & SEC_DATA)
    return get_target_memory_unsigned (targ, addr, tdep->wordsize);

  return addr;
}

/* This wrapper clears areas in the linux gregset not written by
   ppc_collect_gregset.  */

static void
ppc_linux_collect_gregset (const struct regset *regset,
			   const struct regcache *regcache,
			   int regnum, void *gregs, size_t len)
{
  if (regnum == -1)
    memset (gregs, 0, len);
  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
}

/* Regset descriptions.  */
static const struct ppc_reg_offsets ppc32_linux_reg_offsets =
  {
    /* General-purpose registers.  */
    /* .r0_offset = */ 0,
    /* .gpr_size = */ 4,
    /* .xr_size = */ 4,
    /* .pc_offset = */ 128,
    /* .ps_offset = */ 132,
    /* .cr_offset = */ 152,
    /* .lr_offset = */ 144,
    /* .ctr_offset = */ 140,
    /* .xer_offset = */ 148,
    /* .mq_offset = */ 156,

    /* Floating-point registers.  */
    /* .f0_offset = */ 0,
    /* .fpscr_offset = */ 256,
    /* .fpscr_size = */ 8,

    /* AltiVec registers.  */
    /* .vr0_offset = */ 0,
    /* .vrsave_offset = */ 512,
    /* .vscr_offset = */ 512 + 12
  };

static const struct ppc_reg_offsets ppc64_linux_reg_offsets =
  {
    /* General-purpose registers.  */
    /* .r0_offset = */ 0,
    /* .gpr_size = */ 8,
    /* .xr_size = */ 8,
    /* .pc_offset = */ 256,
    /* .ps_offset = */ 264,
    /* .cr_offset = */ 304,
    /* .lr_offset = */ 288,
    /* .ctr_offset = */ 280,
    /* .xer_offset = */ 296,
    /* .mq_offset = */ 312,

    /* Floating-point registers.  */
    /* .f0_offset = */ 0,
    /* .fpscr_offset = */ 256,
    /* .fpscr_size = */ 8,

    /* AltiVec registers.  */
    /* .vr0_offset = */ 0,
    /* .vrsave_offset = */ 528,
    /* .vscr_offset = */ 512 + 12
  };

static const struct regset ppc32_linux_gregset = {
  &ppc32_linux_reg_offsets,
  ppc_supply_gregset,
  ppc_linux_collect_gregset,
  NULL
};

static const struct regset ppc64_linux_gregset = {
  &ppc64_linux_reg_offsets,
  ppc_supply_gregset,
  ppc_linux_collect_gregset,
  NULL
};

static const struct regset ppc32_linux_fpregset = {
  &ppc32_linux_reg_offsets,
  ppc_supply_fpregset,
  ppc_collect_fpregset,
  NULL
};

const struct regset *
ppc_linux_gregset (int wordsize)
{
  return wordsize == 8 ? &ppc64_linux_gregset : &ppc32_linux_gregset;
}

const struct regset *
ppc_linux_fpregset (void)
{
  return &ppc32_linux_fpregset;
}

static const struct regset *
ppc_linux_regset_from_core_section (struct gdbarch *core_arch,
				    const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (core_arch);
  if (strcmp (sect_name, ".reg") == 0)
    {
      if (tdep->wordsize == 4)
	return &ppc32_linux_gregset;
      else
	return &ppc64_linux_gregset;
    }
  if (strcmp (sect_name, ".reg2") == 0)
    return &ppc32_linux_fpregset;
  return NULL;
}

static void
ppc_linux_sigtramp_cache (struct frame_info *next_frame,
			  struct trad_frame_cache *this_cache,
			  CORE_ADDR func, LONGEST offset,
			  int bias)
{
  CORE_ADDR base;
  CORE_ADDR regs;
  CORE_ADDR gpregs;
  CORE_ADDR fpregs;
  int i;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  base = frame_unwind_register_unsigned (next_frame,
					 gdbarch_sp_regnum (current_gdbarch));
  if (bias > 0 && frame_pc_unwind (next_frame) != func)
    /* See below, some signal trampolines increment the stack as their
       first instruction, need to compensate for that.  */
    base -= bias;

  /* Find the address of the register buffer pointer.  */
  regs = base + offset;
  /* Use that to find the address of the corresponding register
     buffers.  */
  gpregs = read_memory_unsigned_integer (regs, tdep->wordsize);
  fpregs = gpregs + 48 * tdep->wordsize;

  /* General purpose.  */
  for (i = 0; i < 32; i++)
    {
      int regnum = i + tdep->ppc_gp0_regnum;
      trad_frame_set_reg_addr (this_cache, regnum, gpregs + i * tdep->wordsize);
    }
  trad_frame_set_reg_addr (this_cache,
			   gdbarch_pc_regnum (current_gdbarch),
			   gpregs + 32 * tdep->wordsize);
  trad_frame_set_reg_addr (this_cache, tdep->ppc_ctr_regnum,
			   gpregs + 35 * tdep->wordsize);
  trad_frame_set_reg_addr (this_cache, tdep->ppc_lr_regnum,
			   gpregs + 36 * tdep->wordsize);
  trad_frame_set_reg_addr (this_cache, tdep->ppc_xer_regnum,
			   gpregs + 37 * tdep->wordsize);
  trad_frame_set_reg_addr (this_cache, tdep->ppc_cr_regnum,
			   gpregs + 38 * tdep->wordsize);

  if (ppc_floating_point_unit_p (gdbarch))
    {
      /* Floating point registers.  */
      for (i = 0; i < 32; i++)
	{
	  int regnum = i + gdbarch_fp0_regnum (current_gdbarch);
	  trad_frame_set_reg_addr (this_cache, regnum,
				   fpregs + i * tdep->wordsize);
	}
      trad_frame_set_reg_addr (this_cache, tdep->ppc_fpscr_regnum,
                         fpregs + 32 * tdep->wordsize);
    }
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

static void
ppc32_linux_sigaction_cache_init (const struct tramp_frame *self,
				  struct frame_info *next_frame,
				  struct trad_frame_cache *this_cache,
				  CORE_ADDR func)
{
  ppc_linux_sigtramp_cache (next_frame, this_cache, func,
			    0xd0 /* Offset to ucontext_t.  */
			    + 0x30 /* Offset to .reg.  */,
			    0);
}

static void
ppc64_linux_sigaction_cache_init (const struct tramp_frame *self,
				  struct frame_info *next_frame,
				  struct trad_frame_cache *this_cache,
				  CORE_ADDR func)
{
  ppc_linux_sigtramp_cache (next_frame, this_cache, func,
			    0x80 /* Offset to ucontext_t.  */
			    + 0xe0 /* Offset to .reg.  */,
			    128);
}

static void
ppc32_linux_sighandler_cache_init (const struct tramp_frame *self,
				   struct frame_info *next_frame,
				   struct trad_frame_cache *this_cache,
				   CORE_ADDR func)
{
  ppc_linux_sigtramp_cache (next_frame, this_cache, func,
			    0x40 /* Offset to ucontext_t.  */
			    + 0x1c /* Offset to .reg.  */,
			    0);
}

static void
ppc64_linux_sighandler_cache_init (const struct tramp_frame *self,
				   struct frame_info *next_frame,
				   struct trad_frame_cache *this_cache,
				   CORE_ADDR func)
{
  ppc_linux_sigtramp_cache (next_frame, this_cache, func,
			    0x80 /* Offset to struct sigcontext.  */
			    + 0x38 /* Offset to .reg.  */,
			    128);
}

static struct tramp_frame ppc32_linux_sigaction_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  { 
    { 0x380000ac, -1 }, /* li r0, 172 */
    { 0x44000002, -1 }, /* sc */
    { TRAMP_SENTINEL_INSN },
  },
  ppc32_linux_sigaction_cache_init
};
static struct tramp_frame ppc64_linux_sigaction_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { 0x38210080, -1 }, /* addi r1,r1,128 */
    { 0x380000ac, -1 }, /* li r0, 172 */
    { 0x44000002, -1 }, /* sc */
    { TRAMP_SENTINEL_INSN },
  },
  ppc64_linux_sigaction_cache_init
};
static struct tramp_frame ppc32_linux_sighandler_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  { 
    { 0x38000077, -1 }, /* li r0,119 */
    { 0x44000002, -1 }, /* sc */
    { TRAMP_SENTINEL_INSN },
  },
  ppc32_linux_sighandler_cache_init
};
static struct tramp_frame ppc64_linux_sighandler_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  { 
    { 0x38210080, -1 }, /* addi r1,r1,128 */
    { 0x38000077, -1 }, /* li r0,119 */
    { 0x44000002, -1 }, /* sc */
    { TRAMP_SENTINEL_INSN },
  },
  ppc64_linux_sighandler_cache_init
};

static void
ppc_linux_init_abi (struct gdbarch_info info,
                    struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* NOTE: jimb/2004-03-26: The System V ABI PowerPC Processor
     Supplement says that long doubles are sixteen bytes long.
     However, as one of the known warts of its ABI, PPC GNU/Linux uses
     eight-byte long doubles.  GCC only recently got 128-bit long
     double support on PPC, so it may be changing soon.  The
     Linux[sic] Standards Base says that programs that use 'long
     double' on PPC GNU/Linux are non-conformant.  */
  /* NOTE: cagney/2005-01-25: True for both 32- and 64-bit.  */
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  /* Handle PPC GNU/Linux 64-bit function pointers (which are really
     function descriptors) and 32-bit secure PLT entries.  */
  set_gdbarch_convert_from_func_ptr_addr
    (gdbarch, ppc_linux_convert_from_func_ptr_addr);

  if (tdep->wordsize == 4)
    {
      /* Until November 2001, gcc did not comply with the 32 bit SysV
	 R4 ABI requirement that structures less than or equal to 8
	 bytes should be returned in registers.  Instead GCC was using
	 the the AIX/PowerOpen ABI - everything returned in memory
	 (well ignoring vectors that is).  When this was corrected, it
	 wasn't fixed for GNU/Linux native platform.  Use the
	 PowerOpen struct convention.  */
      set_gdbarch_return_value (gdbarch, ppc_linux_return_value);

      set_gdbarch_memory_remove_breakpoint (gdbarch,
                                            ppc_linux_memory_remove_breakpoint);

      /* Shared library handling.  */
      set_gdbarch_skip_trampoline_code (gdbarch,
                                        ppc_linux_skip_trampoline_code);
      set_solib_svr4_fetch_link_map_offsets
        (gdbarch, svr4_ilp32_fetch_link_map_offsets);

      /* Trampolines.  */
      tramp_frame_prepend_unwinder (gdbarch, &ppc32_linux_sigaction_tramp_frame);
      tramp_frame_prepend_unwinder (gdbarch, &ppc32_linux_sighandler_tramp_frame);
    }
  
  if (tdep->wordsize == 8)
    {
      /* Shared library handling.  */
      set_gdbarch_skip_trampoline_code (gdbarch, ppc64_skip_trampoline_code);
      set_solib_svr4_fetch_link_map_offsets
        (gdbarch, svr4_lp64_fetch_link_map_offsets);

      /* Trampolines.  */
      tramp_frame_prepend_unwinder (gdbarch, &ppc64_linux_sigaction_tramp_frame);
      tramp_frame_prepend_unwinder (gdbarch, &ppc64_linux_sighandler_tramp_frame);
    }
  set_gdbarch_regset_from_core_section (gdbarch, ppc_linux_regset_from_core_section);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);
}

void
_initialize_ppc_linux_tdep (void)
{
  /* Register for all sub-familes of the POWER/PowerPC: 32-bit and
     64-bit PowerPC, and the older rs6k.  */
  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc64, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_rs6000, bfd_mach_rs6k, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
}
