/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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

/* load the opcode stat structure */

#include "misc.h"
#include "lf.h"
#include "table.h"
#include "ld-decode.h"

#ifndef NULL
#define NULL 0
#endif

enum {
  op_options,
  op_first,
  op_last,
  op_force_first,
  op_force_last,
  op_force_expansion,
  op_special_mask,
  op_special_value,
  op_special_constant,
  nr_decode_fields,
};

static const name_map decode_type_map[] = {
  { "normal", normal_decode_rule },
  { "expand-forced", expand_forced_rule },
  { "boolean", boolean_rule },
  { NULL, normal_decode_rule },
};

static const name_map decode_gen_map[] = {
  { "array", array_gen },
  { "switch", switch_gen },
  { "padded-switch", padded_switch_gen },
  { "goto-switch", goto_switch_gen },
  { NULL, -1 },
};

static const name_map decode_slash_map[] = {
  { "variable-slash", 0 },
  { "constant-slash", 1 },
  { NULL },
};


static decode_gen_type overriding_gen_type = invalid_gen;

void
force_decode_gen_type(const char *type)
{
  overriding_gen_type = name2i(type, decode_gen_map);
}


decode_table *
load_decode_table(char *file_name,
		  int hi_bit_nr)
{
  table *file = table_open(file_name, nr_decode_fields, 0);
  table_entry *entry;
  decode_table *table = NULL;
  decode_table **curr_rule = &table;
  while ((entry = table_entry_read(file)) != NULL) {
    decode_table *new_rule = ZALLOC(decode_table);
    new_rule->type = name2i(entry->fields[op_options], decode_type_map);
    new_rule->gen = (overriding_gen_type != invalid_gen
		     ? overriding_gen_type
		     : name2i(entry->fields[op_options], decode_gen_map));
    new_rule->force_slash = name2i(entry->fields[op_options], decode_slash_map);
    new_rule->first = target_a2i(hi_bit_nr, entry->fields[op_first]);
    new_rule->last = target_a2i(hi_bit_nr, entry->fields[op_last]);
    new_rule->force_first = (strlen(entry->fields[op_force_first])
			     ? target_a2i(hi_bit_nr, entry->fields[op_force_first])
			     : new_rule->last + 1);
    new_rule->force_last = (strlen(entry->fields[op_force_last])
			    ? target_a2i(hi_bit_nr, entry->fields[op_force_last])
			    : new_rule->first - 1);
    new_rule->force_expansion = entry->fields[op_force_expansion];
    new_rule->special_mask = a2i(entry->fields[op_special_mask]);
    new_rule->special_value = a2i(entry->fields[op_special_value]);
    new_rule->special_constant = a2i(entry->fields[op_special_constant]);
    *curr_rule = new_rule;
    curr_rule = &new_rule->next;
  }
  return table;
}

  
void
dump_decode_rule(decode_table *rule,
		 int indent)
{
  dumpf(indent, "((decode_table*)%p\n", rule);
  if (rule) {
    dumpf(indent, " (type %s)\n", i2name(rule->type, decode_type_map));
    dumpf(indent, " (gen %s)\n", i2name(rule->gen, decode_gen_map));
    dumpf(indent, " (force_slash %d)\n", rule->force_slash);
    dumpf(indent, " (first %d)\n", rule->first);
    dumpf(indent, " (last %d)\n", rule->last);
    dumpf(indent, " (force_first %d)\n", rule->force_first);
    dumpf(indent, " (force_last %d)\n", rule->force_last);
    dumpf(indent, " (force_expansion \"%s\")\n", rule->force_expansion);
    dumpf(indent, " (special_mask 0x%x)\n", rule->special_mask);
    dumpf(indent, " (special_value 0x%x)\n", rule->special_value);
    dumpf(indent, " (special_constant 0x%x)\n", rule->special_constant);
    dumpf(indent, " (next 0x%x)\n", rule->next);
  }
  dumpf(indent, " )\n");
}


#ifdef MAIN

static void
dump_decode_rules(decode_table *rule,
		  int indent)
{
  while (rule) {
    dump_decode_rule(rule, indent);
    rule = rule->next;
  }
}

int
main(int argc, char **argv)
{
  decode_table *rules;
  if (argc != 3)
    error("Usage: decode <decode-file> <hi-bit-nr>\n");
  rules = load_decode_table(argv[1], a2i(argv[2]));
  dump_decode_rules(rules, 0);
  return 0;
}
#endif
