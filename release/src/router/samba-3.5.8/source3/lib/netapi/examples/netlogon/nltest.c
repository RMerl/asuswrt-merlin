/*
 * Samba Unix/Linux SMB client library
 * Distributed SMB/CIFS Server Management Utility
 * Nltest netlogon testing tool
 *
 * Copyright (C) Guenther Deschner 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netapi.h>

#include "common.h"

enum {
	OPT_DBFLAG = 1,
	OPT_SC_QUERY,
	OPT_SC_RESET,
	OPT_SC_VERIFY,
	OPT_SC_CHANGE_PWD
};

/****************************************************************
****************************************************************/

static void print_result(uint32_t level,
			 uint8_t *buffer)
{
	struct NETLOGON_INFO_1 *i1 = NULL;
	struct NETLOGON_INFO_2 *i2 = NULL;
	struct NETLOGON_INFO_3 *i3 = NULL;
	struct NETLOGON_INFO_4 *i4 = NULL;

	if (!buffer) {
		return;
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
}

/****************************************************************
****************************************************************/

int main(int argc, const char **argv)
{
	int opt;
	NET_API_STATUS status;
	struct libnetapi_ctx *ctx = NULL;
	const char *server_name = NULL;
	char *opt_domain = NULL;
	int opt_dbflag = 0;
	uint32_t query_level;
	uint8_t *buffer = NULL;

	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"dbflag", 0, POPT_ARG_INT,   &opt_dbflag, OPT_DBFLAG, "New Debug Flag", "HEXFLAGS"},
		{"sc_query", 0, POPT_ARG_STRING,   &opt_domain, OPT_SC_QUERY, "Query secure channel for domain on server", "DOMAIN"},
		{"sc_reset", 0, POPT_ARG_STRING,   &opt_domain, OPT_SC_RESET, "Reset secure channel for domain on server to dcname", "DOMAIN"},
		{"sc_verify", 0, POPT_ARG_STRING,   &opt_domain, OPT_SC_VERIFY, "Verify secure channel for domain on server", "DOMAIN"},
		{"sc_change_pwd", 0, POPT_ARG_STRING,   &opt_domain, OPT_SC_CHANGE_PWD, "Change a secure channel password for domain on server", "DOMAIN"},
		POPT_COMMON_LIBNETAPI_EXAMPLES
		POPT_TABLEEND
	};

	status = libnetapi_init(&ctx);
	if (status != 0) {
		return status;
	}

	pc = poptGetContext("nltest", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "server_name");
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (!poptPeekArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto done;
	}
	server_name = poptGetArg(pc);

	if (argc == 1) {
		poptPrintHelp(pc, stderr, 0);
		goto done;
	}

	if (!server_name || poptGetArg(pc)) {
		poptPrintHelp(pc, stderr, 0);
		goto done;
	}

	if ((server_name[0] == '/' && server_name[1] == '/') ||
	    (server_name[0] == '\\' && server_name[1] ==  '\\')) {
		server_name += 2;
	}

	poptResetContext(pc);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {

		case OPT_DBFLAG:
			query_level = 1;
			status = I_NetLogonControl2(server_name,
						    NETLOGON_CONTROL_SET_DBFLAG,
						    query_level,
						    (uint8_t *)opt_dbflag,
						    &buffer);
			if (status != 0) {
				fprintf(stderr, "I_NetlogonControl failed: Status = %d 0x%x %s\n",
					status, status,
					libnetapi_get_error_string(ctx, status));
				goto done;
			}
			break;
		case OPT_SC_QUERY:
			query_level = 2;
			status = I_NetLogonControl2(server_name,
						    NETLOGON_CONTROL_TC_QUERY,
						    query_level,
						    (uint8_t *)opt_domain,
						    &buffer);
			if (status != 0) {
				fprintf(stderr, "I_NetlogonControl failed: Status = %d 0x%x %s\n",
					status, status,
					libnetapi_get_error_string(ctx, status));
				goto done;
			}
			break;
		case OPT_SC_VERIFY:
			query_level = 2;
			status = I_NetLogonControl2(server_name,
						    NETLOGON_CONTROL_TC_VERIFY,
						    query_level,
						    (uint8_t *)opt_domain,
						    &buffer);
			if (status != 0) {
				fprintf(stderr, "I_NetlogonControl failed: Status = %d 0x%x %s\n",
					status, status,
					libnetapi_get_error_string(ctx, status));
				goto done;
			}
			break;
		case OPT_SC_RESET:
			query_level = 2;
			status = I_NetLogonControl2(server_name,
						    NETLOGON_CONTROL_REDISCOVER,
						    query_level,
						    (uint8_t *)opt_domain,
						    &buffer);
			if (status != 0) {
				fprintf(stderr, "I_NetlogonControl failed: Status = %d 0x%x %s\n",
					status, status,
					libnetapi_get_error_string(ctx, status));
				goto done;
			}
			break;
		case OPT_SC_CHANGE_PWD:
			query_level = 1;
			status = I_NetLogonControl2(server_name,
						    NETLOGON_CONTROL_CHANGE_PASSWORD,
						    query_level,
						    (uint8_t *)opt_domain,
						    &buffer);
			if (status != 0) {
				fprintf(stderr, "I_NetlogonControl failed: Status = %d 0x%x %s\n",
					status, status,
					libnetapi_get_error_string(ctx, status));
				goto done;
			}
			break;
		default:
			poptPrintHelp(pc, stderr, 0);
			goto done;
		}
	}

	print_result(query_level, buffer);

	printf("The command completed successfully\n");
	status = 0;

 done:

	printf("\n");
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
