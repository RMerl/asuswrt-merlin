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



#include <getopt.h>

#include "misc.h"
#include "lf.h"
#include "table.h"
#include "config.h"
#include "filter.h"

#include "igen.h"

#include "ld-insn.h"
#include "ld-decode.h"
#include "ld-cache.h"

#include "gen.h"

#include "gen-model.h"
#include "gen-icache.h"
#include "gen-itable.h"
#include "gen-idecode.h"
#include "gen-semantics.h"
#include "gen-engine.h"
#include "gen-support.h"
#include "gen-engine.h"


/****************************************************************/


/* Semantic functions */

int
print_semantic_function_formal (lf *file, int nr_prefetched_words)
{
  int nr = 0;
  int word_nr;
  if (options.gen.icache || nr_prefetched_words < 0)
    {
      nr += lf_printf (file, "SIM_DESC sd,\n");
      nr += lf_printf (file, "%sidecode_cache *cache_entry,\n",
		       options.module.global.prefix.l);
      nr += lf_printf (file, "%sinstruction_address cia",
		       options.module.global.prefix.l);
    }
  else if (options.gen.smp)
    {
      nr += lf_printf (file, "sim_cpu *cpu,\n");
      for (word_nr = 0; word_nr < nr_prefetched_words; word_nr++)
	{
	  nr += lf_printf (file, "%sinstruction_word instruction_%d,\n",
			   options.module.global.prefix.l, word_nr);
	}
      nr += lf_printf (file, "%sinstruction_address cia",
		       options.module.global.prefix.l);
    }
  else
    {
      nr += lf_printf (file, "SIM_DESC sd,\n");
      for (word_nr = 0; word_nr < nr_prefetched_words; word_nr++)
	{
	  nr += lf_printf (file, "%sinstruction_word instruction_%d,\n",
			   options.module.global.prefix.l, word_nr);
	}
      nr += lf_printf (file, "%sinstruction_address cia",
		       options.module.global.prefix.l);
    }
  return nr;
}

int
print_semantic_function_actual (lf *file, int nr_prefetched_words)
{
  int nr = 0;
  int word_nr;
  if (options.gen.icache || nr_prefetched_words < 0)
    {
      nr += lf_printf (file, "sd, cache_entry, cia");
    }
  else
    {
      if (options.gen.smp)
	nr += lf_printf (file, "cpu");
      else
	nr += lf_printf (file, "sd");
      for (word_nr = 0; word_nr < nr_prefetched_words; word_nr++)
	nr += lf_printf (file, ", instruction_%d", word_nr);
      nr += lf_printf (file, ", cia");
    }
  return nr;
}

int
print_semantic_function_type (lf *file)
{
  int nr = 0;
  nr += lf_printf (file, "%sinstruction_address",
		   options.module.global.prefix.l);
  return nr;
}


/* Idecode functions */

int
print_icache_function_formal (lf *file, int nr_prefetched_words)
{
  int nr = 0;
  int word_nr;
  if (options.gen.smp)
    nr += lf_printf (file, "sim_cpu *cpu,\n");
  else
    nr += lf_printf (file, "SIM_DESC sd,\n");
  for (word_nr = 0; word_nr < nr_prefetched_words; word_nr++)
    nr += lf_printf (file, " %sinstruction_word instruction_%d,\n",
		     options.module.global.prefix.l, word_nr);
  nr += lf_printf (file, " %sinstruction_address cia,\n",
		   options.module.global.prefix.l);
  nr += lf_printf (file, " %sidecode_cache *cache_entry",
		   options.module.global.prefix.l);
  return nr;
}

int
print_icache_function_actual (lf *file, int nr_prefetched_words)
{
  int nr = 0;
  int word_nr;
  if (options.gen.smp)
    nr += lf_printf (file, "cpu");
  else
    nr += lf_printf (file, "sd");
  for (word_nr = 0; word_nr < nr_prefetched_words; word_nr++)
    nr += lf_printf (file, ", instruction_%d", word_nr);
  nr += lf_printf (file, ", cia, cache_entry");
  return nr;
}

int
print_icache_function_type (lf *file)
{
  int nr;
  if (options.gen.semantic_icache)
    {
      nr = print_semantic_function_type (file);
    }
  else
    {
      nr = lf_printf (file, "%sidecode_semantic *",
		      options.module.global.prefix.l);
    }
  return nr;
}


/* Function names */

static int
print_opcode_bits (lf *file, opcode_bits *bits)
{
  int nr = 0;
  if (bits == NULL)
    return nr;
  nr += lf_putchr (file, '_');
  nr += lf_putstr (file, bits->field->val_string);
  if (bits->opcode->is_boolean && bits->value == 0)
    nr += lf_putint (file, bits->opcode->boolean_constant);
  else if (!bits->opcode->is_boolean)
    {
      if (bits->opcode->last < bits->field->last)
	nr +=
	  lf_putint (file,
		     bits->value << (bits->field->last - bits->opcode->last));
      else
	nr += lf_putint (file, bits->value);
    }
  nr += print_opcode_bits (file, bits->next);
  return nr;
}

static int
print_c_name (lf *file, const char *name)
{
  int nr = 0;
  const char *pos;
  for (pos = name; *pos != '\0'; pos++)
    {
      switch (*pos)
	{
	case '/':
	case '-':
	  break;
	case ' ':
	case '.':
	  nr += lf_putchr (file, '_');
	  break;
	default:
	  nr += lf_putchr (file, *pos);
	  break;
	}
    }
  return nr;
}

extern int
print_function_name (lf *file,
		     const char *basename,
		     const char *format_name,
		     const char *model_name,
		     opcode_bits *expanded_bits,
		     lf_function_name_prefixes prefix)
{
  int nr = 0;
  /* the prefix */
  switch (prefix)
    {
    case function_name_prefix_semantics:
      nr += lf_printf (file, "%s", options.module.semantics.prefix.l);
      nr += lf_printf (file, "semantic_");
      break;
    case function_name_prefix_idecode:
      nr += lf_printf (file, "%s", options.module.idecode.prefix.l);
      nr += lf_printf (file, "idecode_");
      break;
    case function_name_prefix_itable:
      nr += lf_printf (file, "%sitable_", options.module.itable.prefix.l);
      break;
    case function_name_prefix_icache:
      nr += lf_printf (file, "%s", options.module.icache.prefix.l);
      nr += lf_printf (file, "icache_");
      break;
    case function_name_prefix_engine:
      nr += lf_printf (file, "%s", options.module.engine.prefix.l);
      nr += lf_printf (file, "engine_");
    default:
      break;
    }

  if (model_name != NULL)
    {
      nr += print_c_name (file, model_name);
      nr += lf_printf (file, "_");
    }

  /* the function name */
  nr += print_c_name (file, basename);

  /* the format name if available */
  if (format_name != NULL)
    {
      nr += lf_printf (file, "_");
      nr += print_c_name (file, format_name);
    }

  /* the suffix */
  nr += print_opcode_bits (file, expanded_bits);

  return nr;
}


