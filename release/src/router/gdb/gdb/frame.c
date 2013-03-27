/* Cache and manage frames for GDB, the GNU debugger.

   Copyright (C) 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000, 2001,
   2002, 2003, 2004, 2007 Free Software Foundation, Inc.

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
#include "target.h"
#include "value.h"
#include "inferior.h"	/* for inferior_ptid */
#include "regcache.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "user-regs.h"
#include "gdb_obstack.h"
#include "dummy-frame.h"
#include "sentinel-frame.h"
#include "gdbcore.h"
#include "annotate.h"
#include "language.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "command.h"
#include "gdbcmd.h"
#include "observer.h"
#include "objfiles.h"
#include "exceptions.h"

static struct frame_info *get_prev_frame_1 (struct frame_info *this_frame);

/* We keep a cache of stack frames, each of which is a "struct
   frame_info".  The innermost one gets allocated (in
   wait_for_inferior) each time the inferior stops; current_frame
   points to it.  Additional frames get allocated (in get_prev_frame)
   as needed, and are chained through the next and prev fields.  Any
   time that the frame cache becomes invalid (most notably when we
   execute something, but also if we change how we interpret the
   frames (e.g. "set heuristic-fence-post" in mips-tdep.c, or anything
   which reads new symbols)), we should call reinit_frame_cache.  */

struct frame_info
{
  /* Level of this frame.  The inner-most (youngest) frame is at level
     0.  As you move towards the outer-most (oldest) frame, the level
     increases.  This is a cached value.  It could just as easily be
     computed by counting back from the selected frame to the inner
     most frame.  */
  /* NOTE: cagney/2002-04-05: Perhaps a level of ``-1'' should be
     reserved to indicate a bogus frame - one that has been created
     just to keep GDB happy (GDB always needs a frame).  For the
     moment leave this as speculation.  */
  int level;

  /* The frame's low-level unwinder and corresponding cache.  The
     low-level unwinder is responsible for unwinding register values
     for the previous frame.  The low-level unwind methods are
     selected based on the presence, or otherwise, of register unwind
     information such as CFI.  */
  void *prologue_cache;
  const struct frame_unwind *unwind;

  /* Cached copy of the previous frame's resume address.  */
  struct {
    int p;
    CORE_ADDR value;
  } prev_pc;
  
  /* Cached copy of the previous frame's function address.  */
  struct
  {
    CORE_ADDR addr;
    int p;
  } prev_func;
  
  /* This frame's ID.  */
  struct
  {
    int p;
    struct frame_id value;
  } this_id;
  
  /* The frame's high-level base methods, and corresponding cache.
     The high level base methods are selected based on the frame's
     debug info.  */
  const struct frame_base *base;
  void *base_cache;

  /* Pointers to the next (down, inner, younger) and previous (up,
     outer, older) frame_info's in the frame cache.  */
  struct frame_info *next; /* down, inner, younger */
  int prev_p;
  struct frame_info *prev; /* up, outer, older */

  /* The reason why we could not set PREV, or UNWIND_NO_REASON if we
     could.  Only valid when PREV_P is set.  */
  enum unwind_stop_reason stop_reason;
};

/* Flag to control debugging.  */

static int frame_debug;
static void
show_frame_debug (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Frame debugging is %s.\n"), value);
}

/* Flag to indicate whether backtraces should stop at main et.al.  */

static int backtrace_past_main;
static void
show_backtrace_past_main (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Whether backtraces should continue past \"main\" is %s.\n"),
		    value);
}

static int backtrace_past_entry;
static void
show_backtrace_past_entry (struct ui_file *file, int from_tty,
			   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Whether backtraces should continue past the entry point of a program is %s.\n"),
		    value);
}

static int backtrace_limit = INT_MAX;
static void
show_backtrace_limit (struct ui_file *file, int from_tty,
		      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
An upper bound on the number of backtrace levels is %s.\n"),
		    value);
}


static void
fprint_field (struct ui_file *file, const char *name, int p, CORE_ADDR addr)
{
  if (p)
    fprintf_unfiltered (file, "%s=0x%s", name, paddr_nz (addr));
  else
    fprintf_unfiltered (file, "!%s", name);
}

void
fprint_frame_id (struct ui_file *file, struct frame_id id)
{
  fprintf_unfiltered (file, "{");
  fprint_field (file, "stack", id.stack_addr_p, id.stack_addr);
  fprintf_unfiltered (file, ",");
  fprint_field (file, "code", id.code_addr_p, id.code_addr);
  fprintf_unfiltered (file, ",");
  fprint_field (file, "special", id.special_addr_p, id.special_addr);
  fprintf_unfiltered (file, "}");
}

static void
fprint_frame_type (struct ui_file *file, enum frame_type type)
{
  switch (type)
    {
    case NORMAL_FRAME:
      fprintf_unfiltered (file, "NORMAL_FRAME");
      return;
    case DUMMY_FRAME:
      fprintf_unfiltered (file, "DUMMY_FRAME");
      return;
    case SIGTRAMP_FRAME:
      fprintf_unfiltered (file, "SIGTRAMP_FRAME");
      return;
    default:
      fprintf_unfiltered (file, "<unknown type>");
      return;
    };
}

static void
fprint_frame (struct ui_file *file, struct frame_info *fi)
{
  if (fi == NULL)
    {
      fprintf_unfiltered (file, "<NULL frame>");
      return;
    }
  fprintf_unfiltered (file, "{");
  fprintf_unfiltered (file, "level=%d", fi->level);
  fprintf_unfiltered (file, ",");
  fprintf_unfiltered (file, "type=");
  if (fi->unwind != NULL)
    fprint_frame_type (file, fi->unwind->type);
  else
    fprintf_unfiltered (file, "<unknown>");
  fprintf_unfiltered (file, ",");
  fprintf_unfiltered (file, "unwind=");
  if (fi->unwind != NULL)
    gdb_print_host_address (fi->unwind, file);
  else
    fprintf_unfiltered (file, "<unknown>");
  fprintf_unfiltered (file, ",");
  fprintf_unfiltered (file, "pc=");
  if (fi->next != NULL && fi->next->prev_pc.p)
    fprintf_unfiltered (file, "0x%s", paddr_nz (fi->next->prev_pc.value));
  else
    fprintf_unfiltered (file, "<unknown>");
  fprintf_unfiltered (file, ",");
  fprintf_unfiltered (file, "id=");
  if (fi->this_id.p)
    fprint_frame_id (file, fi->this_id.value);
  else
    fprintf_unfiltered (file, "<unknown>");
  fprintf_unfiltered (file, ",");
  fprintf_unfiltered (file, "func=");
  if (fi->next != NULL && fi->next->prev_func.p)
    fprintf_unfiltered (file, "0x%s", paddr_nz (fi->next->prev_func.addr));
  else
    fprintf_unfiltered (file, "<unknown>");
  fprintf_unfiltered (file, "}");
}

/* Return a frame uniq ID that can be used to, later, re-find the
   frame.  */

struct frame_id
get_frame_id (struct frame_info *fi)
{
  if (fi == NULL)
    {
      return null_frame_id;
    }
  if (!fi->this_id.p)
    {
      if (frame_debug)
	fprintf_unfiltered (gdb_stdlog, "{ get_frame_id (fi=%d) ",
			    fi->level);
      /* Find the unwinder.  */
      if (fi->unwind == NULL)
	fi->unwind = frame_unwind_find_by_frame (fi->next,
						 &fi->prologue_cache);
      /* Find THIS frame's ID.  */
      fi->unwind->this_id (fi->next, &fi->prologue_cache, &fi->this_id.value);
      fi->this_id.p = 1;
      if (frame_debug)
	{
	  fprintf_unfiltered (gdb_stdlog, "-> ");
	  fprint_frame_id (gdb_stdlog, fi->this_id.value);
	  fprintf_unfiltered (gdb_stdlog, " }\n");
	}
    }
  return fi->this_id.value;
}

