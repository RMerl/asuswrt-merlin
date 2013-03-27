/* Implementation of the GDB variable objects API.

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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
#include "exceptions.h"
#include "value.h"
#include "expression.h"
#include "frame.h"
#include "language.h"
#include "wrapper.h"
#include "gdbcmd.h"
#include "block.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "varobj.h"
#include "vec.h"

/* Non-zero if we want to see trace of varobj level stuff.  */

int varobjdebug = 0;
static void
show_varobjdebug (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Varobj debugging is %s.\n"), value);
}

/* String representations of gdb's format codes */
char *varobj_format_string[] =
  { "natural", "binary", "decimal", "hexadecimal", "octal" };

/* String representations of gdb's known languages */
char *varobj_language_string[] = { "unknown", "C", "C++", "Java" };

/* Data structures */

/* Every root variable has one of these structures saved in its
   varobj. Members which must be free'd are noted. */
struct varobj_root
{

  /* Alloc'd expression for this parent. */
  struct expression *exp;

  /* Block for which this expression is valid */
  struct block *valid_block;

  /* The frame for this expression */
  struct frame_id frame;

  /* If 1, "update" always recomputes the frame & valid block
     using the currently selected frame. */
  int use_selected_frame;

  /* Flag that indicates validity: set to 0 when this varobj_root refers 
     to symbols that do not exist anymore.  */
  int is_valid;

  /* Language info for this variable and its children */
  struct language_specific *lang;

  /* The varobj for this root node. */
  struct varobj *rootvar;

  /* Next root variable */
  struct varobj_root *next;
};

typedef struct varobj *varobj_p;

DEF_VEC_P (varobj_p);

/* Every variable in the system has a structure of this type defined
   for it. This structure holds all information necessary to manipulate
   a particular object variable. Members which must be freed are noted. */
struct varobj
{

  /* Alloc'd name of the variable for this object.. If this variable is a
     child, then this name will be the child's source name.
     (bar, not foo.bar) */
  /* NOTE: This is the "expression" */
  char *name;

  /* Alloc'd expression for this child.  Can be used to create a
     root variable corresponding to this child.  */
  char *path_expr;

  /* The alloc'd name for this variable's object. This is here for
     convenience when constructing this object's children. */
  char *obj_name;

  /* Index of this variable in its parent or -1 */
  int index;

  /* The type of this variable.  This can be NULL
     for artifial variable objects -- currently, the "accessibility" 
     variable objects in C++.  */
  struct type *type;

  /* The value of this expression or subexpression.  A NULL value
     indicates there was an error getting this value.
     Invariant: if varobj_value_is_changeable_p (this) is non-zero, 
     the value is either NULL, or not lazy.  */
  struct value *value;

  /* The number of (immediate) children this variable has */
  int num_children;

  /* If this object is a child, this points to its immediate parent. */
  struct varobj *parent;

  /* Children of this object.  */
  VEC (varobj_p) *children;

  /* Description of the root variable. Points to root variable for children. */
  struct varobj_root *root;

  /* The format of the output for this object */
  enum varobj_display_formats format;

  /* Was this variable updated via a varobj_set_value operation */
  int updated;

  /* Last print value.  */
  char *print_value;

  /* Is this variable frozen.  Frozen variables are never implicitly
     updated by -var-update * 
     or -var-update <direct-or-indirect-parent>.  */
  int frozen;

  /* Is the value of this variable intentionally not fetched?  It is
     not fetched if either the variable is frozen, or any parents is
     frozen.  */
  int not_fetched;
};

struct cpstack
{
  char *name;
  struct cpstack *next;
};

/* A list of varobjs */

struct vlist
{
  struct varobj *var;
  struct vlist *next;
};

/* Private function prototypes */

/* Helper functions for the above subcommands. */

static int delete_variable (struct cpstack **, struct varobj *, int);

static void delete_variable_1 (struct cpstack **, int *,
			       struct varobj *, int, int);

static int install_variable (struct varobj *);

static void uninstall_variable (struct varobj *);

static struct varobj *create_child (struct varobj *, int, char *);

/* Utility routines */

static struct varobj *new_variable (void);

static struct varobj *new_root_variable (void);

static void free_variable (struct varobj *var);

static struct cleanup *make_cleanup_free_variable (struct varobj *var);

static struct type *get_type (struct varobj *var);

static struct type *get_value_type (struct varobj *var);

static struct type *get_target_type (struct type *);

static enum varobj_display_formats variable_default_display (struct varobj *);

static void cppush (struct cpstack **pstack, char *name);

static char *cppop (struct cpstack **pstack);

static int install_new_value (struct varobj *var, struct value *value, 
			      int initial);

/* Language-specific routines. */

static enum varobj_languages variable_language (struct varobj *var);

static int number_of_children (struct varobj *);

static char *name_of_variable (struct varobj *);

static char *name_of_child (struct varobj *, int);

static struct value *value_of_root (struct varobj **var_handle, int *);

static struct value *value_of_child (struct varobj *parent, int index);

static int variable_editable (struct varobj *var);

static char *my_value_of_variable (struct varobj *var);

static char *value_get_print_value (struct value *value,
				    enum varobj_display_formats format);

static int varobj_value_is_changeable_p (struct varobj *var);

static int is_root_p (struct varobj *var);

/* C implementation */

static int c_number_of_children (struct varobj *var);

static char *c_name_of_variable (struct varobj *parent);

static char *c_name_of_child (struct varobj *parent, int index);

static char *c_path_expr_of_child (struct varobj *child);

static struct value *c_value_of_root (struct varobj **var_handle);

static struct value *c_value_of_child (struct varobj *parent, int index);

static struct type *c_type_of_child (struct varobj *parent, int index);

static int c_variable_editable (struct varobj *var);

static char *c_value_of_variable (struct varobj *var);

/* C++ implementation */

static int cplus_number_of_children (struct varobj *var);

static void cplus_class_num_children (struct type *type, int children[3]);

static char *cplus_name_of_variable (struct varobj *parent);

static char *cplus_name_of_child (struct varobj *parent, int index);

static char *cplus_path_expr_of_child (struct varobj *child);

static struct value *cplus_value_of_root (struct varobj **var_handle);

static struct value *cplus_value_of_child (struct varobj *parent, int index);

static struct type *cplus_type_of_child (struct varobj *parent, int index);

static int cplus_variable_editable (struct varobj *var);

static char *cplus_value_of_variable (struct varobj *var);

/* Java implementation */

static int java_number_of_children (struct varobj *var);

static char *java_name_of_variable (struct varobj *parent);

static char *java_name_of_child (struct varobj *parent, int index);

static char *java_path_expr_of_child (struct varobj *child);

static struct value *java_value_of_root (struct varobj **var_handle);

static struct value *java_value_of_child (struct varobj *parent, int index);

static struct type *java_type_of_child (struct varobj *parent, int index);

static int java_variable_editable (struct varobj *var);

static char *java_value_of_variable (struct varobj *var);

/* The language specific vector */

struct language_specific
{

  /* The language of this variable */
  enum varobj_languages language;

  /* The number of children of PARENT. */
  int (*number_of_children) (struct varobj * parent);

  /* The name (expression) of a root varobj. */
  char *(*name_of_variable) (struct varobj * parent);

  /* The name of the INDEX'th child of PARENT. */
  char *(*name_of_child) (struct varobj * parent, int index);

  /* Returns the rooted expression of CHILD, which is a variable
     obtain that has some parent.  */
  char *(*path_expr_of_child) (struct varobj * child);

  /* The ``struct value *'' of the root variable ROOT. */
  struct value *(*value_of_root) (struct varobj ** root_handle);

  /* The ``struct value *'' of the INDEX'th child of PARENT. */
  struct value *(*value_of_child) (struct varobj * parent, int index);

  /* The type of the INDEX'th child of PARENT. */
  struct type *(*type_of_child) (struct varobj * parent, int index);

  /* Is VAR editable? */
  int (*variable_editable) (struct varobj * var);

  /* The current value of VAR. */
  char *(*value_of_variable) (struct varobj * var);
};

/* Array of known source language routines. */
static struct language_specific languages[vlang_end] = {
  /* Unknown (try treating as C */
  {
   vlang_unknown,
   c_number_of_children,
   c_name_of_variable,
   c_name_of_child,
   c_path_expr_of_child,
   c_value_of_root,
   c_value_of_child,
   c_type_of_child,
   c_variable_editable,
   c_value_of_variable}
  ,
  /* C */
  {
   vlang_c,
   c_number_of_children,
   c_name_of_variable,
   c_name_of_child,
   c_path_expr_of_child,
   c_value_of_root,
   c_value_of_child,
   c_type_of_child,
   c_variable_editable,
   c_value_of_variable}
  ,
  /* C++ */
  {
   vlang_cplus,
   cplus_number_of_children,
   cplus_name_of_variable,
   cplus_name_of_child,
   cplus_path_expr_of_child,
   cplus_value_of_root,
   cplus_value_of_child,
   cplus_type_of_child,
   cplus_variable_editable,
   cplus_value_of_variable}
  ,
  /* Java */
  {
   vlang_java,
   java_number_of_children,
   java_name_of_variable,
   java_name_of_child,
   java_path_expr_of_child,
   java_value_of_root,
   java_value_of_child,
   java_type_of_child,
   java_variable_editable,
   java_value_of_variable}
};

