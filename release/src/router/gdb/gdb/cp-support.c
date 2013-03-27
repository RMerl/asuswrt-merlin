/* Helper routines for C++ support in GDB.
   Copyright (C) 2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   Contributed by MontaVista Software.

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
#include <ctype.h>
#include "cp-support.h"
#include "gdb_string.h"
#include "demangle.h"
#include "gdb_assert.h"
#include "gdbcmd.h"
#include "dictionary.h"
#include "objfiles.h"
#include "frame.h"
#include "symtab.h"
#include "block.h"
#include "complaints.h"
#include "gdbtypes.h"

#define d_left(dc) (dc)->u.s_binary.left
#define d_right(dc) (dc)->u.s_binary.right

/* Functions related to demangled name parsing.  */

static unsigned int cp_find_first_component_aux (const char *name,
						 int permissive);

static void demangled_name_complaint (const char *name);

/* Functions/variables related to overload resolution.  */

static int sym_return_val_size;
static int sym_return_val_index;
static struct symbol **sym_return_val;

static void overload_list_add_symbol (struct symbol *sym,
				      const char *oload_name);

static void make_symbol_overload_list_using (const char *func_name,
					     const char *namespace);

static void make_symbol_overload_list_qualified (const char *func_name);

static void read_in_psymtabs (const char *oload_name);

/* The list of "maint cplus" commands.  */

struct cmd_list_element *maint_cplus_cmd_list = NULL;

/* The actual commands.  */

static void maint_cplus_command (char *arg, int from_tty);
static void first_component_command (char *arg, int from_tty);

/* Return the canonicalized form of STRING, or NULL if STRING can not be
   parsed.  The return value is allocated via xmalloc.

   drow/2005-03-07: Should we also return NULL for things that trivially do
   not require any change?  e.g. simple identifiers.  This could be more
   efficient.  */

char *
cp_canonicalize_string (const char *string)
{
  void *storage;
  struct demangle_component *ret_comp;
  char *ret;
  int len = strlen (string);

  len = len + len / 8;

  ret_comp = cp_demangled_name_to_comp (string, &storage, NULL);
  if (ret_comp == NULL)
    return NULL;

  ret = cp_comp_to_string (ret_comp, len);

  xfree (storage);

  return ret;
}

/* Convert a mangled name to a demangle_component tree.  *MEMORY is set to the
   block of used memory that should be freed when finished with the tree. 
   DEMANGLED_P is set to the char * that should be freed when finished with
   the tree, or NULL if none was needed.  OPTIONS will be passed to the
   demangler.  */

static struct demangle_component *
mangled_name_to_comp (const char *mangled_name, int options,
		      void **memory, char **demangled_p)
{
  struct demangle_component *ret;
  char *demangled_name;
  int len;

  /* If it looks like a v3 mangled name, then try to go directly
     to trees.  */
  if (mangled_name[0] == '_' && mangled_name[1] == 'Z')
    {
      ret = cplus_demangle_v3_components (mangled_name, options, memory);
      if (ret)
	{
	  *demangled_p = NULL;
	  return ret;
	}
    }

  /* If it doesn't, or if that failed, then try to demangle the name.  */
  demangled_name = cplus_demangle (mangled_name, options);
  if (demangled_name == NULL)
   return NULL;
  
  /* If we could demangle the name, parse it to build the component tree.  */
  ret = cp_demangled_name_to_comp (demangled_name, memory, NULL);

  if (ret == NULL)
    {
      free (demangled_name);
      return NULL;
    }

  *demangled_p = demangled_name;
  return ret;
}

/* Return the name of the class containing method PHYSNAME.  */

