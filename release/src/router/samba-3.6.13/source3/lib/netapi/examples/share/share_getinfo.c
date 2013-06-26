/*
 *  Unix SMB/CIFS implementation.
 *  NetShareGetInfo query
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
	const char *sharename = NULL;
	uint32_t level = 2;
	uint8_t *buffer = NULL;

	struct SHARE_INFO_0 *i0 = NULL;
	struct SHARE_INFO_1 *i1 = NULL;
	struct SHARE_INFO_2 *i2 = NULL;
	struct SHARE_INFO_501 *i501 = NULL;
	struct SHARE_INFO_1005 *i1005 = NULL;

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

	pc = poptGetContext("share_getinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname sharename level");
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
	sharename = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetShareGetInfo */

	status = NetShareGetInfo(hostname,
				 sharename,
				 level,
				 &buffer);
	if (status != 0) {
		printf("NetShareGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 0:
			i0 = (struct SHARE_INFO_0 *)buffer;
			break;
		case 1:
			i1 = (struct SHARE_INFO_1 *)buffer;
			break;
		case 2:
			i2 = (struct SHARE_INFO_2 *)buffer;
			break;
		case 501:
			i501 = (struct SHARE_INFO_501 *)buffer;
			break;
		case 1005:
			i1005 = (struct SHARE_INFO_1005 *)buffer;
			break;

		default:
			break;
	}

	switch (level) {
		case 0:
			printf("netname: %s\n", i0->shi0_netname);
			break;
		case 1:
			printf("netname: %s\n", i1->shi1_netname);
			printf("type: %d\n", i1->shi1_type);
			printf("remark: %s\n", i1->shi1_remark);
			break;
		case 2:
			printf("netname: %s\n", i2->shi2_netname);
			printf("type: %d\n", i2->shi2_type);
			printf("remark: %s\n", i2->shi2_remark);
			printf("permissions: %d\n", i2->shi2_permissions);
			printf("max users: %d\n", i2->shi2_max_uses);
			printf("current users: %d\n", i2->shi2_current_uses);
			printf("path: %s\n", i2->shi2_path);
			printf("password: %s\n", i2->shi2_passwd);
			break;
		case 501:
			printf("netname: %s\n", i501->shi501_netname);
			printf("type: %d\n", i501->shi501_type);
			printf("remark: %s\n", i501->shi501_remark);
			printf("flags: %d\n", i501->shi501_flags);
			break;
		case 1005:
			printf("flags: %d\n", i1005->shi1005_flags);
			break;
		default:
			break;
	}
	NetApiBufferFree(buffer);

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
