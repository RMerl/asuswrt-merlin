/* MI Command Set - breakpoint and watchpoint commands.
   Copyright (C) 2000, 2001, 2002, 2007 Free Software Foundation, Inc.
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
#include "mi-getopt.h"
#include "ui-out.h"
#include "symtab.h"
#include "source.h"
#include "objfiles.h"

/* Return to the client the absolute path and line number of the 
   current file being executed. */

enum mi_cmd_result
mi_cmd_file_list_exec_source_file(char *command, char **argv, int argc)
{
  struct symtab_and_line st;
  int optind = 0;
  char *optarg;
  
  if ( !mi_valid_noargs("mi_cmd_file_list_exec_source_file", argc, argv) )
    error (_("mi_cmd_file_list_exec_source_file: Usage: No args"));

  /* Set the default file and line, also get them */
  set_default_source_symtab_and_line();
  st = get_current_source_symtab_and_line();

  /* We should always get a symtab. 
     Apparently, filename does not need to be tested for NULL.
     The documentation in symtab.h suggests it will always be correct */
  if (!st.symtab)
    error (_("mi_cmd_file_list_exec_source_file: No symtab"));

  /* Extract the fullname if it is not known yet */
  symtab_to_fullname (st.symtab);

  /* Print to the user the line, filename and fullname */
  ui_out_field_int (uiout, "line", st.line);
  ui_out_field_string (uiout, "file", st.symtab->filename);

  /* We may not be able to open the file (not available). */
  if (st.symtab->fullname)
  ui_out_field_string (uiout, "fullname", st.symtab->fullname);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_file_list_exec_source_files (char *command, char **argv, int argc)
{
  struct symtab *s;
  struct partial_symtab *ps;
  struct objfile *objfile;

  if (!mi_valid_noargs ("mi_cmd_file_list_exec_source_files", argc, argv))
    error (_("mi_cmd_file_list_exec_source_files: Usage: No args"));

  /* Print the table header */
  ui_out_begin (uiout, ui_out_type_list, "files");

  /* Look at all of the symtabs */
  ALL_SYMTABS (objfile, s)
  {
    ui_out_begin (uiout, ui_out_type_tuple, NULL);

    ui_out_field_string (uiout, "file", s->filename);

    /* Extract the fullname if it is not known yet */
    symtab_to_fullname (s);

    if (s->fullname)
      ui_out_field_string (uiout, "fullname", s->fullname);

    ui_out_end (uiout, ui_out_type_tuple);
  }

  /* Look at all of the psymtabs */
  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin)
      {
	ui_out_begin (uiout, ui_out_type_tuple, NULL);

	ui_out_field_string (uiout, "file", ps->filename);

	/* Extract the fullname if it is not known yet */
	psymtab_to_fullname (ps);

	if (ps->fullname)
	  ui_out_field_string (uiout, "fullname", ps->fullname);

	ui_out_end (uiout, ui_out_type_tuple);
      }
  }

  ui_out_end (uiout, ui_out_type_list);

  return MI_CMD_DONE;
}
