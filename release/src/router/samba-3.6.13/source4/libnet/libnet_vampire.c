/* 
   Unix SMB/CIFS implementation.
   
   Extract the user/system database from a remote server

   Copyright (C) Stefan Metzmacher	2004-2006
   Copyright (C) Brad Henry 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2008
   
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
#include "libnet/libnet.h"
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "../lib/util/dlinklist.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "system/time.h"
#include "ldb_wrap.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"
#include "param/provision.h"
#include "libcli/security/security.h"
#include "dsdb/common/util.h"

/* 
List of tasks vampire.py must perform:
- Domain Join
 - but don't write the secrets.ldb
 - results for this should be enough to handle the provision
- if vampire method is samsync 
 - Provision using these results 
  - do we still want to support this NT4 technology?
- Start samsync with libnet code
 - provision in the callback 
- Write out the secrets database, using the code from libnet_Join

*/
struct libnet_vampire_cb_state {
	const char *netbios_name;
	const char *domain_name;
	const char *realm;
	struct cli_credentials *machine_account;

	/* Schema loaded from local LDIF files */
	struct dsdb_schema *provision_schema;

        /* 1st pass, with some OIDs/attribute names/class names not
	 * converted, because we may not know them yet */
	struct dsdb_schema *self_made_schema;

	/* prefixMap in LDB format, from the remote DRS server */
	DATA_BLOB prefixmap_blob;
	const struct dsdb_schema *schema;

	struct ldb_context *ldb;

	struct {
		uint32_t object_count;
		struct drsuapi_DsReplicaObjectListItemEx *first_object;
		struct drsuapi_DsReplicaObjectListItemEx *last_object;
	} schema_part;

	const char *targetdir;

	struct loadparm_context *lp_ctx;
	struct tevent_context *event_ctx;
	unsigned total_objects;
	char *last_partition;
	const char *server_dn_str;
};

/* initialise a state structure ready for replication of chunks */
void *libnet_vampire_replicate_init(TALLOC_CTX *mem_ctx,
				    struct ldb_context *samdb,
				    struct loadparm_context *lp_ctx)
{
	struct libnet_vampire_cb_state *s = talloc_zero(mem_ctx, struct libnet_vampire_cb_state);
	if (!s) {
		return NULL;
	}

	s->ldb              = samdb;
	s->lp_ctx           = lp_ctx;
	s->provision_schema = dsdb_get_schema(s->ldb, s);
	s->schema           = s->provision_schema;
	s->netbios_name     = lpcfg_netbios_name(lp_ctx);
	s->domain_name      = lpcfg_workgroup(lp_ctx);
	s->realm            = lpcfg_realm(lp_ctx);

	return s;
}

/* Caller is expected to keep supplied pointers around for the lifetime of the structure */
void *libnet_vampire_cb_state_init(TALLOC_CTX *mem_ctx,
				   struct loadparm_context *lp_ctx, struct tevent_context *event_ctx,
				   const char *netbios_name, const char *domain_name, const char *realm,
				   const char *targetdir)
{
	struct libnet_vampire_cb_state *s = talloc_zero(mem_ctx, struct libnet_vampire_cb_state);
	if (!s) {
		return NULL;
	}

	s->lp_ctx = lp_ctx;
	s->event_ctx = event_ctx;
	s->netbios_name = netbios_name;
	s->domain_name = domain_name;
	s->realm = realm;
	s->targetdir = targetdir;
	return s;
}

struct ldb_context *libnet_vampire_cb_ldb(struct libnet_vampire_cb_state *state)
{
	state = talloc_get_type_abort(state, struct libnet_vampire_cb_state);
	return state->ldb;
}

struct loadparm_context *libnet_vampire_cb_lp_ctx(struct libnet_vampire_cb_state *state)
{
	state = talloc_get_type_abort(state, struct libnet_vampire_cb_state);
	return state->lp_ctx;
}

