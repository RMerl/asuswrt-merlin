/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Guenther Deschner 2008.
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

#ifdef HAVE_KRB5

/****************************************************************
****************************************************************/

static int keytab_close(struct libnet_keytab_context *ctx)
{
	if (!ctx) {
		return 0;
	}

	if (ctx->keytab && ctx->context) {
		krb5_kt_close(ctx->context, ctx->keytab);
	}

	if (ctx->context) {
		krb5_free_context(ctx->context);
	}

	if (ctx->ads) {
		ads_destroy(&ctx->ads);
	}

	TALLOC_FREE(ctx);

	return 0;
}

/****************************************************************
****************************************************************/

krb5_error_code libnet_keytab_init(TALLOC_CTX *mem_ctx,
				   const char *keytab_name,
				   struct libnet_keytab_context **ctx)
{
	krb5_error_code ret = 0;
	krb5_context context = NULL;
	krb5_keytab keytab = NULL;
	const char *keytab_string = NULL;

	struct libnet_keytab_context *r;

	r = TALLOC_ZERO_P(mem_ctx, struct libnet_keytab_context);
	if (!r) {
		return ENOMEM;
	}

	talloc_set_destructor(r, keytab_close);

	initialize_krb5_error_table();
	ret = krb5_init_context(&context);
	if (ret) {
		DEBUG(1,("keytab_init: could not krb5_init_context: %s\n",
			error_message(ret)));
		return ret;
	}

	ret = smb_krb5_open_keytab(context, keytab_name, true, &keytab);
	if (ret) {
		DEBUG(1,("keytab_init: smb_krb5_open_keytab failed (%s)\n",
			error_message(ret)));
		krb5_free_context(context);
		return ret;
	}

	ret = smb_krb5_keytab_name(mem_ctx, context, keytab, &keytab_string);
	if (ret) {
		krb5_kt_close(context, keytab);
		krb5_free_context(context);
		return ret;
	}

	r->context = context;
	r->keytab = keytab;
	r->keytab_name = keytab_string;
	r->clean_old_entries = false;

	*ctx = r;

	return 0;
}

/****************************************************************
****************************************************************/

/**
 * Remove all entries that have the given principal, kvno and enctype.
 */
static krb5_error_code libnet_keytab_remove_entries(krb5_context context,
						    krb5_keytab keytab,
						    const char *principal,
						    int kvno,
						    const krb5_enctype enctype,
						    bool ignore_kvno)
{
	krb5_error_code ret;
	krb5_kt_cursor cursor;
	krb5_keytab_entry kt_entry;

	ZERO_STRUCT(kt_entry);
	ZERO_STRUCT(cursor);

	ret = krb5_kt_start_seq_get(context, keytab, &cursor);
	if (ret) {
		return 0;
	}

	while (krb5_kt_next_entry(context, keytab, &kt_entry, &cursor) == 0)
	{
		krb5_keyblock *keyp;
		char *princ_s = NULL;

		if (kt_entry.vno != kvno && !ignore_kvno) {
			goto cont;
		}

		keyp = KRB5_KT_KEY(&kt_entry);

		if (KRB5_KEY_TYPE(keyp) != enctype) {
			goto cont;
		}

		ret = smb_krb5_unparse_name(talloc_tos(), context, kt_entry.principal,
					    &princ_s);
		if (ret) {
			DEBUG(5, ("smb_krb5_unparse_name failed (%s)\n",
				  error_message(ret)));
			goto cont;
		}

		if (strcmp(principal, princ_s) != 0) {
			goto cont;
		}

		/* match found - remove */

		DEBUG(10, ("found entry for principal %s, kvno %d, "
			   "enctype %d - trying to remove it\n",
			   princ_s, kt_entry.vno, KRB5_KEY_TYPE(keyp)));

		ret = krb5_kt_end_seq_get(context, keytab, &cursor);
		ZERO_STRUCT(cursor);
		if (ret) {
			DEBUG(5, ("krb5_kt_end_seq_get failed (%s)\n",
				  error_message(ret)));
			goto cont;
		}

		ret = krb5_kt_remove_entry(context, keytab,
					   &kt_entry);
		if (ret) {
			DEBUG(5, ("krb5_kt_remove_entry failed (%s)\n",
				  error_message(ret)));
			goto cont;
		}
		DEBUG(10, ("removed entry for principal %s, kvno %d, "
			   "enctype %d\n", princ_s, kt_entry.vno,
			   KRB5_KEY_TYPE(keyp)));

		ret = krb5_kt_start_seq_get(context, keytab, &cursor);
		if (ret) {
			DEBUG(5, ("krb5_kt_start_seq_get failed (%s)\n",
				  error_message(ret)));
			goto cont;
		}

cont:
		smb_krb5_kt_free_entry(context, &kt_entry);
		TALLOC_FREE(princ_s);
	}

	ret = krb5_kt_end_seq_get(context, keytab, &cursor);
	if (ret) {
		DEBUG(5, ("krb5_kt_end_seq_get failed (%s)\n",
			  error_message(ret)));
	}

	return ret;
}

