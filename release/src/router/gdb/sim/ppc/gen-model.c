/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

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

#include "ld-cache.h"
#include "ld-decode.h"
#include "ld-insn.h"

#include "gen-model.h"

#ifndef NULL
#define NULL 0
#endif


static void
model_c_or_h_data(insn_table *table,
		  lf *file,
		  table_entry *data)
{
  if (data->annex) {
    table_entry_print_cpp_line_nr(file, data);
    lf_print__c_code(file, data->annex);
    lf_print__internal_reference(file);
    lf_printf(file, "\n");
  }
}

static void
model_c_or_h_function(insn_table *entry,
		      lf *file,
		      table_entry *function,
		      char *prefix)
{
  if (function->fields[function_type] == NULL
      || function->fields[function_type][0] == '\0') {
    error("Model function type not specified for %s", function->fields[function_name]);
  }
  lf_printf(file, "\n");
  lf_print_function_type(file, function->fields[function_type], prefix, " ");
  lf_printf(file, "%s\n(%s);\n",
	    function->fields[function_name],
	    function->fields[function_param]);
  lf_printf(file, "\n");
}

void 
gen_model_h(insn_table *table, lf *file)
{
  insn *insn_ptr;
  model *model_ptr;
  insn *macro;
  char *name;
  int model_create_p = 0;
  int model_init_p = 0;
  int model_halt_p = 0;
  int model_mon_info_p = 0;
  int model_mon_info_free_p = 0;

  for(macro = model_macros; macro; macro = macro->next) {
    model_c_or_h_data(table, file, macro->file_entry);
  }

  lf_printf(file, "typedef enum _model_enum {\n");
  lf_printf(file, "  MODEL_NONE,\n");
  for (model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
    lf_printf(file, "  MODEL_%s,\n", model_ptr->name);
  }
  lf_printf(file, "  nr_models\n");
  lf_printf(file, "} model_enum;\n");
  lf_printf(file, "\n");

  lf_printf(file, "#define DEFAULT_MODEL MODEL_%s\n", (models) ? models->name : "NONE");
  lf_printf(file, "\n");

  lf_printf(file, "typedef struct _model_data model_data;\n");
  lf_printf(file, "typedef struct _model_time model_time;\n");
  lf_printf(file, "\n");

  lf_printf(file, "extern model_enum current_model;\n");
  lf_printf(file, "extern const char *model_name[ (int)nr_models ];\n");
  lf_printf(file, "extern const char *const *const model_func_unit_name[ (int)nr_models ];\n");
  lf_printf(file, "extern const model_time *const model_time_mapping[ (int)nr_models ];\n");
  lf_printf(file, "\n");

  for(insn_ptr = model_functions; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_or_h_function(table, file, insn_ptr->file_entry, "INLINE_MODEL");
    name = insn_ptr->file_entry->fields[function_name];
    if (strcmp (name, "model_create") == 0)
      model_create_p = 1;
    else if (strcmp (name, "model_init") == 0)
      model_init_p = 1;
    else if (strcmp (name, "model_halt") == 0)
      model_halt_p = 1;
    else if (strcmp (name, "model_mon_info") == 0)
      model_mon_info_p = 1;
    else if (strcmp (name, "model_mon_info_free") == 0)
      model_mon_info_free_p = 1;
  }

  if (!model_create_p) {
    lf_print_function_type(file, "model_data *", "INLINE_MODEL", " ");
    lf_printf(file, "model_create\n");
    lf_printf(file, "(cpu *processor);\n");
    lf_printf(file, "\n");
  }

  if (!model_init_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", " ");
    lf_printf(file, "model_init\n");
    lf_printf(file, "(model_data *model_ptr);\n");
    lf_printf(file, "\n");
  }

  if (!model_halt_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", " ");
    lf_printf(file, "model_halt\n");
    lf_printf(file, "(model_data *model_ptr);\n");
    lf_printf(file, "\n");
  }

  if (!model_mon_info_p) {
    lf_print_function_type(file, "model_print *", "INLINE_MODEL", " ");
    lf_printf(file, "model_mon_info\n");
    lf_printf(file, "(model_data *model_ptr);\n");
    lf_printf(file, "\n");
  }

  if (!model_mon_info_free_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", " ");
    lf_printf(file, "model_mon_info_free\n");
    lf_printf(file, "(model_data *model_ptr,\n");
    lf_printf(file, " model_print *info_ptr);\n");
    lf_printf(file, "\n");
  }

  lf_print_function_type(file, "void", "INLINE_MODEL", " ");
  lf_printf(file, "model_set\n");
  lf_printf(file, "(const char *name);\n");
}

