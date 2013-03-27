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

#include "gen-idecode.h"
#include "gen-icache.h"
#include "gen-semantics.h"



static void
lf_print_opcodes (lf *file, gen_entry *table)
{
  if (table !=NULL)
    {
      while (1)
	{
	  ASSERT (table->opcode != NULL);
	  lf_printf (file, "_%d_%d",
		     table->opcode->first, table->opcode->last);
	  if (table->parent == NULL)
	    break;
	  lf_printf (file, "__%d", table->opcode_nr);
	  table = table->parent;
	}
    }
}




static void
print_idecode_ifetch (lf *file,
		      int previous_nr_prefetched_words,
		      int current_nr_prefetched_words)
{
  int word_nr;
  for (word_nr = previous_nr_prefetched_words;
       word_nr < current_nr_prefetched_words; word_nr++)
    {
      lf_printf (file,
		 "instruction_word instruction_%d = IMEM%d_IMMED (cia, %d);\n",
		 word_nr, options.insn_bit_size, word_nr);

    }
}



/****************************************************************/


static void
lf_print_table_name (lf *file, gen_entry *table)
{
  lf_printf (file, "idecode_table");
  lf_print_opcodes (file, table);
}



static void
print_idecode_table (lf *file, gen_entry *entry, const char *result)
{
  lf_printf (file, "/* prime the search */\n");
  lf_printf (file, "idecode_table_entry *table = ");
  lf_print_table_name (file, entry);
  lf_printf (file, ";\n");
  lf_printf (file, "int opcode = EXTRACTED%d (instruction, %d, %d);\n",
	     options.insn_bit_size,
	     i2target (options.hi_bit_nr, entry->opcode->first),
	     i2target (options.hi_bit_nr, entry->opcode->last));
  lf_printf (file, "idecode_table_entry *table_entry = table + opcode;\n");

  lf_printf (file, "\n");
  lf_printf (file, "/* iterate until a leaf */\n");
  lf_printf (file, "while (1) {\n");
  lf_printf (file, "  signed shift = table_entry->shift;\n");
  lf_printf (file, "if (shift == function_entry) break;\n");
  lf_printf (file, "  if (shift >= 0) {\n");
  lf_printf (file, "    table = ((idecode_table_entry*)\n");
  lf_printf (file, "             table_entry->function_or_table);\n");
  lf_printf (file, "    opcode = ((instruction & table_entry->mask)\n");
  lf_printf (file, "              >> shift);\n");
  lf_printf (file, "    table_entry = table + opcode;\n");
  lf_printf (file, "  }\n");
  lf_printf (file, "  else {\n");
  lf_printf (file, "    /* must be a boolean */\n");
  lf_printf (file, "    ASSERT(table_entry->shift == boolean_entry);\n");
  lf_printf (file, "    opcode = ((instruction & table_entry->mask)\n");
  lf_printf (file, "              != table_entry->value);\n");
  lf_printf (file, "    table = ((idecode_table_entry*)\n");
  lf_printf (file, "             table_entry->function_or_table);\n");
  lf_printf (file, "    table_entry = table + opcode;\n");
  lf_printf (file, "  }\n");
  lf_printf (file, "}\n");

  lf_printf (file, "\n");
  lf_printf (file, "/* call the leaf code */\n");
  if (options.gen.code == generate_jumps)
    {
      lf_printf (file, "goto *table_entry->function_or_table;\n");
    }
  else
    {
      lf_printf (file, "%s ", result);
      if (options.gen.icache)
	{
	  lf_printf (file,
		     "(((idecode_icache*)table_entry->function_or_table)\n");
	  lf_printf (file, "  (");
	  print_icache_function_actual (file, 1);
	  lf_printf (file, "));\n");
	}
      else
	{
	  lf_printf (file,
		     "((idecode_semantic*)table_entry->function_or_table)\n");
	  lf_printf (file, "  (");
	  print_semantic_function_actual (file, 1);
	  lf_printf (file, ");\n");
	}
    }
}


static void
print_idecode_table_start (lf *file, gen_entry *table, int depth, void *data)
{
  ASSERT (depth == 0);
  /* start of the table */
  if (table->opcode_rule->gen == array_gen)
    {
      lf_printf (file, "\n");
      lf_printf (file, "static idecode_table_entry ");
      lf_print_table_name (file, table);
      lf_printf (file, "[] = {\n");
    }
}

