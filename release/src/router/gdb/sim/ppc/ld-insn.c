/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 2003 Andrew Cagney

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

static void
update_depth(insn_table *entry,
	     lf *file,
	     void *data,
	     insn *instruction,
	     int depth)
{
  int *max_depth = (int*)data;
  if (*max_depth < depth)
    *max_depth = depth;
}


int
insn_table_depth(insn_table *table)
{
  int depth = 0;
  insn_table_traverse_tree(table,
			   NULL,
			   &depth,
			   1,
			   NULL, /*start*/
			   update_depth,
			   NULL, /*end*/
			   NULL); /*padding*/
  return depth;
}


static insn_fields *
parse_insn_format(table_entry *entry,
		  char *format)
{
  char *chp;
  insn_fields *fields = ZALLOC(insn_fields);

  /* create a leading sentinal */
  fields->first = ZALLOC(insn_field);
  fields->first->first = -1;
  fields->first->last = -1;
  fields->first->width = 0;

  /* and a trailing sentinal */
  fields->last = ZALLOC(insn_field);
  fields->last->first = insn_bit_size;
  fields->last->last = insn_bit_size;
  fields->last->width = 0;

  /* link them together */
  fields->first->next = fields->last;
  fields->last->prev = fields->first;

  /* now work through the formats */
  chp = format;

  while (*chp != '\0') {
    char *start_pos;
    char *start_val;
    int strlen_val;
    int strlen_pos;
    insn_field *new_field;

    /* sanity check */
    if (!isdigit(*chp)) {
      error("%s:%d: missing position field at `%s'\n",
	    entry->file_name, entry->line_nr, chp);
    }

    /* break out the bit position */
    start_pos = chp;
    while (isdigit(*chp))
      chp++;
    strlen_pos = chp - start_pos;
    if (*chp == '.' && strlen_pos > 0)
      chp++;
    else {
      error("%s:%d: missing field value at %s\n",
	    entry->file_name, entry->line_nr, chp);
      break;
    }

    /* break out the value */
    start_val = chp;
    while ((*start_val == '/' && *chp == '/')
	   || (isdigit(*start_val) && isdigit(*chp))
	   || (isalpha(*start_val) && (isalnum(*chp) || *chp == '_')))
      chp++;
    strlen_val = chp - start_val;
    if (*chp == ',')
      chp++;
    else if (*chp != '\0' || strlen_val == 0) {
      error("%s:%d: missing field terminator at %s\n",
	    entry->file_name, entry->line_nr, chp);
      break;
    }

    /* create a new field and insert it */
    new_field = ZALLOC(insn_field);
    new_field->next = fields->last;
    new_field->prev = fields->last->prev;
    new_field->next->prev = new_field;
    new_field->prev->next = new_field;

    /* the value */
    new_field->val_string = (char*)zalloc(strlen_val+1);
    strncpy(new_field->val_string, start_val, strlen_val);
    if (isdigit(*new_field->val_string)) {
      new_field->val_int = a2i(new_field->val_string);
      new_field->is_int = 1;
    }
    else if (new_field->val_string[0] == '/') {
      new_field->is_slash = 1;
    }
    else {
      new_field->is_string = 1;
    }
    
    /* the pos */
    new_field->pos_string = (char*)zalloc(strlen_pos+1);
    strncpy(new_field->pos_string, start_pos, strlen_pos);
    new_field->first = target_a2i(hi_bit_nr, new_field->pos_string);
    new_field->last = new_field->next->first - 1; /* guess */
    new_field->width = new_field->last - new_field->first + 1; /* guess */
    new_field->prev->last = new_field->first-1; /*fix*/
    new_field->prev->width = new_field->first - new_field->prev->first; /*fix*/
  }

  /* fiddle first/last so that the sentinals `disapear' */
  ASSERT(fields->first->last < 0);
  ASSERT(fields->last->first >= insn_bit_size);
  fields->first = fields->first->next;
  fields->last = fields->last->prev;

  /* now go over this again, pointing each bit position at a field
     record */
  {
    int i;
    insn_field *field;
    field = fields->first;
    for (i = 0; i < insn_bit_size; i++) {
      while (field->last < i)
	field = field->next;
      fields->bits[i] = field;
    }
  }

  /* go over each of the fields, and compute a `value' for the insn */
  {
    insn_field *field;
    fields->value = 0;
    for (field = fields->first;
	 field->last < insn_bit_size;
	 field = field->next) {
      fields->value <<= field->width;
      if (field->is_int)
	fields->value |= field->val_int;
    }
  }
  return fields;
}


