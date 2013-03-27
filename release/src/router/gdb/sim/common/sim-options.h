/* Header file for simulator argument handling.
   Copyright (C) 1997, 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

#ifndef SIM_OPTIONS_H
#define SIM_OPTIONS_H

#include "getopt.h"

/* ARGV option support.

   Options for the standalone simulator are parsed by sim_open since
   sim_open handles the large majority of them and it also parses the
   options when invoked by gdb [or any external program].

   For OPTION_HANDLER: arg#2 is the processor to apply to option to
   (all if NULL); arg#3 is the option index; arg#4 is the option's
   argument, NULL if optional and missing; arg#5 is nonzero if a
   command is being interpreted. */

typedef SIM_RC (OPTION_HANDLER) PARAMS ((SIM_DESC, sim_cpu *, int, char *, int));

/* Declare option handlers with a macro so it's usable on k&r systems.  */
#define DECLARE_OPTION_HANDLER(fn) SIM_RC fn PARAMS ((SIM_DESC, sim_cpu *, int, char *, int))

typedef struct {

  /* The long option information. */

  struct option opt;

  /* The short option with the same meaning ('\0' if none).

     For short options, when OPT.VAL is non-zero, it, instead of
     SHORTOPT is passed to HANDLER.

     For example, for the below:

	{ {"dc", no_argument, NULL, OPTION_VALUE},
	    'd', NULL, "<<description>>", HANDLER},
	{ {NULL, no_argument, NULL, OPTION_VALUE},
	    'e', NULL, "<<description>>", HANDLER},

     the options --dc, -d and -e all result in OPTION_VALUE being
     passed into HANDLER. */

  char shortopt;

  /* The name of the argument (NULL if none).  */

  const char *arg;

  /* The documentation string.

     If DOC is NULL, this option name is listed as a synonym for the
     previous option.

     If DOC and DOC_NAME are the empty string (i.e. ""), this option
     is not listed in usage and help messages.

     For example, given the aliased options --dc, --dp and -d, then:

	{ {"dc", no_argument, NULL, OPTION_DC},
	    'd', NULL, "<<description>>", HANDLER},
	{ {"dp", no_argument, NULL, OPTION_DP},
	    '\0', NULL, NULL, HANDLER},

     will list ``-d, --dc, --dp <<description>>'' */

  const char *doc;

  /* A function to process the option.  */

  OPTION_HANDLER *handler;

  /* The documentation name.  Used when generating usage and help
     messages.

     If DOC and DOC_NAME are the empty string (i.e. ""), this option
     is not listed in usage and help messages.

     If DOC_NAME is a non-empty string then it, insted of OPT.NAME, is
     listed as the name of the option in usage and help messages.

     For example, given the options --set-pc and --set-sp, then:

	{ {"set-pc", no_argument, NULL, OPTION_SET_PC},
            '\0', NULL, "<<description>>", HANDLER, "--set-REGNAME" },
	{ {"set-sp", no_argument, NULL, OPTION_SET_SP},
	    '\0', NULL, "", HANDLER, "" },

     will list ``--set-REGNAME <<description>>". */

  const char *doc_name;

} OPTION;

/* All options that don't have a short form equivalent begin with this for
   `val'.  130 isn't special, just some non-ASCII value to begin at.
   Modules needn't worry about collisions here, the code dynamically assigned
   the actual numbers used and then passes the original value to the option
   handler.  */
#define OPTION_START 130

/* Identify valid option in the table */
#define OPTION_VALID_P(O) ((O)->opt.name != NULL || (O)->shortopt != '\0')

/* List of options added by various modules.  */
typedef struct option_list {
  struct option_list *next;
  const OPTION *options;
} OPTION_LIST;

/* Add a set of options to the simulator.
   CPU is the cpu the options apply to or NULL for all cpus.
   TABLE is an array of OPTIONS terminated by a NULL `opt.name' entry.  */
SIM_RC sim_add_option_table PARAMS ((SIM_DESC sd, sim_cpu *cpu, const OPTION *table));

/* Install handler for the standard options.  */
MODULE_INSTALL_FN standard_install;

/* Called by sim_open to parse the arguments.  */
SIM_RC sim_parse_args PARAMS ((SIM_DESC sd, char **argv));

/* Print help messages for the options.  IS_COMMAND is non-zero when
   this function is called from the command line interpreter. */
void sim_print_help PARAMS ((SIM_DESC sd, int is_command));

/* Try to parse the command as if it is an option, Only fail when
   totally unsuccessful */
SIM_RC sim_args_command PARAMS ((SIM_DESC sd, char *cmd));

#endif /* SIM_OPTIONS_H */
