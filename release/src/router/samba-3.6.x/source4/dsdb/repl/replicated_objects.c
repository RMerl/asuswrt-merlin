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
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "libcli/auth/libcli_auth.h"
#include "param/param.h"

/**
 * Multi-pass working schema creation
 * Function will:
 *  - shallow copy initial schema supplied
 *  - create a working schema in multiple passes
 *    until all objects are resolved
 * Working schema is a schema with Attributes, Classes
 * and indexes, but w/o subClassOf, possibleSupperiors etc.
 * It is to be used just us cache for converting attribute values.
 */
WERROR dsdb_repl_make_working_schema(struct ldb_context *ldb,
				     const struct dsdb_schema *initial_schema,
				     const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr,
				     uint32_t object_count,
				     const struct drsuapi_DsReplicaObjectListItemEx *first_object,
				     const DATA_BLOB *gensec_skey,
				     TALLOC_CTX *mem_ctx,
				     struct dsdb_schema **_schema_out)
{
	struct schema_list {
		struct schema_list *next, *prev;
		const struct drsuapi_DsReplicaObjectListItemEx *obj;
	};

	WERROR werr;
	struct dsdb_schema_prefixmap *pfm_remote;
	struct schema_list *schema_list = NULL, *schema_list_item, *schema_list_next_item;
	struct dsdb_schema *working_schema;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	int ret, pass_no;
	uint32_t ignore_attids[] = {
			DRSUAPI_ATTID_auxiliaryClass,
			DRSUAPI_ATTID_mayContain,
			DRSUAPI_ATTID_mustContain,
			DRSUAPI_ATTID_possSuperiors,
			DRSUAPI_ATTID_systemPossSuperiors,
			DRSUAPI_ATTID_INVALID
	};

	/* make a copy of the iniatial_scheam so we don't mess with it */
	working_schema = dsdb_schema_copy_shallow(mem_ctx, ldb, initial_schema);
	if (!working_schema) {
		DEBUG(0,(__location__ ": schema copy failed!\n"));
		return WERR_NOMEM;
	}

	/* we are going to need remote prefixMap for decoding */
	werr = dsdb_schema_pfm_from_drsuapi_pfm(mapping_ctr, true,
						mem_ctx, &pfm_remote, NULL);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to decode remote prefixMap: %s",
			 win_errstr(werr)));
		return werr;
	}

	/* create a list of objects yet to be converted */
	for (cur = first_object; cur; cur = cur->next_object) {
		schema_list_item = talloc(mem_ctx, struct schema_list);
		schema_list_item->obj = cur;
		DLIST_ADD_END(schema_list, schema_list_item, struct schema_list);
	}

	/* resolve objects until all are resolved and in local schema */
	pass_no = 1;

	while (schema_list) {
		uint32_t converted_obj_count = 0;
		uint32_t failed_obj_count = 0;
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		for (schema_list_item = schema_list; schema_list_item; schema_list_item=schema_list_next_item) {
			struct dsdb_extended_replicated_object object;

			cur = schema_list_item->obj;

			/* Save the next item, now we have saved out
			 * the current one, so we can DLIST_REMOVE it
			 * safely */
			schema_list_next_item = schema_list_item->next;

			/*
			 * Convert the objects into LDB messages using the
			 * schema we have so far. It's ok if we fail to convert
			 * an object. We should convert more objects on next pass.
			 */
			werr = dsdb_convert_object_ex(ldb, working_schema, pfm_remote,
						      cur, gensec_skey,
						      ignore_attids,
						      tmp_ctx, &object);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(1,("Warning: Failed to convert schema object %s into ldb msg\n",
					 cur->object.identifier->dn));

				failed_obj_count++;
			} else {
				/*
				 * Convert the schema from ldb_message format
				 * (OIDs as OID strings) into schema, using
				 * the remote prefixMap
				 */
				werr = dsdb_schema_set_el_from_ldb_msg(ldb,
								       working_schema,
								       object.msg);
				if (!W_ERROR_IS_OK(werr)) {
					DEBUG(1,("Warning: failed to convert object %s into a schema element: %s\n",
						 ldb_dn_get_linearized(object.msg->dn),
						 win_errstr(werr)));
					failed_obj_count++;
				} else {
					DLIST_REMOVE(schema_list, schema_list_item);
					talloc_free(schema_list_item);
					converted_obj_count++;
				}
			}
		}
		talloc_free(tmp_ctx);

		DEBUG(4,("Schema load pass %d: %d/%d of %d objects left to be converted.\n",
			 pass_no, failed_obj_count, converted_obj_count, object_count));
		pass_no++;

		/* check if we converted any objects in this pass */
		if (converted_obj_count == 0) {
			DEBUG(0,("Can't continue Schema load: didn't manage to convert any objects: all %d remaining of %d objects failed to convert\n", failed_obj_count, object_count));
			return WERR_INTERNAL_ERROR;
		}

		/* rebuild indexes */
		ret = dsdb_setup_sorted_accessors(ldb, working_schema);
		if (LDB_SUCCESS != ret) {
			DEBUG(0,("Failed to create schema-cache indexes!\n"));
			return WERR_INTERNAL_ERROR;
		}
	};

	*_schema_out = working_schema;

	return WERR_OK;
}

