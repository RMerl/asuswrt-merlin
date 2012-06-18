/* 
   Unix SMB/CIFS mplementation.
   Helper functions for applying replicated objects
   
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
    
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
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "libcli/auth/libcli_auth.h"
#include "param/param.h"

static WERROR dsdb_convert_object_ex(struct ldb_context *ldb,
				     const struct dsdb_schema *schema,
				     const struct drsuapi_DsReplicaObjectListItemEx *in,
				     const DATA_BLOB *gensec_skey,
				     TALLOC_CTX *mem_ctx,
				     struct dsdb_extended_replicated_object *out)
{
	NTSTATUS nt_status;
	enum ndr_err_code ndr_err;
	WERROR status;
	uint32_t i;
	struct ldb_message *msg;
	struct replPropertyMetaDataBlob *md;
	struct ldb_val guid_value;
	NTTIME whenChanged = 0;
	time_t whenChanged_t;
	const char *whenChanged_s;
	const char *rdn_name = NULL;
	const struct ldb_val *rdn_value = NULL;
	const struct dsdb_attribute *rdn_attr = NULL;
	uint32_t rdn_attid;
	struct drsuapi_DsReplicaAttribute *name_a = NULL;
	struct drsuapi_DsReplicaMetaData *name_d = NULL;
	struct replPropertyMetaData1 *rdn_m = NULL;
	struct dom_sid *sid = NULL;
	uint32_t rid = 0;
	int ret;

	if (!in->object.identifier) {
		return WERR_FOOBAR;
	}

	if (!in->object.identifier->dn || !in->object.identifier->dn[0]) {
		return WERR_FOOBAR;
	}

	if (in->object.attribute_ctr.num_attributes != 0 && !in->meta_data_ctr) {
		return WERR_FOOBAR;
	}

	if (in->object.attribute_ctr.num_attributes != in->meta_data_ctr->count) {
		return WERR_FOOBAR;
	}

	sid = &in->object.identifier->sid;
	if (sid->num_auths > 0) {
		rid = sid->sub_auths[sid->num_auths - 1];
	}

	msg = ldb_msg_new(mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(msg);

	msg->dn			= ldb_dn_new(msg, ldb, in->object.identifier->dn);
	W_ERROR_HAVE_NO_MEMORY(msg->dn);

	rdn_name	= ldb_dn_get_rdn_name(msg->dn);
	rdn_attr	= dsdb_attribute_by_lDAPDisplayName(schema, rdn_name);
	if (!rdn_attr) {
		return WERR_FOOBAR;
	}
	rdn_attid	= rdn_attr->attributeID_id;
	rdn_value	= ldb_dn_get_rdn_val(msg->dn);

	msg->num_elements	= in->object.attribute_ctr.num_attributes;
	msg->elements		= talloc_array(msg, struct ldb_message_element,
					       msg->num_elements);
	W_ERROR_HAVE_NO_MEMORY(msg->elements);

	md = talloc(mem_ctx, struct replPropertyMetaDataBlob);
	W_ERROR_HAVE_NO_MEMORY(md);

	md->version		= 1;
	md->reserved		= 0;
	md->ctr.ctr1.count	= in->meta_data_ctr->count;
	md->ctr.ctr1.reserved	= 0;
	md->ctr.ctr1.array	= talloc_array(mem_ctx,
					       struct replPropertyMetaData1,
					       md->ctr.ctr1.count + 1); /* +1 because of the RDN attribute */
	W_ERROR_HAVE_NO_MEMORY(md->ctr.ctr1.array);

	for (i=0; i < in->meta_data_ctr->count; i++) {
		struct drsuapi_DsReplicaAttribute *a;
		struct drsuapi_DsReplicaMetaData *d;
		struct replPropertyMetaData1 *m;
		struct ldb_message_element *e;
		int j;

		a = &in->object.attribute_ctr.attributes[i];
		d = &in->meta_data_ctr->meta_data[i];
		m = &md->ctr.ctr1.array[i];
		e = &msg->elements[i];

		for (j=0; j<a->value_ctr.num_values; j++) {
			status = drsuapi_decrypt_attribute(a->value_ctr.values[j].blob, gensec_skey, rid, a);
			W_ERROR_NOT_OK_RETURN(status);
		}

		status = dsdb_attribute_drsuapi_to_ldb(ldb, schema, a, msg->elements, e);
		W_ERROR_NOT_OK_RETURN(status);

		m->attid			= a->attid;
		m->version			= d->version;
		m->originating_change_time	= d->originating_change_time;
		m->originating_invocation_id	= d->originating_invocation_id;
		m->originating_usn		= d->originating_usn;
		m->local_usn			= 0;

		if (d->originating_change_time > whenChanged) {
			whenChanged = d->originating_change_time;
		}

		if (a->attid == DRSUAPI_ATTRIBUTE_name) {
			name_a = a;
			name_d = d;
			rdn_m = &md->ctr.ctr1.array[md->ctr.ctr1.count];
		}
	}

	if (rdn_m) {
		struct ldb_message_element *el;
		el = ldb_msg_find_element(msg, rdn_attr->lDAPDisplayName);
		if (!el) {
			ret = ldb_msg_add_value(msg, rdn_attr->lDAPDisplayName, rdn_value, NULL);
			if (ret != LDB_SUCCESS) {
				return WERR_FOOBAR;
			}
		} else {
			if (el->num_values != 1) {
				DEBUG(0,(__location__ ": Unexpected num_values=%u\n",
					 el->num_values));
				return WERR_FOOBAR;				
			}
			if (!ldb_val_equal_exact(&el->values[0], rdn_value)) {
				DEBUG(0,(__location__ ": RDN value changed? '%*.*s' '%*.*s'\n",
					 (int)el->values[0].length, (int)el->values[0].length, el->values[0].data,
					 (int)rdn_value->length, (int)rdn_value->length, rdn_value->data));
				return WERR_FOOBAR;				
			}
		}

		rdn_m->attid				= rdn_attid;
		rdn_m->version				= name_d->version;
		rdn_m->originating_change_time		= name_d->originating_change_time;
		rdn_m->originating_invocation_id	= name_d->originating_invocation_id;
		rdn_m->originating_usn			= name_d->originating_usn;
		rdn_m->local_usn			= 0;
		md->ctr.ctr1.count++;

	}

	whenChanged_t = nt_time_to_unix(whenChanged);
	whenChanged_s = ldb_timestring(msg, whenChanged_t);
	W_ERROR_HAVE_NO_MEMORY(whenChanged_s);

	ndr_err = ndr_push_struct_blob(&guid_value, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &in->object.identifier->guid,
					 (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		nt_status = ndr_map_error2ntstatus(ndr_err);
		return ntstatus_to_werror(nt_status);
	}

	out->msg		= msg;
	out->guid_value		= guid_value;
	out->when_changed	= whenChanged_s;
	out->meta_data		= md;
	return WERR_OK;
}

