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


#include "misc.h"
#include "lf.h"
#include "table.h"

#include "filter.h"

#include "ld-decode.h"
#include "ld-cache.h"
#include "ld-insn.h"

#include "igen.h"

#include "gen-semantics.h"
#include "gen-idecode.h"
#include "gen-icache.h"



static void
print_icache_function_header(lf *file,
			     const char *basename,
			     insn_bits *expanded_bits,
			     int is_function_definition)
{
  lf_printf(file, "\n");
  lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "EXTERN_ICACHE", " ");
  print_function_name(file,
		      basename,
		      expanded_bits,
		      function_name_prefix_icache);
  lf_printf(file, "\n(%s)", ICACHE_FUNCTION_FORMAL); 
  if (!is_function_definition)
    lf_printf(file, ";");
  lf_printf(file, "\n");
}


void
print_icache_declaration(insn_table *entry,
			 lf *file,
			 void *data,
			 insn *instruction,
			 int depth)
{
  if (generate_expanded_instructions) {
    ASSERT(entry->nr_insn == 1);
    print_icache_function_header(file,
				 entry->insns->file_entry->fields[insn_name],
				 entry->expanded_bits,
				 0/* is not function definition */);
  }
  else {
    print_icache_function_header(file,
				 instruction->file_entry->fields[insn_name],
				 NULL,
				 0/* is not function definition */);
  }
}



static void
print_icache_extraction(lf *file,
			insn *instruction,
			const char *entry_name,
			const char *entry_type,
			const char *entry_expression,
			const char *original_name,
			const char *file_name,
			int line_nr,
			insn_field *cur_field,
			insn_bits *bits,
			icache_decl_type what_to_declare,
			icache_body_type what_to_do,
			const char *reason)
{
  const char *expression;
  ASSERT(entry_name != NULL);

  /* Define a storage area for the cache element */
  if (what_to_declare == undef_variables) {
    /* We've finished with the value - destory it */
    lf_indent_suppress(file);
    lf_printf(file, "#undef %s\n", entry_name);
    return;
  }
  else if (what_to_declare == define_variables) {
    lf_indent_suppress(file);
    lf_printf(file, "#define %s ", entry_name);
  }
  else {
    if (file_name != NULL)
      lf_print__external_reference(file, line_nr, file_name);
    lf_printf(file, "%s const %s UNUSED = ",
	      entry_type == NULL ? "unsigned" : entry_type,
	      entry_name);
  }

  /* define a value for that storage area as determined by what is in
     the cache */
  if (bits != NULL
      && strcmp(entry_name, cur_field->val_string) == 0
      && ((bits->opcode->is_boolean && bits->value == 0)
	  || (!bits->opcode->is_boolean))) {
    /* The simple field has been made constant (as a result of
       expanding instructions or similar).  Remember that for a
       boolean field, value is either 0 (implying the required
       boolean_constant) or nonzero (implying some other value and
       handled later below) - Define the variable accordingly */
    expression = "constant field";
    ASSERT(bits->field == cur_field);
    ASSERT(entry_type == NULL);
    if (bits->opcode->is_boolean)
      lf_printf(file, "%d", bits->opcode->boolean_constant);
    else if (bits->opcode->last < bits->field->last)
      lf_printf(file, "%d",
		bits->value << (bits->field->last - bits->opcode->last));
    else
      lf_printf(file, "%d", bits->value);
  }
  else if (bits != NULL
	   && original_name != NULL
	   && strncmp(entry_name,
		      original_name, strlen(original_name)) == 0
	   && strncmp(entry_name + strlen(original_name),
		      "_is_", strlen("_is_")) == 0
	   && ((bits->opcode->is_boolean
		&& (atol(entry_name + strlen(original_name) + strlen("_is_"))
		    == bits->opcode->boolean_constant))
	       || (!bits->opcode->is_boolean))) {
    expression = "constant compare";
    /* An entry, derived from ORIGINAL_NAME, is testing to see of the
       ORIGINAL_NAME has a specific constant value.  That value
       matching a boolean or constant field */
    if (bits->opcode->is_boolean)
      lf_printf(file, "%d /* %s == %d */",
		bits->value == 0,
		original_name,
		bits->opcode->boolean_constant);
    else if (bits->opcode->last < bits->field->last)
      lf_printf(file, "%d /* %s == %d */",
		(atol(entry_name + strlen(original_name) + strlen("_is_"))
		 == (bits->value << (bits->field->last - bits->opcode->last))),
		original_name,
		(bits->value << (bits->field->last - bits->opcode->last)));
    else
      lf_printf(file, "%d /* %s == %d */",
		(atol(entry_name + strlen(original_name) + strlen("_is_"))
		 == bits->value),
		original_name,
		bits->value);
  }
  else {
    /* put the field in the local variable, possibly also enter it
       into the cache */
    expression = "extraction";
    /* handle the cache */
    if ((what_to_do & get_values_from_icache)
	|| (what_to_do & put_values_in_icache)) {
      lf_printf(file, "cache_entry->crack.%s.%s",
		instruction->file_entry->fields[insn_form],
		entry_name);
      if (what_to_do & put_values_in_icache) /* also put it in the cache? */
	lf_printf(file, " = ");
    }
    if ((what_to_do & put_values_in_icache)
	|| what_to_do == do_not_use_icache) {
      if (cur_field != NULL && strcmp(entry_name, cur_field->val_string) == 0)
	lf_printf(file, "EXTRACTED32(instruction, %d, %d)",
		  i2target(hi_bit_nr, cur_field->first),
		  i2target(hi_bit_nr, cur_field->last));
      else if (entry_expression != NULL)
	lf_printf(file, "%s", entry_expression);
      else
	lf_printf(file, "eval_%s", entry_name);
    }
  }

  if (!((what_to_declare == define_variables)
	|| (what_to_declare == undef_variables)))
    lf_printf(file, ";");
  if (reason != NULL)
    lf_printf(file, " /* %s - %s */", reason, expression);
  lf_printf(file, "\n");
}


