/* Altivec expression macros, for PSIM, the PowerPC simulator.

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

/* AltiVec macro helpers.  */

#define ALTIVEC_SET_CR6(vS, checkone) \
do { \
  if (checkone && ((*vS).w[0] == 0xffffffff && \
		   (*vS).w[1] == 0xffffffff && \
		   (*vS).w[2] == 0xffffffff && \
		   (*vS).w[3] == 0xffffffff)) \
    CR_SET(6, 1 << 3); \
  else if ((*vS).w[0] == 0 && \
           (*vS).w[1] == 0 && \
           (*vS).w[2] == 0 && \
           (*vS).w[3] == 0) \
    CR_SET(6, 1 << 1); \
  else \
    CR_SET(6, 0); \
} while (0)

#define	VSCR_SAT	0x00000001
#define	VSCR_NJ		0x00010000

#define ALTIVEC_SET_SAT(sat) \
do { \
  if (sat) \
    VSCR |= VSCR_SAT; \
} while (0)