NTSTATUS libnet_vampire_cb_prepare_db(void *private_data,
				      const struct libnet_BecomeDC_PrepareDB *p)
{
	struct libnet_vampire_cb_state *s = talloc_get_type(private_data, struct libnet_vampire_cb_state);
	struct provision_settings settings;
	struct provision_result result;
	NTSTATUS status;

	ZERO_STRUCT(settings);
	settings.site_name = p->dest_dsa->site_name;
	settings.root_dn_str = p->forest->root_dn_str;
	settings.domain_dn_str = p->domain->dn_str;
	settings.config_dn_str = p->forest->config_dn_str;
	settings.schema_dn_str = p->forest->schema_dn_str;
	settings.netbios_name = p->dest_dsa->netbios_name;
	settings.realm = s->realm;
	settings.domain = s->domain_name;
	settings.server_dn_str = p->dest_dsa->server_dn_str;
	settings.machine_password = generate_random_password(s, 16, 255);
	settings.targetdir = s->targetdir;

	status = provision_bare(s, s->lp_ctx, &settings, &result);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	s->ldb = talloc_steal(s, result.samdb);
	s->lp_ctx = talloc_reparent(talloc_parent(result.lp_ctx), s, result.lp_ctx);
	s->provision_schema = dsdb_get_schema(s->ldb, s);
	s->server_dn_str = talloc_steal(s, p->dest_dsa->server_dn_str);

	/* wrap the entire vapire operation in a transaction.  This
	   isn't just cosmetic - we use this to ensure that linked
	   attribute back links are added at the end by relying on a
	   transaction commit hook in the linked attributes module. We
	   need to do this as the order of objects coming from the
	   server is not sufficiently deterministic to know that the
	   record that a backlink needs to be created in has itself
	   been created before the object containing the forward link
	   has come over the wire */
	if (ldb_transaction_start(s->ldb) != LDB_SUCCESS) {
		return NT_STATUS_FOOBAR;
	}

        return NT_STATUS_OK;


}

NTSTATUS libnet_vampire_cb_check_options(void *private_data,
					 const struct libnet_BecomeDC_CheckOptions *o)
{
	struct libnet_vampire_cb_state *s = talloc_get_type(private_data, struct libnet_vampire_cb_state);

	DEBUG(0,("Become DC [%s] of Domain[%s]/[%s]\n",
		s->netbios_name,
		o->domain->netbios_name, o->domain->dns_name));

	DEBUG(0,("Promotion Partner is Server[%s] from Site[%s]\n",
		o->source_dsa->dns_name, o->source_dsa->site_name));

	DEBUG(0,("Options:crossRef behavior_version[%u]\n"
		       "\tschema object_version[%u]\n"
		       "\tdomain behavior_version[%u]\n"
		       "\tdomain w2k3_update_revision[%u]\n", 
		o->forest->crossref_behavior_version,
		o->forest->schema_object_version,
		o->domain->behavior_version,
		o->domain->w2k3_update_revision));

	return NT_STATUS_OK;
}

