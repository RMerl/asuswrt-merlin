/*
 *  Unix SMB/CIFS implementation.
 *  NetLocalGroupSetInfo query
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
	struct LOCALGROUP_INFO_0 g0;
	struct LOCALGROUP_INFO_1 g1;
	struct LOCALGROUP_INFO_1002 g1002;
	const char *newname = NULL;
	const char *newcomment = NULL;
	uint32_t parm_err = 0;

	poptContext pc;
	int opt;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_LIBNETAPI_EXAMPLES
		{ "newname", 0, POPT_ARG_STRING, NULL, 'n', "New Local Group Name", "NEWNAME" },
		{ "newcomment", 0, POPT_ARG_STRING, NULL, 'c', "New Local Group Comment", "NETCOMMENT" },
		POPT_TABLEEND
	};

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("localgroup_setinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname groupname level");
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
			case 'n':
				newname = poptGetOptArg(pc);
				break;
			case 'c':
				newcomment = poptGetOptArg(pc);
				break;
		}

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

	if (newname && !newcomment) {
		g0.lgrpi0_name = newname;
		buffer = (uint8_t *)&g0;
		level = 0;
	} else if (newcomment && !newname) {
		g1002.lgrpi1002_comment = newcomment;
		buffer = (uint8_t *)&g1002;
		level = 1002;
	} else if (newname && newcomment) {
		g1.lgrpi1_name = newname;
		g1.lgrpi1_comment = newcomment;
		buffer = (uint8_t *)&g1;
		level = 1;
	} else {
		printf("not enough input\n");
		goto out;
	}

	/* NetLocalGroupSetInfo */

	status = NetLocalGroupSetInfo(hostname,
				      groupname,
				      level,
				      buffer,
				      &parm_err);
	if (status != 0) {
		printf("NetLocalGroupSetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
