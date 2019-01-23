/*
 Unix SMB/CIFS implementation.

 DRS Replica Information

 Copyright (C) Erick Nogueira do Nascimento 2009-2010

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
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/proto.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "lib/messaging/irpc.h"
#include "dsdb/kcc/kcc_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"
#include "dsdb/common/util.h"


/*
   get the stamp values for the linked attribute 'linked_attr_name' of the object 'dn'
*/
static WERROR get_linked_attribute_value_stamp(TALLOC_CTX *mem_ctx, struct ldb_context *samdb,
				       struct ldb_dn *dn, const char *linked_attr_name,
				       uint32_t *attr_version, NTTIME *attr_change_time, uint32_t *attr_orig_usn)
{
	struct ldb_result *res;
	int ret;
	const char *attrs[2];
	struct ldb_dn *attr_ext_dn;
	NTSTATUS ntstatus;

	attrs[0] = linked_attr_name;
	attrs[1] = NULL;

	ret = dsdb_search_dn(samdb, mem_ctx, &res, dn, attrs,
			     DSDB_SEARCH_SHOW_EXTENDED_DN | DSDB_SEARCH_REVEAL_INTERNALS);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, (__location__ ": Failed search for attribute %s on %s",
				linked_attr_name, ldb_dn_get_linearized(dn)));
		return WERR_INTERNAL_ERROR;
	}

	attr_ext_dn = ldb_msg_find_attr_as_dn(samdb, mem_ctx, res->msgs[0], linked_attr_name);
	if (!attr_ext_dn) {
		DEBUG(0, (__location__ ": Failed search for attribute %s on %s",
				linked_attr_name, ldb_dn_get_linearized(dn)));
		return WERR_INTERNAL_ERROR;
	}

	DEBUG(0, ("linked_attr_name = %s, attr_ext_dn = %s", linked_attr_name,
		  ldb_dn_get_extended_linearized(mem_ctx, attr_ext_dn, 1)));

	ntstatus = dsdb_get_extended_dn_uint32(attr_ext_dn, attr_version, "RMD_VERSION");
	if (!NT_STATUS_IS_OK(ntstatus)) {
		DEBUG(0, (__location__ ": Could not extract component %s from dn \"%s\"",
				"RMD_VERSION", ldb_dn_get_extended_linearized(mem_ctx, attr_ext_dn, 1)));
		return WERR_INTERNAL_ERROR;
	}

	ntstatus = dsdb_get_extended_dn_nttime(attr_ext_dn, attr_change_time, "RMD_CHANGETIME");
	if (!NT_STATUS_IS_OK(ntstatus)) {
		DEBUG(0, (__location__ ": Could not extract component %s from dn \"%s\"",
				"RMD_CHANGETIME", ldb_dn_get_extended_linearized(mem_ctx, attr_ext_dn, 1)));
		return WERR_INTERNAL_ERROR;
	}

	ntstatus = dsdb_get_extended_dn_uint32(attr_ext_dn, attr_version, "RMD_ORIGINATING_USN");
	if (!NT_STATUS_IS_OK(ntstatus)) {
		DEBUG(0, (__location__ ": Could not extract component %s from dn \"%s\"",
				"RMD_ORIGINATING_USN", ldb_dn_get_extended_linearized(mem_ctx, attr_ext_dn, 1)));
		return WERR_INTERNAL_ERROR;
	}

	return WERR_OK;
}

static WERROR get_repl_prop_metadata_ctr(TALLOC_CTX *mem_ctx,
					 struct ldb_context *samdb,
					 struct ldb_dn *dn,
					 struct replPropertyMetaDataBlob *obj_metadata_ctr)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { "replPropertyMetaData", NULL };
	const struct ldb_val *omd_value;
	enum ndr_err_code ndr_err;

	ret = ldb_search(samdb, mem_ctx, &res, dn, LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS || res->count != 1) {
		DEBUG(0, (__location__ ": Failed search for replPropertyMetaData attribute on %s",
			  ldb_dn_get_linearized(dn)));
		return WERR_INTERNAL_ERROR;
	}

	omd_value = ldb_msg_find_ldb_val(res->msgs[0], "replPropertyMetaData");
	if (!omd_value) {
                DEBUG(0,(__location__ ": Object %s does not have a replPropertyMetaData attribute\n",
                         ldb_dn_get_linearized(dn)));
		talloc_free(res);
		return WERR_INTERNAL_ERROR;
	}

	ndr_err = ndr_pull_struct_blob(omd_value, mem_ctx,
				        obj_metadata_ctr,
				       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,(__location__ ": Failed to parse replPropertyMetaData for %s\n",
			 ldb_dn_get_linearized(dn)));
		talloc_free(res);
		return WERR_INTERNAL_ERROR;
	}

	talloc_free(res);
	return WERR_OK;
}