static NTSTATUS libnet_vampire_cb_apply_schema(struct libnet_vampire_cb_state *s,
					       const struct libnet_BecomeDC_StoreChunk *c)
{
	struct schema_list {
		struct schema_list *next, *prev;
		const struct drsuapi_DsReplicaObjectListItemEx *obj;
	};

	WERROR status;
	struct dsdb_schema_prefixmap *pfm_remote;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	struct schema_list *schema_list = NULL, *schema_list_item, *schema_list_next_item;
	struct dsdb_schema *working_schema;
	struct dsdb_schema *provision_schema;
	uint32_t object_count = 0;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t linked_attributes_count;
	struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;
	struct dsdb_extended_replicated_objects *schema_objs;
	struct repsFromTo1 *s_dsa;
	char *tmp_dns_name;
	struct ldb_context *schema_ldb;
	struct ldb_message *msg;
	struct ldb_message_element *prefixMap_el;
	uint32_t i;
	int ret, pass_no;
	bool ok;
	uint64_t seq_num;
	uint32_t ignore_attids[] = {
			DRSUAPI_ATTID_auxiliaryClass,
			DRSUAPI_ATTID_mayContain,
			DRSUAPI_ATTID_mustContain,
			DRSUAPI_ATTID_possSuperiors,
			DRSUAPI_ATTID_systemPossSuperiors,
			DRSUAPI_ATTID_INVALID
	};

	DEBUG(0,("Analyze and apply schema objects\n"));

	s_dsa			= talloc_zero(s, struct repsFromTo1);
	NT_STATUS_HAVE_NO_MEMORY(s_dsa);
	s_dsa->other_info	= talloc(s_dsa, struct repsFromTo1OtherInfo);
	NT_STATUS_HAVE_NO_MEMORY(s_dsa->other_info);

	switch (c->ctr_level) {
	case 1:
		mapping_ctr			= &c->ctr1->mapping_ctr;
		object_count			= s->schema_part.object_count;
		first_object			= s->schema_part.first_object;
		linked_attributes_count		= 0;
		linked_attributes		= NULL;
		s_dsa->highwatermark		= c->ctr1->new_highwatermark;
		s_dsa->source_dsa_obj_guid	= c->ctr1->source_dsa_guid;
		s_dsa->source_dsa_invocation_id = c->ctr1->source_dsa_invocation_id;
		uptodateness_vector		= NULL; /* TODO: map it */
		break;
	case 6:
		mapping_ctr			= &c->ctr6->mapping_ctr;
		object_count			= s->schema_part.object_count;
		first_object			= s->schema_part.first_object;
		linked_attributes_count		= c->ctr6->linked_attributes_count;
		linked_attributes		= c->ctr6->linked_attributes;
		s_dsa->highwatermark		= c->ctr6->new_highwatermark;
		s_dsa->source_dsa_obj_guid	= c->ctr6->source_dsa_guid;
		s_dsa->source_dsa_invocation_id = c->ctr6->source_dsa_invocation_id;
		uptodateness_vector		= c->ctr6->uptodateness_vector;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dsdb_schema_pfm_from_drsuapi_pfm(mapping_ctr, true,
						  s, &pfm_remote, NULL);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,(__location__ ": Failed to decode remote prefixMap: %s",
			 win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	s_dsa->replica_flags		= DRSUAPI_DRS_WRIT_REP
					| DRSUAPI_DRS_INIT_SYNC
					| DRSUAPI_DRS_PER_SYNC;
	memset(s_dsa->schedule, 0x11, sizeof(s_dsa->schedule));

	tmp_dns_name	= GUID_string(s_dsa->other_info, &s_dsa->source_dsa_obj_guid);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	tmp_dns_name	= talloc_asprintf_append_buffer(tmp_dns_name, "._msdcs.%s", c->forest->dns_name);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	s_dsa->other_info->dns_name = tmp_dns_name;

	schema_ldb = provision_get_schema(s, s->lp_ctx, &s->prefixmap_blob);
	if (!schema_ldb) {
		DEBUG(0,("Failed to re-load from local provision using remote prefixMap. "
			 "Will continue with local prefixMap\n"));
		provision_schema = dsdb_get_schema(s->ldb, s);
	} else {
		provision_schema = dsdb_get_schema(schema_ldb, s);
		ret = dsdb_reference_schema(s->ldb, provision_schema, false);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,("Failed to attach schema from local provision using remote prefixMap."));
			return NT_STATUS_UNSUCCESSFUL;
		}
		talloc_free(schema_ldb);
	}

	/* create a list of objects yet to be converted */
	for (cur = first_object; cur; cur = cur->next_object) {
		schema_list_item = talloc(s, struct schema_list);
		schema_list_item->obj = cur;
		DLIST_ADD_END(schema_list, schema_list_item, struct schema_list);
	}

	/* resolve objects until all are resolved and in local schema */
	pass_no = 1;
	working_schema = provision_schema;

	while (schema_list) {
		uint32_t converted_obj_count = 0;
		uint32_t failed_obj_count = 0;
		TALLOC_CTX *tmp_ctx = talloc_new(s);
		NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

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
			status = dsdb_convert_object_ex(s->ldb, working_schema, pfm_remote,
							cur, c->gensec_skey,
							ignore_attids,
							tmp_ctx, &object);
			if (!W_ERROR_IS_OK(status)) {
				DEBUG(1,("Warning: Failed to convert schema object %s into ldb msg\n",
					 cur->object.identifier->dn));

				failed_obj_count++;
			} else {
				/*
				 * Convert the schema from ldb_message format
				 * (OIDs as OID strings) into schema, using
				 * the remote prefixMap
				 */
				status = dsdb_schema_set_el_from_ldb_msg(s->ldb,
									 s->self_made_schema,
									 object.msg);
				if (!W_ERROR_IS_OK(status)) {
					DEBUG(1,("Warning: failed to convert object %s into a schema element: %s\n",
						 ldb_dn_get_linearized(object.msg->dn),
						 win_errstr(status)));
					failed_obj_count++;
				} else {
					DLIST_REMOVE(schema_list, schema_list_item);
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
			return NT_STATUS_INTERNAL_ERROR;
		}

		if (schema_list) {
			/* prepare for another cycle */
			working_schema = s->self_made_schema;

			ret = dsdb_setup_sorted_accessors(s->ldb, working_schema);
			if (LDB_SUCCESS != ret) {
				DEBUG(0,("Failed to create schema-cache indexes!\n"));
				return NT_STATUS_INTERNAL_ERROR;
			}
		}
	};

	/* free temp objects for 1st conversion phase */
	talloc_unlink(s, provision_schema);
	TALLOC_FREE(schema_list);

	/*
	 * attach the schema we just brought over DRS to the ldb,
	 * so we can use it in dsdb_convert_object_ex below
	 */
	ret = dsdb_set_schema(s->ldb, s->self_made_schema);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to attach working schema from DRS.\n"));
		return NT_STATUS_FOOBAR;
	}

	/* we don't want to access the self made schema anymore */
	s->schema = s->self_made_schema;
	s->self_made_schema = NULL;

	/* Now convert the schema elements again, using the schema we finalised, ready to actually import */
	status = dsdb_replicated_objects_convert(s->ldb,
						 s->schema,
						 c->partition->nc.dn,
						 mapping_ctr,
						 object_count,
						 first_object,
						 linked_attributes_count,
						 linked_attributes,
						 s_dsa,
						 uptodateness_vector,
						 c->gensec_skey,
						 s, &schema_objs);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to convert objects when trying to import over DRS (2nd pass, to store remote schema): %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	if (lpcfg_parm_bool(s->lp_ctx, NULL, "become dc", "dump objects", false)) {
		for (i=0; i < schema_objs->num_objects; i++) {
			struct ldb_ldif ldif;
			fprintf(stdout, "#\n");
			ldif.changetype = LDB_CHANGETYPE_NONE;
			ldif.msg = schema_objs->objects[i].msg;
			ldb_ldif_write_file(s->ldb, stdout, &ldif);
			NDR_PRINT_DEBUG(replPropertyMetaDataBlob, schema_objs->objects[i].meta_data);
		}
	}

	status = dsdb_replicated_objects_commit(s->ldb, NULL, schema_objs, &seq_num);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to commit objects: %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	msg = ldb_msg_new(schema_objs);
	NT_STATUS_HAVE_NO_MEMORY(msg);
	msg->dn = schema_objs->partition_dn;

	/* We must ensure a prefixMap has been written.  Unlike other
	 * attributes (including schemaInfo), it is not replicated in
	 * the normal replication stream.  We can use the one from
	 * s->prefixmap_blob because we operate with one, unchanging
	 * prefixMap for this entire operation.  */
	ret = ldb_msg_add_value(msg, "prefixMap", &s->prefixmap_blob, &prefixMap_el);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_FOOBAR;
	}
	/* We want to know if a prefixMap was written already, as it
	 * would mean that the above comment was not true, and we have
	 * somehow updated the prefixMap during this transaction */
	prefixMap_el->flags = LDB_FLAG_MOD_ADD;

	ret = ldb_modify(s->ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to add prefixMap: %s\n", ldb_errstring(s->ldb)));
		return NT_STATUS_FOOBAR;
	}

	talloc_free(s_dsa);
	talloc_free(schema_objs);

	/* We must set these up to ensure the replMetaData is written
	 * correctly, before our NTDS Settings entry is replicated */
	ok = samdb_set_ntds_invocation_id(s->ldb, &c->dest_dsa->invocation_id);
	if (!ok) {
		DEBUG(0,("Failed to set cached ntds invocationId\n"));
		return NT_STATUS_FOOBAR;
	}
	ok = samdb_set_ntds_objectGUID(s->ldb, &c->dest_dsa->ntds_guid);
	if (!ok) {
		DEBUG(0,("Failed to set cached ntds objectGUID\n"));
		return NT_STATUS_FOOBAR;
	}

	s->schema = dsdb_get_schema(s->ldb, s);
	if (!s->schema) {
		DEBUG(0,("Failed to get loaded dsdb_schema\n"));
		return NT_STATUS_FOOBAR;
	}

	return NT_STATUS_OK;
}

