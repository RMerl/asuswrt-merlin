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

#include "gen-semantics.h"
#include "gen-support.h"

static void
print_support_function_name (lf *file,
			     function_entry * function,
			     int is_function_definition)
{
  if (function->is_internal)
    {
      lf_print__function_type_function (file, print_semantic_function_type,
					"INLINE_SUPPORT",
					(is_function_definition ? "\n" :
					 " "));
      print_function_name (file, function->name, NULL, NULL, NULL,
			   function_name_prefix_semantics);
      lf_printf (file, "\n(");
      lf_indent (file, +1);
      print_semantic_function_formal (file, 0);
      lf_indent (file, -1);
      lf_printf (file, ")");
      if (!is_function_definition)
	lf_printf (file, ";");
      lf_printf (file, "\n");
    }
  else
    {
      /* map the name onto a globally valid name */
      if (!is_function_definition
	  && strcmp (options.module.support.prefix.l, "") != 0)
	{
	  lf_indent_suppress (file);
	  lf_printf (file, "#define %s %s%s\n",
		     function->name,
		     options.module.support.prefix.l, function->name);
	}
      lf_print__function_type (file,
			       function->type,
			       "INLINE_SUPPORT",
			       (is_function_definition ? "\n" : " "));
      lf_printf (file, "%s%s\n(",
		 options.module.support.prefix.l, function->name);
      if (options.gen.smp)
	lf_printf (file,
		   "sim_cpu *cpu, %sinstruction_address cia, int MY_INDEX",
		   options.module.support.prefix.l);
      else
	lf_printf (file,
		   "SIM_DESC sd, %sinstruction_address cia, int MY_INDEX",
		   options.module.support.prefix.l);
      if (function->param != NULL && strlen (function->param) > 0)
	lf_printf (file, ", %s", function->param);
      lf_printf (file, ")%s", (is_function_definition ? "\n" : ";\n"));
    }
}


static void
support_h_function (lf *file, function_entry * function, void *data)
{
  ASSERT (function->type != NULL);
  print_support_function_name (file, function, 0 /*!is_definition */ );
  lf_printf (file, "\n");
}


extern void
gen_support_h (lf *file, insn_table *table)
{
  /* output the definition of `SD_' */
  if (options.gen.smp)
    {
      lf_printf (file, "#define SD CPU_STATE (cpu)\n");
      lf_printf (file, "#define CPU cpu\n");
      lf_printf (file, "#define CPU_ cpu\n");
    }
  else
    {
      lf_printf (file, "#define SD sd\n");
      lf_printf (file, "#define CPU (STATE_CPU (sd, 0))\n");
      lf_printf (file, "#define CPU_ sd\n");
    }

  lf_printf (file, "#define CIA_ cia\n");
  if (options.gen.delayed_branch)
    {
      lf_printf (file, "#define CIA cia.ip\n");
      lf_printf (file,
		 "/* #define NIA nia.dp -- do not define, ambigious */\n");
    }
  else
    {
      lf_printf (file, "#define CIA cia\n");
      lf_printf (file, "#define NIA nia\n");
    }
  lf_printf (file, "\n");

  lf_printf (file, "#define SD_ CPU_, CIA_, MY_INDEX\n");
  lf_printf (file, "#define _SD SD_ /* deprecated */\n");
  lf_printf (file, "\n");

  /* Map <PREFIX>_xxxx onto the shorter xxxx for the following names:

     instruction_word
     idecode_issue
     semantic_illegal

     Map defined here as name space problems are created when the name is
     defined in idecode.h  */
  if (strcmp (options.module.idecode.prefix.l, "") != 0)
    {
      lf_indent_suppress (file);
      lf_printf (file, "#define %s %s%s\n",
		 "instruction_word",
		 options.module.idecode.prefix.l, "instruction_word");
      lf_printf (file, "\n");
      lf_indent_suppress (file);
      lf_printf (file, "#define %s %s%s\n",
		 "idecode_issue",
		 options.module.idecode.prefix.l, "idecode_issue");
      lf_printf (file, "\n");
      lf_indent_suppress (file);
      lf_printf (file, "#define %s %s%s\n",
		 "semantic_illegal",
		 options.module.idecode.prefix.l, "semantic_illegal");
      lf_printf (file, "\n");
    }

  /* output a declaration for all functions */
  function_entry_traverse (file, table->functions, support_h_function, NULL);
  lf_printf (file, "\n");
  lf_printf (file, "#if defined(SUPPORT_INLINE)\n");
  lf_printf (file, "# if ((SUPPORT_INLINE & INCLUDE_MODULE)\\\n");
  lf_printf (file, "      && (SUPPORT_INLINE & INCLUDED_BY_MODULE))\n");
  lf_printf (file, "#  include \"%ssupport.c\"\n",
	     options.module.support.prefix.l);
  lf_printf (file, "# endif\n");
  lf_printf (file, "#endif\n");
}

static void
support_c_function (lf *file, function_entry * function, void *data)
{
  ASSERT (function->type != NULL);
  print_support_function_name (file, function, 1 /*!is_definition */ );
  lf_printf (file, "{\n");
  lf_indent (file, +2);
  if (function->code == NULL)
    error (function->line, "Function without body (or null statement)");
  lf_print__line_ref (file, function->code->line);
  table_print_code (file, function->code);
  if (function->is_internal)
    {
      lf_printf (file,
		 "sim_engine_abort (SD, CPU, cia, \"Internal function must longjump\\n\");\n");
      lf_printf (file, "return cia;\n");
    }
  lf_indent (file, -2);
  lf_printf (file, "}\n");
  lf_print__internal_ref (file);
  lf_printf (file, "\n");
}


void
gen_support_c (lf *file, insn_table *table)
{
  lf_printf (file, "#include \"sim-main.h\"\n");
  lf_printf (file, "#include \"%sidecode.h\"\n",
	     options.module.idecode.prefix.l);
  lf_printf (file, "#include \"%sitable.h\"\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "#include \"%ssupport.h\"\n",
	     options.module.support.prefix.l);
  lf_printf (file, "\n");

  /* output a definition (c-code) for all functions */
  function_entry_traverse (file, table->functions, support_c_function, NULL);
}
