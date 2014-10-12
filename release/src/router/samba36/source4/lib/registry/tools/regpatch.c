/*
   Unix SMB/CIFS implementation.
   simple registry frontend

   Copyright (C) 2004-2007 Jelmer Vernooij, jelmer@samba.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/events/events.h"
#include "lib/registry/registry.h"
#include "lib/cmdline/popt_common.h"
#include "lib/registry/tools/common.h"
#include "param/param.h"
#include "events/events.h"

int main(int argc, char **argv)
{
  	int opt;
	poptContext pc;
	const char *patch;
	struct registry_context *h;
	const char *file = NULL;
	const char *remote = NULL;
	struct tevent_context *ev_ctx;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"remote", 'R', POPT_ARG_STRING, &remote, 0, "connect to specified remote server", NULL},
		{"file", 'F', POPT_ARG_STRING, &file, 0, "file path", NULL },
		POPT_COMMON_SAMBA
		POPT_COMMON_CREDENTIALS
		{ NULL }
	};

	pc = poptGetContext(argv[0], argc, (const char **) argv, long_options,0);

	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	ev_ctx = s4_event_context_init(NULL);

	if (remote) {
		h = reg_common_open_remote (remote, ev_ctx, cmdline_lp_ctx, cmdline_credentials);
	} else {
		h = reg_common_open_local (cmdline_credentials, ev_ctx, cmdline_lp_ctx);
	}

	if (h == NULL)
		return 1;

	patch = poptGetArg(pc);
	if (patch == NULL) {
		poptPrintUsage(pc, stderr, 0);
		return 1;
	}

	poptFreeContext(pc);

	reg_diff_apply(h, patch);

	return 0;
}
