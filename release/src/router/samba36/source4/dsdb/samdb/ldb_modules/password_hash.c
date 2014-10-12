/* 
   ldb database module

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2006
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2007-2010
   Copyright (C) Matthias Dieter Wallnöfer 2009-2010

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
 *  Description: correctly handle AD password changes fields
 *
 *  Author: Andrew Bartlett
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "ldb_module.h"
#include "libcli/auth/libcli_auth.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "dsdb/samdb/ldb_modules/password_modules.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/crypto.h"
#include "param/param.h"

/* If we have decided there is a reason to work on this request, then
 * setup all the password hash types correctly.
 *
 * If we haven't the hashes yet but the password given as plain-text (attributes
 * 'unicodePwd', 'userPassword' and 'clearTextPassword') we have to check for
 * the constraints. Once this is done, we calculate the password hashes.
 *
 * Notice: unlike the real AD which only supports the UTF16 special based
 * 'unicodePwd' and the UTF8 based 'userPassword' plaintext attribute we
 * understand also a UTF16 based 'clearTextPassword' one.
 * The latter is also accessible through LDAP so it can also be set by external
 * tools and scripts. But be aware that this isn't portable on non SAMBA 4 ADs!
 *
 * Also when the module receives only the password hashes (possible through
 * specifying an internal LDB control - for security reasons) some checks are
 * performed depending on the operation mode (see below) (e.g. if the password
 * has been in use before if the password memory policy was activated).
 *
 * Attention: There is a difference between "modify" and "reset" operations
 * (see MS-ADTS 3.1.1.3.1.5). If the client sends a "add" and "remove"
 * operation for a password attribute we thread this as a "modify"; if it sends
 * only a "replace" one we have an (administrative) reset.
 *
 * Finally, if the administrator has requested that a password history
 * be maintained, then this should also be written out.
 *
 */

/* TODO: [consider always MS-ADTS 3.1.1.3.1.5]
 * - Check for right connection encryption
 */

/* Notice: Definition of "dsdb_control_password_change_status" moved into
 * "samdb.h" */

struct ph_context {
	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_request *dom_req;
	struct ldb_reply *dom_res;

	struct ldb_reply *search_res;

	struct dsdb_control_password_change_status *status;
	struct dsdb_control_password_change *change;

	bool pwd_reset;
	bool change_status;
	bool hash_values;
	bool userPassword;
};


struct setup_password_fields_io {
	struct ph_context *ac;

	struct smb_krb5_context *smb_krb5_context;

	/* infos about the user account */
	struct {
		uint32_t userAccountControl;
		NTTIME pwdLastSet;
		const char *sAMAccountName;
		const char *user_principal_name;
		bool is_computer;
		uint32_t restrictions;
	} u;

	/* new credentials and old given credentials */
	struct setup_password_fields_given {
		const struct ldb_val *cleartext_utf8;
		const struct ldb_val *cleartext_utf16;
		struct samr_Password *nt_hash;
		struct samr_Password *lm_hash;
	} n, og;

	/* old credentials */
	struct {
		struct samr_Password *nt_hash;
		struct samr_Password *lm_hash;
		uint32_t nt_history_len;
		struct samr_Password *nt_history;
		uint32_t lm_history_len;
		struct samr_Password *lm_history;
		const struct ldb_val *supplemental;
		struct supplementalCredentialsBlob scb;
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
	} g;
};

static int password_hash_bypass(struct ldb_module *module, struct ldb_request *request)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	const struct ldb_message *msg;
	struct ldb_message_element *nte;
	struct ldb_message_element *lme;
	struct ldb_message_element *nthe;
	struct ldb_message_element *lmhe;
	struct ldb_message_element *sce;

	switch (request->operation) {
	case LDB_ADD:
		msg = request->op.add.message;
		break;
	case LDB_MODIFY:
		msg = request->op.mod.message;
		break;
	default:
		return ldb_next_request(module, request);
	}

	/* nobody must touch password histories and 'supplementalCredentials' */
	nte = dsdb_get_single_valued_attr(msg, "unicodePwd",
					  request->operation);
	lme = dsdb_get_single_valued_attr(msg, "dBCSPwd",
					  request->operation);
	nthe = dsdb_get_single_valued_attr(msg, "ntPwdHistory",
					   request->operation);
	lmhe = dsdb_get_single_valued_attr(msg, "lmPwdHistory",
					   request->operation);
	sce = dsdb_get_single_valued_attr(msg, "supplementalCredentials",
					  request->operation);

