/* reg.c --- register set model for M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

int verbose = 0;
int trace = 0;
int enable_counting = 0;

regs_type regs;
int addr_mask = 0xffff;
int membus_mask = 0xfffff;
int m32c_cpu = 0;
int step_result;
unsigned int heapbottom = 0;
unsigned int heaptop = 0;

char *reg_names[] = {
  "mem",
  "r0", "r0h", "r0l",
  "r1", "r1h", "r1l",
  "r2", "r2r0",
  "r3", "r3r1",
  "r3r1r2r0",
  "r3r2r1r0",
  "a0",
  "a1", "a1a0",
  "sb", "fb",
  "intb", "intbl", "intbh",
  "sp", "usp", "isp", "pc", "flags"
};

int reg_bytes[] = {
  0,
  2, 1, 1,
  2, 1, 1,
  2, 4,
  2, 4,
  8,
  8,
  2,
  2, 4,
  2, 2,
  2, 1, 3,
  2, 2, 2, 3, 2
};


unsigned int b2mask[] = { 0, 0xff, 0xffff, 0xffffff, 0xffffffff };
unsigned int b2signbit[] = { 0, (1 << 7), (1 << 15), (1 << 24), (1 << 31) };
int b2maxsigned[] = { 0, 0x7f, 0x7fff, 0x7fffff, 0x7fffffff };
int b2minsigned[] = { 0, -128, -32768, -8388608, -2147483647 - 1 };

static regs_type oldregs;

void
init_regs (void)
{
  memset (&regs, 0, sizeof (regs));
  memset (&oldregs, 0, sizeof (oldregs));
}

void
set_pointer_width (int bytes)
{
  if (bytes == 2)
    {
      addr_mask = 0xffff;
      membus_mask = 0x000fffff;
      reg_bytes[a0] = reg_bytes[a1] = reg_bytes[sb] = reg_bytes[fb] =
	reg_bytes[sp] = reg_bytes[usp] = reg_bytes[isp] = 2;
    }
  else
    {
      addr_mask = 0xffffff;
      membus_mask = 0x00ffffff;
      reg_bytes[a0] = reg_bytes[a1] = reg_bytes[sb] = reg_bytes[fb] =
	reg_bytes[sp] = reg_bytes[usp] = reg_bytes[isp] = 3;
    }
}

void
m32c_set_cpu (int cpu)
{
  switch (cpu)
    {
    case CPU_R8C:
    case CPU_M16C:
      set_pointer_width (2);
      decode_opcode = decode_r8c;
      break;
    case CPU_M32CM:
    case CPU_M32C:
      set_pointer_width (3);
      decode_opcode = decode_m32c;
      break;
    default:
      abort ();
    }
  m32c_cpu = cpu;
}

static unsigned int
get_reg_i (reg_id id)
{
  reg_bank_type *b = regs.r + (FLAG_B ? 1 : 0);

  switch (id)
    {
    case r0:
      return b->r_r0;
    case r0h:
      return b->r_r0 >> 8;
    case r0l:
      return b->r_r0 & 0xff;
    case r1:
      return b->r_r1;
    case r1h:
      return b->r_r1 >> 8;
    case r1l:
      return b->r_r1 & 0xff;
    case r2:
      return b->r_r2;
    case r2r0:
      return b->r_r2 * 65536 + b->r_r0;
    case r3:
      return b->r_r3;
    case r3r1:
      return b->r_r3 * 65536 + b->r_r1;

    case a0:
      return b->r_a0 & addr_mask;
    case a1:
      return b->r_a1 & addr_mask;
    case a1a0:
      return (b->r_a1 & 0xffff) * 65536 | (b->r_a0 & 0xffff);

    case sb:
      return b->r_sb & addr_mask;
    case fb:
      return b->r_fb & addr_mask;

    case intb:
      return regs.r_intbh * 65536 + regs.r_intbl;
    case intbl:
      return regs.r_intbl;
    case intbh:
      return regs.r_intbh;

    case sp:
      return ((regs.r_flags & FLAGBIT_U) ? regs.r_usp : regs.
	      r_isp) & addr_mask;
    case usp:
      return regs.r_usp & addr_mask;
    case isp:
      return regs.r_isp & addr_mask;

    case pc:
      return regs.r_pc & membus_mask;
    case flags:
      return regs.r_flags;
    default:
      abort ();
    }
}

unsigned int
get_reg (reg_id id)
{
  unsigned int rv = get_reg_i (id);
  if (trace > ((id != pc && id != fb && id != sp) ? 0 : 1))
    printf ("get_reg (%s) = %0*x\n", reg_names[id], reg_bytes[id] * 2, rv);
  return rv;
}

DI
get_reg_ll (reg_id id)
{
  reg_bank_type *b = regs.r + (FLAG_B ? 1 : 0);

  switch (id)
    {
    case r3r1r2r0:
      return ((DI) b->r_r3 << 48
	      | (DI) b->r_r1 << 32 | (DI) b->r_r2 << 16 | (DI) b->r_r0);
    case r3r2r1r0:
      return ((DI) b->r_r3 << 48
	      | (DI) b->r_r2 << 32 | (DI) b->r_r1 << 16 | (DI) b->r_r0);
    default:
      return get_reg (id);
    }
}

static int highest_sp = 0, lowest_sp = 0xffffff;

void
stack_heap_stats ()
{
  printf ("heap:  %08x - %08x (%d bytes)\n", heapbottom, heaptop,
	  heaptop - heapbottom);
  printf ("stack: %08x - %08x (%d bytes)\n", lowest_sp, highest_sp,
	  highest_sp - lowest_sp);
}

void
put_reg (reg_id id, unsigned int v)
{
  if (trace > ((id != pc) ? 0 : 1))
    printf ("put_reg (%s) = %0*x\n", reg_names[id], reg_bytes[id] * 2, v);

  reg_bank_type *b = regs.r + (FLAG_B ? 1 : 0);
  switch (id)
    {
    case r0:
      b->r_r0 = v;
      break;
    case r0h:
      b->r_r0 = (b->r_r0 & 0xff) | (v << 8);
      break;
    case r0l:
      b->r_r0 = (b->r_r0 & 0xff00) | (v & 0xff);
      break;
    case r1:
      b->r_r1 = v;
      break;
    case r1h:
      b->r_r1 = (b->r_r1 & 0xff) | (v << 8);
      break;
    case r1l:
      b->r_r1 = (b->r_r1 & 0xff00) | (v & 0xff);
      break;
    case r2:
      b->r_r2 = v;
      break;
    case r2r0:
      b->r_r0 = v & 0xffff;
      b->r_r2 = v >> 16;
      break;
    case r3:
      b->r_r3 = v;
      break;
    case r3r1:
      b->r_r1 = v & 0xffff;
      b->r_r3 = v >> 16;
      break;

    case a0:
      b->r_a0 = v & addr_mask;
      break;
    case a1:
      b->r_a1 = v & addr_mask;
      break;
    case a1a0:
      b->r_a0 = v & 0xffff;
      b->r_a1 = v >> 16;
      break;

    case sb:
      b->r_sb = v & addr_mask;
      break;
    case fb:
      b->r_fb = v & addr_mask;
      break;

    case intb:
      regs.r_intbl = v & 0xffff;
      regs.r_intbh = v >> 16;
      break;
    case intbl:
      regs.r_intbl = v & 0xffff;
      break;
    case intbh:
      regs.r_intbh = v & 0xff;
      break;

    case sp:
      {
	SI *spp;
	if (regs.r_flags & FLAGBIT_U)
	  spp = &regs.r_usp;
	else
	  spp = &regs.r_isp;
	*spp = v & addr_mask;
	if (*spp < heaptop)
	  {
	    printf ("collision: pc %08lx heap %08x stack %08lx\n", regs.r_pc,
		    heaptop, *spp);
	    exit (1);
	  }
	if (*spp < lowest_sp)
	  lowest_sp = *spp;
	if (*spp > highest_sp)
	  highest_sp = *spp;
	break;
      }
    case usp:
      regs.r_usp = v & addr_mask;
      break;
    case isp:
      regs.r_isp = v & addr_mask;
      break;

    case pc:
      regs.r_pc = v & membus_mask;
      break;
    case flags:
      regs.r_flags = v;
      break;
    default:
      abort ();
    }
}

int
condition_true (int cond_id)
{
  int f;
  if (A16)
    {
      static const char *cond_name[] = {
	"C", "C&!Z", "Z", "S",
	"!C", "!(C&!Z)", "!Z", "!S",
	"(S^O)|Z", "O", "!(S^O)", "unk",
	"!((S^O)|Z)", "!O", "S^O", "unk"
      };
      switch (cond_id & 15)
	{
	case 0:
	  f = FLAG_C;
	  break;		/* GEU/C */
	case 1:
	  f = FLAG_C & !FLAG_Z;
	  break;		/* GTU */
	case 2:
	  f = FLAG_Z;
	  break;		/* EQ/Z */
	case 3:
	  f = FLAG_S;
	  break;		/* N */
	case 4:
	  f = !FLAG_C;
	  break;		/* LTU/NC */
	case 5:
	  f = !(FLAG_C & !FLAG_Z);
	  break;		/* LEU */
	case 6:
	  f = !FLAG_Z;
	  break;		/* NE/NZ */
	case 7:
	  f = !FLAG_S;
	  break;		/* PZ */

	case 8:
	  f = (FLAG_S ^ FLAG_O) | FLAG_Z;
	  break;		/* LE */
	case 9:
	  f = FLAG_O;
	  break;		/* O */
	case 10:
	  f = !(FLAG_S ^ FLAG_O);
	  break;		/* GE */
	case 12:
	  f = !((FLAG_S ^ FLAG_O) | FLAG_Z);
	  break;		/* GT */
	case 13:
	  f = !FLAG_O;
	  break;		/* NO */
	case 14:
	  f = FLAG_S ^ FLAG_O;
	  break;		/* LT */

	default:
	  f = 0;
	  break;
	}
      if (trace)
	printf ("cond[%d] %s = %s\n", cond_id, cond_name[cond_id & 15],
		f ? "true" : "false");
    }
  else
    {
      static const char *cond_name[] = {
	"!C", "LEU", "!Z", "PZ",
	"!O", "GT", "GE", "?",
	"C", "GTU", "Z", "N",
	"O", "LE", "LT", "!?"
      };
      switch (cond_id & 15)
	{
	case 0:
	  f = !FLAG_C;
	  break;		/* LTU/NC */
	case 1:
	  f = !(FLAG_C & !FLAG_Z);
	  break;		/* LEU */
	case 2:
	  f = !FLAG_Z;
	  break;		/* NE/NZ */
	case 3:
	  f = !FLAG_S;
	  break;		/* PZ */

	case 4:
	  f = !FLAG_O;
	  break;		/* NO */
	case 5:
	  f = !((FLAG_S ^ FLAG_O) | FLAG_Z);
	  break;		/* GT */
	case 6:
	  f = !(FLAG_S ^ FLAG_O);
	  break;		/* GE */

	case 8:
	  f = FLAG_C;
	  break;		/* GEU/C */
	case 9:
	  f = FLAG_C & !FLAG_Z;
	  break;		/* GTU */
	case 10:
	  f = FLAG_Z;
	  break;		/* EQ/Z */
	case 11:
	  f = FLAG_S;
	  break;		/* N */

	case 12:
	  f = FLAG_O;
	  break;		/* O */
	case 13:
	  f = (FLAG_S ^ FLAG_O) | FLAG_Z;
	  break;		/* LE */
	case 14:
	  f = FLAG_S ^ FLAG_O;
	  break;		/* LT */

	default:
	  f = 0;
	  break;
	}
      if (trace)
	printf ("cond[%d] %s = %s\n", cond_id, cond_name[cond_id & 15],
		f ? "true" : "false");
    }
  return f;
}