void
print_my_defines (lf *file,
		  const char *basename,
		  const char *format_name, opcode_bits *expanded_bits)
{
  /* #define MY_INDEX xxxxx */
  lf_indent_suppress (file);
  lf_printf (file, "#undef MY_INDEX\n");
  lf_indent_suppress (file);
  lf_printf (file, "#define MY_INDEX ");
  print_function_name (file,
		       basename, format_name, NULL,
		       NULL, function_name_prefix_itable);
  lf_printf (file, "\n");
  /* #define MY_PREFIX xxxxxx */
  lf_indent_suppress (file);
  lf_printf (file, "#undef ");
  print_function_name (file,
		       basename, format_name, NULL,
		       expanded_bits, function_name_prefix_none);
  lf_printf (file, "\n");
  lf_indent_suppress (file);
  lf_printf (file, "#undef MY_PREFIX\n");
  lf_indent_suppress (file);
  lf_printf (file, "#define MY_PREFIX ");
  print_function_name (file,
		       basename, format_name, NULL,
		       expanded_bits, function_name_prefix_none);
  lf_printf (file, "\n");
  /* #define MY_NAME xxxxxx */
  lf_indent_suppress (file);
  lf_indent_suppress (file);
  lf_printf (file, "#undef MY_NAME\n");
  lf_indent_suppress (file);
  lf_printf (file, "#define MY_NAME \"");
  print_function_name (file,
		       basename, format_name, NULL,
		       expanded_bits, function_name_prefix_none);
  lf_printf (file, "\"\n");
}


static int
print_itrace_prefix (lf *file)
{
  const char *prefix = "trace_prefix (";
  int indent = strlen (prefix);
  lf_printf (file, "%sSD, CPU, cia, CIA, TRACE_LINENUM_P (CPU), \\\n",
	     prefix);
  lf_indent (file, +indent);
  lf_printf (file, "%sitable[MY_INDEX].file, \\\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "%sitable[MY_INDEX].line_nr, \\\n",
	     options.module.itable.prefix.l);
  lf_printf (file, "\"");
  return indent;
}


static void
print_itrace_format (lf *file, insn_mnemonic_entry *assembler)
{
  /* pass=1 is fmt string; pass=2 is arguments */
  int pass;
  /* print the format string */
  for (pass = 1; pass <= 2; pass++)
    {
      const char *chp = assembler->format;
      chp++;			/* skip the leading quote */
      /* write out the format/args */
      while (*chp != '\0')
	{
	  if (chp[0] == '\\' && (chp[1] == '<' || chp[1] == '>'))
	    {
	      if (pass == 1)
		lf_putchr (file, chp[1]);
	      chp += 2;
	    }
	  else if (chp[0] == '<' || chp[0] == '%')
	    {
	      /* parse [ "%" ... ] "<" [ func "#" ] param ">" */
	      const char *fmt;
	      const char *func;
	      int strlen_func;
	      const char *param;
	      int strlen_param;
	      /* the "%" ... "<" format */
	      fmt = chp;
	      while (chp[0] != '<' && chp[0] != '\0')
		chp++;
	      if (chp[0] != '<')
		error (assembler->line, "Missing `<' after `%%'\n");
	      chp++;
	      /* [ "func" # ] OR "param" */
	      func = chp;
	      param = chp;
	      while (chp[0] != '>' && chp[0] != '#' && chp[0] != '\0')
		chp++;
	      strlen_func = chp - func;
	      if (chp[0] == '#')
		{
		  chp++;
		  param = chp;
		  while (chp[0] != '>' && chp[0] != '\0')
		    chp++;
		}
	      strlen_param = chp - param;
	      if (chp[0] != '>')
		error (assembler->line,
		       "Missing closing `>' in assembler string\n");
	      chp++;
	      /* now process it */
	      if (pass == 2)
		lf_printf (file, ", \\\n");
	      if (strncmp (fmt, "<", 1) == 0)
		/* implicit long int format */
		{
		  if (pass == 1)
		    lf_printf (file, "%%ld");
		  else
		    {
		      lf_printf (file, "(long) ");
		      lf_write (file, param, strlen_param);
		    }
		}
	      else if (strncmp (fmt, "%<", 2) == 0)
		/* explicit format */
		{
		  if (pass == 1)
		    lf_printf (file, "%%");
		  else
		    lf_write (file, param, strlen_param);
		}
	      else if (strncmp (fmt, "%s<", 3) == 0)
		/* string format */
		{
		  if (pass == 1)
		    lf_printf (file, "%%s");
		  else
		    {
		      lf_printf (file, "%sstr_",
				 options.module.global.prefix.l);
		      lf_write (file, func, strlen_func);
		      lf_printf (file, " (SD_, ");
		      lf_write (file, param, strlen_param);
		      lf_printf (file, ")");
		    }
		}
	      else if (strncmp (fmt, "%lx<", 4) == 0)
		/* simple hex */
		{
		  if (pass == 1)
		    lf_printf (file, "%%lx");
		  else
		    {
		      lf_printf (file, "(unsigned long) ");
		      lf_write (file, param, strlen_param);
		    }
		}
	      else if (strncmp (fmt, "%#lx<", 5) == 0)
		/* simple hex with 0x prefix */
		{
		  if (pass == 1)
		    lf_printf (file, "%%#lx");
		  else
		    {
		      lf_printf (file, "(unsigned long) ");
		      lf_write (file, param, strlen_param);
		    }
		}
	      else if (strncmp (fmt, "%08lx<", 6) == 0)
		/* simple hex */
		{
		  if (pass == 1)
		    lf_printf (file, "%%08lx");
		  else
		    {
		      lf_printf (file, "(unsigned long) ");
		      lf_write (file, param, strlen_param);
		    }
		}
	      else
		error (assembler->line, "Unknown assembler string format\n");
	    }
	  else
	    {
	      if (pass == 1)
		lf_putchr (file, chp[0]);
	      chp += 1;
	    }
	}
    }
  lf_printf (file, ");\n");
}