void
parse_include_entry (table *file,
                     table_entry *file_entry,
		     filter *filters,
		     table_include *includes)
{
  /* parse the include file_entry */
  if (file_entry->nr_fields < 4)
    error ("Incorrect nr fields for include record\n");
  /* process it */
  if (!is_filtered_out(file_entry->fields[include_flags], filters))
    {
      table_push (file, includes,
                file_entry->fields[include_path],
		file_entry->nr_fields, file_entry->nr_fields);
    }
}

static void
model_table_insert(insn_table *table,
		   table_entry *file_entry)
{
  int len;

  /* create a new model */
  model *new_model = ZALLOC(model);

  new_model->name = file_entry->fields[model_identifer];
  new_model->printable_name = file_entry->fields[model_name];
  new_model->insn_default = file_entry->fields[model_default];

  while (*new_model->insn_default && isspace(*new_model->insn_default))
    new_model->insn_default++;

  len = strlen(new_model->insn_default);
  if (max_model_fields_len < len)
    max_model_fields_len = len;

  /* append it to the end of the model list */
  if (last_model)
    last_model->next = new_model;
  else
    models = new_model;
  last_model = new_model;
}

static void
model_table_insert_specific(insn_table *table,
			    table_entry *file_entry,
			    insn **start_ptr,
			    insn **end_ptr)
{
  insn *ptr = ZALLOC(insn);
  ptr->file_entry = file_entry;
  if (*end_ptr)
    (*end_ptr)->next = ptr;
  else
    (*start_ptr) = ptr;
  (*end_ptr) = ptr;
}


static void
insn_table_insert_function(insn_table *table,
			   table_entry *file_entry)
{
  /* create a new function */
  insn *new_function = ZALLOC(insn);
  new_function->file_entry = file_entry;

  /* append it to the end of the function list */
  if (table->last_function)
    table->last_function->next = new_function;
  else
    table->functions = new_function;
  table->last_function = new_function;
}

extern void
insn_table_insert_insn(insn_table *table,
		       table_entry *file_entry,
		       insn_fields *fields)
{
  insn **ptr_to_cur_insn = &table->insns;
  insn *cur_insn = *ptr_to_cur_insn;
  table_model_entry *insn_model_ptr;
  model *model_ptr;

  /* create a new instruction */
  insn *new_insn = ZALLOC(insn);
  new_insn->file_entry = file_entry;
  new_insn->fields = fields;

  /* Check out any model information returned to make sure the model
     is correct.  */
  for(insn_model_ptr = file_entry->model_first; insn_model_ptr; insn_model_ptr = insn_model_ptr->next) {
    char *name = insn_model_ptr->fields[insn_model_name];
    int len = strlen (insn_model_ptr->fields[insn_model_fields]);

    while (len > 0 && isspace(*insn_model_ptr->fields[insn_model_fields])) {
      len--;
      insn_model_ptr->fields[insn_model_fields]++;
    }

    if (max_model_fields_len < len)
      max_model_fields_len = len;

    for(model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
      if (strcmp(name, model_ptr->printable_name) == 0) {

	/* Replace the name field with that of the global model, so that when we
	   want to print it out, we can just compare pointers.  */
	insn_model_ptr->fields[insn_model_name] = model_ptr->printable_name;
	break;
      }
    }

    if (!model_ptr)
      error("%s:%d: machine model `%s' was not known about\n",
	    file_entry->file_name, file_entry->line_nr, name);
  }

  /* insert it according to the order of the fields */
  while (cur_insn != NULL
	 && new_insn->fields->value >= cur_insn->fields->value) {
    ptr_to_cur_insn = &cur_insn->next;
    cur_insn = *ptr_to_cur_insn;
  }

  new_insn->next = cur_insn;
  *ptr_to_cur_insn = new_insn;

  table->nr_insn++;
}



