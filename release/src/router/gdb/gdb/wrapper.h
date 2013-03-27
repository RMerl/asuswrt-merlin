/* Longjump free calls to GDB internal routines.

   Copyright (C) 1999, 2000, 2005, 2007 Free Software Foundation, Inc.

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

#ifndef WRAPPER_H
#define WRAPPER_H 1

#include "gdb.h"

struct value;
struct expression;
struct block;

extern int gdb_parse_exp_1 (char **, struct block *,
			    int, struct expression **);

extern int gdb_evaluate_expression (struct expression *, struct value **);

extern int gdb_value_fetch_lazy (struct value *);

extern int gdb_value_equal (struct value *, struct value *, int *);

extern int gdb_value_assign (struct value *, struct value *, struct value **);

extern int gdb_value_subscript (struct value *, struct value *,
				struct value **);

extern enum gdb_rc gdb_value_struct_elt (struct ui_out *uiout,
					 struct value **result_ptr,
					 struct value **argp,
					 struct value **args, char *name,
					 int *static_memfuncp, char *err);

extern int gdb_value_ind (struct value *val, struct value ** rval);

extern int gdb_parse_and_eval_type (char *, int, struct type **);

#endif /* wrapper.h */
