/* Definitions for dealing with stack frames, for GDB, the GNU debugger.

   Copyright (C) 1986, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1996, 1997,
   1998, 1999, 2000, 2001, 2002, 2003, 2004, 2007
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

#if !defined (FRAME_H)
#define FRAME_H 1

/* The following is the intended naming schema for frame functions.
   It isn't 100% consistent, but it is aproaching that.  Frame naming
   schema:

   Prefixes:

   get_frame_WHAT...(): Get WHAT from the THIS frame (functionaly
   equivalent to THIS->next->unwind->what)

   frame_unwind_WHAT...(): Unwind THIS frame's WHAT from the NEXT
   frame.

   put_frame_WHAT...(): Put a value into this frame (unsafe, need to
   invalidate the frame / regcache afterwards) (better name more
   strongly hinting at its unsafeness)

   safe_....(): Safer version of various functions, doesn't throw an
   error (leave this for later?).  Returns non-zero / non-NULL if the
   request succeeds, zero / NULL otherwize.

   Suffixes:

   void /frame/_WHAT(): Read WHAT's value into the buffer parameter.

   ULONGEST /frame/_WHAT_unsigned(): Return an unsigned value (the
   alternative is *frame_unsigned_WHAT).

   LONGEST /frame/_WHAT_signed(): Return WHAT signed value.

   What:

   /frame/_memory* (frame, coreaddr, len [, buf]): Extract/return
   *memory.

   /frame/_register* (frame, regnum [, buf]): extract/return register.

   CORE_ADDR /frame/_{pc,sp,...} (frame): Resume address, innner most
   stack *address, ...

   */

struct symtab_and_line;
struct frame_unwind;
struct frame_base;
struct block;
struct gdbarch;
struct ui_file;

/* The frame object.  */

struct frame_info;

/* The frame object's ID.  This provides a per-frame unique identifier
   that can be used to relocate a `struct frame_info' after a target
   resume or a frame cache destruct.  It of course assumes that the
   inferior hasn't unwound the stack past that frame.  */

struct frame_id
{
  /* The frame's stack address.  This shall be constant through out
     the lifetime of a frame.  Note that this requirement applies to
     not just the function body, but also the prologue and (in theory
     at least) the epilogue.  Since that value needs to fall either on
     the boundary, or within the frame's address range, the frame's
     outer-most address (the inner-most address of the previous frame)
     is used.  Watch out for all the legacy targets that still use the
     function pointer register or stack pointer register.  They are
     wrong.

     This field is valid only if stack_addr_p is true.  Otherwise, this
     frame represents the null frame.  */
  CORE_ADDR stack_addr;

  /* The frame's code address.  This shall be constant through out the
     lifetime of the frame.  While the PC (a.k.a. resume address)
     changes as the function is executed, this code address cannot.
     Typically, it is set to the address of the entry point of the
     frame's function (as returned by frame_func_unwind().  

     This field is valid only if code_addr_p is true.  Otherwise, this
     frame is considered to have a wildcard code address, i.e. one that
     matches every address value in frame comparisons.  */
  CORE_ADDR code_addr;

  /* The frame's special address.  This shall be constant through out the
     lifetime of the frame.  This is used for architectures that may have
     frames that do not change the stack but are still distinct and have 
     some form of distinct identifier (e.g. the ia64 which uses a 2nd 
     stack for registers).  This field is treated as unordered - i.e. will
     not be used in frame ordering comparisons such as frame_id_inner().

     This field is valid only if special_addr_p is true.  Otherwise, this
     frame is considered to have a wildcard special address, i.e. one that
     matches every address value in frame comparisons.  */
  CORE_ADDR special_addr;

  /* Flags to indicate the above fields have valid contents.  */
  unsigned int stack_addr_p : 1;
  unsigned int code_addr_p : 1;
  unsigned int special_addr_p : 1;
};

