/* 
   Unix SMB/CIFS implementation.

   Handle user credentials (as regards krb5)

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Tim Potter 2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   
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
#include "auth/kerberos/kerberos.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_proto.h"
#include "auth/credentials/credentials_krb5.h"
#include "param/param.h"

_PUBLIC_ int cli_credentials_get_krb5_context(struct cli_credentials *cred, 
					      struct tevent_context *event_ctx,
				     struct loadparm_context *lp_ctx,
				     struct smb_krb5_context **smb_krb5_context) 
{
	int ret;
	if (cred->smb_krb5_context) {
		*smb_krb5_context = cred->smb_krb5_context;
		return 0;
	}

	ret = smb_krb5_init_context(cred, event_ctx, lp_ctx, &cred->smb_krb5_context);
	if (ret) {
		cred->smb_krb5_context = NULL;
		return ret;
	}
	*smb_krb5_context = cred->smb_krb5_context;
	return 0;
}

/* This needs to be called directly after the cli_credentials_init(),
 * otherwise we might have problems with the krb5 context already
 * being here.
 */
_PUBLIC_ NTSTATUS cli_credentials_set_krb5_context(struct cli_credentials *cred, 
					  struct smb_krb5_context *smb_krb5_context)
{
	if (!talloc_reference(cred, smb_krb5_context)) {
		return NT_STATUS_NO_MEMORY;
	}
	cred->smb_krb5_context = smb_krb5_context;
	return NT_STATUS_OK;
}

static int cli_credentials_set_from_ccache(struct cli_credentials *cred, 
				    struct ccache_container *ccache,
				    enum credentials_obtained obtained)
{
	
	krb5_principal princ;
	krb5_error_code ret;
	char *name;

	if (cred->ccache_obtained > obtained) {
		return 0;
	}

	ret = krb5_cc_get_principal(ccache->smb_krb5_context->krb5_context, 
				    ccache->ccache, &princ);

	if (ret) {
		char *err_mess = smb_get_krb5_error_message(ccache->smb_krb5_context->krb5_context, 
							    ret, cred);
		DEBUG(1,("failed to get principal from ccache: %s\n", 
			 err_mess));
		talloc_free(err_mess);
		return ret;
	}
	
	ret = krb5_unparse_name(ccache->smb_krb5_context->krb5_context, princ, &name);
	if (ret) {
		char *err_mess = smb_get_krb5_error_message(ccache->smb_krb5_context->krb5_context, ret, cred);
		DEBUG(1,("failed to unparse principal from ccache: %s\n", 
			 err_mess));
		talloc_free(err_mess);
		return ret;
	}

	cli_credentials_set_principal(cred, name, obtained);

	free(name);

	krb5_free_principal(ccache->smb_krb5_context->krb5_context, princ);

	/* set the ccache_obtained here, as it just got set to UNINITIALISED by the calls above */
	cred->ccache_obtained = obtained;

	return 0;
}

/* Free a memory ccache */
static int free_mccache(struct ccache_container *ccc)
{
	krb5_cc_destroy(ccc->smb_krb5_context->krb5_context, ccc->ccache);

	return 0;
}

/* Free a disk-based ccache */
static int free_dccache(struct ccache_container *ccc) {
	krb5_cc_close(ccc->smb_krb5_context->krb5_context, ccc->ccache);

	return 0;
}

_PUBLIC_ int cli_credentials_set_ccache(struct cli_credentials *cred, 
					struct tevent_context *event_ctx,
			       struct loadparm_context *lp_ctx,
			       const char *name, 
			       enum credentials_obtained obtained)
{
	krb5_error_code ret;
	krb5_principal princ;
	struct ccache_container *ccc;
	if (cred->ccache_obtained > obtained) {
		return 0;
	}

