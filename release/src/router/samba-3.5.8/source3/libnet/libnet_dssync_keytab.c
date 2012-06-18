/*
   Unix SMB/CIFS implementation.

   Copyright (C) Guenther Deschner <gd@samba.org> 2008
   Copyright (C) Michael Adam 2008

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
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

#if defined(HAVE_ADS) && defined(ENCTYPE_ARCFOUR_HMAC)

static NTSTATUS keytab_startup(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			       struct replUpToDateVectorBlob **pold_utdv)
{
	krb5_error_code ret = 0;
	struct libnet_keytab_context *keytab_ctx;
	struct libnet_keytab_entry *entry;
	struct replUpToDateVectorBlob *old_utdv = NULL;
	char *principal;

	ret = libnet_keytab_init(mem_ctx, ctx->output_filename, &keytab_ctx);
	if (ret) {
		return krb5_to_nt_status(ret);
	}

	keytab_ctx->dns_domain_name = ctx->dns_domain_name;
	keytab_ctx->clean_old_entries = ctx->clean_old_entries;
	ctx->private_data = keytab_ctx;

	principal = talloc_asprintf(mem_ctx, "UTDV/%s@%s",
				    ctx->nc_dn, ctx->dns_domain_name);
	NT_STATUS_HAVE_NO_MEMORY(principal);

	entry = libnet_keytab_search(keytab_ctx, principal, 0, ENCTYPE_NULL,
				     mem_ctx);
	if (entry) {
		enum ndr_err_code ndr_err;
		old_utdv = talloc(mem_ctx, struct replUpToDateVectorBlob);

		ndr_err = ndr_pull_struct_blob(&entry->password, old_utdv,
				NULL, old_utdv,
				(ndr_pull_flags_fn_t)ndr_pull_replUpToDateVectorBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ctx->error_message = talloc_asprintf(ctx,
					"Failed to pull UpToDateVector: %s",
					nt_errstr(status));
			return status;
		}

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(replUpToDateVectorBlob, old_utdv);
		}
	}

	if (pold_utdv) {
		*pold_utdv = old_utdv;
	}

	return NT_STATUS_OK;
}

static NTSTATUS keytab_finish(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			      struct replUpToDateVectorBlob *new_utdv)
{
	NTSTATUS status = NT_STATUS_OK;
	krb5_error_code ret = 0;
	struct libnet_keytab_context *keytab_ctx =
		(struct libnet_keytab_context *)ctx->private_data;

	if (new_utdv) {
		enum ndr_err_code ndr_err;
		DATA_BLOB blob;

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(replUpToDateVectorBlob, new_utdv);
		}

		ndr_err = ndr_push_struct_blob(&blob, mem_ctx, NULL, new_utdv,
				(ndr_push_flags_fn_t)ndr_push_replUpToDateVectorBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			ctx->error_message = talloc_asprintf(ctx,
					"Failed to push UpToDateVector: %s",
					nt_errstr(status));
			goto done;
		}

		status = libnet_keytab_add_to_keytab_entries(mem_ctx, keytab_ctx, 0,
							     ctx->nc_dn, "UTDV",
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
		goto done;
	}

	ctx->result_message = talloc_asprintf(ctx,
		"Vampired %d accounts to keytab %s",
		keytab_ctx->count,
		keytab_ctx->keytab_name);

done:
	TALLOC_FREE(keytab_ctx);
	return status;
}

/****************************************************************
****************************************************************/

