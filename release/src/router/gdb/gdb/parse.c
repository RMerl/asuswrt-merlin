/* Parse expressions for GDB.

   Copyright (C) 1986, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2004, 2005, 2007 Free Software Foundation, Inc.

   Modified from expread.y by the Department of Computer Science at the
   State University of New York at Buffalo, 1991.

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

/* Parse an expression from text in a string,
   and return the result as a  struct expression  pointer.
   That structure contains arithmetic operations in reverse polish,
   with constants represented by operations that are followed by special data.
   See expression.h for the details of the format.
   What is important here is that it can be built up sequentially
   during the process of parsing; the lower levels of the tree always
   come first in the result.  */

#include <ctype.h>

#include "defs.h"
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "expression.h"
#include "value.h"
#include "command.h"
#include "language.h"
#include "f-lang.h"
#include "parser-defs.h"
#include "gdbcmd.h"
#include "symfile.h"		/* for overlay functions */
#include "inferior.h"
#include "doublest.h"
#include "gdb_assert.h"
#include "block.h"
#include "source.h"
#include "objfiles.h"

/* Standard set of definitions for printing, dumping, prefixifying,
 * and evaluating expressions.  */

const struct exp_descriptor exp_descriptor_standard = 
  {
    print_subexp_standard,
    operator_length_standard,
    op_name_standard,
    dump_subexp_body_standard,
    evaluate_subexp_standard
  };

/* Global variables declared in parser-defs.h (and commented there).  */
struct expression *expout;
int expout_size;
int expout_ptr;
struct block *expression_context_block;
CORE_ADDR expression_context_pc;
struct block *innermost_block;
int arglist_len;
union type_stack_elt *type_stack;
int type_stack_depth, type_stack_size;
char *lexptr;
char *prev_lexptr;
int paren_depth;
int comma_terminates;

/* A temporary buffer for identifiers, so we can null-terminate them.

   We allocate this with xrealloc.  parse_exp_1 used to allocate with
   alloca, using the size of the whole expression as a conservative
   estimate of the space needed.  However, macro expansion can
   introduce names longer than the original expression; there's no
   practical way to know beforehand how large that might be.  */
char *namecopy;
size_t namecopy_size;

static int expressiondebug = 0;
static void
show_expressiondebug (struct ui_file *file, int from_tty,
		      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Expression debugging is %s.\n"), value);
}

static void free_funcalls (void *ignore);

static void prefixify_expression (struct expression *);

static void prefixify_subexp (struct expression *, struct expression *, int,
			      int);

static struct expression *parse_exp_in_context (char **, struct block *, int, 
						int);

void _initialize_parse (void);

/* Data structure for saving values of arglist_len for function calls whose
   arguments contain other function calls.  */

struct funcall
  {
    struct funcall *next;
    int arglist_len;
  };

static struct funcall *funcall_chain;

/* Begin counting arguments for a function call,
   saving the data about any containing call.  */

void
start_arglist (void)
{
  struct funcall *new;

  new = (struct funcall *) xmalloc (sizeof (struct funcall));
  new->next = funcall_chain;
  new->arglist_len = arglist_len;
  arglist_len = 0;
  funcall_chain = new;
}

/* Return the number of arguments in a function call just terminated,
   and restore the data for the containing function call.  */

int
end_arglist (void)
{
  int val = arglist_len;
  struct funcall *call = funcall_chain;
  funcall_chain = call->next;
  arglist_len = call->arglist_len;
  xfree (call);
  return val;
}

/* Free everything in the funcall chain.
   Used when there is an error inside parsing.  */

static void
free_funcalls (void *ignore)
{
  struct funcall *call, *next;

  for (call = funcall_chain; call; call = next)
    {
      next = call->next;
      xfree (call);
    }
}

/* This page contains the functions for adding data to the  struct expression
   being constructed.  */

/* Add one element to the end of the expression.  */

/* To avoid a bug in the Sun 4 compiler, we pass things that can fit into
   a register through here */

