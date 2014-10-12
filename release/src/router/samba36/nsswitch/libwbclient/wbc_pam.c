/*
   Unix SMB/CIFS implementation.

   Winbind client API

   Copyright (C) Gerald (Jerry) Carter 2007
   Copyright (C) Guenther Deschner 2008
   Copyright (C) Volker Lendecke 2009

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

/* Authenticate a username/password pair */
wbcErr wbcAuthenticateUser(const char *username,
			   const char *password)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcAuthUserParams params;

	ZERO_STRUCT(params);

	params.account_name		= username;
	params.level			= WBC_AUTH_USER_LEVEL_PLAIN;
	params.password.plaintext	= password;

	wbc_status = wbcAuthenticateUserEx(&params, NULL, NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

done:
	return wbc_status;
}

static bool sid_attr_compose(struct wbcSidWithAttr *s,
			     const struct wbcDomainSid *d,
			     uint32_t rid, uint32_t attr)
{
	if (d->num_auths >= WBC_MAXSUBAUTHS) {
		return false;
	}
	s->sid = *d;
	s->sid.sub_auths[s->sid.num_auths++] = rid;
	s->attributes = attr;
	return true;
}

static void wbcAuthUserInfoDestructor(void *ptr)
{
	struct wbcAuthUserInfo *i = (struct wbcAuthUserInfo *)ptr;
	free(i->account_name);
	free(i->user_principal);
	free(i->full_name);
	free(i->domain_name);
	free(i->dns_domain_name);
	free(i->logon_server);
	free(i->logon_script);
	free(i->profile_path);
	free(i->home_directory);
	free(i->home_drive);
	free(i->sids);
}