/* A little convenience enum for dealing with C++/Java */
enum vsections
{
  v_public = 0, v_private, v_protected
};

/* Private data */

/* Mappings of varobj_display_formats enums to gdb's format codes */
static int format_code[] = { 0, 't', 'd', 'x', 'o' };

/* Header of the list of root variable objects */
static struct varobj_root *rootlist;
static int rootcount = 0;	/* number of root varobjs in the list */

/* Prime number indicating the number of buckets in the hash table */
/* A prime large enough to avoid too many colisions */
#define VAROBJ_TABLE_SIZE 227

/* Pointer to the varobj hash table (built at run time) */
static struct vlist **varobj_table;

/* Is the variable X one of our "fake" children? */
#define CPLUS_FAKE_CHILD(x) \
((x) != NULL && (x)->type == NULL && (x)->value == NULL)


/* API Implementation */
static int
is_root_p (struct varobj *var)
{
  return (var->root->rootvar == var);
}

/* Creates a varobj (not its children) */

/* Return the full FRAME which corresponds to the given CORE_ADDR
   or NULL if no FRAME on the chain corresponds to CORE_ADDR.  */

static struct frame_info *
find_frame_addr_in_frame_chain (CORE_ADDR frame_addr)
{
  struct frame_info *frame = NULL;

  if (frame_addr == (CORE_ADDR) 0)
    return NULL;

  while (1)
    {
      frame = get_prev_frame (frame);
      if (frame == NULL)
	return NULL;
      if (get_frame_base_address (frame) == frame_addr)
	return frame;
    }
}

struct varobj *
varobj_create (char *objname,
	       char *expression, CORE_ADDR frame, enum varobj_type type)
{
  struct varobj *var;
  struct frame_info *fi;
  struct frame_info *old_fi = NULL;
  struct block *block;
  struct cleanup *old_chain;

  /* Fill out a varobj structure for the (root) variable being constructed. */
  var = new_root_variable ();
  old_chain = make_cleanup_free_variable (var);

  if (expression != NULL)
    {
      char *p;
      enum varobj_languages lang;
      struct value *value = NULL;
      int expr_len;

      /* Parse and evaluate the expression, filling in as much
         of the variable's data as possible */

      /* Allow creator to specify context of variable */
      if ((type == USE_CURRENT_FRAME) || (type == USE_SELECTED_FRAME))
	fi = deprecated_safe_get_selected_frame ();
      else
	/* FIXME: cagney/2002-11-23: This code should be doing a
	   lookup using the frame ID and not just the frame's
	   ``address''.  This, of course, means an interface change.
	   However, with out that interface change ISAs, such as the
	   ia64 with its two stacks, won't work.  Similar goes for the
	   case where there is a frameless function.  */
	fi = find_frame_addr_in_frame_chain (frame);

      /* frame = -2 means always use selected frame */
      if (type == USE_SELECTED_FRAME)
	var->root->use_selected_frame = 1;

      block = NULL;
      if (fi != NULL)
	block = get_frame_block (fi, 0);

      p = expression;
      innermost_block = NULL;
      /* Wrap the call to parse expression, so we can 
         return a sensible error. */
      if (!gdb_parse_exp_1 (&p, block, 0, &var->root->exp))
	{
	  return NULL;
	}

      /* Don't allow variables to be created for types. */
      if (var->root->exp->elts[0].opcode == OP_TYPE)
	{
	  do_cleanups (old_chain);
	  fprintf_unfiltered (gdb_stderr, "Attempt to use a type name"
			      " as an expression.\n");
	  return NULL;
	}

      var->format = variable_default_display (var);
      var->root->valid_block = innermost_block;
      expr_len = strlen (expression);
      var->name = savestring (expression, expr_len);
      /* For a root var, the name and the expr are the same.  */
      var->path_expr = savestring (expression, expr_len);

      /* When the frame is different from the current frame, 
         we must select the appropriate frame before parsing
         the expression, otherwise the value will not be current.
         Since select_frame is so benign, just call it for all cases. */
      if (fi != NULL)
	{
	  var->root->frame = get_frame_id (fi);
	  old_fi = get_selected_frame (NULL);
	  select_frame (fi);
	}

      /* We definitively need to catch errors here.
         If evaluate_expression succeeds we got the value we wanted.
         But if it fails, we still go on with a call to evaluate_type()  */
      if (!gdb_evaluate_expression (var->root->exp, &value))
	{
	  /* Error getting the value.  Try to at least get the
	     right type.  */
	  struct value *type_only_value = evaluate_type (var->root->exp);
	  var->type = value_type (type_only_value);
	}
      else 
	var->type = value_type (value);

      install_new_value (var, value, 1 /* Initial assignment */);

      /* Set language info */
      lang = variable_language (var);
      var->root->lang = &languages[lang];

      /* Set ourselves as our root */
      var->root->rootvar = var;

      /* Reset the selected frame */
      if (fi != NULL)
	select_frame (old_fi);
    }

  /* If the variable object name is null, that means this
     is a temporary variable, so don't install it. */

  if ((var != NULL) && (objname != NULL))
    {
      var->obj_name = savestring (objname, strlen (objname));

      /* If a varobj name is duplicated, the install will fail so
         we must clenup */
      if (!install_variable (var))
	{
	  do_cleanups (old_chain);
	  return NULL;
	}
    }

  discard_cleanups (old_chain);
  return var;
}

/* Generates an unique name that can be used for a varobj */

char *
varobj_gen_name (void)
{
  static int id = 0;
  char *obj_name;

  /* generate a name for this object */
  id++;
  obj_name = xstrprintf ("var%d", id);

  return obj_name;
}

/* Given an "objname", returns the pointer to the corresponding varobj
   or NULL if not found */

struct varobj *
varobj_get_handle (char *objname)
{
  struct vlist *cv;
  const char *chp;
  unsigned int index = 0;
  unsigned int i = 1;

  for (chp = objname; *chp; chp++)
    {
      index = (index + (i++ * (unsigned int) *chp)) % VAROBJ_TABLE_SIZE;
    }

  cv = *(varobj_table + index);
  while ((cv != NULL) && (strcmp (cv->var->obj_name, objname) != 0))
    cv = cv->next;

  if (cv == NULL)
    error (_("Variable object not found"));

  return cv->var;
}

/* Given the handle, return the name of the object */

char *
varobj_get_objname (struct varobj *var)
{
  return var->obj_name;
}

/* Given the handle, return the expression represented by the object */

char *
varobj_get_expression (struct varobj *var)
{
  return name_of_variable (var);
}

/* Deletes a varobj and all its children if only_children == 0,
   otherwise deletes only the children; returns a malloc'ed list of all the 
   (malloc'ed) names of the variables that have been deleted (NULL terminated) */

int
varobj_delete (struct varobj *var, char ***dellist, int only_children)
{
  int delcount;
  int mycount;
  struct cpstack *result = NULL;
  char **cp;

  /* Initialize a stack for temporary results */
  cppush (&result, NULL);

  if (only_children)
    /* Delete only the variable children */
    delcount = delete_variable (&result, var, 1 /* only the children */ );
  else
    /* Delete the variable and all its children */
    delcount = delete_variable (&result, var, 0 /* parent+children */ );

  /* We may have been asked to return a list of what has been deleted */
  if (dellist != NULL)
    {
      *dellist = xmalloc ((delcount + 1) * sizeof (char *));

      cp = *dellist;
      mycount = delcount;
      *cp = cppop (&result);
      while ((*cp != NULL) && (mycount > 0))
	{
	  mycount--;
	  cp++;
	  *cp = cppop (&result);
	}

      if (mycount || (*cp != NULL))
	warning (_("varobj_delete: assertion failed - mycount(=%d) <> 0"),
		 mycount);
    }

  return delcount;
}

/* Set/Get variable object display format */

enum varobj_display_formats
varobj_set_display_format (struct varobj *var,
			   enum varobj_display_formats format)
{
  switch (format)
    {
    case FORMAT_NATURAL:
    case FORMAT_BINARY:
    case FORMAT_DECIMAL:
    case FORMAT_HEXADECIMAL:
    case FORMAT_OCTAL:
      var->format = format;
      break;

    default:
      var->format = variable_default_display (var);
    }

  return var->format;
}

enum varobj_display_formats
varobj_get_display_format (struct varobj *var)
{
  return var->format;
}

void
varobj_set_frozen (struct varobj *var, int frozen)
{
  /* When a variable is unfrozen, we don't fetch its value.
     The 'not_fetched' flag remains set, so next -var-update
     won't complain.

     We don't fetch the value, because for structures the client
     should do -var-update anyway.  It would be bad to have different
     client-size logic for structure and other types.  */
  var->frozen = frozen;
}