	ccc = talloc(cred, struct ccache_container);
	if (!ccc) {
		return ENOMEM;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, 
					       &ccc->smb_krb5_context);
	if (ret) {
		talloc_free(ccc);
		return ret;
	}
	if (!talloc_reference(ccc, ccc->smb_krb5_context)) {
		talloc_free(ccc);
		return ENOMEM;
	}

	if (name) {
		ret = krb5_cc_resolve(ccc->smb_krb5_context->krb5_context, name, &ccc->ccache);
		if (ret) {
			DEBUG(1,("failed to read krb5 ccache: %s: %s\n", 
				 name, 
				 smb_get_krb5_error_message(ccc->smb_krb5_context->krb5_context, ret, ccc)));
			talloc_free(ccc);
			return ret;
		}
	} else {
		ret = krb5_cc_default(ccc->smb_krb5_context->krb5_context, &ccc->ccache);
		if (ret) {
			DEBUG(3,("failed to read default krb5 ccache: %s\n", 
				 smb_get_krb5_error_message(ccc->smb_krb5_context->krb5_context, ret, ccc)));
			talloc_free(ccc);
			return ret;
		}
	}

	talloc_set_destructor(ccc, free_dccache);

	ret = krb5_cc_get_principal(ccc->smb_krb5_context->krb5_context, ccc->ccache, &princ);

	if (ret) {
		DEBUG(3,("failed to get principal from default ccache: %s\n", 
			 smb_get_krb5_error_message(ccc->smb_krb5_context->krb5_context, ret, ccc)));
		talloc_free(ccc);		
		return ret;
	}

	krb5_free_principal(ccc->smb_krb5_context->krb5_context, princ);

	ret = cli_credentials_set_from_ccache(cred, ccc, obtained);

	if (ret) {
		return ret;
	}

	cred->ccache = ccc;
	cred->ccache_obtained = obtained;
	talloc_steal(cred, ccc);

	cli_credentials_invalidate_client_gss_creds(cred, cred->ccache_obtained);
	return 0;
}


static int cli_credentials_new_ccache(struct cli_credentials *cred, 
				      struct tevent_context *event_ctx,
				      struct loadparm_context *lp_ctx,
				      struct ccache_container **_ccc)
{
	krb5_error_code ret;
	struct ccache_container *ccc = talloc(cred, struct ccache_container);
	char *ccache_name;
	if (!ccc) {
		return ENOMEM;
	}

	ccache_name = talloc_asprintf(ccc, "MEMORY:%p", 
				      ccc);

	if (!ccache_name) {
		talloc_free(ccc);
		return ENOMEM;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, 
					       &ccc->smb_krb5_context);
	if (ret) {
		talloc_free(ccc);
		return ret;
	}
	if (!talloc_reference(ccc, ccc->smb_krb5_context)) {
		talloc_free(ccc);
		return ENOMEM;
	}

	ret = krb5_cc_resolve(ccc->smb_krb5_context->krb5_context, ccache_name, 
			      &ccc->ccache);
	if (ret) {
		DEBUG(1,("failed to generate a new krb5 ccache (%s): %s\n", 
			 ccache_name,
			 smb_get_krb5_error_message(ccc->smb_krb5_context->krb5_context, ret, ccc)));
		talloc_free(ccache_name);
		talloc_free(ccc);
		return ret;
	}

	talloc_set_destructor(ccc, free_mccache);

	talloc_free(ccache_name);

	*_ccc = ccc;

	return ret;
}

