/*
   Unix SMB/CIFS implementation.

   Winbind client API

   Copyright (C) Gerald (Jerry) Carter 2007


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Required Headers */

#include "replace.h"
#include "libwbclient.h"

/* Convert a binary GUID to a character string */
wbcErr wbcGuidToString(const struct wbcGuid *guid,
		       char **guid_string)
{
	char *result;

	result = (char *)wbcAllocateMemory(37, 1, NULL);
	if (result == NULL) {
		return WBC_ERR_NO_MEMORY;
	}
	snprintf(result, 37,
		 "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		 guid->time_low, guid->time_mid,
		 guid->time_hi_and_version,
		 guid->clock_seq[0],
		 guid->clock_seq[1],
		 guid->node[0], guid->node[1],
		 guid->node[2], guid->node[3],
		 guid->node[4], guid->node[5]);
	*guid_string = result;

	return WBC_ERR_SUCCESS;
}

/* @brief Convert a character string to a binary GUID */
wbcErr wbcStringToGuid(const char *str,
		       struct wbcGuid *guid)
{
	uint32_t time_low;
	uint32_t time_mid, time_hi_and_version;
	uint32_t clock_seq[2];
	uint32_t node[6];
	int i;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!guid) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (!str) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (11 == sscanf(str, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 &time_low, &time_mid, &time_hi_and_version,
			 &clock_seq[0], &clock_seq[1],
			 &node[0], &node[1], &node[2], &node[3], &node[4], &node[5])) {
	        wbc_status = WBC_ERR_SUCCESS;
	} else if (11 == sscanf(str, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
				&time_low, &time_mid, &time_hi_and_version,
				&clock_seq[0], &clock_seq[1],
				&node[0], &node[1], &node[2], &node[3], &node[4], &node[5])) {
	        wbc_status = WBC_ERR_SUCCESS;
	}

	BAIL_ON_WBC_ERROR(wbc_status);

	guid->time_low = time_low;
	guid->time_mid = time_mid;
	guid->time_hi_and_version = time_hi_and_version;
	guid->clock_seq[0] = clock_seq[0];
	guid->clock_seq[1] = clock_seq[1];

	for (i=0;i<6;i++) {
		guid->node[i] = node[i];
	}

	wbc_status = WBC_ERR_SUCCESS;

done:
	return wbc_status;
}
