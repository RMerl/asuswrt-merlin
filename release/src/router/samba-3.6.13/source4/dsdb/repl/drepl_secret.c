/*
   Unix SMB/CIFS mplementation.

   DSDB replication service - repl secret handling

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010

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
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "smbd/service.h"
#include "dsdb/repl/drepl_service.h"
#include "param/param.h"

struct repl_secret_state {
	const char *user_dn;
};

/*
  called when a repl secret has completed
 */
static void drepl_repl_secret_callback(struct dreplsrv_service *service,
				       WERROR werr,
				       enum drsuapi_DsExtendedError ext_err,
				       void *cb_data)
{
	struct repl_secret_state *state = talloc_get_type_abort(cb_data, struct repl_secret_state);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(3,(__location__ ": repl secret failed for user %s - %s: extended_ret[0x%X]\n",
			 state->user_dn, win_errstr(werr), ext_err));
	} else {
		DEBUG(3,(__location__ ": repl secret completed OK for '%s'\n", state->user_dn));
	}
	talloc_free(state);
}


/**
 * Called when the auth code wants us to try and replicate
 * a users secrets
 */
void drepl_repl_secret(struct dreplsrv_service *service,
		       const char *user_dn)
{
	WERROR werr;
	struct ldb_dn *nc_dn, *nc_root, *source_dsa_dn;
	struct dreplsrv_partition *p;
	struct GUID *source_dsa_guid;
	struct repl_secret_state *state;
	int ret;

	state = talloc_zero(service, struct repl_secret_state);
	if (state == NULL) {
		/* nothing to do, no return value */
		return;
	}

	/* keep a copy for logging in the callback */
	state->user_dn = talloc_strdup(state, user_dn);

	nc_dn = ldb_dn_new(state, service->samdb, user_dn);
	if (!ldb_dn_validate(nc_dn)) {
		DEBUG(0,(__location__ ": Failed to parse user_dn '%s'\n", user_dn));
		talloc_free(state);
		return;
	}

	/* work out which partition this is in */
	ret = dsdb_find_nc_root(service->samdb, state, nc_dn, &nc_root);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find nc_root for user_dn '%s'\n", user_dn));
		talloc_free(state);
		return;
	}

	/* find the partition in our list */
	for (p=service->partitions; p; p=p->next) {
		if (ldb_dn_compare(p->dn, nc_root) == 0) {
			break;
		}
	}
	if (p == NULL) {
		DEBUG(0,(__location__ ": Failed to find partition for nc_root '%s'\n", ldb_dn_get_linearized(nc_root)));
		talloc_free(state);
		return;
	}

	if (p->sources == NULL) {
		DEBUG(0,(__location__ ": No sources for nc_root '%s' for user_dn '%s'\n",
			 ldb_dn_get_linearized(nc_root), user_dn));
		talloc_free(state);
		return;
	}

	/* use the first source, for no particularly good reason */
	source_dsa_guid = &p->sources->repsFrom1->source_dsa_obj_guid;

	source_dsa_dn = ldb_dn_new(state, service->samdb,
				   talloc_asprintf(state, "<GUID=%s>",
						   GUID_string(state, source_dsa_guid)));
	if (!ldb_dn_validate(source_dsa_dn)) {
		DEBUG(0,(__location__ ": Invalid source DSA GUID '%s' for user_dn '%s'\n",
			 GUID_string(state, source_dsa_guid), user_dn));
		talloc_free(state);
		return;
	}

	werr = drepl_request_extended_op(service,
					 nc_dn,
					 source_dsa_dn,
					 DRSUAPI_EXOP_REPL_SECRET,
					 0,
					 p->sources->repsFrom1->highwatermark.highest_usn,
					 drepl_repl_secret_callback, state);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(2,(__location__ ": Failed to setup secret replication for user_dn '%s'\n", user_dn));
		talloc_free(state);
		return;
	}
	DEBUG(3,(__location__ ": started secret replication for %s\n", user_dn));
}
