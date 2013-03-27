/* Perform an inferior function call, for GDB, the GNU debugger.

   Copyright (C) 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
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
#include "breakpoint.h"
#include "target.h"
#include "regcache.h"
#include "inferior.h"
#include "gdb_assert.h"
#include "block.h"
#include "gdbcore.h"
#include "language.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "command.h"
#include "gdb_string.h"
#include "infcall.h"
#include "dummy-frame.h"

/* NOTE: cagney/2003-04-16: What's the future of this code?

   GDB needs an asynchronous expression evaluator, that means an
   asynchronous inferior function call implementation, and that in
   turn means restructuring the code so that it is event driven.  */

/* How you should pass arguments to a function depends on whether it
   was defined in K&R style or prototype style.  If you define a
   function using the K&R syntax that takes a `float' argument, then
   callers must pass that argument as a `double'.  If you define the
   function using the prototype syntax, then you must pass the
   argument as a `float', with no promotion.

   Unfortunately, on certain older platforms, the debug info doesn't
   indicate reliably how each function was defined.  A function type's
   TYPE_FLAG_PROTOTYPED flag may be clear, even if the function was
   defined in prototype style.  When calling a function whose
   TYPE_FLAG_PROTOTYPED flag is clear, GDB consults this flag to
   decide what to do.

   For modern targets, it is proper to assume that, if the prototype
   flag is clear, that can be trusted: `float' arguments should be
   promoted to `double'.  For some older targets, if the prototype
   flag is clear, that doesn't tell us anything.  The default is to
   trust the debug information; the user can override this behavior
   with "set coerce-float-to-double 0".  */

static int coerce_float_to_double_p = 1;
static void
show_coerce_float_to_double_p (struct ui_file *file, int from_tty,
			       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Coercion of floats to doubles when calling functions is %s.\n"),
		    value);
}

/* This boolean tells what gdb should do if a signal is received while
   in a function called from gdb (call dummy).  If set, gdb unwinds
   the stack and restore the context to what as it was before the
   call.

   The default is to stop in the frame where the signal was received. */

int unwind_on_signal_p = 0;
static void
show_unwind_on_signal_p (struct ui_file *file, int from_tty,
			 struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Unwinding of stack if a signal is received while in a call dummy is %s.\n"),
		    value);
}


/* Perform the standard coercions that are specified
   for arguments to be passed to C functions.

   If PARAM_TYPE is non-NULL, it is the expected parameter type.
   IS_PROTOTYPED is non-zero if the function declaration is prototyped.  */

static struct value *
value_arg_coerce (struct value *arg, struct type *param_type,
		  int is_prototyped)
{
  struct type *arg_type = check_typedef (value_type (arg));
  struct type *type
    = param_type ? check_typedef (param_type) : arg_type;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_REF:
      {
	struct value *new_value;

	if (TYPE_CODE (arg_type) == TYPE_CODE_REF)
	  return value_cast_pointers (type, arg);

	/* Cast the value to the reference's target type, and then
	   convert it back to a reference.  This will issue an error
	   if the value was not previously in memory - in some cases
	   we should clearly be allowing this, but how?  */
	new_value = value_cast (TYPE_TARGET_TYPE (type), arg);
	new_value = value_ref (new_value);
	return new_value;
      }
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_ENUM:
      /* If we don't have a prototype, coerce to integer type if necessary.  */
      if (!is_prototyped)
	{
	  if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
	    type = builtin_type_int;
	}
      /* Currently all target ABIs require at least the width of an integer
         type for an argument.  We may have to conditionalize the following
         type coercion for future targets.  */
      if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_int))
	type = builtin_type_int;
      break;
    case TYPE_CODE_FLT:
      if (!is_prototyped && coerce_float_to_double_p)
	{
	  if (TYPE_LENGTH (type) < TYPE_LENGTH (builtin_type_double))
	    type = builtin_type_double;
	  else if (TYPE_LENGTH (type) > TYPE_LENGTH (builtin_type_double))
	    type = builtin_type_long_double;
	}
      break;
    case TYPE_CODE_FUNC:
      type = lookup_pointer_type (type);
      break;
    case TYPE_CODE_ARRAY:
      /* Arrays are coerced to pointers to their first element, unless
         they are vectors, in which case we want to leave them alone,
         because they are passed by value.  */
      if (current_language->c_style_arrays)
	if (!TYPE_VECTOR (type))
	  type = lookup_pointer_type (TYPE_TARGET_TYPE (type));
      break;
    case TYPE_CODE_UNDEF:
    case TYPE_CODE_PTR:
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_VOID:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_STRING:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_MEMBERPTR:
    case TYPE_CODE_METHODPTR:
    case TYPE_CODE_METHOD:
    case TYPE_CODE_COMPLEX:
    default:
      break;
    }

  return value_cast (type, arg);
}