WERROR dsdb_extended_replicated_objects_commit(struct ldb_context *ldb,
					       const char *partition_dn,
					       const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr,
					       uint32_t object_count,
					       const struct drsuapi_DsReplicaObjectListItemEx *first_object,
					       uint32_t linked_attributes_count,
					       const struct drsuapi_DsReplicaLinkedAttribute *linked_attributes,
					       const struct repsFromTo1 *source_dsa,
					       const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector,
					       const DATA_BLOB *gensec_skey,
					       TALLOC_CTX *mem_ctx,
					       struct dsdb_extended_replicated_objects **_out,
					       uint64_t *notify_uSN)
{
	WERROR status;
	const struct dsdb_schema *schema;
	struct dsdb_extended_replicated_objects *out;
	struct ldb_result *ext_res;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t i;
	int ret;
	uint64_t seq_num1, seq_num2;

	schema = dsdb_get_schema(ldb);
	if (!schema) {
		return WERR_DS_SCHEMA_NOT_LOADED;
	}

	status = dsdb_verify_oid_mappings_drsuapi(schema, mapping_ctr);
	W_ERROR_NOT_OK_RETURN(status);

	out = talloc_zero(mem_ctx, struct dsdb_extended_replicated_objects);
	W_ERROR_HAVE_NO_MEMORY(out);
	out->version		= DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION;

	out->partition_dn	= ldb_dn_new(out, ldb, partition_dn);
	W_ERROR_HAVE_NO_MEMORY(out->partition_dn);

	out->source_dsa		= source_dsa;
	out->uptodateness_vector= uptodateness_vector;

	out->num_objects	= object_count;
	out->objects		= talloc_array(out,
					       struct dsdb_extended_replicated_object,
					       out->num_objects);
	W_ERROR_HAVE_NO_MEMORY(out->objects);

	/* pass the linked attributes down to the repl_meta_data
	   module */
	out->linked_attributes_count = linked_attributes_count;
	out->linked_attributes       = linked_attributes;

	for (i=0, cur = first_object; cur; cur = cur->next_object, i++) {
		if (i == out->num_objects) {
			return WERR_FOOBAR;
		}

		status = dsdb_convert_object_ex(ldb, schema,
						cur, gensec_skey,
						out->objects, &out->objects[i]);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("Failed to convert object %s\n", cur->object.identifier->dn));
			return status;
		}
	}
	if (i != out->num_objects) {
		return WERR_FOOBAR;
	}

	/* TODO: handle linked attributes */

	/* wrap the extended operation in a transaction 
	   See [MS-DRSR] 3.3.2 Transactions
	 */
	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to start transaction\n"));
		talloc_free(out);
		return WERR_FOOBAR;
	}

	ret = dsdb_load_partition_usn(ldb, out->partition_dn, &seq_num1);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to load partition uSN\n"));
		talloc_free(out);
		ldb_transaction_cancel(ldb);
		return WERR_FOOBAR;		
	}

	ret = ldb_extended(ldb, DSDB_EXTENDED_REPLICATED_OBJECTS_OID, out, &ext_res);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to apply records: %s: %s\n",
			 ldb_errstring(ldb), ldb_strerror(ret)));
		talloc_free(out);
		ldb_transaction_cancel(ldb);
		return WERR_FOOBAR;
	}
	talloc_free(ext_res);

	ret = ldb_transaction_prepare_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to prepare commit of transaction\n"));
		talloc_free(out);
		return WERR_FOOBAR;
	}

	ret = dsdb_load_partition_usn(ldb, out->partition_dn, &seq_num2);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to load partition uSN\n"));
		talloc_free(out);
		ldb_transaction_cancel(ldb);
		return WERR_FOOBAR;		
	}

	/* if this replication partner didn't need to be notified
	   before this transaction then it still doesn't need to be
	   notified, as the changes came from this server */	
	if (seq_num2 > seq_num1 && seq_num1 <= *notify_uSN) {
		*notify_uSN = seq_num2;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to commit transaction\n"));
		talloc_free(out);
		return WERR_FOOBAR;
	}


	DEBUG(2,("Replicated %u objects (%u linked attributes) for %s\n",
		 out->num_objects, out->linked_attributes_count,
		 ldb_dn_get_linearized(out->partition_dn)));
		 

	if (_out) {
		*_out = out;
	} else {
		talloc_free(out);
	}

	return WERR_OK;
}

