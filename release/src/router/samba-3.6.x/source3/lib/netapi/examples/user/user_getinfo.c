/*
 *  Unix SMB/CIFS implementation.
 *  NetUserGetInfo query
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
	const char *username = NULL;
	uint8_t *buffer = NULL;
	uint32_t level = 0;
	char *sid_str = NULL;
	int i;

	struct USER_INFO_0 *u0;
	struct USER_INFO_1 *u1;
	struct USER_INFO_2 *u2;
	struct USER_INFO_3 *u3;
	struct USER_INFO_4 *u4;
	struct USER_INFO_10 *u10;
	struct USER_INFO_11 *u11;
	struct USER_INFO_20 *u20;
	struct USER_INFO_23 *u23;

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

	pc = poptGetContext("user_getinfo", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname username level");
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
	username = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetUserGetInfo */

	status = NetUserGetInfo(hostname,
				username,
				level,
				&buffer);
	if (status != 0) {
		printf("NetUserGetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 0:
			u0 = (struct USER_INFO_0 *)buffer;
			printf("name: %s\n", u0->usri0_name);
			break;
		case 1:
			u1 = (struct USER_INFO_1 *)buffer;
			printf("name: %s\n", u1->usri1_name);
			printf("password: %s\n", u1->usri1_password);
			printf("password_age: %d\n", u1->usri1_password_age);
			printf("priv: %d\n", u1->usri1_priv);
			printf("homedir: %s\n", u1->usri1_home_dir);
			printf("comment: %s\n", u1->usri1_comment);
			printf("flags: 0x%08x\n", u1->usri1_flags);
			printf("script: %s\n", u1->usri1_script_path);
			break;
		case 2:
			u2 = (struct USER_INFO_2 *)buffer;
			printf("name: %s\n", u2->usri2_name);
			printf("password: %s\n", u2->usri2_password);
			printf("password_age: %d\n", u2->usri2_password_age);
			printf("priv: %d\n", u2->usri2_priv);
			printf("homedir: %s\n", u2->usri2_home_dir);
			printf("comment: %s\n", u2->usri2_comment);
			printf("flags: 0x%08x\n", u2->usri2_flags);
			printf("script: %s\n", u2->usri2_script_path);
			printf("auth flags: 0x%08x\n", u2->usri2_auth_flags);
			printf("full name: %s\n", u2->usri2_full_name);
			printf("user comment: %s\n", u2->usri2_usr_comment);
			printf("user parameters: %s\n", u2->usri2_parms);
			printf("workstations: %s\n", u2->usri2_workstations);
			printf("last logon (seconds since jan. 1, 1970 GMT): %d\n",
				u2->usri2_last_logon);
			printf("last logoff (seconds since jan. 1, 1970 GMT): %d\n",
				u2->usri2_last_logoff);
			printf("account expires (seconds since jan. 1, 1970 GMT): %d\n",
				u2->usri2_acct_expires);
			printf("max storage: %d\n", u2->usri2_max_storage);
			printf("units per week: %d\n", u2->usri2_units_per_week);
			printf("logon hours:");
			for (i=0; i<21; i++) {
				printf(" %x", (uint8_t)u2->usri2_logon_hours[i]);
			}
			printf("\n");
			printf("bad password count: %d\n", u2->usri2_bad_pw_count);
			printf("logon count: %d\n", u2->usri2_num_logons);
			printf("logon server: %s\n", u2->usri2_logon_server);
			printf("country code: %d\n", u2->usri2_country_code);
			printf("code page: %d\n", u2->usri2_code_page);
			break;
		case 3:
			u3 = (struct USER_INFO_3 *)buffer;
			printf("name: %s\n", u3->usri3_name);
			printf("password_age: %d\n", u3->usri3_password_age);
			printf("priv: %d\n", u3->usri3_priv);
			printf("homedir: %s\n", u3->usri3_home_dir);
			printf("comment: %s\n", u3->usri3_comment);
			printf("flags: 0x%08x\n", u3->usri3_flags);
			printf("script: %s\n", u3->usri3_script_path);
			printf("auth flags: 0x%08x\n", u3->usri3_auth_flags);
			printf("full name: %s\n", u3->usri3_full_name);
			printf("user comment: %s\n", u3->usri3_usr_comment);
			printf("user parameters: %s\n", u3->usri3_parms);
			printf("workstations: %s\n", u3->usri3_workstations);
			printf("last logon (seconds since jan. 1, 1970 GMT): %d\n",
				u3->usri3_last_logon);
			printf("last logoff (seconds since jan. 1, 1970 GMT): %d\n",
				u3->usri3_last_logoff);
			printf("account expires (seconds since jan. 1, 1970 GMT): %d\n",
				u3->usri3_acct_expires);
			printf("max storage: %d\n", u3->usri3_max_storage);
			printf("units per week: %d\n", u3->usri3_units_per_week);
			printf("logon hours:");
			for (i=0; i<21; i++) {
				printf(" %x", (uint8_t)u3->usri3_logon_hours[i]);
			}
			printf("\n");
			printf("bad password count: %d\n", u3->usri3_bad_pw_count);
			printf("logon count: %d\n", u3->usri3_num_logons);
			printf("logon server: %s\n", u3->usri3_logon_server);
			printf("country code: %d\n", u3->usri3_country_code);
			printf("code page: %d\n", u3->usri3_code_page);
			printf("user id: %d\n", u3->usri3_user_id);
			printf("primary group id: %d\n", u3->usri3_primary_group_id);
			printf("profile: %s\n", u3->usri3_profile);
			printf("home dir drive: %s\n", u3->usri3_home_dir_drive);
			printf("password expired: %d\n", u3->usri3_password_expired);
			break;
		case 4:
			u4 = (struct USER_INFO_4 *)buffer;
			printf("name: %s\n", u4->usri4_name);
			printf("password: %s\n", u4->usri4_password);
			printf("password_age: %d\n", u4->usri4_password_age);
			printf("priv: %d\n", u4->usri4_priv);
			printf("homedir: %s\n", u4->usri4_home_dir);
			printf("comment: %s\n", u4->usri4_comment);
			printf("flags: 0x%08x\n", u4->usri4_flags);
			printf("script: %s\n", u4->usri4_script_path);
			printf("auth flags: 0x%08x\n", u4->usri4_auth_flags);
			printf("full name: %s\n", u4->usri4_full_name);
			printf("user comment: %s\n", u4->usri4_usr_comment);
			printf("user parameters: %s\n", u4->usri4_parms);
			printf("workstations: %s\n", u4->usri4_workstations);
			printf("last logon (seconds since jan. 1, 1970 GMT): %d\n",
				u4->usri4_last_logon);
			printf("last logoff (seconds since jan. 1, 1970 GMT): %d\n",
				u4->usri4_last_logoff);
			printf("account expires (seconds since jan. 1, 1970 GMT): %d\n",
				u4->usri4_acct_expires);
			printf("max storage: %d\n", u4->usri4_max_storage);
			printf("units per week: %d\n", u4->usri4_units_per_week);
			printf("logon hours:");
			for (i=0; i<21; i++) {
				printf(" %x", (uint8_t)u4->usri4_logon_hours[i]);
			}
			printf("\n");
			printf("bad password count: %d\n", u4->usri4_bad_pw_count);
			printf("logon count: %d\n", u4->usri4_num_logons);
			printf("logon server: %s\n", u4->usri4_logon_server);
			printf("country code: %d\n", u4->usri4_country_code);
			printf("code page: %d\n", u4->usri4_code_page);
			if (ConvertSidToStringSid(u4->usri4_user_sid,
						  &sid_str)) {
				printf("user_sid: %s\n", sid_str);
				free(sid_str);
			}
			printf("primary group id: %d\n", u4->usri4_primary_group_id);
			printf("profile: %s\n", u4->usri4_profile);
			printf("home dir drive: %s\n", u4->usri4_home_dir_drive);
			printf("password expired: %d\n", u4->usri4_password_expired);
			break;
		case 10:
			u10 = (struct USER_INFO_10 *)buffer;
			printf("name: %s\n", u10->usri10_name);
			printf("comment: %s\n", u10->usri10_comment);
			printf("usr_comment: %s\n", u10->usri10_usr_comment);
			printf("full_name: %s\n", u10->usri10_full_name);
			break;
		case 11:
			u11 = (struct USER_INFO_11 *)buffer;
			printf("name: %s\n", u11->usri11_name);
			printf("comment: %s\n", u11->usri11_comment);
			printf("user comment: %s\n", u11->usri11_usr_comment);
			printf("full name: %s\n", u11->usri11_full_name);
			printf("priv: %d\n", u11->usri11_priv);
			printf("auth flags: 0x%08x\n", u11->usri11_auth_flags);
			printf("password_age: %d\n", u11->usri11_password_age);
			printf("homedir: %s\n", u11->usri11_home_dir);
			printf("user parameters: %s\n", u11->usri11_parms);
			printf("last logon (seconds since jan. 1, 1970 GMT): %d\n",
				u11->usri11_last_logon);
			printf("last logoff (seconds since jan. 1, 1970 GMT): %d\n",
				u11->usri11_last_logoff);
			printf("bad password count: %d\n", u11->usri11_bad_pw_count);
			printf("logon count: %d\n", u11->usri11_num_logons);
			printf("logon server: %s\n", u11->usri11_logon_server);
			printf("country code: %d\n", u11->usri11_country_code);
			printf("workstations: %s\n", u11->usri11_workstations);
			printf("max storage: %d\n", u11->usri11_max_storage);
			printf("units per week: %d\n", u11->usri11_units_per_week);
			printf("logon hours:");
			for (i=0; i<21; i++) {
				printf(" %x", (uint8_t)u11->usri11_logon_hours[i]);
			}
			printf("\n");
			printf("code page: %d\n", u11->usri11_code_page);
			break;
		case 20:
			u20 = (struct USER_INFO_20 *)buffer;
			printf("name: %s\n", u20->usri20_name);
			printf("comment: %s\n", u20->usri20_comment);
			printf("flags: 0x%08x\n", u20->usri20_flags);
			printf("rid: %d\n", u20->usri20_user_id);
			break;
		case 23:
			u23 = (struct USER_INFO_23 *)buffer;
			printf("name: %s\n", u23->usri23_name);
			printf("comment: %s\n", u23->usri23_comment);
			printf("flags: 0x%08x\n", u23->usri23_flags);
			if (ConvertSidToStringSid(u23->usri23_user_sid,
						  &sid_str)) {
				printf("user_sid: %s\n", sid_str);
				free(sid_str);
			}
			break;
		default:
			break;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