/****************************************************************/

typedef struct _model_c_passed_data model_c_passed_data;
struct _model_c_passed_data {
  lf *file;
  model *model_ptr;
};

static void
model_c_insn(insn_table *entry,
	     lf *phony_file,
	     void *data,
	     insn *instruction,
	     int depth)
{
  model_c_passed_data *data_ptr = (model_c_passed_data *)data;
  lf *file = data_ptr->file;
  char *current_name = data_ptr->model_ptr->printable_name;
  table_model_entry *model_ptr = instruction->file_entry->model_first;

  while (model_ptr) {
    if (model_ptr->fields[insn_model_name] == current_name) {
      lf_printf(file, "  { %-*s },  /* %s */\n",
		max_model_fields_len,
		model_ptr->fields[insn_model_fields],
		instruction->file_entry->fields[insn_name]);
      return;
    }

    model_ptr = model_ptr->next;
  }

  lf_printf(file, "  { %-*s },  /* %s */\n",
	    max_model_fields_len,
	    data_ptr->model_ptr->insn_default,
	    instruction->file_entry->fields[insn_name]);
}

static void
model_c_function(insn_table *table,
		 lf *file,
		 table_entry *function,
		 const char *prefix)
{
  if (function->fields[function_type] == NULL
      || function->fields[function_type][0] == '\0') {
    error("Model function return type not specified for %s", function->fields[function_name]);
  }
  else {
    lf_printf(file, "\n");
    lf_print_function_type(file, function->fields[function_type], prefix, "\n");
    lf_printf(file, "%s(%s)\n",
	      function->fields[function_name],
	      function->fields[function_param]);
  }
  table_entry_print_cpp_line_nr(file, function);
  lf_printf(file, "{\n");
  if (function->annex) {
    lf_indent(file, +2);
    lf_print__c_code(file, function->annex);
    lf_indent(file, -2);
  }
  lf_printf(file, "}\n");
  lf_print__internal_reference(file);
  lf_printf(file, "\n");
}

