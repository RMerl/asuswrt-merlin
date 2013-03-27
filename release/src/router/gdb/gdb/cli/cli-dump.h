/* Dump-to-file commands, for GDB, the GNU debugger.

   Copyright (c) 2001, 2005, 2007 Free Software Foundation, Inc.

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

#ifndef CLI_DUMP_H
#define CLI_DUMP_H

extern void add_dump_command (char *name,
			      void (*func) (char *args, char *mode),
			      char *descr);

/* Utilities for doing the dump.  */
extern char *scan_filename_with_cleanup (char **cmd, const char *defname);

extern char *scan_expression_with_cleanup (char **cmd, const char *defname);

extern FILE *fopen_with_cleanup (const char *filename, const char *mode);

extern char *skip_spaces (char *inp);

extern struct value *parse_and_eval_with_error (char *exp, const char *fmt, ...) ATTR_FORMAT (printf, 2, 3);

#endif