/* Methods for constructing and comparing Frame IDs.

   NOTE: Given stackless functions A and B, where A calls B (and hence
   B is inner-to A).  The relationships: !eq(A,B); !eq(B,A);
   !inner(A,B); !inner(B,A); all hold.

   This is because, while B is inner-to A, B is not strictly inner-to A.  
   Being stackless, they have an identical .stack_addr value, and differ 
   only by their unordered .code_addr and/or .special_addr values.

   Because frame_id_inner is only used as a safety net (e.g.,
   detect a corrupt stack) the lack of strictness is not a problem.
   Code needing to determine an exact relationship between two frames
   must instead use frame_id_eq and frame_id_unwind.  For instance,
   in the above, to determine that A stepped-into B, the equation
   "A.id != B.id && A.id == id_unwind (B)" can be used.  */

/* For convenience.  All fields are zero.  */
extern const struct frame_id null_frame_id;

/* Construct a frame ID.  The first parameter is the frame's constant
   stack address (typically the outer-bound), and the second the
   frame's constant code address (typically the entry point).
   The special identifier address is set to indicate a wild card.  */
extern struct frame_id frame_id_build (CORE_ADDR stack_addr,
				       CORE_ADDR code_addr);

/* Construct a special frame ID.  The first parameter is the frame's constant
   stack address (typically the outer-bound), the second is the
   frame's constant code address (typically the entry point),
   and the third parameter is the frame's special identifier address. */
extern struct frame_id frame_id_build_special (CORE_ADDR stack_addr,
					       CORE_ADDR code_addr,
					       CORE_ADDR special_addr);

/* Construct a wild card frame ID.  The parameter is the frame's constant
   stack address (typically the outer-bound).  The code address as well
   as the special identifier address are set to indicate wild cards.  */
extern struct frame_id frame_id_build_wild (CORE_ADDR stack_addr);

/* Returns non-zero when L is a valid frame (a valid frame has a
   non-zero .base).  */
extern int frame_id_p (struct frame_id l);

/* Returns non-zero when L and R identify the same frame, or, if
   either L or R have a zero .func, then the same frame base.  */
extern int frame_id_eq (struct frame_id l, struct frame_id r);

/* Returns non-zero when L is strictly inner-than R (they have
   different frame .bases).  Neither L, nor R can be `null'.  See note
   above about frameless functions.  */
extern int frame_id_inner (struct frame_id l, struct frame_id r);

/* Write the internal representation of a frame ID on the specified
   stream.  */
extern void fprint_frame_id (struct ui_file *file, struct frame_id id);


/* Frame types.  Some are real, some are signal trampolines, and some
   are completely artificial (dummy).  */

enum frame_type
{
  /* A true stack frame, created by the target program during normal
     execution.  */
  NORMAL_FRAME,
  /* A fake frame, created by GDB when performing an inferior function
     call.  */
  DUMMY_FRAME,
  /* In a signal handler, various OSs handle this in various ways.
     The main thing is that the frame may be far from normal.  */
  SIGTRAMP_FRAME,
  /* Sentinel or registers frame.  This frame obtains register values
     direct from the inferior's registers.  */
  SENTINEL_FRAME
};

/* For every stopped thread, GDB tracks two frames: current and
   selected.  Current frame is the inner most frame of the selected
   thread.  Selected frame is the one being examined by the the GDB
   CLI (selected using `up', `down', ...).  The frames are created
   on-demand (via get_prev_frame()) and then held in a frame cache.  */
/* FIXME: cagney/2002-11-28: Er, there is a lie here.  If you do the
   sequence: `thread 1; up; thread 2; thread 1' you lose thread 1's
   selected frame.  At present GDB only tracks the selected frame of
   the current thread.  But be warned, that might change.  */
/* FIXME: cagney/2002-11-14: At any time, only one thread's selected
   and current frame can be active.  Switching threads causes gdb to
   discard all that cached frame information.  Ulgh!  Instead, current
   and selected frame should be bound to a thread.  */

/* On demand, create the inner most frame using information found in
   the inferior.  If the inner most frame can't be created, throw an
   error.  */
extern struct frame_info *get_current_frame (void);

