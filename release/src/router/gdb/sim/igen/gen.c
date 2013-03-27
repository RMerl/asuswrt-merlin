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

static insn_uint
sub_val (insn_uint val, int val_last_pos, int first_pos, int last_pos)
{
  return ((val >> (val_last_pos - last_pos))
	  & (((insn_uint) 1 << (last_pos - first_pos + 1)) - 1));
}

static void
update_depth (lf *file, gen_entry *entry, int depth, void *data)
{
  int *max_depth = (int *) data;
  if (*max_depth < depth)
    *max_depth = depth;
}


int
gen_entry_depth (gen_entry *table)
{
  int depth = 0;
  gen_entry_traverse_tree (NULL, table, 1, NULL,	/*start */
			   update_depth, NULL,	/*end */
			   &depth);	/* data */
  return depth;
}


static void
print_gen_entry_path (line_ref *line, gen_entry *table, error_func *print)
{
  if (table->parent == NULL)
    {
      if (table->top->model != NULL)
	print (line, "%s", table->top->model->name);
      else
	print (line, "");
    }
  else
    {
      print_gen_entry_path (line, table->parent, print);
      print (NULL, ".%d", table->opcode_nr);
    }
}

static void
print_gen_entry_insns (gen_entry *table,
		       error_func *print,
		       char *first_message, char *next_message)
{
  insn_list *i;
  char *message;
  message = first_message;
  for (i = table->insns; i != NULL; i = i->next)
    {
      insn_entry *insn = i->insn;
      print_gen_entry_path (insn->line, table, print);
      print (NULL, ": %s.%s %s\n", insn->format_name, insn->name, message);
      if (next_message != NULL)
	message = next_message;
    }
}

/* same as strcmp */
static int
insn_field_cmp (insn_word_entry *l, insn_word_entry *r)
{
  while (1)
    {
      int bit_nr;
      if (l == NULL && r == NULL)
	return 0;		/* all previous fields the same */
      if (l == NULL)
	return -1;		/* left shorter than right */
      if (r == NULL)
	return +1;		/* left longer than right */
      for (bit_nr = 0; bit_nr < options.insn_bit_size; bit_nr++)
	{
	  if (l->bit[bit_nr]->field->type != insn_field_string)
	    continue;
	  if (r->bit[bit_nr]->field->type != insn_field_string)
	    continue;
	  if (l->bit[bit_nr]->field->conditions == NULL)
	    continue;
	  if (r->bit[bit_nr]->field->conditions == NULL)
	    continue;
	  if (0)
	    printf ("%s%s%s VS %s%s%s\n",
		    l->bit[bit_nr]->field->val_string,
		    l->bit[bit_nr]->field->conditions->test ==
		    insn_field_cond_eq ? "=" : "!",
		    l->bit[bit_nr]->field->conditions->string,
		    r->bit[bit_nr]->field->val_string,
		    r->bit[bit_nr]->field->conditions->test ==
		    insn_field_cond_eq ? "=" : "!",
		    r->bit[bit_nr]->field->conditions->string);
	  if (l->bit[bit_nr]->field->conditions->test == insn_field_cond_eq
	      && r->bit[bit_nr]->field->conditions->test ==
	      insn_field_cond_eq)
	    {
	      if (l->bit[bit_nr]->field->conditions->type ==
		  insn_field_cond_field
		  && r->bit[bit_nr]->field->conditions->type ==
		  insn_field_cond_field)
		/* somewhat arbitrary */
		{
		  int cmp = strcmp (l->bit[bit_nr]->field->conditions->string,
				    r->bit[bit_nr]->field->conditions->
				    string);
		  if (cmp != 0)
		    return cmp;
		  else
		    continue;
		}
	      if (l->bit[bit_nr]->field->conditions->type ==
		  insn_field_cond_field)
		return +1;
	      if (r->bit[bit_nr]->field->conditions->type ==
		  insn_field_cond_field)
		return -1;
	      /* The case of both fields having constant values should have
	         already have been handled because such fields are converted
	         into normal constant fields. */
	      continue;
	    }
	  if (l->bit[bit_nr]->field->conditions->test == insn_field_cond_eq)
	    return +1;		/* left = only */
	  if (r->bit[bit_nr]->field->conditions->test == insn_field_cond_eq)
	    return -1;		/* right = only */
	  /* FIXME: Need to some what arbitrarily order conditional lists */
	  continue;
	}
      l = l->next;
      r = r->next;
    }
}

/* same as strcmp */
static int
insn_word_cmp (insn_word_entry *l, insn_word_entry *r)
{
  while (1)
    {
      int bit_nr;
      if (l == NULL && r == NULL)
	return 0;		/* all previous fields the same */
      if (l == NULL)
	return -1;		/* left shorter than right */
      if (r == NULL)
	return +1;		/* left longer than right */
      for (bit_nr = 0; bit_nr < options.insn_bit_size; bit_nr++)
	{
	  if (l->bit[bit_nr]->mask < r->bit[bit_nr]->mask)
	    return -1;
	  if (l->bit[bit_nr]->mask > r->bit[bit_nr]->mask)
	    return 1;
	  if (l->bit[bit_nr]->value < r->bit[bit_nr]->value)
	    return -1;
	  if (l->bit[bit_nr]->value > r->bit[bit_nr]->value)
	    return 1;
	}
      l = l->next;
      r = r->next;
    }
}

/* same as strcmp */
static int
opcode_bit_cmp (opcode_bits *l, opcode_bits *r)
{
  if (l == NULL && r == NULL)
    return 0;			/* all previous bits the same */
  if (l == NULL)
    return -1;			/* left shorter than right */
  if (r == NULL)
    return +1;			/* left longer than right */
  /* most significant word */
  if (l->field->word_nr < r->field->word_nr)
    return +1;			/* left has more significant word */
  if (l->field->word_nr > r->field->word_nr)
    return -1;			/* right has more significant word */
  /* most significant bit? */
  if (l->first < r->first)
    return +1;			/* left as more significant bit */
  if (l->first > r->first)
    return -1;			/* right as more significant bit */
  /* nr bits? */
  if (l->last < r->last)
    return +1;			/* left as less bits */
  if (l->last > r->last)
    return -1;			/* right as less bits */
  /* value? */
  if (l->value < r->value)
    return -1;
  if (l->value > r->value)
    return 1;
  return 0;
}