insn_table *
load_insn_table(const char *file_name,
		decode_table *decode_rules,
		filter *filters,
		table_include *includes,
		cache_table **cache_rules)
{
  table *file = table_open(file_name, nr_insn_table_fields, nr_insn_model_table_fields);
  insn_table *table = ZALLOC(insn_table);
  table_entry *file_entry;
  table->opcode_rule = decode_rules;

  while ((file_entry = table_entry_read(file)) != NULL) {
    if (it_is("function", file_entry->fields[insn_flags])
	|| it_is("internal", file_entry->fields[insn_flags])) {
      insn_table_insert_function(table, file_entry);
    }
    else if ((it_is("function", file_entry->fields[insn_form])
	      || it_is("internal", file_entry->fields[insn_form]))
	     && !is_filtered_out(file_entry->fields[insn_flags], filters)) {
      /* Ok, this is evil.  Need to convert a new style function into
         an old style function.  Construct an old style table and then
         copy it back.  */
      char *fields[nr_insn_table_fields];
      memset (fields, 0, sizeof fields);
      fields[insn_flags] = file_entry->fields[insn_form];
      fields[function_type] = file_entry->fields[insn_name];
      fields[function_name] = file_entry->fields[insn_comment];
      fields[function_param] = file_entry->fields[insn_field_6];
      memcpy (file_entry->fields, fields,
	      sizeof (fields[0]) * file_entry->nr_fields);
      insn_table_insert_function(table, file_entry);
#if 0
      ":" "..."
       ":" <filter-flags>
       ":" <filter-models>
       ":" <typedef>
       ":" <name>
       [ ":" <parameter-list> ]
       <nl>
       [ <function-model> ]
       <code-block>
#endif
    }	     
    else if (it_is("model", file_entry->fields[insn_flags])) {
      model_table_insert(table, file_entry);
    }
    else if (it_is("model-macro", file_entry->fields[insn_flags])) {
      model_table_insert_specific(table, file_entry, &model_macros, &last_model_macro);
    }
    else if (it_is("model-function", file_entry->fields[insn_flags])) {
      model_table_insert_specific(table, file_entry, &model_functions, &last_model_function);
    }
    else if (it_is("model-internal", file_entry->fields[insn_flags])) {
      model_table_insert_specific(table, file_entry, &model_internal, &last_model_internal);
    }
    else if (it_is("model-static", file_entry->fields[insn_flags])) {
      model_table_insert_specific(table, file_entry, &model_static, &last_model_static);
    }
    else if (it_is("model-data", file_entry->fields[insn_flags])) {
      model_table_insert_specific(table, file_entry, &model_data, &last_model_data);
    }
    else if (it_is("include", file_entry->fields[insn_form])
             && !is_filtered_out(file_entry->fields[insn_flags], filters)) {
      parse_include_entry (file, file_entry, filters, includes);
    }
    else if ((it_is("cache", file_entry->fields[insn_form])
	      || it_is("compute", file_entry->fields[insn_form])
	      || it_is("scratch", file_entry->fields[insn_form]))
	     && !is_filtered_out(file_entry->fields[insn_flags], filters)) {
      append_cache_rule (cache_rules,
			 file_entry->fields[insn_form], /* type */
			 file_entry->fields[cache_name],
			 file_entry->fields[cache_derived_name],
			 file_entry->fields[cache_type_def],
			 file_entry->fields[cache_expression],
			 file_entry);
    }
    else {
      insn_fields *fields;
      /* skip instructions that aren't relevant to the mode */
      if (is_filtered_out(file_entry->fields[insn_flags], filters)) {
	fprintf(stderr, "Dropping %s - %s\n",
		file_entry->fields[insn_name],
		file_entry->fields[insn_flags]);
      }
      else {
	/* create/insert the new instruction */
	fields = parse_insn_format(file_entry,
				   file_entry->fields[insn_format]);
	insn_table_insert_insn(table, file_entry, fields);
      }
    }
  }
  return table;
}


