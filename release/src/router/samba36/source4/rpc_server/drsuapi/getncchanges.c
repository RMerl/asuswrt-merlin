/* 
   Unix SMB/CIFS implementation.

   implement the DSGetNCChanges call

   Copyright (C) Anatoliy Atanasov 2009
   Copyright (C) Andrew Tridgell 2009
   
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
#include "rpc_server/dcerpc_server.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "libcli/security/session.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "lib/util/binsearch.h"
#include "lib/util/tsort.h"
#include "auth/session.h"
#include "dsdb/common/util.h"

/*
  build a DsReplicaObjectIdentifier from a ldb msg
 */
static struct drsuapi_DsReplicaObjectIdentifier *get_object_identifier(TALLOC_CTX *mem_ctx,
								       struct ldb_message *msg)
{
	struct drsuapi_DsReplicaObjectIdentifier *identifier;
	struct dom_sid *sid;

	identifier = talloc(mem_ctx, struct drsuapi_DsReplicaObjectIdentifier);
	if (identifier == NULL) {
		return NULL;
	}

	identifier->dn = ldb_dn_alloc_linearized(identifier, msg->dn);
	identifier->guid = samdb_result_guid(msg, "objectGUID");

	sid = samdb_result_dom_sid(identifier, msg, "objectSid");
	if (sid) {
		identifier->sid = *sid;
	} else {
		ZERO_STRUCT(identifier->sid);
	}
	return identifier;
}

static int udv_compare(const struct GUID *guid1, struct GUID guid2)
{
	return GUID_compare(guid1, &guid2);
}

/*
  see if we can filter an attribute using the uptodateness_vector
 */
static bool udv_filter(const struct drsuapi_DsReplicaCursorCtrEx *udv,
		       const struct GUID *originating_invocation_id,
		       uint64_t originating_usn)
{
	const struct drsuapi_DsReplicaCursor *c;
	if (udv == NULL) return false;
	BINARY_ARRAY_SEARCH(udv->cursors, udv->count, source_dsa_invocation_id, 
			    originating_invocation_id, udv_compare, c);
	if (c && originating_usn <= c->highest_usn) {
		return true;
	}
	return false;
	
}

static int attid_cmp(enum drsuapi_DsAttributeId a1, enum drsuapi_DsAttributeId a2)
{
	if (a1 == a2) return 0;
	return ((uint32_t)a1) > ((uint32_t)a2) ? 1 : -1;
}

/*
  check if an attribute is in a partial_attribute_set
 */
static bool check_partial_attribute_set(const struct dsdb_attribute *sa,
					struct drsuapi_DsPartialAttributeSet *pas)
{
	enum drsuapi_DsAttributeId *result;
	BINARY_ARRAY_SEARCH_V(pas->attids, pas->num_attids, (enum drsuapi_DsAttributeId)sa->attributeID_id,
			      attid_cmp, result);
	return result != NULL;
}


/* 
  drsuapi_DsGetNCChanges for one object
*/
static WERROR get_nc_changes_build_object(struct drsuapi_DsReplicaObjectListItemEx *obj,
					  struct ldb_message *msg,
					  struct ldb_context *sam_ctx,
					  struct ldb_dn *ncRoot_dn,
					  bool   is_schema_nc,
					  struct dsdb_schema *schema,
					  DATA_BLOB *session_key,
					  uint64_t highest_usn,
					  uint32_t replica_flags,
					  struct drsuapi_DsPartialAttributeSet *partial_attribute_set,
					  struct drsuapi_DsReplicaCursorCtrEx *uptodateness_vector,
					  enum drsuapi_DsExtendedOperation extended_op)
{
	const struct ldb_val *md_value;
	uint32_t i, n;
	struct replPropertyMetaDataBlob md;
	uint32_t rid = 0;
	enum ndr_err_code ndr_err;
	uint32_t *attids;
	const char *rdn;
	const struct dsdb_attribute *rdn_sa;
	unsigned int instanceType;
	struct dsdb_syntax_ctx syntax_ctx;

	/* make dsdb sytanx context for conversions */
	dsdb_syntax_ctx_init(&syntax_ctx, sam_ctx, schema);
	syntax_ctx.is_schema_nc = is_schema_nc;

