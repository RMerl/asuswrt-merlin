/*
 *  Unix SMB/CIFS implementation.
 *  NetGroup testsuite
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

static NET_API_STATUS test_netgroupenum(const char *hostname,
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

	struct GROUP_INFO_0 *info0 = NULL;
	struct GROUP_INFO_1 *info1 = NULL;
	struct GROUP_INFO_2 *info2 = NULL;
	struct GROUP_INFO_3 *info3 = NULL;

	printf("testing NetGroupEnum level %d\n", level);

	do {
		status = NetGroupEnum(hostname,
				      level,
				      &buffer,
				      120, //(uint32_t)-1,
				      &entries_read,
				      &total_entries,
				      &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 0:
					info0 = (struct GROUP_INFO_0 *)buffer;
					break;
				case 1:
					info1 = (struct GROUP_INFO_1 *)buffer;
					break;
				case 2:
					info2 = (struct GROUP_INFO_2 *)buffer;
					break;
				case 3:
					info3 = (struct GROUP_INFO_3 *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 0:
						current_name = 	info0->grpi0_name;
						break;
					case 1:
						current_name = 	info1->grpi1_name;
						break;
					case 2:
						current_name = 	info2->grpi2_name;
						break;
					case 3:
						current_name = 	info3->grpi3_name;
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
					case 2:
						info2++;
						break;
					case 3:
						info3++;
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

static NET_API_STATUS test_netgroupgetusers(const char *hostname,
					    uint32_t level,
					    const char *groupname,
					    const char *username)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	int found_user = 0;
	const char *current_name = NULL;
	uint8_t *buffer = NULL;
	int i;

	struct GROUP_USERS_INFO_0 *info0 = NULL;
	struct GROUP_USERS_INFO_1 *info1 = NULL;

	printf("testing NetGroupGetUsers level %d\n", level);

	do {
		status = NetGroupGetUsers(hostname,
					  groupname,
					  level,
					  &buffer,
					  (uint32_t)-1,
					  &entries_read,
					  &total_entries,
					  &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {

			switch (level) {
				case 0:
					info0 = (struct GROUP_USERS_INFO_0 *)buffer;
					break;
				case 1:
					info1 = (struct GROUP_USERS_INFO_1 *)buffer;
					break;
				default:
					break;
			}
			for (i=0; i<entries_read; i++) {
				switch (level) {
					case 0:
						current_name = info0->grui0_name;
						break;
					case 1:
						current_name = info1->grui1_name;
						break;
					default:
						break;
				}

				if (username && strcasecmp(current_name, username) == 0) {
					found_user = 1;
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

	if (username && !found_user) {
		printf("failed to get user\n");
		return -1;
	}

	return 0;
}

static NET_API_STATUS test_netgroupsetusers(const char *hostname,
					    const char *groupname,
					    uint32_t level,
					    size_t num_entries,
					    const char **names)
{
	NET_API_STATUS status;
	uint8_t *buffer = NULL;
	int i = 0;
	size_t buf_size = 0;

	struct GROUP_USERS_INFO_0 *g0 = NULL;
	struct GROUP_USERS_INFO_1 *g1 = NULL;

	printf("testing NetGroupSetUsers level %d\n", level);

	switch (level) {
		case 0:
			buf_size = sizeof(struct GROUP_USERS_INFO_0) * num_entries;

			status = NetApiBufferAllocate(buf_size, (void **)&g0);
			if (status) {
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
	if (status) {
		goto out;
	}

 out:
	NetApiBufferFree(buffer);
	return status;
}

NET_API_STATUS netapitest_group(struct libnetapi_ctx *ctx,
				const char *hostname)
{
	NET_API_STATUS status = 0;
	const char *username, *groupname, *groupname2;
	uint8_t *buffer = NULL;
	struct GROUP_INFO_0 g0;
	uint32_t parm_err = 0;
	uint32_t levels[] = { 0, 1, 2, 3};
	uint32_t enum_levels[] = { 0, 1, 2, 3};
	uint32_t getmem_levels[] = { 0, 1};
	int i;

	printf("NetGroup tests\n");

	username = "torture_test_user";
	groupname = "torture_test_group";
	groupname2 = "torture_test_group2";

	/* cleanup */
	NetGroupDel(hostname, groupname);
	NetGroupDel(hostname, groupname2);
	NetUserDel(hostname, username);

	/* add a group */

	g0.grpi0_name = groupname;

	printf("testing NetGroupAdd\n");

	status = NetGroupAdd(hostname, 0, (uint8_t *)&g0, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupAdd");
		goto out;
	}

	/* 2nd add must fail */

	status = NetGroupAdd(hostname, 0, (uint8_t *)&g0, &parm_err);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetGroupAdd");
		goto out;
	}

	/* test enum */

	for (i=0; i<ARRAY_SIZE(enum_levels); i++) {

		status = test_netgroupenum(hostname, enum_levels[i], groupname);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetGroupEnum");
			goto out;
		}
	}

	/* basic queries */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		printf("testing NetGroupGetInfo level %d\n", levels[i]);

		status = NetGroupGetInfo(hostname, groupname, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetGroupGetInfo");
			goto out;
		}
	}

	/* group rename */

	g0.grpi0_name = groupname2;

	printf("testing NetGroupSetInfo level 0\n");

	status = NetGroupSetInfo(hostname, groupname, 0, (uint8_t *)&g0, &parm_err);
	switch (status) {
		case 0:
			break;
		case 50: /* not supported */
		case 124: /* not implemented */
			groupname2 = groupname;
			goto skip_rename;
		default:
			NETAPI_STATUS(ctx, status, "NetGroupSetInfo");
			goto out;
	}

	/* should not exist anymore */

	status = NetGroupDel(hostname, groupname);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetGroupDel");
		goto out;
	}

 skip_rename:
	/* query info */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		status = NetGroupGetInfo(hostname, groupname2, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetGroupGetInfo");
			goto out;
		}
	}

	/* add user to group */

	status = test_netuseradd(hostname, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserAdd");
		goto out;
	}

	/* should not be member */

	for (i=0; i<ARRAY_SIZE(getmem_levels); i++) {

		status = test_netgroupgetusers(hostname, getmem_levels[i], groupname2, NULL);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetGroupGetUsers");
			goto out;
		}
	}

	printf("testing NetGroupAddUser\n");

	status = NetGroupAddUser(hostname, groupname2, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupAddUser");
		goto out;
	}

	/* should be member */

	for (i=0; i<ARRAY_SIZE(getmem_levels); i++) {

		status = test_netgroupgetusers(hostname, getmem_levels[i], groupname2, username);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetGroupGetUsers");
			goto out;
		}
	}

	printf("testing NetGroupDelUser\n");

	status = NetGroupDelUser(hostname, groupname2, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupDelUser");
		goto out;
	}

	/* should not be member */

	status = test_netgroupgetusers(hostname, 0, groupname2, NULL);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupGetUsers");
		goto out;
	}

	/* set it again via exlicit member set */

	status = test_netgroupsetusers(hostname, groupname2, 0, 1, &username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupSetUsers");
		goto out;
	}

	/* should be member */

	status = test_netgroupgetusers(hostname, 0, groupname2, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupGetUsers");
		goto out;
	}
#if 0
	/* wipe out member list */

	status = test_netgroupsetusers(hostname, groupname2, 0, 0, NULL);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupSetUsers");
		goto out;
	}

	/* should not be member */

	status = test_netgroupgetusers(hostname, 0, groupname2, NULL);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupGetUsers");
		goto out;
	}
#endif
	status = NetUserDel(hostname, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserDel");
		goto out;
	}

	/* delete */

	printf("testing NetGroupDel\n");

	status = NetGroupDel(hostname, groupname2);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetGroupDel");
		goto out;
	};

	/* should not exist anymore */

	status = NetGroupGetInfo(hostname, groupname2, 0, &buffer);
	if (status == 0) {
		NETAPI_STATUS_MSG(ctx, status, "NetGroupGetInfo", "expected failure and error code");
		goto out;
	};

	status = 0;

	printf("NetGroup tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetGroup testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