int
varobj_get_frozen (struct varobj *var)
{
  return var->frozen;
}


int
varobj_get_num_children (struct varobj *var)
{
  if (var->num_children == -1)
    var->num_children = number_of_children (var);

  return var->num_children;
}

/* Creates a list of the immediate children of a variable object;
   the return code is the number of such children or -1 on error */

int
varobj_list_children (struct varobj *var, struct varobj ***childlist)
{
  struct varobj *child;
  char *name;
  int i;

  /* sanity check: have we been passed a pointer? */
  if (childlist == NULL)
    return -1;

  *childlist = NULL;

  if (var->num_children == -1)
    var->num_children = number_of_children (var);

  /* If that failed, give up.  */
  if (var->num_children == -1)
    return -1;

  /* If we're called when the list of children is not yet initialized,
     allocate enough elements in it.  */
  while (VEC_length (varobj_p, var->children) < var->num_children)
    VEC_safe_push (varobj_p, var->children, NULL);

  /* List of children */
  *childlist = xmalloc ((var->num_children + 1) * sizeof (struct varobj *));

  for (i = 0; i < var->num_children; i++)
    {
      varobj_p existing;

      /* Mark as the end in case we bail out */
      *((*childlist) + i) = NULL;

      existing = VEC_index (varobj_p, var->children, i);

      if (existing == NULL)
	{
	  /* Either it's the first call to varobj_list_children for
	     this variable object, and the child was never created,
	     or it was explicitly deleted by the client.  */
	  name = name_of_child (var, i);
	  existing = create_child (var, i, name);
	  VEC_replace (varobj_p, var->children, i, existing);
	}

      *((*childlist) + i) = existing;
    }

  /* End of list is marked by a NULL pointer */
  *((*childlist) + i) = NULL;

  return var->num_children;
}

/* Obtain the type of an object Variable as a string similar to the one gdb
   prints on the console */

char *
varobj_get_type (struct varobj *var)
{
  struct value *val;
  struct cleanup *old_chain;
  struct ui_file *stb;
  char *thetype;
  long length;

  /* For the "fake" variables, do not return a type. (It's type is
     NULL, too.)
     Do not return a type for invalid variables as well.  */
  if (CPLUS_FAKE_CHILD (var) || !var->root->is_valid)
    return NULL;

  stb = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (stb);

  /* To print the type, we simply create a zero ``struct value *'' and
     cast it to our type. We then typeprint this variable. */
  val = value_zero (var->type, not_lval);
  type_print (value_type (val), "", stb, -1);

  thetype = ui_file_xstrdup (stb, &length);
  do_cleanups (old_chain);
  return thetype;
}

/* Obtain the type of an object variable.  */

struct type *
varobj_get_gdb_type (struct varobj *var)
{
  return var->type;
}

/* Return a pointer to the full rooted expression of varobj VAR.
   If it has not been computed yet, compute it.  */
char *
varobj_get_path_expr (struct varobj *var)
{
  if (var->path_expr != NULL)
    return var->path_expr;
  else 
    {
      /* For root varobjs, we initialize path_expr
	 when creating varobj, so here it should be
	 child varobj.  */
      gdb_assert (!is_root_p (var));
      return (*var->root->lang->path_expr_of_child) (var);
    }
}

enum varobj_languages
varobj_get_language (struct varobj *var)
{
  return variable_language (var);
}

int
varobj_get_attributes (struct varobj *var)
{
  int attributes = 0;

  if (var->root->is_valid && variable_editable (var))
    /* FIXME: define masks for attributes */
    attributes |= 0x00000001;	/* Editable */

  return attributes;
}

char *
varobj_get_value (struct varobj *var)
{
  return my_value_of_variable (var);
}

/* Set the value of an object variable (if it is editable) to the
   value of the given expression */
/* Note: Invokes functions that can call error() */

int
varobj_set_value (struct varobj *var, char *expression)
{
  struct value *val;
  int offset = 0;
  int error = 0;

  /* The argument "expression" contains the variable's new value.
     We need to first construct a legal expression for this -- ugh! */
  /* Does this cover all the bases? */
  struct expression *exp;
  struct value *value;
  int saved_input_radix = input_radix;

  if (var->value != NULL && variable_editable (var))
    {
      char *s = expression;
      int i;

      input_radix = 10;		/* ALWAYS reset to decimal temporarily */
      exp = parse_exp_1 (&s, 0, 0);
      if (!gdb_evaluate_expression (exp, &value))
	{
	  /* We cannot proceed without a valid expression. */
	  xfree (exp);
	  return 0;
	}

      /* All types that are editable must also be changeable.  */
      gdb_assert (varobj_value_is_changeable_p (var));

      /* The value of a changeable variable object must not be lazy.  */
      gdb_assert (!value_lazy (var->value));

      /* Need to coerce the input.  We want to check if the
	 value of the variable object will be different
	 after assignment, and the first thing value_assign
	 does is coerce the input.
	 For example, if we are assigning an array to a pointer variable we
	 should compare the pointer with the the array's address, not with the
	 array's content.  */
      value = coerce_array (value);

      /* The new value may be lazy.  gdb_value_assign, or 
	 rather value_contents, will take care of this.
	 If fetching of the new value will fail, gdb_value_assign
	 with catch the exception.  */
      if (!gdb_value_assign (var->value, value, &val))
	return 0;
     
      /* If the value has changed, record it, so that next -var-update can
	 report this change.  If a variable had a value of '1', we've set it
	 to '333' and then set again to '1', when -var-update will report this
	 variable as changed -- because the first assignment has set the
	 'updated' flag.  There's no need to optimize that, because return value
	 of -var-update should be considered an approximation.  */
      var->updated = install_new_value (var, val, 0 /* Compare values. */);
      input_radix = saved_input_radix;
      return 1;
    }

  return 0;
}

/* Returns a malloc'ed list with all root variable objects */
int
varobj_list (struct varobj ***varlist)
{
  struct varobj **cv;
  struct varobj_root *croot;
  int mycount = rootcount;

  /* Alloc (rootcount + 1) entries for the result */
  *varlist = xmalloc ((rootcount + 1) * sizeof (struct varobj *));

  cv = *varlist;
  croot = rootlist;
  while ((croot != NULL) && (mycount > 0))
    {
      *cv = croot->rootvar;
      mycount--;
      cv++;
      croot = croot->next;
    }
  /* Mark the end of the list */
  *cv = NULL;

  if (mycount || (croot != NULL))
    warning
      ("varobj_list: assertion failed - wrong tally of root vars (%d:%d)",
       rootcount, mycount);

  return rootcount;
}

/* Assign a new value to a variable object.  If INITIAL is non-zero,
   this is the first assignement after the variable object was just
   created, or changed type.  In that case, just assign the value 
   and return 0.
   Otherwise, assign the value and if type_changeable returns non-zero,
   find if the new value is different from the current value.
   Return 1 if so, and 0 if the values are equal.  

   The VALUE parameter should not be released -- the function will
   take care of releasing it when needed.  */