static krb5_error_code libnet_keytab_add_entry(krb5_context context,
					       krb5_keytab keytab,
					       krb5_kvno kvno,
					       const char *princ_s,
					       krb5_enctype enctype,
					       krb5_data password)
{
	krb5_keyblock *keyp;
	krb5_keytab_entry kt_entry;
	krb5_error_code ret;

	/* remove duplicates first ... */
	ret = libnet_keytab_remove_entries(context, keytab, princ_s, kvno,
					   enctype, false);
	if (ret) {
		DEBUG(1, ("libnet_keytab_remove_entries failed: %s\n",
			  error_message(ret)));
	}

	ZERO_STRUCT(kt_entry);

	kt_entry.vno = kvno;

	ret = smb_krb5_parse_name(context, princ_s, &kt_entry.principal);
	if (ret) {
		DEBUG(1, ("smb_krb5_parse_name(%s) failed (%s)\n",
			  princ_s, error_message(ret)));
		return ret;
	}

	keyp = KRB5_KT_KEY(&kt_entry);

	if (create_kerberos_key_from_string(context, kt_entry.principal,
					    &password, keyp, enctype, true))
	{
		ret = KRB5KRB_ERR_GENERIC;
		goto done;
	}

	ret = krb5_kt_add_entry(context, keytab, &kt_entry);
	if (ret) {
		DEBUG(1, ("adding entry to keytab failed (%s)\n",
			  error_message(ret)));
	}

done:
	krb5_free_keyblock_contents(context, keyp);
	krb5_free_principal(context, kt_entry.principal);
	ZERO_STRUCT(kt_entry);
	smb_krb5_kt_free_entry(context, &kt_entry);

	return ret;
}

krb5_error_code libnet_keytab_add(struct libnet_keytab_context *ctx)
{
	krb5_error_code ret = 0;
	uint32_t i;


	if (ctx->clean_old_entries) {
		DEBUG(0, ("cleaning old entries...\n"));
		for (i=0; i < ctx->count; i++) {
			struct libnet_keytab_entry *entry = &ctx->entries[i];

			ret = libnet_keytab_remove_entries(ctx->context,
							   ctx->keytab,
							   entry->principal,
							   0,
							   entry->enctype,
							   true);
			if (ret) {
				DEBUG(1,("libnet_keytab_add: Failed to remove "
					 "old entries for %s (enctype %u): %s\n",
					 entry->principal, entry->enctype,
					 error_message(ret)));
				return ret;
			}
		}
	}

	for (i=0; i<ctx->count; i++) {

		struct libnet_keytab_entry *entry = &ctx->entries[i];
		krb5_data password;

		ZERO_STRUCT(password);
		password.data = (char *)entry->password.data;
		password.length = entry->password.length;

		ret = libnet_keytab_add_entry(ctx->context,
					      ctx->keytab,
					      entry->kvno,
					      entry->principal,
					      entry->enctype,
					      password);
		if (ret) {
			DEBUG(1,("libnet_keytab_add: "
				"Failed to add entry to keytab file\n"));
			return ret;
		}
	}

	return ret;
}

