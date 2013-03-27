/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003 Andrew Cagney

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

#include "gen-idecode.h"
#include "gen-icache.h"
#include "gen-semantics.h"



static void
lf_print_opcodes(lf *file,
		 insn_table *table)
{
  if (table != NULL) {
    while (1) {
      ASSERT(table->opcode != NULL);
      lf_printf(file, "_%d_%d",
		table->opcode->first,
		table->opcode->last);
      if (table->parent == NULL) break;
      lf_printf(file, "__%d", table->opcode_nr);
      table = table->parent;
    }
  }
}

/****************************************************************/


static void
lf_print_table_name(lf *file,
		    insn_table *table)
{
  lf_printf(file, "idecode_table");
  lf_print_opcodes(file, table);
}



static void
print_idecode_table(lf *file,
		    insn_table *entry,
		    const char *result)
{
  lf_printf(file, "/* prime the search */\n");
  lf_printf(file, "idecode_table_entry *table = ");
  lf_print_table_name(file, entry);
  lf_printf(file, ";\n");
  lf_printf(file, "int opcode = EXTRACTED32(instruction, %d, %d);\n",
	    i2target(hi_bit_nr, entry->opcode->first),
	    i2target(hi_bit_nr, entry->opcode->last));
  lf_printf(file, "idecode_table_entry *table_entry = table + opcode;\n");

  lf_printf(file, "\n");
  lf_printf(file, "/* iterate until a leaf */\n");
  lf_printf(file, "while (1) {\n");
  lf_printf(file, "  signed shift = table_entry->shift;\n");
  lf_printf(file, "if (shift == function_entry) break;\n");
  lf_printf(file, "  if (shift >= 0) {\n");
  lf_printf(file, "    table = ((idecode_table_entry*)\n");
  lf_printf(file, "             table_entry->function_or_table);\n");
  lf_printf(file, "    opcode = ((instruction & table_entry->mask)\n");
  lf_printf(file, "              >> shift);\n");
  lf_printf(file, "    table_entry = table + opcode;\n");
  lf_printf(file, "  }\n");
  lf_printf(file, "  else {\n");
  lf_printf(file, "    /* must be a boolean */\n");
  lf_printf(file, "    ASSERT(table_entry->shift == boolean_entry);\n");
  lf_printf(file, "    opcode = ((instruction & table_entry->mask)\n");
  lf_printf(file, "              != table_entry->value);\n");
  lf_printf(file, "    table = ((idecode_table_entry*)\n");
  lf_printf(file, "             table_entry->function_or_table);\n");
  lf_printf(file, "    table_entry = table + opcode;\n");
  lf_printf(file, "  }\n");
  lf_printf(file, "}\n");

  lf_printf(file, "\n");
  lf_printf(file, "/* call the leaf code */\n");
  if ((code & generate_jumps)) {
    lf_printf(file, "goto *table_entry->function_or_table;\n");
  }
  else {
    lf_printf(file, "%s ", result);
    if ((code & generate_with_icache)) {
      lf_printf(file, "(((idecode_icache*)table_entry->function_or_table)\n");
      lf_printf(file, "  (%s));\n", ICACHE_FUNCTION_ACTUAL);
    }
    else {
      lf_printf(file, "((idecode_semantic*)table_entry->function_or_table)\n");
      lf_printf(file, "  (%s);\n", SEMANTIC_FUNCTION_ACTUAL);
    }
  }
}


static void
print_idecode_table_start(insn_table *table,
			  lf *file,
			  void *data,
			  int depth)
{
  ASSERT(depth == 0);
  /* start of the table */
  if (table->opcode_rule->gen == array_gen) {
    lf_printf(file, "\n");
    lf_printf(file, "static idecode_table_entry ");
    lf_print_table_name(file, table);
    lf_printf(file, "[] = {\n");
  }
}

static void
print_idecode_table_leaf(insn_table *entry,
			 lf *file,
			 void *data,
			 insn *instruction,
			 int depth)
{
  ASSERT(entry->parent != NULL);
  ASSERT(depth == 0);

  /* add an entry to the table */
  if (entry->parent->opcode_rule->gen == array_gen) {
    lf_printf(file, "  /*%d*/ { ", entry->opcode_nr);
    if (entry->opcode == NULL) {
      /* table leaf entry */
      lf_printf(file, "function_entry, 0, 0, ");
      if ((code & generate_jumps))
	lf_printf(file, "&&");
      print_function_name(file,
			  entry->insns->file_entry->fields[insn_name],
			  entry->expanded_bits,
			  ((code & generate_with_icache)
			   ? function_name_prefix_icache
			   : function_name_prefix_semantics));
    }
    else if (entry->opcode_rule->gen == switch_gen
	     || entry->opcode_rule->gen == goto_switch_gen
	     || entry->opcode_rule->gen == padded_switch_gen) {
      /* table calling switch statement */
      lf_printf(file, "function_entry, 0, 0, ");
      if ((code & generate_jumps))
	lf_printf(file, "&&");
      lf_print_table_name(file, entry);
    }
    else if (entry->opcode->is_boolean) {
      /* table `calling' boolean table */
      lf_printf(file, "boolean_entry, ");
      lf_printf(file, "MASK32(%d, %d), ",
		i2target(hi_bit_nr, entry->opcode->first),
		i2target(hi_bit_nr, entry->opcode->last));
      lf_printf(file, "INSERTED32(%d, %d, %d), ",
		entry->opcode->boolean_constant,
		i2target(hi_bit_nr, entry->opcode->first),
		i2target(hi_bit_nr, entry->opcode->last));
      lf_print_table_name(file, entry);
    }
    else {
      /* table `calling' another table */
      lf_printf(file, "%d, ", insn_bit_size - entry->opcode->last - 1);
      lf_printf(file, "MASK32(%d,%d), ",
		i2target(hi_bit_nr, entry->opcode->first),
		i2target(hi_bit_nr, entry->opcode->last));
      lf_printf(file, "0, ");
      lf_print_table_name(file, entry);
    }
    lf_printf(file, " },\n");
  }
}

static void
print_idecode_table_end(insn_table *table,
			lf *file,
			void *data,
			int depth)
{
  ASSERT(depth == 0);
  if (table->opcode_rule->gen == array_gen) {
    lf_printf(file, "};\n");
  }
}

static void
print_idecode_table_padding(insn_table *table,
			    lf *file,
			    void *data,
			    int depth,
			    int opcode_nr)
{
  ASSERT(depth == 0);
  if (table->opcode_rule->gen == array_gen) {
    lf_printf(file, "  /*%d*/ { function_entry, 0, 0, ", opcode_nr);
    if ((code & generate_jumps))
      lf_printf(file, "&&");
    lf_printf(file, "%s_illegal },\n",
	      ((code & generate_with_icache) ? "icache" : "semantic"));
  }
}


