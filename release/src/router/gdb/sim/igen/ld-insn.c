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

static insn_word_entry *
parse_insn_word (line_ref *line, char *string, int word_nr)
{
  char *chp;
  insn_word_entry *word = ZALLOC (insn_word_entry);

  /* create a leading sentinal */
  word->first = ZALLOC (insn_field_entry);
  word->first->first = -1;
  word->first->last = -1;
  word->first->width = 0;

  /* and a trailing sentinal */
  word->last = ZALLOC (insn_field_entry);
  word->last->first = options.insn_bit_size;
  word->last->last = options.insn_bit_size;
  word->last->width = 0;

  /* link them together */
  word->first->next = word->last;
  word->last->prev = word->first;

  /* now work through the formats */
  chp = skip_spaces (string);

  while (*chp != '\0')
    {
      char *start_pos;
      int strlen_pos;
      char *start_val;
      int strlen_val;
      insn_field_entry *new_field;

      /* create / link in the new field */
      new_field = ZALLOC (insn_field_entry);
      new_field->next = word->last;
      new_field->prev = word->last->prev;
      new_field->next->prev = new_field;
      new_field->prev->next = new_field;
      new_field->word_nr = word_nr;

      /* break out the first field (if present) */
      start_pos = chp;
      chp = skip_to_separator (chp, ".,!");
      strlen_pos = back_spaces (start_pos, chp) - start_pos;

      /* break out the second field (if present) */
      if (*chp != '.')
	{
	  /* assume what was specified was the value (and not the start
	     position).  Assume the value length implicitly specifies
	     the number of bits */
	  start_val = start_pos;
	  strlen_val = strlen_pos;
	  start_pos = "";
	  strlen_pos = 0;
	}
      else
	{
	  chp++;		/* skip `.' */
	  chp = skip_spaces (chp);
	  start_val = chp;
	  if (*chp == '/' || *chp == '*')
	    {
	      do
		{
		  chp++;
		}
	      while (*chp == '/' || *chp == '*');
	    }
	  else if (isalpha (*start_val))
	    {
	      do
		{
		  chp++;
		}
	      while (isalnum (*chp) || *chp == '_');
	    }
	  else if (isdigit (*start_val))
	    {
	      do
		{
		  chp++;
		}
	      while (isalnum (*chp));
	    }
	  strlen_val = chp - start_val;
	  chp = skip_spaces (chp);
	}
      if (strlen_val == 0)
	error (line, "Empty value field\n");

      /* break out any conditional fields - { [ "!" | "=" [ <value> | <field-name> } */
      while (*chp == '!' || *chp == '=')
	{
	  char *start;
	  char *end;
	  int len;
	  insn_field_cond *new_cond = ZALLOC (insn_field_cond);

	  /* determine the conditional test */
	  switch (*chp)
	    {
	    case '=':
	      new_cond->test = insn_field_cond_eq;
	      break;
	    case '!':
	      new_cond->test = insn_field_cond_ne;
	      break;
	    default:
	      ASSERT (0);
	    }

	  /* save the value */
	  chp++;
	  chp = skip_spaces (chp);
	  start = chp;
	  chp = skip_to_separator (chp, "+,:!=");
	  end = back_spaces (start, chp);
	  len = end - start;
	  if (len == 0)
	    error (line, "Missing or invalid conditional value\n");
	  new_cond->string = NZALLOC (char, len + 1);
	  strncpy (new_cond->string, start, len);

	  /* determine the conditional type */
	  if (isdigit (*start))
	    {
	      /* [ "!" | "=" ] <value> */
	      new_cond->type = insn_field_cond_value;
	      new_cond->value = a2i (new_cond->string);
	    }
	  else
	    {
	      /* [ "!" | "=" ] <field>  - check field valid */
	      new_cond->type = insn_field_cond_field;
	      /* new_cond->field is determined in later */
	    }

	  /* Only a single `=' is permitted. */
	  if ((new_cond->test == insn_field_cond_eq
	       && new_field->conditions != NULL)
	      || (new_field->conditions != NULL
		  && new_field->conditions->test == insn_field_cond_eq))
	    error (line, "Only single conditional when `=' allowed\n");

	  /* insert it */
	  {
	    insn_field_cond **last = &new_field->conditions;
	    while (*last != NULL)
	      last = &(*last)->next;
	    *last = new_cond;
	  }
	}

      /* NOW verify that the field was finished */
      if (*chp == ',')
	{
	  chp = skip_spaces (chp + 1);
	  if (*chp == '\0')
	    error (line, "empty field\n");
	}
      else if (*chp != '\0')
	{
	  error (line, "Missing field separator\n");
	}

      /* copy the value */
      new_field->val_string = NZALLOC (char, strlen_val + 1);
      strncpy (new_field->val_string, start_val, strlen_val);
      if (isdigit (new_field->val_string[0]))
	{
	  if (strlen_pos == 0)
	    {
	      /* when the length/pos field is omited, an integer field
	         is always binary */
	      unsigned64 val = 0;
	      int i;
	      for (i = 0; i < strlen_val; i++)
		{
		  if (new_field->val_string[i] != '0'
		      && new_field->val_string[i] != '1')
		    error (line, "invalid binary field %s\n",
			   new_field->val_string);
		  val = (val << 1) + (new_field->val_string[i] == '1');
		}
	      new_field->val_int = val;
	      new_field->type = insn_field_int;
	    }
	  else
	    {
	      new_field->val_int = a2i (new_field->val_string);
	      new_field->type = insn_field_int;
	    }
	}
      else if (new_field->val_string[0] == '/')
	{
	  new_field->type = insn_field_reserved;
	}
      else if (new_field->val_string[0] == '*')
	{
	  new_field->type = insn_field_wild;
	}
      else
	{
	  new_field->type = insn_field_string;
	  if (filter_is_member (word->field_names, new_field->val_string))
	    error (line, "Field name %s is duplicated\n",
		   new_field->val_string);
	  filter_parse (&word->field_names, new_field->val_string);
	}
      if (new_field->type != insn_field_string
	  && new_field->conditions != NULL)
	error (line, "Conditionals can only be applied to named fields\n");

      /* the copy the position */
      new_field->pos_string = NZALLOC (char, strlen_pos + 1);
      strncpy (new_field->pos_string, start_pos, strlen_pos);
      if (strlen_pos == 0)
	{
	  new_field->first = new_field->prev->last + 1;
	  if (new_field->first == 0	/* first field */
	      && *chp == '\0'	/* no further fields */
	      && new_field->type == insn_field_string)
	    {
	      /* A single string without any position, assume that it
	         represents the entire instruction word */
	      new_field->width = options.insn_bit_size;
	    }
	  else
	    {
	      /* No explicit width/position, assume value implicitly
	         supplies the width */
	      new_field->width = strlen_val;
	    }
	  new_field->last = new_field->first + new_field->width - 1;
	  if (new_field->last >= options.insn_bit_size)
	    error (line, "Bit position %d exceed instruction bit size (%d)\n",
		   new_field->last, options.insn_bit_size);
	}
      else if (options.insn_specifying_widths)
	{
	  new_field->first = new_field->prev->last + 1;
	  new_field->width = a2i (new_field->pos_string);
	  new_field->last = new_field->first + new_field->width - 1;
	  if (new_field->last >= options.insn_bit_size)
	    error (line, "Bit position %d exceed instruction bit size (%d)\n",
		   new_field->last, options.insn_bit_size);
	}
      else
	{
	  new_field->first = target_a2i (options.hi_bit_nr,
					 new_field->pos_string);
	  new_field->last = new_field->next->first - 1;	/* guess */
	  new_field->width = new_field->last - new_field->first + 1;	/* guess */
	  new_field->prev->last = new_field->first - 1;	/*fix */
	  new_field->prev->width = new_field->first - new_field->prev->first;	/*fix */
	}
    }

  /* fiddle first/last so that the sentinals disapear */
  ASSERT (word->first->last < 0);
  ASSERT (word->last->first >= options.insn_bit_size);
  word->first = word->first->next;
  word->last = word->last->prev;

  /* check that the last field goes all the way to the last bit */
  if (word->last->last != options.insn_bit_size - 1)
    {
      if (options.warn.width)
	options.warning (line, "Instruction format is not %d bits wide\n",
			 options.insn_bit_size);
      word->last->last = options.insn_bit_size - 1;
    }

  /* now go over this again, pointing each bit position at a field
     record */
  {
    insn_field_entry *field;
    for (field = word->first;
	 field->last < options.insn_bit_size; field = field->next)
      {
	int i;
	for (i = field->first; i <= field->last; i++)
	  {
	    word->bit[i] = ZALLOC (insn_bit_entry);
	    word->bit[i]->field = field;
	    switch (field->type)
	      {
	      case insn_field_invalid:
		ASSERT (0);
		break;
	      case insn_field_int:
		word->bit[i]->mask = 1;
		word->bit[i]->value = ((field->val_int
					& ((insn_uint) 1 <<
					   (field->last - i))) != 0);
	      case insn_field_reserved:
	      case insn_field_wild:
	      case insn_field_string:
		/* if we encounter a constant conditional, encode
		   their bit value. */
		if (field->conditions != NULL
		    && field->conditions->test == insn_field_cond_eq
		    && field->conditions->type == insn_field_cond_value)
		  {
		    word->bit[i]->mask = 1;
		    word->bit[i]->value = ((field->conditions->value
					    & ((insn_uint) 1 <<
					       (field->last - i))) != 0);
		  }
		break;
	      }
	  }
      }
  }

  return word;
}