void
set_flags (int mask, int newbits)
{
  int i;
  regs.r_flags &= ~mask;
  regs.r_flags |= newbits & mask;
  if (trace)
    {
      printf ("flags now \033[32m %d", (regs.r_flags >> (A16 ? 8 : 12)) & 7);
      for (i = 7; i >= 0; i--)
	if (regs.r_flags & (1 << i))
	  putchar ("CDZSBOIU"[i]);
	else
	  putchar ('-');
      printf ("\033[0m\n");
    }
}

void
set_oszc (int value, int b, int c)
{
  int mask = b2mask[b];
  int f = 0;

  if (c)
    f |= FLAGBIT_C;
  if ((value & mask) == 0)
    f |= FLAGBIT_Z;
  if (value & b2signbit[b])
    f |= FLAGBIT_S;
  if ((value > b2maxsigned[b]) || (value < b2minsigned[b]))
    f |= FLAGBIT_O;
  set_flags (FLAGBIT_Z | FLAGBIT_S | FLAGBIT_O | FLAGBIT_C, f);
}

void
set_szc (int value, int b, int c)
{
  int mask = b2mask[b];
  int f = 0;

  if (c)
    f |= FLAGBIT_C;
  if ((value & mask) == 0)
    f |= FLAGBIT_Z;
  if (value & b2signbit[b])
    f |= FLAGBIT_S;
  set_flags (FLAGBIT_Z | FLAGBIT_S | FLAGBIT_C, f);
}