static int
install_new_value (struct varobj *var, struct value *value, int initial)
{ 
  int changeable;
  int need_to_fetch;
  int changed = 0;
  int intentionally_not_fetched = 0;

  /* We need to know the varobj's type to decide if the value should
     be fetched or not.  C++ fake children (public/protected/private) don't have
     a type. */
  gdb_assert (var->type || CPLUS_FAKE_CHILD (var));
  changeable = varobj_value_is_changeable_p (var);
  need_to_fetch = changeable;

  /* We are not interested in the address of references, and given
     that in C++ a reference is not rebindable, it cannot
     meaningfully change.  So, get hold of the real value.  */
  if (value)
    {
      value = coerce_ref (value);
      release_value (value);
    }

  if (var->type && TYPE_CODE (var->type) == TYPE_CODE_UNION)
    /* For unions, we need to fetch the value implicitly because
       of implementation of union member fetch.  When gdb
       creates a value for a field and the value of the enclosing
       structure is not lazy,  it immediately copies the necessary
       bytes from the enclosing values.  If the enclosing value is
       lazy, the call to value_fetch_lazy on the field will read
       the data from memory.  For unions, that means we'll read the
       same memory more than once, which is not desirable.  So
       fetch now.  */
    need_to_fetch = 1;

  /* The new value might be lazy.  If the type is changeable,
     that is we'll be comparing values of this type, fetch the
     value now.  Otherwise, on the next update the old value
     will be lazy, which means we've lost that old value.  */
  if (need_to_fetch && value && value_lazy (value))
    {
      struct varobj *parent = var->parent;
      int frozen = var->frozen;
      for (; !frozen && parent; parent = parent->parent)
	frozen |= parent->frozen;

      if (frozen && initial)
	{
	  /* For variables that are frozen, or are children of frozen
	     variables, we don't do fetch on initial assignment.
	     For non-initial assignemnt we do the fetch, since it means we're
	     explicitly asked to compare the new value with the old one.  */
	  intentionally_not_fetched = 1;
	}
      else if (!gdb_value_fetch_lazy (value))
	{
	  /* Set the value to NULL, so that for the next -var-update,
	     we don't try to compare the new value with this value,
	     that we couldn't even read.  */
	  value = NULL;
	}
    }

  /* If the type is changeable, compare the old and the new values.
     If this is the initial assignment, we don't have any old value
     to compare with.  */
  if (initial && changeable)
    var->print_value = value_get_print_value (value, var->format);
  else if (changeable)
    {
      /* If the value of the varobj was changed by -var-set-value, then the 
	 value in the varobj and in the target is the same.  However, that value
	 is different from the value that the varobj had after the previous
	 -var-update. So need to the varobj as changed.  */
      if (var->updated)
	{
	  xfree (var->print_value);
	  var->print_value = value_get_print_value (value, var->format);
	  changed = 1;
	}
      else 
	{
	  /* Try to compare the values.  That requires that both
	     values are non-lazy.  */
	  if (var->not_fetched && value_lazy (var->value))
	    {
	      /* This is a frozen varobj and the value was never read.
		 Presumably, UI shows some "never read" indicator.
		 Now that we've fetched the real value, we need to report
		 this varobj as changed so that UI can show the real
		 value.  */
	      changed = 1;
	    }
          else  if (var->value == NULL && value == NULL)
	    /* Equal. */
	    ;
	  else if (var->value == NULL || value == NULL)
	    {
	      xfree (var->print_value);
	      var->print_value = value_get_print_value (value, var->format);
	      changed = 1;
	    }
	  else
	    {
	      char *print_value;
	      gdb_assert (!value_lazy (var->value));
	      gdb_assert (!value_lazy (value));
	      print_value = value_get_print_value (value, var->format);

	      gdb_assert (var->print_value != NULL && print_value != NULL);
	      if (strcmp (var->print_value, print_value) != 0)
		{
		  xfree (var->print_value);
		  var->print_value = print_value;
		  changed = 1;
		}
	      else
		xfree (print_value);
	    }
	}
    }

  /* We must always keep the new value, since children depend on it.  */
  if (var->value != NULL && var->value != value)
    value_free (var->value);
  var->value = value;
  if (value && value_lazy (value) && intentionally_not_fetched)
    var->not_fetched = 1;
  else
    var->not_fetched = 0;
  var->updated = 0;

  gdb_assert (!var->value || value_type (var->value));

  return changed;
}

/* Update the values for a variable and its children.  This is a
   two-pronged attack.  First, re-parse the value for the root's
   expression to see if it's changed.  Then go all the way
   through its children, reconstructing them and noting if they've
   changed.
   Return value: 
    < 0 for error values, see varobj.h.
    Otherwise it is the number of children + parent changed.

   The EXPLICIT parameter specifies if this call is result
   of MI request to update this specific variable, or 
   result of implicit -var-update *. For implicit request, we don't
   update frozen variables.

   NOTE: This function may delete the caller's varobj. If it
   returns TYPE_CHANGED, then it has done this and VARP will be modified
   to point to the new varobj.  */

int
varobj_update (struct varobj **varp, struct varobj ***changelist,
	       int explicit)
{
  int changed = 0;
  int type_changed = 0;
  int i;
  int vleft;
  struct varobj *v;
  struct varobj **cv;
  struct varobj **templist = NULL;
  struct value *new;
  VEC (varobj_p) *stack = NULL;
  VEC (varobj_p) *result = NULL;
  struct frame_id old_fid;
  struct frame_info *fi;

  /* sanity check: have we been passed a pointer?  */
  gdb_assert (changelist);

  /* Frozen means frozen -- we don't check for any change in
     this varobj, including its going out of scope, or
     changing type.  One use case for frozen varobjs is
     retaining previously evaluated expressions, and we don't
     want them to be reevaluated at all.  */
  if (!explicit && (*varp)->frozen)
    return 0;

  if (!(*varp)->root->is_valid)
    return INVALID;

  if ((*varp)->root->rootvar == *varp)
    {
      /* Save the selected stack frame, since we will need to change it
	 in order to evaluate expressions.  */
      old_fid = get_frame_id (deprecated_safe_get_selected_frame ());
      
      /* Update the root variable. value_of_root can return NULL
	 if the variable is no longer around, i.e. we stepped out of
	 the frame in which a local existed. We are letting the 
	 value_of_root variable dispose of the varobj if the type
	 has changed.  */
      type_changed = 1;
      new = value_of_root (varp, &type_changed);

      /* Restore selected frame.  */
      fi = frame_find_by_id (old_fid);
      if (fi)
	select_frame (fi);
      
      /* If this is a "use_selected_frame" varobj, and its type has changed,
	 them note that it's changed.  */
      if (type_changed)
	VEC_safe_push (varobj_p, result, *varp);
      
        if (install_new_value ((*varp), new, type_changed))
	  {
	    /* If type_changed is 1, install_new_value will never return
	       non-zero, so we'll never report the same variable twice.  */
	    gdb_assert (!type_changed);
	    VEC_safe_push (varobj_p, result, *varp);
	  }

      if (new == NULL)
	{
	  /* This means the varobj itself is out of scope.
	     Report it.  */
	  VEC_free (varobj_p, result);
	  return NOT_IN_SCOPE;
	}
    }

  VEC_safe_push (varobj_p, stack, *varp);

  /* Walk through the children, reconstructing them all.  */
  while (!VEC_empty (varobj_p, stack))
    {
      v = VEC_pop (varobj_p, stack);

      /* Push any children.  Use reverse order so that the first
	 child is popped from the work stack first, and so
	 will be added to result first.  This does not
	 affect correctness, just "nicer".  */
      for (i = VEC_length (varobj_p, v->children)-1; i >= 0; --i)
	{
	  varobj_p c = VEC_index (varobj_p, v->children, i);
	  /* Child may be NULL if explicitly deleted by -var-delete.  */
	  if (c != NULL && !c->frozen)
	    VEC_safe_push (varobj_p, stack, c);
	}

      /* Update this variable, unless it's a root, which is already
	 updated.  */
      if (v->root->rootvar != v)
	{	  
	  new = value_of_child (v->parent, v->index);
	  if (install_new_value (v, new, 0 /* type not changed */))
	    {
	      /* Note that it's changed */
	      VEC_safe_push (varobj_p, result, v);
	      v->updated = 0;
	    }
	}
    }

  /* Alloc (changed + 1) list entries.  */
  changed = VEC_length (varobj_p, result);
  *changelist = xmalloc ((changed + 1) * sizeof (struct varobj *));
  cv = *changelist;

  for (i = 0; i < changed; ++i)
    {
      *cv = VEC_index (varobj_p, result, i);
      gdb_assert (*cv != NULL);
      ++cv;
    }
  *cv = 0;

  VEC_free (varobj_p, stack);
  VEC_free (varobj_p, result);

  if (type_changed)
    return TYPE_CHANGED;
  else
    return changed;
}


/* Helper functions */

/*
 * Variable object construction/destruction
 */

static int
delete_variable (struct cpstack **resultp, struct varobj *var,
		 int only_children_p)
{
  int delcount = 0;

  delete_variable_1 (resultp, &delcount, var,
		     only_children_p, 1 /* remove_from_parent_p */ );

  return delcount;
}

/* Delete the variable object VAR and its children */
/* IMPORTANT NOTE: If we delete a variable which is a child
   and the parent is not removed we dump core.  It must be always
   initially called with remove_from_parent_p set */
static void
delete_variable_1 (struct cpstack **resultp, int *delcountp,
		   struct varobj *var, int only_children_p,
		   int remove_from_parent_p)
{
  int i;

  /* Delete any children of this variable, too. */
  for (i = 0; i < VEC_length (varobj_p, var->children); ++i)
    {   
      varobj_p child = VEC_index (varobj_p, var->children, i);
      if (!remove_from_parent_p)
	child->parent = NULL;
      delete_variable_1 (resultp, delcountp, child, 0, only_children_p);
    }
  VEC_free (varobj_p, var->children);

  /* if we were called to delete only the children we are done here */
  if (only_children_p)
    return;

  /* Otherwise, add it to the list of deleted ones and proceed to do so */
  /* If the name is null, this is a temporary variable, that has not
     yet been installed, don't report it, it belongs to the caller... */
  if (var->obj_name != NULL)
    {
      cppush (resultp, xstrdup (var->obj_name));
      *delcountp = *delcountp + 1;
    }

  /* If this variable has a parent, remove it from its parent's list */
  /* OPTIMIZATION: if the parent of this variable is also being deleted, 
     (as indicated by remove_from_parent_p) we don't bother doing an
     expensive list search to find the element to remove when we are
     discarding the list afterwards */
  if ((remove_from_parent_p) && (var->parent != NULL))
    {
      VEC_replace (varobj_p, var->parent->children, var->index, NULL);
    }

  if (var->obj_name != NULL)
    uninstall_variable (var);

  /* Free memory associated with this variable */
  free_variable (var);
}

