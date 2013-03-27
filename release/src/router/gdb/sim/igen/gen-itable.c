/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

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



#include "misc.h"
#include "lf.h"
#include "table.h"
#include "filter.h"
#include "igen.h"

#include "ld-insn.h"
#include "ld-decode.h"

#include "gen.h"

#include "gen-itable.h"

#ifndef NULL
#define NULL 0
#endif



typedef struct _itable_info
{
  int sizeof_form;
  int sizeof_name;
  int sizeof_file;
}
itable_info;


static void
itable_h_insn (lf *file,
	       insn_table *entry, insn_entry * instruction, void *data)
{
  int len;
  itable_info *info = data;
  lf_print__line_ref (file, instruction->line);
  lf_printf (file, "  ");
  print_function_name (file,
		       instruction->name,
		       instruction->format_name,
		       NULL, NULL, function_name_prefix_itable);
  lf_printf (file, ",\n");
  /* update summary info */
  len = strlen (instruction->format_name);
  if (info->sizeof_form <= len)
    info->sizeof_form = len + 1;
  len = strlen (instruction->name);
  if (info->sizeof_name <= len)
    info->sizeof_name = len + 1;
  len = strlen (filter_filename (instruction->line->file_name));
  if (info->sizeof_file <= len)
    info->sizeof_file = len + 1;
}


/* print the list of all the different options */

static void
itable_print_enum (lf *file, filter *set, char *name)
{
  char *elem;
  lf_printf (file, "typedef enum {\n");
  lf_indent (file, +2);
  for (elem = filter_next (set, "");
       elem != NULL; elem = filter_next (set, elem))
    {
      lf_printf (file, "%sitable_%s_%s,\n",
		 options.module.itable.prefix.l, name, elem);
      if (strlen (options.module.itable.prefix.l) > 0)
	{
	  lf_indent_suppress (file);
	  lf_printf (file, "#define itable_%s_%s %sitable_%s_%s\n",
		     name, elem, options.module.itable.prefix.l, name, elem);
	}
    }
  lf_printf (file, "nr_%sitable_%ss,\n", options.module.itable.prefix.l,
	     name);

  lf_indent (file, -2);
  lf_printf (file, "} %sitable_%ss;\n", options.module.itable.prefix.l, name);
  if (strlen (options.module.itable.prefix.l) > 0)
    {
      lf_indent_suppress (file);
      lf_printf (file, "#define itable_%ss %sitable_%ss\n",
		 name, options.module.itable.prefix.l, name);
      lf_indent_suppress (file);
      lf_printf (file, "#define nr_itable_%ss nr_%sitable_%ss\n",
		 name, options.module.itable.prefix.l, name);
    }
}

/* print an array of the option names as strings */

static void
itable_print_names (lf *file, filter *set, char *name)
{
  char *elem;
  lf_printf (file, "const char *%sitable_%s_names[nr_%sitable_%ss + 1] = {\n",
	     options.module.itable.prefix.l, name,
	     options.module.itable.prefix.l, name);
  lf_indent (file, +2);
  for (elem = filter_next (set, "");
       elem != NULL; elem = filter_next (set, elem))
    {
      lf_printf (file, "\"%s\",\n", elem);
    }
  lf_printf (file, "0,\n");
  lf_indent (file, -2);
  lf_printf (file, "};\n");
}

