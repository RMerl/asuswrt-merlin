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


/* load the opcode stat structure */

#include "misc.h"
#include "lf.h"
#include "table.h"
#include "filter.h"

#include "igen.h"

#include "ld-decode.h"

#ifndef NULL
#define NULL 0
#endif


static const name_map decode_type_map[] = {
  {"normal", normal_decode_rule},
  {"boolean", boolean_rule},
  {NULL, normal_decode_rule},
};

static const name_map decode_gen_map[] = {
  {"array", array_gen},
  {"switch", switch_gen},
  {"padded-switch", padded_switch_gen},
  {"goto-switch", goto_switch_gen},
  {NULL, -1},
};

static const name_map decode_reserved_map[] = {
  {"zero-reserved", 1},
  {NULL, 0},
};

static const name_map decode_duplicates_map[] = {
  {"duplicate", 1},
  {NULL, 0},
};

static const name_map decode_combine_map[] = {
  {"combine", 1},
  {NULL, 0},
};

static const name_map decode_search_map[] = {
  {"constants", decode_find_constants},
  {"mixed", decode_find_mixed},
  {"strings", decode_find_strings},
  {NULL, decode_find_mixed},
};


static void
set_bits (int bit[max_insn_bit_size], unsigned64 value)
{
  int bit_nr;
  for (bit_nr = 0; bit_nr < max_insn_bit_size; bit_nr++)
    {
      if (bit_nr < options.insn_bit_size)
	bit[bit_nr] = (value >> (options.insn_bit_size - bit_nr - 1)) & 1;
      else
	bit[bit_nr] = 0;
    }
}