/****************************************************************/


static void
print_goto_switch_name(lf *file,
		       insn_table *entry)
{
  lf_printf(file, "case_");
  if (entry->opcode == NULL)
    print_function_name(file,
			entry->insns->file_entry->fields[insn_name],
			entry->expanded_bits,
			((code & generate_with_icache)
			 ? function_name_prefix_icache
			 : function_name_prefix_semantics));
  else
    lf_print_table_name(file, entry);
}

static void
print_goto_switch_table_leaf(insn_table *entry,
			     lf *file,
			     void *data,
			     insn *instruction,
			     int depth)
{
  ASSERT(entry->parent != NULL);
  ASSERT(depth == 0);
  ASSERT(entry->parent->opcode_rule->gen == goto_switch_gen);
  ASSERT(entry->parent->opcode);

  lf_printf(file, "&&");
  print_goto_switch_name(file, entry);
  lf_printf(file, ",\n");
}

static void
print_goto_switch_table_padding(insn_table *table,
				lf *file,
				void *data,
				int depth,
				int opcode_nr)
{
  ASSERT(depth == 0);
  ASSERT(table->opcode_rule->gen == goto_switch_gen);

  lf_printf(file, "&&illegal_");
  lf_print_table_name(file, table);
  lf_printf(file, ",\n");
}

static void
print_goto_switch_break(lf *file,
			insn_table *entry)
{
  lf_printf(file, "goto break_");
  lf_print_table_name(file, entry->parent);
  lf_printf(file, ";\n");
}


static void
print_goto_switch_table(lf *file,
			insn_table *table)
{
  lf_printf(file, "const static void *");
  lf_print_table_name(file, table);
  lf_printf(file, "[] = {\n");
  lf_indent(file, +2);
  insn_table_traverse_tree(table,
			   file, NULL/*data*/,
			   0,
			   NULL/*start*/,
			   print_goto_switch_table_leaf,
			   NULL/*end*/,
			   print_goto_switch_table_padding);
  lf_indent(file, -2);
  lf_printf(file, "};\n");
}


void print_idecode_switch
(lf *file, 
 insn_table *table,
 const char *result);

static void
idecode_switch_start(insn_table *table,
		     lf *file,
		     void *data,
		     int depth)
{
  /* const char *result = data; */
  ASSERT(depth == 0);
  ASSERT(table->opcode_rule->gen == switch_gen
	 || table->opcode_rule->gen == goto_switch_gen
	 || table->opcode_rule->gen == padded_switch_gen);

  if (table->opcode->is_boolean
      || table->opcode_rule->gen == switch_gen
	 || table->opcode_rule->gen == padded_switch_gen) {
    lf_printf(file, "switch (EXTRACTED32(instruction, %d, %d)) {\n",
	      i2target(hi_bit_nr, table->opcode->first),
	      i2target(hi_bit_nr, table->opcode->last));
  }
  else if (table->opcode_rule->gen == goto_switch_gen) {
    if (table->parent != NULL
	&& (table->parent->opcode_rule->gen == switch_gen
	    || table->parent->opcode_rule->gen == goto_switch_gen
	    || table->parent->opcode_rule->gen == padded_switch_gen)) {
      lf_printf(file, "{\n");
      lf_indent(file, +2);
    }
    print_goto_switch_table(file, table);
    lf_printf(file, "ASSERT(EXTRACTED32(instruction, %d, %d)\n",
	      i2target(hi_bit_nr, table->opcode->first),
	      i2target(hi_bit_nr, table->opcode->last));
    lf_printf(file, "       < (sizeof(");
    lf_print_table_name(file, table);
    lf_printf(file, ") / sizeof(void*)));\n");
    lf_printf(file, "goto *");
    lf_print_table_name(file, table);
    lf_printf(file, "[EXTRACTED32(instruction, %d, %d)];\n",
	      i2target(hi_bit_nr, table->opcode->first),
	      i2target(hi_bit_nr, table->opcode->last));
  }
  else {
    ASSERT("bad switch" == NULL);
  }
}


static void
idecode_switch_leaf(insn_table *entry,
		    lf *file,
		    void *data,
		    insn *instruction,
		    int depth)
{
  const char *result = data;
  ASSERT(entry->parent != NULL);
  ASSERT(depth == 0);
  ASSERT(entry->parent->opcode_rule->gen == switch_gen
	 || entry->parent->opcode_rule->gen == goto_switch_gen
	 || entry->parent->opcode_rule->gen == padded_switch_gen);
  ASSERT(entry->parent->opcode);

  if (entry->parent->opcode->is_boolean
      && entry->opcode_nr == 0) {
    /* boolean false target */
    lf_printf(file, "case %d:\n", entry->parent->opcode->boolean_constant);
  }
  else if (entry->parent->opcode->is_boolean
	   && entry->opcode_nr != 0) {
    /* boolean true case */
    lf_printf(file, "default:\n");
  }
  else if (entry->parent->opcode_rule->gen == switch_gen
	   || entry->parent->opcode_rule->gen == padded_switch_gen) {
    /* normal goto */
    lf_printf(file, "case %d:\n", entry->opcode_nr);
  }
  else if (entry->parent->opcode_rule->gen == goto_switch_gen) {
    /* lf_indent(file, -1); */
    print_goto_switch_name(file, entry);
    lf_printf(file, ":\n");
    /* lf_indent(file, +1); */
  }
  else {
    ASSERT("bad switch" == NULL);
  }
  lf_indent(file, +2);
  {
    if (entry->opcode == NULL) {
      /* switch calling leaf */
      if ((code & generate_jumps))
	lf_printf(file, "goto ");
      if ((code & generate_calls))
	lf_printf(file, "%s ", result);
      print_function_name(file,
			  entry->insns->file_entry->fields[insn_name],
			  entry->expanded_bits,
			  ((code & generate_with_icache)
			   ? function_name_prefix_icache
			   : function_name_prefix_semantics));
      if ((code & generate_calls))
	lf_printf(file, "(%s)", SEMANTIC_FUNCTION_ACTUAL);
      lf_printf(file, ";\n");
    }
    else if (entry->opcode_rule->gen == switch_gen
	     || entry->opcode_rule->gen == goto_switch_gen
	     || entry->opcode_rule->gen == padded_switch_gen) {
      /* switch calling switch */
      print_idecode_switch(file, entry, result);
    }
    else {
      /* switch looking up a table */
      lf_printf(file, "{\n");
      lf_indent(file, -2);
      print_idecode_table(file, entry, result);
      lf_indent(file, -2);
      lf_printf(file, "}\n");
    }
    if (entry->parent->opcode->is_boolean
	|| entry->parent->opcode_rule->gen == switch_gen
	|| entry->parent->opcode_rule->gen == padded_switch_gen) {
      lf_printf(file, "break;\n");
    }
    else if (entry->parent->opcode_rule->gen == goto_switch_gen) {
      print_goto_switch_break(file, entry);
    }
    else {
      ASSERT("bad switch" == NULL);
    }
  }
  lf_indent(file, -2);
}