/* Invalidates the frame cache (this function should have been called
   invalidate_cached_frames).

   FIXME: cagney/2002-11-28: There should be two methods: one that
   reverts the thread's selected frame back to current frame (for when
   the inferior resumes) and one that does not (for when the user
   modifies the target invalidating the frame cache).  */
extern void reinit_frame_cache (void);

/* On demand, create the selected frame and then return it.  If the
   selected frame can not be created, this function prints then throws
   an error.  When MESSAGE is non-NULL, use it for the error message,
   otherwize use a generic error message.  */
/* FIXME: cagney/2002-11-28: At present, when there is no selected
   frame, this function always returns the current (inner most) frame.
   It should instead, when a thread has previously had its frame
   selected (but not resumed) and the frame cache invalidated, find
   and then return that thread's previously selected frame.  */
extern struct frame_info *get_selected_frame (const char *message);

/* Select a specific frame.  NULL, apparently implies re-select the
   inner most frame.  */
extern void select_frame (struct frame_info *);

/* Given a FRAME, return the next (more inner, younger) or previous
   (more outer, older) frame.  */
extern struct frame_info *get_prev_frame (struct frame_info *);
extern struct frame_info *get_next_frame (struct frame_info *);

/* Given a frame's ID, relocate the frame.  Returns NULL if the frame
   is not found.  */
extern struct frame_info *frame_find_by_id (struct frame_id id);

/* Base attributes of a frame: */

/* The frame's `resume' address.  Where the program will resume in
   this frame.

   This replaced: frame->pc; */
extern CORE_ADDR get_frame_pc (struct frame_info *);

/* An address (not necessarily aligned to an instruction boundary)
   that falls within THIS frame's code block.

   When a function call is the last statement in a block, the return
   address for the call may land at the start of the next block.
   Similarly, if a no-return function call is the last statement in
   the function, the return address may end up pointing beyond the
   function, and possibly at the start of the next function.

   These methods make an allowance for this.  For call frames, this
   function returns the frame's PC-1 which "should" be an address in
   the frame's block.  */

extern CORE_ADDR get_frame_address_in_block (struct frame_info *this_frame);

/* Similar to get_frame_address_in_block, find an address in the
   block which logically called NEXT_FRAME, assuming it is a THIS_TYPE
   frame.  */

extern CORE_ADDR frame_unwind_address_in_block (struct frame_info *next_frame,
						enum frame_type this_type);

/* The frame's inner-most bound.  AKA the stack-pointer.  Confusingly
   known as top-of-stack.  */

extern CORE_ADDR get_frame_sp (struct frame_info *);
extern CORE_ADDR frame_sp_unwind (struct frame_info *);


/* Following on from the `resume' address.  Return the entry point
   address of the function containing that resume address, or zero if
   that function isn't known.  */
extern CORE_ADDR get_frame_func (struct frame_info *fi);

/* Similar to get_frame_func, find the start of the function which
   logically called NEXT_FRAME, assuming it is a THIS_TYPE frame.  */
extern CORE_ADDR frame_func_unwind (struct frame_info *next_frame,
				    enum frame_type this_type);

/* Closely related to the resume address, various symbol table
   attributes that are determined by the PC.  Note that for a normal
   frame, the PC refers to the resume address after the return, and
   not the call instruction.  In such a case, the address is adjusted
   so that it (approximately) identifies the call site (and not the
   return site).

   NOTE: cagney/2002-11-28: The frame cache could be used to cache the
   computed value.  Working on the assumption that the bottle-neck is
   in the single step code, and that code causes the frame cache to be
   constantly flushed, caching things in a frame is probably of little
   benefit.  As they say `show us the numbers'.

   NOTE: cagney/2002-11-28: Plenty more where this one came from:
   find_frame_block(), find_frame_partial_function(),
   find_frame_symtab(), find_frame_function().  Each will need to be
   carefully considered to determine if the real intent was for it to
   apply to the PC or the adjusted PC.  */