void
print_itrace (lf *file, insn_entry * insn, int idecode)
{
  /* NB: Here we escape each EOLN. This is so that the the compiler
     treats a trace function call as a single line.  Consequently any
     errors in the line are refered back to the same igen assembler
     source line */
  const char *phase = (idecode) ? "DECODE" : "INSN";
  lf_printf (file, "\n");
  lf_indent_suppress (file);
  lf_printf (file, "#if defined (WITH_TRACE)\n");
  lf_printf (file, "/* generate a trace prefix if any tracing enabled */\n");
  lf_printf (file, "if (TRACE_ANY_P (CPU))\n");
  lf_printf (file, "  {\n");
  lf_indent (file, +4);
  {
    if (insn->mnemonics != NULL)
      {
	insn_mnemonic_entry *assembler = insn->mnemonics;
	int is_first = 1;
	do
	  {
	    if (assembler->condition != NULL)
	      {
		int indent;
		lf_printf (file, "%sif (%s)\n",
			   is_first ? "" : "else ", assembler->condition);
		lf_indent (file, +2);
		lf_print__line_ref (file, assembler->line);
		indent = print_itrace_prefix (file);
		print_itrace_format (file, assembler);
		lf_print__internal_ref (file);
		lf_indent (file, -indent);
		lf_indent (file, -2);
		if (assembler->next == NULL)
		  error (assembler->line,
			 "Missing final unconditional assembler\n");
	      }
	    else
	      {
		int indent;
		if (!is_first)
		  {
		    lf_printf (file, "else\n");
		    lf_indent (file, +2);
		  }
		lf_print__line_ref (file, assembler->line);
		indent = print_itrace_prefix (file);
		print_itrace_format (file, assembler);
		lf_print__internal_ref (file);
		lf_indent (file, -indent);
		if (!is_first)
		  lf_indent (file, -2);
		if (assembler->next != NULL)
		  error (assembler->line,
			 "Unconditional assembler is not last\n");
	      }
	    is_first = 0;
	    assembler = assembler->next;
	  }
	while (assembler != NULL);
      }
    else
      {
	int indent;
	lf_indent (file, +2);
	lf_print__line_ref (file, insn->line);
	indent = print_itrace_prefix (file);
	lf_printf (file, "%%s\", \\\n");
	lf_printf (file, "itable[MY_INDEX].name);\n");
	lf_print__internal_ref (file);
	lf_indent (file, -indent);
	lf_indent (file, -2);
      }
    lf_printf (file, "/* trace the instruction execution if enabled */\n");
    lf_printf (file, "if (TRACE_%s_P (CPU))\n", phase);
    lf_printf (file,
	       "  trace_generic (SD, CPU, TRACE_%s_IDX, \" %%s\", itable[MY_INDEX].name);\n",
	       phase);
  }
  lf_indent (file, -4);
  lf_printf (file, "  }\n");
  lf_indent_suppress (file);
  lf_printf (file, "#endif\n");
}


void
print_sim_engine_abort (lf *file, const char *message)
{
  lf_printf (file, "sim_engine_abort (SD, CPU, cia, ");
  lf_printf (file, "\"%s\"", message);
  lf_printf (file, ");\n");
}


void
print_include (lf *file, igen_module module)
{
  lf_printf (file, "#include \"%s%s.h\"\n", module.prefix.l, module.suffix.l);
}

void
print_include_inline (lf *file, igen_module module)
{
  lf_printf (file, "#if C_REVEALS_MODULE_P (%s_INLINE)\n", module.suffix.u);
  lf_printf (file, "#include \"%s%s.c\"\n", module.prefix.l, module.suffix.l);
  lf_printf (file, "#else\n");
  print_include (file, module);
  lf_printf (file, "#endif\n");
  lf_printf (file, "\n");
}

void
print_includes (lf *file)
{
  lf_printf (file, "\n");
  lf_printf (file, "#include \"sim-inline.c\"\n");
  lf_printf (file, "\n");
  print_include_inline (file, options.module.itable);
  print_include_inline (file, options.module.idecode);
  print_include_inline (file, options.module.support);
}


/****************************************************************/


static void
gen_semantics_h (lf *file, insn_list *semantics, int max_nr_words)
{
  int word_nr;
  insn_list *semantic;
  for (word_nr = -1; word_nr <= max_nr_words; word_nr++)
    {
      lf_printf (file, "typedef ");
      print_semantic_function_type (file);
      lf_printf (file, " %sidecode_semantic", options.module.global.prefix.l);
      if (word_nr >= 0)
	lf_printf (file, "_%d", word_nr);
      lf_printf (file, "\n(");
      lf_indent (file, +1);
      print_semantic_function_formal (file, word_nr);
      lf_indent (file, -1);
      lf_printf (file, ");\n");
      lf_printf (file, "\n");
    }
  switch (options.gen.code)
    {
    case generate_calls:
      for (semantic = semantics; semantic != NULL; semantic = semantic->next)
	{
	  /* Ignore any special/internal instructions */
	  if (semantic->insn->nr_words == 0)
	    continue;
	  print_semantic_declaration (file,
				      semantic->insn,
				      semantic->expanded_bits,
				      semantic->opcodes,
				      semantic->nr_prefetched_words);
	}
      break;
    case generate_jumps:
      lf_print__this_file_is_empty (file, "generating jumps");
      break;
    }
}


static void
gen_semantics_c (lf *file, insn_list *semantics, cache_entry *cache_rules)
{
  if (options.gen.code == generate_calls)
    {
      insn_list *semantic;
      print_includes (file);
      print_include (file, options.module.semantics);
      lf_printf (file, "\n");

      for (semantic = semantics; semantic != NULL; semantic = semantic->next)
	{
	  /* Ignore any special/internal instructions */
	  if (semantic->insn->nr_words == 0)
	    continue;
	  print_semantic_definition (file,
				     semantic->insn,
				     semantic->expanded_bits,
				     semantic->opcodes,
				     cache_rules,
				     semantic->nr_prefetched_words);
	}
    }
  else
    {
      lf_print__this_file_is_empty (file, "generating jump engine");
    }
}


/****************************************************************/