static wbcErr wbc_create_auth_info(const struct winbindd_response *resp,
				   struct wbcAuthUserInfo **_i)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcAuthUserInfo *i;
	struct wbcDomainSid domain_sid;
	char *p;
	uint32_t sn = 0;
	uint32_t j;

	i = (struct wbcAuthUserInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcAuthUserInfo),
		wbcAuthUserInfoDestructor);
	BAIL_ON_PTR_ERROR(i, wbc_status);

	i->user_flags	= resp->data.auth.info3.user_flgs;

	i->account_name	= strdup(resp->data.auth.info3.user_name);
	BAIL_ON_PTR_ERROR(i->account_name, wbc_status);
	i->user_principal= NULL;
	i->full_name	= strdup(resp->data.auth.info3.full_name);
	BAIL_ON_PTR_ERROR(i->full_name, wbc_status);
	i->domain_name	= strdup(resp->data.auth.info3.logon_dom);
	BAIL_ON_PTR_ERROR(i->domain_name, wbc_status);
	i->dns_domain_name= NULL;

	i->acct_flags	= resp->data.auth.info3.acct_flags;
	memcpy(i->user_session_key,
	       resp->data.auth.user_session_key,
	       sizeof(i->user_session_key));
	memcpy(i->lm_session_key,
	       resp->data.auth.first_8_lm_hash,
	       sizeof(i->lm_session_key));

	i->logon_count		= resp->data.auth.info3.logon_count;
	i->bad_password_count	= resp->data.auth.info3.bad_pw_count;

	i->logon_time		= resp->data.auth.info3.logon_time;
	i->logoff_time		= resp->data.auth.info3.logoff_time;
	i->kickoff_time		= resp->data.auth.info3.kickoff_time;
	i->pass_last_set_time	= resp->data.auth.info3.pass_last_set_time;
	i->pass_can_change_time	= resp->data.auth.info3.pass_can_change_time;
	i->pass_must_change_time= resp->data.auth.info3.pass_must_change_time;

	i->logon_server	= strdup(resp->data.auth.info3.logon_srv);
	BAIL_ON_PTR_ERROR(i->logon_server, wbc_status);
	i->logon_script	= strdup(resp->data.auth.info3.logon_script);
	BAIL_ON_PTR_ERROR(i->logon_script, wbc_status);
	i->profile_path	= strdup(resp->data.auth.info3.profile_path);
	BAIL_ON_PTR_ERROR(i->profile_path, wbc_status);
	i->home_directory= strdup(resp->data.auth.info3.home_dir);
	BAIL_ON_PTR_ERROR(i->home_directory, wbc_status);
	i->home_drive	= strdup(resp->data.auth.info3.dir_drive);
	BAIL_ON_PTR_ERROR(i->home_drive, wbc_status);

	i->num_sids	= 2;
	i->num_sids 	+= resp->data.auth.info3.num_groups;
	i->num_sids	+= resp->data.auth.info3.num_other_sids;

	i->sids	= (struct wbcSidWithAttr *)calloc(
		sizeof(struct wbcSidWithAttr), i->num_sids);
	BAIL_ON_PTR_ERROR(i->sids, wbc_status);

	wbc_status = wbcStringToSid(resp->data.auth.info3.dom_sid,
				    &domain_sid);
	BAIL_ON_WBC_ERROR(wbc_status);

	sn = 0;
	if (!sid_attr_compose(&i->sids[sn], &domain_sid,
			      resp->data.auth.info3.user_rid, 0)) {
		wbc_status = WBC_ERR_INVALID_SID;
		goto done;
	}
	sn++;
	if (!sid_attr_compose(&i->sids[sn], &domain_sid,
			      resp->data.auth.info3.group_rid, 0)) {
		wbc_status = WBC_ERR_INVALID_SID;
		goto done;
	}
	sn++;

	p = (char *)resp->extra_data.data;
	if (!p) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	for (j=0; j < resp->data.auth.info3.num_groups; j++) {
		uint32_t rid;
		uint32_t attrs;
		int ret;
		char *s = p;
		char *e = strchr(p, '\n');
		if (!e) {
			wbc_status = WBC_ERR_INVALID_RESPONSE;
			BAIL_ON_WBC_ERROR(wbc_status);
		}
		e[0] = '\0';
		p = &e[1];

		ret = sscanf(s, "0x%08X:0x%08X", &rid, &attrs);
		if (ret != 2) {
			wbc_status = WBC_ERR_INVALID_RESPONSE;
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		if (!sid_attr_compose(&i->sids[sn], &domain_sid,
				      rid, attrs)) {
			wbc_status = WBC_ERR_INVALID_SID;
			goto done;
		}
		sn++;
	}

	for (j=0; j < resp->data.auth.info3.num_other_sids; j++) {
		uint32_t attrs;
		int ret;
		char *s = p;
		char *a;
		char *e = strchr(p, '\n');
		if (!e) {
			wbc_status = WBC_ERR_INVALID_RESPONSE;
			BAIL_ON_WBC_ERROR(wbc_status);
		}
		e[0] = '\0';
		p = &e[1];

		e = strchr(s, ':');
		if (!e) {
			wbc_status = WBC_ERR_INVALID_RESPONSE;
			BAIL_ON_WBC_ERROR(wbc_status);
		}
		e[0] = '\0';
		a = &e[1];

		ret = sscanf(a, "0x%08X",
			     &attrs);
		if (ret != 1) {
			wbc_status = WBC_ERR_INVALID_RESPONSE;
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = wbcStringToSid(s, &i->sids[sn].sid);
		BAIL_ON_WBC_ERROR(wbc_status);

		i->sids[sn].attributes = attrs;
		sn++;
	}

	i->num_sids = sn;

	*_i = i;
	i = NULL;
done:
	wbcFreeMemory(i);
	return wbc_status;
}

static void wbcAuthErrorInfoDestructor(void *ptr)
{
	struct wbcAuthErrorInfo *e = (struct wbcAuthErrorInfo *)ptr;
	free(e->nt_string);
	free(e->display_string);
}

static wbcErr wbc_create_error_info(const struct winbindd_response *resp,
				    struct wbcAuthErrorInfo **_e)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcAuthErrorInfo *e;

	e = (struct wbcAuthErrorInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcAuthErrorInfo),
		wbcAuthErrorInfoDestructor);
	BAIL_ON_PTR_ERROR(e, wbc_status);

	e->nt_status = resp->data.auth.nt_status;
	e->pam_error = resp->data.auth.pam_error;
	e->nt_string = strdup(resp->data.auth.nt_status_string);
	BAIL_ON_PTR_ERROR(e->nt_string, wbc_status);

	e->display_string = strdup(resp->data.auth.error_string);
	BAIL_ON_PTR_ERROR(e->display_string, wbc_status);

	*_e = e;
	e = NULL;

