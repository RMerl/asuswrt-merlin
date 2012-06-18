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
#include "tdb_wrap.h"
#include "lib/ldb/include/ldb.h"
#include "../tdb/include/tdb.h"
#include "../lib/util/util_tdb.h"
#include "../lib/util/util_ldb.h"
#include "librpc/gen_ndr/ndr_security.h"

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
					struct tevent_context *ev_ctx,
					struct loadparm_context *lp_ctx)
{
	char *path;
	const char *url;
	struct ldb_context *ldb;

	url = lp_secrets_url(lp_ctx);
	if (!url || !url[0]) {
		return NULL;
	}

	path = private_path(mem_ctx, lp_ctx, url);
	if (!path) {
		return NULL;
	}

	/* Secrets.ldb *must* always be local.  If we call for a
	 * system_session() we will recurse */
	ldb = ldb_init(mem_ctx, ev_ctx);
	if (!ldb) {
		talloc_free(path);
		return NULL;
	}

	ldb_set_modules_dir(ldb, 
			    talloc_asprintf(ldb, "%s/ldb", lp_modulesdir(lp_ctx)));

	if (ldb_connect(ldb, path, 0, NULL) != 0) {
		talloc_free(path);
		return NULL;
	}

	/* the update_keytab module relies on this being setup */
	if (ldb_set_opaque(ldb, "loadparm", lp_ctx) != LDB_SUCCESS) {
		talloc_free(path);
		talloc_free(ldb);
		return NULL;
	}

	talloc_free(path);
	
	return ldb;
}

/**
 * Retrieve the domain SID from the secrets database.
 * @return pointer to a SID object if the SID could be obtained, NULL otherwise
 */
struct dom_sid *secrets_get_domain_sid(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev_ctx,
				       struct loadparm_context *lp_ctx,
				       const char *domain)
{
	struct ldb_context *ldb;
	struct ldb_message **msgs;
	int ldb_ret;
	const char *attrs[] = { "objectSid", NULL };
	struct dom_sid *result = NULL;
	const struct ldb_val *v;
	enum ndr_err_code ndr_err;

	ldb = secrets_db_connect(mem_ctx, ev_ctx, lp_ctx);
	if (ldb == NULL) {
		DEBUG(5, ("secrets_db_connect failed\n"));
		return NULL;
	}

	ldb_ret = gendb_search(ldb, ldb,
			       ldb_dn_new(mem_ctx, ldb, SECRETS_PRIMARY_DOMAIN_DN), 
			       &msgs, attrs,
			       SECRETS_PRIMARY_DOMAIN_FILTER, domain);

	if (ldb_ret == -1) {
		DEBUG(5, ("Error searching for domain SID for %s: %s", 
			  domain, ldb_errstring(ldb))); 
		talloc_free(ldb);
		return NULL;
	}

	if (ldb_ret == 0) {
		DEBUG(5, ("Did not find domain record for %s\n", domain));
		talloc_free(ldb);
		return NULL;
	}

	if (ldb_ret > 1) {
		DEBUG(5, ("Found more than one (%d) domain records for %s\n",
			  ldb_ret, domain));
		talloc_free(ldb);
		return NULL;
	}

	v = ldb_msg_find_ldb_val(msgs[0], "objectSid");
	if (v == NULL) {
		DEBUG(0, ("Domain object for %s does not contain a SID!\n",
			  domain));
		return NULL;
	}
	result = talloc(mem_ctx, struct dom_sid);
	if (result == NULL) {
		talloc_free(ldb);
		return NULL;
	}

	ndr_err = ndr_pull_struct_blob(v, result, NULL, result,
				       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(result);
		talloc_free(ldb);
		return NULL;
	}

	return result;
}