static void
gen_icache_h (lf *file,
	      insn_list *semantic,
	      function_entry * functions, int max_nr_words)
{
  int word_nr;
  for (word_nr = 0; word_nr <= max_nr_words; word_nr++)
    {
      lf_printf (file, "typedef ");
      print_icache_function_type (file);
      lf_printf (file, " %sidecode_icache_%d\n(",
		 options.module.global.prefix.l, word_nr);
      print_icache_function_formal (file, word_nr);
      lf_printf (file, ");\n");
      lf_printf (file, "\n");
    }
  if (options.gen.code == generate_calls && options.gen.icache)
    {
      function_entry_traverse (file, functions,
			       print_icache_internal_function_declaration,
			       NULL);
      while (semantic != NULL)
	{
	  print_icache_declaration (file,
				    semantic->insn,
				    semantic->expanded_bits,
				    semantic->opcodes,
				    semantic->nr_prefetched_words);
	  semantic = semantic->next;
	}
    }
  else
    {
      lf_print__this_file_is_empty (file, "generating jump engine");
    }
}

static void
gen_icache_c (lf *file,
	      insn_list *semantic,
	      function_entry * functions, cache_entry *cache_rules)
{
  /* output `internal' invalid/floating-point unavailable functions
     where needed */
  if (options.gen.code == generate_calls && options.gen.icache)
    {
      lf_printf (file, "\n");
      lf_printf (file, "#include \"cpu.h\"\n");
      lf_printf (file, "#include \"idecode.h\"\n");
      lf_printf (file, "#include \"semantics.h\"\n");
      lf_printf (file, "#include \"icache.h\"\n");
      lf_printf (file, "#include \"support.h\"\n");
      lf_printf (file, "\n");
      function_entry_traverse (file, functions,
			       print_icache_internal_function_definition,
			       NULL);
      lf_printf (file, "\n");
      while (semantic != NULL)
	{
	  print_icache_definition (file,
				   semantic->insn,
				   semantic->expanded_bits,
				   semantic->opcodes,
				   cache_rules,
				   semantic->nr_prefetched_words);
	  semantic = semantic->next;
	}
    }
  else
    {
      lf_print__this_file_is_empty (file, "generating jump engine");
    }
}


/****************************************************************/


static void
gen_idecode_h (lf *file,
	       gen_table *gen, insn_table *insns, cache_entry *cache_rules)
{
  lf_printf (file, "typedef unsigned%d %sinstruction_word;\n",
	     options.insn_bit_size, options.module.global.prefix.l);
  if (options.gen.delayed_branch)
    {
      lf_printf (file, "typedef struct _%sinstruction_address {\n",
		 options.module.global.prefix.l);
      lf_printf (file, "  address_word ip; /* instruction pointer */\n");
      lf_printf (file, "  address_word dp; /* delayed-slot pointer */\n");
      lf_printf (file, "} %sinstruction_address;\n",
		 options.module.global.prefix.l);
    }
  else
    {
      lf_printf (file, "typedef address_word %sinstruction_address;\n",
		 options.module.global.prefix.l);

    }
  if (options.gen.nia == nia_is_invalid
      && strlen (options.module.global.prefix.u) > 0)
    {
      lf_indent_suppress (file);
      lf_printf (file, "#define %sINVALID_INSTRUCTION_ADDRESS ",
		 options.module.global.prefix.u);
      lf_printf (file, "INVALID_INSTRUCTION_ADDRESS\n");
    }
  lf_printf (file, "\n");
  print_icache_struct (file, insns, cache_rules);
  lf_printf (file, "\n");
  if (options.gen.icache)
    {
      ERROR ("FIXME - idecode with icache suffering from bit-rot");
    }
  else
    {
      gen_list *entry;
      for (entry = gen->tables; entry != NULL; entry = entry->next)
	{
	  print_idecode_issue_function_header (file,
					       (options.gen.multi_sim
						? entry->model->name
						: NULL),
					       is_function_declaration,
					       1 /*ALWAYS ONE WORD */ );
	}
      if (options.gen.multi_sim)
	{
	  print_idecode_issue_function_header (file,
					       NULL,
					       is_function_variable,
					       1 /*ALWAYS ONE WORD */ );
	}
    }
}


static void
gen_idecode_c (lf *file,
	       gen_table *gen, insn_table *isa, cache_entry *cache_rules)
{
  /* the intro */
  print_includes (file);
  print_include_inline (file, options.module.semantics);
  lf_printf (file, "\n");

  print_idecode_globals (file);
  lf_printf (file, "\n");

  switch (options.gen.code)
    {
    case generate_calls:
      {
	gen_list *entry;
	for (entry = gen->tables; entry != NULL; entry = entry->next)
	  {
	    print_idecode_lookups (file, entry->table, cache_rules);

	    /* output the main idecode routine */
	    if (!options.gen.icache)
	      {
		print_idecode_issue_function_header (file,
						     (options.gen.multi_sim
						      ? entry->model->name
						      : NULL),
						     1 /*is definition */ ,
						     1 /*ALWAYS ONE WORD */ );
		lf_printf (file, "{\n");
		lf_indent (file, +2);
		lf_printf (file, "%sinstruction_address nia;\n",
			   options.module.global.prefix.l);
		print_idecode_body (file, entry->table, "nia =");
		lf_printf (file, "return nia;");
		lf_indent (file, -2);
		lf_printf (file, "}\n");
	      }
	  }
	break;
      }
    case generate_jumps:
      {
	lf_print__this_file_is_empty (file, "generating a jump engine");
	break;
      }
    }
}


/****************************************************************/