struct frame_id
frame_unwind_id (struct frame_info *next_frame)
{
  /* Use prev_frame, and not get_prev_frame.  The latter will truncate
     the frame chain, leading to this function unintentionally
     returning a null_frame_id (e.g., when a caller requests the frame
     ID of "main()"s caller.  */
  return get_frame_id (get_prev_frame_1 (next_frame));
}

const struct frame_id null_frame_id; /* All zeros.  */

struct frame_id
frame_id_build_special (CORE_ADDR stack_addr, CORE_ADDR code_addr,
                        CORE_ADDR special_addr)
{
  struct frame_id id = null_frame_id;
  id.stack_addr = stack_addr;
  id.stack_addr_p = 1;
  id.code_addr = code_addr;
  id.code_addr_p = 1;
  id.special_addr = special_addr;
  id.special_addr_p = 1;
  return id;
}

struct frame_id
frame_id_build (CORE_ADDR stack_addr, CORE_ADDR code_addr)
{
  struct frame_id id = null_frame_id;
  id.stack_addr = stack_addr;
  id.stack_addr_p = 1;
  id.code_addr = code_addr;
  id.code_addr_p = 1;
  return id;
}

struct frame_id
frame_id_build_wild (CORE_ADDR stack_addr)
{
  struct frame_id id = null_frame_id;
  id.stack_addr = stack_addr;
  id.stack_addr_p = 1;
  return id;
}

int
frame_id_p (struct frame_id l)
{
  int p;
  /* The frame is valid iff it has a valid stack address.  */
  p = l.stack_addr_p;
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ frame_id_p (l=");
      fprint_frame_id (gdb_stdlog, l);
      fprintf_unfiltered (gdb_stdlog, ") -> %d }\n", p);
    }
  return p;
}

int
frame_id_eq (struct frame_id l, struct frame_id r)
{
  int eq;
  if (!l.stack_addr_p || !r.stack_addr_p)
    /* Like a NaN, if either ID is invalid, the result is false.
       Note that a frame ID is invalid iff it is the null frame ID.  */
    eq = 0;
  else if (l.stack_addr != r.stack_addr)
    /* If .stack addresses are different, the frames are different.  */
    eq = 0;
  else if (!l.code_addr_p || !r.code_addr_p)
    /* An invalid code addr is a wild card, always succeed.  */
    eq = 1;
  else if (l.code_addr != r.code_addr)
    /* If .code addresses are different, the frames are different.  */
    eq = 0;
  else if (!l.special_addr_p || !r.special_addr_p)
    /* An invalid special addr is a wild card (or unused), always succeed.  */
    eq = 1;
  else if (l.special_addr == r.special_addr)
    /* Frames are equal.  */
    eq = 1;
  else
    /* No luck.  */
    eq = 0;
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ frame_id_eq (l=");
      fprint_frame_id (gdb_stdlog, l);
      fprintf_unfiltered (gdb_stdlog, ",r=");
      fprint_frame_id (gdb_stdlog, r);
      fprintf_unfiltered (gdb_stdlog, ") -> %d }\n", eq);
    }
  return eq;
}

int
frame_id_inner (struct frame_id l, struct frame_id r)
{
  int inner;
  if (!l.stack_addr_p || !r.stack_addr_p)
    /* Like NaN, any operation involving an invalid ID always fails.  */
    inner = 0;
  else
    /* Only return non-zero when strictly inner than.  Note that, per
       comment in "frame.h", there is some fuzz here.  Frameless
       functions are not strictly inner than (same .stack but
       different .code and/or .special address).  */
    inner = gdbarch_inner_than (current_gdbarch, l.stack_addr, r.stack_addr);
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ frame_id_inner (l=");
      fprint_frame_id (gdb_stdlog, l);
      fprintf_unfiltered (gdb_stdlog, ",r=");
      fprint_frame_id (gdb_stdlog, r);
      fprintf_unfiltered (gdb_stdlog, ") -> %d }\n", inner);
    }
  return inner;
}

struct frame_info *
frame_find_by_id (struct frame_id id)
{
  struct frame_info *frame;

  /* ZERO denotes the null frame, let the caller decide what to do
     about it.  Should it instead return get_current_frame()?  */
  if (!frame_id_p (id))
    return NULL;

  for (frame = get_current_frame ();
       frame != NULL;
       frame = get_prev_frame (frame))
    {
      struct frame_id this = get_frame_id (frame);
      if (frame_id_eq (id, this))
	/* An exact match.  */
	return frame;
      if (frame_id_inner (id, this))
	/* Gone to far.  */
	return NULL;
      /* Either we're not yet gone far enough out along the frame
         chain (inner(this,id)), or we're comparing frameless functions
         (same .base, different .func, no test available).  Struggle
         on until we've definitly gone to far.  */
    }
  return NULL;
}

CORE_ADDR
frame_pc_unwind (struct frame_info *this_frame)
{
  if (!this_frame->prev_pc.p)
    {
      CORE_ADDR pc;
      if (this_frame->unwind == NULL)
	this_frame->unwind
	  = frame_unwind_find_by_frame (this_frame->next,
					&this_frame->prologue_cache);
      if (this_frame->unwind->prev_pc != NULL)
	/* A per-frame unwinder, prefer it.  */
	pc = this_frame->unwind->prev_pc (this_frame->next,
					  &this_frame->prologue_cache);
      else if (gdbarch_unwind_pc_p (current_gdbarch))
	{
	  /* The right way.  The `pure' way.  The one true way.  This
	     method depends solely on the register-unwind code to
	     determine the value of registers in THIS frame, and hence
	     the value of this frame's PC (resume address).  A typical
	     implementation is no more than:
	   
	     frame_unwind_register (this_frame, ISA_PC_REGNUM, buf);
	     return extract_unsigned_integer (buf, size of ISA_PC_REGNUM);

	     Note: this method is very heavily dependent on a correct
	     register-unwind implementation, it pays to fix that
	     method first; this method is frame type agnostic, since
	     it only deals with register values, it works with any
	     frame.  This is all in stark contrast to the old
	     FRAME_SAVED_PC which would try to directly handle all the
	     different ways that a PC could be unwound.  */
	  pc = gdbarch_unwind_pc (current_gdbarch, this_frame);
	}
      else
	internal_error (__FILE__, __LINE__, _("No unwind_pc method"));
      this_frame->prev_pc.value = pc;
      this_frame->prev_pc.p = 1;
      if (frame_debug)
	fprintf_unfiltered (gdb_stdlog,
			    "{ frame_pc_unwind (this_frame=%d) -> 0x%s }\n",
			    this_frame->level,
			    paddr_nz (this_frame->prev_pc.value));
    }
  return this_frame->prev_pc.value;
}

CORE_ADDR
frame_func_unwind (struct frame_info *fi, enum frame_type this_type)
{
  if (!fi->prev_func.p)
    {
      /* Make certain that this, and not the adjacent, function is
         found.  */
      CORE_ADDR addr_in_block = frame_unwind_address_in_block (fi, this_type);
      fi->prev_func.p = 1;
      fi->prev_func.addr = get_pc_function_start (addr_in_block);
      if (frame_debug)
	fprintf_unfiltered (gdb_stdlog,
			    "{ frame_func_unwind (fi=%d) -> 0x%s }\n",
			    fi->level, paddr_nz (fi->prev_func.addr));
    }
  return fi->prev_func.addr;
}