extern void
insn_table_traverse_tree(insn_table *table,
			 lf *file,
			 void *data,
			 int depth,
			 leaf_handler *start,
			 insn_handler *leaf,
			 leaf_handler *end,
			 padding_handler *padding)
{
  insn_table *entry;
  int entry_nr;
  
  ASSERT(table != NULL
	 && table->opcode != NULL
	 && table->nr_entries > 0
	 && table->entries != 0);

  if (start != NULL && depth >= 0)
    start(table, file, data, depth);

  for (entry_nr = 0, entry = table->entries;
       entry_nr < (table->opcode->is_boolean
		   ? 2
		   : (1 << (table->opcode->last - table->opcode->first + 1)));
       entry_nr ++) {
    if (entry == NULL
	|| (!table->opcode->is_boolean
	    && entry_nr < entry->opcode_nr)) {
      if (padding != NULL && depth >= 0)
	padding(table, file, data, depth, entry_nr);
    }
    else {
      ASSERT(entry != NULL && (entry->opcode_nr == entry_nr
			       || table->opcode->is_boolean));
      if (entry->opcode != NULL && depth != 0) {
	insn_table_traverse_tree(entry, file, data, depth+1,
				 start, leaf, end, padding);
      }
      else if (depth >= 0) {
	if (leaf != NULL)
	  leaf(entry, file, data, entry->insns, depth);
      }
      entry = entry->sibling;
    }
  }
  if (end != NULL && depth >= 0)
    end(table, file, data, depth);
}


extern void
insn_table_traverse_function(insn_table *table,
			     lf *file,
			     void *data,
			     function_handler *leaf)
{
  insn *function;
  for (function = table->functions;
       function != NULL;
       function = function->next) {
    leaf(table, file, data, function->file_entry);
  }
}

extern void
insn_table_traverse_insn(insn_table *table,
			 lf *file,
			 void *data,
			 insn_handler *handler)
{
  insn *instruction;
  for (instruction = table->insns;
       instruction != NULL;
       instruction = instruction->next) {
    handler(table, file, data, instruction, 0);
  }
}


/****************************************************************/

typedef enum {
  field_constant_int = 1,
  field_constant_slash = 2,
  field_constant_string = 3
} constant_field_types;


static int
insn_field_is_constant(insn_field *field,
		       decode_table *rule)
{
  /* field is an integer */
  if (field->is_int)
    return field_constant_int;
  /* field is `/' and treating that as a constant */
  if (field->is_slash && rule->force_slash)
    return field_constant_slash;
  /* field, though variable is on the list */
  if (field->is_string && rule->force_expansion != NULL) {
    char *forced_fields = rule->force_expansion;
    while (*forced_fields != '\0') {
      int field_len;
      char *end = strchr(forced_fields, ',');
      if (end == NULL)
	field_len = strlen(forced_fields);
      else
	field_len = end-forced_fields;
      if (strncmp(forced_fields, field->val_string, field_len) == 0
	  && field->val_string[field_len] == '\0')
	return field_constant_string;
      forced_fields += field_len;
      if (*forced_fields == ',')
	forced_fields++;
    }
  }
  return 0;
}


