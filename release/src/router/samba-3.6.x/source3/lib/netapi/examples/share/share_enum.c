/*
 *  Unix SMB/CIFS implementation.
 *  NetShareEnum query
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
	uint32_t level = 0;
	uint8_t *buffer = NULL;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	int i;

	struct SHARE_INFO_0 *i0 = NULL;
	struct SHARE_INFO_1 *i1 = NULL;
	struct SHARE_INFO_2 *i2 = NULL;

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

	pc = poptGetContext("share_enum", argc, argv, long_options, 0);

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

	/* NetShareEnum */

	do {
		status = NetShareEnum(hostname,
				      level,
				      &buffer,
				      (uint32_t)-1,
				      &entries_read,
				      &total_entries,
				      &resume_handle);
		if (status == 0 || status == ERROR_MORE_DATA) {
			printf("total entries: %d\n", total_entries);
			switch (level) {
				case 0:
					i0 = (struct SHARE_INFO_0 *)buffer;
					break;
				case 1:
					i1 = (struct SHARE_INFO_1 *)buffer;
					break;
				case 2:
					i2 = (struct SHARE_INFO_2 *)buffer;
					break;
				default:
					break;
			}
			for (i=0; i<entries_read; i++) {
				switch (level) {
					case 0:
						printf("#%d netname: %s\n", i, i0->shi0_netname);
						i0++;
						break;
					case 1:
						printf("#%d netname: %s\n", i, i1->shi1_netname);
						printf("#%d type: %d\n", i, i1->shi1_type);
						printf("#%d remark: %s\n", i, i1->shi1_remark);
						i1++;
						break;
					case 2:
						printf("#%d netname: %s\n", i, i2->shi2_netname);
						printf("#%d type: %d\n", i, i2->shi2_type);
						printf("#%d remark: %s\n", i, i2->shi2_remark);
						printf("#%d permissions: %d\n", i, i2->shi2_permissions);
						printf("#%d max users: %d\n", i, i2->shi2_max_uses);
						printf("#%d current users: %d\n", i, i2->shi2_current_uses);
						printf("#%d path: %s\n", i, i2->shi2_path);
						printf("#%d password: %s\n", i, i2->shi2_passwd);
						i2++;
						break;
					default:
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status != 0) {
		printf("NetShareEnum failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