/* Install the given variable VAR with the object name VAR->OBJ_NAME. */
static int
install_variable (struct varobj *var)
{
  struct vlist *cv;
  struct vlist *newvl;
  const char *chp;
  unsigned int index = 0;
  unsigned int i = 1;

  for (chp = var->obj_name; *chp; chp++)
    {
      index = (index + (i++ * (unsigned int) *chp)) % VAROBJ_TABLE_SIZE;
    }

  cv = *(varobj_table + index);
  while ((cv != NULL) && (strcmp (cv->var->obj_name, var->obj_name) != 0))
    cv = cv->next;

  if (cv != NULL)
    error (_("Duplicate variable object name"));

  /* Add varobj to hash table */
  newvl = xmalloc (sizeof (struct vlist));
  newvl->next = *(varobj_table + index);
  newvl->var = var;
  *(varobj_table + index) = newvl;

  /* If root, add varobj to root list */
  if (is_root_p (var))
    {
      /* Add to list of root variables */
      if (rootlist == NULL)
	var->root->next = NULL;
      else
	var->root->next = rootlist;
      rootlist = var->root;
      rootcount++;
    }

  return 1;			/* OK */
}

/* Unistall the object VAR. */
static void
uninstall_variable (struct varobj *var)
{
  struct vlist *cv;
  struct vlist *prev;
  struct varobj_root *cr;
  struct varobj_root *prer;
  const char *chp;
  unsigned int index = 0;
  unsigned int i = 1;

  /* Remove varobj from hash table */
  for (chp = var->obj_name; *chp; chp++)
    {
      index = (index + (i++ * (unsigned int) *chp)) % VAROBJ_TABLE_SIZE;
    }

  cv = *(varobj_table + index);
  prev = NULL;
  while ((cv != NULL) && (strcmp (cv->var->obj_name, var->obj_name) != 0))
    {
      prev = cv;
      cv = cv->next;
    }

  if (varobjdebug)
    fprintf_unfiltered (gdb_stdlog, "Deleting %s\n", var->obj_name);

  if (cv == NULL)
    {
      warning
	("Assertion failed: Could not find variable object \"%s\" to delete",
	 var->obj_name);
      return;
    }

  if (prev == NULL)
    *(varobj_table + index) = cv->next;
  else
    prev->next = cv->next;

  xfree (cv);

  /* If root, remove varobj from root list */
  if (is_root_p (var))
    {
      /* Remove from list of root variables */
      if (rootlist == var->root)
	rootlist = var->root->next;
      else
	{
	  prer = NULL;
	  cr = rootlist;
	  while ((cr != NULL) && (cr->rootvar != var))
	    {
	      prer = cr;
	      cr = cr->next;
	    }
	  if (cr == NULL)
	    {
	      warning
		("Assertion failed: Could not find varobj \"%s\" in root list",
		 var->obj_name);
	      return;
	    }
	  if (prer == NULL)
	    rootlist = NULL;
	  else
	    prer->next = cr->next;
	}
      rootcount--;
    }

}

/* Create and install a child of the parent of the given name */
static struct varobj *
create_child (struct varobj *parent, int index, char *name)
{
  struct varobj *child;
  char *childs_name;
  struct value *value;

  child = new_variable ();

  /* name is allocated by name_of_child */
  child->name = name;
  child->index = index;
  value = value_of_child (parent, index);
  child->parent = parent;
  child->root = parent->root;
  childs_name = xstrprintf ("%s.%s", parent->obj_name, name);
  child->obj_name = childs_name;
  install_variable (child);

  /* Compute the type of the child.  Must do this before
     calling install_new_value.  */
  if (value != NULL)
    /* If the child had no evaluation errors, var->value
       will be non-NULL and contain a valid type. */
    child->type = value_type (value);
  else
    /* Otherwise, we must compute the type. */
    child->type = (*child->root->lang->type_of_child) (child->parent, 
						       child->index);
  install_new_value (child, value, 1);

  return child;
}


/*
 * Miscellaneous utility functions.
 */

/* Allocate memory and initialize a new variable */
static struct varobj *
new_variable (void)
{
  struct varobj *var;

  var = (struct varobj *) xmalloc (sizeof (struct varobj));
  var->name = NULL;
  var->path_expr = NULL;
  var->obj_name = NULL;
  var->index = -1;
  var->type = NULL;
  var->value = NULL;
  var->num_children = -1;
  var->parent = NULL;
  var->children = NULL;
  var->format = 0;
  var->root = NULL;
  var->updated = 0;
  var->print_value = NULL;
  var->frozen = 0;
  var->not_fetched = 0;

  return var;
}

/* Allocate memory and initialize a new root variable */
static struct varobj *
new_root_variable (void)
{
  struct varobj *var = new_variable ();
  var->root = (struct varobj_root *) xmalloc (sizeof (struct varobj_root));;
  var->root->lang = NULL;
  var->root->exp = NULL;
  var->root->valid_block = NULL;
  var->root->frame = null_frame_id;
  var->root->use_selected_frame = 0;
  var->root->rootvar = NULL;
  var->root->is_valid = 1;

  return var;
}

/* Free any allocated memory associated with VAR. */
static void
free_variable (struct varobj *var)
{
  /* Free the expression if this is a root variable. */
  if (is_root_p (var))
    {
      free_current_contents (&var->root->exp);
      xfree (var->root);
    }

  xfree (var->name);
  xfree (var->obj_name);
  xfree (var->print_value);
  xfree (var->path_expr);
  xfree (var);
}

static void
do_free_variable_cleanup (void *var)
{
  free_variable (var);
}

static struct cleanup *
make_cleanup_free_variable (struct varobj *var)
{
  return make_cleanup (do_free_variable_cleanup, var);
}

/* This returns the type of the variable. It also skips past typedefs
   to return the real type of the variable.

   NOTE: TYPE_TARGET_TYPE should NOT be used anywhere in this file
   except within get_target_type and get_type. */
static struct type *
get_type (struct varobj *var)
{
  struct type *type;
  type = var->type;

  if (type != NULL)
    type = check_typedef (type);

  return type;
}

/* Return the type of the value that's stored in VAR,
   or that would have being stored there if the
   value were accessible.  

   This differs from VAR->type in that VAR->type is always
   the true type of the expession in the source language.
   The return value of this function is the type we're
   actually storing in varobj, and using for displaying
   the values and for comparing previous and new values.

   For example, top-level references are always stripped.  */
static struct type *
get_value_type (struct varobj *var)
{
  struct type *type;

  if (var->value)
    type = value_type (var->value);
  else
    type = var->type;

  type = check_typedef (type);

  if (TYPE_CODE (type) == TYPE_CODE_REF)
    type = get_target_type (type);

  type = check_typedef (type);

  return type;
}

/* This returns the target type (or NULL) of TYPE, also skipping
   past typedefs, just like get_type ().

   NOTE: TYPE_TARGET_TYPE should NOT be used anywhere in this file
   except within get_target_type and get_type. */
static struct type *
get_target_type (struct type *type)
{
  if (type != NULL)
    {
      type = TYPE_TARGET_TYPE (type);
      if (type != NULL)
	type = check_typedef (type);
    }

  return type;
}

/* What is the default display for this variable? We assume that
   everything is "natural". Any exceptions? */
static enum varobj_display_formats
variable_default_display (struct varobj *var)
{
  return FORMAT_NATURAL;
}

/* FIXME: The following should be generic for any pointer */
static void
cppush (struct cpstack **pstack, char *name)
{
  struct cpstack *s;

  s = (struct cpstack *) xmalloc (sizeof (struct cpstack));
  s->name = name;
  s->next = *pstack;
  *pstack = s;
}

/* FIXME: The following should be generic for any pointer */
static char *
cppop (struct cpstack **pstack)
{
  struct cpstack *s;
  char *v;

  if ((*pstack)->name == NULL && (*pstack)->next == NULL)
    return NULL;

  s = *pstack;
  v = s->name;
  *pstack = (*pstack)->next;
  xfree (s);

  return v;
}

/*
 * Language-dependencies
 */

/* Common entry points */

/* Get the language of variable VAR. */
static enum varobj_languages
variable_language (struct varobj *var)
{
  enum varobj_languages lang;

  switch (var->root->exp->language_defn->la_language)
    {
    default:
    case language_c:
      lang = vlang_c;
      break;
    case language_cplus:
      lang = vlang_cplus;
      break;
    case language_java:
      lang = vlang_java;
      break;
    }

  return lang;
}

/* Return the number of children for a given variable.
   The result of this function is defined by the language
   implementation. The number of children returned by this function
   is the number of children that the user will see in the variable
   display. */
static int
number_of_children (struct varobj *var)
{
  return (*var->root->lang->number_of_children) (var);;
}

