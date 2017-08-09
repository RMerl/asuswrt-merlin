/*
 *  Unix SMB/CIFS implementation.
 *  NetLocalGroupGetInfo query
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
	const char *groupname = NULL;
	uint8_t *buffer = NULL;
	uint32_t level = 0;
	struct LOCALGROUP_INFO_0 *g0;
	struct LOCALGROUP_INFO_1 *g1;
	struct LOCALGROUP_INFO_1002 *g1002;

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

	pc = poptGetContext("localgroup_getinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname groupname level");
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
	groupname = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetLocalGroupGetInfo */

	status = NetLocalGroupGetInfo(hostname,
				      groupname,
				      level,
				      &buffer);
	if (status != 0) {
		printf("NetLocalGroupGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 0:
			g0 = (struct LOCALGROUP_INFO_0 *)buffer;
			printf("name: %s\n", g0->lgrpi0_name);
			break;
		case 1:
			g1 = (struct LOCALGROUP_INFO_1 *)buffer;
			printf("name: %s\n", g1->lgrpi1_name);
			printf("comment: %s\n", g1->lgrpi1_comment);
			break;
		case 1002:
			g1002 = (struct LOCALGROUP_INFO_1002 *)buffer;
			printf("comment: %s\n", g1002->lgrpi1002_comment);
			break;
	}
	NetApiBufferFree(buffer);

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