char *
cp_class_name_from_physname (const char *physname)
{
  void *storage;
  char *demangled_name = NULL, *ret;
  struct demangle_component *ret_comp, *prev_comp, *cur_comp;
  int done;

  ret_comp = mangled_name_to_comp (physname, DMGL_ANSI, &storage,
				   &demangled_name);
  if (ret_comp == NULL)
    return NULL;

  done = 0;

  /* First strip off any qualifiers, if we have a function or method.  */
  while (!done)
    switch (ret_comp->type)
      {
      case DEMANGLE_COMPONENT_CONST:
      case DEMANGLE_COMPONENT_RESTRICT:
      case DEMANGLE_COMPONENT_VOLATILE:
      case DEMANGLE_COMPONENT_CONST_THIS:
      case DEMANGLE_COMPONENT_RESTRICT_THIS:
      case DEMANGLE_COMPONENT_VOLATILE_THIS:
      case DEMANGLE_COMPONENT_VENDOR_TYPE_QUAL:
        ret_comp = d_left (ret_comp);
        break;
      default:
	done = 1;
	break;
      }

  /* If what we have now is a function, discard the argument list.  */
  if (ret_comp->type == DEMANGLE_COMPONENT_TYPED_NAME)
    ret_comp = d_left (ret_comp);

  /* If what we have now is a template, strip off the template
     arguments.  The left subtree may be a qualified name.  */
  if (ret_comp->type == DEMANGLE_COMPONENT_TEMPLATE)
    ret_comp = d_left (ret_comp);

  /* What we have now should be a name, possibly qualified.  Additional
     qualifiers could live in the left subtree or the right subtree.  Find
     the last piece.  */
  done = 0;
  prev_comp = NULL;
  cur_comp = ret_comp;
  while (!done)
    switch (cur_comp->type)
      {
      case DEMANGLE_COMPONENT_QUAL_NAME:
      case DEMANGLE_COMPONENT_LOCAL_NAME:
	prev_comp = cur_comp;
        cur_comp = d_right (cur_comp);
        break;
      case DEMANGLE_COMPONENT_TEMPLATE:
      case DEMANGLE_COMPONENT_NAME:
      case DEMANGLE_COMPONENT_CTOR:
      case DEMANGLE_COMPONENT_DTOR:
      case DEMANGLE_COMPONENT_OPERATOR:
      case DEMANGLE_COMPONENT_EXTENDED_OPERATOR:
	done = 1;
	break;
      default:
	done = 1;
	cur_comp = NULL;
	break;
      }

  ret = NULL;
  if (cur_comp != NULL && prev_comp != NULL)
    {
      /* We want to discard the rightmost child of PREV_COMP.  */
      *prev_comp = *d_left (prev_comp);
      /* The ten is completely arbitrary; we don't have a good estimate.  */
      ret = cp_comp_to_string (ret_comp, 10);
    }

  xfree (storage);
  if (demangled_name)
    xfree (demangled_name);
  return ret;
}

/* Return the child of COMP which is the basename of a method, variable,
   et cetera.  All scope qualifiers are discarded, but template arguments
   will be included.  The component tree may be modified.  */

static struct demangle_component *
unqualified_name_from_comp (struct demangle_component *comp)
{
  struct demangle_component *ret_comp = comp, *last_template;
  int done;

  done = 0;
  last_template = NULL;
  while (!done)
    switch (ret_comp->type)
      {
      case DEMANGLE_COMPONENT_QUAL_NAME:
      case DEMANGLE_COMPONENT_LOCAL_NAME:
        ret_comp = d_right (ret_comp);
        break;
      case DEMANGLE_COMPONENT_TYPED_NAME:
        ret_comp = d_left (ret_comp);
        break;
      case DEMANGLE_COMPONENT_TEMPLATE:
	gdb_assert (last_template == NULL);
	last_template = ret_comp;
	ret_comp = d_left (ret_comp);
	break;
      case DEMANGLE_COMPONENT_CONST:
      case DEMANGLE_COMPONENT_RESTRICT:
      case DEMANGLE_COMPONENT_VOLATILE:
      case DEMANGLE_COMPONENT_CONST_THIS:
      case DEMANGLE_COMPONENT_RESTRICT_THIS:
      case DEMANGLE_COMPONENT_VOLATILE_THIS:
      case DEMANGLE_COMPONENT_VENDOR_TYPE_QUAL:
        ret_comp = d_left (ret_comp);
        break;
      case DEMANGLE_COMPONENT_NAME:
      case DEMANGLE_COMPONENT_CTOR:
      case DEMANGLE_COMPONENT_DTOR:
      case DEMANGLE_COMPONENT_OPERATOR:
      case DEMANGLE_COMPONENT_EXTENDED_OPERATOR:
	done = 1;
	break;
      default:
	return NULL;
	break;
      }

  if (last_template)
    {
      d_left (last_template) = ret_comp;
      return last_template;
    }

  return ret_comp;
}

/* Return the name of the method whose linkage name is PHYSNAME.  */