static void
print_idecode_switch_illegal(lf *file,
			     const char *result)
{
  lf_indent(file, +2);
  print_idecode_illegal(file, result);
  lf_printf(file, "break;\n");
  lf_indent(file, -2);
}

static void
idecode_switch_end(insn_table *table,
		   lf *file,
		   void *data,
		   int depth)
{
  const char *result = data;
  ASSERT(depth == 0);
  ASSERT(table->opcode_rule->gen == switch_gen
	 || table->opcode_rule->gen == goto_switch_gen
	 || table->opcode_rule->gen == padded_switch_gen);
  ASSERT(table->opcode);

  if (table->opcode->is_boolean) {
    lf_printf(file, "}\n");
  }
  else if (table->opcode_rule->gen == switch_gen
	   || table->opcode_rule->gen == padded_switch_gen) {
    lf_printf(file, "default:\n");
    switch (table->opcode_rule->gen) {
    case switch_gen:
      print_idecode_switch_illegal(file, result);
      break;
    case padded_switch_gen:
      lf_printf(file, "  error(\"Internal error - bad switch generated\\n\");\n");
      lf_printf(file, "  break;\n");
      break;
    default:
      ASSERT("bad switch" == NULL);
    }
    lf_printf(file, "}\n");
  }
  else if (table->opcode_rule->gen == goto_switch_gen) {
    lf_printf(file, "illegal_");
    lf_print_table_name(file, table);
    lf_printf(file, ":\n");
    print_idecode_illegal(file, result);
    lf_printf(file, "break_");
    lf_print_table_name(file, table);
    lf_printf(file, ":;\n");
    if (table->parent != NULL
	&& (table->parent->opcode_rule->gen == switch_gen
	    || table->parent->opcode_rule->gen == goto_switch_gen
	    || table->parent->opcode_rule->gen == padded_switch_gen)) {
      lf_indent(file, -2);
      lf_printf(file, "}\n");
    }
  }
  else {
    ASSERT("bad switch" == NULL);
  }
}

static void
idecode_switch_padding(insn_table *table,
		       lf *file,
		       void *data,
		       int depth,
		       int opcode_nr)
{
  const char *result = data;
  ASSERT(depth == 0);
  ASSERT(table->opcode_rule->gen == switch_gen
	 || table->opcode_rule->gen == goto_switch_gen
	 || table->opcode_rule->gen == padded_switch_gen);

  switch (table->opcode_rule->gen) {
  case switch_gen:
    break;
  case padded_switch_gen:
    lf_printf(file, "case %d:\n", opcode_nr);
    print_idecode_switch_illegal(file, result);
    break;
  case goto_switch_gen:
    /* no padding needed */
    break;
  default:
    ASSERT("bad switch" != NULL);
  }
}


void
print_idecode_switch(lf *file, 
		     insn_table *table,
		     const char *result)
{
  insn_table_traverse_tree(table,
			   file, (void*)result,
			   0,
			   idecode_switch_start,
			   idecode_switch_leaf,
			   idecode_switch_end,
			   idecode_switch_padding);
}


static void
print_idecode_switch_function_header(lf *file,
				     insn_table *table,
				     int is_function_definition)
{
  lf_printf(file, "\n");
  if ((code & generate_calls)) {
    lf_printf(file, "static ");
    if ((code & generate_with_icache))
      lf_printf(file, "idecode_semantic *");
    else
      lf_printf(file, "unsigned_word");
    if (is_function_definition)
      lf_printf(file, "\n");
    else
      lf_printf(file, " ");
    lf_print_table_name(file, table);
    lf_printf(file, "\n(%s)", ICACHE_FUNCTION_FORMAL);
    if (!is_function_definition)
      lf_printf(file, ";");
    lf_printf(file, "\n");
  }
  if ((code & generate_jumps) && is_function_definition) {
    lf_indent(file, -1);
    lf_print_table_name(file, table);
    lf_printf(file, ":\n");
    lf_indent(file, +1);
  }
}


static void
idecode_declare_if_switch(insn_table *table,
			  lf *file,
			  void *data,
			  int depth)
{
  if ((table->opcode_rule->gen == switch_gen
       || table->opcode_rule->gen == goto_switch_gen
       || table->opcode_rule->gen == padded_switch_gen)
      && table->parent != NULL /* don't declare the top one yet */
      && table->parent->opcode_rule->gen == array_gen) {
    print_idecode_switch_function_header(file,
					 table,
					 0/*isnt function definition*/);
  }
}


static void
idecode_expand_if_switch(insn_table *table,
			 lf *file,
			 void *data,
			 int depth)
{
  if ((table->opcode_rule->gen == switch_gen
       || table->opcode_rule->gen == goto_switch_gen
       || table->opcode_rule->gen == padded_switch_gen)
      && table->parent != NULL /* don't expand the top one yet */
      && table->parent->opcode_rule->gen == array_gen) {
    print_idecode_switch_function_header(file,
					    table,
					    1/*is function definition*/);
    if ((code & generate_calls)) {
      lf_printf(file, "{\n");
      lf_indent(file, +2);
    }
    print_idecode_switch(file, table, "return");
    if ((code & generate_calls)) {
      lf_indent(file, -2);
      lf_printf(file, "}\n");
    }
  }
}


/****************************************************************/


static void
print_idecode_lookups(lf *file,
		      insn_table *table,
		      cache_table *cache_rules)
{
  int depth;

  /* output switch function declarations where needed by tables */
  insn_table_traverse_tree(table,
			   file, NULL,
			   1,
			   idecode_declare_if_switch, /* START */
			   NULL, NULL, NULL);
  
  /* output tables where needed */
  for (depth = insn_table_depth(table);
       depth > 0;
       depth--) {
    insn_table_traverse_tree(table,
			     file, NULL,
			     1-depth,
			     print_idecode_table_start,
			     print_idecode_table_leaf,
			     print_idecode_table_end,
			     print_idecode_table_padding);
  }
  
  /* output switch functions where needed */
  insn_table_traverse_tree(table,
			   file, NULL,
			   1,
			   idecode_expand_if_switch, /* START */
			   NULL, NULL, NULL);
}


