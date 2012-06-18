/*
 *  Unix SMB/CIFS implementation.
 *  GetDCName query
 *  Copyright (C) Guenther Deschner 2007
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
	const char *domain = NULL;
	uint8_t *buffer = NULL;

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

	pc = poptGetContext("getdc", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname domainname");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	hostname = poptGetArg(pc);

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	domain = poptGetArg(pc);

	/* NetGetDCName */

	status = NetGetDCName(hostname, domain, &buffer);
	if (status != 0) {
		printf("GetDcName failed with: %s\n", libnetapi_errstr(status));
	} else {
		printf("%s\n", (char *)buffer);
	}

 out:
	NetApiBufferFree(buffer);
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
