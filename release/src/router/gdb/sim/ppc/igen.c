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



#include <getopt.h>

#include "misc.h"
#include "lf.h"
#include "table.h"
#include "config.h"

#include "filter.h"

#include "ld-cache.h"
#include "ld-decode.h"
#include "ld-insn.h"

#include "igen.h"

#include "gen-model.h"
#include "gen-icache.h"
#include "gen-itable.h"
#include "gen-idecode.h"
#include "gen-semantics.h"
#include "gen-support.h"

int hi_bit_nr;
int insn_bit_size = max_insn_bit_size;

igen_code code = generate_calls;

int generate_expanded_instructions;
int icache_size = 1024;
int generate_smp;

/****************************************************************/

static int
print_insn_bits(lf *file, insn_bits *bits)
{
  int nr = 0;
  if (bits == NULL)
    return nr;
  nr += print_insn_bits(file, bits->last);
  nr += lf_putchr(file, '_');
  nr += lf_putstr(file, bits->field->val_string);
  if (bits->opcode->is_boolean && bits->value == 0)
    nr += lf_putint(file, bits->opcode->boolean_constant);
  else if (!bits->opcode->is_boolean) {
    if (bits->opcode->last < bits->field->last)
      nr += lf_putint(file, bits->value << (bits->field->last - bits->opcode->last));
    else
      nr += lf_putint(file, bits->value);
  }
  return nr;
}

extern int
print_function_name(lf *file,
		    const char *basename,
		    insn_bits *expanded_bits,
		    lf_function_name_prefixes prefix)
{
  int nr = 0;
  /* the prefix */
  switch (prefix) {
  case function_name_prefix_semantics:
    nr += lf_putstr(file, "semantic_");
    break;
  case function_name_prefix_idecode:
    nr += lf_printf(file, "idecode_");
    break;
  case function_name_prefix_itable:
    nr += lf_putstr(file, "itable_");
    break;
  case function_name_prefix_icache:
    nr += lf_putstr(file, "icache_");
    break;
  default:
    break;
  }

  /* the function name */
  {
    const char *pos;
    for (pos = basename;
	 *pos != '\0';
	 pos++) {
      switch (*pos) {
      case '/':
      case '-':
      case '(':
      case ')':
	break;
      case ' ':
	nr += lf_putchr(file, '_');
	break;
      default:
	nr += lf_putchr(file, *pos);
	break;
      }
    }
  }

  /* the suffix */
  if (generate_expanded_instructions)
    nr += print_insn_bits(file, expanded_bits);

  return nr;
}


void
print_my_defines(lf *file,
		 insn_bits *expanded_bits,
		 table_entry *file_entry)
{
  /* #define MY_INDEX xxxxx */
  lf_indent_suppress(file);
  lf_printf(file, "#undef MY_INDEX\n");
  lf_indent_suppress(file);
  lf_printf(file, "#define MY_INDEX ");
  print_function_name(file,
                      file_entry->fields[insn_name],
                      NULL,
                      function_name_prefix_itable);
  lf_printf(file, "\n");
  /* #define MY_PREFIX xxxxxx */
  lf_indent_suppress(file);
  lf_printf(file, "#undef MY_PREFIX\n");
  lf_indent_suppress(file);
  lf_printf(file, "#define MY_PREFIX ");
  print_function_name(file,
		      file_entry->fields[insn_name],
		      expanded_bits,
		      function_name_prefix_none);
  lf_printf(file, "\n");
}


void
print_itrace(lf *file,
	     table_entry *file_entry,
	     int idecode)
{
  lf_print__external_reference(file, file_entry->line_nr, file_entry->file_name);
  lf_printf(file, "ITRACE(trace_%s, (\"%s %s\\n\"));\n",
	    (idecode ? "idecode" : "semantics"),
	    (idecode ? "idecode" : "semantics"),
	    file_entry->fields[insn_name]);
  lf_print__internal_reference(file);
}


/****************************************************************/


