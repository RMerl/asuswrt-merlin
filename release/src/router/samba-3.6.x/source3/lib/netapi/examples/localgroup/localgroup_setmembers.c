/*
 *  Unix SMB/CIFS implementation.
 *  NetLocalGroupSetMembers query
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
	struct LOCALGROUP_MEMBERS_INFO_0 *g0;
	struct LOCALGROUP_MEMBERS_INFO_3 *g3;
	uint32_t total_entries = 0;
	uint8_t *buffer = NULL;
	uint32_t level = 3;
	const char **names = NULL;
	int i = 0;
	size_t buf_size = 0;

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

	pc = poptGetContext("localgroup_setmembers", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname groupname member1 member2 ...");
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
		total_entries++;
	}

	switch (level) {
		case 0:
			buf_size = sizeof(struct LOCALGROUP_MEMBERS_INFO_0) * total_entries;

			status = NetApiBufferAllocate(buf_size, (void **)&g0);
			if (status) {
				printf("NetApiBufferAllocate failed with: %s\n",
					libnetapi_get_error_string(ctx, status));
				goto out;
			}

			for (i=0; i<total_entries; i++) {
				if (!ConvertStringSidToSid(names[i], &g0[i].lgrmi0_sid)) {
					printf("could not convert sid\n");
					goto out;
				}
			}

			buffer = (uint8_t *)g0;
			break;
		case 3:
			buf_size = sizeof(struct LOCALGROUP_MEMBERS_INFO_3) * total_entries;

			status = NetApiBufferAllocate(buf_size, (void **)&g3);
			if (status) {
				printf("NetApiBufferAllocate failed with: %s\n",
					libnetapi_get_error_string(ctx, status));
				goto out;
			}

			for (i=0; i<total_entries; i++) {
				g3[i].lgrmi3_domainandname = names[i];
			}

			buffer = (uint8_t *)g3;
			break;
		default:
			break;
	}

	/* NetLocalGroupSetMembers */

	status = NetLocalGroupSetMembers(hostname,
					 groupname,
					 level,
					 buffer,
					 total_entries);
	if (status != 0) {
		printf("NetLocalGroupSetMembers failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	NetApiBufferFree(buffer);

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