static void
print_idecode_table_leaf (lf *file, gen_entry *entry, int depth, void *data)
{
  gen_entry *master_entry;
  ASSERT (entry->parent != NULL);
  ASSERT (depth == 0);
  if (entry->combined_parent == NULL)
    master_entry = entry;
  else
    master_entry = entry->combined_parent;

  /* add an entry to the table */
  if (entry->parent->opcode_rule->gen == array_gen)
    {
      lf_printf (file, "  /*%d*/ { ", entry->opcode_nr);
      if (entry->opcode == NULL)
	{
	  ASSERT (entry->nr_insns == 1);
	  /* table leaf entry */
	  lf_printf (file, "function_entry, 0, 0, ");
	  if (options.gen.code == generate_jumps)
	    {
	      lf_printf (file, "&&");
	    }
	  print_function_name (file,
			       entry->insns->insn->name,
			       entry->insns->insn->format_name,
			       NULL,
			       master_entry->expanded_bits,
			       (options.gen.icache
				? function_name_prefix_icache
				: function_name_prefix_semantics));
	}
      else if (entry->opcode_rule->gen == switch_gen
	       || entry->opcode_rule->gen == goto_switch_gen
	       || entry->opcode_rule->gen == padded_switch_gen)
	{
	  /* table calling switch statement */
	  lf_printf (file, "function_entry, 0, 0, ");
	  if (options.gen.code == generate_jumps)
	    {
	      lf_printf (file, "&&");
	    }
	  lf_print_table_name (file, entry);
	}
      else if (entry->opcode->is_boolean)
	{
	  /* table `calling' boolean table */
	  lf_printf (file, "boolean_entry, ");
	  lf_printf (file, "MASK32(%d, %d), ",
		     i2target (options.hi_bit_nr, entry->opcode->first),
		     i2target (options.hi_bit_nr, entry->opcode->last));
	  lf_printf (file, "INSERTED32(%d, %d, %d), ",
		     entry->opcode->boolean_constant,
		     i2target (options.hi_bit_nr, entry->opcode->first),
		     i2target (options.hi_bit_nr, entry->opcode->last));
	  lf_print_table_name (file, entry);
	}
      else
	{
	  /* table `calling' another table */
	  lf_printf (file, "%d, ",
		     options.insn_bit_size - entry->opcode->last - 1);
	  lf_printf (file, "MASK%d(%d,%d), ", options.insn_bit_size,
		     i2target (options.hi_bit_nr, entry->opcode->first),
		     i2target (options.hi_bit_nr, entry->opcode->last));
	  lf_printf (file, "0, ");
	  lf_print_table_name (file, entry);
	}
      lf_printf (file, " },\n");
    }
}

static void
print_idecode_table_end (lf *file, gen_entry *table, int depth, void *data)
{
  ASSERT (depth == 0);
  if (table->opcode_rule->gen == array_gen)
    {
      lf_printf (file, "};\n");
    }
}

/****************************************************************/


static void
print_goto_switch_name (lf *file, gen_entry *entry)
{
  lf_printf (file, "case_");
  if (entry->opcode == NULL)
    {
      print_function_name (file,
			   entry->insns->insn->name,
			   entry->insns->insn->format_name,
			   NULL,
			   entry->expanded_bits,
			   (options.gen.icache
			    ? function_name_prefix_icache
			    : function_name_prefix_semantics));
    }
  else
    {
      lf_print_table_name (file, entry);
    }
}

static void
print_goto_switch_table_leaf (lf *file,
			      gen_entry *entry, int depth, void *data)
{
  ASSERT (entry->parent != NULL);
  ASSERT (depth == 0);
  ASSERT (entry->parent->opcode_rule->gen == goto_switch_gen);
  ASSERT (entry->parent->opcode);

  lf_printf (file, "/* %d */ &&", entry->opcode_nr);
  if (entry->combined_parent != NULL)
    print_goto_switch_name (file, entry->combined_parent);
  else
    print_goto_switch_name (file, entry);
  lf_printf (file, ",\n");
}

static void
print_goto_switch_break (lf *file, gen_entry *entry)
{
  lf_printf (file, "goto break_");
  lf_print_table_name (file, entry->parent);
  lf_printf (file, ";\n");
}


static void
print_goto_switch_table (lf *file, gen_entry *table)
{
  lf_printf (file, "const static void *");
  lf_print_table_name (file, table);
  lf_printf (file, "[] = {\n");
  lf_indent (file, +2);
  gen_entry_traverse_tree (file, table, 0, NULL /*start */ ,
			   print_goto_switch_table_leaf, NULL /*end */ ,
			   NULL /*data */ );
  lf_indent (file, -2);
  lf_printf (file, "};\n");
}


void print_idecode_switch (lf *file, gen_entry *table, const char *result);