	instanceType = ldb_msg_find_attr_as_uint(msg, "instanceType", 0);
	if (instanceType & INSTANCE_TYPE_IS_NC_HEAD) {
		obj->is_nc_prefix = true;
		obj->parent_object_guid = NULL;
	} else {
		obj->is_nc_prefix = false;
		obj->parent_object_guid = talloc(obj, struct GUID);
		if (obj->parent_object_guid == NULL) {
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		*obj->parent_object_guid = samdb_result_guid(msg, "parentGUID");
		if (GUID_all_zero(obj->parent_object_guid)) {
			DEBUG(0,(__location__ ": missing parentGUID for %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
	}
	obj->next_object = NULL;
	
	md_value = ldb_msg_find_ldb_val(msg, "replPropertyMetaData");
	if (!md_value) {
		/* nothing to send */
		return WERR_OK;
	}

	if (instanceType & INSTANCE_TYPE_UNINSTANT) {
		/* don't send uninstantiated objects */
		return WERR_OK;
	}

	ndr_err = ndr_pull_struct_blob(md_value, obj, &md,
				       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	
	if (md.version != 1) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	rdn = ldb_dn_get_rdn_name(msg->dn);
	if (rdn == NULL) {
		DEBUG(0,(__location__ ": No rDN for %s\n", ldb_dn_get_linearized(msg->dn)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	rdn_sa = dsdb_attribute_by_lDAPDisplayName(schema, rdn);
	if (rdn_sa == NULL) {
		DEBUG(0,(__location__ ": Can't find dsds_attribute for rDN %s in %s\n", 
			 rdn, ldb_dn_get_linearized(msg->dn)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	obj->meta_data_ctr = talloc(obj, struct drsuapi_DsReplicaMetaDataCtr);
	attids = talloc_array(obj, uint32_t, md.ctr.ctr1.count);

	obj->object.identifier = get_object_identifier(obj, msg);
	if (obj->object.identifier == NULL) {
		return WERR_NOMEM;
	}
	dom_sid_split_rid(NULL, &obj->object.identifier->sid, NULL, &rid);
	
	obj->meta_data_ctr->meta_data = talloc_array(obj, struct drsuapi_DsReplicaMetaData, md.ctr.ctr1.count);
	for (n=i=0; i<md.ctr.ctr1.count; i++) {
		const struct dsdb_attribute *sa;
		bool force_attribute = false;

		/* if the attribute has not changed, and it is not the
		   instanceType then don't include it */
		if (md.ctr.ctr1.array[i].local_usn < highest_usn &&
		    extended_op != DRSUAPI_EXOP_REPL_SECRET &&
		    md.ctr.ctr1.array[i].attid != DRSUAPI_ATTID_instanceType) continue;

		/* don't include the rDN */
		if (md.ctr.ctr1.array[i].attid == rdn_sa->attributeID_id) continue;

		sa = dsdb_attribute_by_attributeID_id(schema, md.ctr.ctr1.array[i].attid);
		if (!sa) {
			DEBUG(0,(__location__ ": Failed to find attribute in schema for attrid %u mentioned in replPropertyMetaData of %s\n", 
				 (unsigned int)md.ctr.ctr1.array[i].attid, 
				 ldb_dn_get_linearized(msg->dn)));
			return WERR_DS_DRA_INTERNAL_ERROR;		
		}

		if (sa->linkID) {
			struct ldb_message_element *el;
			el = ldb_msg_find_element(msg, sa->lDAPDisplayName);
			if (el && el->num_values && dsdb_dn_is_upgraded_link_val(&el->values[0])) {
				/* don't send upgraded links inline */
				continue;
			}
		}

		if (extended_op == DRSUAPI_EXOP_REPL_SECRET &&
		    !dsdb_attr_in_rodc_fas(sa)) {
			force_attribute = true;
			DEBUG(4,("Forcing attribute %s in %s\n",
				 sa->lDAPDisplayName, ldb_dn_get_linearized(msg->dn)));
		}

		/* filter by uptodateness_vector */
		if (md.ctr.ctr1.array[i].attid != DRSUAPI_ATTID_instanceType &&
		    !force_attribute &&
		    udv_filter(uptodateness_vector,
			       &md.ctr.ctr1.array[i].originating_invocation_id, 
			       md.ctr.ctr1.array[i].originating_usn)) {
			continue;
		}

		/* filter by partial_attribute_set */
		if (partial_attribute_set && !check_partial_attribute_set(sa, partial_attribute_set)) {
			continue;
		}

		obj->meta_data_ctr->meta_data[n].originating_change_time = md.ctr.ctr1.array[i].originating_change_time;
		obj->meta_data_ctr->meta_data[n].version = md.ctr.ctr1.array[i].version;
		obj->meta_data_ctr->meta_data[n].originating_invocation_id = md.ctr.ctr1.array[i].originating_invocation_id;
		obj->meta_data_ctr->meta_data[n].originating_usn = md.ctr.ctr1.array[i].originating_usn;
		attids[n] = md.ctr.ctr1.array[i].attid;
		n++;
	}

	/* ignore it if its an empty change. Note that renames always
	 * change the 'name' attribute, so they won't be ignored by
	 * this */
	if (n == 0 ||
	    (n == 1 && attids[0] == DRSUAPI_ATTID_instanceType)) {
		talloc_free(obj->meta_data_ctr);
		obj->meta_data_ctr = NULL;
		return WERR_OK;
	}

	obj->meta_data_ctr->count = n;

	obj->object.flags = DRSUAPI_DS_REPLICA_OBJECT_FROM_MASTER;
	obj->object.attribute_ctr.num_attributes = obj->meta_data_ctr->count;
	obj->object.attribute_ctr.attributes = talloc_array(obj, struct drsuapi_DsReplicaAttribute,
							    obj->object.attribute_ctr.num_attributes);

	/*
	 * Note that the meta_data array and the attributes array must
	 * be the same size and in the same order
	 */
	for (i=0; i<obj->object.attribute_ctr.num_attributes; i++) {
		struct ldb_message_element *el;
		WERROR werr;
		const struct dsdb_attribute *sa;
	
		sa = dsdb_attribute_by_attributeID_id(schema, attids[i]);
		if (!sa) {
			DEBUG(0,("Unable to find attributeID %u in schema\n", attids[i]));
			return WERR_DS_DRA_INTERNAL_ERROR;
		}

		el = ldb_msg_find_element(msg, sa->lDAPDisplayName);
		if (el == NULL) {
			/* this happens for attributes that have been removed */
			DEBUG(5,("No element '%s' for attributeID %u in message\n",
				 sa->lDAPDisplayName, attids[i]));
			ZERO_STRUCT(obj->object.attribute_ctr.attributes[i]);
			obj->object.attribute_ctr.attributes[i].attid =
					dsdb_attribute_get_attid(sa, syntax_ctx.is_schema_nc);
		} else {
			werr = sa->syntax->ldb_to_drsuapi(&syntax_ctx, sa, el, obj,
			                                  &obj->object.attribute_ctr.attributes[i]);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("Unable to convert %s to DRS object - %s\n", 
					 sa->lDAPDisplayName, win_errstr(werr)));
				return werr;
			}
			/* if DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING is set
			 * check if attribute is secret and send a null value
			 */
			if (replica_flags & DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING) {
				drsuapi_process_secret_attribute(&obj->object.attribute_ctr.attributes[i],
								 &obj->meta_data_ctr->meta_data[i]);
			}
			/* some attributes needs to be encrypted
			   before being sent */
			werr = drsuapi_encrypt_attribute(obj, session_key, rid, 
							 &obj->object.attribute_ctr.attributes[i]);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("Unable to encrypt %s in DRS object - %s\n", 
					 sa->lDAPDisplayName, win_errstr(werr)));
				return werr;
			}
		}
	}

	return WERR_OK;
}


/*
  add one linked attribute from an object to the list of linked
  attributes in a getncchanges request
 */
static WERROR get_nc_changes_add_la(TALLOC_CTX *mem_ctx,
				    struct ldb_context *sam_ctx,
				    const struct dsdb_schema *schema,
				    const struct dsdb_attribute *sa,
				    struct ldb_message *msg,
				    struct dsdb_dn *dsdb_dn,
				    struct drsuapi_DsReplicaLinkedAttribute **la_list,
				    uint32_t *la_count)
{
	struct drsuapi_DsReplicaLinkedAttribute *la;
	bool active;
	NTSTATUS status;
	WERROR werr;

	(*la_list) = talloc_realloc(mem_ctx, *la_list, struct drsuapi_DsReplicaLinkedAttribute, (*la_count)+1);
	W_ERROR_HAVE_NO_MEMORY(*la_list);

	la = &(*la_list)[*la_count];

	la->identifier = get_object_identifier(*la_list, msg);
	W_ERROR_HAVE_NO_MEMORY(la->identifier);

	active = (dsdb_dn_rmd_flags(dsdb_dn->dn) & DSDB_RMD_FLAG_DELETED) == 0;

	la->attid = sa->attributeID_id;
	la->flags = active?DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE:0;

	status = dsdb_get_extended_dn_nttime(dsdb_dn->dn, &la->originating_add_time, "RMD_ADDTIME");
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	status = dsdb_get_extended_dn_uint32(dsdb_dn->dn, &la->meta_data.version, "RMD_VERSION");
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	status = dsdb_get_extended_dn_nttime(dsdb_dn->dn, &la->meta_data.originating_change_time, "RMD_CHANGETIME");
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	status = dsdb_get_extended_dn_guid(dsdb_dn->dn, &la->meta_data.originating_invocation_id, "RMD_INVOCID");
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}
	status = dsdb_get_extended_dn_uint64(dsdb_dn->dn, &la->meta_data.originating_usn, "RMD_ORIGINATING_USN");
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	werr = dsdb_dn_la_to_blob(sam_ctx, sa, schema, *la_list, dsdb_dn, &la->value.blob);
	W_ERROR_NOT_OK_RETURN(werr);

	(*la_count)++;
	return WERR_OK;
}


/*
  add linked attributes from an object to the list of linked
  attributes in a getncchanges request
 */
static WERROR get_nc_changes_add_links(struct ldb_context *sam_ctx,
				       TALLOC_CTX *mem_ctx,
				       struct ldb_dn *ncRoot_dn,
				       struct dsdb_schema *schema,
				       uint64_t highest_usn,
				       uint32_t replica_flags,
				       struct ldb_message *msg,
				       struct drsuapi_DsReplicaLinkedAttribute **la_list,
				       uint32_t *la_count,
				       struct drsuapi_DsReplicaCursorCtrEx *uptodateness_vector)
{
	unsigned int i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	uint64_t uSNChanged = ldb_msg_find_attr_as_int(msg, "uSNChanged", -1);

	for (i=0; i<msg->num_elements; i++) {
		struct ldb_message_element *el = &msg->elements[i];
		const struct dsdb_attribute *sa;
		unsigned int j;

		sa = dsdb_attribute_by_lDAPDisplayName(schema, el->name);

		if (!sa || sa->linkID == 0 || (sa->linkID & 1)) {
			/* we only want forward links */
			continue;
		}

		if (el->num_values && !dsdb_dn_is_upgraded_link_val(&el->values[0])) {
			/* its an old style link, it will have been
			 * sent in the main replication data */
			continue;
		}

		for (j=0; j<el->num_values; j++) {
			struct dsdb_dn *dsdb_dn;
			uint64_t local_usn;
			NTSTATUS status;
			WERROR werr;

			dsdb_dn = dsdb_dn_parse(tmp_ctx, sam_ctx, &el->values[j], sa->syntax->ldap_oid);
			if (dsdb_dn == NULL) {
				DEBUG(1,(__location__ ": Failed to parse DN for %s in %s\n",
					 el->name, ldb_dn_get_linearized(msg->dn)));
				talloc_free(tmp_ctx);
				return WERR_DS_DRA_INTERNAL_ERROR;
			}

			status = dsdb_get_extended_dn_uint64(dsdb_dn->dn, &local_usn, "RMD_LOCAL_USN");
			if (!NT_STATUS_IS_OK(status)) {
				/* this can happen for attributes
				   given to us with old style meta
				   data */
				continue;
			}

			if (local_usn > uSNChanged) {
				DEBUG(1,(__location__ ": uSNChanged less than RMD_LOCAL_USN for %s on %s\n",
					 el->name, ldb_dn_get_linearized(msg->dn)));
				talloc_free(tmp_ctx);
				return WERR_DS_DRA_INTERNAL_ERROR;
			}

			if (local_usn < highest_usn) {
				continue;
			}

			werr = get_nc_changes_add_la(mem_ctx, sam_ctx, schema, sa, msg,
						     dsdb_dn, la_list, la_count);
			if (!W_ERROR_IS_OK(werr)) {
				talloc_free(tmp_ctx);
				return werr;
			}
		}
	}

	talloc_free(tmp_ctx);
	return WERR_OK;
}

/*
  fill in the cursors return based on the replUpToDateVector for the ncRoot_dn
 */
static WERROR get_nc_changes_udv(struct ldb_context *sam_ctx,
				 struct ldb_dn *ncRoot_dn,
				 struct drsuapi_DsReplicaCursor2CtrEx *udv)
{
	int ret;

	udv->version = 2;
	udv->reserved1 = 0;
	udv->reserved2 = 0;

	ret = dsdb_load_udv_v2(sam_ctx, ncRoot_dn, udv, &udv->cursors, &udv->count);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to load UDV for %s - %s\n",
			 ldb_dn_get_linearized(ncRoot_dn), ldb_errstring(sam_ctx)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	
	return WERR_OK;
}


/* comparison function for linked attributes - see CompareLinks() in
 * MS-DRSR section 4.1.10.5.17 */
static int linked_attribute_compare(const struct drsuapi_DsReplicaLinkedAttribute *la1,
				    const struct drsuapi_DsReplicaLinkedAttribute *la2,
				    struct ldb_context *sam_ctx)
{
	int c;
	WERROR werr;
	TALLOC_CTX *tmp_ctx;
	const struct dsdb_schema *schema;
	const struct dsdb_attribute *schema_attrib;
	struct dsdb_dn *dn1, *dn2;
	struct GUID guid1, guid2;
	NTSTATUS status;

	c = GUID_compare(&la1->identifier->guid,
			 &la2->identifier->guid);
	if (c != 0) return c;

	if (la1->attid != la2->attid) {
		return la1->attid < la2->attid? -1:1;
	}

	if ((la1->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE) !=
	    (la2->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE)) {
		return (la1->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE)? 1:-1;
	}

	/* we need to get the target GUIDs to compare */
	tmp_ctx = talloc_new(sam_ctx);

	schema = dsdb_get_schema(sam_ctx, tmp_ctx);
	schema_attrib = dsdb_attribute_by_attributeID_id(schema, la1->attid);

	werr = dsdb_dn_la_from_blob(sam_ctx, schema_attrib, schema, tmp_ctx, la1->value.blob, &dn1);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Bad la1 blob in sort\n"));
		talloc_free(tmp_ctx);
		return 0;
	}

	werr = dsdb_dn_la_from_blob(sam_ctx, schema_attrib, schema, tmp_ctx, la2->value.blob, &dn2);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Bad la2 blob in sort\n"));
		talloc_free(tmp_ctx);
		return 0;
	}

	status = dsdb_get_extended_dn_guid(dn1->dn, &guid1, "GUID");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ ": Bad la1 guid in sort\n"));
		talloc_free(tmp_ctx);
		return 0;
	}
	status = dsdb_get_extended_dn_guid(dn2->dn, &guid2, "GUID");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ ": Bad la2 guid in sort\n"));
		talloc_free(tmp_ctx);
		return 0;
	}

	talloc_free(tmp_ctx);

	return GUID_compare(&guid1, &guid2);
}


