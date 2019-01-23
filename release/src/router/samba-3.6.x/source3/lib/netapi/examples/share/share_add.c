/*
 *  Unix SMB/CIFS implementation.
 *  NetShareAdd query
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
	const char *path = NULL;
	uint32_t level = 0;
	uint32_t parm_err = 0;

	struct SHARE_INFO_2 i2;

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

	pc = poptGetContext("share_add", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname sharename path");
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

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	path = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetShareAdd */

	i2.shi2_netname		= sharename;
	i2.shi2_type		= 0;
	i2.shi2_remark		= "Test share created via NetApi";
	i2.shi2_permissions	= 0;
	i2.shi2_max_uses	= (uint32_t)-1;
	i2.shi2_current_uses	= 0;
	i2.shi2_path		= path;
	i2.shi2_passwd		= NULL;

	status = NetShareAdd(hostname,
			     2,
			     (uint8_t *)&i2,
			     &parm_err);
	if (status != 0) {
		printf("NetShareAdd failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