/* same as strcmp */
static int
opcode_bits_cmp (opcode_bits *l, opcode_bits *r)
{
  while (1)
    {
      int cmp;
      if (l == NULL && r == NULL)
	return 0;		/* all previous bits the same */
      cmp = opcode_bit_cmp (l, r);
      if (cmp != 0)
	return cmp;
      l = l->next;
      r = r->next;
    }
}

/* same as strcmp */
static opcode_bits *
new_opcode_bits (opcode_bits *old_bits,
		 int value,
		 int first,
		 int last, insn_field_entry *field, opcode_field *opcode)
{
  opcode_bits *new_bits = ZALLOC (opcode_bits);
  new_bits->field = field;
  new_bits->value = value;
  new_bits->first = first;
  new_bits->last = last;
  new_bits->opcode = opcode;

  if (old_bits != NULL)
    {
      opcode_bits *new_list;
      opcode_bits **last = &new_list;
      new_list = new_opcode_bits (old_bits->next,
				  old_bits->value,
				  old_bits->first,
				  old_bits->last,
				  old_bits->field, old_bits->opcode);
      while (*last != NULL)
	{
	  int cmp = opcode_bit_cmp (new_bits, *last);
	  if (cmp < 0)		/* new < new_list */
	    {
	      break;
	    }
	  if (cmp == 0)
	    {
	      ERROR ("Duplicated insn bits in list");
	    }
	  last = &(*last)->next;
	}
      new_bits->next = *last;
      *last = new_bits;
      return new_list;
    }
  else
    {
      return new_bits;
    }
}

/* Same as strcmp().  */
static int
name_cmp (const char *l, const char *r)
{
  if (l == NULL && r == NULL)
    return 0;
  if (l != NULL && r == NULL)
    return -1;
  if (l == NULL && r != NULL)
    return +1;
  return strcmp (l, r);
}


typedef enum
{
  merge_duplicate_insns,
  report_duplicate_insns,
}
duplicate_insn_actions;

static insn_list *
insn_list_insert (insn_list **cur_insn_ptr,
		  int *nr_insns,
		  insn_entry * insn,
		  opcode_bits *expanded_bits,
		  opcode_field *opcodes,
		  int nr_prefetched_words,
		  duplicate_insn_actions duplicate_action)
{
  /* insert it according to the order of the fields & bits */
  for (; (*cur_insn_ptr) != NULL; cur_insn_ptr = &(*cur_insn_ptr)->next)
    {
      int cmp;

      /* key#1 sort according to the constant fields of each instruction */
      cmp = insn_word_cmp (insn->words, (*cur_insn_ptr)->insn->words);
      if (cmp < 0)
	break;
      else if (cmp > 0)
	continue;

      /* key#2 sort according to the expanded bits of each instruction */
      cmp = opcode_bits_cmp (expanded_bits, (*cur_insn_ptr)->expanded_bits);
      if (cmp < 0)
	break;
      else if (cmp > 0)
	continue;

      /* key#3 sort according to the non-constant fields of each instruction */
      cmp = insn_field_cmp (insn->words, (*cur_insn_ptr)->insn->words);
      if (cmp < 0)
	break;
      else if (cmp > 0)
	continue;

      if (duplicate_action == merge_duplicate_insns)
	{
	  /* key#4: If we're going to merge duplicates, also sort
	     according to the format_name.  Two instructions with
	     identical decode patterns, but different names, are
	     considered different when merging.  Duplicates are only
	     important when creating a decode table (implied by
	     report_duplicate_insns) as such a table only has the
	     instruction's bit code as a way of differentiating
	     between instructions.  */
	  int cmp = name_cmp (insn->format_name,
			      (*cur_insn_ptr)->insn->format_name);
	  if (cmp < 0)
	    break;
	  else if (cmp > 0)
	    continue;
	}

      if (duplicate_action == merge_duplicate_insns)
	{
	  /* key#5: If we're going to merge duplicates, also sort
	     according to the name.  See comment above for
	     format_name.  */
	  int cmp = name_cmp (insn->name, (*cur_insn_ptr)->insn->name);
	  if (cmp < 0)
	    break;
	  else if (cmp > 0)
	    continue;
	}

      /* duplicate keys, report problem */
      switch (duplicate_action)
	{
	case report_duplicate_insns:
	  /* It would appear that we have two instructions with the
	     same constant field values across all words and bits.
	     This error can also occure when insn_field_cmp() is
	     failing to differentiate between two instructions that
	     differ only in their conditional fields. */
	  warning (insn->line,
		   "Two instructions with identical constant fields\n");
	  error ((*cur_insn_ptr)->insn->line,
		 "Location of duplicate instruction\n");
	case merge_duplicate_insns:
	  /* Add the opcode path to the instructions list */
	  if (options.trace.insn_insertion)
	    {
	      notify ((*cur_insn_ptr)->insn->line,
		      "%s.%s: insert merge %s.%s\n",
		      (*cur_insn_ptr)->insn->format_name,
		      (*cur_insn_ptr)->insn->name,
		      insn->format_name,
		      insn->name);
	    }
	  if (opcodes != NULL)
	    {
	      insn_opcodes **last = &(*cur_insn_ptr)->opcodes;
	      while (*last != NULL)
		{
		  last = &(*last)->next;
		}
	      (*last) = ZALLOC (insn_opcodes);
	      (*last)->opcode = opcodes;
	    }
	  /* Use the larger nr_prefetched_words */
	  if ((*cur_insn_ptr)->nr_prefetched_words < nr_prefetched_words)
	    (*cur_insn_ptr)->nr_prefetched_words = nr_prefetched_words;
	  return (*cur_insn_ptr);
	}

    }

  /* create a new list entry and insert it */
  {
    insn_list *new_insn = ZALLOC (insn_list);
    if (options.trace.insn_insertion)
      {
	notify (insn->line,
		"%s.%s: insert new\n",
		insn->format_name,
		insn->name);
      }
    new_insn->insn = insn;
    new_insn->expanded_bits = expanded_bits;
    new_insn->next = (*cur_insn_ptr);
    new_insn->nr_prefetched_words = nr_prefetched_words;
    if (opcodes != NULL)
      {
	new_insn->opcodes = ZALLOC (insn_opcodes);
	new_insn->opcodes->opcode = opcodes;
      }
    (*cur_insn_ptr) = new_insn;
  }

  *nr_insns += 1;

  return (*cur_insn_ptr);
}


