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


void print_idecode_issue_function_header
  (lf *file,
   const char *processor,
   function_decl_type decl_type, int nr_prefetched_words);

void print_idecode_globals (lf *file);

void print_idecode_lookups
  (lf *file, gen_entry *table, cache_entry *cache_rules);

void print_idecode_body (lf *file, gen_entry *table, const char *result);



/* Output code to do any final checks on the decoded instruction.
   This includes things like verifying any on decoded fields have the
   correct value and checking that (for floating point) floating point
   hardware isn't disabled */

extern void print_idecode_validate
  (lf *file, insn_entry * instruction, insn_opcodes *opcode_paths);