static void
print_idecode_body(lf *file,
		   insn_table *table,
		   const char *result)
{
  if (table->opcode_rule->gen == switch_gen
      || table->opcode_rule->gen == goto_switch_gen
      || table->opcode_rule->gen == padded_switch_gen)
    print_idecode_switch(file, table, result);
  else
    print_idecode_table(file, table, result);
}


/****************************************************************/


static void
print_run_until_stop_body(lf *file,
			  insn_table *table,
			  int can_stop)
{
  /* Output the function to execute real code:

     Unfortunatly, there are multiple cases to consider vis:

     <icache> X <smp> X <events> X <keep-running-flag> X ...

     Consequently this function is written in multiple different ways */

  lf_putstr(file, "{\n");
  lf_indent(file, +2);
  lf_putstr(file, "jmp_buf halt;\n");
  lf_putstr(file, "jmp_buf restart;\n");
  if (!generate_smp) {
    lf_putstr(file, "cpu *processor = NULL;\n");
    lf_putstr(file, "unsigned_word cia = -1;\n");
  }
  lf_putstr(file, "int last_cpu;\n");
  if (generate_smp) {
    lf_putstr(file, "int current_cpu;\n");
  }

  if ((code & generate_with_icache)) {
    lf_putstr(file, "int cpu_nr;\n");
    lf_putstr(file, "\n");
    lf_putstr(file, "/* flush the icache of a possible break insn */\n");
    lf_putstr(file, "for (cpu_nr = 0; cpu_nr < nr_cpus; cpu_nr++)\n");
    lf_putstr(file, "  cpu_flush_icache(processors[cpu_nr]);\n");
  }

  lf_putstr(file, "\n");
  lf_putstr(file, "/* set the halt target initially */\n");
  lf_putstr(file, "psim_set_halt_and_restart(system, &halt, NULL);\n");
  lf_putstr(file, "if (setjmp(halt))\n");
  lf_putstr(file, "  return;\n");

  lf_putstr(file, "\n");
  lf_putstr(file, "/* where were we before the halt? */\n");
  lf_putstr(file, "last_cpu = psim_last_cpu(system);\n");

  lf_putstr(file, "\n");
  lf_putstr(file, "/* check for need to force event processing first */\n");
  lf_putstr(file, "if (WITH_EVENTS) {\n");
  lf_putstr(file, "  if (last_cpu == nr_cpus) {\n");
  lf_putstr(file, "    /* halted during event processing */\n");
  lf_putstr(file, "    event_queue_process(events);\n");
  lf_putstr(file, "    last_cpu = -1;\n");
  lf_putstr(file, "  }\n");
  lf_putstr(file, "  else if (last_cpu == nr_cpus - 1) {\n");
  lf_putstr(file, "    /* last cpu did halt */\n");
  lf_putstr(file, "    if (event_queue_tick(events)) {\n");
  lf_putstr(file, "      event_queue_process(events);\n");
  lf_putstr(file, "    }\n");
  lf_putstr(file, "    last_cpu = -1;\n");
  lf_putstr(file, "  }\n");
  lf_putstr(file, "}\n");
  lf_putstr(file, "else {\n");
  lf_putstr(file, " if (last_cpu == nr_cpus - 1)\n");
  lf_putstr(file, "   /* cpu zero is next */\n");
  lf_putstr(file, "   last_cpu = -1;\n");
  lf_putstr(file, "}\n");

  lf_putstr(file, "\n");
  lf_putstr(file, "/* have ensured that the event queue can not be first */\n");
  lf_putstr(file, "ASSERT(last_cpu >= -1 && last_cpu < nr_cpus - 1);\n");

  if (!generate_smp) {

    lf_putstr(file, "\n\
/* CASE 1: NO SMP (with or with out instruction cache).\n\
\n\
   In this case, we can take advantage of the fact that the current\n\
   instruction address does not need to be returned to the cpu object\n\
   after every execution of an instruction.  Instead it only needs to\n\
   be saved when either A. the main loop exits or B. A cpu-halt or\n\
   cpu-restart call forces the loop to be re-enered.  The later\n\
   functions always save the current cpu instruction address.\n\
\n\
   Two subcases also exist that with and that without an instruction\n\
   cache.\n\
\n\
   An additional complexity is the need to ensure that a 1:1 ratio\n\
   is maintained between the execution of an instruction and the\n\
   incrementing of the simulation clock */");

    lf_putstr(file, "\n");

    lf_putstr(file, "\n");
    lf_putstr(file, "/* now add restart target as ready to run */\n");
    lf_putstr(file, "psim_set_halt_and_restart(system, &halt, &restart);\n");
    lf_putstr(file, "if (setjmp(restart)) {\n");
    lf_putstr(file, "  if (WITH_EVENTS) {\n");
    lf_putstr(file, "    /* when restart, cpu must have been last, clock next */\n");
    lf_putstr(file, "    if (event_queue_tick(events)) {\n");
    lf_putstr(file, "      event_queue_process(events);\n");
    lf_putstr(file, "    }\n");
    lf_putstr(file, "  }\n");
    lf_putstr(file, "}\n");

    lf_putstr(file, "\n");
    lf_putstr(file, "/* prime the main loop */\n");
    lf_putstr(file, "processor = processors[0];\n");
    lf_putstr(file, "cia = cpu_get_program_counter(processor);\n");

    lf_putstr(file, "\n");
    lf_putstr(file, "while (1) {\n");
    lf_indent(file, +2);

    if (!(code & generate_with_icache)) {
      lf_putstr(file, "instruction_word instruction =\n");
      lf_putstr(file, "  vm_instruction_map_read(cpu_instruction_map(processor), processor, cia);\n");
      lf_putstr(file, "\n");
      print_idecode_body(file, table, "cia =");;
    }

    if ((code & generate_with_icache)) {
      lf_putstr(file, "idecode_cache *cache_entry =\n");
      lf_putstr(file, "  cpu_icache_entry(processor, cia);\n");
      lf_putstr(file, "if (cache_entry->address == cia) {\n");
      lf_putstr(file, "  /* cache hit */\n");
      lf_putstr(file, "  idecode_semantic *const semantic = cache_entry->semantic;\n");
      lf_putstr(file, "  cia = semantic(processor, cache_entry, cia);\n");
      /* tail */
      if (can_stop) {
	lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
	lf_putstr(file, "  cpu_halt(processor, cia, was_continuing, 0/*ignore*/);\n");
      }
      lf_putstr(file, "}\n");
      lf_putstr(file, "else {\n");
      lf_putstr(file, "  /* cache miss */\n");
      if (!(code & generate_with_semantic_icache)) {
	lf_indent(file, +2);
	lf_putstr(file, "idecode_semantic *semantic;\n");
	lf_indent(file, -2);
      }
      lf_putstr(file, "  instruction_word instruction =\n");
      lf_putstr(file, "    vm_instruction_map_read(cpu_instruction_map(processor), processor, cia);\n");
      lf_putstr(file, "  if (WITH_MON != 0)\n");
      lf_putstr(file, "    mon_event(mon_event_icache_miss, processor, cia);\n");
      if ((code & generate_with_semantic_icache)) {
	lf_putstr(file, "{\n");
	lf_indent(file, +2);
	print_idecode_body(file, table, "cia =");
	lf_indent(file, -2);
	lf_putstr(file, "}\n");
      }
      else {
	print_idecode_body(file, table, "semantic =");
	lf_putstr(file, "  cia = semantic(processor, cache_entry, cia);\n");
      }
      lf_putstr(file, "}\n");
    }

    /* events */
    lf_putstr(file, "\n");
    lf_putstr(file, "/* process any events */\n");
    lf_putstr(file, "if (WITH_EVENTS) {\n");
    lf_putstr(file, "  if (event_queue_tick(events)) {\n");
    lf_putstr(file, "    cpu_set_program_counter(processor, cia);\n");
    lf_putstr(file, "    event_queue_process(events);\n");
    lf_putstr(file, "    cia = cpu_get_program_counter(processor);\n");
    lf_putstr(file, "  }\n");
    lf_putstr(file, "}\n");

    /* tail */
    if (can_stop) {
      lf_putstr(file, "\n");
      lf_putstr(file, "/* abort if necessary */\n");
      lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
      lf_putstr(file, "  cpu_halt(processor, cia, was_continuing, 0/*not important*/);\n");
    }

    lf_indent(file, -2);
    lf_putstr(file, "}\n");
  }
    
  if (generate_smp) {

    lf_putstr(file, "\n\
/* CASE 2: SMP (With or without ICACHE)\n\
\n\
   The complexity here comes from needing to correctly restart the\n\
   system when it is aborted.  In particular if cpu0 requests a\n\
   restart, the next cpu is still cpu1.  Cpu0 being restarted after\n\
   all the other CPU's and the event queue have been processed */");

    lf_putstr(file, "\n");

    lf_putstr(file, "\n");
    lf_putstr(file, "/* now establish the restart target */\n");
    lf_putstr(file, "psim_set_halt_and_restart(system, &halt, &restart);\n");
    lf_putstr(file, "if (setjmp(restart)) {\n");
    lf_putstr(file, "  current_cpu = psim_last_cpu(system);\n");
    lf_putstr(file, "  ASSERT(current_cpu >= 0 && current_cpu < nr_cpus);\n");
    lf_putstr(file, "}\n");
    lf_putstr(file, "else {\n");
    lf_putstr(file, "  current_cpu = last_cpu;\n");
    lf_putstr(file, "  ASSERT(current_cpu >= -1 && current_cpu < nr_cpus);\n");
    lf_putstr(file, "}\n");
    

    lf_putstr(file, "\n");
    lf_putstr(file, "while (1) {\n");
    lf_indent(file, +2);

    lf_putstr(file, "\n");
    lf_putstr(file, "if (WITH_EVENTS) {\n");
    lf_putstr(file, "  current_cpu += 1;\n");
    lf_putstr(file, "  if (current_cpu == nr_cpus) {\n");
    lf_putstr(file, "    if (event_queue_tick(events)) {\n");
    lf_putstr(file, "      event_queue_process(events);\n");
    lf_putstr(file, "    }\n");
    lf_putstr(file, "    current_cpu = 0;\n");
    lf_putstr(file, "  }\n");
    lf_putstr(file, "}\n");
    lf_putstr(file, "else {\n");
    lf_putstr(file, "  current_cpu = (current_cpu + 1) % nr_cpus;\n");
    lf_putstr(file, "}\n");

    lf_putstr(file, "\n");
    lf_putstr(file, "{\n");
    lf_indent(file, +2);
    lf_putstr(file, "cpu *processor = processors[current_cpu];\n");
    lf_putstr(file, "unsigned_word cia =\n");
    lf_putstr(file, "  cpu_get_program_counter(processor);\n");

    if (!(code & generate_with_icache)) {
      lf_putstr(file, "instruction_word instruction =\n");
      lf_putstr(file, "  vm_instruction_map_read(cpu_instruction_map(processor), processor, cia);\n");
      print_idecode_body(file, table, "cia =");
      if (can_stop) {
	lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
	lf_putstr(file, "  cpu_halt(processor, cia, was_continuing, 0/*ignore*/);\n");
      }
      lf_putstr(file, "cpu_set_program_counter(processor, cia);\n");
    }

    if ((code & generate_with_icache)) {
      lf_putstr(file, "idecode_cache *cache_entry =\n");
      lf_putstr(file, "  cpu_icache_entry(processor, cia);\n");
      lf_putstr(file, "\n");
      lf_putstr(file, "if (cache_entry->address == cia) {\n");
      {
	lf_indent(file, +2);
	lf_putstr(file, "\n");
	lf_putstr(file, "/* cache hit */\n");
	lf_putstr(file, "idecode_semantic *semantic = cache_entry->semantic;\n");
	lf_putstr(file, "cia = semantic(processor, cache_entry, cia);\n");
	/* tail */
	if (can_stop) {
	  lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
	  lf_putstr(file, "  cpu_halt(processor, cia, was_continuing, 0/*ignore-signal*/);\n");
	}
	lf_putstr(file, "cpu_set_program_counter(processor, cia);\n");
	lf_putstr(file, "\n");
	lf_indent(file, -2);
      }
      lf_putstr(file, "}\n");
      lf_putstr(file, "else {\n");
      {
	lf_indent(file, +2);
	lf_putstr(file, "\n");
	lf_putstr(file, "/* cache miss */\n");
	if (!(code & generate_with_semantic_icache)) {
	  lf_putstr(file, "idecode_semantic *semantic;\n");
	}
	lf_putstr(file, "instruction_word instruction =\n");
	lf_putstr(file, "  vm_instruction_map_read(cpu_instruction_map(processor), processor, cia);\n");
	lf_putstr(file, "if (WITH_MON != 0)\n");
	lf_putstr(file, "  mon_event(mon_event_icache_miss, processors[current_cpu], cia);\n");
	if ((code & generate_with_semantic_icache)) {
	  lf_putstr(file, "{\n");
	  lf_indent(file, +2);
	  print_idecode_body(file, table, "cia =");
	  lf_indent(file, -2);
	  lf_putstr(file, "}\n");
	}
	else {
	  print_idecode_body(file, table, "semantic = ");
	  lf_putstr(file, "cia = semantic(processor, cache_entry, cia);\n");
	}
	/* tail */
	if (can_stop) {
	  lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
	  lf_putstr(file, "  cpu_halt(processor, cia, was_continuing, 0/*ignore-signal*/);\n");
	}
	lf_putstr(file, "cpu_set_program_counter(processor, cia);\n");
	lf_putstr(file, "\n");
	lf_indent(file, -2);
      }
      lf_putstr(file, "}\n");
    }

    /* close */
    lf_indent(file, -2);
    lf_putstr(file, "}\n");

    /* tail */
    lf_indent(file, -2);
    lf_putstr(file, "}\n");
  }


  lf_indent(file, -2);
  lf_putstr(file, "}\n");
}


