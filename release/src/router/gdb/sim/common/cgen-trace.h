/* Simulator tracing support for Cpu tools GENerated simulators.
   Copyright (C) 1996, 1997, 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

#ifndef CGEN_TRACE_H
#define CGEN_TRACE_H

void trace_insn_init (SIM_CPU *, int);
void trace_insn_fini (SIM_CPU *, const struct argbuf *, int);
void trace_insn (SIM_CPU *, const struct cgen_insn *,
		 const struct argbuf *, IADDR);
void trace_extract (SIM_CPU *, IADDR, char *, ...);
void trace_result (SIM_CPU *, char *, int, ...);
void cgen_trace_printf (SIM_CPU *, char *fmt, ...);

/* Trace instruction results.  */
#define TRACE_RESULT_P(cpu, abuf) (TRACE_INSN_P (cpu) && ARGBUF_TRACE_P (abuf))

#define TRACE_INSN_INIT(cpu, abuf, first_p) \
do { \
  if (TRACE_INSN_P (cpu)) \
    trace_insn_init ((cpu), (first_p)); \
} while (0)
#define TRACE_INSN_FINI(cpu, abuf, last_p) \
do { \
  if (TRACE_INSN_P (cpu)) \
    trace_insn_fini ((cpu), (abuf), (last_p)); \
} while (0)
#define TRACE_PRINTF(cpu, what, args) \
do { \
  if (TRACE_P ((cpu), (what))) \
    cgen_trace_printf args ; \
} while (0)
#define TRACE_INSN(cpu, insn, abuf, pc) \
do { \
  if (TRACE_INSN_P (cpu) && ARGBUF_TRACE_P (abuf)) \
    trace_insn ((cpu), (insn), (abuf), (pc)) ; \
} while (0)
#define TRACE_EXTRACT(cpu, abuf, args) \
do { \
  if (TRACE_EXTRACT_P (cpu)) \
    trace_extract args ; \
} while (0)
#define TRACE_RESULT(cpu, abuf, name, type, val) \
do { \
  if (TRACE_RESULT_P ((cpu), (abuf))) \
    trace_result ((cpu), (name), (type), (val)) ; \
} while (0)

/* Disassembly support.  */

/* Function to use for cgen-based disassemblers.  */
extern CGEN_DISASSEMBLER sim_cgen_disassemble_insn;

/* Pseudo FILE object for strings.  */
typedef struct {
  char *buffer;
  char *current;
} SFILE;

/* String printer for the disassembler.  */
extern int sim_disasm_sprintf (SFILE *, const char *, ...);

/* For opcodes based disassemblers.  */
#ifdef __BFD_H_SEEN__
struct disassemble_info;
extern int
sim_disasm_read_memory (bfd_vma memaddr_, bfd_byte *myaddr_, unsigned int length_,
			struct disassemble_info *info_);
extern void
sim_disasm_perror_memory (int status_, bfd_vma memaddr_,
			  struct disassemble_info *info_);
#endif

#endif /* CGEN_TRACE_H */