/*
  get the DN of the nTDSDSA object from the configuration partition
  whose invocationId is 'invocation_id'
  put the value on 'dn_str'
*/
static WERROR get_dn_from_invocation_id(TALLOC_CTX *mem_ctx,
					struct ldb_context *samdb,
					struct GUID *invocation_id,
					const char **dn_str)
{
	char *invocation_id_str;
	const char *attrs_invocation[] = { NULL };
	struct ldb_message *msg;
	int ret;

	invocation_id_str = GUID_string(mem_ctx, invocation_id);
	W_ERROR_HAVE_NO_MEMORY(invocation_id_str);

	ret = dsdb_search_one(samdb, invocation_id_str, &msg, ldb_get_config_basedn(samdb), LDB_SCOPE_SUBTREE,
			      attrs_invocation, 0, "(&(objectClass=nTDSDSA)(invocationId=%s))", invocation_id_str);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, (__location__ ": Failed search for the object DN under %s whose invocationId is %s",
			  invocation_id_str, ldb_dn_get_linearized(ldb_get_config_basedn(samdb))));
		talloc_free(invocation_id_str);
		return WERR_INTERNAL_ERROR;
	}

	*dn_str = ldb_dn_alloc_linearized(mem_ctx, msg->dn);
	talloc_free(invocation_id_str);
	return WERR_OK;
}

/*
  get metadata version 2 info for a specified object DN
*/
static WERROR kccdrs_replica_get_info_obj_metadata2(TALLOC_CTX *mem_ctx,
						    struct ldb_context *samdb,
						    struct drsuapi_DsReplicaGetInfo *r,
						    union drsuapi_DsReplicaInfo *reply,
						    struct ldb_dn *dn,
						    uint32_t base_index)
{
	WERROR status;
	struct replPropertyMetaDataBlob omd_ctr;
	struct replPropertyMetaData1 *attr;
	struct drsuapi_DsReplicaObjMetaData2Ctr *metadata2;
	const struct dsdb_schema *schema;

	uint32_t i, j;

	DEBUG(0, ("kccdrs_replica_get_info_obj_metadata2() called\n"));

	if (!dn) {
		return WERR_INVALID_PARAMETER;
	}

	if (!ldb_dn_validate(dn)) {
		return WERR_DS_DRA_BAD_DN;
	}

	status = get_repl_prop_metadata_ctr(mem_ctx, samdb, dn, &omd_ctr);
	W_ERROR_NOT_OK_RETURN(status);

	schema = dsdb_get_schema(samdb, reply);
	if (!schema) {
		DEBUG(0,(__location__": Failed to get the schema\n"));
		return WERR_INTERNAL_ERROR;
	}

	reply->objmetadata2 = talloc_zero(mem_ctx, struct drsuapi_DsReplicaObjMetaData2Ctr);
	W_ERROR_HAVE_NO_MEMORY(reply->objmetadata2);
	metadata2 = reply->objmetadata2;
	metadata2->enumeration_context = 0;

	/* For each replicated attribute of the object */
	for (i = 0, j = 0; i < omd_ctr.ctr.ctr1.count; i++) {
		const struct dsdb_attribute *schema_attr;
		uint32_t attr_version;
		NTTIME attr_change_time;
		uint32_t attr_originating_usn;

		/*
		  attr := attrsSeq[i]
		  s := AttrStamp(object, attr)
		*/
		/* get a reference to the attribute on 'omd_ctr' */
		attr = &omd_ctr.ctr.ctr1.array[j];

		schema_attr = dsdb_attribute_by_attributeID_id(schema, attr->attid);

		DEBUG(0, ("attribute_id = %d, attribute_name: %s\n", attr->attid, schema_attr->lDAPDisplayName));

		/*
		  if (attr in Link Attributes of object and
		    dwInVersion = 2 and DS_REPL_INFO_FLAG_IMPROVE_LINKED_ATTRS in msgIn.ulFlags)
		*/
		if (schema_attr &&
		    schema_attr->linkID != 0 && /* Checks if attribute is a linked attribute */
		    (schema_attr->linkID % 2) == 0 && /* is it a forward link? only forward links have the LinkValueStamp */
		    r->in.level == 2 &&
		    (r->in.req->req2.flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE)) /* on MS-DRSR it is DS_REPL_INFO_FLAG_IMPROVE_LINKED_ATTRS */
		{
			/*
			  ls := LinkValueStamp of the most recent
				value change in object!attr
			*/
			status = get_linked_attribute_value_stamp(mem_ctx, samdb, dn, schema_attr->lDAPDisplayName,
								  &attr_version, &attr_change_time, &attr_originating_usn);
			W_ERROR_NOT_OK_RETURN(status);

			/*
			 Aligning to MS-DRSR 4.1.13.3:
			 's' on the doc is 'attr->originating_change_time' here
			 'ls' on the doc is 'attr_change_time' here
			*/

			/* if (ls is more recent than s (based on order in which the change was applied on server)) then */
			if (attr_change_time > attr->originating_change_time) {
				/*
				 Improve the stamp with the link value stamp.
				  s.dwVersion := ls.dwVersion
				  s.timeChanged := ls.timeChanged
				  s.uuidOriginating := NULLGUID
				  s.usnOriginating := ls.usnOriginating
				*/
				attr->version = attr_version;
				attr->originating_change_time = attr_change_time;
				attr->originating_invocation_id = GUID_zero();
				attr->originating_usn = attr_originating_usn;
			}
		}

		if (i < base_index) {
			continue;
		}

		metadata2->array = talloc_realloc(mem_ctx, metadata2->array,
						  struct drsuapi_DsReplicaObjMetaData2, j + 1);
		W_ERROR_HAVE_NO_MEMORY(metadata2->array);
		metadata2->array[j].attribute_name = schema_attr->lDAPDisplayName;
		metadata2->array[j].local_usn = attr->local_usn;
		metadata2->array[j].originating_change_time = attr->originating_change_time;
		metadata2->array[j].originating_invocation_id = attr->originating_invocation_id;
		metadata2->array[j].originating_usn = attr->originating_usn;
		metadata2->array[j].version = attr->version;

		/*
		  originating_dsa_dn := GetDNFromInvocationID(originating_invocation_id)
		  GetDNFromInvocationID() should return the DN of the nTDSDSAobject that has the specified invocation ID
		  See MS-DRSR 4.1.13.3 and 4.1.13.2.1
		*/
		status = get_dn_from_invocation_id(mem_ctx, samdb,
						   &attr->originating_invocation_id,
						   &metadata2->array[j].originating_dsa_dn);
		W_ERROR_NOT_OK_RETURN(status);
		j++;
		metadata2->count = j;

	}

	return WERR_OK;
}