char *
method_name_from_physname (const char *physname)
{
  void *storage;
  char *demangled_name = NULL, *ret;
  struct demangle_component *ret_comp;
  int done;

  ret_comp = mangled_name_to_comp (physname, DMGL_ANSI, &storage,
				   &demangled_name);
  if (ret_comp == NULL)
    return NULL;

  ret_comp = unqualified_name_from_comp (ret_comp);

  ret = NULL;
  if (ret_comp != NULL)
    /* The ten is completely arbitrary; we don't have a good estimate.  */
    ret = cp_comp_to_string (ret_comp, 10);

  xfree (storage);
  if (demangled_name)
    xfree (demangled_name);
  return ret;
}

/* If FULL_NAME is the demangled name of a C++ function (including an
   arg list, possibly including namespace/class qualifications),
   return a new string containing only the function name (without the
   arg list/class qualifications).  Otherwise, return NULL.  The
   caller is responsible for freeing the memory in question.  */

char *
cp_func_name (const char *full_name)
{
  void *storage;
  char *ret;
  struct demangle_component *ret_comp;
  int done;

  ret_comp = cp_demangled_name_to_comp (full_name, &storage, NULL);
  if (!ret_comp)
    return NULL;

  ret_comp = unqualified_name_from_comp (ret_comp);

  ret = NULL;
  if (ret_comp != NULL)
    ret = cp_comp_to_string (ret_comp, 10);

  xfree (storage);
  return ret;
}

/* DEMANGLED_NAME is the name of a function, including parameters and
   (optionally) a return type.  Return the name of the function without
   parameters or return type, or NULL if we can not parse the name.  */

static char *
remove_params (const char *demangled_name)
{
  int done = 0;
  struct demangle_component *ret_comp;
  void *storage;
  char *ret = NULL;

  if (demangled_name == NULL)
    return NULL;

  ret_comp = cp_demangled_name_to_comp (demangled_name, &storage, NULL);
  if (ret_comp == NULL)
    return NULL;

  /* First strip off any qualifiers, if we have a function or method.  */
  while (!done)
    switch (ret_comp->type)
      {
      case DEMANGLE_COMPONENT_CONST:
      case DEMANGLE_COMPONENT_RESTRICT:
      case DEMANGLE_COMPONENT_VOLATILE:
      case DEMANGLE_COMPONENT_CONST_THIS:
      case DEMANGLE_COMPONENT_RESTRICT_THIS:
      case DEMANGLE_COMPONENT_VOLATILE_THIS:
      case DEMANGLE_COMPONENT_VENDOR_TYPE_QUAL:
        ret_comp = d_left (ret_comp);
        break;
      default:
	done = 1;
	break;
      }

  /* What we have now should be a function.  Return its name.  */
  if (ret_comp->type == DEMANGLE_COMPONENT_TYPED_NAME)
    ret = cp_comp_to_string (d_left (ret_comp), 10);

  xfree (storage);
  return ret;
}

/* Here are some random pieces of trivia to keep in mind while trying
   to take apart demangled names:

   - Names can contain function arguments or templates, so the process
     has to be, to some extent recursive: maybe keep track of your
     depth based on encountering <> and ().

   - Parentheses don't just have to happen at the end of a name: they
     can occur even if the name in question isn't a function, because
     a template argument might be a type that's a function.

   - Conversely, even if you're trying to deal with a function, its
     demangled name might not end with ')': it could be a const or
     volatile class method, in which case it ends with "const" or
     "volatile".

   - Parentheses are also used in anonymous namespaces: a variable
     'foo' in an anonymous namespace gets demangled as "(anonymous
     namespace)::foo".

   - And operator names can contain parentheses or angle brackets.  */

/* FIXME: carlton/2003-03-13: We have several functions here with
   overlapping functionality; can we combine them?  Also, do they
   handle all the above considerations correctly?  */


/* This returns the length of first component of NAME, which should be
   the demangled name of a C++ variable/function/method/etc.
   Specifically, it returns the index of the first colon forming the
   boundary of the first component: so, given 'A::foo' or 'A::B::foo'
   it returns the 1, and given 'foo', it returns 0.  */

/* The character in NAME indexed by the return value is guaranteed to
   always be either ':' or '\0'.  */

/* NOTE: carlton/2003-03-13: This function is currently only intended
   for internal use: it's probably not entirely safe when called on
   user-generated input, because some of the 'index += 2' lines in
   cp_find_first_component_aux might go past the end of malformed
   input.  */

