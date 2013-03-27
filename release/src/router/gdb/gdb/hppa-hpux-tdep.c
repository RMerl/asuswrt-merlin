/* Target-dependent code for HP-UX on PA-RISC.

   Copyright (C) 2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "arch-utils.h"
#include "gdbcore.h"
#include "osabi.h"
#include "frame.h"
#include "frame-unwind.h"
#include "trad-frame.h"
#include "symtab.h"
#include "objfiles.h"
#include "inferior.h"
#include "infcall.h"
#include "observer.h"
#include "hppa-tdep.h"
#include "solib-som.h"
#include "solib-pa64.h"
#include "regset.h"
#include "regcache.h"
#include "exceptions.h"

#include "gdb_string.h"

#define IS_32BIT_TARGET(_gdbarch) \
	((gdbarch_tdep (_gdbarch))->bytes_per_address == 4)

/* Bit in the `ss_flag' member of `struct save_state' that indicates
   that the 64-bit register values are live.  From
   <machine/save_state.h>.  */
#define HPPA_HPUX_SS_WIDEREGS		0x40

/* Offsets of various parts of `struct save_state'.  From
   <machine/save_state.h>.  */
#define HPPA_HPUX_SS_FLAGS_OFFSET	0
#define HPPA_HPUX_SS_NARROW_OFFSET	4
#define HPPA_HPUX_SS_FPBLOCK_OFFSET 	256
#define HPPA_HPUX_SS_WIDE_OFFSET        640

/* The size of `struct save_state.  */
#define HPPA_HPUX_SAVE_STATE_SIZE	1152

/* The size of `struct pa89_save_state', which corresponds to PA-RISC
   1.1, the lowest common denominator that we support.  */
#define HPPA_HPUX_PA89_SAVE_STATE_SIZE	512


/* Forward declarations.  */
extern void _initialize_hppa_hpux_tdep (void);
extern initialize_file_ftype _initialize_hppa_hpux_tdep;

static int
in_opd_section (CORE_ADDR pc)
{
  struct obj_section *s;
  int retval = 0;

  s = find_pc_section (pc);

  retval = (s != NULL
	    && s->the_bfd_section->name != NULL
	    && strcmp (s->the_bfd_section->name, ".opd") == 0);
  return (retval);
}

/* Return one if PC is in the call path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

static int
hppa32_hpux_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  struct minimal_symbol *minsym;
  struct unwind_table_entry *u;

  /* First see if PC is in one of the two C-library trampolines.  */
  if (pc == hppa_symbol_address("$$dyncall") 
      || pc == hppa_symbol_address("_sr4export"))
    return 1;

  minsym = lookup_minimal_symbol_by_pc (pc);
  if (minsym && strcmp (DEPRECATED_SYMBOL_NAME (minsym), ".stub") == 0)
    return 1;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  if (u->stub_unwind.stub_type == 0)
    return 0;

  /* By definition a long-branch stub is a call stub.  */
  if (u->stub_unwind.stub_type == LONG_BRANCH)
    return 1;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 1;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 0;
	}

      /* Should never happen.  */
      warning (_("Unable to find branch in parameter relocation stub."));
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;
}

static int
hppa64_hpux_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  /* PA64 has a completely different stub/trampoline scheme.  Is it
     better?  Maybe.  It's certainly harder to determine with any
     certainty that we are in a stub because we can not refer to the
     unwinders to help. 

     The heuristic is simple.  Try to lookup the current PC value in th
     minimal symbol table.  If that fails, then assume we are not in a
     stub and return.

     Then see if the PC value falls within the section bounds for the
     section containing the minimal symbol we found in the first
     step.  If it does, then assume we are not in a stub and return.

     Finally peek at the instructions to see if they look like a stub.  */
  struct minimal_symbol *minsym;
  asection *sec;
  CORE_ADDR addr;
  int insn, i;

  minsym = lookup_minimal_symbol_by_pc (pc);
  if (! minsym)
    return 0;

  sec = SYMBOL_BFD_SECTION (minsym);

  if (bfd_get_section_vma (sec->owner, sec) <= pc
      && pc < (bfd_get_section_vma (sec->owner, sec)
		 + bfd_section_size (sec->owner, sec)))
      return 0;

  /* We might be in a stub.  Peek at the instructions.  Stubs are 3
     instructions long. */
  insn = read_memory_integer (pc, 4);

  /* Find out where we think we are within the stub.  */
  if ((insn & 0xffffc00e) == 0x53610000)
    addr = pc;
  else if ((insn & 0xffffffff) == 0xe820d000)
    addr = pc - 4;
  else if ((insn & 0xffffc00e) == 0x537b0000)
    addr = pc - 8;
  else
    return 0;

  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr, 4);
  if ((insn & 0xffffc00e) != 0x53610000)
    return 0;
	
  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr + 4, 4);
  if ((insn & 0xffffffff) != 0xe820d000)
    return 0;
    
  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr + 8, 4);
  if ((insn & 0xffffc00e) != 0x537b0000)
    return 0;

  /* Looks like a stub.  */
  return 1;
}

/* Return one if PC is in the return path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

static int
hppa_hpux_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  struct unwind_table_entry *u;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub or it's just a long branch stub, then
     return zero.  */
  if (u->stub_unwind.stub_type == 0 || u->stub_unwind.stub_type == LONG_BRANCH)
    return 0;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 0;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 1;
	}

      /* Should never happen.  */
      warning (_("Unable to find branch in parameter relocation stub."));
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;

}

/* Figure out if PC is in a trampoline, and if so find out where
   the trampoline will jump to.  If not in a trampoline, return zero.

   Simple code examination probably is not a good idea since the code
   sequences in trampolines can also appear in user code.

   We use unwinds and information from the minimal symbol table to
   determine when we're in a trampoline.  This won't work for ELF
   (yet) since it doesn't create stub unwind entries.  Whether or
   not ELF will create stub unwinds or normal unwinds for linker
   stubs is still being debated.

   This should handle simple calls through dyncall or sr4export,
   long calls, argument relocation stubs, and dyncall/sr4export
   calling an argument relocation stub.  It even handles some stubs
   used in dynamic executables.  */

