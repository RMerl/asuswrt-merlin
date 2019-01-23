/*
 *  Unix SMB/CIFS implementation.
 *  NetGroupSetUsers query
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
	uint32_t level = 0;
	uint8_t *buffer = NULL;
	uint32_t num_entries = 0;
	const char **names = NULL;
	int i = 0;
	size_t buf_size = 0;

	struct GROUP_USERS_INFO_0 *g0 = NULL;
	struct GROUP_USERS_INFO_1 *g1 = NULL;

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

	pc = poptGetContext("group_setusers", argc, argv, long_options, 0);

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

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}

	names = poptGetArgs(pc);
	for (i=0; names[i] != NULL; i++) {
		num_entries++;
	}

	switch (level) {
		case 0:
			buf_size = sizeof(struct GROUP_USERS_INFO_0) * num_entries;

			status = NetApiBufferAllocate(buf_size, (void **)&g0);
			if (status) {
				printf("NetApiBufferAllocate failed with: %s\n",
					libnetapi_get_error_string(ctx, status));
				goto out;
			}

			for (i=0; i<num_entries; i++) {
				g0[i].grui0_name = names[i];
			}

			buffer = (uint8_t *)g0;
			break;
		case 1:
			buf_size = sizeof(struct GROUP_USERS_INFO_1) * num_entries;

			status = NetApiBufferAllocate(buf_size, (void **)&g1);
			if (status) {
				printf("NetApiBufferAllocate failed with: %s\n",
					libnetapi_get_error_string(ctx, status));
				goto out;
			}

			for (i=0; i<num_entries; i++) {
				g1[i].grui1_name = names[i];
			}

			buffer = (uint8_t *)g1;
			break;
		default:
			break;
	}

	/* NetGroupSetUsers */

	status = NetGroupSetUsers(hostname,
				  groupname,
				  level,
				  buffer,
				  num_entries);
	if (status != 0) {
		printf("NetGroupSetUsers failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