extern void
gen_entry_traverse_tree (lf *file,
			 gen_entry *table,
			 int depth,
			 gen_entry_handler * start,
			 gen_entry_handler * leaf,
			 gen_entry_handler * end, void *data)
{
  gen_entry *entry;

  ASSERT (table !=NULL);
  ASSERT (table->opcode != NULL);
  ASSERT (table->nr_entries > 0);
  ASSERT (table->entries != 0);

  /* prefix */
  if (start != NULL && depth >= 0)
    {
      start (file, table, depth, data);
    }
  /* infix leaves */
  for (entry = table->entries; entry != NULL; entry = entry->sibling)
    {
      if (entry->entries != NULL && depth != 0)
	{
	  gen_entry_traverse_tree (file, entry, depth + 1,
				   start, leaf, end, data);
	}
      else if (depth >= 0)
	{
	  if (leaf != NULL)
	    {
	      leaf (file, entry, depth, data);
	    }
	}
    }
  /* postfix */
  if (end != NULL && depth >= 0)
    {
      end (file, table, depth, data);
    }
}



/* create a list element containing a single gen_table entry */

static gen_list *
make_table (insn_table *isa, decode_table *rules, model_entry *model)
{
  insn_entry *insn;
  gen_list *entry = ZALLOC (gen_list);
  entry->table = ZALLOC (gen_entry);
  entry->table->top = entry;
  entry->model = model;
  entry->isa = isa;
  for (insn = isa->insns; insn != NULL; insn = insn->next)
    {
      if (model == NULL
	  || insn->processors == NULL
	  || filter_is_member (insn->processors, model->name))
	{
	  insn_list_insert (&entry->table->insns, &entry->table->nr_insns, insn, NULL,	/* expanded_bits - none yet */
			    NULL,	/* opcodes - none yet */
			    0,	/* nr_prefetched_words - none yet */
			    report_duplicate_insns);
	}
    }
  entry->table->opcode_rule = rules;
  return entry;
}


gen_table *
make_gen_tables (insn_table *isa, decode_table *rules)
{
  gen_table *gen = ZALLOC (gen_table);
  gen->isa = isa;
  gen->rules = rules;
  if (options.gen.multi_sim)
    {
      gen_list **last = &gen->tables;
      model_entry *model;
      filter *processors;
      if (options.model_filter != NULL)
	processors = options.model_filter;
      else
	processors = isa->model->processors;
      for (model = isa->model->models; model != NULL; model = model->next)
	{
	  if (filter_is_member (processors, model->name))
	    {
	      *last = make_table (isa, rules, model);
	      last = &(*last)->next;
	    }
	}
    }
  else
    {
      gen->tables = make_table (isa, rules, NULL);
    }
  return gen;
}


/****************************************************************/

#if 0
typedef enum
{
  field_is_not_constant = 0,
  field_constant_int = 1,
  field_constant_reserved = 2,
  field_constant_string = 3
}
constant_field_types;

static constant_field_types
insn_field_is_constant (insn_field * field, decode_table *rule)
{
  switch (field->type)
    {
    case insn_field_int:
      /* field is an integer */
      return field_constant_int;
    case insn_field_reserved:
      /* field is `/' and treating that as a constant */
      if (rule->with_zero_reserved)
	return field_constant_reserved;
      else
	return field_is_not_constant;
    case insn_field_wild:
      return field_is_not_constant;	/* never constant */
    case insn_field_string:
      /* field, though variable, is on the list of forced constants */
      if (filter_is_member (rule->constant_field_names, field->val_string))
	return field_constant_string;
      else
	return field_is_not_constant;
    }
  ERROR ("Internal error");
  return field_is_not_constant;
}
#endif


/****************************************************************/


/* Is the bit, according to the decode rule, identical across all the
   instructions? */
static int
insns_bit_useless (insn_list *insns, decode_table *rule, int bit_nr)
{
  insn_list *entry;
  int value = -1;
  int is_useless = 1;		/* cleared if something actually found */

  /* check the instructions for some constant value in at least one of
     the bit fields */
  for (entry = insns; entry != NULL; entry = entry->next)
    {
      insn_word_entry *word = entry->insn->word[rule->word_nr];
      insn_bit_entry *bit = word->bit[bit_nr];
      switch (bit->field->type)
	{
	case insn_field_invalid:
	  ASSERT (0);
	  break;
	case insn_field_wild:
	case insn_field_reserved:
	  /* neither useless or useful - ignore */
	  break;
	case insn_field_int:
	  switch (rule->search)
	    {
	    case decode_find_strings:
	      /* an integer isn't a string */
	      return 1;
	    case decode_find_constants:
	    case decode_find_mixed:
	      /* an integer is useful if its value isn't the same
	         between all instructions.  The first time through the
	         value is saved, the second time through (if the
	         values differ) it is marked as useful. */
	      if (value < 0)
		value = bit->value;
	      else if (value != bit->value)
		is_useless = 0;
	      break;
	    }
	  break;
	case insn_field_string:
	  switch (rule->search)
	    {
	    case decode_find_strings:
	      /* at least one string, keep checking */
	      is_useless = 0;
	      break;
	    case decode_find_constants:
	    case decode_find_mixed:
	      if (filter_is_member (rule->constant_field_names,
				    bit->field->val_string))
		/* a string field forced to constant? */
		is_useless = 0;
	      else if (rule->search == decode_find_constants)
		/* the string field isn't constant */
		return 1;
	      break;
	    }
	}
    }

  /* Given only one constant value has been found, check through all
     the instructions to see if at least one conditional makes it
     usefull */
  if (value >= 0 && is_useless)
    {
      for (entry = insns; entry != NULL; entry = entry->next)
	{
	  insn_word_entry *word = entry->insn->word[rule->word_nr];
	  insn_bit_entry *bit = word->bit[bit_nr];
	  switch (bit->field->type)
	    {
	    case insn_field_invalid:
	      ASSERT (0);
	      break;
	    case insn_field_wild:
	    case insn_field_reserved:
	    case insn_field_int:
	      /* already processed */
	      break;
	    case insn_field_string:
	      switch (rule->search)
		{
		case decode_find_strings:
		case decode_find_constants:
		  /* already processed */
		  break;
		case decode_find_mixed:
		  /* string field with conditions.  If this condition
		     eliminates the value then the compare is useful */
		  if (bit->field->conditions != NULL)
		    {
		      insn_field_cond *condition;
		      int shift = bit->field->last - bit_nr;
		      for (condition = bit->field->conditions;
			   condition != NULL; condition = condition->next)
			{
			  switch (condition->type)
			    {
			    case insn_field_cond_value:
			      switch (condition->test)
				{
				case insn_field_cond_ne:
				  if (((condition->value >> shift) & 1)
				      == (unsigned) value)
				    /* conditional field excludes the
				       current value */
				    is_useless = 0;
				  break;
				case insn_field_cond_eq:
				  if (((condition->value >> shift) & 1)
				      != (unsigned) value)
				    /* conditional field requires the
				       current value */
				    is_useless = 0;
				  break;
				}
			      break;
			    case insn_field_cond_field:
			      /* are these handled separatly? */
			      break;
			    }
			}
		    }
		}
	    }
	}
    }

  return is_useless;
}