/* What is the expression for the root varobj VAR? Returns a malloc'd string. */
static char *
name_of_variable (struct varobj *var)
{
  return (*var->root->lang->name_of_variable) (var);
}

/* What is the name of the INDEX'th child of VAR? Returns a malloc'd string. */
static char *
name_of_child (struct varobj *var, int index)
{
  return (*var->root->lang->name_of_child) (var, index);
}

/* What is the ``struct value *'' of the root variable VAR? 
   TYPE_CHANGED controls what to do if the type of a
   use_selected_frame = 1 variable changes.  On input,
   TYPE_CHANGED = 1 means discard the old varobj, and replace
   it with this one.  TYPE_CHANGED = 0 means leave it around.
   NB: In both cases, var_handle will point to the new varobj,
   so if you use TYPE_CHANGED = 0, you will have to stash the
   old varobj pointer away somewhere before calling this.
   On return, TYPE_CHANGED will be 1 if the type has changed, and 
   0 otherwise. */
static struct value *
value_of_root (struct varobj **var_handle, int *type_changed)
{
  struct varobj *var;

  if (var_handle == NULL)
    return NULL;

  var = *var_handle;

  /* This should really be an exception, since this should
     only get called with a root variable. */

  if (!is_root_p (var))
    return NULL;

  if (var->root->use_selected_frame)
    {
      struct varobj *tmp_var;
      char *old_type, *new_type;

      tmp_var = varobj_create (NULL, var->name, (CORE_ADDR) 0,
			       USE_SELECTED_FRAME);
      if (tmp_var == NULL)
	{
	  return NULL;
	}
      old_type = varobj_get_type (var);
      new_type = varobj_get_type (tmp_var);
      if (strcmp (old_type, new_type) == 0)
	{
	  varobj_delete (tmp_var, NULL, 0);
	  *type_changed = 0;
	}
      else
	{
	  if (*type_changed)
	    {
	      tmp_var->obj_name =
		savestring (var->obj_name, strlen (var->obj_name));
	      varobj_delete (var, NULL, 0);
	    }
	  else
	    {
	      tmp_var->obj_name = varobj_gen_name ();
	    }
	  install_variable (tmp_var);
	  *var_handle = tmp_var;
	  var = *var_handle;
	  *type_changed = 1;
	}
      xfree (old_type);
      xfree (new_type);
    }
  else
    {
      *type_changed = 0;
    }

  return (*var->root->lang->value_of_root) (var_handle);
}

/* What is the ``struct value *'' for the INDEX'th child of PARENT? */
static struct value *
value_of_child (struct varobj *parent, int index)
{
  struct value *value;

  value = (*parent->root->lang->value_of_child) (parent, index);

  return value;
}

/* Is this variable editable? Use the variable's type to make
   this determination. */
static int
variable_editable (struct varobj *var)
{
  return (*var->root->lang->variable_editable) (var);
}

/* GDB already has a command called "value_of_variable". Sigh. */
static char *
my_value_of_variable (struct varobj *var)
{
  if (var->root->is_valid)
    return (*var->root->lang->value_of_variable) (var);
  else
    return NULL;
}

static char *
value_get_print_value (struct value *value, enum varobj_display_formats format)
{
  long dummy;
  struct ui_file *stb;
  struct cleanup *old_chain;
  char *thevalue;

  if (value == NULL)
    return NULL;

  stb = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (stb);

  common_val_print (value, stb, format_code[(int) format], 1, 0, 0);
  thevalue = ui_file_xstrdup (stb, &dummy);

  do_cleanups (old_chain);
  return thevalue;
}

/* Return non-zero if changes in value of VAR
   must be detected and reported by -var-update.
   Return zero is -var-update should never report
   changes of such values.  This makes sense for structures
   (since the changes in children values will be reported separately),
   or for artifical objects (like 'public' pseudo-field in C++).

   Return value of 0 means that gdb need not call value_fetch_lazy
   for the value of this variable object.  */
static int
varobj_value_is_changeable_p (struct varobj *var)
{
  int r;
  struct type *type;

  if (CPLUS_FAKE_CHILD (var))
    return 0;

  type = get_value_type (var);

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ARRAY:
      r = 0;
      break;

    default:
      r = 1;
    }

  return r;
}

/* Given the value and the type of a variable object,
   adjust the value and type to those necessary
   for getting children of the variable object.
   This includes dereferencing top-level references
   to all types and dereferencing pointers to
   structures.  

   Both TYPE and *TYPE should be non-null. VALUE
   can be null if we want to only translate type.
   *VALUE can be null as well -- if the parent
   value is not known.  

   If WAS_PTR is not NULL, set *WAS_PTR to 0 or 1
   depending on whether pointer was deferenced
   in this function.  */
static void
adjust_value_for_child_access (struct value **value,
				  struct type **type,
				  int *was_ptr)
{
  gdb_assert (type && *type);

  if (was_ptr)
    *was_ptr = 0;

  *type = check_typedef (*type);
  
  /* The type of value stored in varobj, that is passed
     to us, is already supposed to be
     reference-stripped.  */

  gdb_assert (TYPE_CODE (*type) != TYPE_CODE_REF);

  /* Pointers to structures are treated just like
     structures when accessing children.  Don't
     dererences pointers to other types.  */
  if (TYPE_CODE (*type) == TYPE_CODE_PTR)
    {
      struct type *target_type = get_target_type (*type);
      if (TYPE_CODE (target_type) == TYPE_CODE_STRUCT
	  || TYPE_CODE (target_type) == TYPE_CODE_UNION)
	{
	  if (value && *value)
	    gdb_value_ind (*value, value);	  
	  *type = target_type;
	  if (was_ptr)
	    *was_ptr = 1;
	}
    }

  /* The 'get_target_type' function calls check_typedef on
     result, so we can immediately check type code.  No
     need to call check_typedef here.  */
}

/* C */
static int
c_number_of_children (struct varobj *var)
{
  struct type *type = get_value_type (var);
  int children = 0;
  struct type *target;

  adjust_value_for_child_access (NULL, &type, NULL);
  target = get_target_type (type);

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (target) > 0
	  && TYPE_ARRAY_UPPER_BOUND_TYPE (type) != BOUND_CANNOT_BE_DETERMINED)
	children = TYPE_LENGTH (type) / TYPE_LENGTH (target);
      else
	/* If we don't know how many elements there are, don't display
	   any.  */
	children = 0;
      break;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      children = TYPE_NFIELDS (type);
      break;

    case TYPE_CODE_PTR:
      /* The type here is a pointer to non-struct. Typically, pointers
	 have one child, except for function ptrs, which have no children,
	 and except for void*, as we don't know what to show.

         We can show char* so we allow it to be dereferenced.  If you decide
         to test for it, please mind that a little magic is necessary to
         properly identify it: char* has TYPE_CODE == TYPE_CODE_INT and 
         TYPE_NAME == "char" */
      if (TYPE_CODE (target) == TYPE_CODE_FUNC
	  || TYPE_CODE (target) == TYPE_CODE_VOID)
	children = 0;
      else
	children = 1;
      break;

    default:
      /* Other types have no children */
      break;
    }

  return children;
}

static char *
c_name_of_variable (struct varobj *parent)
{
  return savestring (parent->name, strlen (parent->name));
}

/* Return the value of element TYPE_INDEX of a structure
   value VALUE.  VALUE's type should be a structure,
   or union, or a typedef to struct/union.  

   Returns NULL if getting the value fails.  Never throws.  */
static struct value *
value_struct_element_index (struct value *value, int type_index)
{
  struct value *result = NULL;
  volatile struct gdb_exception e;

  struct type *type = value_type (value);
  type = check_typedef (type);

  gdb_assert (TYPE_CODE (type) == TYPE_CODE_STRUCT
	      || TYPE_CODE (type) == TYPE_CODE_UNION);

  TRY_CATCH (e, RETURN_MASK_ERROR)
    {
      if (TYPE_FIELD_STATIC (type, type_index))
	result = value_static_field (type, type_index);
      else
	result = value_primitive_field (value, 0, type_index, type);
    }
  if (e.reason < 0)
    {
      return NULL;
    }
  else
    {
      return result;
    }
}

/* Obtain the information about child INDEX of the variable
   object PARENT.  
   If CNAME is not null, sets *CNAME to the name of the child relative
   to the parent.
   If CVALUE is not null, sets *CVALUE to the value of the child.
   If CTYPE is not null, sets *CTYPE to the type of the child.

   If any of CNAME, CVALUE, or CTYPE is not null, but the corresponding
   information cannot be determined, set *CNAME, *CVALUE, or *CTYPE
   to NULL.  */