static void
gen_semantics_h(insn_table *table,
		lf *file,
		igen_code generate)
{
  lf_printf(file, "typedef %s idecode_semantic\n(%s);\n",
	    SEMANTIC_FUNCTION_TYPE,
	    SEMANTIC_FUNCTION_FORMAL);
  lf_printf(file, "\n");
  if ((code & generate_calls)) {
    lf_printf(file, "extern int option_mpc860c0;\n");
    lf_printf(file, "#define PAGE_SIZE 0x1000\n");
    lf_printf(file, "\n");
    lf_printf(file, "PSIM_EXTERN_SEMANTICS(void)\n");
    lf_printf(file, "semantic_init(device* root);\n");
    lf_printf(file, "\n");
    if (generate_expanded_instructions)
      insn_table_traverse_tree(table,
			       file, NULL,
			       1,
			       NULL, /* start */
			       print_semantic_declaration, /* leaf */
			       NULL, /* end */
			       NULL); /* padding */
    else
      insn_table_traverse_insn(table,
			       file, NULL,
			       print_semantic_declaration);
    
  }
  else {
    lf_print__this_file_is_empty(file);
  }
}


static void
gen_semantics_c(insn_table *table,
		cache_table *cache_rules,
		lf *file,
		igen_code generate)
{
  if ((code & generate_calls)) {
    lf_printf(file, "\n");
    lf_printf(file, "#include \"cpu.h\"\n");
    lf_printf(file, "#include \"idecode.h\"\n");
    lf_printf(file, "#include \"semantics.h\"\n");
    lf_printf(file, "#ifdef HAVE_COMMON_FPU\n");
    lf_printf(file, "#include \"sim-inline.h\"\n");
    lf_printf(file, "#include \"sim-fpu.h\"\n");
    lf_printf(file, "#endif\n");
    lf_printf(file, "#include \"support.h\"\n");
    lf_printf(file, "\n");
    lf_printf(file, "int option_mpc860c0 = 0;\n");
    lf_printf(file, "\n");
    lf_printf(file, "PSIM_EXTERN_SEMANTICS(void)\n");
    lf_printf(file, "semantic_init(device* root)\n");
    lf_printf(file, "{\n");
    lf_printf(file, "  option_mpc860c0 = 0;\n");
    lf_printf(file, "  if (tree_find_property(root, \"/options/mpc860c0\"))\n");
    lf_printf(file, "    option_mpc860c0 = tree_find_integer_property(root, \"/options/mpc860c0\");\n");
    lf_printf(file, "    option_mpc860c0 *= 4;   /* convert word count to byte count */\n");
    lf_printf(file, "}\n");
    lf_printf(file, "\n");
    if (generate_expanded_instructions)
      insn_table_traverse_tree(table,
			       file, cache_rules,
			       1,
			       NULL, /* start */
			       print_semantic_definition, /* leaf */
			       NULL, /* end */
			       NULL); /* padding */
    else
      insn_table_traverse_insn(table,
			       file, cache_rules,
			       print_semantic_definition);
    
  }
  else {
    lf_print__this_file_is_empty(file);
  }
}


/****************************************************************/


static void
gen_icache_h(insn_table *table,
	     lf *file,
	     igen_code generate)
{
  lf_printf(file, "typedef %s idecode_icache\n(%s);\n",
	    ICACHE_FUNCTION_TYPE,
	    ICACHE_FUNCTION_FORMAL);
  lf_printf(file, "\n");
  if ((code & generate_calls)
      && (code & generate_with_icache)) {
    insn_table_traverse_function(table,
				 file, NULL,
				 print_icache_internal_function_declaration);
    if (generate_expanded_instructions)
      insn_table_traverse_tree(table,
			       file, NULL,
			       1,
			       NULL, /* start */
			       print_icache_declaration, /* leaf */
			       NULL, /* end */
			       NULL); /* padding */
    else
      insn_table_traverse_insn(table,
			       file, NULL,
			       print_icache_declaration);
    
  }
  else {
    lf_print__this_file_is_empty(file);
  }
}