_PUBLIC_ int cli_credentials_get_ccache(struct cli_credentials *cred, 
					struct tevent_context *event_ctx,
			       struct loadparm_context *lp_ctx,
			       struct ccache_container **ccc)
{
	krb5_error_code ret;
	
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred, lp_ctx);
	}

	if (cred->ccache_obtained >= cred->ccache_threshold && 
	    cred->ccache_obtained > CRED_UNINITIALISED) {
		*ccc = cred->ccache;
		return 0;
	}
	if (cli_credentials_is_anonymous(cred)) {
		return EINVAL;
	}

	ret = cli_credentials_new_ccache(cred, event_ctx, lp_ctx, ccc);
	if (ret) {
		return ret;
	}

	ret = kinit_to_ccache(cred, cred, (*ccc)->smb_krb5_context, (*ccc)->ccache);
	if (ret) {
		return ret;
	}

	ret = cli_credentials_set_from_ccache(cred, *ccc, 
					      (MAX(MAX(cred->principal_obtained, 
						       cred->username_obtained), 
						   cred->password_obtained)));
	
	cred->ccache = *ccc;
	cred->ccache_obtained = cred->principal_obtained;
	if (ret) {
		return ret;
	}
	cli_credentials_invalidate_client_gss_creds(cred, cred->ccache_obtained);
	return ret;
}

void cli_credentials_invalidate_client_gss_creds(struct cli_credentials *cred, 
						 enum credentials_obtained obtained)
{
	/* If the caller just changed the username/password etc, then
	 * any cached credentials are now invalid */
	if (obtained >= cred->client_gss_creds_obtained) {
		if (cred->client_gss_creds_obtained > CRED_UNINITIALISED) {
			talloc_unlink(cred, cred->client_gss_creds);
			cred->client_gss_creds = NULL;
		}
		cred->client_gss_creds_obtained = CRED_UNINITIALISED;
	}
	/* Now that we know that the data is 'this specified', then
	 * don't allow something less 'known' to be returned as a
	 * ccache.  Ie, if the username is on the commmand line, we
	 * don't want to later guess to use a file-based ccache */
	if (obtained > cred->client_gss_creds_threshold) {
		cred->client_gss_creds_threshold = obtained;
	}
}

_PUBLIC_ void cli_credentials_invalidate_ccache(struct cli_credentials *cred, 
				       enum credentials_obtained obtained)
{
	/* If the caller just changed the username/password etc, then
	 * any cached credentials are now invalid */
	if (obtained >= cred->ccache_obtained) {
		if (cred->ccache_obtained > CRED_UNINITIALISED) {
			talloc_unlink(cred, cred->ccache);
			cred->ccache = NULL;
		}
		cred->ccache_obtained = CRED_UNINITIALISED;
	}
	/* Now that we know that the data is 'this specified', then
	 * don't allow something less 'known' to be returned as a
	 * ccache.  Ie, if the username is on the commmand line, we
	 * don't want to later guess to use a file-based ccache */
	if (obtained > cred->ccache_threshold) {
		cred->ccache_threshold  = obtained;
	}

	cli_credentials_invalidate_client_gss_creds(cred, 
						    obtained);
}

static int free_gssapi_creds(struct gssapi_creds_container *gcc)
{
	OM_uint32 min_stat, maj_stat;
	maj_stat = gss_release_cred(&min_stat, &gcc->creds);
	return 0;
}

_PUBLIC_ int cli_credentials_get_client_gss_creds(struct cli_credentials *cred, 
					 struct tevent_context *event_ctx,
					 struct loadparm_context *lp_ctx,
					 struct gssapi_creds_container **_gcc) 
{
	int ret = 0;
	OM_uint32 maj_stat, min_stat;
	struct gssapi_creds_container *gcc;
	struct ccache_container *ccache;
	gss_buffer_desc empty_buffer = GSS_C_EMPTY_BUFFER;
	krb5_enctype *etypes = NULL;

	if (cred->client_gss_creds_obtained >= cred->client_gss_creds_threshold && 
	    cred->client_gss_creds_obtained > CRED_UNINITIALISED) {
		*_gcc = cred->client_gss_creds;
		return 0;
	}

	ret = cli_credentials_get_ccache(cred, event_ctx, lp_ctx, 
					 &ccache);
	if (ret) {
		DEBUG(1, ("Failed to get CCACHE for GSSAPI client: %s\n", error_message(ret)));
		return ret;
	}

