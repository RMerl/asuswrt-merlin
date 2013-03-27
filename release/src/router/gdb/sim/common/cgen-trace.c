/* Tracing support for CGEN-based simulators.
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

#include <errno.h>
#include "dis-asm.h"
#include "bfd.h"
#include "sim-main.h"
#include "sim-fpu.h"

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

#ifndef SIZE_INSTRUCTION
#define SIZE_INSTRUCTION 16
#endif

#ifndef SIZE_LOCATION
#define SIZE_LOCATION 20
#endif

#ifndef SIZE_PC
#define SIZE_PC 6
#endif

#ifndef SIZE_LINE_NUMBER
#define SIZE_LINE_NUMBER 4
#endif

#ifndef SIZE_CYCLE_COUNT
#define SIZE_CYCLE_COUNT 2
#endif

#ifndef SIZE_TOTAL_CYCLE_COUNT
#define SIZE_TOTAL_CYCLE_COUNT 9
#endif

#ifndef SIZE_TRACE_BUF
#define SIZE_TRACE_BUF 1024
#endif

/* Text is queued in TRACE_BUF because we want to output the insn's cycle
   count first but that isn't known until after the insn has executed.
   This also handles the queueing of trace results, TRACE_RESULT may be
   called multiple times for one insn.  */
static char trace_buf[SIZE_TRACE_BUF];
/* If NULL, output to stdout directly.  */
static char *bufptr;

/* Non-zero if this is the first insn in a set of parallel insns.  */
static int first_insn_p;

/* For communication between trace_insn and trace_result.  */
static int printed_result_p;

/* Insn and its extracted fields.
   Set by trace_insn, used by trace_insn_fini.
   ??? Move to SIM_CPU to support heterogeneous multi-cpu case.  */
static const struct cgen_insn *current_insn;
static const struct argbuf *current_abuf;

void
trace_insn_init (SIM_CPU *cpu, int first_p)
{
  bufptr = trace_buf;
  *bufptr = 0;
  first_insn_p = first_p;

  /* Set to NULL so trace_insn_fini can know if trace_insn was called.  */
  current_insn = NULL;
  current_abuf = NULL;
}

void
trace_insn_fini (SIM_CPU *cpu, const struct argbuf *abuf, int last_p)
{
  SIM_DESC sd = CPU_STATE (cpu);

  /* Was insn traced?  It might not be if trace ranges are in effect.  */
  if (current_insn == NULL)
    return;

  /* The first thing printed is current and total cycle counts.  */

  if (PROFILE_MODEL_P (cpu)
      && ARGBUF_PROFILE_P (current_abuf))
    {
      unsigned long total = PROFILE_MODEL_TOTAL_CYCLES (CPU_PROFILE_DATA (cpu));
      unsigned long this_insn = PROFILE_MODEL_CUR_INSN_CYCLES (CPU_PROFILE_DATA (cpu));

      if (last_p)
	{
	  trace_printf (sd, cpu, "%-*ld %-*ld ",
			SIZE_CYCLE_COUNT, this_insn,
			SIZE_TOTAL_CYCLE_COUNT, total);
	}
      else
	{
	  trace_printf (sd, cpu, "%-*ld %-*s ",
			SIZE_CYCLE_COUNT, this_insn,
			SIZE_TOTAL_CYCLE_COUNT, "---");
	}
    }

  /* Print the disassembled insn.  */

  trace_printf (sd, cpu, "%s", TRACE_PREFIX (CPU_TRACE_DATA (cpu)));

#if 0
  /* Print insn results.  */
  {
    const CGEN_OPINST *opinst = CGEN_INSN_OPERANDS (current_insn);

    if (opinst)
      {
	int i;
	int indices[MAX_OPERAND_INSTANCES];

	/* Fetch the operands used by the insn.  */
	/* FIXME: Add fn ptr to CGEN_CPU_DESC.  */
	CGEN_SYM (get_insn_operands) (CPU_CPU_DESC (cpu), current_insn,
				      0, CGEN_FIELDS_BITSIZE (&insn_fields),
				      indices);

	for (i = 0;
	     CGEN_OPINST_TYPE (opinst) != CGEN_OPINST_END;
	     ++i, ++opinst)
	  {
	    if (CGEN_OPINST_TYPE (opinst) == CGEN_OPINST_OUTPUT)
	      trace_result (cpu, current_insn, opinst, indices[i]);
	  }
      }
  }
#endif

  /* Print anything else requested.  */

  if (*trace_buf)
    trace_printf (sd, cpu, " %s\n", trace_buf);
  else
    trace_printf (sd, cpu, "\n");
}