static void
print_idecode_switch_start (lf *file, gen_entry *table, int depth, void *data)
{
  /* const char *result = data; */
  ASSERT (depth == 0);
  ASSERT (table->opcode_rule->gen == switch_gen
	  || table->opcode_rule->gen == goto_switch_gen
	  || table->opcode_rule->gen == padded_switch_gen);

  if (table->opcode->is_boolean
      || table->opcode_rule->gen == switch_gen
      || table->opcode_rule->gen == padded_switch_gen)
    {
      lf_printf (file, "switch (EXTRACTED%d (instruction_%d, %d, %d))\n",
		 options.insn_bit_size,
		 table->opcode_rule->word_nr,
		 i2target (options.hi_bit_nr, table->opcode->first),
		 i2target (options.hi_bit_nr, table->opcode->last));
      lf_indent (file, +2);
      lf_printf (file, "{\n");
    }
  else if (table->opcode_rule->gen == goto_switch_gen)
    {
      if (table->parent != NULL
	  && (table->parent->opcode_rule->gen == switch_gen
	      || table->parent->opcode_rule->gen == goto_switch_gen
	      || table->parent->opcode_rule->gen == padded_switch_gen))
	{
	  lf_printf (file, "{\n");
	  lf_indent (file, +2);
	}
      print_goto_switch_table (file, table);
      lf_printf (file, "ASSERT (EXTRACTED%d (instruction_%d, %d, %d)\n",
		 options.insn_bit_size,
		 table->opcode->word_nr,
		 i2target (options.hi_bit_nr, table->opcode->first),
		 i2target (options.hi_bit_nr, table->opcode->last));
      lf_printf (file, "        < (sizeof (");
      lf_print_table_name (file, table);
      lf_printf (file, ") / sizeof(void*)));\n");
      lf_printf (file, "goto *");
      lf_print_table_name (file, table);
      lf_printf (file, "[EXTRACTED%d (instruction_%d, %d, %d)];\n",
		 options.insn_bit_size,
		 table->opcode->word_nr,
		 i2target (options.hi_bit_nr, table->opcode->first),
		 i2target (options.hi_bit_nr, table->opcode->last));
    }
  else
    {
      ASSERT ("bad switch" == NULL);
    }
}


static void
print_idecode_switch_leaf (lf *file, gen_entry *entry, int depth, void *data)
{
  const char *result = data;
  ASSERT (entry->parent != NULL);
  ASSERT (depth == 0);
  ASSERT (entry->parent->opcode_rule->gen == switch_gen
	  || entry->parent->opcode_rule->gen == goto_switch_gen
	  || entry->parent->opcode_rule->gen == padded_switch_gen);
  ASSERT (entry->parent->opcode);

  /* skip over any instructions combined into another entry */
  if (entry->combined_parent != NULL)
    return;

  if (entry->parent->opcode->is_boolean && entry->opcode_nr == 0)
    {
      /* case: boolean false target */
      lf_printf (file, "case %d:\n", entry->parent->opcode->boolean_constant);
    }
  else if (entry->parent->opcode->is_boolean && entry->opcode_nr != 0)
    {
      /* case: boolean true case */
      lf_printf (file, "default:\n");
    }
  else if (entry->parent->opcode_rule->gen == switch_gen
	   || entry->parent->opcode_rule->gen == padded_switch_gen)
    {
      /* case: <opcode-nr> - switch */
      gen_entry *cob;
      for (cob = entry; cob != NULL; cob = cob->combined_next)
	lf_printf (file, "case %d:\n", cob->opcode_nr);
    }
  else if (entry->parent->opcode_rule->gen == goto_switch_gen)
    {
      /* case: <opcode-nr> - goto-switch */
      print_goto_switch_name (file, entry);
      lf_printf (file, ":\n");
    }
  else
    {
      ERROR ("bad switch");
    }
  lf_printf (file, "  {\n");
  lf_indent (file, +4);
  {
    if (entry->opcode == NULL)
      {
	/* switch calling leaf */
	ASSERT (entry->nr_insns == 1);
	print_idecode_ifetch (file, entry->nr_prefetched_words,
			      entry->insns->semantic->nr_prefetched_words);
	switch (options.gen.code)
	  {
	  case generate_jumps:
	    lf_printf (file, "goto ");
	    break;
	  case generate_calls:
	    lf_printf (file, "%s", result);
	    break;
	  }
	print_function_name (file,
			     entry->insns->insn->name,
			     entry->insns->insn->format_name,
			     NULL,
			     entry->expanded_bits,
			     (options.gen.icache
			      ? function_name_prefix_icache
			      : function_name_prefix_semantics));
	if (options.gen.code == generate_calls)
	  {
	    lf_printf (file, " (");
	    print_semantic_function_actual (file,
					    entry->insns->semantic->
					    nr_prefetched_words);
	    lf_printf (file, ")");
	  }
	lf_printf (file, ";\n");
      }
    else if (entry->opcode_rule->gen == switch_gen
	     || entry->opcode_rule->gen == goto_switch_gen
	     || entry->opcode_rule->gen == padded_switch_gen)
      {
	/* switch calling switch */
	lf_printf (file, "{\n");
	lf_indent (file, +2);
	print_idecode_ifetch (file, entry->parent->nr_prefetched_words,
			      entry->nr_prefetched_words);
	print_idecode_switch (file, entry, result);
	lf_indent (file, -2);
	lf_printf (file, "}\n");
      }
    else
      {
	/* switch looking up a table */
	lf_printf (file, "{\n");
	lf_indent (file, +2);
	print_idecode_ifetch (file, entry->parent->nr_prefetched_words,
			      entry->nr_prefetched_words);
	print_idecode_table (file, entry, result);
	lf_indent (file, -2);
	lf_printf (file, "}\n");
      }
    if (entry->parent->opcode->is_boolean
	|| entry->parent->opcode_rule->gen == switch_gen
	|| entry->parent->opcode_rule->gen == padded_switch_gen)
      {
	lf_printf (file, "break;\n");
      }
    else if (entry->parent->opcode_rule->gen == goto_switch_gen)
      {
	print_goto_switch_break (file, entry);
      }
    else
      {
	ERROR ("bad switch");
      }
  }
  lf_indent (file, -4);
  lf_printf (file, "  }\n");
}


