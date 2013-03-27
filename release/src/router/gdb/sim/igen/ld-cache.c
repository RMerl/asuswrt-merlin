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
#include "ld-cache.h"

#ifndef NULL
#define NULL 0
#endif


enum
{
  ca_type,
  ca_field_name,
  ca_derived_name,
  ca_type_def,
  ca_expression,
  nr_cache_rule_fields,
};

static const name_map cache_type_map[] = {
  {"cache", cache_value},
  {"compute", compute_value},
  {"scratch", scratch_value},
  {NULL, 0},
};


cache_entry *
load_cache_table (char *file_name)
{
  cache_entry *cache = NULL;
  cache_entry **last = &cache;
  table *file = table_open (file_name);
  table_entry *entry;
  while ((entry = table_read (file)) != NULL)
    {
      cache_entry *new_rule = ZALLOC (cache_entry);
      new_rule->line = entry->line;
      new_rule->entry_type = name2i (entry->field[ca_type], cache_type_map);
      new_rule->name = entry->field[ca_derived_name];
      filter_parse (&new_rule->original_fields, entry->field[ca_field_name]);
      new_rule->type = entry->field[ca_type_def];
      /* expression is the concatenation of the remaining fields */
      if (entry->nr_fields > ca_expression)
	{
	  int len = 0;
	  int chi;
	  for (chi = ca_expression; chi < entry->nr_fields; chi++)
	    {
	      len += strlen (" : ") + strlen (entry->field[chi]);
	    }
	  new_rule->expression = NZALLOC (char, len);
	  strcpy (new_rule->expression, entry->field[ca_expression]);
	  for (chi = ca_expression + 1; chi < entry->nr_fields; chi++)
	    {
	      strcat (new_rule->expression, " : ");
	      strcat (new_rule->expression, entry->field[chi]);
	    }
	}
      /* insert it */
      *last = new_rule;
      last = &new_rule->next;
    }
  return cache;
}



#ifdef MAIN

igen_options options;

int
main (int argc, char **argv)
{
  cache_entry *rules = NULL;
  lf *l;

  if (argc != 2)
    error (NULL, "Usage: cache <cache-file>\n");

  rules = load_cache_table (argv[1]);
  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-ld-insn");
  dump_cache_entries (l, "(", rules, ")\n");

  return 0;
}
#endif
