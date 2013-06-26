/*
 *  Unix SMB/CIFS implementation.
 *  NetUserModalsSet query
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
	uint8_t *buffer = NULL;
	uint32_t level = 0;
	uint32_t value = 0;
	uint32_t parm_err = 0;

	struct USER_MODALS_INFO_0 u0;
	struct USER_MODALS_INFO_1 u1;
	struct USER_MODALS_INFO_2 u2;
	struct USER_MODALS_INFO_3 u3;
	struct USER_MODALS_INFO_1001 u1001;
	struct USER_MODALS_INFO_1002 u1002;
	struct USER_MODALS_INFO_1003 u1003;
	struct USER_MODALS_INFO_1004 u1004;
	struct USER_MODALS_INFO_1005 u1005;
	struct USER_MODALS_INFO_1006 u1006;
	struct USER_MODALS_INFO_1007 u1007;

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

	pc = poptGetContext("user_modalsset", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname level value");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	hostname = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	if (poptPeekArg(pc)) {
		value = atoi(poptGetArg(pc));
	}

	switch (level) {
		case 0:
			u0.usrmod0_min_passwd_len = 0;
			u0.usrmod0_max_passwd_age = (86400 * 30); /* once a month */
			u0.usrmod0_min_passwd_age = 0;
			u0.usrmod0_force_logoff = TIMEQ_FOREVER;
			u0.usrmod0_password_hist_len = 0;
			buffer = (uint8_t *)&u0;
			break;
		case 1:
		case 2:
		case 3:
			break;
		case 1001:
			u1001.usrmod1001_min_passwd_len = 0;
			buffer = (uint8_t *)&u1001;
			break;
		case 1002:
			u1002.usrmod1002_max_passwd_age = 0;
			buffer = (uint8_t *)&u1002;
			break;
		case 1003:
			u1003.usrmod1003_min_passwd_age = (86400 * 30); /* once a month */
			buffer = (uint8_t *)&u1003;
			break;
		case 1004:
			u1004.usrmod1004_force_logoff = TIMEQ_FOREVER;
			buffer = (uint8_t *)&u1004;
			break;
		case 1005:
			u1005.usrmod1005_password_hist_len = 0;
			buffer = (uint8_t *)&u1005;
			break;
		case 1006:
		case 1007:
		default:
			break;
	}

	/* NetUserModalsSet */

	status = NetUserModalsSet(hostname,
				  level,
				  buffer,
				  &parm_err);
	if (status != 0) {
		printf("NetUserModalsSet failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
