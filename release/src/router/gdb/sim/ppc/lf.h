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


/* LF: Line Numbered Output Stream */

typedef struct _lf lf;

typedef enum {
  lf_is_h,
  lf_is_c,
  lf_is_text,
} lf_file_type;


typedef enum {
  lf_include_references,
  lf_omit_references,
} lf_file_references;


/* Open the file NAME for writing.  REAL_NAME is to be included in any
   line number outputs.  The output of line number information can be
   suppressed with LINE_NUMBERS */

extern lf *lf_open
(char *name,
 char *real_name,
 lf_file_references file_references,
 lf_file_type type,
 const char *program);

extern void lf_close
(lf *file);


/* Basic output functions */

extern int lf_putchr
(lf *file,
 const char ch);

extern int lf_putstr
(lf *file,
 const char *string);

extern int lf_putint
(lf *file,
 int decimal);

extern int lf_putbin
(lf *file,
 int decimal,
 int width);

extern int lf_printf
(lf *file,
 const char *fmt,
 ...) __attribute__((format(printf, 2, 3)));


/* Indentation control.

   lf_indent_suppress suppresses indentation on the next line (current
   line if that has not yet been started) */

extern void lf_indent_suppress
(lf *file);

extern void lf_indent
(lf *file,
 int delta);


/* Print generic text: */


extern int lf_print__gnu_copyleft
(lf *file);

extern int lf_print__file_start
(lf *file);

extern int lf_print__this_file_is_empty
(lf *file);

extern int lf_print__file_finish
(lf *file);

extern int lf_print__internal_reference
(lf *file);

extern int lf_print__external_reference
(lf *file,
 int line_nr,
 const char *file_name);

extern int lf_print__ucase_filename
(lf *file);

/* Tab prefix is suppressed */

extern int lf_print__c_code
(lf *file,
 const char *code);


extern int lf_print_function_type
(lf *file,
 const char *type,
 const char *prefix,
 const char *trailing_space);
