/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

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



/* LF: Line Numbered Output Stream */

typedef struct _lf lf;

typedef enum
{
  lf_is_h,
  lf_is_c,
  lf_is_text,
}
lf_file_type;


typedef enum
{
  lf_include_references,
  lf_omit_references,
}
lf_file_references;


/* Open the file NAME for writing ("-" for stdout).  Use REAL_NAME
   when refering to the opened file.  Line number information (in the
   output) can be suppressed with FILE_REFERENCES ==
   LF_OMIT_REFERENCES.  TYPE is to determine the formatting of some of
   the print messages below. */

extern lf *lf_open
  (char *name,
   char *real_name,
   lf_file_references file_references,
   lf_file_type type, const char *program);

extern void lf_close (lf *file);


/* Basic output functions */

extern int lf_write (lf *file, const char *string, int len);

extern int lf_putchr (lf *file, const char ch);

extern int lf_putstr (lf *file, const char *string);

extern int lf_putint (lf *file, int decimal);

extern int lf_putbin (lf *file, int decimal, int width);

extern int lf_printf
  (lf *file, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));


/* Indentation control.

   lf_indent_suppress suppresses indentation on the next line (current
   line if that has not yet been started) */

extern void lf_indent_suppress (lf *file);

extern void lf_indent (lf *file, int delta);


/* Print generic text: */


extern int lf_print__gnu_copyleft (lf *file);

extern int lf_print__file_start (lf *file);

extern int lf_print__this_file_is_empty (lf *file, const char *reason);

extern int lf_print__file_finish (lf *file);

extern int lf_print__internal_ref (lf *file);

extern int lf_print__external_ref
  (lf *file, int line_nr, const char *file_name);

extern int lf_print__line_ref (lf *file, line_ref *line);

extern int lf_print__ucase_filename (lf *file);

extern int lf_print__function_type
  (lf *file,
   const char *type, const char *prefix, const char *trailing_space);

typedef int print_function (lf *file);

extern int lf_print__function_type_function
  (lf *file,
   print_function * print_type,
   const char *prefix, const char *trailing_space);
