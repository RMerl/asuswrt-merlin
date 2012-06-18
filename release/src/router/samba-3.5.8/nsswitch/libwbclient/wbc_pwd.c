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

/** @brief The maximum number of pwent structs to get from winbindd
 *
 */
#define MAX_GETPWENT_USERS 500

/** @brief The maximum number of grent structs to get from winbindd
 *
 */
#define MAX_GETGRENT_GROUPS 500

/**
 *
 **/

static struct passwd *copy_passwd_entry(struct winbindd_pw *p)
{
	struct passwd *pwd = NULL;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	pwd = talloc(NULL, struct passwd);
	BAIL_ON_PTR_ERROR(pwd, wbc_status);

	pwd->pw_name = talloc_strdup(pwd,p->pw_name);
	BAIL_ON_PTR_ERROR(pwd->pw_name, wbc_status);

	pwd->pw_passwd = talloc_strdup(pwd, p->pw_passwd);
	BAIL_ON_PTR_ERROR(pwd->pw_passwd, wbc_status);

	pwd->pw_gecos = talloc_strdup(pwd, p->pw_gecos);
	BAIL_ON_PTR_ERROR(pwd->pw_gecos, wbc_status);

	pwd->pw_shell = talloc_strdup(pwd, p->pw_shell);
	BAIL_ON_PTR_ERROR(pwd->pw_shell, wbc_status);

	pwd->pw_dir = talloc_strdup(pwd, p->pw_dir);
	BAIL_ON_PTR_ERROR(pwd->pw_dir, wbc_status);

	pwd->pw_uid = p->pw_uid;
	pwd->pw_gid = p->pw_gid;

done:
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		talloc_free(pwd);
		pwd = NULL;
	}

	return pwd;
}

/**
 *
 **/

static struct group *copy_group_entry(struct winbindd_gr *g,
				      char *mem_buf)
{
	struct group *grp = NULL;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int i;
	char *mem_p, *mem_q;

	grp = talloc(NULL, struct group);
	BAIL_ON_PTR_ERROR(grp, wbc_status);

	grp->gr_name = talloc_strdup(grp, g->gr_name);
	BAIL_ON_PTR_ERROR(grp->gr_name, wbc_status);

	grp->gr_passwd = talloc_strdup(grp, g->gr_passwd);
	BAIL_ON_PTR_ERROR(grp->gr_passwd, wbc_status);

	grp->gr_gid = g->gr_gid;

	grp->gr_mem = talloc_array(grp, char*, g->num_gr_mem+1);

	mem_p = mem_q = mem_buf;
	for (i=0; i<g->num_gr_mem && mem_p; i++) {
		if ((mem_q = strchr(mem_p, ',')) != NULL) {
			*mem_q = '\0';
		}

		grp->gr_mem[i] = talloc_strdup(grp, mem_p);
		BAIL_ON_PTR_ERROR(grp->gr_mem[i], wbc_status);

		if (mem_q == NULL) {
			i += 1;
			break;
		}
		mem_p = mem_q + 1;
	}
	grp->gr_mem[i] = NULL;

	wbc_status = WBC_ERR_SUCCESS;

done:
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		talloc_free(grp);
		grp = NULL;
	}

	return grp;
}

/* Fill in a struct passwd* for a domain user based on username */
wbcErr wbcGetpwnam(const char *name, struct passwd **pwd)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	if (!name || !pwd) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* dst is already null terminated from the memset above */

	strncpy(request.data.username, name, sizeof(request.data.username)-1);

	wbc_status = wbcRequestResponse(WINBINDD_GETPWNAM,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*pwd = copy_passwd_entry(&response.data.pw);
	BAIL_ON_PTR_ERROR(*pwd, wbc_status);

 done:
	return wbc_status;
}

/* Fill in a struct passwd* for a domain user based on uid */
wbcErr wbcGetpwuid(uid_t uid, struct passwd **pwd)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	if (!pwd) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.uid = uid;

	wbc_status = wbcRequestResponse(WINBINDD_GETPWUID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*pwd = copy_passwd_entry(&response.data.pw);
	BAIL_ON_PTR_ERROR(*pwd, wbc_status);

 done:
	return wbc_status;
}