static void
gen_icache_c(insn_table *table,
	     cache_table *cache_rules,
	     lf *file,
	     igen_code generate)
{
  /* output `internal' invalid/floating-point unavailable functions
     where needed */
  if ((code & generate_calls)
      && (code & generate_with_icache)) {
    lf_printf(file, "\n");
    lf_printf(file, "#include \"cpu.h\"\n");
    lf_printf(file, "#include \"idecode.h\"\n");
    lf_printf(file, "#include \"semantics.h\"\n");
    lf_printf(file, "#include \"icache.h\"\n");
    lf_printf(file, "#ifdef HAVE_COMMON_FPU\n");
    lf_printf(file, "#include \"sim-inline.h\"\n");
    lf_printf(file, "#include \"sim-fpu.h\"\n");
    lf_printf(file, "#endif\n");
    lf_printf(file, "#include \"support.h\"\n");
    lf_printf(file, "\n");
    insn_table_traverse_function(table,
				 file, NULL,
				 print_icache_internal_function_definition);
    lf_printf(file, "\n");
    if (generate_expanded_instructions)
      insn_table_traverse_tree(table,
			       file, cache_rules,
			       1,
			       NULL, /* start */
			       print_icache_definition, /* leaf */
			       NULL, /* end */
			       NULL); /* padding */
    else
      insn_table_traverse_insn(table,
			       file, cache_rules,
			       print_icache_definition);
    
  }
  else {
    lf_print__this_file_is_empty(file);
  }
}


/****************************************************************/