static bool dsdb_attid_in_list(const uint32_t attid_list[], uint32_t attid)
{
	const uint32_t *cur;
	if (!attid_list) {
		return false;
	}
	for (cur = attid_list; *cur != DRSUAPI_ATTID_INVALID; cur++) {
		if (*cur == attid) {
			return true;
		}
	}
	return false;
}

WERROR dsdb_convert_object_ex(struct ldb_context *ldb,
			      const struct dsdb_schema *schema,
			      const struct dsdb_schema_prefixmap *pfm_remote,
			      const struct drsuapi_DsReplicaObjectListItemEx *in,
			      const DATA_BLOB *gensec_skey,
			      const uint32_t *ignore_attids,
			      TALLOC_CTX *mem_ctx,
			      struct dsdb_extended_replicated_object *out)
{
	NTSTATUS nt_status;
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
	uint32_t attr_count;
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

	for (i=0, attr_count=0; i < in->meta_data_ctr->count; i++, attr_count++) {
		struct drsuapi_DsReplicaAttribute *a;
		struct drsuapi_DsReplicaMetaData *d;
		struct replPropertyMetaData1 *m;
		struct ldb_message_element *e;
		uint32_t j;

		a = &in->object.attribute_ctr.attributes[i];
		d = &in->meta_data_ctr->meta_data[i];
		m = &md->ctr.ctr1.array[attr_count];
		e = &msg->elements[attr_count];

		if (dsdb_attid_in_list(ignore_attids, a->attid)) {
			attr_count--;
			continue;
		}

		for (j=0; j<a->value_ctr.num_values; j++) {
			status = drsuapi_decrypt_attribute(a->value_ctr.values[j].blob, gensec_skey, rid, a);
			W_ERROR_NOT_OK_RETURN(status);
		}

		status = dsdb_attribute_drsuapi_to_ldb(ldb, schema, pfm_remote,
						       a, msg->elements, e);
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

		if (a->attid == DRSUAPI_ATTID_name) {
			name_a = a;
			name_d = d;
		}
	}

	msg->num_elements = attr_count;
	md->ctr.ctr1.count = attr_count;
	if (name_a) {
		rdn_m = &md->ctr.ctr1.array[md->ctr.ctr1.count];
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

	nt_status = GUID_to_ndr_blob(&in->object.identifier->guid, msg, &guid_value);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return ntstatus_to_werror(nt_status);
	}

	out->msg		= msg;
	out->guid_value		= guid_value;
	out->when_changed	= whenChanged_s;
	out->meta_data		= md;
	return WERR_OK;
}