done:
	wbcFreeMemory(e);
	return wbc_status;
}

static wbcErr wbc_create_password_policy_info(const struct winbindd_response *resp,
					      struct wbcUserPasswordPolicyInfo **_i)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcUserPasswordPolicyInfo *i;

	i = (struct wbcUserPasswordPolicyInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcUserPasswordPolicyInfo), NULL);
	BAIL_ON_PTR_ERROR(i, wbc_status);

	i->min_passwordage	= resp->data.auth.policy.min_passwordage;
	i->min_length_password	= resp->data.auth.policy.min_length_password;
	i->password_history	= resp->data.auth.policy.password_history;
	i->password_properties	= resp->data.auth.policy.password_properties;
	i->expire		= resp->data.auth.policy.expire;

	*_i = i;
	i = NULL;

done:
	wbcFreeMemory(i);
	return wbc_status;
}

static void wbcLogonUserInfoDestructor(void *ptr)
{
	struct wbcLogonUserInfo *i = (struct wbcLogonUserInfo *)ptr;
	wbcFreeMemory(i->info);
	wbcFreeMemory(i->blobs);
}

static wbcErr wbc_create_logon_info(struct winbindd_response *resp,
				    struct wbcLogonUserInfo **_i)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcLogonUserInfo *i;

	i = (struct wbcLogonUserInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcLogonUserInfo),
		wbcLogonUserInfoDestructor);
	BAIL_ON_PTR_ERROR(i, wbc_status);

	wbc_status = wbc_create_auth_info(resp, &i->info);
	BAIL_ON_WBC_ERROR(wbc_status);

	if (resp->data.auth.krb5ccname[0] != '\0') {
		wbc_status = wbcAddNamedBlob(&i->num_blobs,
					     &i->blobs,
					     "krb5ccname",
					     0,
					     (uint8_t *)resp->data.auth.krb5ccname,
					     strlen(resp->data.auth.krb5ccname)+1);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (resp->data.auth.unix_username[0] != '\0') {
		wbc_status = wbcAddNamedBlob(&i->num_blobs,
					     &i->blobs,
					     "unix_username",
					     0,
					     (uint8_t *)resp->data.auth.unix_username,
					     strlen(resp->data.auth.unix_username)+1);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	*_i = i;
	i = NULL;
done:
	wbcFreeMemory(i);
	return wbc_status;
}


/* Authenticate with more detailed information */
wbcErr wbcAuthenticateUserEx(const struct wbcAuthUserParams *params,
			     struct wbcAuthUserInfo **info,
			     struct wbcAuthErrorInfo **error)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int cmd = 0;
	struct winbindd_request request;
	struct winbindd_response response;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (error) {
		*error = NULL;
	}

	if (!params) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (!params->account_name) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	switch (params->level) {
	case WBC_AUTH_USER_LEVEL_PLAIN:
		cmd = WINBINDD_PAM_AUTH;
		request.flags = WBFLAG_PAM_INFO3_TEXT |
				WBFLAG_PAM_USER_SESSION_KEY |
				WBFLAG_PAM_LMKEY;

		if (!params->password.plaintext) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		if (params->domain_name && params->domain_name[0]) {
			/* We need to get the winbind separator :-( */
			struct winbindd_response sep_response;

			ZERO_STRUCT(sep_response);

			wbc_status = wbcRequestResponse(WINBINDD_INFO,
							NULL, &sep_response);
			BAIL_ON_WBC_ERROR(wbc_status);

			snprintf(request.data.auth.user,
				 sizeof(request.data.auth.user)-1,
				 "%s%c%s",
				 params->domain_name,
				 sep_response.data.info.winbind_separator,
				 params->account_name);
		} else {
			strncpy(request.data.auth.user,
				params->account_name,
				sizeof(request.data.auth.user)-1);
		}

		strncpy(request.data.auth.pass,
			params->password.plaintext,
			sizeof(request.data.auth.pass)-1);
		break;

	case WBC_AUTH_USER_LEVEL_HASH:
		wbc_status = WBC_ERR_NOT_IMPLEMENTED;
		BAIL_ON_WBC_ERROR(wbc_status);
		break;

	case WBC_AUTH_USER_LEVEL_RESPONSE:
		cmd = WINBINDD_PAM_AUTH_CRAP;
		request.flags = WBFLAG_PAM_INFO3_TEXT |
				WBFLAG_PAM_USER_SESSION_KEY |
				WBFLAG_PAM_LMKEY;

		if (params->password.response.lm_length &&
		    !params->password.response.lm_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			BAIL_ON_WBC_ERROR(wbc_status);
		}
		if (params->password.response.lm_length == 0 &&
		    params->password.response.lm_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		if (params->password.response.nt_length &&
		    !params->password.response.nt_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			BAIL_ON_WBC_ERROR(wbc_status);
		}
		if (params->password.response.nt_length == 0&&
		    params->password.response.nt_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		strncpy(request.data.auth_crap.user,
			params->account_name,
			sizeof(request.data.auth_crap.user)-1);
		if (params->domain_name) {
			strncpy(request.data.auth_crap.domain,
				params->domain_name,
				sizeof(request.data.auth_crap.domain)-1);
		}
		if (params->workstation_name) {
			strncpy(request.data.auth_crap.workstation,
				params->workstation_name,
				sizeof(request.data.auth_crap.workstation)-1);
		}

		request.data.auth_crap.logon_parameters =
				params->parameter_control;

		memcpy(request.data.auth_crap.chal,
		       params->password.response.challenge,
		       sizeof(request.data.auth_crap.chal));

		request.data.auth_crap.lm_resp_len =
				MIN(params->password.response.lm_length,
				    sizeof(request.data.auth_crap.lm_resp));
		if (params->password.response.lm_data) {
			memcpy(request.data.auth_crap.lm_resp,
			       params->password.response.lm_data,
			       request.data.auth_crap.lm_resp_len);
		}
		request.data.auth_crap.nt_resp_len = params->password.response.nt_length;
		if (params->password.response.nt_length > sizeof(request.data.auth_crap.nt_resp)) {
			request.flags |= WBFLAG_BIG_NTLMV2_BLOB;
			request.extra_len = params->password.response.nt_length;
			request.extra_data.data = (char *)malloc(
				request.extra_len);
			if (request.extra_data.data == NULL) {
				wbc_status = WBC_ERR_NO_MEMORY;
				BAIL_ON_WBC_ERROR(wbc_status);
			}
			memcpy(request.extra_data.data,
			       params->password.response.nt_data,
			       request.data.auth_crap.nt_resp_len);
		} else if (params->password.response.nt_data) {
			memcpy(request.data.auth_crap.nt_resp,
			       params->password.response.nt_data,
			       request.data.auth_crap.nt_resp_len);
		}
		break;
	default:
		break;
	}

	if (cmd == 0) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (params->flags) {
		request.flags |= params->flags;
	}

	if (cmd == WINBINDD_PAM_AUTH_CRAP) {
		wbc_status = wbcRequestResponsePriv(cmd, &request, &response);
	} else {
		wbc_status = wbcRequestResponse(cmd, &request, &response);
	}
	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

	if (info) {
		wbc_status = wbc_create_auth_info(&response, info);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

done:
	winbindd_free_response(&response);

	free(request.extra_data.data);

	return wbc_status;
}

/* Trigger a verification of the trust credentials of a specific domain */
wbcErr wbcCheckTrustCredentials(const char *domain,
				struct wbcAuthErrorInfo **error)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (domain) {
		strncpy(request.domain_name, domain,
			sizeof(request.domain_name)-1);
	}

	/* Send request */

	wbc_status = wbcRequestResponsePriv(WINBINDD_CHECK_MACHACC,
					    &request, &response);
	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Trigger a change of the trust credentials for a specific domain */
wbcErr wbcChangeTrustCredentials(const char *domain,
				 struct wbcAuthErrorInfo **error)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (domain) {
		strncpy(request.domain_name, domain,
			sizeof(request.domain_name)-1);
	}

	/* Send request */

	wbc_status = wbcRequestResponsePriv(WINBINDD_CHANGE_MACHACC,
					&request, &response);
	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/*
 * Trigger a no-op NETLOGON call. Lightweight version of
 * wbcCheckTrustCredentials
 */
wbcErr wbcPingDc(const char *domain, struct wbcAuthErrorInfo **error)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	if (domain) {
		/*
		 * the current protocol doesn't support
		 * specifying a domain
		 */
		wbc_status = WBC_ERR_NOT_IMPLEMENTED;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_PING_DC,
					&request,
					&response);
	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Trigger an extended logoff notification to Winbind for a specific user */
wbcErr wbcLogoffUserEx(const struct wbcLogoffUserParams *params,
		       struct wbcAuthErrorInfo **error)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int i;

	/* validate input */

	if (!params || !params->username) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if ((params->num_blobs > 0) && (params->blobs == NULL)) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	if ((params->num_blobs == 0) && (params->blobs != NULL)) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.logoff.user, params->username,
		sizeof(request.data.logoff.user)-1);

	for (i=0; i<params->num_blobs; i++) {

		if (strcasecmp(params->blobs[i].name, "ccfilename") == 0) {
			if (params->blobs[i].blob.data) {
				strncpy(request.data.logoff.krb5ccname,
					(const char *)params->blobs[i].blob.data,
					sizeof(request.data.logoff.krb5ccname) - 1);
			}
			continue;
		}

		if (strcasecmp(params->blobs[i].name, "user_uid") == 0) {
			if (params->blobs[i].blob.data) {
				memcpy(&request.data.logoff.uid,
					params->blobs[i].blob.data,
					MIN(params->blobs[i].blob.length,
					    sizeof(request.data.logoff.uid)));
			}
			continue;
		}

		if (strcasecmp(params->blobs[i].name, "flags") == 0) {
			if (params->blobs[i].blob.data) {
				memcpy(&request.flags,
					params->blobs[i].blob.data,
					MIN(params->blobs[i].blob.length,
					    sizeof(request.flags)));
			}
			continue;
		}
	}

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_PAM_LOGOFF,
					&request,
					&response);

	/* Take the response above and return it to the caller */
	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Trigger a logoff notification to Winbind for a specific user */
wbcErr wbcLogoffUser(const char *username,
		     uid_t uid,
		     const char *ccfilename)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;

	/* validate input */

	if (!username) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.logoff.user, username,
		sizeof(request.data.logoff.user)-1);
	request.data.logoff.uid = uid;

	if (ccfilename) {
		strncpy(request.data.logoff.krb5ccname, ccfilename,
			sizeof(request.data.logoff.krb5ccname)-1);
	}

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_PAM_LOGOFF,
					&request,
					&response);

	/* Take the response above and return it to the caller */

 done:
	return wbc_status;
}

