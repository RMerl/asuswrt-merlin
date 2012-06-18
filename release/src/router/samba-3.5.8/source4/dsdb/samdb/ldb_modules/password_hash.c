/* 
   ldb database module

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2006
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2007

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

/*
 *  Name: ldb
 *
 *  Component: ldb password_hash module
 *
 *  Description: correctly update hash values based on changes to userPassword and friends
 *
 *  Author: Andrew Bartlett
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "libcli/ldap/ldap_ndr.h"
#include "ldb_module.h"
#include "librpc/gen_ndr/misc.h"
#include "librpc/gen_ndr/samr.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/security/security.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "system/time.h"
#include "dsdb/samdb/samdb.h"
#include "../libds/common/flags.h"
#include "dsdb/samdb/ldb_modules/password_modules.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/crypto.h"
#include "param/param.h"

/* If we have decided there is reason to work on this request, then
 * setup all the password hash types correctly.
 *
 * If the administrator doesn't want the userPassword stored (set in the
 * domain and per-account policies) then we must strip that out before
 * we do the first operation.
 *
 * Once this is done (which could update anything at all), we
 * calculate the password hashes.
 *
 * This function must not only update the unicodePwd, dBCSPwd and
 * supplementalCredentials fields, it must also atomicly increment the
 * msDS-KeyVersionNumber.  We should be in a transaction, so all this
 * should be quite safe...
 *
 * Finally, if the administrator has requested that a password history
 * be maintained, then this should also be written out.
 *
 */

struct ph_context {

	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_request *dom_req;
	struct ldb_reply *dom_res;

	struct ldb_reply *search_res;

	struct dom_sid *domain_sid;
	struct domain_data *domain;
};

struct domain_data {
	bool store_cleartext;
	uint_t pwdProperties;
	uint_t pwdHistoryLength;
	char *netbios_domain;
	char *dns_domain;
	char *realm;
};

struct setup_password_fields_io {
	struct ph_context *ac;
	struct domain_data *domain;
	struct smb_krb5_context *smb_krb5_context;

	/* infos about the user account */
	struct {
		uint32_t user_account_control;
		const char *sAMAccountName;
		const char *user_principal_name;
		bool is_computer;
	} u;

	/* new credentials */
	struct {
		const struct ldb_val *cleartext_utf8;
		const struct ldb_val *cleartext_utf16;
		struct ldb_val quoted_utf16;
		struct samr_Password *nt_hash;
		struct samr_Password *lm_hash;
	} n;

	/* old credentials */
	struct {
		uint32_t nt_history_len;
		struct samr_Password *nt_history;
		uint32_t lm_history_len;
		struct samr_Password *lm_history;
		const struct ldb_val *supplemental;
		struct supplementalCredentialsBlob scb;
		uint32_t kvno;
	} o;

	/* generated credentials */
	struct {
		struct samr_Password *nt_hash;
		struct samr_Password *lm_hash;
		uint32_t nt_history_len;
		struct samr_Password *nt_history;
		uint32_t lm_history_len;
		struct samr_Password *lm_history;
		const char *salt;
		DATA_BLOB aes_256;
		DATA_BLOB aes_128;
		DATA_BLOB des_md5;
		DATA_BLOB des_crc;
		struct ldb_val supplemental;
		NTTIME last_set;
		uint32_t kvno;
	} g;
};

/* Get the NT hash, and fill it in as an entry in the password history, 
   and specify it into io->g.nt_hash */

static int setup_nt_fields(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	uint32_t i;

	io->g.nt_hash = io->n.nt_hash;
	ldb = ldb_module_get_ctx(io->ac->module);

	if (io->domain->pwdHistoryLength == 0) {
		return LDB_SUCCESS;
	}

	/* We might not have an old NT password */
	io->g.nt_history = talloc_array(io->ac,
					struct samr_Password,
					io->domain->pwdHistoryLength);
	if (!io->g.nt_history) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < MIN(io->domain->pwdHistoryLength-1, io->o.nt_history_len); i++) {
		io->g.nt_history[i+1] = io->o.nt_history[i];
	}
	io->g.nt_history_len = i + 1;

	if (io->g.nt_hash) {
		io->g.nt_history[0] = *io->g.nt_hash;
	} else {
		/* 
		 * TODO: is this correct?
		 * the simular behavior is correct for the lm history case
		 */
		E_md4hash("", io->g.nt_history[0].hash);
	}

	return LDB_SUCCESS;
}

/* Get the LANMAN hash, and fill it in as an entry in the password history, 
   and specify it into io->g.lm_hash */

static int setup_lm_fields(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	uint32_t i;

	io->g.lm_hash = io->n.lm_hash;
	ldb = ldb_module_get_ctx(io->ac->module);

	if (io->domain->pwdHistoryLength == 0) {
		return LDB_SUCCESS;
	}

	/* We might not have an old NT password */
	io->g.lm_history = talloc_array(io->ac,
					struct samr_Password,
					io->domain->pwdHistoryLength);
	if (!io->g.lm_history) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < MIN(io->domain->pwdHistoryLength-1, io->o.lm_history_len); i++) {
		io->g.lm_history[i+1] = io->o.lm_history[i];
	}
	io->g.lm_history_len = i + 1;

	if (io->g.lm_hash) {
		io->g.lm_history[0] = *io->g.lm_hash;
	} else {
		E_deshash("", io->g.lm_history[0].hash);
	}

	return LDB_SUCCESS;
}

static int setup_kerberos_keys(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	krb5_error_code krb5_ret;
	Principal *salt_principal;
	krb5_salt salt;
	krb5_keyblock key;
	krb5_data cleartext_data;

	ldb = ldb_module_get_ctx(io->ac->module);
	cleartext_data.data = io->n.cleartext_utf8->data;
	cleartext_data.length = io->n.cleartext_utf8->length;

	/* Many, many thanks to lukeh@padl.com for this
	 * algorithm, described in his Nov 10 2004 mail to
	 * samba-technical@samba.org */

	/*
	 * Determine a salting principal
	 */
	if (io->u.is_computer) {
		char *name;
		char *saltbody;

		name = strlower_talloc(io->ac, io->u.sAMAccountName);
		if (!name) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (name[strlen(name)-1] == '$') {
			name[strlen(name)-1] = '\0';
		}

		saltbody = talloc_asprintf(io->ac, "%s.%s", name, io->domain->dns_domain);
		if (!saltbody) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->domain->realm, "host",
					       saltbody, NULL);
	} else if (io->u.user_principal_name) {
		char *user_principal_name;
		char *p;

		user_principal_name = talloc_strdup(io->ac, io->u.user_principal_name);
		if (!user_principal_name) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		p = strchr(user_principal_name, '@');
		if (p) {
			p[0] = '\0';
		}

		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->domain->realm, user_principal_name,
					       NULL);
	} else {
		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->domain->realm, io->u.sAMAccountName,
					       NULL);
	}
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a salting principal failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * create salt from salt_principal
	 */
	krb5_ret = krb5_get_pw_salt(io->smb_krb5_context->krb5_context,
				    salt_principal, &salt);
	krb5_free_principal(io->smb_krb5_context->krb5_context, salt_principal);
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of krb5_salt failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	/* create a talloc copy */
	io->g.salt = talloc_strndup(io->ac,
				    salt.saltvalue.data,
				    salt.saltvalue.length);
	krb5_free_salt(io->smb_krb5_context->krb5_context, salt);
	if (!io->g.salt) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	salt.saltvalue.data	= discard_const(io->g.salt);
	salt.saltvalue.length	= strlen(io->g.salt);

	/*
	 * create ENCTYPE_AES256_CTS_HMAC_SHA1_96 key out of
	 * the salt and the cleartext password
	 */
	krb5_ret = krb5_string_to_key_data_salt(io->smb_krb5_context->krb5_context,
						ENCTYPE_AES256_CTS_HMAC_SHA1_96,
						cleartext_data,
						salt,
						&key);
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a aes256-cts-hmac-sha1-96 key failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.aes_256 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.aes_256.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * create ENCTYPE_AES128_CTS_HMAC_SHA1_96 key out of
	 * the salt and the cleartext password
	 */
	krb5_ret = krb5_string_to_key_data_salt(io->smb_krb5_context->krb5_context,
						ENCTYPE_AES128_CTS_HMAC_SHA1_96,
						cleartext_data,
						salt,
						&key);
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a aes128-cts-hmac-sha1-96 key failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.aes_128 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.aes_128.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * create ENCTYPE_DES_CBC_MD5 key out of
	 * the salt and the cleartext password
	 */
	krb5_ret = krb5_string_to_key_data_salt(io->smb_krb5_context->krb5_context,
						ENCTYPE_DES_CBC_MD5,
						cleartext_data,
						salt,
						&key);
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a des-cbc-md5 key failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.des_md5 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.des_md5.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * create ENCTYPE_DES_CBC_CRC key out of
	 * the salt and the cleartext password
	 */
	krb5_ret = krb5_string_to_key_data_salt(io->smb_krb5_context->krb5_context,
						ENCTYPE_DES_CBC_CRC,
						cleartext_data,
						salt,
						&key);
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a des-cbc-crc key failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context, krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.des_crc = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.des_crc.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static int setup_primary_kerberos(struct setup_password_fields_io *io,
				  const struct supplementalCredentialsBlob *old_scb,
				  struct package_PrimaryKerberosBlob *pkb)
{
	struct ldb_context *ldb;
	struct package_PrimaryKerberosCtr3 *pkb3 = &pkb->ctr.ctr3;
	struct supplementalCredentialsPackage *old_scp = NULL;
	struct package_PrimaryKerberosBlob _old_pkb;
	struct package_PrimaryKerberosCtr3 *old_pkb3 = NULL;
	uint32_t i;
	enum ndr_err_code ndr_err;