static void
print_idecode_switch_illegal (lf *file, const char *result)
{
  lf_indent (file, +2);
  print_idecode_invalid (file, result, invalid_illegal);
  lf_printf (file, "break;\n");
  lf_indent (file, -2);
}

static void
print_idecode_switch_end (lf *file, gen_entry *table, int depth, void *data)
{
  const char *result = data;
  ASSERT (depth == 0);
  ASSERT (table->opcode_rule->gen == switch_gen
	  || table->opcode_rule->gen == goto_switch_gen
	  || table->opcode_rule->gen == padded_switch_gen);
  ASSERT (table->opcode);

  if (table->opcode->is_boolean)
    {
      lf_printf (file, "}\n");
      lf_indent (file, -2);
    }
  else if (table->opcode_rule->gen == switch_gen
	   || table->opcode_rule->gen == padded_switch_gen)
    {
      lf_printf (file, "default:\n");
      lf_indent (file, +2);
      if (table->nr_entries == table->opcode->nr_opcodes)
	{
	  print_sim_engine_abort (file,
				  "Internal error - bad switch generated");
	  lf_printf (file, "%sNULL_CIA;\n", result);
	  lf_printf (file, "break;\n");
	}
      else
	{
	  print_idecode_switch_illegal (file, result);
	}
      lf_indent (file, -2);
      lf_printf (file, "}\n");
      lf_indent (file, -2);
    }
  else if (table->opcode_rule->gen == goto_switch_gen)
    {
      lf_printf (file, "illegal_");
      lf_print_table_name (file, table);
      lf_printf (file, ":\n");
      print_idecode_invalid (file, result, invalid_illegal);
      lf_printf (file, "break_");
      lf_print_table_name (file, table);
      lf_printf (file, ":;\n");
      if (table->parent != NULL
	  && (table->parent->opcode_rule->gen == switch_gen
	      || table->parent->opcode_rule->gen == goto_switch_gen
	      || table->parent->opcode_rule->gen == padded_switch_gen))
	{
	  lf_indent (file, -2);
	  lf_printf (file, "}\n");
	}
    }
  else
    {
      ERROR ("bad switch");
    }
}


void
print_idecode_switch (lf *file, gen_entry *table, const char *result)
{
  gen_entry_traverse_tree (file, table,
			   0,
			   print_idecode_switch_start,
			   print_idecode_switch_leaf,
			   print_idecode_switch_end, (void *) result);
}


static void
print_idecode_switch_function_header (lf *file,
				      gen_entry *table,
				      int is_function_definition,
				      int nr_prefetched_words)
{
  lf_printf (file, "\n");
  if (options.gen.code == generate_calls)
    {
      lf_printf (file, "static ");
      if (options.gen.icache)
	{
	  lf_printf (file, "idecode_semantic *");
	}
      else
	{
	  lf_printf (file, "unsigned_word");
	}
      if (is_function_definition)
	{
	  lf_printf (file, "\n");
	}
      else
	{
	  lf_printf (file, " ");
	}
      lf_print_table_name (file, table);
      lf_printf (file, "\n(");
      print_icache_function_formal (file, nr_prefetched_words);
      lf_printf (file, ")");
      if (!is_function_definition)
	{
	  lf_printf (file, ";");
	}
      lf_printf (file, "\n");
    }
  if (options.gen.code == generate_jumps && is_function_definition)
    {
      lf_indent (file, -1);
      lf_print_table_name (file, table);
      lf_printf (file, ":\n");
      lf_indent (file, +1);
    }
}


static void
idecode_declare_if_switch (lf *file, gen_entry *table, int depth, void *data)
{
  if ((table->opcode_rule->gen == switch_gen || table->opcode_rule->gen == goto_switch_gen || table->opcode_rule->gen == padded_switch_gen) &&table->parent != NULL	/* don't declare the top one yet */
      && table->parent->opcode_rule->gen == array_gen)
    {
      print_idecode_switch_function_header (file,
					    table,
					    0 /*isnt function definition */ ,
					    0);
    }
}


static void
idecode_expand_if_switch (lf *file, gen_entry *table, int depth, void *data)
{
  if ((table->opcode_rule->gen == switch_gen || table->opcode_rule->gen == goto_switch_gen || table->opcode_rule->gen == padded_switch_gen) &&table->parent != NULL	/* don't expand the top one yet */
      && table->parent->opcode_rule->gen == array_gen)
    {
      print_idecode_switch_function_header (file,
					    table,
					    1 /*is function definition */ ,
					    0);
      if (options.gen.code == generate_calls)
	{
	  lf_printf (file, "{\n");
	  lf_indent (file, +2);
	}
      print_idecode_switch (file, table, "return");
      if (options.gen.code == generate_calls)
	{
	  lf_indent (file, -2);
	  lf_printf (file, "}\n");
	}
    }
}


/****************************************************************/