/* Change a password for a user with more detailed information upon failure */
wbcErr wbcChangeUserPasswordEx(const struct wbcChangePasswordParams *params,
			       struct wbcAuthErrorInfo **error,
			       enum wbcPasswordChangeRejectReason *reject_reason,
			       struct wbcUserPasswordPolicyInfo **policy)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int cmd = 0;

	/* validate input */

	if (!params->account_name) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		goto done;
	}

	if (error) {
		*error = NULL;
	}

	if (policy) {
		*policy = NULL;
	}

	if (reject_reason) {
		*reject_reason = -1;
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	switch (params->level) {
	case WBC_CHANGE_PASSWORD_LEVEL_PLAIN:
		cmd = WINBINDD_PAM_CHAUTHTOK;

		if (!params->account_name) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		strncpy(request.data.chauthtok.user, params->account_name,
			sizeof(request.data.chauthtok.user) - 1);

		if (params->old_password.plaintext) {
			strncpy(request.data.chauthtok.oldpass,
				params->old_password.plaintext,
				sizeof(request.data.chauthtok.oldpass) - 1);
		}

		if (params->new_password.plaintext) {
			strncpy(request.data.chauthtok.newpass,
				params->new_password.plaintext,
				sizeof(request.data.chauthtok.newpass) - 1);
		}
		break;

	case WBC_CHANGE_PASSWORD_LEVEL_RESPONSE:
		cmd = WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP;

		if (!params->account_name || !params->domain_name) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->old_password.response.old_lm_hash_enc_length &&
		    !params->old_password.response.old_lm_hash_enc_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->old_password.response.old_lm_hash_enc_length == 0 &&
		    params->old_password.response.old_lm_hash_enc_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->old_password.response.old_nt_hash_enc_length &&
		    !params->old_password.response.old_nt_hash_enc_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->old_password.response.old_nt_hash_enc_length == 0 &&
		    params->old_password.response.old_nt_hash_enc_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->new_password.response.lm_length &&
		    !params->new_password.response.lm_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->new_password.response.lm_length == 0 &&
		    params->new_password.response.lm_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->new_password.response.nt_length &&
		    !params->new_password.response.nt_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		if (params->new_password.response.nt_length == 0 &&
		    params->new_password.response.nt_data) {
			wbc_status = WBC_ERR_INVALID_PARAM;
			goto done;
		}

		strncpy(request.data.chng_pswd_auth_crap.user,
			params->account_name,
			sizeof(request.data.chng_pswd_auth_crap.user) - 1);

		strncpy(request.data.chng_pswd_auth_crap.domain,
			params->domain_name,
			sizeof(request.data.chng_pswd_auth_crap.domain) - 1);

		if (params->new_password.response.nt_data) {
			request.data.chng_pswd_auth_crap.new_nt_pswd_len =
				params->new_password.response.nt_length;
			memcpy(request.data.chng_pswd_auth_crap.new_nt_pswd,
			       params->new_password.response.nt_data,
			       request.data.chng_pswd_auth_crap.new_nt_pswd_len);
		}

		if (params->new_password.response.lm_data) {
			request.data.chng_pswd_auth_crap.new_lm_pswd_len =
				params->new_password.response.lm_length;
			memcpy(request.data.chng_pswd_auth_crap.new_lm_pswd,
			       params->new_password.response.lm_data,
			       request.data.chng_pswd_auth_crap.new_lm_pswd_len);
		}

		if (params->old_password.response.old_nt_hash_enc_data) {
			request.data.chng_pswd_auth_crap.old_nt_hash_enc_len =
				params->old_password.response.old_nt_hash_enc_length;
			memcpy(request.data.chng_pswd_auth_crap.old_nt_hash_enc,
			       params->old_password.response.old_nt_hash_enc_data,
			       request.data.chng_pswd_auth_crap.old_nt_hash_enc_len);
		}

		if (params->old_password.response.old_lm_hash_enc_data) {
			request.data.chng_pswd_auth_crap.old_lm_hash_enc_len =
				params->old_password.response.old_lm_hash_enc_length;
			memcpy(request.data.chng_pswd_auth_crap.old_lm_hash_enc,
			       params->old_password.response.old_lm_hash_enc_data,
			       request.data.chng_pswd_auth_crap.old_lm_hash_enc_len);
		}

		break;
	default:
		wbc_status = WBC_ERR_INVALID_PARAM;
		goto done;
		break;
	}

	/* Send request */

	wbc_status = wbcRequestResponse(cmd,
					&request,
					&response);
	if (WBC_ERROR_IS_OK(wbc_status)) {
		goto done;
	}

	/* Take the response above and return it to the caller */

	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

	}

	if (policy) {
		wbc_status = wbc_create_password_policy_info(&response,
							     policy);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (reject_reason) {
		*reject_reason = response.data.auth.reject_reason;
	}

	wbc_status = WBC_ERR_PWD_CHANGE_FAILED;
	BAIL_ON_WBC_ERROR(wbc_status);

 done:
	return wbc_status;
}