static CORE_ADDR
hppa_hpux_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  long orig_pc = pc;
  long prev_inst, curr_inst, loc;
  struct minimal_symbol *msym;
  struct unwind_table_entry *u;

  /* Addresses passed to dyncall may *NOT* be the actual address
     of the function.  So we may have to do something special.  */
  if (pc == hppa_symbol_address("$$dyncall"))
    {
      pc = (CORE_ADDR) get_frame_register_unsigned (frame, 22);

      /* If bit 30 (counting from the left) is on, then pc is the address of
         the PLT entry for this function, not the address of the function
         itself.  Bit 31 has meaning too, but only for MPE.  */
      if (pc & 0x2)
	pc = (CORE_ADDR) read_memory_integer
			   (pc & ~0x3, gdbarch_ptr_bit (current_gdbarch) / 8);
    }
  if (pc == hppa_symbol_address("$$dyncall_external"))
    {
      pc = (CORE_ADDR) get_frame_register_unsigned (frame, 22);
      pc = (CORE_ADDR) read_memory_integer
			 (pc & ~0x3, gdbarch_ptr_bit (current_gdbarch) / 8);
    }
  else if (pc == hppa_symbol_address("_sr4export"))
    pc = (CORE_ADDR) get_frame_register_unsigned (frame, 22);

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  /* elz: attention here! (FIXME) because of a compiler/linker 
     error, some stubs which should have a non zero stub_unwind.stub_type 
     have unfortunately a value of zero. So this function would return here
     as if we were not in a trampoline. To fix this, we go look at the partial
     symbol information, which reports this guy as a stub.
     (FIXME): Unfortunately, we are not that lucky: it turns out that the 
     partial symbol information is also wrong sometimes. This is because 
     when it is entered (somread.c::som_symtab_read()) it can happen that
     if the type of the symbol (from the som) is Entry, and the symbol is
     in a shared library, then it can also be a trampoline.  This would
     be OK, except that I believe the way they decide if we are ina shared library
     does not work. SOOOO..., even if we have a regular function w/o trampolines
     its minimal symbol can be assigned type mst_solib_trampoline.
     Also, if we find that the symbol is a real stub, then we fix the unwind
     descriptor, and define the stub type to be EXPORT.
     Hopefully this is correct most of the times. */
  if (u->stub_unwind.stub_type == 0)
    {

/* elz: NOTE (FIXME!) once the problem with the unwind information is fixed
   we can delete all the code which appears between the lines */
/*--------------------------------------------------------------------------*/
      msym = lookup_minimal_symbol_by_pc (pc);

      if (msym == NULL || MSYMBOL_TYPE (msym) != mst_solib_trampoline)
	return orig_pc == pc ? 0 : pc & ~0x3;

      else if (msym != NULL && MSYMBOL_TYPE (msym) == mst_solib_trampoline)
	{
	  struct objfile *objfile;
	  struct minimal_symbol *msymbol;
	  int function_found = 0;

	  /* go look if there is another minimal symbol with the same name as 
	     this one, but with type mst_text. This would happen if the msym
	     is an actual trampoline, in which case there would be another
	     symbol with the same name corresponding to the real function */

	  ALL_MSYMBOLS (objfile, msymbol)
	  {
	    if (MSYMBOL_TYPE (msymbol) == mst_text
		&& DEPRECATED_STREQ (DEPRECATED_SYMBOL_NAME (msymbol), DEPRECATED_SYMBOL_NAME (msym)))
	      {
		function_found = 1;
		break;
	      }
	  }

	  if (function_found)
	    /* the type of msym is correct (mst_solib_trampoline), but
	       the unwind info is wrong, so set it to the correct value */
	    u->stub_unwind.stub_type = EXPORT;
	  else
	    /* the stub type info in the unwind is correct (this is not a
	       trampoline), but the msym type information is wrong, it
	       should be mst_text. So we need to fix the msym, and also
	       get out of this function */
	    {
	      MSYMBOL_TYPE (msym) = mst_text;
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

/*--------------------------------------------------------------------------*/
    }

  /* It's a stub.  Search for a branch and figure out where it goes.
     Note we have to handle multi insn branch sequences like ldil;ble.
     Most (all?) other branches can be determined by examining the contents
     of certain registers and the stack.  */

  loc = pc;
  curr_inst = 0;
  prev_inst = 0;
  while (1)
    {
      /* Make sure we haven't walked outside the range of this stub.  */
      if (u != find_unwind_entry (loc))
	{
	  warning (_("Unable to find branch in linker stub"));
	  return orig_pc == pc ? 0 : pc & ~0x3;
	}

      prev_inst = curr_inst;
      curr_inst = read_memory_integer (loc, 4);

      /* Does it look like a branch external using %r1?  Then it's the
         branch from the stub to the actual function.  */
      if ((curr_inst & 0xffe0e000) == 0xe0202000)
	{
	  /* Yup.  See if the previous instruction loaded
	     a value into %r1.  If so compute and return the jump address.  */
	  if ((prev_inst & 0xffe00000) == 0x20200000)
	    return (hppa_extract_21 (prev_inst) + hppa_extract_17 (curr_inst)) & ~0x3;
	  else
	    {
	      warning (_("Unable to find ldil X,%%r1 before ble Y(%%sr4,%%r1)."));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* Does it look like a be 0(sr0,%r21)? OR 
         Does it look like a be, n 0(sr0,%r21)? OR 
         Does it look like a bve (r21)? (this is on PA2.0)
         Does it look like a bve, n(r21)? (this is also on PA2.0)
         That's the branch from an
         import stub to an export stub.

         It is impossible to determine the target of the branch via
         simple examination of instructions and/or data (consider
         that the address in the plabel may be the address of the
         bind-on-reference routine in the dynamic loader).

         So we have try an alternative approach.

         Get the name of the symbol at our current location; it should
         be a stub symbol with the same name as the symbol in the
         shared library.

         Then lookup a minimal symbol with the same name; we should
         get the minimal symbol for the target routine in the shared
         library as those take precedence of import/export stubs.  */
      if ((curr_inst == 0xe2a00000) ||
	  (curr_inst == 0xe2a00002) ||
	  (curr_inst == 0xeaa0d000) ||
	  (curr_inst == 0xeaa0d002))
	{
	  struct minimal_symbol *stubsym, *libsym;

	  stubsym = lookup_minimal_symbol_by_pc (loc);
	  if (stubsym == NULL)
	    {
	      warning (_("Unable to find symbol for 0x%lx"), loc);
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  libsym = lookup_minimal_symbol (DEPRECATED_SYMBOL_NAME (stubsym), NULL, NULL);
	  if (libsym == NULL)
	    {
	      warning (_("Unable to find library symbol for %s."),
		       DEPRECATED_SYMBOL_NAME (stubsym));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  return SYMBOL_VALUE (libsym);
	}

      /* Does it look like bl X,%rp or bl X,%r0?  Another way to do a
         branch from the stub to the actual function.  */
      /*elz */
      else if ((curr_inst & 0xffe0e000) == 0xe8400000
	       || (curr_inst & 0xffe0e000) == 0xe8000000
	       || (curr_inst & 0xffe0e000) == 0xe800A000)
	return (loc + hppa_extract_17 (curr_inst) + 8) & ~0x3;

      /* Does it look like bv (rp)?   Note this depends on the
         current stack pointer being the same as the stack
         pointer in the stub itself!  This is a branch on from the
         stub back to the original caller.  */
      /*else if ((curr_inst & 0xffe0e000) == 0xe840c000) */
      else if ((curr_inst & 0xffe0f000) == 0xe840c000)
	{
	  /* Yup.  See if the previous instruction loaded
	     rp from sp - 8.  */
	  if (prev_inst == 0x4bc23ff1)
	    {
	      CORE_ADDR sp;
	      sp = get_frame_register_unsigned (frame, HPPA_SP_REGNUM);
	      return read_memory_integer (sp - 8, 4) & ~0x3;
	    }
	  else
	    {
	      warning (_("Unable to find restore of %%rp before bv (%%rp)."));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* elz: added this case to capture the new instruction
         at the end of the return part of an export stub used by
         the PA2.0: BVE, n (rp) */
      else if ((curr_inst & 0xffe0f000) == 0xe840d000)
	{
	  return (read_memory_integer
		  (get_frame_register_unsigned (frame, HPPA_SP_REGNUM) - 24,
		   gdbarch_ptr_bit (current_gdbarch) / 8)) & ~0x3;
	}

      /* What about be,n 0(sr0,%rp)?  It's just another way we return to
         the original caller from the stub.  Used in dynamic executables.  */
      else if (curr_inst == 0xe0400002)
	{
	  /* The value we jump to is sitting in sp - 24.  But that's
	     loaded several instructions before the be instruction.
	     I guess we could check for the previous instruction being
	     mtsp %r1,%sr0 if we want to do sanity checking.  */
	  return (read_memory_integer
		  (get_frame_register_unsigned (frame, HPPA_SP_REGNUM) - 24,
		   gdbarch_ptr_bit (current_gdbarch) / 8)) & ~0x3;
	}

      /* Haven't found the branch yet, but we're still in the stub.
         Keep looking.  */
      loc += 4;
    }
}

static void
hppa_skip_permanent_breakpoint (struct regcache *regcache)
{
  /* To step over a breakpoint instruction on the PA takes some
     fiddling with the instruction address queue.

     When we stop at a breakpoint, the IA queue front (the instruction
     we're executing now) points at the breakpoint instruction, and
     the IA queue back (the next instruction to execute) points to
     whatever instruction we would execute after the breakpoint, if it
     were an ordinary instruction.  This is the case even if the
     breakpoint is in the delay slot of a branch instruction.

     Clearly, to step past the breakpoint, we need to set the queue
     front to the back.  But what do we put in the back?  What
     instruction comes after that one?  Because of the branch delay
     slot, the next insn is always at the back + 4.  */

  ULONGEST pcoq_tail, pcsq_tail;
  regcache_cooked_read_unsigned (regcache, HPPA_PCOQ_TAIL_REGNUM, &pcoq_tail);
  regcache_cooked_read_unsigned (regcache, HPPA_PCSQ_TAIL_REGNUM, &pcsq_tail);

  regcache_cooked_write_unsigned (regcache, HPPA_PCOQ_HEAD_REGNUM, pcoq_tail);
  regcache_cooked_write_unsigned (regcache, HPPA_PCSQ_HEAD_REGNUM, pcsq_tail);

  regcache_cooked_write_unsigned (regcache, HPPA_PCOQ_TAIL_REGNUM, pcoq_tail + 4);
  /* We can leave the tail's space the same, since there's no jump.  */
}


/* Signal frames.  */
struct hppa_hpux_sigtramp_unwind_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static int hppa_hpux_tramp_reg[] = {
  HPPA_SAR_REGNUM,
  HPPA_PCOQ_HEAD_REGNUM,
  HPPA_PCSQ_HEAD_REGNUM,
  HPPA_PCOQ_TAIL_REGNUM,
  HPPA_PCSQ_TAIL_REGNUM,
  HPPA_EIEM_REGNUM,
  HPPA_IIR_REGNUM,
  HPPA_ISR_REGNUM,
  HPPA_IOR_REGNUM,
  HPPA_IPSW_REGNUM,
  -1,
  HPPA_SR4_REGNUM,
  HPPA_SR4_REGNUM + 1,
  HPPA_SR4_REGNUM + 2,
  HPPA_SR4_REGNUM + 3,
  HPPA_SR4_REGNUM + 4,
  HPPA_SR4_REGNUM + 5,
  HPPA_SR4_REGNUM + 6,
  HPPA_SR4_REGNUM + 7,
  HPPA_RCR_REGNUM,
  HPPA_PID0_REGNUM,
  HPPA_PID1_REGNUM,
  HPPA_CCR_REGNUM,
  HPPA_PID2_REGNUM,
  HPPA_PID3_REGNUM,
  HPPA_TR0_REGNUM,
  HPPA_TR0_REGNUM + 1,
  HPPA_TR0_REGNUM + 2,
  HPPA_CR27_REGNUM
};

static struct hppa_hpux_sigtramp_unwind_cache *
hppa_hpux_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
				       void **this_cache)

{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct hppa_hpux_sigtramp_unwind_cache *info;
  unsigned int flag;
  CORE_ADDR sp, scptr, off;
  int i, incr, szoff;

  if (*this_cache)
    return *this_cache;

  info = FRAME_OBSTACK_ZALLOC (struct hppa_hpux_sigtramp_unwind_cache);
  *this_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  sp = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  if (IS_32BIT_TARGET (gdbarch))
    scptr = sp - 1352;
  else
    scptr = sp - 1520;

  off = scptr;

  /* See /usr/include/machine/save_state.h for the structure of the save_state_t
     structure. */
  
  flag = read_memory_unsigned_integer(scptr + HPPA_HPUX_SS_FLAGS_OFFSET, 4);

  if (!(flag & HPPA_HPUX_SS_WIDEREGS))
    {
      /* Narrow registers. */
      off = scptr + HPPA_HPUX_SS_NARROW_OFFSET;
      incr = 4;
      szoff = 0;
    }
  else
    {
      /* Wide registers. */
      off = scptr + HPPA_HPUX_SS_WIDE_OFFSET + 8;
      incr = 8;
      szoff = (tdep->bytes_per_address == 4 ? 4 : 0);
    }

  for (i = 1; i < 32; i++)
    {
      info->saved_regs[HPPA_R0_REGNUM + i].addr = off + szoff;
      off += incr;
    }

  for (i = 0; i < ARRAY_SIZE (hppa_hpux_tramp_reg); i++)
    {
      if (hppa_hpux_tramp_reg[i] > 0)
        info->saved_regs[hppa_hpux_tramp_reg[i]].addr = off + szoff;

      off += incr;
    }

  /* TODO: fp regs */

  info->base = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  return info;
}

static void
hppa_hpux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_prologue_cache,
				   struct frame_id *this_id)
{
  struct hppa_hpux_sigtramp_unwind_cache *info
    = hppa_hpux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->base, frame_pc_unwind (next_frame));
}

static void
hppa_hpux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					void **this_prologue_cache,
					int regnum, int *optimizedp,
					enum lval_type *lvalp, 
					CORE_ADDR *addrp,
					int *realnump, gdb_byte *valuep)
{
  struct hppa_hpux_sigtramp_unwind_cache *info
    = hppa_hpux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  hppa_frame_prev_register_helper (next_frame, info->saved_regs, regnum,
		                   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind hppa_hpux_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  hppa_hpux_sigtramp_frame_this_id,
  hppa_hpux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
hppa_hpux_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  struct unwind_table_entry *u;
  CORE_ADDR pc = frame_pc_unwind (next_frame);

  u = find_unwind_entry (pc);

  /* If this is an export stub, try to get the unwind descriptor for
     the actual function itself.  */
  if (u && u->stub_unwind.stub_type == EXPORT)
    {
      gdb_byte buf[HPPA_INSN_SIZE];
      unsigned long insn;

      if (!safe_frame_unwind_memory (next_frame, u->region_start,
				     buf, sizeof buf))
	return NULL;

      insn = extract_unsigned_integer (buf, sizeof buf);
      if ((insn & 0xffe0e000) == 0xe8400000)
	u = find_unwind_entry(u->region_start + hppa_extract_17 (insn) + 8);
    }

  if (u && u->HP_UX_interrupt_marker)
    return &hppa_hpux_sigtramp_frame_unwind;

  return NULL;
}

static CORE_ADDR
hppa32_hpux_find_global_pointer (struct value *function)
{
  CORE_ADDR faddr;
  
  faddr = value_as_address (function);

  /* Is this a plabel? If so, dereference it to get the gp value.  */
  if (faddr & 2)
    {
      int status;
      char buf[4];

      faddr &= ~3;

      status = target_read_memory (faddr + 4, buf, sizeof (buf));
      if (status == 0)
	return extract_unsigned_integer (buf, sizeof (buf));
    }

  return gdbarch_tdep (current_gdbarch)->solib_get_got_by_pc (faddr);
}

static CORE_ADDR
hppa64_hpux_find_global_pointer (struct value *function)
{
  CORE_ADDR faddr;
  char buf[32];

  faddr = value_as_address (function);

  if (in_opd_section (faddr))
    {
      target_read_memory (faddr, buf, sizeof (buf));
      return extract_unsigned_integer (&buf[24], 8);
    }
  else
    {
      return gdbarch_tdep (current_gdbarch)->solib_get_got_by_pc (faddr);
    }
}

static unsigned int ldsid_pattern[] = {
  0x000010a0, /* ldsid (rX),rY */
  0x00001820, /* mtsp rY,sr0 */
  0xe0000000  /* be,n (sr0,rX) */
};

static CORE_ADDR
hppa_hpux_search_pattern (CORE_ADDR start, CORE_ADDR end, 
			  unsigned int *patterns, int count)
{
  int num_insns = (end - start + HPPA_INSN_SIZE) / HPPA_INSN_SIZE;
  unsigned int *insns;
  gdb_byte *buf;
  int offset, i;

  buf = alloca (num_insns * HPPA_INSN_SIZE);
  insns = alloca (num_insns * sizeof (unsigned int));

  read_memory (start, buf, num_insns * HPPA_INSN_SIZE);
  for (i = 0; i < num_insns; i++, buf += HPPA_INSN_SIZE)
    insns[i] = extract_unsigned_integer (buf, HPPA_INSN_SIZE);

  for (offset = 0; offset <= num_insns - count; offset++)
    {
      for (i = 0; i < count; i++)
        {
	  if ((insns[offset + i] & patterns[i]) != patterns[i])
	    break;
	}
      if (i == count)
        break;
    }

  if (offset <= num_insns - count)
    return start + offset * HPPA_INSN_SIZE;
  else
    return 0;
}

static CORE_ADDR
hppa32_hpux_search_dummy_call_sequence (struct gdbarch *gdbarch, CORE_ADDR pc,
					int *argreg)
{
  struct objfile *obj;
  struct obj_section *sec;
  struct hppa_objfile_private *priv;
  struct frame_info *frame;
  struct unwind_table_entry *u;
  CORE_ADDR addr, rp;
  char buf[4];
  unsigned int insn;

  sec = find_pc_section (pc);
  obj = sec->objfile;
  priv = objfile_data (obj, hppa_objfile_priv_data);

  if (!priv)
    priv = hppa_init_objfile_priv_data (obj);
  if (!priv)
    error (_("Internal error creating objfile private data."));

  /* Use the cached value if we have one.  */
  if (priv->dummy_call_sequence_addr != 0)
    {
      *argreg = priv->dummy_call_sequence_reg;
      return priv->dummy_call_sequence_addr;
    }

  /* First try a heuristic; if we are in a shared library call, our return
     pointer is likely to point at an export stub.  */
  frame = get_current_frame ();
  rp = frame_unwind_register_unsigned (frame, 2);
  u = find_unwind_entry (rp);
  if (u && u->stub_unwind.stub_type == EXPORT)
    {
      addr = hppa_hpux_search_pattern (u->region_start, u->region_end, 
				       ldsid_pattern, 
				       ARRAY_SIZE (ldsid_pattern));
      if (addr)
	goto found_pattern;
    }

  /* Next thing to try is to look for an export stub.  */
  if (priv->unwind_info)
    {
      int i;

      for (i = 0; i < priv->unwind_info->last; i++)
        {
	  struct unwind_table_entry *u;
	  u = &priv->unwind_info->table[i];
	  if (u->stub_unwind.stub_type == EXPORT)
	    {
	      addr = hppa_hpux_search_pattern (u->region_start, u->region_end, 
					       ldsid_pattern, 
					       ARRAY_SIZE (ldsid_pattern));
	      if (addr)
	        {
		  goto found_pattern;
		}
	    }
	}
    }

  /* Finally, if this is the main executable, try to locate a sequence 
     from noshlibs */
  addr = hppa_symbol_address ("noshlibs");
  sec = find_pc_section (addr);

  if (sec && sec->objfile == obj)
    {
      CORE_ADDR start, end;

      find_pc_partial_function (addr, NULL, &start, &end);
      if (start != 0 && end != 0)
        {
	  addr = hppa_hpux_search_pattern (start, end, ldsid_pattern,
					   ARRAY_SIZE (ldsid_pattern));
	  if (addr)
	    goto found_pattern;
        }
    }

  /* Can't find a suitable sequence.  */
  return 0;

found_pattern:
  target_read_memory (addr, buf, sizeof (buf));
  insn = extract_unsigned_integer (buf, sizeof (buf));
  priv->dummy_call_sequence_addr = addr;
  priv->dummy_call_sequence_reg = (insn >> 21) & 0x1f;

  *argreg = priv->dummy_call_sequence_reg;
  return priv->dummy_call_sequence_addr;
}

static CORE_ADDR
hppa64_hpux_search_dummy_call_sequence (struct gdbarch *gdbarch, CORE_ADDR pc,
					int *argreg)
{
  struct objfile *obj;
  struct obj_section *sec;
  struct hppa_objfile_private *priv;
  CORE_ADDR addr;
  struct minimal_symbol *msym;
  int i;

  sec = find_pc_section (pc);
  obj = sec->objfile;
  priv = objfile_data (obj, hppa_objfile_priv_data);

  if (!priv)
    priv = hppa_init_objfile_priv_data (obj);
  if (!priv)
    error (_("Internal error creating objfile private data."));

  /* Use the cached value if we have one.  */
  if (priv->dummy_call_sequence_addr != 0)
    {
      *argreg = priv->dummy_call_sequence_reg;
      return priv->dummy_call_sequence_addr;
    }

  /* FIXME: Without stub unwind information, locating a suitable sequence is
     fairly difficult.  For now, we implement a very naive and inefficient
     scheme; try to read in blocks of code, and look for a "bve,n (rp)" 
     instruction.  These are likely to occur at the end of functions, so
     we only look at the last two instructions of each function.  */
  for (i = 0, msym = obj->msymbols; i < obj->minimal_symbol_count; i++, msym++)
    {
      CORE_ADDR begin, end;
      char *name;
      gdb_byte buf[2 * HPPA_INSN_SIZE];
      int offset;

      find_pc_partial_function (SYMBOL_VALUE_ADDRESS (msym), &name,
      				&begin, &end);

      if (name == NULL || begin == 0 || end == 0)
        continue;

      if (target_read_memory (end - sizeof (buf), buf, sizeof (buf)) == 0)
        {
	  for (offset = 0; offset < sizeof (buf); offset++)
	    {
	      unsigned int insn;

	      insn = extract_unsigned_integer (buf + offset, HPPA_INSN_SIZE);
	      if (insn == 0xe840d002) /* bve,n (rp) */
	        {
		  addr = (end - sizeof (buf)) + offset;
		  goto found_pattern;
		}
	    }
	}
    }

  /* Can't find a suitable sequence.  */
  return 0;

found_pattern:
  priv->dummy_call_sequence_addr = addr;
  /* Right now we only look for a "bve,l (rp)" sequence, so the register is 
     always HPPA_RP_REGNUM.  */
  priv->dummy_call_sequence_reg = HPPA_RP_REGNUM;

  *argreg = priv->dummy_call_sequence_reg;
  return priv->dummy_call_sequence_addr;
}

static CORE_ADDR
hppa_hpux_find_import_stub_for_addr (CORE_ADDR funcaddr)
{
  struct objfile *objfile;
  struct minimal_symbol *funsym, *stubsym;
  CORE_ADDR stubaddr;

  funsym = lookup_minimal_symbol_by_pc (funcaddr);
  stubaddr = 0;

  ALL_OBJFILES (objfile)
    {
      stubsym = lookup_minimal_symbol_solib_trampoline
	(SYMBOL_LINKAGE_NAME (funsym), objfile);

      if (stubsym)
	{
	  struct unwind_table_entry *u;

	  u = find_unwind_entry (SYMBOL_VALUE (stubsym));
	  if (u == NULL 
	      || (u->stub_unwind.stub_type != IMPORT
		  && u->stub_unwind.stub_type != IMPORT_SHLIB))
	    continue;

          stubaddr = SYMBOL_VALUE (stubsym);

	  /* If we found an IMPORT stub, then we can stop searching;
	     if we found an IMPORT_SHLIB, we want to continue the search
	     in the hopes that we will find an IMPORT stub.  */
	  if (u->stub_unwind.stub_type == IMPORT)
	    break;
	}
    }

  return stubaddr;
}

static int
hppa_hpux_sr_for_addr (CORE_ADDR addr)
{
  int sr;
  /* The space register to use is encoded in the top 2 bits of the address.  */
  sr = addr >> (gdbarch_tdep (current_gdbarch)->bytes_per_address * 8 - 2);
  return sr + 4;
}

static CORE_ADDR
hppa_hpux_find_dummy_bpaddr (CORE_ADDR addr)
{
  /* In order for us to restore the space register to its starting state, 
     we need the dummy trampoline to return to the an instruction address in 
     the same space as where we started the call.  We used to place the 
     breakpoint near the current pc, however, this breaks nested dummy calls 
     as the nested call will hit the breakpoint address and terminate 
     prematurely.  Instead, we try to look for an address in the same space to 
     put the breakpoint.  
     
     This is similar in spirit to putting the breakpoint at the "entry point"
     of an executable.  */

  struct obj_section *sec;
  struct unwind_table_entry *u;
  struct minimal_symbol *msym;
  CORE_ADDR func;
  int i;

  sec = find_pc_section (addr);
  if (sec)
    {
      /* First try the lowest address in the section; we can use it as long
         as it is "regular" code (i.e. not a stub) */
      u = find_unwind_entry (sec->addr);
      if (!u || u->stub_unwind.stub_type == 0)
        return sec->addr;

      /* Otherwise, we need to find a symbol for a regular function.  We
         do this by walking the list of msymbols in the objfile.  The symbol
	 we find should not be the same as the function that was passed in.  */

      /* FIXME: this is broken, because we can find a function that will be
         called by the dummy call target function, which will still not 
	 work.  */

      find_pc_partial_function (addr, NULL, &func, NULL);
      for (i = 0, msym = sec->objfile->msymbols;
      	   i < sec->objfile->minimal_symbol_count;
	   i++, msym++)
	{
	  u = find_unwind_entry (SYMBOL_VALUE_ADDRESS (msym));
	  if (func != SYMBOL_VALUE_ADDRESS (msym) 
	      && (!u || u->stub_unwind.stub_type == 0))
	    return SYMBOL_VALUE_ADDRESS (msym);
	}
    }

  warning (_("Cannot find suitable address to place dummy breakpoint; nested "
	     "calls may fail."));
  return addr - 4;
}

static CORE_ADDR
hppa_hpux_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp,
			   CORE_ADDR funcaddr, int using_gcc,
			   struct value **args, int nargs,
			   struct type *value_type,
			   CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
			   struct regcache *regcache)
{
  CORE_ADDR pc, stubaddr;
  int argreg = 0;

  pc = read_pc ();

  /* Note: we don't want to pass a function descriptor here; push_dummy_call
     fills in the PIC register for us.  */
  funcaddr = gdbarch_convert_from_func_ptr_addr (gdbarch, funcaddr, NULL);

  /* The simple case is where we call a function in the same space that we are
     currently in; in that case we don't really need to do anything.  */
  if (hppa_hpux_sr_for_addr (pc) == hppa_hpux_sr_for_addr (funcaddr))
    {
      /* Intraspace call.  */
      *bp_addr = hppa_hpux_find_dummy_bpaddr (pc);
      *real_pc = funcaddr;
      regcache_cooked_write_unsigned (regcache, HPPA_RP_REGNUM, *bp_addr);

      return sp;
    }

  /* In order to make an interspace call, we need to go through a stub.
     gcc supplies an appropriate stub called "__gcc_plt_call", however, if
     an application is compiled with HP compilers then this stub is not
     available.  We used to fallback to "__d_plt_call", however that stub
     is not entirely useful for us because it doesn't do an interspace
     return back to the caller.  Also, on hppa64-hpux, there is no 
     __gcc_plt_call available.  In order to keep the code uniform, we
     instead don't use either of these stubs, but instead write our own
     onto the stack.

     A problem arises since the stack is located in a different space than
     code, so in order to branch to a stack stub, we will need to do an
     interspace branch.  Previous versions of gdb did this by modifying code
     at the current pc and doing single-stepping to set the pcsq.  Since this
     is highly undesirable, we use a different scheme:

     All we really need to do the branch to the stub is a short instruction
     sequence like this:
      
     PA1.1:
      		ldsid (rX),r1
		mtsp r1,sr0
		be,n (sr0,rX)

     PA2.0:
      		bve,n (sr0,rX)

     Instead of writing these sequences ourselves, we can find it in
     the instruction stream that belongs to the current space.  While this
     seems difficult at first, we are actually guaranteed to find the sequences
     in several places:

     For 32-bit code:
     - in export stubs for shared libraries
     - in the "noshlibs" routine in the main module

     For 64-bit code:
     - at the end of each "regular" function

     We cache the address of these sequences in the objfile's private data
     since these operations can potentially be quite expensive.

     So, what we do is:
     - write a stack trampoline
     - look for a suitable instruction sequence in the current space
     - point the sequence at the trampoline
     - set the return address of the trampoline to the current space 
       (see hppa_hpux_find_dummy_call_bpaddr)
     - set the continuing address of the "dummy code" as the sequence.

*/

  if (IS_32BIT_TARGET (gdbarch))
    {
      static unsigned int hppa32_tramp[] = {
        0x0fdf1291, /* stw r31,-8(,sp) */
        0x02c010a1, /* ldsid (,r22),r1 */
        0x00011820, /* mtsp r1,sr0 */
        0xe6c00000, /* be,l 0(sr0,r22),%sr0,%r31 */
        0x081f0242, /* copy r31,rp */
        0x0fd11082, /* ldw -8(,sp),rp */
        0x004010a1, /* ldsid (,rp),r1 */
        0x00011820, /* mtsp r1,sr0 */
        0xe0400000, /* be 0(sr0,rp) */
        0x08000240  /* nop */
      };

      /* for hppa32, we must call the function through a stub so that on
         return it can return to the space of our trampoline.  */
      stubaddr = hppa_hpux_find_import_stub_for_addr (funcaddr);
      if (stubaddr == 0)
        error (_("Cannot call external function not referenced by application "
	       "(no import stub).\n"));
      regcache_cooked_write_unsigned (regcache, 22, stubaddr);

      write_memory (sp, (char *)&hppa32_tramp, sizeof (hppa32_tramp));

      *bp_addr = hppa_hpux_find_dummy_bpaddr (pc);
      regcache_cooked_write_unsigned (regcache, 31, *bp_addr);

      *real_pc = hppa32_hpux_search_dummy_call_sequence (gdbarch, pc, &argreg);
      if (*real_pc == 0)
        error (_("Cannot make interspace call from here."));

      regcache_cooked_write_unsigned (regcache, argreg, sp);

      sp += sizeof (hppa32_tramp);
    }
  else
    {
      static unsigned int hppa64_tramp[] = {
        0xeac0f000, /* bve,l (r22),%r2 */
        0x0fdf12d1, /* std r31,-8(,sp) */
        0x0fd110c2, /* ldd -8(,sp),rp */
        0xe840d002, /* bve,n (rp) */
        0x08000240  /* nop */
      };

      /* for hppa64, we don't need to call through a stub; all functions
         return via a bve.  */
      regcache_cooked_write_unsigned (regcache, 22, funcaddr);
      write_memory (sp, (char *)&hppa64_tramp, sizeof (hppa64_tramp));

      *bp_addr = pc - 4;
      regcache_cooked_write_unsigned (regcache, 31, *bp_addr);

      *real_pc = hppa64_hpux_search_dummy_call_sequence (gdbarch, pc, &argreg);
      if (*real_pc == 0)
        error (_("Cannot make interspace call from here."));

      regcache_cooked_write_unsigned (regcache, argreg, sp);

      sp += sizeof (hppa64_tramp);
    }

  sp = gdbarch_frame_align (gdbarch, sp);

  return sp;
}



static void
hppa_hpux_supply_ss_narrow (struct regcache *regcache,
			    int regnum, const char *save_state)
{
  const char *ss_narrow = save_state + HPPA_HPUX_SS_NARROW_OFFSET;
  int i, offset = 0;

  for (i = HPPA_R1_REGNUM; i < HPPA_FP0_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, ss_narrow + offset);

      offset += 4;
    }
}

static void
hppa_hpux_supply_ss_fpblock (struct regcache *regcache,
			     int regnum, const char *save_state)
{
  const char *ss_fpblock = save_state + HPPA_HPUX_SS_FPBLOCK_OFFSET;
  int i, offset = 0;

  /* FIXME: We view the floating-point state as 64 single-precision
     registers for 32-bit code, and 32 double-precision register for
     64-bit code.  This distinction is artificial and should be
     eliminated.  If that ever happens, we should remove the if-clause
     below.  */

  if (register_size (get_regcache_arch (regcache), HPPA_FP0_REGNUM) == 4)
    {
      for (i = HPPA_FP0_REGNUM; i < HPPA_FP0_REGNUM + 64; i++)
	{
	  if (regnum == i || regnum == -1)
	    regcache_raw_supply (regcache, i, ss_fpblock + offset);

	  offset += 4;
	}
    }
  else
    {
      for (i = HPPA_FP0_REGNUM; i < HPPA_FP0_REGNUM + 32; i++)
	{
	  if (regnum == i || regnum == -1)
	    regcache_raw_supply (regcache, i, ss_fpblock + offset);

	  offset += 8;
	}
    }
}

static void
hppa_hpux_supply_ss_wide (struct regcache *regcache,
			  int regnum, const char *save_state)
{
  const char *ss_wide = save_state + HPPA_HPUX_SS_WIDE_OFFSET;
  int i, offset = 8;

  if (register_size (get_regcache_arch (regcache), HPPA_R1_REGNUM) == 4)
    offset += 4;

  for (i = HPPA_R1_REGNUM; i < HPPA_FP0_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, ss_wide + offset);

      offset += 8;
    }
}

static void
hppa_hpux_supply_save_state (const struct regset *regset,
			     struct regcache *regcache,
			     int regnum, const void *regs, size_t len)
{
  const char *proc_info = regs;
  const char *save_state = proc_info + 8;
  ULONGEST flags;

  flags = extract_unsigned_integer (save_state + HPPA_HPUX_SS_FLAGS_OFFSET, 4);
  if (regnum == -1 || regnum == HPPA_FLAGS_REGNUM)
    {
      struct gdbarch *arch = get_regcache_arch (regcache);
      size_t size = register_size (arch, HPPA_FLAGS_REGNUM);
      char buf[8];

      store_unsigned_integer (buf, size, flags);
      regcache_raw_supply (regcache, HPPA_FLAGS_REGNUM, buf);
    }

  /* If the SS_WIDEREGS flag is set, we really do need the full
     `struct save_state'.  */
  if (flags & HPPA_HPUX_SS_WIDEREGS && len < HPPA_HPUX_SAVE_STATE_SIZE)
    error (_("Register set contents too small"));

  if (flags & HPPA_HPUX_SS_WIDEREGS)
    hppa_hpux_supply_ss_wide (regcache, regnum, save_state);
  else
    hppa_hpux_supply_ss_narrow (regcache, regnum, save_state);

  hppa_hpux_supply_ss_fpblock (regcache, regnum, save_state);
}

/* HP-UX register set.  */

static struct regset hppa_hpux_regset =
{
  NULL,
  hppa_hpux_supply_save_state
};

static const struct regset *
hppa_hpux_regset_from_core_section (struct gdbarch *gdbarch,
				    const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0
      && sect_size >= HPPA_HPUX_PA89_SAVE_STATE_SIZE + 8)
    return &hppa_hpux_regset;

  return NULL;
}


