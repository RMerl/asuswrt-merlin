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

#include "gen-semantics.h"
#include "gen-idecode.h"
#include "gen-icache.h"



static void
print_icache_function_header (lf *file,
			      const char *basename,
			      const char *format_name,
			      opcode_bits *expanded_bits,
			      int is_function_definition,
			      int nr_prefetched_words)
{
  lf_printf (file, "\n");
  lf_print__function_type_function (file, print_icache_function_type,
				    "EXTERN_ICACHE", " ");
  print_function_name (file,
		       basename, format_name, NULL,
		       expanded_bits, function_name_prefix_icache);
  lf_printf (file, "\n(");
  print_icache_function_formal (file, nr_prefetched_words);
  lf_printf (file, ")");
  if (!is_function_definition)
    lf_printf (file, ";");
  lf_printf (file, "\n");
}


void
print_icache_declaration (lf *file,
			  insn_entry * insn,
			  opcode_bits *expanded_bits,
			  insn_opcodes *opcodes, int nr_prefetched_words)
{
  print_icache_function_header (file,
				insn->name,
				insn->format_name,
				expanded_bits,
				0 /* is not function definition */ ,
				nr_prefetched_words);
}



static void
print_icache_extraction (lf *file,
			 const char *format_name,
			 cache_entry_type cache_type,
			 const char *entry_name,
			 const char *entry_type,
			 const char *entry_expression,
			 char *single_insn_field,
			 line_ref *line,
			 insn_field_entry *cur_field,
			 opcode_bits *expanded_bits,
			 icache_decl_type what_to_declare,
			 icache_body_type what_to_do)
{
  const char *expression;
  opcode_bits *bits;
  char *reason;
  ASSERT (format_name != NULL);
  ASSERT (entry_name != NULL);

  /* figure out exactly what should be going on here */
  switch (cache_type)
    {
    case scratch_value:
      if ((what_to_do & put_values_in_icache)
	  || what_to_do == do_not_use_icache)
	{
	  reason = "scratch";
	  what_to_do = do_not_use_icache;
	}
      else
	return;
      break;
    case compute_value:
      if ((what_to_do & get_values_from_icache)
	  || what_to_do == do_not_use_icache)
	{
	  reason = "compute";
	  what_to_do = do_not_use_icache;
	}
      else
	return;
      break;
    case cache_value:
      if ((what_to_declare != undef_variables)
	  || !(what_to_do & put_values_in_icache))
	{
	  reason = "cache";
	  what_to_declare = ((what_to_do & put_values_in_icache)
			     ? declare_variables : what_to_declare);
	}
      else
	return;
      break;
    default:
      abort ();			/* Bad switch.  */
    }

  /* For the type, default to a simple unsigned */
  if (entry_type == NULL || strlen (entry_type) == 0)
    entry_type = "unsigned";

  /* look through the set of expanded sub fields to see if this field
     has been given a constant value */
  for (bits = expanded_bits; bits != NULL; bits = bits->next)
    {
      if (bits->field == cur_field)
	break;
    }

  /* Define a storage area for the cache element */
  switch (what_to_declare)
    {
    case undef_variables:
      /* We've finished with the #define value - destory it */
      lf_indent_suppress (file);
      lf_printf (file, "#undef %s\n", entry_name);
      return;
    case define_variables:
      /* Using direct access for this entry, clear any prior
         definition, then define it */
      lf_indent_suppress (file);
      lf_printf (file, "#undef %s\n", entry_name);
      /* Don't type cast pointer types! */
      lf_indent_suppress (file);
      if (strchr (entry_type, '*') != NULL)
	lf_printf (file, "#define %s (", entry_name);
      else
	lf_printf (file, "#define %s ((%s) ", entry_name, entry_type);
      break;
    case declare_variables:
      /* using variables to define the value */
      if (line != NULL)
	lf_print__line_ref (file, line);
      lf_printf (file, "%s const %s UNUSED = ", entry_type, entry_name);
      break;
    }


  /* define a value for that storage area as determined by what is in
     the cache */
  if (bits != NULL
      && single_insn_field != NULL
      && strcmp (entry_name, single_insn_field) == 0
      && strcmp (entry_name, cur_field->val_string) == 0
      && ((bits->opcode->is_boolean && bits->value == 0)
	  || (!bits->opcode->is_boolean)))
    {
      /* The cache rule is specifying what to do with a simple
         instruction field.

         Because of instruction expansion, the field is either a
         constant value or equal to the specified constant (boolean
         comparison). (The latter indicated by bits->value == 0).

         The case of a field not being equal to the specified boolean
         value is handled later. */
      expression = "constant field";
      ASSERT (bits->field == cur_field);
      if (bits->opcode->is_boolean)
	{
	  ASSERT (bits->value == 0);
	  lf_printf (file, "%d", bits->opcode->boolean_constant);
	}
      else if (bits->opcode->last < bits->field->last)
	{
	  lf_printf (file, "%d",
		     bits->value << (bits->field->last - bits->opcode->last));
	}
      else
	{
	  lf_printf (file, "%d", bits->value);
	}
    }
  else if (bits != NULL
	   && single_insn_field != NULL
	   && strncmp (entry_name,
		       single_insn_field,
		       strlen (single_insn_field)) == 0
	   && strncmp (entry_name + strlen (single_insn_field),
		       "_is_",
		       strlen ("_is_")) == 0
	   && ((bits->opcode->is_boolean
		&& ((unsigned)
		    atol (entry_name + strlen (single_insn_field) +
			  strlen ("_is_")) == bits->opcode->boolean_constant))
	       || (!bits->opcode->is_boolean)))
    {
      /* The cache rule defines an entry for the comparison between a
         single instruction field and a constant.  The value of the
         comparison in someway matches that of the opcode field that
         was made constant through expansion. */
      expression = "constant compare";
      if (bits->opcode->is_boolean)
	{
	  lf_printf (file, "%d /* %s == %d */",
		     bits->value == 0,
		     single_insn_field, bits->opcode->boolean_constant);
	}
      else if (bits->opcode->last < bits->field->last)
	{
	  lf_printf (file, "%d /* %s == %d */",
		     (atol
		      (entry_name + strlen (single_insn_field) +
		       strlen ("_is_")) ==
		      (bits->
		       value << (bits->field->last - bits->opcode->last))),
		     single_insn_field,
		     (bits->
		      value << (bits->field->last - bits->opcode->last)));
	}
      else
	{
	  lf_printf (file, "%d /* %s == %d */",
		     (atol
		      (entry_name + strlen (single_insn_field) +
		       strlen ("_is_")) == bits->value), single_insn_field,
		     bits->value);
	}
    }
  else
    {
      /* put the field in the local variable, possibly also enter it
         into the cache */
      expression = "extraction";
      /* handle the cache */
      if ((what_to_do & get_values_from_icache)
	  || (what_to_do & put_values_in_icache))
	{
	  lf_printf (file, "cache_entry->crack.%s.%s",
		     format_name, entry_name);
	  if (what_to_do & put_values_in_icache)	/* also put it in the cache? */
	    {
	      lf_printf (file, " = ");
	    }
	}
      if ((what_to_do & put_values_in_icache)
	  || what_to_do == do_not_use_icache)
	{
	  if (cur_field != NULL)
	    {
	      if (entry_expression != NULL && strlen (entry_expression) > 0)
		error (line,
		       "Instruction field entry with nonempty expression\n");
	      if (cur_field->first == 0
		  && cur_field->last == options.insn_bit_size - 1)
		lf_printf (file, "(instruction_%d)", cur_field->word_nr);
	      else if (cur_field->last == options.insn_bit_size - 1)
		lf_printf (file, "MASKED%d (instruction_%d, %d, %d)",
			   options.insn_bit_size,
			   cur_field->word_nr,
			   i2target (options.hi_bit_nr, cur_field->first),
			   i2target (options.hi_bit_nr, cur_field->last));
	      else
		lf_printf (file, "EXTRACTED%d (instruction_%d, %d, %d)",
			   options.insn_bit_size,
			   cur_field->word_nr,
			   i2target (options.hi_bit_nr, cur_field->first),
			   i2target (options.hi_bit_nr, cur_field->last));
	    }
	  else
	    {
	      lf_printf (file, "%s", entry_expression);
	    }
	}
    }

  switch (what_to_declare)
    {
    case define_variables:
      lf_printf (file, ")");
      break;
    case undef_variables:
      break;
    case declare_variables:
      lf_printf (file, ";");
      break;
    }

  ASSERT (reason != NULL && expression != NULL);
  lf_printf (file, " /* %s - %s */\n", reason, expression);
}