	gcc = talloc(cred, struct gssapi_creds_container);
	if (!gcc) {
		return ENOMEM;
	}

	maj_stat = gss_krb5_import_cred(&min_stat, ccache->ccache, NULL, NULL, 
					&gcc->creds);
	if (maj_stat) {
		talloc_free(gcc);
		if (min_stat) {
			ret = min_stat;
		} else {
			ret = EINVAL;
		}
		return ret;
	}

	/*
	 * transfer the enctypes from the smb_krb5_context to the gssapi layer
	 *
	 * We use 'our' smb_krb5_context to do the AS-REQ and it is possible
	 * to configure the enctypes via the krb5.conf.
	 *
	 * And the gss_init_sec_context() creates it's own krb5_context and
	 * the TGS-REQ had all enctypes in it and only the ones configured
	 * and used for the AS-REQ, so it wasn't possible to disable the usage
	 * of AES keys.
	 */
	min_stat = krb5_get_default_in_tkt_etypes(ccache->smb_krb5_context->krb5_context,
						  &etypes);
	if (min_stat == 0) {
		OM_uint32 num_ktypes;

		for (num_ktypes = 0; etypes[num_ktypes]; num_ktypes++);

		maj_stat = gss_krb5_set_allowable_enctypes(&min_stat, gcc->creds,
							   num_ktypes, etypes);
		krb5_xfree (etypes);
		if (maj_stat) {
			talloc_free(gcc);
			if (min_stat) {
				ret = min_stat;
			} else {
				ret = EINVAL;
			}
			return ret;
		}
	}

	/* don't force GSS_C_CONF_FLAG and GSS_C_INTEG_FLAG */
	maj_stat = gss_set_cred_option(&min_stat, &gcc->creds,
				       GSS_KRB5_CRED_NO_CI_FLAGS_X,
				       &empty_buffer);
	if (maj_stat) {
		talloc_free(gcc);
		if (min_stat) {
			ret = min_stat;
		} else {
			ret = EINVAL;
		}
		return ret;
	}

	cred->client_gss_creds_obtained = cred->ccache_obtained;
	talloc_set_destructor(gcc, free_gssapi_creds);
	cred->client_gss_creds = gcc;
	*_gcc = gcc;
	return 0;
}

/**
   Set a gssapi cred_id_t into the credentials system. (Client case)

   This grabs the credentials both 'intact' and getting the krb5
   ccache out of it.  This routine can be generalised in future for
   the case where we deal with GSSAPI mechs other than krb5.  

   On sucess, the caller must not free gssapi_cred, as it now belongs
   to the credentials system.
*/

 int cli_credentials_set_client_gss_creds(struct cli_credentials *cred, 
					  struct tevent_context *event_ctx,
					  struct loadparm_context *lp_ctx,
					  gss_cred_id_t gssapi_cred,
					  enum credentials_obtained obtained) 
{
	int ret;
	OM_uint32 maj_stat, min_stat;
	struct ccache_container *ccc;
	struct gssapi_creds_container *gcc;
	if (cred->client_gss_creds_obtained > obtained) {
		return 0;
	}

	gcc = talloc(cred, struct gssapi_creds_container);
	if (!gcc) {
		return ENOMEM;
	}

	ret = cli_credentials_new_ccache(cred, event_ctx, lp_ctx, &ccc);
	if (ret != 0) {
		return ret;
	}

	maj_stat = gss_krb5_copy_ccache(&min_stat, 
					gssapi_cred, ccc->ccache);
	if (maj_stat) {
		if (min_stat) {
			ret = min_stat;
		} else {
			ret = EINVAL;
		}
	}