void
print_icache_body(lf *file,
		  insn *instruction,
		  insn_bits *expanded_bits,
		  cache_table *cache_rules,
		  icache_decl_type what_to_declare,
		  icache_body_type what_to_do)
{
  insn_field *cur_field;
  
  /* extract instruction fields */
  lf_printf(file, "/* extraction: %s ",
	    instruction->file_entry->fields[insn_format]);
  switch (what_to_declare) {
  case define_variables:
    lf_printf(file, "#define");
    break;
  case declare_variables:
    lf_printf(file, "declare");
    break;
  case undef_variables:
    lf_printf(file, "#undef");
    break;
  }
  lf_printf(file, " ");
  switch (what_to_do) {
  case get_values_from_icache:
    lf_printf(file, "get-values-from-icache");
    break;
  case put_values_in_icache:
    lf_printf(file, "put-values-in-icache");
    break;
  case both_values_and_icache:
    lf_printf(file, "get-values-from-icache|put-values-in-icache");
    break;
  case do_not_use_icache:
    lf_printf(file, "do-not-use-icache");
    break;
  }
  lf_printf(file, " */\n");
  
  for (cur_field = instruction->fields->first;
       cur_field->first < insn_bit_size;
       cur_field = cur_field->next) {
    if (cur_field->is_string) {
      insn_bits *bits;
      int found_rule = 0;
      /* find any corresponding value */
      for (bits = expanded_bits;
	   bits != NULL;
	   bits = bits->last) {
	if (bits->field == cur_field)
	  break;
      }
      /* try the cache rule table for what to do */
      {
	cache_table *cache_rule;
	for (cache_rule = cache_rules;
	     cache_rule != NULL;
	     cache_rule = cache_rule->next) {
	  if (strcmp(cur_field->val_string, cache_rule->field_name) == 0) {
	    found_rule = 1;
	    if (cache_rule->type == scratch_value
		&& ((what_to_do & put_values_in_icache)
		    || what_to_do == do_not_use_icache))
	      print_icache_extraction(file,
				      instruction,
				      cache_rule->derived_name,
				      cache_rule->type_def,
				      cache_rule->expression,
				      cache_rule->field_name,
				      cache_rule->file_entry->file_name,
				      cache_rule->file_entry->line_nr,
				      cur_field,
				      bits,
				      what_to_declare,
				      do_not_use_icache,
				      "icache scratch");
	    else if (cache_rule->type == compute_value
		     && ((what_to_do & get_values_from_icache)
			 || what_to_do == do_not_use_icache))
	      print_icache_extraction(file,
				      instruction,
				      cache_rule->derived_name,
				      cache_rule->type_def,
				      cache_rule->expression,
				      cache_rule->field_name,
				      cache_rule->file_entry->file_name,
				      cache_rule->file_entry->line_nr,
				      cur_field,
				      bits,
				      what_to_declare,
				      do_not_use_icache,
				      "semantic compute");
	    else if (cache_rule->type == cache_value
		     && ((what_to_declare != undef_variables)
			 || !(what_to_do & put_values_in_icache)))
	      print_icache_extraction(file,
				      instruction,
				      cache_rule->derived_name,
				      cache_rule->type_def,
				      cache_rule->expression,
				      cache_rule->field_name,
				      cache_rule->file_entry->file_name,
				      cache_rule->file_entry->line_nr,
				      cur_field,
				      bits,
				      ((what_to_do & put_values_in_icache)
				       ? declare_variables
				       : what_to_declare),
				      what_to_do,
				      "in icache");
	  }
	}
      }
      /* No rule at all, assume that this is needed in the semantic
         function (when values are extracted from the icache) and
         hence must be put into the cache */
      if (found_rule == 0
	  && ((what_to_declare != undef_variables)
	      || !(what_to_do & put_values_in_icache)))
	print_icache_extraction(file,
				instruction,
				cur_field->val_string,
				NULL, NULL, NULL, /* type, exp, orig */
				instruction->file_entry->file_name,
				instruction->file_entry->line_nr,
				cur_field,
				bits,
				((what_to_do & put_values_in_icache)
				 ? declare_variables
				 : what_to_declare),
				what_to_do,
				"default in icache");
      /* any thing else ... */
    }
  }

  lf_print__internal_reference(file);

  if ((code & generate_with_insn_in_icache)) {
    lf_printf(file, "\n");
    print_icache_extraction(file,
			    instruction,
			    "insn",
			    "instruction_word",
			    "instruction",
			    NULL, /* origin */
			    NULL, 0, /* file_name & line_nr */
			    NULL, NULL,
			    what_to_declare,
			    what_to_do,
			    NULL);
  }
}