struct libnet_keytab_entry *libnet_keytab_search(struct libnet_keytab_context *ctx,
						 const char *principal,
						 int kvno,
						 const krb5_enctype enctype,
						 TALLOC_CTX *mem_ctx)
{
	krb5_error_code ret = 0;
	krb5_kt_cursor cursor;
	krb5_keytab_entry kt_entry;
	struct libnet_keytab_entry *entry = NULL;

	ZERO_STRUCT(kt_entry);
	ZERO_STRUCT(cursor);

	ret = krb5_kt_start_seq_get(ctx->context, ctx->keytab, &cursor);
	if (ret) {
		DEBUG(10, ("krb5_kt_start_seq_get failed: %s\n",
			  error_message(ret)));
		return NULL;
	}

	while (krb5_kt_next_entry(ctx->context, ctx->keytab, &kt_entry, &cursor) == 0)
	{
		krb5_keyblock *keyp;
		char *princ_s = NULL;

		entry = NULL;

		if (kt_entry.vno != kvno) {
			goto cont;
		}

		keyp = KRB5_KT_KEY(&kt_entry);

		if (KRB5_KEY_TYPE(keyp) != enctype) {
			goto cont;
		}

		entry = talloc_zero(mem_ctx, struct libnet_keytab_entry);
		if (!entry) {
			DEBUG(3, ("talloc failed\n"));
			goto fail;
		}

		ret = smb_krb5_unparse_name(entry, ctx->context, kt_entry.principal,
					    &princ_s);
		if (ret) {
			goto cont;
		}

		if (strcmp(principal, princ_s) != 0) {
			goto cont;
		}

		entry->principal = talloc_strdup(entry, princ_s);
		if (!entry->principal) {
			DEBUG(3, ("talloc_strdup_failed\n"));
			goto fail;
		}

		entry->name = talloc_move(entry, &princ_s);

		entry->password = data_blob_talloc(entry, KRB5_KEY_DATA(keyp),
						   KRB5_KEY_LENGTH(keyp));
		if (!entry->password.data) {
			DEBUG(3, ("data_blob_talloc failed\n"));
			goto fail;
		}

		DEBUG(10, ("found entry\n"));

		smb_krb5_kt_free_entry(ctx->context, &kt_entry);
		break;

fail:
		smb_krb5_kt_free_entry(ctx->context, &kt_entry);
		TALLOC_FREE(entry);
		break;

cont:
		smb_krb5_kt_free_entry(ctx->context, &kt_entry);
		TALLOC_FREE(entry);
		continue;
	}

	krb5_kt_end_seq_get(ctx->context, ctx->keytab, &cursor);
	return entry;
}

/**
 * Helper function to add data to the list
 * of keytab entries. It builds the prefix from the input.
 */
NTSTATUS libnet_keytab_add_to_keytab_entries(TALLOC_CTX *mem_ctx,
					     struct libnet_keytab_context *ctx,
					     uint32_t kvno,
					     const char *name,
					     const char *prefix,
					     const krb5_enctype enctype,
					     DATA_BLOB blob)
{
	struct libnet_keytab_entry entry;

	entry.kvno = kvno;
	entry.name = talloc_strdup(mem_ctx, name);
	entry.principal = talloc_asprintf(mem_ctx, "%s%s%s@%s",
					  prefix ? prefix : "",
					  prefix ? "/" : "",
					  name, ctx->dns_domain_name);
	entry.enctype = enctype;
	entry.password = blob;
	NT_STATUS_HAVE_NO_MEMORY(entry.name);
	NT_STATUS_HAVE_NO_MEMORY(entry.principal);
	NT_STATUS_HAVE_NO_MEMORY(entry.password.data);

	ADD_TO_ARRAY(mem_ctx, struct libnet_keytab_entry, entry,
		     &ctx->entries, &ctx->count);
	NT_STATUS_HAVE_NO_MEMORY(ctx->entries);

	return NT_STATUS_OK;
}

#endif /* HAVE_KRB5 */