void
print_idecode_lookups (lf *file, gen_entry *table, cache_entry *cache_rules)
{
  int depth;

  /* output switch function declarations where needed by tables */
  gen_entry_traverse_tree (file, table, 1, idecode_declare_if_switch,	/* START */
			   NULL, NULL, NULL);

  /* output tables where needed */
  for (depth = gen_entry_depth (table); depth > 0; depth--)
    {
      gen_entry_traverse_tree (file, table,
			       1 - depth,
			       print_idecode_table_start,
			       print_idecode_table_leaf,
			       print_idecode_table_end, NULL);
    }

  /* output switch functions where needed */
  gen_entry_traverse_tree (file, table, 1, idecode_expand_if_switch,	/* START */
			   NULL, NULL, NULL);
}


void
print_idecode_body (lf *file, gen_entry *table, const char *result)
{
  if (table->opcode_rule->gen == switch_gen
      || table->opcode_rule->gen == goto_switch_gen
      || table->opcode_rule->gen == padded_switch_gen)
    {
      print_idecode_switch (file, table, result);
    }
  else
    {
      print_idecode_table (file, table, result);
    }
}


/****************************************************************/

#if 0
static void
print_jump (lf *file, int is_tail)
{
  if (is_tail)
    {
      lf_putstr (file, "if (keep_running != NULL && !*keep_running)\n");
      lf_putstr (file, "  cpu_halt(cpu, nia, was_continuing, 0/*na*/);\n");
    }

  if (!options.generate_smp)
    {
      lf_putstr (file, "if (WITH_EVENTS) {\n");
      lf_putstr (file, "  if (event_queue_tick(events)) {\n");
      lf_putstr (file, "    cpu_set_program_counter(cpu, nia);\n");
      lf_putstr (file, "    event_queue_process(events);\n");
      lf_putstr (file, "    nia = cpu_get_program_counter(cpu);\n");
      lf_putstr (file, "  }\n");
      lf_putstr (file, "}\n");
    }

  if (options.generate_smp)
    {
      if (is_tail)
	{
	  lf_putstr (file, "cpu_set_program_counter(cpu, nia);\n");
	}
      lf_putstr (file, "if (WITH_EVENTS) {\n");
      lf_putstr (file, "  current_cpu += 1;\n");
      lf_putstr (file, "  if (current_cpu >= nr_cpus) {\n");
      lf_putstr (file, "    if (event_queue_tick(events)) {\n");
      lf_putstr (file, "      event_queue_process(events);\n");
      lf_putstr (file, "    }\n");
      lf_putstr (file, "    current_cpu = 0;\n");
      lf_putstr (file, "  }\n");
      lf_putstr (file, "}\n");
      lf_putstr (file, "else {\n");
      lf_putstr (file, "  current_cpu = (current_cpu + 1) % nr_cpus;\n");
      lf_putstr (file, "}\n");
      lf_putstr (file, "cpu = cpus[current_cpu];\n");
      lf_putstr (file, "nia = cpu_get_program_counter(cpu);\n");
    }

  if (options.gen.icache)
    {
      lf_putstr (file, "cache_entry = cpu_icache_entry(cpu, nia);\n");
      lf_putstr (file, "if (cache_entry->address == nia) {\n");
      lf_putstr (file, "  /* cache hit */\n");
      lf_putstr (file, "  goto *cache_entry->semantic;\n");
      lf_putstr (file, "}\n");
      if (is_tail)
	{
	  lf_putstr (file, "goto cache_miss;\n");
	}
    }

  if (!options.gen.icache && is_tail)
    {
      lf_printf (file, "goto idecode;\n");
    }

}
#endif



#if 0
static void
print_jump_insn (lf *file,
		 insn_entry * instruction,
		 insn_bits * expanded_bits,
		 opcode_field *opcodes, cache_entry *cache_rules)
{

  /* what we are for the moment */
  lf_printf (file, "\n");
  print_my_defines (file, expanded_bits, instruction->name);

  /* output the icache entry */
  if (options.gen.icache)
    {
      lf_printf (file, "\n");
      lf_indent (file, -1);
      print_function_name (file,
			   instruction->name,
			   expanded_bits, function_name_prefix_icache);
      lf_printf (file, ":\n");
      lf_indent (file, +1);
      lf_printf (file, "{\n");
      lf_indent (file, +2);
      lf_putstr (file, "const unsigned_word cia = nia;\n");
      print_itrace (file, instruction, 1 /*putting-value-in-cache */ );
      print_idecode_validate (file, instruction, opcodes);
      lf_printf (file, "\n");
      lf_printf (file, "{\n");
      lf_indent (file, +2);
      print_icache_body (file, instruction, expanded_bits, cache_rules, 0,	/*use_defines */
			 put_values_in_icache);
      lf_printf (file, "cache_entry->address = nia;\n");
      lf_printf (file, "cache_entry->semantic = &&");
      print_function_name (file,
			   instruction->name,
			   expanded_bits, function_name_prefix_semantics);
      lf_printf (file, ";\n");
      if (options.gen.semantic_icache)
	{
	  print_semantic_body (file, instruction, expanded_bits, opcodes);
	  print_jump (file, 1 /*is-tail */ );
	}
      else
	{
	  lf_printf (file, "/* goto ");
	  print_function_name (file,
			       instruction->name,
			       expanded_bits, function_name_prefix_semantics);
	  lf_printf (file, "; */\n");
	}
      lf_indent (file, -2);
      lf_putstr (file, "}\n");
      lf_indent (file, -2);
      lf_printf (file, "}\n");
    }

  /* print the semantics */
  lf_printf (file, "\n");
  lf_indent (file, -1);
  print_function_name (file,
		       instruction->name,
		       expanded_bits, function_name_prefix_semantics);
  lf_printf (file, ":\n");
  lf_indent (file, +1);
  lf_printf (file, "{\n");
  lf_indent (file, +2);
  lf_putstr (file, "const unsigned_word cia = nia;\n");
  print_icache_body (file,
		     instruction,
		     expanded_bits,
		     cache_rules,
		     (options.gen.direct_access
		      ? define_variables
		      : declare_variables),
		     (options.gen.icache
		      ? get_values_from_icache : do_not_use_icache));
  print_semantic_body (file, instruction, expanded_bits, opcodes);
  if (options.gen.direct_access)
    print_icache_body (file,
		       instruction,
		       expanded_bits,
		       cache_rules,
		       undef_variables,
		       (options.gen.icache
			? get_values_from_icache : do_not_use_icache));
  print_jump (file, 1 /*is tail */ );
  lf_indent (file, -2);
  lf_printf (file, "}\n");
}
#endif