typedef struct _icache_tree icache_tree;
struct _icache_tree {
  char *name;
  icache_tree *next;
  icache_tree *children;
};

static icache_tree *
icache_tree_insert(icache_tree *tree,
		   char *name)
{
  icache_tree *new_tree;
  /* find it */
  icache_tree **ptr_to_cur_tree = &tree->children;
  icache_tree *cur_tree = *ptr_to_cur_tree;
  while (cur_tree != NULL
	 && strcmp(cur_tree->name, name) < 0) {
    ptr_to_cur_tree = &cur_tree->next;
    cur_tree = *ptr_to_cur_tree;
  }
  ASSERT(cur_tree == NULL
	 || strcmp(cur_tree->name, name) >= 0);
  /* already in the tree */
  if (cur_tree != NULL
      && strcmp(cur_tree->name, name) == 0)
    return cur_tree;
  /* missing, insert it */
  ASSERT(cur_tree == NULL
	 || strcmp(cur_tree->name, name) > 0);
  new_tree = ZALLOC(icache_tree);
  new_tree->name = name;
  new_tree->next = cur_tree;
  *ptr_to_cur_tree = new_tree;
  return new_tree;
}


static icache_tree *
insn_table_cache_fields(insn_table *table)
{
  icache_tree *tree = ZALLOC(icache_tree);
  insn *instruction;
  for (instruction = table->insns;
       instruction != NULL;
       instruction = instruction->next) {
    insn_field *field;
    icache_tree *form =
      icache_tree_insert(tree,
			 instruction->file_entry->fields[insn_form]);
    for (field = instruction->fields->first;
	 field != NULL;
	 field = field->next) {
      if (field->is_string)
	icache_tree_insert(form, field->val_string);
    }
  }
  return tree;
}