/* Determine a function's address and its return type from its value.
   Calls error() if the function is not valid for calling.  */

CORE_ADDR
find_function_addr (struct value *function, struct type **retval_type)
{
  struct type *ftype = check_typedef (value_type (function));
  enum type_code code = TYPE_CODE (ftype);
  struct type *value_type;
  CORE_ADDR funaddr;

  /* If it's a member function, just look at the function
     part of it.  */

  /* Determine address to call.  */
  if (code == TYPE_CODE_FUNC || code == TYPE_CODE_METHOD)
    {
      funaddr = VALUE_ADDRESS (function);
      value_type = TYPE_TARGET_TYPE (ftype);
    }
  else if (code == TYPE_CODE_PTR)
    {
      funaddr = value_as_address (function);
      ftype = check_typedef (TYPE_TARGET_TYPE (ftype));
      if (TYPE_CODE (ftype) == TYPE_CODE_FUNC
	  || TYPE_CODE (ftype) == TYPE_CODE_METHOD)
	{
	  funaddr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
							funaddr,
							&current_target);
	  value_type = TYPE_TARGET_TYPE (ftype);
	}
      else
	value_type = builtin_type_int;
    }
  else if (code == TYPE_CODE_INT)
    {
      /* Handle the case of functions lacking debugging info.
         Their values are characters since their addresses are char */
      if (TYPE_LENGTH (ftype) == 1)
	funaddr = value_as_address (value_addr (function));
      else
	{
	  /* Handle function descriptors lacking debug info.  */
	  int found_descriptor = 0;
	  if (VALUE_LVAL (function) == lval_memory)
	    {
	      CORE_ADDR nfunaddr;
	      funaddr = value_as_address (value_addr (function));
	      nfunaddr = funaddr;
	      funaddr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
							    funaddr,
							    &current_target);
	      if (funaddr != nfunaddr)
		found_descriptor = 1;
	    }
	  if (!found_descriptor)
	    /* Handle integer used as address of a function.  */
	    funaddr = (CORE_ADDR) value_as_long (function);
	}

      value_type = builtin_type_int;
    }
  else
    error (_("Invalid data type for function to be called."));

  if (retval_type != NULL)
    *retval_type = value_type;
  return funaddr + gdbarch_deprecated_function_start_offset (current_gdbarch);
}

/* Call breakpoint_auto_delete on the current contents of the bpstat
   pointed to by arg (which is really a bpstat *).  */

static void
breakpoint_auto_delete_contents (void *arg)
{
  breakpoint_auto_delete (*(bpstat *) arg);
}

static CORE_ADDR
generic_push_dummy_code (struct gdbarch *gdbarch,
			 CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc,
			 struct value **args, int nargs,
			 struct type *value_type,
			 CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
			 struct regcache *regcache)
{
  /* Something here to findout the size of a breakpoint and then
     allocate space for it on the stack.  */
  int bplen;
  /* This code assumes frame align.  */
  gdb_assert (gdbarch_frame_align_p (gdbarch));
  /* Force the stack's alignment.  The intent is to ensure that the SP
     is aligned to at least a breakpoint instruction's boundary.  */
  sp = gdbarch_frame_align (gdbarch, sp);
  /* Allocate space for, and then position the breakpoint on the
     stack.  */
  if (gdbarch_inner_than (gdbarch, 1, 2))
    {
      CORE_ADDR bppc = sp;
      gdbarch_breakpoint_from_pc (gdbarch, &bppc, &bplen);
      sp = gdbarch_frame_align (gdbarch, sp - bplen);
      (*bp_addr) = sp;
      /* Should the breakpoint size/location be re-computed here?  */
    }      
  else
    {
      (*bp_addr) = sp;
      gdbarch_breakpoint_from_pc (gdbarch, bp_addr, &bplen);
      sp = gdbarch_frame_align (gdbarch, sp + bplen);
    }
  /* Inferior resumes at the function entry point.  */
  (*real_pc) = funaddr;
  return sp;
}

