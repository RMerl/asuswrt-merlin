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


/* code-generation options: */

typedef enum
{

  /* Transfer control to an instructions semantic code using the the
     standard call/return mechanism */

  generate_calls,

  /* Transfer control to an instructions semantic code using
     (computed) goto's instead of the more conventional call/return
     mechanism */

  generate_jumps,

}
igen_code;

typedef enum
{
  nia_is_cia_plus_one,
  nia_is_void,
  nia_is_invalid,
}
igen_nia;



typedef struct _igen_gen_options igen_gen_options;
struct _igen_gen_options
{
  int direct_access;
  int semantic_icache;
  int insn_in_icache;
  int conditional_issue;
  int slot_verification;
  int delayed_branch;

  /* If zeroing a register, which one? */
  int zero_reg;
  int zero_reg_nr;

  /* should multiple simulators be generated? */
  int multi_sim;

  /* name of the default multi-sim model */
  char *default_model;

  /* should the simulator support multi word instructions and if so,
     what is the max nr of words. */
  int multi_word;

  /* SMP?  Should the generated code include SMP support (>0) and if
     so, for how many processors? */
  int smp;

  /* how should the next instruction address be computed? */
  igen_nia nia;

  /* nr of instructions in the decoded instruction cache */
  int icache;
  int icache_size;

  /* see above */
  igen_code code;
};


typedef struct _igen_trace_options igen_trace_options;
struct _igen_trace_options
{
  int rule_selection;
  int rule_rejection;
  int insn_insertion;
  int insn_expansion;
  int entries;
  int combine;
};

typedef struct _igen_name
{
  char *u;
  char *l;
}
igen_name;
typedef struct _igen_module
{
  igen_name prefix;
  igen_name suffix;
}
igen_module;

typedef struct _igen_module_options
{
  igen_module global;
  igen_module engine;
  igen_module icache;
  igen_module idecode;
  igen_module itable;
  igen_module semantics;
  igen_module support;
}
igen_module_options;

typedef struct _igen_decode_options igen_decode_options;
struct _igen_decode_options
{

  /* Combine tables?  Should the generator make a second pass through
     each generated table looking for any sub-entries that contain the
     same instructions.  Those entries being merged into a single
     table */
  int combine;

  /* Instruction expansion? Should the semantic code for each
     instruction, when the oportunity arrises, be expanded according
     to the variable opcode files that the instruction decode process
     renders constant */
  int duplicate;

  /* Treat reserved fields as constant (zero) instead of ignoring
     their value when determining decode tables */
  int zero_reserved;

  /* Convert any padded switch rules into goto_switch */
  int switch_as_goto;

  /* Force all tables to be generated with this lookup mechanism */
  char *overriding_gen;
};


typedef struct _igen_warn_options igen_warn_options;
struct _igen_warn_options
{

  /* Issue warning about discarded instructions */
  int discard;

  /* Issue warning about invalid instruction widths */
  int width;

  /* Issue warning about unimplemented instructions */
  int unimplemented;

};



typedef struct _igen_options igen_options;
struct _igen_options
{

  /* What does the instruction look like - bit ordering, size, widths or
     offesets */
  int hi_bit_nr;
  int insn_bit_size;
  int insn_specifying_widths;

  /* what should global names be prefixed with? */
  igen_module_options module;

  /* See above for options and flags */
  igen_gen_options gen;

  /* See above for trace options */
  igen_trace_options trace;

  /* See above for include options */
  table_include *include;

  /* See above for decode options */
  igen_decode_options decode;

  /* Filter set to be used on the flag field of the instruction table */
  filter *flags_filter;

  /* See above for warn options */
  igen_warn_options warn;

  /* Be more picky about the input */
  error_func (*warning);

  /* Model (processor) set - like flags_filter. Used to select the
     specific ISA within a processor family. */
  filter *model_filter;

  /* Format name set */
  filter *format_name_filter;
};

extern igen_options options;

/* default options - hopefully backward compatible */
#define INIT_OPTIONS() \
do { \
  memset (&options, 0, sizeof options); \
  memset (&options.warn, -1, sizeof (options.warn)); \
  options.hi_bit_nr = 0; \
  options.insn_bit_size = default_insn_bit_size; \
  options.insn_specifying_widths = 0; \
  options.module.global.prefix.u = ""; \
  options.module.global.prefix.l = ""; \
  /* the prefixes */ \
  options.module.engine = options.module.global; \
  options.module.icache = options.module.global; \
  options.module.idecode = options.module.global; \
  options.module.itable = options.module.global; \
  options.module.semantics = options.module.global; \
  options.module.support = options.module.global; \
  /* the suffixes */ \
  options.module.engine.suffix.l = "engine"; \
  options.module.engine.suffix.u = "ENGINE"; \
  options.module.icache.suffix.l = "icache"; \
  options.module.icache.suffix.u = "ICACHE"; \
  options.module.idecode.suffix.l = "idecode"; \
  options.module.idecode.suffix.u = "IDECODE"; \
  options.module.itable.suffix.l = "itable"; \
  options.module.itable.suffix.u = "ITABLE"; \
  options.module.semantics.suffix.l = "semantics"; \
  options.module.semantics.suffix.u = "SEMANTICS"; \
  options.module.support.suffix.l = "support"; \
  options.module.support.suffix.u = "SUPPORT"; \
  /* misc stuff */ \
  options.gen.code = generate_calls; \
  options.gen.icache_size = 1024; \
  options.warning = warning; \
} while (0)