	ldb = ldb_module_get_ctx(io->ac->module);

	/*
	 * prepare generation of keys
	 *
	 * ENCTYPE_DES_CBC_MD5
	 * ENCTYPE_DES_CBC_CRC
	 */
	pkb->version		= 3;
	pkb3->salt.string	= io->g.salt;
	pkb3->num_keys		= 2;
	pkb3->keys		= talloc_array(io->ac,
					       struct package_PrimaryKerberosKey3,
					       pkb3->num_keys);
	if (!pkb3->keys) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	pkb3->keys[0].keytype	= ENCTYPE_DES_CBC_MD5;
	pkb3->keys[0].value	= &io->g.des_md5;
	pkb3->keys[1].keytype	= ENCTYPE_DES_CBC_CRC;
	pkb3->keys[1].value	= &io->g.des_crc;

	/* initialize the old keys to zero */
	pkb3->num_old_keys	= 0;
	pkb3->old_keys		= NULL;

	/* if there're no old keys, then we're done */
	if (!old_scb) {
		return LDB_SUCCESS;
	}

	for (i=0; i < old_scb->sub.num_packages; i++) {
		if (strcmp("Primary:Kerberos", old_scb->sub.packages[i].name) != 0) {
			continue;
		}

		if (!old_scb->sub.packages[i].data || !old_scb->sub.packages[i].data[0]) {
			continue;
		}

		old_scp = &old_scb->sub.packages[i];
		break;
	}
	/* Primary:Kerberos element of supplementalCredentials */
	if (old_scp) {
		DATA_BLOB blob;

		blob = strhex_to_data_blob(io->ac, old_scp->data);
		if (!blob.data) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* TODO: use ndr_pull_struct_blob_all(), when the ndr layer handles it correct with relative pointers */
		ndr_err = ndr_pull_struct_blob(&blob, io->ac, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), &_old_pkb,
					       (ndr_pull_flags_fn_t)ndr_pull_package_PrimaryKerberosBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ldb_asprintf_errstring(ldb,
					       "setup_primary_kerberos: "
					       "failed to pull old package_PrimaryKerberosBlob: %s",
					       nt_errstr(status));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (_old_pkb.version != 3) {
			ldb_asprintf_errstring(ldb,
					       "setup_primary_kerberos: "
					       "package_PrimaryKerberosBlob version[%u] expected[3]",
					       _old_pkb.version);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		old_pkb3 = &_old_pkb.ctr.ctr3;
	}

	/* if we didn't found the old keys we're done */
	if (!old_pkb3) {
		return LDB_SUCCESS;
	}

	/* fill in the old keys */
	pkb3->num_old_keys	= old_pkb3->num_keys;
	pkb3->old_keys		= old_pkb3->keys;

	return LDB_SUCCESS;
}

static int setup_primary_kerberos_newer(struct setup_password_fields_io *io,
					const struct supplementalCredentialsBlob *old_scb,
					struct package_PrimaryKerberosBlob *pkb)
{
	struct ldb_context *ldb;
	struct package_PrimaryKerberosCtr4 *pkb4 = &pkb->ctr.ctr4;
	struct supplementalCredentialsPackage *old_scp = NULL;
	struct package_PrimaryKerberosBlob _old_pkb;
	struct package_PrimaryKerberosCtr4 *old_pkb4 = NULL;
	uint32_t i;
	enum ndr_err_code ndr_err;

	ldb = ldb_module_get_ctx(io->ac->module);

	/*
	 * prepare generation of keys
	 *
	 * ENCTYPE_AES256_CTS_HMAC_SHA1_96
	 * ENCTYPE_AES128_CTS_HMAC_SHA1_96
	 * ENCTYPE_DES_CBC_MD5
	 * ENCTYPE_DES_CBC_CRC
	 */
	pkb->version			= 4;
	pkb4->salt.string		= io->g.salt;
	pkb4->default_iteration_count	= 4096;
	pkb4->num_keys			= 4;

	pkb4->keys = talloc_array(io->ac,
				  struct package_PrimaryKerberosKey4,
				  pkb4->num_keys);
	if (!pkb4->keys) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	pkb4->keys[0].iteration_count	= 4096;
	pkb4->keys[0].keytype		= ENCTYPE_AES256_CTS_HMAC_SHA1_96;
	pkb4->keys[0].value		= &io->g.aes_256;
	pkb4->keys[1].iteration_count	= 4096;
	pkb4->keys[1].keytype		= ENCTYPE_AES128_CTS_HMAC_SHA1_96;
	pkb4->keys[1].value		= &io->g.aes_128;
	pkb4->keys[2].iteration_count	= 4096;
	pkb4->keys[2].keytype		= ENCTYPE_DES_CBC_MD5;
	pkb4->keys[2].value		= &io->g.des_md5;
	pkb4->keys[3].iteration_count	= 4096;
	pkb4->keys[3].keytype		= ENCTYPE_DES_CBC_CRC;
	pkb4->keys[3].value		= &io->g.des_crc;

	/* initialize the old keys to zero */
	pkb4->num_old_keys	= 0;
	pkb4->old_keys		= NULL;
	pkb4->num_older_keys	= 0;
	pkb4->older_keys	= NULL;

	/* if there're no old keys, then we're done */
	if (!old_scb) {
		return LDB_SUCCESS;
	}