static opcode_field *
insn_table_find_opcode_field(insn *insns,
			     decode_table *rule,
			     int string_only)
{
  opcode_field *curr_opcode = ZALLOC(opcode_field);
  insn *entry;
  ASSERT(rule);

  curr_opcode->first = insn_bit_size;
  curr_opcode->last = -1;
  for (entry = insns; entry != NULL; entry = entry->next) {
    insn_fields *fields = entry->fields;
    opcode_field new_opcode;

    /* find a start point for the opcode field */
    new_opcode.first = rule->first;
    while (new_opcode.first <= rule->last
	   && (!string_only
	       || insn_field_is_constant(fields->bits[new_opcode.first],
					 rule) != field_constant_string)
	   && (string_only
	       || !insn_field_is_constant(fields->bits[new_opcode.first],
					  rule)))
      new_opcode.first = fields->bits[new_opcode.first]->last + 1;
    ASSERT(new_opcode.first > rule->last
	   || (string_only
	       && insn_field_is_constant(fields->bits[new_opcode.first],
					 rule) == field_constant_string)
	   || (!string_only
	       && insn_field_is_constant(fields->bits[new_opcode.first],
					 rule)));
  
    /* find the end point for the opcode field */
    new_opcode.last = rule->last;
    while (new_opcode.last >= rule->first
	   && (!string_only
	       || insn_field_is_constant(fields->bits[new_opcode.last],
					 rule) != field_constant_string)
	   && (string_only
	       || !insn_field_is_constant(fields->bits[new_opcode.last],
					  rule)))
      new_opcode.last = fields->bits[new_opcode.last]->first - 1;
    ASSERT(new_opcode.last < rule->first
	   || (string_only
	       && insn_field_is_constant(fields->bits[new_opcode.last],
					 rule) == field_constant_string)
	   || (!string_only
	       && insn_field_is_constant(fields->bits[new_opcode.last],
					 rule)));

    /* now see if our current opcode needs expanding */
    if (new_opcode.first <= rule->last
	&& curr_opcode->first > new_opcode.first)
      curr_opcode->first = new_opcode.first;
    if (new_opcode.last >= rule->first
	&& curr_opcode->last < new_opcode.last)
      curr_opcode->last = new_opcode.last;
    
  }

  /* was any thing interesting found? */
  if (curr_opcode->first > rule->last) {
    ASSERT(curr_opcode->last < rule->first);
    return NULL;
  }
  ASSERT(curr_opcode->last >= rule->first);
  ASSERT(curr_opcode->first <= rule->last);

  /* if something was found, check it includes the forced field range */
  if (!string_only
      && curr_opcode->first > rule->force_first) {
    curr_opcode->first = rule->force_first;
  }
  if (!string_only
      && curr_opcode->last < rule->force_last) {
    curr_opcode->last = rule->force_last;
  }
  /* handle special case elminating any need to do shift after mask */
  if (string_only
      && rule->force_last == insn_bit_size-1) {
    curr_opcode->last = insn_bit_size-1;
  }

  /* handle any special cases */
  switch (rule->type) {
  case normal_decode_rule:
    /* let the above apply */
    break;
  case expand_forced_rule:
    /* expand a limited nr of bits, ignoring the rest */
    curr_opcode->first = rule->force_first;
    curr_opcode->last = rule->force_last;
    break;
  case boolean_rule:
    curr_opcode->is_boolean = 1;
    curr_opcode->boolean_constant = rule->special_constant;
    break;
  default:
    error("Something is going wrong\n");
  }

  return curr_opcode;
}


static void
insn_table_insert_expanded(insn_table *table,
			   insn *old_insn,
			   int new_opcode_nr,
			   insn_bits *new_bits)
{
  insn_table **ptr_to_cur_entry = &table->entries;
  insn_table *cur_entry = *ptr_to_cur_entry;

  /* find the new table for this entry */
  while (cur_entry != NULL
	 && cur_entry->opcode_nr < new_opcode_nr) {
    ptr_to_cur_entry = &cur_entry->sibling;
    cur_entry = *ptr_to_cur_entry;
  }

  if (cur_entry == NULL || cur_entry->opcode_nr != new_opcode_nr) {
    insn_table *new_entry = ZALLOC(insn_table);
    new_entry->opcode_nr = new_opcode_nr;
    new_entry->expanded_bits = new_bits;
    new_entry->opcode_rule = table->opcode_rule->next;
    new_entry->sibling = cur_entry;
    new_entry->parent = table;
    *ptr_to_cur_entry = new_entry;
    cur_entry = new_entry;
    table->nr_entries++;
  }
  /* ASSERT new_bits == cur_entry bits */
  ASSERT(cur_entry != NULL && cur_entry->opcode_nr == new_opcode_nr);
  insn_table_insert_insn(cur_entry,
			 old_insn->file_entry,
			 old_insn->fields);
}