/****************************************************************/

static void
print_jump(lf *file,
	   int is_tail)
{
  if (is_tail) {
    lf_putstr(file, "if (keep_running != NULL && !*keep_running)\n");
    lf_putstr(file, "  cpu_halt(processor, nia, was_continuing, 0/*na*/);\n");
  }
  
  if (!generate_smp) {
    lf_putstr(file, "if (WITH_EVENTS) {\n");
    lf_putstr(file, "  if (event_queue_tick(events)) {\n");
    lf_putstr(file, "    cpu_set_program_counter(processor, nia);\n");
    lf_putstr(file, "    event_queue_process(events);\n");
    lf_putstr(file, "    nia = cpu_get_program_counter(processor);\n");
    lf_putstr(file, "  }\n");
    lf_putstr(file, "}\n");
  }

  if (generate_smp) {
    if (is_tail)
      lf_putstr(file, "cpu_set_program_counter(processor, nia);\n");
    lf_putstr(file, "if (WITH_EVENTS) {\n");
    lf_putstr(file, "  current_cpu += 1;\n");
    lf_putstr(file, "  if (current_cpu >= nr_cpus) {\n");
    lf_putstr(file, "    if (event_queue_tick(events)) {\n");
    lf_putstr(file, "      event_queue_process(events);\n");
    lf_putstr(file, "    }\n");
    lf_putstr(file, "    current_cpu = 0;\n");
    lf_putstr(file, "  }\n");
    lf_putstr(file, "}\n");
    lf_putstr(file, "else {\n");
    lf_putstr(file, "  current_cpu = (current_cpu + 1) % nr_cpus;\n");
    lf_putstr(file, "}\n");
    lf_putstr(file, "processor = processors[current_cpu];\n");
    lf_putstr(file, "nia = cpu_get_program_counter(processor);\n");
  }

  if ((code & generate_with_icache)) {
    lf_putstr(file, "cache_entry = cpu_icache_entry(processor, nia);\n");
    lf_putstr(file, "if (cache_entry->address == nia) {\n");
    lf_putstr(file, "  /* cache hit */\n");
    lf_putstr(file, "  goto *cache_entry->semantic;\n");
    lf_putstr(file, "}\n");
    if (is_tail) {
      lf_putstr(file, "goto cache_miss;\n");
    }
  }

  if (!(code & generate_with_icache) && is_tail) {
    lf_printf(file, "goto idecode;\n");
  }

}