extern void find_frame_sal (struct frame_info *frame,
			    struct symtab_and_line *sal);

/* Set the current source and line to the location given by frame
   FRAME, if possible.  When CENTER is true, adjust so the relevant
   line is in the center of the next 'list'.  */

void set_current_sal_from_frame (struct frame_info *, int);

/* Return the frame base (what ever that is) (DEPRECATED).

   Old code was trying to use this single method for two conflicting
   purposes.  Such code needs to be updated to use either of:

   get_frame_id: A low level frame unique identifier, that consists of
   both a stack and a function address, that can be used to uniquely
   identify a frame.  This value is determined by the frame's
   low-level unwinder, the stack part [typically] being the
   top-of-stack of the previous frame, and the function part being the
   function's start address.  Since the correct identification of a
   frameless function requires both the a stack and function address,
   the old get_frame_base method was not sufficient.

   get_frame_base_address: get_frame_locals_address:
   get_frame_args_address: A set of high-level debug-info dependant
   addresses that fall within the frame.  These addresses almost
   certainly will not match the stack address part of a frame ID (as
   returned by get_frame_base).

   This replaced: frame->frame; */

extern CORE_ADDR get_frame_base (struct frame_info *);

/* Return the per-frame unique identifer.  Can be used to relocate a
   frame after a frame cache flush (and other similar operations).  If
   FI is NULL, return the null_frame_id.

   NOTE: kettenis/20040508: These functions return a structure.  On
   platforms where structures are returned in static storage (vax,
   m68k), this may trigger compiler bugs in code like:

   if (frame_id_eq (get_frame_id (l), get_frame_id (r)))

   where the return value from the first get_frame_id (l) gets
   overwritten by the second get_frame_id (r).  Please avoid writing
   code like this.  Use code like:

   struct frame_id id = get_frame_id (l);
   if (frame_id_eq (id, get_frame_id (r)))

   instead, since that avoids the bug.  */
extern struct frame_id get_frame_id (struct frame_info *fi);
extern struct frame_id frame_unwind_id (struct frame_info *next_frame);

/* Assuming that a frame is `normal', return its base-address, or 0 if
   the information isn't available.  NOTE: This address is really only
   meaningful to the frame's high-level debug info.  */
extern CORE_ADDR get_frame_base_address (struct frame_info *);

/* Assuming that a frame is `normal', return the base-address of the
   local variables, or 0 if the information isn't available.  NOTE:
   This address is really only meaningful to the frame's high-level
   debug info.  Typically, the argument and locals share a single
   base-address.  */
extern CORE_ADDR get_frame_locals_address (struct frame_info *);

/* Assuming that a frame is `normal', return the base-address of the
   parameter list, or 0 if that information isn't available.  NOTE:
   This address is really only meaningful to the frame's high-level
   debug info.  Typically, the argument and locals share a single
   base-address.  */
extern CORE_ADDR get_frame_args_address (struct frame_info *);

/* The frame's level: 0 for innermost, 1 for its caller, ...; or -1
   for an invalid frame).  */
extern int frame_relative_level (struct frame_info *fi);

/* Return the frame's type.  */

extern enum frame_type get_frame_type (struct frame_info *);

/* For frames where we can not unwind further, describe why.  */

enum unwind_stop_reason
  {
    /* No particular reason; either we haven't tried unwinding yet,
       or we didn't fail.  */
    UNWIND_NO_REASON,

    /* The previous frame's analyzer returns an invalid result
       from this_id.

       FIXME drow/2006-08-16: This is how GDB used to indicate end of
       stack.  We should migrate to a model where frames always have a
       valid ID, and this becomes not just an error but an internal
       error.  But that's a project for another day.  */
    UNWIND_NULL_ID,

    /* All the conditions after this point are considered errors;
       abnormal stack termination.  If a backtrace stops for one
       of these reasons, we'll let the user know.  This marker
       is not a valid stop reason.  */
    UNWIND_FIRST_ERROR,

    /* This frame ID looks like it ought to belong to a NEXT frame,
       but we got it for a PREV frame.  Normally, this is a sign of
       unwinder failure.  It could also indicate stack corruption.  */
    UNWIND_INNER_ID,

    /* This frame has the same ID as the previous one.  That means
       that unwinding further would almost certainly give us another
       frame with exactly the same ID, so break the chain.  Normally,
       this is a sign of unwinder failure.  It could also indicate
       stack corruption.  */
    UNWIND_SAME_ID,

    /* The frame unwinder didn't find any saved PC, but we needed
       one to unwind further.  */
    UNWIND_NO_SAVED_PC,
  };