static void
insn_table_expand_opcode(insn_table *table,
			 insn *instruction,
			 int field_nr,
			 int opcode_nr,
			 insn_bits *bits)
{

  if (field_nr > table->opcode->last) {
    insn_table_insert_expanded(table, instruction, opcode_nr, bits);
  }
  else {
    insn_field *field = instruction->fields->bits[field_nr];
    if (field->is_int || field->is_slash) {
      ASSERT(field->first >= table->opcode->first
	     && field->last <= table->opcode->last);
      insn_table_expand_opcode(table, instruction, field->last+1,
			       ((opcode_nr << field->width) + field->val_int),
			       bits);
    }
    else {
      int val;
      int last_pos = ((field->last < table->opcode->last)
			? field->last : table->opcode->last);
      int first_pos = ((field->first > table->opcode->first)
			 ? field->first : table->opcode->first);
      int width = last_pos - first_pos + 1;
      int last_val = (table->opcode->is_boolean
		      ? 2 : (1 << width));
      for (val = 0; val < last_val; val++) {
	insn_bits *new_bits = ZALLOC(insn_bits);
	new_bits->field = field;
	new_bits->value = val;
	new_bits->last = bits;
	new_bits->opcode = table->opcode;
	insn_table_expand_opcode(table, instruction, last_pos+1,
				 ((opcode_nr << width) | val),
				 new_bits);
      }
    }
  }
}

static void
insn_table_insert_expanding(insn_table *table,
			    insn *entry)
{
  insn_table_expand_opcode(table,
			   entry,
			   table->opcode->first,
			   0,
			   table->expanded_bits);
}


extern void
insn_table_expand_insns(insn_table *table)
{

  ASSERT(table->nr_insn >= 1);

  /* determine a valid opcode */
  while (table->opcode_rule) {
    /* specials only for single instructions */
    if ((table->nr_insn > 1
	 && table->opcode_rule->special_mask == 0
	 && table->opcode_rule->type == normal_decode_rule)
	|| (table->nr_insn == 1
	    && table->opcode_rule->special_mask != 0
	    && ((table->insns->fields->value
		 & table->opcode_rule->special_mask)
		== table->opcode_rule->special_value))
	|| (generate_expanded_instructions
	    && table->opcode_rule->special_mask == 0
	    && table->opcode_rule->type == normal_decode_rule))
      table->opcode =
	insn_table_find_opcode_field(table->insns,
				     table->opcode_rule,
				     table->nr_insn == 1/*string*/
				     );
    if (table->opcode != NULL)
      break;
    table->opcode_rule = table->opcode_rule->next;
  }

  /* did we find anything */
  if (table->opcode == NULL) {
    return;
  }
  ASSERT(table->opcode != NULL);

  /* back link what we found to its parent */
  if (table->parent != NULL) {
    ASSERT(table->parent->opcode != NULL);
    table->opcode->parent = table->parent->opcode;
  }

  /* expand the raw instructions according to the opcode */
  {
    insn *entry;
    for (entry = table->insns; entry != NULL; entry = entry->next) {
      insn_table_insert_expanding(table, entry);
    }
  }

  /* and do the same for the sub entries */
  {
    insn_table *entry;
    for (entry = table->entries; entry != NULL; entry =  entry->sibling) {
      insn_table_expand_insns(entry);
    }
  }
}




#ifdef MAIN

static void
dump_insn_field(insn_field *field,
		int indent)
{

  printf("(insn_field*)0x%x\n", (unsigned)field);

  dumpf(indent, "(first %d)\n", field->first);

  dumpf(indent, "(last %d)\n", field->last);

  dumpf(indent, "(width %d)\n", field->width);

  if (field->is_int)
    dumpf(indent, "(is_int %d)\n", field->val_int);

  if (field->is_slash)
    dumpf(indent, "(is_slash)\n");

  if (field->is_string)
    dumpf(indent, "(is_string `%s')\n", field->val_string);
  
  dumpf(indent, "(next 0x%x)\n", field->next);
  
  dumpf(indent, "(prev 0x%x)\n", field->prev);
  

}

static void
dump_insn_fields(insn_fields *fields,
		 int indent)
{
  int i;

  printf("(insn_fields*)%p\n", fields);

  dumpf(indent, "(first 0x%x)\n", fields->first);
  dumpf(indent, "(last 0x%x)\n", fields->last);

  dumpf(indent, "(value 0x%x)\n", fields->value);

  for (i = 0; i < insn_bit_size; i++) {
    dumpf(indent, "(bits[%d] ", i, fields->bits[i]);
    dump_insn_field(fields->bits[i], indent+1);
    dumpf(indent, " )\n");
  }

}