extern void
print_icache_struct(insn_table *instructions,
		    cache_table *cache_rules,
		    lf *file)
{
  icache_tree *tree = insn_table_cache_fields(instructions);
  
  lf_printf(file, "\n");
  lf_printf(file, "#define WITH_IDECODE_CACHE_SIZE %d\n",
	    (code & generate_with_icache) ? icache_size : 0);
  lf_printf(file, "\n");
  
  /* create an instruction cache if being used */
  if ((code & generate_with_icache)) {
    icache_tree *form;
    lf_printf(file, "typedef struct _idecode_cache {\n");
    lf_printf(file, "  unsigned_word address;\n");
    lf_printf(file, "  void *semantic;\n");
    lf_printf(file, "  union {\n");
    for (form = tree->children;
	 form != NULL;
	 form = form->next) {
      icache_tree *field;
      lf_printf(file, "    struct {\n");
      if (code & generate_with_insn_in_icache)
	lf_printf(file, "      instruction_word insn;\n");
      for (field = form->children;
	   field != NULL;
	   field = field->next) {
	cache_table *cache_rule;
	int found_rule = 0;
	for (cache_rule = cache_rules;
	     cache_rule != NULL;
	     cache_rule = cache_rule->next) {
	  if (strcmp(field->name, cache_rule->field_name) == 0) {
	    found_rule = 1;
	    if (cache_rule->derived_name != NULL)
	      lf_printf(file, "      %s %s; /* %s */\n",
			(cache_rule->type_def == NULL
			 ? "unsigned" 
			 : cache_rule->type_def),
			cache_rule->derived_name,
			cache_rule->field_name);
	  }
	}
	if (!found_rule)
	  lf_printf(file, "      unsigned %s;\n", field->name);
      }
      lf_printf(file, "    } %s;\n", form->name);
    }
    lf_printf(file, "  } crack;\n");
    lf_printf(file, "} idecode_cache;\n");
  }
  else {
    /* alernativly, since no cache, emit a dummy definition for
       idecode_cache so that code refering to the type can still compile */
    lf_printf(file, "typedef void idecode_cache;\n");
  }
  lf_printf(file, "\n");
}