static void
gen_run_c (lf *file, gen_table *gen)
{
  gen_list *entry;
  lf_printf (file, "#include \"sim-main.h\"\n");
  lf_printf (file, "#include \"engine.h\"\n");
  lf_printf (file, "#include \"idecode.h\"\n");
  lf_printf (file, "#include \"bfd.h\"\n");
  lf_printf (file, "\n");

  if (options.gen.multi_sim)
    {
      print_idecode_issue_function_header (file, NULL, is_function_variable,
					   1);
      lf_printf (file, "\n");
      print_engine_run_function_header (file, NULL, is_function_variable);
      lf_printf (file, "\n");
    }

  lf_printf (file, "void\n");
  lf_printf (file, "sim_engine_run (SIM_DESC sd,\n");
  lf_printf (file, "                int next_cpu_nr,\n");
  lf_printf (file, "                int nr_cpus,\n");
  lf_printf (file, "                int siggnal)\n");
  lf_printf (file, "{\n");
  lf_indent (file, +2);
  if (options.gen.multi_sim)
    {
      lf_printf (file, "int mach;\n");
      lf_printf (file, "if (STATE_ARCHITECTURE (sd) == NULL)\n");
      lf_printf (file, "  mach = 0;\n");
      lf_printf (file, "else\n");
      lf_printf (file, "  mach = STATE_ARCHITECTURE (sd)->mach;\n");
      lf_printf (file, "switch (mach)\n");
      lf_printf (file, "  {\n");
      lf_indent (file, +2);
      for (entry = gen->tables; entry != NULL; entry = entry->next)
	{
	  if (options.gen.default_model != NULL
	      && (strcmp (entry->model->name, options.gen.default_model) == 0
		  || strcmp (entry->model->full_name,
			     options.gen.default_model) == 0))
	    lf_printf (file, "default:\n");
	  lf_printf (file, "case bfd_mach_%s:\n", entry->model->full_name);
	  lf_indent (file, +2);
	  print_function_name (file, "issue", NULL,	/* format name */
			       NULL,	/* NO processor */
			       NULL,	/* expanded bits */
			       function_name_prefix_idecode);
	  lf_printf (file, " = ");
	  print_function_name (file, "issue", NULL,	/* format name */
			       entry->model->name, NULL,	/* expanded bits */
			       function_name_prefix_idecode);
	  lf_printf (file, ";\n");
	  print_function_name (file, "run", NULL,	/* format name */
			       NULL,	/* NO processor */
			       NULL,	/* expanded bits */
			       function_name_prefix_engine);
	  lf_printf (file, " = ");
	  print_function_name (file, "run", NULL,	/* format name */
			       entry->model->name, NULL,	/* expanded bits */
			       function_name_prefix_engine);
	  lf_printf (file, ";\n");
	  lf_printf (file, "break;\n");
	  lf_indent (file, -2);
	}
      if (options.gen.default_model == NULL)
	{
	  lf_printf (file, "default:\n");
	  lf_indent (file, +2);
	  lf_printf (file, "sim_engine_abort (sd, NULL, NULL_CIA,\n");
	  lf_printf (file,
		     "                  \"sim_engine_run - unknown machine\");\n");
	  lf_printf (file, "break;\n");
	  lf_indent (file, -2);
	}
      lf_indent (file, -2);
      lf_printf (file, "  }\n");
    }
  print_function_name (file, "run", NULL,	/* format name */
		       NULL,	/* NO processor */
		       NULL,	/* expanded bits */
		       function_name_prefix_engine);
  lf_printf (file, " (sd, next_cpu_nr, nr_cpus, siggnal);\n");
  lf_indent (file, -2);
  lf_printf (file, "}\n");
}

/****************************************************************/

static gen_table *
do_gen (insn_table *isa, decode_table *decode_rules)
{
  gen_table *gen;
  if (decode_rules == NULL)
    error (NULL, "Must specify a decode table\n");
  if (isa == NULL)
    error (NULL, "Must specify an instruction table\n");
  if (decode_table_max_word_nr (decode_rules) > 0)
    options.gen.multi_word = decode_table_max_word_nr (decode_rules);
  gen = make_gen_tables (isa, decode_rules);
  gen_tables_expand_insns (gen);
  gen_tables_expand_semantics (gen);
  return gen;
}

/****************************************************************/

igen_options options;