void
set_osz (int value, int b)
{
  int mask = b2mask[b];
  int f = 0;

  if ((value & mask) == 0)
    f |= FLAGBIT_Z;
  if (value & b2signbit[b])
    f |= FLAGBIT_S;
  if (value & ~mask && (value & ~mask) != ~mask)
    f |= FLAGBIT_O;
  set_flags (FLAGBIT_Z | FLAGBIT_S | FLAGBIT_O, f);
}

void
set_sz (int value, int b)
{
  int mask = b2mask[b];
  int f = 0;

  if ((value & mask) == 0)
    f |= FLAGBIT_Z;
  if (value & b2signbit[b])
    f |= FLAGBIT_S;
  set_flags (FLAGBIT_Z | FLAGBIT_S, f);
}

void
set_zc (int z, int c)
{
  set_flags (FLAGBIT_C | FLAGBIT_Z,
	     (c ? FLAGBIT_C : 0) | (z ? FLAGBIT_Z : 0));
}

void
set_c (int c)
{
  set_flags (FLAGBIT_C, c ? FLAGBIT_C : 0);
}

void
put_reg_ll (reg_id id, DI v)
{
  reg_bank_type *b = regs.r + (FLAG_B ? 1 : 0);

  switch (id)
    {
    case r3r1r2r0:
      b->r_r3 = v >> 48;
      b->r_r1 = v >> 32;
      b->r_r2 = v >> 16;
      b->r_r0 = v;
      break;
    case r3r2r1r0:
      b->r_r3 = v >> 48;
      b->r_r2 = v >> 32;
      b->r_r1 = v >> 16;
      b->r_r0 = v;
      break;
    default:
      put_reg (id, v);
    }
}