/* go through a gen-table's list of instruction formats looking for a
   range of bits that meet the decode table RULEs requirements */

static opcode_field *
gen_entry_find_opcode_field (insn_list *insns,
			     decode_table *rule, int string_only)
{
  opcode_field curr_opcode;
  ASSERT (rule != NULL);

  memset (&curr_opcode, 0, sizeof (curr_opcode));
  curr_opcode.word_nr = rule->word_nr;
  curr_opcode.first = rule->first;
  curr_opcode.last = rule->last;

  /* Try to reduce the size of first..last in accordance with the
     decode rules */

  while (curr_opcode.first <= rule->last)
    {
      if (insns_bit_useless (insns, rule, curr_opcode.first))
	curr_opcode.first++;
      else
	break;
    }
  while (curr_opcode.last >= rule->first)
    {
      if (insns_bit_useless (insns, rule, curr_opcode.last))
	curr_opcode.last--;
      else
	break;
    }


#if 0
  for (entry = insns; entry != NULL; entry = entry->next)
    {
      insn_word_entry *fields = entry->insn->word[rule->word_nr];
      opcode_field new_opcode;

      ASSERT (fields != NULL);

      /* find a start point for the opcode field */
      new_opcode.first = rule->first;
      while (new_opcode.first <= rule->last
	     && (!string_only
		 ||
		 (insn_field_is_constant (fields->bit[new_opcode.first], rule)
		  != field_constant_string)) && (string_only
						 ||
						 (insn_field_is_constant
						  (fields->
						   bit[new_opcode.first],
						   rule) ==
						  field_is_not_constant)))
	{
	  int new_first = fields->bit[new_opcode.first]->last + 1;
	  ASSERT (new_first > new_opcode.first);
	  new_opcode.first = new_first;
	}
      ASSERT (new_opcode.first > rule->last
	      || (string_only
		  && insn_field_is_constant (fields->bit[new_opcode.first],
					     rule) == field_constant_string)
	      || (!string_only
		  && insn_field_is_constant (fields->bit[new_opcode.first],
					     rule)));

      /* find the end point for the opcode field */
      new_opcode.last = rule->last;
      while (new_opcode.last >= rule->first
	     && (!string_only
		 || insn_field_is_constant (fields->bit[new_opcode.last],
					    rule) != field_constant_string)
	     && (string_only
		 || !insn_field_is_constant (fields->bit[new_opcode.last],
					     rule)))
	{
	  int new_last = fields->bit[new_opcode.last]->first - 1;
	  ASSERT (new_last < new_opcode.last);
	  new_opcode.last = new_last;
	}
      ASSERT (new_opcode.last < rule->first
	      || (string_only
		  && insn_field_is_constant (fields->bit[new_opcode.last],
					     rule) == field_constant_string)
	      || (!string_only
		  && insn_field_is_constant (fields->bit[new_opcode.last],
					     rule)));

      /* now see if our current opcode needs expanding to include the
         interesting fields within this instruction */
      if (new_opcode.first <= rule->last
	  && curr_opcode.first > new_opcode.first)
	curr_opcode.first = new_opcode.first;
      if (new_opcode.last >= rule->first
	  && curr_opcode.last < new_opcode.last)
	curr_opcode.last = new_opcode.last;

    }
#endif

  /* did the final opcode field end up being empty? */
  if (curr_opcode.first > curr_opcode.last)
    {
      return NULL;
    }
  ASSERT (curr_opcode.last >= rule->first);
  ASSERT (curr_opcode.first <= rule->last);
  ASSERT (curr_opcode.first <= curr_opcode.last);

  /* Ensure that, for the non string only case, the opcode includes
     the range forced_first .. forced_last */
  if (!string_only && curr_opcode.first > rule->force_first)
    {
      curr_opcode.first = rule->force_first;
    }
  if (!string_only && curr_opcode.last < rule->force_last)
    {
      curr_opcode.last = rule->force_last;
    }

  /* For the string only case, force just the lower bound (so that the
     shift can be eliminated) */
  if (string_only && rule->force_last == options.insn_bit_size - 1)
    {
      curr_opcode.last = options.insn_bit_size - 1;
    }

  /* handle any special cases */
  switch (rule->type)
    {
    case normal_decode_rule:
      /* let the above apply */
      curr_opcode.nr_opcodes =
	(1 << (curr_opcode.last - curr_opcode.first + 1));
      break;
    case boolean_rule:
      curr_opcode.is_boolean = 1;
      curr_opcode.boolean_constant = rule->constant;
      curr_opcode.nr_opcodes = 2;
      break;
    }

  {
    opcode_field *new_field = ZALLOC (opcode_field);
    memcpy (new_field, &curr_opcode, sizeof (opcode_field));
    return new_field;
  }
}


static void
gen_entry_insert_insn (gen_entry *table,
		       insn_entry * old_insn,
		       int new_word_nr,
		       int new_nr_prefetched_words,
		       int new_opcode_nr, opcode_bits *new_bits)
{
  gen_entry **entry = &table->entries;

  /* find the new table for this entry */
  while ((*entry) != NULL && (*entry)->opcode_nr < new_opcode_nr)
    {
      entry = &(*entry)->sibling;
    }

  if ((*entry) == NULL || (*entry)->opcode_nr != new_opcode_nr)
    {
      /* insert the missing entry */
      gen_entry *new_entry = ZALLOC (gen_entry);
      new_entry->sibling = (*entry);
      (*entry) = new_entry;
      table->nr_entries++;
      /* fill it in */
      new_entry->top = table->top;
      new_entry->opcode_nr = new_opcode_nr;
      new_entry->word_nr = new_word_nr;
      new_entry->expanded_bits = new_bits;
      new_entry->opcode_rule = table->opcode_rule->next;
      new_entry->parent = table;
      new_entry->nr_prefetched_words = new_nr_prefetched_words;
    }
  /* ASSERT new_bits == cur_entry bits */
  ASSERT ((*entry) != NULL && (*entry)->opcode_nr == new_opcode_nr);
  insn_list_insert (&(*entry)->insns, &(*entry)->nr_insns, old_insn, NULL,	/* expanded_bits - only in final list */
		    NULL,	/* opcodes - only in final list */
		    new_nr_prefetched_words,	/* for this table */
		    report_duplicate_insns);
}


