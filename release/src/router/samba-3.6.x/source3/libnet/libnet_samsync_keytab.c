/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Guenther Deschner 2008.

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
#include "smb_krb5.h"
#include "ads.h"
#include "libnet/libnet_keytab.h"
#include "libnet/libnet_samsync.h"
#include "krb5_env.h"

#if defined(HAVE_ADS)

/****************************************************************
****************************************************************/

static NTSTATUS keytab_ad_connect(TALLOC_CTX *mem_ctx,
				  const char *domain_name,
				  const char *dc,
				  const char *username,
				  const char *password,
				  struct libnet_keytab_context *ctx)
{
	ADS_STATUS ad_status;
	ADS_STRUCT *ads;

	ads = ads_init(NULL, domain_name, dc);
	NT_STATUS_HAVE_NO_MEMORY(ads);

	if (getenv(KRB5_ENV_CCNAME) == NULL) {
		setenv(KRB5_ENV_CCNAME, "MEMORY:libnet_samsync_keytab", 1);
	}

	ads->auth.user_name = SMB_STRDUP(username);
	ads->auth.password = SMB_STRDUP(password);

	ad_status = ads_connect_user_creds(ads);
	if (!ADS_ERR_OK(ad_status)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx->ads = ads;

	ctx->dns_domain_name = talloc_strdup_upper(mem_ctx, ads->config.realm);
	NT_STATUS_HAVE_NO_MEMORY(ctx->dns_domain_name);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entry_keytab(TALLOC_CTX *mem_ctx,
				       enum netr_SamDatabaseID database_id,
				       uint32_t rid,
				       struct netr_DELTA_USER *r,
				       struct libnet_keytab_context *ctx)
{
	NTSTATUS status;
	uint32_t kvno = 0;
	DATA_BLOB blob;

	if (memcmp(r->ntpassword.hash, ctx->zero_buf, 16) == 0) {
		return NT_STATUS_OK;
	}

	kvno = ads_get_kvno(ctx->ads, r->account_name.string);
	blob = data_blob_const(r->ntpassword.hash, 16);

	status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx,
						     kvno,
						     r->account_name.string,
						     NULL,
						     ENCTYPE_ARCFOUR_HMAC,
						     blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS init_keytab(TALLOC_CTX *mem_ctx,
			    struct samsync_context *ctx,
			    enum netr_SamDatabaseID database_id,
			    uint64_t *sequence_num)
{
	krb5_error_code ret = 0;
	NTSTATUS status;
	struct libnet_keytab_context *keytab_ctx = NULL;
	struct libnet_keytab_entry *entry;
	uint64_t old_sequence_num = 0;
	const char *principal = NULL;
	struct netr_DsRGetDCNameInfo *info = NULL;
	const char *dc;

	ret = libnet_keytab_init(mem_ctx, ctx->output_filename, &keytab_ctx);
	if (ret) {
		return krb5_to_nt_status(ret);
	}

	status = dsgetdcname(mem_ctx, ctx->msg_ctx,
			     ctx->domain_name, NULL, NULL, 0, &info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dc = strip_hostname(info->dc_unc);

	keytab_ctx->clean_old_entries = ctx->clean_old_entries;
	ctx->private_data = keytab_ctx;

	status = keytab_ad_connect(mem_ctx,
				   ctx->domain_name,
				   dc,
				   ctx->username,
				   ctx->password,
				   keytab_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(keytab_ctx);
		return status;
	}

	principal = talloc_asprintf(mem_ctx, "SEQUENCE_NUM@%s",
				    keytab_ctx->dns_domain_name);
	NT_STATUS_HAVE_NO_MEMORY(principal);

	entry = libnet_keytab_search(keytab_ctx, principal, 0, ENCTYPE_NULL,
				     mem_ctx);
	if (entry && (entry->password.length == 8)) {
		old_sequence_num = BVAL(entry->password.data, 0);
	}

	if (sequence_num) {
		*sequence_num = old_sequence_num;
	}

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entries_keytab(TALLOC_CTX *mem_ctx,
					 enum netr_SamDatabaseID database_id,
					 struct netr_DELTA_ENUM_ARRAY *r,
					 uint64_t *sequence_num,
					 struct samsync_context *ctx)
{
	struct libnet_keytab_context *keytab_ctx =
		(struct libnet_keytab_context *)ctx->private_data;

	NTSTATUS status = NT_STATUS_OK;
	int i;

	for (i = 0; i < r->num_deltas; i++) {

		switch (r->delta_enum[i].delta_type) {
		case NETR_DELTA_USER:
			break;
		case NETR_DELTA_DOMAIN:
			if (sequence_num) {
				*sequence_num =
					r->delta_enum[i].delta_union.domain->sequence_num;
			}
			continue;
		case NETR_DELTA_MODIFY_COUNT:
			if (sequence_num) {
				*sequence_num =
					*r->delta_enum[i].delta_union.modified_count;
			}
			continue;
		default:
			continue;
		}

		status = fetch_sam_entry_keytab(mem_ctx, database_id,
						r->delta_enum[i].delta_id_union.rid,
						r->delta_enum[i].delta_union.user,
						keytab_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
	}
 out:
	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS close_keytab(TALLOC_CTX *mem_ctx,
			     struct samsync_context *ctx,
			     enum netr_SamDatabaseID database_id,
			     uint64_t sequence_num)
{
	struct libnet_keytab_context *keytab_ctx =
		(struct libnet_keytab_context *)ctx->private_data;
	krb5_error_code ret;
	NTSTATUS status;
	struct libnet_keytab_entry *entry;
	uint64_t old_sequence_num = 0;
	const char *principal = NULL;

	principal = talloc_asprintf(mem_ctx, "SEQUENCE_NUM@%s",
				    keytab_ctx->dns_domain_name);
	NT_STATUS_HAVE_NO_MEMORY(principal);


	entry = libnet_keytab_search(keytab_ctx, principal, 0, ENCTYPE_NULL,
				     mem_ctx);
	if (entry && (entry->password.length == 8)) {
		old_sequence_num = BVAL(entry->password.data, 0);
	}


	if (sequence_num > old_sequence_num) {
		DATA_BLOB blob;
		blob = data_blob_talloc_zero(mem_ctx, 8);
		SBVAL(blob.data, 0, sequence_num);

		status = libnet_keytab_add_to_keytab_entries(mem_ctx, keytab_ctx,
							     0,
							     "SEQUENCE_NUM",
							     NULL,
							     ENCTYPE_NULL,
							     blob);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
	}

	ret = libnet_keytab_add(keytab_ctx);
	if (ret) {
		status = krb5_to_nt_status(ret);
		ctx->error_message = talloc_asprintf(ctx,
			"Failed to add entries to keytab %s: %s",
			keytab_ctx->keytab_name, error_message(ret));
		TALLOC_FREE(keytab_ctx);
		return status;
	}

	ctx->result_message = talloc_asprintf(ctx,
		"Vampired %d accounts to keytab %s",
		keytab_ctx->count,
		keytab_ctx->keytab_name);

	status = NT_STATUS_OK;

 done:
	TALLOC_FREE(keytab_ctx);

	return status;
}

#else

static NTSTATUS init_keytab(TALLOC_CTX *mem_ctx,
			    struct samsync_context *ctx,
			    enum netr_SamDatabaseID database_id,
			    uint64_t *sequence_num)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS fetch_sam_entries_keytab(TALLOC_CTX *mem_ctx,
					 enum netr_SamDatabaseID database_id,
					 struct netr_DELTA_ENUM_ARRAY *r,
					 uint64_t *sequence_num,
					 struct samsync_context *ctx)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS close_keytab(TALLOC_CTX *mem_ctx,
			     struct samsync_context *ctx,
			     enum netr_SamDatabaseID database_id,
			     uint64_t sequence_num)
{
	return NT_STATUS_NOT_SUPPORTED;
}

#endif /* defined(HAVE_ADS) */

const struct samsync_ops libnet_samsync_keytab_ops = {
	.startup		= init_keytab,
	.process_objects	= fetch_sam_entries_keytab,
	.finish			= close_keytab
};