static void
parse_insn_words (insn_entry * insn, char *formats)
{
  insn_word_entry **last_word = &insn->words;
  char *chp;

  /* now work through the formats */
  insn->nr_words = 0;
  chp = formats;

  while (1)
    {
      char *start_pos;
      char *end_pos;
      int strlen_pos;
      char *format;
      insn_word_entry *new_word;

      /* skip leading spaces */
      chp = skip_spaces (chp);

      /* break out the format */
      start_pos = chp;
      chp = skip_to_separator (chp, "+");
      end_pos = back_spaces (start_pos, chp);
      strlen_pos = end_pos - start_pos;

      /* check that something was there */
      if (strlen_pos == 0)
	error (insn->line, "missing or empty instruction format\n");

      /* parse the field */
      format = NZALLOC (char, strlen_pos + 1);
      strncpy (format, start_pos, strlen_pos);
      new_word = parse_insn_word (insn->line, format, insn->nr_words);
      insn->nr_words++;
      if (filter_is_common (insn->field_names, new_word->field_names))
	error (insn->line, "Field name duplicated between two words\n");
      filter_add (&insn->field_names, new_word->field_names);

      /* insert it */
      *last_word = new_word;
      last_word = &new_word->next;

      /* last format? */
      if (*chp == '\0')
	break;
      ASSERT (*chp == '+');
      chp++;
    }

  /* create a quick access array (indexed by word) of the same structure */
  {
    int i;
    insn_word_entry *word;
    insn->word = NZALLOC (insn_word_entry *, insn->nr_words + 1);
    for (i = 0, word = insn->words;
	 i < insn->nr_words; i++, word = word->next)
      insn->word[i] = word;
  }

  /* Go over all fields that have conditionals refering to other
     fields.  Link the fields up.  Verify that the two fields have the
     same size. Verify that the two fields are different */
  {
    int i;
    for (i = 0; i < insn->nr_words; i++)
      {
	insn_word_entry *word = insn->word[i];
	insn_field_entry *f;
	for (f = word->first; f->last < options.insn_bit_size; f = f->next)
	  {
	    insn_field_cond *cond;
	    for (cond = f->conditions; cond != NULL; cond = cond->next)
	      {
		if (cond->type == insn_field_cond_field)
		  {
		    int j;
		    if (strcmp (cond->string, f->val_string) == 0)
		      error (insn->line,
			     "Conditional `%s' of field `%s' refers to its self\n",
			     cond->string, f->val_string);
		    for (j = 0; j <= i && cond->field == NULL; j++)
		      {
			insn_word_entry *refered_word = insn->word[j];
			insn_field_entry *refered_field;
			for (refered_field = refered_word->first;
			     refered_field != NULL && cond->field == NULL;
			     refered_field = refered_field->next)
			  {
			    if (refered_field->type == insn_field_string
				&& strcmp (refered_field->val_string,
					   cond->string) == 0)
			      {
				/* found field being refered to by conditonal */
				cond->field = refered_field;
				/* check refered to and this field are
				   the same size */
				if (f->width != refered_field->width)
				  error (insn->line,
					 "Conditional `%s' of field `%s' should be of size %s\n",
					 cond->string, f->val_string,
					 refered_field->width);
			      }
			  }
		      }
		    if (cond->field == NULL)
		      error (insn->line,
			     "Conditional `%s' of field `%s' not yet defined\n",
			     cond->string, f->val_string);
		  }
	      }
	  }
      }
  }

}