/* Return the reason why we can't unwind past this frame.  */

enum unwind_stop_reason get_frame_unwind_stop_reason (struct frame_info *);

/* Translate a reason code to an informative string.  */

const char *frame_stop_reason_string (enum unwind_stop_reason);

/* Unwind the stack frame so that the value of REGNUM, in the previous
   (up, older) frame is returned.  If VALUEP is NULL, don't
   fetch/compute the value.  Instead just return the location of the
   value.  */
extern void frame_register_unwind (struct frame_info *frame, int regnum,
				   int *optimizedp, enum lval_type *lvalp,
				   CORE_ADDR *addrp, int *realnump,
				   gdb_byte *valuep);

/* Fetch a register from this, or unwind a register from the next
   frame.  Note that the get_frame methods are wrappers to
   frame->next->unwind.  They all [potentially] throw an error if the
   fetch fails.  */

extern void frame_unwind_register (struct frame_info *frame,
				   int regnum, gdb_byte *buf);
extern void get_frame_register (struct frame_info *frame,
				int regnum, gdb_byte *buf);

extern LONGEST frame_unwind_register_signed (struct frame_info *frame,
					     int regnum);
extern LONGEST get_frame_register_signed (struct frame_info *frame,
					  int regnum);
extern ULONGEST frame_unwind_register_unsigned (struct frame_info *frame,
					       int regnum);
extern ULONGEST get_frame_register_unsigned (struct frame_info *frame,
					     int regnum);


/* Use frame_unwind_register_signed.  */
extern void frame_unwind_unsigned_register (struct frame_info *frame,
					    int regnum, ULONGEST *val);

/* Get the value of the register that belongs to this FRAME.  This
   function is a wrapper to the call sequence ``frame_register_unwind
   (get_next_frame (FRAME))''.  As per frame_register_unwind(), if
   VALUEP is NULL, the registers value is not fetched/computed.  */

extern void frame_register (struct frame_info *frame, int regnum,
			    int *optimizedp, enum lval_type *lvalp,
			    CORE_ADDR *addrp, int *realnump,
			    gdb_byte *valuep);

/* The reverse.  Store a register value relative to the specified
   frame.  Note: this call makes the frame's state undefined.  The
   register and frame caches must be flushed.  */
extern void put_frame_register (struct frame_info *frame, int regnum,
				const gdb_byte *buf);

/* Read LEN bytes from one or multiple registers starting with REGNUM
   in frame FRAME, starting at OFFSET, into BUF.  */
extern int get_frame_register_bytes (struct frame_info *frame, int regnum,
				     CORE_ADDR offset, int len,
				     gdb_byte *myaddr);

/* Write LEN bytes to one or multiple registers starting with REGNUM
   in frame FRAME, starting at OFFSET, into BUF.  */
extern void put_frame_register_bytes (struct frame_info *frame, int regnum,
				      CORE_ADDR offset, int len,
				      const gdb_byte *myaddr);

/* Map between a frame register number and its name.  A frame register
   space is a superset of the cooked register space --- it also
   includes builtin registers.  If NAMELEN is negative, use the NAME's
   length when doing the comparison.  */

extern int frame_map_name_to_regnum (struct frame_info *frame,
				     const char *name, int namelen);
extern const char *frame_map_regnum_to_name (struct frame_info *frame,
					     int regnum);

/* Unwind the PC.  Strictly speaking return the resume address of the
   calling frame.  For GDB, `pc' is the resume address and not a
   specific register.  */