WERROR dsdb_replicated_objects_convert(struct ldb_context *ldb,
				       const struct dsdb_schema *schema,
				       const char *partition_dn_str,
				       const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr,
				       uint32_t object_count,
				       const struct drsuapi_DsReplicaObjectListItemEx *first_object,
				       uint32_t linked_attributes_count,
				       const struct drsuapi_DsReplicaLinkedAttribute *linked_attributes,
				       const struct repsFromTo1 *source_dsa,
				       const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector,
				       const DATA_BLOB *gensec_skey,
				       TALLOC_CTX *mem_ctx,
				       struct dsdb_extended_replicated_objects **objects)
{
	WERROR status;
	struct ldb_dn *partition_dn;
	struct dsdb_schema_prefixmap *pfm_remote;
	struct dsdb_extended_replicated_objects *out;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t i;

	out = talloc_zero(mem_ctx, struct dsdb_extended_replicated_objects);
	W_ERROR_HAVE_NO_MEMORY(out);
	out->version		= DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION;

	/*
	 * Ensure schema is kept valid for as long as 'out'
	 * which may contain pointers to it
	 */
	schema = talloc_reference(out, schema);
	W_ERROR_HAVE_NO_MEMORY(schema);

	partition_dn = ldb_dn_new(out, ldb, partition_dn_str);
	W_ERROR_HAVE_NO_MEMORY_AND_FREE(partition_dn, out);

	status = dsdb_schema_pfm_from_drsuapi_pfm(mapping_ctr, true,
						  out, &pfm_remote, NULL);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,(__location__ ": Failed to decode remote prefixMap: %s",
			 win_errstr(status)));
		talloc_free(out);
		return status;
	}

	if (ldb_dn_compare(partition_dn, ldb_get_schema_basedn(ldb)) != 0) {
		/*
		 * check for schema changes in case
		 * we are not replicating Schema NC
		 */
		status = dsdb_schema_info_cmp(schema, mapping_ctr);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(1,("Remote schema has changed while replicating %s\n",
				 partition_dn_str));
			talloc_free(out);
			return status;
		}
	}

	out->partition_dn	= partition_dn;

	out->source_dsa		= source_dsa;
	out->uptodateness_vector= uptodateness_vector;

	out->num_objects	= object_count;
	out->objects		= talloc_array(out,
					       struct dsdb_extended_replicated_object,
					       out->num_objects);
	W_ERROR_HAVE_NO_MEMORY_AND_FREE(out->objects, out);

	/* pass the linked attributes down to the repl_meta_data
	   module */
	out->linked_attributes_count = linked_attributes_count;
	out->linked_attributes       = linked_attributes;

	for (i=0, cur = first_object; cur; cur = cur->next_object, i++) {
		if (i == out->num_objects) {
			talloc_free(out);
			return WERR_FOOBAR;
		}

		status = dsdb_convert_object_ex(ldb, schema, pfm_remote,
						cur, gensec_skey,
						NULL,
						out->objects, &out->objects[i]);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(out);
			DEBUG(0,("Failed to convert object %s: %s\n",
				 cur->object.identifier->dn,
				 win_errstr(status)));
			return status;
		}
	}
	if (i != out->num_objects) {
		talloc_free(out);
		return WERR_FOOBAR;
	}

	/* free pfm_remote, we won't need it anymore */
	talloc_free(pfm_remote);

	*objects = out;
	return WERR_OK;
}

/**
 * Commits a list of replicated objects.
 *
 * @param working_schema dsdb_schema to be used for resolving
 * 			 Classes/Attributes during Schema replication. If not NULL,
 * 			 it will be set on ldb and used while committing replicated objects
 */