static  NTSTATUS parse_supplemental_credentials(TALLOC_CTX *mem_ctx,
			const DATA_BLOB *blob,
			struct package_PrimaryKerberosCtr3 **pkb3,
			struct package_PrimaryKerberosCtr4 **pkb4)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct supplementalCredentialsBlob scb;
	struct supplementalCredentialsPackage *scpk = NULL;
	DATA_BLOB scpk_blob;
	struct package_PrimaryKerberosBlob *pkb;
	bool newer_keys = false;
	uint32_t j;

	ndr_err = ndr_pull_struct_blob_all(blob, mem_ctx, NULL, &scb,
			(ndr_pull_flags_fn_t)ndr_pull_supplementalCredentialsBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}
	if (scb.sub.signature !=
	    SUPPLEMENTAL_CREDENTIALS_SIGNATURE)
	{
		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(supplementalCredentialsBlob, &scb);
		}
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}
	for (j=0; j < scb.sub.num_packages; j++) {
		if (strcmp("Primary:Kerberos-Newer-Keys",
		    scb.sub.packages[j].name) == 0)
		{
			scpk = &scb.sub.packages[j];
			if (!scpk->data || !scpk->data[0]) {
				scpk = NULL;
				continue;
			}
			newer_keys = true;
			break;
		} else  if (strcmp("Primary:Kerberos",
				   scb.sub.packages[j].name) == 0)
		{
			/*
			 * grab this but don't break here:
			 * there might still be newer-keys ...
			 */
			scpk = &scb.sub.packages[j];
			if (!scpk->data || !scpk->data[0]) {
				scpk = NULL;
			}
		}
	}

	if (!scpk) {
		/* no data */
		status = NT_STATUS_OK;
		goto done;
	}

	scpk_blob = strhex_to_data_blob(mem_ctx, scpk->data);
	if (!scpk_blob.data) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	pkb = TALLOC_ZERO_P(mem_ctx, struct package_PrimaryKerberosBlob);
	if (!pkb) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	ndr_err = ndr_pull_struct_blob(&scpk_blob, mem_ctx, NULL, pkb,
			(ndr_pull_flags_fn_t)ndr_pull_package_PrimaryKerberosBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}

	if (!newer_keys && pkb->version != 3) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (newer_keys && pkb->version != 4) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (pkb->version == 4 && pkb4) {
		*pkb4 = &pkb->ctr.ctr4;
	} else if (pkb->version == 3 && pkb3) {
		*pkb3 = &pkb->ctr.ctr3;
	}

	status = NT_STATUS_OK;

done:
	return status;
}

static NTSTATUS parse_object(TALLOC_CTX *mem_ctx,
			     struct libnet_keytab_context *ctx,
			     struct drsuapi_DsReplicaObjectListItemEx *cur)
{
	NTSTATUS status = NT_STATUS_OK;
	uchar nt_passwd[16];
	DATA_BLOB *blob;
	int i = 0;
	struct drsuapi_DsReplicaAttribute *attr;
	bool got_pwd = false;

	struct package_PrimaryKerberosCtr3 *pkb3 = NULL;
	struct package_PrimaryKerberosCtr4 *pkb4 = NULL;

	char *object_dn = NULL;
	char *upn = NULL;
	char **spn = NULL;
	uint32_t num_spns = 0;
	char *name = NULL;
	uint32_t kvno = 0;
	uint32_t uacc = 0;
	uint32_t sam_type = 0;

	uint32_t pwd_history_len = 0;
	uint8_t *pwd_history = NULL;

	ZERO_STRUCT(nt_passwd);

	object_dn = talloc_strdup(mem_ctx, cur->object.identifier->dn);
	if (!object_dn) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(3, ("parsing object '%s'\n", object_dn));

