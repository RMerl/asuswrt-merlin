/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _REGISTERS_C_
#define _REGISTERS_C_

#include <ctype.h>

#include "basics.h"
#include "registers.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


INLINE_REGISTERS\
(void)
registers_dump(registers *registers)
{
  int i;
  int j;
  for (i = 0; i < 8; i++) {
    printf_filtered("GPR %2d:", i*4);
    for (j = 0; j < 4; j++) {
      printf_filtered(" 0x%08lx", (long)registers->gpr[i*4 + j]);
    }
    printf_filtered("\n");
  }
}

STATIC_INLINE_REGISTERS\
(sprs)
find_spr(const char name[])
{
  sprs spr;
  for (spr = 0; spr < nr_of_sprs; spr++)
    if (spr_is_valid(spr)
	&& !strcmp(name, spr_name(spr))
	&& spr_index(spr) == spr)
      return spr;
  return nr_of_sprs;
}

STATIC_INLINE_REGISTERS\
(int)
are_digits(const char *digits)
{
  while (isdigit(*digits))
    digits++;
  return *digits == '\0';
}


INLINE_REGISTERS\
(register_descriptions)
register_description(const char reg[])
{
  register_descriptions description;

  /* try for a general-purpose integer or floating point register */
  if (reg[0] == 'r' && are_digits(reg + 1)) {
    description.type = reg_gpr;
    description.index = atoi(reg+1);
    description.size = sizeof(gpreg);
  }
  else if (reg[0] == 'f' && are_digits(reg + 1)) {
    description.type = reg_fpr;
    description.index = atoi(reg+1);
    description.size = sizeof(fpreg);
  }
  else if (!strcmp(reg, "pc") || !strcmp(reg, "nia")) {
    description.type = reg_pc;
    description.index = 0;
    description.size = sizeof(unsigned_word);
  }
  else if (!strcmp(reg, "sp")) {
    description.type = reg_gpr;
    description.index = 1;
    description.size = sizeof(gpreg);
  }
  else if (!strcmp(reg, "toc")) {
    description.type = reg_gpr;
    description.index = 2;
    description.size = sizeof(gpreg);
  }
  else if (!strcmp(reg, "cr") || !strcmp(reg, "cnd")) {
    description.type = reg_cr;
    description.index = 0;
    description.size = sizeof(creg); /* FIXME */
  }
  else if (!strcmp(reg, "msr") || !strcmp(reg, "ps")) {
    description.type = reg_msr;
    description.index = 0;
    description.size = sizeof(msreg);
  }
  else if (!strcmp(reg, "fpscr")) {
    description.type = reg_fpscr;
    description.index = 0;
    description.size = sizeof(fpscreg);
  }
  else if (!strncmp(reg, "sr", 2) && are_digits(reg + 2)) {
    description.type = reg_sr;
    description.index = atoi(reg+2);
    description.size = sizeof(sreg);
  }
  else if (!strcmp(reg, "cnt")) {
    description.type = reg_spr;
    description.index = spr_ctr;
    description.size = sizeof(spreg);
  }
  else if (!strcmp(reg, "insns")) {
    description.type = reg_insns;
    description.index = spr_ctr;
    description.size = sizeof(unsigned_word);
  }
  else if (!strcmp(reg, "stalls")) {
    description.type = reg_stalls;
    description.index = spr_ctr;
    description.size = sizeof(unsigned_word);
  }
  else if (!strcmp(reg, "cycles")) {
    description.type = reg_cycles;
    description.index = spr_ctr;
    description.size = sizeof(unsigned_word);
  }
#ifdef WITH_ALTIVEC
  else if (reg[0] == 'v' && reg[1] == 'r' && are_digits(reg + 2)) {
    description.type = reg_vr;
    description.index = atoi(reg+2);
    description.size = sizeof(vreg);
  }
   else if (!strcmp(reg, "vscr")) {
    description.type = reg_vscr;
    description.index = 0;
    description.size = sizeof(vscreg);
  }
#endif
#ifdef WITH_E500
  else if (reg[0] == 'e' && reg[1] == 'v' && are_digits(reg + 2)) {
    description.type = reg_evr;
    description.index = atoi(reg+2);
    description.size = sizeof(unsigned64);
  }
  else if (reg[0] == 'r' && reg[1] == 'h' && are_digits(reg + 2)) {
    description.type = reg_gprh;
    description.index = atoi(reg+2);
    description.size = sizeof(gpreg);
  }
  else if (!strcmp(reg, "acc")) {
    description.type = reg_acc;
    description.index = 0;
    description.size = sizeof(unsigned64);
  }
#endif
  else {
    sprs spr = find_spr(reg);
    if (spr != nr_of_sprs) {
      description.type = reg_spr;
      description.index = spr;
      description.size = sizeof(spreg);
    }
    else {
      description.type = reg_invalid;
      description.index = 0;
      description.size = 0;
    }
  }
  return description;
}

#endif /* _REGISTERS_C_ */
