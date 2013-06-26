/*
 *  Unix SMB/CIFS implementation.
 *  NetFileGetInfo query
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
	uint32_t fileid = 0;
	uint32_t level = 3;
	uint8_t *buffer = NULL;

	struct FILE_INFO_2 *i2 = NULL;
	struct FILE_INFO_3 *i3 = NULL;

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

	pc = poptGetContext("file_getinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname fileid");
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
	fileid = atoi(poptGetArg(pc));

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetFileGetInfo */

	status = NetFileGetInfo(hostname,
				fileid,
				level,
				&buffer);
	if (status != 0) {
		printf("NetFileGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 2:
			i2 = (struct FILE_INFO_2 *)buffer;
			printf("file_id: %d\n", i2->fi2_id);
			break;
		case 3:
			i3 = (struct FILE_INFO_3 *)buffer;
			printf("file_id: %d\n", i3->fi3_id);
			printf("permissions: %d\n", i3->fi3_permissions);
			printf("num_locks: %d\n", i3->fi3_num_locks);
			printf("pathname: %s\n", i3->fi3_pathname);
			printf("username: %s\n", i3->fi3_username);
			break;
		default:
			break;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