WERROR dsdb_replicated_objects_commit(struct ldb_context *ldb,
				      struct dsdb_schema *working_schema,
				      struct dsdb_extended_replicated_objects *objects,
				      uint64_t *notify_uSN)
{
	WERROR werr;
	struct ldb_result *ext_res;
	struct dsdb_schema *cur_schema = NULL;
	int ret;
	uint64_t seq_num1, seq_num2;

	/* TODO: handle linked attributes */

	/* wrap the extended operation in a transaction 
	   See [MS-DRSR] 3.3.2 Transactions
	 */
	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to start transaction\n"));
		return WERR_FOOBAR;
	}

	ret = dsdb_load_partition_usn(ldb, objects->partition_dn, &seq_num1, NULL);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ " Failed to load partition uSN\n"));
		ldb_transaction_cancel(ldb);
		return WERR_FOOBAR;		
	}

	/*
	 * Set working_schema for ldb in case we are replicating from Schema NC.
	 * Schema won't be reloaded during Replicated Objects commit, as it is
	 * done in a transaction. So we need some way to search for newly
	 * added Classes and Attributes
	 */
	if (working_schema) {
		/* store current schema so we can fall back in case of failure */
		cur_schema = dsdb_get_schema(ldb, working_schema);

		ret = dsdb_reference_schema(ldb, working_schema, false);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ "Failed to reference working schema - %s\n",
				 ldb_strerror(ret)));
			/* TODO: Map LDB Error to NTSTATUS? */
			ldb_transaction_cancel(ldb);
			return WERR_INTERNAL_ERROR;
		}
	}

	ret = ldb_extended(ldb, DSDB_EXTENDED_REPLICATED_OBJECTS_OID, objects, &ext_res);
	if (ret != LDB_SUCCESS) {
		/* restore previous schema */
		if (cur_schema ) {
			dsdb_reference_schema(ldb, cur_schema, false);
			dsdb_make_schema_global(ldb, cur_schema);
		}

		DEBUG(0,("Failed to apply records: %s: %s\n",
			 ldb_errstring(ldb), ldb_strerror(ret)));
		ldb_transaction_cancel(ldb);
		return WERR_FOOBAR;
	}
	talloc_free(ext_res);

	/* Save our updated prefixMap */
	if (working_schema) {
		werr = dsdb_write_prefixes_from_schema_to_ldb(working_schema,
							      ldb,
							      working_schema);
		if (!W_ERROR_IS_OK(werr)) {
			/* restore previous schema */
			if (cur_schema ) {
				dsdb_reference_schema(ldb, cur_schema, false);
				dsdb_make_schema_global(ldb, cur_schema);
			}
			DEBUG(0,("Failed to save updated prefixMap: %s\n",
				 win_errstr(werr)));
			return werr;
		}
	}

	ret = ldb_transaction_prepare_commit(ldb);
	if (ret != LDB_SUCCESS) {
		/* restore previous schema */
		if (cur_schema ) {
			dsdb_reference_schema(ldb, cur_schema, false);
			dsdb_make_schema_global(ldb, cur_schema);
		}
		DEBUG(0,(__location__ " Failed to prepare commit of transaction: %s\n",
			 ldb_errstring(ldb)));
		return WERR_FOOBAR;
	}

	ret = dsdb_load_partition_usn(ldb, objects->partition_dn, &seq_num2, NULL);
	if (ret != LDB_SUCCESS) {
		/* restore previous schema */
		if (cur_schema ) {
			dsdb_reference_schema(ldb, cur_schema, false);
			dsdb_make_schema_global(ldb, cur_schema);
		}
		DEBUG(0,(__location__ " Failed to load partition uSN\n"));
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
		/* restore previous schema */
		if (cur_schema ) {
			dsdb_reference_schema(ldb, cur_schema, false);
			dsdb_make_schema_global(ldb, cur_schema);
		}
		DEBUG(0,(__location__ " Failed to commit transaction\n"));
		return WERR_FOOBAR;
	}

	/*
	 * Reset the Schema used by ldb. This will lead to
	 * a schema cache being refreshed from database.
	 */
	if (working_schema) {
		cur_schema = dsdb_get_schema(ldb, NULL);
		/* TODO: What we do in case dsdb_get_schema() fail?
		 *       We can't fallback at this point anymore */
		if (cur_schema) {
			dsdb_make_schema_global(ldb, cur_schema);
		}
	}

	DEBUG(2,("Replicated %u objects (%u linked attributes) for %s\n",
		 objects->num_objects, objects->linked_attributes_count,
		 ldb_dn_get_linearized(objects->partition_dn)));
		 
	return WERR_OK;
}

