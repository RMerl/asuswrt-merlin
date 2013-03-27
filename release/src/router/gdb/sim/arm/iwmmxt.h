/*  iwmmxt.h -- Intel(r) Wireless MMX(tm) technology co-processor interface.
    Copyright (C) 2002, 2007 Free Software Foundation, Inc.
    Contributed by matthew green (mrg@redhat.com).
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

extern unsigned IwmmxtLDC (ARMul_State *, unsigned, ARMword, ARMword);
extern unsigned IwmmxtSTC (ARMul_State *, unsigned, ARMword, ARMword *);
extern unsigned IwmmxtMCR (ARMul_State *, unsigned, ARMword, ARMword);
extern unsigned IwmmxtMRC (ARMul_State *, unsigned, ARMword, ARMword *);
extern unsigned IwmmxtCDP (ARMul_State *, unsigned, ARMword);

extern int ARMul_HandleIwmmxt (ARMul_State *, ARMword);

extern int Fetch_Iwmmxt_Register (unsigned int, unsigned char *);
extern int Store_Iwmmxt_Register (unsigned int, unsigned char *);
