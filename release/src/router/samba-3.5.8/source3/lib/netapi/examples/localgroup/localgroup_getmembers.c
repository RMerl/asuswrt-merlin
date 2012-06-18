/*
 *  Unix SMB/CIFS implementation.
 *  NetLocalGroupGetMembers query
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
	const char *groupname = NULL;
	uint32_t level = 0;
	uint8_t *buffer = NULL;
	uint32_t entries_read = 0;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	char *sid_str = NULL;
	int i;

	struct LOCALGROUP_MEMBERS_INFO_0 *info0 = NULL;
	struct LOCALGROUP_MEMBERS_INFO_1 *info1 = NULL;
	struct LOCALGROUP_MEMBERS_INFO_2 *info2 = NULL;
	struct LOCALGROUP_MEMBERS_INFO_3 *info3 = NULL;

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

	pc = poptGetContext("localgroup_getmembers", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "hostname groupname level");
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
	groupname = poptGetArg(pc);

	if (poptPeekArg(pc)) {
		level = atoi(poptGetArg(pc));
	}

	/* NetLocalGroupGetMembers */

	do {
		status = NetLocalGroupGetMembers(hostname,
						 groupname,
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
					info0 = (struct LOCALGROUP_MEMBERS_INFO_0 *)buffer;
					break;
				case 1:
					info1 = (struct LOCALGROUP_MEMBERS_INFO_1 *)buffer;
					break;
				case 2:
					info2 = (struct LOCALGROUP_MEMBERS_INFO_2 *)buffer;
					break;
				case 3:
					info3 = (struct LOCALGROUP_MEMBERS_INFO_3 *)buffer;
					break;
				default:
					break;
			}
			for (i=0; i<entries_read; i++) {
				switch (level) {
					case 0:
						if (ConvertSidToStringSid(info0->lgrmi0_sid,
									  &sid_str)) {
							printf("#%d member sid: %s\n", i, sid_str);
							free(sid_str);
						}
						info0++;
						break;
					case 1:
						if (ConvertSidToStringSid(info1->lgrmi1_sid,
									  &sid_str)) {
							printf("#%d member sid: %s\n", i, sid_str);
							free(sid_str);
						}
						printf("#%d sid type: %d\n", i, info1->lgrmi1_sidusage);
						printf("#%d name: %s\n", i, info1->lgrmi1_name);
						info1++;
						break;
					case 2:
						if (ConvertSidToStringSid(info2->lgrmi2_sid,
									  &sid_str)) {
							printf("#%d member sid: %s\n", i, sid_str);
							free(sid_str);
						}
						printf("#%d sid type: %d\n", i, info2->lgrmi2_sidusage);
						printf("#%d full name: %s\n", i, info2->lgrmi2_domainandname);
						info2++;
						break;
					case 3:
						printf("#%d full name: %s\n", i, info3->lgrmi3_domainandname);
						info3++;
						break;
					default:
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
	} while (status == ERROR_MORE_DATA);

	if (status != 0) {
		printf("NetLocalGroupGetMembers failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

 out:
	libnetapi_free(ctx);
	poptFreeContext(pc);

	return status;
}
