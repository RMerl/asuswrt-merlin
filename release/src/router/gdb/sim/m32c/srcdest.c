/* srcdest.c --- decoding M32C addressing modes.

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

#include "cpu.h"
#include "mem.h"

static int src_indirect = 0;
static int dest_indirect = 0;
static int src_addend = 0;
static int dest_addend = 0;

static int
disp8 ()
{
  int rv;
  int tsave = trace;

  if (trace == 1)
    trace = 0;
  rv = mem_get_qi (get_reg (pc));
  regs.r_pc++;
  trace = tsave;
  return rv;
}

static int
disp16 ()
{
  int rv;
  int tsave = trace;

  if (trace == 1)
    trace = 0;
  rv = mem_get_hi (get_reg (pc));
  regs.r_pc += 2;
  trace = tsave;
  return rv;
}

static int
disp24 ()
{
  int rv;
  int tsave = trace;

  if (trace == 1)
    trace = 0;
  rv = mem_get_psi (get_reg (pc));
  regs.r_pc += 3;
  trace = tsave;
  return rv;
}

static int
disp20 ()
{
  return disp24 () & 0x000fffff;
}

const char *
bits (int v, int b)
{
  static char buf[17];
  char *bp = buf + 16;
  *bp = 0;
  while (b)
    {
      *--bp = (v & 1) ? '1' : '0';
      v >>= 1;
      b--;
    }
  return bp;
}

static const char *the_bits = 0;

void
decode_indirect (int si, int di)
{
  src_indirect = si;
  dest_indirect = di;
  if (trace && (si || di))
    printf ("indirect: s:%d d:%d\n", si, di);
}

void
decode_index (int sa, int da)
{
  src_addend = sa;
  dest_addend = da;
  if (trace && (sa || da))
    printf ("index: s:%d d:%d\n", sa, da);
}

srcdest
decode_srcdest4 (int destcode, int bw)
{
  srcdest sd;
  sd.bytes = bw ? 2 : 1;
  sd.mem = (destcode >= 6) ? 1 : 0;
  static const char *dc_wnames[16] = { "r0", "r1", "r2", "r3",
    "a0", "a1", "[a0]", "[a1]",
    "disp8[a0]", "disp8[a1]", "disp8[sb]", "disp8[fb]",
    "disp16[a0]", "disp16[a1]", "disp16[sb]", "disp16"
  };
  static const char *dc_bnames[4] = { "r0l", "r0h", "r1l", "r1h" };;

  if (trace)
    {
      const char *n = dc_wnames[destcode];
      if (bw == 0 && destcode <= 3)
	n = dc_bnames[destcode];
      if (!the_bits)
	the_bits = bits (destcode, 4);
      printf ("decode: %s (%d) : %s\n", the_bits, destcode, n);
      the_bits = 0;
    }

  switch (destcode)
    {
    case 0x0:
      sd.u.reg = bw ? r0 : r0l;
      break;
    case 0x1:
      sd.u.reg = bw ? r1 : r0h;
      break;
    case 0x2:
      sd.u.reg = bw ? r2 : r1l;
      break;
    case 0x3:
      sd.u.reg = bw ? r3 : r1h;
      break;
    case 0x4:
      sd.u.reg = a0;
      break;
    case 0x5:
      sd.u.reg = a1;
      break;
    case 0x6:
      sd.u.addr = get_reg (a0);
      break;
    case 0x7:
      sd.u.addr = get_reg (a1);
      break;
    case 0x8:
      sd.u.addr = get_reg (a0) + disp8 ();
      break;
    case 0x9:
      sd.u.addr = get_reg (a1) + disp8 ();
      break;
    case 0xa:
      sd.u.addr = get_reg (sb) + disp8 ();
      break;
    case 0xb:
      sd.u.addr = get_reg (fb) + sign_ext (disp8 (), 8);
      break;
    case 0xc:
      sd.u.addr = get_reg (a0) + disp16 ();
      break;
    case 0xd:
      sd.u.addr = get_reg (a1) + disp16 ();
      break;
    case 0xe:
      sd.u.addr = get_reg (sb) + disp16 ();
      break;
    case 0xf:
      sd.u.addr = disp16 ();
      break;
    default:
      abort ();
    }
  if (sd.mem)
    sd.u.addr &= addr_mask;
  return sd;
}

srcdest
decode_jumpdest (int destcode, int w)
{
  srcdest sd;
  sd.bytes = w ? 2 : 3;
  sd.mem = (destcode >= 6) ? 1 : 0;
  static const char *dc_wnames[16] = { "r0", "r1", "r2", "r3",
    "a0", "a1", "[a0]", "[a1]",
    "disp8[a0]", "disp8[a1]", "disp8[sb]", "disp8[fb]",
    "disp20[a0]", "disp20[a1]", "disp16[sb]", "abs16"
  };
  static const char *dc_anames[4] = { "r0l", "r0h", "r1l", "r1h" };

  if (trace)
    {
      const char *n = dc_wnames[destcode];
      if (w == 0 && destcode <= 3)
	n = dc_anames[destcode];
      if (!the_bits)
	the_bits = bits (destcode, 4);
      printf ("decode: %s : %s\n", the_bits, n);
      the_bits = 0;
    }

  switch (destcode)
    {
    case 0x0:
      sd.u.reg = w ? r0 : r2r0;
      break;
    case 0x1:
      sd.u.reg = w ? r1 : r2r0;
      break;
    case 0x2:
      sd.u.reg = w ? r2 : r3r1;
      break;
    case 0x3:
      sd.u.reg = w ? r3 : r3r1;
      break;
    case 0x4:
      sd.u.reg = w ? a0 : a1a0;
      break;
    case 0x5:
      sd.u.reg = w ? a1 : a1a0;
      break;
    case 0x6:
      sd.u.addr = get_reg (a0);
      break;
    case 0x7:
      sd.u.addr = get_reg (a1);
      break;
    case 0x8:
      sd.u.addr = get_reg (a0) + disp8 ();
      break;
    case 0x9:
      sd.u.addr = get_reg (a1) + disp8 ();
      break;
    case 0xa:
      sd.u.addr = get_reg (sb) + disp8 ();
      break;
    case 0xb:
      sd.u.addr = get_reg (fb) + sign_ext (disp8 (), 8);
      break;
    case 0xc:
      sd.u.addr = get_reg (a0) + disp20 ();
      break;
    case 0xd:
      sd.u.addr = get_reg (a1) + disp20 ();
      break;
    case 0xe:
      sd.u.addr = get_reg (sb) + disp16 ();
      break;
    case 0xf:
      sd.u.addr = disp16 ();
      break;
    default:
      abort ();
    }
  if (sd.mem)
    sd.u.addr &= addr_mask;
  return sd;
}

srcdest
decode_dest3 (int destcode, int bw)
{
  static char map[8] = { -1, -1, -1, 1, 0, 10, 11, 15 };

  the_bits = bits (destcode, 3);
  return decode_srcdest4 (map[destcode], bw);
}

srcdest
decode_src2 (int srccode, int bw, int d)
{
  static char map[4] = { 0, 10, 11, 15 };

  the_bits = bits (srccode, 2);
  return decode_srcdest4 (srccode ? map[srccode] : 1 - d, bw);
}

static struct
{
  reg_id b_regno;
  reg_id w_regno;
  int is_memory;
  int disp_bytes;
  char *name;
} modes23[] =
{
  {
  a0, a0, 1, 0, "[A0]"},	/* 0 0 0 0 0 */
  {
  a1, a1, 1, 0, "[A1]"},	/* 0 0 0 0 1 */
  {
  a0, a0, 0, 0, "A0"},		/* 0 0 0 1 0 */
  {
  a1, a1, 0, 0, "A1"},		/* 0 0 0 1 1 */
  {
  a0, a0, 1, 1, "dsp:8[A0]"},	/* 0 0 1 0 0 */
  {
  a1, a1, 1, 1, "dsp:8[A1]"},	/* 0 0 1 0 1 */
  {
  sb, sb, 1, 1, "dsp:8[SB]"},	/* 0 0 1 1 0 */
  {
  fb, fb, 1, -1, "dsp:8[FB]"},	/* 0 0 1 1 1 */
  {
  a0, a0, 1, 2, "dsp:16[A0]"},	/* 0 1 0 0 0 */
  {
  a1, a1, 1, 2, "dsp:16[A1]"},	/* 0 1 0 0 1 */
  {
  sb, sb, 1, 2, "dsp:16[SB]"},	/* 0 1 0 1 0 */
  {
  fb, fb, 1, -2, "dsp:16[FB]"},	/* 0 1 0 1 1 */
  {
  a0, a0, 1, 3, "dsp:24[A0]"},	/* 0 1 1 0 0 */
  {
  a1, a1, 1, 3, "dsp:24[A1]"},	/* 0 1 1 0 1 */
  {
  mem, mem, 1, 3, "abs24"},	/* 0 1 1 1 0 */
  {
  mem, mem, 1, 2, "abs16"},	/* 0 1 1 1 1 */
  {
  r0h, r2, 0, 0, "R0H/R2"},	/* 1 0 0 0 0 */
  {
  r1h, r3, 0, 0, "R1H/R3"},	/* 1 0 0 0 1 */
  {
  r0l, r0, 0, 0, "R0L/R0"},	/* 1 0 0 1 0 */
  {
  r1l, r1, 0, 0, "R1L/R1"},	/* 1 0 0 1 1 */
};