CORE_ADDR
get_frame_func (struct frame_info *fi)
{
  return frame_func_unwind (fi->next, get_frame_type (fi));
}

static int
do_frame_register_read (void *src, int regnum, gdb_byte *buf)
{
  frame_register_read (src, regnum, buf);
  return 1;
}

struct regcache *
frame_save_as_regcache (struct frame_info *this_frame)
{
  struct regcache *regcache = regcache_xmalloc (current_gdbarch);
  struct cleanup *cleanups = make_cleanup_regcache_xfree (regcache);
  regcache_save (regcache, do_frame_register_read, this_frame);
  discard_cleanups (cleanups);
  return regcache;
}

void
frame_pop (struct frame_info *this_frame)
{
  struct frame_info *prev_frame;
  struct regcache *scratch;
  struct cleanup *cleanups;

  /* Ensure that we have a frame to pop to.  */
  prev_frame = get_prev_frame_1 (this_frame);

  if (!prev_frame)
    error (_("Cannot pop the initial frame."));

  /* Make a copy of all the register values unwound from this frame.
     Save them in a scratch buffer so that there isn't a race between
     trying to extract the old values from the current regcache while
     at the same time writing new values into that same cache.  */
  scratch = frame_save_as_regcache (prev_frame);
  cleanups = make_cleanup_regcache_xfree (scratch);

  /* FIXME: cagney/2003-03-16: It should be possible to tell the
     target's register cache that it is about to be hit with a burst
     register transfer and that the sequence of register writes should
     be batched.  The pair target_prepare_to_store() and
     target_store_registers() kind of suggest this functionality.
     Unfortunately, they don't implement it.  Their lack of a formal
     definition can lead to targets writing back bogus values
     (arguably a bug in the target code mind).  */
  /* Now copy those saved registers into the current regcache.
     Here, regcache_cpy() calls regcache_restore().  */
  regcache_cpy (get_current_regcache (), scratch);
  do_cleanups (cleanups);

  /* We've made right mess of GDB's local state, just discard
     everything.  */
  reinit_frame_cache ();
}

void
frame_register_unwind (struct frame_info *frame, int regnum,
		       int *optimizedp, enum lval_type *lvalp,
		       CORE_ADDR *addrp, int *realnump, gdb_byte *bufferp)
{
  struct frame_unwind_cache *cache;

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "\
{ frame_register_unwind (frame=%d,regnum=%d(%s),...) ",
			  frame->level, regnum,
			  frame_map_regnum_to_name (frame, regnum));
    }

  /* Require all but BUFFERP to be valid.  A NULL BUFFERP indicates
     that the value proper does not need to be fetched.  */
  gdb_assert (optimizedp != NULL);
  gdb_assert (lvalp != NULL);
  gdb_assert (addrp != NULL);
  gdb_assert (realnump != NULL);
  /* gdb_assert (bufferp != NULL); */

  /* NOTE: cagney/2002-11-27: A program trying to unwind a NULL frame
     is broken.  There is always a frame.  If there, for some reason,
     isn't a frame, there is some pretty busted code as it should have
     detected the problem before calling here.  */
  gdb_assert (frame != NULL);

  /* Find the unwinder.  */
  if (frame->unwind == NULL)
    frame->unwind = frame_unwind_find_by_frame (frame->next,
						&frame->prologue_cache);

  /* Ask this frame to unwind its register.  See comment in
     "frame-unwind.h" for why NEXT frame and this unwind cache are
     passed in.  */
  frame->unwind->prev_register (frame->next, &frame->prologue_cache, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "->");
      fprintf_unfiltered (gdb_stdlog, " *optimizedp=%d", (*optimizedp));
      fprintf_unfiltered (gdb_stdlog, " *lvalp=%d", (int) (*lvalp));
      fprintf_unfiltered (gdb_stdlog, " *addrp=0x%s", paddr_nz ((*addrp)));
      fprintf_unfiltered (gdb_stdlog, " *bufferp=");
      if (bufferp == NULL)
	fprintf_unfiltered (gdb_stdlog, "<NULL>");
      else
	{
	  int i;
	  const unsigned char *buf = bufferp;
	  fprintf_unfiltered (gdb_stdlog, "[");
	  for (i = 0; i < register_size (current_gdbarch, regnum); i++)
	    fprintf_unfiltered (gdb_stdlog, "%02x", buf[i]);
	  fprintf_unfiltered (gdb_stdlog, "]");
	}
      fprintf_unfiltered (gdb_stdlog, " }\n");
    }
}

void
frame_register (struct frame_info *frame, int regnum,
		int *optimizedp, enum lval_type *lvalp,
		CORE_ADDR *addrp, int *realnump, gdb_byte *bufferp)
{
  /* Require all but BUFFERP to be valid.  A NULL BUFFERP indicates
     that the value proper does not need to be fetched.  */
  gdb_assert (optimizedp != NULL);
  gdb_assert (lvalp != NULL);
  gdb_assert (addrp != NULL);
  gdb_assert (realnump != NULL);
  /* gdb_assert (bufferp != NULL); */

  /* Obtain the register value by unwinding the register from the next
     (more inner frame).  */
  gdb_assert (frame != NULL && frame->next != NULL);
  frame_register_unwind (frame->next, regnum, optimizedp, lvalp, addrp,
			 realnump, bufferp);
}

void
frame_unwind_register (struct frame_info *frame, int regnum, gdb_byte *buf)
{
  int optimized;
  CORE_ADDR addr;
  int realnum;
  enum lval_type lval;
  frame_register_unwind (frame, regnum, &optimized, &lval, &addr,
			 &realnum, buf);
}

void
get_frame_register (struct frame_info *frame,
		    int regnum, gdb_byte *buf)
{
  frame_unwind_register (frame->next, regnum, buf);
}

LONGEST
frame_unwind_register_signed (struct frame_info *frame, int regnum)
{
  gdb_byte buf[MAX_REGISTER_SIZE];
  frame_unwind_register (frame, regnum, buf);
  return extract_signed_integer (buf, register_size (get_frame_arch (frame),
						     regnum));
}

LONGEST
get_frame_register_signed (struct frame_info *frame, int regnum)
{
  return frame_unwind_register_signed (frame->next, regnum);
}

ULONGEST
frame_unwind_register_unsigned (struct frame_info *frame, int regnum)
{
  gdb_byte buf[MAX_REGISTER_SIZE];
  frame_unwind_register (frame, regnum, buf);
  return extract_unsigned_integer (buf, register_size (get_frame_arch (frame),
						       regnum));
}

ULONGEST
get_frame_register_unsigned (struct frame_info *frame, int regnum)
{
  return frame_unwind_register_unsigned (frame->next, regnum);
}

void
frame_unwind_unsigned_register (struct frame_info *frame, int regnum,
				ULONGEST *val)
{
  gdb_byte buf[MAX_REGISTER_SIZE];
  frame_unwind_register (frame, regnum, buf);
  (*val) = extract_unsigned_integer (buf,
				     register_size (get_frame_arch (frame),
						    regnum));
}

void
put_frame_register (struct frame_info *frame, int regnum,
		    const gdb_byte *buf)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  int realnum;
  int optim;
  enum lval_type lval;
  CORE_ADDR addr;
  frame_register (frame, regnum, &optim, &lval, &addr, &realnum, NULL);
  if (optim)
    error (_("Attempt to assign to a value that was optimized out."));
  switch (lval)
    {
    case lval_memory:
      {
	/* FIXME: write_memory doesn't yet take constant buffers.
           Arrrg!  */
	gdb_byte tmp[MAX_REGISTER_SIZE];
	memcpy (tmp, buf, register_size (gdbarch, regnum));
	write_memory (addr, tmp, register_size (gdbarch, regnum));
	break;
      }
    case lval_register:
      regcache_cooked_write (get_current_regcache (), realnum, buf);
      break;
    default:
      error (_("Attempt to assign to an unmodifiable value."));
    }
}