static void
gen_entry_expand_opcode (gen_entry *table,
			 insn_entry * instruction,
			 int bit_nr, int opcode_nr, opcode_bits *bits)
{
  if (bit_nr > table->opcode->last)
    {
      /* Only include the hardwired bit information with an entry IF
         that entry (and hence its functions) are being duplicated.  */
      if (options.trace.insn_expansion)
	{
	  print_gen_entry_path (table->opcode_rule->line, table, notify);
	  notify (NULL, ": insert %d - %s.%s%s\n",
		  opcode_nr,
		  instruction->format_name,
		  instruction->name,
		  (table->opcode_rule->
		   with_duplicates ? " (duplicated)" : ""));
	}
      if (table->opcode_rule->with_duplicates)
	{
	  gen_entry_insert_insn (table, instruction,
				 table->opcode->word_nr,
				 table->nr_prefetched_words, opcode_nr, bits);
	}
      else
	{
	  gen_entry_insert_insn (table, instruction,
				 table->opcode->word_nr,
				 table->nr_prefetched_words, opcode_nr, NULL);
	}
    }
  else
    {
      insn_word_entry *word = instruction->word[table->opcode->word_nr];
      insn_field_entry *field = word->bit[bit_nr]->field;
      int last_pos = ((field->last < table->opcode->last)
		      ? field->last : table->opcode->last);
      int first_pos = ((field->first > table->opcode->first)
		       ? field->first : table->opcode->first);
      int width = last_pos - first_pos + 1;
      switch (field->type)
	{
	case insn_field_int:
	  {
	    int val;
	    val = sub_val (field->val_int, field->last, first_pos, last_pos);
	    gen_entry_expand_opcode (table, instruction,
				     last_pos + 1,
				     ((opcode_nr << width) | val), bits);
	    break;
	  }
	default:
	  {
	    if (field->type == insn_field_reserved)
	      gen_entry_expand_opcode (table, instruction,
				       last_pos + 1,
				       ((opcode_nr << width)), bits);
	    else
	      {
		int val;
		int last_val = (table->opcode->is_boolean ? 2 : (1 << width));
		for (val = 0; val < last_val; val++)
		  {
		    /* check to see if the value has been precluded
		       (by a conditional) in some way */
		    int is_precluded;
		    insn_field_cond *condition;
		    for (condition = field->conditions, is_precluded = 0;
			 condition != NULL && !is_precluded;
			 condition = condition->next)
		      {
			switch (condition->type)
			  {
			  case insn_field_cond_value:
			    {
			      int value =
				sub_val (condition->value, field->last,
					 first_pos, last_pos);
			      switch (condition->test)
				{
				case insn_field_cond_ne:
				  if (value == val)
				    is_precluded = 1;
				  break;
				case insn_field_cond_eq:
				  if (value != val)
				    is_precluded = 1;
				  break;
				}
			      break;
			    }
			  case insn_field_cond_field:
			    {
			      int value = -1;
			      opcode_bits *bit;
			      gen_entry *t = NULL;
			      /* Try to find a value for the
			         conditional by looking back through
			         the previously defined bits for one
			         that covers the designated
			         conditional field */
			      for (bit = bits; bit != NULL; bit = bit->next)
				{
				  if (bit->field->word_nr ==
				      condition->field->word_nr
				      && bit->first <= condition->field->first
				      && bit->last >= condition->field->last)
				    {
				      /* the bit field fully specified
				         the conditional field's value */
				      value = sub_val (bit->value, bit->last,
						       condition->field->
						       first,
						       condition->field->
						       last);
				    }
				}
			      /* Try to find a value by looking
			         through this and previous tables */
			      if (bit == NULL)
				{
				  for (t = table;
				       t->parent != NULL; t = t->parent)
				    {
				      if (t->parent->opcode->word_nr ==
					  condition->field->word_nr
					  && t->parent->opcode->first <=
					  condition->field->first
					  && t->parent->opcode->last >=
					  condition->field->last)
					{
					  /* the table entry fully
					     specified the condition
					     field's value */
					  /* extract the field's value
					     from the opcode */
					  value =
					    sub_val (t->opcode_nr,
						     t->parent->opcode->last,
						     condition->field->first,
						     condition->field->last);
					  /* this is a requirement of
					     a conditonal field
					     refering to another field */
					  ASSERT ((condition->field->first -
						   condition->field->last) ==
						  (first_pos - last_pos));
					  printf
					    ("value=%d, opcode_nr=%d, last=%d, [%d..%d]\n",
					     value, t->opcode_nr,
					     t->parent->opcode->last,
					     condition->field->first,
					     condition->field->last);
					}
				    }
				}
			      if (bit == NULL && t == NULL)
				error (instruction->line,
				       "Conditional `%s' of field `%s' isn't expanded",
				       condition->string, field->val_string);
			      switch (condition->test)
				{
				case insn_field_cond_ne:
				  if (value == val)
				    is_precluded = 1;
				  break;
				case insn_field_cond_eq:
				  if (value != val)
				    is_precluded = 1;
				  break;
				}
			      break;
			    }
			  }
		      }
		    if (!is_precluded)
		      {
			/* Only add additional hardwired bit
			   information if the entry is not going to
			   later be combined */
			if (table->opcode_rule->with_combine)
			  {
			    gen_entry_expand_opcode (table, instruction,
						     last_pos + 1,
						     ((opcode_nr << width) |
						      val), bits);
			  }
			else
			  {
			    opcode_bits *new_bits =
			      new_opcode_bits (bits, val,
					       first_pos, last_pos,
					       field,
					       table->opcode);
			    gen_entry_expand_opcode (table, instruction,
						     last_pos + 1,
						     ((opcode_nr << width) |
						      val), new_bits);
			  }
		      }
		  }
	      }
	  }
	}
    }
}