static void
dump_opcode_field(opcode_field *field, int indent, int levels)
{
  printf("(opcode_field*)%p\n", field);
  if (levels && field != NULL) {
    dumpf(indent, "(first %d)\n", field->first);
    dumpf(indent, "(last %d)\n", field->last);
    dumpf(indent, "(is_boolean %d)\n", field->is_boolean);
    dumpf(indent, "(parent ");
    dump_opcode_field(field->parent, indent, levels-1);
  }
}


static void
dump_insn_bits(insn_bits *bits, int indent, int levels)
{
  printf("(insn_bits*)%p\n", bits);

  if (levels && bits != NULL) {
    dumpf(indent, "(value %d)\n", bits->value);
    dumpf(indent, "(opcode ");
    dump_opcode_field(bits->opcode, indent+1, 0);
    dumpf(indent, " )\n");
    dumpf(indent, "(field ");
    dump_insn_field(bits->field, indent+1);
    dumpf(indent, " )\n");
    dumpf(indent, "(last ");
    dump_insn_bits(bits->last, indent+1, levels-1);
  }
}



static void
dump_insn(insn *entry, int indent, int levels)
{
  printf("(insn*)%p\n", entry);

  if (levels && entry != NULL) {

    dumpf(indent, "(file_entry ");
    dump_table_entry(entry->file_entry, indent+1);
    dumpf(indent, " )\n");

    dumpf(indent, "(fields ");
    dump_insn_fields(entry->fields, indent+1);
    dumpf(indent, " )\n");

    dumpf(indent, "(next ");
    dump_insn(entry->next, indent+1, levels-1);
    dumpf(indent, " )\n");

  }

}


static void
dump_insn_table(insn_table *table,
		int indent, int levels)
{

  printf("(insn_table*)%p\n", table);

  if (levels && table != NULL) {

    dumpf(indent, "(opcode_nr %d)\n", table->opcode_nr);

    dumpf(indent, "(expanded_bits ");
    dump_insn_bits(table->expanded_bits, indent+1, -1);
    dumpf(indent, " )\n");

    dumpf(indent, "(int nr_insn %d)\n", table->nr_insn);

    dumpf(indent, "(insns ");
    dump_insn(table->insns, indent+1, table->nr_insn);
    dumpf(indent, " )\n");

    dumpf(indent, "(opcode_rule ");
    dump_decode_rule(table->opcode_rule, indent+1);
    dumpf(indent, " )\n");

    dumpf(indent, "(opcode ");
    dump_opcode_field(table->opcode, indent+1, 1);
    dumpf(indent, " )\n");

    dumpf(indent, "(nr_entries %d)\n", table->entries);
    dumpf(indent, "(entries ");
    dump_insn_table(table->entries, indent+1, table->nr_entries);
    dumpf(indent, " )\n");

    dumpf(indent, "(sibling ", table->sibling);
    dump_insn_table(table->sibling, indent+1, levels-1);
    dumpf(indent, " )\n");

    dumpf(indent, "(parent ", table->parent);
    dump_insn_table(table->parent, indent+1, 0);
    dumpf(indent, " )\n");

  }
}

int insn_bit_size = max_insn_bit_size;
int hi_bit_nr;
int generate_expanded_instructions;

int
main(int argc, char **argv)
{
  filter *filters = NULL;
  decode_table *decode_rules = NULL;
  insn_table *instructions = NULL;
  cache_table *cache_rules = NULL;

  if (argc != 5)
    error("Usage: insn <filter> <hi-bit-nr> <decode-table> <insn-table>\n");

  filters = new_filter(argv[1], filters);
  hi_bit_nr = a2i(argv[2]);
  ASSERT(hi_bit_nr < insn_bit_size);
  decode_rules = load_decode_table(argv[3], hi_bit_nr);
  instructions = load_insn_table(argv[4], decode_rules, filters, NULL,
				 &cache_rules);
  insn_table_expand_insns(instructions);

  dump_insn_table(instructions, 0, -1);
  return 0;
}

#endif