/* frame_register_read ()

   Find and return the value of REGNUM for the specified stack frame.
   The number of bytes copied is REGISTER_SIZE (REGNUM).

   Returns 0 if the register value could not be found.  */

int
frame_register_read (struct frame_info *frame, int regnum,
		     gdb_byte *myaddr)
{
  int optimized;
  enum lval_type lval;
  CORE_ADDR addr;
  int realnum;
  frame_register (frame, regnum, &optimized, &lval, &addr, &realnum, myaddr);

  return !optimized;
}

int
get_frame_register_bytes (struct frame_info *frame, int regnum,
			  CORE_ADDR offset, int len, gdb_byte *myaddr)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);

  /* Skip registers wholly inside of OFFSET.  */
  while (offset >= register_size (gdbarch, regnum))
    {
      offset -= register_size (gdbarch, regnum);
      regnum++;
    }

  /* Copy the data.  */
  while (len > 0)
    {
      int curr_len = register_size (gdbarch, regnum) - offset;
      if (curr_len > len)
	curr_len = len;

      if (curr_len == register_size (gdbarch, regnum))
	{
	  if (!frame_register_read (frame, regnum, myaddr))
	    return 0;
	}
      else
	{
	  gdb_byte buf[MAX_REGISTER_SIZE];
	  if (!frame_register_read (frame, regnum, buf))
	    return 0;
	  memcpy (myaddr, buf + offset, curr_len);
	}

      myaddr += curr_len;
      len -= curr_len;
      offset = 0;
      regnum++;
    }

  return 1;
}

void
put_frame_register_bytes (struct frame_info *frame, int regnum,
			  CORE_ADDR offset, int len, const gdb_byte *myaddr)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);

  /* Skip registers wholly inside of OFFSET.  */
  while (offset >= register_size (gdbarch, regnum))
    {
      offset -= register_size (gdbarch, regnum);
      regnum++;
    }

  /* Copy the data.  */
  while (len > 0)
    {
      int curr_len = register_size (gdbarch, regnum) - offset;
      if (curr_len > len)
	curr_len = len;

      if (curr_len == register_size (gdbarch, regnum))
	{
	  put_frame_register (frame, regnum, myaddr);
	}
      else
	{
	  gdb_byte buf[MAX_REGISTER_SIZE];
	  frame_register_read (frame, regnum, buf);
	  memcpy (buf + offset, myaddr, curr_len);
	  put_frame_register (frame, regnum, buf);
	}

      myaddr += curr_len;
      len -= curr_len;
      offset = 0;
      regnum++;
    }
}

/* Map between a frame register number and its name.  A frame register
   space is a superset of the cooked register space --- it also
   includes builtin registers.  */

int
frame_map_name_to_regnum (struct frame_info *frame, const char *name, int len)
{
  return user_reg_map_name_to_regnum (get_frame_arch (frame), name, len);
}

const char *
frame_map_regnum_to_name (struct frame_info *frame, int regnum)
{
  return user_reg_map_regnum_to_name (get_frame_arch (frame), regnum);
}

/* Create a sentinel frame.  */

static struct frame_info *
create_sentinel_frame (struct regcache *regcache)
{
  struct frame_info *frame = FRAME_OBSTACK_ZALLOC (struct frame_info);
  frame->level = -1;
  /* Explicitly initialize the sentinel frame's cache.  Provide it
     with the underlying regcache.  In the future additional
     information, such as the frame's thread will be added.  */
  frame->prologue_cache = sentinel_frame_cache (regcache);
  /* For the moment there is only one sentinel frame implementation.  */
  frame->unwind = sentinel_frame_unwind;
  /* Link this frame back to itself.  The frame is self referential
     (the unwound PC is the same as the pc), so make it so.  */
  frame->next = frame;
  /* Make the sentinel frame's ID valid, but invalid.  That way all
     comparisons with it should fail.  */
  frame->this_id.p = 1;
  frame->this_id.value = null_frame_id;
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ create_sentinel_frame (...) -> ");
      fprint_frame (gdb_stdlog, frame);
      fprintf_unfiltered (gdb_stdlog, " }\n");
    }
  return frame;
}

/* Info about the innermost stack frame (contents of FP register) */

static struct frame_info *current_frame;

/* Cache for frame addresses already read by gdb.  Valid only while
   inferior is stopped.  Control variables for the frame cache should
   be local to this module.  */

static struct obstack frame_cache_obstack;

void *
frame_obstack_zalloc (unsigned long size)
{
  void *data = obstack_alloc (&frame_cache_obstack, size);
  memset (data, 0, size);
  return data;
}

/* Return the innermost (currently executing) stack frame.  This is
   split into two functions.  The function unwind_to_current_frame()
   is wrapped in catch exceptions so that, even when the unwind of the
   sentinel frame fails, the function still returns a stack frame.  */

static int
unwind_to_current_frame (struct ui_out *ui_out, void *args)
{
  struct frame_info *frame = get_prev_frame (args);
  /* A sentinel frame can fail to unwind, e.g., because its PC value
     lands in somewhere like start.  */
  if (frame == NULL)
    return 1;
  current_frame = frame;
  return 0;
}

struct frame_info *
get_current_frame (void)
{
  /* First check, and report, the lack of registers.  Having GDB
     report "No stack!" or "No memory" when the target doesn't even
     have registers is very confusing.  Besides, "printcmd.exp"
     explicitly checks that ``print $pc'' with no registers prints "No
     registers".  */
  if (!target_has_registers)
    error (_("No registers."));
  if (!target_has_stack)
    error (_("No stack."));
  if (!target_has_memory)
    error (_("No memory."));
  if (current_frame == NULL)
    {
      struct frame_info *sentinel_frame =
	create_sentinel_frame (get_current_regcache ());
      if (catch_exceptions (uiout, unwind_to_current_frame, sentinel_frame,
			    RETURN_MASK_ERROR) != 0)
	{
	  /* Oops! Fake a current frame?  Is this useful?  It has a PC
             of zero, for instance.  */
	  current_frame = sentinel_frame;
	}
    }
  return current_frame;
}

/* The "selected" stack frame is used by default for local and arg
   access.  May be zero, for no selected frame.  */

static struct frame_info *selected_frame;

/* Return the selected frame.  Always non-NULL (unless there isn't an
   inferior sufficient for creating a frame) in which case an error is
   thrown.  */

struct frame_info *
get_selected_frame (const char *message)
{
  if (selected_frame == NULL)
    {
      if (message != NULL && (!target_has_registers
			      || !target_has_stack
			      || !target_has_memory))
	error (("%s"), message);
      /* Hey!  Don't trust this.  It should really be re-finding the
	 last selected frame of the currently selected thread.  This,
	 though, is better than nothing.  */
      select_frame (get_current_frame ());
    }
  /* There is always a frame.  */
  gdb_assert (selected_frame != NULL);
  return selected_frame;
}

/* This is a variant of get_selected_frame() which can be called when
   the inferior does not have a frame; in that case it will return
   NULL instead of calling error().  */

struct frame_info *
deprecated_safe_get_selected_frame (void)
{
  if (!target_has_registers || !target_has_stack || !target_has_memory)
    return NULL;
  return get_selected_frame (NULL);
}

/* Select frame FI (or NULL - to invalidate the current frame).  */