#define CHECK_HASH_ELEMENT(e, min, max) do {\
	if (e && e->num_values) { \
		unsigned int _count; \
		if (e->num_values != 1) { \
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION, \
					 "num_values != 1"); \
		} \
		if ((e->values[0].length % 16) != 0) { \
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION, \
					 "length % 16 != 0"); \
		} \
		_count = e->values[0].length / 16; \
		if (_count < min) { \
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION, \
					 "count < min"); \
		} \
		if (_count > max) { \
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION, \
					 "count > max"); \
		} \
	} \
} while (0)

	CHECK_HASH_ELEMENT(nte, 1, 1);
	CHECK_HASH_ELEMENT(lme, 1, 1);
	CHECK_HASH_ELEMENT(nthe, 1, INT32_MAX);
	CHECK_HASH_ELEMENT(lmhe, 1, INT32_MAX);

	if (sce && sce->num_values) {
		enum ndr_err_code ndr_err;
		struct supplementalCredentialsBlob *scb;
		struct supplementalCredentialsPackage *scpp = NULL;
		struct supplementalCredentialsPackage *scpk = NULL;
		struct supplementalCredentialsPackage *scpkn = NULL;
		struct supplementalCredentialsPackage *scpct = NULL;
		DATA_BLOB scpbp = data_blob_null;
		DATA_BLOB scpbk = data_blob_null;
		DATA_BLOB scpbkn = data_blob_null;
		DATA_BLOB scpbct = data_blob_null;
		DATA_BLOB blob;
		uint32_t i;

		if (sce->num_values != 1) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "num_values != 1");
		}

		scb = talloc_zero(request, struct supplementalCredentialsBlob);
		if (!scb) {
			return ldb_module_oom(module);
		}

		ndr_err = ndr_pull_struct_blob_all(&sce->values[0], scb, scb,
				(ndr_pull_flags_fn_t)ndr_pull_supplementalCredentialsBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "ndr_pull_struct_blob_all");
		}

		if (scb->sub.num_packages < 2) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "num_packages < 2");
		}

		for (i=0; i < scb->sub.num_packages; i++) {
			DATA_BLOB subblob;

			subblob = strhex_to_data_blob(scb, scb->sub.packages[i].data);
			if (subblob.data == NULL) {
				return ldb_module_oom(module);
			}

			if (strcmp(scb->sub.packages[i].name, "Packages") == 0) {
				if (scpp) {
					return ldb_error(ldb,
							 LDB_ERR_CONSTRAINT_VIOLATION,
							 "Packages twice");
				}
				scpp = &scb->sub.packages[i];
				scpbp = subblob;
				continue;
			}
			if (strcmp(scb->sub.packages[i].name, "Primary:Kerberos") == 0) {
				if (scpk) {
					return ldb_error(ldb,
							 LDB_ERR_CONSTRAINT_VIOLATION,
							 "Primary:Kerberos twice");
				}
				scpk = &scb->sub.packages[i];
				scpbk = subblob;
				continue;
			}
			if (strcmp(scb->sub.packages[i].name, "Primary:Kerberos-Newer-Keys") == 0) {
				if (scpkn) {
					return ldb_error(ldb,
							 LDB_ERR_CONSTRAINT_VIOLATION,
							 "Primary:Kerberos-Newer-Keys twice");
				}
				scpkn = &scb->sub.packages[i];
				scpbkn = subblob;
				continue;
			}
			if (strcmp(scb->sub.packages[i].name, "Primary:CLEARTEXT") == 0) {
				if (scpct) {
					return ldb_error(ldb,
							 LDB_ERR_CONSTRAINT_VIOLATION,
							 "Primary:CLEARTEXT twice");
				}
				scpct = &scb->sub.packages[i];
				scpbct = subblob;
				continue;
			}

			data_blob_free(&subblob);
		}

		if (scpp) {
			struct package_PackagesBlob *p;
			uint32_t n;

			p = talloc_zero(scb, struct package_PackagesBlob);
			if (p == NULL) {
				return ldb_module_oom(module);
			}

			ndr_err = ndr_pull_struct_blob(&scpbp, p, p,
					(ndr_pull_flags_fn_t)ndr_pull_package_PackagesBlob);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "ndr_pull_struct_blob Packages");
			}

			if (p->names == NULL) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "Packages names == NULL");
			}

			for (n = 0; p->names[n]; n++) {
				/* noop */
			}

			if (scb->sub.num_packages != (n + 1)) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "Packages num_packages != num_names + 1");
			}

			talloc_free(p);
		}

		if (scpk) {
			struct package_PrimaryKerberosBlob *k;

			k = talloc_zero(scb, struct package_PrimaryKerberosBlob);
			if (k == NULL) {
				return ldb_module_oom(module);
			}

			ndr_err = ndr_pull_struct_blob(&scpbk, k, k,
					(ndr_pull_flags_fn_t)ndr_pull_package_PrimaryKerberosBlob);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "ndr_pull_struct_blob PrimaryKerberos");
			}

			if (k->version != 3) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos version != 3");
			}

			if (k->ctr.ctr3.salt.string == NULL) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos salt == NULL");
			}

			if (strlen(k->ctr.ctr3.salt.string) == 0) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos strlen(salt) == 0");
			}

			if (k->ctr.ctr3.num_keys != 2) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos num_keys != 2");
			}

			if (k->ctr.ctr3.num_old_keys > k->ctr.ctr3.num_keys) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos num_old_keys > num_keys");
			}

			if (k->ctr.ctr3.keys[0].keytype != ENCTYPE_DES_CBC_MD5) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos key[0] != DES_CBC_MD5");
			}
			if (k->ctr.ctr3.keys[1].keytype != ENCTYPE_DES_CBC_CRC) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos key[1] != DES_CBC_CRC");
			}

			if (k->ctr.ctr3.keys[0].value_len != 8) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos key[0] value_len != 8");
			}
			if (k->ctr.ctr3.keys[1].value_len != 8) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos key[1] value_len != 8");
			}

			for (i = 0; i < k->ctr.ctr3.num_old_keys; i++) {
				if (k->ctr.ctr3.old_keys[i].keytype ==
				    k->ctr.ctr3.keys[i].keytype &&
				    k->ctr.ctr3.old_keys[i].value_len ==
				    k->ctr.ctr3.keys[i].value_len) {
					continue;
				}

				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryKerberos old_keys type/value_len doesn't match");
			}

			talloc_free(k);
		}

		if (scpkn) {
			struct package_PrimaryKerberosBlob *k;

			k = talloc_zero(scb, struct package_PrimaryKerberosBlob);
			if (k == NULL) {
				return ldb_module_oom(module);
			}

			ndr_err = ndr_pull_struct_blob(&scpbkn, k, k,
					(ndr_pull_flags_fn_t)ndr_pull_package_PrimaryKerberosBlob);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "ndr_pull_struct_blob PrimaryKerberosNeverKeys");
			}

			if (k->version != 4) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNerverKeys version != 4");
			}

			if (k->ctr.ctr4.salt.string == NULL) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys salt == NULL");
			}

			if (strlen(k->ctr.ctr4.salt.string) == 0) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys strlen(salt) == 0");
			}

			if (k->ctr.ctr4.num_keys != 4) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys num_keys != 2");
			}

			if (k->ctr.ctr4.num_old_keys > k->ctr.ctr4.num_keys) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys num_old_keys > num_keys");
			}

			if (k->ctr.ctr4.num_older_keys > k->ctr.ctr4.num_old_keys) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys num_older_keys > num_old_keys");
			}

			if (k->ctr.ctr4.keys[0].keytype != ENCTYPE_AES256_CTS_HMAC_SHA1_96) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[0] != AES256");
			}
			if (k->ctr.ctr4.keys[1].keytype != ENCTYPE_AES128_CTS_HMAC_SHA1_96) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[1] != AES128");
			}
			if (k->ctr.ctr4.keys[2].keytype != ENCTYPE_DES_CBC_MD5) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[2] != DES_CBC_MD5");
			}
			if (k->ctr.ctr4.keys[3].keytype != ENCTYPE_DES_CBC_CRC) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[3] != DES_CBC_CRC");
			}

			if (k->ctr.ctr4.keys[0].value_len != 32) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[0] value_len != 32");
			}
			if (k->ctr.ctr4.keys[1].value_len != 16) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[1] value_len != 16");
			}
			if (k->ctr.ctr4.keys[2].value_len != 8) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[2] value_len != 8");
			}
			if (k->ctr.ctr4.keys[3].value_len != 8) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "KerberosNewerKeys key[3] value_len != 8");
			}

			/*
			 * TODO:
			 * Maybe we can check old and older keys here.
			 * But we need to do some tests, if the old keys
			 * can be taken from the PrimaryKerberos blob
			 * (with only des keys), when the domain was upgraded
			 * from w2k3 to w2k8.
			 */

			talloc_free(k);
		}

		if (scpct) {
			struct package_PrimaryCLEARTEXTBlob *ct;

			ct = talloc_zero(scb, struct package_PrimaryCLEARTEXTBlob);
			if (ct == NULL) {
				return ldb_module_oom(module);
			}

			ndr_err = ndr_pull_struct_blob(&scpbct, ct, ct,
					(ndr_pull_flags_fn_t)ndr_pull_package_PrimaryCLEARTEXTBlob);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "ndr_pull_struct_blob PrimaryCLEARTEXT");
			}

			if ((ct->cleartext.length % 2) != 0) {
				return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
						 "PrimaryCLEARTEXT length % 2 != 0");
			}

			talloc_free(ct);
		}

		ndr_err = ndr_push_struct_blob(&blob, scb, scb,
				(ndr_push_flags_fn_t)ndr_push_supplementalCredentialsBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "ndr_pull_struct_blob_all");
		}

		if (sce->values[0].length != blob.length) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "supplementalCredentialsBlob length differ");
		}

		if (memcmp(sce->values[0].data, blob.data, blob.length) != 0) {
			return ldb_error(ldb, LDB_ERR_CONSTRAINT_VIOLATION,
					 "supplementalCredentialsBlob memcmp differ");
		}

		talloc_free(scb);
	}

	ldb_debug(ldb, LDB_DEBUG_TRACE, "password_hash_bypass - validated\n");
	return ldb_next_request(module, request);
}

/* Get the NT hash, and fill it in as an entry in the password history, 
   and specify it into io->g.nt_hash */

