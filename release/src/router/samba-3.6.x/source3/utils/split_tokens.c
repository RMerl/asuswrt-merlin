/*
   Unix SMB/CIFS implementation.
   test program for the next_token() function

   Copyright (C) 2009 Michael Adam

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Diagnostic output for "next_token()".
 */

#include "includes.h"
#include "popt_common.h"

int main(int argc, const char *argv[])
{
	const char *config_file = get_dyn_CONFIGFILE();
	const char *sequence = "";
	poptContext pc;
	char *buff;
	TALLOC_CTX *ctx = talloc_stackframe();

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};

	load_case_tables();

	pc = poptGetContext(NULL, argc, argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);
	poptSetOtherOptionHelp(pc, "[OPTION...] <sequence-string>");

	while(poptGetNextOpt(pc) != -1);

	setup_logging(poptGetArg(pc), DEBUG_STDERR);

	sequence = poptGetArg(pc);

	if (sequence == NULL) {
		fprintf(stderr, "ERROR: missing sequence string\n");
		return 1;
	}
	
	lp_set_cmdline("log level", "0");

	if (!lp_load(config_file,false,true,false,true)) {
		fprintf(stderr,"Error loading services.\n");
		return 1;
	}

	while(next_token_talloc(ctx, &sequence, &buff, NULL)) {
		printf("[%s]\n", buff);
	}

	talloc_free(ctx);

	return 0;
}