static WERROR dsdb_origin_object_convert(struct ldb_context *ldb,
					 const struct dsdb_schema *schema,
					 const struct drsuapi_DsReplicaObjectListItem *in,
					 TALLOC_CTX *mem_ctx,
					 struct ldb_message **_msg)
{
	WERROR status;
	unsigned int i;
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

		status = dsdb_attribute_drsuapi_to_ldb(ldb, schema, schema->prefixmap,
						       a, msg->elements, e);
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

	for (cur = first_object; cur; cur = cur->next_object) {
		num_objects++;
	}

	if (num_objects == 0) {
		return WERR_OK;
	}

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return WERR_DS_INTERNAL_FAILURE;
	}

	objects	= talloc_array(mem_ctx, struct ldb_message *,
			       num_objects);
	if (objects == NULL) {
		status = WERR_NOMEM;
		goto cancel;
	}

	schema = dsdb_get_schema(ldb, objects);
	if (!schema) {
		return WERR_DS_SCHEMA_NOT_LOADED;
	}

	for (i=0, cur = first_object; cur; cur = cur->next_object, i++) {
		status = dsdb_origin_object_convert(ldb, schema, cur,
						    objects, &objects[i]);
		if (!W_ERROR_IS_OK(status)) {
			goto cancel;
		}
	}

	ids = talloc_array(mem_ctx,
			   struct drsuapi_DsReplicaObjectIdentifier2,
			   num_objects);
	if (ids == NULL) {
		status = WERR_NOMEM;
		goto cancel;
	}

	for (i=0; i < num_objects; i++) {
		struct dom_sid *sid = NULL;
		struct ldb_request *add_req;

		DEBUG(6,(__location__ ": adding %s\n", 
			 ldb_dn_get_linearized(objects[i]->dn)));

		ret = ldb_build_add_req(&add_req,
					ldb,
					objects,
					objects[i],
					NULL,
					NULL,
					ldb_op_default_callback,
					NULL);
		if (ret != LDB_SUCCESS) {
			status = WERR_DS_INTERNAL_FAILURE;
			goto cancel;
		}

		ret = ldb_request_add_control(add_req, LDB_CONTROL_RELAX_OID, true, NULL);
		if (ret != LDB_SUCCESS) {
			status = WERR_DS_INTERNAL_FAILURE;
			goto cancel;
		}
		
		ret = ldb_request(ldb, add_req);
		if (ret == LDB_SUCCESS) {
			ret = ldb_wait(add_req->handle, LDB_WAIT_ALL);
		}
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed add of %s - %s\n",
				 ldb_dn_get_linearized(objects[i]->dn), ldb_errstring(ldb)));
			status = WERR_DS_INTERNAL_FAILURE;
			goto cancel;
		}

		talloc_free(add_req);

		ret = ldb_search(ldb, objects, &res, objects[i]->dn,
				 LDB_SCOPE_BASE, attrs,
				 "(objectClass=*)");
		if (ret != LDB_SUCCESS) {
			status = WERR_DS_INTERNAL_FAILURE;
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

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		return WERR_DS_INTERNAL_FAILURE;
	}

	talloc_free(objects);

	*_num = num_objects;
	*_ids = ids;
	return WERR_OK;

cancel:
	talloc_free(objects);
	ldb_transaction_cancel(ldb);
	return status;
}
