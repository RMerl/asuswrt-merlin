/*
 *  Unix SMB/CIFS implementation.
 *  DsGetDcName query
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
	const char *domain = NULL;
	uint32_t flags = 0;
	struct DOMAIN_CONTROLLER_INFO *info = NULL;

	poptContext pc;
	int opt;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_LIBNETAPI_EXAMPLES
		{ "flags", 0, POPT_ARG_INT, NULL, 'f', "Query flags", "FLAGS" },
		POPT_TABLEEND
	};

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("dsgetdc", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname domainname");
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
			case 'f':
				sscanf(poptGetOptArg(pc), "%x", &flags);
		}
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
	domain = poptGetArg(pc);

	/* DsGetDcName */

	status = DsGetDcName(hostname, domain, NULL, NULL, flags, &info);
	if (status != 0) {
		printf("DsGetDcName failed with: %s\n",
			libnetapi_errstr(status));
		return status;
	}

	printf("DC Name:\t\t%s\n", info->domain_controller_name);
	printf("DC Address:\t\t%s\n", info->domain_controller_address);
	printf("DC Address Type:\t%d\n", info->domain_controller_address_type);
	printf("Domain Name:\t\t%s\n", info->domain_name);
	printf("DNS Forest Name:\t%s\n", info->dns_forest_name);
	printf("DC flags:\t\t0x%08x\n", info->flags);
	printf("DC Sitename:\t\t%s\n", info->dc_site_name);
	printf("Client Sitename:\t%s\n", info->client_site_name);

 out:
	NetApiBufferFree(info);
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
