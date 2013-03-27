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
#include "ld-cache.h"

#ifndef NULL
#define NULL 0
#endif


enum {
  ca_type,
  ca_field_name,
  ca_derived_name,
  ca_type_def,
  ca_expression,
  nr_cache_rule_fields,
};

static const name_map cache_type_map[] = {
  { "cache", cache_value },
  { "compute", compute_value },
  { "scratch", scratch_value },
  { NULL, 0 },
};


void
append_cache_rule (cache_table **table, char *type, char *field_name,
		   char *derived_name, char *type_def,
		   char *expression, table_entry *file_entry)
{
  while ((*table) != NULL)
    table = &(*table)->next;
  (*table) = ZALLOC(cache_table);
  (*table)->type = name2i(type, cache_type_map);
  (*table)->field_name = field_name;
  (*table)->derived_name = derived_name;
  (*table)->type_def = (strlen(type_def) > 0 ? type_def : NULL);
  (*table)->expression = (strlen(expression) > 0 ? expression : NULL);
  (*table)->file_entry = file_entry;
  (*table)->next = NULL;
}


cache_table *
load_cache_table(char *file_name,
		 int hi_bit_nr)
{
  table *file = table_open(file_name, nr_cache_rule_fields, 0);
  table_entry *entry;
  cache_table *table = NULL;
  cache_table **curr_rule = &table;
  while ((entry = table_entry_read(file)) != NULL) {
    append_cache_rule (curr_rule, entry->fields[ca_type],
		       entry->fields[ca_field_name],
		       entry->fields[ca_derived_name],
		       entry->fields[ca_type_def],
		       entry->fields[ca_expression],
		       entry);
    curr_rule = &(*curr_rule)->next;
  }
  return table;
}



#ifdef MAIN

static void
dump_cache_rule(cache_table* rule,
		int indent)
{
  dumpf(indent, "((cache_table*)0x%x\n", rule);
  dumpf(indent, " (type %s)\n", i2name(rule->type, cache_type_map));
  dumpf(indent, " (field_name \"%s\")\n", rule->field_name);
  dumpf(indent, " (derived_name \"%s\")\n", rule->derived_name);
  dumpf(indent, " (type-def \"%s\")\n", rule->type_def);
  dumpf(indent, " (expression \"%s\")\n", rule->expression);
  dumpf(indent, " (next 0x%x)\n", rule->next);
  dumpf(indent, " )\n");
}


static void
dump_cache_rules(cache_table* rule,
		 int indent)
{
  while (rule) {
    dump_cache_rule(rule, indent);
    rule = rule->next;
  }
}


int
main(int argc, char **argv)
{
  cache_table *rules;
  if (argc != 3)
    error("Usage: cache <cache-file> <hi-bit-nr>\n");
  rules = load_cache_table(argv[1], a2i(argv[2]));
  dump_cache_rules(rules, 0);
  return 0;
}
#endif