void
select_frame (struct frame_info *fi)
{
  struct symtab *s;

  selected_frame = fi;
  /* NOTE: cagney/2002-05-04: FI can be NULL.  This occurs when the
     frame is being invalidated.  */
  if (deprecated_selected_frame_level_changed_hook)
    deprecated_selected_frame_level_changed_hook (frame_relative_level (fi));

  /* FIXME: kseitz/2002-08-28: It would be nice to call
     selected_frame_level_changed_event() right here, but due to limitations
     in the current interfaces, we would end up flooding UIs with events
     because select_frame() is used extensively internally.

     Once we have frame-parameterized frame (and frame-related) commands,
     the event notification can be moved here, since this function will only
     be called when the user's selected frame is being changed. */

  /* Ensure that symbols for this frame are read in.  Also, determine the
     source language of this frame, and switch to it if desired.  */
  if (fi)
    {
      /* We retrieve the frame's symtab by using the frame PC.  However
         we cannot use the frame PC as-is, because it usually points to
         the instruction following the "call", which is sometimes the
         first instruction of another function.  So we rely on
         get_frame_address_in_block() which provides us with a PC which
         is guaranteed to be inside the frame's code block.  */
      s = find_pc_symtab (get_frame_address_in_block (fi));
      if (s
	  && s->language != current_language->la_language
	  && s->language != language_unknown
	  && language_mode == language_mode_auto)
	{
	  set_language (s->language);
	}
    }
}
	
/* Create an arbitrary (i.e. address specified by user) or innermost frame.
   Always returns a non-NULL value.  */

struct frame_info *
create_new_frame (CORE_ADDR addr, CORE_ADDR pc)
{
  struct frame_info *fi;

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog,
			  "{ create_new_frame (addr=0x%s, pc=0x%s) ",
			  paddr_nz (addr), paddr_nz (pc));
    }

  fi = FRAME_OBSTACK_ZALLOC (struct frame_info);

  fi->next = create_sentinel_frame (get_current_regcache ());

  /* Select/initialize both the unwind function and the frame's type
     based on the PC.  */
  fi->unwind = frame_unwind_find_by_frame (fi->next, &fi->prologue_cache);

  fi->this_id.p = 1;
  deprecated_update_frame_base_hack (fi, addr);
  deprecated_update_frame_pc_hack (fi, pc);

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "-> ");
      fprint_frame (gdb_stdlog, fi);
      fprintf_unfiltered (gdb_stdlog, " }\n");
    }

  return fi;
}

/* Return the frame that THIS_FRAME calls (NULL if THIS_FRAME is the
   innermost frame).  Be careful to not fall off the bottom of the
   frame chain and onto the sentinel frame.  */

struct frame_info *
get_next_frame (struct frame_info *this_frame)
{
  if (this_frame->level > 0)
    return this_frame->next;
  else
    return NULL;
}

/* Observer for the target_changed event.  */

void
frame_observer_target_changed (struct target_ops *target)
{
  reinit_frame_cache ();
}

/* Flush the entire frame cache.  */

void
reinit_frame_cache (void)
{
  struct frame_info *fi;

  /* Tear down all frame caches.  */
  for (fi = current_frame; fi != NULL; fi = fi->prev)
    {
      if (fi->prologue_cache && fi->unwind->dealloc_cache)
	fi->unwind->dealloc_cache (fi, fi->prologue_cache);
      if (fi->base_cache && fi->base->unwind->dealloc_cache)
	fi->base->unwind->dealloc_cache (fi, fi->base_cache);
    }

  /* Since we can't really be sure what the first object allocated was */
  obstack_free (&frame_cache_obstack, 0);
  obstack_init (&frame_cache_obstack);

  current_frame = NULL;		/* Invalidate cache */
  select_frame (NULL);
  annotate_frames_invalid ();
  if (frame_debug)
    fprintf_unfiltered (gdb_stdlog, "{ reinit_frame_cache () }\n");
}

/* Find where a register is saved (in memory or another register).
   The result of frame_register_unwind is just where it is saved
   relative to this particular frame.  */

static void
frame_register_unwind_location (struct frame_info *this_frame, int regnum,
				int *optimizedp, enum lval_type *lvalp,
				CORE_ADDR *addrp, int *realnump)
{
  gdb_assert (this_frame == NULL || this_frame->level >= 0);

  while (this_frame != NULL)
    {
      frame_register_unwind (this_frame, regnum, optimizedp, lvalp,
			     addrp, realnump, NULL);

      if (*optimizedp)
	break;

      if (*lvalp != lval_register)
	break;

      regnum = *realnump;
      this_frame = get_next_frame (this_frame);
    }
}

/* Return a "struct frame_info" corresponding to the frame that called
   THIS_FRAME.  Returns NULL if there is no such frame.

   Unlike get_prev_frame, this function always tries to unwind the
   frame.  */

