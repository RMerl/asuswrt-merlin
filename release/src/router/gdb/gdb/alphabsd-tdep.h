/* Common target dependent code for Alpha BSD's.

   Copyright (C) 2002, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef ALPHABSD_TDEP_H
#define ALPHABSD_TDEP_H

struct regcache;

void alphabsd_supply_reg (struct regcache *, const char *, int);
void alphabsd_fill_reg (const struct regcache *, char *, int);

void alphabsd_supply_fpreg (struct regcache *, const char *, int);
void alphabsd_fill_fpreg (const struct regcache *, char *, int);


/* Functions exported from alphanbsd-tdep.c.  */

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */
extern const struct regset *
  alphanbsd_regset_from_core_section (struct gdbarch *gdbarch,
				      const char *sect_name, size_t len);

#endif /* alphabsd-tdep.h */
