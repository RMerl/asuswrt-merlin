/*
 *  Unix SMB/CIFS implementation.
 *  Join Support (cmdline + netapi)
 *  Copyright (C) Guenther Deschner 2009
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
	const char *name_buffer = NULL;
	uint16_t name_type = 0;
	struct libnetapi_ctx *ctx = NULL;

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

	pc = poptGetContext("getjoininformation", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	host_name = poptGetArg(pc);

	/* NetGetJoinInformation */

	status = NetGetJoinInformation(host_name,
				       &name_buffer,
				       &name_type);
	if (status != 0) {
		printf("failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	} else {
		printf("Successfully queried join information:\n");

		switch (name_type) {
		case NetSetupUnknownStatus:
			printf("%s's join status unknown (name: %s)\n",
				host_name, name_buffer);
			break;
		case NetSetupUnjoined:
			printf("%s is not joined (name: %s)\n",
				host_name, name_buffer);
			break;
		case NetSetupWorkgroupName:
			printf("%s is joined to workgroup %s\n",
				host_name, name_buffer);
			break;
		case NetSetupDomainName:
			printf("%s is joined to domain %s\n",
				host_name, name_buffer);
			break;
		default:
			printf("%s is in unknown status %d (name: %s)\n",
				host_name, name_type, name_buffer);
			break;
		}
	}

 out:
	NetApiBufferFree((void *)name_buffer);
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