void
print_icache_body (lf *file,
		   insn_entry * instruction,
		   opcode_bits *expanded_bits,
		   cache_entry *cache_rules,
		   icache_decl_type what_to_declare,
		   icache_body_type what_to_do, int nr_prefetched_words)
{
  /* extract instruction fields */
  lf_printf (file, "/* Extraction: %s\n", instruction->name);
  lf_printf (file, "     ");
  switch (what_to_declare)
    {
    case define_variables:
      lf_printf (file, "#define");
      break;
    case declare_variables:
      lf_printf (file, "declare");
      break;
    case undef_variables:
      lf_printf (file, "#undef");
      break;
    }
  lf_printf (file, " ");
  switch (what_to_do)
    {
    case get_values_from_icache:
      lf_printf (file, "get-values-from-icache");
      break;
    case put_values_in_icache:
      lf_printf (file, "put-values-in-icache");
      break;
    case both_values_and_icache:
      lf_printf (file, "get-values-from-icache|put-values-in-icache");
      break;
    case do_not_use_icache:
      lf_printf (file, "do-not-use-icache");
      break;
    }
  lf_printf (file, "\n     ");
  print_insn_words (file, instruction);
  lf_printf (file, " */\n");

  /* pass zero - fetch from memory any missing instructions.

     Some of the instructions will have already been fetched (in the
     instruction array), others will still need fetching. */
  switch (what_to_do)
    {
    case get_values_from_icache:
      break;
    case put_values_in_icache:
    case both_values_and_icache:
    case do_not_use_icache:
      {
	int word_nr;
	switch (what_to_declare)
	  {
	  case undef_variables:
	    break;
	  case define_variables:
	  case declare_variables:
	    for (word_nr = nr_prefetched_words;
		 word_nr < instruction->nr_words; word_nr++)
	      {
		/* FIXME - should be using print_icache_extraction? */
		lf_printf (file,
			   "%sinstruction_word instruction_%d UNUSED = ",
			   options.module.global.prefix.l, word_nr);
		lf_printf (file, "IMEM%d_IMMED (cia, %d)",
			   options.insn_bit_size, word_nr);
		lf_printf (file, ";\n");
	      }
	  }
      }
    }

  /* if putting the instruction words in the cache, define references
     for them */
  if (options.gen.insn_in_icache)
    {
      /* FIXME: is the instruction_word type correct? */
      print_icache_extraction (file, instruction->format_name, cache_value, "insn",	/* name */
			       "instruction_word",	/* type */
			       "instruction",	/* expression */
			       NULL,	/* origin */
			       NULL,	/* line */
			       NULL, NULL, what_to_declare, what_to_do);
    }
  lf_printf (file, "\n");

  /* pass one - process instruction fields.

     If there is no cache rule, the default is to enter the field into
     the cache */
  {
    insn_word_entry *word;
    for (word = instruction->words; word != NULL; word = word->next)
      {
	insn_field_entry *cur_field;
	for (cur_field = word->first;
	     cur_field->first < options.insn_bit_size;
	     cur_field = cur_field->next)
	  {
	    if (cur_field->type == insn_field_string)
	      {
		cache_entry *cache_rule;
		cache_entry_type value_type = cache_value;
		line_ref *value_line = instruction->line;
		/* check the cache table to see if it contains a rule
		   overriding the default cache action for an
		   instruction field */
		for (cache_rule = cache_rules;
		     cache_rule != NULL; cache_rule = cache_rule->next)
		  {
		    if (filter_is_subset (instruction->field_names,
					  cache_rule->original_fields)
			&& strcmp (cache_rule->name,
				   cur_field->val_string) == 0)
		      {
			value_type = cache_rule->entry_type;
			value_line = cache_rule->line;
			if (value_type == compute_value)
			  {
			    options.warning (cache_rule->line,
					     "instruction field of type `compute' changed to `cache'\n");
			    cache_rule->entry_type = cache_value;
			  }
			break;
		      }
		  }
		/* Define an entry for the field within the
		   instruction */
		print_icache_extraction (file, instruction->format_name, value_type, cur_field->val_string,	/* name */
					 NULL,	/* type */
					 NULL,	/* expression */
					 cur_field->val_string,	/* insn field */
					 value_line,
					 cur_field,
					 expanded_bits,
					 what_to_declare, what_to_do);
	      }
	  }
      }
  }

  /* pass two - any cache fields not processed above */
  {
    cache_entry *cache_rule;
    for (cache_rule = cache_rules;
	 cache_rule != NULL; cache_rule = cache_rule->next)
      {
	if (filter_is_subset (instruction->field_names,
			      cache_rule->original_fields)
	    && !filter_is_member (instruction->field_names, cache_rule->name))
	  {
	    char *single_field =
	      filter_next (cache_rule->original_fields, "");
	    if (filter_next (cache_rule->original_fields, single_field) !=
		NULL)
	      single_field = NULL;
	    print_icache_extraction (file, instruction->format_name, cache_rule->entry_type, cache_rule->name, cache_rule->type, cache_rule->expression, single_field, cache_rule->line, NULL,	/* cur_field */
				     expanded_bits,
				     what_to_declare, what_to_do);
	  }
      }
  }

  lf_print__internal_ref (file);
}