/*
  get cursors info for a specified DN
*/
static WERROR kccdrs_replica_get_info_cursors(TALLOC_CTX *mem_ctx,
					      struct ldb_context *samdb,
					      struct drsuapi_DsReplicaGetInfo *r,
					      union drsuapi_DsReplicaInfo *reply,
					      struct ldb_dn *dn)
{
	int ret;

	if (!ldb_dn_validate(dn)) {
		return WERR_INVALID_PARAMETER;
	}
	reply->cursors = talloc(mem_ctx, struct drsuapi_DsReplicaCursorCtr);
	W_ERROR_HAVE_NO_MEMORY(reply->cursors);

	reply->cursors->reserved = 0;

	ret = dsdb_load_udv_v1(samdb, dn, reply->cursors, &reply->cursors->array, &reply->cursors->count);
	if (ret != LDB_SUCCESS) {
		return WERR_DS_DRA_BAD_NC;
	}
	return WERR_OK;
}

/*
  get cursors2 info for a specified DN
*/
static WERROR kccdrs_replica_get_info_cursors2(TALLOC_CTX *mem_ctx,
					       struct ldb_context *samdb,
					       struct drsuapi_DsReplicaGetInfo *r,
					       union drsuapi_DsReplicaInfo *reply,
					       struct ldb_dn *dn)
{
	int ret;

	if (!ldb_dn_validate(dn)) {
		return WERR_INVALID_PARAMETER;
	}
	reply->cursors2 = talloc(mem_ctx, struct drsuapi_DsReplicaCursor2Ctr);
	W_ERROR_HAVE_NO_MEMORY(reply->cursors2);

	ret = dsdb_load_udv_v2(samdb, dn, reply->cursors2, &reply->cursors2->array, &reply->cursors2->count);
	if (ret != LDB_SUCCESS) {
		return WERR_DS_DRA_BAD_NC;
	}

	reply->cursors2->enumeration_context = reply->cursors2->count;
	return WERR_OK;
}

