/*
 *  Unix SMB/CIFS implementation.
 *  Join Support (cmdline + netapi)
 *  Copyright (C) Guenther Deschner 2007-2008
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

enum {
	OPT_OU = 1000
};

int main(int argc, const char **argv)
{
	NET_API_STATUS status;
	const char *host_name = NULL;
	const char *domain_name = NULL;
	const char *account_ou = NULL;
	const char *account = NULL;
	const char *password = NULL;
	uint32_t join_flags = NETSETUP_JOIN_DOMAIN |
			      NETSETUP_ACCT_CREATE |
			      NETSETUP_DOMAIN_JOIN_IF_JOINED;
	struct libnetapi_ctx *ctx = NULL;

	poptContext pc;
	int opt;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "ou", 0, POPT_ARG_STRING, &account_ou, 'U', "Account ou", "ACCOUNT_OU" },
		{ "domain", 0, POPT_ARG_STRING, &domain_name, 'U', "Domain name (required)", "DOMAIN" },
		{ "userd", 0, POPT_ARG_STRING, &account, 'U', "Domain admin account", "USERNAME" },
		{ "passwordd", 0, POPT_ARG_STRING, &password, 'U', "Domain admin password", "PASSWORD" },
		POPT_COMMON_LIBNETAPI_EXAMPLES
		POPT_TABLEEND
	};


	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("netdomjoin", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	host_name = poptGetArg(pc);

	if (!domain_name) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}

	/* NetJoinDomain */

	status = NetJoinDomain(host_name,
			       domain_name,
			       account_ou,
			       account,
			       password,
			       join_flags);
	if (status != 0) {
		const char *errstr = NULL;
		errstr = libnetapi_get_error_string(ctx, status);
		printf("Join failed with: %s\n", errstr);
	} else {
		printf("Successfully joined\n");
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