static srcdest
decode_sd23 (int bbb, int bb, int bytes, int ind, int add)
{
  srcdest sd;
  int code = (bbb << 2) | bb;

  if (code >= sizeof (modes23) / sizeof (modes23[0]))
    abort ();

  if (trace)
    {
      char *b1 = "";
      char *b2 = "";
      char ad[30];
      if (ind)
	{
	  b1 = "[";
	  b2 = "]";
	}
      if (add)
	sprintf (ad, "%+d", add);
      else
	ad[0] = 0;
      if (!the_bits)
	the_bits = bits (code, 4);
      printf ("decode: %s (%d) : %s%s%s%s\n", the_bits, code, b1,
	      modes23[code].name, ad, b2);
      the_bits = 0;
    }

  sd.bytes = bytes;
  sd.mem = modes23[code].is_memory;
  if (sd.mem)
    {
      if (modes23[code].w_regno == mem)
	sd.u.addr = 0;
      else
	sd.u.addr = get_reg (modes23[code].w_regno);
      switch (modes23[code].disp_bytes)
	{
	case 1:
	  sd.u.addr += disp8 ();
	  break;
	case 2:
	  sd.u.addr += disp16 ();
	  break;
	case -1:
	  sd.u.addr += sign_ext (disp8 (), 8);
	  break;
	case -2:
	  sd.u.addr += sign_ext (disp16 (), 16);
	  break;
	case 3:
	  sd.u.addr += disp24 ();
	  break;
	default:
	  break;
	}
      if (add)
	sd.u.addr += add;
      if (ind)
	sd.u.addr = mem_get_si (sd.u.addr & membus_mask);
      sd.u.addr &= membus_mask;
    }
  else
    {
      sd.u.reg = (bytes > 1) ? modes23[code].w_regno : modes23[code].b_regno;
      if (bytes == 3 || bytes == 4)
	{
	  switch (sd.u.reg)
	    {
	    case r0:
	      sd.u.reg = r2r0;
	      break;
	    case r1:
	      sd.u.reg = r3r1;
	      break;
	    case r2:
	      abort ();
	    case r3:
	      abort ();
	    default:;
	    }
	}

    }
  return sd;
}