typedef struct _form_fields form_fields;
struct _form_fields
{
  char *name;
  filter *fields;
  form_fields *next;
};

static form_fields *
insn_table_cache_fields (insn_table *isa)
{
  form_fields *forms = NULL;
  insn_entry *insn;
  for (insn = isa->insns; insn != NULL; insn = insn->next)
    {
      form_fields **form = &forms;
      while (1)
	{
	  if (*form == NULL)
	    {
	      /* new format name, add it */
	      form_fields *new_form = ZALLOC (form_fields);
	      new_form->name = insn->format_name;
	      filter_add (&new_form->fields, insn->field_names);
	      *form = new_form;
	      break;
	    }
	  else if (strcmp ((*form)->name, insn->format_name) == 0)
	    {
	      /* already present, add field names to the existing list */
	      filter_add (&(*form)->fields, insn->field_names);
	      break;
	    }
	  form = &(*form)->next;
	}
    }
  return forms;
}



extern void
print_icache_struct (lf *file, insn_table *isa, cache_entry *cache_rules)
{
  /* Create a list of all the different instruction formats with their
     corresponding field names. */
  form_fields *formats = insn_table_cache_fields (isa);

  lf_printf (file, "\n");
  lf_printf (file, "#define WITH_%sIDECODE_CACHE_SIZE %d\n",
	     options.module.global.prefix.u,
	     (options.gen.icache ? options.gen.icache_size : 0));
  lf_printf (file, "\n");

  /* create an instruction cache if being used */
  if (options.gen.icache)
    {
      lf_printf (file, "typedef struct _%sidecode_cache {\n",
		 options.module.global.prefix.l);
      lf_indent (file, +2);
      {
	form_fields *format;
	lf_printf (file, "unsigned_word address;\n");
	lf_printf (file, "void *semantic;\n");
	lf_printf (file, "union {\n");
	lf_indent (file, +2);
	for (format = formats; format != NULL; format = format->next)
	  {
	    lf_printf (file, "struct {\n");
	    lf_indent (file, +2);
	    {
	      cache_entry *cache_rule;
	      char *field;
	      /* space for any instruction words */
	      if (options.gen.insn_in_icache)
		lf_printf (file, "instruction_word insn[%d];\n",
			   isa->max_nr_words);
	      /* define an entry for any applicable cache rules */
	      for (cache_rule = cache_rules;
		   cache_rule != NULL; cache_rule = cache_rule->next)
		{
		  /* nb - sort of correct - should really check against
		     individual instructions */
		  if (filter_is_subset
		      (format->fields, cache_rule->original_fields))
		    {
		      char *memb;
		      lf_printf (file, "%s %s;",
				 (cache_rule->type == NULL
				  ? "unsigned"
				  : cache_rule->type), cache_rule->name);
		      lf_printf (file, " /*");
		      for (memb =
			   filter_next (cache_rule->original_fields, "");
			   memb != NULL;
			   memb =
			   filter_next (cache_rule->original_fields, memb))
			{
			  lf_printf (file, " %s", memb);
			}
		      lf_printf (file, " */\n");
		    }
		}
	      /* define an entry for any fields not covered by a cache rule */
	      for (field = filter_next (format->fields, "");
		   field != NULL; field = filter_next (format->fields, field))
		{
		  cache_entry *cache_rule;
		  int found_rule = 0;
		  for (cache_rule = cache_rules;
		       cache_rule != NULL; cache_rule = cache_rule->next)
		    {
		      if (strcmp (cache_rule->name, field) == 0)
			{
			  found_rule = 1;
			  break;
			}
		    }
		  if (!found_rule)
		    lf_printf (file, "unsigned %s; /* default */\n", field);
		}
	    }
	    lf_indent (file, -2);
	    lf_printf (file, "} %s;\n", format->name);
	  }
	lf_indent (file, -2);
	lf_printf (file, "} crack;\n");
      }
      lf_indent (file, -2);
      lf_printf (file, "} %sidecode_cache;\n",
		 options.module.global.prefix.l);
    }
  else
    {
      /* alernativly, since no cache, emit a dummy definition for
         idecode_cache so that code refering to the type can still compile */
      lf_printf (file, "typedef void %sidecode_cache;\n",
		 options.module.global.prefix.l);
    }
  lf_printf (file, "\n");
}