/* For CALL_DUMMY_ON_STACK, push a breakpoint sequence that the called
   function returns to.  */

static CORE_ADDR
push_dummy_code (struct gdbarch *gdbarch,
		 CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc,
		 struct value **args, int nargs,
		 struct type *value_type,
		 CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
		 struct regcache *regcache)
{
  if (gdbarch_push_dummy_code_p (gdbarch))
    return gdbarch_push_dummy_code (gdbarch, sp, funaddr, using_gcc,
				    args, nargs, value_type, real_pc, bp_addr,
				    regcache);
  else    
    return generic_push_dummy_code (gdbarch, sp, funaddr, using_gcc,
				    args, nargs, value_type, real_pc, bp_addr,
				    regcache);
}

/* All this stuff with a dummy frame may seem unnecessarily complicated
   (why not just save registers in GDB?).  The purpose of pushing a dummy
   frame which looks just like a real frame is so that if you call a
   function and then hit a breakpoint (get a signal, etc), "backtrace"
   will look right.  Whether the backtrace needs to actually show the
   stack at the time the inferior function was called is debatable, but
   it certainly needs to not display garbage.  So if you are contemplating
   making dummy frames be different from normal frames, consider that.  */

/* Perform a function call in the inferior.
   ARGS is a vector of values of arguments (NARGS of them).
   FUNCTION is a value, the function to be called.
   Returns a value representing what the function returned.
   May fail to return, if a breakpoint or signal is hit
   during the execution of the function.

   ARGS is modified to contain coerced values. */

