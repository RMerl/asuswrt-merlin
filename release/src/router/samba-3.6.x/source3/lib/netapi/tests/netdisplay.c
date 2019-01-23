/*
 *  Unix SMB/CIFS implementation.
 *  NetGroup testsuite
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

static NET_API_STATUS test_netquerydisplayinformation(const char *hostname,
						      uint32_t level,
						      const char *name)
{
	NET_API_STATUS status;
	uint32_t entries_read = 0;
	int found_name = 0;
	const char *current_name;
	uint8_t *buffer = NULL;
	uint32_t idx = 0;
	int i;

	struct NET_DISPLAY_USER *user;
	struct NET_DISPLAY_GROUP *group;
	struct NET_DISPLAY_MACHINE *machine;

	printf("testing NetQueryDisplayInformation level %d\n", level);

	do {
		status = NetQueryDisplayInformation(hostname,
						    level,
						    idx,
						    1000,
						    (uint32_t)-1,
						    &entries_read,
						    (void **)&buffer);
		if (status == 0 || status == ERROR_MORE_DATA) {
			switch (level) {
				case 1:
					user = (struct NET_DISPLAY_USER *)buffer;
					break;
				case 2:
					machine = (struct NET_DISPLAY_MACHINE *)buffer;
					break;
				case 3:
					group = (struct NET_DISPLAY_GROUP *)buffer;
					break;
				default:
					return -1;
			}

			for (i=0; i<entries_read; i++) {

				switch (level) {
					case 1:
						current_name = 	user->usri1_name;
						break;
					case 2:
						current_name = 	machine->usri2_name;
						break;
					case 3:
						current_name = 	group->grpi3_name;
						break;
					default:
						break;
				}

				if (name && strcasecmp(current_name, name) == 0) {
					found_name = 1;
				}

				switch (level) {
					case 1:
						user++;
						break;
					case 2:
						machine++;
						break;
					case 3:
						group++;
						break;
				}
			}
			NetApiBufferFree(buffer);
		}
		idx += entries_read;
	} while (status == ERROR_MORE_DATA);

	if (status) {
		return status;
	}

	if (name && !found_name) {
		printf("failed to get name\n");
		return -1;
	}

	return 0;
}

NET_API_STATUS netapitest_display(struct libnetapi_ctx *ctx,
				  const char *hostname)
{
	NET_API_STATUS status = 0;
	uint32_t levels[] = { 1, 2, 3};
	int i;

	printf("NetDisplay tests\n");

	/* test enum */

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		status = test_netquerydisplayinformation(hostname, levels[i], NULL);
		if (status) {
			NETAPI_STATUS(ctx, status, "NetQueryDisplayInformation");
			goto out;
		}
	}

	status = 0;

	printf("NetDisplay tests succeeded\n");
 out:
	if (status != 0) {
		printf("NetDisplay testsuite failed with: %s\n",
			libnetapi_get_error_string(ctx, status));
	}

	return status;
}