typedef enum
{
  unknown_record = 0,
  insn_record,			/* default */
  code_record,
  cache_record,
  compute_record,
  scratch_record,
  option_record,
  string_function_record,
  function_record,
  internal_record,
  define_record,
  include_record,
  model_processor_record,
  model_macro_record,
  model_data_record,
  model_static_record,
  model_function_record,
  model_internal_record,
}
insn_record_type;

static const name_map insn_type_map[] = {
  {"option", option_record},
  {"cache", cache_record},
  {"compute", compute_record},
  {"scratch", scratch_record},
  {"define", define_record},
  {"include", include_record},
  {"%s", string_function_record},
  {"function", function_record},
  {"internal", internal_record},
  {"model", model_processor_record},
  {"model-macro", model_macro_record},
  {"model-data", model_data_record},
  {"model-static", model_static_record},
  {"model-internal", model_internal_record},
  {"model-function", model_function_record},
  {NULL, insn_record},
};


static int
record_is_old (table_entry *entry)
{
  if (entry->nr_fields > record_type_field
      && strlen (entry->field[record_type_field]) == 0)
    return 1;
  return 0;
}

static insn_record_type
record_type (table_entry *entry)
{
  switch (entry->type)
    {
    case table_code_entry:
      return code_record;

    case table_colon_entry:
      if (record_is_old (entry))
	{
	  /* old-format? */
	  if (entry->nr_fields > old_record_type_field)
	    {
	      int i = name2i (entry->field[old_record_type_field],
			      insn_type_map);
	      return i;
	    }
	  else
	    {
	      return unknown_record;
	    }
	}
      else if (entry->nr_fields > record_type_field
	       && entry->field[0][0] == '\0')
	{
	  /* new-format? */
	  int i = name2i (entry->field[record_type_field],
			  insn_type_map);
	  return i;
	}
      else
	return insn_record;	/* default */
    }
  return unknown_record;
}

static int
record_prefix_is (table_entry *entry, char ch, int nr_fields)
{
  if (entry->type != table_colon_entry)
    return 0;
  if (entry->nr_fields < nr_fields)
    return 0;
  if (entry->field[0][0] != ch && ch != '\0')
    return 0;
  return 1;
}

static table_entry *
parse_model_data_record (insn_table *isa,
			 table *file,
			 table_entry *record,
			 int nr_fields, model_data **list)
{
  table_entry *model_record = record;
  table_entry *code_record = NULL;
  model_data *new_data;
  if (record->nr_fields < nr_fields)
    error (record->line, "Incorrect number of fields\n");
  record = table_read (file);
  if (record->type == table_code_entry)
    {
      code_record = record;
      record = table_read (file);
    }
  /* create the new data record */
  new_data = ZALLOC (model_data);
  new_data->line = model_record->line;
  filter_parse (&new_data->flags,
		model_record->field[record_filter_flags_field]);
  new_data->entry = model_record;
  new_data->code = code_record;
  /* append it if not filtered out */
  if (!is_filtered_out (options.flags_filter,
			model_record->field[record_filter_flags_field])
      && !is_filtered_out (options.model_filter,
			   model_record->field[record_filter_models_field]))
    {
      while (*list != NULL)
	list = &(*list)->next;
      *list = new_data;
    }
  return record;
}


typedef enum
{
  insn_bit_size_option = 1,
  insn_specifying_widths_option,
  hi_bit_nr_option,
  flags_filter_option,
  model_filter_option,
  multi_sim_option,
  format_names_option,
  gen_delayed_branch,
  unknown_option,
}
option_names;

static const name_map option_map[] = {
  {"insn-bit-size", insn_bit_size_option},
  {"insn-specifying-widths", insn_specifying_widths_option},
  {"hi-bit-nr", hi_bit_nr_option},
  {"flags-filter", flags_filter_option},
  {"model-filter", model_filter_option},
  {"multi-sim", multi_sim_option},
  {"format-names", format_names_option},
  {"gen-delayed-branch", gen_delayed_branch},
  {NULL, unknown_option},
};

static table_entry *
parse_include_record (table *file, table_entry *record)
{
  /* parse the include record */
  if (record->nr_fields < nr_include_fields)
    error (record->line, "Incorrect nr fields for include record\n");
  /* process it */
  if (!is_filtered_out (options.flags_filter,
			record->field[record_filter_flags_field])
      && !is_filtered_out (options.model_filter,
			   record->field[record_filter_models_field]))
    {
      table_push (file, record->line, options.include,
		  record->field[include_filename_field]);
    }
  /* nb: can't read next record until after the file has been pushed */
  record = table_read (file);
  return record;
}


static table_entry *
parse_option_record (table *file, table_entry *record)
{
  table_entry *option_record;
  /* parse the option record */
  option_record = record;
  if (record->nr_fields < nr_option_fields)
    error (record->line, "Incorrect nr of fields for option record\n");
  record = table_read (file);
  /* process it */
  if (!is_filtered_out (options.flags_filter,
			option_record->field[record_filter_flags_field])
      && !is_filtered_out (options.model_filter,
			   option_record->field[record_filter_models_field]))
    {
      char *name = option_record->field[option_name_field];
      option_names option = name2i (name, option_map);
      char *value = option_record->field[option_value_field];
      switch (option)
	{
	case insn_bit_size_option:
	  {
	    options.insn_bit_size = a2i (value);
	    if (options.insn_bit_size < 0
		|| options.insn_bit_size > max_insn_bit_size)
	      error (option_record->line,
		     "Instruction bit size out of range\n");
	    if (options.hi_bit_nr != options.insn_bit_size - 1
		&& options.hi_bit_nr != 0)
	      error (option_record->line,
		     "insn-bit-size / hi-bit-nr conflict\n");
	    break;
	  }
	case insn_specifying_widths_option:
	  {
	    options.insn_specifying_widths = a2i (value);
	    break;
	  }
	case hi_bit_nr_option:
	  {
	    options.hi_bit_nr = a2i (value);
	    if (options.hi_bit_nr != 0
		&& options.hi_bit_nr != options.insn_bit_size - 1)
	      error (option_record->line,
		     "hi-bit-nr / insn-bit-size conflict\n");
	    break;
	  }
	case flags_filter_option:
	  {
	    filter_parse (&options.flags_filter, value);
	    break;
	  }
	case model_filter_option:
	  {
	    filter_parse (&options.model_filter, value);
	    break;
	  }
	case multi_sim_option:
	  {
	    options.gen.multi_sim = a2i (value);
	    break;
	  }
	case format_names_option:
	  {
	    filter_parse (&options.format_name_filter, value);
	    break;
	  }
	case gen_delayed_branch:
	  {
	    options.gen.delayed_branch = a2i (value);
	    break;
	  }
	case unknown_option:
	  {
	    error (option_record->line, "Unknown option - %s\n", name);
	    break;
	  }
	}
    }
  return record;
}