static void 
c_describe_child (struct varobj *parent, int index,
		  char **cname, struct value **cvalue, struct type **ctype,
		  char **cfull_expression)
{
  struct value *value = parent->value;
  struct type *type = get_value_type (parent);
  char *parent_expression = NULL;
  int was_ptr;

  if (cname)
    *cname = NULL;
  if (cvalue)
    *cvalue = NULL;
  if (ctype)
    *ctype = NULL;
  if (cfull_expression)
    {
      *cfull_expression = NULL;
      parent_expression = varobj_get_path_expr (parent);
    }
  adjust_value_for_child_access (&value, &type, &was_ptr);
      
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (cname)
	*cname = xstrprintf ("%d", index
			     + TYPE_LOW_BOUND (TYPE_INDEX_TYPE (type)));

      if (cvalue && value)
	{
	  int real_index = index + TYPE_LOW_BOUND (TYPE_INDEX_TYPE (type));
	  struct value *indval = 
	    value_from_longest (builtin_type_int, (LONGEST) real_index);
	  gdb_value_subscript (value, indval, cvalue);
	}

      if (ctype)
	*ctype = get_target_type (type);

      if (cfull_expression)
	*cfull_expression = xstrprintf ("(%s)[%d]", parent_expression, 
					index
					+ TYPE_LOW_BOUND (TYPE_INDEX_TYPE (type)));


      break;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      if (cname)
	{
	  char *string = TYPE_FIELD_NAME (type, index);
	  *cname = savestring (string, strlen (string));
	}

      if (cvalue && value)
	{
	  /* For C, varobj index is the same as type index.  */
	  *cvalue = value_struct_element_index (value, index);
	}

      if (ctype)
	*ctype = TYPE_FIELD_TYPE (type, index);

      if (cfull_expression)
	{
	  char *join = was_ptr ? "->" : ".";
	  *cfull_expression = xstrprintf ("(%s)%s%s", parent_expression, join,
					  TYPE_FIELD_NAME (type, index));
	}

      break;

    case TYPE_CODE_PTR:
      if (cname)
	*cname = xstrprintf ("*%s", parent->name);

      if (cvalue && value)
	gdb_value_ind (value, cvalue);

      /* Don't use get_target_type because it calls
	 check_typedef and here, we want to show the true
	 declared type of the variable.  */
      if (ctype)
	*ctype = TYPE_TARGET_TYPE (type);

      if (cfull_expression)
	*cfull_expression = xstrprintf ("*(%s)", parent_expression);
      
      break;

    default:
      /* This should not happen */
      if (cname)
	*cname = xstrdup ("???");
      if (cfull_expression)
	*cfull_expression = xstrdup ("???");
      /* Don't set value and type, we don't know then. */
    }
}

static char *
c_name_of_child (struct varobj *parent, int index)
{
  char *name;
  c_describe_child (parent, index, &name, NULL, NULL, NULL);
  return name;
}

static char *
c_path_expr_of_child (struct varobj *child)
{
  c_describe_child (child->parent, child->index, NULL, NULL, NULL, 
		    &child->path_expr);
  return child->path_expr;
}

static struct value *
c_value_of_root (struct varobj **var_handle)
{
  struct value *new_val = NULL;
  struct varobj *var = *var_handle;
  struct frame_info *fi;
  int within_scope;

  /*  Only root variables can be updated... */
  if (!is_root_p (var))
    /* Not a root var */
    return NULL;


  /* Determine whether the variable is still around. */
  if (var->root->valid_block == NULL || var->root->use_selected_frame)
    within_scope = 1;
  else
    {
      fi = frame_find_by_id (var->root->frame);
      within_scope = fi != NULL;
      /* FIXME: select_frame could fail */
      if (fi)
	{
	  CORE_ADDR pc = get_frame_pc (fi);
	  if (pc <  BLOCK_START (var->root->valid_block) ||
	      pc >= BLOCK_END (var->root->valid_block))
	    within_scope = 0;
	  else
	    select_frame (fi);
	}	  
    }

  if (within_scope)
    {
      /* We need to catch errors here, because if evaluate
         expression fails we want to just return NULL.  */
      gdb_evaluate_expression (var->root->exp, &new_val);
      return new_val;
    }

  return NULL;
}

static struct value *
c_value_of_child (struct varobj *parent, int index)
{
  struct value *value = NULL;
  c_describe_child (parent, index, NULL, &value, NULL, NULL);

  return value;
}

static struct type *
c_type_of_child (struct varobj *parent, int index)
{
  struct type *type = NULL;
  c_describe_child (parent, index, NULL, NULL, &type, NULL);
  return type;
}

static int
c_variable_editable (struct varobj *var)
{
  switch (TYPE_CODE (get_value_type (var)))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_METHOD:
      return 0;
      break;

    default:
      return 1;
      break;
    }
}

static char *
c_value_of_variable (struct varobj *var)
{
  /* BOGUS: if val_print sees a struct/class, or a reference to one,
     it will print out its children instead of "{...}".  So we need to
     catch that case explicitly.  */
  struct type *type = get_type (var);

  /* Strip top-level references. */
  while (TYPE_CODE (type) == TYPE_CODE_REF)
    type = check_typedef (TYPE_TARGET_TYPE (type));

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      return xstrdup ("{...}");
      /* break; */

    case TYPE_CODE_ARRAY:
      {
	char *number;
	number = xstrprintf ("[%d]", var->num_children);
	return (number);
      }
      /* break; */

    default:
      {
	if (var->value == NULL)
	  {
	    /* This can happen if we attempt to get the value of a struct
	       member when the parent is an invalid pointer. This is an
	       error condition, so we should tell the caller. */
	    return NULL;
	  }
	else
	  {
	    if (var->not_fetched && value_lazy (var->value))
	      /* Frozen variable and no value yet.  We don't
		 implicitly fetch the value.  MI response will
		 use empty string for the value, which is OK.  */
	      return NULL;

	    gdb_assert (varobj_value_is_changeable_p (var));
	    gdb_assert (!value_lazy (var->value));
	    return value_get_print_value (var->value, var->format);
	  }
      }
    }
}


/* C++ */

static int
cplus_number_of_children (struct varobj *var)
{
  struct type *type;
  int children, dont_know;

  dont_know = 1;
  children = 0;

  if (!CPLUS_FAKE_CHILD (var))
    {
      type = get_value_type (var);
      adjust_value_for_child_access (NULL, &type, NULL);

      if (((TYPE_CODE (type)) == TYPE_CODE_STRUCT) ||
	  ((TYPE_CODE (type)) == TYPE_CODE_UNION))
	{
	  int kids[3];

	  cplus_class_num_children (type, kids);
	  if (kids[v_public] != 0)
	    children++;
	  if (kids[v_private] != 0)
	    children++;
	  if (kids[v_protected] != 0)
	    children++;

	  /* Add any baseclasses */
	  children += TYPE_N_BASECLASSES (type);
	  dont_know = 0;

	  /* FIXME: save children in var */
	}
    }
  else
    {
      int kids[3];

      type = get_value_type (var->parent);
      adjust_value_for_child_access (NULL, &type, NULL);

      cplus_class_num_children (type, kids);
      if (strcmp (var->name, "public") == 0)
	children = kids[v_public];
      else if (strcmp (var->name, "private") == 0)
	children = kids[v_private];
      else
	children = kids[v_protected];
      dont_know = 0;
    }

  if (dont_know)
    children = c_number_of_children (var);

  return children;
}

/* Compute # of public, private, and protected variables in this class.
   That means we need to descend into all baseclasses and find out
   how many are there, too. */
static void
cplus_class_num_children (struct type *type, int children[3])
{
  int i;

  children[v_public] = 0;
  children[v_private] = 0;
  children[v_protected] = 0;

  for (i = TYPE_N_BASECLASSES (type); i < TYPE_NFIELDS (type); i++)
    {
      /* If we have a virtual table pointer, omit it. */
      if (TYPE_VPTR_BASETYPE (type) == type && TYPE_VPTR_FIELDNO (type) == i)
	continue;

      if (TYPE_FIELD_PROTECTED (type, i))
	children[v_protected]++;
      else if (TYPE_FIELD_PRIVATE (type, i))
	children[v_private]++;
      else
	children[v_public]++;
    }
}

static char *
cplus_name_of_variable (struct varobj *parent)
{
  return c_name_of_variable (parent);
}

enum accessibility { private_field, protected_field, public_field };

/* Check if field INDEX of TYPE has the specified accessibility.
   Return 0 if so and 1 otherwise.  */
static int 
match_accessibility (struct type *type, int index, enum accessibility acc)
{
  if (acc == private_field && TYPE_FIELD_PRIVATE (type, index))
    return 1;
  else if (acc == protected_field && TYPE_FIELD_PROTECTED (type, index))
    return 1;
  else if (acc == public_field && !TYPE_FIELD_PRIVATE (type, index)
	   && !TYPE_FIELD_PROTECTED (type, index))
    return 1;
  else
    return 0;
}