/* Bit in the `ss_flag' member of `struct save_state' that indicates
   the state was saved from a system call.  From
   <machine/save_state.h>.  */
#define HPPA_HPUX_SS_INSYSCALL	0x02

static CORE_ADDR
hppa_hpux_read_pc (struct regcache *regcache)
{
  ULONGEST flags;

  /* If we're currently in a system call return the contents of %r31.  */
  regcache_cooked_read_unsigned (regcache, HPPA_FLAGS_REGNUM, &flags);
  if (flags & HPPA_HPUX_SS_INSYSCALL)
    {
      ULONGEST pc;
      regcache_cooked_read_unsigned (regcache, HPPA_R31_REGNUM, &pc);
      return pc & ~0x3;
    }

  return hppa_read_pc (regcache);
}

static void
hppa_hpux_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  ULONGEST flags;

  /* If we're currently in a system call also write PC into %r31.  */
  regcache_cooked_read_unsigned (regcache, HPPA_FLAGS_REGNUM, &flags);
  if (flags & HPPA_HPUX_SS_INSYSCALL)
    regcache_cooked_write_unsigned (regcache, HPPA_R31_REGNUM, pc | 0x3);

  return hppa_write_pc (regcache, pc);
}

static CORE_ADDR
hppa_hpux_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST flags;

  /* If we're currently in a system call return the contents of %r31.  */
  flags = frame_unwind_register_unsigned (next_frame, HPPA_FLAGS_REGNUM);
  if (flags & HPPA_HPUX_SS_INSYSCALL)
    return frame_unwind_register_unsigned (next_frame, HPPA_R31_REGNUM) & ~0x3;

  return hppa_unwind_pc (gdbarch, next_frame);
}


