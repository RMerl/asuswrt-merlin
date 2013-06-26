/*
   Unix SMB/CIFS implementation.

   Create and parse the krb5 PAC

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005,2008
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Stefan Metzmacher 2004-2005

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
#include "system/kerberos.h"
#include "auth/auth.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include <ldb.h>
#include "auth/auth_sam_reply.h"

krb5_error_code check_pac_checksum(TALLOC_CTX *mem_ctx,
				   DATA_BLOB pac_data,
				   struct PAC_SIGNATURE_DATA *sig,
				   krb5_context context,
				   const krb5_keyblock *keyblock)
{
	krb5_error_code ret;
	krb5_crypto crypto;
	Checksum cksum;

	cksum.cksumtype		= (CKSUMTYPE)sig->type;
	cksum.checksum.length	= sig->signature.length;
	cksum.checksum.data	= sig->signature.data;

	ret = krb5_crypto_init(context,
			       keyblock,
			       0,
			       &crypto);
	if (ret) {
		DEBUG(0,("krb5_crypto_init() failed: %s\n",
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
		return ret;
	}
	ret = krb5_verify_checksum(context,
				   crypto,
				   KRB5_KU_OTHER_CKSUM,
				   pac_data.data,
				   pac_data.length,
				   &cksum);
	krb5_crypto_destroy(context, crypto);

	return ret;
}

 NTSTATUS kerberos_decode_pac(TALLOC_CTX *mem_ctx,
			      struct PAC_DATA **pac_data_out,
			      DATA_BLOB blob,
			      krb5_context context,
			      const krb5_keyblock *krbtgt_keyblock,
			      const krb5_keyblock *service_keyblock,
			      krb5_const_principal client_principal,
			      time_t tgs_authtime,
			      krb5_error_code *k5ret)
{
	krb5_error_code ret;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct PAC_SIGNATURE_DATA *srv_sig_ptr = NULL;
	struct PAC_SIGNATURE_DATA *kdc_sig_ptr = NULL;
	struct PAC_SIGNATURE_DATA *srv_sig_wipe = NULL;
	struct PAC_SIGNATURE_DATA *kdc_sig_wipe = NULL;
	struct PAC_LOGON_INFO *logon_info = NULL;
	struct PAC_LOGON_NAME *logon_name = NULL;
	struct PAC_DATA *pac_data;
	struct PAC_DATA_RAW *pac_data_raw;

	DATA_BLOB *srv_sig_blob = NULL;
	DATA_BLOB *kdc_sig_blob = NULL;

	DATA_BLOB modified_pac_blob;
	NTTIME tgs_authtime_nttime;
	krb5_principal client_principal_pac;
	uint32_t i;

	krb5_clear_error_message(context);

	if (k5ret) {
		*k5ret = KRB5_PARSE_MALFORMED;
	}

	pac_data = talloc(mem_ctx, struct PAC_DATA);
	pac_data_raw = talloc(mem_ctx, struct PAC_DATA_RAW);
	kdc_sig_wipe = talloc(mem_ctx, struct PAC_SIGNATURE_DATA);
	srv_sig_wipe = talloc(mem_ctx, struct PAC_SIGNATURE_DATA);
	if (!pac_data_raw || !pac_data || !kdc_sig_wipe || !srv_sig_wipe) {
		if (k5ret) {
			*k5ret = ENOMEM;
		}
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_pull_struct_blob(&blob, pac_data,
			pac_data, (ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
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

	ndr_err = ndr_pull_struct_blob(&blob, pac_data_raw,
				       pac_data_raw,
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
		DEBUG(0,("misparse!  PAC_DATA has %d buffers while PAC_DATA_RAW has %d\n",
			 pac_data->num_buffers, pac_data_raw->num_buffers));
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (i=0; i < pac_data->num_buffers; i++) {
		if (pac_data->buffers[i].type != pac_data_raw->buffers[i].type) {
			DEBUG(0,("misparse!  PAC_DATA buffer %d has type %d while PAC_DATA_RAW has %d\n",
				 i, pac_data->buffers[i].type, pac_data->buffers[i].type));
			return NT_STATUS_INVALID_PARAMETER;
		}
		switch (pac_data->buffers[i].type) {
			case PAC_TYPE_LOGON_INFO:
				if (!pac_data->buffers[i].info) {
					break;
				}
				logon_info = pac_data->buffers[i].info->logon_info.info;
				break;
			case PAC_TYPE_SRV_CHECKSUM:
				if (!pac_data->buffers[i].info) {
					break;
				}
				srv_sig_ptr = &pac_data->buffers[i].info->srv_cksum;
				srv_sig_blob = &pac_data_raw->buffers[i].info->remaining;
				break;
			case PAC_TYPE_KDC_CHECKSUM:
				if (!pac_data->buffers[i].info) {
					break;
				}
				kdc_sig_ptr = &pac_data->buffers[i].info->kdc_cksum;
				kdc_sig_blob = &pac_data_raw->buffers[i].info->remaining;
				break;
			case PAC_TYPE_LOGON_NAME:
				logon_name = &pac_data->buffers[i].info->logon_name;
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

	/* Find and zero out the signatures, as required by the signing algorithm */

	/* We find the data blobs above, now we parse them to get at the exact portion we should zero */
	ndr_err = ndr_pull_struct_blob(kdc_sig_blob, kdc_sig_wipe,
				       kdc_sig_wipe,
				       (ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the KDC signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	ndr_err = ndr_pull_struct_blob(srv_sig_blob, srv_sig_wipe,
				       srv_sig_wipe,
				       (ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the SRV signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	/* Now zero the decoded structure */
	memset(kdc_sig_wipe->signature.data, '\0', kdc_sig_wipe->signature.length);
	memset(srv_sig_wipe->signature.data, '\0', srv_sig_wipe->signature.length);

	/* and reencode, back into the same place it came from */
	ndr_err = ndr_push_struct_blob(kdc_sig_blob, pac_data_raw,
				       kdc_sig_wipe,
				       (ndr_push_flags_fn_t)ndr_push_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't repack the KDC signature: %s\n",
			nt_errstr(status)));
		return status;
	}
	ndr_err = ndr_push_struct_blob(srv_sig_blob, pac_data_raw,
				       srv_sig_wipe,
				       (ndr_push_flags_fn_t)ndr_push_PAC_SIGNATURE_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't repack the SRV signature: %s\n",
			nt_errstr(status)));
		return status;
	}

	/* push out the whole structure, but now with zero'ed signatures */
	ndr_err = ndr_push_struct_blob(&modified_pac_blob, pac_data_raw,
				       pac_data_raw,
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
		DEBUG(1, ("PAC Decode: Failed to verify the service signature: %s\n",
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
		if (k5ret) {
			*k5ret = ret;
		}
		return NT_STATUS_ACCESS_DENIED;
	}

	if (krbtgt_keyblock) {
		ret = check_pac_checksum(mem_ctx,
					    srv_sig_ptr->signature, kdc_sig_ptr,
					    context, krbtgt_keyblock);
		if (ret) {
			DEBUG(1, ("PAC Decode: Failed to verify the KDC signature: %s\n",
				  smb_get_krb5_error_message(context, ret, mem_ctx)));
			if (k5ret) {
				*k5ret = ret;
			}
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/* Convert to NT time, so as not to loose accuracy in comparison */
	unix_to_nt_time(&tgs_authtime_nttime, tgs_authtime);

	if (tgs_authtime_nttime != logon_name->logon_time) {
		DEBUG(2, ("PAC Decode: Logon time mismatch between ticket and PAC!\n"));
		DEBUG(2, ("PAC Decode: PAC: %s\n", nt_time_string(mem_ctx, logon_name->logon_time)));
		DEBUG(2, ("PAC Decode: Ticket: %s\n", nt_time_string(mem_ctx, tgs_authtime_nttime)));
		return NT_STATUS_ACCESS_DENIED;
	}

	ret = krb5_parse_name_flags(context, logon_name->account_name, KRB5_PRINCIPAL_PARSE_NO_REALM,
				    &client_principal_pac);
	if (ret) {
		DEBUG(2, ("Could not parse name from incoming PAC: [%s]: %s\n",
			  logon_name->account_name,
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
		if (k5ret) {
			*k5ret = ret;
		}
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!krb5_principal_compare_any_realm(context, client_principal, client_principal_pac)) {
		DEBUG(2, ("Name in PAC [%s] does not match principal name in ticket\n",
			  logon_name->account_name));
		krb5_free_principal(context, client_principal_pac);
		return NT_STATUS_ACCESS_DENIED;
	}

	krb5_free_principal(context, client_principal_pac);

#if 0
	if (strcasecmp(logon_info->info3.base.account_name.string,
		       "Administrator")== 0) {
		file_save("tmp_pac_data-admin.dat",blob.data,blob.length);
	}
#endif

	DEBUG(3,("Found account name from PAC: %s [%s]\n",
		 logon_info->info3.base.account_name.string,
		 logon_info->info3.base.full_name.string));
	*pac_data_out = pac_data;

	return NT_STATUS_OK;
}

_PUBLIC_  NTSTATUS kerberos_pac_logon_info(TALLOC_CTX *mem_ctx,
				  struct PAC_LOGON_INFO **logon_info,
				  DATA_BLOB blob,
				  krb5_context context,
				  const krb5_keyblock *krbtgt_keyblock,
				  const krb5_keyblock *service_keyblock,
				  krb5_const_principal client_principal,
				  time_t tgs_authtime,
				  krb5_error_code *k5ret)
{
	NTSTATUS nt_status;
	struct PAC_DATA *pac_data;
	int i;
	nt_status = kerberos_decode_pac(mem_ctx,
					&pac_data,
					blob,
					context,
					krbtgt_keyblock,
					service_keyblock,
					client_principal,
					tgs_authtime,
					k5ret);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	*logon_info = NULL;
	for (i=0; i < pac_data->num_buffers; i++) {
		if (pac_data->buffers[i].type != PAC_TYPE_LOGON_INFO) {
			continue;
		}
		*logon_info = pac_data->buffers[i].info->logon_info.info;
	}
	if (!*logon_info) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	return NT_STATUS_OK;
}

static krb5_error_code make_pac_checksum(TALLOC_CTX *mem_ctx,
					 DATA_BLOB *pac_data,
					 struct PAC_SIGNATURE_DATA *sig,
					 krb5_context context,
					 const krb5_keyblock *keyblock)
{
	krb5_error_code ret;
	krb5_crypto crypto;
	Checksum cksum;


	ret = krb5_crypto_init(context,
			       keyblock,
			       0,
			       &crypto);
	if (ret) {
		DEBUG(0,("krb5_crypto_init() failed: %s\n",
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
		return ret;
	}
	ret = krb5_create_checksum(context,
				   crypto,
				   KRB5_KU_OTHER_CKSUM,
				   0,
				   pac_data->data,
				   pac_data->length,
				   &cksum);
	if (ret) {
		DEBUG(2, ("PAC Verification failed: %s\n",
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
	}

	krb5_crypto_destroy(context, crypto);

	if (ret) {
		return ret;
	}

	sig->type = cksum.cksumtype;
	sig->signature = data_blob_talloc(mem_ctx, cksum.checksum.data, cksum.checksum.length);
	free_Checksum(&cksum);

	return 0;
}

 krb5_error_code kerberos_encode_pac(TALLOC_CTX *mem_ctx,
				    struct PAC_DATA *pac_data,
				    krb5_context context,
				    const krb5_keyblock *krbtgt_keyblock,
				    const krb5_keyblock *service_keyblock,
				    DATA_BLOB *pac)
{
	NTSTATUS nt_status;
	krb5_error_code ret;
	enum ndr_err_code ndr_err;
	DATA_BLOB zero_blob = data_blob(NULL, 0);
	DATA_BLOB tmp_blob = data_blob(NULL, 0);
	struct PAC_SIGNATURE_DATA *kdc_checksum = NULL;
	struct PAC_SIGNATURE_DATA *srv_checksum = NULL;
	int i;

	/* First, just get the keytypes filled in (and lengths right, eventually) */
	for (i=0; i < pac_data->num_buffers; i++) {
		if (pac_data->buffers[i].type != PAC_TYPE_KDC_CHECKSUM) {
			continue;
		}
		kdc_checksum = &pac_data->buffers[i].info->kdc_cksum,
		ret = make_pac_checksum(mem_ctx, &zero_blob,
					kdc_checksum,
					context, krbtgt_keyblock);
		if (ret) {
			DEBUG(2, ("making krbtgt PAC checksum failed: %s\n",
				  smb_get_krb5_error_message(context, ret, mem_ctx)));
			talloc_free(pac_data);
			return ret;
		}
	}

	for (i=0; i < pac_data->num_buffers; i++) {
		if (pac_data->buffers[i].type != PAC_TYPE_SRV_CHECKSUM) {
			continue;
		}
		srv_checksum = &pac_data->buffers[i].info->srv_cksum;
		ret = make_pac_checksum(mem_ctx, &zero_blob,
					srv_checksum,
					context, service_keyblock);
		if (ret) {
			DEBUG(2, ("making service PAC checksum failed: %s\n",
				  smb_get_krb5_error_message(context, ret, mem_ctx)));
			talloc_free(pac_data);
			return ret;
		}
	}

	if (!kdc_checksum) {
		DEBUG(2, ("Invalid PAC constructed for signing, no KDC checksum present!"));
		return EINVAL;
	}
	if (!srv_checksum) {
		DEBUG(2, ("Invalid PAC constructed for signing, no SRV checksum present!"));
		return EINVAL;
	}

	/* But wipe out the actual signatures */
	memset(kdc_checksum->signature.data, '\0', kdc_checksum->signature.length);
	memset(srv_checksum->signature.data, '\0', srv_checksum->signature.length);

	ndr_err = ndr_push_struct_blob(&tmp_blob, mem_ctx,
				       pac_data,
				       (ndr_push_flags_fn_t)ndr_push_PAC_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		nt_status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(1, ("PAC (presig) push failed: %s\n", nt_errstr(nt_status)));
		talloc_free(pac_data);
		return EINVAL;
	}

	/* Then sign the result of the previous push, where the sig was zero'ed out */
	ret = make_pac_checksum(mem_ctx, &tmp_blob, srv_checksum,
				context, service_keyblock);

	/* Then sign Server checksum */
	ret = make_pac_checksum(mem_ctx, &srv_checksum->signature, kdc_checksum, context, krbtgt_keyblock);
	if (ret) {
		DEBUG(2, ("making krbtgt PAC checksum failed: %s\n",
			  smb_get_krb5_error_message(context, ret, mem_ctx)));
		talloc_free(pac_data);
		return ret;
	}

	/* And push it out again, this time to the world.  This relies on determanistic pointer values */
	ndr_err = ndr_push_struct_blob(&tmp_blob, mem_ctx,
				       pac_data,
				       (ndr_push_flags_fn_t)ndr_push_PAC_DATA);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		nt_status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(1, ("PAC (final) push failed: %s\n", nt_errstr(nt_status)));
		talloc_free(pac_data);
		return EINVAL;
	}

	*pac = tmp_blob;

	return ret;
}


 krb5_error_code kerberos_create_pac(TALLOC_CTX *mem_ctx,
				     struct auth_user_info_dc *user_info_dc,
				     krb5_context context,
				     const krb5_keyblock *krbtgt_keyblock,
				     const krb5_keyblock *service_keyblock,
				     krb5_principal client_principal,
				     time_t tgs_authtime,
				     DATA_BLOB *pac)
{
	NTSTATUS nt_status;
	krb5_error_code ret;
	struct PAC_DATA *pac_data = talloc(mem_ctx, struct PAC_DATA);
	struct netr_SamInfo3 *sam3;
	union PAC_INFO *u_LOGON_INFO;
	struct PAC_LOGON_INFO *LOGON_INFO;
	union PAC_INFO *u_LOGON_NAME;
	struct PAC_LOGON_NAME *LOGON_NAME;
	union PAC_INFO *u_KDC_CHECKSUM;
	union PAC_INFO *u_SRV_CHECKSUM;

	char *name;

	enum {
		PAC_BUF_LOGON_INFO = 0,
		PAC_BUF_LOGON_NAME = 1,
		PAC_BUF_SRV_CHECKSUM = 2,
		PAC_BUF_KDC_CHECKSUM = 3,
		PAC_BUF_NUM_BUFFERS = 4
	};

	if (!pac_data) {
		return ENOMEM;
	}

	pac_data->num_buffers = PAC_BUF_NUM_BUFFERS;
	pac_data->version = 0;

	pac_data->buffers = talloc_array(pac_data,
					 struct PAC_BUFFER,
					 pac_data->num_buffers);
	if (!pac_data->buffers) {
		talloc_free(pac_data);
		return ENOMEM;
	}

	/* LOGON_INFO */
	u_LOGON_INFO = talloc_zero(pac_data->buffers, union PAC_INFO);
	if (!u_LOGON_INFO) {
		talloc_free(pac_data);
		return ENOMEM;
	}
	pac_data->buffers[PAC_BUF_LOGON_INFO].type = PAC_TYPE_LOGON_INFO;
	pac_data->buffers[PAC_BUF_LOGON_INFO].info = u_LOGON_INFO;

	/* LOGON_NAME */
	u_LOGON_NAME = talloc_zero(pac_data->buffers, union PAC_INFO);
	if (!u_LOGON_NAME) {
		talloc_free(pac_data);
		return ENOMEM;
	}
	pac_data->buffers[PAC_BUF_LOGON_NAME].type = PAC_TYPE_LOGON_NAME;
	pac_data->buffers[PAC_BUF_LOGON_NAME].info = u_LOGON_NAME;
	LOGON_NAME = &u_LOGON_NAME->logon_name;

	/* SRV_CHECKSUM */
	u_SRV_CHECKSUM = talloc_zero(pac_data->buffers, union PAC_INFO);
	if (!u_SRV_CHECKSUM) {
		talloc_free(pac_data);
		return ENOMEM;
	}
	pac_data->buffers[PAC_BUF_SRV_CHECKSUM].type = PAC_TYPE_SRV_CHECKSUM;
	pac_data->buffers[PAC_BUF_SRV_CHECKSUM].info = u_SRV_CHECKSUM;

	/* KDC_CHECKSUM */
	u_KDC_CHECKSUM = talloc_zero(pac_data->buffers, union PAC_INFO);
	if (!u_KDC_CHECKSUM) {
		talloc_free(pac_data);
		return ENOMEM;
	}
	pac_data->buffers[PAC_BUF_KDC_CHECKSUM].type = PAC_TYPE_KDC_CHECKSUM;
	pac_data->buffers[PAC_BUF_KDC_CHECKSUM].info = u_KDC_CHECKSUM;

	/* now the real work begins... */

	LOGON_INFO = talloc_zero(u_LOGON_INFO, struct PAC_LOGON_INFO);
	if (!LOGON_INFO) {
		talloc_free(pac_data);
		return ENOMEM;
	}
	nt_status = auth_convert_user_info_dc_saminfo3(LOGON_INFO, user_info_dc, &sam3);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("Getting Samba info failed: %s\n", nt_errstr(nt_status)));
		talloc_free(pac_data);
		return EINVAL;
	}

	u_LOGON_INFO->logon_info.info		= LOGON_INFO;
	LOGON_INFO->info3 = *sam3;

	ret = krb5_unparse_name_flags(context, client_principal,
				      KRB5_PRINCIPAL_UNPARSE_NO_REALM, &name);
	if (ret) {
		return ret;
	}
	LOGON_NAME->account_name	= talloc_strdup(LOGON_NAME, name);
	free(name);
	/*
	  this logon_time field is absolutely critical. This is what
	  caused all our PAC troubles :-)
	*/
	unix_to_nt_time(&LOGON_NAME->logon_time, tgs_authtime);

	ret = kerberos_encode_pac(mem_ctx,
				  pac_data,
				  context,
				  krbtgt_keyblock,
				  service_keyblock,
				  pac);
	talloc_free(pac_data);
	return ret;
}

krb5_error_code kerberos_pac_to_user_info_dc(TALLOC_CTX *mem_ctx,
					     krb5_pac pac,
					     krb5_context context,
					     struct auth_user_info_dc **user_info_dc,
					     struct PAC_SIGNATURE_DATA *pac_srv_sig,
					     struct PAC_SIGNATURE_DATA *pac_kdc_sig)
{
	NTSTATUS nt_status;
	enum ndr_err_code ndr_err;
	krb5_error_code ret;

	DATA_BLOB pac_logon_info_in, pac_srv_checksum_in, pac_kdc_checksum_in;
	krb5_data k5pac_logon_info_in, k5pac_srv_checksum_in, k5pac_kdc_checksum_in;

	union PAC_INFO info;
	struct auth_user_info_dc *user_info_dc_out;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	if (!tmp_ctx) {
		return ENOMEM;
	}

	ret = krb5_pac_get_buffer(context, pac, PAC_TYPE_LOGON_INFO, &k5pac_logon_info_in);
	if (ret != 0) {
		talloc_free(tmp_ctx);
		return EINVAL;
	}

	pac_logon_info_in = data_blob_const(k5pac_logon_info_in.data, k5pac_logon_info_in.length);

	ndr_err = ndr_pull_union_blob(&pac_logon_info_in, tmp_ctx, &info,
				      PAC_TYPE_LOGON_INFO,
				      (ndr_pull_flags_fn_t)ndr_pull_PAC_INFO);
	krb5_data_free(&k5pac_logon_info_in);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err) || !info.logon_info.info) {
		nt_status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("can't parse the PAC LOGON_INFO: %s\n", nt_errstr(nt_status)));
		talloc_free(tmp_ctx);
		return EINVAL;
	}

	/* Pull this right into the normal auth sysstem structures */
	nt_status = make_user_info_dc_pac(mem_ctx,
					 info.logon_info.info,
					 &user_info_dc_out);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return EINVAL;
	}

	if (pac_srv_sig) {
		ret = krb5_pac_get_buffer(context, pac, PAC_TYPE_SRV_CHECKSUM, &k5pac_srv_checksum_in);
		if (ret != 0) {
			talloc_free(tmp_ctx);
			return ret;
		}

		pac_srv_checksum_in = data_blob_const(k5pac_srv_checksum_in.data, k5pac_srv_checksum_in.length);

		ndr_err = ndr_pull_struct_blob(&pac_srv_checksum_in, pac_srv_sig,
					       pac_srv_sig,
					       (ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
		krb5_data_free(&k5pac_srv_checksum_in);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			nt_status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(0,("can't parse the KDC signature: %s\n",
				 nt_errstr(nt_status)));
			return EINVAL;
		}
	}

	if (pac_kdc_sig) {
		ret = krb5_pac_get_buffer(context, pac, PAC_TYPE_KDC_CHECKSUM, &k5pac_kdc_checksum_in);
		if (ret != 0) {
			talloc_free(tmp_ctx);
			return ret;
		}

		pac_kdc_checksum_in = data_blob_const(k5pac_kdc_checksum_in.data, k5pac_kdc_checksum_in.length);

		ndr_err = ndr_pull_struct_blob(&pac_kdc_checksum_in, pac_kdc_sig,
					       pac_kdc_sig,
					       (ndr_pull_flags_fn_t)ndr_pull_PAC_SIGNATURE_DATA);
		krb5_data_free(&k5pac_kdc_checksum_in);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			nt_status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(0,("can't parse the KDC signature: %s\n",
				 nt_errstr(nt_status)));
			return EINVAL;
		}
	}
	*user_info_dc = user_info_dc_out;

	return 0;
}


NTSTATUS kerberos_pac_blob_to_user_info_dc(TALLOC_CTX *mem_ctx,
					   DATA_BLOB pac_blob,
					   krb5_context context,
					   struct auth_user_info_dc **user_info_dc,
					   struct PAC_SIGNATURE_DATA *pac_srv_sig,
					   struct PAC_SIGNATURE_DATA *pac_kdc_sig)
{
	krb5_error_code ret;
	krb5_pac pac;
	ret = krb5_pac_parse(context,
			     pac_blob.data, pac_blob.length,
			     &pac);
	if (ret) {
		return map_nt_error_from_unix(ret);
	}


	ret = kerberos_pac_to_user_info_dc(mem_ctx, pac, context, user_info_dc, pac_srv_sig, pac_kdc_sig);
	krb5_pac_free(context, pac);
	if (ret) {
		return map_nt_error_from_unix(ret);
	}
	return NT_STATUS_OK;
}