static WERROR dsdb_convert_object(struct ldb_context *ldb,
				  const struct dsdb_schema *schema,
				  const struct drsuapi_DsReplicaObjectListItem *in,
				  TALLOC_CTX *mem_ctx,
				  struct ldb_message **_msg)
{
	WERROR status;
	uint32_t i;
	struct ldb_message *msg;

	if (!in->object.identifier) {
		return WERR_FOOBAR;
	}

	if (!in->object.identifier->dn || !in->object.identifier->dn[0]) {
		return WERR_FOOBAR;
	}

	msg = ldb_msg_new(mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(msg);

	msg->dn	= ldb_dn_new(msg, ldb, in->object.identifier->dn);
	W_ERROR_HAVE_NO_MEMORY(msg->dn);

	msg->num_elements	= in->object.attribute_ctr.num_attributes;
	msg->elements		= talloc_array(msg, struct ldb_message_element,
					       msg->num_elements);
	W_ERROR_HAVE_NO_MEMORY(msg->elements);

	for (i=0; i < msg->num_elements; i++) {
		struct drsuapi_DsReplicaAttribute *a;
		struct ldb_message_element *e;

		a = &in->object.attribute_ctr.attributes[i];
		e = &msg->elements[i];

		status = dsdb_attribute_drsuapi_to_ldb(ldb, schema, a, msg->elements, e);
		W_ERROR_NOT_OK_RETURN(status);
	}


	*_msg = msg;

	return WERR_OK;
}

WERROR dsdb_origin_objects_commit(struct ldb_context *ldb,
				  TALLOC_CTX *mem_ctx,
				  const struct drsuapi_DsReplicaObjectListItem *first_object,
				  uint32_t *_num,
				  struct drsuapi_DsReplicaObjectIdentifier2 **_ids)
{
	WERROR status;
	const struct dsdb_schema *schema;
	const struct drsuapi_DsReplicaObjectListItem *cur;
	struct ldb_message **objects;
	struct drsuapi_DsReplicaObjectIdentifier2 *ids;
	uint32_t i;
	uint32_t num_objects = 0;
	const char * const attrs[] = {
		"objectGUID",
		"objectSid",
		NULL
	};
	struct ldb_result *res;
	int ret;