static void
print_icache_function(lf *file,
		      insn *instruction,
		      insn_bits *expanded_bits,
		      opcode_field *opcodes,
		      cache_table *cache_rules)
{
  int indent;

  /* generate code to enter decoded instruction into the icache */
  lf_printf(file, "\n");
  lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "EXTERN_ICACHE", "\n");
  indent = print_function_name(file,
			       instruction->file_entry->fields[insn_name],
			       expanded_bits,
			       function_name_prefix_icache);
  lf_indent(file, +indent);
  lf_printf(file, "(%s)\n", ICACHE_FUNCTION_FORMAL);
  lf_indent(file, -indent);
  
  /* function header */
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  
  print_my_defines(file, expanded_bits, instruction->file_entry);
  print_itrace(file, instruction->file_entry, 1/*putting-value-in-cache*/);
  
  print_idecode_validate(file, instruction, opcodes);
  
  lf_printf(file, "\n");
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  if ((code & generate_with_semantic_icache))
    lf_printf(file, "unsigned_word nia;\n");
  print_icache_body(file,
		    instruction,
		    expanded_bits,
		    cache_rules,
		    ((code & generate_with_direct_access)
		     ? define_variables
		     : declare_variables),
		    ((code & generate_with_semantic_icache)
		     ? both_values_and_icache
		     : put_values_in_icache));
  
  lf_printf(file, "\n");
  lf_printf(file, "cache_entry->address = cia;\n");
  lf_printf(file, "cache_entry->semantic = ");
  print_function_name(file,
		      instruction->file_entry->fields[insn_name],
		      expanded_bits,
		      function_name_prefix_semantics);
  lf_printf(file, ";\n");
  lf_printf(file, "\n");

  if ((code & generate_with_semantic_icache)) {
    lf_printf(file, "/* semantic routine */\n");
    print_semantic_body(file,
			instruction,
			expanded_bits,
			opcodes);
    lf_printf(file, "return nia;\n");
  }
  
  if (!(code & generate_with_semantic_icache)) {
    lf_printf(file, "/* return the function proper */\n");
    lf_printf(file, "return ");
    print_function_name(file,
			instruction->file_entry->fields[insn_name],
			expanded_bits,
			function_name_prefix_semantics);
    lf_printf(file, ";\n");
  }
  
  if ((code & generate_with_direct_access))
    print_icache_body(file,
		      instruction,
		      expanded_bits,
		      cache_rules,
		      undef_variables,
		      ((code & generate_with_semantic_icache)
		       ? both_values_and_icache
		       : put_values_in_icache));

  lf_indent(file, -2);
  lf_printf(file, "}\n");
  lf_indent(file, -2);
  lf_printf(file, "}\n");
}


void
print_icache_definition(insn_table *entry,
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
    print_icache_function(file,
			  entry->insns,
			  entry->expanded_bits,
			  entry->opcode,
			  cache_rules);
  }
  else {
    print_icache_function(file,
			  instruction,
			  NULL,
			  NULL,
			  cache_rules);
  }
}



void
print_icache_internal_function_declaration(insn_table *table,
					   lf *file,
					   void *data,
					   table_entry *function)
{
  ASSERT((code & generate_with_icache) != 0);
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "\n");
    lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "PSIM_INLINE_ICACHE",
			   "\n");
    print_function_name(file,
			function->fields[insn_name],
			NULL,
			function_name_prefix_icache);
    lf_printf(file, "\n(%s);\n", ICACHE_FUNCTION_FORMAL);
  }
}


void
print_icache_internal_function_definition(insn_table *table,
					  lf *file,
					  void *data,
					  table_entry *function)
{
  ASSERT((code & generate_with_icache) != 0);
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "\n");
    lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "PSIM_INLINE_ICACHE",
			   "\n");
    print_function_name(file,
			function->fields[insn_name],
			NULL,
			function_name_prefix_icache);
    lf_printf(file, "\n(%s)\n", ICACHE_FUNCTION_FORMAL);
    lf_printf(file, "{\n");
    lf_indent(file, +2);
    lf_printf(file, "/* semantic routine */\n");
    table_entry_print_cpp_line_nr(file, function);
    if ((code & generate_with_semantic_icache)) {
      lf_print__c_code(file, function->annex);
      lf_printf(file, "error(\"Internal function must longjump\\n\");\n");
      lf_printf(file, "return 0;\n");
    }
    else {
      lf_printf(file, "return ");
      print_function_name(file,
			  function->fields[insn_name],
			  NULL,
			  function_name_prefix_semantics);
      lf_printf(file, ";\n");
    }
    
    lf_print__internal_reference(file);
    lf_indent(file, -2);
    lf_printf(file, "}\n");
  }
}