void
write_exp_elt (union exp_element expelt)
{
  if (expout_ptr >= expout_size)
    {
      expout_size *= 2;
      expout = (struct expression *)
	xrealloc ((char *) expout, sizeof (struct expression)
		  + EXP_ELEM_TO_BYTES (expout_size));
    }
  expout->elts[expout_ptr++] = expelt;
}

void
write_exp_elt_opcode (enum exp_opcode expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.opcode = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_sym (struct symbol *expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.symbol = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_block (struct block *b)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));
  tmp.block = b;
  write_exp_elt (tmp);
}

void
write_exp_elt_objfile (struct objfile *objfile)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));
  tmp.objfile = objfile;
  write_exp_elt (tmp);
}

void
write_exp_elt_longcst (LONGEST expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.longconst = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_dblcst (DOUBLEST expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.doubleconst = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_type (struct type *expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.type = expelt;

  write_exp_elt (tmp);
}

void
write_exp_elt_intern (struct internalvar *expelt)
{
  union exp_element tmp;
  memset (&tmp, 0, sizeof (union exp_element));

  tmp.internalvar = expelt;

  write_exp_elt (tmp);
}

/* Add a string constant to the end of the expression.

   String constants are stored by first writing an expression element
   that contains the length of the string, then stuffing the string
   constant itself into however many expression elements are needed
   to hold it, and then writing another expression element that contains
   the length of the string.  I.E. an expression element at each end of
   the string records the string length, so you can skip over the 
   expression elements containing the actual string bytes from either
   end of the string.  Note that this also allows gdb to handle
   strings with embedded null bytes, as is required for some languages.

   Don't be fooled by the fact that the string is null byte terminated,
   this is strictly for the convenience of debugging gdb itself.  Gdb
   Gdb does not depend up the string being null terminated, since the
   actual length is recorded in expression elements at each end of the
   string.  The null byte is taken into consideration when computing how
   many expression elements are required to hold the string constant, of
   course. */


void
write_exp_string (struct stoken str)
{
  int len = str.length;
  int lenelt;
  char *strdata;

  /* Compute the number of expression elements required to hold the string
     (including a null byte terminator), along with one expression element
     at each end to record the actual string length (not including the
     null byte terminator). */

  lenelt = 2 + BYTES_TO_EXP_ELEM (len + 1);

  /* Ensure that we have enough available expression elements to store
     everything. */

  if ((expout_ptr + lenelt) >= expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + lenelt + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  /* Write the leading length expression element (which advances the current
     expression element index), then write the string constant followed by a
     terminating null byte, and then write the trailing length expression
     element. */

  write_exp_elt_longcst ((LONGEST) len);
  strdata = (char *) &expout->elts[expout_ptr];
  memcpy (strdata, str.ptr, len);
  *(strdata + len) = '\0';
  expout_ptr += lenelt - 2;
  write_exp_elt_longcst ((LONGEST) len);
}

/* Add a bitstring constant to the end of the expression.

   Bitstring constants are stored by first writing an expression element
   that contains the length of the bitstring (in bits), then stuffing the
   bitstring constant itself into however many expression elements are
   needed to hold it, and then writing another expression element that
   contains the length of the bitstring.  I.E. an expression element at
   each end of the bitstring records the bitstring length, so you can skip
   over the expression elements containing the actual bitstring bytes from
   either end of the bitstring. */

void
write_exp_bitstring (struct stoken str)
{
  int bits = str.length;	/* length in bits */
  int len = (bits + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
  int lenelt;
  char *strdata;

  /* Compute the number of expression elements required to hold the bitstring,
     along with one expression element at each end to record the actual
     bitstring length in bits. */

  lenelt = 2 + BYTES_TO_EXP_ELEM (len);

  /* Ensure that we have enough available expression elements to store
     everything. */

  if ((expout_ptr + lenelt) >= expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + lenelt + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  /* Write the leading length expression element (which advances the current
     expression element index), then write the bitstring constant, and then
     write the trailing length expression element. */

  write_exp_elt_longcst ((LONGEST) bits);
  strdata = (char *) &expout->elts[expout_ptr];
  memcpy (strdata, str.ptr, len);
  expout_ptr += lenelt - 2;
  write_exp_elt_longcst ((LONGEST) bits);
}

/* Add the appropriate elements for a minimal symbol to the end of
   the expression.  The rationale behind passing in text_symbol_type and
   data_symbol_type was so that Modula-2 could pass in WORD for
   data_symbol_type.  Perhaps it still is useful to have those types vary
   based on the language, but they no longer have names like "int", so
   the initial rationale is gone.  */

void
write_exp_msymbol (struct minimal_symbol *msymbol, 
		   struct type *text_symbol_type, 
		   struct type *data_symbol_type)
{
  struct gdbarch *gdbarch = current_gdbarch;
  CORE_ADDR addr;

  write_exp_elt_opcode (OP_LONG);
  /* Let's make the type big enough to hold a 64-bit address.  */
  write_exp_elt_type (builtin_type_CORE_ADDR);

  addr = SYMBOL_VALUE_ADDRESS (msymbol);
  if (overlay_debugging)
    addr = symbol_overlayed_address (addr, SYMBOL_BFD_SECTION (msymbol));
  write_exp_elt_longcst ((LONGEST) addr);

  write_exp_elt_opcode (OP_LONG);

  if (SYMBOL_BFD_SECTION (msymbol)
      && SYMBOL_BFD_SECTION (msymbol)->flags & SEC_THREAD_LOCAL)
    {
      bfd *bfd = SYMBOL_BFD_SECTION (msymbol)->owner;
      struct objfile *ofp;

      ALL_OBJFILES (ofp)
	if (ofp->obfd == bfd)
	  break;

      write_exp_elt_opcode (UNOP_MEMVAL_TLS);
      write_exp_elt_objfile (ofp);
      write_exp_elt_type (builtin_type (gdbarch)->nodebug_tls_symbol);
      write_exp_elt_opcode (UNOP_MEMVAL_TLS);
      return;
    }

  write_exp_elt_opcode (UNOP_MEMVAL);
  switch (msymbol->type)
    {
    case mst_text:
    case mst_file_text:
    case mst_solib_trampoline:
      write_exp_elt_type (builtin_type (gdbarch)->nodebug_text_symbol);
      break;

    case mst_data:
    case mst_file_data:
    case mst_bss:
    case mst_file_bss:
      write_exp_elt_type (builtin_type (gdbarch)->nodebug_data_symbol);
      break;

    default:
      write_exp_elt_type (builtin_type (gdbarch)->nodebug_unknown_symbol);
      break;
    }
  write_exp_elt_opcode (UNOP_MEMVAL);
}

/* Recognize tokens that start with '$'.  These include:

   $regname     A native register name or a "standard
   register name".

   $variable    A convenience variable with a name chosen
   by the user.

   $digits              Value history with index <digits>, starting
   from the first value which has index 1.

   $$digits     Value history with index <digits> relative
   to the last value.  I.E. $$0 is the last
   value, $$1 is the one previous to that, $$2
   is the one previous to $$1, etc.

   $ | $0 | $$0 The last value in the value history.

   $$           An abbreviation for the second to the last
   value in the value history, I.E. $$1

 */

void
write_dollar_variable (struct stoken str)
{
  struct symbol *sym = NULL;
  struct minimal_symbol *msym = NULL;

  /* Handle the tokens $digits; also $ (short for $0) and $$ (short for $$1)
     and $$digits (equivalent to $<-digits> if you could type that). */

  int negate = 0;
  int i = 1;
  /* Double dollar means negate the number and add -1 as well.
     Thus $$ alone means -1.  */
  if (str.length >= 2 && str.ptr[1] == '$')
    {
      negate = 1;
      i = 2;
    }
  if (i == str.length)
    {
      /* Just dollars (one or two) */
      i = -negate;
      goto handle_last;
    }
  /* Is the rest of the token digits?  */
  for (; i < str.length; i++)
    if (!(str.ptr[i] >= '0' && str.ptr[i] <= '9'))
      break;
  if (i == str.length)
    {
      i = atoi (str.ptr + 1 + negate);
      if (negate)
	i = -i;
      goto handle_last;
    }

  /* Handle tokens that refer to machine registers:
     $ followed by a register name.  */
  i = frame_map_name_to_regnum (deprecated_safe_get_selected_frame (),
				str.ptr + 1, str.length - 1);
  if (i >= 0)
    goto handle_register;

  /* On some systems, such as HP-UX and hppa-linux, certain system routines 
     have names beginning with $ or $$.  Check for those, first. */

  sym = lookup_symbol (copy_name (str), (struct block *) NULL,
		       VAR_DOMAIN, (int *) NULL, (struct symtab **) NULL);
  if (sym)
    {
      write_exp_elt_opcode (OP_VAR_VALUE);
      write_exp_elt_block (block_found);	/* set by lookup_symbol */
      write_exp_elt_sym (sym);
      write_exp_elt_opcode (OP_VAR_VALUE);
      return;
    }
  msym = lookup_minimal_symbol (copy_name (str), NULL, NULL);
  if (msym)
    {
      write_exp_msymbol (msym,
			 lookup_function_type (builtin_type_int),
			 builtin_type_int);
      return;
    }

  /* Any other names starting in $ are debugger internal variables.  */

  write_exp_elt_opcode (OP_INTERNALVAR);
  write_exp_elt_intern (lookup_internalvar (copy_name (str) + 1));
  write_exp_elt_opcode (OP_INTERNALVAR);
  return;
handle_last:
  write_exp_elt_opcode (OP_LAST);
  write_exp_elt_longcst ((LONGEST) i);
  write_exp_elt_opcode (OP_LAST);
  return;
handle_register:
  write_exp_elt_opcode (OP_REGISTER);
  str.length--;
  str.ptr++;
  write_exp_string (str);
  write_exp_elt_opcode (OP_REGISTER);
  return;
}


char *
find_template_name_end (char *p)
{
  int depth = 1;
  int just_seen_right = 0;
  int just_seen_colon = 0;
  int just_seen_space = 0;

  if (!p || (*p != '<'))
    return 0;

  while (*++p)
    {
      switch (*p)
	{
	case '\'':
	case '\"':
	case '{':
	case '}':
	  /* In future, may want to allow these?? */
	  return 0;
	case '<':
	  depth++;		/* start nested template */
	  if (just_seen_colon || just_seen_right || just_seen_space)
	    return 0;		/* but not after : or :: or > or space */
	  break;
	case '>':
	  if (just_seen_colon || just_seen_right)
	    return 0;		/* end a (nested?) template */
	  just_seen_right = 1;	/* but not after : or :: */
	  if (--depth == 0)	/* also disallow >>, insist on > > */
	    return ++p;		/* if outermost ended, return */
	  break;
	case ':':
	  if (just_seen_space || (just_seen_colon > 1))
	    return 0;		/* nested class spec coming up */
	  just_seen_colon++;	/* we allow :: but not :::: */
	  break;
	case ' ':
	  break;
	default:
	  if (!((*p >= 'a' && *p <= 'z') ||	/* allow token chars */
		(*p >= 'A' && *p <= 'Z') ||
		(*p >= '0' && *p <= '9') ||
		(*p == '_') || (*p == ',') ||	/* commas for template args */
		(*p == '&') || (*p == '*') ||	/* pointer and ref types */
		(*p == '(') || (*p == ')') ||	/* function types */
		(*p == '[') || (*p == ']')))	/* array types */
	    return 0;
	}
      if (*p != ' ')
	just_seen_space = 0;
      if (*p != ':')
	just_seen_colon = 0;
      if (*p != '>')
	just_seen_right = 0;
    }
  return 0;
}



/* Return a null-terminated temporary copy of the name
   of a string token.  */

char *
copy_name (struct stoken token)
{
  /* Make sure there's enough space for the token.  */
  if (namecopy_size < token.length + 1)
    {
      namecopy_size = token.length + 1;
      namecopy = xrealloc (namecopy, token.length + 1);
    }
      
  memcpy (namecopy, token.ptr, token.length);
  namecopy[token.length] = 0;

  return namecopy;
}

/* Reverse an expression from suffix form (in which it is constructed)
   to prefix form (in which we can conveniently print or execute it).  */

static void
prefixify_expression (struct expression *expr)
{
  int len =
  sizeof (struct expression) + EXP_ELEM_TO_BYTES (expr->nelts);
  struct expression *temp;
  int inpos = expr->nelts, outpos = 0;

  temp = (struct expression *) alloca (len);

  /* Copy the original expression into temp.  */
  memcpy (temp, expr, len);

  prefixify_subexp (temp, expr, inpos, outpos);
}

/* Return the number of exp_elements in the postfix subexpression 
   of EXPR whose operator is at index ENDPOS - 1 in EXPR.  */

int
length_of_subexp (struct expression *expr, int endpos)
{
  int oplen, args, i;

  operator_length (expr, endpos, &oplen, &args);

  while (args > 0)
    {
      oplen += length_of_subexp (expr, endpos - oplen);
      args--;
    }

  return oplen;
}

/* Sets *OPLENP to the length of the operator whose (last) index is 
   ENDPOS - 1 in EXPR, and sets *ARGSP to the number of arguments that
   operator takes.  */

void
operator_length (struct expression *expr, int endpos, int *oplenp, int *argsp)
{
  expr->language_defn->la_exp_desc->operator_length (expr, endpos,
						     oplenp, argsp);
}

/* Default value for operator_length in exp_descriptor vectors.  */

void
operator_length_standard (struct expression *expr, int endpos,
			  int *oplenp, int *argsp)
{
  int oplen = 1;
  int args = 0;
  enum f90_range_type range_type;
  int i;

  if (endpos < 1)
    error (_("?error in operator_length_standard"));

  i = (int) expr->elts[endpos - 1].opcode;

  switch (i)
    {
      /* C++  */
    case OP_SCOPE:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = 5 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_LONG:
    case OP_DOUBLE:
    case OP_VAR_VALUE:
      oplen = 4;
      break;

    case OP_TYPE:
    case OP_BOOL:
    case OP_LAST:
    case OP_INTERNALVAR:
      oplen = 3;
      break;

    case OP_COMPLEX:
      oplen = 1;
      args = 2;
      break;

    case OP_FUNCALL:
    case OP_F77_UNDETERMINED_ARGLIST:
      oplen = 3;
      args = 1 + longest_to_int (expr->elts[endpos - 2].longconst);
      break;

    case OP_OBJC_MSGCALL:	/* Objective C message (method) call */
      oplen = 4;
      args = 1 + longest_to_int (expr->elts[endpos - 2].longconst);
      break;

    case UNOP_MAX:
    case UNOP_MIN:
      oplen = 3;
      break;

    case BINOP_VAL:
    case UNOP_CAST:
    case UNOP_MEMVAL:
      oplen = 3;
      args = 1;
      break;

    case UNOP_MEMVAL_TLS:
      oplen = 4;
      args = 1;
      break;

    case UNOP_ABS:
    case UNOP_CAP:
    case UNOP_CHR:
    case UNOP_FLOAT:
    case UNOP_HIGH:
    case UNOP_ODD:
    case UNOP_ORD:
    case UNOP_TRUNC:
      oplen = 1;
      args = 1;
      break;

    case OP_LABELED:
    case STRUCTOP_STRUCT:
    case STRUCTOP_PTR:
      args = 1;
      /* fall through */
    case OP_REGISTER:
    case OP_M2_STRING:
    case OP_STRING:
    case OP_OBJC_NSSTRING:	/* Objective C Foundation Class NSString constant */
    case OP_OBJC_SELECTOR:	/* Objective C "@selector" pseudo-op */
    case OP_NAME:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen + 1);
      break;

    case OP_BITSTRING:
      oplen = longest_to_int (expr->elts[endpos - 2].longconst);
      oplen = (oplen + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
      oplen = 4 + BYTES_TO_EXP_ELEM (oplen);
      break;

    case OP_ARRAY:
      oplen = 4;
      args = longest_to_int (expr->elts[endpos - 2].longconst);
      args -= longest_to_int (expr->elts[endpos - 3].longconst);
      args += 1;
      break;

    case TERNOP_COND:
    case TERNOP_SLICE:
    case TERNOP_SLICE_COUNT:
      args = 3;
      break;

      /* Modula-2 */
    case MULTI_SUBSCRIPT:
      oplen = 3;
      args = 1 + longest_to_int (expr->elts[endpos - 2].longconst);
      break;

    case BINOP_ASSIGN_MODIFY:
      oplen = 3;
      args = 2;
      break;

      /* C++ */
    case OP_THIS:
    case OP_OBJC_SELF:
      oplen = 2;
      break;

    case OP_F90_RANGE:
      oplen = 3;

      range_type = longest_to_int (expr->elts[endpos - 2].longconst);
      switch (range_type)
	{
	case LOW_BOUND_DEFAULT:
	case HIGH_BOUND_DEFAULT:
	  args = 1;
	  break;
	case BOTH_BOUND_DEFAULT:
	  args = 0;
	  break;
	case NONE_BOUND_DEFAULT:
	  args = 2;
	  break;
	}

      break;

    default:
      args = 1 + (i < (int) BINOP_END);
    }

  *oplenp = oplen;
  *argsp = args;
}

/* Copy the subexpression ending just before index INEND in INEXPR
   into OUTEXPR, starting at index OUTBEG.
   In the process, convert it from suffix to prefix form.  */

static void
prefixify_subexp (struct expression *inexpr,
		  struct expression *outexpr, int inend, int outbeg)
{
  int oplen;
  int args;
  int i;
  int *arglens;
  enum exp_opcode opcode;

  operator_length (inexpr, inend, &oplen, &args);

  /* Copy the final operator itself, from the end of the input
     to the beginning of the output.  */
  inend -= oplen;
  memcpy (&outexpr->elts[outbeg], &inexpr->elts[inend],
	  EXP_ELEM_TO_BYTES (oplen));
  outbeg += oplen;

  /* Find the lengths of the arg subexpressions.  */
  arglens = (int *) alloca (args * sizeof (int));
  for (i = args - 1; i >= 0; i--)
    {
      oplen = length_of_subexp (inexpr, inend);
      arglens[i] = oplen;
      inend -= oplen;
    }

  /* Now copy each subexpression, preserving the order of
     the subexpressions, but prefixifying each one.
     In this loop, inend starts at the beginning of
     the expression this level is working on
     and marches forward over the arguments.
     outbeg does similarly in the output.  */
  for (i = 0; i < args; i++)
    {
      oplen = arglens[i];
      inend += oplen;
      prefixify_subexp (inexpr, outexpr, inend, outbeg);
      outbeg += oplen;
    }
}

/* This page contains the two entry points to this file.  */

/* Read an expression from the string *STRINGPTR points to,
   parse it, and return a pointer to a  struct expression  that we malloc.
   Use block BLOCK as the lexical context for variable names;
   if BLOCK is zero, use the block of the selected stack frame.
   Meanwhile, advance *STRINGPTR to point after the expression,
   at the first nonwhite character that is not part of the expression
   (possibly a null character).

   If COMMA is nonzero, stop if a comma is reached.  */

struct expression *
parse_exp_1 (char **stringptr, struct block *block, int comma)
{
  return parse_exp_in_context (stringptr, block, comma, 0);
}

/* As for parse_exp_1, except that if VOID_CONTEXT_P, then
   no value is expected from the expression.  */

static struct expression *
parse_exp_in_context (char **stringptr, struct block *block, int comma, 
		      int void_context_p)
{
  struct cleanup *old_chain;

  lexptr = *stringptr;
  prev_lexptr = NULL;

  paren_depth = 0;
  type_stack_depth = 0;

  comma_terminates = comma;

  if (lexptr == 0 || *lexptr == 0)
    error_no_arg (_("expression to compute"));

  old_chain = make_cleanup (free_funcalls, 0 /*ignore*/);
  funcall_chain = 0;

  /* If no context specified, try using the current frame, if any. */

  if (!block)
    block = get_selected_block (&expression_context_pc);

  /* Fall back to using the current source static context, if any. */

  if (!block)
    {
      struct symtab_and_line cursal = get_current_source_symtab_and_line ();
      if (cursal.symtab)
	block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (cursal.symtab), STATIC_BLOCK);
    }

  /* Save the context, if specified by caller, or found above. */

  if (block)
    {
      expression_context_block = block;
      expression_context_pc = BLOCK_START (block);
    }

  expout_size = 10;
  expout_ptr = 0;
  expout = (struct expression *)
    xmalloc (sizeof (struct expression) + EXP_ELEM_TO_BYTES (expout_size));
  expout->language_defn = current_language;
  make_cleanup (free_current_contents, &expout);

  if (current_language->la_parser ())
    current_language->la_error (NULL);

  discard_cleanups (old_chain);

  /* Record the actual number of expression elements, and then
     reallocate the expression memory so that we free up any
     excess elements. */

  expout->nelts = expout_ptr;
  expout = (struct expression *)
    xrealloc ((char *) expout,
	      sizeof (struct expression) + EXP_ELEM_TO_BYTES (expout_ptr));;

  /* Convert expression from postfix form as generated by yacc
     parser, to a prefix form. */

  if (expressiondebug)
    dump_raw_expression (expout, gdb_stdlog,
			 "before conversion to prefix form");

  prefixify_expression (expout);

  current_language->la_post_parser (&expout, void_context_p);

  if (expressiondebug)
    dump_prefix_expression (expout, gdb_stdlog);

  *stringptr = lexptr;
  return expout;
}

/* Parse STRING as an expression, and complain if this fails
   to use up all of the contents of STRING.  */

struct expression *
parse_expression (char *string)
{
  struct expression *exp;
  exp = parse_exp_1 (&string, 0, 0);
  if (*string)
    error (_("Junk after end of expression."));
  return exp;
}


/* As for parse_expression, except that if VOID_CONTEXT_P, then
   no value is expected from the expression.  */

struct expression *
parse_expression_in_context (char *string, int void_context_p)
{
  struct expression *exp;
  exp = parse_exp_in_context (&string, 0, 0, void_context_p);
  if (*string != '\000')
    error (_("Junk after end of expression."));
  return exp;
}

/* A post-parser that does nothing */

void
null_post_parser (struct expression **exp, int void_context_p)
{
}

/* Stuff for maintaining a stack of types.  Currently just used by C, but
   probably useful for any language which declares its types "backwards".  */

static void
check_type_stack_depth (void)
{
  if (type_stack_depth == type_stack_size)
    {
      type_stack_size *= 2;
      type_stack = (union type_stack_elt *)
	xrealloc ((char *) type_stack, type_stack_size * sizeof (*type_stack));
    }
}

void
push_type (enum type_pieces tp)
{
  check_type_stack_depth ();
  type_stack[type_stack_depth++].piece = tp;
}

void
push_type_int (int n)
{
  check_type_stack_depth ();
  type_stack[type_stack_depth++].int_val = n;
}

void
push_type_address_space (char *string)
{
  push_type_int (address_space_name_to_int (string));
}

enum type_pieces
pop_type (void)
{
  if (type_stack_depth)
    return type_stack[--type_stack_depth].piece;
  return tp_end;
}

int
pop_type_int (void)
{
  if (type_stack_depth)
    return type_stack[--type_stack_depth].int_val;
  /* "Can't happen".  */
  return 0;
}

/* Pop the type stack and return the type which corresponds to FOLLOW_TYPE
   as modified by all the stuff on the stack.  */
struct type *
follow_types (struct type *follow_type)
{
  int done = 0;
  int make_const = 0;
  int make_volatile = 0;
  int make_addr_space = 0;
  int array_size;
  struct type *range_type;

  while (!done)
    switch (pop_type ())
      {
      case tp_end:
	done = 1;
	if (make_const)
	  follow_type = make_cv_type (make_const, 
				      TYPE_VOLATILE (follow_type), 
				      follow_type, 0);
	if (make_volatile)
	  follow_type = make_cv_type (TYPE_CONST (follow_type), 
				      make_volatile, 
				      follow_type, 0);
	if (make_addr_space)
	  follow_type = make_type_with_address_space (follow_type, 
						      make_addr_space);
	make_const = make_volatile = 0;
	make_addr_space = 0;
	break;
      case tp_const:
	make_const = 1;
	break;
      case tp_volatile:
	make_volatile = 1;
	break;
      case tp_space_identifier:
	make_addr_space = pop_type_int ();
	break;
      case tp_pointer:
	follow_type = lookup_pointer_type (follow_type);
	if (make_const)
	  follow_type = make_cv_type (make_const, 
				      TYPE_VOLATILE (follow_type), 
				      follow_type, 0);
	if (make_volatile)
	  follow_type = make_cv_type (TYPE_CONST (follow_type), 
				      make_volatile, 
				      follow_type, 0);
	if (make_addr_space)
	  follow_type = make_type_with_address_space (follow_type, 
						      make_addr_space);
	make_const = make_volatile = 0;
	make_addr_space = 0;
	break;
      case tp_reference:
	follow_type = lookup_reference_type (follow_type);
	if (make_const)
	  follow_type = make_cv_type (make_const, 
				      TYPE_VOLATILE (follow_type), 
				      follow_type, 0);
	if (make_volatile)
	  follow_type = make_cv_type (TYPE_CONST (follow_type), 
				      make_volatile, 
				      follow_type, 0);
	if (make_addr_space)
	  follow_type = make_type_with_address_space (follow_type, 
						      make_addr_space);
	make_const = make_volatile = 0;
	make_addr_space = 0;
	break;
      case tp_array:
	array_size = pop_type_int ();
	/* FIXME-type-allocation: need a way to free this type when we are
	   done with it.  */
	range_type =
	  create_range_type ((struct type *) NULL,
			     builtin_type_int, 0,
			     array_size >= 0 ? array_size - 1 : 0);
	follow_type =
	  create_array_type ((struct type *) NULL,
			     follow_type, range_type);
	if (array_size < 0)
	  TYPE_ARRAY_UPPER_BOUND_TYPE (follow_type)
	    = BOUND_CANNOT_BE_DETERMINED;
	break;
      case tp_function:
	/* FIXME-type-allocation: need a way to free this type when we are
	   done with it.  */
	follow_type = lookup_function_type (follow_type);
	break;
      }
  return follow_type;
}

/* This function avoids direct calls to fprintf 
   in the parser generated debug code.  */
void
parser_fprintf (FILE *x, const char *y, ...)
{ 
  va_list args;
  va_start (args, y);
  if (x == stderr)
    vfprintf_unfiltered (gdb_stderr, y, args); 
  else
    {
      fprintf_unfiltered (gdb_stderr, " Unknown FILE used.\n");
      vfprintf_unfiltered (gdb_stderr, y, args);
    }
  va_end (args);
}

void
_initialize_parse (void)
{
  type_stack_size = 80;
  type_stack_depth = 0;
  type_stack = (union type_stack_elt *)
    xmalloc (type_stack_size * sizeof (*type_stack));

  add_setshow_zinteger_cmd ("expression", class_maintenance,
			    &expressiondebug, _("\
Set expression debugging."), _("\
Show expression debugging."), _("\
When non-zero, the internal representation of expressions will be printed."),
			    NULL,
			    show_expressiondebug,
			    &setdebuglist, &showdebuglist);
}