extern void
gen_itable_h (lf *file, insn_table *isa)
{
  itable_info *info = ZALLOC (itable_info);

  /* output an enumerated type for each instruction */
  lf_printf (file, "typedef enum {\n");
  insn_table_traverse_insn (file, isa, itable_h_insn, info);
  lf_printf (file, "  nr_%sitable_entries,\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "} %sitable_index;\n", options.module.itable.prefix.l);
  lf_printf (file, "\n");

  /* output an enumeration type for each flag */
  itable_print_enum (file, isa->flags, "flag");
  lf_printf (file, "extern const char *%sitable_flag_names[];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "\n");

  /* output an enumeration of all the possible options */
  itable_print_enum (file, isa->options, "option");
  lf_printf (file, "extern const char *%sitable_option_names[];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "\n");

  /* output an enumeration of all the processor models */
  itable_print_enum (file, isa->model->processors, "processor");
  lf_printf (file, "extern const char *%sitable_processor_names[];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "\n");

  /* output the table that contains the actual instruction info */
  lf_printf (file, "typedef struct _%sitable_instruction_info {\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "  %sitable_index nr;\n", options.module.itable.prefix.l);
  lf_printf (file, "  char *format;\n");
  lf_printf (file, "  char *form;\n");
  lf_printf (file, "  char *flags;\n");

  /* nr_itable_* may be zero, so we add 1 to avoid an
     illegal zero-sized array. */
  lf_printf (file, "  char flag[nr_%sitable_flags + 1];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "  char *options;\n");
  lf_printf (file, "  char option[nr_%sitable_options + 1];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "  char *processors;\n");
  lf_printf (file, "  char processor[nr_%sitable_processors + 1];\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "  char *name;\n");
  lf_printf (file, "  char *file;\n");
  lf_printf (file, "  int line_nr;\n");
  lf_printf (file, "} %sitable_info;\n", options.module.itable.prefix.l);
  lf_printf (file, "\n");
  lf_printf (file, "extern %sitable_info %sitable[nr_%sitable_entries];\n",
	     options.module.itable.prefix.l, options.module.itable.prefix.l,
	     options.module.itable.prefix.l);
  if (strlen (options.module.itable.prefix.l) > 0)
    {
      lf_indent_suppress (file);
      lf_printf (file, "#define itable %sitable\n",
		 options.module.itable.prefix.l);
    }
  lf_printf (file, "\n");

  /* output an enum defining the max size of various itable members */
  lf_printf (file, "enum {\n");
  lf_printf (file, "  sizeof_%sitable_form = %d,\n",
	     options.module.itable.prefix.l, info->sizeof_form);
  lf_printf (file, "  sizeof_%sitable_name = %d,\n",
	     options.module.itable.prefix.l, info->sizeof_name);
  lf_printf (file, "  sizeof_%sitable_file = %d,\n",
	     options.module.itable.prefix.l, info->sizeof_file);
  lf_printf (file, "};\n");
}


/****************************************************************/

static void
itable_print_set (lf *file, filter *set, filter *members)
{
  char *elem;
  lf_printf (file, "\"");
  elem = filter_next (members, "");
  if (elem != NULL)
    {
      while (1)
	{
	  lf_printf (file, "%s", elem);
	  elem = filter_next (members, elem);
	  if (elem == NULL)
	    break;
	  lf_printf (file, ",");
	}
    }
  lf_printf (file, "\",\n");

  lf_printf (file, "{");
  for (elem = filter_next (set, "");
       elem != NULL; elem = filter_next (set, elem))
    {
      if (filter_is_member (members, elem))
	{
	  lf_printf (file, " 1,");
	}
      else
	{
	  lf_printf (file, " 0,");
	}

    }
  /* always print a dummy element, to avoid empty initializers. */
  lf_printf (file, " 99 },\n");
}


static void
itable_c_insn (lf *file,
	       insn_table *isa, insn_entry * instruction, void *data)
{
  lf_printf (file, "{ ");
  lf_indent (file, +2);
  print_function_name (file,
		       instruction->name,
		       instruction->format_name,
		       NULL, NULL, function_name_prefix_itable);
  lf_printf (file, ",\n");
  lf_printf (file, "\"");
  print_insn_words (file, instruction);
  lf_printf (file, "\",\n");
  lf_printf (file, "\"%s\",\n", instruction->format_name);

  itable_print_set (file, isa->flags, instruction->flags);
  itable_print_set (file, isa->options, instruction->options);
  itable_print_set (file, isa->model->processors, instruction->processors);

  lf_printf (file, "\"%s\",\n", instruction->name);
  lf_printf (file, "\"%s\",\n",
	     filter_filename (instruction->line->file_name));
  lf_printf (file, "%d,\n", instruction->line->line_nr);
  lf_printf (file, "},\n");
  lf_indent (file, -2);
}


extern void
gen_itable_c (lf *file, insn_table *isa)
{
  /* leader */
  lf_printf (file, "#include \"%sitable.h\"\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "\n");

  /* FIXME - output model data??? */
  /* FIXME - output assembler data??? */

  /* output the flag, option and processor name tables */
  itable_print_names (file, isa->flags, "flag");
  itable_print_names (file, isa->options, "option");
  itable_print_names (file, isa->model->processors, "processor");

  /* output the table that contains the actual instruction info */
  lf_printf (file, "%sitable_info %sitable[nr_%sitable_entries] = {\n",
	     options.module.itable.prefix.l,
	     options.module.itable.prefix.l, options.module.itable.prefix.l);
  insn_table_traverse_insn (file, isa, itable_c_insn, NULL);

  lf_printf (file, "};\n");
}