/*
  get pending ops info for a specified DN
*/
static WERROR kccdrs_replica_get_info_pending_ops(TALLOC_CTX *mem_ctx,
						  struct ldb_context *samdb,
						  struct drsuapi_DsReplicaGetInfo *r,
						  union drsuapi_DsReplicaInfo *reply,
						  struct ldb_dn *dn)
{
	struct timeval now = timeval_current();

	if (!ldb_dn_validate(dn)) {
		return WERR_INVALID_PARAMETER;
	}
	reply->pendingops = talloc(mem_ctx, struct drsuapi_DsReplicaOpCtr);
	W_ERROR_HAVE_NO_MEMORY(reply->pendingops);

	/* claim no pending ops for now */
	reply->pendingops->time = timeval_to_nttime(&now);
	reply->pendingops->count = 0;
	reply->pendingops->array = NULL;

	return WERR_OK;
}

struct ncList {
	struct ldb_dn *dn;
	struct ncList *prev, *next;
};

/*
  Fill 'master_nc_list' with the master ncs hosted by this server
*/
static WERROR get_master_ncs(TALLOC_CTX *mem_ctx, struct ldb_context *samdb,
			     const char *ntds_guid_str, struct ncList **master_nc_list)
{
	const char *attrs[] = { "hasMasterNCs", NULL };
	struct ldb_result *res;
	struct ncList *nc_list = NULL;
	struct ncList *nc_list_elem;
	int ret;
	unsigned int i;
	char *nc_str;

	ret = ldb_search(samdb, mem_ctx, &res, ldb_get_config_basedn(samdb),
			LDB_SCOPE_DEFAULT, attrs, "(objectguid=%s)", ntds_guid_str);

	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed objectguid search - %s\n", ldb_errstring(samdb)));
		return WERR_INTERNAL_ERROR;
	}

	if (res->count == 0) {
		DEBUG(0,(__location__ ": Failed: objectguid=%s not found\n", ntds_guid_str));
		return WERR_INTERNAL_ERROR;
	}

	for (i = 0; i < res->count; i++) {
		struct ldb_message_element *msg_elem = ldb_msg_find_element(res->msgs[i], "hasMasterNCs");
		unsigned int k;

		if (!msg_elem || msg_elem->num_values == 0) {
			DEBUG(0,(__location__ ": Failed: Attribute hasMasterNCs not found - %s\n",
			      ldb_errstring(samdb)));
			return WERR_INTERNAL_ERROR;
		}

		for (k = 0; k < msg_elem->num_values; k++) {
			/* copy the string on msg_elem->values[k]->data to nc_str */
			nc_str = talloc_strndup(mem_ctx, (char *)msg_elem->values[k].data, msg_elem->values[k].length);
			W_ERROR_HAVE_NO_MEMORY(nc_str);

			nc_list_elem = talloc_zero(mem_ctx, struct ncList);
			W_ERROR_HAVE_NO_MEMORY(nc_list_elem);
			nc_list_elem->dn = ldb_dn_new(mem_ctx, samdb, nc_str);
			W_ERROR_HAVE_NO_MEMORY(nc_list_elem);
			DLIST_ADD(nc_list, nc_list_elem);
		}

	}

	*master_nc_list = nc_list;
	return WERR_OK;
}

/*
  Fill 'nc_list' with the ncs list. (MS-DRSR 4.1.13.3)
  if the object dn is specified, fill 'nc_list' only with this dn
  otherwise, fill 'nc_list' with all master ncs hosted by this server
*/
static WERROR get_ncs_list(TALLOC_CTX *mem_ctx,
		struct ldb_context *samdb,
		struct kccsrv_service *service,
		const char *object_dn_str,
		struct ncList **nc_list)
{
	WERROR status;
	struct ncList *nc_list_elem;
	struct ldb_dn *nc_dn;