srcdest
decode_dest23 (int ddd, int dd, int bytes)
{
  return decode_sd23 (ddd, dd, bytes, dest_indirect, dest_addend);
}

srcdest
decode_src23 (int sss, int ss, int bytes)
{
  return decode_sd23 (sss, ss, bytes, src_indirect, src_addend);
}

srcdest
decode_dest2 (int dd, int bytes)
{
  /* r0l/r0, abs16, dsp:8[SB], dsp:8[FB] */
  static char map[4] = { 0x12, 0x0f, 0x06, 0x07 };

  the_bits = bits (dd, 2);
  return decode_sd23 (map[dd] >> 2, map[dd] & 3, bytes, dest_indirect,
		      dest_addend);
}

srcdest
decode_src3 (int sss, int bytes)
{
  /* r0, r1, a0, a1, r2, r3, N/A, N/A */
  static char map[8] = { 0x12, 0x13, 0x02, 0x03, 0x10, 0x11, 0, 0 };

  the_bits = bits (sss, 3);
  return decode_sd23 (map[sss] >> 2, map[sss] & 3, bytes, src_indirect,
		      src_addend);
}

srcdest
decode_dest1 (int destcode, int bw)
{
  the_bits = bits (destcode, 1);
  return decode_srcdest4 (destcode, bw);
}

