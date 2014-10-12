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
#include "../winbind_client.h"

/* Convert a Windows SID to a Unix uid, allocating an uid if needed */
wbcErr wbcSidToUid(const struct wbcDomainSid *sid, uid_t *puid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!sid || !puid) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	wbcSidToStringBuf(sid, request.data.sid, sizeof(request.data.sid));

	/* Make request */

	wbc_status = wbcRequestResponse(WINBINDD_SID_TO_UID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*puid = response.data.uid;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

/* Convert a Windows SID to a Unix uid if there already is a mapping */
wbcErr wbcQuerySidToUid(const struct wbcDomainSid *sid,
			uid_t *puid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Convert a Unix uid to a Windows SID, allocating a SID if needed */
wbcErr wbcUidToSid(uid_t uid, struct wbcDomainSid *sid)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	if (!sid) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.uid = uid;

	/* Make request */

	wbc_status = wbcRequestResponse(WINBINDD_UID_TO_SID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	wbc_status = wbcStringToSid(response.data.sid.sid, sid);
	BAIL_ON_WBC_ERROR(wbc_status);

done:
	return wbc_status;
}

/* Convert a Unix uid to a Windows SID if there already is a mapping */
wbcErr wbcQueryUidToSid(uid_t uid,
			struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/** @brief Convert a Windows SID to a Unix gid, allocating a gid if needed
 *
 * @param *sid        Pointer to the domain SID to be resolved
 * @param *pgid       Pointer to the resolved gid_t value
 *
 * @return #wbcErr
 *
 **/

wbcErr wbcSidToGid(const struct wbcDomainSid *sid, gid_t *pgid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!sid || !pgid) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

        wbcSidToStringBuf(sid, request.data.sid, sizeof(request.data.sid));

	/* Make request */

	wbc_status = wbcRequestResponse(WINBINDD_SID_TO_GID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*pgid = response.data.gid;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}


/* Convert a Windows SID to a Unix gid if there already is a mapping */

wbcErr wbcQuerySidToGid(const struct wbcDomainSid *sid,
			gid_t *pgid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}


/* Convert a Unix gid to a Windows SID, allocating a SID if needed */
wbcErr wbcGidToSid(gid_t gid, struct wbcDomainSid *sid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!sid) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.gid = gid;

	/* Make request */

	wbc_status = wbcRequestResponse(WINBINDD_GID_TO_SID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	wbc_status = wbcStringToSid(response.data.sid.sid, sid);
	BAIL_ON_WBC_ERROR(wbc_status);

done:
	return wbc_status;
}

/* Convert a Unix gid to a Windows SID if there already is a mapping */
wbcErr wbcQueryGidToSid(gid_t gid,
			struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Obtain a new uid from Winbind */
wbcErr wbcAllocateUid(uid_t *puid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!puid)
		return WBC_ERR_INVALID_PARAM;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	wbc_status = wbcRequestResponsePriv(WINBINDD_ALLOCATE_UID,
					    &request, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Copy out result */
	*puid = response.data.uid;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

/* Obtain a new gid from Winbind */
wbcErr wbcAllocateGid(gid_t *pgid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (!pgid)
		return WBC_ERR_INVALID_PARAM;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	wbc_status = wbcRequestResponsePriv(WINBINDD_ALLOCATE_GID,
					    &request, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Copy out result */
	*pgid = response.data.gid;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

/* we can't include smb.h here... */
#define _ID_TYPE_UID 1
#define _ID_TYPE_GID 2

/* Set an user id mapping - not implemented any more */
wbcErr wbcSetUidMapping(uid_t uid, const struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Set a group id mapping - not implemented any more */
wbcErr wbcSetGidMapping(gid_t gid, const struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Remove a user id mapping - not implemented any more */
wbcErr wbcRemoveUidMapping(uid_t uid, const struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Remove a group id mapping - not implemented any more */
wbcErr wbcRemoveGidMapping(gid_t gid, const struct wbcDomainSid *sid)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Set the highwater mark for allocated uids - not implemented any more */
wbcErr wbcSetUidHwm(uid_t uid_hwm)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Set the highwater mark for allocated gids - not implemented any more */
wbcErr wbcSetGidHwm(gid_t gid_hwm)
{
	return WBC_ERR_NOT_IMPLEMENTED;
}

/* Convert a list of SIDs */
wbcErr wbcSidsToUnixIds(const struct wbcDomainSid *sids, uint32_t num_sids,
			struct wbcUnixId *ids)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int buflen, extra_len;
	uint32_t i;
	char *sidlist, *p, *extra_data;

	buflen = num_sids * (WBC_SID_STRING_BUFLEN + 1) + 1;

	sidlist = (char *)malloc(buflen);
	if (sidlist == NULL) {
		return WBC_ERR_NO_MEMORY;
	}

	p = sidlist;

	for (i=0; i<num_sids; i++) {
		int remaining;
		int len;

		remaining = buflen - (p - sidlist);

		len = wbcSidToStringBuf(&sids[i], p, remaining);
		if (len > remaining) {
			free(sidlist);
			return WBC_ERR_UNKNOWN_FAILURE;
		}

		p += len;
		*p++ = '\n';
	}
	*p++ = '\0';

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.extra_data.data = sidlist;
	request.extra_len = p - sidlist;

	wbc_status = wbcRequestResponse(WINBINDD_SIDS_TO_XIDS,
					&request, &response);
	free(sidlist);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return wbc_status;
	}

	extra_len = response.length - sizeof(struct winbindd_response);
	extra_data = (char *)response.extra_data.data;

	if ((extra_len <= 0) || (extra_data[extra_len-1] != '\0')) {
		goto wbc_err_invalid;
	}

	p = extra_data;

	for (i=0; i<num_sids; i++) {
		struct wbcUnixId *id = &ids[i];
		char *q;

		switch (p[0]) {
		case 'U':
			id->type = WBC_ID_TYPE_UID;
			id->id.uid = strtoul(p+1, &q, 10);
			break;
		case 'G':
			id->type = WBC_ID_TYPE_GID;
			id->id.gid = strtoul(p+1, &q, 10);
			break;
		default:
			id->type = WBC_ID_TYPE_NOT_SPECIFIED;
			q = p;
			break;
		};
		if (q[0] != '\n') {
			goto wbc_err_invalid;
		}
		p = q+1;
	}
	wbc_status = WBC_ERR_SUCCESS;
	goto done;

wbc_err_invalid:
	wbc_status = WBC_ERR_INVALID_RESPONSE;
done:
	winbindd_free_response(&response);
	return wbc_status;
}