unsigned int
cp_find_first_component (const char *name)
{
  return cp_find_first_component_aux (name, 0);
}

/* Helper function for cp_find_first_component.  Like that function,
   it returns the length of the first component of NAME, but to make
   the recursion easier, it also stops if it reaches an unexpected ')'
   or '>' if the value of PERMISSIVE is nonzero.  */

/* Let's optimize away calls to strlen("operator").  */

#define LENGTH_OF_OPERATOR 8

static unsigned int
cp_find_first_component_aux (const char *name, int permissive)
{
  unsigned int index = 0;
  /* Operator names can show up in unexpected places.  Since these can
     contain parentheses or angle brackets, they can screw up the
     recursion.  But not every string 'operator' is part of an
     operater name: e.g. you could have a variable 'cooperator'.  So
     this variable tells us whether or not we should treat the string
     'operator' as starting an operator.  */
  int operator_possible = 1;

  for (;; ++index)
    {
      switch (name[index])
	{
	case '<':
	  /* Template; eat it up.  The calls to cp_first_component
	     should only return (I hope!) when they reach the '>'
	     terminating the component or a '::' between two
	     components.  (Hence the '+ 2'.)  */
	  index += 1;
	  for (index += cp_find_first_component_aux (name + index, 1);
	       name[index] != '>';
	       index += cp_find_first_component_aux (name + index, 1))
	    {
	      if (name[index] != ':')
		{
		  demangled_name_complaint (name);
		  return strlen (name);
		}
	      index += 2;
	    }
	  operator_possible = 1;
	  break;
	case '(':
	  /* Similar comment as to '<'.  */
	  index += 1;
	  for (index += cp_find_first_component_aux (name + index, 1);
	       name[index] != ')';
	       index += cp_find_first_component_aux (name + index, 1))
	    {
	      if (name[index] != ':')
		{
		  demangled_name_complaint (name);
		  return strlen (name);
		}
	      index += 2;
	    }
	  operator_possible = 1;
	  break;
	case '>':
	case ')':
	  if (permissive)
	    return index;
	  else
	    {
	      demangled_name_complaint (name);
	      return strlen (name);
	    }
	case '\0':
	case ':':
	  return index;
	case 'o':
	  /* Operator names can screw up the recursion.  */
	  if (operator_possible
	      && strncmp (name + index, "operator", LENGTH_OF_OPERATOR) == 0)
	    {
	      index += LENGTH_OF_OPERATOR;
	      while (isspace(name[index]))
		++index;
	      switch (name[index])
		{
		  /* Skip over one less than the appropriate number of
		     characters: the for loop will skip over the last
		     one.  */
		case '<':
		  if (name[index + 1] == '<')
		    index += 1;
		  else
		    index += 0;
		  break;
		case '>':
		case '-':
		  if (name[index + 1] == '>')
		    index += 1;
		  else
		    index += 0;
		  break;
		case '(':
		  index += 1;
		  break;
		default:
		  index += 0;
		  break;
		}
	    }
	  operator_possible = 0;
	  break;
	case ' ':
	case ',':
	case '.':
	case '&':
	case '*':
	  /* NOTE: carlton/2003-04-18: I'm not sure what the precise
	     set of relevant characters are here: it's necessary to
	     include any character that can show up before 'operator'
	     in a demangled name, and it's safe to include any
	     character that can't be part of an identifier's name.  */
	  operator_possible = 1;
	  break;
	default:
	  operator_possible = 0;
	  break;
	}
    }
}

/* Complain about a demangled name that we don't know how to parse.
   NAME is the demangled name in question.  */

static void
demangled_name_complaint (const char *name)
{
  complaint (&symfile_complaints,
	     "unexpected demangled name '%s'", name);
}

/* If NAME is the fully-qualified name of a C++
   function/variable/method/etc., this returns the length of its
   entire prefix: all of the namespaces and classes that make up its
   name.  Given 'A::foo', it returns 1, given 'A::B::foo', it returns
   4, given 'foo', it returns 0.  */

unsigned int
cp_entire_prefix_len (const char *name)
{
  unsigned int current_len = cp_find_first_component (name);
  unsigned int previous_len = 0;

  while (name[current_len] != '\0')
    {
      gdb_assert (name[current_len] == ':');
      previous_len = current_len;
      /* Skip the '::'.  */
      current_len += 2;
      current_len += cp_find_first_component (name + current_len);
    }

  return previous_len;
}

