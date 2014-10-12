/*
   Unix SMB/CIFS implementation.
   kerberos authorization data (PAC) utility library
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Stefan Metzmacher 2004-2005
   Copyright (C) Guenther Deschner 2005,2007,2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "smb_krb5.h"
#include "libads/kerberos_proto.h"

#ifdef HAVE_KRB5

/****************************************************************
****************************************************************/

static krb5_error_code check_pac_checksum(TALLOC_CTX *mem_ctx,
					  DATA_BLOB pac_data,
					  struct PAC_SIGNATURE_DATA *sig,
					  krb5_context context,
					  krb5_keyblock *keyblock)
{
	krb5_error_code ret;
	krb5_checksum cksum;
	krb5_keyusage usage = 0;

	smb_krb5_checksum_from_pac_sig(&cksum, sig);

#ifdef HAVE_KRB5_KU_OTHER_CKSUM /* Heimdal */
	usage = KRB5_KU_OTHER_CKSUM;
#elif defined(HAVE_KRB5_KEYUSAGE_APP_DATA_CKSUM) /* MIT */
	usage = KRB5_KEYUSAGE_APP_DATA_CKSUM;
#else
#error UNKNOWN_KRB5_KEYUSAGE
#endif

	ret = smb_krb5_verify_checksum(context,
				       keyblock,
				       usage,
				       &cksum,
				       pac_data.data,
				       pac_data.length);

	if (ret) {
		DEBUG(2,("check_pac_checksum: PAC Verification failed: %s (%d)\n",
			error_message(ret), ret));
		return ret;
	}

	return ret;
}

