/*
 *  Unix SMB/CIFS implementation.
 *  NetShare testsuite
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

static NET_API_STATUS test_netshareenum(const char *hostname,
					uint32_t level,
					const char *sharename)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	int found_share = 0;
	const char *current_name = NULL;
	uint8_t *buffer = NULL;
	int i;

	struct SHARE_INFO_0 *i0 = NULL;
	struct SHARE_INFO_1 *i1 = NULL;
	struct SHARE_INFO_2 *i2 = NULL;

	printf("testing NetShareEnum level %d\n", level);

	do {
		status = NetShareEnum(hostname,
				      level,
				      &buffer,
				      (uint32_t)-1,
				      &entries_read,
				      &total_entries,
				      &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
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
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 0:
						current_name = i0->shi0_netname;
						break;
					case 1:
						current_name = i1->shi1_netname;
						break;
					case 2:
						current_name = i2->shi2_netname;
						break;
					default:
						break;
				}

				if (strcasecmp(current_name, sharename) == 0) {
					found_share = 1;
				}

				switch (level) {
					case 0:
						i0++;
						break;
					case 1:
						i1++;
						break;
					case 2:
						i2++;
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	if (!found_share) {
		printf("failed to get share\n");
		return -1;
	}

	return 0;
}

NET_API_STATUS netapitest_share(struct libnetapi_ctx *ctx,
				const char *hostname)
{
	NET_API_STATUS status = 0;
	const char *sharename, *comment;
	uint8_t *buffer = NULL;
	struct SHARE_INFO_2 i2;
	struct SHARE_INFO_1004 i1004;
	struct SHARE_INFO_501 *i501 = NULL;
	uint32_t parm_err = 0;
	uint32_t levels[] = { 0, 1, 2, 501, 1005 };
	uint32_t enum_levels[] = { 0, 1, 2 };
	int i;

	printf("NetShare tests\n");

	sharename = "torture_test_share";

	/* cleanup */
	NetShareDel(hostname, sharename, 0);

	/* add a share */

	printf("testing NetShareAdd\n");

	ZERO_STRUCT(i2);

	i2.shi2_netname = sharename;
	i2.shi2_path = "c:\\";

	status = NetShareAdd(hostname, 2, (uint8_t *)&i2, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetShareAdd");
		goto out;
	};

	/* test enum */

	for (i=0; i<ARRAY_SIZE(enum_levels); i++) {

		status = test_netshareenum(hostname, enum_levels[i], sharename);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetShareEnum");
			goto out;
		}
	}

	/* basic queries */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		printf("testing NetShareGetInfo level %d\n", levels[i]);

		status = NetShareGetInfo(hostname, sharename, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetShareGetInfo");
			goto out;
		}
	}


	comment = "NetApi generated comment";

	i1004.shi1004_remark = comment;

	printf("testing NetShareSetInfo level 1004\n");

	status = NetShareSetInfo(hostname, sharename, 1004, (uint8_t *)&i1004, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetShareSetInfo");
		goto out;
	}

	status = NetShareGetInfo(hostname, sharename, 501, (uint8_t **)&i501);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetShareGetInfo");
		goto out;
	}

	if (strcasecmp(i501->shi501_remark, comment) != 0) {
		NETAPI_STATUS(ctx, status, "NetShareGetInfo");
		goto out;
	}

	/* delete */

	printf("testing NetShareDel\n");

	status = NetShareDel(hostname, sharename, 0);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetShareDel");
		goto out;
	};

	/* should not exist anymore */

	status = NetShareGetInfo(hostname, sharename, 0, &buffer);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetShareGetInfo");
		goto out;
	};

	status = 0;

	printf("NetShare tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetShare testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