static void
print_jump_insn(lf *file,
		insn *instruction,
		insn_bits *expanded_bits,
		opcode_field *opcodes,
		cache_table *cache_rules)
{

  /* what we are for the moment */
  lf_printf(file, "\n");
  print_my_defines(file, expanded_bits, instruction->file_entry);

  /* output the icache entry */
  if ((code & generate_with_icache)) {
    lf_printf(file, "\n");
    lf_indent(file, -1);
    print_function_name(file,
			instruction->file_entry->fields[insn_name],
			expanded_bits,
			function_name_prefix_icache);
    lf_printf(file, ":\n");
    lf_indent(file, +1);
    lf_printf(file, "{\n");
    lf_indent(file, +2);
    lf_putstr(file, "const unsigned_word cia = nia;\n");
    print_itrace(file, instruction->file_entry, 1/*putting-value-in-cache*/);
    print_idecode_validate(file, instruction, opcodes);
    lf_printf(file, "\n");
    lf_printf(file, "{\n");
    lf_indent(file, +2);
    print_icache_body(file,
		      instruction,
		      expanded_bits,
		      cache_rules,
		      0, /*use_defines*/
		      put_values_in_icache);
    lf_printf(file, "cache_entry->address = nia;\n");
    lf_printf(file, "cache_entry->semantic = &&");
    print_function_name(file,
			instruction->file_entry->fields[insn_name],
			expanded_bits,
			function_name_prefix_semantics);
    lf_printf(file, ";\n");
    if ((code & generate_with_semantic_icache)) {
      print_semantic_body(file,
			  instruction,
			  expanded_bits,
			  opcodes);
      print_jump(file, 1/*is-tail*/);
    }
    else {
      lf_printf(file, "/* goto ");
      print_function_name(file,
			  instruction->file_entry->fields[insn_name],
			  expanded_bits,
			  function_name_prefix_semantics);
      lf_printf(file, "; */\n");
    }
    lf_indent(file, -2);
    lf_putstr(file, "}\n");
    lf_indent(file, -2);
    lf_printf(file, "}\n");
  }

  /* print the semantics */
  lf_printf(file, "\n");
  lf_indent(file, -1);
  print_function_name(file,
		      instruction->file_entry->fields[insn_name],
		      expanded_bits,
		      function_name_prefix_semantics);
  lf_printf(file, ":\n");
  lf_indent(file, +1);
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  lf_putstr(file, "const unsigned_word cia = nia;\n");
  print_icache_body(file,
		    instruction,
		    expanded_bits,
		    cache_rules,
		    ((code & generate_with_direct_access)
		     ? define_variables
		     : declare_variables),
		    ((code & generate_with_icache)
		     ? get_values_from_icache
		     : do_not_use_icache));
  print_semantic_body(file,
		      instruction,
		      expanded_bits,
		      opcodes);
  if (code & generate_with_direct_access)
    print_icache_body(file,
		      instruction,
		      expanded_bits,
		      cache_rules,
		      undef_variables,
		      ((code & generate_with_icache)
		       ? get_values_from_icache
		       : do_not_use_icache));
  print_jump(file, 1/*is tail*/);
  lf_indent(file, -2);
  lf_printf(file, "}\n");
}

