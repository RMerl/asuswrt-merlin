/* Register test program.

   Copyright 2007 Free Software Foundation, Inc.

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

void
read_regs (unsigned long long regs[16], unsigned long control_regs[6])
{
  asm volatile ("wstrd wR0, %0" : "=m" (regs[0]));
  asm volatile ("wstrd wR1, %0" : "=m" (regs[1]));
  asm volatile ("wstrd wR2, %0" : "=m" (regs[2]));
  asm volatile ("wstrd wR3, %0" : "=m" (regs[3]));
  asm volatile ("wstrd wR4, %0" : "=m" (regs[4]));
  asm volatile ("wstrd wR5, %0" : "=m" (regs[5]));
  asm volatile ("wstrd wR6, %0" : "=m" (regs[6]));
  asm volatile ("wstrd wR7, %0" : "=m" (regs[7]));
  asm volatile ("wstrd wR8, %0" : "=m" (regs[8]));
  asm volatile ("wstrd wR9, %0" : "=m" (regs[9]));
  asm volatile ("wstrd wR10, %0" : "=m" (regs[10]));
  asm volatile ("wstrd wR11, %0" : "=m" (regs[11]));
  asm volatile ("wstrd wR12, %0" : "=m" (regs[12]));
  asm volatile ("wstrd wR13, %0" : "=m" (regs[13]));
  asm volatile ("wstrd wR14, %0" : "=m" (regs[14]));
  asm volatile ("wstrd wR15, %0" : "=m" (regs[15]));

  asm volatile ("wstrw wCSSF, %0" : "=m" (control_regs[0]));
  asm volatile ("wstrw wCASF, %0" : "=m" (control_regs[1]));
  asm volatile ("wstrw wCGR0, %0" : "=m" (control_regs[2]));
  asm volatile ("wstrw wCGR1, %0" : "=m" (control_regs[3]));
  asm volatile ("wstrw wCGR2, %0" : "=m" (control_regs[4]));
  asm volatile ("wstrw wCGR3, %0" : "=m" (control_regs[5]));
}

void
write_regs (unsigned long long regs[16], unsigned long control_regs[6])
{
  asm volatile ("wldrd wR0, %0" : : "m" (regs[0]));
  asm volatile ("wldrd wR1, %0" : : "m" (regs[1]));
  asm volatile ("wldrd wR2, %0" : : "m" (regs[2]));
  asm volatile ("wldrd wR3, %0" : : "m" (regs[3]));
  asm volatile ("wldrd wR4, %0" : : "m" (regs[4]));
  asm volatile ("wldrd wR5, %0" : : "m" (regs[5]));
  asm volatile ("wldrd wR6, %0" : : "m" (regs[6]));
  asm volatile ("wldrd wR7, %0" : : "m" (regs[7]));
  asm volatile ("wldrd wR8, %0" : : "m" (regs[8]));
  asm volatile ("wldrd wR9, %0" : : "m" (regs[9]));
  asm volatile ("wldrd wR10, %0" : : "m" (regs[10]));
  asm volatile ("wldrd wR11, %0" : : "m" (regs[11]));
  asm volatile ("wldrd wR12, %0" : : "m" (regs[12]));
  asm volatile ("wldrd wR13, %0" : : "m" (regs[13]));
  asm volatile ("wldrd wR14, %0" : : "m" (regs[14]));
  asm volatile ("wldrd wR15, %0" : : "m" (regs[15]));

  asm volatile ("wldrw wCSSF, %0" : : "m" (control_regs[0]));
  asm volatile ("wldrw wCASF, %0" : : "m" (control_regs[1]));
  asm volatile ("wldrw wCGR0, %0" : : "m" (control_regs[2]));
  asm volatile ("wldrw wCGR1, %0" : : "m" (control_regs[3]));
  asm volatile ("wldrw wCGR2, %0" : : "m" (control_regs[4]));
  asm volatile ("wldrw wCGR3, %0" : : "m" (control_regs[5]));
}

int
main ()
{
  unsigned long long regs[16];
  unsigned long control_regs[6];

  read_regs (regs, control_regs);
  write_regs (regs, control_regs);

  return 0;
}