void 
gen_model_c(insn_table *table, lf *file)
{
  insn *insn_ptr;
  model *model_ptr;
  char *name;
  int model_create_p = 0;
  int model_init_p = 0;
  int model_halt_p = 0;
  int model_mon_info_p = 0;
  int model_mon_info_free_p = 0;

  lf_printf(file, "\n");
  lf_printf(file, "#include \"cpu.h\"\n");
  lf_printf(file, "#include \"mon.h\"\n");
  lf_printf(file, "\n");
  lf_printf(file, "#ifdef HAVE_STDLIB_H\n");
  lf_printf(file, "#include <stdlib.h>\n");
  lf_printf(file, "#endif\n");
  lf_printf(file, "\n");

  for(insn_ptr = model_data; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_or_h_data(table, file, insn_ptr->file_entry);
  }

  for(insn_ptr = model_static; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_or_h_function(table, file, insn_ptr->file_entry, "/*h*/STATIC");
  }

  for(insn_ptr = model_internal; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_or_h_function(table, file, insn_ptr->file_entry, "STATIC_INLINE_MODEL");
  }

  for(insn_ptr = model_static; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_function(table, file, insn_ptr->file_entry, "/*c*/STATIC");
  }

  for(insn_ptr = model_internal; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_function(table, file, insn_ptr->file_entry, "STATIC_INLINE_MODEL");
  }

  for(insn_ptr = model_functions; insn_ptr; insn_ptr = insn_ptr->next) {
    model_c_function(table, file, insn_ptr->file_entry, "INLINE_MODEL");
    name = insn_ptr->file_entry->fields[function_name];
    if (strcmp (name, "model_create") == 0)
      model_create_p = 1;
    else if (strcmp (name, "model_init") == 0)
      model_init_p = 1;
    else if (strcmp (name, "model_halt") == 0)
      model_halt_p = 1;
    else if (strcmp (name, "model_mon_info") == 0)
      model_mon_info_p = 1;
    else if (strcmp (name, "model_mon_info_free") == 0)
      model_mon_info_free_p = 1;
  }

  if (!model_create_p) {
    lf_print_function_type(file, "model_data *", "INLINE_MODEL", "\n");
    lf_printf(file, "model_create(cpu *processor)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "  return (model_data *)0;\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
  }

  if (!model_init_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", "\n");
    lf_printf(file, "model_init(model_data *model_ptr)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
  }

  if (!model_halt_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", "\n");
    lf_printf(file, "model_halt(model_data *model_ptr)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
  }

  if (!model_mon_info_p) {
    lf_print_function_type(file, "model_print *", "INLINE_MODEL", "\n");
    lf_printf(file, "model_mon_info(model_data *model_ptr)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "  return (model_print *)0;\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
  }

  if (!model_mon_info_free_p) {
    lf_print_function_type(file, "void", "INLINE_MODEL", "\n");
    lf_printf(file, "model_mon_info_free(model_data *model_ptr,\n");
    lf_printf(file, "                    model_print *info_ptr)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
  }

  lf_printf(file, "/* Insn functional unit info */\n");
  for(model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
    model_c_passed_data data;

    lf_printf(file, "static const model_time model_time_%s[] = {\n", model_ptr->name);
    data.file = file;
    data.model_ptr = model_ptr;
    insn_table_traverse_insn(table,
			     NULL, (void *)&data,
			     model_c_insn);

    lf_printf(file, "};\n");
    lf_printf(file, "\n");
    lf_printf(file, "\f\n");
  }

  lf_printf(file, "#ifndef _INLINE_C_\n");
  lf_printf(file, "const model_time *const model_time_mapping[ (int)nr_models ] = {\n");
  lf_printf(file, "  (const model_time *const)0,\n");
  for(model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
    lf_printf(file, "  model_time_%s,\n", model_ptr->name);
  }
  lf_printf(file, "};\n");
  lf_printf(file, "#endif\n");
  lf_printf(file, "\n");

  lf_printf(file, "\f\n");
  lf_printf(file, "/* map model enumeration into printable string */\n");
  lf_printf(file, "#ifndef _INLINE_C_\n");
  lf_printf(file, "const char *model_name[ (int)nr_models ] = {\n");
  lf_printf(file, "  \"NONE\",\n");
  for (model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
    lf_printf(file, "  \"%s\",\n", model_ptr->printable_name);
  }
  lf_printf(file, "};\n");
  lf_printf(file, "#endif\n");
  lf_printf(file, "\n");

  lf_print_function_type(file, "void", "INLINE_MODEL", "\n");
  lf_printf(file, "model_set(const char *name)\n");
  lf_printf(file, "{\n");
  if (models) {
    lf_printf(file, "  model_enum model;\n");
    lf_printf(file, "  for(model = MODEL_%s; model < nr_models; model++) {\n", models->name);
    lf_printf(file, "    if(strcmp(name, model_name[model]) == 0) {\n");
    lf_printf(file, "      current_model = model;\n");
    lf_printf(file, "      return;\n");
    lf_printf(file, "    }\n");
    lf_printf(file, "  }\n");
    lf_printf(file, "\n");
    lf_printf(file, "  error(\"Unknown model '%%s', Models which are known are:%%s\\n\",\n");
    lf_printf(file, "        name,\n");
    lf_printf(file, "        \"");
    for(model_ptr = models; model_ptr; model_ptr = model_ptr->next) {
      lf_printf(file, "\\n\\t%s", model_ptr->printable_name);
    }
    lf_printf(file, "\");\n");
  } else {
    lf_printf(file, "  error(\"No models are currently known about\");\n");
  }

  lf_printf(file, "}\n");
}