static void
cplus_describe_child (struct varobj *parent, int index,
		      char **cname, struct value **cvalue, struct type **ctype,
		      char **cfull_expression)
{
  char *name = NULL;
  struct value *value;
  struct type *type;
  int was_ptr;
  char *parent_expression = NULL;

  if (cname)
    *cname = NULL;
  if (cvalue)
    *cvalue = NULL;
  if (ctype)
    *ctype = NULL;
  if (cfull_expression)
    *cfull_expression = NULL;

  if (CPLUS_FAKE_CHILD (parent))
    {
      value = parent->parent->value;
      type = get_value_type (parent->parent);
      if (cfull_expression)
	parent_expression = varobj_get_path_expr (parent->parent);
    }
  else
    {
      value = parent->value;
      type = get_value_type (parent);
      if (cfull_expression)
	parent_expression = varobj_get_path_expr (parent);
    }

  adjust_value_for_child_access (&value, &type, &was_ptr);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      || TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      char *join = was_ptr ? "->" : ".";
      if (CPLUS_FAKE_CHILD (parent))
	{
	  /* The fields of the class type are ordered as they
	     appear in the class.  We are given an index for a
	     particular access control type ("public","protected",
	     or "private").  We must skip over fields that don't
	     have the access control we are looking for to properly
	     find the indexed field. */
	  int type_index = TYPE_N_BASECLASSES (type);
	  enum accessibility acc = public_field;
	  if (strcmp (parent->name, "private") == 0)
	    acc = private_field;
	  else if (strcmp (parent->name, "protected") == 0)
	    acc = protected_field;

	  while (index >= 0)
	    {
	      if (TYPE_VPTR_BASETYPE (type) == type
		  && type_index == TYPE_VPTR_FIELDNO (type))
		; /* ignore vptr */
	      else if (match_accessibility (type, type_index, acc))
		    --index;
		  ++type_index;
	    }
	  --type_index;

	  if (cname)
	    *cname = xstrdup (TYPE_FIELD_NAME (type, type_index));

	  if (cvalue && value)
	    *cvalue = value_struct_element_index (value, type_index);

	  if (ctype)
	    *ctype = TYPE_FIELD_TYPE (type, type_index);

	  if (cfull_expression)
	    *cfull_expression = xstrprintf ("((%s)%s%s)", parent_expression,
					    join, 
					    TYPE_FIELD_NAME (type, type_index));
	}
      else if (index < TYPE_N_BASECLASSES (type))
	{
	  /* This is a baseclass.  */
	  if (cname)
	    *cname = xstrdup (TYPE_FIELD_NAME (type, index));

	  if (cvalue && value)
	    {
	      *cvalue = value_cast (TYPE_FIELD_TYPE (type, index), value);
	      release_value (*cvalue);
	    }

	  if (ctype)
	    {
	      *ctype = TYPE_FIELD_TYPE (type, index);
	    }

	  if (cfull_expression)
	    {
	      char *ptr = was_ptr ? "*" : "";
	      /* Cast the parent to the base' type. Note that in gdb,
		 expression like 
		         (Base1)d
		 will create an lvalue, for all appearences, so we don't
		 need to use more fancy:
		         *(Base1*)(&d)
		 construct.  */
	      *cfull_expression = xstrprintf ("(%s(%s%s) %s)", 
					      ptr, 
					      TYPE_FIELD_NAME (type, index),
					      ptr,
					      parent_expression);
	    }
	}
      else
	{
	  char *access = NULL;
	  int children[3];
	  cplus_class_num_children (type, children);

	  /* Everything beyond the baseclasses can
	     only be "public", "private", or "protected"

	     The special "fake" children are always output by varobj in
	     this order. So if INDEX == 2, it MUST be "protected". */
	  index -= TYPE_N_BASECLASSES (type);
	  switch (index)
	    {
	    case 0:
	      if (children[v_public] > 0)
	 	access = "public";
	      else if (children[v_private] > 0)
	 	access = "private";
	      else 
	 	access = "protected";
	      break;
	    case 1:
	      if (children[v_public] > 0)
		{
		  if (children[v_private] > 0)
		    access = "private";
		  else
		    access = "protected";
		}
	      else if (children[v_private] > 0)
	 	access = "protected";
	      break;
	    case 2:
	      /* Must be protected */
	      access = "protected";
	      break;
	    default:
	      /* error! */
	      break;
	    }

	  gdb_assert (access);
	  if (cname)
	    *cname = xstrdup (access);

	  /* Value and type and full expression are null here.  */
	}
    }
  else
    {
      c_describe_child (parent, index, cname, cvalue, ctype, cfull_expression);
    }  
}

static char *
cplus_name_of_child (struct varobj *parent, int index)
{
  char *name = NULL;
  cplus_describe_child (parent, index, &name, NULL, NULL, NULL);
  return name;
}

static char *
cplus_path_expr_of_child (struct varobj *child)
{
  cplus_describe_child (child->parent, child->index, NULL, NULL, NULL, 
			&child->path_expr);
  return child->path_expr;
}

static struct value *
cplus_value_of_root (struct varobj **var_handle)
{
  return c_value_of_root (var_handle);
}

static struct value *
cplus_value_of_child (struct varobj *parent, int index)
{
  struct value *value = NULL;
  cplus_describe_child (parent, index, NULL, &value, NULL, NULL);
  return value;
}

static struct type *
cplus_type_of_child (struct varobj *parent, int index)
{
  struct type *type = NULL;
  cplus_describe_child (parent, index, NULL, NULL, &type, NULL);
  return type;
}

static int
cplus_variable_editable (struct varobj *var)
{
  if (CPLUS_FAKE_CHILD (var))
    return 0;

  return c_variable_editable (var);
}

static char *
cplus_value_of_variable (struct varobj *var)
{

  /* If we have one of our special types, don't print out
     any value. */
  if (CPLUS_FAKE_CHILD (var))
    return xstrdup ("");

  return c_value_of_variable (var);
}

/* Java */

static int
java_number_of_children (struct varobj *var)
{
  return cplus_number_of_children (var);
}

static char *
java_name_of_variable (struct varobj *parent)
{
  char *p, *name;

  name = cplus_name_of_variable (parent);
  /* If  the name has "-" in it, it is because we
     needed to escape periods in the name... */
  p = name;

  while (*p != '\000')
    {
      if (*p == '-')
	*p = '.';
      p++;
    }

  return name;
}

static char *
java_name_of_child (struct varobj *parent, int index)
{
  char *name, *p;

  name = cplus_name_of_child (parent, index);
  /* Escape any periods in the name... */
  p = name;

  while (*p != '\000')
    {
      if (*p == '.')
	*p = '-';
      p++;
    }

  return name;
}

static char *
java_path_expr_of_child (struct varobj *child)
{
  return NULL;
}

static struct value *
java_value_of_root (struct varobj **var_handle)
{
  return cplus_value_of_root (var_handle);
}

static struct value *
java_value_of_child (struct varobj *parent, int index)
{
  return cplus_value_of_child (parent, index);
}

static struct type *
java_type_of_child (struct varobj *parent, int index)
{
  return cplus_type_of_child (parent, index);
}

static int
java_variable_editable (struct varobj *var)
{
  return cplus_variable_editable (var);
}

static char *
java_value_of_variable (struct varobj *var)
{
  return cplus_value_of_variable (var);
}

extern void _initialize_varobj (void);
void
_initialize_varobj (void)
{
  int sizeof_table = sizeof (struct vlist *) * VAROBJ_TABLE_SIZE;

  varobj_table = xmalloc (sizeof_table);
  memset (varobj_table, 0, sizeof_table);

  add_setshow_zinteger_cmd ("debugvarobj", class_maintenance,
			    &varobjdebug, _("\
Set varobj debugging."), _("\
Show varobj debugging."), _("\
When non-zero, varobj debugging is enabled."),
			    NULL,
			    show_varobjdebug,
			    &setlist, &showlist);
}

/* Invalidate the varobjs that are tied to locals and re-create the ones that
   are defined on globals.
   Invalidated varobjs will be always printed in_scope="invalid".  */
void 
varobj_invalidate (void)
{
  struct varobj **all_rootvarobj;
  struct varobj **varp;

  if (varobj_list (&all_rootvarobj) > 0)
  {
    varp = all_rootvarobj;
    while (*varp != NULL)
      {
        /* global var must be re-evaluated.  */     
        if ((*varp)->root->valid_block == NULL)
        {
          struct varobj *tmp_var;

          /* Try to create a varobj with same expression.  If we succeed replace
             the old varobj, otherwise invalidate it.  */
          tmp_var = varobj_create (NULL, (*varp)->name, (CORE_ADDR) 0, USE_CURRENT_FRAME);
          if (tmp_var != NULL) 
            { 
	      tmp_var->obj_name = xstrdup ((*varp)->obj_name);
              varobj_delete (*varp, NULL, 0);
              install_variable (tmp_var);
            }
          else
              (*varp)->root->is_valid = 0;
        }
        else /* locals must be invalidated.  */
          (*varp)->root->is_valid = 0;

        varp++;
      }
    xfree (all_rootvarobj);
  }
  return;
}