srcdest
decode_cr (int crcode)
{
  static int regcode[] = { 0, intbl, intbh, flags, isp, sp, sb, fb };
  srcdest sd;
  sd.mem = 0;
  sd.bytes = 2;
  sd.u.reg = regcode[crcode & 7];
  return sd;
}

srcdest
decode_cr_b (int crcode, int bank)
{
  /* FIXME: intbl, intbh, isp */
  static int regcode[3][8] = {
    {0, 0, flags, 0, 0, 0, 0, 0},
    {intb, sp, sb, fb, 0, 0, 0, isp},
    {0, 0, 0, 0, 0, 0, 0, 0}
  };
  srcdest sd;
  sd.mem = 0;
  sd.bytes = bank ? 3 : 2;
  sd.u.reg = regcode[bank][crcode & 7];
  return sd;
}

srcdest
widen_sd (srcdest sd)
{
  sd.bytes *= 2;
  if (!sd.mem)
    switch (sd.u.reg)
      {
      case r0l:
	sd.u.reg = r0;
	break;
      case r0:
	sd.u.reg = r2r0;
	break;
      case r1l:
	sd.u.reg = r1;
	break;
      case r1:
	sd.u.reg = r3r1;
	break;
      case a0:
	if (A16)
	  sd.u.reg = a1a0;
	break;
      default:
	break;
      }
  return sd;
}

srcdest
reg_sd (reg_id reg)
{
  srcdest rv;
  rv.bytes = reg_bytes[reg];
  rv.mem = 0;
  rv.u.reg = reg;
  return rv;
}

int
get_src (srcdest sd)
{
  int v;
  if (sd.mem)
    {
      switch (sd.bytes)
	{
	case 1:
	  v = mem_get_qi (sd.u.addr);
	  break;
	case 2:
	  v = mem_get_hi (sd.u.addr);
	  break;
	case 3:
	  v = mem_get_psi (sd.u.addr);
	  break;
	case 4:
	  v = mem_get_si (sd.u.addr);
	  break;
	default:
	  abort ();
	}
    }
  else
    {
      v = get_reg (sd.u.reg);
      switch (sd.bytes)
	{
	case 1:
	  v &= 0xff;
	  break;
	case 2:
	  v &= 0xffff;
	  break;
	case 3:
	  v &= 0xffffff;
	  break;
	}
    }
  return v;
}

void
put_dest (srcdest sd, int v)
{
  if (sd.mem)
    {
      switch (sd.bytes)
	{
	case 1:
	  mem_put_qi (sd.u.addr, v);
	  break;
	case 2:
	  mem_put_hi (sd.u.addr, v);
	  break;
	case 3:
	  mem_put_psi (sd.u.addr, v);
	  break;
	case 4:
	  mem_put_si (sd.u.addr, v);
	  break;
	}
    }
  else
    {
      switch (sd.bytes)
	{
	case 1:
	  v &= 0xff;
	  break;
	case 2:
	  v &= 0xffff;
	  break;
	case 3:
	  v &= 0xffffff;
	  break;
	}
      put_reg (sd.u.reg, v);
    }
}