/* Overload resolution functions.  */

/* Test to see if SYM is a symbol that we haven't seen corresponding
   to a function named OLOAD_NAME.  If so, add it to the current
   completion list. */

static void
overload_list_add_symbol (struct symbol *sym, const char *oload_name)
{
  int newsize;
  int i;
  char *sym_name;

  /* If there is no type information, we can't do anything, so skip */
  if (SYMBOL_TYPE (sym) == NULL)
    return;

  /* skip any symbols that we've already considered. */
  for (i = 0; i < sym_return_val_index; ++i)
    if (strcmp (SYMBOL_LINKAGE_NAME (sym),
		SYMBOL_LINKAGE_NAME (sym_return_val[i])) == 0)
      return;

  /* Get the demangled name without parameters */
  sym_name = remove_params (SYMBOL_NATURAL_NAME (sym));
  if (!sym_name)
    return;

  /* skip symbols that cannot match */
  if (strcmp (sym_name, oload_name) != 0)
    {
      xfree (sym_name);
      return;
    }

  xfree (sym_name);

  /* We have a match for an overload instance, so add SYM to the current list
   * of overload instances */
  if (sym_return_val_index + 3 > sym_return_val_size)
    {
      newsize = (sym_return_val_size *= 2) * sizeof (struct symbol *);
      sym_return_val = (struct symbol **) xrealloc ((char *) sym_return_val, newsize);
    }
  sym_return_val[sym_return_val_index++] = sym;
  sym_return_val[sym_return_val_index] = NULL;
}

/* Return a null-terminated list of pointers to function symbols that
   are named FUNC_NAME and are visible within NAMESPACE.  */

struct symbol **
make_symbol_overload_list (const char *func_name,
			   const char *namespace)
{
  struct cleanup *old_cleanups;

  sym_return_val_size = 100;
  sym_return_val_index = 0;
  sym_return_val = xmalloc ((sym_return_val_size + 1) *
			    sizeof (struct symbol *));
  sym_return_val[0] = NULL;

  old_cleanups = make_cleanup (xfree, sym_return_val);

  make_symbol_overload_list_using (func_name, namespace);

  discard_cleanups (old_cleanups);

  return sym_return_val;
}

/* This applies the using directives to add namespaces to search in,
   and then searches for overloads in all of those namespaces.  It
   adds the symbols found to sym_return_val.  Arguments are as in
   make_symbol_overload_list.  */

static void
make_symbol_overload_list_using (const char *func_name,
				 const char *namespace)
{
  const struct using_direct *current;

  /* First, go through the using directives.  If any of them apply,
     look in the appropriate namespaces for new functions to match
     on.  */

  for (current = block_using (get_selected_block (0));
       current != NULL;
       current = current->next)
    {
      if (strcmp (namespace, current->outer) == 0)
	{
	  make_symbol_overload_list_using (func_name,
					   current->inner);
	}
    }

  /* Now, add names for this namespace.  */
  
  if (namespace[0] == '\0')
    {
      make_symbol_overload_list_qualified (func_name);
    }
  else
    {
      char *concatenated_name
	= alloca (strlen (namespace) + 2 + strlen (func_name) + 1);
      strcpy (concatenated_name, namespace);
      strcat (concatenated_name, "::");
      strcat (concatenated_name, func_name);
      make_symbol_overload_list_qualified (concatenated_name);
    }
}

/* This does the bulk of the work of finding overloaded symbols.
   FUNC_NAME is the name of the overloaded function we're looking for
   (possibly including namespace info).  */

