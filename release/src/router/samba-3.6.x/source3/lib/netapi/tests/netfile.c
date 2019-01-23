/*
 *  Unix SMB/CIFS implementation.
 *  NetFile testsuite
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

static NET_API_STATUS test_netfileenum(const char *hostname,
				       uint32_t level)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint8_t *buffer = NULL;
	int i;

	struct FILE_INFO_2 *i2 = NULL;
	struct FILE_INFO_3 *i3 = NULL;

	printf("testing NetFileEnum level %d\n", level);

	do {
		status = NetFileEnum(hostname,
				     NULL,
				     NULL,
				     level,
				     &buffer,
				     (uint32_t)-1,
				     &entries_read,
				     &total_entries,
				     &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 2:
					i2 = (struct FILE_INFO_2 *)buffer;
					break;
				case 3:
					i3 = (struct FILE_INFO_3 *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 2:
					case 3:
						break;
					default:
						break;
				}

				switch (level) {
					case 2:
						i2++;
						break;
					case 3:
						i3++;
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	return 0;
}

NET_API_STATUS netapitest_file(struct libnetapi_ctx *ctx,
			       const char *hostname)
{
	NET_API_STATUS status = 0;
	uint32_t enum_levels[] = { 2, 3 };
	int i;

	printf("NetFile tests\n");

	/* test enum */

	for (i=0; i<ARRAY_SIZE(enum_levels); i++) {

		status = test_netfileenum(hostname, enum_levels[i]);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetFileEnum");
			goto out;
		}
	}

	/* basic queries */
#if 0
	{
		uint32_t levels[] = { 2, 3 };
		for (i=0; i<ARRAY_SIZE(levels); i++) {
			uint8_t *buffer = NULL;

			printf("testing NetFileGetInfo level %d\n", levels[i]);

			status = NetFileGetInfo(hostname, fid, levels[i], &buffer);
			if (status && status != 124) {
				NETAPI_STATUS(ctx, status, "NetFileGetInfo");
				goto out;
			}
		}
	}
#endif

	status = 0;

	printf("NetFile tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetFile testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