/* Given the current value of the pc, check to see if it is inside a stub, and
   if so, change the value of the pc to point to the caller of the stub.
   NEXT_FRAME is the next frame in the current list of frames.
   BASE contains to stack frame base of the current frame. 
   SAVE_REGS is the register file stored in the frame cache. */
static void
hppa_hpux_unwind_adjust_stub (struct frame_info *next_frame, CORE_ADDR base,
			      struct trad_frame_saved_reg *saved_regs)
{
  int optimized, realreg;
  enum lval_type lval;
  CORE_ADDR addr;
  char buffer[sizeof(ULONGEST)];
  ULONGEST val;
  CORE_ADDR stubpc;
  struct unwind_table_entry *u;

  trad_frame_get_prev_register (next_frame, saved_regs, 
				HPPA_PCOQ_HEAD_REGNUM, 
				&optimized, &lval, &addr, &realreg, buffer);
  val = extract_unsigned_integer (buffer, 
				  register_size (get_frame_arch (next_frame), 
      				  		 HPPA_PCOQ_HEAD_REGNUM));

  u = find_unwind_entry (val);
  if (u && u->stub_unwind.stub_type == EXPORT)
    {
      stubpc = read_memory_integer
		 (base - 24, gdbarch_ptr_bit (current_gdbarch) / 8);
      trad_frame_set_value (saved_regs, HPPA_PCOQ_HEAD_REGNUM, stubpc);
    }
  else if (hppa_symbol_address ("__gcc_plt_call") 
           == get_pc_function_start (val))
    {
      stubpc = read_memory_integer
		 (base - 8, gdbarch_ptr_bit (current_gdbarch) / 8);
      trad_frame_set_value (saved_regs, HPPA_PCOQ_HEAD_REGNUM, stubpc);
    }
}