#define TRC(f,n, id) \
  if (oldregs.f != regs.f) \
    { \
      printf("  %s %0*x:%0*x", n, \
	     reg_bytes[id]*2, (unsigned int)oldregs.f, \
	     reg_bytes[id]*2, (unsigned int)regs.f); \
      oldregs.f = regs.f; \
    }

void
trace_register_changes ()
{
  if (!trace)
    return;
  printf ("\033[36mREGS:");
  TRC (r[0].r_r0, "r0", r0);
  TRC (r[0].r_r1, "r1", r1);
  TRC (r[0].r_r2, "r2", r2);
  TRC (r[0].r_r3, "r3", r3);
  TRC (r[0].r_a0, "a0", a0);
  TRC (r[0].r_a1, "a1", a1);
  TRC (r[0].r_sb, "sb", sb);
  TRC (r[0].r_fb, "fb", fb);
  TRC (r[1].r_r0, "r0'", r0);
  TRC (r[1].r_r1, "r1'", r1);
  TRC (r[1].r_r2, "r2'", r2);
  TRC (r[1].r_r3, "r3'", r3);
  TRC (r[1].r_a0, "a0'", a0);
  TRC (r[1].r_a1, "a1'", a1);
  TRC (r[1].r_sb, "sb'", sb);
  TRC (r[1].r_fb, "fb'", fb);
  TRC (r_intbh, "intbh", intbh);
  TRC (r_intbl, "intbl", intbl);
  TRC (r_usp, "usp", usp);
  TRC (r_isp, "isp", isp);
  TRC (r_pc, "pc", pc);
  TRC (r_flags, "flags", flags);
  printf ("\033[0m\n");
}
