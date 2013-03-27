/* MI Command Set - varobj commands.

   Copyright (C) 2000, 2002, 2004, 2005, 2007 Free Software Foundation, Inc.

   Contributed by Cygnus Solutions (a Red Hat company).

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
#include "mi-cmds.h"
#include "ui-out.h"
#include "mi-out.h"
#include "varobj.h"
#include "value.h"
#include <ctype.h>
#include "gdb_string.h"

const char mi_no_values[] = "--no-values";
const char mi_simple_values[] = "--simple-values";
const char mi_all_values[] = "--all-values";

extern int varobjdebug;		/* defined in varobj.c.  */

static void varobj_update_one (struct varobj *var,
			      enum print_values print_values,
			      int explicit);

static int mi_print_value_p (struct type *type, enum print_values print_values);

/* Print variable object VAR.  The PRINT_VALUES parameter controls
   if the value should be printed.  The PRINT_EXPRESSION parameter
   controls if the expression should be printed.  */
static void 
print_varobj (struct varobj *var, enum print_values print_values,
	      int print_expression)
{
  struct type *gdb_type;
  char *type;

  ui_out_field_string (uiout, "name", varobj_get_objname (var));
  if (print_expression)
    ui_out_field_string (uiout, "exp", varobj_get_expression (var));
  ui_out_field_int (uiout, "numchild", varobj_get_num_children (var));
  
  gdb_type = varobj_get_gdb_type (var);
  if (gdb_type && mi_print_value_p (gdb_type, print_values))
    ui_out_field_string (uiout, "value", varobj_get_value (var));

  type = varobj_get_type (var);
  if (type != NULL)
    {
      ui_out_field_string (uiout, "type", type);
      xfree (type);
    }

  if (varobj_get_frozen (var))
    ui_out_field_int (uiout, "frozen", 1);
}

/* VAROBJ operations */