	for (i=0; i < old_scb->sub.num_packages; i++) {
		if (strcmp("Primary:Kerberos-Newer-Keys", old_scb->sub.packages[i].name) != 0) {
			continue;
		}

		if (!old_scb->sub.packages[i].data || !old_scb->sub.packages[i].data[0]) {
			continue;
		}

		old_scp = &old_scb->sub.packages[i];
		break;
	}
	/* Primary:Kerberos-Newer-Keys element of supplementalCredentials */
	if (old_scp) {
		DATA_BLOB blob;

		blob = strhex_to_data_blob(io->ac, old_scp->data);
		if (!blob.data) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* TODO: use ndr_pull_struct_blob_all(), when the ndr layer handles it correct with relative pointers */
		ndr_err = ndr_pull_struct_blob(&blob, io->ac,
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
					       &_old_pkb,
					       (ndr_pull_flags_fn_t)ndr_pull_package_PrimaryKerberosBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ldb_asprintf_errstring(ldb,
					       "setup_primary_kerberos_newer: "
					       "failed to pull old package_PrimaryKerberosBlob: %s",
					       nt_errstr(status));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (_old_pkb.version != 4) {
			ldb_asprintf_errstring(ldb,
					       "setup_primary_kerberos_newer: "
					       "package_PrimaryKerberosBlob version[%u] expected[4]",
					       _old_pkb.version);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		old_pkb4 = &_old_pkb.ctr.ctr4;
	}

	/* if we didn't found the old keys we're done */
	if (!old_pkb4) {
		return LDB_SUCCESS;
	}

	/* fill in the old keys */
	pkb4->num_old_keys	= old_pkb4->num_keys;
	pkb4->old_keys		= old_pkb4->keys;
	pkb4->num_older_keys	= old_pkb4->num_old_keys;
	pkb4->older_keys	= old_pkb4->old_keys;

	return LDB_SUCCESS;
}

static int setup_primary_wdigest(struct setup_password_fields_io *io,
				 const struct supplementalCredentialsBlob *old_scb,
				 struct package_PrimaryWDigestBlob *pdb)
{
	struct ldb_context *ldb = ldb_module_get_ctx(io->ac->module);
	DATA_BLOB sAMAccountName;
	DATA_BLOB sAMAccountName_l;
	DATA_BLOB sAMAccountName_u;
	const char *user_principal_name = io->u.user_principal_name;
	DATA_BLOB userPrincipalName;
	DATA_BLOB userPrincipalName_l;
	DATA_BLOB userPrincipalName_u;
	DATA_BLOB netbios_domain;
	DATA_BLOB netbios_domain_l;
	DATA_BLOB netbios_domain_u;
	DATA_BLOB dns_domain;
	DATA_BLOB dns_domain_l;
	DATA_BLOB dns_domain_u;
	DATA_BLOB digest;
	DATA_BLOB delim;
	DATA_BLOB backslash;
	uint8_t i;
	struct {
		DATA_BLOB *user;
		DATA_BLOB *realm;
		DATA_BLOB *nt4dom;
	} wdigest[] = {
	/*
	 * See
	 * http://technet2.microsoft.com/WindowsServer/en/library/717b450c-f4a0-4cc9-86f4-cc0633aae5f91033.mspx?mfr=true
	 * for what precalculated hashes are supposed to be stored...
	 *
	 * I can't reproduce all values which should contain "Digest" as realm,
	 * am I doing something wrong or is w2k3 just broken...?
	 *
	 * W2K3 fills in following for a user:
	 *
	 * dn: CN=NewUser,OU=newtop,DC=sub1,DC=w2k3,DC=vmnet1,DC=vm,DC=base
	 * sAMAccountName: NewUser2Sam
	 * userPrincipalName: NewUser2Princ@sub1.w2k3.vmnet1.vm.base
	 *
	 * 4279815024bda54fc074a5f8bd0a6e6f => NewUser2Sam:SUB1:TestPwd2007
	 * b7ec9da91062199aee7d121e6710fe23 => newuser2sam:sub1:TestPwd2007
	 * 17d290bc5c9f463fac54c37a8cea134d => NEWUSER2SAM:SUB1:TestPwd2007
	 * 4279815024bda54fc074a5f8bd0a6e6f => NewUser2Sam:SUB1:TestPwd2007
	 * 5d57e7823938348127322e08cd81bcb5 => NewUser2Sam:sub1:TestPwd2007
	 * 07dd701bf8a011ece585de3d47237140 => NEWUSER2SAM:sub1:TestPwd2007
	 * e14fb0eb401498d2cb33c9aae1cc7f37 => newuser2sam:SUB1:TestPwd2007
	 * 8dadc90250f873d8b883f79d890bef82 => NewUser2Sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * f52da1266a6bdd290ffd48b2c823dda7 => newuser2sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * d2b42f171248cec37a3c5c6b55404062 => NEWUSER2SAM:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * fff8d790ff6c152aaeb6ebe17b4021de => NewUser2Sam:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * 8dadc90250f873d8b883f79d890bef82 => NewUser2Sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * 2a7563c3715bc418d626dabef378c008 => NEWUSER2SAM:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * c8e9557a87cd4200fda0c11d2fa03f96 => newuser2sam:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * 221c55284451ae9b3aacaa2a3c86f10f => NewUser2Princ@sub1.w2k3.vmnet1.vm.base::TestPwd2007
	 * 74e1be668853d4324d38c07e2acfb8ea => (w2k3 has a bug here!) newuser2princ@sub1.w2k3.vmnet1.vm.base::TestPwd2007
	 * e1e244ab7f098e3ae1761be7f9229bbb => NEWUSER2PRINC@SUB1.W2K3.VMNET1.VM.BASE::TestPwd2007
	 * 86db637df42513039920e605499c3af6 => SUB1\NewUser2Sam::TestPwd2007
	 * f5e43474dfaf067fee8197a253debaa2 => sub1\newuser2sam::TestPwd2007
	 * 2ecaa8382e2518e4b77a52422b279467 => SUB1\NEWUSER2SAM::TestPwd2007
	 * 31dc704d3640335b2123d4ee28aa1f11 => ??? changes with NewUser2Sam => NewUser1Sam
	 * 36349f5cecd07320fb3bb0e119230c43 => ??? changes with NewUser2Sam => NewUser1Sam
	 * 12adf019d037fb535c01fd0608e78d9d => ??? changes with NewUser2Sam => NewUser1Sam
	 * 6feecf8e724906f3ee1105819c5105a1 => ??? changes with NewUser2Princ => NewUser1Princ
	 * 6c6911f3de6333422640221b9c51ff1f => ??? changes with NewUser2Princ => NewUser1Princ
	 * 4b279877e742895f9348ac67a8de2f69 => ??? changes with NewUser2Princ => NewUser1Princ
	 * db0c6bff069513e3ebb9870d29b57490 => ??? changes with NewUser2Sam => NewUser1Sam
	 * 45072621e56b1c113a4e04a8ff68cd0e => ??? changes with NewUser2Sam => NewUser1Sam
	 * 11d1220abc44a9c10cf91ef4a9c1de02 => ??? changes with NewUser2Sam => NewUser1Sam
	 *
	 * dn: CN=NewUser,OU=newtop,DC=sub1,DC=w2k3,DC=vmnet1,DC=vm,DC=base
	 * sAMAccountName: NewUser2Sam
	 *
	 * 4279815024bda54fc074a5f8bd0a6e6f => NewUser2Sam:SUB1:TestPwd2007
	 * b7ec9da91062199aee7d121e6710fe23 => newuser2sam:sub1:TestPwd2007
	 * 17d290bc5c9f463fac54c37a8cea134d => NEWUSER2SAM:SUB1:TestPwd2007
	 * 4279815024bda54fc074a5f8bd0a6e6f => NewUser2Sam:SUB1:TestPwd2007
	 * 5d57e7823938348127322e08cd81bcb5 => NewUser2Sam:sub1:TestPwd2007
	 * 07dd701bf8a011ece585de3d47237140 => NEWUSER2SAM:sub1:TestPwd2007
	 * e14fb0eb401498d2cb33c9aae1cc7f37 => newuser2sam:SUB1:TestPwd2007
	 * 8dadc90250f873d8b883f79d890bef82 => NewUser2Sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * f52da1266a6bdd290ffd48b2c823dda7 => newuser2sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * d2b42f171248cec37a3c5c6b55404062 => NEWUSER2SAM:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * fff8d790ff6c152aaeb6ebe17b4021de => NewUser2Sam:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * 8dadc90250f873d8b883f79d890bef82 => NewUser2Sam:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * 2a7563c3715bc418d626dabef378c008 => NEWUSER2SAM:sub1.w2k3.vmnet1.vm.base:TestPwd2007
	 * c8e9557a87cd4200fda0c11d2fa03f96 => newuser2sam:SUB1.W2K3.VMNET1.VM.BASE:TestPwd2007
	 * 8a140d30b6f0a5912735dc1e3bc993b4 => NewUser2Sam@sub1.w2k3.vmnet1.vm.base::TestPwd2007
	 * 86d95b2faae6cae4ec261e7fbaccf093 => (here w2k3 is correct) newuser2sam@sub1.w2k3.vmnet1.vm.base::TestPwd2007
	 * dfeff1493110220efcdfc6362e5f5450 => NEWUSER2SAM@SUB1.W2K3.VMNET1.VM.BASE::TestPwd2007
	 * 86db637df42513039920e605499c3af6 => SUB1\NewUser2Sam::TestPwd2007
	 * f5e43474dfaf067fee8197a253debaa2 => sub1\newuser2sam::TestPwd2007
	 * 2ecaa8382e2518e4b77a52422b279467 => SUB1\NEWUSER2SAM::TestPwd2007
	 * 31dc704d3640335b2123d4ee28aa1f11 => ???M1   changes with NewUser2Sam => NewUser1Sam
	 * 36349f5cecd07320fb3bb0e119230c43 => ???M1.L changes with newuser2sam => newuser1sam
	 * 12adf019d037fb535c01fd0608e78d9d => ???M1.U changes with NEWUSER2SAM => NEWUSER1SAM
	 * 569b4533f2d9e580211dd040e5e360a8 => ???M2   changes with NewUser2Princ => NewUser1Princ
	 * 52528bddf310a587c5d7e6a9ae2cbb20 => ???M2.L changes with newuser2princ => newuser1princ
	 * 4f629a4f0361289ca4255ab0f658fcd5 => ???M3 changes with NewUser2Princ => NewUser1Princ (doesn't depend on case of userPrincipal )
	 * db0c6bff069513e3ebb9870d29b57490 => ???M4 changes with NewUser2Sam => NewUser1Sam
	 * 45072621e56b1c113a4e04a8ff68cd0e => ???M5 changes with NewUser2Sam => NewUser1Sam (doesn't depend on case of sAMAccountName)
	 * 11d1220abc44a9c10cf91ef4a9c1de02 => ???M4.U changes with NEWUSER2SAM => NEWUSER1SAM
	 */

	/*
	 * sAMAccountName, netbios_domain
	 */
		{
		.user	= &sAMAccountName,
		.realm	= &netbios_domain,
		},
		{
		.user	= &sAMAccountName_l,
		.realm	= &netbios_domain_l,
		},
		{
		.user	= &sAMAccountName_u,
		.realm	= &netbios_domain_u,
		},
		{
		.user	= &sAMAccountName,
		.realm	= &netbios_domain_u,
		},
		{
		.user	= &sAMAccountName,
		.realm	= &netbios_domain_l,
		},
		{
		.user	= &sAMAccountName_u,
		.realm	= &netbios_domain_l,
		},
		{
		.user	= &sAMAccountName_l,
		.realm	= &netbios_domain_u,
		},
	/* 
	 * sAMAccountName, dns_domain
	 */
		{
		.user	= &sAMAccountName,
		.realm	= &dns_domain,
		},
		{
		.user	= &sAMAccountName_l,
		.realm	= &dns_domain_l,
		},
		{
		.user	= &sAMAccountName_u,
		.realm	= &dns_domain_u,
		},
		{
		.user	= &sAMAccountName,
		.realm	= &dns_domain_u,
		},
		{
		.user	= &sAMAccountName,
		.realm	= &dns_domain_l,
		},
		{
		.user	= &sAMAccountName_u,
		.realm	= &dns_domain_l,
		},
		{
		.user	= &sAMAccountName_l,
		.realm	= &dns_domain_u,
		},
	/* 
	 * userPrincipalName, no realm
	 */
		{
		.user	= &userPrincipalName,
		},
		{
		/* 
		 * NOTE: w2k3 messes this up, if the user has a real userPrincipalName,
		 *       the fallback to the sAMAccountName based userPrincipalName is correct
		 */
		.user	= &userPrincipalName_l,
		},
		{
		.user	= &userPrincipalName_u,
		},
	/* 
	 * nt4dom\sAMAccountName, no realm
	 */
		{
		.user	= &sAMAccountName,
		.nt4dom	= &netbios_domain
		},
		{
		.user	= &sAMAccountName_l,
		.nt4dom	= &netbios_domain_l
		},
		{
		.user	= &sAMAccountName_u,
		.nt4dom	= &netbios_domain_u
		},

	/*
	 * the following ones are guessed depending on the technet2 article
	 * but not reproducable on a w2k3 server
	 */
	/* sAMAccountName with "Digest" realm */
		{
		.user 	= &sAMAccountName,
		.realm	= &digest
		},
		{
		.user 	= &sAMAccountName_l,
		.realm	= &digest
		},
		{
		.user 	= &sAMAccountName_u,
		.realm	= &digest
		},
	/* userPrincipalName with "Digest" realm */
		{
		.user	= &userPrincipalName,
		.realm	= &digest
		},
		{
		.user	= &userPrincipalName_l,
		.realm	= &digest
		},
		{
		.user	= &userPrincipalName_u,
		.realm	= &digest
		},
	/* nt4dom\\sAMAccountName with "Digest" realm */
		{
		.user 	= &sAMAccountName,
		.nt4dom	= &netbios_domain,
		.realm	= &digest
		},
		{
		.user 	= &sAMAccountName_l,
		.nt4dom	= &netbios_domain_l,
		.realm	= &digest
		},
		{
		.user 	= &sAMAccountName_u,
		.nt4dom	= &netbios_domain_u,
		.realm	= &digest
		},
	};

	/* prepare DATA_BLOB's used in the combinations array */
	sAMAccountName		= data_blob_string_const(io->u.sAMAccountName);
	sAMAccountName_l	= data_blob_string_const(strlower_talloc(io->ac, io->u.sAMAccountName));
	if (!sAMAccountName_l.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	sAMAccountName_u	= data_blob_string_const(strupper_talloc(io->ac, io->u.sAMAccountName));
	if (!sAMAccountName_u.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* if the user doesn't have a userPrincipalName, create one (with lower case realm) */
	if (!user_principal_name) {
		user_principal_name = talloc_asprintf(io->ac, "%s@%s",
						      io->u.sAMAccountName,
						      io->domain->dns_domain);
		if (!user_principal_name) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}	
	}
	userPrincipalName	= data_blob_string_const(user_principal_name);
	userPrincipalName_l	= data_blob_string_const(strlower_talloc(io->ac, user_principal_name));
	if (!userPrincipalName_l.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	userPrincipalName_u	= data_blob_string_const(strupper_talloc(io->ac, user_principal_name));
	if (!userPrincipalName_u.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	netbios_domain		= data_blob_string_const(io->domain->netbios_domain);
	netbios_domain_l	= data_blob_string_const(strlower_talloc(io->ac, io->domain->netbios_domain));
	if (!netbios_domain_l.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	netbios_domain_u	= data_blob_string_const(strupper_talloc(io->ac, io->domain->netbios_domain));
	if (!netbios_domain_u.data) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dns_domain		= data_blob_string_const(io->domain->dns_domain);
	dns_domain_l		= data_blob_string_const(io->domain->dns_domain);
	dns_domain_u		= data_blob_string_const(io->domain->realm);

	digest			= data_blob_string_const("Digest");

	delim			= data_blob_string_const(":");
	backslash		= data_blob_string_const("\\");

	pdb->num_hashes	= ARRAY_SIZE(wdigest);
	pdb->hashes	= talloc_array(io->ac, struct package_PrimaryWDigestHash, pdb->num_hashes);
	if (!pdb->hashes) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i < ARRAY_SIZE(wdigest); i++) {
		struct MD5Context md5;
		MD5Init(&md5);
		if (wdigest[i].nt4dom) {
			MD5Update(&md5, wdigest[i].nt4dom->data, wdigest[i].nt4dom->length);
			MD5Update(&md5, backslash.data, backslash.length);
		}
		MD5Update(&md5, wdigest[i].user->data, wdigest[i].user->length);
		MD5Update(&md5, delim.data, delim.length);
		if (wdigest[i].realm) {
			MD5Update(&md5, wdigest[i].realm->data, wdigest[i].realm->length);
		}
		MD5Update(&md5, delim.data, delim.length);
		MD5Update(&md5, io->n.cleartext_utf8->data, io->n.cleartext_utf8->length);
		MD5Final(pdb->hashes[i].hash, &md5);
	}

	return LDB_SUCCESS;
}

static int setup_supplemental_field(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	struct supplementalCredentialsBlob scb;
	struct supplementalCredentialsBlob _old_scb;
	struct supplementalCredentialsBlob *old_scb = NULL;
	/* Packages + (Kerberos-Newer-Keys, Kerberos, WDigest and CLEARTEXT) */
	uint32_t num_names = 0;
	const char *names[1+4];
	uint32_t num_packages = 0;
	struct supplementalCredentialsPackage packages[1+4];
	/* Packages */
	struct supplementalCredentialsPackage *pp = NULL;
	struct package_PackagesBlob pb;
	DATA_BLOB pb_blob;
	char *pb_hexstr;
	/* Primary:Kerberos-Newer-Keys */
	const char **nkn = NULL;
	struct supplementalCredentialsPackage *pkn = NULL;
	struct package_PrimaryKerberosBlob pknb;
	DATA_BLOB pknb_blob;
	char *pknb_hexstr;
	/* Primary:Kerberos */
	const char **nk = NULL;
	struct supplementalCredentialsPackage *pk = NULL;
	struct package_PrimaryKerberosBlob pkb;
	DATA_BLOB pkb_blob;
	char *pkb_hexstr;
	/* Primary:WDigest */
	const char **nd = NULL;
	struct supplementalCredentialsPackage *pd = NULL;
	struct package_PrimaryWDigestBlob pdb;
	DATA_BLOB pdb_blob;
	char *pdb_hexstr;
	/* Primary:CLEARTEXT */
	const char **nc = NULL;
	struct supplementalCredentialsPackage *pc = NULL;
	struct package_PrimaryCLEARTEXTBlob pcb;
	DATA_BLOB pcb_blob;
	char *pcb_hexstr;
	int ret;
	enum ndr_err_code ndr_err;
	uint8_t zero16[16];
	bool do_newer_keys = false;
	bool do_cleartext = false;
	int *domainFunctionality;

	ZERO_STRUCT(zero16);
	ZERO_STRUCT(names);

	ldb = ldb_module_get_ctx(io->ac->module);

	if (!io->n.cleartext_utf8) {
		/* 
		 * when we don't have a cleartext password
		 * we can't setup a supplementalCredential value
		 */
		return LDB_SUCCESS;
	}

	/* if there's an old supplementaCredentials blob then parse it */
	if (io->o.supplemental) {
		ndr_err = ndr_pull_struct_blob_all(io->o.supplemental, io->ac,
						   lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
						   &_old_scb,
						   (ndr_pull_flags_fn_t)ndr_pull_supplementalCredentialsBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ldb_asprintf_errstring(ldb,
					       "setup_supplemental_field: "
					       "failed to pull old supplementalCredentialsBlob: %s",
					       nt_errstr(status));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (_old_scb.sub.signature == SUPPLEMENTAL_CREDENTIALS_SIGNATURE) {
			old_scb = &_old_scb;
		} else {
			ldb_debug(ldb, LDB_DEBUG_ERROR,
					       "setup_supplemental_field: "
					       "supplementalCredentialsBlob signature[0x%04X] expected[0x%04X]",
					       _old_scb.sub.signature, SUPPLEMENTAL_CREDENTIALS_SIGNATURE);
		}
	}
	/* Per MS-SAMR 3.1.1.8.11.6 we create AES keys if our domain functionality level is 2008 or higher */
	domainFunctionality = talloc_get_type(ldb_get_opaque(ldb, "domainFunctionality"), int);

	do_newer_keys = *domainFunctionality &&
		(*domainFunctionality >= DS_DOMAIN_FUNCTION_2008);

	if (io->domain->store_cleartext &&
	    (io->u.user_account_control & UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED)) {
		do_cleartext = true;
	}

	/*
	 * The ordering is this
	 *
	 * Primary:Kerberos-Newer-Keys (optional)
	 * Primary:Kerberos
	 * Primary:WDigest
	 * Primary:CLEARTEXT (optional)
	 *
	 * And the 'Packages' package is insert before the last
	 * other package.
	 */
	if (do_newer_keys) {
		/* Primary:Kerberos-Newer-Keys */
		nkn = &names[num_names++];
		pkn = &packages[num_packages++];
	}

	/* Primary:Kerberos */
	nk = &names[num_names++];
	pk = &packages[num_packages++];

	if (!do_cleartext) {
		/* Packages */
		pp = &packages[num_packages++];
	}

	/* Primary:WDigest */
	nd = &names[num_names++];
	pd = &packages[num_packages++];

	if (do_cleartext) {
		/* Packages */
		pp = &packages[num_packages++];

		/* Primary:CLEARTEXT */
		nc = &names[num_names++];
		pc = &packages[num_packages++];
	}

	if (pkn) {
		/*
		 * setup 'Primary:Kerberos-Newer-Keys' element
		 */
		*nkn = "Kerberos-Newer-Keys";

		ret = setup_primary_kerberos_newer(io, old_scb, &pknb);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ndr_err = ndr_push_struct_blob(&pknb_blob, io->ac,
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
					       &pknb,
					       (ndr_push_flags_fn_t)ndr_push_package_PrimaryKerberosBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ldb_asprintf_errstring(ldb,
					       "setup_supplemental_field: "
					       "failed to push package_PrimaryKerberosNeverBlob: %s",
					       nt_errstr(status));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		pknb_hexstr = data_blob_hex_string(io->ac, &pknb_blob);
		if (!pknb_hexstr) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		pkn->name	= "Primary:Kerberos-Newer-Keys";
		pkn->reserved	= 1;
		pkn->data	= pknb_hexstr;
	}

	/*
	 * setup 'Primary:Kerberos' element
	 */
	*nk = "Kerberos";

	ret = setup_primary_kerberos(io, old_scb, &pkb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ndr_err = ndr_push_struct_blob(&pkb_blob, io->ac, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &pkb,
				       (ndr_push_flags_fn_t)ndr_push_package_PrimaryKerberosBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		ldb_asprintf_errstring(ldb,
				       "setup_supplemental_field: "
				       "failed to push package_PrimaryKerberosBlob: %s",
				       nt_errstr(status));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pkb_hexstr = data_blob_hex_string(io->ac, &pkb_blob);
	if (!pkb_hexstr) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pk->name	= "Primary:Kerberos";
	pk->reserved	= 1;
	pk->data	= pkb_hexstr;

	/*
	 * setup 'Primary:WDigest' element
	 */
	*nd = "WDigest";

	ret = setup_primary_wdigest(io, old_scb, &pdb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ndr_err = ndr_push_struct_blob(&pdb_blob, io->ac, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &pdb,
				       (ndr_push_flags_fn_t)ndr_push_package_PrimaryWDigestBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		ldb_asprintf_errstring(ldb,
				       "setup_supplemental_field: "
				       "failed to push package_PrimaryWDigestBlob: %s",
				       nt_errstr(status));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pdb_hexstr = data_blob_hex_string(io->ac, &pdb_blob);
	if (!pdb_hexstr) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pd->name	= "Primary:WDigest";
	pd->reserved	= 1;
	pd->data	= pdb_hexstr;

	/*
	 * setup 'Primary:CLEARTEXT' element
	 */
	if (pc) {
		*nc		= "CLEARTEXT";

		pcb.cleartext	= *io->n.cleartext_utf16;

		ndr_err = ndr_push_struct_blob(&pcb_blob, io->ac, 
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
					       &pcb,
					       (ndr_push_flags_fn_t)ndr_push_package_PrimaryCLEARTEXTBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
			ldb_asprintf_errstring(ldb,
					       "setup_supplemental_field: "
					       "failed to push package_PrimaryCLEARTEXTBlob: %s",
					       nt_errstr(status));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		pcb_hexstr = data_blob_hex_string(io->ac, &pcb_blob);
		if (!pcb_hexstr) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		pc->name	= "Primary:CLEARTEXT";
		pc->reserved	= 1;
		pc->data	= pcb_hexstr;
	}

	/*
	 * setup 'Packages' element
	 */
	pb.names = names;
	ndr_err = ndr_push_struct_blob(&pb_blob, io->ac, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
				       &pb,
				       (ndr_push_flags_fn_t)ndr_push_package_PackagesBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		ldb_asprintf_errstring(ldb,
				       "setup_supplemental_field: "
				       "failed to push package_PackagesBlob: %s",
				       nt_errstr(status));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pb_hexstr = data_blob_hex_string(io->ac, &pb_blob);
	if (!pb_hexstr) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	pp->name	= "Packages";
	pp->reserved	= 2;
	pp->data	= pb_hexstr;

	/*
	 * setup 'supplementalCredentials' value
	 */
	ZERO_STRUCT(scb);
	scb.sub.num_packages	= num_packages;
	scb.sub.packages	= packages;

	ndr_err = ndr_push_struct_blob(&io->g.supplemental, io->ac, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &scb,
				       (ndr_push_flags_fn_t)ndr_push_supplementalCredentialsBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		ldb_asprintf_errstring(ldb,
				       "setup_supplemental_field: "
				       "failed to push supplementalCredentialsBlob: %s",
				       nt_errstr(status));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static int setup_last_set_field(struct setup_password_fields_io *io)
{
	/* set it as now */
	unix_to_nt_time(&io->g.last_set, time(NULL));

	return LDB_SUCCESS;
}

static int setup_kvno_field(struct setup_password_fields_io *io)
{
	/* increment by one */
	io->g.kvno = io->o.kvno + 1;

	return LDB_SUCCESS;
}

static int setup_password_fields(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	bool ok;
	int ret;
	size_t converted_pw_len;

	ldb = ldb_module_get_ctx(io->ac->module);

	/*
	 * refuse the change if someone want to change the cleartext
	 * and supply his own hashes at the same time...
	 */
	if ((io->n.cleartext_utf8 || io->n.cleartext_utf16) && (io->n.nt_hash || io->n.lm_hash)) {
		ldb_asprintf_errstring(ldb,
				       "setup_password_fields: "
				       "it's only allowed to set the cleartext password or the password hashes");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	
	if (io->n.cleartext_utf8 && io->n.cleartext_utf16) {
		ldb_asprintf_errstring(ldb,
				       "setup_password_fields: "
				       "it's only allowed to set the cleartext password as userPassword or clearTextPasssword, not both at once");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	
	if (io->n.cleartext_utf8) {
		char **cleartext_utf16_str;
		struct ldb_val *cleartext_utf16_blob;
		io->n.cleartext_utf16 = cleartext_utf16_blob = talloc(io->ac, struct ldb_val);
		if (!io->n.cleartext_utf16) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (!convert_string_talloc_convenience(io->ac, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
							 CH_UTF8, CH_UTF16, io->n.cleartext_utf8->data, io->n.cleartext_utf8->length, 
							 (void **)&cleartext_utf16_str, &converted_pw_len, false)) {
			ldb_asprintf_errstring(ldb,
					       "setup_password_fields: "
					       "failed to generate UTF16 password from cleartext UTF8 password");
			return LDB_ERR_OPERATIONS_ERROR;
		}
		*cleartext_utf16_blob = data_blob_const(cleartext_utf16_str, converted_pw_len);
	} else if (io->n.cleartext_utf16) {
		char *cleartext_utf8_str;
		struct ldb_val *cleartext_utf8_blob;
		io->n.cleartext_utf8 = cleartext_utf8_blob = talloc(io->ac, struct ldb_val);
		if (!io->n.cleartext_utf8) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (!convert_string_talloc_convenience(io->ac, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
							 CH_UTF16MUNGED, CH_UTF8, io->n.cleartext_utf16->data, io->n.cleartext_utf16->length, 
							 (void **)&cleartext_utf8_str, &converted_pw_len, false)) {
			/* We can't bail out entirely, as these unconvertable passwords are frustratingly valid */
			io->n.cleartext_utf8 = NULL;	
			talloc_free(cleartext_utf8_blob);
		}
		*cleartext_utf8_blob = data_blob_const(cleartext_utf8_str, converted_pw_len);
	}
	if (io->n.cleartext_utf16) {
		struct samr_Password *nt_hash;
		nt_hash = talloc(io->ac, struct samr_Password);
		if (!nt_hash) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		io->n.nt_hash = nt_hash;

		/* compute the new nt hash */
		mdfour(nt_hash->hash, io->n.cleartext_utf16->data, io->n.cleartext_utf16->length);
	}

	if (io->n.cleartext_utf8) {
		struct samr_Password *lm_hash;
		char *cleartext_unix;
		if (lp_lanman_auth(ldb_get_opaque(ldb, "loadparm")) &&
		    convert_string_talloc_convenience(io->ac, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
							 CH_UTF8, CH_UNIX, io->n.cleartext_utf8->data, io->n.cleartext_utf8->length, 
							 (void **)&cleartext_unix, &converted_pw_len, false)) {
			lm_hash = talloc(io->ac, struct samr_Password);
			if (!lm_hash) {
				ldb_oom(ldb);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			
			/* compute the new lm hash.   */
			ok = E_deshash((char *)cleartext_unix, lm_hash->hash);
			if (ok) {
				io->n.lm_hash = lm_hash;
			} else {
				talloc_free(lm_hash->hash);
			}
		}

		ret = setup_kerberos_keys(io);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	ret = setup_nt_fields(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = setup_lm_fields(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = setup_supplemental_field(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = setup_last_set_field(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = setup_kvno_field(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

static int setup_io(struct ph_context *ac, 
		    const struct ldb_message *new_msg, 
		    const struct ldb_message *searched_msg, 
		    struct setup_password_fields_io *io) 
{ 
	const struct ldb_val *quoted_utf16;
	struct ldb_context *ldb = ldb_module_get_ctx(ac->module);

	ZERO_STRUCTP(io);

	/* Some operations below require kerberos contexts */
	if (smb_krb5_init_context(ac,
				  ldb_get_event_context(ldb),
				  (struct loadparm_context *)ldb_get_opaque(ldb, "loadparm"),
				  &io->smb_krb5_context) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	io->ac				= ac;
	io->domain			= ac->domain;

	io->u.user_account_control	= samdb_result_uint(searched_msg, "userAccountControl", 0);
	io->u.sAMAccountName		= samdb_result_string(searched_msg, "samAccountName", NULL);
	io->u.user_principal_name	= samdb_result_string(searched_msg, "userPrincipalName", NULL);
	io->u.is_computer		= ldb_msg_check_string_attribute(searched_msg, "objectClass", "computer");

	io->n.cleartext_utf8		= ldb_msg_find_ldb_val(new_msg, "userPassword");
	io->n.cleartext_utf16		= ldb_msg_find_ldb_val(new_msg, "clearTextPassword");

	/* this rather strange looking piece of code is there to
	   handle a ldap client setting a password remotely using the
	   unicodePwd ldap field. The syntax is that the password is
	   in UTF-16LE, with a " at either end. Unfortunately the
	   unicodePwd field is also used to store the nt hashes
	   internally in Samba, and is used in the nt hash format on
	   the wire in DRS replication, so we have a single name for
	   two distinct values. The code below leaves us with a small
	   chance (less than 1 in 2^32) of a mixup, if someone manages
	   to create a MD4 hash which starts and ends in 0x22 0x00, as
	   that would then be treated as a UTF16 password rather than
	   a nthash */
	quoted_utf16			= ldb_msg_find_ldb_val(new_msg, "unicodePwd");
	if (quoted_utf16 && 
	    quoted_utf16->length >= 4 &&
	    quoted_utf16->data[0] == '"' && 
	    quoted_utf16->data[1] == 0 && 
	    quoted_utf16->data[quoted_utf16->length-2] == '"' && 
	    quoted_utf16->data[quoted_utf16->length-1] == 0) {
		io->n.quoted_utf16.data = talloc_memdup(io->ac, quoted_utf16->data+2, quoted_utf16->length-4);
		io->n.quoted_utf16.length = quoted_utf16->length-4;
		io->n.cleartext_utf16 = &io->n.quoted_utf16;
		io->n.nt_hash = NULL;
	} else {
		io->n.nt_hash		= samdb_result_hash(io->ac, new_msg, "unicodePwd");
	}

	io->n.lm_hash			= samdb_result_hash(io->ac, new_msg, "dBCSPwd");

	return LDB_SUCCESS;
}

static struct ph_context *ph_init_context(struct ldb_module *module,
					  struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ph_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct ph_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int ph_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ph_context *ac;

	ac = talloc_get_type(req->context, struct ph_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return ldb_module_done(ac->req, ares->controls,
				ares->response, ares->error);
}

static int password_hash_add_do_add(struct ph_context *ac);
static int ph_modify_callback(struct ldb_request *req, struct ldb_reply *ares);
static int password_hash_mod_search_self(struct ph_context *ac);
static int ph_mod_search_callback(struct ldb_request *req, struct ldb_reply *ares);
static int password_hash_mod_do_mod(struct ph_context *ac);

static int get_domain_data_callback(struct ldb_request *req,
				    struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct domain_data *data;
	struct ph_context *ac;
	int ret;
	char *tmp;
	char *p;

	ac = talloc_get_type(req->context, struct ph_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		if (ac->domain != NULL) {
			ldb_set_errstring(ldb, "Too many results");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		data = talloc_zero(ac, struct domain_data);
		if (data == NULL) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		data->pwdProperties = samdb_result_uint(ares->message, "pwdProperties", 0);
		data->store_cleartext = data->pwdProperties & DOMAIN_PASSWORD_STORE_CLEARTEXT;
		data->pwdHistoryLength = samdb_result_uint(ares->message, "pwdHistoryLength", 0);

		/* For a domain DN, this puts things in dotted notation */
		/* For builtin domains, this will give details for the host,
		 * but that doesn't really matter, as it's just used for salt
		 * and kerberos principals, which don't exist here */

		tmp = ldb_dn_canonical_string(data, ares->message->dn);
		if (!tmp) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* But it puts a trailing (or just before 'builtin') / on things, so kill that */
		p = strchr(tmp, '/');
		if (p) {
			p[0] = '\0';
		}

		data->dns_domain = strlower_talloc(data, tmp);
		if (data->dns_domain == NULL) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		data->realm = strupper_talloc(data, tmp);
		if (data->realm == NULL) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		/* FIXME: NetbIOS name is *always* the first domain component ?? -SSS */
		p = strchr(tmp, '.');
		if (p) {
			p[0] = '\0';
		}
		data->netbios_domain = strupper_talloc(data, tmp);
		if (data->netbios_domain == NULL) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		talloc_free(tmp);
		ac->domain = data;
		break;

	case LDB_REPLY_DONE:

		/* call the next step */
		switch (ac->req->operation) {
		case LDB_ADD:
			ret = password_hash_add_do_add(ac);
			break;

		case LDB_MODIFY:
			ret = password_hash_mod_do_mod(ac);
			break;

		default:
			ret = LDB_ERR_OPERATIONS_ERROR;
			break;
		}
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int build_domain_data_request(struct ph_context *ac)
{
	/* attrs[] is returned from this function in
	   ac->dom_req->op.search.attrs, so it must be static, as
	   otherwise the compiler can put it on the stack */
	struct ldb_context *ldb;
	static const char * const attrs[] = { "pwdProperties", "pwdHistoryLength", NULL };
	char *filter;

	ldb = ldb_module_get_ctx(ac->module);

	filter = talloc_asprintf(ac,
				"(&(objectSid=%s)(|(|(objectClass=domain)(objectClass=builtinDomain))(objectClass=samba4LocalDomain)))",
				 ldap_encode_ndr_dom_sid(ac, ac->domain_sid));
	if (filter == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_build_search_req(&ac->dom_req, ldb, ac,
				    ldb_get_default_basedn(ldb),
				    LDB_SCOPE_SUBTREE,
				    filter, attrs,
				    NULL,
				    ac, get_domain_data_callback,
				    ac->req);
}

static int password_hash_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	struct ldb_message_element *sambaAttr;
	struct ldb_message_element *clearTextPasswordAttr;
	struct ldb_message_element *ntAttr;
	struct ldb_message_element *lmAttr;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "password_hash_add\n");

	if (ldb_dn_is_special(req->op.add.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	/* If the caller is manipulating the local passwords directly, let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.add.message->dn) == 0) {
		return ldb_next_request(module, req);
	}

	/* nobody must touch these fields */
	if (ldb_msg_find_element(req->op.add.message, "ntPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "lmPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "supplementalCredentials")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* If no part of this ADD touches the userPassword, or the NT
	 * or LM hashes, then we don't need to make any changes.  */

	sambaAttr = ldb_msg_find_element(req->op.mod.message, "userPassword");
	clearTextPasswordAttr = ldb_msg_find_element(req->op.mod.message, "clearTextPassword");
	ntAttr = ldb_msg_find_element(req->op.mod.message, "unicodePwd");
	lmAttr = ldb_msg_find_element(req->op.mod.message, "dBCSPwd");

	if ((!sambaAttr) && (!clearTextPasswordAttr) && (!ntAttr) && (!lmAttr)) {
		return ldb_next_request(module, req);
	}

	/* if it is not an entry of type person its an error */
	/* TODO: remove this when userPassword will be in schema */
	if (!ldb_msg_check_string_attribute(req->op.add.message, "objectClass", "person")) {
		ldb_set_errstring(ldb, "Cannot set a password on entry that does not have objectClass 'person'");
		return LDB_ERR_OBJECT_CLASS_VIOLATION;
	}

	/* check userPassword is single valued here */
	/* TODO: remove this when userPassword will be single valued in schema */
	if (sambaAttr && sambaAttr->num_values > 1) {
		ldb_set_errstring(ldb, "mupltiple values for userPassword not allowed!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (clearTextPasswordAttr && clearTextPasswordAttr->num_values > 1) {
		ldb_set_errstring(ldb, "mupltiple values for clearTextPassword not allowed!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	if (ntAttr && (ntAttr->num_values > 1)) {
		ldb_set_errstring(ldb, "mupltiple values for unicodePwd not allowed!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (lmAttr && (lmAttr->num_values > 1)) {
		ldb_set_errstring(ldb, "mupltiple values for dBCSPwd not allowed!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	if (sambaAttr && sambaAttr->num_values == 0) {
		ldb_set_errstring(ldb, "userPassword must have a value!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	if (clearTextPasswordAttr && clearTextPasswordAttr->num_values == 0) {
		ldb_set_errstring(ldb, "clearTextPassword must have a value!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	if (ntAttr && (ntAttr->num_values == 0)) {
		ldb_set_errstring(ldb, "unicodePwd must have a value!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (lmAttr && (lmAttr->num_values == 0)) {
		ldb_set_errstring(ldb, "dBCSPwd must have a value!\n");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = ph_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* get user domain data */
	ac->domain_sid = samdb_result_sid_prefix(ac, req->op.add.message, "objectSid");
	if (ac->domain_sid == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "can't handle entry with missing objectSid!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = build_domain_data_request(ac);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, ac->dom_req);
}

static int password_hash_add_do_add(struct ph_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	struct setup_password_fields_io io;
	int ret;

	/* Prepare the internal data structure containing the passwords */
	ret = setup_io(ac, ac->req->op.add.message, ac->req->op.add.message, &io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* remove attributes that we just read into 'io' */
	ldb_msg_remove_attr(msg, "userPassword");
	ldb_msg_remove_attr(msg, "clearTextPassword");
	ldb_msg_remove_attr(msg, "unicodePwd");
	ldb_msg_remove_attr(msg, "dBCSPwd");
	ldb_msg_remove_attr(msg, "pwdLastSet");
	io.o.kvno = samdb_result_uint(msg, "msDs-KeyVersionNumber", 1) - 1;
	ldb_msg_remove_attr(msg, "msDs-KeyVersionNumber");

	ldb = ldb_module_get_ctx(ac->module);

	ret = setup_password_fields(&io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (io.g.nt_hash) {
		ret = samdb_msg_add_hash(ldb, ac, msg,
					 "unicodePwd", io.g.nt_hash);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_hash) {
		ret = samdb_msg_add_hash(ldb, ac, msg,
					 "dBCSPwd", io.g.lm_hash);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.nt_history_len > 0) {
		ret = samdb_msg_add_hashes(ac, msg,
					   "ntPwdHistory",
					   io.g.nt_history,
					   io.g.nt_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_history_len > 0) {
		ret = samdb_msg_add_hashes(ac, msg,
					   "lmPwdHistory",
					   io.g.lm_history,
					   io.g.lm_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.supplemental.length > 0) {
		ret = ldb_msg_add_value(msg, "supplementalCredentials",
					&io.g.supplemental, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	ret = samdb_msg_add_uint64(ldb, ac, msg,
				   "pwdLastSet",
				   io.g.last_set);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = samdb_msg_add_uint(ldb, ac, msg,
				 "msDs-KeyVersionNumber",
				 io.g.kvno);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, ph_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, down_req);
}

static int password_hash_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	struct ldb_message_element *sambaAttr;
	struct ldb_message_element *clearTextAttr;
	struct ldb_message_element *ntAttr;
	struct ldb_message_element *lmAttr;
	struct ldb_message *msg;
	struct ldb_request *down_req;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "password_hash_modify\n");

	if (ldb_dn_is_special(req->op.mod.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}
	
	/* If the caller is manipulating the local passwords directly, let them pass */
	if (ldb_dn_compare_base(ldb_dn_new(req, ldb, LOCAL_BASE),
				req->op.mod.message->dn) == 0) {
		return ldb_next_request(module, req);
	}

	/* nobody must touch password Histories */
	if (ldb_msg_find_element(req->op.add.message, "ntPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "lmPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "supplementalCredentials")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	sambaAttr = ldb_msg_find_element(req->op.mod.message, "userPassword");
	clearTextAttr = ldb_msg_find_element(req->op.mod.message, "clearTextPassword");
	ntAttr = ldb_msg_find_element(req->op.mod.message, "unicodePwd");
	lmAttr = ldb_msg_find_element(req->op.mod.message, "dBCSPwd");

	/* If no part of this touches the userPassword OR
	 * clearTextPassword OR unicodePwd and/or dBCSPwd, then we
	 * don't need to make any changes.  For password changes/set
	 * there should be a 'delete' or a 'modify' on this
	 * attribute. */
	if ((!sambaAttr) && (!clearTextAttr) && (!ntAttr) && (!lmAttr)) {
		return ldb_next_request(module, req);
	}

	/* check passwords are single valued here */
	/* TODO: remove this when passwords will be single valued in schema */
	if (sambaAttr && (sambaAttr->num_values > 1)) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (clearTextAttr && (clearTextAttr->num_values > 1)) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (ntAttr && (ntAttr->num_values > 1)) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (lmAttr && (lmAttr->num_values > 1)) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = ph_init_context(module, req);
	if (!ac) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* use a new message structure so that we can modify it */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* - remove any modification to the password from the first commit
	 *   we will make the real modification later */
	if (sambaAttr) ldb_msg_remove_attr(msg, "userPassword");
	if (clearTextAttr) ldb_msg_remove_attr(msg, "clearTextPassword");
	if (ntAttr) ldb_msg_remove_attr(msg, "unicodePwd");
	if (lmAttr) ldb_msg_remove_attr(msg, "dBCSPwd");

	/* if there was nothing else to be modified skip to next step */
	if (msg->num_elements == 0) {
		return password_hash_mod_search_self(ac);
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, ph_modify_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

static int ph_modify_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ph_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct ph_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ret = password_hash_mod_search_self(ac);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int ph_mod_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct ph_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* we are interested only in the single reply (base search) */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:

		if (ac->search_res != NULL) {
			ldb_set_errstring(ldb, "Too many results");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* if it is not an entry of type person this is an error */
		/* TODO: remove this when sambaPassword will be in schema */
		if (!ldb_msg_check_string_attribute(ares->message, "objectClass", "person")) {
			ldb_set_errstring(ldb, "Object class violation");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OBJECT_CLASS_VIOLATION);
		}

		ac->search_res = talloc_steal(ac, ares);
		return LDB_SUCCESS;

	case LDB_REPLY_DONE:

		/* get object domain sid */
		ac->domain_sid = samdb_result_sid_prefix(ac,
							ac->search_res->message,
							"objectSid");
		if (ac->domain_sid == NULL) {
			ldb_debug(ldb, LDB_DEBUG_ERROR,
				  "can't handle entry without objectSid!\n");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* get user domain data */
		ret = build_domain_data_request(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL,ret);
		}

		return ldb_next_request(ac->module, ac->dom_req);

	case LDB_REPLY_REFERRAL:
		/*ignore anything else for now */
		break;
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int password_hash_mod_search_self(struct ph_context *ac)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "userAccountControl", "lmPwdHistory", 
					      "ntPwdHistory", 
					      "objectSid", "msDS-KeyVersionNumber", 
					      "objectClass", "userPrincipalName",
					      "sAMAccountName", 
					      "dBCSPwd", "unicodePwd",
					      "supplementalCredentials",
					      NULL };
	struct ldb_request *search_req;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->req->op.mod.message->dn,
				   LDB_SCOPE_BASE,
				   "(objectclass=*)",
				   attrs,
				   NULL,
				   ac, ph_mod_search_callback,
				   ac->req);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, search_req);
}

static int password_hash_mod_do_mod(struct ph_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *mod_req;
	struct ldb_message *msg;
	const struct ldb_message *searched_msg;
	struct setup_password_fields_io io;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* use a new message structure so that we can modify it */
	msg = ldb_msg_new(ac);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* modify dn */
	msg->dn = ac->req->op.mod.message->dn;

	/* Prepare the internal data structure containing the passwords */
	ret = setup_io(ac, 
		       ac->req->op.mod.message, 
		       ac->search_res->message, 
		       &io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	searched_msg = ac->search_res->message;

	/* Fill in some final details (only relevent once the password has been set) */
	io.o.kvno			= samdb_result_uint(searched_msg, "msDs-KeyVersionNumber", 0);
	io.o.nt_history_len		= samdb_result_hashes(io.ac, searched_msg, "ntPwdHistory", &io.o.nt_history);
	io.o.lm_history_len		= samdb_result_hashes(io.ac, searched_msg, "lmPwdHistory", &io.o.lm_history);
	io.o.supplemental		= ldb_msg_find_ldb_val(searched_msg, "supplementalCredentials");

	ret = setup_password_fields(&io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* make sure we replace all the old attributes */
	ret = ldb_msg_add_empty(msg, "unicodePwd", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "dBCSPwd", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "ntPwdHistory", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "lmPwdHistory", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "supplementalCredentials", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "pwdLastSet", LDB_FLAG_MOD_REPLACE, NULL);
	ret = ldb_msg_add_empty(msg, "msDs-KeyVersionNumber", LDB_FLAG_MOD_REPLACE, NULL);

	if (io.g.nt_hash) {
		ret = samdb_msg_add_hash(ldb, ac, msg,
					 "unicodePwd", io.g.nt_hash);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_hash) {
		ret = samdb_msg_add_hash(ldb, ac, msg,
					 "dBCSPwd", io.g.lm_hash);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.nt_history_len > 0) {
		ret = samdb_msg_add_hashes(ac, msg,
					   "ntPwdHistory",
					   io.g.nt_history,
					   io.g.nt_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_history_len > 0) {
		ret = samdb_msg_add_hashes(ac, msg,
					   "lmPwdHistory",
					   io.g.lm_history,
					   io.g.lm_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.supplemental.length > 0) {
		ret = ldb_msg_add_value(msg, "supplementalCredentials",
					&io.g.supplemental, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	ret = samdb_msg_add_uint64(ldb, ac, msg,
				   "pwdLastSet",
				   io.g.last_set);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = samdb_msg_add_uint(ldb, ac, msg,
				 "msDs-KeyVersionNumber",
				 io.g.kvno);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_mod_req(&mod_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, ph_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, mod_req);
}

_PUBLIC_ const struct ldb_module_ops ldb_password_hash_module_ops = {
	.name          = "password_hash",
	.add           = password_hash_add,
	.modify        = password_hash_modify,
};
