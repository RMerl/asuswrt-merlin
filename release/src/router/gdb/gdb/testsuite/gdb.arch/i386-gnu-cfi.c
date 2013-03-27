/* Unwinding of DW_CFA_GNU_negative_offset_extended test program.

   Copyright 2007, Free Software Foundation, Inc.

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

#include <stdlib.h>

/* i386-gnu-cfi-asm.S:  */
extern void *gate (void *(*gate) (void *data), void *data);

int main (void)
{
  gate ((void *(*) (void *data)) abort, NULL);
  return 0;
}