/* Fill in a struct passwd* for a domain user based on sid */
wbcErr wbcGetpwsid(struct wbcDomainSid *sid, struct passwd **pwd)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;
	char * sid_string = NULL;

	if (!pwd) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	wbc_status = wbcSidToString(sid, &sid_string);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.sid, sid_string, sizeof(request.data.sid));

	wbc_status = wbcRequestResponse(WINBINDD_GETPWSID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*pwd = copy_passwd_entry(&response.data.pw);
	BAIL_ON_PTR_ERROR(*pwd, wbc_status);

 done:
	if (sid_string) {
		wbcFreeMemory(sid_string);
	}

	return wbc_status;
}

/* Fill in a struct passwd* for a domain user based on username */
wbcErr wbcGetgrnam(const char *name, struct group **grp)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (!name || !grp) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* dst is already null terminated from the memset above */

	strncpy(request.data.groupname, name, sizeof(request.data.groupname)-1);

	wbc_status = wbcRequestResponse(WINBINDD_GETGRNAM,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*grp = copy_group_entry(&response.data.gr,
				(char*)response.extra_data.data);
	BAIL_ON_PTR_ERROR(*grp, wbc_status);

 done:
	if (response.extra_data.data)
		free(response.extra_data.data);

	return wbc_status;
}

/* Fill in a struct passwd* for a domain user based on uid */
wbcErr wbcGetgrgid(gid_t gid, struct group **grp)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (!grp) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	request.data.gid = gid;

	wbc_status = wbcRequestResponse(WINBINDD_GETGRGID,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	*grp = copy_group_entry(&response.data.gr,
				(char*)response.extra_data.data);
	BAIL_ON_PTR_ERROR(*grp, wbc_status);

 done:
	if (response.extra_data.data)
		free(response.extra_data.data);

	return wbc_status;
}

/** @brief Number of cached passwd structs
 *
 */
static uint32_t pw_cache_size;

/** @brief Position of the pwent context
 *
 */
static uint32_t pw_cache_idx;

/** @brief Winbindd response containing the passwd structs
 *
 */
static struct winbindd_response pw_response;