static void
print_icache_function (lf *file,
		       insn_entry * instruction,
		       opcode_bits *expanded_bits,
		       insn_opcodes *opcodes,
		       cache_entry *cache_rules, int nr_prefetched_words)
{
  int indent;

  /* generate code to enter decoded instruction into the icache */
  lf_printf (file, "\n");
  lf_print__function_type_function (file, print_icache_function_type,
				    "EXTERN_ICACHE", "\n");
  indent = print_function_name (file,
				instruction->name,
				instruction->format_name,
				NULL,
				expanded_bits, function_name_prefix_icache);
  indent += lf_printf (file, " ");
  lf_indent (file, +indent);
  lf_printf (file, "(");
  print_icache_function_formal (file, nr_prefetched_words);
  lf_printf (file, ")\n");
  lf_indent (file, -indent);

  /* function header */
  lf_printf (file, "{\n");
  lf_indent (file, +2);

  print_my_defines (file,
		    instruction->name,
		    instruction->format_name, expanded_bits);
  print_itrace (file, instruction, 1 /*putting-value-in-cache */ );

  print_idecode_validate (file, instruction, opcodes);

  lf_printf (file, "\n");
  lf_printf (file, "{\n");
  lf_indent (file, +2);
  if (options.gen.semantic_icache)
    lf_printf (file, "unsigned_word nia;\n");
  print_icache_body (file,
		     instruction,
		     expanded_bits,
		     cache_rules,
		     (options.gen.direct_access
		      ? define_variables
		      : declare_variables),
		     (options.gen.semantic_icache
		      ? both_values_and_icache
		      : put_values_in_icache), nr_prefetched_words);

  lf_printf (file, "\n");
  lf_printf (file, "cache_entry->address = cia;\n");
  lf_printf (file, "cache_entry->semantic = ");
  print_function_name (file,
		       instruction->name,
		       instruction->format_name,
		       NULL, expanded_bits, function_name_prefix_semantics);
  lf_printf (file, ";\n");
  lf_printf (file, "\n");

  if (options.gen.semantic_icache)
    {
      lf_printf (file, "/* semantic routine */\n");
      print_semantic_body (file, instruction, expanded_bits, opcodes);
      lf_printf (file, "return nia;\n");
    }

  if (!options.gen.semantic_icache)
    {
      lf_printf (file, "/* return the function proper */\n");
      lf_printf (file, "return ");
      print_function_name (file,
			   instruction->name,
			   instruction->format_name,
			   NULL,
			   expanded_bits, function_name_prefix_semantics);
      lf_printf (file, ";\n");
    }

  if (options.gen.direct_access)
    {
      print_icache_body (file,
			 instruction,
			 expanded_bits,
			 cache_rules,
			 undef_variables,
			 (options.gen.semantic_icache
			  ? both_values_and_icache
			  : put_values_in_icache), nr_prefetched_words);
    }

  lf_indent (file, -2);
  lf_printf (file, "}\n");
  lf_indent (file, -2);
  lf_printf (file, "}\n");
}