static void
make_symbol_overload_list_qualified (const char *func_name)
{
  struct symbol *sym;
  struct symtab *s;
  struct objfile *objfile;
  const struct block *b, *surrounding_static_block = 0;
  struct dict_iterator iter;
  const struct dictionary *dict;

  /* Look through the partial symtabs for all symbols which begin
     by matching FUNC_NAME.  Make sure we read that symbol table in. */

  read_in_psymtabs (func_name);

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */

  for (b = get_selected_block (0); b != NULL; b = BLOCK_SUPERBLOCK (b))
    {
      dict = BLOCK_DICT (b);

      for (sym = dict_iter_name_first (dict, func_name, &iter);
	   sym;
	   sym = dict_iter_name_next (func_name, &iter))
	{
	  overload_list_add_symbol (sym, func_name);
	}
    }

  surrounding_static_block = block_static_block (get_selected_block (0));

  /* Go through the symtabs and check the externs and statics for
     symbols which match.  */

  ALL_PRIMARY_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
    dict = BLOCK_DICT (b);

    for (sym = dict_iter_name_first (dict, func_name, &iter);
	 sym;
	 sym = dict_iter_name_next (func_name, &iter))
    {
      overload_list_add_symbol (sym, func_name);
    }
  }

  ALL_PRIMARY_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
    /* Don't do this block twice.  */
    if (b == surrounding_static_block)
      continue;
    dict = BLOCK_DICT (b);

    for (sym = dict_iter_name_first (dict, func_name, &iter);
	 sym;
	 sym = dict_iter_name_next (func_name, &iter))
    {
      overload_list_add_symbol (sym, func_name);
    }
  }
}

/* Look through the partial symtabs for all symbols which begin
   by matching FUNC_NAME.  Make sure we read that symbol table in. */

static void
read_in_psymtabs (const char *func_name)
{
  struct partial_symtab *ps;
  struct objfile *objfile;

  ALL_PSYMTABS (objfile, ps)
  {
    if (ps->readin)
      continue;

    if ((lookup_partial_symbol (ps, func_name, NULL, 1, VAR_DOMAIN)
	 != NULL)
	|| (lookup_partial_symbol (ps, func_name, NULL, 0, VAR_DOMAIN)
	    != NULL))
      psymtab_to_symtab (ps);
  }
}

/* Lookup the rtti type for a class name. */

struct type *
cp_lookup_rtti_type (const char *name, struct block *block)
{
  struct symbol * rtti_sym;
  struct type * rtti_type;

  rtti_sym = lookup_symbol (name, block, STRUCT_DOMAIN, NULL, NULL);

  if (rtti_sym == NULL)
    {
      warning (_("RTTI symbol not found for class '%s'"), name);
      return NULL;
    }

  if (SYMBOL_CLASS (rtti_sym) != LOC_TYPEDEF)
    {
      warning (_("RTTI symbol for class '%s' is not a type"), name);
      return NULL;
    }

  rtti_type = SYMBOL_TYPE (rtti_sym);

  switch (TYPE_CODE (rtti_type))
    {
    case TYPE_CODE_CLASS:
      break;
    case TYPE_CODE_NAMESPACE:
      /* chastain/2003-11-26: the symbol tables often contain fake
	 symbols for namespaces with the same name as the struct.
	 This warning is an indication of a bug in the lookup order
	 or a bug in the way that the symbol tables are populated.  */
      warning (_("RTTI symbol for class '%s' is a namespace"), name);
      return NULL;
    default:
      warning (_("RTTI symbol for class '%s' has bad type"), name);
      return NULL;
    }

  return rtti_type;
}

/* Don't allow just "maintenance cplus".  */

static  void
maint_cplus_command (char *arg, int from_tty)
{
  printf_unfiltered (_("\"maintenance cplus\" must be followed by the name of a command.\n"));
  help_list (maint_cplus_cmd_list, "maintenance cplus ", -1, gdb_stdout);
}

/* This is a front end for cp_find_first_component, for unit testing.
   Be careful when using it: see the NOTE above
   cp_find_first_component.  */

static void
first_component_command (char *arg, int from_tty)
{
  int len = cp_find_first_component (arg);
  char *prefix = alloca (len + 1);

  memcpy (prefix, arg, len);
  prefix[len] = '\0';

  printf_unfiltered ("%s\n", prefix);
}

extern initialize_file_ftype _initialize_cp_support; /* -Wmissing-prototypes */

void
_initialize_cp_support (void)
{
  add_prefix_cmd ("cplus", class_maintenance, maint_cplus_command,
		  _("C++ maintenance commands."), &maint_cplus_cmd_list,
		  "maintenance cplus ", 0, &maintenancelist);
  add_alias_cmd ("cp", "cplus", class_maintenance, 1, &maintenancelist);

  add_cmd ("first_component", class_maintenance, first_component_command,
	   _("Print the first class/namespace component of NAME."),
	   &maint_cplus_cmd_list);
}