	if (object_dn_str != NULL) {
		/* ncs := { object_dn } */
		*nc_list = NULL;
		nc_dn = ldb_dn_new(mem_ctx, samdb, object_dn_str);
		nc_list_elem = talloc_zero(mem_ctx, struct ncList);
		W_ERROR_HAVE_NO_MEMORY(nc_list_elem);
		nc_list_elem->dn = nc_dn;
		DLIST_ADD_END(*nc_list, nc_list_elem, struct ncList*);
	} else {
		/* ncs := getNCs() from ldb database.
		 * getNCs() must return an array containing
		 * the DSNames of all NCs hosted by this
		 * server.
		 */
		char *ntds_guid_str = GUID_string(mem_ctx, &service->ntds_guid);
		W_ERROR_HAVE_NO_MEMORY(ntds_guid_str);
		status = get_master_ncs(mem_ctx, samdb, ntds_guid_str, nc_list);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}

/*
  Copy the fields from 'reps1' to 'reps2', leaving zeroed the fields on
  'reps2' that aren't available on 'reps1'.
*/
static WERROR copy_repsfrom_1_to_2(TALLOC_CTX *mem_ctx,
				 struct repsFromTo2 **reps2,
				 struct repsFromTo1 *reps1)
{
	struct repsFromTo2* reps;

	reps = talloc_zero(mem_ctx, struct repsFromTo2);
	W_ERROR_HAVE_NO_MEMORY(reps);

	reps->blobsize = reps1->blobsize;
	reps->consecutive_sync_failures = reps1->consecutive_sync_failures;
	reps->last_attempt = reps1->last_attempt;
	reps->last_success = reps1->last_success;
	reps->result_last_attempt = reps1->result_last_attempt;
	reps->other_info = talloc_zero(mem_ctx, struct repsFromTo2OtherInfo);
	W_ERROR_HAVE_NO_MEMORY(reps->other_info);
	reps->other_info->dns_name1 = reps1->other_info->dns_name;
	reps->replica_flags = reps1->replica_flags;
	memcpy(reps->schedule, reps1->schedule, sizeof(reps1->schedule));
	reps->reserved = reps1->reserved;
	reps->highwatermark = reps1->highwatermark;
	reps->source_dsa_obj_guid = reps1->source_dsa_obj_guid;
	reps->source_dsa_invocation_id = reps1->source_dsa_invocation_id;
	reps->transport_guid = reps1->transport_guid;

	*reps2 = reps;
	return WERR_OK;
}

static WERROR fill_neighbor_from_repsFrom(TALLOC_CTX *mem_ctx,
					  struct ldb_context *samdb,
					  struct ldb_dn *nc_dn,
					  struct drsuapi_DsReplicaNeighbour *neigh,
					  struct repsFromTo2 *reps_from)
{
	struct ldb_dn *source_dsa_dn;
	int ret;
	struct ldb_dn *transport_obj_dn = NULL;

	neigh->source_dsa_address = reps_from->other_info->dns_name1;
	neigh->replica_flags = reps_from->replica_flags;
	neigh->last_attempt = reps_from->last_attempt;
	neigh->source_dsa_obj_guid = reps_from->source_dsa_obj_guid;

	ret = dsdb_find_dn_by_guid(samdb, mem_ctx, &reps_from->source_dsa_obj_guid,
				   &source_dsa_dn);

	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find DN for neighbor GUID %s\n",
			 GUID_string(mem_ctx, &reps_from->source_dsa_obj_guid)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	neigh->source_dsa_obj_dn = ldb_dn_get_linearized(source_dsa_dn);
	neigh->naming_context_dn = ldb_dn_get_linearized(nc_dn);