static table_entry *
parse_function_record (table *file,
		       table_entry *record,
		       function_entry ** list,
		       function_entry ** list_entry,
		       int is_internal, model_table *model)
{
  function_entry *new_function;
  new_function = ZALLOC (function_entry);
  new_function->line = record->line;
  new_function->is_internal = is_internal;
  /* parse the function header */
  if (record_is_old (record))
    {
      if (record->nr_fields < nr_old_function_fields)
	error (record->line, "Missing fields from (old) function record\n");
      new_function->type = record->field[old_function_typedef_field];
      new_function->type = record->field[old_function_typedef_field];
      if (record->nr_fields > old_function_param_field)
	new_function->param = record->field[old_function_param_field];
      new_function->name = record->field[old_function_name_field];
    }
  else
    {
      if (record->nr_fields < nr_function_fields)
	error (record->line, "Missing fields from function record\n");
      filter_parse (&new_function->flags,
		    record->field[record_filter_flags_field]);
      filter_parse (&new_function->models,
		    record->field[record_filter_models_field]);
      new_function->type = record->field[function_typedef_field];
      new_function->param = record->field[function_param_field];
      new_function->name = record->field[function_name_field];
    }
  record = table_read (file);
  /* parse any function-model records */
  while (record != NULL
	 && record_prefix_is (record, '*', nr_function_model_fields))
    {
      char *model_name = record->field[function_model_name_field] + 1;	/*skip `*' */
      filter_parse (&new_function->models, model_name);
      if (!filter_is_subset (model->processors, new_function->models))
	{
	  error (record->line, "machine model `%s' undefined\n", model_name);
	}
      record = table_read (file);
    }
  /* parse the function body */
  if (record->type == table_code_entry)
    {
      new_function->code = record;
      record = table_read (file);
    }
  /* insert it */
  if (!filter_is_subset (options.flags_filter, new_function->flags))
    {
      if (options.warn.discard)
	notify (new_function->line, "Discarding function %s - filter flags\n",
		new_function->name);
    }
  else if (new_function->models != NULL
	   && !filter_is_common (options.model_filter, new_function->models))
    {
      if (options.warn.discard)
	notify (new_function->line,
		"Discarding function %s - filter models\n",
		new_function->name);
    }
  else
    {
      while (*list != NULL)
	list = &(*list)->next;
      *list = new_function;
      if (list_entry != NULL)
	*list_entry = new_function;
    }
  /* done */
  return record;
}

static void
parse_insn_model_record (table *file,
			 table_entry *record,
			 insn_entry * insn, model_table *model)
{
  insn_model_entry **last_insn_model;
  insn_model_entry *new_insn_model = ZALLOC (insn_model_entry);
  /* parse it */
  new_insn_model->line = record->line;
  if (record->nr_fields > insn_model_unit_data_field)
    new_insn_model->unit_data = record->field[insn_model_unit_data_field];
  new_insn_model->insn = insn;
  /* parse the model names, verify that all were defined */
  new_insn_model->names = NULL;
  filter_parse (&new_insn_model->names,
		record->field[insn_model_name_field] + 1 /*skip `*' */ );
  if (new_insn_model->names == NULL)
    {
      /* No processor names - a generic model entry, enter it into all
         the non-empty fields */
      int index;
      for (index = 0; index < model->nr_models; index++)
	if (insn->model[index] == 0)
	  {
	    insn->model[index] = new_insn_model;
	  }
      /* also add the complete processor set to this processor's set */
      filter_add (&insn->processors, model->processors);
    }
  else
    {
      /* Find the corresponding master model record for each name so
         that they can be linked in. */
      int index;
      char *name = "";
      while (1)
	{
	  name = filter_next (new_insn_model->names, name);
	  if (name == NULL)
	    break;
	  index = filter_is_member (model->processors, name) - 1;
	  if (index < 0)
	    {
	      error (new_insn_model->line,
		     "machine model `%s' undefined\n", name);
	    }
	  /* store it in the corresponding model array entry */
	  if (insn->model[index] != NULL && insn->model[index]->names != NULL)
	    {
	      warning (new_insn_model->line,
		       "machine model `%s' previously defined\n", name);
	      error (insn->model[index]->line, "earlier definition\n");
	    }
	  insn->model[index] = new_insn_model;
	  /* also add the name to the instructions processor set as an
	     alternative lookup mechanism */
	  filter_parse (&insn->processors, name);
	}
    }
#if 0
  /* for some reason record the max length of any
     function unit field */
  int len = strlen (insn_model_ptr->field[insn_model_fields]);
  if (model->max_model_fields_len < len)
    model->max_model_fields_len = len;
#endif
  /* link it in */
  last_insn_model = &insn->models;
  while ((*last_insn_model) != NULL)
    last_insn_model = &(*last_insn_model)->next;
  *last_insn_model = new_insn_model;
}


