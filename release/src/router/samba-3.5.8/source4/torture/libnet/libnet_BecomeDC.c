/* 
   Unix SMB/CIFS implementation.

   libnet_BecomeDC() tests

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006
   
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
#include "lib/cmdline/popt_common.h"
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "libnet/libnet.h"
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "../lib/util/dlinklist.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "system/time.h"
#include "lib/ldb_wrap.h"
#include "auth/auth.h"
#include "param/param.h"
#include "torture/util.h"
#include "param/provision.h"

struct test_become_dc_state {
	struct libnet_context *ctx;
	struct torture_context *tctx;
	const char *netbios_name;
	struct test_join *tj;
	struct cli_credentials *machine_account;
	struct dsdb_schema *self_made_schema;
	const struct dsdb_schema *schema;

	struct ldb_context *ldb;

	struct {
		uint32_t object_count;
		struct drsuapi_DsReplicaObjectListItemEx *first_object;
		struct drsuapi_DsReplicaObjectListItemEx *last_object;
	} schema_part;

	const char *targetdir;

	struct loadparm_context *lp_ctx;
};

static NTSTATUS test_become_dc_prepare_db(void *private_data,
					      const struct libnet_BecomeDC_PrepareDB *p)
{
	struct test_become_dc_state *s = talloc_get_type(private_data, struct test_become_dc_state);
	struct provision_settings settings;
	struct provision_result result;
	NTSTATUS status;

	settings.site_name = p->dest_dsa->site_name;
	settings.root_dn_str = p->forest->root_dn_str;
	settings.domain_dn_str = p->domain->dn_str;
	settings.config_dn_str = p->forest->config_dn_str;
	settings.schema_dn_str = p->forest->schema_dn_str;
	settings.server_dn_str = torture_join_server_dn_str(s->tj);
	settings.invocation_id = &p->dest_dsa->invocation_id;
	settings.netbios_name = p->dest_dsa->netbios_name;
	settings.host_ip = NULL;
	settings.realm = torture_join_dom_dns_name(s->tj);
	settings.domain = torture_join_dom_netbios_name(s->tj);
	settings.ntds_dn_str = p->dest_dsa->ntds_dn_str;
	settings.machine_password = cli_credentials_get_password(s->machine_account);
	settings.targetdir = s->targetdir;

	status = provision_bare(s, s->lp_ctx, &settings, &result);
	
	s->ldb = result.samdb;
	s->lp_ctx = result.lp_ctx;
        return NT_STATUS_OK;


}

static NTSTATUS test_become_dc_check_options(void *private_data,
					     const struct libnet_BecomeDC_CheckOptions *o)
{
	struct test_become_dc_state *s = talloc_get_type(private_data, struct test_become_dc_state);

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

static NTSTATUS test_apply_schema(struct test_become_dc_state *s,
				  const struct libnet_BecomeDC_StoreChunk *c)
{
	WERROR status;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	uint32_t object_count;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t linked_attributes_count;
	struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;
	struct dsdb_extended_replicated_objects *objs;
	struct repsFromTo1 *s_dsa;
	char *tmp_dns_name;
	struct ldb_message *msg;
	struct ldb_val prefixMap_val;
	struct ldb_message_element *prefixMap_el;
	struct ldb_val schemaInfo_val;
	char *sam_ldb_path;
	uint32_t i;
	int ret;
	bool ok;
	uint64_t seq_num;

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
		linked_attributes_count		= 0; /* TODO: ! */
		linked_attributes		= NULL; /* TODO: ! */;
		s_dsa->highwatermark		= c->ctr6->new_highwatermark;
		s_dsa->source_dsa_obj_guid	= c->ctr6->source_dsa_guid;
		s_dsa->source_dsa_invocation_id = c->ctr6->source_dsa_invocation_id;
		uptodateness_vector		= c->ctr6->uptodateness_vector;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	s_dsa->replica_flags		= DRSUAPI_DS_REPLICA_NEIGHBOUR_WRITEABLE
					| DRSUAPI_DS_REPLICA_NEIGHBOUR_SYNC_ON_STARTUP
					| DRSUAPI_DS_REPLICA_NEIGHBOUR_DO_SCHEDULED_SYNCS;
	memset(s_dsa->schedule, 0x11, sizeof(s_dsa->schedule));

	tmp_dns_name	= GUID_string(s_dsa->other_info, &s_dsa->source_dsa_obj_guid);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	tmp_dns_name	= talloc_asprintf_append_buffer(tmp_dns_name, "._msdcs.%s", c->forest->dns_name);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	s_dsa->other_info->dns_name = tmp_dns_name;

	for (cur = first_object; cur; cur = cur->next_object) {
		bool is_attr = false;
		bool is_class = false;

		for (i=0; i < cur->object.attribute_ctr.num_attributes; i++) {
			struct drsuapi_DsReplicaAttribute *a;
			uint32_t j;
			const char *oid = NULL;

			a = &cur->object.attribute_ctr.attributes[i];
			status = dsdb_map_int2oid(s->self_made_schema, a->attid, s, &oid);
			if (!W_ERROR_IS_OK(status)) {
				return werror_to_ntstatus(status);
			}

			switch (a->attid) {
			case DRSUAPI_ATTRIBUTE_objectClass:
				for (j=0; j < a->value_ctr.num_values; j++) {
					uint32_t val = 0xFFFFFFFF;

					if (a->value_ctr.values[i].blob
					    && a->value_ctr.values[i].blob->length == 4) {
						val = IVAL(a->value_ctr.values[i].blob->data,0);
					}

					if (val == DRSUAPI_OBJECTCLASS_attributeSchema) {
						is_attr = true;
					}
					if (val == DRSUAPI_OBJECTCLASS_classSchema) {
						is_class = true;
					}
				}

				break;
			default:
				break;
			}
		}

		if (is_attr) {
			struct dsdb_attribute *sa;

			sa = talloc_zero(s->self_made_schema, struct dsdb_attribute);
			NT_STATUS_HAVE_NO_MEMORY(sa);

			status = dsdb_attribute_from_drsuapi(s->ldb, s->self_made_schema, &cur->object, s, sa);
			if (!W_ERROR_IS_OK(status)) {
				return werror_to_ntstatus(status);
			}

			DLIST_ADD_END(s->self_made_schema->attributes, sa, struct dsdb_attribute *);
		}

		if (is_class) {
			struct dsdb_class *sc;

			sc = talloc_zero(s->self_made_schema, struct dsdb_class);
			NT_STATUS_HAVE_NO_MEMORY(sc);

			status = dsdb_class_from_drsuapi(s->self_made_schema, &cur->object, s, sc);
			if (!W_ERROR_IS_OK(status)) {
				return werror_to_ntstatus(status);
			}

			DLIST_ADD_END(s->self_made_schema->classes, sc, struct dsdb_class *);
		}
	}

	/* attach the schema to the ldb */
	ret = dsdb_set_schema(s->ldb, s->self_made_schema);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_FOOBAR;
	}
	/* we don't want to access the self made schema anymore */
	s->self_made_schema = NULL;
	s->schema = dsdb_get_schema(s->ldb);

	status = dsdb_extended_replicated_objects_commit(s->ldb,
							 c->partition->nc.dn,
							 mapping_ctr,
							 object_count,
							 first_object,
							 linked_attributes_count,
							 linked_attributes,
							 s_dsa,
							 uptodateness_vector,
							 c->gensec_skey,
							 s, &objs, &seq_num);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to commit objects: %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	if (lp_parm_bool(s->tctx->lp_ctx, NULL, "become dc", "dump objects", false)) {
		for (i=0; i < objs->num_objects; i++) {
			struct ldb_ldif ldif;
			fprintf(stdout, "#\n");
			ldif.changetype = LDB_CHANGETYPE_NONE;
			ldif.msg = objs->objects[i].msg;
			ldb_ldif_write_file(s->ldb, stdout, &ldif);
			NDR_PRINT_DEBUG(replPropertyMetaDataBlob, objs->objects[i].meta_data);
		}
	}

	msg = ldb_msg_new(objs);
	NT_STATUS_HAVE_NO_MEMORY(msg);
	msg->dn = objs->partition_dn;

	status = dsdb_get_oid_mappings_ldb(s->schema, msg, &prefixMap_val, &schemaInfo_val);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed dsdb_get_oid_mappings_ldb(%s)\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	/* we only add prefixMap here, because schemaInfo is a replicated attribute and already applied */
	ret = ldb_msg_add_value(msg, "prefixMap", &prefixMap_val, &prefixMap_el);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_FOOBAR;
	}
	prefixMap_el->flags = LDB_FLAG_MOD_REPLACE;

	ret = ldb_modify(s->ldb, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to add prefixMap and schemaInfo %s\n", ldb_strerror(ret)));
		return NT_STATUS_FOOBAR;
	}

	talloc_free(s_dsa);
	talloc_free(objs);

	/* reopen the ldb */
	talloc_free(s->ldb); /* this also free's the s->schema, because dsdb_set_schema() steals it */
	s->schema = NULL;

	sam_ldb_path = talloc_asprintf(s, "%s/%s", s->targetdir, "private/sam.ldb");
	DEBUG(0,("Reopen the SAM LDB with system credentials and a already stored schema: %s\n", sam_ldb_path));
	s->ldb = ldb_wrap_connect(s, s->tctx->ev, s->tctx->lp_ctx, sam_ldb_path,
				  system_session(s, s->tctx->lp_ctx),
				  NULL, 0, NULL);
	if (!s->ldb) {
		DEBUG(0,("Failed to open '%s'\n",
			sam_ldb_path));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

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

	s->schema = dsdb_get_schema(s->ldb);
	if (!s->schema) {
		DEBUG(0,("Failed to get loaded dsdb_schema\n"));
		return NT_STATUS_FOOBAR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS test_become_dc_schema_chunk(void *private_data,
					    const struct libnet_BecomeDC_StoreChunk *c)
{
	struct test_become_dc_state *s = talloc_get_type(private_data, struct test_become_dc_state);
	WERROR status;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	uint32_t nc_object_count;
	uint32_t object_count;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	struct drsuapi_DsReplicaObjectListItemEx *cur;
	uint32_t nc_linked_attributes_count;
	uint32_t linked_attributes_count;

	switch (c->ctr_level) {
	case 1:
		mapping_ctr			= &c->ctr1->mapping_ctr;
		nc_object_count			= c->ctr1->extended_ret; /* maybe w2k send this unexpected? */
		object_count			= c->ctr1->object_count;
		first_object			= c->ctr1->first_object;
		nc_linked_attributes_count	= 0;
		linked_attributes_count		= 0;
		break;
	case 6:
		mapping_ctr			= &c->ctr6->mapping_ctr;
		nc_object_count			= c->ctr6->nc_object_count;
		object_count			= c->ctr6->object_count;
		first_object			= c->ctr6->first_object;
		nc_linked_attributes_count	= c->ctr6->nc_linked_attributes_count;
		linked_attributes_count		= c->ctr6->linked_attributes_count;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (nc_object_count) {
		DEBUG(0,("Schema-DN[%s] objects[%u/%u] linked_values[%u/%u]\n",
			c->partition->nc.dn, object_count, nc_object_count,
			linked_attributes_count, nc_linked_attributes_count));
	} else {
		DEBUG(0,("Schema-DN[%s] objects[%u] linked_values[%u\n",
		c->partition->nc.dn, object_count, linked_attributes_count));
	}

	if (!s->schema) {
		s->self_made_schema = dsdb_new_schema(s, lp_iconv_convenience(s->lp_ctx));

		NT_STATUS_HAVE_NO_MEMORY(s->self_made_schema);

		status = dsdb_load_oid_mappings_drsuapi(s->self_made_schema, mapping_ctr);
		if (!W_ERROR_IS_OK(status)) {
			return werror_to_ntstatus(status);
		}

		s->schema = s->self_made_schema;
	} else {
		status = dsdb_verify_oid_mappings_drsuapi(s->schema, mapping_ctr);
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
		return test_apply_schema(s, c);
	}

	return NT_STATUS_OK;
}

static NTSTATUS test_become_dc_store_chunk(void *private_data,
					   const struct libnet_BecomeDC_StoreChunk *c)
{
	struct test_become_dc_state *s = talloc_get_type(private_data, struct test_become_dc_state);
	WERROR status;
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

	s_dsa->replica_flags		= DRSUAPI_DS_REPLICA_NEIGHBOUR_WRITEABLE
					| DRSUAPI_DS_REPLICA_NEIGHBOUR_SYNC_ON_STARTUP
					| DRSUAPI_DS_REPLICA_NEIGHBOUR_DO_SCHEDULED_SYNCS;
	memset(s_dsa->schedule, 0x11, sizeof(s_dsa->schedule));

	tmp_dns_name	= GUID_string(s_dsa->other_info, &s_dsa->source_dsa_obj_guid);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	tmp_dns_name	= talloc_asprintf_append_buffer(tmp_dns_name, "._msdcs.%s", c->forest->dns_name);
	NT_STATUS_HAVE_NO_MEMORY(tmp_dns_name);
	s_dsa->other_info->dns_name = tmp_dns_name;

	if (nc_object_count) {
		DEBUG(0,("Partition[%s] objects[%u/%u] linked_values[%u/%u]\n",
			c->partition->nc.dn, object_count, nc_object_count,
			linked_attributes_count, nc_linked_attributes_count));
	} else {
		DEBUG(0,("Partition[%s] objects[%u] linked_values[%u\n",
		c->partition->nc.dn, object_count, linked_attributes_count));
	}

	status = dsdb_extended_replicated_objects_commit(s->ldb,
							 c->partition->nc.dn,
							 mapping_ctr,
							 object_count,
							 first_object,
							 linked_attributes_count,
							 linked_attributes,
							 s_dsa,
							 uptodateness_vector,
							 c->gensec_skey,
							 s, &objs, &seq_num);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to commit objects: %s\n", win_errstr(status)));
		return werror_to_ntstatus(status);
	}

	if (lp_parm_bool(s->tctx->lp_ctx, NULL, "become dc", "dump objects", false)) {
		for (i=0; i < objs->num_objects; i++) {
			struct ldb_ldif ldif;
			fprintf(stdout, "#\n");
			ldif.changetype = LDB_CHANGETYPE_NONE;
			ldif.msg = objs->objects[i].msg;
			ldb_ldif_write_file(s->ldb, stdout, &ldif);
			NDR_PRINT_DEBUG(replPropertyMetaDataBlob, objs->objects[i].meta_data);
		}
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

		if (lp_parm_bool(s->tctx->lp_ctx, NULL, "become dc", "dump objects", false)) {
			DEBUG(0,("# %s\n", sa->lDAPDisplayName));
			NDR_PRINT_DEBUG(drsuapi_DsReplicaLinkedAttribute, &linked_attributes[i]);
			dump_data(0,
				linked_attributes[i].value.blob->data,
				linked_attributes[i].value.blob->length);
		}
	}

	return NT_STATUS_OK;
}

bool torture_net_become_dc(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	struct libnet_BecomeDC b;
	struct libnet_UnbecomeDC u;
	struct test_become_dc_state *s;
	struct ldb_message *msg;
	int ldb_ret;
	uint32_t i;
	char *sam_ldb_path;

	char *location = NULL;
	torture_assert_ntstatus_ok(torture, torture_temp_dir(torture, "libnet_BecomeDC", &location), 
				   "torture_temp_dir should return NT_STATUS_OK" );

	s = talloc_zero(torture, struct test_become_dc_state);
	if (!s) return false;

	s->tctx = torture;
	s->lp_ctx = torture->lp_ctx;

	s->netbios_name = lp_parm_string(torture->lp_ctx, NULL, "become dc", "smbtorture dc");
	if (!s->netbios_name || !s->netbios_name[0]) {
		s->netbios_name = "smbtorturedc";
	}

	s->targetdir = location;

	/* Join domain as a member server. */
	s->tj = torture_join_domain(torture, s->netbios_name,
				 ACB_WSTRUST,
				 &s->machine_account);
	if (!s->tj) {
		DEBUG(0, ("%s failed to join domain as workstation\n",
			  s->netbios_name));
		return false;
	}

	s->ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	s->ctx->cred = cmdline_credentials;

	s->ldb = ldb_init(s, torture->ev);

	ZERO_STRUCT(b);
	b.in.domain_dns_name		= torture_join_dom_dns_name(s->tj);
	b.in.domain_netbios_name	= torture_join_dom_netbios_name(s->tj);
	b.in.domain_sid			= torture_join_sid(s->tj);
	b.in.source_dsa_address		= torture_setting_string(torture, "host", NULL);
	b.in.dest_dsa_netbios_name	= s->netbios_name;

	b.in.callbacks.private_data	= s;
	b.in.callbacks.check_options	= test_become_dc_check_options;
	b.in.callbacks.prepare_db = test_become_dc_prepare_db;
	b.in.callbacks.schema_chunk	= test_become_dc_schema_chunk;
	b.in.callbacks.config_chunk	= test_become_dc_store_chunk;
	b.in.callbacks.domain_chunk	= test_become_dc_store_chunk;

	status = libnet_BecomeDC(s->ctx, s, &b);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_BecomeDC() failed - %s\n", nt_errstr(status));
		ret = false;
		goto cleanup;
	}

	msg = ldb_msg_new(s);
	if (!msg) {
		printf("ldb_msg_new() failed\n");
		ret = false;
		goto cleanup;
	}
	msg->dn = ldb_dn_new(msg, s->ldb, "@ROOTDSE");
	if (!msg->dn) {
		printf("ldb_msg_new(@ROOTDSE) failed\n");
		ret = false;
		goto cleanup;
	}

	ldb_ret = ldb_msg_add_string(msg, "isSynchronized", "TRUE");
	if (ldb_ret != LDB_SUCCESS) {
		printf("ldb_msg_add_string(msg, isSynchronized, TRUE) failed: %d\n", ldb_ret);
		ret = false;
		goto cleanup;
	}

	for (i=0; i < msg->num_elements; i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	printf("mark ROOTDSE with isSynchronized=TRUE\n");
	ldb_ret = ldb_modify(s->ldb, msg);
	if (ldb_ret != LDB_SUCCESS) {
		printf("ldb_modify() failed: %d\n", ldb_ret);
		ret = false;
		goto cleanup;
	}
	
	/* reopen the ldb */
	talloc_free(s->ldb); /* this also free's the s->schema, because dsdb_set_schema() steals it */
	s->schema = NULL;

	sam_ldb_path = talloc_asprintf(s, "%s/%s", s->targetdir, "private/sam.ldb");
	DEBUG(0,("Reopen the SAM LDB with system credentials and all replicated data: %s\n", sam_ldb_path));
	s->ldb = ldb_wrap_connect(s, s->tctx->ev, s->lp_ctx, sam_ldb_path,
				  system_session(s, s->lp_ctx),
				  NULL, 0, NULL);
	if (!s->ldb) {
		DEBUG(0,("Failed to open '%s'\n",
			sam_ldb_path));
		ret = false;
		goto cleanup;
	}

	s->schema = dsdb_get_schema(s->ldb);
	if (!s->schema) {
		DEBUG(0,("Failed to get loaded dsdb_schema\n"));
		ret = false;
		goto cleanup;
	}

	/* Make sure we get this from the command line */
	if (lp_parm_bool(torture->lp_ctx, NULL, "become dc", "do not unjoin", false)) {
		talloc_free(s);
		return ret;
	}

cleanup:
	ZERO_STRUCT(u);
	u.in.domain_dns_name		= torture_join_dom_dns_name(s->tj);
	u.in.domain_netbios_name	= torture_join_dom_netbios_name(s->tj);
	u.in.source_dsa_address		= torture_setting_string(torture, "host", NULL);
	u.in.dest_dsa_netbios_name	= s->netbios_name;

	status = libnet_UnbecomeDC(s->ctx, s, &u);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_UnbecomeDC() failed - %s\n", nt_errstr(status));
		ret = false;
	}

	/* Leave domain. */                          
	torture_leave_domain(torture, s->tj);

	talloc_free(s);
	return ret;
}