/*
  sort the objects we send by tree order
 */
static int site_res_cmp_parent_order(struct ldb_message **m1, struct ldb_message **m2)
{
	return ldb_dn_compare((*m2)->dn, (*m1)->dn);
}

/*
  sort the objects we send first by uSNChanged
 */
static int site_res_cmp_usn_order(struct ldb_message **m1, struct ldb_message **m2)
{
	unsigned usnchanged1, usnchanged2;
	unsigned cn1, cn2;
	cn1 = ldb_dn_get_comp_num((*m1)->dn);
	cn2 = ldb_dn_get_comp_num((*m2)->dn);
	if (cn1 != cn2) {
		return cn1 > cn2 ? 1 : -1;
	}
	usnchanged1 = ldb_msg_find_attr_as_uint(*m1, "uSNChanged", 0);
	usnchanged2 = ldb_msg_find_attr_as_uint(*m2, "uSNChanged", 0);
	if (usnchanged1 == usnchanged2) {
		return 0;
	}
	return usnchanged1 > usnchanged2 ? 1 : -1;
}


/*
  handle a DRSUAPI_EXOP_FSMO_RID_ALLOC call
 */
static WERROR getncchanges_rid_alloc(struct drsuapi_bind_state *b_state,
				     TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsGetNCChangesRequest10 *req10,
				     struct drsuapi_DsGetNCChangesCtr6 *ctr6)
{
	struct ldb_dn *rid_manager_dn, *fsmo_role_dn, *req_dn;
	int ret;
	struct ldb_context *ldb = b_state->sam_ctx;
	struct ldb_result *ext_res;
	struct ldb_dn *base_dn;
	struct dsdb_fsmo_extended_op *exop;

	/*
	  steps:
	    - verify that the DN being asked for is the RID Manager DN
	    - verify that we are the RID Manager
	 */

	/* work out who is the RID Manager */
	ret = samdb_rid_manager_dn(ldb, mem_ctx, &rid_manager_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, (__location__ ": Failed to find RID Manager object - %s\n", ldb_errstring(ldb)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	req_dn = drs_ObjectIdentifier_to_dn(mem_ctx, ldb, req10->naming_context);
	if (!ldb_dn_validate(req_dn) ||
	    ldb_dn_compare(req_dn, rid_manager_dn) != 0) {
		/* that isn't the RID Manager DN */
		DEBUG(0,(__location__ ": RID Alloc request for wrong DN %s\n",
			 drs_ObjectIdentifier_to_string(mem_ctx, req10->naming_context)));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_MISMATCH;
		return WERR_OK;
	}

	/* find the DN of the RID Manager */
	ret = samdb_reference_dn(ldb, mem_ctx, rid_manager_dn, "fSMORoleOwner", &fsmo_role_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in RID Manager object - %s\n",
			 ldb_errstring(ldb)));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_FSMO_NOT_OWNER;
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), fsmo_role_dn) != 0) {
		/* we're not the RID Manager - go away */
		DEBUG(0,(__location__ ": RID Alloc request when not RID Manager\n"));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_FSMO_NOT_OWNER;
		return WERR_OK;
	}

	exop = talloc(mem_ctx, struct dsdb_fsmo_extended_op);
	W_ERROR_HAVE_NO_MEMORY(exop);

	exop->fsmo_info = req10->fsmo_info;
	exop->destination_dsa_guid = req10->destination_dsa_guid;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed transaction start - %s\n",
			 ldb_errstring(ldb)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	/*
	 * FIXME (kim): this is a temp hack to return just few object,
	 * but not the whole domain NC.
	 * We should remove this hack and implement a 'scope'
	 * building function to return just the set of object
	 * documented for DRSUAPI_EXOP_FSMO_RID_ALLOC extended_op
	 */
	ldb_sequence_number(ldb, LDB_SEQ_HIGHEST_SEQ, &req10->highwatermark.highest_usn);

	ret = ldb_extended(ldb, DSDB_EXTENDED_ALLOCATE_RID_POOL, exop, &ext_res);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed extended allocation RID pool operation - %s\n",
			 ldb_errstring(ldb)));
		ldb_transaction_cancel(ldb);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed transaction commit - %s\n",
			 ldb_errstring(ldb)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	talloc_free(ext_res);

	base_dn = ldb_get_default_basedn(ldb);

	DEBUG(2,("Allocated RID pool for server %s\n",
		 GUID_string(mem_ctx, &req10->destination_dsa_guid)));

	ctr6->extended_ret = DRSUAPI_EXOP_ERR_SUCCESS;

	return WERR_OK;
}