#if 0
static void
print_jump_definition (lf *file,
		       gen_entry *entry,
		       insn_entry * insn, int depth, void *data)
{
  cache_entry *cache_rules = (cache_entry *) data;
  if (options.generate_expanded_instructions)
    {
      ASSERT (entry->nr_insns == 1
	      && entry->opcode == NULL
	      && entry->parent != NULL && entry->parent->opcode != NULL);
      ASSERT (entry->nr_insns == 1
	      && entry->opcode == NULL
	      && entry->parent != NULL
	      && entry->parent->opcode != NULL
	      && entry->parent->opcode_rule != NULL);
      print_jump_insn (file,
		       entry->insns->words[0]->insn,
		       entry->expanded_bits, entry->opcode, cache_rules);
    }
  else
    {
      print_jump_insn (file,
		       instruction->words[0]->insn, NULL, NULL, cache_rules);
    }
}
#endif

#if 0
static void
print_jump_internal_function (lf *file,
			      gen_entry *table,
			      function_entry * function, void *data)
{
  if (function->is_internal)
    {
      lf_printf (file, "\n");
      lf_print__line_ref (file, function->line);
      lf_indent (file, -1);
      print_function_name (file,
			   function->name,
			   NULL,
			   (options.gen.icache
			    ? function_name_prefix_icache
			    : function_name_prefix_semantics));
      lf_printf (file, ":\n");
      lf_indent (file, +1);
      lf_printf (file, "{\n");
      lf_indent (file, +2);
      lf_printf (file, "const unsigned_word cia = nia;\n");
      table_print_code (file, function->code);
      lf_print__internal_ref (file);
      print_sim_engine_abort (file, "Internal function must longjump");
      lf_indent (file, -2);
      lf_printf (file, "}\n");
    }
}
#endif



#if 0
static void
print_jump_until_stop_body (lf *file,
			    insn_table *table, cache_table * cache_rules)
{
  lf_printf (file, "{\n");
  lf_indent (file, +2);
  lf_putstr (file, "jmp_buf halt;\n");
  lf_putstr (file, "jmp_buf restart;\n");
  lf_putstr (file, "sim_cpu *cpu = NULL;\n");
  lf_putstr (file, "unsigned_word nia = -1;\n");
  lf_putstr (file, "instruction_word instruction = 0;\n");
  if ((code & generate_with_icache))
    {
      lf_putstr (file, "idecode_cache *cache_entry = NULL;\n");
    }
  if (generate_smp)
    {
      lf_putstr (file, "int current_cpu = -1;\n");
    }

  /* all the switches and tables - they know about jumping */
  print_idecode_lookups (file, table, cache_rules);

  /* start the simulation up */
  if ((code & generate_with_icache))
    {
      lf_putstr (file, "\n");
      lf_putstr (file, "{\n");
      lf_putstr (file, "  int cpu_nr;\n");
      lf_putstr (file, "  for (cpu_nr = 0; cpu_nr < nr_cpus; cpu_nr++)\n");
      lf_putstr (file, "    cpu_flush_icache(cpus[cpu_nr]);\n");
      lf_putstr (file, "}\n");
    }

  lf_putstr (file, "\n");
  lf_putstr (file, "psim_set_halt_and_restart(system, &halt, &restart);\n");

  lf_putstr (file, "\n");
  lf_putstr (file, "if (setjmp(halt))\n");
  lf_putstr (file, "  return;\n");

  lf_putstr (file, "\n");
  lf_putstr (file, "setjmp(restart);\n");

  lf_putstr (file, "\n");
  if (!generate_smp)
    {
      lf_putstr (file, "cpu = cpus[0];\n");
      lf_putstr (file, "nia = cpu_get_program_counter(cpu);\n");
    }
  else
    {
      lf_putstr (file, "current_cpu = psim_last_cpu(system);\n");
    }

  if (!(code & generate_with_icache))
    {
      lf_printf (file, "\n");
      lf_indent (file, -1);
      lf_printf (file, "idecode:\n");
      lf_indent (file, +1);
    }

  print_jump (file, 0 /*is_tail */ );

  if ((code & generate_with_icache))
    {
      lf_indent (file, -1);
      lf_printf (file, "cache_miss:\n");
      lf_indent (file, +1);
    }

  lf_putstr (file, "instruction\n");
  lf_putstr (file, "  = vm_instruction_map_read(cpu_instruction_map(cpu),\n");
  lf_putstr (file, "                            cpu, nia);\n");
  print_idecode_body (file, table, "/*IGORE*/");

  /* print out a table of all the internals functions */
  insn_table_traverse_function (table,
				file, NULL, print_jump_internal_function);

  /* print out a table of all the instructions */
  if (generate_expanded_instructions)
    insn_table_traverse_tree (table, file, cache_rules, 1, NULL,	/* start */
			      print_jump_definition,	/* leaf */
			      NULL,	/* end */
			      NULL);	/* padding */
  else
    insn_table_traverse_insn (table,
			      file, cache_rules, print_jump_definition);
  lf_indent (file, -2);
  lf_printf (file, "}\n");
}
#endif