static void
gen_entry_insert_expanding (gen_entry *table, insn_entry * instruction)
{
  gen_entry_expand_opcode (table,
			   instruction,
			   table->opcode->first, 0, table->expanded_bits);
}


static int
insns_match_format_names (insn_list *insns, filter *format_names)
{
  if (format_names != NULL)
    {
      insn_list *i;
      for (i = insns; i != NULL; i = i->next)
	{
	  if (i->insn->format_name != NULL
	      && !filter_is_member (format_names, i->insn->format_name))
	    return 0;
	}
    }
  return 1;
}

static int
table_matches_path (gen_entry *table, decode_path_list *paths)
{
  if (paths == NULL)
    return 1;
  while (paths != NULL)
    {
      gen_entry *entry = table;
      decode_path *path = paths->path;
      while (1)
	{
	  if (entry == NULL && path == NULL)
	    return 1;
	  if (entry == NULL || path == NULL)
	    break;
	  if (entry->opcode_nr != path->opcode_nr)
	    break;
	  entry = entry->parent;
	  path = path->parent;
	}
      paths = paths->next;
    }
  return 0;
}


static int
insns_match_conditions (insn_list *insns, decode_cond *conditions)
{
  if (conditions != NULL)
    {
      insn_list *i;
      for (i = insns; i != NULL; i = i->next)
	{
	  decode_cond *cond;
	  for (cond = conditions; cond != NULL; cond = cond->next)
	    {
	      int bit_nr;
	      if (i->insn->nr_words <= cond->word_nr)
		return 0;
	      for (bit_nr = 0; bit_nr < options.insn_bit_size; bit_nr++)
		{
		  if (!cond->mask[bit_nr])
		    continue;
		  if (!i->insn->word[cond->word_nr]->bit[bit_nr]->mask)
		    return 0;
		  if ((i->insn->word[cond->word_nr]->bit[bit_nr]->value
		       == cond->value[bit_nr]) == !cond->is_equal)
		    return 0;
		}
	    }
	}
    }
  return 1;
}

static int
insns_match_nr_words (insn_list *insns, int nr_words)
{
  insn_list *i;
  for (i = insns; i != NULL; i = i->next)
    {
      if (i->insn->nr_words < nr_words)
	return 0;
    }
  return 1;
}

static int
insn_list_cmp (insn_list *l, insn_list *r)
{
  while (1)
    {
      insn_entry *insn;
      if (l == NULL && r == NULL)
	return 0;
      if (l == NULL)
	return -1;
      if (r == NULL)
	return 1;
      if (l->insn != r->insn)
	return -1;		/* somewhat arbitrary at present */
      /* skip this insn */
      insn = l->insn;
      while (l != NULL && l->insn == insn)
	l = l->next;
      while (r != NULL && r->insn == insn)
	r = r->next;
    }
}



