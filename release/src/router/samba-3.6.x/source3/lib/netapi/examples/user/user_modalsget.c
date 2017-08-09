/*
 *  Unix SMB/CIFS implementation.
 *  NetUserModalsGet query
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
	char *sid_str = NULL;

	struct USER_MODALS_INFO_0 *u0;
	struct USER_MODALS_INFO_1 *u1;
	struct USER_MODALS_INFO_2 *u2;
	struct USER_MODALS_INFO_3 *u3;

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

	pc = poptGetContext("user_modalsget", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname level");
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

	/* NetUserModalsGet */

	status = NetUserModalsGet(hostname,
				  level,
				  &buffer);
	if (status != 0) {
		printf("NetUserModalsGet failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	switch (level) {
		case 0:
			u0 = (struct USER_MODALS_INFO_0 *)buffer;
			printf("min passwd len: %d character\n",
				u0->usrmod0_min_passwd_len);
			printf("max passwd age: %d (days)\n",
				u0->usrmod0_max_passwd_age/86400);
			printf("min passwd age: %d (days)\n",
				u0->usrmod0_min_passwd_age/86400);
			printf("force logoff: %d (seconds)\n",
				u0->usrmod0_force_logoff);
			printf("password history length: %d entries\n",
				u0->usrmod0_password_hist_len);
			break;
		case 1:
			u1 = (struct USER_MODALS_INFO_1 *)buffer;
			printf("role: %d\n", u1->usrmod1_role);
			printf("primary: %s\n", u1->usrmod1_primary);
			break;
		case 2:
			u2 = (struct USER_MODALS_INFO_2 *)buffer;
			printf("domain name: %s\n", u2->usrmod2_domain_name);
			if (ConvertSidToStringSid(u2->usrmod2_domain_id,
						  &sid_str)) {
				printf("domain sid: %s\n", sid_str);
				free(sid_str);
			}
			break;
		case 3:
			u3 = (struct USER_MODALS_INFO_3 *)buffer;
			printf("lockout duration: %d (seconds)\n",
				u3->usrmod3_lockout_duration);
			printf("lockout observation window: %d (seconds)\n",
				u3->usrmod3_lockout_observation_window);
			printf("lockout threshold: %d entries\n",
				u3->usrmod3_lockout_threshold);
			break;
		default:
			break;
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