enum mi_cmd_result
mi_cmd_var_create (char *command, char **argv, int argc)
{
  CORE_ADDR frameaddr = 0;
  struct varobj *var;
  char *name;
  char *frame;
  char *expr;
  struct cleanup *old_cleanups;
  enum varobj_type var_type;

  if (argc != 3)
    {
      /* mi_error_message = xstrprintf ("mi_cmd_var_create: Usage:
         ...."); return MI_CMD_ERROR; */
      error (_("mi_cmd_var_create: Usage: NAME FRAME EXPRESSION."));
    }

  name = xstrdup (argv[0]);
  /* Add cleanup for name. Must be free_current_contents as
     name can be reallocated */
  old_cleanups = make_cleanup (free_current_contents, &name);

  frame = xstrdup (argv[1]);
  make_cleanup (xfree, frame);

  expr = xstrdup (argv[2]);
  make_cleanup (xfree, expr);

  if (strcmp (name, "-") == 0)
    {
      xfree (name);
      name = varobj_gen_name ();
    }
  else if (!isalpha (*name))
    error (_("mi_cmd_var_create: name of object must begin with a letter"));

  if (strcmp (frame, "*") == 0)
    var_type = USE_CURRENT_FRAME;
  else if (strcmp (frame, "@") == 0)
    var_type = USE_SELECTED_FRAME;  
  else
    {
      var_type = USE_SPECIFIED_FRAME;
      frameaddr = string_to_core_addr (frame);
    }

  if (varobjdebug)
    fprintf_unfiltered (gdb_stdlog,
		    "Name=\"%s\", Frame=\"%s\" (0x%s), Expression=\"%s\"\n",
			name, frame, paddr (frameaddr), expr);

  var = varobj_create (name, expr, frameaddr, var_type);

  if (var == NULL)
    error (_("mi_cmd_var_create: unable to create variable object"));

  print_varobj (var, PRINT_ALL_VALUES, 0 /* don't print expression */);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_delete (char *command, char **argv, int argc)
{
  char *name;
  struct varobj *var;
  int numdel;
  int children_only_p = 0;
  struct cleanup *old_cleanups;

  if (argc < 1 || argc > 2)
    error (_("mi_cmd_var_delete: Usage: [-c] EXPRESSION."));

  name = xstrdup (argv[0]);
  /* Add cleanup for name. Must be free_current_contents as
     name can be reallocated */
  old_cleanups = make_cleanup (free_current_contents, &name);

  /* If we have one single argument it cannot be '-c' or any string
     starting with '-'. */
  if (argc == 1)
    {
      if (strcmp (name, "-c") == 0)
	error (_("mi_cmd_var_delete: Missing required argument after '-c': variable object name"));
      if (*name == '-')
	error (_("mi_cmd_var_delete: Illegal variable object name"));
    }

  /* If we have 2 arguments they must be '-c' followed by a string
     which would be the variable name. */
  if (argc == 2)
    {
      if (strcmp (name, "-c") != 0)
	error (_("mi_cmd_var_delete: Invalid option."));
      children_only_p = 1;
      do_cleanups (old_cleanups);
      name = xstrdup (argv[1]);
      make_cleanup (free_current_contents, &name);
    }

  /* If we didn't error out, now NAME contains the name of the
     variable. */

  var = varobj_get_handle (name);

  if (var == NULL)
    error (_("mi_cmd_var_delete: Variable object not found."));

  numdel = varobj_delete (var, NULL, children_only_p);

  ui_out_field_int (uiout, "ndeleted", numdel);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_set_format (char *command, char **argv, int argc)
{
  enum varobj_display_formats format;
  int len;
  struct varobj *var;
  char *formspec;

  if (argc != 2)
    error (_("mi_cmd_var_set_format: Usage: NAME FORMAT."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);

  if (var == NULL)
    error (_("mi_cmd_var_set_format: Variable object not found"));

  formspec = argv[1];
  if (formspec == NULL)
    error (_("mi_cmd_var_set_format: Must specify the format as: \"natural\", \"binary\", \"decimal\", \"hexadecimal\", or \"octal\""));

  len = strlen (formspec);

  if (strncmp (formspec, "natural", len) == 0)
    format = FORMAT_NATURAL;
  else if (strncmp (formspec, "binary", len) == 0)
    format = FORMAT_BINARY;
  else if (strncmp (formspec, "decimal", len) == 0)
    format = FORMAT_DECIMAL;
  else if (strncmp (formspec, "hexadecimal", len) == 0)
    format = FORMAT_HEXADECIMAL;
  else if (strncmp (formspec, "octal", len) == 0)
    format = FORMAT_OCTAL;
  else
    error (_("mi_cmd_var_set_format: Unknown display format: must be: \"natural\", \"binary\", \"decimal\", \"hexadecimal\", or \"octal\""));

  /* Set the format of VAR to given format */
  varobj_set_display_format (var, format);

  /* Report the new current format */
  ui_out_field_string (uiout, "format", varobj_format_string[(int) format]);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_set_frozen (char *command, char **argv, int argc)
{
  struct varobj *var;
  int frozen;

  if (argc != 2)
    error (_("-var-set-format: Usage: NAME FROZEN_FLAG."));

  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("Variable object not found"));

  if (strcmp (argv[1], "0") == 0)
    frozen = 0;
  else if (strcmp (argv[1], "1") == 0)
    frozen = 1;
  else
    error (_("Invalid flag value"));

  varobj_set_frozen (var, frozen);

  /* We don't automatically return the new value, or what varobjs got new
     values during unfreezing.  If this information is required, client
     should call -var-update explicitly.  */
  return MI_CMD_DONE;
}


enum mi_cmd_result
mi_cmd_var_show_format (char *command, char **argv, int argc)
{
  enum varobj_display_formats format;
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_show_format: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_show_format: Variable object not found"));

  format = varobj_get_display_format (var);

  /* Report the current format */
  ui_out_field_string (uiout, "format", varobj_format_string[(int) format]);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_num_children (char *command, char **argv, int argc)
{
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_info_num_children: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_info_num_children: Variable object not found"));

  ui_out_field_int (uiout, "numchild", varobj_get_num_children (var));
  return MI_CMD_DONE;
}

/* Parse a string argument into a print_values value.  */

static enum print_values
mi_parse_values_option (const char *arg)
{
  if (strcmp (arg, "0") == 0
      || strcmp (arg, mi_no_values) == 0)
    return PRINT_NO_VALUES;
  else if (strcmp (arg, "1") == 0
	   || strcmp (arg, mi_all_values) == 0)
    return PRINT_ALL_VALUES;
  else if (strcmp (arg, "2") == 0
	   || strcmp (arg, mi_simple_values) == 0)
    return PRINT_SIMPLE_VALUES;
  else
    error (_("Unknown value for PRINT_VALUES\n\
Must be: 0 or \"%s\", 1 or \"%s\", 2 or \"%s\""),
	   mi_no_values, mi_simple_values, mi_all_values);
}

/* Return 1 if given the argument PRINT_VALUES we should display
   a value of type TYPE.  */

static int
mi_print_value_p (struct type *type, enum print_values print_values)
{
  type = check_typedef (type);

  if (print_values == PRINT_NO_VALUES)
    return 0;

  if (print_values == PRINT_ALL_VALUES)
    return 1;

  /* For PRINT_SIMPLE_VALUES, only print the value if it has a type
     and that type is not a compound type.  */

  return (TYPE_CODE (type) != TYPE_CODE_ARRAY
	  && TYPE_CODE (type) != TYPE_CODE_STRUCT
	  && TYPE_CODE (type) != TYPE_CODE_UNION);
}

enum mi_cmd_result
mi_cmd_var_list_children (char *command, char **argv, int argc)
{
  struct varobj *var;
  struct varobj **childlist;
  struct varobj **cc;
  struct cleanup *cleanup_children;
  int numchild;
  enum print_values print_values;

  if (argc != 1 && argc != 2)
    error (_("mi_cmd_var_list_children: Usage: [PRINT_VALUES] NAME"));

  /* Get varobj handle, if a valid var obj name was specified */
  if (argc == 1)
    var = varobj_get_handle (argv[0]);
  else
    var = varobj_get_handle (argv[1]);
  if (var == NULL)
    error (_("Variable object not found"));

  numchild = varobj_list_children (var, &childlist);
  ui_out_field_int (uiout, "numchild", numchild);
  if (argc == 2)
    print_values = mi_parse_values_option (argv[0]);
  else
    print_values = PRINT_NO_VALUES;

  if (numchild <= 0)
    {
      xfree (childlist);
      return MI_CMD_DONE;
    }

  if (mi_version (uiout) == 1)
    cleanup_children = make_cleanup_ui_out_tuple_begin_end (uiout, "children");
  else
    cleanup_children = make_cleanup_ui_out_list_begin_end (uiout, "children");
  cc = childlist;
  while (*cc != NULL)
    {
      struct cleanup *cleanup_child;
      cleanup_child = make_cleanup_ui_out_tuple_begin_end (uiout, "child");
      print_varobj (*cc, print_values, 1 /* print expression */);
      cc++;
      do_cleanups (cleanup_child);
    }
  do_cleanups (cleanup_children);
  xfree (childlist);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_type (char *command, char **argv, int argc)
{
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_info_type: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_info_type: Variable object not found"));

  ui_out_field_string (uiout, "type", varobj_get_type (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_path_expression (char *command, char **argv, int argc)
{
  struct varobj *var;
  char *path_expr;

  if (argc != 1)
    error (_("Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified.  */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("Variable object not found"));
  
  path_expr = varobj_get_path_expr (var);

  ui_out_field_string (uiout, "path_expr", path_expr);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_info_expression (char *command, char **argv, int argc)
{
  enum varobj_languages lang;
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_info_expression: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_info_expression: Variable object not found"));

  lang = varobj_get_language (var);

  ui_out_field_string (uiout, "lang", varobj_language_string[(int) lang]);
  ui_out_field_string (uiout, "exp", varobj_get_expression (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_show_attributes (char *command, char **argv, int argc)
{
  int attr;
  char *attstr;
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_show_attributes: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_show_attributes: Variable object not found"));

  attr = varobj_get_attributes (var);
  /* FIXME: define masks for attributes */
  if (attr & 0x00000001)
    attstr = "editable";
  else
    attstr = "noneditable";

  ui_out_field_string (uiout, "attr", attstr);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_evaluate_expression (char *command, char **argv, int argc)
{
  struct varobj *var;

  if (argc != 1)
    error (_("mi_cmd_var_evaluate_expression: Usage: NAME."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_evaluate_expression: Variable object not found"));

  ui_out_field_string (uiout, "value", varobj_get_value (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_assign (char *command, char **argv, int argc)
{
  struct varobj *var;
  char *expression;

  if (argc != 2)
    error (_("mi_cmd_var_assign: Usage: NAME EXPRESSION."));

  /* Get varobj handle, if a valid var obj name was specified */
  var = varobj_get_handle (argv[0]);
  if (var == NULL)
    error (_("mi_cmd_var_assign: Variable object not found"));

  /* FIXME: define masks for attributes */
  if (!(varobj_get_attributes (var) & 0x00000001))
    error (_("mi_cmd_var_assign: Variable object is not editable"));

  expression = xstrdup (argv[1]);

  if (!varobj_set_value (var, expression))
    error (_("mi_cmd_var_assign: Could not assign expression to variable object"));

  ui_out_field_string (uiout, "value", varobj_get_value (var));
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_var_update (char *command, char **argv, int argc)
{
  struct varobj *var;
  struct varobj **rootlist;
  struct varobj **cr;
  struct cleanup *cleanup;
  char *name;
  int nv;
  enum print_values print_values;

  if (argc != 1 && argc != 2)
    error (_("mi_cmd_var_update: Usage: [PRINT_VALUES] NAME."));

  if (argc == 1)
    name = argv[0];
  else
    name = (argv[1]);

  if (argc == 2)
    print_values = mi_parse_values_option (argv[0]);
  else
    print_values = PRINT_NO_VALUES;

  /* Check if the parameter is a "*" which means that we want
     to update all variables */

  if ((*name == '*') && (*(name + 1) == '\0'))
    {
      nv = varobj_list (&rootlist);
      cleanup = make_cleanup (xfree, rootlist);
      if (mi_version (uiout) <= 1)
        make_cleanup_ui_out_tuple_begin_end (uiout, "changelist");
      else
        make_cleanup_ui_out_list_begin_end (uiout, "changelist");
      if (nv <= 0)
	{
	  do_cleanups (cleanup);
	  return MI_CMD_DONE;
	}
      cr = rootlist;
      while (*cr != NULL)
	{
	  varobj_update_one (*cr, print_values, 0 /* implicit */);
	  cr++;
	}
      do_cleanups (cleanup);
    }
  else
    {
      /* Get varobj handle, if a valid var obj name was specified */
      var = varobj_get_handle (name);
      if (var == NULL)
	error (_("mi_cmd_var_update: Variable object not found"));

      if (mi_version (uiout) <= 1)
        cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "changelist");
      else
        cleanup = make_cleanup_ui_out_list_begin_end (uiout, "changelist");
      varobj_update_one (var, print_values, 1 /* explicit */);
      do_cleanups (cleanup);
    }
    return MI_CMD_DONE;
}

/* Helper for mi_cmd_var_update().  */

static void
varobj_update_one (struct varobj *var, enum print_values print_values,
		   int explicit)
{
  struct varobj **changelist;
  struct varobj **cc;
  struct cleanup *cleanup = NULL;
  int nc;

  nc = varobj_update (&var, &changelist, explicit);

  /* nc >= 0  represents the number of changes reported into changelist.
     nc < 0   means that an error occured or the the variable has 
              changed type (TYPE_CHANGED).  */
  
  if (nc == 0)
    return;
  else if (nc < 0)
    {
      if (mi_version (uiout) > 1)
        cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_field_string (uiout, "name", varobj_get_objname(var));

      switch (nc)
      {
        case NOT_IN_SCOPE:
          ui_out_field_string (uiout, "in_scope", "false");
	  break;
        case INVALID:
          ui_out_field_string (uiout, "in_scope", "invalid");
 	  break;
        case TYPE_CHANGED:
	  ui_out_field_string (uiout, "in_scope", "true");
          ui_out_field_string (uiout, "new_type", varobj_get_type(var));
          ui_out_field_int (uiout, "new_num_children", 
			    varobj_get_num_children(var));
	  break;
      }
      if (mi_version (uiout) > 1)
        do_cleanups (cleanup);
    }
  else
    {
      cc = changelist;
      while (*cc != NULL)
	{
	  if (mi_version (uiout) > 1)
	    cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	  ui_out_field_string (uiout, "name", varobj_get_objname (*cc));
	  if (mi_print_value_p (varobj_get_gdb_type (*cc), print_values))
	    ui_out_field_string (uiout, "value", varobj_get_value (*cc));
	  ui_out_field_string (uiout, "in_scope", "true");
	  ui_out_field_string (uiout, "type_changed", "false");
	  if (mi_version (uiout) > 1)
	    do_cleanups (cleanup);
	  cc++;
	}
      xfree (changelist);
    }
}