	for (i=0; i < cur->object.attribute_ctr.num_attributes; i++) {

		attr = &cur->object.attribute_ctr.attributes[i];

		if (attr->attid == DRSUAPI_ATTRIBUTE_servicePrincipalName) {
			uint32_t count;
			num_spns = attr->value_ctr.num_values;
			spn = TALLOC_ARRAY(mem_ctx, char *, num_spns);
			for (count = 0; count < num_spns; count++) {
				blob = attr->value_ctr.values[count].blob;
				pull_string_talloc(spn, NULL, 0,
						   &spn[count],
						   blob->data, blob->length,
						   STR_UNICODE);
			}
		}

		if (attr->value_ctr.num_values != 1) {
			continue;
		}

		if (!attr->value_ctr.values[0].blob) {
			continue;
		}

		blob = attr->value_ctr.values[0].blob;

		switch (attr->attid) {
			case DRSUAPI_ATTRIBUTE_unicodePwd:

				if (blob->length != 16) {
					break;
				}

				memcpy(&nt_passwd, blob->data, 16);
				got_pwd = true;

				/* pick the kvno from the meta_data version,
				 * thanks, metze, for explaining this */

				if (!cur->meta_data_ctr) {
					break;
				}
				if (cur->meta_data_ctr->count !=
				    cur->object.attribute_ctr.num_attributes) {
					break;
				}
				kvno = cur->meta_data_ctr->meta_data[i].version;
				break;
			case DRSUAPI_ATTRIBUTE_ntPwdHistory:
				pwd_history_len = blob->length / 16;
				pwd_history = blob->data;
				break;
			case DRSUAPI_ATTRIBUTE_userPrincipalName:
				pull_string_talloc(mem_ctx, NULL, 0, &upn,
						   blob->data, blob->length,
						   STR_UNICODE);
				break;
			case DRSUAPI_ATTRIBUTE_sAMAccountName:
				pull_string_talloc(mem_ctx, NULL, 0, &name,
						   blob->data, blob->length,
						   STR_UNICODE);
				break;
			case DRSUAPI_ATTRIBUTE_sAMAccountType:
				sam_type = IVAL(blob->data, 0);
				break;
			case DRSUAPI_ATTRIBUTE_userAccountControl:
				uacc = IVAL(blob->data, 0);
				break;
			case DRSUAPI_ATTRIBUTE_supplementalCredentials:
				status = parse_supplemental_credentials(mem_ctx,
									blob,
									&pkb3,
									&pkb4);
				if (!NT_STATUS_IS_OK(status)) {
					DEBUG(2, ("parsing of supplemental "
						  "credentials failed: %s\n",
						  nt_errstr(status)));
				}
				break;
			default:
				break;
		}
	}

	if (!got_pwd) {
		DEBUG(10, ("no password (unicodePwd) found - skipping.\n"));
		return NT_STATUS_OK;
	}

	if (name) {
		status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, 0, object_dn,
							     "SAMACCOUNTNAME",
							     ENCTYPE_NULL,
							     data_blob_talloc(mem_ctx, name,
							     strlen(name) + 1));
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else {
		/* look into keytab ... */
		struct libnet_keytab_entry *entry = NULL;
		char *principal = NULL;

		DEBUG(10, ("looking for SAMACCOUNTNAME/%s@%s in keytayb...\n",
			   object_dn, ctx->dns_domain_name));

		principal = talloc_asprintf(mem_ctx, "%s/%s@%s",
					    "SAMACCOUNTNAME",
					    object_dn,
					    ctx->dns_domain_name);
		if (!principal) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
		entry = libnet_keytab_search(ctx, principal, 0, ENCTYPE_NULL,
					     mem_ctx);
		if (entry) {
			name = (char *)TALLOC_MEMDUP(mem_ctx,
						     entry->password.data,
						     entry->password.length);
			if (!name) {
				DEBUG(1, ("talloc failed!"));
				return NT_STATUS_NO_MEMORY;
			} else {
				DEBUG(10, ("found name %s\n", name));
			}
			TALLOC_FREE(entry);
		} else {
			DEBUG(10, ("entry not found\n"));
		}
		TALLOC_FREE(principal);
	}

	if (!name) {
		DEBUG(10, ("no name (sAMAccountName) found - skipping.\n"));
		return NT_STATUS_OK;
	}

	DEBUG(1,("#%02d: %s:%d, ", ctx->count, name, kvno));
	DEBUGADD(1,("sAMAccountType: 0x%08x, userAccountControl: 0x%08x",
		sam_type, uacc));
	if (upn) {
		DEBUGADD(1,(", upn: %s", upn));
	}
	if (num_spns > 0) {
		DEBUGADD(1, (", spns: ["));
		for (i = 0; i < num_spns; i++) {
			DEBUGADD(1, ("%s%s", spn[i],
				     (i+1 == num_spns)?"]":", "));
		}
	}
	DEBUGADD(1,("\n"));

