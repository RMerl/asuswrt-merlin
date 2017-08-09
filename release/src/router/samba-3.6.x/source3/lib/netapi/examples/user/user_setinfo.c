/*
 *  Unix SMB/CIFS implementation.
 *  NetUserSetInfo query
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
	uint32_t level = 0;
	uint32_t parm_err = 0;
	uint8_t *buffer = NULL;
	const char *val = NULL;

	struct USER_INFO_0 u0;
	struct USER_INFO_1 u1;
	struct USER_INFO_2 u2;
	struct USER_INFO_3 u3;
	struct USER_INFO_4 u4;
	struct USER_INFO_21 u21;
	struct USER_INFO_22 u22;
	struct USER_INFO_1003 u1003;
	struct USER_INFO_1005 u1005;
	struct USER_INFO_1006 u1006;
	struct USER_INFO_1007 u1007;
	struct USER_INFO_1008 u1008;
	struct USER_INFO_1009 u1009;
	struct USER_INFO_1010 u1010;
	struct USER_INFO_1011 u1011;
	struct USER_INFO_1012 u1012;
	struct USER_INFO_1014 u1014;
	struct USER_INFO_1017 u1017;
	struct USER_INFO_1020 u1020;
	struct USER_INFO_1024 u1024;
	struct USER_INFO_1051 u1051;
	struct USER_INFO_1052 u1052;
	struct USER_INFO_1053 u1053;

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

	pc = poptGetContext("user_setinfo", argc, argv, long_options, 0);

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

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	level = atoi(poptGetArg(pc));

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	val = poptGetArg(pc);

	/* NetUserSetInfo */

	switch (level) {
		case 0:
			u0.usri0_name = val;
			buffer = (uint8_t *)&u0;
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			break;
		case 21:
			break;
		case 22:
			break;
		case 1003:
			u1003.usri1003_password = val;
			buffer = (uint8_t *)&u1003;
			break;
		case 1005:
			u1005.usri1005_priv = atoi(val);
			buffer = (uint8_t *)&u1005;
			break;
		case 1006:
			u1006.usri1006_home_dir = val;
			buffer = (uint8_t *)&u1006;
			break;
		case 1007:
			u1007.usri1007_comment = val;
			buffer = (uint8_t *)&u1007;
			break;
		case 1008:
			u1008.usri1008_flags = atoi(val);
			buffer = (uint8_t *)&u1008;
			break;
		case 1009:
			u1009.usri1009_script_path = val;
			buffer = (uint8_t *)&u1009;
			break;
		case 1010:
			u1010.usri1010_auth_flags = atoi(val);
			buffer = (uint8_t *)&u1010;
			break;
		case 1011:
			u1011.usri1011_full_name = val;
			buffer = (uint8_t *)&u1011;
			break;
		case 1012:
			u1012.usri1012_usr_comment = val;
			buffer = (uint8_t *)&u1012;
			break;
		case 1014:
			u1014.usri1014_workstations = val;
			buffer = (uint8_t *)&u1014;
			break;
		case 1017:
			u1017.usri1017_acct_expires = atoi(val);
			buffer = (uint8_t *)&u1017;
			break;
		case 1020:
			break;
		case 1024:
			u1024.usri1024_country_code = atoi(val);
			buffer = (uint8_t *)&u1024;
			break;
		case 1051:
			u1051.usri1051_primary_group_id = atoi(val);
			buffer = (uint8_t *)&u1051;
			break;
		case 1052:
			u1052.usri1052_profile = val;
			buffer = (uint8_t *)&u1052;
			break;
		case 1053:
			u1053.usri1053_home_dir_drive = val;
			buffer = (uint8_t *)&u1053;
			break;
		default:
			break;
	}

	status = NetUserSetInfo(hostname,
				username,
				level,
				buffer,
				&parm_err);
	if (status != 0) {
		printf("NetUserSetInfo failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