int
main (int argc, char **argv, char **envp)
{
  cache_entry *cache_rules = NULL;
  lf_file_references file_references = lf_include_references;
  decode_table *decode_rules = NULL;
  insn_table *isa = NULL;
  gen_table *gen = NULL;
  char *real_file_name = NULL;
  int is_header = 0;
  int ch;
  lf *standard_out =
    lf_open ("-", "stdout", lf_omit_references, lf_is_text, "igen");

  INIT_OPTIONS ();

  if (argc == 1)
    {
      printf ("Usage:\n");
      printf ("\n");
      printf ("  igen <config-opts> ... <input-opts>... <output-opts>...\n");
      printf ("\n");
      printf ("Config options:\n");
      printf ("\n");
      printf ("  -B <bit-size>\n");
      printf ("\t Set the number of bits in an instruction (deprecated).\n");
      printf
	("\t This option can now be set directly in the instruction table.\n");
      printf ("\n");
      printf ("  -D <data-structure>\n");
      printf
	("\t Dump the specified data structure to stdout. Valid structures include:\n");
      printf
	("\t processor-names - list the names of all the processors (models)\n");
      printf ("\n");
      printf ("  -F <filter-list>\n");
      printf
	("\t Filter out any instructions with a non-empty flags field that contains\n");
      printf ("\t a flag not listed in the <filter-list>.\n");
      printf ("\n");
      printf ("  -H <high-bit>\n");
      printf
	("\t Set the number of the high (most significant) instruction bit (deprecated).\n");
      printf
	("\t This option can now be set directly in the instruction table.\n");
      printf ("\n");
      printf ("  -I <directory>\n");
      printf
	("\t Add <directory> to the list of directories searched when opening a file\n");
      printf ("\n");
      printf ("  -M <model-list>\n");
      printf
	("\t Filter out any instructions that do not support at least one of the listed\n");
      printf
	("\t models (An instructions with no model information is considered to support\n");
      printf ("\t all models.).\n");
      printf ("\n");
      printf ("  -N <nr-cpus>\n");
      printf ("\t Generate a simulator supporting <nr-cpus>\n");
      printf
	("\t Specify `-N 0' to disable generation of the SMP. Specifying `-N 1' will\n");
      printf
	("\t still generate an SMP enabled simulator but will only support one CPU.\n");
      printf ("\n");
      printf ("  -T <mechanism>\n");
      printf
	("\t Override the decode mechanism specified by the decode rules\n");
      printf ("\n");
      printf ("  -P <prefix>\n");
      printf
	("\t Prepend global names (except itable) with the string <prefix>.\n");
      printf
	("\t Specify -P <module>=<prefix> to set a specific <module>'s prefix.\n");
      printf ("\n");
      printf ("  -S <suffix>\n");
      printf
	("\t Replace a global name (suffix) (except itable) with the string <suffix>.\n");
      printf
	("\t Specify -S <module>=<suffix> to change a specific <module>'s name (suffix).\n");
      printf ("\n");
      printf ("  -Werror\n");
      printf ("\t Make warnings errors\n");
      printf ("  -Wnodiscard\n");
      printf
	("\t Suppress warnings about discarded functions and instructions\n");
      printf ("  -Wnowidth\n");
      printf
	("\t Suppress warnings about instructions with invalid widths\n");
      printf ("  -Wnounimplemented\n");
      printf ("\t Suppress warnings about unimplemented instructions\n");
      printf ("\n");
      printf ("  -G [!]<gen-option>\n");
      printf ("\t Any of the following options:\n");
      printf ("\n");
      printf
	("\t decode-duplicate       - Override the decode rules, forcing the duplication of\n");
      printf ("\t                          semantic functions\n");
      printf
	("\t decode-combine         - Combine any duplicated entries within a table\n");
      printf
	("\t decode-zero-reserved   - Override the decode rules, forcing reserved bits to be\n");
      printf ("\t                          treated as zero.\n");
      printf
	("\t decode-switch-is-goto  - Overfide the padded-switch code type as a goto-switch\n");
      printf ("\n");
      printf
	("\t gen-conditional-issue  - conditionally issue each instruction\n");
      printf
	("\t gen-delayed-branch     - need both cia and nia passed around\n");
      printf
	("\t gen-direct-access      - use #defines to directly access values\n");
      printf
	("\t gen-zero-r<N>          - arch assumes GPR(<N>) == 0, keep it that way\n");
      printf
	("\t gen-icache[=<N>        - generate an instruction cracking cache of size <N>\n");
      printf ("\t                          Default size is %d\n",
	      options.gen.icache_size);
      printf
	("\t gen-insn-in-icache     - save original instruction when cracking\n");
      printf
	("\t gen-multi-sim[=MODEL]  - generate multiple simulators - one per model\n");
      printf
	("\t                          If specified MODEL is made the default architecture.\n");
      printf
	("\t                          By default, a single simulator that will\n");
      printf
	("\t                          execute any instruction is generated\n");
      printf
	("\t gen-multi-word         - generate code allowing for multi-word insns\n");
      printf
	("\t gen-semantic-icache    - include semantic code in cracking functions\n");
      printf
	("\t gen-slot-verification  - perform slot verification as part of decode\n");
      printf ("\t gen-nia-invalid        - NIA defaults to nia_invalid\n");
      printf ("\t gen-nia-void           - do not compute/return NIA\n");
      printf ("\n");
      printf
	("\t trace-combine          - report combined entries a rule application\n");
      printf
	("\t trace-entries          - report entries after a rules application\n");
      printf ("\t trace-rule-rejection   - report each rule as rejected\n");
      printf ("\t trace-rule-selection   - report each rule as selected\n");
      printf
	("\t trace-insn-insertion   - report each instruction as it is inserted into a decode table\n");
      printf
	("\t trace-rule-expansion   - report each instruction as it is expanded (before insertion into a decode table)\n");
      printf ("\t trace-all              - enable all trace options\n");
      printf ("\n");
      printf
	("\t field-widths           - instruction formats specify widths (deprecated)\n");
      printf
	("\t                          By default, an instruction format specifies bit\n");
      printf ("\t                          positions\n");
      printf
	("\t                          This option can now be set directly in the\n");
      printf ("\t                          instruction table\n");
      printf
	("\t jumps                  - use jumps instead of function calls\n");
      printf
	("\t omit-line-numbers      - do not include line number information in the output\n");
      printf ("\n");
      printf ("Input options:\n");
      printf ("\n");
      printf ("  -k <cache-rules> (deprecated)\n");
      printf ("  -o <decode-rules>\n");
      printf ("  -i <instruction-table>\n");
      printf ("\n");
      printf ("Output options:\n");
      printf ("\n");
      printf ("  -x                    Perform expansion (required)\n");
      printf
	("  -n <real-name>        Specify the real name of the next output file\n");
      printf
	("  -h 		       Generate the header (.h) file rather than the body (.c)\n");
      printf ("  -c <output-file>      output icache\n");
      printf ("  -d <output-file>      output idecode\n");
      printf ("  -e <output-file>      output engine\n");
      printf ("  -f <output-file>      output support functions\n");
      printf ("  -m <output-file>      output model\n");
      printf ("  -r <output-file>      output multi-sim run\n");
      printf ("  -s <output-file>      output schematic\n");
      printf ("  -t <output-file>      output itable\n");
    }

  while ((ch = getopt (argc, argv,
		       "B:D:F:G:H:I:M:N:P:T:W:o:k:i:n:hc:d:e:m:r:s:t:f:x"))
	 != -1)
    {
      fprintf (stderr, "  -%c ", ch);
      if (optarg)
	fprintf (stderr, "%s ", optarg);
      fprintf (stderr, "\\\n");

      switch (ch)
	{

	case 'M':
	  filter_parse (&options.model_filter, optarg);
	  break;

	case 'D':
	  if (strcmp (optarg, "processor-names"))
	    {
	      char *processor;
	      for (processor = filter_next (options.model_filter, "");
		   processor != NULL;
		   processor = filter_next (options.model_filter, processor))
		lf_printf (standard_out, "%s\n", processor);
	    }
	  else
	    error (NULL, "Unknown data structure %s, not dumped\n", optarg);
	  break;

	case 'F':
	  filter_parse (&options.flags_filter, optarg);
	  break;

	case 'I':
	  {
	    table_include **dir = &options.include;
	    while ((*dir) != NULL)
	      dir = &(*dir)->next;
	    (*dir) = ZALLOC (table_include);
	    (*dir)->dir = strdup (optarg);
	  }
	  break;

	case 'B':
	  options.insn_bit_size = a2i (optarg);
	  if (options.insn_bit_size <= 0
	      || options.insn_bit_size > max_insn_bit_size)
	    {
	      error (NULL, "Instruction bitsize must be in range 1..%d\n",
		     max_insn_bit_size);
	    }
	  if (options.hi_bit_nr != options.insn_bit_size - 1
	      && options.hi_bit_nr != 0)
	    {
	      error (NULL, "Conflict betweem hi-bit-nr and insn-bit-size\n");
	    }
	  break;

	case 'H':
	  options.hi_bit_nr = a2i (optarg);
	  if (options.hi_bit_nr != options.insn_bit_size - 1
	      && options.hi_bit_nr != 0)
	    {
	      error (NULL, "Conflict between hi-bit-nr and insn-bit-size\n");
	    }
	  break;

	case 'N':
	  options.gen.smp = a2i (optarg);
	  break;

	case 'P':
	case 'S':
	  {
	    igen_module *names;
	    igen_name *name;
	    char *chp;
	    chp = strchr (optarg, '=');
	    if (chp == NULL)
	      {
		names = &options.module.global;
		chp = optarg;
	      }
	    else
	      {
		chp = chp + 1;	/* skip `=' */
		names = NULL;
		if (strncmp (optarg, "global=", chp - optarg) == 0)
		  {
		    names = &options.module.global;
		  }
		if (strncmp (optarg, "engine=", chp - optarg) == 0)
		  {
		    names = &options.module.engine;
		  }
		if (strncmp (optarg, "icache=", chp - optarg) == 0)
		  {
		    names = &options.module.icache;
		  }
		if (strncmp (optarg, "idecode=", chp - optarg) == 0)
		  {
		    names = &options.module.idecode;
		  }
		if (strncmp (optarg, "itable=", chp - optarg) == 0)
		  {
		    names = &options.module.itable;
		  }
		if (strncmp (optarg, "semantics=", chp - optarg) == 0)
		  {
		    names = &options.module.semantics;
		  }
		if (strncmp (optarg, "support=", chp - optarg) == 0)
		  {
		    names = &options.module.support;
		  }
		if (names == NULL)
		  {
		    error (NULL, "Prefix `%s' unreconized\n", optarg);
		  }
	      }
	    switch (ch)
	      {
	      case 'P':
		name = &names->prefix;
		break;
	      case 'S':
		name = &names->suffix;
		break;
	      default:
		abort ();	/* Bad switch.  */
	      }
	    name->u = strdup (chp);
	    name->l = strdup (chp);
	    chp = name->u;
	    while (*chp)
	      {
		if (islower (*chp))
		  *chp = toupper (*chp);
		chp++;
	      }
	    if (name == &options.module.global.prefix)
	      {
		options.module.engine.prefix = options.module.global.prefix;
		options.module.icache.prefix = options.module.global.prefix;
		options.module.idecode.prefix = options.module.global.prefix;
		/* options.module.itable.prefix = options.module.global.prefix; */
		options.module.semantics.prefix =
		  options.module.global.prefix;
		options.module.support.prefix = options.module.global.prefix;
	      }
	    if (name == &options.module.global.suffix)
	      {
		options.module.engine.suffix = options.module.global.suffix;
		options.module.icache.suffix = options.module.global.suffix;
		options.module.idecode.suffix = options.module.global.suffix;
		/* options.module.itable.suffix = options.module.global.suffix; */
		options.module.semantics.suffix =
		  options.module.global.suffix;
		options.module.support.suffix = options.module.global.suffix;
	      }
	    break;
	  }

	case 'W':
	  {
	    if (strcmp (optarg, "error") == 0)
	      options.warning = error;
	    else if (strcmp (optarg, "nodiscard") == 0)
	      options.warn.discard = 0;
	    else if (strcmp (optarg, "discard") == 0)
	      options.warn.discard = 1;
	    else if (strcmp (optarg, "nowidth") == 0)
	      options.warn.width = 0;
	    else if (strcmp (optarg, "width") == 0)
	      options.warn.width = 1;
	    else if (strcmp (optarg, "nounimplemented") == 0)
	      options.warn.unimplemented = 0;
	    else if (strcmp (optarg, "unimplemented") == 0)
	      options.warn.unimplemented = 1;
	    else
	      error (NULL, "Unknown -W argument `%s'\n", optarg);
	    break;
	  }


	case 'G':
	  {
	    int enable_p;
	    char *argp;
	    if (strncmp (optarg, "no-", strlen ("no-")) == 0)
	      {
		argp = optarg + strlen ("no-");
		enable_p = 0;
	      }
	    else if (strncmp (optarg, "!", strlen ("!")) == 0)
	      {
		argp = optarg + strlen ("no-");
		enable_p = 0;
	      }
	    else
	      {
		argp = optarg;
		enable_p = 1;
	      }
	    if (strcmp (argp, "decode-duplicate") == 0)
	      {
		options.decode.duplicate = enable_p;
	      }
	    else if (strcmp (argp, "decode-combine") == 0)
	      {
		options.decode.combine = enable_p;
	      }
	    else if (strcmp (argp, "decode-zero-reserved") == 0)
	      {
		options.decode.zero_reserved = enable_p;
	      }

	    else if (strcmp (argp, "gen-conditional-issue") == 0)
	      {
		options.gen.conditional_issue = enable_p;
	      }
	    else if (strcmp (argp, "conditional-issue") == 0)
	      {
		options.gen.conditional_issue = enable_p;
		options.warning (NULL,
				 "Option conditional-issue replaced by gen-conditional-issue\n");
	      }
	    else if (strcmp (argp, "gen-delayed-branch") == 0)
	      {
		options.gen.delayed_branch = enable_p;
	      }
	    else if (strcmp (argp, "delayed-branch") == 0)
	      {
		options.gen.delayed_branch = enable_p;
		options.warning (NULL,
				 "Option delayed-branch replaced by gen-delayed-branch\n");
	      }
	    else if (strcmp (argp, "gen-direct-access") == 0)
	      {
		options.gen.direct_access = enable_p;
	      }
	    else if (strcmp (argp, "direct-access") == 0)
	      {
		options.gen.direct_access = enable_p;
		options.warning (NULL,
				 "Option direct-access replaced by gen-direct-access\n");
	      }
	    else if (strncmp (argp, "gen-zero-r", strlen ("gen-zero-r")) == 0)
	      {
		options.gen.zero_reg = enable_p;
		options.gen.zero_reg_nr = atoi (argp + strlen ("gen-zero-r"));
	      }
	    else if (strncmp (argp, "zero-r", strlen ("zero-r")) == 0)
	      {
		options.gen.zero_reg = enable_p;
		options.gen.zero_reg_nr = atoi (argp + strlen ("zero-r"));
		options.warning (NULL,
				 "Option zero-r<N> replaced by gen-zero-r<N>\n");
	      }
	    else if (strncmp (argp, "gen-icache", strlen ("gen-icache")) == 0)
	      {
		switch (argp[strlen ("gen-icache")])
		  {
		  case '=':
		    options.gen.icache_size =
		      atoi (argp + strlen ("gen-icache") + 1);
		    options.gen.icache = enable_p;
		    break;
		  case '\0':
		    options.gen.icache = enable_p;
		    break;
		  default:
		    error (NULL,
			   "Expecting -Ggen-icache or -Ggen-icache=<N>\n");
		  }
	      }
	    else if (strcmp (argp, "gen-insn-in-icache") == 0)
	      {
		options.gen.insn_in_icache = enable_p;
	      }
	    else if (strncmp (argp, "gen-multi-sim", strlen ("gen-multi-sim"))
		     == 0)
	      {
		char *arg = &argp[strlen ("gen-multi-sim")];
		switch (arg[0])
		  {
		  case '=':
		    options.gen.multi_sim = enable_p;
		    options.gen.default_model = arg + 1;
		    if (!filter_is_member
			(options.model_filter, options.gen.default_model))
		      error (NULL, "multi-sim model %s unknown\n",
			     options.gen.default_model);
		    break;
		  case '\0':
		    options.gen.multi_sim = enable_p;
		    options.gen.default_model = NULL;
		    break;
		  default:
		    error (NULL,
			   "Expecting -Ggen-multi-sim or -Ggen-multi-sim=<MODEL>\n");
		    break;
		  }
	      }
	    else if (strcmp (argp, "gen-multi-word") == 0)
	      {
		options.gen.multi_word = enable_p;
	      }
	    else if (strcmp (argp, "gen-semantic-icache") == 0)
	      {
		options.gen.semantic_icache = enable_p;
	      }
	    else if (strcmp (argp, "gen-slot-verification") == 0)
	      {
		options.gen.slot_verification = enable_p;
	      }
	    else if (strcmp (argp, "verify-slot") == 0)
	      {
		options.gen.slot_verification = enable_p;
		options.warning (NULL,
				 "Option verify-slot replaced by gen-slot-verification\n");
	      }
	    else if (strcmp (argp, "gen-nia-invalid") == 0)
	      {
		options.gen.nia = nia_is_invalid;
	      }
	    else if (strcmp (argp, "default-nia-minus-one") == 0)
	      {
		options.gen.nia = nia_is_invalid;
		options.warning (NULL,
				 "Option default-nia-minus-one replaced by gen-nia-invalid\n");
	      }
	    else if (strcmp (argp, "gen-nia-void") == 0)
	      {
		options.gen.nia = nia_is_void;
	      }
	    else if (strcmp (argp, "trace-all") == 0)
	      {
		memset (&options.trace, enable_p, sizeof (options.trace));
	      }
	    else if (strcmp (argp, "trace-combine") == 0)
	      {
		options.trace.combine = enable_p;
	      }
	    else if (strcmp (argp, "trace-entries") == 0)
	      {
		options.trace.entries = enable_p;
	      }
	    else if (strcmp (argp, "trace-rule-rejection") == 0)
	      {
		options.trace.rule_rejection = enable_p;
	      }
	    else if (strcmp (argp, "trace-rule-selection") == 0)
	      {
		options.trace.rule_selection = enable_p;
	      }
	    else if (strcmp (argp, "trace-insn-insertion") == 0)
	      {
		options.trace.insn_insertion = enable_p;
	      }
	    else if (strcmp (argp, "trace-insn-expansion") == 0)
	      {
		options.trace.insn_expansion = enable_p;
	      }
	    else if (strcmp (argp, "jumps") == 0)
	      {
		options.gen.code = generate_jumps;
	      }
	    else if (strcmp (argp, "field-widths") == 0)
	      {
		options.insn_specifying_widths = enable_p;
	      }
	    else if (strcmp (argp, "omit-line-numbers") == 0)
	      {
		file_references = lf_omit_references;
	      }
	    else
	      {
		error (NULL, "Unknown option %s\n", optarg);
	      }
	    break;
	  }

	case 'i':
	  isa = load_insn_table (optarg, cache_rules);
	  if (isa->illegal_insn == NULL)
	    error (NULL, "illegal-instruction missing from insn table\n");
	  break;

	case 'x':
	  gen = do_gen (isa, decode_rules);
	  break;

	case 'o':
	  decode_rules = load_decode_table (optarg);
	  break;

	case 'k':
	  if (isa != NULL)
	    error (NULL, "Cache file must appear before the insn file\n");
	  cache_rules = load_cache_table (optarg);
	  break;

	case 'n':
	  real_file_name = strdup (optarg);
	  break;

	case 'h':
	  is_header = 1;
	  break;

	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'm':
	case 'r':
	case 's':
	case 't':
	  {
	    lf *file = lf_open (optarg, real_file_name, file_references,
				(is_header ? lf_is_h : lf_is_c),
				argv[0]);
	    if (gen == NULL && ch != 't' && ch != 'm' && ch != 'f')
	      {
		options.warning (NULL,
				 "Explicitly generate tables with -x option\n");
		gen = do_gen (isa, decode_rules);
	      }
	    lf_print__file_start (file);
	    switch (ch)
	      {
	      case 'm':
		if (is_header)
		  gen_model_h (file, isa);
		else
		  gen_model_c (file, isa);
		break;
	      case 't':
		if (is_header)
		  gen_itable_h (file, isa);
		else
		  gen_itable_c (file, isa);
		break;
	      case 'f':
		if (is_header)
		  gen_support_h (file, isa);
		else
		  gen_support_c (file, isa);
		break;
	      case 'r':
		if (is_header)
		  options.warning (NULL, "-hr option ignored\n");
		else
		  gen_run_c (file, gen);
		break;
	      case 's':
		if (is_header)
		  gen_semantics_h (file, gen->semantics, isa->max_nr_words);
		else
		  gen_semantics_c (file, gen->semantics, isa->caches);
		break;
	      case 'd':
		if (is_header)
		  gen_idecode_h (file, gen, isa, cache_rules);
		else
		  gen_idecode_c (file, gen, isa, cache_rules);
		break;
	      case 'e':
		if (is_header)
		  gen_engine_h (file, gen, isa, cache_rules);
		else
		  gen_engine_c (file, gen, isa, cache_rules);
		break;
	      case 'c':
		if (is_header)
		  gen_icache_h (file,
				gen->semantics,
				isa->functions, isa->max_nr_words);
		else
		  gen_icache_c (file,
				gen->semantics, isa->functions, cache_rules);
		break;
	      }
	    lf_print__file_finish (file);
	    lf_close (file);
	    is_header = 0;
	  }
	  real_file_name = NULL;
	  break;
	default:
	  ERROR ("Bad switch");
	}
    }
  return (0);
}