static struct frame_info *
get_prev_frame_1 (struct frame_info *this_frame)
{
  struct frame_info *prev_frame;
  struct frame_id this_id;

  gdb_assert (this_frame != NULL);

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ get_prev_frame_1 (this_frame=");
      if (this_frame != NULL)
	fprintf_unfiltered (gdb_stdlog, "%d", this_frame->level);
      else
	fprintf_unfiltered (gdb_stdlog, "<NULL>");
      fprintf_unfiltered (gdb_stdlog, ") ");
    }

  /* Only try to do the unwind once.  */
  if (this_frame->prev_p)
    {
      if (frame_debug)
	{
	  fprintf_unfiltered (gdb_stdlog, "-> ");
	  fprint_frame (gdb_stdlog, this_frame->prev);
	  fprintf_unfiltered (gdb_stdlog, " // cached \n");
	}
      return this_frame->prev;
    }
  this_frame->prev_p = 1;
  this_frame->stop_reason = UNWIND_NO_REASON;

  /* Check that this frame's ID was valid.  If it wasn't, don't try to
     unwind to the prev frame.  Be careful to not apply this test to
     the sentinel frame.  */
  this_id = get_frame_id (this_frame);
  if (this_frame->level >= 0 && !frame_id_p (this_id))
    {
      if (frame_debug)
	{
	  fprintf_unfiltered (gdb_stdlog, "-> ");
	  fprint_frame (gdb_stdlog, NULL);
	  fprintf_unfiltered (gdb_stdlog, " // this ID is NULL }\n");
	}
      this_frame->stop_reason = UNWIND_NULL_ID;
      return NULL;
    }

  /* Check that this frame's ID isn't inner to (younger, below, next)
     the next frame.  This happens when a frame unwind goes backwards.
     Exclude signal trampolines (due to sigaltstack the frame ID can
     go backwards) and sentinel frames (the test is meaningless).  */
  if (this_frame->next->level >= 0
      && this_frame->next->unwind->type != SIGTRAMP_FRAME
      && frame_id_inner (this_id, get_frame_id (this_frame->next)))
    {
      if (frame_debug)
	{
	  fprintf_unfiltered (gdb_stdlog, "-> ");
	  fprint_frame (gdb_stdlog, NULL);
	  fprintf_unfiltered (gdb_stdlog, " // this frame ID is inner }\n");
	}
      this_frame->stop_reason = UNWIND_INNER_ID;
      return NULL;
    }

  /* Check that this and the next frame are not identical.  If they
     are, there is most likely a stack cycle.  As with the inner-than
     test above, avoid comparing the inner-most and sentinel frames.  */
  if (this_frame->level > 0
      && frame_id_eq (this_id, get_frame_id (this_frame->next)))
    {
      if (frame_debug)
	{
	  fprintf_unfiltered (gdb_stdlog, "-> ");
	  fprint_frame (gdb_stdlog, NULL);
	  fprintf_unfiltered (gdb_stdlog, " // this frame has same ID }\n");
	}
      this_frame->stop_reason = UNWIND_SAME_ID;
      return NULL;
    }

  /* Check that this and the next frame do not unwind the PC register
     to the same memory location.  If they do, then even though they
     have different frame IDs, the new frame will be bogus; two
     functions can't share a register save slot for the PC.  This can
     happen when the prologue analyzer finds a stack adjustment, but
     no PC save.

     This check does assume that the "PC register" is roughly a
     traditional PC, even if the gdbarch_unwind_pc method adjusts
     it (we do not rely on the value, only on the unwound PC being
     dependent on this value).  A potential improvement would be
     to have the frame prev_pc method and the gdbarch unwind_pc
     method set the same lval and location information as
     frame_register_unwind.  */
  if (this_frame->level > 0
      && gdbarch_pc_regnum (current_gdbarch) >= 0
      && get_frame_type (this_frame) == NORMAL_FRAME
      && get_frame_type (this_frame->next) == NORMAL_FRAME)
    {
      int optimized, realnum;
      enum lval_type lval, nlval;
      CORE_ADDR addr, naddr;

      frame_register_unwind_location (this_frame,
				      gdbarch_pc_regnum (current_gdbarch),
				      &optimized, &lval, &addr, &realnum);
      frame_register_unwind_location (get_next_frame (this_frame),
				      gdbarch_pc_regnum (current_gdbarch),
				      &optimized, &nlval, &naddr, &realnum);

      if (lval == lval_memory && lval == nlval && addr == naddr)
	{
	  if (frame_debug)
	    {
	      fprintf_unfiltered (gdb_stdlog, "-> ");
	      fprint_frame (gdb_stdlog, NULL);
	      fprintf_unfiltered (gdb_stdlog, " // no saved PC }\n");
	    }

	  this_frame->stop_reason = UNWIND_NO_SAVED_PC;
	  this_frame->prev = NULL;
	  return NULL;
	}
    }

  /* Allocate the new frame but do not wire it in to the frame chain.
     Some (bad) code in INIT_FRAME_EXTRA_INFO tries to look along
     frame->next to pull some fancy tricks (of course such code is, by
     definition, recursive).  Try to prevent it.

     There is no reason to worry about memory leaks, should the
     remainder of the function fail.  The allocated memory will be
     quickly reclaimed when the frame cache is flushed, and the `we've
     been here before' check above will stop repeated memory
     allocation calls.  */
  prev_frame = FRAME_OBSTACK_ZALLOC (struct frame_info);
  prev_frame->level = this_frame->level + 1;

  /* Don't yet compute ->unwind (and hence ->type).  It is computed
     on-demand in get_frame_type, frame_register_unwind, and
     get_frame_id.  */

  /* Don't yet compute the frame's ID.  It is computed on-demand by
     get_frame_id().  */

  /* The unwound frame ID is validate at the start of this function,
     as part of the logic to decide if that frame should be further
     unwound, and not here while the prev frame is being created.
     Doing this makes it possible for the user to examine a frame that
     has an invalid frame ID.

     Some very old VAX code noted: [...]  For the sake of argument,
     suppose that the stack is somewhat trashed (which is one reason
     that "info frame" exists).  So, return 0 (indicating we don't
     know the address of the arglist) if we don't know what frame this
     frame calls.  */

  /* Link it in.  */
  this_frame->prev = prev_frame;
  prev_frame->next = this_frame;

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "-> ");
      fprint_frame (gdb_stdlog, prev_frame);
      fprintf_unfiltered (gdb_stdlog, " }\n");
    }

  return prev_frame;
}

/* Debug routine to print a NULL frame being returned.  */

static void
frame_debug_got_null_frame (struct ui_file *file,
			    struct frame_info *this_frame,
			    const char *reason)
{
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ get_prev_frame (this_frame=");
      if (this_frame != NULL)
	fprintf_unfiltered (gdb_stdlog, "%d", this_frame->level);
      else
	fprintf_unfiltered (gdb_stdlog, "<NULL>");
      fprintf_unfiltered (gdb_stdlog, ") -> // %s}\n", reason);
    }
}

/* Is this (non-sentinel) frame in the "main"() function?  */

static int
inside_main_func (struct frame_info *this_frame)
{
  struct minimal_symbol *msymbol;
  CORE_ADDR maddr;

  if (symfile_objfile == 0)
    return 0;
  msymbol = lookup_minimal_symbol (main_name (), NULL, symfile_objfile);
  if (msymbol == NULL)
    return 0;
  /* Make certain that the code, and not descriptor, address is
     returned.  */
  maddr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
					      SYMBOL_VALUE_ADDRESS (msymbol),
					      &current_target);
  return maddr == get_frame_func (this_frame);
}

/* Test whether THIS_FRAME is inside the process entry point function.  */

static int
inside_entry_func (struct frame_info *this_frame)
{
  return (get_frame_func (this_frame) == entry_point_address ());
}

/* Return a structure containing various interesting information about
   the frame that called THIS_FRAME.  Returns NULL if there is entier
   no such frame or the frame fails any of a set of target-independent
   condition that should terminate the frame chain (e.g., as unwinding
   past main()).

   This function should not contain target-dependent tests, such as
   checking whether the program-counter is zero.  */