struct value *
call_function_by_hand (struct value *function, int nargs, struct value **args)
{
  CORE_ADDR sp;
  CORE_ADDR dummy_addr;
  struct type *values_type;
  unsigned char struct_return;
  CORE_ADDR struct_addr = 0;
  struct regcache *retbuf;
  struct cleanup *retbuf_cleanup;
  struct inferior_status *inf_status;
  struct cleanup *inf_status_cleanup;
  CORE_ADDR funaddr;
  int using_gcc;		/* Set to version of gcc in use, or zero if not gcc */
  CORE_ADDR real_pc;
  struct type *ftype = check_typedef (value_type (function));
  CORE_ADDR bp_addr;
  struct regcache *caller_regcache;
  struct cleanup *caller_regcache_cleanup;
  struct frame_id dummy_id;

  if (TYPE_CODE (ftype) == TYPE_CODE_PTR)
    ftype = check_typedef (TYPE_TARGET_TYPE (ftype));

  if (!target_has_execution)
    noprocess ();

  if (!gdbarch_push_dummy_call_p (current_gdbarch))
    error (_("This target does not support function calls"));

  /* Create a cleanup chain that contains the retbuf (buffer
     containing the register values).  This chain is create BEFORE the
     inf_status chain so that the inferior status can cleaned up
     (restored or discarded) without having the retbuf freed.  */
  retbuf = regcache_xmalloc (current_gdbarch);
  retbuf_cleanup = make_cleanup_regcache_xfree (retbuf);

  /* A cleanup for the inferior status.  Create this AFTER the retbuf
     so that this can be discarded or applied without interfering with
     the regbuf.  */
  inf_status = save_inferior_status (1);
  inf_status_cleanup = make_cleanup_restore_inferior_status (inf_status);

  /* Save the caller's registers so that they can be restored once the
     callee returns.  To allow nested calls the registers are (further
     down) pushed onto a dummy frame stack.  Include a cleanup (which
     is tossed once the regcache has been pushed).  */
  caller_regcache = frame_save_as_regcache (get_current_frame ());
  caller_regcache_cleanup = make_cleanup_regcache_xfree (caller_regcache);

  /* Ensure that the initial SP is correctly aligned.  */
  {
    CORE_ADDR old_sp = get_frame_sp (get_current_frame ());
    if (gdbarch_frame_align_p (current_gdbarch))
      {
	sp = gdbarch_frame_align (current_gdbarch, old_sp);
	/* NOTE: cagney/2003-08-13: Skip the "red zone".  For some
	   ABIs, a function can use memory beyond the inner most stack
	   address.  AMD64 called that region the "red zone".  Skip at
	   least the "red zone" size before allocating any space on
	   the stack.  */
	if (gdbarch_inner_than (current_gdbarch, 1, 2))
	  sp -= gdbarch_frame_red_zone_size (current_gdbarch);
	else
	  sp += gdbarch_frame_red_zone_size (current_gdbarch);
	/* Still aligned?  */
	gdb_assert (sp == gdbarch_frame_align (current_gdbarch, sp));
	/* NOTE: cagney/2002-09-18:
	   
	   On a RISC architecture, a void parameterless generic dummy
	   frame (i.e., no parameters, no result) typically does not
	   need to push anything the stack and hence can leave SP and
	   FP.  Similarly, a frameless (possibly leaf) function does
	   not push anything on the stack and, hence, that too can
	   leave FP and SP unchanged.  As a consequence, a sequence of
	   void parameterless generic dummy frame calls to frameless
	   functions will create a sequence of effectively identical
	   frames (SP, FP and TOS and PC the same).  This, not
	   suprisingly, results in what appears to be a stack in an
	   infinite loop --- when GDB tries to find a generic dummy
	   frame on the internal dummy frame stack, it will always
	   find the first one.

	   To avoid this problem, the code below always grows the
	   stack.  That way, two dummy frames can never be identical.
	   It does burn a few bytes of stack but that is a small price
	   to pay :-).  */
	if (sp == old_sp)
	  {
	    if (gdbarch_inner_than (current_gdbarch, 1, 2))
	      /* Stack grows down.  */
	      sp = gdbarch_frame_align (current_gdbarch, old_sp - 1);
	    else
	      /* Stack grows up.  */
	      sp = gdbarch_frame_align (current_gdbarch, old_sp + 1);
	  }
	gdb_assert ((gdbarch_inner_than (current_gdbarch, 1, 2)
		    && sp <= old_sp)
		    || (gdbarch_inner_than (current_gdbarch, 2, 1)
		       && sp >= old_sp));
      }
    else
      /* FIXME: cagney/2002-09-18: Hey, you loose!

	 Who knows how badly aligned the SP is!

	 If the generic dummy frame ends up empty (because nothing is
	 pushed) GDB won't be able to correctly perform back traces.
	 If a target is having trouble with backtraces, first thing to
	 do is add FRAME_ALIGN() to the architecture vector. If that
	 fails, try unwind_dummy_id().

         If the ABI specifies a "Red Zone" (see the doco) the code
         below will quietly trash it.  */
      sp = old_sp;
  }

  funaddr = find_function_addr (function, &values_type);
  CHECK_TYPEDEF (values_type);

  {
    struct block *b = block_for_pc (funaddr);
    /* If compiled without -g, assume GCC 2.  */
    using_gcc = (b == NULL ? 2 : BLOCK_GCC_COMPILED (b));
  }

  /* Are we returning a value using a structure return or a normal
     value return? */

  struct_return = using_struct_return (values_type, using_gcc);

  /* Determine the location of the breakpoint (and possibly other
     stuff) that the called function will return to.  The SPARC, for a
     function returning a structure or union, needs to make space for
     not just the breakpoint but also an extra word containing the
     size (?) of the structure being passed.  */

  /* The actual breakpoint (at BP_ADDR) is inserted separatly so there
     is no need to write that out.  */

  switch (gdbarch_call_dummy_location (current_gdbarch))
    {
    case ON_STACK:
      /* "dummy_addr" is here just to keep old targets happy.  New
	 targets return that same information via "sp" and "bp_addr".  */
      if (gdbarch_inner_than (current_gdbarch, 1, 2))
	{
	  sp = push_dummy_code (current_gdbarch, sp, funaddr,
				using_gcc, args, nargs, values_type,
				&real_pc, &bp_addr, get_current_regcache ());
	  dummy_addr = sp;
	}
      else
	{
	  dummy_addr = sp;
	  sp = push_dummy_code (current_gdbarch, sp, funaddr,
				using_gcc, args, nargs, values_type,
				&real_pc, &bp_addr, get_current_regcache ());
	}
      break;
    case AT_ENTRY_POINT:
      real_pc = funaddr;
      dummy_addr = entry_point_address ();
      /* Make certain that the address points at real code, and not a
         function descriptor.  */
      dummy_addr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
						       dummy_addr,
						       &current_target);
      /* A call dummy always consists of just a single breakpoint, so
         it's address is the same as the address of the dummy.  */
      bp_addr = dummy_addr;
      break;
    case AT_SYMBOL:
      /* Some executables define a symbol __CALL_DUMMY_ADDRESS whose
	 address is the location where the breakpoint should be
	 placed.  Once all targets are using the overhauled frame code
	 this can be deleted - ON_STACK is a better option.  */
      {
	struct minimal_symbol *sym;

	sym = lookup_minimal_symbol ("__CALL_DUMMY_ADDRESS", NULL, NULL);
	real_pc = funaddr;
	if (sym)
	  dummy_addr = SYMBOL_VALUE_ADDRESS (sym);
	else
	  dummy_addr = entry_point_address ();
	/* Make certain that the address points at real code, and not
	   a function descriptor.  */
	dummy_addr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
							 dummy_addr,
							 &current_target);
	/* A call dummy always consists of just a single breakpoint,
	   so it's address is the same as the address of the dummy.  */
	bp_addr = dummy_addr;
	break;
      }
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }

  if (nargs < TYPE_NFIELDS (ftype))
    error (_("too few arguments in function call"));

  {
    int i;
    for (i = nargs - 1; i >= 0; i--)
      {
	int prototyped;
	struct type *param_type;
	
	/* FIXME drow/2002-05-31: Should just always mark methods as
	   prototyped.  Can we respect TYPE_VARARGS?  Probably not.  */
	if (TYPE_CODE (ftype) == TYPE_CODE_METHOD)
	  prototyped = 1;
	else if (i < TYPE_NFIELDS (ftype))
	  prototyped = TYPE_PROTOTYPED (ftype);
	else
	  prototyped = 0;

	if (i < TYPE_NFIELDS (ftype))
	  param_type = TYPE_FIELD_TYPE (ftype, i);
	else
	  param_type = NULL;
	
	args[i] = value_arg_coerce (args[i], param_type, prototyped);

	/* elz: this code is to handle the case in which the function
	   to be called has a pointer to function as parameter and the
	   corresponding actual argument is the address of a function
	   and not a pointer to function variable.  In aCC compiled
	   code, the calls through pointers to functions (in the body
	   of the function called by hand) are made via
	   $$dyncall_external which requires some registers setting,
	   this is taken care of if we call via a function pointer
	   variable, but not via a function address.  In cc this is
	   not a problem. */

	if (using_gcc == 0)
	  {
	    if (param_type != NULL && TYPE_CODE (ftype) != TYPE_CODE_METHOD)
	      {
		/* if this parameter is a pointer to function.  */
		if (TYPE_CODE (param_type) == TYPE_CODE_PTR)
		  if (TYPE_CODE (TYPE_TARGET_TYPE (param_type)) == TYPE_CODE_FUNC)
		    /* elz: FIXME here should go the test about the
		       compiler used to compile the target. We want to
		       issue the error message only if the compiler
		       used was HP's aCC.  If we used HP's cc, then
		       there is no problem and no need to return at
		       this point.  */
		    /* Go see if the actual parameter is a variable of
		       type pointer to function or just a function.  */
		    if (VALUE_LVAL (args[i]) == not_lval)
		      {
			char *arg_name;
			/* NOTE: cagney/2005-01-02: THIS IS BOGUS.  */
			if (find_pc_partial_function ((CORE_ADDR) value_contents (args[i])[0], &arg_name, NULL, NULL))
			  error (_("\
You cannot use function <%s> as argument. \n\
You must use a pointer to function type variable. Command ignored."), arg_name);
		      }
	      }
	  }
      }
  }

  if (gdbarch_deprecated_reg_struct_has_addr_p (current_gdbarch))
    {
      int i;
      /* This is a machine like the sparc, where we may need to pass a
	 pointer to the structure, not the structure itself.  */
      for (i = nargs - 1; i >= 0; i--)
	{
	  struct type *arg_type = check_typedef (value_type (args[i]));
	  if ((TYPE_CODE (arg_type) == TYPE_CODE_STRUCT
	       || TYPE_CODE (arg_type) == TYPE_CODE_UNION
	       || TYPE_CODE (arg_type) == TYPE_CODE_ARRAY
	       || TYPE_CODE (arg_type) == TYPE_CODE_STRING
	       || TYPE_CODE (arg_type) == TYPE_CODE_BITSTRING
	       || TYPE_CODE (arg_type) == TYPE_CODE_SET
	       || (TYPE_CODE (arg_type) == TYPE_CODE_FLT
		   && TYPE_LENGTH (arg_type) > 8)
	       )
	      && gdbarch_deprecated_reg_struct_has_addr
		   (current_gdbarch, using_gcc, arg_type))
	    {
	      CORE_ADDR addr;
	      int len;		/*  = TYPE_LENGTH (arg_type); */
	      int aligned_len;
	      arg_type = check_typedef (value_enclosing_type (args[i]));
	      len = TYPE_LENGTH (arg_type);

	      aligned_len = len;
	      if (gdbarch_inner_than (current_gdbarch, 1, 2))
		{
		  /* stack grows downward */
		  sp -= aligned_len;
		  /* ... so the address of the thing we push is the
		     stack pointer after we push it.  */
		  addr = sp;
		}
	      else
		{
		  /* The stack grows up, so the address of the thing
		     we push is the stack pointer before we push it.  */
		  addr = sp;
		  sp += aligned_len;
		}
	      /* Push the structure.  */
	      write_memory (addr, value_contents_all (args[i]), len);
	      /* The value we're going to pass is the address of the
		 thing we just pushed.  */
	      /*args[i] = value_from_longest (lookup_pointer_type (values_type),
		(LONGEST) addr); */
	      args[i] = value_from_pointer (lookup_pointer_type (arg_type),
					    addr);
	    }
	}
    }


  /* Reserve space for the return structure to be written on the
     stack, if necessary.  Make certain that the value is correctly
     aligned. */

  if (struct_return)
    {
      int len = TYPE_LENGTH (values_type);
      if (gdbarch_inner_than (current_gdbarch, 1, 2))
	{
	  /* Stack grows downward.  Align STRUCT_ADDR and SP after
             making space for the return value.  */
	  sp -= len;
	  if (gdbarch_frame_align_p (current_gdbarch))
	    sp = gdbarch_frame_align (current_gdbarch, sp);
	  struct_addr = sp;
	}
      else
	{
	  /* Stack grows upward.  Align the frame, allocate space, and
             then again, re-align the frame??? */
	  if (gdbarch_frame_align_p (current_gdbarch))
	    sp = gdbarch_frame_align (current_gdbarch, sp);
	  struct_addr = sp;
	  sp += len;
	  if (gdbarch_frame_align_p (current_gdbarch))
	    sp = gdbarch_frame_align (current_gdbarch, sp);
	}
    }

  /* Create the dummy stack frame.  Pass in the call dummy address as,
     presumably, the ABI code knows where, in the call dummy, the
     return address should be pointed.  */
  sp = gdbarch_push_dummy_call (current_gdbarch, function,
				get_current_regcache (), bp_addr, nargs, args,
				sp, struct_return, struct_addr);

  /* Set up a frame ID for the dummy frame so we can pass it to
     set_momentary_breakpoint.  We need to give the breakpoint a frame
     ID so that the breakpoint code can correctly re-identify the
     dummy breakpoint.  */
  /* Sanity.  The exact same SP value is returned by PUSH_DUMMY_CALL,
     saved as the dummy-frame TOS, and used by unwind_dummy_id to form
     the frame ID's stack address.  */
  dummy_id = frame_id_build (sp, bp_addr);

  /* Create a momentary breakpoint at the return address of the
     inferior.  That way it breaks when it returns.  */

  {
    struct breakpoint *bpt;
    struct symtab_and_line sal;
    init_sal (&sal);		/* initialize to zeroes */
    sal.pc = bp_addr;
    sal.section = find_pc_overlay (sal.pc);
    /* Sanity.  The exact same SP value is returned by
       PUSH_DUMMY_CALL, saved as the dummy-frame TOS, and used by
       unwind_dummy_id to form the frame ID's stack address.  */
    bpt = set_momentary_breakpoint (sal, dummy_id, bp_call_dummy);
    bpt->disposition = disp_del;
  }

  /* Everything's ready, push all the info needed to restore the
     caller (and identify the dummy-frame) onto the dummy-frame
     stack.  */
  dummy_frame_push (caller_regcache, &dummy_id);
  discard_cleanups (caller_regcache_cleanup);

  /* - SNIP - SNIP - SNIP - SNIP - SNIP - SNIP - SNIP - SNIP - SNIP -
     If you're looking to implement asynchronous dummy-frames, then
     just below is the place to chop this function in two..  */

  /* Now proceed, having reached the desired place.  */
  clear_proceed_status ();
    
  /* Execute a "stack dummy", a piece of code stored in the stack by
     the debugger to be executed in the inferior.

     The dummy's frame is automatically popped whenever that break is
     hit.  If that is the first time the program stops,
     call_function_by_hand returns to its caller with that frame
     already gone and sets RC to 0.
   
     Otherwise, set RC to a non-zero value.  If the called function
     receives a random signal, we do not allow the user to continue
     executing it as this may not work.  The dummy frame is poped and
     we return 1.  If we hit a breakpoint, we leave the frame in place
     and return 2 (the frame will eventually be popped when we do hit
     the dummy end breakpoint).  */

  {
    struct cleanup *old_cleanups = make_cleanup (null_cleanup, 0);
    int saved_async = 0;

    /* If all error()s out of proceed ended up calling normal_stop
       (and perhaps they should; it already does in the special case
       of error out of resume()), then we wouldn't need this.  */
    make_cleanup (breakpoint_auto_delete_contents, &stop_bpstat);

    disable_watchpoints_before_interactive_call_start ();
    proceed_to_finish = 1;	/* We want stop_registers, please... */

    if (target_can_async_p ())
      saved_async = target_async_mask (0);
    
    proceed (real_pc, TARGET_SIGNAL_0, 0);
    
    if (saved_async)
      target_async_mask (saved_async);
    
    enable_watchpoints_after_interactive_call_stop ();
      
    discard_cleanups (old_cleanups);
  }

  if (stopped_by_random_signal || !stop_stack_dummy)
    {
      /* Find the name of the function we're about to complain about.  */
      const char *name = NULL;
      {
	struct symbol *symbol = find_pc_function (funaddr);
	if (symbol)
	  name = SYMBOL_PRINT_NAME (symbol);
	else
	  {
	    /* Try the minimal symbols.  */
	    struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (funaddr);
	    if (msymbol)
	      name = SYMBOL_PRINT_NAME (msymbol);
	  }
	if (name == NULL)
	  {
	    /* Can't use a cleanup here.  It is discarded, instead use
               an alloca.  */
	    char *tmp = xstrprintf ("at %s", hex_string (funaddr));
	    char *a = alloca (strlen (tmp) + 1);
	    strcpy (a, tmp);
	    xfree (tmp);
	    name = a;
	  }
      }
      if (stopped_by_random_signal)
	{
	  /* We stopped inside the FUNCTION because of a random
	     signal.  Further execution of the FUNCTION is not
	     allowed. */

	  if (unwind_on_signal_p)
	    {
	      /* The user wants the context restored. */

	      /* We must get back to the frame we were before the
		 dummy call. */
	      frame_pop (get_current_frame ());

	      /* FIXME: Insert a bunch of wrap_here; name can be very
		 long if it's a C++ name with arguments and stuff.  */
	      error (_("\
The program being debugged was signaled while in a function called from GDB.\n\
GDB has restored the context to what it was before the call.\n\
To change this behavior use \"set unwindonsignal off\"\n\
Evaluation of the expression containing the function (%s) will be abandoned."),
		     name);
	    }
	  else
	    {
	      /* The user wants to stay in the frame where we stopped
                 (default).*/
	      /* If we restored the inferior status (via the cleanup),
		 we would print a spurious error message (Unable to
		 restore previously selected frame), would write the
		 registers from the inf_status (which is wrong), and
		 would do other wrong things.  */
	      discard_cleanups (inf_status_cleanup);
	      discard_inferior_status (inf_status);
	      /* FIXME: Insert a bunch of wrap_here; name can be very
		 long if it's a C++ name with arguments and stuff.  */
	      error (_("\
The program being debugged was signaled while in a function called from GDB.\n\
GDB remains in the frame where the signal was received.\n\
To change this behavior use \"set unwindonsignal on\"\n\
Evaluation of the expression containing the function (%s) will be abandoned."),
		     name);
	    }
	}

      if (!stop_stack_dummy)
	{
	  /* We hit a breakpoint inside the FUNCTION. */
	  /* If we restored the inferior status (via the cleanup), we
	     would print a spurious error message (Unable to restore
	     previously selected frame), would write the registers
	     from the inf_status (which is wrong), and would do other
	     wrong things.  */
	  discard_cleanups (inf_status_cleanup);
	  discard_inferior_status (inf_status);
	  /* The following error message used to say "The expression
	     which contained the function call has been discarded."
	     It is a hard concept to explain in a few words.  Ideally,
	     GDB would be able to resume evaluation of the expression
	     when the function finally is done executing.  Perhaps
	     someday this will be implemented (it would not be easy).  */
	  /* FIXME: Insert a bunch of wrap_here; name can be very long if it's
	     a C++ name with arguments and stuff.  */
	  error (_("\
The program being debugged stopped while in a function called from GDB.\n\
When the function (%s) is done executing, GDB will silently\n\
stop (instead of continuing to evaluate the expression containing\n\
the function call)."), name);
	}

      /* The above code errors out, so ...  */
      internal_error (__FILE__, __LINE__, _("... should not be here"));
    }

  /* If we get here the called FUNCTION run to completion. */

  /* On normal return, the stack dummy has been popped already.  */
  regcache_cpy_no_passthrough (retbuf, stop_registers);

  /* Restore the inferior status, via its cleanup.  At this stage,
     leave the RETBUF alone.  */
  do_cleanups (inf_status_cleanup);

  /* Figure out the value returned by the function.  */
  {
    struct value *retval = NULL;

    if (TYPE_CODE (values_type) == TYPE_CODE_VOID)
      {
	/* If the function returns void, don't bother fetching the
	   return value.  */
	retval = allocate_value (values_type);
      }
    else
      {
	struct gdbarch *arch = current_gdbarch;

	switch (gdbarch_return_value (arch, values_type, NULL, NULL, NULL))
	  {
	  case RETURN_VALUE_REGISTER_CONVENTION:
	  case RETURN_VALUE_ABI_RETURNS_ADDRESS:
	  case RETURN_VALUE_ABI_PRESERVES_ADDRESS:
	    retval = allocate_value (values_type);
	    gdbarch_return_value (current_gdbarch, values_type, retbuf,
				  value_contents_raw (retval), NULL);
	    break;
	  case RETURN_VALUE_STRUCT_CONVENTION:
	    retval = value_at (values_type, struct_addr);
	    break;
	  }
      }

    do_cleanups (retbuf_cleanup);

    gdb_assert(retval);
    return retval;
  }
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_infcall (void);

void
_initialize_infcall (void)
{
  add_setshow_boolean_cmd ("coerce-float-to-double", class_obscure,
			   &coerce_float_to_double_p, _("\
Set coercion of floats to doubles when calling functions."), _("\
Show coercion of floats to doubles when calling functions"), _("\
Variables of type float should generally be converted to doubles before\n\
calling an unprototyped function, and left alone when calling a prototyped\n\
function.  However, some older debug info formats do not provide enough\n\
information to determine that a function is prototyped.  If this flag is\n\
set, GDB will perform the conversion for a function it considers\n\
unprototyped.\n\
The default is to perform the conversion.\n"),
			   NULL,
			   show_coerce_float_to_double_p,
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("unwindonsignal", no_class,
			   &unwind_on_signal_p, _("\
Set unwinding of stack if a signal is received while in a call dummy."), _("\
Show unwinding of stack if a signal is received while in a call dummy."), _("\
The unwindonsignal lets the user determine what gdb should do if a signal\n\
is received while in a function called from gdb (call dummy).  If set, gdb\n\
unwinds the stack and restore the context to what as it was before the call.\n\
The default is to stop in the frame where the signal was received."),
			   NULL,
			   show_unwind_on_signal_p,
			   &setlist, &showlist);
}