	if (ret == 0) {
		ret = cli_credentials_set_from_ccache(cred, ccc, obtained);
	}
	cred->ccache = ccc;
	cred->ccache_obtained = obtained;
	if (ret == 0) {
		gcc->creds = gssapi_cred;
		talloc_set_destructor(gcc, free_gssapi_creds);
		
		/* set the clinet_gss_creds_obtained here, as it just 
		   got set to UNINITIALISED by the calls above */
		cred->client_gss_creds_obtained = obtained;
		cred->client_gss_creds = gcc;
	}
	return ret;
}

/* Get the keytab (actually, a container containing the krb5_keytab)
 * attached to this context.  If this hasn't been done or set before,
 * it will be generated from the password.
 */
_PUBLIC_ int cli_credentials_get_keytab(struct cli_credentials *cred, 
					struct tevent_context *event_ctx,
			       struct loadparm_context *lp_ctx,
			       struct keytab_container **_ktc)
{
	krb5_error_code ret;
	struct keytab_container *ktc;
	struct smb_krb5_context *smb_krb5_context;
	const char **enctype_strings;
	TALLOC_CTX *mem_ctx;

	if (cred->keytab_obtained >= (MAX(cred->principal_obtained, 
					  cred->username_obtained))) {
		*_ktc = cred->keytab;
		return 0;
	}

	if (cli_credentials_is_anonymous(cred)) {
		return EINVAL;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, 
					       &smb_krb5_context);
	if (ret) {
		return ret;
	}

	mem_ctx = talloc_new(cred);
	if (!mem_ctx) {
		return ENOMEM;
	}

	enctype_strings = cli_credentials_get_enctype_strings(cred);
	
	ret = smb_krb5_create_memory_keytab(mem_ctx, cred, 
					    smb_krb5_context, 
					    enctype_strings, &ktc);
	if (ret) {
		talloc_free(mem_ctx);
		return ret;
	}

	cred->keytab_obtained = (MAX(cred->principal_obtained, 
				     cred->username_obtained));

	talloc_steal(cred, ktc);
	cred->keytab = ktc;
	*_ktc = cred->keytab;
	talloc_free(mem_ctx);
	return ret;
}

/* Given the name of a keytab (presumably in the format
 * FILE:/etc/krb5.keytab), open it and attach it */

_PUBLIC_ int cli_credentials_set_keytab_name(struct cli_credentials *cred, 
					     struct tevent_context *event_ctx,
				    struct loadparm_context *lp_ctx,
				    const char *keytab_name, 
				    enum credentials_obtained obtained) 
{
	krb5_error_code ret;
	struct keytab_container *ktc;
	struct smb_krb5_context *smb_krb5_context;
	TALLOC_CTX *mem_ctx;

	if (cred->keytab_obtained >= obtained) {
		return 0;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, &smb_krb5_context);
	if (ret) {
		return ret;
	}

	mem_ctx = talloc_new(cred);
	if (!mem_ctx) {
		return ENOMEM;
	}

	ret = smb_krb5_open_keytab(mem_ctx, smb_krb5_context, 
				   keytab_name, &ktc);
	if (ret) {
		return ret;
	}

	cred->keytab_obtained = obtained;

	talloc_steal(cred, ktc);
	cred->keytab = ktc;
	talloc_free(mem_ctx);

	return ret;
}

_PUBLIC_ int cli_credentials_update_keytab(struct cli_credentials *cred, 
					   struct tevent_context *event_ctx,
				  struct loadparm_context *lp_ctx) 
{
	krb5_error_code ret;
	struct keytab_container *ktc;
	struct smb_krb5_context *smb_krb5_context;
	const char **enctype_strings;
	TALLOC_CTX *mem_ctx;
	
	mem_ctx = talloc_new(cred);
	if (!mem_ctx) {
		return ENOMEM;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, &smb_krb5_context);
	if (ret) {
		talloc_free(mem_ctx);
		return ret;
	}

	enctype_strings = cli_credentials_get_enctype_strings(cred);
	
	ret = cli_credentials_get_keytab(cred, event_ctx, lp_ctx, &ktc);
	if (ret != 0) {
		talloc_free(mem_ctx);
		return ret;
	}

