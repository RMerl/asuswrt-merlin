/*
 *  Unix SMB/CIFS implementation.
 *  NetUserEnum query
 *  Copyright (C) Guenther Deschner 2007
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
	uint32_t level = 0;
	uint8_t *buffer = NULL;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	char *sid_str = NULL;
	int i;

	struct USER_INFO_0 *info0 = NULL;
	struct USER_INFO_10 *info10 = NULL;
	struct USER_INFO_20 *info20 = NULL;
	struct USER_INFO_23 *info23 = NULL;

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

	pc = poptGetContext("user_enum", argc, argv, long_options, 0);

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

	/* NetUserEnum */

	do {
		status = NetUserEnum(hostname,
				     level,
				     FILTER_NORMAL_ACCOUNT,
				     &buffer,
				     (uint32_t)-1,
				     &entries_read,
				     &total_entries,
				     &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {

			switch (level) {
				case 0:
					info0 = (struct USER_INFO_0 *)buffer;
					break;
				case 10:
					info10 = (struct USER_INFO_10 *)buffer;
					break;
				case 20:
					info20 = (struct USER_INFO_20 *)buffer;
					break;
				case 23:
					info23 = (struct USER_INFO_23 *)buffer;
					break;
				default:
					break;
			}

			for (i=0; i<entries_read; i++) {
				switch (level) {
					case 0:
						printf("#%d user: %s\n", i, info0->usri0_name);
						info0++;
						break;
					case 10:
						printf("#%d user: %s\n", i, info10->usri10_name);
						printf("#%d comment: %s\n", i, info10->usri10_comment);
						printf("#%d usr_comment: %s\n", i, info10->usri10_usr_comment);
						printf("#%d full_name: %s\n", i, info10->usri10_full_name);
						info10++;
						break;
					case 20:
						printf("#%d user: %s\n", i, info20->usri20_name);
						printf("#%d comment: %s\n", i, info20->usri20_comment);
						printf("#%d flags: 0x%08x\n", i, info20->usri20_flags);
						printf("#%d rid: %d\n", i, info20->usri20_user_id);
						info20++;
						break;
					case 23:
						printf("#%d user: %s\n", i, info23->usri23_name);
						printf("#%d comment: %s\n", i, info23->usri23_comment);
						printf("#%d flags: 0x%08x\n", i, info23->usri23_flags);
						if (ConvertSidToStringSid(info23->usri23_user_sid,
									  &sid_str)) {
							printf("#%d sid: %s\n", i, sid_str);
							free(sid_str);
						}
						info23++;
						break;
					default:
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status != 0) {
		printf("NetUserEnum failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
