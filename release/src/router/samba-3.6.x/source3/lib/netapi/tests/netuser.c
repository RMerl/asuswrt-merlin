/*
 *  Unix SMB/CIFS implementation.
 *  NetUser testsuite
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

static NET_API_STATUS test_netuserenum(const char *hostname,
				       uint32_t level,
				       const char *username)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	const char *current_name = NULL;
	int found_user = 0;
	uint8_t *buffer = NULL;
	int i;

	struct USER_INFO_0 *info0 = NULL;
	struct USER_INFO_1 *info1 = NULL;
	struct USER_INFO_2 *info2 = NULL;
	struct USER_INFO_3 *info3 = NULL;
	struct USER_INFO_4 *info4 = NULL;
	struct USER_INFO_10 *info10 = NULL;
	struct USER_INFO_11 *info11 = NULL;
	struct USER_INFO_20 *info20 = NULL;
	struct USER_INFO_23 *info23 = NULL;

	printf("testing NetUserEnum level %d\n", level);

	do {
		status = NetUserEnum(hostname,
				     level,
				     FILTER_NORMAL_ACCOUNT,
				     &buffer,
				     120, //(uint32_t)-1,
				     &entries_read,
				     &total_entries,
				     &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 0:
					info0 = (struct USER_INFO_0 *)buffer;
					break;
				case 1:
					info1 = (struct USER_INFO_1 *)buffer;
					break;
				case 2:
					info2 = (struct USER_INFO_2 *)buffer;
					break;
				case 3:
					info3 = (struct USER_INFO_3 *)buffer;
					break;
				case 4:
					info4 = (struct USER_INFO_4 *)buffer;
					break;
				case 10:
					info10 = (struct USER_INFO_10 *)buffer;
					break;
				case 11:
					info11 = (struct USER_INFO_11 *)buffer;
					break;
				case 20:
					info20 = (struct USER_INFO_20 *)buffer;
					break;
				case 23:
					info23 = (struct USER_INFO_23 *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 0:
						current_name = info0->usri0_name;
						break;
					case 1:
						current_name = info1->usri1_name;
						break;
					case 2:
						current_name = info2->usri2_name;
						break;
					case 3:
						current_name = info3->usri3_name;
						break;
					case 4:
						current_name = info4->usri4_name;
						break;
					case 10:
						current_name = info10->usri10_name;
						break;
					case 11:
						current_name = info11->usri11_name;
						break;
					case 20:
						current_name = info20->usri20_name;
						break;
					case 23:
						current_name = info23->usri23_name;
						break;
					default:
						return -1;
				}

				if (strcasecmp(current_name, username) == 0) {
					found_user = 1;
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
					case 4:
						info4++;
						break;
					case 10:
						info10++;
						break;
					case 11:
						info11++;
						break;
					case 20:
						info20++;
						break;
					case 23:
						info23++;
						break;
					default:
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	if (!found_user) {
		printf("failed to get user\n");
		return -1;
	}

	return 0;
}

NET_API_STATUS test_netuseradd(const char *hostname,
			       const char *username)
{
	struct USER_INFO_1 u1;
	uint32_t parm_err = 0;

	ZERO_STRUCT(u1);

	printf("testing NetUserAdd\n");

	u1.usri1_name = username;
	u1.usri1_password = "W297!832jD8J";
	u1.usri1_password_age = 0;
	u1.usri1_priv = 0;
	u1.usri1_home_dir = NULL;
	u1.usri1_comment = "User created using Samba NetApi Example code";
	u1.usri1_flags = 0;
	u1.usri1_script_path = NULL;

	return NetUserAdd(hostname, 1, (uint8_t *)&u1, &parm_err);
}

static NET_API_STATUS test_netusermodals(struct libnetapi_ctx *ctx,
					 const char *hostname)
{
	NET_API_STATUS status;
	struct USER_MODALS_INFO_0 *u0 = NULL;
	struct USER_MODALS_INFO_0 *_u0 = NULL;
	uint8_t *buffer = NULL;
	uint32_t parm_err = 0;
	uint32_t levels[] = { 0, 1, 2, 3 };
	int i = 0;

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		printf("testing NetUserModalsGet level %d\n", levels[i]);

		status = NetUserModalsGet(hostname, levels[i], &buffer);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetUserModalsGet");
			return status;
		}
	}

	status = NetUserModalsGet(hostname, 0, (uint8_t **)&u0);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserModalsGet");
		return status;
	}

	printf("testing NetUserModalsSet\n");

	status = NetUserModalsSet(hostname, 0, (uint8_t *)u0, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserModalsSet");
		return status;
	}

	status = NetUserModalsGet(hostname, 0, (uint8_t **)&_u0);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserModalsGet");
		return status;
	}

	if (memcmp(u0, _u0, sizeof(u0) != 0)) {
		printf("USER_MODALS_INFO_0 struct has changed!!!!\n");
		return -1;
	}

	return 0;
}

static NET_API_STATUS test_netusergetgroups(const char *hostname,
					    uint32_t level,
					    const char *username,
					    const char *groupname)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	const char *current_name;
	int found_group = 0;
	uint8_t *buffer = NULL;
	int i;

	struct GROUP_USERS_INFO_0 *i0;
	struct GROUP_USERS_INFO_1 *i1;

	printf("testing NetUserGetGroups level %d\n", level);

	do {
		status = NetUserGetGroups(hostname,
					  username,
					  level,
					  &buffer,
					  120, //(uint32_t)-1,
					  &entries_read,
					  &total_entries);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 0:
					i0 = (struct GROUP_USERS_INFO_0 *)buffer;
					break;
				case 1:
					i1 = (struct GROUP_USERS_INFO_1 *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 0:
						current_name = i0->grui0_name;
						break;
					case 1:
						current_name = i1->grui1_name;
						break;
					default:
						return -1;
				}

				if (groupname && strcasecmp(current_name, groupname) == 0) {
					found_group = 1;
				}

				switch (level) {
					case 0:
						i0++;
						break;
					case 1:
						i1++;
						break;
					default:
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	if (groupname && !found_group) {
		printf("failed to get membership\n");
		return -1;
	}

	return 0;
}

NET_API_STATUS netapitest_user(struct libnetapi_ctx *ctx,
			       const char *hostname)
{
	NET_API_STATUS status = 0;
	const char *username, *username2;
	uint8_t *buffer = NULL;
	uint32_t levels[] = { 0, 1, 2, 3, 4, 10, 11, 20, 23 };
	uint32_t enum_levels[] = { 0, 1, 2, 3, 4, 10, 11, 20, 23 };
	uint32_t getgr_levels[] = { 0, 1 };
	int i;

	struct USER_INFO_1007 u1007;
	uint32_t parm_err = 0;

	printf("NetUser tests\n");

	username = "torture_test_user";
	username2 = "torture_test_user2";

	/* cleanup */
	NetUserDel(hostname, username);
	NetUserDel(hostname, username2);

	/* add a user */

	status = test_netuseradd(hostname, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserAdd");
		goto out;
	}

	/* enum the new user */

	for (i=0; i<ARRAY_SIZE(enum_levels); i++) {

		status = test_netuserenum(hostname, enum_levels[i], username);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetUserEnum");
			goto out;
		}
	}

	/* basic queries */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		printf("testing NetUserGetInfo level %d\n", levels[i]);

		status = NetUserGetInfo(hostname, username, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetUserGetInfo");
			goto out;
		}
	}

	/* testing getgroups */

	for (i=0; i<ARRAY_SIZE(getgr_levels); i++) {

		status = test_netusergetgroups(hostname, getgr_levels[i], username, NULL);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetUserGetGroups");
			goto out;
		}
	}

	/* modify description */

	printf("testing NetUserSetInfo level %d\n", 1007);

	u1007.usri1007_comment = "NetApi modified user";

	status = NetUserSetInfo(hostname, username, 1007, (uint8_t *)&u1007, &parm_err);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserSetInfo");
		goto out;
	}

	/* query info */

	for (i=0; i<ARRAY_SIZE(levels); i++) {
		status = NetUserGetInfo(hostname, username, levels[i], &buffer);
		if (status && status != 124) {
			NETAPI_STATUS(ctx, status, "NetUserGetInfo");
			goto out;
		}
	}

	/* delete */

	printf("testing NetUserDel\n");

	status = NetUserDel(hostname, username);
	if (status) {
		NETAPI_STATUS(ctx, status, "NetUserDel");
		goto out;
	}

	/* should not exist anymore */

	status = NetUserGetInfo(hostname, username, 0, &buffer);
	if (status == 0) {
		NETAPI_STATUS(ctx, status, "NetUserGetInfo");
		goto out;
	}

	status = test_netusermodals(ctx, hostname);
	if (status) {
		goto out;
	}

	status = 0;

	printf("NetUser tests succeeded\n");
 out:
	/* cleanup */
	NetUserDel(hostname, username);
	NetUserDel(hostname, username2);

	if (status != 0) {
		printf("NetUser testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
