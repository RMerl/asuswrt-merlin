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


/* Read a table, line by line, from a file.

   A table line has several forms:

   Field line:

       <text> { ":" <text> }
       type == table_colon_entry

       Fields points to a NULL terminated list of pointers.

   Tab indented block:

     <tab> <text> <nl> { <tab> <text> <nl> }
     type == table_code_entry

     The leading tab at the start of each line is discarded.
     fields[i] is the i'th line with the <nl> discarded.
     

   Code block:

     "{" <ignore-text> <nl> { <text> <nl> } "}" <ignore-text> <nl>
     type == table_code_entry

     The leading/trailing {/} lines are discarded.
     Lines containing two leading spaces have those spaces striped.
     fields[i] is the i'th line with the <nl> discarded.

   In addition, the table parser reconises and handles internally the
   following (when not in a code block):

     "#" <line-nr> '"' <file> '"'

     As per CPP/CC, treat following lines as if they were taken from
     <file> starting at <line-nr>

   No support for CPP's "#if/#else/#endif" style conditions are
   planned. */

typedef struct _table table;

typedef enum
{
  table_colon_entry,
  table_code_entry,
}
table_entry_type;


typedef struct _table_entry table_entry;
struct _table_entry
{
  table *file;
  line_ref *line;
  table_entry_type type;
  int nr_fields;
  char **field;
};

/* List of directories to search when opening a pushed file.  Current
   directory is always searched first */
typedef struct _table_include table_include;
struct _table_include
{
  char *dir;
  table_include *next;
};


/* Open/read a table file.  Since the file is read once during open
   (and then closed immediatly) there is no close method. */

extern table *table_open (const char *file_name);

extern table_entry *table_read (table *file);


/* Push the the state of the current file and open FILE_NAME.  When
   the end of FILE_NAME is reached, return to the pushed file */

extern void table_push
  (table *file, line_ref *line, table_include *search, const char *file_name);


/* Expand the specified field_nr using the internal expansion table.
   A field is only expanded when explicitly specified.  */

extern void table_expand_field (table_entry *entry, int field_nr);


/* Given a code entry, write the code to FILE.  Since any
   leading/trailing braces were striped as part of the read, they are
   not written. */

extern void table_print_code (lf *file, table_entry *entry);


/* Debugging */

extern void dump_line_ref
  (lf *file, char *prefix, const line_ref *line, char *suffix);

extern void dump_table_entry
  (lf *file, char *prefix, const table_entry *entry, char *suffix);



/* Utilities for skipping around text */

extern char *skip_digits (char *chp);

extern char *skip_spaces (char *chp);

extern char *skip_to_separator (char *chp, char *separators);

extern char *back_spaces (char *start, char *chp);