/**
* @brief Decode a blob containing a NDR envoded PAC structure
*
* @param mem_ctx	  - The memory context
* @param pac_data_blob	  - The data blob containing the NDR encoded data
* @param context	  - The Kerberos Context
* @param service_keyblock - The Service Key used to verify the checksum
* @param client_principal - The client principal
* @param tgs_authtime     - The ticket timestamp
* @param pac_data_out	  - [out] The decoded PAC
*
* @return - A NTSTATUS error code
*/
NTSTATUS decode_pac_data(TALLOC_CTX *mem_ctx,
			 DATA_BLOB *pac_data_blob,
			 krb5_context context,
			 krb5_keyblock *service_keyblock,
			 krb5_const_principal client_principal,
			 time_t tgs_authtime,
			 struct PAC_DATA **pac_data_out)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	krb5_error_code ret;
	DATA_BLOB modified_pac_blob;

	NTTIME tgs_authtime_nttime;
	krb5_principal client_principal_pac = NULL;
	int i;

	struct PAC_SIGNATURE_DATA *srv_sig_ptr = NULL;
	struct PAC_SIGNATURE_DATA *kdc_sig_ptr = NULL;
	struct PAC_SIGNATURE_DATA *srv_sig_wipe = NULL;
	struct PAC_SIGNATURE_DATA *kdc_sig_wipe = NULL;
	struct PAC_LOGON_NAME *logon_name = NULL;
	struct PAC_LOGON_INFO *logon_info = NULL;
	struct PAC_DATA *pac_data = NULL;
	struct PAC_DATA_RAW *pac_data_raw = NULL;

	DATA_BLOB *srv_sig_blob = NULL;
	DATA_BLOB *kdc_sig_blob = NULL;

	bool bool_ret;

	*pac_data_out = NULL;

	pac_data = TALLOC_ZERO_P(mem_ctx, struct PAC_DATA);
	pac_data_raw = TALLOC_ZERO_P(mem_ctx, struct PAC_DATA_RAW);
	kdc_sig_wipe = TALLOC_ZERO_P(mem_ctx, struct PAC_SIGNATURE_DATA);
	srv_sig_wipe = TALLOC_ZERO_P(mem_ctx, struct PAC_SIGNATURE_DATA);
	if (!pac_data_raw || !pac_data || !kdc_sig_wipe || !srv_sig_wipe) {
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_pull_struct_blob(pac_data_blob, pac_data, pac_data,
		       (ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the PAC: %s\n",
			nt_errstr(status)));
		return status;
	}

	if (pac_data->num_buffers < 4) {
		/* we need logon_ingo, service_key and kdc_key */
		DEBUG(0,("less than 4 PAC buffers\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ndr_err = ndr_pull_struct_blob(
				pac_data_blob, pac_data_raw, pac_data_raw,
				(ndr_pull_flags_fn_t)ndr_pull_PAC_DATA_RAW);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the PAC: %s\n",
			nt_errstr(status)));
		return status;
	}

	if (pac_data_raw->num_buffers < 4) {
		/* we need logon_ingo, service_key and kdc_key */
		DEBUG(0,("less than 4 PAC buffers\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (pac_data->num_buffers != pac_data_raw->num_buffers) {
		/* we need logon_ingo, service_key and kdc_key */
		DEBUG(0, ("misparse! PAC_DATA has %d buffers while "
			  "PAC_DATA_RAW has %d\n", pac_data->num_buffers,
			  pac_data_raw->num_buffers));
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (i=0; i < pac_data->num_buffers; i++) {
		struct PAC_BUFFER *data_buf = &pac_data->buffers[i];
		struct PAC_BUFFER_RAW *raw_buf = &pac_data_raw->buffers[i];

		if (data_buf->type != raw_buf->type) {
			DEBUG(0, ("misparse! PAC_DATA buffer %d has type "
				  "%d while PAC_DATA_RAW has %d\n", i,
				  data_buf->type, raw_buf->type));
			return NT_STATUS_INVALID_PARAMETER;
		}
		switch (data_buf->type) {
		case PAC_TYPE_LOGON_INFO:
			if (!data_buf->info) {
				break;
			}
			logon_info = data_buf->info->logon_info.info;
			break;
		case PAC_TYPE_SRV_CHECKSUM:
			if (!data_buf->info) {
				break;
			}
			srv_sig_ptr = &data_buf->info->srv_cksum;
			srv_sig_blob = &raw_buf->info->remaining;
			break;
		case PAC_TYPE_KDC_CHECKSUM:
			if (!data_buf->info) {
				break;
			}
			kdc_sig_ptr = &data_buf->info->kdc_cksum;
			kdc_sig_blob = &raw_buf->info->remaining;
			break;
		case PAC_TYPE_LOGON_NAME:
			logon_name = &data_buf->info->logon_name;
			break;
		default:
			break;
		}
	}

	if (!logon_info) {
		DEBUG(0,("PAC no logon_info\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!logon_name) {
		DEBUG(0,("PAC no logon_name\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!srv_sig_ptr || !srv_sig_blob) {
		DEBUG(0,("PAC no srv_key\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!kdc_sig_ptr || !kdc_sig_blob) {
		DEBUG(0,("PAC no kdc_key\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Find and zero out the signatures,
	 * as required by the signing algorithm */

	/* We find the data blobs above,
	 * now we parse them to get at the exact portion we should zero */
	ndr_err = ndr_pull_struct_blob(
			kdc_sig_blob, kdc_sig_wipe, kdc_sig_wipe,
			(ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the KDC signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	ndr_err = ndr_pull_struct_blob(
			srv_sig_blob, srv_sig_wipe, srv_sig_wipe,
			(ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the SRV signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	/* Now zero the decoded structure */
	memset(kdc_sig_wipe->signature.data,
		'\0', kdc_sig_wipe->signature.length);
	memset(srv_sig_wipe->signature.data,
		'\0', srv_sig_wipe->signature.length);

	/* and reencode, back into the same place it came from */
	ndr_err = ndr_push_struct_blob(
			kdc_sig_blob, pac_data_raw, kdc_sig_wipe,
			(ndr_push_flags_fn_t)ndr_push_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't repack the KDC signature: %s\n",
			nt_errstr(status)));
		return status;
	}
	ndr_err = ndr_push_struct_blob(
			srv_sig_blob, pac_data_raw, srv_sig_wipe,
			(ndr_push_flags_fn_t)ndr_push_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't repack the SRV signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	/* push out the whole structure, but now with zero'ed signatures */
	ndr_err = ndr_push_struct_blob(
			&modified_pac_blob, pac_data_raw, pac_data_raw,
			(ndr_push_flags_fn_t)ndr_push_PAC_DATA_RAW);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't repack the RAW PAC: %s\n",
			nt_errstr(status)));
		return status;
	}

	/* verify by service_key */
	ret = check_pac_checksum(mem_ctx,
				 modified_pac_blob, srv_sig_ptr,
				 context,
				 service_keyblock);
	if (ret) {
		DEBUG(1, ("PAC Decode: Failed to verify the service "
			  "signature: %s\n", error_message(ret)));
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Convert to NT time, so as not to loose accuracy in comparison */
	unix_to_nt_time(&tgs_authtime_nttime, tgs_authtime);

	if (tgs_authtime_nttime != logon_name->logon_time) {
		DEBUG(2, ("PAC Decode: "
			  "Logon time mismatch between ticket and PAC!\n"));
		DEBUG(2, ("PAC Decode: PAC: %s\n",
			  nt_time_string(mem_ctx, logon_name->logon_time)));
		DEBUG(2, ("PAC Decode: Ticket: %s\n",
			  nt_time_string(mem_ctx, tgs_authtime_nttime)));
		return NT_STATUS_ACCESS_DENIED;
	}

	ret = smb_krb5_parse_name_norealm(context,
					  logon_name->account_name,
					  &client_principal_pac);
	if (ret) {
		DEBUG(2, ("Could not parse name from PAC: [%s]:%s\n",
			  logon_name->account_name, error_message(ret)));
		return NT_STATUS_INVALID_PARAMETER;
	}

	bool_ret = smb_krb5_principal_compare_any_realm(context,
							client_principal,
							client_principal_pac);

	krb5_free_principal(context, client_principal_pac);

	if (!bool_ret) {
		DEBUG(2, ("Name in PAC [%s] does not match principal name "
			  "in ticket\n", logon_name->account_name));
		return NT_STATUS_ACCESS_DENIED;
	}

	DEBUG(3,("Found account name from PAC: %s [%s]\n",
		 logon_info->info3.base.account_name.string,
		 logon_info->info3.base.full_name.string));

	DEBUG(10,("Successfully validated Kerberos PAC\n"));

	if (DEBUGLEVEL >= 10) {
		const char *s;
		s = NDR_PRINT_STRUCT_STRING(mem_ctx, PAC_DATA, pac_data);
		if (s) {
			DEBUGADD(10,("%s\n", s));
		}
	}

	*pac_data_out = pac_data;

	return NT_STATUS_OK;
}

/****************************************************************
Given a username, password and other details, return the
PAC_LOGON_INFO (the structure containing the important user
information such as groups).
****************************************************************/

NTSTATUS kerberos_return_pac(TALLOC_CTX *mem_ctx,
			     const char *name,
			     const char *pass,
			     time_t time_offset,
			     time_t *expire_time,
			     time_t *renew_till_time,
			     const char *cache_name,
			     bool request_pac,
			     bool add_netbios_addr,
			     time_t renewable_time,
			     const char *impersonate_princ_s,
			     struct PAC_LOGON_INFO **logon_info)
{
	krb5_error_code ret;
	NTSTATUS status = NT_STATUS_INVALID_PARAMETER;
	DATA_BLOB tkt, ap_rep, sesskey1, sesskey2;
	char *client_princ_out = NULL;
	const char *auth_princ = NULL;
	const char *local_service = NULL;
	const char *cc = "MEMORY:kerberos_return_pac";

	ZERO_STRUCT(tkt);
	ZERO_STRUCT(ap_rep);
	ZERO_STRUCT(sesskey1);
	ZERO_STRUCT(sesskey2);

	if (!name || !pass) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (cache_name) {
		cc = cache_name;
	}

	if (!strchr_m(name, '@')) {
		auth_princ = talloc_asprintf(mem_ctx, "%s@%s", name,
			lp_realm());
	} else {
		auth_princ = name;
	}
	NT_STATUS_HAVE_NO_MEMORY(auth_princ);

	local_service = talloc_asprintf(mem_ctx, "%s$@%s",
					global_myname(), lp_realm());
	NT_STATUS_HAVE_NO_MEMORY(local_service);

	ret = kerberos_kinit_password_ext(auth_princ,
					  pass,
					  time_offset,
					  expire_time,
					  renew_till_time,
					  cc,
					  request_pac,
					  add_netbios_addr,
					  renewable_time,
					  &status);
	if (ret) {
		DEBUG(1,("kinit failed for '%s' with: %s (%d)\n",
			auth_princ, error_message(ret), ret));
		/* status already set */
		goto out;
	}

	DEBUG(10,("got TGT for %s in %s\n", auth_princ, cc));
	if (expire_time) {
		DEBUGADD(10,("\tvalid until: %s (%d)\n",
			http_timestring(talloc_tos(), *expire_time),
			(int)*expire_time));
	}
	if (renew_till_time) {
		DEBUGADD(10,("\trenewable till: %s (%d)\n",
			http_timestring(talloc_tos(), *renew_till_time),
			(int)*renew_till_time));
	}

	/* we cannot continue with krb5 when UF_DONT_REQUIRE_PREAUTH is set,
	 * in that case fallback to NTLM - gd */

	if (expire_time && renew_till_time &&
	    (*expire_time == 0) && (*renew_till_time == 0)) {
		return NT_STATUS_INVALID_LOGON_TYPE;
	}

	ret = cli_krb5_get_ticket(mem_ctx,
				  local_service,
				  time_offset,
				  &tkt,
				  &sesskey1,
				  0,
				  cc,
				  NULL,
				  impersonate_princ_s);
	if (ret) {
		DEBUG(1,("failed to get ticket for %s: %s\n",
			local_service, error_message(ret)));
		if (impersonate_princ_s) {
			DEBUGADD(1,("tried S4U2SELF impersonation as: %s\n",
				impersonate_princ_s));
		}
		status = krb5_to_nt_status(ret);
		goto out;
	}
	status = ads_verify_ticket(mem_ctx,
				   lp_realm(),
				   time_offset,
				   &tkt,
				   &client_princ_out,
				   logon_info,
				   &ap_rep,
				   &sesskey2,
				   False);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("ads_verify_ticket failed: %s\n",
			nt_errstr(status)));
		goto out;
	}

	if (!*logon_info) {
		DEBUG(1,("no PAC\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}

out:
	if (cc != cache_name) {
		ads_kdestroy(cc);
	}

	data_blob_free(&tkt);
	data_blob_free(&ap_rep);
	data_blob_free(&sesskey1);
	data_blob_free(&sesskey2);

	TALLOC_FREE(client_princ_out);

	return status;
}

#endif