/*
  return an array of SIDs from a ldb_message given an attribute name
  assumes the SIDs are in extended DN format
 */
static WERROR samdb_result_sid_array_dn(struct ldb_context *sam_ctx,
					struct ldb_message *msg,
					TALLOC_CTX *mem_ctx,
					const char *attr,
					const struct dom_sid ***sids)
{
	struct ldb_message_element *el;
	unsigned int i;

	el = ldb_msg_find_element(msg, attr);
	if (!el) {
		*sids = NULL;
		return WERR_OK;
	}

	(*sids) = talloc_array(mem_ctx, const struct dom_sid *, el->num_values + 1);
	W_ERROR_HAVE_NO_MEMORY(*sids);

	for (i=0; i<el->num_values; i++) {
		struct ldb_dn *dn = ldb_dn_from_ldb_val(mem_ctx, sam_ctx, &el->values[i]);
		NTSTATUS status;
		struct dom_sid *sid;

		sid = talloc(*sids, struct dom_sid);
		W_ERROR_HAVE_NO_MEMORY(sid);
		status = dsdb_get_extended_dn_sid(dn, sid, "SID");
		if (!NT_STATUS_IS_OK(status)) {
			return WERR_INTERNAL_DB_CORRUPTION;
		}
		(*sids)[i] = sid;
	}
	(*sids)[i] = NULL;

	return WERR_OK;
}


/*
  return an array of SIDs from a ldb_message given an attribute name
  assumes the SIDs are in NDR form
 */
static WERROR samdb_result_sid_array_ndr(struct ldb_context *sam_ctx,
					 struct ldb_message *msg,
					 TALLOC_CTX *mem_ctx,
					 const char *attr,
					 const struct dom_sid ***sids)
{
	struct ldb_message_element *el;
	unsigned int i;

	el = ldb_msg_find_element(msg, attr);
	if (!el) {
		*sids = NULL;
		return WERR_OK;
	}

	(*sids) = talloc_array(mem_ctx, const struct dom_sid *, el->num_values + 1);
	W_ERROR_HAVE_NO_MEMORY(*sids);

	for (i=0; i<el->num_values; i++) {
		enum ndr_err_code ndr_err;
		struct dom_sid *sid;

		sid = talloc(*sids, struct dom_sid);
		W_ERROR_HAVE_NO_MEMORY(sid);

		ndr_err = ndr_pull_struct_blob(&el->values[i], sid, sid,
					       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return WERR_INTERNAL_DB_CORRUPTION;
		}
		(*sids)[i] = sid;
	}
	(*sids)[i] = NULL;

	return WERR_OK;
}

/*
  see if any SIDs in list1 are in list2
 */
static bool sid_list_match(const struct dom_sid **list1, const struct dom_sid **list2)
{
	unsigned int i, j;
	/* do we ever have enough SIDs here to worry about O(n^2) ? */
	for (i=0; list1[i]; i++) {
		for (j=0; list2[j]; j++) {
			if (dom_sid_equal(list1[i], list2[j])) {
				return true;
			}
		}
	}
	return false;
}

/*
  handle a DRSUAPI_EXOP_REPL_SECRET call
 */
static WERROR getncchanges_repl_secret(struct drsuapi_bind_state *b_state,
				       TALLOC_CTX *mem_ctx,
				       struct drsuapi_DsGetNCChangesRequest10 *req10,
				       struct dom_sid *user_sid,
				       struct drsuapi_DsGetNCChangesCtr6 *ctr6)
{
	struct drsuapi_DsReplicaObjectIdentifier *ncRoot = req10->naming_context;
	struct ldb_dn *obj_dn, *rodc_dn, *krbtgt_link_dn;
	int ret;
	const char *rodc_attrs[] = { "msDS-KrbTgtLink", "msDS-NeverRevealGroup", "msDS-RevealOnDemandGroup", NULL };
	const char *obj_attrs[] = { "tokenGroups", "objectSid", "UserAccountControl", "msDS-KrbTgtLinkBL", NULL };
	struct ldb_result *rodc_res, *obj_res;
	const struct dom_sid **never_reveal_sids, **reveal_sids, **token_sids;
	WERROR werr;

	DEBUG(3,(__location__ ": DRSUAPI_EXOP_REPL_SECRET extended op on %s\n",
		 drs_ObjectIdentifier_to_string(mem_ctx, ncRoot)));

	/*
	 * we need to work out if we will allow this RODC to
	 * replicate the secrets for this object
	 *
	 * see 4.1.10.5.14 GetRevealSecretsPolicyForUser for details
	 * of this function
	 */

	if (b_state->sam_ctx_system == NULL) {
		/* this operation needs system level access */
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_ACCESS_DENIED;
		return WERR_DS_DRA_SOURCE_DISABLED;
	}

	obj_dn = drs_ObjectIdentifier_to_dn(mem_ctx, b_state->sam_ctx_system, ncRoot);
	if (!ldb_dn_validate(obj_dn)) goto failed;

	rodc_dn = ldb_dn_new_fmt(mem_ctx, b_state->sam_ctx_system, "<SID=%s>",
				 dom_sid_string(mem_ctx, user_sid));
	if (!ldb_dn_validate(rodc_dn)) goto failed;

	/* do the two searches we need */
	ret = dsdb_search_dn(b_state->sam_ctx_system, mem_ctx, &rodc_res, rodc_dn, rodc_attrs,
			     DSDB_SEARCH_SHOW_EXTENDED_DN);
	if (ret != LDB_SUCCESS || rodc_res->count != 1) goto failed;

	ret = dsdb_search_dn(b_state->sam_ctx_system, mem_ctx, &obj_res, obj_dn, obj_attrs, 0);
	if (ret != LDB_SUCCESS || obj_res->count != 1) goto failed;

	/* if the object SID is equal to the user_sid, allow */
	if (dom_sid_equal(user_sid,
			  samdb_result_dom_sid(mem_ctx, obj_res->msgs[0], "objectSid"))) {
		goto allowed;
	}

	/* an RODC is allowed to get its own krbtgt account secrets */
	krbtgt_link_dn = samdb_result_dn(b_state->sam_ctx_system, mem_ctx,
					 rodc_res->msgs[0], "msDS-KrbTgtLink", NULL);
	if (krbtgt_link_dn != NULL &&
	    ldb_dn_compare(obj_dn, krbtgt_link_dn) == 0) {
		goto allowed;
	}

	/* but it isn't allowed to get anyone elses krbtgt secrets */
	if (samdb_result_dn(b_state->sam_ctx_system, mem_ctx,
			    obj_res->msgs[0], "msDS-KrbTgtLinkBL", NULL)) {
		goto denied;
	}

	if (ldb_msg_find_attr_as_uint(obj_res->msgs[0],
				      "userAccountControl", 0) &
	    UF_INTERDOMAIN_TRUST_ACCOUNT) {
		goto denied;
	}

	werr = samdb_result_sid_array_dn(b_state->sam_ctx_system, rodc_res->msgs[0],
					 mem_ctx, "msDS-NeverRevealGroup", &never_reveal_sids);
	if (!W_ERROR_IS_OK(werr)) {
		goto denied;
	}

	werr = samdb_result_sid_array_dn(b_state->sam_ctx_system, rodc_res->msgs[0],
					 mem_ctx, "msDS-RevealOnDemandGroup", &reveal_sids);
	if (!W_ERROR_IS_OK(werr)) {
		goto denied;
	}

	werr = samdb_result_sid_array_ndr(b_state->sam_ctx_system, obj_res->msgs[0],
					 mem_ctx, "tokenGroups", &token_sids);
	if (!W_ERROR_IS_OK(werr) || token_sids==NULL) {
		goto denied;
	}

	if (never_reveal_sids &&
	    sid_list_match(token_sids, never_reveal_sids)) {
		goto denied;
	}

	if (reveal_sids &&
	    sid_list_match(token_sids, reveal_sids)) {
		goto allowed;
	}