/* Change a password for a user */
wbcErr wbcChangeUserPassword(const char *username,
			     const char *old_password,
			     const char *new_password)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcChangePasswordParams params;

	ZERO_STRUCT(params);

	params.account_name		= username;
	params.level			= WBC_CHANGE_PASSWORD_LEVEL_PLAIN;
	params.old_password.plaintext	= old_password;
	params.new_password.plaintext	= new_password;

	wbc_status = wbcChangeUserPasswordEx(&params,
					     NULL,
					     NULL,
					     NULL);
	BAIL_ON_WBC_ERROR(wbc_status);

done:
	return wbc_status;
}

/* Logon a User */
wbcErr wbcLogonUser(const struct wbcLogonUserParams *params,
		    struct wbcLogonUserInfo **info,
		    struct wbcAuthErrorInfo **error,
		    struct wbcUserPasswordPolicyInfo **policy)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;
	uint32_t i;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (info) {
		*info = NULL;
	}
	if (error) {
		*error = NULL;
	}
	if (policy) {
		*policy = NULL;
	}

	if (!params) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (!params->username) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if ((params->num_blobs > 0) && (params->blobs == NULL)) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	if ((params->num_blobs == 0) && (params->blobs != NULL)) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	request.flags = WBFLAG_PAM_INFO3_TEXT |
			WBFLAG_PAM_USER_SESSION_KEY |
			WBFLAG_PAM_LMKEY;

	if (!params->password) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	strncpy(request.data.auth.user,
		params->username,
		sizeof(request.data.auth.user)-1);

	strncpy(request.data.auth.pass,
		params->password,
		sizeof(request.data.auth.pass)-1);

	for (i=0; i<params->num_blobs; i++) {

		if (strcasecmp(params->blobs[i].name, "krb5_cc_type") == 0) {
			if (params->blobs[i].blob.data) {
				strncpy(request.data.auth.krb5_cc_type,
					(const char *)params->blobs[i].blob.data,
					sizeof(request.data.auth.krb5_cc_type) - 1);
			}
			continue;
		}

		if (strcasecmp(params->blobs[i].name, "user_uid") == 0) {
			if (params->blobs[i].blob.data) {
				memcpy(&request.data.auth.uid,
					params->blobs[i].blob.data,
					MIN(sizeof(request.data.auth.uid),
					    params->blobs[i].blob.length));
			}
			continue;
		}

		if (strcasecmp(params->blobs[i].name, "flags") == 0) {
			if (params->blobs[i].blob.data) {
				uint32_t flags;
				memcpy(&flags,
					params->blobs[i].blob.data,
					MIN(sizeof(flags),
					    params->blobs[i].blob.length));
				request.flags |= flags;
			}
			continue;
		}

		if (strcasecmp(params->blobs[i].name, "membership_of") == 0) {
			if (params->blobs[i].blob.data &&
			    params->blobs[i].blob.data[0] > 0) {
				strncpy(request.data.auth.require_membership_of_sid,
					(const char *)params->blobs[i].blob.data,
					sizeof(request.data.auth.require_membership_of_sid) - 1);
			}
			continue;
		}
	}

	wbc_status = wbcRequestResponse(WINBINDD_PAM_AUTH,
					&request,
					&response);

	if (response.data.auth.nt_status != 0) {
		if (error) {
			wbc_status = wbc_create_error_info(&response,
							   error);
			BAIL_ON_WBC_ERROR(wbc_status);
		}

		wbc_status = WBC_ERR_AUTH_ERROR;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	BAIL_ON_WBC_ERROR(wbc_status);

	if (info) {
		wbc_status = wbc_create_logon_info(&response,
						   info);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (policy) {
		wbc_status = wbc_create_password_policy_info(&response,
							     policy);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

done:
	winbindd_free_response(&response);

	return wbc_status;
}

static void wbcCredentialCacheInfoDestructor(void *ptr)
{
	struct wbcCredentialCacheInfo *i =
		(struct wbcCredentialCacheInfo *)ptr;
	wbcFreeMemory(i->blobs);
}

/* Authenticate a user with cached credentials */
wbcErr wbcCredentialCache(struct wbcCredentialCacheParams *params,
                          struct wbcCredentialCacheInfo **info,
                          struct wbcAuthErrorInfo **error)
{
	wbcErr status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcCredentialCacheInfo *result = NULL;
	struct winbindd_request request;
	struct winbindd_response response;
	struct wbcNamedBlob *initial_blob = NULL;
	struct wbcNamedBlob *challenge_blob = NULL;
	int i;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (info != NULL) {
		*info = NULL;
	}
	if (error != NULL) {
		*error = NULL;
	}
	if ((params == NULL)
	    || (params->account_name == NULL)
	    || (params->level != WBC_CREDENTIAL_CACHE_LEVEL_NTLMSSP)) {
		status = WBC_ERR_INVALID_PARAM;
		goto fail;
	}

	if (params->domain_name != NULL) {
		status = wbcRequestResponse(WINBINDD_INFO, NULL, &response);
		if (!WBC_ERROR_IS_OK(status)) {
			goto fail;
		}
		snprintf(request.data.ccache_ntlm_auth.user,
			 sizeof(request.data.ccache_ntlm_auth.user)-1,
			 "%s%c%s", params->domain_name,
			 response.data.info.winbind_separator,
			 params->account_name);
	} else {
		strncpy(request.data.ccache_ntlm_auth.user,
			params->account_name,
			sizeof(request.data.ccache_ntlm_auth.user)-1);
	}
	request.data.ccache_ntlm_auth.uid = getuid();

	for (i=0; i<params->num_blobs; i++) {
		if (strcasecmp(params->blobs[i].name, "initial_blob") == 0) {
			initial_blob = &params->blobs[i];
			break;
		}
		if (strcasecmp(params->blobs[i].name, "challenge_blob") == 0) {
			challenge_blob = &params->blobs[i];
			break;
		}
	}

	request.data.ccache_ntlm_auth.initial_blob_len = 0;
	request.data.ccache_ntlm_auth.challenge_blob_len = 0;
	request.extra_len = 0;

	if (initial_blob != NULL) {
		request.data.ccache_ntlm_auth.initial_blob_len =
			initial_blob->blob.length;
		request.extra_len += initial_blob->blob.length;
	}
	if (challenge_blob != NULL) {
		request.data.ccache_ntlm_auth.challenge_blob_len =
			challenge_blob->blob.length;
		request.extra_len += challenge_blob->blob.length;
	}

	if (request.extra_len != 0) {
		request.extra_data.data = (char *)malloc(request.extra_len);
		if (request.extra_data.data == NULL) {
			status = WBC_ERR_NO_MEMORY;
			goto fail;
		}
	}
	if (initial_blob != NULL) {
		memcpy(request.extra_data.data,
		       initial_blob->blob.data, initial_blob->blob.length);
	}
	if (challenge_blob != NULL) {
		memcpy(request.extra_data.data
		       + request.data.ccache_ntlm_auth.initial_blob_len,
		       challenge_blob->blob.data,
		       challenge_blob->blob.length);
	}

	status = wbcRequestResponse(WINBINDD_CCACHE_NTLMAUTH, &request,
				    &response);
	if (!WBC_ERROR_IS_OK(status)) {
		goto fail;
	}

	result = (struct wbcCredentialCacheInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcCredentialCacheInfo),
		wbcCredentialCacheInfoDestructor);
	if (result == NULL) {
		status = WBC_ERR_NO_MEMORY;
		goto fail;
	}
	result->num_blobs = 0;
	result->blobs = NULL;
	status = wbcAddNamedBlob(&result->num_blobs, &result->blobs,
				 "auth_blob", 0,
				 (uint8_t *)response.extra_data.data,
				 response.data.ccache_ntlm_auth.auth_blob_len);
	if (!WBC_ERROR_IS_OK(status)) {
		goto fail;
	}
	status = wbcAddNamedBlob(
		&result->num_blobs, &result->blobs, "session_key", 0,
		response.data.ccache_ntlm_auth.session_key,
		sizeof(response.data.ccache_ntlm_auth.session_key));
	if (!WBC_ERROR_IS_OK(status)) {
		goto fail;
	}

	*info = result;
	result = NULL;
	status = WBC_ERR_SUCCESS;
fail:
	free(request.extra_data.data);
	winbindd_free_response(&response);
	wbcFreeMemory(result);
	return status;
}

/* Authenticate a user with cached credentials */
wbcErr wbcCredentialSave(const char *user, const char *password)
{
	struct winbindd_request request;
	struct winbindd_response response;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.ccache_save.user, user,
		sizeof(request.data.ccache_save.user)-1);
	strncpy(request.data.ccache_save.pass, password,
		sizeof(request.data.ccache_save.pass)-1);
	request.data.ccache_save.uid = getuid();

	return wbcRequestResponse(WINBINDD_CCACHE_SAVE, &request, &response);
}
