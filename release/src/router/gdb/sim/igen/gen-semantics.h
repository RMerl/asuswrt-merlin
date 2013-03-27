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


/* Creates the files semantics.[hc].

   The generated file semantics contains functions that implement the
   operations required to model a single target processor instruction.

   Several different variations on the semantics file can be created:

	o	uncached

        	No instruction cache exists.  The semantic function
		needs to generate any required values locally.

	o	cached - separate cracker and semantic

		Two independant functions are created.  Firstly the
		function that cracks an instruction entering it into a
		cache and secondly the semantic function propper that
		uses the cache.

	o	cached - semantic + cracking semantic

		The function that cracks the instruction and enters
		all values into the cache also contains a copy of the
		semantic code (avoiding the need to call both the
		cracker and the semantic function when there is a
		cache miss).

   For each of these general forms, several refinements can occure:

	o	do/don't duplicate/expand semantic functions

		As a consequence of decoding an instruction, the
		decoder, as part of its table may have effectivly made
		certain of the variable fields in an instruction
		constant. Separate functions for each of the
		alternative values for what would have been treated as
		a variable part can be created.

	o	use cache struct directly.

		When a cracking cache is present, the semantic
		functions can be generated to either hold intermediate
		cache values in local variables or always refer to the
		contents of the cache directly. */






extern void print_semantic_declaration
  (lf *file,
   insn_entry * insn,
   opcode_bits *bits, insn_opcodes *opcodes, int nr_prefetched_words);

extern void print_semantic_definition
  (lf *file,
   insn_entry * insn,
   opcode_bits *bits,
   insn_opcodes *opcodes, cache_entry *cache_rules, int nr_prefetched_words);


typedef enum
{
  invalid_illegal,
  invalid_fp_unavailable,
  invalid_wrong_slot,
}
invalid_type;

extern void print_idecode_invalid
  (lf *file, const char *result, invalid_type type);

extern void print_semantic_body
  (lf *file,
   insn_entry * instruction,
   opcode_bits *expanded_bits, insn_opcodes *opcodes);
