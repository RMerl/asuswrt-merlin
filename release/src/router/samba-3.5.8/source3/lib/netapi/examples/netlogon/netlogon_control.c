/*
 *  Unix SMB/CIFS implementation.
 *  I_NetLogonControl query
 *  Copyright (C) Guenther Deschner 2009
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
	uint32_t function_code = NETLOGON_CONTROL_QUERY;
	uint32_t level = 1;
	uint8_t *buffer = NULL;
	struct NETLOGON_INFO_1 *i1 = NULL;
	struct NETLOGON_INFO_2 *i2 = NULL;
	struct NETLOGON_INFO_3 *i3 = NULL;
	struct NETLOGON_INFO_4 *i4 = NULL;

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

	pc = poptGetContext("netlogon_control", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto out;
	}
	hostname = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		function_code = atoi(poptGetArg(pc));
	}

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* I_NetLogonControl */

	status = I_NetLogonControl(hostname,
				   function_code,
				   level,
				   &buffer);
	if (status != 0) {
		printf("I_NetLogonControl failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
		goto out;
	}

	if (!buffer) {
		goto out;
	}

	switch (level) {
	case 1:
		i1 = (struct NETLOGON_INFO_1 *)buffer;

		printf("Flags: %x\n", i1->netlog1_flags);
		printf("Connection Status Status = %d 0x%x %s\n",
			i1->netlog1_pdc_connection_status,
			i1->netlog1_pdc_connection_status,
			libnetapi_errstr(i1->netlog1_pdc_connection_status));

		break;
	case 2:
		i2 = (struct NETLOGON_INFO_2 *)buffer;

		printf("Flags: %x\n", i2->netlog2_flags);
		printf("Trusted DC Name %s\n", i2->netlog2_trusted_dc_name);
		printf("Trusted DC Connection Status Status = %d 0x%x %s\n",
			i2->netlog2_tc_connection_status,
			i2->netlog2_tc_connection_status,
			libnetapi_errstr(i2->netlog2_tc_connection_status));
		printf("Trust Verification Status Status = %d 0x%x %s\n",
			i2->netlog2_pdc_connection_status,
			i2->netlog2_pdc_connection_status,
			libnetapi_errstr(i2->netlog2_pdc_connection_status));

		break;
	case 3:
		i3 = (struct NETLOGON_INFO_3 *)buffer;

		printf("Flags: %x\n", i3->netlog1_flags);
		printf("Logon Attempts: %d\n", i3->netlog3_logon_attempts);

		break;
	case 4:
		i4 = (struct NETLOGON_INFO_4 *)buffer;

		printf("Trusted DC Name %s\n", i4->netlog4_trusted_dc_name);
		printf("Trusted Domain Name %s\n", i4->netlog4_trusted_domain_name);

		break;
	default:
		break;
	}

 out:
	NetApiBufferFree(buffer);
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
