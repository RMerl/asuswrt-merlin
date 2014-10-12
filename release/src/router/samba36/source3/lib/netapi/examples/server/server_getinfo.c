/*
 *  Unix SMB/CIFS implementation.
 *  NetServerGetInfo query
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
	uint8_t *buffer = NULL;
	uint32_t level = 100;

	struct SERVER_INFO_100 *i100;
	struct SERVER_INFO_101 *i101;
	struct SERVER_INFO_102 *i102;
	struct SERVER_INFO_402 *i402;
	struct SERVER_INFO_403 *i403;
	struct SERVER_INFO_502 *i502;
	struct SERVER_INFO_503 *i503;
	struct SERVER_INFO_1005 *i1005;

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

	pc = poptGetContext("server_getinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname level");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	hostname = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetServerGetInfo */

	status = NetServerGetInfo(hostname,
				  level,
				  &buffer);
	if (status != 0) {
		printf("NetServerGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 100:
			i100 = (struct SERVER_INFO_100 *)buffer;
			printf("platform id: %d\n", i100->sv100_platform_id);
			printf("name: %s\n", i100->sv100_name);
			break;
		case 101:
			i101 = (struct SERVER_INFO_101 *)buffer;
			printf("platform id: %d\n", i101->sv101_platform_id);
			printf("name: %s\n", i101->sv101_name);
			printf("version major: %d\n", i101->sv101_version_major);
			printf("version minor: %d\n", i101->sv101_version_minor);
			printf("type: 0x%08x\n", i101->sv101_type);
			printf("comment: %s\n", i101->sv101_comment);
			break;
		case 102:
			i102 = (struct SERVER_INFO_102 *)buffer;
			printf("platform id: %d\n", i102->sv102_platform_id);
			printf("name: %s\n", i102->sv102_name);
			printf("version major: %d\n", i102->sv102_version_major);
			printf("version minor: %d\n", i102->sv102_version_minor);
			printf("type: 0x%08x\n", i102->sv102_type);
			printf("comment: %s\n", i102->sv102_comment);
			printf("users: %d\n", i102->sv102_users);
			printf("disc: %d\n", i102->sv102_disc);
			printf("hidden: %d\n", i102->sv102_hidden);
			printf("announce: %d\n", i102->sv102_announce);
			printf("anndelta: %d\n", i102->sv102_anndelta);
			printf("licenses: %d\n", i102->sv102_licenses);
			printf("userpath: %s\n", i102->sv102_userpath);
			break;
		case 402:
			i402 = (struct SERVER_INFO_402 *)buffer;
			break;
		case 403:
			i403 = (struct SERVER_INFO_403 *)buffer;
			break;
		case 502:
			i502 = (struct SERVER_INFO_502 *)buffer;
			break;
		case 503:
			i503 = (struct SERVER_INFO_503 *)buffer;
			break;
		case 1005:
			i1005 = (struct SERVER_INFO_1005 *)buffer;
			printf("comment: %s\n", i1005->sv1005_comment);
			break;
		default:
			break;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