	status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno, name, NULL,
						     ENCTYPE_ARCFOUR_HMAC,
						     data_blob_talloc(mem_ctx, nt_passwd, 16));

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* add kerberos keys (if any) */

	if (pkb4) {
		for (i=0; i < pkb4->num_keys; i++) {
			if (!pkb4->keys[i].value) {
				continue;
			}
			status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno,
								     name,
								     NULL,
								     pkb4->keys[i].keytype,
								     *pkb4->keys[i].value);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
		for (i=0; i < pkb4->num_old_keys; i++) {
			if (!pkb4->old_keys[i].value) {
				continue;
			}
			status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno - 1,
								     name,
								     NULL,
								     pkb4->old_keys[i].keytype,
								     *pkb4->old_keys[i].value);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
		for (i=0; i < pkb4->num_older_keys; i++) {
			if (!pkb4->older_keys[i].value) {
				continue;
			}
			status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno - 2,
								     name,
								     NULL,
								     pkb4->older_keys[i].keytype,
								     *pkb4->older_keys[i].value);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
	}

	if (pkb3) {
		for (i=0; i < pkb3->num_keys; i++) {
			if (!pkb3->keys[i].value) {
				continue;
			}
			status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno, name,
								     NULL,
								     pkb3->keys[i].keytype,
								     *pkb3->keys[i].value);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
		for (i=0; i < pkb3->num_old_keys; i++) {
			if (!pkb3->old_keys[i].value) {
				continue;
			}
			status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno - 1,
								     name,
								     NULL,
								     pkb3->old_keys[i].keytype,
								     *pkb3->old_keys[i].value);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
	}

	if ((kvno < 0) && (kvno < pwd_history_len)) {
		return status;
	}

	/* add password history */

	/* skip first entry */
	if (got_pwd) {
		kvno--;
		i = 1;
	} else {
		i = 0;
	}

	for (; i<pwd_history_len; i++) {
		status = libnet_keytab_add_to_keytab_entries(mem_ctx, ctx, kvno--, name, NULL,
							     ENCTYPE_ARCFOUR_HMAC,
							     data_blob_talloc(mem_ctx, &pwd_history[i*16], 16));
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}
	}

	return status;
}

static bool dn_is_in_object_list(struct dssync_context *ctx,
				 const char *dn)
{
	uint32_t count;

	if (ctx->object_count == 0) {
		return true;
	}

	for (count = 0; count < ctx->object_count; count++) {
		if (strequal(ctx->object_dns[count], dn)) {
			return true;
		}
	}

	return false;
}

/****************************************************************
****************************************************************/

static NTSTATUS keytab_process_objects(struct dssync_context *ctx,
				       TALLOC_CTX *mem_ctx,
				       struct drsuapi_DsReplicaObjectListItemEx *cur,
				       struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr)
{
	NTSTATUS status = NT_STATUS_OK;
	struct libnet_keytab_context *keytab_ctx =
		(struct libnet_keytab_context *)ctx->private_data;

	for (; cur; cur = cur->next_object) {
		/*
		 * When not in single object replication mode,
		 * the object_dn list is used as a positive write filter.
		 */
		if (!ctx->single_object_replication &&
		    !dn_is_in_object_list(ctx, cur->object.identifier->dn))
		{
			continue;
		}

		status = parse_object(mem_ctx, keytab_ctx, cur);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
	}

 out:
	return status;
}

#else

static NTSTATUS keytab_startup(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			       struct replUpToDateVectorBlob **pold_utdv)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS keytab_finish(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			      struct replUpToDateVectorBlob *new_utdv)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS keytab_process_objects(struct dssync_context *ctx,
				       TALLOC_CTX *mem_ctx,
				       struct drsuapi_DsReplicaObjectListItemEx *cur,
				       struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr)
{
	return NT_STATUS_NOT_SUPPORTED;
}
#endif /* defined(HAVE_ADS) && defined(ENCTYPE_ARCFOUR_HMAC) */

const struct dssync_ops libnet_dssync_keytab_ops = {
	.startup		= keytab_startup,
	.process_objects	= keytab_process_objects,
	.finish			= keytab_finish,
};