extern CORE_ADDR frame_pc_unwind (struct frame_info *frame);

/* Discard the specified frame.  Restoring the registers to the state
   of the caller.  */
extern void frame_pop (struct frame_info *frame);

/* Return memory from the specified frame.  A frame knows its thread /
   LWP and hence can find its way down to a target.  The assumption
   here is that the current and previous frame share a common address
   space.

   If the memory read fails, these methods throw an error.

   NOTE: cagney/2003-06-03: Should there be unwind versions of these
   methods?  That isn't clear.  Can code, for instance, assume that
   this and the previous frame's memory or architecture are identical?
   If architecture / memory changes are always separated by special
   adaptor frames this should be ok.  */

extern void get_frame_memory (struct frame_info *this_frame, CORE_ADDR addr,
			      gdb_byte *buf, int len);
extern LONGEST get_frame_memory_signed (struct frame_info *this_frame,
					CORE_ADDR memaddr, int len);
extern ULONGEST get_frame_memory_unsigned (struct frame_info *this_frame,
					   CORE_ADDR memaddr, int len);

/* Same as above, but return non-zero when the entire memory read
   succeeds, zero otherwize.  */
extern int safe_frame_unwind_memory (struct frame_info *this_frame,
				     CORE_ADDR addr, gdb_byte *buf, int len);

/* Return this frame's architecture.  */

extern struct gdbarch *get_frame_arch (struct frame_info *this_frame);


/* Values for the source flag to be used in print_frame_info_base().  */
enum print_what
  { 
    /* Print only the source line, like in stepi. */
    SRC_LINE = -1, 
    /* Print only the location, i.e. level, address (sometimes)
       function, args, file, line, line num. */
    LOCATION,
    /* Print both of the above. */
    SRC_AND_LOC, 
    /* Print location only, but always include the address. */
    LOC_AND_ADDRESS 
  };

/* Allocate additional space for appendices to a struct frame_info.
   NOTE: Much of GDB's code works on the assumption that the allocated
   saved_regs[] array is the size specified below.  If you try to make
   that array smaller, GDB will happily walk off its end.  */

#ifdef SIZEOF_FRAME_SAVED_REGS
#error "SIZEOF_FRAME_SAVED_REGS can not be re-defined"
#endif
#define SIZEOF_FRAME_SAVED_REGS \
        (sizeof (CORE_ADDR) * (gdbarch_num_regs (current_gdbarch)\
			       + gdbarch_num_pseudo_regs (current_gdbarch)))

/* Allocate zero initialized memory from the frame cache obstack.
   Appendices to the frame info (such as the unwind cache) should
   allocate memory using this method.  */

extern void *frame_obstack_zalloc (unsigned long size);
#define FRAME_OBSTACK_ZALLOC(TYPE) ((TYPE *) frame_obstack_zalloc (sizeof (TYPE)))
#define FRAME_OBSTACK_CALLOC(NUMBER,TYPE) ((TYPE *) frame_obstack_zalloc ((NUMBER) * sizeof (TYPE)))

/* Create a regcache, and copy the frame's registers into it.  */
struct regcache *frame_save_as_regcache (struct frame_info *this_frame);

extern struct block *get_frame_block (struct frame_info *,
                                      CORE_ADDR *addr_in_block);

/* Return the `struct block' that belongs to the selected thread's
   selected frame.  If the inferior has no state, return NULL.

   NOTE: cagney/2002-11-29:

   No state?  Does the inferior have any execution state (a core file
   does, an executable does not).  At present the code tests
   `target_has_stack' but I'm left wondering if it should test
   `target_has_registers' or, even, a merged target_has_state.

   Should it look at the most recently specified SAL?  If the target
   has no state, should this function try to extract a block from the
   most recently selected SAL?  That way `list foo' would give it some
   sort of reference point.  Then again, perhaps that would confuse
   things.

   Calls to this function can be broken down into two categories: Code
   that uses the selected block as an additional, but optional, data
   point; Code that uses the selected block as a prop, when it should
   have the relevant frame/block/pc explicitly passed in.

   The latter can be eliminated by correctly parameterizing the code,
   the former though is more interesting.  Per the "address" command,
   it occurs in the CLI code and makes it possible for commands to
   work, even when the inferior has no state.  */