static void
parse_insn_mnemonic_record (table *file,
			    table_entry *record, insn_entry * insn)
{
  insn_mnemonic_entry **last_insn_mnemonic;
  insn_mnemonic_entry *new_insn_mnemonic = ZALLOC (insn_mnemonic_entry);
  /* parse it */
  new_insn_mnemonic->line = record->line;
  ASSERT (record->nr_fields > insn_mnemonic_format_field);
  new_insn_mnemonic->format = record->field[insn_mnemonic_format_field];
  ASSERT (new_insn_mnemonic->format[0] == '"');
  if (new_insn_mnemonic->format[strlen (new_insn_mnemonic->format) - 1] !=
      '"')
    error (new_insn_mnemonic->line,
	   "Missing closing double quote in mnemonic field\n");
  if (record->nr_fields > insn_mnemonic_condition_field)
    new_insn_mnemonic->condition =
      record->field[insn_mnemonic_condition_field];
  new_insn_mnemonic->insn = insn;
  /* insert it */
  last_insn_mnemonic = &insn->mnemonics;
  while ((*last_insn_mnemonic) != NULL)
    last_insn_mnemonic = &(*last_insn_mnemonic)->next;
  insn->nr_mnemonics++;
  *last_insn_mnemonic = new_insn_mnemonic;
}


static table_entry *
parse_macro_record (table *file, table_entry *record)
{
#if 1
  error (record->line, "Macros are not implemented");
#else
  /* parse the define record */
  if (record->nr_fields < nr_define_fields)
    error (record->line, "Incorrect nr fields for define record\n");
  /* process it */
  if (!is_filtered_out (options.flags_filter,
			record->field[record_filter_flags_field])
      && !is_filtered_out (options.model_filter,
			   record->field[record_filter_models_field]))
    {
      table_define (file,
		    record->line,
		    record->field[macro_name_field],
		    record->field[macro_args_field],
		    record->field[macro_expr_field]);
    }
  record = table_read (file);
#endif
  return record;
}


insn_table *
load_insn_table (char *file_name, cache_entry *cache)
{
  table *file = table_open (file_name);
  table_entry *record = table_read (file);

  insn_table *isa = ZALLOC (insn_table);
  model_table *model = ZALLOC (model_table);

  isa->model = model;
  isa->caches = cache;

  while (record != NULL)
    {

      switch (record_type (record))
	{

	case include_record:
	  {
	    record = parse_include_record (file, record);
	    break;
	  }

	case option_record:
	  {
	    if (isa->insns != NULL)
	      error (record->line, "Option after first instruction\n");
	    record = parse_option_record (file, record);
	    break;
	  }

	case string_function_record:
	  {
	    function_entry *function = NULL;
	    record = parse_function_record (file, record,
					    &isa->functions,
					    &function, 0 /*is-internal */ ,
					    model);
	    /* convert a string function record into an internal function */
	    if (function != NULL)
	      {
		char *name = NZALLOC (char,
				      (strlen ("str_")
				       + strlen (function->name) + 1));
		strcat (name, "str_");
		strcat (name, function->name);
		function->name = name;
		function->type = "const char *";
	      }
	    break;
	  }

	case function_record:	/* function record */
	  {
	    record = parse_function_record (file, record,
					    &isa->functions,
					    NULL, 0 /*is-internal */ ,
					    model);
	    break;
	  }

	case internal_record:
	  {
	    /* only insert it into the function list if it is unknown */
	    function_entry *function = NULL;
	    record = parse_function_record (file, record,
					    &isa->functions,
					    &function, 1 /*is-internal */ ,
					    model);
	    /* check what was inserted to see if a pseudo-instruction
	       entry also needs to be created */
	    if (function != NULL)
	      {
		insn_entry **insn = NULL;
		if (strcmp (function->name, "illegal") == 0)
		  {
		    /* illegal function save it away */
		    if (isa->illegal_insn != NULL)
		      {
			warning (function->line,
				 "Multiple illegal instruction definitions\n");
			error (isa->illegal_insn->line,
			       "Location of first illegal instruction\n");
		      }
		    else
		      insn = &isa->illegal_insn;
		  }
		if (insn != NULL)
		  {
		    *insn = ZALLOC (insn_entry);
		    (*insn)->line = function->line;
		    (*insn)->name = function->name;
		    (*insn)->code = function->code;
		  }
	      }
	    break;
	  }

	case scratch_record:	/* cache macro records */
	case cache_record:
	case compute_record:
	  {
	    cache_entry *new_cache;
	    /* parse the cache record */
	    if (record->nr_fields < nr_cache_fields)
	      error (record->line,
		     "Incorrect nr of fields for scratch/cache/compute record\n");
	    /* create it */
	    new_cache = ZALLOC (cache_entry);
	    new_cache->line = record->line;
	    filter_parse (&new_cache->flags,
			  record->field[record_filter_flags_field]);
	    filter_parse (&new_cache->models,
			  record->field[record_filter_models_field]);
	    new_cache->type = record->field[cache_typedef_field];
	    new_cache->name = record->field[cache_name_field];
	    filter_parse (&new_cache->original_fields,
			  record->field[cache_original_fields_field]);
	    new_cache->expression = record->field[cache_expression_field];
	    /* insert it but only if not filtered out */
	    if (!filter_is_subset (options.flags_filter, new_cache->flags))
	      {
		notify (new_cache->line,
			"Discarding cache entry %s - filter flags\n",
			new_cache->name);
	      }
	    else if (is_filtered_out (options.model_filter,
				      record->
				      field[record_filter_models_field]))
	      {
		notify (new_cache->line,
			"Discarding cache entry %s - filter models\n",
			new_cache->name);
	      }
	    else
	      {
		cache_entry **last;
		last = &isa->caches;
		while (*last != NULL)
		  last = &(*last)->next;
		*last = new_cache;
	      }
	    /* advance things */
	    record = table_read (file);
	    break;
	  }

	  /* model records */
	case model_processor_record:
	  {
	    model_entry *new_model;
	    /* parse the model */
	    if (record->nr_fields < nr_model_processor_fields)
	      error (record->line,
		     "Incorrect nr of fields for model record\n");
	    if (isa->insns != NULL)
	      error (record->line, "Model appears after first instruction\n");
	    new_model = ZALLOC (model_entry);
	    filter_parse (&new_model->flags,
			  record->field[record_filter_flags_field]);
	    new_model->line = record->line;
	    new_model->name = record->field[model_name_field];
	    new_model->full_name = record->field[model_full_name_field];
	    new_model->unit_data = record->field[model_unit_data_field];
	    /* only insert it if not filtered out */
	    if (!filter_is_subset (options.flags_filter, new_model->flags))
	      {
		notify (new_model->line,
			"Discarding processor model %s - filter flags\n",
			new_model->name);
	      }
	    else if (is_filtered_out (options.model_filter,
				      record->
				      field[record_filter_models_field]))
	      {
		notify (new_model->line,
			"Discarding processor model %s - filter models\n",
			new_model->name);
	      }
	    else if (filter_is_member (model->processors, new_model->name))
	      {
		error (new_model->line, "Duplicate processor model %s\n",
		       new_model->name);
	      }
	    else
	      {
		model_entry **last;
		last = &model->models;
		while (*last != NULL)
		  last = &(*last)->next;
		*last = new_model;
		/* count it */
		model->nr_models++;
		filter_parse (&model->processors, new_model->name);
	      }
	    /* advance things */
	    record = table_read (file);
	  }
	  break;

	case model_macro_record:
	  record = parse_model_data_record (isa, file, record,
					    nr_model_macro_fields,
					    &model->macros);
	  break;

	case model_data_record:
	  record = parse_model_data_record (isa, file, record,
					    nr_model_data_fields,
					    &model->data);
	  break;

	case model_static_record:
	  record = parse_function_record (file, record,
					  &model->statics,
					  NULL, 0 /*is internal */ ,
					  model);
	  break;

	case model_internal_record:
	  record = parse_function_record (file, record,
					  &model->internals,
					  NULL, 1 /*is internal */ ,
					  model);
	  break;

	case model_function_record:
	  record = parse_function_record (file, record,
					  &model->functions,
					  NULL, 0 /*is internal */ ,
					  model);
	  break;

	case insn_record:	/* instruction records */
	  {
	    insn_entry *new_insn;
	    char *format;
	    /* parse the instruction */
	    if (record->nr_fields < nr_insn_fields)
	      error (record->line,
		     "Incorrect nr of fields for insn record\n");
	    new_insn = ZALLOC (insn_entry);
	    new_insn->line = record->line;
	    filter_parse (&new_insn->flags,
			  record->field[record_filter_flags_field]);
	    /* save the format field.  Can't parse it until after the
	       filter-out checks.  Could be filtered out because the
	       format is invalid */
	    format = record->field[insn_word_field];
	    new_insn->format_name = record->field[insn_format_name_field];
	    if (options.format_name_filter != NULL
		&& !filter_is_member (options.format_name_filter,
				      new_insn->format_name))
	      error (new_insn->line,
		     "Unreconized instruction format name `%s'\n",
		     new_insn->format_name);
	    filter_parse (&new_insn->options,
			  record->field[insn_options_field]);
	    new_insn->name = record->field[insn_name_field];
	    record = table_read (file);
	    /* Parse any model/assember records */
	    new_insn->nr_models = model->nr_models;
	    new_insn->model =
	      NZALLOC (insn_model_entry *, model->nr_models + 1);
	    while (record != NULL)
	      {
		if (record_prefix_is (record, '*', nr_insn_model_fields))
		  parse_insn_model_record (file, record, new_insn, model);
		else
		  if (record_prefix_is (record, '"', nr_insn_mnemonic_fields))
		  parse_insn_mnemonic_record (file, record, new_insn);
		else
		  break;
		/* advance */
		record = table_read (file);
	      }
	    /* Parse the code record */
	    if (record != NULL && record->type == table_code_entry)
	      {
		new_insn->code = record;
		record = table_read (file);
	      }
	    else if (options.warn.unimplemented)
	      notify (new_insn->line, "unimplemented\n");
	    /* insert it */
	    if (!filter_is_subset (options.flags_filter, new_insn->flags))
	      {
		if (options.warn.discard)
		  notify (new_insn->line,
			  "Discarding instruction %s (flags-filter)\n",
			  new_insn->name);
	      }
	    else if (new_insn->processors != NULL
		     && options.model_filter != NULL
		     && !filter_is_common (options.model_filter,
					   new_insn->processors))
	      {
		/* only discard an instruction based in the processor
		   model when both the instruction and the options are
		   nonempty */
		if (options.warn.discard)
		  notify (new_insn->line,
			  "Discarding instruction %s (processor-model)\n",
			  new_insn->name);
	      }
	    else
	      {
		insn_entry **last;
		/* finish the parsing */
		parse_insn_words (new_insn, format);
		/* append it */
		last = &isa->insns;
		while (*last)
		  last = &(*last)->next;
		*last = new_insn;
		/* update global isa counters */
		isa->nr_insns++;
		if (isa->max_nr_words < new_insn->nr_words)
		  isa->max_nr_words = new_insn->nr_words;
		filter_add (&isa->flags, new_insn->flags);
		filter_add (&isa->options, new_insn->options);
	      }
	    break;
	  }

	case define_record:
	  record = parse_macro_record (file, record);
	  break;

	case unknown_record:
	case code_record:
	  error (record->line, "Unknown or unexpected entry\n");


	}
    }
  return isa;
}


