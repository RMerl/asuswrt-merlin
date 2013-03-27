/* Altivec registers, for PSIM, the PowerPC simulator.

   Copyright 2003, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat Inc; developed under contract from Motorola.
   Written by matthew green <mrg@redhat.com>.

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

/* Manage this as 4 32-bit entities, 8 16-bit entities or 16 8-bit
   entities.  */
typedef union
{
  unsigned8 b[16];
  unsigned16 h[8];
  unsigned32 w[4];
} vreg;

typedef unsigned32 vscreg;

struct altivec_regs {
  /* AltiVec Registers */
  vreg vr[32];
  vscreg vscr;
};

/* AltiVec registers */
#define VR(N)		cpu_registers(processor)->altivec.vr[N]

/* AltiVec vector status and control register */
#define VSCR		cpu_registers(processor)->altivec.vscr

/* AltiVec endian helpers, wrong endian hosts vs targets need to be
   sure to get the right bytes/halfs/words when the order matters.
   Note that many AltiVec instructions do not depend on byte order and
   work on N independant bits of data.  This is only for the
   instructions that actually move data around.  */

#if (WITH_HOST_BYTE_ORDER == BIG_ENDIAN)
#define AV_BINDEX(x)	((x) & 15)
#define AV_HINDEX(x)	((x) & 7)
#else
static char endian_b2l_bindex[16] = { 3, 2, 1, 0, 7, 6, 5, 4,
			     11, 10, 9, 8, 15, 14, 13, 12 };
static char endian_b2l_hindex[16] = { 1, 0, 3, 2, 5, 4, 7, 6 };
#define AV_BINDEX(x)	endian_b2l_bindex[(x) & 15]
#define AV_HINDEX(x)	endian_b2l_hindex[(x) & 7]
#endif