static void
print_jump_definition(insn_table *entry,
		      lf *file,
		      void *data,
		      insn *instruction,
		      int depth)
{
  cache_table *cache_rules = (cache_table*)data;
  if (generate_expanded_instructions) {
    ASSERT(entry->nr_insn == 1
	   && entry->opcode == NULL
	   && entry->parent != NULL
	   && entry->parent->opcode != NULL);
    ASSERT(entry->nr_insn == 1
	   && entry->opcode == NULL
	   && entry->parent != NULL
	   && entry->parent->opcode != NULL
	   && entry->parent->opcode_rule != NULL);
    print_jump_insn(file,
		    entry->insns,
		    entry->expanded_bits,
		    entry->opcode,
		    cache_rules);
  }
  else {
    print_jump_insn(file,
		    instruction,
		    NULL,
		    NULL,
		    cache_rules);
  }
}


static void
print_jump_internal_function(insn_table *table,
			     lf *file,
			     void *data,
			     table_entry *function)
{
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "\n");
    table_entry_print_cpp_line_nr(file, function);
    lf_indent(file, -1);
    print_function_name(file,
			function->fields[insn_name],
			NULL,
			((code & generate_with_icache)
			 ? function_name_prefix_icache
			 : function_name_prefix_semantics));
    lf_printf(file, ":\n");
    lf_indent(file, +1);
    lf_printf(file, "{\n");
    lf_indent(file, +2);
    lf_printf(file, "const unsigned_word cia = nia;\n");
    lf_print__c_code(file, function->annex);
    lf_print__internal_reference(file);
    lf_printf(file, "error(\"Internal function must longjump\\n\");\n");
    lf_indent(file, -2);
    lf_printf(file, "}\n");
  }
}

static void
print_jump_until_stop_body(lf *file,
			   insn_table *table,
			   cache_table *cache_rules,
			   int can_stop)
{
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  if (!can_stop)
    lf_printf(file, "int *keep_running = NULL;\n");
  lf_putstr(file, "jmp_buf halt;\n");
  lf_putstr(file, "jmp_buf restart;\n");
  lf_putstr(file, "cpu *processor = NULL;\n");
  lf_putstr(file, "unsigned_word nia = -1;\n");
  lf_putstr(file, "instruction_word instruction = 0;\n");
  if ((code & generate_with_icache)) {
    lf_putstr(file, "idecode_cache *cache_entry = NULL;\n");
  }
  if (generate_smp) {
    lf_putstr(file, "int current_cpu = -1;\n");
  }

  /* all the switches and tables - they know about jumping */
  print_idecode_lookups(file, table, cache_rules);
 
  /* start the simulation up */
  if ((code & generate_with_icache)) {
    lf_putstr(file, "\n");
    lf_putstr(file, "{\n");
    lf_putstr(file, "  int cpu_nr;\n");
    lf_putstr(file, "  for (cpu_nr = 0; cpu_nr < nr_cpus; cpu_nr++)\n");
    lf_putstr(file, "    cpu_flush_icache(processors[cpu_nr]);\n");
    lf_putstr(file, "}\n");
  }

  lf_putstr(file, "\n");
  lf_putstr(file, "psim_set_halt_and_restart(system, &halt, &restart);\n");

  lf_putstr(file, "\n");
  lf_putstr(file, "if (setjmp(halt))\n");
  lf_putstr(file, "  return;\n");

  lf_putstr(file, "\n");
  lf_putstr(file, "setjmp(restart);\n");

  lf_putstr(file, "\n");
  if (!generate_smp) {
    lf_putstr(file, "processor = processors[0];\n");
    lf_putstr(file, "nia = cpu_get_program_counter(processor);\n");
  }
  else {
    lf_putstr(file, "current_cpu = psim_last_cpu(system);\n");
  }

  if (!(code & generate_with_icache)) {
    lf_printf(file, "\n");
    lf_indent(file, -1);
    lf_printf(file, "idecode:\n");
    lf_indent(file, +1);
  }

  print_jump(file, 0/*is_tail*/);

  if ((code & generate_with_icache)) {
    lf_indent(file, -1);
    lf_printf(file, "cache_miss:\n");
    lf_indent(file, +1);
  }

  lf_putstr(file, "instruction\n");
  lf_putstr(file, "  = vm_instruction_map_read(cpu_instruction_map(processor),\n");
  lf_putstr(file, "                            processor, nia);\n");
  print_idecode_body(file, table, "/*IGORE*/");

  /* print out a table of all the internals functions */
  insn_table_traverse_function(table,
			       file, NULL,
			       print_jump_internal_function);

 /* print out a table of all the instructions */
  if (generate_expanded_instructions)
    insn_table_traverse_tree(table,
			     file, cache_rules,
			     1,
			     NULL, /* start */
			     print_jump_definition, /* leaf */
			     NULL, /* end */
			     NULL); /* padding */
  else
    insn_table_traverse_insn(table,
			     file, cache_rules,
			     print_jump_definition);
  lf_indent(file, -2);
  lf_printf(file, "}\n");
}


/****************************************************************/



static void
print_idecode_floating_point_unavailable(lf *file)
{
  if ((code & generate_jumps))
    lf_printf(file, "goto %s_floating_point_unavailable;\n", (code & generate_with_icache) ? "icache" : "semantic");
  else if ((code & generate_with_icache))
    lf_printf(file, "return icache_floating_point_unavailable(%s);\n",
	      ICACHE_FUNCTION_ACTUAL);
  else
    lf_printf(file, "return semantic_floating_point_unavailable(%s);\n",
	      SEMANTIC_FUNCTION_ACTUAL);
}


/* Output code to do any final checks on the decoded instruction.
   This includes things like verifying any on decoded fields have the
   correct value and checking that (for floating point) floating point
   hardware isn't disabled */