static void
hppa_hpux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (IS_32BIT_TARGET (gdbarch))
    tdep->in_solib_call_trampoline = hppa32_hpux_in_solib_call_trampoline;
  else
    tdep->in_solib_call_trampoline = hppa64_hpux_in_solib_call_trampoline;

  tdep->unwind_adjust_stub = hppa_hpux_unwind_adjust_stub;

  set_gdbarch_in_solib_return_trampoline
    (gdbarch, hppa_hpux_in_solib_return_trampoline);
  set_gdbarch_skip_trampoline_code (gdbarch, hppa_hpux_skip_trampoline_code);

  set_gdbarch_push_dummy_code (gdbarch, hppa_hpux_push_dummy_code);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);

  set_gdbarch_read_pc (gdbarch, hppa_hpux_read_pc);
  set_gdbarch_write_pc (gdbarch, hppa_hpux_write_pc);
  set_gdbarch_unwind_pc (gdbarch, hppa_hpux_unwind_pc);
  set_gdbarch_skip_permanent_breakpoint
    (gdbarch, hppa_skip_permanent_breakpoint);

  set_gdbarch_regset_from_core_section
    (gdbarch, hppa_hpux_regset_from_core_section);

  frame_unwind_append_sniffer (gdbarch, hppa_hpux_sigtramp_unwind_sniffer);
}