static void
gen_entry_expand_insns (gen_entry *table)
{
  decode_table *opcode_rule;

  ASSERT (table->nr_insns >= 1);

  /* determine a valid opcode */
  for (opcode_rule = table->opcode_rule;
       opcode_rule != NULL; opcode_rule = opcode_rule->next)
    {
      char *discard_reason;
      if (table->top->model != NULL
	  && opcode_rule->model_names != NULL
	  && !filter_is_member (opcode_rule->model_names,
				table->top->model->name))
	{
	  /* the rule isn't applicable to this processor */
	  discard_reason = "wrong model";
	}
      else if (table->nr_insns == 1 && opcode_rule->conditions == NULL)
	{
	  /* for safety, require a pre-codition when attempting to
	     apply a rule to a single instruction */
	  discard_reason = "need pre-condition when nr-insn == 1";
	}
      else if (table->nr_insns == 1 && !opcode_rule->with_duplicates)
	{
	  /* Little point in expanding a single instruction when we're
	     not duplicating the semantic functions that this table
	     calls */
	  discard_reason = "need duplication with nr-insns == 1";
	}
      else
	if (!insns_match_format_names
	    (table->insns, opcode_rule->format_names))
	{
	  discard_reason = "wrong format name";
	}
      else if (!insns_match_nr_words (table->insns, opcode_rule->word_nr + 1))
	{
	  discard_reason = "wrong nr words";
	}
      else if (!table_matches_path (table, opcode_rule->paths))
	{
	  discard_reason = "path failed";
	}
      else
	if (!insns_match_conditions (table->insns, opcode_rule->conditions))
	{
	  discard_reason = "condition failed";
	}
      else
	{
	  discard_reason = "no opcode field";
	  table->opcode = gen_entry_find_opcode_field (table->insns,
						       opcode_rule,
						       table->nr_insns == 1	/*string-only */
	    );
	  if (table->opcode != NULL)
	    {
	      table->opcode_rule = opcode_rule;
	      break;
	    }
	}

      if (options.trace.rule_rejection)
	{
	  print_gen_entry_path (opcode_rule->line, table, notify);
	  notify (NULL, ": rule discarded - %s\n", discard_reason);
	}
    }

  /* did we find anything */
  if (opcode_rule == NULL)
    {
      /* the decode table failed, this set of instructions haven't
         been uniquely identified */
      if (table->nr_insns > 1)
	{
	  print_gen_entry_insns (table, warning,
				 "was not uniquely decoded",
				 "decodes to the same entry");
	  error (NULL, "");
	}
      return;
    }

  /* Determine the number of words that must have been prefetched for
     this table to function */
  if (table->parent == NULL)
    table->nr_prefetched_words = table->opcode_rule->word_nr + 1;
  else if (table->opcode_rule->word_nr + 1 >
	   table->parent->nr_prefetched_words)
    table->nr_prefetched_words = table->opcode_rule->word_nr + 1;
  else
    table->nr_prefetched_words = table->parent->nr_prefetched_words;

  /* back link what we found to its parent */
  if (table->parent != NULL)
    {
      ASSERT (table->parent->opcode != NULL);
      table->opcode->parent = table->parent->opcode;
    }

  /* report the rule being used to expand the instructions */
  if (options.trace.rule_selection)
    {
      print_gen_entry_path (table->opcode_rule->line, table, notify);
      notify (NULL,
	      ": decode - word %d, bits [%d..%d] in [%d..%d], opcodes %d, entries %d\n",
	      table->opcode->word_nr,
	      i2target (options.hi_bit_nr, table->opcode->first),
	      i2target (options.hi_bit_nr, table->opcode->last),
	      i2target (options.hi_bit_nr, table->opcode_rule->first),
	      i2target (options.hi_bit_nr, table->opcode_rule->last),
	      table->opcode->nr_opcodes, table->nr_entries);
    }

  /* expand the raw instructions according to the opcode */
  {
    insn_list *entry;
    for (entry = table->insns; entry != NULL; entry = entry->next)
      {
	if (options.trace.insn_expansion)
	  {
	    print_gen_entry_path (table->opcode_rule->line, table, notify);
	    notify (NULL, ": expand - %s.%s\n",
		    entry->insn->format_name, entry->insn->name);
	  }
	gen_entry_insert_expanding (table, entry->insn);
      }
  }

  /* dump the results */
  if (options.trace.entries)
    {
      gen_entry *entry;
      for (entry = table->entries; entry != NULL; entry = entry->sibling)
	{
	  insn_list *l;
	  print_gen_entry_path (table->opcode_rule->line, entry, notify);
	  notify (NULL, ": %d - entries %d -",
		  entry->opcode_nr, entry->nr_insns);
	  for (l = entry->insns; l != NULL; l = l->next)
	    notify (NULL, " %s.%s", l->insn->format_name, l->insn->name);
	  notify (NULL, "\n");
	}
    }

  /* perform a combine pass if needed */
  if (table->opcode_rule->with_combine)
    {
      gen_entry *entry;
      for (entry = table->entries; entry != NULL; entry = entry->sibling)
	{
	  if (entry->combined_parent == NULL)
	    {
	      gen_entry **last = &entry->combined_next;
	      gen_entry *alt;
	      for (alt = entry->sibling; alt != NULL; alt = alt->sibling)
		{
		  if (alt->combined_parent == NULL
		      && insn_list_cmp (entry->insns, alt->insns) == 0)
		    {
		      alt->combined_parent = entry;
		      *last = alt;
		      last = &alt->combined_next;
		    }
		}
	    }
	}
      if (options.trace.combine)
	{
	  int nr_unique = 0;
	  gen_entry *entry;
	  for (entry = table->entries; entry != NULL; entry = entry->sibling)
	    {
	      if (entry->combined_parent == NULL)
		{
		  insn_list *l;
		  gen_entry *duplicate;
		  nr_unique++;
		  print_gen_entry_path (table->opcode_rule->line, entry,
					notify);
		  for (duplicate = entry->combined_next; duplicate != NULL;
		       duplicate = duplicate->combined_next)
		    {
		      notify (NULL, "+%d", duplicate->opcode_nr);
		    }
		  notify (NULL, ": entries %d -", entry->nr_insns);
		  for (l = entry->insns; l != NULL; l = l->next)
		    {
		      notify (NULL, " %s.%s",
			      l->insn->format_name, l->insn->name);
		    }
		  notify (NULL, "\n");
		}
	    }
	  print_gen_entry_path (table->opcode_rule->line, table, notify);
	  notify (NULL,
		  ": combine - word %d, bits [%d..%d] in [%d..%d], opcodes %d, entries %d, unique %d\n",
		  table->opcode->word_nr, i2target (options.hi_bit_nr,
						    table->opcode->first),
		  i2target (options.hi_bit_nr, table->opcode->last),
		  i2target (options.hi_bit_nr, table->opcode_rule->first),
		  i2target (options.hi_bit_nr, table->opcode_rule->last),
		  table->opcode->nr_opcodes, table->nr_entries, nr_unique);
	}
    }

  /* Check that the rule did more than re-arange the order of the
     instructions */
  {
    gen_entry *entry;
    for (entry = table->entries; entry != NULL; entry = entry->sibling)
      {
	if (entry->combined_parent == NULL)
	  {
	    if (insn_list_cmp (table->insns, entry->insns) == 0)
	      {
		print_gen_entry_path (table->opcode_rule->line, table,
				      warning);
		warning (NULL,
			 ": Applying rule just copied all instructions\n");
		print_gen_entry_insns (entry, warning, "Copied", NULL);
		error (NULL, "");
	      }
	  }
      }
  }

  /* if some form of expanded table, fill in the missing dots */
  switch (table->opcode_rule->gen)
    {
    case padded_switch_gen:
    case array_gen:
    case goto_switch_gen:
      if (!table->opcode->is_boolean)
	{
	  gen_entry **entry = &table->entries;
	  gen_entry *illegals = NULL;
	  gen_entry **last_illegal = &illegals;
	  int opcode_nr = 0;
	  while (opcode_nr < table->opcode->nr_opcodes)
	    {
	      if ((*entry) == NULL || (*entry)->opcode_nr != opcode_nr)
		{
		  /* missing - insert it under our feet at *entry */
		  gen_entry_insert_insn (table, table->top->isa->illegal_insn, table->opcode->word_nr, 0,	/* nr_prefetched_words == 0 for invalid */
					 opcode_nr, NULL);
		  ASSERT ((*entry) != NULL);
		  ASSERT ((*entry)->opcode_nr == opcode_nr);
		  (*last_illegal) = *entry;
		  (*last_illegal)->combined_parent = illegals;
		  last_illegal = &(*last_illegal)->combined_next;
		}
	      entry = &(*entry)->sibling;
	      opcode_nr++;
	    }
	  /* oops, will have pointed the first illegal insn back to
	     its self.  Fix this */
	  if (illegals != NULL)
	    illegals->combined_parent = NULL;
	}
      break;
    case switch_gen:
    case invalid_gen:
      /* ignore */
      break;
    }

  /* and do the same for the newly created sub entries but *only*
     expand entries that haven't been combined. */
  {
    gen_entry *entry;
    for (entry = table->entries; entry != NULL; entry = entry->sibling)
      {
	if (entry->combined_parent == NULL)
	  {
	    gen_entry_expand_insns (entry);
	  }
      }
  }
}

void
gen_tables_expand_insns (gen_table *gen)
{
  gen_list *entry;
  for (entry = gen->tables; entry != NULL; entry = entry->next)
    {
      gen_entry_expand_insns (entry->table);
    }
}


/* create a list of all the semantic functions that need to be
   generated.  Eliminate any duplicates. Verify that the decode stage
   worked. */