/****************************************************************/



/* Output code to do any final checks on the decoded instruction.
   This includes things like verifying any on decoded fields have the
   correct value and checking that (for floating point) floating point
   hardware isn't disabled */

void
print_idecode_validate (lf *file,
			insn_entry * instruction, insn_opcodes *opcode_paths)
{
  /* Validate: unchecked instruction fields

     If any constant fields in the instruction were not checked by the
     idecode tables, output code to check that they have the correct
     value here */
  {
    int nr_checks = 0;
    int word_nr;
    lf_printf (file, "\n");
    lf_indent_suppress (file);
    lf_printf (file, "#if defined (WITH_RESERVED_BITS)\n");
    lf_printf (file, "/* validate: ");
    print_insn_words (file, instruction);
    lf_printf (file, " */\n");
    for (word_nr = 0; word_nr < instruction->nr_words; word_nr++)
      {
	insn_uint check_mask = 0;
	insn_uint check_val = 0;
	insn_word_entry *word = instruction->word[word_nr];
	int bit_nr;

	/* form check_mask/check_val containing what needs to be checked
	   in the instruction */
	for (bit_nr = 0; bit_nr < options.insn_bit_size; bit_nr++)
	  {
	    insn_bit_entry *bit = word->bit[bit_nr];
	    insn_field_entry *field = bit->field;

	    /* Make space for the next bit */
	    check_mask <<= 1;
	    check_val <<= 1;

	    /* Only need to validate constant (and reserved)
	       bits. Skip any others */
	    if (field->type != insn_field_int
		&& field->type != insn_field_reserved)
	      continue;

	    /* Look through the list of opcode paths that lead to this
	       instruction.  See if any have failed to check the
	       relevant bit */
	    if (opcode_paths != NULL)
	      {
		insn_opcodes *entry;
		for (entry = opcode_paths; entry != NULL; entry = entry->next)
		  {
		    opcode_field *opcode;
		    for (opcode = entry->opcode;
			 opcode != NULL; opcode = opcode->parent)
		      {
			if (opcode->word_nr == word_nr
			    && opcode->first <= bit_nr
			    && opcode->last >= bit_nr)
			  /* we've decoded on this bit */
			  break;
		      }
		    if (opcode == NULL)
		      /* the bit wasn't decoded on */
		      break;
		  }
		if (entry == NULL)
		  /* all the opcode paths decoded on BIT_NR, no need
		     to check it */
		  continue;
	      }

	    check_mask |= 1;
	    check_val |= bit->value;
	  }

	/* if any bits not checked by opcode tables, output code to check them */
	if (check_mask)
	  {
	    if (nr_checks == 0)
	      {
		lf_printf (file, "if (WITH_RESERVED_BITS)\n");
		lf_printf (file, "  {\n");
		lf_indent (file, +4);
	      }
	    nr_checks++;
	    if (options.insn_bit_size > 32)
	      {
		lf_printf (file, "if ((instruction_%d\n", word_nr);
		lf_printf (file, "     & UNSIGNED64 (0x%08lx%08lx))\n",
			   (unsigned long) (check_mask >> 32),
			   (unsigned long) (check_mask));
		lf_printf (file, "    != UNSIGNED64 (0x%08lx%08lx))\n",
			   (unsigned long) (check_val >> 32),
			   (unsigned long) (check_val));
	      }
	    else
	      {
		lf_printf (file,
			   "if ((instruction_%d & 0x%08lx) != 0x%08lx)\n",
			   word_nr, (unsigned long) (check_mask),
			   (unsigned long) (check_val));
	      }
	    lf_indent (file, +2);
	    print_idecode_invalid (file, "return", invalid_illegal);
	    lf_indent (file, -2);
	  }
      }
    if (nr_checks > 0)
      {
	lf_indent (file, -4);
	lf_printf (file, "  }\n");
      }
    lf_indent_suppress (file);
    lf_printf (file, "#endif\n");
  }

  /* Validate: Floating Point hardware

     If the simulator is being built with out floating point hardware
     (different to it being disabled in the MSR) then floating point
     instructions are invalid */
  {
    if (filter_is_member (instruction->flags, "f"))
      {
	lf_printf (file, "\n");
	lf_indent_suppress (file);
	lf_printf (file, "#if defined(CURRENT_FLOATING_POINT)\n");
	lf_printf (file, "/* Validate: FP hardware exists */\n");
	lf_printf (file,
		   "if (CURRENT_FLOATING_POINT != HARD_FLOATING_POINT) {\n");
	lf_indent (file, +2);
	print_idecode_invalid (file, "return", invalid_illegal);
	lf_indent (file, -2);
	lf_printf (file, "}\n");
	lf_indent_suppress (file);
	lf_printf (file, "#endif\n");
      }
  }

  /* Validate: Floating Point available

     If floating point is not available, we enter a floating point
     unavailable interrupt into the cache instead of the instruction
     proper.

     The PowerPC spec requires a CSI after MSR[FP] is changed and when
     ever a CSI occures we flush the instruction cache. */

  {
    if (filter_is_member (instruction->flags, "f"))
      {
	lf_printf (file, "\n");
	lf_indent_suppress (file);
	lf_printf (file, "#if defined(IS_FP_AVAILABLE)\n");
	lf_printf (file, "/* Validate: FP available according to cpu */\n");
	lf_printf (file, "if (!IS_FP_AVAILABLE) {\n");
	lf_indent (file, +2);
	print_idecode_invalid (file, "return", invalid_fp_unavailable);
	lf_indent (file, -2);
	lf_printf (file, "}\n");
	lf_indent_suppress (file);
	lf_printf (file, "#endif\n");
      }
  }

  /* Validate: Validate Instruction in correct slot

     Some architectures place restrictions on the slot that an
     instruction can be issued in */

  {
    if (filter_is_member (instruction->options, "s")
	|| options.gen.slot_verification)
      {
	lf_printf (file, "\n");
	lf_indent_suppress (file);
	lf_printf (file, "#if defined(IS_WRONG_SLOT)\n");
	lf_printf (file,
		   "/* Validate: Instruction issued in correct slot */\n");
	lf_printf (file, "if (IS_WRONG_SLOT) {\n");
	lf_indent (file, +2);
	print_idecode_invalid (file, "return", invalid_wrong_slot);
	lf_indent (file, -2);
	lf_printf (file, "}\n");
	lf_indent_suppress (file);
	lf_printf (file, "#endif\n");
      }
  }

}