/* Reset the passwd iterator */
wbcErr wbcSetpwent(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (pw_cache_size > 0) {
		pw_cache_idx = pw_cache_size = 0;
		if (pw_response.extra_data.data) {
			free(pw_response.extra_data.data);
		}
	}

	ZERO_STRUCT(pw_response);

	wbc_status = wbcRequestResponse(WINBINDD_SETPWENT,
					NULL, NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Close the passwd iterator */
wbcErr wbcEndpwent(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (pw_cache_size > 0) {
		pw_cache_idx = pw_cache_size = 0;
		if (pw_response.extra_data.data) {
			free(pw_response.extra_data.data);
		}
	}

	wbc_status = wbcRequestResponse(WINBINDD_ENDPWENT,
					NULL, NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Return the next struct passwd* entry from the pwent iterator */
wbcErr wbcGetpwent(struct passwd **pwd)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_pw *wb_pw;

	/* If there's a cached result, return that. */
	if (pw_cache_idx < pw_cache_size) {
		goto return_result;
	}

	/* Otherwise, query winbindd for some entries. */

	pw_cache_idx = 0;

	if (pw_response.extra_data.data) {
		free(pw_response.extra_data.data);
		ZERO_STRUCT(pw_response);
	}

	ZERO_STRUCT(request);
	request.data.num_entries = MAX_GETPWENT_USERS;

	wbc_status = wbcRequestResponse(WINBINDD_GETPWENT, &request,
					&pw_response);

	BAIL_ON_WBC_ERROR(wbc_status);

	pw_cache_size = pw_response.data.num_entries;

return_result:

	wb_pw = (struct winbindd_pw *) pw_response.extra_data.data;

	*pwd = copy_passwd_entry(&wb_pw[pw_cache_idx]);

	BAIL_ON_PTR_ERROR(*pwd, wbc_status);

	pw_cache_idx++;

done:
	return wbc_status;
}

/** @brief Number of cached group structs
 *
 */
static uint32_t gr_cache_size;

/** @brief Position of the grent context
 *
 */
static uint32_t gr_cache_idx;

/** @brief Winbindd response containing the group structs
 *
 */
static struct winbindd_response gr_response;

/* Reset the group iterator */
wbcErr wbcSetgrent(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (gr_cache_size > 0) {
		gr_cache_idx = gr_cache_size = 0;
		if (gr_response.extra_data.data) {
			free(gr_response.extra_data.data);
		}
	}

	ZERO_STRUCT(gr_response);

	wbc_status = wbcRequestResponse(WINBINDD_SETGRENT,
					NULL, NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Close the group iterator */
wbcErr wbcEndgrent(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (gr_cache_size > 0) {
		gr_cache_idx = gr_cache_size = 0;
		if (gr_response.extra_data.data) {
			free(gr_response.extra_data.data);
		}
	}

	wbc_status = wbcRequestResponse(WINBINDD_ENDGRENT,
					NULL, NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Return the next struct group* entry from the pwent iterator */
wbcErr wbcGetgrent(struct group **grp)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_gr *wb_gr;
	uint32_t mem_ofs;

	/* If there's a cached result, return that. */
	if (gr_cache_idx < gr_cache_size) {
		goto return_result;
	}

	/* Otherwise, query winbindd for some entries. */

	gr_cache_idx = 0;

	if (gr_response.extra_data.data) {
		free(gr_response.extra_data.data);
		ZERO_STRUCT(gr_response);
	}

	ZERO_STRUCT(request);
	request.data.num_entries = MAX_GETGRENT_GROUPS;

	wbc_status = wbcRequestResponse(WINBINDD_GETGRENT, &request,
					&gr_response);

	BAIL_ON_WBC_ERROR(wbc_status);

	gr_cache_size = gr_response.data.num_entries;

return_result:

	wb_gr = (struct winbindd_gr *) gr_response.extra_data.data;

	mem_ofs = wb_gr[gr_cache_idx].gr_mem_ofs +
		  gr_cache_size * sizeof(struct winbindd_gr);

	*grp = copy_group_entry(&wb_gr[gr_cache_idx],
				((char *)gr_response.extra_data.data)+mem_ofs);

	BAIL_ON_PTR_ERROR(*grp, wbc_status);

	gr_cache_idx++;

done:
	return wbc_status;
}

/* Return the next struct group* entry from the pwent iterator */
wbcErr wbcGetgrlist(struct group **grp)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_gr *wb_gr;

	/* If there's a cached result, return that. */
	if (gr_cache_idx < gr_cache_size) {
		goto return_result;
	}

	/* Otherwise, query winbindd for some entries. */

	gr_cache_idx = 0;

	if (gr_response.extra_data.data) {
		free(gr_response.extra_data.data);
		ZERO_STRUCT(gr_response);
	}

	ZERO_STRUCT(request);
	request.data.num_entries = MAX_GETGRENT_GROUPS;

	wbc_status = wbcRequestResponse(WINBINDD_GETGRLST, &request,
					&gr_response);

	BAIL_ON_WBC_ERROR(wbc_status);

	gr_cache_size = gr_response.data.num_entries;

return_result:

	wb_gr = (struct winbindd_gr *) gr_response.extra_data.data;

	*grp = copy_group_entry(&wb_gr[gr_cache_idx], NULL);

	BAIL_ON_PTR_ERROR(*grp, wbc_status);

	gr_cache_idx++;

done:
	return wbc_status;
}

/* Return the unix group array belonging to the given user */
wbcErr wbcGetGroups(const char *account,
		    uint32_t *num_groups,
		    gid_t **_groups)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;
	uint32_t i;
	gid_t *groups = NULL;

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (!account) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Send request */

	strncpy(request.data.username, account, sizeof(request.data.username)-1);

	wbc_status = wbcRequestResponse(WINBINDD_GETGROUPS,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	groups = talloc_array(NULL, gid_t, response.data.num_entries);
	BAIL_ON_PTR_ERROR(groups, wbc_status);

	for (i = 0; i < response.data.num_entries; i++) {
		groups[i] = ((gid_t *)response.extra_data.data)[i];
	}

	*num_groups = response.data.num_entries;
	*_groups = groups;
	groups = NULL;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	if (response.extra_data.data) {
		free(response.extra_data.data);
	}
	if (groups) {
		talloc_free(groups);
	}

	return wbc_status;
}