struct frame_info *
get_prev_frame (struct frame_info *this_frame)
{
  struct frame_info *prev_frame;

  /* Return the inner-most frame, when the caller passes in NULL.  */
  /* NOTE: cagney/2002-11-09: Not sure how this would happen.  The
     caller should have previously obtained a valid frame using
     get_selected_frame() and then called this code - only possibility
     I can think of is code behaving badly.

     NOTE: cagney/2003-01-10: Talk about code behaving badly.  Check
     block_innermost_frame().  It does the sequence: frame = NULL;
     while (1) { frame = get_prev_frame (frame); .... }.  Ulgh!  Why
     it couldn't be written better, I don't know.

     NOTE: cagney/2003-01-11: I suspect what is happening in
     block_innermost_frame() is, when the target has no state
     (registers, memory, ...), it is still calling this function.  The
     assumption being that this function will return NULL indicating
     that a frame isn't possible, rather than checking that the target
     has state and then calling get_current_frame() and
     get_prev_frame().  This is a guess mind.  */
  if (this_frame == NULL)
    {
      /* NOTE: cagney/2002-11-09: There was a code segment here that
	 would error out when CURRENT_FRAME was NULL.  The comment
	 that went with it made the claim ...

	 ``This screws value_of_variable, which just wants a nice
	 clean NULL return from block_innermost_frame if there are no
	 frames.  I don't think I've ever seen this message happen
	 otherwise.  And returning NULL here is a perfectly legitimate
	 thing to do.''

         Per the above, this code shouldn't even be called with a NULL
         THIS_FRAME.  */
      frame_debug_got_null_frame (gdb_stdlog, this_frame, "this_frame NULL");
      return current_frame;
    }

  /* There is always a frame.  If this assertion fails, suspect that
     something should be calling get_selected_frame() or
     get_current_frame().  */
  gdb_assert (this_frame != NULL);

  /* tausq/2004-12-07: Dummy frames are skipped because it doesn't make much
     sense to stop unwinding at a dummy frame.  One place where a dummy
     frame may have an address "inside_main_func" is on HPUX.  On HPUX, the
     pcsqh register (space register for the instruction at the head of the
     instruction queue) cannot be written directly; the only way to set it
     is to branch to code that is in the target space.  In order to implement
     frame dummies on HPUX, the called function is made to jump back to where 
     the inferior was when the user function was called.  If gdb was inside 
     the main function when we created the dummy frame, the dummy frame will 
     point inside the main function.  */
  if (this_frame->level >= 0
      && get_frame_type (this_frame) != DUMMY_FRAME
      && !backtrace_past_main
      && inside_main_func (this_frame))
    /* Don't unwind past main().  Note, this is done _before_ the
       frame has been marked as previously unwound.  That way if the
       user later decides to enable unwinds past main(), that will
       automatically happen.  */
    {
      frame_debug_got_null_frame (gdb_stdlog, this_frame, "inside main func");
      return NULL;
    }

  /* If the user's backtrace limit has been exceeded, stop.  We must
     add two to the current level; one of those accounts for backtrace_limit
     being 1-based and the level being 0-based, and the other accounts for
     the level of the new frame instead of the level of the current
     frame.  */
  if (this_frame->level + 2 > backtrace_limit)
    {
      frame_debug_got_null_frame (gdb_stdlog, this_frame,
				  "backtrace limit exceeded");
      return NULL;
    }

  /* If we're already inside the entry function for the main objfile,
     then it isn't valid.  Don't apply this test to a dummy frame -
     dummy frame PCs typically land in the entry func.  Don't apply
     this test to the sentinel frame.  Sentinel frames should always
     be allowed to unwind.  */
  /* NOTE: cagney/2003-07-07: Fixed a bug in inside_main_func() -
     wasn't checking for "main" in the minimal symbols.  With that
     fixed asm-source tests now stop in "main" instead of halting the
     backtrace in weird and wonderful ways somewhere inside the entry
     file.  Suspect that tests for inside the entry file/func were
     added to work around that (now fixed) case.  */
  /* NOTE: cagney/2003-07-15: danielj (if I'm reading it right)
     suggested having the inside_entry_func test use the
     inside_main_func() msymbol trick (along with entry_point_address()
     I guess) to determine the address range of the start function.
     That should provide a far better stopper than the current
     heuristics.  */
  /* NOTE: tausq/2004-10-09: this is needed if, for example, the compiler
     applied tail-call optimizations to main so that a function called 
     from main returns directly to the caller of main.  Since we don't
     stop at main, we should at least stop at the entry point of the
     application.  */
  if (!backtrace_past_entry
      && get_frame_type (this_frame) != DUMMY_FRAME && this_frame->level >= 0
      && inside_entry_func (this_frame))
    {
      frame_debug_got_null_frame (gdb_stdlog, this_frame, "inside entry func");
      return NULL;
    }

  /* Assume that the only way to get a zero PC is through something
     like a SIGSEGV or a dummy frame, and hence that NORMAL frames
     will never unwind a zero PC.  */
  if (this_frame->level > 0
      && get_frame_type (this_frame) == NORMAL_FRAME
      && get_frame_type (get_next_frame (this_frame)) == NORMAL_FRAME
      && get_frame_pc (this_frame) == 0)
    {
      frame_debug_got_null_frame (gdb_stdlog, this_frame, "zero PC");
      return NULL;
    }

  return get_prev_frame_1 (this_frame);
}

CORE_ADDR
get_frame_pc (struct frame_info *frame)
{
  gdb_assert (frame->next != NULL);
  return frame_pc_unwind (frame->next);
}

/* Return an address that falls within NEXT_FRAME's caller's code
   block, assuming that the caller is a THIS_TYPE frame.  */

CORE_ADDR
frame_unwind_address_in_block (struct frame_info *next_frame,
			       enum frame_type this_type)
{
  /* A draft address.  */
  CORE_ADDR pc = frame_pc_unwind (next_frame);

  /* If NEXT_FRAME was called by a signal frame or dummy frame, then
     we shold not adjust the unwound PC.  These frames may not call
     their next frame in the normal way; the operating system or GDB
     may have pushed their resume address manually onto the stack, so
     it may be the very first instruction.  Even if the resume address
     was not manually pushed, they expect to be returned to.  */
  if (this_type != NORMAL_FRAME)
    return pc;

  /* If THIS frame is not inner most (i.e., NEXT isn't the sentinel),
     and NEXT is `normal' (i.e., not a sigtramp, dummy, ....) THIS
     frame's PC ends up pointing at the instruction fallowing the
     "call".  Adjust that PC value so that it falls on the call
     instruction (which, hopefully, falls within THIS frame's code
     block).  So far it's proved to be a very good approximation.  See
     get_frame_type() for why ->type can't be used.  */
  if (next_frame->level >= 0
      && get_frame_type (next_frame) == NORMAL_FRAME)
    --pc;
  return pc;
}

CORE_ADDR
get_frame_address_in_block (struct frame_info *this_frame)
{
  return frame_unwind_address_in_block (this_frame->next,
					get_frame_type (this_frame));
}

static int
pc_notcurrent (struct frame_info *frame)
{
  /* If FRAME is not the innermost frame, that normally means that
     FRAME->pc points at the return instruction (which is *after* the
     call instruction), and we want to get the line containing the
     call (because the call is where the user thinks the program is).
     However, if the next frame is either a SIGTRAMP_FRAME or a
     DUMMY_FRAME, then the next frame will contain a saved interrupt
     PC and such a PC indicates the current (rather than next)
     instruction/line, consequently, for such cases, want to get the
     line containing fi->pc.  */
  struct frame_info *next = get_next_frame (frame);
  int notcurrent = (next != NULL && get_frame_type (next) == NORMAL_FRAME);
  return notcurrent;
}

void
find_frame_sal (struct frame_info *frame, struct symtab_and_line *sal)
{
  (*sal) = find_pc_line (get_frame_pc (frame), pc_notcurrent (frame));
}

/* Per "frame.h", return the ``address'' of the frame.  Code should
   really be using get_frame_id().  */
CORE_ADDR
get_frame_base (struct frame_info *fi)
{
  return get_frame_id (fi).stack_addr;
}

/* High-level offsets into the frame.  Used by the debug info.  */

CORE_ADDR
get_frame_base_address (struct frame_info *fi)
{
  if (get_frame_type (fi) != NORMAL_FRAME)
    return 0;
  if (fi->base == NULL)
    fi->base = frame_base_find_by_frame (fi->next);
  /* Sneaky: If the low-level unwind and high-level base code share a
     common unwinder, let them share the prologue cache.  */
  if (fi->base->unwind == fi->unwind)
    return fi->base->this_base (fi->next, &fi->prologue_cache);
  return fi->base->this_base (fi->next, &fi->base_cache);
}

CORE_ADDR
get_frame_locals_address (struct frame_info *fi)
{
  void **cache;
  if (get_frame_type (fi) != NORMAL_FRAME)
    return 0;
  /* If there isn't a frame address method, find it.  */
  if (fi->base == NULL)
    fi->base = frame_base_find_by_frame (fi->next);
  /* Sneaky: If the low-level unwind and high-level base code share a
     common unwinder, let them share the prologue cache.  */
  if (fi->base->unwind == fi->unwind)
    cache = &fi->prologue_cache;
  else
    cache = &fi->base_cache;
  return fi->base->this_locals (fi->next, cache);
}

CORE_ADDR
get_frame_args_address (struct frame_info *fi)
{
  void **cache;
  if (get_frame_type (fi) != NORMAL_FRAME)
    return 0;
  /* If there isn't a frame address method, find it.  */
  if (fi->base == NULL)
    fi->base = frame_base_find_by_frame (fi->next);
  /* Sneaky: If the low-level unwind and high-level base code share a
     common unwinder, let them share the prologue cache.  */
  if (fi->base->unwind == fi->unwind)
    cache = &fi->prologue_cache;
  else
    cache = &fi->base_cache;
  return fi->base->this_args (fi->next, cache);
}

/* Level of the selected frame: 0 for innermost, 1 for its caller, ...
   or -1 for a NULL frame.  */

int
frame_relative_level (struct frame_info *fi)
{
  if (fi == NULL)
    return -1;
  else
    return fi->level;
}