	ret = smb_krb5_update_keytab(mem_ctx, cred, smb_krb5_context, enctype_strings, ktc);

	talloc_free(mem_ctx);
	return ret;
}

/* Get server gss credentials (in gsskrb5, this means the keytab) */

_PUBLIC_ int cli_credentials_get_server_gss_creds(struct cli_credentials *cred, 
						  struct tevent_context *event_ctx,
					 struct loadparm_context *lp_ctx,
					 struct gssapi_creds_container **_gcc) 
{
	int ret = 0;
	OM_uint32 maj_stat, min_stat;
	struct gssapi_creds_container *gcc;
	struct keytab_container *ktc;
	struct smb_krb5_context *smb_krb5_context;
	TALLOC_CTX *mem_ctx;
	krb5_principal princ;

	if (cred->server_gss_creds_obtained >= (MAX(cred->keytab_obtained, 
						    MAX(cred->principal_obtained, 
							cred->username_obtained)))) {
		*_gcc = cred->server_gss_creds;
		return 0;
	}

	ret = cli_credentials_get_krb5_context(cred, event_ctx, lp_ctx, &smb_krb5_context);
	if (ret) {
		return ret;
	}

	ret = cli_credentials_get_keytab(cred, event_ctx, lp_ctx, &ktc);
	if (ret) {
		DEBUG(1, ("Failed to get keytab for GSSAPI server: %s\n", error_message(ret)));
		return ret;
	}

	mem_ctx = talloc_new(cred);
	if (!mem_ctx) {
		return ENOMEM;
	}

	ret = principal_from_credentials(mem_ctx, cred, smb_krb5_context, &princ);
	if (ret) {
		DEBUG(1,("cli_credentials_get_server_gss_creds: makeing krb5 principal failed (%s)\n",
			 smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						    ret, mem_ctx)));
		talloc_free(mem_ctx);
		return ret;
	}

	gcc = talloc(cred, struct gssapi_creds_container);
	if (!gcc) {
		talloc_free(mem_ctx);
		return ENOMEM;
	}

	/* This creates a GSSAPI cred_id_t with the principal and keytab set */
	maj_stat = gss_krb5_import_cred(&min_stat, NULL, princ, ktc->keytab, 
					&gcc->creds);
	if (maj_stat) {
		if (min_stat) {
			ret = min_stat;
		} else {
			ret = EINVAL;
		}
	}
	if (ret == 0) {
		cred->server_gss_creds_obtained = cred->keytab_obtained;
		talloc_set_destructor(gcc, free_gssapi_creds);
		cred->server_gss_creds = gcc;
		*_gcc = gcc;
	}
	talloc_free(mem_ctx);
	return ret;
}

/** 
 * Set Kerberos KVNO
 */

_PUBLIC_ void cli_credentials_set_kvno(struct cli_credentials *cred,
			      int kvno)
{
	cred->kvno = kvno;
}

/**
 * Return Kerberos KVNO
 */

_PUBLIC_ int cli_credentials_get_kvno(struct cli_credentials *cred)
{
	return cred->kvno;
}


const char **cli_credentials_get_enctype_strings(struct cli_credentials *cred) 
{
	/* If this is ever made user-configurable, we need to add code
	 * to remove/hide the other entries from the generated
	 * keytab */
	static const char *default_enctypes[] = {
		"des-cbc-md5",
		"aes256-cts-hmac-sha1-96",
		"des3-cbc-sha1",
		"arcfour-hmac-md5",
		NULL
	};
	return default_enctypes;
}

const char *cli_credentials_get_salt_principal(struct cli_credentials *cred) 
{
	return cred->salt_principal;
}

_PUBLIC_ void cli_credentials_set_salt_principal(struct cli_credentials *cred, const char *principal) 
{
	cred->salt_principal = talloc_strdup(cred, principal);
}