	/* default deny */
denied:
	DEBUG(2,(__location__ ": Denied RODC secret replication for %s by RODC %s\n",
		 ldb_dn_get_linearized(obj_dn), ldb_dn_get_linearized(rodc_res->msgs[0]->dn)));
	ctr6->extended_ret = DRSUAPI_EXOP_ERR_NONE;
	return WERR_DS_DRA_ACCESS_DENIED;

allowed:
	DEBUG(2,(__location__ ": Allowed RODC secret replication for %s by RODC %s\n",
		 ldb_dn_get_linearized(obj_dn), ldb_dn_get_linearized(rodc_res->msgs[0]->dn)));
	ctr6->extended_ret = DRSUAPI_EXOP_ERR_SUCCESS;
	req10->highwatermark.highest_usn = 0;
	return WERR_OK;

failed:
	DEBUG(2,(__location__ ": Failed RODC secret replication for %s by RODC %s\n",
		 ldb_dn_get_linearized(obj_dn), dom_sid_string(mem_ctx, user_sid)));
	ctr6->extended_ret = DRSUAPI_EXOP_ERR_NONE;
	return WERR_DS_DRA_BAD_DN;
}


/*
  handle a DRSUAPI_EXOP_REPL_OBJ call
 */
static WERROR getncchanges_repl_obj(struct drsuapi_bind_state *b_state,
				    TALLOC_CTX *mem_ctx,
				    struct drsuapi_DsGetNCChangesRequest10 *req10,
				    struct dom_sid *user_sid,
				    struct drsuapi_DsGetNCChangesCtr6 *ctr6)
{
	struct drsuapi_DsReplicaObjectIdentifier *ncRoot = req10->naming_context;

	DEBUG(3,(__location__ ": DRSUAPI_EXOP_REPL_OBJ extended op on %s\n",
		 drs_ObjectIdentifier_to_string(mem_ctx, ncRoot)));

	ctr6->extended_ret = DRSUAPI_EXOP_ERR_SUCCESS;
	req10->highwatermark.highest_usn = 0;
	return WERR_OK;
}


/*
  handle DRSUAPI_EXOP_FSMO_REQ_ROLE,
  DRSUAPI_EXOP_FSMO_RID_REQ_ROLE,
  and DRSUAPI_EXOP_FSMO_REQ_PDC calls
 */
static WERROR getncchanges_change_master(struct drsuapi_bind_state *b_state,
					 TALLOC_CTX *mem_ctx,
					 struct drsuapi_DsGetNCChangesRequest10 *req10,
					 struct drsuapi_DsGetNCChangesCtr6 *ctr6)
{
	struct ldb_dn *fsmo_role_dn, *req_dn, *ntds_dn;
	int ret;
	unsigned int i;
	struct ldb_context *ldb = b_state->sam_ctx;
	struct ldb_message *msg;

	/*
	  steps:
	    - verify that the client dn exists
	    - verify that we are the current master
	 */

	req_dn = drs_ObjectIdentifier_to_dn(mem_ctx, ldb, req10->naming_context);
	if (!ldb_dn_validate(req_dn)) {
		/* that is not a valid dn */
		DEBUG(0,(__location__ ": FSMO role transfer request for invalid DN %s\n",
			 drs_ObjectIdentifier_to_string(mem_ctx, req10->naming_context)));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_MISMATCH;
		return WERR_OK;
	}

	/* retrieve the current role owner */
	ret = samdb_reference_dn(ldb, mem_ctx, req_dn, "fSMORoleOwner", &fsmo_role_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in context - %s\n",
			 ldb_errstring(ldb)));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_FSMO_NOT_OWNER;
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), fsmo_role_dn) != 0) {
		/* we're not the current owner - go away */
		DEBUG(0,(__location__ ": FSMO transfer request when not owner\n"));
		ctr6->extended_ret = DRSUAPI_EXOP_ERR_FSMO_NOT_OWNER;
		return WERR_OK;
	}

	/* change the current master */
	msg = ldb_msg_new(ldb);
	W_ERROR_HAVE_NO_MEMORY(msg);
	msg->dn = drs_ObjectIdentifier_to_dn(msg, ldb, req10->naming_context);
	W_ERROR_HAVE_NO_MEMORY(msg->dn);

	ret = dsdb_find_dn_by_guid(ldb, msg, &req10->destination_dsa_guid, &ntds_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, (__location__ ": Unable to find NTDS object for guid %s - %s\n",
			  GUID_string(mem_ctx, &req10->destination_dsa_guid), ldb_errstring(ldb)));
		talloc_free(msg);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	ret = ldb_msg_add_string(msg, "fSMORoleOwner", ldb_dn_get_linearized(ntds_dn));
	if (ret != 0) {
		talloc_free(msg);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed transaction start - %s\n",
			 ldb_errstring(ldb)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	ret = ldb_modify(ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to change current owner - %s\n",
			 ldb_errstring(ldb)));
		ldb_transaction_cancel(ldb);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed transaction commit - %s\n",
			 ldb_errstring(ldb)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	ctr6->extended_ret = DRSUAPI_EXOP_ERR_SUCCESS;

	return WERR_OK;
}

/* state of a partially completed getncchanges call */
struct drsuapi_getncchanges_state {
	struct GUID *guids;
	uint32_t num_records;
	uint32_t num_processed;
	struct ldb_dn *ncRoot_dn;
	bool is_schema_nc;
	uint64_t min_usn;
	uint64_t highest_usn;
	struct ldb_dn *last_dn;
	struct drsuapi_DsReplicaLinkedAttribute *la_list;
	uint32_t la_count;
	bool la_sorted;
	uint32_t la_idx;
	struct drsuapi_DsReplicaCursorCtrEx *uptodateness_vector;
};

/*
  see if this getncchanges request includes a request to reveal secret information
 */
static WERROR dcesrv_drsuapi_is_reveal_secrets_request(struct drsuapi_bind_state *b_state,
						       struct drsuapi_DsGetNCChangesRequest10 *req10,
						       bool *is_secret_request)
{
	enum drsuapi_DsExtendedOperation exop;
	uint32_t i;
	struct dsdb_schema *schema;

	*is_secret_request = true;

	exop = req10->extended_op;

	switch (exop) {
	case DRSUAPI_EXOP_FSMO_REQ_ROLE:
	case DRSUAPI_EXOP_FSMO_RID_ALLOC:
	case DRSUAPI_EXOP_FSMO_RID_REQ_ROLE:
	case DRSUAPI_EXOP_FSMO_REQ_PDC:
	case DRSUAPI_EXOP_FSMO_ABANDON_ROLE:
		/* FSMO exops can reveal secrets */
		*is_secret_request = true;
		return WERR_OK;
	case DRSUAPI_EXOP_REPL_SECRET:
	case DRSUAPI_EXOP_REPL_OBJ:
	case DRSUAPI_EXOP_NONE:
		break;
	}

	if (req10->replica_flags & DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING) {
		*is_secret_request = false;
		return WERR_OK;
	}

	if (exop == DRSUAPI_EXOP_REPL_SECRET ||
	    req10->partial_attribute_set == NULL) {
		/* they want secrets */
		*is_secret_request = true;
		return WERR_OK;
	}

	schema = dsdb_get_schema(b_state->sam_ctx, NULL);

	/* check the attributes they asked for */
	for (i=0; i<req10->partial_attribute_set->num_attids; i++) {
		const struct dsdb_attribute *sa;
		sa = dsdb_attribute_by_attributeID_id(schema, req10->partial_attribute_set->attids[i]);
		if (sa == NULL) {
			return WERR_DS_DRA_SCHEMA_MISMATCH;
		}
		if (!dsdb_attr_in_rodc_fas(sa)) {
			*is_secret_request = true;
			return WERR_OK;
		}
	}

	/* check the attributes they asked for */
	for (i=0; i<req10->partial_attribute_set_ex->num_attids; i++) {
		const struct dsdb_attribute *sa;
		sa = dsdb_attribute_by_attributeID_id(schema, req10->partial_attribute_set_ex->attids[i]);
		if (sa == NULL) {
			return WERR_DS_DRA_SCHEMA_MISMATCH;
		}
		if (!dsdb_attr_in_rodc_fas(sa)) {
			*is_secret_request = true;
			return WERR_OK;
		}
	}

	*is_secret_request = false;
	return WERR_OK;
}