static int setup_nt_fields(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	uint32_t i;

	io->g.nt_hash = io->n.nt_hash;
	ldb = ldb_module_get_ctx(io->ac->module);

	if (io->ac->status->domain_data.pwdHistoryLength == 0) {
		return LDB_SUCCESS;
	}

	/* We might not have an old NT password */
	io->g.nt_history = talloc_array(io->ac,
					struct samr_Password,
					io->ac->status->domain_data.pwdHistoryLength);
	if (!io->g.nt_history) {
		return ldb_oom(ldb);
	}

	for (i = 0; i < MIN(io->ac->status->domain_data.pwdHistoryLength-1,
			    io->o.nt_history_len); i++) {
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

	if (io->ac->status->domain_data.pwdHistoryLength == 0) {
		return LDB_SUCCESS;
	}

	/* We might not have an old LM password */
	io->g.lm_history = talloc_array(io->ac,
					struct samr_Password,
					io->ac->status->domain_data.pwdHistoryLength);
	if (!io->g.lm_history) {
		return ldb_oom(ldb);
	}

	for (i = 0; i < MIN(io->ac->status->domain_data.pwdHistoryLength-1,
			    io->o.lm_history_len); i++) {
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
			return ldb_oom(ldb);
		}

		if (name[strlen(name)-1] == '$') {
			name[strlen(name)-1] = '\0';
		}

		saltbody = talloc_asprintf(io->ac, "%s.%s", name,
					   io->ac->status->domain_data.dns_domain);
		if (!saltbody) {
			return ldb_oom(ldb);
		}
		
		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->ac->status->domain_data.realm,
					       "host", saltbody, NULL);
	} else if (io->u.user_principal_name) {
		char *user_principal_name;
		char *p;

		user_principal_name = talloc_strdup(io->ac, io->u.user_principal_name);
		if (!user_principal_name) {
			return ldb_oom(ldb);
		}

		p = strchr(user_principal_name, '@');
		if (p) {
			p[0] = '\0';
		}

		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->ac->status->domain_data.realm,
					       user_principal_name, NULL);
	} else {
		krb5_ret = krb5_make_principal(io->smb_krb5_context->krb5_context,
					       &salt_principal,
					       io->ac->status->domain_data.realm,
					       io->u.sAMAccountName, NULL);
	}
	if (krb5_ret) {
		ldb_asprintf_errstring(ldb,
				       "setup_kerberos_keys: "
				       "generation of a salting principal failed: %s",
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
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
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	/* create a talloc copy */
	io->g.salt = talloc_strndup(io->ac,
				    (char *)salt.saltvalue.data,
				    salt.saltvalue.length);
	krb5_free_salt(io->smb_krb5_context->krb5_context, salt);
	if (!io->g.salt) {
		return ldb_oom(ldb);
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
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.aes_256 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.aes_256.data) {
		return ldb_oom(ldb);
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
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.aes_128 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.aes_128.data) {
		return ldb_oom(ldb);
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
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.des_md5 = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.des_md5.data) {
		return ldb_oom(ldb);
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
				       smb_get_krb5_error_message(io->smb_krb5_context->krb5_context,
								  krb5_ret, io->ac));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	io->g.des_crc = data_blob_talloc(io->ac,
					 key.keyvalue.data,
					 key.keyvalue.length);
	krb5_free_keyblock_contents(io->smb_krb5_context->krb5_context, &key);
	if (!io->g.des_crc.data) {
		return ldb_oom(ldb);
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
		return ldb_oom(ldb);
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
			return ldb_oom(ldb);
		}

		/* TODO: use ndr_pull_struct_blob_all(), when the ndr layer handles it correct with relative pointers */
		ndr_err = ndr_pull_struct_blob(&blob, io->ac, &_old_pkb,
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
		return ldb_oom(ldb);
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
			return ldb_oom(ldb);
		}

		/* TODO: use ndr_pull_struct_blob_all(), when the ndr layer handles it correct with relative pointers */
		ndr_err = ndr_pull_struct_blob(&blob, io->ac,
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
		return ldb_oom(ldb);
	}
	sAMAccountName_u	= data_blob_string_const(strupper_talloc(io->ac, io->u.sAMAccountName));
	if (!sAMAccountName_u.data) {
		return ldb_oom(ldb);
	}

	/* if the user doesn't have a userPrincipalName, create one (with lower case realm) */
	if (!user_principal_name) {
		user_principal_name = talloc_asprintf(io->ac, "%s@%s",
						      io->u.sAMAccountName,
						      io->ac->status->domain_data.dns_domain);
		if (!user_principal_name) {
			return ldb_oom(ldb);
		}	
	}
	userPrincipalName	= data_blob_string_const(user_principal_name);
	userPrincipalName_l	= data_blob_string_const(strlower_talloc(io->ac, user_principal_name));
	if (!userPrincipalName_l.data) {
		return ldb_oom(ldb);
	}
	userPrincipalName_u	= data_blob_string_const(strupper_talloc(io->ac, user_principal_name));
	if (!userPrincipalName_u.data) {
		return ldb_oom(ldb);
	}

	netbios_domain		= data_blob_string_const(io->ac->status->domain_data.netbios_domain);
	netbios_domain_l	= data_blob_string_const(strlower_talloc(io->ac,
									 io->ac->status->domain_data.netbios_domain));
	if (!netbios_domain_l.data) {
		return ldb_oom(ldb);
	}
	netbios_domain_u	= data_blob_string_const(strupper_talloc(io->ac,
									 io->ac->status->domain_data.netbios_domain));
	if (!netbios_domain_u.data) {
		return ldb_oom(ldb);
	}

	dns_domain		= data_blob_string_const(io->ac->status->domain_data.dns_domain);
	dns_domain_l		= data_blob_string_const(io->ac->status->domain_data.dns_domain);
	dns_domain_u		= data_blob_string_const(io->ac->status->domain_data.realm);

	digest			= data_blob_string_const("Digest");

	delim			= data_blob_string_const(":");
	backslash		= data_blob_string_const("\\");

	pdb->num_hashes	= ARRAY_SIZE(wdigest);
	pdb->hashes	= talloc_array(io->ac, struct package_PrimaryWDigestHash,
				       pdb->num_hashes);
	if (!pdb->hashes) {
		return ldb_oom(ldb);
	}

	for (i=0; i < ARRAY_SIZE(wdigest); i++) {
		MD5_CTX md5;
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
	do_newer_keys = (dsdb_functional_level(ldb) >= DS_DOMAIN_FUNCTION_2008);

	if (io->ac->status->domain_data.store_cleartext &&
	    (io->u.userAccountControl & UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED)) {
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
		pknb_hexstr = data_blob_hex_string_upper(io->ac, &pknb_blob);
		if (!pknb_hexstr) {
			return ldb_oom(ldb);
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
	pkb_hexstr = data_blob_hex_string_upper(io->ac, &pkb_blob);
	if (!pkb_hexstr) {
		return ldb_oom(ldb);
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
	pdb_hexstr = data_blob_hex_string_upper(io->ac, &pdb_blob);
	if (!pdb_hexstr) {
		return ldb_oom(ldb);
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
		pcb_hexstr = data_blob_hex_string_upper(io->ac, &pcb_blob);
		if (!pcb_hexstr) {
			return ldb_oom(ldb);
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
	pb_hexstr = data_blob_hex_string_upper(io->ac, &pb_blob);
	if (!pb_hexstr) {
		return ldb_oom(ldb);
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

static int setup_given_passwords(struct setup_password_fields_io *io,
				 struct setup_password_fields_given *g)
{
	struct ldb_context *ldb;
	bool ok;

	ldb = ldb_module_get_ctx(io->ac->module);

	if (g->cleartext_utf8) {
		struct ldb_val *cleartext_utf16_blob;

		cleartext_utf16_blob = talloc(io->ac, struct ldb_val);
		if (!cleartext_utf16_blob) {
			return ldb_oom(ldb);
		}
		if (!convert_string_talloc(io->ac,
					   CH_UTF8, CH_UTF16,
					   g->cleartext_utf8->data,
					   g->cleartext_utf8->length,
					   (void *)&cleartext_utf16_blob->data,
					   &cleartext_utf16_blob->length,
					   false)) {
			if (g->cleartext_utf8->length != 0) {
				talloc_free(cleartext_utf16_blob);
				ldb_asprintf_errstring(ldb,
						       "setup_password_fields: "
						       "failed to generate UTF16 password from cleartext UTF8 one for user '%s'!",
						       io->u.sAMAccountName);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			} else {
				/* passwords with length "0" are valid! */
				cleartext_utf16_blob->data = NULL;
				cleartext_utf16_blob->length = 0;
			}
		}
		g->cleartext_utf16 = cleartext_utf16_blob;
	} else if (g->cleartext_utf16) {
		struct ldb_val *cleartext_utf8_blob;

		cleartext_utf8_blob = talloc(io->ac, struct ldb_val);
		if (!cleartext_utf8_blob) {
			return ldb_oom(ldb);
		}
		if (!convert_string_talloc(io->ac,
					   CH_UTF16MUNGED, CH_UTF8,
					   g->cleartext_utf16->data,
					   g->cleartext_utf16->length,
					   (void *)&cleartext_utf8_blob->data,
					   &cleartext_utf8_blob->length,
					   false)) {
			if (g->cleartext_utf16->length != 0) {
				/* We must bail out here, the input wasn't even
				 * a multiple of 2 bytes */
				talloc_free(cleartext_utf8_blob);
				ldb_asprintf_errstring(ldb,
						       "setup_password_fields: "
						       "failed to generate UTF8 password from cleartext UTF 16 one for user '%s' - the latter had odd length (length must be a multiple of 2)!",
						       io->u.sAMAccountName);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			} else {
				/* passwords with length "0" are valid! */
				cleartext_utf8_blob->data = NULL;
				cleartext_utf8_blob->length = 0;
			}
		}
		g->cleartext_utf8 = cleartext_utf8_blob;
	}

	if (g->cleartext_utf16) {
		struct samr_Password *nt_hash;

		nt_hash = talloc(io->ac, struct samr_Password);
		if (!nt_hash) {
			return ldb_oom(ldb);
		}
		g->nt_hash = nt_hash;

		/* compute the new nt hash */
		mdfour(nt_hash->hash,
		       g->cleartext_utf16->data,
		       g->cleartext_utf16->length);
	}

	if (g->cleartext_utf8) {
		struct samr_Password *lm_hash;

		lm_hash = talloc(io->ac, struct samr_Password);
		if (!lm_hash) {
			return ldb_oom(ldb);
		}

		/* compute the new lm hash */
		ok = E_deshash((char *)g->cleartext_utf8->data, lm_hash->hash);
		if (ok) {
			g->lm_hash = lm_hash;
		} else {
			talloc_free(lm_hash);
		}
	}

	return LDB_SUCCESS;
}

static int setup_password_fields(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb = ldb_module_get_ctx(io->ac->module);
	struct loadparm_context *lp_ctx =
		lp_ctx = talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
					 struct loadparm_context);
	int ret;

	/* transform the old password (for password changes) */
	ret = setup_given_passwords(io, &io->og);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* transform the new password */
	ret = setup_given_passwords(io, &io->n);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (io->n.cleartext_utf8) {
		ret = setup_kerberos_keys(io);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	ret = setup_nt_fields(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (lpcfg_lanman_auth(lp_ctx)) {
		ret = setup_lm_fields(io);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	} else {
		io->g.lm_hash = NULL;
		io->g.lm_history_len = 0;
	}

	ret = setup_supplemental_field(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = setup_last_set_field(io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

static int check_password_restrictions(struct setup_password_fields_io *io)
{
	struct ldb_context *ldb;
	int ret;
	enum samr_ValidationStatus stat;

	ldb = ldb_module_get_ctx(io->ac->module);

	/* First check the old password is correct, for password changes */
	if (!io->ac->pwd_reset) {
		bool nt_hash_checked = false;

		/* we need the old nt or lm hash given by the client */
		if (!io->og.nt_hash && !io->og.lm_hash) {
			ldb_asprintf_errstring(ldb,
				"check_password_restrictions: "
				"You need to provide the old password in order "
				"to change it!");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		/* The password modify through the NT hash is encouraged and
		   has no problems at all */
		if (io->og.nt_hash) {
			if (!io->o.nt_hash) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"There's no old nt_hash, which is needed "
					"in order to change your password!",
					W_ERROR_V(WERR_INVALID_PASSWORD),
					ldb_strerror(ret));
				return ret;
			}

			if (memcmp(io->og.nt_hash->hash, io->o.nt_hash->hash, 16) != 0) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"The old password specified doesn't match!",
					W_ERROR_V(WERR_INVALID_PASSWORD),
					ldb_strerror(ret));
				return ret;
			}

			nt_hash_checked = true;
		}

		/* But it is also possible to change a password by the LM hash
		 * alone for compatibility reasons. This check is optional if
		 * the NT hash was already checked - otherwise it's mandatory.
		 * (as the SAMR operations request it). */
		if (io->og.lm_hash) {
			if (!io->o.lm_hash && !nt_hash_checked) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"There's no old lm_hash, which is needed "
					"in order to change your password!",
					W_ERROR_V(WERR_INVALID_PASSWORD),
					ldb_strerror(ret));
				return ret;
			}

			if (io->o.lm_hash &&
			    memcmp(io->og.lm_hash->hash, io->o.lm_hash->hash, 16) != 0) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"The old password specified doesn't match!",
					W_ERROR_V(WERR_INVALID_PASSWORD),
					ldb_strerror(ret));
				return ret;
			}
		}
	}

	if (io->u.restrictions == 0) {
		/* FIXME: Is this right? */
		return LDB_SUCCESS;
	}

	/*
	 * Fundamental password checks done by the call
	 * "samdb_check_password".
	 * It is also in use by "dcesrv_samr_ValidatePassword".
	 */
	if (io->n.cleartext_utf8 != NULL) {
		stat = samdb_check_password(io->n.cleartext_utf8,
					    io->ac->status->domain_data.pwdProperties,
					    io->ac->status->domain_data.minPwdLength);
		switch (stat) {
		case SAMR_VALIDATION_STATUS_SUCCESS:
				/* perfect -> proceed! */
			break;

		case SAMR_VALIDATION_STATUS_PWD_TOO_SHORT:
			ret = LDB_ERR_CONSTRAINT_VIOLATION;
			ldb_asprintf_errstring(ldb,
				"%08X: %s - check_password_restrictions: "
				"the password is too short. It should be equal or longer than %u characters!",
				W_ERROR_V(WERR_PASSWORD_RESTRICTION),
				ldb_strerror(ret),
				io->ac->status->domain_data.minPwdLength);
			io->ac->status->reject_reason = SAM_PWD_CHANGE_PASSWORD_TOO_SHORT;
			return ret;

		case SAMR_VALIDATION_STATUS_NOT_COMPLEX_ENOUGH:
			ret = LDB_ERR_CONSTRAINT_VIOLATION;
			ldb_asprintf_errstring(ldb,
				"%08X: %s - check_password_restrictions: "
				"the password does not meet the complexity criteria!",
				W_ERROR_V(WERR_PASSWORD_RESTRICTION),
				ldb_strerror(ret));
			io->ac->status->reject_reason = SAM_PWD_CHANGE_NOT_COMPLEX;
			return ret;

		default:
			ret = LDB_ERR_CONSTRAINT_VIOLATION;
			ldb_asprintf_errstring(ldb,
				"%08X: %s - check_password_restrictions: "
				"the password doesn't fit by a certain reason!",
				W_ERROR_V(WERR_PASSWORD_RESTRICTION),
				ldb_strerror(ret));
			return ret;
		}
	}

	if (io->ac->pwd_reset) {
		return LDB_SUCCESS;
	}

	if (io->n.nt_hash) {
		uint32_t i;

		/* checks the NT hash password history */
		for (i = 0; i < io->o.nt_history_len; i++) {
			ret = memcmp(io->n.nt_hash, io->o.nt_history[i].hash, 16);
			if (ret == 0) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"the password was already used (in history)!",
					W_ERROR_V(WERR_PASSWORD_RESTRICTION),
					ldb_strerror(ret));
				io->ac->status->reject_reason = SAM_PWD_CHANGE_PWD_IN_HISTORY;
				return ret;
			}
		}
	}

	if (io->n.lm_hash) {
		uint32_t i;

		/* checks the LM hash password history */
		for (i = 0; i < io->o.lm_history_len; i++) {
			ret = memcmp(io->n.nt_hash, io->o.lm_history[i].hash, 16);
			if (ret == 0) {
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				ldb_asprintf_errstring(ldb,
					"%08X: %s - check_password_restrictions: "
					"the password was already used (in history)!",
					W_ERROR_V(WERR_PASSWORD_RESTRICTION),
					ldb_strerror(ret));
				io->ac->status->reject_reason = SAM_PWD_CHANGE_PWD_IN_HISTORY;
				return ret;
			}
		}
	}

	/* are all password changes disallowed? */
	if (io->ac->status->domain_data.pwdProperties & DOMAIN_REFUSE_PASSWORD_CHANGE) {
		ret = LDB_ERR_CONSTRAINT_VIOLATION;
		ldb_asprintf_errstring(ldb,
			"%08X: %s - check_password_restrictions: "
			"password changes disabled!",
			W_ERROR_V(WERR_PASSWORD_RESTRICTION),
			ldb_strerror(ret));
		return ret;
	}

	/* can this user change the password? */
	if (io->u.userAccountControl & UF_PASSWD_CANT_CHANGE) {
		ret = LDB_ERR_CONSTRAINT_VIOLATION;
		ldb_asprintf_errstring(ldb,
			"%08X: %s - check_password_restrictions: "
			"password can't be changed on this account!",
			W_ERROR_V(WERR_PASSWORD_RESTRICTION),
			ldb_strerror(ret));
		return ret;
	}

	/* Password minimum age: yes, this is a minus. The ages are in negative 100nsec units! */
	if (io->u.pwdLastSet - io->ac->status->domain_data.minPwdAge > io->g.last_set) {
		ret = LDB_ERR_CONSTRAINT_VIOLATION;
		ldb_asprintf_errstring(ldb,
			"%08X: %s - check_password_restrictions: "
			"password is too young to change!",
			W_ERROR_V(WERR_PASSWORD_RESTRICTION),
			ldb_strerror(ret));
		return ret;
	}

	return LDB_SUCCESS;
}

/*
 * This is intended for use by the "password_hash" module since there
 * password changes can be specified through one message element with the
 * new password (to set) and another one with the old password (to unset).
 *
 * The first which sets a password (new value) can have flags
 * (LDB_FLAG_MOD_ADD, LDB_FLAG_MOD_REPLACE) but also none (on "add" operations
 * for entries). The latter (old value) has always specified
 * LDB_FLAG_MOD_DELETE.
 *
 * Returns LDB_ERR_CONSTRAINT_VIOLATION and LDB_ERR_UNWILLING_TO_PERFORM if
 * matching message elements are malformed in respect to the set/change rules.
 * Otherwise it returns LDB_SUCCESS.
 */
static int msg_find_old_and_new_pwd_val(const struct ldb_message *msg,
					const char *name,
					enum ldb_request_type operation,
					const struct ldb_val **new_val,
					const struct ldb_val **old_val)
{
	unsigned int i;

	*new_val = NULL;
	*old_val = NULL;

	if (msg == NULL) {
		return LDB_SUCCESS;
	}

	for (i = 0; i < msg->num_elements; i++) {
		if (ldb_attr_cmp(msg->elements[i].name, name) != 0) {
			continue;
		}

		if ((operation == LDB_MODIFY) &&
		    (LDB_FLAG_MOD_TYPE(msg->elements[i].flags) == LDB_FLAG_MOD_DELETE)) {
			/* 0 values are allowed */
			if (msg->elements[i].num_values == 1) {
				*old_val = &msg->elements[i].values[0];
			} else if (msg->elements[i].num_values > 1) {
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
		} else if ((operation == LDB_MODIFY) &&
			   (LDB_FLAG_MOD_TYPE(msg->elements[i].flags) == LDB_FLAG_MOD_REPLACE)) {
			if (msg->elements[i].num_values > 0) {
				*new_val = &msg->elements[i].values[msg->elements[i].num_values - 1];
			} else {
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}
		} else {
			/* Add operations and LDB_FLAG_MOD_ADD */
			if (msg->elements[i].num_values > 0) {
				*new_val = &msg->elements[i].values[msg->elements[i].num_values - 1];
			} else {
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
		}
	}

	return LDB_SUCCESS;
}

static int setup_io(struct ph_context *ac, 
		    const struct ldb_message *orig_msg,
		    const struct ldb_message *searched_msg, 
		    struct setup_password_fields_io *io) 
{ 
	const struct ldb_val *quoted_utf16, *old_quoted_utf16, *lm_hash, *old_lm_hash;
	struct ldb_context *ldb = ldb_module_get_ctx(ac->module);
	struct loadparm_context *lp_ctx =
		lp_ctx = talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
					 struct loadparm_context);
	int ret;

	ZERO_STRUCTP(io);

	/* Some operations below require kerberos contexts */

	if (smb_krb5_init_context(ac,
				  ldb_get_event_context(ldb),
				  (struct loadparm_context *)ldb_get_opaque(ldb, "loadparm"),
				  &io->smb_krb5_context) != 0) {
		return ldb_operr(ldb);
	}

	io->ac				= ac;

	io->u.userAccountControl	= ldb_msg_find_attr_as_uint(searched_msg,
								    "userAccountControl", 0);
	io->u.pwdLastSet		= samdb_result_nttime(searched_msg, "pwdLastSet", 0);
	io->u.sAMAccountName		= ldb_msg_find_attr_as_string(searched_msg,
								      "sAMAccountName", NULL);
	io->u.user_principal_name	= ldb_msg_find_attr_as_string(searched_msg,
								      "userPrincipalName", NULL);
	io->u.is_computer		= ldb_msg_check_string_attribute(searched_msg, "objectClass", "computer");

	if (io->u.sAMAccountName == NULL) {
		ldb_asprintf_errstring(ldb,
				       "setup_io: sAMAccountName attribute is missing on %s for attempted password set/change",
				       ldb_dn_get_linearized(searched_msg->dn));

		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	/* Only non-trust accounts have restrictions (possibly this test is the
	 * wrong way around, but we like to be restrictive if possible */
	io->u.restrictions = !(io->u.userAccountControl
		& (UF_INTERDOMAIN_TRUST_ACCOUNT | UF_WORKSTATION_TRUST_ACCOUNT
			| UF_SERVER_TRUST_ACCOUNT));

	if ((io->u.userAccountControl & UF_PASSWD_NOTREQD) != 0) {
		/* see [MS-ADTS] 2.2.15 */
		io->u.restrictions = 0;
	}

	if (ac->userPassword) {
		ret = msg_find_old_and_new_pwd_val(orig_msg, "userPassword",
						   ac->req->operation,
						   &io->n.cleartext_utf8,
						   &io->og.cleartext_utf8);
		if (ret != LDB_SUCCESS) {
			ldb_asprintf_errstring(ldb,
				"setup_io: "
				"it's only allowed to set the old password once!");
			return ret;
		}
	}

	ret = msg_find_old_and_new_pwd_val(orig_msg, "clearTextPassword",
					   ac->req->operation,
					   &io->n.cleartext_utf16,
					   &io->og.cleartext_utf16);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to set the old password once!");
		return ret;
	}

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

	ret = msg_find_old_and_new_pwd_val(orig_msg, "unicodePwd",
					   ac->req->operation,
					   &quoted_utf16,
					   &old_quoted_utf16);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to set the old password once!");
		return ret;
	}

	/* Checks and converts the actual "unicodePwd" attribute */
	if (quoted_utf16 &&
	    quoted_utf16->length >= 4 &&
	    quoted_utf16->data[0] == '"' &&
	    quoted_utf16->data[1] == 0 &&
	    quoted_utf16->data[quoted_utf16->length-2] == '"' &&
	    quoted_utf16->data[quoted_utf16->length-1] == 0) {
		struct ldb_val *quoted_utf16_2;

		if (io->n.cleartext_utf16) {
			/* refuse the change if someone wants to change with
			   with both UTF16 possibilities at the same time... */
			ldb_asprintf_errstring(ldb,
				"setup_io: "
				"it's only allowed to set the cleartext password as 'unicodePwd' or as 'clearTextPassword'");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		/*
		 * adapt the quoted UTF16 string to be a real
		 * cleartext one
		 */
		quoted_utf16_2 = talloc(io->ac, struct ldb_val);
		if (quoted_utf16_2 == NULL) {
			return ldb_oom(ldb);
		}

		quoted_utf16_2->data = quoted_utf16->data + 2;
		quoted_utf16_2->length = quoted_utf16->length-4;
		io->n.cleartext_utf16 = quoted_utf16_2;
		io->n.nt_hash = NULL;

	} else if (quoted_utf16) {
		/* We have only the hash available -> so no plaintext here */
		if (!ac->hash_values) {
			/* refuse the change if someone wants to change
			   the hash without control specified... */
			ldb_asprintf_errstring(ldb,
				"setup_io: "
				"it's not allowed to set the NT hash password directly'");
			/* this looks odd but this is what Windows does:
			   returns "UNWILLING_TO_PERFORM" on wrong
			   password sets and "CONSTRAINT_VIOLATION" on
			   wrong password changes. */
			if (old_quoted_utf16 == NULL) {
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}

			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		io->n.nt_hash = talloc(io->ac, struct samr_Password);
		memcpy(io->n.nt_hash->hash, quoted_utf16->data,
		       MIN(quoted_utf16->length, sizeof(io->n.nt_hash->hash)));
	}

	/* Checks and converts the previous "unicodePwd" attribute */
	if (old_quoted_utf16 &&
	    old_quoted_utf16->length >= 4 &&
	    old_quoted_utf16->data[0] == '"' &&
	    old_quoted_utf16->data[1] == 0 &&
	    old_quoted_utf16->data[old_quoted_utf16->length-2] == '"' &&
	    old_quoted_utf16->data[old_quoted_utf16->length-1] == 0) {
		struct ldb_val *old_quoted_utf16_2;

		if (io->og.cleartext_utf16) {
			/* refuse the change if someone wants to change with
			   both UTF16 possibilities at the same time... */
			ldb_asprintf_errstring(ldb,
				"setup_io: "
				"it's only allowed to set the cleartext password as 'unicodePwd' or as 'clearTextPassword'");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		/*
		 * adapt the quoted UTF16 string to be a real
		 * cleartext one
		 */
		old_quoted_utf16_2 = talloc(io->ac, struct ldb_val);
		if (old_quoted_utf16_2 == NULL) {
			return ldb_oom(ldb);
		}

		old_quoted_utf16_2->data = old_quoted_utf16->data + 2;
		old_quoted_utf16_2->length = old_quoted_utf16->length-4;

		io->og.cleartext_utf16 = old_quoted_utf16_2;
		io->og.nt_hash = NULL;
	} else if (old_quoted_utf16) {
		/* We have only the hash available -> so no plaintext here */
		if (!ac->hash_values) {
			/* refuse the change if someone wants to change
			   the hash without control specified... */
			ldb_asprintf_errstring(ldb,
				"setup_io: "
				"it's not allowed to set the NT hash password directly'");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		io->og.nt_hash = talloc(io->ac, struct samr_Password);
		memcpy(io->og.nt_hash->hash, old_quoted_utf16->data,
		       MIN(old_quoted_utf16->length, sizeof(io->og.nt_hash->hash)));
	}

	/* Handles the "dBCSPwd" attribute (LM hash) */
	io->n.lm_hash = NULL; io->og.lm_hash = NULL;
	ret = msg_find_old_and_new_pwd_val(orig_msg, "dBCSPwd",
					   ac->req->operation,
					   &lm_hash, &old_lm_hash);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to set the old password once!");
		return ret;
	}

	if (((lm_hash != NULL) || (old_lm_hash != NULL)) && (!ac->hash_values)) {
		/* refuse the change if someone wants to change the hash
		   without control specified... */
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's not allowed to set the LM hash password directly'");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	if (lpcfg_lanman_auth(lp_ctx) && (lm_hash != NULL)) {
		io->n.lm_hash = talloc(io->ac, struct samr_Password);
		memcpy(io->n.lm_hash->hash, lm_hash->data, MIN(lm_hash->length,
		       sizeof(io->n.lm_hash->hash)));
	}
	if (lpcfg_lanman_auth(lp_ctx) && (old_lm_hash != NULL)) {
		io->og.lm_hash = talloc(io->ac, struct samr_Password);
		memcpy(io->og.lm_hash->hash, old_lm_hash->data, MIN(old_lm_hash->length,
		       sizeof(io->og.lm_hash->hash)));
	}

	/*
	 * Handles the password change control if it's specified. It has the
	 * precedance and overrides already specified old password values of
	 * change requests (but that shouldn't happen since the control is
	 * fully internal and only used in conjunction with replace requests!).
	 */
	if (ac->change != NULL) {
		io->og.nt_hash = NULL;
		if (ac->change->old_nt_pwd_hash != NULL) {
			io->og.nt_hash = talloc_memdup(io->ac,
						       ac->change->old_nt_pwd_hash,
						       sizeof(struct samr_Password));
		}
		io->og.lm_hash = NULL;
		if (lpcfg_lanman_auth(lp_ctx) && (ac->change->old_lm_pwd_hash != NULL)) {
			io->og.lm_hash = talloc_memdup(io->ac,
						       ac->change->old_lm_pwd_hash,
						       sizeof(struct samr_Password));
		}
	}

	/* refuse the change if someone wants to change the clear-
	   text and supply his own hashes at the same time... */
	if ((io->n.cleartext_utf8 || io->n.cleartext_utf16)
			&& (io->n.nt_hash || io->n.lm_hash)) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to set the password in form of cleartext attributes or as hashes");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* refuse the change if someone wants to change the password
	   using both plaintext methods (UTF8 and UTF16) at the same time... */
	if (io->n.cleartext_utf8 && io->n.cleartext_utf16) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to set the cleartext password as 'unicodePwd' or as 'userPassword' or as 'clearTextPassword'");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* refuse the change if someone tries to set/change the password by
	 * the lanman hash alone and we've deactivated that mechanism. This
	 * would end in an account without any password! */
	if ((!io->n.cleartext_utf8) && (!io->n.cleartext_utf16)
	    && (!io->n.nt_hash) && (!io->n.lm_hash)) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"It' not possible to delete the password (changes using the LAN Manager hash alone could be deactivated)!");
		/* on "userPassword" and "clearTextPassword" we've to return
		 * something different, since these are virtual attributes */
		if ((ldb_msg_find_element(orig_msg, "userPassword") != NULL) ||
		    (ldb_msg_find_element(orig_msg, "clearTextPassword") != NULL)) {
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* refuse the change if someone wants to compare against a plaintext
	   or hash at the same time for a "password modify" operation... */
	if ((io->og.cleartext_utf8 || io->og.cleartext_utf16)
	    && (io->og.nt_hash || io->og.lm_hash)) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to provide the old password in form of cleartext attributes or as hashes");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* refuse the change if someone wants to compare against both
	 * plaintexts at the same time for a "password modify" operation... */
	if (io->og.cleartext_utf8 && io->og.cleartext_utf16) {
		ldb_asprintf_errstring(ldb,
			"setup_io: "
			"it's only allowed to provide the old cleartext password as 'unicodePwd' or as 'userPassword' or as 'clearTextPassword'");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* Decides if we have a password modify or password reset operation */
	if (ac->req->operation == LDB_ADD) {
		/* On "add" we have only "password reset" */
		ac->pwd_reset = true;
	} else if (ac->req->operation == LDB_MODIFY) {
		if (io->og.cleartext_utf8 || io->og.cleartext_utf16
		    || io->og.nt_hash || io->og.lm_hash) {
			/* If we have an old password specified then for sure it
			 * is a user "password change" */
			ac->pwd_reset = false;
		} else {
			/* Otherwise we have also here a "password reset" */
			ac->pwd_reset = true;
		}
	} else {
		/* this shouldn't happen */
		return ldb_operr(ldb);
	}

	return LDB_SUCCESS;
}

static struct ph_context *ph_init_context(struct ldb_module *module,
					  struct ldb_request *req,
					  bool userPassword)
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
	ac->userPassword = userPassword;

	return ac;
}

static void ph_apply_controls(struct ph_context *ac)
{
	struct ldb_control *ctrl;

	ac->change_status = false;
	ctrl = ldb_request_get_control(ac->req,
				       DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID);
	if (ctrl != NULL) {
		ac->change_status = true;

		/* Mark the "change status" control as uncritical (done) */
		ctrl->critical = false;
	}

	ac->hash_values = false;
	ctrl = ldb_request_get_control(ac->req,
				       DSDB_CONTROL_PASSWORD_HASH_VALUES_OID);
	if (ctrl != NULL) {
		ac->hash_values = true;

		/* Mark the "hash values" control as uncritical (done) */
		ctrl->critical = false;
	}

	ctrl = ldb_request_get_control(ac->req,
				       DSDB_CONTROL_PASSWORD_CHANGE_OID);
	if (ctrl != NULL) {
		ac->change = (struct dsdb_control_password_change *) ctrl->data;

		/* Mark the "change" control as uncritical (done) */
		ctrl->critical = false;
	}
}

static int ph_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ph_context *ac;

	ac = talloc_get_type(req->context, struct ph_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->type == LDB_REPLY_REFERRAL) {
		return ldb_module_send_referral(ac->req, ares->referral);
	}

	if ((ares->error != LDB_ERR_OPERATIONS_ERROR) && (ac->change_status)) {
		/* On success and trivial errors a status control is being
		 * added (used for example by the "samdb_set_password" call) */
		ldb_reply_add_control(ares,
				      DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID,
				      false,
				      ac->status);
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
	struct ph_context *ac;
	struct loadparm_context *lp_ctx;
	int ret;

	ac = talloc_get_type(req->context, struct ph_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		if (ac->status != NULL) {
			talloc_free(ares);

			ldb_set_errstring(ldb, "Too many results");
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto done;
		}

		/* Setup the "status" structure (used as control later) */
		ac->status = talloc_zero(ac->req,
					 struct dsdb_control_password_change_status);
		if (ac->status == NULL) {
			talloc_free(ares);

			ldb_oom(ldb);
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto done;
		}

		/* Setup the "domain data" structure */
		ac->status->domain_data.pwdProperties =
			ldb_msg_find_attr_as_uint(ares->message, "pwdProperties", -1);
		ac->status->domain_data.pwdHistoryLength =
			ldb_msg_find_attr_as_uint(ares->message, "pwdHistoryLength", -1);
		ac->status->domain_data.maxPwdAge =
			ldb_msg_find_attr_as_int64(ares->message, "maxPwdAge", -1);
		ac->status->domain_data.minPwdAge =
			ldb_msg_find_attr_as_int64(ares->message, "minPwdAge", -1);
		ac->status->domain_data.minPwdLength =
			ldb_msg_find_attr_as_uint(ares->message, "minPwdLength", -1);
		ac->status->domain_data.store_cleartext =
			ac->status->domain_data.pwdProperties & DOMAIN_PASSWORD_STORE_CLEARTEXT;

		talloc_free(ares);

		/* For a domain DN, this puts things in dotted notation */
		/* For builtin domains, this will give details for the host,
		 * but that doesn't really matter, as it's just used for salt
		 * and kerberos principals, which don't exist here */

		lp_ctx = talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
					 struct loadparm_context);

		ac->status->domain_data.dns_domain = lpcfg_dnsdomain(lp_ctx);
		ac->status->domain_data.realm = lpcfg_realm(lp_ctx);
		ac->status->domain_data.netbios_domain = lpcfg_sam_name(lp_ctx);

		ac->status->reject_reason = SAM_PWD_CHANGE_NO_ERROR;

		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		talloc_free(ares);
		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_DONE:
		talloc_free(ares);
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
		break;
	}

done:
	if (ret != LDB_SUCCESS) {
		struct ldb_reply *new_ares;

		new_ares = talloc_zero(ac->req, struct ldb_reply);
		if (new_ares == NULL) {
			ldb_oom(ldb);
			return ldb_module_done(ac->req, NULL, NULL,
					       LDB_ERR_OPERATIONS_ERROR);
		}

		new_ares->error = ret;
		if ((ret != LDB_ERR_OPERATIONS_ERROR) && (ac->change_status)) {
			/* On success and trivial errors a status control is being
			 * added (used for example by the "samdb_set_password" call) */
			ldb_reply_add_control(new_ares,
					      DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID,
					      false,
					      ac->status);
		}

		return ldb_module_done(ac->req, new_ares->controls,
				       new_ares->response, new_ares->error);
	}

	return LDB_SUCCESS;
}

static int build_domain_data_request(struct ph_context *ac)
{
	/* attrs[] is returned from this function in
	   ac->dom_req->op.search.attrs, so it must be static, as
	   otherwise the compiler can put it on the stack */
	struct ldb_context *ldb;
	static const char * const attrs[] = { "pwdProperties",
					      "pwdHistoryLength",
					      "maxPwdAge",
					      "minPwdAge",
					      "minPwdLength",
					      NULL };
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	ret = ldb_build_search_req(&ac->dom_req, ldb, ac,
				   ldb_get_default_basedn(ldb),
				   LDB_SCOPE_BASE,
				   NULL, attrs,
				   NULL,
				   ac, get_domain_data_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(ac->dom_req);
	return ret;
}

static int password_hash_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	struct ldb_message_element *userPasswordAttr, *clearTextPasswordAttr,
		*ntAttr, *lmAttr;
	int ret;
	struct ldb_control *bypass = NULL;
	bool userPassword = dsdb_user_password_support(module, req, req);

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

	bypass = ldb_request_get_control(req,
					 DSDB_CONTROL_BYPASS_PASSWORD_HASH_OID);
	if (bypass != NULL) {
		/* Mark the "bypass" control as uncritical (done) */
		bypass->critical = false;
		ldb_debug(ldb, LDB_DEBUG_TRACE, "password_hash_add (bypassing)\n");
		return password_hash_bypass(module, req);
	}

	/* nobody must touch password histories and 'supplementalCredentials' */
	if (ldb_msg_find_element(req->op.add.message, "ntPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "lmPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.add.message, "supplementalCredentials")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* If no part of this touches the 'userPassword' OR 'clearTextPassword'
	 * OR 'unicodePwd' OR 'dBCSPwd' we don't need to make any changes. */

	userPasswordAttr = NULL;
	if (userPassword) {
		userPasswordAttr = ldb_msg_find_element(req->op.add.message,
							"userPassword");
		/* MS-ADTS 3.1.1.3.1.5.2 */
		if ((userPasswordAttr != NULL) &&
		    (dsdb_functional_level(ldb) < DS_DOMAIN_FUNCTION_2003)) {
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
	}
	clearTextPasswordAttr = ldb_msg_find_element(req->op.add.message, "clearTextPassword");
	ntAttr = ldb_msg_find_element(req->op.add.message, "unicodePwd");
	lmAttr = ldb_msg_find_element(req->op.add.message, "dBCSPwd");

	if ((!userPasswordAttr) && (!clearTextPasswordAttr) && (!ntAttr) && (!lmAttr)) {
		return ldb_next_request(module, req);
	}

	/* Make sure we are performing the password set action on a (for us)
	 * valid object. Those are instances of either "user" and/or
	 * "inetOrgPerson". Otherwise continue with the submodules. */
	if ((!ldb_msg_check_string_attribute(req->op.add.message, "objectClass", "user"))
		&& (!ldb_msg_check_string_attribute(req->op.add.message, "objectClass", "inetOrgPerson"))) {

		if (ldb_msg_find_element(req->op.add.message, "clearTextPassword") != NULL) {
			ldb_set_errstring(ldb,
					  "'clearTextPassword' is only allowed on objects of class 'user' and/or 'inetOrgPerson'!");
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}

		return ldb_next_request(module, req);
	}

	ac = ph_init_context(module, req, userPassword);
	if (ac == NULL) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return ldb_operr(ldb);
	}
	ph_apply_controls(ac);

	/* get user domain data */
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

	ldb = ldb_module_get_ctx(ac->module);

	msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);
	if (msg == NULL) {
		return ldb_operr(ldb);
	}

	/* remove attributes that we just read into 'io' */
	if (ac->userPassword) {
		ldb_msg_remove_attr(msg, "userPassword");
	}
	ldb_msg_remove_attr(msg, "clearTextPassword");
	ldb_msg_remove_attr(msg, "unicodePwd");
	ldb_msg_remove_attr(msg, "dBCSPwd");
	ldb_msg_remove_attr(msg, "pwdLastSet");

	ret = setup_password_fields(&io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = check_password_restrictions(&io);
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
		ret = samdb_msg_add_hashes(ldb, ac, msg,
					   "ntPwdHistory",
					   io.g.nt_history,
					   io.g.nt_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_history_len > 0) {
		ret = samdb_msg_add_hashes(ldb, ac, msg,
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

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, ph_op_callback,
				ac->req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, down_req);
}

static int password_hash_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	const char *passwordAttrs[] = { "userPassword", "clearTextPassword",
		"unicodePwd", "dBCSPwd", NULL }, **l;
	unsigned int attr_cnt, del_attr_cnt, add_attr_cnt, rep_attr_cnt;
	struct ldb_message_element *passwordAttr;
	struct ldb_message *msg;
	struct ldb_request *down_req;
	int ret;
	struct ldb_control *bypass = NULL;
	bool userPassword = dsdb_user_password_support(module, req, req);

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

	bypass = ldb_request_get_control(req,
					 DSDB_CONTROL_BYPASS_PASSWORD_HASH_OID);
	if (bypass != NULL) {
		/* Mark the "bypass" control as uncritical (done) */
		bypass->critical = false;
		ldb_debug(ldb, LDB_DEBUG_TRACE, "password_hash_modify (bypassing)\n");
		return password_hash_bypass(module, req);
	}

	/* nobody must touch password histories and 'supplementalCredentials' */
	if (ldb_msg_find_element(req->op.mod.message, "ntPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.mod.message, "lmPwdHistory")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ldb_msg_find_element(req->op.mod.message, "supplementalCredentials")) {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* If no part of this touches the 'userPassword' OR 'clearTextPassword'
	 * OR 'unicodePwd' OR 'dBCSPwd' we don't need to make any changes.
	 * For password changes/set there should be a 'delete' or a 'modify'
	 * on these attributes. */
	attr_cnt = 0;
	for (l = passwordAttrs; *l != NULL; l++) {
		if ((!userPassword) && (ldb_attr_cmp(*l, "userPassword") == 0)) {
			continue;
		}

		if (ldb_msg_find_element(req->op.mod.message, *l) != NULL) {
			/* MS-ADTS 3.1.1.3.1.5.2 */
			if ((ldb_attr_cmp(*l, "userPassword") == 0) &&
			    (dsdb_functional_level(ldb) < DS_DOMAIN_FUNCTION_2003)) {
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}

			++attr_cnt;
		}
	}
	if (attr_cnt == 0) {
		return ldb_next_request(module, req);
	}

	ac = ph_init_context(module, req, userPassword);
	if (!ac) {
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return ldb_operr(ldb);
	}
	ph_apply_controls(ac);

	/* use a new message structure so that we can modify it */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		return ldb_oom(ldb);
	}

	/* - check for single-valued password attributes
	 *   (if not return "CONSTRAINT_VIOLATION")
	 * - check that for a password change operation one add and one delete
	 *   operation exists
	 *   (if not return "CONSTRAINT_VIOLATION" or "UNWILLING_TO_PERFORM")
	 * - check that a password change and a password set operation cannot
	 *   be mixed
	 *   (if not return "UNWILLING_TO_PERFORM")
	 * - remove all password attributes modifications from the first change
	 *   operation (anything without the passwords) - we will make the real
	 *   modification later */
	del_attr_cnt = 0;
	add_attr_cnt = 0;
	rep_attr_cnt = 0;
	for (l = passwordAttrs; *l != NULL; l++) {
		if ((!ac->userPassword) &&
		    (ldb_attr_cmp(*l, "userPassword") == 0)) {
			continue;
		}

		while ((passwordAttr = ldb_msg_find_element(msg, *l)) != NULL) {
			if (LDB_FLAG_MOD_TYPE(passwordAttr->flags) == LDB_FLAG_MOD_DELETE) {
				++del_attr_cnt;
			}
			if (LDB_FLAG_MOD_TYPE(passwordAttr->flags) == LDB_FLAG_MOD_ADD) {
				++add_attr_cnt;
			}
			if (LDB_FLAG_MOD_TYPE(passwordAttr->flags) == LDB_FLAG_MOD_REPLACE) {
				++rep_attr_cnt;
			}
			if ((passwordAttr->num_values != 1) &&
			    (LDB_FLAG_MOD_TYPE(passwordAttr->flags) == LDB_FLAG_MOD_ADD)) {
				talloc_free(ac);
				ldb_asprintf_errstring(ldb,
						       "'%s' attribute must have exactly one value on add operations!",
						       *l);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
			if ((passwordAttr->num_values > 1) &&
			    (LDB_FLAG_MOD_TYPE(passwordAttr->flags) == LDB_FLAG_MOD_DELETE)) {
				talloc_free(ac);
				ldb_asprintf_errstring(ldb,
						       "'%s' attribute must have zero or one value(s) on delete operations!",
						       *l);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
			ldb_msg_remove_element(msg, passwordAttr);
		}
	}
	if ((del_attr_cnt == 0) && (add_attr_cnt > 0)) {
		talloc_free(ac);
		ldb_set_errstring(ldb,
				  "Only the add action for a password change specified!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if ((del_attr_cnt > 1) || (add_attr_cnt > 1)) {
		talloc_free(ac);
		ldb_set_errstring(ldb,
				  "Only one delete and one add action for a password change allowed!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if ((rep_attr_cnt > 0) && ((del_attr_cnt > 0) || (add_attr_cnt > 0))) {
		talloc_free(ac);
		ldb_set_errstring(ldb,
				  "Either a password change or a password set operation is allowed!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* if there was nothing else to be modified skip to next step */
	if (msg->num_elements == 0) {
		return password_hash_mod_search_self(ac);
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, ph_modify_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

static int ph_modify_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ph_context *ac;

	ac = talloc_get_type(req->context, struct ph_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->type == LDB_REPLY_REFERRAL) {
		return ldb_module_send_referral(ac->req, ares->referral);
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

	talloc_free(ares);

	return password_hash_mod_search_self(ac);
}

static int ph_mod_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct ph_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct ph_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* we are interested only in the single reply (base search) */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		/* Make sure we are performing the password change action on a
		 * (for us) valid object. Those are instances of either "user"
		 * and/or "inetOrgPerson". Otherwise continue with the
		 * submodules. */
		if ((!ldb_msg_check_string_attribute(ares->message, "objectClass", "user"))
			&& (!ldb_msg_check_string_attribute(ares->message, "objectClass", "inetOrgPerson"))) {
			talloc_free(ares);

			if (ldb_msg_find_element(ac->req->op.mod.message, "clearTextPassword") != NULL) {
				ldb_set_errstring(ldb,
						  "'clearTextPassword' is only allowed on objects of class 'user' and/or 'inetOrgPerson'!");
				ret = LDB_ERR_NO_SUCH_ATTRIBUTE;
				goto done;
			}

			ret = ldb_next_request(ac->module, ac->req);
			goto done;
		}

		if (ac->search_res != NULL) {
			talloc_free(ares);

			ldb_set_errstring(ldb, "Too many results");
			ret = LDB_ERR_OPERATIONS_ERROR;
			goto done;
		}

		ac->search_res = talloc_steal(ac, ares);
		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore anything else for now */
		talloc_free(ares);
		ret = LDB_SUCCESS;
		break;

	case LDB_REPLY_DONE:
		talloc_free(ares);

		/* get user domain data */
		ret = build_domain_data_request(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		ret = ldb_next_request(ac->module, ac->dom_req);
		break;
	}

done:
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	return LDB_SUCCESS;
}

static int password_hash_mod_search_self(struct ph_context *ac)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "objectClass",
					      "userAccountControl",
					      "pwdLastSet",
					      "sAMAccountName",
					      "objectSid",
					      "userPrincipalName",
					      "supplementalCredentials",
					      "lmPwdHistory",
					      "ntPwdHistory",
					      "dBCSPwd",
					      "unicodePwd",
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
	LDB_REQ_SET_LOCATION(search_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, search_req);
}

static int password_hash_mod_do_mod(struct ph_context *ac)
{
	struct ldb_context *ldb = ldb_module_get_ctx(ac->module);
	struct loadparm_context *lp_ctx =
				talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
						struct loadparm_context);
	struct ldb_request *mod_req;
	struct ldb_message *msg;
	const struct ldb_message *orig_msg, *searched_msg;
	struct setup_password_fields_io io;
	int ret;
	NTSTATUS status;

	/* use a new message structure so that we can modify it */
	msg = ldb_msg_new(ac);
	if (msg == NULL) {
		return ldb_operr(ldb);
	}

	/* modify dn */
	msg->dn = ac->req->op.mod.message->dn;

	orig_msg = ac->req->op.mod.message;
	searched_msg = ac->search_res->message;

	/* Prepare the internal data structure containing the passwords */
	ret = setup_io(ac, orig_msg, searched_msg, &io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	/* Get the old password from the database */
	status = samdb_result_passwords(io.ac,
					lp_ctx,
					discard_const_p(struct ldb_message, searched_msg),
					&io.o.lm_hash, &io.o.nt_hash);
	if (!NT_STATUS_IS_OK(status)) {
		return ldb_operr(ldb);
	}

	io.o.nt_history_len		= samdb_result_hashes(io.ac, searched_msg, "ntPwdHistory", &io.o.nt_history);
	io.o.lm_history_len		= samdb_result_hashes(io.ac, searched_msg, "lmPwdHistory", &io.o.lm_history);
	io.o.supplemental		= ldb_msg_find_ldb_val(searched_msg, "supplementalCredentials");

	ret = setup_password_fields(&io);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = check_password_restrictions(&io);
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
		ret = samdb_msg_add_hashes(ldb, ac, msg,
					   "ntPwdHistory",
					   io.g.nt_history,
					   io.g.nt_history_len);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	if (io.g.lm_history_len > 0) {
		ret = samdb_msg_add_hashes(ldb, ac, msg,
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

	ret = ldb_build_mod_req(&mod_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, ph_op_callback,
				ac->req);
	LDB_REQ_SET_LOCATION(mod_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, mod_req);
}

static const struct ldb_module_ops ldb_password_hash_module_ops = {
	.name          = "password_hash",
	.add           = password_hash_add,
	.modify        = password_hash_modify
};

int ldb_password_hash_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_password_hash_module_ops);
}
