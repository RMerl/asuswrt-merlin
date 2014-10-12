/*
 *  Unix SMB/CIFS implementation.
 *  NetLocalGroup testsuite
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

static NET_API_STATUS test_netlocalgroupenum(const char *hostname,
					     uint32_t level,
					     const char *groupname)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	int found_group = 0;
	const char *current_name = NULL;
	uint8_t *buffer = NULL;
	int i;

	struct LOCALGROUP_INFO_0 *info0 = NULL;
	struct LOCALGROUP_INFO_1 *info1 = NULL;

	printf("testing NetLocalGroupEnum level %d\n", level);

	do {
		status = NetLocalGroupEnum(hostname,
					   level,
					   &buffer,
					   (uint32_t)-1,
					   &entries_read,
					   &total_entries,
					   &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 0:
					info0 = (struct LOCALGROUP_INFO_0 *)buffer;
					break;
				case 1:
					info1 = (struct LOCALGROUP_INFO_1 *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 0:
						current_name = 	info0->lgrpi0_name;
						break;
					case 1:
						current_name = 	info1->lgrpi1_name;
						break;
					default:
						break;
				}

				if (strcasecmp(current_name, groupname) == 0) {
					found_group = 1;
				}

				switch (level) {
					case 0:
						info0++;
						break;
					case 1:
						info1++;
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	if (!found_group) {
		printf("failed to get group\n");
		return -1;
	}

	return 0;
}

NET_API_STATUS netapitest_localgroup(struct libnetapi_ctx *ctx,
				     const char *hostname)
{
	NET_API_STATUS status = 0;
	const char *groupname, *groupname2;
	uint8_t *buffer = NULL;
	struct LOCALGROUP_INFO_0 g0;
	uint32_t parm_err = 0;
	uint32_t levels[] = { 0, 1, 1002 };
	uint32_t enum_levels[] = { 0, 1 };
	int i;

	printf("NetLocalgroup tests\n");

	groupname = "torture_test_localgroup";
	groupname2 = "torture_test_localgroup2";

	/* cleanup */
	NetLocalGroupDel(hostname, groupname);
	NetLocalGroupDel(hostname, groupname2);

	/* add a localgroup */

	printf("testing NetLocalGroupAdd\n");

	g0.lgrpi0_name = groupname;

	status = NetLocalGroupAdd(hostname, 0, (uint8_t *)&g0, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetLocalGroupAdd");
		goto out;
	};

	/* test enum */

	for (i=0; i<ARRAY_SIZE(enum_levels); i++) {

		status = test_netlocalgroupenum(hostname, enum_levels[i], groupname);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetLocalGroupEnum");
			goto out;
		}
	}


	/* basic queries */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		printf("testing NetLocalGroupGetInfo level %d\n", levels[i]);

		status = NetLocalGroupGetInfo(hostname, groupname, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetLocalGroupGetInfo");
			goto out;
		};
	}

	/* alias rename */

	printf("testing NetLocalGroupSetInfo level 0\n");

	g0.lgrpi0_name = groupname2;

	status = NetLocalGroupSetInfo(hostname, groupname, 0, (uint8_t *)&g0, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetLocalGroupSetInfo");
		goto out;
	};

	/* should not exist anymore */

	status = NetLocalGroupDel(hostname, groupname);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetLocalGroupDel");
		goto out;
	};

	/* query info */

	for (i=0; i<ARRAY_SIZE(levels); i++) {
		status = NetLocalGroupGetInfo(hostname, groupname2, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetLocalGroupGetInfo");
			goto out;
		};
	}

	/* delete */

	printf("testing NetLocalGroupDel\n");

	status = NetLocalGroupDel(hostname, groupname2);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetLocalGroupDel");
		goto out;
	};

	/* should not exist anymore */

	status = NetLocalGroupGetInfo(hostname, groupname2, 0, &buffer);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetLocalGroupGetInfo");
		goto out;
	};

	status = 0;

	printf("NetLocalgroup tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetLocalGroup testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