decode_table *
load_decode_table (char *file_name)
{
  table *file = table_open (file_name);
  table_entry *entry;
  decode_table *table = NULL;
  decode_table **curr_rule = &table;
  while ((entry = table_read (file)) != NULL)
    {
      char *decode_options = entry->field[decode_options_field];
      decode_table *new_rule = ZALLOC (decode_table);
      if (entry->nr_fields < min_nr_decode_fields)
	error (entry->line, "Missing decode table fields\n");
      new_rule->line = entry->line;

      /* the options field */
      new_rule->type = name2i (decode_options, decode_type_map);
      if (options.decode.overriding_gen != NULL)
	new_rule->gen =
	  name2i (options.decode.overriding_gen, decode_gen_map);
      else
	new_rule->gen = name2i (decode_options, decode_gen_map);
      if (new_rule->gen == padded_switch_gen && options.decode.switch_as_goto)
	new_rule->gen = goto_switch_gen;
      if (options.decode.zero_reserved)
	new_rule->with_zero_reserved = 1;
      else
	new_rule->with_zero_reserved =
	  name2i (decode_options, decode_reserved_map);
      if (options.decode.duplicate)
	new_rule->with_duplicates = 1;
      else
	new_rule->with_duplicates =
	  name2i (decode_options, decode_duplicates_map);
      if (options.decode.combine)
	new_rule->with_combine = 1;
      else
	new_rule->with_combine = name2i (decode_options, decode_combine_map);
      if (new_rule->type == boolean_rule)
	{
	  char *chp = decode_options;
	  while (*chp != '\0')
	    {
	      if (isdigit (*chp))
		{
		  new_rule->constant = a2i (chp);
		  break;
		}
	      chp = skip_to_separator (chp, ",");
	      chp = skip_spaces (chp);
	    }
	}

      /* First and last */
      if (entry->nr_fields > decode_first_field
	  && strlen (entry->field[decode_first_field]) > 0)
	{
	  new_rule->first = target_a2i (options.hi_bit_nr,
					entry->field[decode_first_field]);
	  if (new_rule->first < 0 || new_rule->first >= options.insn_bit_size)
	    error (new_rule->line, "First field out of range\n");
	}
      else
	new_rule->first = 0;
      if (entry->nr_fields > decode_last_field
	  && strlen (entry->field[decode_last_field]) > 0)
	{
	  new_rule->last = target_a2i (options.hi_bit_nr,
				       entry->field[decode_last_field]);
	  if (new_rule->last < 0 || new_rule->last >= options.insn_bit_size)
	    error (new_rule->line, "Last field out of range\n");
	}
      else
	new_rule->last = options.insn_bit_size - 1;
      if (new_rule->first > new_rule->last)
	error (new_rule->line, "First must preceed last\n");

      /* force first/last, with default values based on first/last */
      if (entry->nr_fields > decode_force_first_field
	  && strlen (entry->field[decode_force_first_field]) > 0)
	{
	  new_rule->force_first = target_a2i (options.hi_bit_nr,
					      entry->
					      field
					      [decode_force_first_field]);
	  if (new_rule->force_first < new_rule->first
	      || new_rule->force_first > new_rule->last + 1)
	    error (new_rule->line, "Force first out of range\n");
	}
      else
	new_rule->force_first = new_rule->last + 1;
      if (entry->nr_fields > decode_force_last_field
	  && strlen (entry->field[decode_force_last_field]) > 0)
	{
	  new_rule->force_last = target_a2i (options.hi_bit_nr,
					     entry->
					     field[decode_force_last_field]);
	  if (new_rule->force_last > new_rule->last
	      || new_rule->force_last < new_rule->first - 1)
	    error (new_rule->line, "Force-last out of range\n");
	}
      else
	new_rule->force_last = new_rule->first - 1;

      /* fields to be treated as constant */
      if (entry->nr_fields > decode_constant_field_names_field)
	filter_parse (&new_rule->constant_field_names,
		      entry->field[decode_constant_field_names_field]);

      /* applicable word nr */
      if (entry->nr_fields > decode_word_nr_field)
	new_rule->word_nr = a2i (entry->field[decode_word_nr_field]);

      /* required instruction format names */
      if (entry->nr_fields > decode_format_names_field)
	filter_parse (&new_rule->format_names,
		      entry->field[decode_format_names_field]);

      /* required processor models */
      if (entry->nr_fields > decode_model_names_field)
	filter_parse (&new_rule->model_names,
		      entry->field[decode_model_names_field]);

      /* required paths */
      if (entry->nr_fields > decode_paths_field
	  && strlen (entry->field[decode_paths_field]) > 0)
	{
	  decode_path_list **last = &new_rule->paths;
	  char *chp = entry->field[decode_paths_field];
	  do
	    {
	      (*last) = ZALLOC (decode_path_list);
	      /* extra root/zero entry */
	      (*last)->path = ZALLOC (decode_path);
	      do
		{
		  decode_path *entry = ZALLOC (decode_path);
		  entry->opcode_nr = a2i (chp);
		  entry->parent = (*last)->path;
		  (*last)->path = entry;
		  chp = skip_digits (chp);
		  chp = skip_spaces (chp);
		}
	      while (*chp == '.');
	      last = &(*last)->next;
	    }
	  while (*chp == ',');
	  if (*chp != '\0')
	    error (entry->line, "Invalid path field\n");
	}

      /* collect up the list of optional special conditions applicable
         to the rule */
      {
	int field_nr = nr_decode_fields;
	while (entry->nr_fields > field_nr)
	  {
	    decode_cond *cond = ZALLOC (decode_cond);
	    decode_cond **last;
	    if (entry->nr_fields > field_nr + decode_cond_mask_field)
	      set_bits (cond->mask,
			a2i (entry->
			     field[field_nr + decode_cond_mask_field]));
	    if (entry->nr_fields > field_nr + decode_cond_value_field)
	      {
		if (entry->field[field_nr + decode_cond_value_field][0] ==
		    '!')
		  {
		    cond->is_equal = 0;
		    set_bits (cond->value,
			      a2i (entry->
				   field[field_nr + decode_cond_value_field] +
				   1));
		  }
		else
		  {
		    cond->is_equal = 1;
		    set_bits (cond->value,
			      a2i (entry->
				   field[field_nr +
					 decode_cond_value_field]));
		  }
	      }
	    if (entry->nr_fields > field_nr + decode_cond_word_nr_field)
	      cond->word_nr =
		a2i (entry->field[field_nr + decode_cond_word_nr_field]);
	    field_nr += nr_decode_cond_fields;
	    /* insert it */
	    last = &new_rule->conditions;
	    while (*last != NULL)
	      last = &(*last)->next;
	    *last = cond;
	  }
      }
      *curr_rule = new_rule;
      curr_rule = &new_rule->next;
    }
  return table;
}


