/*
 *  Unix SMB/CIFS implementation.
 *  NetGroupGetInfo query
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
	struct GROUP_INFO_0 *g0;
	struct GROUP_INFO_1 *g1;
	struct GROUP_INFO_2 *g2;
	struct GROUP_INFO_3 *g3;
	char *sid_str = NULL;

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

	pc = poptGetContext("group_getinfo", argc, argv, long_options, 0);

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

	/* NetGroupGetInfo */

	status = NetGroupGetInfo(hostname,
				 groupname,
				 level,
				 &buffer);
	if (status != 0) {
		printf("NetGroupGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 0:
			g0 = (struct GROUP_INFO_0 *)buffer;
			printf("name: %s\n", g0->grpi0_name);
			break;
		case 1:
			g1 = (struct GROUP_INFO_1 *)buffer;
			printf("name: %s\n", g1->grpi1_name);
			printf("comment: %s\n", g1->grpi1_comment);
			break;
		case 2:
			g2 = (struct GROUP_INFO_2 *)buffer;
			printf("name: %s\n", g2->grpi2_name);
			printf("comment: %s\n", g2->grpi2_comment);
			printf("group_id: %d\n", g2->grpi2_group_id);
			printf("attributes: %d\n", g2->grpi2_attributes);
			break;
		case 3:
			g3 = (struct GROUP_INFO_3 *)buffer;
			printf("name: %s\n", g3->grpi3_name);
			printf("comment: %s\n", g3->grpi3_comment);
			if (ConvertSidToStringSid(g3->grpi3_group_sid,
						  &sid_str)) {
				printf("group_sid: %s\n", sid_str);
				free(sid_str);
			}
			printf("attributes: %d\n", g3->grpi3_attributes);
			break;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