void
print_insn_words (lf *file, insn_entry * insn)
{
  insn_word_entry *word = insn->words;
  if (word != NULL)
    {
      while (1)
	{
	  insn_field_entry *field = word->first;
	  while (1)
	    {
	      if (options.insn_specifying_widths)
		lf_printf (file, "%d.", field->width);
	      else
		lf_printf (file, "%d.",
			   i2target (options.hi_bit_nr, field->first));
	      switch (field->type)
		{
		case insn_field_invalid:
		  ASSERT (0);
		  break;
		case insn_field_int:
		  lf_printf (file, "0x%lx", (long) field->val_int);
		  break;
		case insn_field_reserved:
		  lf_printf (file, "/");
		  break;
		case insn_field_wild:
		  lf_printf (file, "*");
		  break;
		case insn_field_string:
		  lf_printf (file, "%s", field->val_string);
		  break;
		}
	      if (field == word->last)
		break;
	      field = field->next;
	      lf_printf (file, ",");
	    }
	  word = word->next;
	  if (word == NULL)
	    break;
	  lf_printf (file, "+");
	}
    }
}



void
function_entry_traverse (lf *file,
			 function_entry * functions,
			 function_entry_handler * handler, void *data)
{
  function_entry *function;
  for (function = functions; function != NULL; function = function->next)
    {
      handler (file, function, data);
    }
}

void
insn_table_traverse_insn (lf *file,
			  insn_table *isa,
			  insn_entry_handler * handler, void *data)
{
  insn_entry *insn;
  for (insn = isa->insns; insn != NULL; insn = insn->next)
    {
      handler (file, isa, insn, data);
    }
}