srcdest
decode_bit (int destcode)
{
  srcdest sd;
  int addr = 0;
  static const char *dc_names[] = { "r0", "r1", "r2", "r3",
    "a0", "a1", "[a0]", "[a1]",
    "disp8[a0]", "disp8[a1]", "disp8[sb]", "disp8[fb]",
    "disp16[a0]", "disp16[a1]", "disp16[sb]", "abs16"
  };

  if (trace)
    {
      const char *the_bits = bits (destcode, 4);
      printf ("decode: %s : %s\n", the_bits, dc_names[destcode]);
    }

  switch (destcode)
    {
    case 0:
      sd.u.reg = r0;
      break;
    case 1:
      sd.u.reg = r1;
      break;
    case 2:
      sd.u.reg = r2;
      break;
    case 3:
      sd.u.reg = r3;
      break;
    case 4:
      sd.u.reg = a0;
      break;
    case 5:
      sd.u.reg = a1;
      break;
    case 6:
      addr = get_reg (a0);
      break;
    case 7:
      addr = get_reg (a1);
      break;
    case 8:
      addr = get_reg (a0) + disp8 ();
      break;
    case 9:
      addr = get_reg (a1) + disp8 ();
      break;
    case 10:
      addr = get_reg (sb) * 8 + disp8 ();
      break;
    case 11:
      addr = get_reg (fb) * 8 + sign_ext (disp8 (), 8);
      break;
    case 12:
      addr = get_reg (a0) + disp16 ();
      break;
    case 13:
      addr = get_reg (a1) + disp16 ();
      break;
    case 14:
      addr = get_reg (sb) + disp16 ();
      break;
    case 15:
      addr = disp16 ();
      break;
    }

  if (destcode < 6)
    {
      int d = disp8 ();
      sd.mem = 0;
      sd.mask = 1 << (d & 0x0f);
    }
  else
    {
      addr &= addr_mask;
      sd.mem = 1;
      sd.mask = 1 << (addr & 7);
      sd.u.addr = addr >> 3;
    }
  return sd;
}

srcdest
decode_bit11 (int op0)
{
  srcdest sd;
  sd.mask = 1 << (op0 & 7);
  sd.mem = 1;
  sd.u.addr = get_reg (sb) + disp8 ();
  return sd;
}

int
get_bit (srcdest sd)
{
  int b;
  if (sd.mem)
    b = mem_get_qi (sd.u.addr) & sd.mask;
  else
    b = get_reg (sd.u.reg) & sd.mask;
  return b ? 1 : 0;
}

void
put_bit (srcdest sd, int val)
{
  int b;
  if (sd.mem)
    b = mem_get_qi (sd.u.addr);
  else
    b = get_reg (sd.u.reg);
  if (val)
    b |= sd.mask;
  else
    b &= ~sd.mask;
  if (sd.mem)
    mem_put_qi (sd.u.addr, b);
  else
    put_reg (sd.u.reg, b);
}

int
get_bit2 (srcdest sd, int bit)
{
  int b;
  if (sd.mem)
    b = mem_get_qi (sd.u.addr + (bit >> 3)) & (1 << (bit & 7));
  else
    b = get_reg (sd.u.reg) & (1 << bit);
  return b ? 1 : 0;
}

void
put_bit2 (srcdest sd, int bit, int val)
{
  int b;
  if (sd.mem)
    b = mem_get_qi (sd.u.addr + (bit >> 3));
  else
    b = get_reg (sd.u.reg);
  if (val)
    b |= (1 << (bit & 7));
  else
    b &= ~(1 << (bit & 7));
  if (sd.mem)
    mem_put_qi (sd.u.addr + (bit >> 3), b);
  else
    put_reg (sd.u.reg, b);
}
