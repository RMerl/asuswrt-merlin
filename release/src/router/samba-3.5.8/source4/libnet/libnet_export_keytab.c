#include "includes.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include <hdb.h>
#include "kdc/hdb-samba4.h"
#include "libnet/libnet.h"

NTSTATUS libnet_export_keytab(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_export_keytab *r)
{
	krb5_error_code ret;
	struct smb_krb5_context *smb_krb5_context;
	const char *from_keytab;

	/* Register hdb-samba4 hooks for use as a keytab */

	struct hdb_samba4_context *hdb_samba4_context = talloc(mem_ctx, struct hdb_samba4_context);
	if (!hdb_samba4_context) {
		return NT_STATUS_NO_MEMORY; 
	}

	hdb_samba4_context->ev_ctx = ctx->event_ctx;
	hdb_samba4_context->lp_ctx = ctx->lp_ctx;

	from_keytab = talloc_asprintf(hdb_samba4_context, "HDB:samba4&%p", hdb_samba4_context);
	if (!from_keytab) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = smb_krb5_init_context(ctx, ctx->event_ctx, ctx->lp_ctx, &smb_krb5_context);
	if (ret) {
		return NT_STATUS_NO_MEMORY; 
	}

	ret = krb5_plugin_register(smb_krb5_context->krb5_context, 
				   PLUGIN_TYPE_DATA, "hdb",
				   &hdb_samba4);
	if(ret) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = krb5_kt_register(smb_krb5_context->krb5_context, &hdb_kt_ops);
	if(ret) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = kt_copy(smb_krb5_context->krb5_context, from_keytab, r->in.keytab_name);
	if(ret) {
		r->out.error_string = smb_get_krb5_error_message(smb_krb5_context->krb5_context,
								 ret, mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	return NT_STATUS_OK;
}
