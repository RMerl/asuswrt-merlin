/* Native-dependent code for SPARC.

   Copyright (C) 2003, 2004, 2007 Free Software Foundation, Inc.

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

#ifndef SPARC_NAT_H
#define SPARC_NAT_H 1

struct sparc_gregset;

extern const struct sparc_gregset *sparc_gregset;
extern void (*sparc_supply_gregset) (const struct sparc_gregset *,
				     struct regcache *, int , const void *);
extern void (*sparc_collect_gregset) (const struct sparc_gregset *,
				      const struct regcache *, int, void *);
extern void (*sparc_supply_fpregset) (struct regcache *, int , const void *);
extern void (*sparc_collect_fpregset) (const struct regcache *, int , void *);
extern int (*sparc_gregset_supplies_p) (int);
extern int (*sparc_fpregset_supplies_p) (int);

extern int sparc32_gregset_supplies_p (int regnum);
extern int sparc32_fpregset_supplies_p (int regnum);

/* Create a prototype generic SPARC target.  The client can override
   it with local methods.  */

extern struct target_ops *sparc_target (void);

extern void sparc_fetch_inferior_registers (struct regcache *, int);
extern void sparc_store_inferior_registers (struct regcache *, int);

#endif /* sparc-nat.h */
