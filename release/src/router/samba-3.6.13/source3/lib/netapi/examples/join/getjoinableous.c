/*
 *  Unix SMB/CIFS implementation.
 *  Join Support (cmdline + netapi)
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

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>

#include <netapi.h>

#include "common.h"

int main(int argc, const char **argv)
{
	NET_API_STATUS status;
	const char *host_name = NULL;
	const char *domain_name = NULL;
	const char **ous = NULL;
	uint32_t num_ous = 0;
	struct libnetapi_ctx *ctx = NULL;
	int i;

	poptContext pc;
	int opt;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "domain", 0, POPT_ARG_STRING, NULL, 'D', "Domain name", "DOMAIN" },
		POPT_COMMON_LIBNETAPI_EXAMPLES
		POPT_TABLEEND
	};

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("getjoinableous", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname domainname");
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
			case 'D':
				domain_name = poptGetOptArg(pc);
				break;
		}
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	host_name = poptGetArg(pc);

	/* NetGetJoinableOUs */

	status = NetGetJoinableOUs(host_name,
				   domain_name,
				   ctx->username,
				   ctx->password,
				   &num_ous,
				   &ous);
	if (status != 0) {
		printf("failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	} else {
		printf("Successfully queried joinable ous:\n");
		for (i=0; i<num_ous; i++) {
			printf("ou: %s\n", ous[i]);
		}
	}

 out:
	NetApiBufferFree(ous);
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