static void
dump_function_entry (lf *file,
		     char *prefix, function_entry * entry, char *suffix)
{
  lf_printf (file, "%s(function_entry *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      dump_line_ref (file, "\n(line ", entry->line, ")");
      dump_filter (file, "\n(flags ", entry->flags, ")");
      lf_printf (file, "\n(type \"%s\")", entry->type);
      lf_printf (file, "\n(name \"%s\")", entry->name);
      lf_printf (file, "\n(param \"%s\")", entry->param);
      dump_table_entry (file, "\n(code ", entry->code, ")");
      lf_printf (file, "\n(is_internal %d)", entry->is_internal);
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_function_entries (lf *file,
		       char *prefix, function_entry * entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  lf_indent (file, +1);
  while (entry != NULL)
    {
      dump_function_entry (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_indent (file, -1);
  lf_printf (file, "%s", suffix);
}

static char *
cache_entry_type_to_str (cache_entry_type type)
{
  switch (type)
    {
    case scratch_value:
      return "scratch";
    case cache_value:
      return "cache";
    case compute_value:
      return "compute";
    }
  ERROR ("Bad switch");
  return 0;
}

static void
dump_cache_entry (lf *file, char *prefix, cache_entry *entry, char *suffix)
{
  lf_printf (file, "%s(cache_entry *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      dump_line_ref (file, "\n(line ", entry->line, ")");
      dump_filter (file, "\n(flags ", entry->flags, ")");
      lf_printf (file, "\n(entry_type \"%s\")",
		 cache_entry_type_to_str (entry->entry_type));
      lf_printf (file, "\n(name \"%s\")", entry->name);
      dump_filter (file, "\n(original_fields ", entry->original_fields, ")");
      lf_printf (file, "\n(type \"%s\")", entry->type);
      lf_printf (file, "\n(expression \"%s\")", entry->expression);
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
    }
  lf_printf (file, "%s", suffix);
}

void
dump_cache_entries (lf *file, char *prefix, cache_entry *entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  lf_indent (file, +1);
  while (entry != NULL)
    {
      dump_cache_entry (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_indent (file, -1);
  lf_printf (file, "%s", suffix);
}

static void
dump_model_data (lf *file, char *prefix, model_data *entry, char *suffix)
{
  lf_printf (file, "%s(model_data *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", entry->line, ")");
      dump_filter (file, "\n(flags ", entry->flags, ")");
      dump_table_entry (file, "\n(entry ", entry->entry, ")");
      dump_table_entry (file, "\n(code ", entry->code, ")");
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", prefix);
}

static void
dump_model_datas (lf *file, char *prefix, model_data *entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  lf_indent (file, +1);
  while (entry != NULL)
    {
      dump_model_data (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_indent (file, -1);
  lf_printf (file, "%s", suffix);
}

static void
dump_model_entry (lf *file, char *prefix, model_entry *entry, char *suffix)
{
  lf_printf (file, "%s(model_entry *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", entry->line, ")");
      dump_filter (file, "\n(flags ", entry->flags, ")");
      lf_printf (file, "\n(name \"%s\")", entry->name);
      lf_printf (file, "\n(full_name \"%s\")", entry->full_name);
      lf_printf (file, "\n(unit_data \"%s\")", entry->unit_data);
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", prefix);
}

static void
dump_model_entries (lf *file, char *prefix, model_entry *entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  lf_indent (file, +1);
  while (entry != NULL)
    {
      dump_model_entry (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_indent (file, -1);
  lf_printf (file, "%s", suffix);
}


static void
dump_model_table (lf *file, char *prefix, model_table *entry, char *suffix)
{
  lf_printf (file, "%s(model_table *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      lf_indent (file, +1);
      dump_filter (file, "\n(processors ", entry->processors, ")");
      lf_printf (file, "\n(nr_models %d)", entry->nr_models);
      dump_model_entries (file, "\n(models ", entry->models, ")");
      dump_model_datas (file, "\n(macros ", entry->macros, ")");
      dump_model_datas (file, "\n(data ", entry->data, ")");
      dump_function_entries (file, "\n(statics ", entry->statics, ")");
      dump_function_entries (file, "\n(internals ", entry->functions, ")");
      dump_function_entries (file, "\n(functions ", entry->functions, ")");
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


static char *
insn_field_type_to_str (insn_field_type type)
{
  switch (type)
    {
    case insn_field_invalid:
      ASSERT (0);
      return "(invalid)";
    case insn_field_int:
      return "int";
    case insn_field_reserved:
      return "reserved";
    case insn_field_wild:
      return "wild";
    case insn_field_string:
      return "string";
    }
  ERROR ("bad switch");
  return 0;
}

void
dump_insn_field (lf *file,
		 char *prefix, insn_field_entry *field, char *suffix)
{
  char *sep = " ";
  lf_printf (file, "%s(insn_field_entry *) 0x%lx", prefix, (long) field);
  if (field != NULL)
    {
      lf_indent (file, +1);
      lf_printf (file, "%s(first %d)", sep, field->first);
      lf_printf (file, "%s(last %d)", sep, field->last);
      lf_printf (file, "%s(width %d)", sep, field->width);
      lf_printf (file, "%s(type %s)", sep,
		 insn_field_type_to_str (field->type));
      switch (field->type)
	{
	case insn_field_invalid:
	  ASSERT (0);
	  break;
	case insn_field_int:
	  lf_printf (file, "%s(val 0x%lx)", sep, (long) field->val_int);
	  break;
	case insn_field_reserved:
	  /* nothing output */
	  break;
	case insn_field_wild:
	  /* nothing output */
	  break;
	case insn_field_string:
	  lf_printf (file, "%s(val \"%s\")", sep, field->val_string);
	  break;
	}
      lf_printf (file, "%s(next 0x%lx)", sep, (long) field->next);
      lf_printf (file, "%s(prev 0x%lx)", sep, (long) field->prev);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

void
dump_insn_word_entry (lf *file,
		      char *prefix, insn_word_entry *word, char *suffix)
{
  lf_printf (file, "%s(insn_word_entry *) 0x%lx", prefix, (long) word);
  if (word != NULL)
    {
      int i;
      insn_field_entry *field;
      lf_indent (file, +1);
      lf_printf (file, "\n(first 0x%lx)", (long) word->first);
      lf_printf (file, "\n(last 0x%lx)", (long) word->last);
      lf_printf (file, "\n(bit");
      for (i = 0; i < options.insn_bit_size; i++)
	lf_printf (file, "\n ((value %d) (mask %d) (field 0x%lx))",
		   word->bit[i]->value, word->bit[i]->mask,
		   (long) word->bit[i]->field);
      lf_printf (file, ")");
      for (field = word->first; field != NULL; field = field->next)
	dump_insn_field (file, "\n(", field, ")");
      dump_filter (file, "\n(field_names ", word->field_names, ")");
      lf_printf (file, "\n(next 0x%lx)", (long) word->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_insn_word_entries (lf *file,
			char *prefix, insn_word_entry *word, char *suffix)
{
  lf_printf (file, "%s", prefix);
  while (word != NULL)
    {
      dump_insn_word_entry (file, "\n(", word, ")");
      word = word->next;
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_insn_model_entry (lf *file,
		       char *prefix, insn_model_entry *model, char *suffix)
{
  lf_printf (file, "%s(insn_model_entry *) 0x%lx", prefix, (long) model);
  if (model != NULL)
    {
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", model->line, ")");
      dump_filter (file, "\n(names ", model->names, ")");
      lf_printf (file, "\n(full_name \"%s\")", model->full_name);
      lf_printf (file, "\n(unit_data \"%s\")", model->unit_data);
      lf_printf (file, "\n(insn (insn_entry *) 0x%lx)", (long) model->insn);
      lf_printf (file, "\n(next (insn_model_entry *) 0x%lx)",
		 (long) model->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_insn_model_entries (lf *file,
			 char *prefix, insn_model_entry *model, char *suffix)
{
  lf_printf (file, "%s", prefix);
  while (model != NULL)
    {
      dump_insn_model_entry (file, "\n", model, "");
      model = model->next;
    }
  lf_printf (file, "%s", suffix);
}


static void
dump_insn_mnemonic_entry (lf *file,
			  char *prefix,
			  insn_mnemonic_entry *mnemonic, char *suffix)
{
  lf_printf (file, "%s(insn_mnemonic_entry *) 0x%lx", prefix,
	     (long) mnemonic);
  if (mnemonic != NULL)
    {
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", mnemonic->line, ")");
      lf_printf (file, "\n(format \"%s\")", mnemonic->format);
      lf_printf (file, "\n(condition \"%s\")", mnemonic->condition);
      lf_printf (file, "\n(insn (insn_entry *) 0x%lx)",
		 (long) mnemonic->insn);
      lf_printf (file, "\n(next (insn_mnemonic_entry *) 0x%lx)",
		 (long) mnemonic->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_insn_mnemonic_entries (lf *file,
			    char *prefix,
			    insn_mnemonic_entry *mnemonic, char *suffix)
{
  lf_printf (file, "%s", prefix);
  while (mnemonic != NULL)
    {
      dump_insn_mnemonic_entry (file, "\n", mnemonic, "");
      mnemonic = mnemonic->next;
    }
  lf_printf (file, "%s", suffix);
}

void
dump_insn_entry (lf *file, char *prefix, insn_entry * entry, char *suffix)
{
  lf_printf (file, "%s(insn_entry *) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      int i;
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", entry->line, ")");
      dump_filter (file, "\n(flags ", entry->flags, ")");
      lf_printf (file, "\n(nr_words %d)", entry->nr_words);
      dump_insn_word_entries (file, "\n(words ", entry->words, ")");
      lf_printf (file, "\n(word");
      for (i = 0; i < entry->nr_models; i++)
	lf_printf (file, " 0x%lx", (long) entry->word[i]);
      lf_printf (file, ")");
      dump_filter (file, "\n(field_names ", entry->field_names, ")");
      lf_printf (file, "\n(format_name \"%s\")", entry->format_name);
      dump_filter (file, "\n(options ", entry->options, ")");
      lf_printf (file, "\n(name \"%s\")", entry->name);
      lf_printf (file, "\n(nr_models %d)", entry->nr_models);
      dump_insn_model_entries (file, "\n(models ", entry->models, ")");
      lf_printf (file, "\n(model");
      for (i = 0; i < entry->nr_models; i++)
	lf_printf (file, " 0x%lx", (long) entry->model[i]);
      lf_printf (file, ")");
      dump_filter (file, "\n(processors ", entry->processors, ")");
      dump_insn_mnemonic_entries (file, "\n(mnemonics ", entry->mnemonics,
				  ")");
      dump_table_entry (file, "\n(code ", entry->code, ")");
      lf_printf (file, "\n(next 0x%lx)", (long) entry->next);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

static void
dump_insn_entries (lf *file, char *prefix, insn_entry * entry, char *suffix)
{
  lf_printf (file, "%s", prefix);
  lf_indent (file, +1);
  while (entry != NULL)
    {
      dump_insn_entry (file, "\n(", entry, ")");
      entry = entry->next;
    }
  lf_indent (file, -1);
  lf_printf (file, "%s", suffix);
}



void
dump_insn_table (lf *file, char *prefix, insn_table *isa, char *suffix)
{
  lf_printf (file, "%s(insn_table *) 0x%lx", prefix, (long) isa);
  if (isa != NULL)
    {
      lf_indent (file, +1);
      dump_cache_entries (file, "\n(caches ", isa->caches, ")");
      lf_printf (file, "\n(nr_insns %d)", isa->nr_insns);
      lf_printf (file, "\n(max_nr_words %d)", isa->max_nr_words);
      dump_insn_entries (file, "\n(insns ", isa->insns, ")");
      dump_function_entries (file, "\n(functions ", isa->functions, ")");
      dump_insn_entry (file, "\n(illegal_insn ", isa->illegal_insn, ")");
      dump_model_table (file, "\n(model ", isa->model, ")");
      dump_filter (file, "\n(flags ", isa->flags, ")");
      dump_filter (file, "\n(options ", isa->options, ")");
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}

#ifdef MAIN

igen_options options;

int
main (int argc, char **argv)
{
  insn_table *isa;
  lf *l;

  INIT_OPTIONS (options);

  if (argc == 3)
    filter_parse (&options.flags_filter, argv[2]);
  else if (argc != 2)
    error (NULL, "Usage: insn <insn-table> [ <filter-in> ]\n");

  isa = load_insn_table (argv[1], NULL);
  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-ld-insn");
  dump_insn_table (l, "(isa ", isa, ")\n");

  return 0;
}

#endif