static void
make_gen_semantics_list (lf *file, gen_entry *entry, int depth, void *data)
{
  gen_table *gen = (gen_table *) data;
  insn_list *insn;
  /* Not interested in an entrie that have been combined into some
     other entry at the same level */
  if (entry->combined_parent != NULL)
    return;

  /* a leaf should contain exactly one instruction. If not the decode
     stage failed. */
  ASSERT (entry->nr_insns == 1);

  /* Enter this instruction into the list of semantic functions. */
  insn = insn_list_insert (&gen->semantics, &gen->nr_semantics,
			   entry->insns->insn,
			   entry->expanded_bits,
			   entry->parent->opcode,
			   entry->insns->nr_prefetched_words,
			   merge_duplicate_insns);
  /* point the table entry at the real semantic function */
  ASSERT (insn != NULL);
  entry->insns->semantic = insn;
}


void
gen_tables_expand_semantics (gen_table *gen)
{
  gen_list *entry;
  for (entry = gen->tables; entry != NULL; entry = entry->next)
    {
      gen_entry_traverse_tree (NULL, entry->table, 1,	/* depth */
			       NULL,	/* start-handler */
			       make_gen_semantics_list,	/* leaf-handler */
			       NULL,	/* end-handler */
			       gen);	/* data */
    }
}



#ifdef MAIN


static void
dump_opcode_field (lf *file,
		   char *prefix,
		   opcode_field *field, char *suffix, int levels)
{
  lf_printf (file, "%s(opcode_field *) 0x%lx", prefix, (long) field);
  if (levels && field != NULL)
    {
      lf_indent (file, +1);
      lf_printf (file, "\n(first %d)", field->first);
      lf_printf (file, "\n(last %d)", field->last);
      lf_printf (file, "\n(nr_opcodes %d)", field->nr_opcodes);
      lf_printf (file, "\n(is_boolean %d)", field->is_boolean);
      lf_printf (file, "\n(boolean_constant %d)", field->boolean_constant);
      dump_opcode_field (file, "\n(parent ", field->parent, ")", levels - 1);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


static void
dump_opcode_bits (lf *file,
		  char *prefix, opcode_bits *bits, char *suffix, int levels)
{
  lf_printf (file, "%s(opcode_bits *) 0x%lx", prefix, (long) bits);

  if (levels && bits != NULL)
    {
      lf_indent (file, +1);
      lf_printf (file, "\n(value %d)", bits->value);
      dump_opcode_field (file, "\n(opcode ", bits->opcode, ")", 0);
      dump_insn_field (file, "\n(field ", bits->field, ")");
      dump_opcode_bits (file, "\n(next ", bits->next, ")", levels - 1);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}



static void
dump_insn_list (lf *file, char *prefix, insn_list *entry, char *suffix)
{
  lf_printf (file, "%s(insn_list *) 0x%lx", prefix, (long) entry);

  if (entry != NULL)
    {
      lf_indent (file, +1);
      dump_insn_entry (file, "\n(insn ", entry->insn, ")");
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


static void
dump_insn_word_entry_list_entries (lf *file,
				   char *prefix,
				   insn_list *entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  while (entry != NULL)
    {
      dump_insn_list (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_printf (file, "%s", suffix);
}


static void
dump_gen_entry (lf *file,
		char *prefix, gen_entry *table, char *suffix, int levels)
{

  lf_printf (file, "%s(gen_entry *) 0x%lx", prefix, (long) table);

  if (levels && table !=NULL)
    {

      lf_indent (file, +1);
      lf_printf (file, "\n(opcode_nr %d)", table->opcode_nr);
      lf_printf (file, "\n(word_nr %d)", table->word_nr);
      dump_opcode_bits (file, "\n(expanded_bits ", table->expanded_bits, ")",
			-1);
      lf_printf (file, "\n(nr_insns %d)", table->nr_insns);
      dump_insn_word_entry_list_entries (file, "\n(insns ", table->insns,
					 ")");
      dump_decode_rule (file, "\n(opcode_rule ", table->opcode_rule, ")");
      dump_opcode_field (file, "\n(opcode ", table->opcode, ")", 0);
      lf_printf (file, "\n(nr_entries %d)", table->nr_entries);
      dump_gen_entry (file, "\n(entries ", table->entries, ")",
		      table->nr_entries);
      dump_gen_entry (file, "\n(sibling ", table->sibling, ")", levels - 1);
      dump_gen_entry (file, "\n(parent ", table->parent, ")", 0);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_gen_list (lf *file,
	       char *prefix, gen_list *entry, char *suffix, int levels)
{
  while (entry != NULL)
    {
      lf_printf (file, "%s(gen_list *) 0x%lx", prefix, (long) entry);
      dump_gen_entry (file, "\n(", entry->table, ")", levels);
      lf_printf (file, "\n(next (gen_list *) 0x%lx)", (long) entry->next);
      lf_printf (file, "%s", suffix);
    }
}


static void
dump_gen_table (lf *file,
		char *prefix, gen_table *gen, char *suffix, int levels)
{
  lf_printf (file, "%s(gen_table *) 0x%lx", prefix, (long) gen);
  lf_printf (file, "\n(isa (insn_table *) 0x%lx)", (long) gen->isa);
  lf_printf (file, "\n(rules (decode_table *) 0x%lx)", (long) gen->rules);
  dump_gen_list (file, "\n(", gen->tables, ")", levels);
  lf_printf (file, "%s", suffix);
}


igen_options options;

int
main (int argc, char **argv)
{
  decode_table *decode_rules;
  insn_table *instructions;
  gen_table *gen;
  lf *l;

  if (argc != 7)
    error (NULL,
	   "Usage: insn <filter-in> <hi-bit-nr> <insn-bit-size> <widths> <decode-table> <insn-table>\n");

  INIT_OPTIONS (options);

  filter_parse (&options.flags_filter, argv[1]);

  options.hi_bit_nr = a2i (argv[2]);
  options.insn_bit_size = a2i (argv[3]);
  options.insn_specifying_widths = a2i (argv[4]);
  ASSERT (options.hi_bit_nr < options.insn_bit_size);

  instructions = load_insn_table (argv[6], NULL);
  decode_rules = load_decode_table (argv[5]);
  gen = make_gen_tables (instructions, decode_rules);

  gen_tables_expand_insns (gen);

  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-ld-insn");

  dump_gen_table (l, "(", gen, ")\n", -1);
  return 0;
}

#endif