	if (dsdb_find_guid_by_dn(samdb, nc_dn, &neigh->naming_context_obj_guid)
			!= LDB_SUCCESS) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (!GUID_all_zero(&reps_from->transport_guid)) {
		ret = dsdb_find_dn_by_guid(samdb, mem_ctx, &reps_from->transport_guid,
					   &transport_obj_dn);
		if (ret != LDB_SUCCESS) {
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
	}

	neigh->transport_obj_dn = ldb_dn_get_linearized(transport_obj_dn);
	neigh->source_dsa_invocation_id = reps_from->source_dsa_invocation_id;
	neigh->transport_obj_guid = reps_from->transport_guid;
	neigh->highest_usn = reps_from->highwatermark.highest_usn;
	neigh->tmp_highest_usn = reps_from->highwatermark.tmp_highest_usn;
	neigh->last_success = reps_from->last_success;
	neigh->result_last_attempt = reps_from->result_last_attempt;
	neigh->consecutive_sync_failures = reps_from->consecutive_sync_failures;
	neigh->reserved = 0; /* Unused. MUST be 0. */

	return WERR_OK;
}

/*
  Get the inbound neighbours of this DC
  See details on MS-DRSR 4.1.13.3, for infoType DS_REPL_INFO_NEIGHBORS
*/
static WERROR kccdrs_replica_get_info_neighbours(TALLOC_CTX *mem_ctx,
						 struct kccsrv_service *service,
						 struct ldb_context *samdb,
						 struct drsuapi_DsReplicaGetInfo *r,
						 union drsuapi_DsReplicaInfo *reply,
						 uint32_t base_index,
						 struct GUID req_src_dsa_guid,
						 const char *object_dn_str)
{
	WERROR status;
	uint32_t i, j;
	struct ldb_dn *nc_dn = NULL;
	struct ncList *p_nc_list = NULL;
	struct repsFromToBlob *reps_from_blob = NULL;
	struct repsFromTo2 *reps_from = NULL;
	uint32_t c_reps_from;
	uint32_t i_rep;
	struct ncList *nc_list = NULL;

	status = get_ncs_list(mem_ctx, samdb, service, object_dn_str, &nc_list);
	W_ERROR_NOT_OK_RETURN(status);

	i = j = 0;

	reply->neighbours = talloc_zero(mem_ctx, struct drsuapi_DsReplicaNeighbourCtr);
	W_ERROR_HAVE_NO_MEMORY(reply->neighbours);
	reply->neighbours->reserved = 0;
	reply->neighbours->count = 0;

	/* foreach nc in ncs */
	for (p_nc_list = nc_list; p_nc_list != NULL; p_nc_list = p_nc_list->next) {

		nc_dn = p_nc_list->dn;

		/* load the nc's repsFromTo blob */
		status = dsdb_loadreps(samdb, mem_ctx, nc_dn, "repsFrom",
				&reps_from_blob, &c_reps_from);
		W_ERROR_NOT_OK_RETURN(status);

		/* foreach r in nc!repsFrom */
		for (i_rep = 0; i_rep < c_reps_from; i_rep++) {

			/* put all info on reps_from */
			if (reps_from_blob[i_rep].version == 1) {
				status = copy_repsfrom_1_to_2(mem_ctx, &reps_from,
							      &reps_from_blob[i_rep].ctr.ctr1);
				W_ERROR_NOT_OK_RETURN(status);
			} else { /* reps_from->version == 2 */
				reps_from = &reps_from_blob[i_rep].ctr.ctr2;
			}

			if (GUID_all_zero(&req_src_dsa_guid) ||
			    GUID_compare(&req_src_dsa_guid, &reps_from->source_dsa_obj_guid) == 0)
			{

				if (i >= base_index) {
					struct drsuapi_DsReplicaNeighbour neigh;
					ZERO_STRUCT(neigh);
					status = fill_neighbor_from_repsFrom(mem_ctx, samdb,
									     nc_dn, &neigh,
									     reps_from);
					W_ERROR_NOT_OK_RETURN(status);

					/* append the neighbour to the neighbours array */
					reply->neighbours->array = talloc_realloc(mem_ctx,
										  reply->neighbours->array,
										  struct drsuapi_DsReplicaNeighbour,
										  reply->neighbours->count + 1);
					reply->neighbours->array[reply->neighbours->count] = neigh;
					reply->neighbours->count++;
					j++;
				}

				i++;
			}
		}
	}

	return WERR_OK;
}

static WERROR fill_neighbor_from_repsTo(TALLOC_CTX *mem_ctx,
					struct ldb_context *samdb, struct ldb_dn *nc_dn,
					struct drsuapi_DsReplicaNeighbour *neigh,
					struct repsFromTo2 *reps_to)
{
	int ret;
	struct ldb_dn *source_dsa_dn;

	neigh->source_dsa_address = reps_to->other_info->dns_name1;
	neigh->replica_flags = reps_to->replica_flags;
	neigh->last_attempt = reps_to->last_attempt;
	neigh->source_dsa_obj_guid = reps_to->source_dsa_obj_guid;