void
trace_insn (SIM_CPU *cpu, const struct cgen_insn *opcode,
	    const struct argbuf *abuf, IADDR pc)
{
  char disasm_buf[50];

  printed_result_p = 0;
  current_insn = opcode;
  current_abuf = abuf;

  if (CGEN_INSN_VIRTUAL_P (opcode))
    {
      trace_prefix (CPU_STATE (cpu), cpu, NULL_CIA, pc, 0,
		    NULL, 0, CGEN_INSN_NAME (opcode));
      return;
    }

  CPU_DISASSEMBLER (cpu) (cpu, opcode, abuf, pc, disasm_buf);
  trace_prefix (CPU_STATE (cpu), cpu, NULL_CIA, pc, TRACE_LINENUM_P (cpu),
		NULL, 0,
		"%s%-*s",
		first_insn_p ? " " : "|",
		SIZE_INSTRUCTION, disasm_buf);
}

void
trace_extract (SIM_CPU *cpu, IADDR pc, char *name, ...)
{
  va_list args;
  int printed_one_p = 0;
  char *fmt;

  va_start (args, name);

  trace_printf (CPU_STATE (cpu), cpu, "Extract: 0x%.*lx: %s ",
		SIZE_PC, pc, name);

  do {
    int type,ival;

    fmt = va_arg (args, char *);

    if (fmt)
      {
	if (printed_one_p)
	  trace_printf (CPU_STATE (cpu), cpu, ", ");
	printed_one_p = 1;
	type = va_arg (args, int);
	switch (type)
	  {
	  case 'x' :
	    ival = va_arg (args, int);
	    trace_printf (CPU_STATE (cpu), cpu, fmt, ival);
	    break;
	  default :
	    abort ();
	  }
      }
  } while (fmt);

  va_end (args);
  trace_printf (CPU_STATE (cpu), cpu, "\n");
}

void
trace_result (SIM_CPU *cpu, char *name, int type, ...)
{
  va_list args;

  va_start (args, type);
  if (printed_result_p)
    cgen_trace_printf (cpu, ", ");

  switch (type)
    {
    case 'x' :
    default :
      cgen_trace_printf (cpu, "%s <- 0x%x", name, va_arg (args, int));
      break;
    case 'f':
      {
	DI di;
	sim_fpu f;

	/* this is separated from previous line for sunos cc */
	di = va_arg (args, DI);
	sim_fpu_64to (&f, di);

	cgen_trace_printf (cpu, "%s <- ", name);
	sim_fpu_printn_fpu (&f, (sim_fpu_print_func *) cgen_trace_printf, 4, cpu);
	break;
      }
    case 'D' :
      {
	DI di;
	/* this is separated from previous line for sunos cc */
	di = va_arg (args, DI);
	cgen_trace_printf (cpu, "%s <- 0x%x%08x", name,
			   GETHIDI(di), GETLODI (di));
	break;
      }
    }

  printed_result_p = 1;
  va_end (args);
}

/* Print trace output to BUFPTR if active, otherwise print normally.
   This is only for tracing semantic code.  */

void
cgen_trace_printf (SIM_CPU *cpu, char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);

  if (bufptr == NULL)
    {
      if (TRACE_FILE (CPU_TRACE_DATA (cpu)) == NULL)
	(* STATE_CALLBACK (CPU_STATE (cpu))->evprintf_filtered)
	  (STATE_CALLBACK (CPU_STATE (cpu)), fmt, args);
      else
	vfprintf (TRACE_FILE (CPU_TRACE_DATA (cpu)), fmt, args);
    }
  else
    {
      vsprintf (bufptr, fmt, args);
      bufptr += strlen (bufptr);
      /* ??? Need version of SIM_ASSERT that is always enabled.  */
      if (bufptr - trace_buf > SIZE_TRACE_BUF)
	abort ();
    }

  va_end (args);
}

/* Disassembly support.  */

/* sprintf to a "stream" */

int
sim_disasm_sprintf VPARAMS ((SFILE *f, const char *format, ...))
{
#ifndef __STDC__
  SFILE *f;
  const char *format;
#endif
  int n;
  va_list args;

  VA_START (args, format);
#ifndef __STDC__
  f = va_arg (args, SFILE *);
  format = va_arg (args, char *);
#endif
  vsprintf (f->current, format, args);
  f->current += n = strlen (f->current);
  va_end (args);
  return n;
}

