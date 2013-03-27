/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */



#include "misc.h"
#include "lf.h"
#include "table.h"

#include "filter.h"

#include "ld-cache.h"
#include "ld-decode.h"
#include "ld-insn.h"

#include "igen.h"
#include "gen-itable.h"

#ifndef NULL
#define NULL 0
#endif



static void
itable_h_insn(insn_table *entry,
	      lf *file,
	      void *data,
	      insn *instruction,
	      int depth)
{
  lf_printf(file, "  ");
  print_function_name(file,
		      instruction->file_entry->fields[insn_name],
		      NULL,
		      function_name_prefix_itable);
  lf_printf(file, ",\n");
}


extern void 
gen_itable_h(insn_table *table, lf *file)
{
  /* output an enumerated type for each instruction */
  lf_printf(file, "typedef enum {\n");
  insn_table_traverse_insn(table,
			   file, NULL,
			   itable_h_insn);
  lf_printf(file, "  nr_itable_entries,\n");
  lf_printf(file, "} itable_index;\n");
  lf_printf(file, "\n");

  /* output the table that contains the actual instruction info */
  lf_printf(file, "typedef struct _itable_instruction_info {\n");
  lf_printf(file, "  itable_index nr;\n");
  lf_printf(file, "  char *format;\n");
  lf_printf(file, "  char *form;\n");
  lf_printf(file, "  char *flags;\n");
  lf_printf(file, "  char *mnemonic;\n");
  lf_printf(file, "  char *name;\n");
  lf_printf(file, "  char *file;\n");
  lf_printf(file, "  int line_nr;\n");
  lf_printf(file, "} itable_info;\n");
  lf_printf(file, "\n");
  lf_printf(file, "extern itable_info itable[nr_itable_entries];\n");
}

/****************************************************************/

static void
itable_c_insn(insn_table *entry,
	      lf *file,
	      void *data,
	      insn *instruction,
	      int depth)
{
  char **fields = instruction->file_entry->fields;
  lf_printf(file, "  { ");
  print_function_name(file,
		      instruction->file_entry->fields[insn_name],
		      NULL,
		      function_name_prefix_itable);
  lf_printf(file, ",\n");
  lf_printf(file, "    \"%s\",\n", fields[insn_format]);
  lf_printf(file, "    \"%s\",\n", fields[insn_form]);
  lf_printf(file, "    \"%s\",\n", fields[insn_flags]);
  lf_printf(file, "    \"%s\",\n", fields[insn_mnemonic]);
  lf_printf(file, "    \"%s\",\n", fields[insn_name]);
  lf_printf(file, "    \"%s\",\n", filter_filename (instruction->file_entry->file_name));
  lf_printf(file, "    %d,\n", instruction->file_entry->line_nr);
  lf_printf(file, "    },\n");
}


extern void 
gen_itable_c(insn_table *table, lf *file)
{
  /* output the table that contains the actual instruction info */
  lf_printf(file, "#include \"itable.h\"\n");
  lf_printf(file, "\n");
  lf_printf(file, "itable_info itable[nr_itable_entries] = {\n");
  insn_table_traverse_insn(table,
			   file, NULL,
			   itable_c_insn);
  lf_printf(file, "};\n");
}