void
print_idecode_validate(lf *file,
		       insn *instruction,
		       opcode_field *opcodes)
{
  /* Validate: unchecked instruction fields

     If any constant fields in the instruction were not checked by the
     idecode tables, output code to check that they have the correct
     value here */
  { 
    unsigned check_mask = 0;
    unsigned check_val = 0;
    insn_field *field;
    opcode_field *opcode;

    /* form check_mask/check_val containing what needs to be checked
       in the instruction */
    for (field = instruction->fields->first;
	 field->first < insn_bit_size;
	 field = field->next) {

      check_mask <<= field->width;
      check_val <<= field->width;

      /* is it a constant that could need validating? */
      if (!field->is_int && !field->is_slash)
	continue;

      /* has it been checked by a table? */
      for (opcode = opcodes; opcode != NULL; opcode = opcode->parent) {
	if (field->first >= opcode->first
	    && field->last <= opcode->last)
	  break;
      }
      if (opcode != NULL)
	continue;

      check_mask |= (1 << field->width)-1;
      check_val |= field->val_int;
    }

    /* if any bits not checked by opcode tables, output code to check them */
    if (check_mask) {
      lf_printf(file, "\n");
      lf_printf(file, "/* validate: %s */\n",
		instruction->file_entry->fields[insn_format]);
      lf_printf(file, "if (WITH_RESERVED_BITS && (instruction & 0x%x) != 0x%x)\n",
		check_mask, check_val);
      lf_indent(file, +2);
      print_idecode_illegal(file, "return");
      lf_indent(file, -2);
    }
  }

  /* Validate floating point hardware

     If the simulator is being built with out floating point hardware
     (different to it being disabled in the MSR) then floating point
     instructions are invalid */
  {
    if (it_is("f", instruction->file_entry->fields[insn_flags])) {
      lf_printf(file, "\n");
      lf_printf(file, "/* Validate: FP hardware exists */\n");
      lf_printf(file, "if (CURRENT_FLOATING_POINT != HARD_FLOATING_POINT)\n");
      lf_indent(file, +2);
      print_idecode_illegal(file, "return");
      lf_indent(file, -2);
    }
  }

  /* Validate: Floating Point available

     If floating point is not available, we enter a floating point
     unavailable interrupt into the cache instead of the instruction
     proper.

     The PowerPC spec requires a CSI after MSR[FP] is changed and when
     ever a CSI occures we flush the instruction cache. */

  {
    if (it_is("f", instruction->file_entry->fields[insn_flags])) {
      lf_printf(file, "\n");
      lf_printf(file, "/* Validate: FP available according to MSR[FP] */\n");
      lf_printf(file, "if (!IS_FP_AVAILABLE(processor))\n");
      lf_indent(file, +2);
      print_idecode_floating_point_unavailable(file);
      lf_indent(file, -2);
    }
  }
}


/****************************************************************/


static void
print_idecode_run_function_header(lf *file,
				  int can_stop,
				  int is_definition)
{
  int indent;
  lf_printf(file, "\n");
  lf_print_function_type(file, "void", "PSIM_INLINE_IDECODE", (is_definition ? " " : "\n"));
  indent = lf_putstr(file, (can_stop ? "idecode_run_until_stop" : "idecode_run"));
  if (is_definition)
    lf_putstr(file, "\n");
  else
    lf_indent(file, +indent);
  lf_putstr(file, "(psim *system,\n");
  if (can_stop)
    lf_putstr(file, " volatile int *keep_running,\n");
  lf_printf(file, " event_queue *events,\n");
  lf_putstr(file, " cpu *const processors[],\n");
  lf_putstr(file, " const int nr_cpus)");
  if (is_definition)
    lf_putstr(file, ";");
  else
    lf_indent(file, -indent);
  lf_putstr(file, "\n");
}


void
gen_idecode_h(lf *file,
	      insn_table *table,
	      cache_table *cache_rules)
{
  lf_printf(file, "/* The idecode_*.h functions shall move to support */\n");
  lf_printf(file, "#include \"idecode_expression.h\"\n");
  lf_printf(file, "#include \"idecode_fields.h\"\n");
  lf_printf(file, "#include \"idecode_branch.h\"\n");
  lf_printf(file, "\n");
  print_icache_struct(table, cache_rules, file);
  lf_printf(file, "\n");
  lf_printf(file, "#define WITH_IDECODE_SMP %d\n", generate_smp);
  lf_printf(file, "\n");
  print_idecode_run_function_header(file, 0/*can stop*/, 1/*is definition*/);
  print_idecode_run_function_header(file, 1/*can stop*/, 1/*is definition*/);
}


void
gen_idecode_c(lf *file,
	      insn_table *table,
	      cache_table *cache_rules)
{
  /* the intro */
  lf_printf(file, "#include \"inline.c\"\n");
  lf_printf(file, "\n");
  lf_printf(file, "#include \"cpu.h\"\n");
  lf_printf(file, "#include \"idecode.h\"\n");
  lf_printf(file, "#include \"semantics.h\"\n");
  lf_printf(file, "#include \"icache.h\"\n");
  lf_printf(file, "#ifdef HAVE_COMMON_FPU\n");
  lf_printf(file, "#include \"sim-inline.h\"\n");
  lf_printf(file, "#include \"sim-fpu.h\"\n");
  lf_printf(file, "#endif\n");
  lf_printf(file, "#include \"support.h\"\n");
  lf_printf(file, "\n");
  lf_printf(file, "#include <setjmp.h>\n");
  lf_printf(file, "\n");
  lf_printf(file, "enum {\n");
  lf_printf(file, "  /* greater or equal to zero => table */\n");
  lf_printf(file, "  function_entry = -1,\n");
  lf_printf(file, "  boolean_entry = -2,\n");
  lf_printf(file, "};\n");
  lf_printf(file, "\n");
  lf_printf(file, "typedef struct _idecode_table_entry {\n");
  lf_printf(file, "  int shift;\n");
  lf_printf(file, "  instruction_word mask;\n");
  lf_printf(file, "  instruction_word value;");
  lf_printf(file, "  void *function_or_table;\n");
  lf_printf(file, "} idecode_table_entry;\n");
  lf_printf(file, "\n");
  lf_printf(file, "\n");

  if ((code & generate_calls)) {

    print_idecode_lookups(file, table, cache_rules);

    /* output the main idecode routine */
    print_idecode_run_function_header(file, 0/*can stop*/, 0/*is definition*/);
    print_run_until_stop_body(file, table, 0/* have stop argument */);

    print_idecode_run_function_header(file, 1/*can stop*/, 0/*is definition*/);
    print_run_until_stop_body(file, table, 1/* no stop argument */);

  }
  else if ((code & generate_jumps)) {

    print_idecode_run_function_header(file, 0/*can stop*/, 0/*is definition*/);
    print_jump_until_stop_body(file, table, cache_rules, 0 /* have stop argument */);

    print_idecode_run_function_header(file, 1/*can stop*/, 0/*is definition*/);
    print_jump_until_stop_body(file, table, cache_rules, 1/* have stop argument */);

  }
  else {
    error("Something is wrong!\n");
  }
}
