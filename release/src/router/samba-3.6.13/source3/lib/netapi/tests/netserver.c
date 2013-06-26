/*
 *  Unix SMB/CIFS implementation.
 *  NetServer testsuite
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

NET_API_STATUS netapitest_server(struct libnetapi_ctx *ctx,
				 const char *hostname)
{
	NET_API_STATUS status = 0;
	uint32_t levels[] = { 100, 101, 102, 402, 403, 502, 503, 1005 };
	int i;

	printf("NetServer tests\n");

	/* basic queries */
	for (i=0; i<ARRAY_SIZE(levels); i++) {
		uint8_t *buffer = NULL;
		printf("testing NetServerGetInfo level %d\n", levels[i]);

		status = NetServerGetInfo(hostname, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetServerGetInfo");
			goto out;
		}
	}

	status = 0;

	printf("NetServer tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetServer testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
