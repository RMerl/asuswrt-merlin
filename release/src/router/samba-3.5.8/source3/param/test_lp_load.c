/*
 * Unix SMB/CIFS implementation.
 * Test for lp_load()
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

extern bool AllowDebugChange;

int main(int argc, const char **argv)
{
	const char *config_file = get_dyn_CONFIGFILE();
	int ret = 0;
	poptContext pc;
	char *count_str = NULL;
	int i, count = 1;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"count", 'c', POPT_ARG_STRING, &count_str, 1,
		 "Load config <count> number of times"},
		POPT_COMMON_DEBUGLEVEL
		POPT_TABLEEND
	};

	TALLOC_CTX *frame = talloc_stackframe();

	load_case_tables();
	DEBUGLEVEL_CLASS[DBGC_ALL] = 0;

	pc = poptGetContext(NULL, argc, argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);
	poptSetOtherOptionHelp(pc, "[OPTION...] <config-file>");

	while(poptGetNextOpt(pc) != -1);

	setup_logging(poptGetArg(pc), True);

	if (poptPeekArg(pc)) {
		config_file = poptGetArg(pc);
	}

	poptFreeContext(pc);

	if (count_str != NULL) {
		count = atoi(count_str);
	}

	dbf = x_stderr;
	/* Don't let the debuglevel be changed by smb.conf. */
	AllowDebugChange = False;

	for (i=0; i < count; i++) {
		printf("call lp_load() #%d: ", i+1);
		if (!lp_load_with_registry_shares(config_file,
						  False, /* global only */
						  True,  /* save defaults */
						  False, /*add_ipc */
						  True)) /*init globals */
		{
			printf("ERROR.\n");
			ret = 1;
			goto done;
		}
		printf("ok.\n");
	}


done:
	gfree_loadparm();
	TALLOC_FREE(frame);
	return ret;
}

