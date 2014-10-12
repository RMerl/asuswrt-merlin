/*
   Unix SMB/CIFS implementation.
   KCC service periodic handling

   Copyright (C) Crístian Deives

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
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/messaging/irpc.h"
#include "dsdb/kcc/kcc_service.h"
#include "dsdb/kcc/kcc_connection.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"

static int kccsrv_add_connection(struct kccsrv_service *s,
				 struct kcc_connection *conn)
{
	struct ldb_message *msg;
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *new_dn, *server_dn;
	struct GUID guid;
	/* struct ldb_val schedule_val; */
	int ret;
	bool ok;

	tmp_ctx = talloc_new(s);
	new_dn = samdb_ntds_settings_dn(s->samdb);
	if (!new_dn) {
		DEBUG(0, ("failed to find NTDS settings\n"));
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	new_dn = ldb_dn_copy(tmp_ctx, new_dn);
	if (!new_dn) {
		DEBUG(0, ("failed to copy NTDS settings\n"));
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	guid = GUID_random();
	ok = ldb_dn_add_child_fmt(new_dn, "CN=%s", GUID_string(tmp_ctx, &guid));
	if (!ok) {
		DEBUG(0, ("failed to create nTDSConnection DN\n"));
		ret = LDB_ERR_INVALID_DN_SYNTAX;
		goto done;
	}
	ret = dsdb_find_dn_by_guid(s->samdb, tmp_ctx, &conn->dsa_guid, &server_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("failed to find fromServer DN '%s'\n",
			  GUID_string(tmp_ctx, &conn->dsa_guid)));
		goto done;
	}
	/*schedule_val = data_blob_const(r1->schedule, sizeof(r1->schedule));*/

	msg = ldb_msg_new(tmp_ctx);
	msg->dn = new_dn;
	ldb_msg_add_string(msg, "objectClass", "nTDSConnection");
	ldb_msg_add_string(msg, "showInAdvancedViewOnly", "TRUE");
	ldb_msg_add_string(msg, "enabledConnection", "TRUE");
	ldb_msg_add_linearized_dn(msg, "fromServer", server_dn);
	/* ldb_msg_add_value(msg, "schedule", &schedule_val, NULL); */
	samdb_msg_add_uint(s->samdb, msg, msg, "options", 1);

	ret = ldb_add(s->samdb, msg);
	if (ret == LDB_SUCCESS) {
		DEBUG(2, ("added nTDSConnection object '%s'\n",
			  ldb_dn_get_linearized(new_dn)));
	} else {
		DEBUG(0, ("failed to add an nTDSConnection object: %s\n",
			  ldb_strerror(ret)));
	}

done:
	talloc_free(tmp_ctx);
	return ret;
}

static int kccsrv_delete_connection(struct kccsrv_service *s,
				    struct kcc_connection *conn)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *dn;
	int ret;

	tmp_ctx = talloc_new(s);
	ret = dsdb_find_dn_by_guid(s->samdb, tmp_ctx, &conn->obj_guid, &dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("failed to find nTDSConnection's DN: %s\n",
			  ldb_strerror(ret)));
		goto done;
	}

	ret = ldb_delete(s->samdb, dn);
	if (ret == LDB_SUCCESS) {
		DEBUG(2, ("deleted nTDSConnection object '%s'\n",
			  ldb_dn_get_linearized(dn)));
	} else {
		DEBUG(0, ("failed to delete an nTDSConnection object: %s\n",
			  ldb_strerror(ret)));
	}

done:
	talloc_free(tmp_ctx);
	return ret;
}

void kccsrv_apply_connections(struct kccsrv_service *s,
			      struct kcc_connection_list *ntds_list,
			      struct kcc_connection_list *dsa_list)
{
	unsigned int i, j, deleted = 0, added = 0;
	int ret;

	for (i = 0; ntds_list && i < ntds_list->count; i++) {
		struct kcc_connection *ntds = &ntds_list->servers[i];
		for (j = 0; j < dsa_list->count; j++) {
			struct kcc_connection *dsa = &dsa_list->servers[j];
			if (GUID_equal(&ntds->dsa_guid, &dsa->dsa_guid)) {
				break;
			}
		}
		if (j == dsa_list->count) {
			ret = kccsrv_delete_connection(s, ntds);
			if (ret == LDB_SUCCESS) {
				deleted++;
			}
		}
	}
	DEBUG(4, ("%d connections have been deleted\n", deleted));

	for (i = 0; i < dsa_list->count; i++) {
		struct kcc_connection *dsa = &dsa_list->servers[i];
		for (j = 0; ntds_list && j < ntds_list->count; j++) {
			struct kcc_connection *ntds = &ntds_list->servers[j];
			if (GUID_equal(&dsa->dsa_guid, &ntds->dsa_guid)) {
				break;
			}
		}
		if (ntds_list == NULL || j == ntds_list->count) {
			ret = kccsrv_add_connection(s, dsa);
			if (ret == LDB_SUCCESS) {
				added++;
			}
		}
	}
	DEBUG(4, ("%d connections have been added\n", added));
}

struct kcc_connection_list *kccsrv_find_connections(struct kccsrv_service *s,
						    TALLOC_CTX *mem_ctx)
{
	unsigned int i;
	int ret;
	struct ldb_dn *base_dn;
	struct ldb_result *res;
	const char *attrs[] = { "objectGUID", "fromServer", NULL };
	struct kcc_connection_list *list;

	kcctpl_test(s);

	base_dn = samdb_ntds_settings_dn(s->samdb);
	if (!base_dn) {
		DEBUG(0, ("failed to find our own NTDS settings DN\n"));
		return NULL;
	}

	ret = ldb_search(s->samdb, mem_ctx, &res, base_dn, LDB_SCOPE_ONELEVEL,
			 attrs, "objectClass=nTDSConnection");
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("failed nTDSConnection search: %s\n",
			  ldb_strerror(ret)));
		return NULL;
	}

	list = talloc(mem_ctx, struct kcc_connection_list);
	if (!list) {
		DEBUG(0, ("out of memory"));
		return NULL;
	}
	list->servers = talloc_array(mem_ctx, struct kcc_connection,
				     res->count);
	if (!list->servers) {
		DEBUG(0, ("out of memory"));
		return NULL;
	}
	list->count = 0;

	for (i = 0; i < res->count; i++) {
		struct ldb_dn *server_dn;

		list->servers[i].obj_guid = samdb_result_guid(res->msgs[i],
							      "objectGUID");
		server_dn = samdb_result_dn(s->samdb, mem_ctx, res->msgs[i],
					    "fromServer", NULL);
		ret = dsdb_find_guid_by_dn(s->samdb, server_dn,
					   &list->servers[i].dsa_guid);
		if (ret != LDB_SUCCESS) {
			DEBUG(0, ("Failed to find connection server's GUID by "
				  "DN=%s: %s\n",
				  ldb_dn_get_linearized(server_dn),
				  ldb_strerror(ret)));
			continue;
		}
		list->count++;
	}
	DEBUG(4, ("found %d existing nTDSConnection objects\n", list->count));
	return list;
}