void
print_icache_definition (lf *file,
			 insn_entry * insn,
			 opcode_bits *expanded_bits,
			 insn_opcodes *opcodes,
			 cache_entry *cache_rules, int nr_prefetched_words)
{
  print_icache_function (file,
			 insn,
			 expanded_bits,
			 opcodes, cache_rules, nr_prefetched_words);
}



void
print_icache_internal_function_declaration (lf *file,
					    function_entry * function,
					    void *data)
{
  ASSERT (options.gen.icache);
  if (function->is_internal)
    {
      lf_printf (file, "\n");
      lf_print__function_type_function (file, print_icache_function_type,
					"INLINE_ICACHE", "\n");
      print_function_name (file,
			   function->name,
			   NULL, NULL, NULL, function_name_prefix_icache);
      lf_printf (file, "\n(");
      print_icache_function_formal (file, 0);
      lf_printf (file, ");\n");
    }
}


void
print_icache_internal_function_definition (lf *file,
					   function_entry * function,
					   void *data)
{
  ASSERT (options.gen.icache);
  if (function->is_internal)
    {
      lf_printf (file, "\n");
      lf_print__function_type_function (file, print_icache_function_type,
					"INLINE_ICACHE", "\n");
      print_function_name (file,
			   function->name,
			   NULL, NULL, NULL, function_name_prefix_icache);
      lf_printf (file, "\n(");
      print_icache_function_formal (file, 0);
      lf_printf (file, ")\n");
      lf_printf (file, "{\n");
      lf_indent (file, +2);
      lf_printf (file, "/* semantic routine */\n");
      if (options.gen.semantic_icache)
	{
	  lf_print__line_ref (file, function->code->line);
	  table_print_code (file, function->code);
	  lf_printf (file,
		     "error (\"Internal function must longjump\\n\");\n");
	  lf_printf (file, "return 0;\n");
	}
      else
	{
	  lf_printf (file, "return ");
	  print_function_name (file,
			       function->name,
			       NULL,
			       NULL, NULL, function_name_prefix_semantics);
	  lf_printf (file, ";\n");
	}

      lf_print__internal_ref (file);
      lf_indent (file, -2);
      lf_printf (file, "}\n");
    }
}
