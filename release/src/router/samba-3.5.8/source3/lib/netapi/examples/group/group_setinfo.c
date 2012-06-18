/*
 *  Unix SMB/CIFS implementation.
 *  NetGroupSetInfo query
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
	const char *option = NULL;
	uint8_t *buffer = NULL;
	uint32_t level = 0;
	uint32_t parm_err = 0;
	struct GROUP_INFO_0 g0;
	struct GROUP_INFO_1 g1;
	struct GROUP_INFO_2 g2;
	struct GROUP_INFO_3 g3;
	struct GROUP_INFO_1002 g1002;
	struct GROUP_INFO_1005 g1005;

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

	pc = poptGetContext("group_setinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname groupname level option");
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
	level = atoi(poptGetArg(pc));

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	option = poptGetArg(pc);

	/* NetGroupSetInfo */

	switch (level) {
		case 0:
			g0.grpi0_name = option;
			buffer = (uint8_t *)&g0;
			break;
		case 1:
			g1.grpi1_name = option; /* this one will be ignored */
			g1.grpi1_comment = option;
			buffer = (uint8_t *)&g1;
			break;
		case 2:
			g2.grpi2_name = option; /* this one will be ignored */
			g2.grpi2_comment = option;
			g2.grpi2_group_id = 4711; /* this one will be ignored */
			g2.grpi2_attributes = 7;
			buffer = (uint8_t *)&g2;
			break;
		case 3:
			g3.grpi3_name = option; /* this one will be ignored */
			g3.grpi3_comment = option;
			g2.grpi2_attributes = 7;
			buffer = (uint8_t *)&g3;
			break;
		case 1002:
			g1002.grpi1002_comment = option;
			buffer = (uint8_t *)&g1002;
			break;
		case 1005:
			g1005.grpi1005_attributes = atoi(option);
			buffer = (uint8_t *)&g1005;
			break;
	}

	status = NetGroupSetInfo(hostname,
				 groupname,
				 level,
				 buffer,
				 &parm_err);
	if (status != 0) {
		printf("NetGroupSetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
