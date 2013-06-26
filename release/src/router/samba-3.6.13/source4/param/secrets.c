/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Andrew Bartlett      2002
   Copyright (C) Rafal Szczesniak     2002
   
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

/* the Samba secrets database stores any generated, private information
   such as the local SID and machine trust password */

#include "includes.h"
#include "secrets.h"
#include "param/param.h"
#include "system/filesys.h"
#include "lib/util/tdb_wrap.h"
#include "lib/ldb-samba/ldb_wrap.h"
#include <ldb.h>
#include "../lib/util/util_tdb.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "dsdb/samdb/samdb.h"

/**
 * Use a TDB to store an incrementing random seed.
 *
 * Initialised to the current pid, the very first time Samba starts,
 * and incremented by one each time it is needed.  
 * 
 * @note Not called by systems with a working /dev/urandom.
 */
static void get_rand_seed(struct tdb_wrap *secretsdb, int *new_seed) 
{
	*new_seed = getpid();
	if (secretsdb != NULL) {
		tdb_change_int32_atomic(secretsdb->tdb, "INFO/random_seed", new_seed, 1);
	}
}

/**
 * open up the secrets database
 */
struct tdb_wrap *secrets_init(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	char *fname;
	uint8_t dummy;
	struct tdb_wrap *tdb;

	fname = private_path(mem_ctx, lp_ctx, "secrets.tdb");

	tdb = tdb_wrap_open(mem_ctx, fname, 0, TDB_DEFAULT, O_RDWR|O_CREAT, 0600);

	if (!tdb) {
		DEBUG(0,("Failed to open %s\n", fname));
		talloc_free(fname);
		return NULL;
	}
	talloc_free(fname);

	/**
	 * Set a reseed function for the crypto random generator 
	 * 
	 * This avoids a problem where systems without /dev/urandom
	 * could send the same challenge to multiple clients
	 */
	set_rand_reseed_callback((void (*) (void *, int *))get_rand_seed, tdb);

	/* Ensure that the reseed is done now, while we are root, etc */
	generate_random_buffer(&dummy, sizeof(dummy));

	return tdb;
}

/**
  connect to the secrets ldb
*/
struct ldb_context *secrets_db_connect(TALLOC_CTX *mem_ctx,
					struct loadparm_context *lp_ctx)
{
	return ldb_wrap_connect(mem_ctx, NULL, lp_ctx, lpcfg_secrets_url(lp_ctx),
			       NULL, NULL, 0);
}

/**
 * Retrieve the domain SID from the secrets database.
 * @return pointer to a SID object if the SID could be obtained, NULL otherwise
 */
struct dom_sid *secrets_get_domain_sid(TALLOC_CTX *mem_ctx,
				       struct loadparm_context *lp_ctx,
				       const char *domain,
				       enum netr_SchannelType *sec_channel_type,
				       char **errstring)
{
	struct ldb_context *ldb;
	struct ldb_message *msg;
	int ldb_ret;
	const char *attrs[] = { "objectSid", "secureChannelType", NULL };
	struct dom_sid *result = NULL;
	const struct ldb_val *v;
	enum ndr_err_code ndr_err;

	*errstring = NULL;

	ldb = secrets_db_connect(mem_ctx, lp_ctx);
	if (ldb == NULL) {
		DEBUG(5, ("secrets_db_connect failed\n"));
		return NULL;
	}

	ldb_ret = dsdb_search_one(ldb, ldb, &msg,
			      ldb_dn_new(mem_ctx, ldb, SECRETS_PRIMARY_DOMAIN_DN),
			      LDB_SCOPE_ONELEVEL,
			      attrs, 0, SECRETS_PRIMARY_DOMAIN_FILTER, domain);

	if (ldb_ret != LDB_SUCCESS) {
		*errstring = talloc_asprintf(mem_ctx, "Failed to find record for %s in %s: %s: %s", 
					     domain, (char *) ldb_get_opaque(ldb, "ldb_url"),
					     ldb_strerror(ldb_ret), ldb_errstring(ldb));
		return NULL;
	}
	v = ldb_msg_find_ldb_val(msg, "objectSid");
	if (v == NULL) {
		*errstring = talloc_asprintf(mem_ctx, "Failed to find a SID on record for %s in %s", 
					     domain, (char *) ldb_get_opaque(ldb, "ldb_url"));
		return NULL;
	}

	if (sec_channel_type) {
		int t;
		t = ldb_msg_find_attr_as_int(msg, "secureChannelType", -1);
		if (t == -1) {
			*errstring = talloc_asprintf(mem_ctx, "Failed to find secureChannelType for %s in %s",
						     domain, (char *) ldb_get_opaque(ldb, "ldb_url"));
			return NULL;
		}
		*sec_channel_type = t;
	}

	result = talloc(mem_ctx, struct dom_sid);
	if (result == NULL) {
		talloc_free(ldb);
		return NULL;
	}

	ndr_err = ndr_pull_struct_blob(v, result, result,
				       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		*errstring = talloc_asprintf(mem_ctx, "Failed to parse SID on record for %s in %s", 
					     domain, (char *) ldb_get_opaque(ldb, "ldb_url"));
		talloc_free(result);
		talloc_free(ldb);
		return NULL;
	}

	return result;
}

char *keytab_name_from_msg(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, struct ldb_message *msg) 
{
	const char *krb5keytab = ldb_msg_find_attr_as_string(msg, "krb5Keytab", NULL);
	if (krb5keytab) {
		return talloc_strdup(mem_ctx, krb5keytab);
	} else {
		char *file_keytab;
		char *relative_path;
		const char *privateKeytab = ldb_msg_find_attr_as_string(msg, "privateKeytab", NULL);
		if (!privateKeytab) {
			return NULL;
		}

		relative_path = ldb_relative_path(ldb, mem_ctx, privateKeytab);
		if (!relative_path) {
			return NULL;
		}
		file_keytab = talloc_asprintf(mem_ctx, "FILE:%s", relative_path);
		talloc_free(relative_path);
		return file_keytab;
	}
	return NULL;
}