enum frame_type
get_frame_type (struct frame_info *frame)
{
  if (frame->unwind == NULL)
    /* Initialize the frame's unwinder because that's what
       provides the frame's type.  */
    frame->unwind = frame_unwind_find_by_frame (frame->next, 
						&frame->prologue_cache);
  return frame->unwind->type;
}

void
deprecated_update_frame_pc_hack (struct frame_info *frame, CORE_ADDR pc)
{
  if (frame_debug)
    fprintf_unfiltered (gdb_stdlog,
			"{ deprecated_update_frame_pc_hack (frame=%d,pc=0x%s) }\n",
			frame->level, paddr_nz (pc));
  /* NOTE: cagney/2003-03-11: Some architectures (e.g., Arm) are
     maintaining a locally allocated frame object.  Since such frames
     are not in the frame chain, it isn't possible to assume that the
     frame has a next.  Sigh.  */
  if (frame->next != NULL)
    {
      /* While we're at it, update this frame's cached PC value, found
	 in the next frame.  Oh for the day when "struct frame_info"
	 is opaque and this hack on hack can just go away.  */
      frame->next->prev_pc.value = pc;
      frame->next->prev_pc.p = 1;
    }
}

void
deprecated_update_frame_base_hack (struct frame_info *frame, CORE_ADDR base)
{
  if (frame_debug)
    fprintf_unfiltered (gdb_stdlog,
			"{ deprecated_update_frame_base_hack (frame=%d,base=0x%s) }\n",
			frame->level, paddr_nz (base));
  /* See comment in "frame.h".  */
  frame->this_id.value.stack_addr = base;
}

/* Memory access methods.  */

void
get_frame_memory (struct frame_info *this_frame, CORE_ADDR addr,
		  gdb_byte *buf, int len)
{
  read_memory (addr, buf, len);
}

LONGEST
get_frame_memory_signed (struct frame_info *this_frame, CORE_ADDR addr,
			 int len)
{
  return read_memory_integer (addr, len);
}

ULONGEST
get_frame_memory_unsigned (struct frame_info *this_frame, CORE_ADDR addr,
			   int len)
{
  return read_memory_unsigned_integer (addr, len);
}

int
safe_frame_unwind_memory (struct frame_info *this_frame,
			  CORE_ADDR addr, gdb_byte *buf, int len)
{
  /* NOTE: read_memory_nobpt returns zero on success!  */
  return !read_memory_nobpt (addr, buf, len);
}

/* Architecture method.  */

struct gdbarch *
get_frame_arch (struct frame_info *this_frame)
{
  return current_gdbarch;
}

/* Stack pointer methods.  */

CORE_ADDR
get_frame_sp (struct frame_info *this_frame)
{
  return frame_sp_unwind (this_frame->next);
}

CORE_ADDR
frame_sp_unwind (struct frame_info *next_frame)
{
  /* Normality - an architecture that provides a way of obtaining any
     frame inner-most address.  */
  if (gdbarch_unwind_sp_p (current_gdbarch))
    return gdbarch_unwind_sp (current_gdbarch, next_frame);
  /* Now things are really are grim.  Hope that the value returned by
     the gdbarch_sp_regnum register is meaningful.  */
  if (gdbarch_sp_regnum (current_gdbarch) >= 0)
    {
      ULONGEST sp;
      frame_unwind_unsigned_register (next_frame,
				      gdbarch_sp_regnum (current_gdbarch), &sp);
      return sp;
    }
  internal_error (__FILE__, __LINE__, _("Missing unwind SP method"));
}

/* Return the reason why we can't unwind past FRAME.  */

enum unwind_stop_reason
get_frame_unwind_stop_reason (struct frame_info *frame)
{
  /* If we haven't tried to unwind past this point yet, then assume
     that unwinding would succeed.  */
  if (frame->prev_p == 0)
    return UNWIND_NO_REASON;

  /* Otherwise, we set a reason when we succeeded (or failed) to
     unwind.  */
  return frame->stop_reason;
}

/* Return a string explaining REASON.  */

const char *
frame_stop_reason_string (enum unwind_stop_reason reason)
{
  switch (reason)
    {
    case UNWIND_NULL_ID:
      return _("unwinder did not report frame ID");

    case UNWIND_INNER_ID:
      return _("previous frame inner to this frame (corrupt stack?)");

    case UNWIND_SAME_ID:
      return _("previous frame identical to this frame (corrupt stack?)");

    case UNWIND_NO_SAVED_PC:
      return _("frame did not save the PC");

    case UNWIND_NO_REASON:
    case UNWIND_FIRST_ERROR:
    default:
      internal_error (__FILE__, __LINE__,
		      "Invalid frame stop reason");
    }
}

extern initialize_file_ftype _initialize_frame; /* -Wmissing-prototypes */

static struct cmd_list_element *set_backtrace_cmdlist;
static struct cmd_list_element *show_backtrace_cmdlist;

static void
set_backtrace_cmd (char *args, int from_tty)
{
  help_list (set_backtrace_cmdlist, "set backtrace ", -1, gdb_stdout);
}

static void
show_backtrace_cmd (char *args, int from_tty)
{
  cmd_show_list (show_backtrace_cmdlist, from_tty, "");
}

void
_initialize_frame (void)
{
  obstack_init (&frame_cache_obstack);

  observer_attach_target_changed (frame_observer_target_changed);

  add_prefix_cmd ("backtrace", class_maintenance, set_backtrace_cmd, _("\
Set backtrace specific variables.\n\
Configure backtrace variables such as the backtrace limit"),
		  &set_backtrace_cmdlist, "set backtrace ",
		  0/*allow-unknown*/, &setlist);
  add_prefix_cmd ("backtrace", class_maintenance, show_backtrace_cmd, _("\
Show backtrace specific variables\n\
Show backtrace variables such as the backtrace limit"),
		  &show_backtrace_cmdlist, "show backtrace ",
		  0/*allow-unknown*/, &showlist);

  add_setshow_boolean_cmd ("past-main", class_obscure,
			   &backtrace_past_main, _("\
Set whether backtraces should continue past \"main\"."), _("\
Show whether backtraces should continue past \"main\"."), _("\
Normally the caller of \"main\" is not of interest, so GDB will terminate\n\
the backtrace at \"main\".  Set this variable if you need to see the rest\n\
of the stack trace."),
			   NULL,
			   show_backtrace_past_main,
			   &set_backtrace_cmdlist,
			   &show_backtrace_cmdlist);

  add_setshow_boolean_cmd ("past-entry", class_obscure,
			   &backtrace_past_entry, _("\
Set whether backtraces should continue past the entry point of a program."),
			   _("\
Show whether backtraces should continue past the entry point of a program."),
			   _("\
Normally there are no callers beyond the entry point of a program, so GDB\n\
will terminate the backtrace there.  Set this variable if you need to see \n\
the rest of the stack trace."),
			   NULL,
			   show_backtrace_past_entry,
			   &set_backtrace_cmdlist,
			   &show_backtrace_cmdlist);

  add_setshow_integer_cmd ("limit", class_obscure,
			   &backtrace_limit, _("\
Set an upper bound on the number of backtrace levels."), _("\
Show the upper bound on the number of backtrace levels."), _("\
No more than the specified number of frames can be displayed or examined.\n\
Zero is unlimited."),
			   NULL,
			   show_backtrace_limit,
			   &set_backtrace_cmdlist,
			   &show_backtrace_cmdlist);

  /* Debug this files internals. */
  add_setshow_zinteger_cmd ("frame", class_maintenance, &frame_debug,  _("\
Set frame debugging."), _("\
Show frame debugging."), _("\
When non-zero, frame specific internal debugging is enabled."),
			    NULL,
			    show_frame_debug,
			    &setdebuglist, &showdebuglist);
}
