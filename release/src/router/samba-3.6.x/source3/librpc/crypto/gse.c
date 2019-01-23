/*
 *  GSSAPI Security Extensions
 *  RPC Pipe client and server routines
 *  Copyright (C) Simo Sorce 2010.
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

/* We support only GSSAPI/KRB5 here */

#include "includes.h"
#include "gse.h"

#if defined(HAVE_KRB5) \
	&& defined(HAVE_GSSAPI_GSSAPI_EXT_H) \
	&& defined(HAVE_GSS_WRAP_IOV) \
	&& defined(HAVE_GSS_GET_NAME_ATTRIBUTE)

#include "smb_krb5.h"
#include "gse_krb5.h"

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <gssapi/gssapi_ext.h>

#ifndef GSS_KRB5_INQ_SSPI_SESSION_KEY_OID
#define GSS_KRB5_INQ_SSPI_SESSION_KEY_OID_LENGTH 11
#define GSS_KRB5_INQ_SSPI_SESSION_KEY_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x05"
#endif

gss_OID_desc gse_sesskey_inq_oid = {
	GSS_KRB5_INQ_SSPI_SESSION_KEY_OID_LENGTH,
	(void *)GSS_KRB5_INQ_SSPI_SESSION_KEY_OID
};

#ifndef GSS_KRB5_SESSION_KEY_ENCTYPE_OID
#define GSS_KRB5_SESSION_KEY_ENCTYPE_OID_LENGTH 10
#define GSS_KRB5_SESSION_KEY_ENCTYPE_OID  "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x04"
#endif

gss_OID_desc gse_sesskeytype_oid = {
	GSS_KRB5_SESSION_KEY_ENCTYPE_OID_LENGTH,
	(void *)GSS_KRB5_SESSION_KEY_ENCTYPE_OID
};

#define GSE_EXTRACT_RELEVANT_AUTHZ_DATA_OID_LENGTH 12
/*					    EXTRACTION OID				   AUTHZ ID */
#define GSE_EXTRACT_RELEVANT_AUTHZ_DATA_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0a" "\x01"

gss_OID_desc gse_authz_data_oid = {
	GSE_EXTRACT_RELEVANT_AUTHZ_DATA_OID_LENGTH,
	(void *)GSE_EXTRACT_RELEVANT_AUTHZ_DATA_OID
};

#ifndef GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID
#define GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID_LENGTH 11
#define GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x0c"
#endif

gss_OID_desc gse_authtime_oid = {
	GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID_LENGTH,
	(void *)GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID
};

static char *gse_errstr(TALLOC_CTX *mem_ctx, OM_uint32 maj, OM_uint32 min);

struct gse_context {
	krb5_context k5ctx;
	krb5_ccache ccache;
	krb5_keytab keytab;

	gss_ctx_id_t gss_ctx;

	gss_OID_desc gss_mech;
	OM_uint32 gss_c_flags;
	gss_cred_id_t creds;
	gss_name_t server_name;

	gss_OID ret_mech;
	OM_uint32 ret_flags;
	gss_cred_id_t delegated_creds;
	gss_name_t client_name;

	bool more_processing;
	bool authenticated;
};

/* free non talloc dependent contexts */
static int gse_context_destructor(void *ptr)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_min, gss_maj;

	gse_ctx = talloc_get_type_abort(ptr, struct gse_context);
	if (gse_ctx->k5ctx) {
		if (gse_ctx->ccache) {
			krb5_cc_close(gse_ctx->k5ctx, gse_ctx->ccache);
			gse_ctx->ccache = NULL;
		}
		if (gse_ctx->keytab) {
			krb5_kt_close(gse_ctx->k5ctx, gse_ctx->keytab);
			gse_ctx->keytab = NULL;
		}
		krb5_free_context(gse_ctx->k5ctx);
		gse_ctx->k5ctx = NULL;
	}
	if (gse_ctx->gss_ctx != GSS_C_NO_CONTEXT) {
		gss_maj = gss_delete_sec_context(&gss_min,
						 &gse_ctx->gss_ctx,
						 GSS_C_NO_BUFFER);
	}
	if (gse_ctx->server_name) {
		gss_maj = gss_release_name(&gss_min,
					   &gse_ctx->server_name);
	}
	if (gse_ctx->client_name) {
		gss_maj = gss_release_name(&gss_min,
					   &gse_ctx->client_name);
	}
	if (gse_ctx->creds) {
		gss_maj = gss_release_cred(&gss_min,
					   &gse_ctx->creds);
	}
	if (gse_ctx->delegated_creds) {
		gss_maj = gss_release_cred(&gss_min,
					   &gse_ctx->delegated_creds);
	}
	if (gse_ctx->ret_mech) {
		gss_maj = gss_release_oid(&gss_min,
					  &gse_ctx->ret_mech);
	}
	return 0;
}