	ret = dsdb_find_dn_by_guid(samdb, mem_ctx, &reps_to->source_dsa_obj_guid, &source_dsa_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find DN for neighbor GUID %s\n",
			 GUID_string(mem_ctx, &reps_to->source_dsa_obj_guid)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	neigh->source_dsa_obj_dn = ldb_dn_get_linearized(source_dsa_dn);
	neigh->naming_context_dn = ldb_dn_get_linearized(nc_dn);

	ret = dsdb_find_guid_by_dn(samdb, nc_dn,
			&neigh->naming_context_obj_guid);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find GUID for DN %s\n",
			 ldb_dn_get_linearized(nc_dn)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	return WERR_OK;
}

/*
  Get the outbound neighbours of this DC
  See details on MS-DRSR 4.1.13.3, for infoType DS_REPL_INFO_REPSTO
*/
static WERROR kccdrs_replica_get_info_repsto(TALLOC_CTX *mem_ctx,
					     struct kccsrv_service *service,
					     struct ldb_context *samdb,
					     struct drsuapi_DsReplicaGetInfo *r,
					     union drsuapi_DsReplicaInfo *reply,
					     uint32_t base_index,
					     struct GUID req_src_dsa_guid,
					     const char *object_dn_str)
{
	WERROR status;
	uint32_t i, j;
	struct ncList *p_nc_list = NULL;
	struct ldb_dn *nc_dn = NULL;
	struct repsFromToBlob *reps_to_blob;
	struct repsFromTo2 *reps_to;
	uint32_t c_reps_to;
	uint32_t i_rep;
	struct ncList *nc_list = NULL;

	status = get_ncs_list(mem_ctx, samdb, service, object_dn_str, &nc_list);
	W_ERROR_NOT_OK_RETURN(status);

	i = j = 0;

	reply->repsto = talloc_zero(mem_ctx, struct drsuapi_DsReplicaNeighbourCtr);
	W_ERROR_HAVE_NO_MEMORY(reply->repsto);
	reply->repsto->reserved = 0;
	reply->repsto->count = 0;

	/* foreach nc in ncs */
	for (p_nc_list = nc_list; p_nc_list != NULL; p_nc_list = p_nc_list->next) {

		nc_dn = p_nc_list->dn;

		status = dsdb_loadreps(samdb, mem_ctx, nc_dn, "repsTo",
				&reps_to_blob, &c_reps_to);
		W_ERROR_NOT_OK_RETURN(status);

		/* foreach r in nc!repsTo */
		for (i_rep = 0; i_rep < c_reps_to; i_rep++) {
			struct drsuapi_DsReplicaNeighbour neigh;
			ZERO_STRUCT(neigh);

			/* put all info on reps_to */
			if (reps_to_blob[i_rep].version == 1) {
				status = copy_repsfrom_1_to_2(mem_ctx,
							      &reps_to,
							      &reps_to_blob[i_rep].ctr.ctr1);
				W_ERROR_NOT_OK_RETURN(status);
			} else { /* reps_to->version == 2 */
				reps_to = &reps_to_blob[i_rep].ctr.ctr2;
			}

			if (i >= base_index) {
				status = fill_neighbor_from_repsTo(mem_ctx,
								   samdb, nc_dn,
								   &neigh, reps_to);
				W_ERROR_NOT_OK_RETURN(status);

				/* append the neighbour to the neighbours array */
				reply->repsto->array = talloc_realloc(mem_ctx,
									    reply->repsto->array,
									    struct drsuapi_DsReplicaNeighbour,
									    reply->repsto->count + 1);
				reply->repsto->array[reply->repsto->count++] = neigh;
				j++;
			}
			i++;
		}
	}

	return WERR_OK;
}

NTSTATUS kccdrs_replica_get_info(struct irpc_message *msg,
				 struct drsuapi_DsReplicaGetInfo *req)
{
	WERROR status;
	struct drsuapi_DsReplicaGetInfoRequest1 *req1;
	struct drsuapi_DsReplicaGetInfoRequest2 *req2;
	uint32_t base_index;
	union drsuapi_DsReplicaInfo *reply;
	struct GUID req_src_dsa_guid;
	const char *object_dn_str = NULL;
	struct kccsrv_service *service;
	struct ldb_context *samdb;
	TALLOC_CTX *mem_ctx;
	enum drsuapi_DsReplicaInfoType info_type;
	uint32_t flags;
	const char *attribute_name, *value_dn;

	service = talloc_get_type(msg->private_data, struct kccsrv_service);
	samdb = service->samdb;
	mem_ctx = talloc_new(msg);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

#if 0
	NDR_PRINT_IN_DEBUG(drsuapi_DsReplicaGetInfo, req);
#endif

	/* check request version */
	if (req->in.level != DRSUAPI_DS_REPLICA_GET_INFO &&
	    req->in.level != DRSUAPI_DS_REPLICA_GET_INFO2)
	{
		DEBUG(1,(__location__ ": Unsupported DsReplicaGetInfo level %u\n",
			 req->in.level));
		status = WERR_REVISION_MISMATCH;
		goto done;
	}