extern struct block *get_selected_block (CORE_ADDR *addr_in_block);

extern struct symbol *get_frame_function (struct frame_info *);

extern CORE_ADDR get_pc_function_start (CORE_ADDR);

extern struct frame_info *find_relative_frame (struct frame_info *, int *);

extern void show_and_print_stack_frame (struct frame_info *fi, int print_level,
					enum print_what print_what);

extern void print_stack_frame (struct frame_info *, int print_level,
			       enum print_what print_what);

extern void show_stack_frame (struct frame_info *);

extern void print_frame_info (struct frame_info *, int print_level,
			      enum print_what print_what, int args);

extern struct frame_info *block_innermost_frame (struct block *);

extern int deprecated_pc_in_call_dummy (CORE_ADDR pc);

/* FIXME: cagney/2003-02-02: Should be deprecated or replaced with a
   function called get_frame_register_p().  This slightly weird (and
   older) variant of get_frame_register() returns zero (indicating the
   register is unavailable) if either: the register isn't cached; or
   the register has been optimized out.  Problem is, neither check is
   exactly correct.  A register can't be optimized out (it may not
   have been saved as part of a function call); The fact that a
   register isn't in the register cache doesn't mean that the register
   isn't available (it could have been fetched from memory).  */

extern int frame_register_read (struct frame_info *frame, int regnum,
				gdb_byte *buf);

/* From stack.c.  */
extern void args_info (char *, int);

extern void locals_info (char *, int);

extern void (*deprecated_selected_frame_level_changed_hook) (int);

extern void return_command (char *, int);


/* Notes (cagney/2002-11-27, drow/2003-09-06):

   You might think that calls to this function can simply be replaced by a
   call to get_selected_frame().

   Unfortunately, it isn't that easy.

   The relevant code needs to be audited to determine if it is
   possible (or practical) to instead pass the applicable frame in as a
   parameter.  For instance, DEPRECATED_DO_REGISTERS_INFO() relied on
   the deprecated_selected_frame global, while its replacement,
   PRINT_REGISTERS_INFO(), is parameterized with the selected frame.
   The only real exceptions occur at the edge (in the CLI code) where
   user commands need to pick up the selected frame before proceeding.

   There are also some functions called with a NULL frame meaning either "the
   program is not running" or "use the selected frame".

   This is important.  GDB is trying to stamp out the hack:

   saved_frame = deprecated_safe_get_selected_frame ();
   select_frame (...);
   hack_using_global_selected_frame ();
   select_frame (saved_frame);

   Take care!

   This function calls get_selected_frame if the inferior should have a
   frame, or returns NULL otherwise.  */

extern struct frame_info *deprecated_safe_get_selected_frame (void);

/* Create a frame using the specified BASE and PC.  */

extern struct frame_info *create_new_frame (CORE_ADDR base, CORE_ADDR pc);

/* FIXME: cagney/2002-12-06: Has the PC in the current frame changed?
   "infrun.c", Thanks to gdbarch_decr_pc_after_break, can change the PC after
   the initial frame create.  This puts things back in sync.

   This replaced: frame->pc = ....; */
extern void deprecated_update_frame_pc_hack (struct frame_info *frame,
					     CORE_ADDR pc);

/* FIXME: cagney/2002-12-18: Has the frame's base changed?  Or to be
   more exact, was that initial guess at the frame's base as returned
   by the deleted read_fp() wrong?  If it was, fix it.  This shouldn't
   be necessary since the code should be getting the frame's base
   correct from the outset.

   This replaced: frame->frame = ....; */
extern void deprecated_update_frame_base_hack (struct frame_info *frame,
					       CORE_ADDR base);

#endif /* !defined (FRAME_H)  */
