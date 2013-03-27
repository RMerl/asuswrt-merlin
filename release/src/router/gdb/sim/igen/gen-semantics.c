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
#include "gen-icache.h"
#include "gen-idecode.h"


static void
print_semantic_function_header (lf *file,
				const char *basename,
				const char *format_name,
				opcode_bits *expanded_bits,
				int is_function_definition,
				int nr_prefetched_words)
{
  int indent;
  lf_printf (file, "\n");
  lf_print__function_type_function (file, print_semantic_function_type,
				    "EXTERN_SEMANTICS",
				    (is_function_definition ? "\n" : " "));
  indent = print_function_name (file,
				basename,
				format_name,
				NULL,
				expanded_bits,
				function_name_prefix_semantics);
  if (is_function_definition)
    {
      indent += lf_printf (file, " ");
      lf_indent (file, +indent);
    }
  else
    {
      lf_printf (file, "\n");
    }
  lf_printf (file, "(");
  lf_indent (file, +1);
  print_semantic_function_formal (file, nr_prefetched_words);
  lf_indent (file, -1);
  lf_printf (file, ")");
  if (is_function_definition)
    {
      lf_indent (file, -indent);
    }
  else
    {
      lf_printf (file, ";");
    }
  lf_printf (file, "\n");
}

void
print_semantic_declaration (lf *file,
			    insn_entry * insn,
			    opcode_bits *expanded_bits,
			    insn_opcodes *opcodes, int nr_prefetched_words)
{
  print_semantic_function_header (file,
				  insn->name,
				  insn->format_name,
				  expanded_bits,
				  0 /* is not function definition */ ,
				  nr_prefetched_words);
}



/* generate the semantics.c file */


void
print_idecode_invalid (lf *file, const char *result, invalid_type type)
{
  const char *name;
  switch (type)
    {
    default:
      name = "unknown";
      break;
    case invalid_illegal:
      name = "illegal";
      break;
    case invalid_fp_unavailable:
      name = "fp_unavailable";
      break;
    case invalid_wrong_slot:
      name = "wrong_slot";
      break;
    }
  if (options.gen.code == generate_jumps)
    {
      lf_printf (file, "goto %s_%s;\n",
		 (options.gen.icache ? "icache" : "semantic"), name);
    }
  else if (options.gen.icache)
    {
      lf_printf (file, "%s %sicache_%s (", result,
		 options.module.global.prefix.l, name);
      print_icache_function_actual (file, 0);
      lf_printf (file, ");\n");
    }
  else
    {
      lf_printf (file, "%s %ssemantic_%s (", result,
		 options.module.global.prefix.l, name);
      print_semantic_function_actual (file, 0);
      lf_printf (file, ");\n");
    }
}