	if (req->in.level == DRSUAPI_DS_REPLICA_GET_INFO) {
		req1 = &req->in.req->req1;
		base_index = 0;
		info_type = req1->info_type;
		object_dn_str = req1->object_dn;
		req_src_dsa_guid = req1->source_dsa_guid;
	} else { /* r->in.level == DRSUAPI_DS_REPLICA_GET_INFO2 */
		req2 = &req->in.req->req2;
		if (req2->enumeration_context == 0xffffffff) {
			/* no more data is available */
			status = WERR_NO_MORE_ITEMS; /* on MS-DRSR it is ERROR_NO_MORE_ITEMS */
			goto done;
		}

		base_index = req2->enumeration_context;
		info_type = req2->info_type;
		object_dn_str = req2->object_dn;
		req_src_dsa_guid = req2->source_dsa_guid;
		flags = req2->flags;
		attribute_name = req2->attribute_name;
		value_dn = req2->value_dn_str;
	}

	reply = req->out.info;
	*req->out.info_type = info_type;

	/* Based on the infoType requested, retrieve the corresponding
	 * information and construct the response message */
	switch (info_type) {

	case DRSUAPI_DS_REPLICA_INFO_NEIGHBORS:
		status = kccdrs_replica_get_info_neighbours(mem_ctx, service, samdb, req,
							    reply, base_index, req_src_dsa_guid,
							    object_dn_str);
		break;
	case DRSUAPI_DS_REPLICA_INFO_REPSTO:
		status = kccdrs_replica_get_info_repsto(mem_ctx, service, samdb, req,
							reply, base_index, req_src_dsa_guid,
							object_dn_str);
		break;
	case DRSUAPI_DS_REPLICA_INFO_CURSORS: /* On MS-DRSR it is DS_REPL_INFO_CURSORS_FOR_NC */
		status = kccdrs_replica_get_info_cursors(mem_ctx, samdb, req, reply,
							 ldb_dn_new(mem_ctx, samdb, object_dn_str));
		break;
	case DRSUAPI_DS_REPLICA_INFO_CURSORS2: /* On MS-DRSR it is DS_REPL_INFO_CURSORS_2_FOR_NC */
		status = kccdrs_replica_get_info_cursors2(mem_ctx, samdb, req, reply,
							  ldb_dn_new(mem_ctx, samdb, object_dn_str));
		break;
	case DRSUAPI_DS_REPLICA_INFO_PENDING_OPS:
		status = kccdrs_replica_get_info_pending_ops(mem_ctx, samdb, req, reply,
							     ldb_dn_new(mem_ctx, samdb, object_dn_str));
		break;
	case DRSUAPI_DS_REPLICA_INFO_CURSORS3: /* On MS-DRSR it is DS_REPL_INFO_CURSORS_3_FOR_NC */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_UPTODATE_VECTOR_V1: /* On MS-DRSR it is DS_REPL_INFO_UPTODATE_VECTOR_V1 */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA: /* On MS-DRSR it is DS_REPL_INFO_METADATA_FOR_OBJ */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA2: /* On MS-DRSR it is DS_REPL_INFO_METADATA_FOR_OBJ */
		status = kccdrs_replica_get_info_obj_metadata2(mem_ctx, samdb, req, reply,
							       ldb_dn_new(mem_ctx, samdb, object_dn_str), base_index);
		break;
	case DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA: /* On MS-DRSR it is DS_REPL_INFO_METADATA_FOR_ATTR_VALUE */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA2: /* On MS-DRSR it is DS_REPL_INFO_METADATA_2_FOR_ATTR_VALUE */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_KCC_DSA_CONNECT_FAILURES: /* On MS-DRSR it is DS_REPL_INFO_KCC_DSA_CONNECT_FAILURES */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_KCC_DSA_LINK_FAILURES: /* On MS-DRSR it is DS_REPL_INFO_KCC_LINK_FAILURES */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_CLIENT_CONTEXTS: /* On MS-DRSR it is DS_REPL_INFO_CLIENT_CONTEXTS */
		status = WERR_INVALID_LEVEL;
		break;
	case DRSUAPI_DS_REPLICA_INFO_SERVER_OUTGOING_CALLS: /* On MS-DRSR it is DS_REPL_INFO_SERVER_OUTGOING_CALLS */
		status = WERR_INVALID_LEVEL;
		break;
	default:
		DEBUG(1,(__location__ ": Unsupported DsReplicaGetInfo info_type %u\n",
			 info_type));
		status = WERR_INVALID_LEVEL;
		break;
	}

done:
	/* put the status on the result field of the reply */
	req->out.result = status;
#if 0
	NDR_PRINT_OUT_DEBUG(drsuapi_DsReplicaGetInfo, req);
#endif
	return NT_STATUS_OK;
}