NTSTATUS libnet_vampire_cb_schema_chunk(void *private_data,
					const struct libnet_BecomeDC_StoreChunk *c)
{
	struct libnet_vampire_cb_state *s = talloc_get_type(private_data, struct libnet_vampire_cb_state);
	WERROR status;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	uint32_t nc_object_count;
	uint32_t object_count;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t nc_linked_attributes_count;
	uint32_t linked_attributes_count;
	struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;

	switch (c->ctr_level) {
	case 1:
		mapping_ctr			= &c->ctr1->mapping_ctr;
		nc_object_count			= c->ctr1->extended_ret; /* maybe w2k send this unexpected? */
		object_count			= c->ctr1->object_count;
		first_object			= c->ctr1->first_object;
		nc_linked_attributes_count	= 0;
		linked_attributes_count		= 0;
		linked_attributes		= NULL;
		break;
	case 6:
		mapping_ctr			= &c->ctr6->mapping_ctr;
		nc_object_count			= c->ctr6->nc_object_count;
		object_count			= c->ctr6->object_count;
		first_object			= c->ctr6->first_object;
		nc_linked_attributes_count	= c->ctr6->nc_linked_attributes_count;
		linked_attributes_count		= c->ctr6->linked_attributes_count;
		linked_attributes		= c->ctr6->linked_attributes;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (nc_object_count) {
		DEBUG(0,("Schema-DN[%s] objects[%u/%u] linked_values[%u/%u]\n",
			c->partition->nc.dn, object_count, nc_object_count,
			linked_attributes_count, nc_linked_attributes_count));
	} else {
		DEBUG(0,("Schema-DN[%s] objects[%u] linked_values[%u]\n",
		c->partition->nc.dn, object_count, linked_attributes_count));
	}

	if (!s->self_made_schema) {
		WERROR werr;
		struct drsuapi_DsReplicaOIDMapping_Ctr mapping_ctr_without_schema_info;
		/* Put the DRS prefixmap aside for the schema we are
		 * about to load in the provision, and into the one we
		 * are making with the help of DRS */

		mapping_ctr_without_schema_info = *mapping_ctr;

		/* This strips off the 0xFF schema info from the end,
		 * because we don't want it in the blob */
		if (mapping_ctr_without_schema_info.num_mappings > 0) {
			mapping_ctr_without_schema_info.num_mappings--;
		}
		werr = dsdb_get_drsuapi_prefixmap_as_blob(&mapping_ctr_without_schema_info, s, &s->prefixmap_blob);
		if (!W_ERROR_IS_OK(werr)) {
			return werror_to_ntstatus(werr);
		}

		/* Set up two manually-constructed schema - the local
		 * schema from the provision will be used to build
		 * one, which will then in turn be used to build the
		 * other. */
		s->self_made_schema = dsdb_new_schema(s);
		NT_STATUS_HAVE_NO_MEMORY(s->self_made_schema);

		status = dsdb_load_prefixmap_from_drsuapi(s->self_made_schema, mapping_ctr);
		if (!W_ERROR_IS_OK(status)) {
			return werror_to_ntstatus(status);
		}
	} else {
		status = dsdb_schema_pfm_contains_drsuapi_pfm(s->self_made_schema->prefixmap, mapping_ctr);
		if (!W_ERROR_IS_OK(status)) {
			return werror_to_ntstatus(status);
		}
	}

	if (!s->schema_part.first_object) {
		s->schema_part.object_count = object_count;
		s->schema_part.first_object = talloc_steal(s, first_object);
	} else {
		s->schema_part.object_count		+= object_count;
		s->schema_part.last_object->next_object = talloc_steal(s->schema_part.last_object,
								       first_object);
	}
	for (cur = first_object; cur->next_object; cur = cur->next_object) {}
	s->schema_part.last_object = cur;

	if (!c->partition->more_data) {
		return libnet_vampire_cb_apply_schema(s, c);
	}

	return NT_STATUS_OK;
}

NTSTATUS libnet_vampire_cb_store_chunk(void *private_data,
			     const struct libnet_BecomeDC_StoreChunk *c)
{
	struct libnet_vampire_cb_state *s = talloc_get_type(private_data, struct libnet_vampire_cb_state);
	WERROR status;
	struct dsdb_schema *schema;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	uint32_t nc_object_count;
	uint32_t object_count;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	uint32_t nc_linked_attributes_count;
	uint32_t linked_attributes_count;
	struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;
	struct dsdb_extended_replicated_objects *objs;
	struct repsFromTo1 *s_dsa;
	char *tmp_dns_name;
	uint32_t i;
	uint64_t seq_num;

	s_dsa			= talloc_zero(s, struct repsFromTo1);
	NT_STATUS_HAVE_NO_MEMORY(s_dsa);
	s_dsa->other_info	= talloc(s_dsa, struct repsFromTo1OtherInfo);
	NT_STATUS_HAVE_NO_MEMORY(s_dsa->other_info);

	switch (c->ctr_level) {
	case 1:
		mapping_ctr			= &c->ctr1->mapping_ctr;
		nc_object_count			= c->ctr1->extended_ret; /* maybe w2k send this unexpected? */
		object_count			= c->ctr1->object_count;
		first_object			= c->ctr1->first_object;
		nc_linked_attributes_count	= 0;
		linked_attributes_count		= 0;
		linked_attributes		= NULL;
		s_dsa->highwatermark		= c->ctr1->new_highwatermark;
		s_dsa->source_dsa_obj_guid	= c->ctr1->source_dsa_guid;
		s_dsa->source_dsa_invocation_id = c->ctr1->source_dsa_invocation_id;
		uptodateness_vector		= NULL; /* TODO: map it */
		break;
	case 6:
		mapping_ctr			= &c->ctr6->mapping_ctr;
		nc_object_count			= c->ctr6->nc_object_count;
		object_count			= c->ctr6->object_count;
		first_object			= c->ctr6->first_object;
		nc_linked_attributes_count	= c->ctr6->nc_linked_attributes_count;
		linked_attributes_count		= c->ctr6->linked_attributes_count;
		linked_attributes		= c->ctr6->linked_attributes;
		s_dsa->highwatermark		= c->ctr6->new_highwatermark;
		s_dsa->source_dsa_obj_guid	= c->ctr6->source_dsa_guid;
		s_dsa->source_dsa_invocation_id = c->ctr6->source_dsa_invocation_id;
		uptodateness_vector		= c->ctr6->uptodateness_vector;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	s_dsa->replica_flags		= DRSUAPI_DRS_WRIT_REP
					| DRSUAPI_DRS_INIT_SYNC
					| DRSUAPI_DRS_PER_SYNC;
	memset(s_dsa->schedule, 0x11, sizeof(s_dsa->schedule));

	tmp_dns_name	= GUID_string(s_dsa->other_info, &s_dsa->source_dsa_obj_guid);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	tmp_dns_name	= talloc_asprintf_append_buffer(tmp_dns_name, "._msdcs.%s", c->forest->dns_name);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	s_dsa->other_info->dns_name = tmp_dns_name;

	/* we want to show a count per partition */
	if (!s->last_partition || strcmp(s->last_partition, c->partition->nc.dn) != 0) {
		s->total_objects = 0;
		talloc_free(s->last_partition);
		s->last_partition = talloc_strdup(s, c->partition->nc.dn);
	}
	s->total_objects += object_count;

	if (nc_object_count) {
		DEBUG(0,("Partition[%s] objects[%u/%u] linked_values[%u/%u]\n",
			c->partition->nc.dn, s->total_objects, nc_object_count,
			linked_attributes_count, nc_linked_attributes_count));
	} else {
		DEBUG(0,("Partition[%s] objects[%u] linked_values[%u]\n",
		c->partition->nc.dn, s->total_objects, linked_attributes_count));
	}


	schema = dsdb_get_schema(s->ldb, NULL);
	if (!schema) {
		DEBUG(0,(__location__ ": Schema is not loaded yet!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = dsdb_replicated_objects_convert(s->ldb,
						 schema,
						 c->partition->nc.dn,
						 mapping_ctr,
						 object_count,
						 first_object,
						 linked_attributes_count,
						 linked_attributes,
						 s_dsa,
						 uptodateness_vector,
						 c->gensec_skey,
						 s, &objs);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to convert objects: %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	if (lpcfg_parm_bool(s->lp_ctx, NULL, "become dc", "dump objects", false)) {
		for (i=0; i < objs->num_objects; i++) {
			struct ldb_ldif ldif;
			fprintf(stdout, "#\n");
			ldif.changetype = LDB_CHANGETYPE_NONE;
			ldif.msg = objs->objects[i].msg;
			ldb_ldif_write_file(s->ldb, stdout, &ldif);
			NDR_PRINT_DEBUG(replPropertyMetaDataBlob, objs->objects[i].meta_data);
		}
	}
	status = dsdb_replicated_objects_commit(s->ldb, NULL, objs, &seq_num);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to commit objects: %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	talloc_free(s_dsa);
	talloc_free(objs);

	for (i=0; i < linked_attributes_count; i++) {
		const struct dsdb_attribute *sa;

		if (!linked_attributes[i].identifier) {
			return NT_STATUS_FOOBAR;		
		}

		if (!linked_attributes[i].value.blob) {
			return NT_STATUS_FOOBAR;		
		}

		sa = dsdb_attribute_by_attributeID_id(s->schema,
						      linked_attributes[i].attid);
		if (!sa) {
			return NT_STATUS_FOOBAR;
		}

		if (lpcfg_parm_bool(s->lp_ctx, NULL, "become dc", "dump objects", false)) {
			DEBUG(0,("# %s\n", sa->lDAPDisplayName));
			NDR_PRINT_DEBUG(drsuapi_DsReplicaLinkedAttribute, &linked_attributes[i]);
			dump_data(0,
				linked_attributes[i].value.blob->data,
				linked_attributes[i].value.blob->length);
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS update_dnshostname_for_server(TALLOC_CTX *mem_ctx,
					      struct ldb_context *ldb,
					      const char *server_dn_str,
					      const char *netbios_name,
					      const char *realm)
{
	int ret;
	struct ldb_message *msg;
	struct ldb_message_element *el;
	struct ldb_dn *server_dn;
	const char *dNSHostName = strlower_talloc(mem_ctx,
						  talloc_asprintf(mem_ctx,
								  "%s.%s",
								  netbios_name,
								  realm));
	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	server_dn = ldb_dn_new(mem_ctx, ldb, server_dn_str);
	if (!server_dn) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	msg->dn = server_dn;
	ret = ldb_msg_add_empty(msg, "dNSHostName", LDB_FLAG_MOD_ADD, &el);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ret = ldb_msg_add_steal_string(msg,
				       "dNSHostName",
				       talloc_asprintf(el->values, "%s", dNSHostName));
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ret = dsdb_modify(ldb, msg, DSDB_MODIFY_PERMISSIVE);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to add dnsHostName to the Server object: %s\n",
			 ldb_errstring(ldb)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	return NT_STATUS_OK;
}


NTSTATUS libnet_Vampire(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, 
			struct libnet_Vampire *r)
{
	struct libnet_JoinDomain *join;
	struct libnet_Replicate rep;
	NTSTATUS status;

	const char *account_name;
	const char *netbios_name;
	
	r->out.error_string = NULL;

	join = talloc_zero(mem_ctx, struct libnet_JoinDomain);
	if (!join) {
		return NT_STATUS_NO_MEMORY;
	}
		
	if (r->in.netbios_name != NULL) {
		netbios_name = r->in.netbios_name;
	} else {
		netbios_name = talloc_reference(join, lpcfg_netbios_name(ctx->lp_ctx));
		if (!netbios_name) {
			talloc_free(join);
			r->out.error_string = NULL;
			return NT_STATUS_NO_MEMORY;
		}
	}

	account_name = talloc_asprintf(join, "%s$", netbios_name);
	if (!account_name) {
		talloc_free(join);
		r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	
	/* Re-use the domain we are joining as the domain for the user
	 * to be authenticated with, unless they specified
	 * otherwise */
	cli_credentials_set_domain(ctx->cred, r->in.domain_name, CRED_GUESS_ENV);

	join->in.domain_name	= r->in.domain_name;
	join->in.account_name	= account_name;
	join->in.netbios_name	= netbios_name;
	join->in.level		= LIBNET_JOINDOMAIN_AUTOMATIC;
	join->in.acct_type	= ACB_WSTRUST;
	join->in.recreate_account = false;
	status = libnet_JoinDomain(ctx, join, join);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_steal(mem_ctx, join->out.error_string);
		talloc_free(join);
		return status;
	}

	rep.in.domain_name   = join->out.domain_name;
	rep.in.netbios_name  = netbios_name;
	rep.in.targetdir     = r->in.targetdir;
	rep.in.domain_sid    = join->out.domain_sid;
	rep.in.realm         = join->out.realm;
	rep.in.server        = join->out.samr_binding->host;
	rep.in.join_password = join->out.join_password;
	rep.in.kvno          = join->out.kvno;

	status = libnet_Replicate(ctx, mem_ctx, &rep);

	r->out.domain_sid   = join->out.domain_sid;
	r->out.domain_name  = join->out.domain_name;
	r->out.error_string = rep.out.error_string;

	return status;
}



NTSTATUS libnet_Replicate(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			  struct libnet_Replicate *r)
{
	struct provision_store_self_join_settings *set_secrets;
	struct libnet_BecomeDC b;
	struct libnet_vampire_cb_state *s;
	struct ldb_message *msg;
	const char *error_string;
	int ldb_ret;
	uint32_t i;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	const char *account_name;
	const char *netbios_name;

	r->out.error_string = NULL;

	netbios_name = r->in.netbios_name;
	account_name = talloc_asprintf(tmp_ctx, "%s$", netbios_name);
	if (!account_name) {
		talloc_free(tmp_ctx);
		r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	
	/* Re-use the domain we are joining as the domain for the user
	 * to be authenticated with, unless they specified
	 * otherwise */
	cli_credentials_set_domain(ctx->cred, r->in.domain_name, CRED_GUESS_ENV);

	s = libnet_vampire_cb_state_init(mem_ctx, ctx->lp_ctx, ctx->event_ctx,
					 netbios_name, r->in.domain_name, r->in.realm,
					 r->in.targetdir);
	if (!s) {
		return NT_STATUS_NO_MEMORY;
	}
	talloc_steal(s, tmp_ctx);

	ZERO_STRUCT(b);

	/* Be more robust:
	 * We now know the domain and realm for sure - if they didn't
	 * put one on the command line, use this for the rest of the
	 * join */
	cli_credentials_set_realm(ctx->cred, r->in.realm, CRED_GUESS_ENV);
	cli_credentials_set_domain(ctx->cred, r->in.domain_name, CRED_GUESS_ENV);

	/* Now set these values into the smb.conf - we probably had
	 * empty or useless defaults here from whatever smb.conf we
	 * started with */
	lpcfg_set_cmdline(s->lp_ctx, "realm", r->in.realm);
	lpcfg_set_cmdline(s->lp_ctx, "workgroup", r->in.domain_name);

	b.in.domain_dns_name		= r->in.realm;
	b.in.domain_netbios_name	= r->in.domain_name;
	b.in.domain_sid			= r->in.domain_sid;
	b.in.source_dsa_address		= r->in.server;
	b.in.dest_dsa_netbios_name	= netbios_name;

	b.in.callbacks.private_data	= s;
	b.in.callbacks.check_options	= libnet_vampire_cb_check_options;
	b.in.callbacks.prepare_db       = libnet_vampire_cb_prepare_db;
	b.in.callbacks.schema_chunk	= libnet_vampire_cb_schema_chunk;
	b.in.callbacks.config_chunk	= libnet_vampire_cb_store_chunk;
	b.in.callbacks.domain_chunk	= libnet_vampire_cb_store_chunk;

	b.in.rodc_join = lpcfg_parm_bool(s->lp_ctx, NULL, "repl", "RODC", false);

	status = libnet_BecomeDC(ctx, s, &b);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_BecomeDC() failed - %s\n", nt_errstr(status));
		talloc_free(s);
		return status;
	}

	msg = ldb_msg_new(s);
	if (!msg) {
		printf("ldb_msg_new() failed\n");
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = ldb_dn_new(msg, s->ldb, "@ROOTDSE");
	if (!msg->dn) {
		printf("ldb_msg_new(@ROOTDSE) failed\n");
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}

	ldb_ret = ldb_msg_add_string(msg, "isSynchronized", "TRUE");
	if (ldb_ret != LDB_SUCCESS) {
		printf("ldb_msg_add_string(msg, isSynchronized, TRUE) failed: %d\n", ldb_ret);
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i < msg->num_elements; i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	printf("mark ROOTDSE with isSynchronized=TRUE\n");
	ldb_ret = ldb_modify(s->ldb, msg);
	if (ldb_ret != LDB_SUCCESS) {
		printf("ldb_modify() failed: %d : %s\n", ldb_ret, ldb_errstring(s->ldb));
		talloc_free(s);
		return NT_STATUS_INTERNAL_DB_ERROR;
	}
	/* during dcpromo the 2nd computer adds dNSHostName attribute to his Server object
	 * the attribute appears on the original DC after replication
	 */
	status = update_dnshostname_for_server(s, s->ldb, s->server_dn_str, s->netbios_name, s->realm);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to update dNSHostName on Server object - %s\n", nt_errstr(status));
		talloc_free(s);
		return status;
	}
	/* prepare the transaction - this prepares to commit all the changes in
	   the ldb from the whole vampire.  Note that this 
	   triggers the writing of the linked attribute backlinks.
	*/
	if (ldb_transaction_prepare_commit(s->ldb) != LDB_SUCCESS) {
		printf("Failed to prepare_commit vampire transaction: %s\n", ldb_errstring(s->ldb));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	set_secrets = talloc(s, struct provision_store_self_join_settings);
	if (!set_secrets) {
		r->out.error_string = NULL;
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}
	
	ZERO_STRUCTP(set_secrets);
	set_secrets->domain_name = r->in.domain_name;
	set_secrets->realm = r->in.realm;
	set_secrets->netbios_name = netbios_name;
	set_secrets->secure_channel_type = SEC_CHAN_BDC;
	set_secrets->machine_password = r->in.join_password;
	set_secrets->key_version_number = r->in.kvno;
	set_secrets->domain_sid = r->in.domain_sid;
	
	status = provision_store_self_join(ctx, s->lp_ctx, ctx->event_ctx, set_secrets, &error_string);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_steal(mem_ctx, error_string);
		talloc_free(s);
		return status;
	}

	/* commit the transaction now we know the secrets were written
	 * out properly
	*/
	if (ldb_transaction_commit(s->ldb) != LDB_SUCCESS) {
		printf("Failed to commit vampire transaction\n");
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	talloc_free(s);

	return NT_STATUS_OK;
}