/*
  map from req8 to req10
 */
static struct drsuapi_DsGetNCChangesRequest10 *
getncchanges_map_req8(TALLOC_CTX *mem_ctx,
		      struct drsuapi_DsGetNCChangesRequest8 *req8)
{
	struct drsuapi_DsGetNCChangesRequest10 *req10 = talloc_zero(mem_ctx,
								    struct drsuapi_DsGetNCChangesRequest10);
	if (req10 == NULL) {
		return NULL;
	}

	req10->destination_dsa_guid = req8->destination_dsa_guid;
	req10->source_dsa_invocation_id = req8->source_dsa_invocation_id;
	req10->naming_context = req8->naming_context;
	req10->highwatermark = req8->highwatermark;
	req10->uptodateness_vector = req8->uptodateness_vector;
	req10->replica_flags = req8->replica_flags;
	req10->max_object_count = req8->max_object_count;
	req10->max_ndr_size = req8->max_ndr_size;
	req10->extended_op = req8->extended_op;
	req10->fsmo_info = req8->fsmo_info;
	req10->partial_attribute_set = req8->partial_attribute_set;
	req10->partial_attribute_set_ex = req8->partial_attribute_set_ex;
	req10->mapping_ctr = req8->mapping_ctr;

	return req10;
}


/* 
  drsuapi_DsGetNCChanges

  see MS-DRSR 4.1.10.5.2 for basic logic of this function
*/
WERROR dcesrv_drsuapi_DsGetNCChanges(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsGetNCChanges *r)
{
	struct drsuapi_DsReplicaObjectIdentifier *ncRoot;
	int ret;
	uint32_t i;
	struct dsdb_schema *schema;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;
	struct drsuapi_DsReplicaObjectListItemEx **currentObject;
	NTSTATUS status;
	DATA_BLOB session_key;
	const char *attrs[] = { "uSNChanged",
				"objectGUID" ,
				NULL };
	WERROR werr;
	struct dcesrv_handle *h;
	struct drsuapi_bind_state *b_state;	
	struct drsuapi_getncchanges_state *getnc_state;
	struct drsuapi_DsGetNCChangesRequest10 *req10;
	uint32_t options;
	uint32_t max_objects;
	uint32_t max_links;
	uint32_t link_count = 0;
	uint32_t link_total = 0;
	uint32_t link_given = 0;
	struct ldb_dn *search_dn = NULL;
	bool am_rodc, null_scope=false;
	enum security_user_level security_level;
	struct ldb_context *sam_ctx;
	struct dom_sid *user_sid;
	bool is_secret_request;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	sam_ctx = b_state->sam_ctx_system?b_state->sam_ctx_system:b_state->sam_ctx;

	*r->out.level_out = 6;
	/* TODO: linked attributes*/
	r->out.ctr->ctr6.linked_attributes_count = 0;
	r->out.ctr->ctr6.linked_attributes = NULL;

	r->out.ctr->ctr6.object_count = 0;
	r->out.ctr->ctr6.nc_object_count = 0;
	r->out.ctr->ctr6.more_data = false;
	r->out.ctr->ctr6.uptodateness_vector = NULL;

	/* a RODC doesn't allow for any replication */
	ret = samdb_rodc(sam_ctx, &am_rodc);
	if (ret == LDB_SUCCESS && am_rodc) {
		DEBUG(0,(__location__ ": DsGetNCChanges attempt on RODC\n"));
		return WERR_DS_DRA_SOURCE_DISABLED;
	}

	/* Check request revision. 
	 */
	switch (r->in.level) {
	case 8:
		req10 = getncchanges_map_req8(mem_ctx, &r->in.req->req8);
		if (req10 == NULL) {
			return WERR_NOMEM;
		}
		break;
	case 10:
		req10 = &r->in.req->req10;
		break;
	default:
		DEBUG(0,(__location__ ": Request for DsGetNCChanges with unsupported level %u\n",
			 r->in.level));
		return WERR_REVISION_MISMATCH;
	}


        /* Perform access checks. */
	/* TODO: we need to support a sync on a specific non-root
	 * DN. We'll need to find the real partition root here */
	ncRoot = req10->naming_context;
	if (ncRoot == NULL) {
		DEBUG(0,(__location__ ": Request for DsGetNCChanges with no NC\n"));
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	if (samdb_ntds_options(sam_ctx, &options) != LDB_SUCCESS) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	
	if ((options & DS_NTDSDSA_OPT_DISABLE_OUTBOUND_REPL) &&
	    !(req10->replica_flags & DRSUAPI_DRS_SYNC_FORCED)) {
		return WERR_DS_DRA_SOURCE_DISABLED;
	}

	user_sid = &dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];

	werr = drs_security_access_check_nc_root(b_state->sam_ctx,
						 mem_ctx,
						 dce_call->conn->auth_state.session_info->security_token,
						 req10->naming_context,
						 GUID_DRS_GET_CHANGES);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	werr = dcesrv_drsuapi_is_reveal_secrets_request(b_state, req10, &is_secret_request);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}
	if (is_secret_request && req10->extended_op != DRSUAPI_EXOP_REPL_SECRET) {
		werr = drs_security_access_check_nc_root(b_state->sam_ctx,
							 mem_ctx,
							 dce_call->conn->auth_state.session_info->security_token,
							 req10->naming_context,
							 GUID_DRS_GET_ALL_CHANGES);
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}
	}

	/* for non-administrator replications, check that they have
	   given the correct source_dsa_invocation_id */
	security_level = security_session_user_level(dce_call->conn->auth_state.session_info,
						     samdb_domain_sid(sam_ctx));
	if (security_level == SECURITY_RO_DOMAIN_CONTROLLER) {
		if (req10->replica_flags & DRSUAPI_DRS_WRIT_REP) {
			/* we rely on this flag being unset for RODC requests */
			req10->replica_flags &= ~DRSUAPI_DRS_WRIT_REP;
		}
	}

	if (req10->replica_flags & DRSUAPI_DRS_FULL_SYNC_PACKET) {
		/* Ignore the _in_ uptpdateness vector*/
		req10->uptodateness_vector = NULL;
	} 

	getnc_state = b_state->getncchanges_state;

	/* see if a previous replication has been abandoned */
	if (getnc_state) {
		struct ldb_dn *new_dn = drs_ObjectIdentifier_to_dn(getnc_state, sam_ctx, ncRoot);
		if (ldb_dn_compare(new_dn, getnc_state->ncRoot_dn) != 0) {
			DEBUG(0,(__location__ ": DsGetNCChanges 2nd replication on different DN %s %s (last_dn %s)\n",
				 ldb_dn_get_linearized(new_dn),
				 ldb_dn_get_linearized(getnc_state->ncRoot_dn),
				 ldb_dn_get_linearized(getnc_state->last_dn)));
			talloc_free(getnc_state);
			getnc_state = NULL;
		}
	}

	if (getnc_state == NULL) {
		getnc_state = talloc_zero(b_state, struct drsuapi_getncchanges_state);
		if (getnc_state == NULL) {
			return WERR_NOMEM;
		}
		b_state->getncchanges_state = getnc_state;
		getnc_state->ncRoot_dn = drs_ObjectIdentifier_to_dn(getnc_state, sam_ctx, ncRoot);

		/* find out if we are to replicate Schema NC */
		ret = ldb_dn_compare(getnc_state->ncRoot_dn,
				     ldb_get_schema_basedn(b_state->sam_ctx));
		getnc_state->is_schema_nc = (0 == ret);

		/*
		 * This is the first replication cycle and it is
		 * a good place to handle extended operations
		 *
		 * FIXME: we don't fully support extended operations yet
		 */
		switch (req10->extended_op) {
		case DRSUAPI_EXOP_NONE:
			break;
		case DRSUAPI_EXOP_FSMO_RID_ALLOC:
			werr = getncchanges_rid_alloc(b_state, mem_ctx, req10, &r->out.ctr->ctr6);
			W_ERROR_NOT_OK_RETURN(werr);
			search_dn = ldb_get_default_basedn(sam_ctx);
			break;
		case DRSUAPI_EXOP_REPL_SECRET:
			werr = getncchanges_repl_secret(b_state, mem_ctx, req10, user_sid, &r->out.ctr->ctr6);
			r->out.result = werr;
			W_ERROR_NOT_OK_RETURN(werr);
			break;
		case DRSUAPI_EXOP_FSMO_REQ_ROLE:
			werr = getncchanges_change_master(b_state, mem_ctx, req10, &r->out.ctr->ctr6);
			W_ERROR_NOT_OK_RETURN(werr);
			break;
		case DRSUAPI_EXOP_FSMO_RID_REQ_ROLE:
			werr = getncchanges_change_master(b_state, mem_ctx, req10, &r->out.ctr->ctr6);
			W_ERROR_NOT_OK_RETURN(werr);
			break;
		case DRSUAPI_EXOP_FSMO_REQ_PDC:
			werr = getncchanges_change_master(b_state, mem_ctx, req10, &r->out.ctr->ctr6);
			W_ERROR_NOT_OK_RETURN(werr);
			break;
		case DRSUAPI_EXOP_REPL_OBJ:
			werr = getncchanges_repl_obj(b_state, mem_ctx, req10, user_sid, &r->out.ctr->ctr6);
			r->out.result = werr;
			W_ERROR_NOT_OK_RETURN(werr);
			break;

		case DRSUAPI_EXOP_FSMO_ABANDON_ROLE:

			DEBUG(0,(__location__ ": Request for DsGetNCChanges unsupported extended op 0x%x\n",
				 (unsigned)req10->extended_op));
			return WERR_DS_DRA_NOT_SUPPORTED;
		}
	}

	if (!ldb_dn_validate(getnc_state->ncRoot_dn) ||
	    ldb_dn_is_null(getnc_state->ncRoot_dn)) {
		DEBUG(0,(__location__ ": Bad DN '%s'\n",
			 drs_ObjectIdentifier_to_string(mem_ctx, ncRoot)));
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	/* we need the session key for encrypting password attributes */
	status = dcesrv_inherited_session_key(dce_call->conn, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ ": Failed to get session key\n"));
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	/* 
	   TODO: MS-DRSR section 4.1.10.1.1
	   Work out if this is the start of a new cycle */

	if (getnc_state->guids == NULL) {
		char* search_filter;
		enum ldb_scope scope = LDB_SCOPE_SUBTREE;
		const char *extra_filter;
		struct ldb_result *search_res;

		if (req10->extended_op == DRSUAPI_EXOP_REPL_OBJ ||
		    req10->extended_op == DRSUAPI_EXOP_REPL_SECRET) {
			scope = LDB_SCOPE_BASE;
		}

		extra_filter = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "drs", "object filter");

		getnc_state->min_usn = req10->highwatermark.highest_usn;

		/* Construct response. */
		search_filter = talloc_asprintf(mem_ctx,
						"(uSNChanged>=%llu)",
						(unsigned long long)(getnc_state->min_usn+1));
	
		if (extra_filter) {
			search_filter = talloc_asprintf(mem_ctx, "(&%s(%s))", search_filter, extra_filter);
		}

		if (req10->replica_flags & DRSUAPI_DRS_CRITICAL_ONLY) {
			search_filter = talloc_asprintf(mem_ctx,
							"(&%s(isCriticalSystemObject=TRUE))",
							search_filter);
		}
		
		if (req10->replica_flags & DRSUAPI_DRS_ASYNC_REP) {
			scope = LDB_SCOPE_BASE;
		}
		
		if (!search_dn) {
			search_dn = getnc_state->ncRoot_dn;
		}

		DEBUG(2,(__location__ ": getncchanges on %s using filter %s\n",
			 ldb_dn_get_linearized(getnc_state->ncRoot_dn), search_filter));
		ret = drsuapi_search_with_extended_dn(sam_ctx, getnc_state, &search_res,
						      search_dn, scope, attrs,
						      search_filter);
		if (ret != LDB_SUCCESS) {
			return WERR_DS_DRA_INTERNAL_ERROR;
		}

		if (req10->replica_flags & DRSUAPI_DRS_GET_ANC) {
			TYPESAFE_QSORT(search_res->msgs,
				       search_res->count,
				       site_res_cmp_parent_order);
		} else {
			TYPESAFE_QSORT(search_res->msgs,
				       search_res->count,
				       site_res_cmp_usn_order);
		}

		/* extract out the GUIDs list */
		getnc_state->num_records = search_res->count;
		getnc_state->guids = talloc_array(getnc_state, struct GUID, getnc_state->num_records);
		W_ERROR_HAVE_NO_MEMORY(getnc_state->guids);

		for (i=0; i<getnc_state->num_records; i++) {
			getnc_state->guids[i] = samdb_result_guid(search_res->msgs[i], "objectGUID");
			if (GUID_all_zero(&getnc_state->guids[i])) {
				DEBUG(2,("getncchanges: bad objectGUID from %s\n", ldb_dn_get_linearized(search_res->msgs[i]->dn)));
				return WERR_DS_DRA_INTERNAL_ERROR;
			}
		}


		talloc_free(search_res);

		getnc_state->uptodateness_vector = talloc_steal(getnc_state, req10->uptodateness_vector);
		if (getnc_state->uptodateness_vector) {
			/* make sure its sorted */
			TYPESAFE_QSORT(getnc_state->uptodateness_vector->cursors,
				       getnc_state->uptodateness_vector->count,
				       drsuapi_DsReplicaCursor_compare);
		}
	}

	/* Prefix mapping */
	schema = dsdb_get_schema(sam_ctx, mem_ctx);
	if (!schema) {
		DEBUG(0,("No schema in sam_ctx\n"));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	r->out.ctr->ctr6.naming_context = talloc(mem_ctx, struct drsuapi_DsReplicaObjectIdentifier);
	*r->out.ctr->ctr6.naming_context = *ncRoot;

	if (dsdb_find_guid_by_dn(sam_ctx, getnc_state->ncRoot_dn,
				 &r->out.ctr->ctr6.naming_context->guid) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find GUID of ncRoot_dn %s\n",
			 ldb_dn_get_linearized(getnc_state->ncRoot_dn)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	/* find the SID if there is one */
	dsdb_find_sid_by_dn(sam_ctx, getnc_state->ncRoot_dn, &r->out.ctr->ctr6.naming_context->sid);

	dsdb_get_oid_mappings_drsuapi(schema, true, mem_ctx, &ctr);
	r->out.ctr->ctr6.mapping_ctr = *ctr;

	r->out.ctr->ctr6.source_dsa_guid = *(samdb_ntds_objectGUID(sam_ctx));
	r->out.ctr->ctr6.source_dsa_invocation_id = *(samdb_ntds_invocation_id(sam_ctx));

	r->out.ctr->ctr6.old_highwatermark = req10->highwatermark;
	r->out.ctr->ctr6.new_highwatermark = req10->highwatermark;

	r->out.ctr->ctr6.first_object = NULL;
	currentObject = &r->out.ctr->ctr6.first_object;

	/* use this to force single objects at a time, which is useful
	 * for working out what object is giving problems
	 */
	max_objects = lpcfg_parm_int(dce_call->conn->dce_ctx->lp_ctx, NULL, "drs", "max object sync", 1000);
	if (req10->max_object_count < max_objects) {
		max_objects = req10->max_object_count;
	}
	/*
	 * TODO: work out how the maximum should be calculated
	 */
	max_links = lpcfg_parm_int(dce_call->conn->dce_ctx->lp_ctx, NULL, "drs", "max link sync", 1500);

	for (i=getnc_state->num_processed;
	     i<getnc_state->num_records &&
		     !null_scope &&
		     (r->out.ctr->ctr6.object_count < max_objects);
	    i++) {
		int uSN;
		struct drsuapi_DsReplicaObjectListItemEx *obj;
		struct ldb_message *msg;
		static const char * const msg_attrs[] = {
					    "*",
					    "nTSecurityDescriptor",
					    "parentGUID",
					    "replPropertyMetaData",
					    DSDB_SECRET_ATTRIBUTES,
					    NULL };
		struct ldb_result *msg_res;
		struct ldb_dn *msg_dn;

		obj = talloc_zero(mem_ctx, struct drsuapi_DsReplicaObjectListItemEx);
		W_ERROR_HAVE_NO_MEMORY(obj);

		msg_dn = ldb_dn_new_fmt(obj, sam_ctx, "<GUID=%s>", GUID_string(obj, &getnc_state->guids[i]));
		W_ERROR_HAVE_NO_MEMORY(msg_dn);


		/* by re-searching here we avoid having a lot of full
		 * records in memory between calls to getncchanges
		 */
		ret = drsuapi_search_with_extended_dn(sam_ctx, obj, &msg_res,
						      msg_dn,
						      LDB_SCOPE_BASE, msg_attrs, NULL);
		if (ret != LDB_SUCCESS) {
			if (ret != LDB_ERR_NO_SUCH_OBJECT) {
				DEBUG(1,("getncchanges: failed to fetch DN %s - %s\n",
					 ldb_dn_get_extended_linearized(obj, msg_dn, 1), ldb_errstring(sam_ctx)));
			}
			talloc_free(obj);
			continue;
		}

		msg = msg_res->msgs[0];

		werr = get_nc_changes_build_object(obj, msg,
						   sam_ctx, getnc_state->ncRoot_dn,
						   getnc_state->is_schema_nc,
						   schema, &session_key, getnc_state->min_usn,
						   req10->replica_flags,
						   req10->partial_attribute_set,
						   getnc_state->uptodateness_vector,
						   req10->extended_op);
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}

		werr = get_nc_changes_add_links(sam_ctx, getnc_state,
						getnc_state->ncRoot_dn,
						schema, getnc_state->min_usn,
						req10->replica_flags,
						msg,
						&getnc_state->la_list,
						&getnc_state->la_count,
						getnc_state->uptodateness_vector);
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}

		uSN = ldb_msg_find_attr_as_int(msg, "uSNChanged", -1);
		if (uSN > r->out.ctr->ctr6.new_highwatermark.tmp_highest_usn) {
			r->out.ctr->ctr6.new_highwatermark.tmp_highest_usn = uSN;
		}
		if (uSN > getnc_state->highest_usn) {
			getnc_state->highest_usn = uSN;
		}

		if (obj->meta_data_ctr == NULL) {
			DEBUG(8,(__location__ ": getncchanges skipping send of object %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			/* no attributes to send */
			talloc_free(obj);
			continue;
		}

		r->out.ctr->ctr6.object_count++;
		
		*currentObject = obj;
		currentObject = &obj->next_object;

		talloc_free(getnc_state->last_dn);
		getnc_state->last_dn = ldb_dn_copy(getnc_state, msg->dn);

		DEBUG(8,(__location__ ": replicating object %s\n", ldb_dn_get_linearized(msg->dn)));

		talloc_free(msg_res);
		talloc_free(msg_dn);
	}

	getnc_state->num_processed = i;

	r->out.ctr->ctr6.nc_object_count = getnc_state->num_records;

	/* the client can us to call UpdateRefs on its behalf to
	   re-establish monitoring of the NC */
	if ((req10->replica_flags & (DRSUAPI_DRS_ADD_REF | DRSUAPI_DRS_REF_GCSPN)) &&
	    !GUID_all_zero(&req10->destination_dsa_guid)) {
		struct drsuapi_DsReplicaUpdateRefsRequest1 ureq;
		DEBUG(3,("UpdateRefs on getncchanges for %s\n",
			 GUID_string(mem_ctx, &req10->destination_dsa_guid)));
		ureq.naming_context = ncRoot;
		ureq.dest_dsa_dns_name = talloc_asprintf(mem_ctx, "%s._msdcs.%s",
							 GUID_string(mem_ctx, &req10->destination_dsa_guid),
							 lpcfg_dnsdomain(dce_call->conn->dce_ctx->lp_ctx));
		if (!ureq.dest_dsa_dns_name) {
			return WERR_NOMEM;
		}
		ureq.dest_dsa_guid = req10->destination_dsa_guid;
		ureq.options = DRSUAPI_DRS_ADD_REF |
			DRSUAPI_DRS_ASYNC_OP |
			DRSUAPI_DRS_GETCHG_CHECK;

		/* we also need to pass through the
		   DRSUAPI_DRS_REF_GCSPN bit so that repsTo gets flagged
		   to send notifies using the GC SPN */
		ureq.options |= (req10->replica_flags & DRSUAPI_DRS_REF_GCSPN);

		werr = drsuapi_UpdateRefs(b_state, mem_ctx, &ureq);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,(__location__ ": Failed UpdateRefs in DsGetNCChanges - %s\n",
				 win_errstr(werr)));
		}
	}

	/*
	 * TODO:
	 * This is just a guess, how to calculate the
	 * number of linked attributes to send, we need to
	 * find out how to do this right.
	 */
	if (r->out.ctr->ctr6.object_count >= max_links) {
		max_links = 0;
	} else {
		max_links -= r->out.ctr->ctr6.object_count;
	}

	link_total = getnc_state->la_count;

	if (i < getnc_state->num_records) {
		r->out.ctr->ctr6.more_data = true;
	} else {
		/* sort the whole array the first time */
		if (!getnc_state->la_sorted) {
			LDB_TYPESAFE_QSORT(getnc_state->la_list, getnc_state->la_count,
					   sam_ctx, linked_attribute_compare);
			getnc_state->la_sorted = true;
		}

		link_count = getnc_state->la_count - getnc_state->la_idx;
		link_count = MIN(max_links, link_count);

		r->out.ctr->ctr6.linked_attributes_count = link_count;
		r->out.ctr->ctr6.linked_attributes = getnc_state->la_list + getnc_state->la_idx;

		getnc_state->la_idx += link_count;
		link_given = getnc_state->la_idx;

		if (getnc_state->la_idx < getnc_state->la_count) {
			r->out.ctr->ctr6.more_data = true;
		}
	}

	if (!r->out.ctr->ctr6.more_data) {
		talloc_steal(mem_ctx, getnc_state->la_list);

		r->out.ctr->ctr6.uptodateness_vector = talloc(mem_ctx, struct drsuapi_DsReplicaCursor2CtrEx);
		r->out.ctr->ctr6.new_highwatermark.highest_usn = r->out.ctr->ctr6.new_highwatermark.tmp_highest_usn;

		werr = get_nc_changes_udv(sam_ctx, getnc_state->ncRoot_dn,
					  r->out.ctr->ctr6.uptodateness_vector);
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}

		talloc_free(getnc_state);
		b_state->getncchanges_state = NULL;
	}

	if (req10->extended_op != DRSUAPI_EXOP_NONE) {
		r->out.ctr->ctr6.uptodateness_vector = NULL;
		r->out.ctr->ctr6.nc_object_count = 0;
		ZERO_STRUCT(r->out.ctr->ctr6.new_highwatermark);
		r->out.ctr->ctr6.extended_ret = DRSUAPI_EXOP_ERR_SUCCESS;
	}

	DEBUG(r->out.ctr->ctr6.more_data?4:2,
	      ("DsGetNCChanges with uSNChanged >= %llu flags 0x%08x on %s gave %u objects (done %u/%u) %u links (done %u/%u (as %s))\n",
	       (unsigned long long)(req10->highwatermark.highest_usn+1),
	       req10->replica_flags, drs_ObjectIdentifier_to_string(mem_ctx, ncRoot),
	       r->out.ctr->ctr6.object_count,
	       i, r->out.ctr->ctr6.more_data?getnc_state->num_records:i,
	       r->out.ctr->ctr6.linked_attributes_count,
	       link_given, link_total,
	       dom_sid_string(mem_ctx, user_sid)));

#if 0
	if (!r->out.ctr->ctr6.more_data && req10->extended_op != DRSUAPI_EXOP_NONE) {
		NDR_PRINT_FUNCTION_DEBUG(drsuapi_DsGetNCChanges, NDR_BOTH, r);
	}
#endif

	return WERR_OK;
}