int
decode_table_max_word_nr (decode_table *entry)
{
  int max_word_nr = 0;
  while (entry != NULL)
    {
      decode_cond *cond;
      if (entry->word_nr > max_word_nr)
	max_word_nr = entry->word_nr;
      for (cond = entry->conditions; cond != NULL; cond = cond->next)
	{
	  if (cond->word_nr > max_word_nr)
	    max_word_nr = cond->word_nr;
	}
      entry = entry->next;
    }
  return max_word_nr;
}



static void
dump_decode_cond (lf *file, char *prefix, decode_cond *cond, char *suffix)
{
  lf_printf (file, "%s(decode_cond *) 0x%lx", prefix, (long) cond);
  if (cond != NULL)
    {
      lf_indent (file, +1);
      lf_printf (file, "\n(word_nr %d)", cond->word_nr);
      lf_printf (file, "\n(mask 0x%lx)", (long) cond->mask);
      lf_printf (file, "\n(value 0x%lx)", (long) cond->value);
      lf_printf (file, "\n(is_equal 0x%lx)", (long) cond->is_equal);
      lf_printf (file, "\n(next (decode_cond *) 0%lx)", (long) cond->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


static void
dump_decode_conds (lf *file, char *prefix, decode_cond *cond, char *suffix)
{
  lf_printf (file, "%s(decode_cond *) 0x%lx", prefix, (long) cond);
  while (cond != NULL)
    {
      dump_decode_cond (file, "\n(", cond, ")");
      cond = cond->next;
    }
  lf_printf (file, "%s", suffix);
}


void
dump_decode_rule (lf *file, char *prefix, decode_table *rule, char *suffix)
{
  lf_printf (file, "%s(decode_table *) 0x%lx", prefix, (long) rule);
  if (rule != NULL)
    {
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", rule->line, ")");
      lf_printf (file, "\n(type %s)", i2name (rule->type, decode_type_map));
      lf_printf (file, "\n(gen %s)", i2name (rule->gen, decode_gen_map));
      lf_printf (file, "\n(first %d)", rule->first);
      lf_printf (file, "\n(last %d)", rule->last);
      lf_printf (file, "\n(force_first %d)", rule->force_first);
      lf_printf (file, "\n(force_last %d)", rule->force_last);
      dump_filter (file, "\n(constant_field_names \"",
		   rule->constant_field_names, "\")");
      lf_printf (file, "\n(constant 0x%x)", rule->constant);
      lf_printf (file, "\n(word_nr %d)", rule->word_nr);
      lf_printf (file, "\n(with_zero_reserved %d)", rule->with_zero_reserved);
      lf_printf (file, "\n(with_duplicates %d)", rule->with_duplicates);
      lf_printf (file, "\n(with_combine %d)", rule->with_combine);
      dump_filter (file, "\n(format_names \"", rule->format_names, "\")");
      dump_filter (file, "\n(model_names \"", rule->model_names, "\")");
      dump_decode_conds (file, "\n(conditions ", rule->conditions, ")");
      lf_printf (file, "\n(next 0x%lx)", (long) rule->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


#ifdef MAIN

static void
dump_decode_rules (lf *file, char *prefix, decode_table *rule, char *suffix)
{
  lf_printf (file, "%s", prefix);
  while (rule != NULL)
    {
      lf_indent (file, +1);
      dump_decode_rule (file, "\n(", rule, ")");
      lf_indent (file, -1);
      rule = rule->next;
    }
  lf_printf (file, "%s", suffix);
}

igen_options options;

int
main (int argc, char **argv)
{
  lf *l;
  decode_table *rules;

  INIT_OPTIONS (options);

  if (argc != 3)
    error (NULL, "Usage: decode <decode-file> <hi-bit-nr>\n");

  options.hi_bit_nr = a2i (argv[2]);
  rules = load_decode_table (argv[1]);
  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-ld-insn");
  dump_decode_rules (l, "(rules ", rules, ")\n");

  return 0;
}
#endif