static NTSTATUS gse_context_init(TALLOC_CTX *mem_ctx,
				 bool do_sign, bool do_seal,
				 const char *ccache_name,
				 uint32_t add_gss_c_flags,
				 struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	krb5_error_code k5ret;
	NTSTATUS status;

	gse_ctx = talloc_zero(mem_ctx, struct gse_context);
	if (!gse_ctx) {
		return NT_STATUS_NO_MEMORY;
	}
	talloc_set_destructor((TALLOC_CTX *)gse_ctx, gse_context_destructor);

	memcpy(&gse_ctx->gss_mech, gss_mech_krb5, sizeof(gss_OID_desc));

	gse_ctx->gss_c_flags = GSS_C_MUTUAL_FLAG |
				GSS_C_DELEG_FLAG |
				GSS_C_DELEG_POLICY_FLAG |
				GSS_C_REPLAY_FLAG |
				GSS_C_SEQUENCE_FLAG;
	if (do_sign) {
		gse_ctx->gss_c_flags |= GSS_C_INTEG_FLAG;
	}
	if (do_seal) {
		gse_ctx->gss_c_flags |= GSS_C_CONF_FLAG;
	}

	gse_ctx->gss_c_flags |= add_gss_c_flags;

	/* Initialize Kerberos Context */
	initialize_krb5_error_table();

	k5ret = krb5_init_context(&gse_ctx->k5ctx);
	if (k5ret) {
		DEBUG(0, ("Failed to initialize kerberos context! (%s)\n",
			  error_message(k5ret)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	if (!ccache_name) {
		ccache_name = krb5_cc_default_name(gse_ctx->k5ctx);
	}
	k5ret = krb5_cc_resolve(gse_ctx->k5ctx, ccache_name,
				&gse_ctx->ccache);
	if (k5ret) {
		DEBUG(1, ("Failed to resolve credential cache! (%s)\n",
			  error_message(k5ret)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	/* TODO: Should we enforce a enc_types list ?
	ret = krb5_set_default_tgs_ktypes(gse_ctx->k5ctx, enc_types);
	*/

	*_gse_ctx = gse_ctx;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(gse_ctx);
	return status;
}

NTSTATUS gse_init_client(TALLOC_CTX *mem_ctx,
			  bool do_sign, bool do_seal,
			  const char *ccache_name,
			  const char *server,
			  const char *service,
			  const char *username,
			  const char *password,
			  uint32_t add_gss_c_flags,
			  struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_maj, gss_min;
	gss_buffer_desc name_buffer = {0, NULL};
	gss_OID_set_desc mech_set;
	NTSTATUS status;

	if (!server || !service) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = gse_context_init(mem_ctx, do_sign, do_seal,
				  ccache_name, add_gss_c_flags,
				  &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_MEMORY;
	}

	name_buffer.value = talloc_asprintf(gse_ctx,
					    "%s@%s", service, server);
	if (!name_buffer.value) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}
	name_buffer.length = strlen((char *)name_buffer.value);
	gss_maj = gss_import_name(&gss_min, &name_buffer,
				  GSS_C_NT_HOSTBASED_SERVICE,
				  &gse_ctx->server_name);
	if (gss_maj) {
		DEBUG(0, ("gss_import_name failed for %s, with [%s]\n",
			  (char *)name_buffer.value,
			  gse_errstr(gse_ctx, gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	/* TODO: get krb5 ticket using username/password, if no valid
	 * one already available in ccache */

	mech_set.count = 1;
	mech_set.elements = &gse_ctx->gss_mech;

	gss_maj = gss_acquire_cred(&gss_min,
				   GSS_C_NO_NAME,
				   GSS_C_INDEFINITE,
				   &mech_set,
				   GSS_C_INITIATE,
				   &gse_ctx->creds,
				   NULL, NULL);
	if (gss_maj) {
		DEBUG(0, ("gss_acquire_creds failed for %s, with [%s]\n",
			  (char *)name_buffer.value,
			  gse_errstr(gse_ctx, gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	*_gse_ctx = gse_ctx;
	TALLOC_FREE(name_buffer.value);
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(name_buffer.value);
	TALLOC_FREE(gse_ctx);
	return status;
}

NTSTATUS gse_get_client_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out)
{
	OM_uint32 gss_maj, gss_min;
	gss_buffer_desc in_data;
	gss_buffer_desc out_data;
	DATA_BLOB blob = data_blob_null;
	NTSTATUS status;

	in_data.value = token_in->data;
	in_data.length = token_in->length;

	gss_maj = gss_init_sec_context(&gss_min,
					gse_ctx->creds,
					&gse_ctx->gss_ctx,
					gse_ctx->server_name,
					&gse_ctx->gss_mech,
					gse_ctx->gss_c_flags,
					0, GSS_C_NO_CHANNEL_BINDINGS,
					&in_data, NULL, &out_data,
					NULL, NULL);
	switch (gss_maj) {
	case GSS_S_COMPLETE:
		/* we are done with it */
		gse_ctx->more_processing = false;
		status = NT_STATUS_OK;
		break;
	case GSS_S_CONTINUE_NEEDED:
		/* we will need a third leg */
		gse_ctx->more_processing = true;
		/* status = NT_STATUS_MORE_PROCESSING_REQUIRED; */
		status = NT_STATUS_OK;
		break;
	default:
		DEBUG(0, ("gss_init_sec_context failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	blob = data_blob_talloc(mem_ctx, out_data.value, out_data.length);
	if (!blob.data) {
		status = NT_STATUS_NO_MEMORY;
	}

	gss_maj = gss_release_buffer(&gss_min, &out_data);

done:
	*token_out = blob;
	return status;
}

NTSTATUS gse_init_server(TALLOC_CTX *mem_ctx,
			 bool do_sign, bool do_seal,
			 uint32_t add_gss_c_flags,
			 const char *keytab_name,
			 struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_maj, gss_min;
	gss_OID_set_desc mech_set;
	krb5_error_code ret;
	const char *ktname;
	NTSTATUS status;

	status = gse_context_init(mem_ctx, do_sign, do_seal,
				  NULL, add_gss_c_flags, &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!keytab_name) {
		ret = gse_krb5_get_server_keytab(gse_ctx->k5ctx,
						 &gse_ctx->keytab);
		if (ret) {
			status = NT_STATUS_INTERNAL_ERROR;
			goto done;
		}
		ret = smb_krb5_keytab_name(gse_ctx, gse_ctx->k5ctx,
					   gse_ctx->keytab, &ktname);
		if (ret) {
			status = NT_STATUS_INTERNAL_ERROR;
			goto done;
		}
	} else {
		ktname = keytab_name;
	}

	/* FIXME!!!
	 * This call sets the default keytab for the whole server, not
	 * just for this context. Need to find a way that does not alter
	 * the state of the whole server ... */
	ret = gsskrb5_register_acceptor_identity(ktname);
	if (ret) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	mech_set.count = 1;
	mech_set.elements = &gse_ctx->gss_mech;

	gss_maj = gss_acquire_cred(&gss_min,
				   GSS_C_NO_NAME,
				   GSS_C_INDEFINITE,
				   &mech_set,
				   GSS_C_ACCEPT,
				   &gse_ctx->creds,
				   NULL, NULL);
	if (gss_maj) {
		DEBUG(0, ("gss_acquire_creds failed with [%s]\n",
			  gse_errstr(gse_ctx, gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(gse_ctx);
	}

	*_gse_ctx = gse_ctx;
	return status;
}

NTSTATUS gse_get_server_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out)
{
	OM_uint32 gss_maj, gss_min;
	gss_buffer_desc in_data;
	gss_buffer_desc out_data;
	DATA_BLOB blob = data_blob_null;
	NTSTATUS status;

	in_data.value = token_in->data;
	in_data.length = token_in->length;

	gss_maj = gss_accept_sec_context(&gss_min,
					 &gse_ctx->gss_ctx,
					 gse_ctx->creds,
					 &in_data,
					 GSS_C_NO_CHANNEL_BINDINGS,
					 &gse_ctx->client_name,
					 &gse_ctx->ret_mech,
					 &out_data,
					 &gse_ctx->ret_flags, NULL,
					 &gse_ctx->delegated_creds);
	switch (gss_maj) {
	case GSS_S_COMPLETE:
		/* we are done with it */
		gse_ctx->more_processing = false;
		gse_ctx->authenticated = true;
		status = NT_STATUS_OK;
		break;
	case GSS_S_CONTINUE_NEEDED:
		/* we will need a third leg */
		gse_ctx->more_processing = true;
		/* status = NT_STATUS_MORE_PROCESSING_REQUIRED; */
		status = NT_STATUS_OK;
		break;
	default:
		DEBUG(0, ("gss_init_sec_context failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));

		if (gse_ctx->gss_ctx) {
			gss_delete_sec_context(&gss_min,
						&gse_ctx->gss_ctx,
						GSS_C_NO_BUFFER);
		}

		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	/* we may be told to return nothing */
	if (out_data.length) {
		blob = data_blob_talloc(mem_ctx, out_data.value, out_data.length);
		if (!blob.data) {
			status = NT_STATUS_NO_MEMORY;
		}
		gss_maj = gss_release_buffer(&gss_min, &out_data);
	}


done:
	*token_out = blob;
	return status;
}

NTSTATUS gse_verify_server_auth_flags(struct gse_context *gse_ctx)
{
	if (!gse_ctx->authenticated) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (memcmp(gse_ctx->ret_mech,
		   gss_mech_krb5, sizeof(gss_OID_desc)) != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* GSS_C_MUTUAL_FLAG */
	if (gse_ctx->gss_c_flags & GSS_C_MUTUAL_FLAG) {
		if (!(gse_ctx->ret_flags & GSS_C_MUTUAL_FLAG)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/* GSS_C_DELEG_FLAG */
	/* GSS_C_DELEG_POLICY_FLAG */
	/* GSS_C_REPLAY_FLAG */
	/* GSS_C_SEQUENCE_FLAG */

	/* GSS_C_INTEG_FLAG */
	if (gse_ctx->gss_c_flags & GSS_C_INTEG_FLAG) {
		if (!(gse_ctx->ret_flags & GSS_C_INTEG_FLAG)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/* GSS_C_CONF_FLAG */
	if (gse_ctx->gss_c_flags & GSS_C_CONF_FLAG) {
		if (!(gse_ctx->ret_flags & GSS_C_CONF_FLAG)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	return NT_STATUS_OK;
}

static char *gse_errstr(TALLOC_CTX *mem_ctx, OM_uint32 maj, OM_uint32 min)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_desc msg_min;
	gss_buffer_desc msg_maj;
	OM_uint32 msg_ctx = 0;

	char *errstr = NULL;

	ZERO_STRUCT(msg_min);
	ZERO_STRUCT(msg_maj);

	gss_maj = gss_display_status(&gss_min, maj, GSS_C_GSS_CODE,
				     GSS_C_NO_OID, &msg_ctx, &msg_maj);
	if (gss_maj) {
		goto done;
	}
	gss_maj = gss_display_status(&gss_min, min, GSS_C_MECH_CODE,
				     (gss_OID)discard_const(gss_mech_krb5),
				     &msg_ctx, &msg_min);
	if (gss_maj) {
		goto done;
	}

	errstr = talloc_strndup(mem_ctx,
				(char *)msg_maj.value,
					msg_maj.length);
	if (!errstr) {
		goto done;
	}
	errstr = talloc_strdup_append_buffer(errstr, ": ");
	if (!errstr) {
		goto done;
	}
	errstr = talloc_strndup_append_buffer(errstr,
						(char *)msg_min.value,
							msg_min.length);
	if (!errstr) {
		goto done;
	}

done:
	if (msg_min.value) {
		gss_maj = gss_release_buffer(&gss_min, &msg_min);
	}
	if (msg_maj.value) {
		gss_maj = gss_release_buffer(&gss_min, &msg_maj);
	}
	return errstr;
}

bool gse_require_more_processing(struct gse_context *gse_ctx)
{
	return gse_ctx->more_processing;
}

DATA_BLOB gse_get_session_key(TALLOC_CTX *mem_ctx,
				struct gse_context *gse_ctx)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_set_t set = GSS_C_NO_BUFFER_SET;
	DATA_BLOB ret;

	gss_maj = gss_inquire_sec_context_by_oid(
				&gss_min, gse_ctx->gss_ctx,
				&gse_sesskey_inq_oid, &set);
	if (gss_maj) {
		DEBUG(0, ("gss_inquire_sec_context_by_oid failed [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		return data_blob_null;
	}

	if ((set == GSS_C_NO_BUFFER_SET) ||
	    (set->count != 2) ||
	    (memcmp(set->elements[1].value,
		    gse_sesskeytype_oid.elements,
		    gse_sesskeytype_oid.length) != 0)) {
		DEBUG(0, ("gss_inquire_sec_context_by_oid returned unknown "
			  "OID for data in results:\n"));
		dump_data(1, (uint8_t *)set->elements[1].value,
			     set->elements[1].length);
		return data_blob_null;
	}

	ret = data_blob_talloc(mem_ctx, set->elements[0].value,
					set->elements[0].length);

	gss_maj = gss_release_buffer_set(&gss_min, &set);
	return ret;
}

NTSTATUS gse_get_client_name(struct gse_context *gse_ctx,
			     TALLOC_CTX *mem_ctx, char **cli_name)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_desc name_buffer;

	if (!gse_ctx->authenticated) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!gse_ctx->client_name) {
		return NT_STATUS_NOT_FOUND;
	}

	/* TODO: check OID matches KRB5 Principal Name OID ? */

	gss_maj = gss_display_name(&gss_min,
				   gse_ctx->client_name,
				   &name_buffer, NULL);
	if (gss_maj) {
		DEBUG(0, ("gss_display_name failed [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	*cli_name = talloc_strndup(talloc_tos(),
					(char *)name_buffer.value,
					name_buffer.length);

	gss_maj = gss_release_buffer(&gss_min, &name_buffer);

	if (!*cli_name) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

NTSTATUS gse_get_authz_data(struct gse_context *gse_ctx,
			    TALLOC_CTX *mem_ctx, DATA_BLOB *pac)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_set_t set = GSS_C_NO_BUFFER_SET;

	if (!gse_ctx->authenticated) {
		return NT_STATUS_ACCESS_DENIED;
	}

	gss_maj = gss_inquire_sec_context_by_oid(
				&gss_min, gse_ctx->gss_ctx,
				&gse_authz_data_oid, &set);
	if (gss_maj) {
		DEBUG(0, ("gss_inquire_sec_context_by_oid failed [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		return NT_STATUS_NOT_FOUND;
	}

	if (set == GSS_C_NO_BUFFER_SET) {
		DEBUG(0, ("gss_inquire_sec_context_by_oid returned unknown "
			  "data in results.\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* for now we just hope it is the first value */
	*pac = data_blob_talloc(mem_ctx,
				set->elements[0].value,
				set->elements[0].length);

	gss_maj = gss_release_buffer_set(&gss_min, &set);

	return NT_STATUS_OK;
}

NTSTATUS gse_get_pac_blob(struct gse_context *gse_ctx,
			  TALLOC_CTX *mem_ctx, DATA_BLOB *pac_blob)
{
	OM_uint32 gss_min, gss_maj;
/*
 * gss_get_name_attribute() in MIT krb5 1.10.0 can return unintialized pac_display_buffer
 * and later gss_release_buffer() will crash on attempting to release it.
 *
 * So always initialize the buffer descriptors.
 *
 * See following links for more details:
 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=658514
 * http://krbdev.mit.edu/rt/Ticket/Display.html?user=guest&pass=guest&id=7087
 */
	gss_buffer_desc pac_buffer = {
		.value = NULL,
		.length = 0
	};
	gss_buffer_desc pac_display_buffer = {
		.value = NULL,
		.length = 0
	};
	gss_buffer_desc pac_name = {
		.value = discard_const_p(char, "urn:mspac:"),
		.length = sizeof("urn:mspac:") - 1
	};
	int more = -1;
	int authenticated = false;
	int complete = false;
	NTSTATUS status;

	if (!gse_ctx->authenticated) {
		return NT_STATUS_ACCESS_DENIED;
	}

	gss_maj = gss_get_name_attribute(&gss_min,
					 gse_ctx->client_name, &pac_name,
					 &authenticated, &complete,
					 &pac_buffer, &pac_display_buffer,
					 &more);

	if (gss_maj != 0) {
		DEBUG(0, ("obtaining PAC via GSSAPI gss_get_name_attribute "
			  "failed: %s\n",
			  gse_errstr(mem_ctx, gss_maj, gss_min)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (authenticated && complete) {
		/* The PAC blob is returned directly */
		*pac_blob = data_blob_talloc(mem_ctx,
					     pac_buffer.value,
					     pac_buffer.length);
		if (!pac_blob->data) {
			status = NT_STATUS_NO_MEMORY;
		} else {
			status = NT_STATUS_OK;
		}

		gss_maj = gss_release_buffer(&gss_min, &pac_buffer);
		gss_maj = gss_release_buffer(&gss_min, &pac_display_buffer);

		return status;
	}

	DEBUG(0, ("obtaining PAC via GSSAPI failed: authenticated: %s, "
		  "complete: %s, more: %s\n",
		  authenticated ? "true" : "false",
		  complete ? "true" : "false",
		  more ? "true" : "false"));

	return NT_STATUS_ACCESS_DENIED;
}

size_t gse_get_signature_length(struct gse_context *gse_ctx,
				int seal, size_t payload_size)
{
	OM_uint32 gss_min, gss_maj;
	gss_iov_buffer_desc iov[2];
	uint8_t fakebuf[payload_size];
	int sealed;

	iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
	iov[0].buffer.value = NULL;
	iov[0].buffer.length = 0;
	iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
	iov[1].buffer.value = fakebuf;
	iov[1].buffer.length = payload_size;

	gss_maj = gss_wrap_iov_length(&gss_min, gse_ctx->gss_ctx,
					seal, GSS_C_QOP_DEFAULT,
					&sealed, iov, 2);
	if (gss_maj) {
		DEBUG(0, ("gss_wrap_iov_length failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		return 0;
	}

	return iov[0].buffer.length;
}

NTSTATUS gse_seal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature)
{
	OM_uint32 gss_min, gss_maj;
	gss_iov_buffer_desc iov[2];
	int req_seal = 1; /* setting to 1 means we request sign+seal */
	int sealed;
	NTSTATUS status;

	/* allocate the memory ourselves so we do not need to talloc_memdup */
	signature->length = gse_get_signature_length(gse_ctx, 1, data->length);
	if (!signature->length) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	signature->data = (uint8_t *)talloc_size(mem_ctx, signature->length);
	if (!signature->data) {
		return NT_STATUS_NO_MEMORY;
	}
	iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
	iov[0].buffer.value = signature->data;
	iov[0].buffer.length = signature->length;

	/* data is encrypted in place, which is ok */
	iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
	iov[1].buffer.value = data->data;
	iov[1].buffer.length = data->length;

	gss_maj = gss_wrap_iov(&gss_min, gse_ctx->gss_ctx,
				req_seal, GSS_C_QOP_DEFAULT,
				&sealed, iov, 2);
	if (gss_maj) {
		DEBUG(0, ("gss_wrap_iov failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	if (!sealed) {
		DEBUG(0, ("gss_wrap_iov says data was not sealed!\n"));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	status = NT_STATUS_OK;

	DEBUG(10, ("Sealed %d bytes, and got %d bytes header/signature.\n",
		   (int)iov[1].buffer.length, (int)iov[0].buffer.length));

done:
	return status;
}

NTSTATUS gse_unseal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		    DATA_BLOB *data, DATA_BLOB *signature)
{
	OM_uint32 gss_min, gss_maj;
	gss_iov_buffer_desc iov[2];
	int sealed;
	NTSTATUS status;

	iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
	iov[0].buffer.value = signature->data;
	iov[0].buffer.length = signature->length;

	/* data is decrypted in place, which is ok */
	iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
	iov[1].buffer.value = data->data;
	iov[1].buffer.length = data->length;

	gss_maj = gss_unwrap_iov(&gss_min, gse_ctx->gss_ctx,
				 &sealed, NULL, iov, 2);
	if (gss_maj) {
		DEBUG(0, ("gss_unwrap_iov failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	if (!sealed) {
		DEBUG(0, ("gss_unwrap_iov says data is not sealed!\n"));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	status = NT_STATUS_OK;

	DEBUG(10, ("Unsealed %d bytes, with %d bytes header/signature.\n",
		   (int)iov[1].buffer.length, (int)iov[0].buffer.length));

done:
	return status;
}

NTSTATUS gse_sign(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_desc in_data = { 0, NULL };
	gss_buffer_desc out_data = { 0, NULL};
	NTSTATUS status;

	in_data.value = data->data;
	in_data.length = data->length;

	gss_maj = gss_get_mic(&gss_min, gse_ctx->gss_ctx,
			      GSS_C_QOP_DEFAULT,
			      &in_data, &out_data);
	if (gss_maj) {
		DEBUG(0, ("gss_get_mic failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	*signature = data_blob_talloc(mem_ctx,
					out_data.value, out_data.length);
	if (!signature->data) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = NT_STATUS_OK;

done:
	if (out_data.value) {
		gss_maj = gss_release_buffer(&gss_min, &out_data);
	}
	return status;
}

NTSTATUS gse_sigcheck(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		      DATA_BLOB *data, DATA_BLOB *signature)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_desc in_data = { 0, NULL };
	gss_buffer_desc in_token = { 0, NULL};
	NTSTATUS status;

	in_data.value = data->data;
	in_data.length = data->length;
	in_token.value = signature->data;
	in_token.length = signature->length;

	gss_maj = gss_verify_mic(&gss_min, gse_ctx->gss_ctx,
				 &in_data, &in_token, NULL);
	if (gss_maj) {
		DEBUG(0, ("gss_verify_mic failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	status = NT_STATUS_OK;

done:
	return status;
}

#else

NTSTATUS gse_init_client(TALLOC_CTX *mem_ctx,
			  bool do_sign, bool do_seal,
			  const char *ccache_name,
			  const char *server,
			  const char *service,
			  const char *username,
			  const char *password,
			  uint32_t add_gss_c_flags,
			  struct gse_context **_gse_ctx)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_get_client_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_init_server(TALLOC_CTX *mem_ctx,
			 bool do_sign, bool do_seal,
			 uint32_t add_gss_c_flags,
			 const char *keytab,
			 struct gse_context **_gse_ctx)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_get_server_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_verify_server_auth_flags(struct gse_context *gse_ctx)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

bool gse_require_more_processing(struct gse_context *gse_ctx)
{
	return false;
}

DATA_BLOB gse_get_session_key(TALLOC_CTX *mem_ctx,
			      struct gse_context *gse_ctx)
{
	return data_blob_null;
}

NTSTATUS gse_get_client_name(struct gse_context *gse_ctx,
			     TALLOC_CTX *mem_ctx, char **cli_name)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_get_authz_data(struct gse_context *gse_ctx,
			    TALLOC_CTX *mem_ctx, DATA_BLOB *pac)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_get_pac_blob(struct gse_context *gse_ctx,
			  TALLOC_CTX *mem_ctx, DATA_BLOB *pac)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

size_t gse_get_signature_length(struct gse_context *gse_ctx,
				int seal, size_t payload_size)
{
	return 0;
}

NTSTATUS gse_seal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_unseal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		    DATA_BLOB *data, DATA_BLOB *signature)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_sign(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS gse_sigcheck(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		      DATA_BLOB *data, DATA_BLOB *signature)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

#endif /* HAVE_KRB5 && HAVE_GSSAPI_EXT_H && HAVE_GSS_WRAP_IOV */
