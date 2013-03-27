/* C language support definitions for GDB, the GNU debugger.

   Copyright (C) 1992, 1994, 1995, 1996, 1997, 1998, 2000, 2002, 2005, 2006,
   2007 Free Software Foundation, Inc.

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


#if !defined (C_LANG_H)
#define C_LANG_H 1

struct ui_file;
struct language_arch_info;

#include "value.h"
#include "macroexp.h"


extern int c_parse (void);	/* Defined in c-exp.y */

extern void c_error (char *);	/* Defined in c-exp.y */

/* Defined in c-typeprint.c */
extern void c_print_type (struct type *, char *, struct ui_file *, int,
			  int);

extern int c_val_print (struct type *, const gdb_byte *, int, CORE_ADDR,
			struct ui_file *, int, int, int,
			enum val_prettyprint);

extern int c_value_print (struct value *, struct ui_file *, int,
			  enum val_prettyprint);

/* These are in c-lang.c: */

extern void c_printchar (int, struct ui_file *);

extern void c_printstr (struct ui_file * stream, const gdb_byte *string,
			unsigned int length, int width,
			int force_ellipses);

extern void scan_macro_expansion (char *expansion);
extern int scanning_macro_expansion (void);
extern void finished_macro_expansion (void);

extern macro_lookup_ftype *expression_macro_lookup_func;
extern void *expression_macro_lookup_baton;

extern struct type *c_create_fundamental_type (struct objfile *, int);

extern void c_language_arch_info (struct gdbarch *gdbarch,
				  struct language_arch_info *lai);

/* These are in c-typeprint.c: */

extern void c_type_print_base (struct type *, struct ui_file *, int, int);

/* These are in cp-valprint.c */

extern int vtblprint;		/* Controls printing of vtbl's */

extern int static_field_print;

extern void cp_print_class_member (const gdb_byte *, struct type *,
				   struct ui_file *, char *);

extern void cp_print_value_fields (struct type *, struct type *,
				   const gdb_byte *, int, CORE_ADDR,
				   struct ui_file *, int,
				   int, enum val_prettyprint,
				   struct type **, int);

extern int cp_is_vtbl_ptr_type (struct type *);

extern int cp_is_vtbl_member (struct type *);


#endif /* !defined (C_LANG_H) */