int
main(int argc,
     char **argv,
     char **envp)
{
  cache_table *cache_rules = NULL;
  lf_file_references file_references = lf_include_references;
  decode_table *decode_rules = NULL;
  filter *filters = NULL;
  insn_table *instructions = NULL;
  table_include *includes = NULL;
  char *real_file_name = NULL;
  int is_header = 0;
  int ch;

  if (argc == 1) {
    printf("Usage:\n");
    printf("  igen <config-opts> ... <input-opts>... <output-opts>...\n");
    printf("Config options:\n");
    printf("  -F <filter-out-flag>  eg -F 64 to skip 64bit instructions\n");
    printf("  -E                    Expand (duplicate) semantic functions\n");
    printf("  -I <icache-size>      Generate cracking cache version\n");
    printf("  -C                    Include semantics in cache functions\n");
    printf("  -S                    Include insn (instruction) in icache\n");
    printf("  -R                    Use defines to reference cache vars\n");
    printf("  -L                    Supress line numbering in output files\n");
    printf("  -B <bit-size>         Set the number of bits in an instruction\n");
    printf("  -H <high-bit>         Set the nr of the high (msb bit)\n");
    printf("  -N <nr-cpus>          Specify the max number of cpus the simulation will support\n");
    printf("  -J                    Use jumps instead of function calls\n");
    printf("  -T <mechanism>        Override the mechanism used to decode an instruction\n");
    printf("                        using <mechanism> instead of what was specified in the\n");
    printf("                        decode-rules input file\n");
    printf("\n");
    printf("Input options (ucase version also dumps loaded table):\n");
    printf("  -o <decode-rules>\n");
    printf("  -k <cache-rules>\n");
    printf("  -i <instruction-table>\n");
    printf("\n");
    printf("Output options:\n");
    printf("  -n <real-name>        Specify the real name of for the next output file\n"); 
    printf("  -h 		    Generate header file\n");
    printf("  -c <output-file>      output icache\n");
    printf("  -d <output-file>      output idecode\n");
    printf("  -m <output-file>      output model\n");
    printf("  -s <output-file>      output schematic\n");
    printf("  -t <output-file>      output itable\n");
    printf("  -f <output-file>      output support functions\n");
  }

  while ((ch = getopt(argc, argv,
		      "F:EI:RSLJT:CB:H:N:o:k:i:n:hc:d:m:s:t:f:"))
	 != -1) {
    fprintf(stderr, "\t-%c %s\n", ch, (optarg ? optarg : ""));
    switch(ch) {
    case 'C':
      code |= generate_with_icache;
      code |= generate_with_semantic_icache;
      break;
    case 'S':
      code |= generate_with_icache;
      code |= generate_with_insn_in_icache;
      break;
    case 'L':
      file_references = lf_omit_references;
      break;
    case 'E':
      generate_expanded_instructions = 1;
      break;
    case 'G':
      {
	int enable_p;
	char *argp;
	if (strncmp (optarg, "no-", strlen ("no-")) == 0)
	  {
	    argp = optarg + strlen ("no-");
	    enable_p = 0;
	  }
	else if (strncmp (optarg, "!", strlen ("!")) == 0)
	  {
	    argp = optarg + strlen ("no-");
	    enable_p = 0;
	  }
	else
	  {
	    argp = optarg;
	    enable_p = 1;
	  }
        if (strncmp (argp, "gen-icache", strlen ("gen-icache")) == 0)
          {
            switch (argp[strlen ("gen-icache")])
              {
              case '=':
	        icache_size = atoi (argp + strlen ("gen-icache") + 1);
	        code |= generate_with_icache;
                break;
              case '\0':
	        code |= generate_with_icache;
                break;
              default:
                error (NULL, "Expecting -Ggen-icache or -Ggen-icache=<N>\n");
              }
          }
	}
    case 'I':
      {
	table_include **dir = &includes;
	while ((*dir) != NULL)
	  dir = &(*dir)->next;
	(*dir) = ZALLOC (table_include);
	(*dir)->dir = strdup (optarg);
      }
      break;
    case 'N':
      generate_smp = a2i(optarg);
      break;
    case 'R':
      code |= generate_with_direct_access;
      break;
    case 'B':
      insn_bit_size = a2i(optarg);
      ASSERT(insn_bit_size > 0 && insn_bit_size <= max_insn_bit_size
	     && (hi_bit_nr == insn_bit_size-1 || hi_bit_nr == 0));
      break;
    case 'H':
      hi_bit_nr = a2i(optarg);
      ASSERT(hi_bit_nr == insn_bit_size-1 || hi_bit_nr == 0);
      break;
    case 'F':
      filters = new_filter(optarg, filters);
      break;
    case 'J':
      code &= ~generate_calls;
      code |= generate_jumps;
      break;
    case 'T':
      force_decode_gen_type(optarg);
      break;
    case 'i':
      if (decode_rules == NULL) {
	fprintf(stderr, "Must specify decode tables\n");
	exit (1);
      }
      instructions = load_insn_table(optarg, decode_rules, filters, includes,
				     &cache_rules);
      fprintf(stderr, "\texpanding ...\n");
      insn_table_expand_insns(instructions);
      break;
    case 'o':
      decode_rules = load_decode_table(optarg, hi_bit_nr);
      break;
    case 'k':
      cache_rules = load_cache_table(optarg, hi_bit_nr);
      break;
    case 'n':
      real_file_name = strdup(optarg);
      break;
    case 'h':
      is_header = 1;
      break;
    case 's':
    case 'd':
    case 'm':
    case 't':
    case 'f':
    case 'c':
      {
	lf *file = lf_open(optarg, real_file_name, file_references,
			   (is_header ? lf_is_h : lf_is_c),
			   argv[0]);
	lf_print__file_start(file);
	ASSERT(instructions != NULL);
	switch (ch) {
	case 's':
	  if(is_header)
	    gen_semantics_h(instructions, file, code);
	  else
	    gen_semantics_c(instructions, cache_rules, file, code);
	  break;
	case 'd':
	  if (is_header)
	    gen_idecode_h(file, instructions, cache_rules);
	  else
	    gen_idecode_c(file, instructions, cache_rules);
	  break;
	case 'm':
	  if (is_header)
	    gen_model_h(instructions, file);
	  else
	    gen_model_c(instructions, file);
	  break;
	case 't':
	  if (is_header)
	    gen_itable_h(instructions, file);
	  else
	    gen_itable_c(instructions, file);
	  break;
	case 'f':
	  if (is_header)
	    gen_support_h(instructions, file);
	  else
	    gen_support_c(instructions, file);
	  break;
	case 'c':
	  if (is_header)
	    gen_icache_h(instructions, file, code);
	  else
	    gen_icache_c(instructions, cache_rules, file, code);
	  break;
	}
	lf_print__file_finish(file);
	lf_close(file);
	is_header = 0;
      }
      real_file_name = NULL;
      break;
    default:
      error("unknown option\n");
    }
  }
  return 0;
}