	schema = dsdb_get_schema(ldb);
	if (!schema) {
		return WERR_DS_SCHEMA_NOT_LOADED;
	}

	for (cur = first_object; cur; cur = cur->next_object) {
		num_objects++;
	}

	if (num_objects == 0) {
		return WERR_OK;
	}

	objects	= talloc_array(mem_ctx, struct ldb_message *,
			       num_objects);
	W_ERROR_HAVE_NO_MEMORY(objects);

	for (i=0, cur = first_object; cur; cur = cur->next_object, i++) {
		status = dsdb_convert_object(ldb, schema,
					     cur, objects, &objects[i]);
		W_ERROR_NOT_OK_RETURN(status);
	}

	ids = talloc_array(mem_ctx,
			   struct drsuapi_DsReplicaObjectIdentifier2,
			   num_objects);
	W_ERROR_HAVE_NO_MEMORY(objects);

	for (i=0; i < num_objects; i++) {
		struct dom_sid *sid = NULL;

		DEBUG(6,(__location__ ": adding %s\n", 
			 ldb_dn_get_linearized(objects[i]->dn)));
		
		ret = ldb_add(ldb, objects[i]);
		if (ret != 0) {
			goto cancel;
		}
		ret = ldb_search(ldb, objects, &res, objects[i]->dn,
				 LDB_SCOPE_BASE, attrs,
				 "(objectClass=*)");
		if (ret != 0) {
			goto cancel;
		}
		ids[i].guid = samdb_result_guid(res->msgs[0], "objectGUID");
		sid = samdb_result_dom_sid(objects, res->msgs[0], "objectSid");
		if (sid) {
			ids[i].sid = *sid;
		} else {
			ZERO_STRUCT(ids[i].sid);
		}
	}

	talloc_free(objects);

	*_num = num_objects;
	*_ids = ids;
	return WERR_OK;
cancel:
	talloc_free(objects);
	ldb_transaction_cancel(ldb);
	return WERR_FOOBAR;
}