void
print_semantic_body (lf *file,
		     insn_entry * instruction,
		     opcode_bits *expanded_bits, insn_opcodes *opcodes)
{
  /* validate the instruction, if a cache this has already been done */
  if (!options.gen.icache)
    {
      print_idecode_validate (file, instruction, opcodes);
    }

  print_itrace (file, instruction, 0 /*put_value_in_cache */ );

  /* generate the instruction profile call - this is delayed until
     after the instruction has been verified.  The count macro
     generated is prefixed by ITABLE_PREFIX */
  {
    lf_printf (file, "\n");
    lf_indent_suppress (file);
    lf_printf (file, "#if defined (%sPROFILE_COUNT_INSN)\n",
	       options.module.itable.prefix.u);
    lf_printf (file, "%sPROFILE_COUNT_INSN (CPU, CIA, MY_INDEX);\n",
	       options.module.itable.prefix.u);
    lf_indent_suppress (file);
    lf_printf (file, "#endif\n");
  }

  /* generate the model call - this is delayed until after the
     instruction has been verified */
  {
    lf_printf (file, "\n");
    lf_indent_suppress (file);
    lf_printf (file, "#if defined (WITH_MON)\n");
    lf_printf (file, "/* monitoring: */\n");
    lf_printf (file, "if (WITH_MON & MONITOR_INSTRUCTION_ISSUE)\n");
    lf_printf (file, "  mon_issue (");
    print_function_name (file,
			 instruction->name,
			 instruction->format_name,
			 NULL, NULL, function_name_prefix_itable);
    lf_printf (file, ", cpu, cia);\n");
    lf_indent_suppress (file);
    lf_printf (file, "#endif\n");
    lf_printf (file, "\n");
  }

  /* determine the new instruction address */
  {
    lf_printf (file, "/* keep the next instruction address handy */\n");
    if (options.gen.nia == nia_is_invalid)
      {
	lf_printf (file, "nia = %sINVALID_INSTRUCTION_ADDRESS;\n",
		   options.module.global.prefix.u);
      }
    else
      {
	int nr_immeds = instruction->nr_words - 1;
	if (options.gen.delayed_branch)
	  {
	    if (nr_immeds > 0)
	      {
		lf_printf (file, "cia.dp += %d * %d; %s\n",
			   options.insn_bit_size / 8, nr_immeds,
			   "/* skip dp immeds */");
	      }
	    lf_printf (file, "nia.ip = cia.dp; %s\n",
		       "/* instruction pointer */");
	    lf_printf (file, "nia.dp = cia.dp + %d; %s\n",
		       options.insn_bit_size / 8,
		       "/* delayed-slot pointer */");
	  }
	else
	  {
	    if (nr_immeds > 0)
	      {
		lf_printf (file, "nia = cia + %d * (%d + 1); %s\n",
			   options.insn_bit_size / 8, nr_immeds,
			   "/* skip immeds as well */");

	      }
	    else
	      {
		lf_printf (file, "nia = cia + %d;\n",
			   options.insn_bit_size / 8);
	      }
	  }
      }
  }

  /* if conditional, generate code to verify that the instruction
     should be issued */
  if (filter_is_member (instruction->options, "c")
      || options.gen.conditional_issue)
    {
      lf_printf (file, "\n");
      lf_printf (file, "/* execute only if conditional passes */\n");
      lf_printf (file, "if (IS_CONDITION_OK)\n");
      lf_printf (file, "  {\n");
      lf_indent (file, +4);
      /* FIXME - need to log a conditional failure */
    }

  /* Architecture expects a REG to be zero.  Instead of having to
     check every read to see if it is refering to that REG just zap it
     at the start of every instruction */
  if (options.gen.zero_reg)
    {
      lf_printf (file, "\n");
      lf_printf (file, "/* Architecture expects REG to be zero */\n");
      lf_printf (file, "GPR_CLEAR(%d);\n", options.gen.zero_reg_nr);
    }

  /* generate the code (or at least something */
  lf_printf (file, "\n");
  lf_printf (file, "/* semantics: */\n");
  if (instruction->code != NULL)
    {
      /* true code */
      lf_printf (file, "{\n");
      lf_indent (file, +2);
      lf_print__line_ref (file, instruction->code->line);
      table_print_code (file, instruction->code);
      lf_indent (file, -2);
      lf_printf (file, "}\n");
      lf_print__internal_ref (file);
    }
  else if (filter_is_member (instruction->options, "nop"))
    {
      lf_print__internal_ref (file);
    }
  else
    {
      const char *prefix = "sim_engine_abort (";
      int indent = strlen (prefix);
      /* abort so it is implemented now */
      lf_print__line_ref (file, instruction->line);
      lf_printf (file, "%sSD, CPU, cia, \\\n", prefix);
      lf_indent (file, +indent);
      lf_printf (file, "\"%s:%d:0x%%08lx:%%s unimplemented\\n\", \\\n",
		 filter_filename (instruction->line->file_name),
		 instruction->line->line_nr);
      lf_printf (file, "(long) CIA, \\\n");
      lf_printf (file, "%sitable[MY_INDEX].name);\n",
		 options.module.itable.prefix.l);
      lf_indent (file, -indent);
      lf_print__internal_ref (file);
    }

  /* Close off the conditional execution */
  if (filter_is_member (instruction->options, "c")
      || options.gen.conditional_issue)
    {
      lf_indent (file, -4);
      lf_printf (file, "  }\n");
    }
}

static void
print_c_semantic (lf *file,
		  insn_entry * instruction,
		  opcode_bits *expanded_bits,
		  insn_opcodes *opcodes,
		  cache_entry *cache_rules, int nr_prefetched_words)
{

  lf_printf (file, "{\n");
  lf_indent (file, +2);

  print_my_defines (file,
		    instruction->name,
		    instruction->format_name, expanded_bits);
  lf_printf (file, "\n");
  print_icache_body (file,
		     instruction,
		     expanded_bits,
		     cache_rules,
		     (options.gen.direct_access
		      ? define_variables
		      : declare_variables),
		     (options.gen.icache
		      ? get_values_from_icache
		      : do_not_use_icache), nr_prefetched_words);

  lf_printf (file, "%sinstruction_address nia;\n",
	     options.module.global.prefix.l);
  print_semantic_body (file, instruction, expanded_bits, opcodes);
  lf_printf (file, "return nia;\n");

  /* generate something to clean up any #defines created for the cache */
  if (options.gen.direct_access)
    {
      print_icache_body (file,
			 instruction,
			 expanded_bits,
			 cache_rules,
			 undef_variables,
			 (options.gen.icache
			  ? get_values_from_icache
			  : do_not_use_icache), nr_prefetched_words);
    }

  lf_indent (file, -2);
  lf_printf (file, "}\n");
}

static void
print_c_semantic_function (lf *file,
			   insn_entry * instruction,
			   opcode_bits *expanded_bits,
			   insn_opcodes *opcodes,
			   cache_entry *cache_rules, int nr_prefetched_words)
{
  /* build the semantic routine to execute the instruction */
  print_semantic_function_header (file,
				  instruction->name,
				  instruction->format_name,
				  expanded_bits,
				  1 /*is-function-definition */ ,
				  nr_prefetched_words);
  print_c_semantic (file,
		    instruction,
		    expanded_bits, opcodes, cache_rules, nr_prefetched_words);
}

void
print_semantic_definition (lf *file,
			   insn_entry * insn,
			   opcode_bits *expanded_bits,
			   insn_opcodes *opcodes,
			   cache_entry *cache_rules, int nr_prefetched_words)
{
  print_c_semantic_function (file,
			     insn,
			     expanded_bits,
			     opcodes, cache_rules, nr_prefetched_words);
}
