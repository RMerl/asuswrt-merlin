/*
 *  Unix SMB/CIFS implementation.
 *  NetQueryDisplayInformation query
 *  Copyright (C) Guenther Deschner 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netapi.h>

#include "common.h"

int main(int argc, const char **argv)
{
	NET_API_STATUS status;
	struct libnetapi_ctx *ctx = NULL;
	const char *hostname = NULL;
	void *buffer = NULL;
	uint32_t entries_read = 0;
	uint32_t idx = 0;
	int i;

	struct NET_DISPLAY_USER *user;

	poptContext pc;
	int opt;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_LIBNETAPI_EXAMPLES
		POPT_TABLEEND
	};

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("user_dispinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	hostname = poptGetArg(pc);

	/* NetQueryDisplayInformation */

	do {
		status = NetQueryDisplayInformation(hostname,
						    1,
						    idx,
						    1000,
						    (uint32_t)-1,
						    &entries_read,
						    &buffer);
		if (status == 0 || status == ERROR_MORE_DATA) {
			user = (struct NET_DISPLAY_USER *)buffer;
			for (i=0; i<entries_read; i++) {
				printf("user %d: %s\n", i + idx,
				       user->usri1_name);
				user++;
			}
			NetApiBufferFree(buffer);
		}
		idx += entries_read;
	} while (status == ERROR_MORE_DATA);

	if (status != 0) {
		printf("NetQueryDisplayInformation failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