/****************************************************************/


void
print_idecode_issue_function_header (lf *file,
				     const char *processor,
				     function_decl_type decl_type,
				     int nr_prefetched_words)
{
  int indent;
  lf_printf (file, "\n");
  switch (decl_type)
    {
    case is_function_declaration:
      lf_print__function_type_function (file, print_semantic_function_type,
					"INLINE_IDECODE", " ");
      break;
    case is_function_definition:
      lf_print__function_type_function (file, print_semantic_function_type,
					"INLINE_IDECODE", "\n");
      break;
    case is_function_variable:
      print_semantic_function_type (file);
      lf_printf (file, " (*");
      break;
    }
  indent = print_function_name (file,
				"issue",
				NULL,
				processor,
				NULL, function_name_prefix_idecode);
  switch (decl_type)
    {
    case is_function_definition:
      indent += lf_printf (file, " (");
      break;
    case is_function_declaration:
      lf_putstr (file, "\n(");
      indent = 1;
      break;
    case is_function_variable:
      lf_putstr (file, ")\n(");
      indent = 1;
      break;
    }
  lf_indent (file, +indent);
  print_semantic_function_formal (file, nr_prefetched_words);
  lf_putstr (file, ")");
  lf_indent (file, -indent);
  switch (decl_type)
    {
    case is_function_definition:
      lf_printf (file, "\n");
      break;
    case is_function_declaration:
    case is_function_variable:
      lf_putstr (file, ";\n");
      break;
    }
}



void
print_idecode_globals (lf *file)
{
  lf_printf (file, "enum {\n");
  lf_printf (file, "  /* greater or equal to zero => table */\n");
  lf_printf (file, "  function_entry = -1,\n");
  lf_printf (file, "  boolean_entry = -2,\n");
  lf_printf (file, "};\n");
  lf_printf (file, "\n");
  lf_printf (file, "typedef struct _idecode_table_entry {\n");
  lf_printf (file, "  int shift;\n");
  lf_printf (file, "  unsigned%d mask;\n", options.insn_bit_size);
  lf_printf (file, "  unsigned%d value;\n", options.insn_bit_size);
  lf_printf (file, "  void *function_or_table;\n");
  lf_printf (file, "} idecode_table_entry;\n");
}