static void
hppa_hpux_som_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->is_elf = 0;

  tdep->find_global_pointer = hppa32_hpux_find_global_pointer;

  hppa_hpux_init_abi (info, gdbarch);
  som_solib_select (tdep);
}

static void
hppa_hpux_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->is_elf = 1;
  tdep->find_global_pointer = hppa64_hpux_find_global_pointer;

  hppa_hpux_init_abi (info, gdbarch);
  pa64_solib_select (tdep);
}

static enum gdb_osabi
hppa_hpux_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "hpux-core") == 0)
    return GDB_OSABI_HPUX_SOM;
  else if (strcmp (bfd_get_target (abfd), "elf64-hppa") == 0)
    {
      asection *section;
      
      section = bfd_get_section_by_name (abfd, ".kernel");
      if (section)
        {
	  bfd_size_type size;
	  char *contents;

	  size = bfd_section_size (abfd, section);
	  contents = alloca (size);
 	  if (bfd_get_section_contents (abfd, section, contents, 
	  				(file_ptr) 0, size)
	      && strcmp (contents, "HP-UX") == 0)
	    return GDB_OSABI_HPUX_ELF;
	}
    }

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_hppa_hpux_tdep (void)
{
  /* BFD doesn't set a flavour for HP-UX style core files.  It doesn't
     set the architecture either.  */
  gdbarch_register_osabi_sniffer (bfd_arch_unknown,
				  bfd_target_unknown_flavour,
				  hppa_hpux_core_osabi_sniffer);
  gdbarch_register_osabi_sniffer (bfd_arch_hppa,
                                  bfd_target_elf_flavour,
				  hppa_hpux_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_HPUX_SOM,
                          hppa_hpux_som_init_abi);
  gdbarch_register_osabi (bfd_arch_hppa, bfd_mach_hppa20w, GDB_OSABI_HPUX_ELF,
                          hppa_hpux_elf_init_abi);
}
