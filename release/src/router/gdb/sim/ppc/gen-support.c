/*  This file is part of the program psim.

    Copyright 1994, 1995, 2003 Andrew Cagney

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

#include "ld-decode.h"
#include "ld-cache.h"
#include "ld-insn.h"

#include "igen.h"

#include "gen-semantics.h"
#include "gen-support.h"

static void
print_support_function_name(lf *file,
			    table_entry *function,
			    int is_function_definition)
{
  if (it_is("internal", function->fields[insn_flags])) {
    lf_print_function_type(file, SEMANTIC_FUNCTION_TYPE, "PSIM_INLINE_SUPPORT",
			   (is_function_definition ? "\n" : " "));
    print_function_name(file,
			function->fields[function_name],
			NULL,
			function_name_prefix_semantics);
    lf_printf(file, "\n(%s)", SEMANTIC_FUNCTION_FORMAL);
    if (!is_function_definition)
      lf_printf(file, ";");
    lf_printf(file, "\n");
  }
  else {
    lf_print_function_type(file,
			   function->fields[function_type],
			   "PSIM_INLINE_SUPPORT",
			   (is_function_definition ? "\n" : " "));
    lf_printf(file, "%s\n(%s)%s",
	      function->fields[function_name],
	      function->fields[function_param],
	      (is_function_definition ? "\n" : ";\n"));
  }
}


static void
support_h_function(insn_table *entry,
		   lf *file,
		   void *data,
		   table_entry *function)
{
  ASSERT(function->fields[function_type] != NULL);
  ASSERT(function->fields[function_param] != NULL);
  print_support_function_name(file,
			      function,
			      0/*!is_definition*/);
  lf_printf(file, "\n");
}


extern void
gen_support_h(insn_table *table,
	      lf *file)
{
  /* output a declaration for all functions */
  insn_table_traverse_function(table,
			       file, NULL,
			       support_h_function);
  lf_printf(file, "\n");
  lf_printf(file, "#if (SUPPORT_INLINE & INCLUDE_MODULE)\n");
  lf_printf(file, "# include \"support.c\"\n");
  lf_printf(file, "#endif\n");
}

static void
support_c_function(insn_table *table,
		   lf *file,
		   void *data,
		   table_entry *function)
{
  ASSERT(function->fields[function_type] != NULL);
  print_support_function_name(file,
			      function,
			      1/*!is_definition*/);
  table_entry_print_cpp_line_nr(file, function);
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  lf_print__c_code(file, function->annex);
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "error(\"Internal function must longjump\\n\");\n");
    lf_printf(file, "return 0;\n");
  }
  lf_indent(file, -2);
  lf_printf(file, "}\n");
  lf_print__internal_reference(file);
  lf_printf(file, "\n");
}


void
gen_support_c(insn_table *table,
	      lf *file)
{
  lf_printf(file, "#include \"cpu.h\"\n");
  lf_printf(file, "#include \"idecode.h\"\n");
  lf_printf(file, "#ifdef HAVE_COMMON_FPU\n");
  lf_printf(file, "#include \"sim-inline.h\"\n");
  lf_printf(file, "#include \"sim-fpu.h\"\n");
  lf_printf(file, "#endif\n");
  lf_printf(file, "#include \"support.h\"\n");
  lf_printf(file, "\n");

  /* output a definition (c-code) for all functions */
  insn_table_traverse_function(table,
			       file, NULL,
			       support_c_function);
}