/* Memory read support for an opcodes disassembler.  */

int
sim_disasm_read_memory (bfd_vma memaddr, bfd_byte *myaddr, unsigned int length,
			struct disassemble_info *info)
{
  SIM_CPU *cpu = (SIM_CPU *) info->application_data;
  SIM_DESC sd = CPU_STATE (cpu);
  unsigned length_read;

  length_read = sim_core_read_buffer (sd, cpu, read_map, myaddr, memaddr,
				      length);
  if (length_read != length)
    return EIO;
  return 0;
}

/* Memory error support for an opcodes disassembler.  */

void
sim_disasm_perror_memory (int status, bfd_vma memaddr,
			  struct disassemble_info *info)
{
  if (status != EIO)
    /* Can't happen.  */
    info->fprintf_func (info->stream, "Unknown error %d.", status);
  else
    /* Actually, address between memaddr and memaddr + len was
       out of bounds.  */
    info->fprintf_func (info->stream,
			"Address 0x%x is out of bounds.",
			(int) memaddr);
}

/* Disassemble using the CGEN opcode table.
   ??? While executing an instruction, the insn has been decoded and all its
   fields have been extracted.  It is certainly possible to do the disassembly
   with that data.  This seems simpler, but maybe in the future the already
   extracted fields will be used.  */

void
sim_cgen_disassemble_insn (SIM_CPU *cpu, const CGEN_INSN *insn,
			   const ARGBUF *abuf, IADDR pc, char *buf)
{
  unsigned int length;
  unsigned int base_length;
  unsigned long insn_value;
  struct disassemble_info disasm_info;
  SFILE sfile;
  union {
    unsigned8 bytes[CGEN_MAX_INSN_SIZE];
    unsigned16 shorts[8];
    unsigned32 words[4];
  } insn_buf;
  SIM_DESC sd = CPU_STATE (cpu);
  CGEN_CPU_DESC cd = CPU_CPU_DESC (cpu);
  CGEN_EXTRACT_INFO ex_info;
  CGEN_FIELDS *fields = alloca (CGEN_CPU_SIZEOF_FIELDS (cd));
  int insn_bit_length = CGEN_INSN_BITSIZE (insn);
  int insn_length = insn_bit_length / 8;

  sfile.buffer = sfile.current = buf;
  INIT_DISASSEMBLE_INFO (disasm_info, (FILE *) &sfile,
			 (fprintf_ftype) sim_disasm_sprintf);
  disasm_info.endian =
    (bfd_big_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_BIG
     : bfd_little_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_LITTLE
     : BFD_ENDIAN_UNKNOWN);

  length = sim_core_read_buffer (sd, cpu, read_map, &insn_buf, pc,
				 insn_length);

  if (length != insn_length)
  {
    sim_io_error (sd, "unable to read address %x", pc);
  }

  /* If the entire insn will fit into an integer, then do it. Otherwise, just
     use the bits of the base_insn.  */
  if (insn_bit_length <= 32)
    base_length = insn_bit_length;
  else
    base_length = min (cd->base_insn_bitsize, insn_bit_length);
  switch (base_length)
    {
    case 0 : return; /* fake insn, typically "compile" (aka "invalid") */
    case 8 : insn_value = insn_buf.bytes[0]; break;
    case 16 : insn_value = T2H_2 (insn_buf.shorts[0]); break;
    case 32 : insn_value = T2H_4 (insn_buf.words[0]); break;
    default: abort ();
    }

  disasm_info.buffer_vma = pc;
  disasm_info.buffer = insn_buf.bytes;
  disasm_info.buffer_length = length;

  ex_info.dis_info = (PTR) &disasm_info;
  ex_info.valid = (1 << length) - 1;
  ex_info.insn_bytes = insn_buf.bytes;

  length = (*CGEN_EXTRACT_FN (cd, insn)) (cd, insn, &ex_info, insn_value, fields, pc);
  /* Result of extract fn is in bits.  */
  /* ??? This assumes that each instruction has a fixed length (and thus
     for insns with multiple versions of variable lengths they would each
     have their own table entry).  */
  if (length == insn_bit_length)
    {
      (*CGEN_PRINT_FN (cd, insn)) (cd, &disasm_info, insn, fields, pc, length);
    }
  else
    {
      /* This shouldn't happen, but aborting is too drastic.  */
      strcpy (buf, "***unknown***");
    }
}
