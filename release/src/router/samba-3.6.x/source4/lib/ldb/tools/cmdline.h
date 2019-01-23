/* 
   ldb database library - command line handling for ldb tools

   Copyright (C) Andrew Tridgell  2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <popt.h>

struct ldb_cmdline {
	const char *url;
	enum ldb_scope scope;
	const char *basedn;
	const char *modules_path;
	int interactive;
	int sorted;
	const char *editor;
	int verbose;
	int recursive;
	int all_records;
	int nosync;
	const char **options;
	int argc;
	const char **argv;
	int num_records;
	int num_searches;
	const char *sasl_mechanism;
	const char **controls;
	int show_binary;
	int tracing;
};

struct ldb_cmdline *ldb_cmdline_process(struct ldb_context *ldb, int argc,
					const char **argv,
					void (*usage)(struct ldb_context *));


int handle_controls_reply(struct ldb_control **reply, struct ldb_control **request);
void ldb_cmdline_help(struct ldb_context *ldb, const char *cmdname, FILE *f);

