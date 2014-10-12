/*
   Unix SMB/CIFS implementation.

   msDS-IntId attribute replication test.

   Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010

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
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/cldap/cldap.h"
#include "torture/torture.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/drs/proto.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/resolve/resolve.h"

struct DsSyncBindInfo {
	struct dcerpc_pipe *drs_pipe;
	struct dcerpc_binding_handle *drs_handle;
	struct drsuapi_DsBind req;
	struct GUID bind_guid;
	struct drsuapi_DsBindInfoCtr our_bind_info_ctr;
	struct drsuapi_DsBindInfo28 our_bind_info28;
	struct drsuapi_DsBindInfo28 peer_bind_info28;
	struct policy_handle bind_handle;
};

struct DsaBindInfo {
	struct dcerpc_binding 		*server_binding;

	struct dcerpc_pipe 		*drs_pipe;
	struct dcerpc_binding_handle 	*drs_handle;

	DATA_BLOB 			gensec_skey;
	struct drsuapi_DsBindInfo48	srv_info48;
	struct policy_handle		rpc_handle;
};

struct DsIntIdTestCtx {
	const char *ldap_url;
	const char *domain_dn;
	const char *config_dn;
	const char *schema_dn;

	/* what we need to do as 'Administrator' */
	struct cli_credentials 	*creds;
	struct DsaBindInfo  	dsa_bind;
	struct ldb_context 	*ldb;

};

/* Format string to create provision LDIF with */
#define PROVISION_LDIF_FMT \
		"###########################################################\n" \
		"# Format string with positional params:\n" \
		"#  1 - (int) Unique ID between 1 and 2^16\n" \
		"#  2 - (string) Domain DN\n" \
		"###########################################################\n" \
		"\n" \
		"###########################################################\n" \
		"# Update schema\n" \
		"###########################################################\n" \
		"dn: CN=msds-intid-%1$d,CN=Schema,CN=Configuration,%2$s\n" \
		"changetype: add\n" \
		"objectClass: top\n" \
		"objectClass: attributeSchema\n" \
		"cn: msds-intid-%1$d\n" \
		"attributeID: 1.2.840.%1$d.1.5.9940\n" \
		"attributeSyntax: 2.5.5.10\n" \
		"omSyntax: 4\n" \
		"instanceType: 4\n" \
		"isSingleValued: TRUE\n" \
		"systemOnly: FALSE\n" \
		"\n" \
		"# schemaUpdateNow\n" \
		"DN:\n" \
		"changeType: modify\n" \
		"add: schemaUpdateNow\n" \
		"schemaUpdateNow: 1\n" \
		"-\n" \
		"\n" \
		"###########################################################\n" \
		"# Update User class\n" \
		"###########################################################\n" \
		"dn: CN=User,CN=Schema,CN=Configuration,%2$s\n" \
		"changetype: modify\n" \
		"add: mayContain\n" \
		"mayContain: msdsIntid%1$d\n" \
		"-\n" \
		"\n" \
		"# schemaUpdateNow\n" \
		"DN:\n" \
		"changeType: modify\n" \
		"add: schemaUpdateNow\n" \
		"schemaUpdateNow: 1\n" \
		"-\n" \
		"\n" \
		"###########################################################\n" \
		"# create user to test with\n" \
		"###########################################################\n" \
		"dn: CN=dsIntId_usr_%1$d,CN=Users,%2$s\n" \
		"changetype: add\n" \
		"objectClass: user\n" \
		"cn: dsIntId_usr_%1$d\n" \
		"name: dsIntId_usr_%1$d\n" \
		"displayName: dsIntId_usr_%1$d\n" \
		"sAMAccountName: dsIntId_usr_%1$d\n" \
		"msdsIntid%1$d: msDS-IntId-%1$d attribute value\n" \
		"\n"


static struct DsIntIdTestCtx *_dsintid_create_context(struct torture_context *tctx)
{
	NTSTATUS status;
	struct DsIntIdTestCtx *ctx;
	struct dcerpc_binding *server_binding;
	const char *binding = torture_setting_string(tctx, "binding", NULL);

	/* Create test suite context */
	ctx = talloc_zero(tctx, struct DsIntIdTestCtx);
	if (!ctx) {
		torture_result(tctx, TORTURE_FAIL, "Not enough memory!");
		return NULL;
	}

	/* parse binding object */
	status = dcerpc_parse_binding(ctx, binding, &server_binding);
	if (!NT_STATUS_IS_OK(status)) {
		torture_result(tctx, TORTURE_FAIL,
		               "Bad binding string '%s': %s", binding, nt_errstr(status));
		return NULL;
	}

	server_binding->flags |= DCERPC_SIGN | DCERPC_SEAL;

	/* populate test suite context */
	ctx->creds = cmdline_credentials;
	ctx->dsa_bind.server_binding = server_binding;

	ctx->ldap_url = talloc_asprintf(ctx, "ldap://%s", server_binding->host);

	return ctx;
}

static bool _test_DsaBind(struct torture_context *tctx,
			 TALLOC_CTX *mem_ctx,
			 struct cli_credentials *credentials,
			 uint32_t req_extensions,
			 struct DsaBindInfo *bi)
{
	NTSTATUS status;
	struct GUID bind_guid;
	struct drsuapi_DsBind r;
	struct drsuapi_DsBindInfoCtr bind_info_ctr;
	uint32_t supported_extensions;

	/* make DCE RPC connection */
	status = dcerpc_pipe_connect_b(mem_ctx,
				       &bi->drs_pipe,
				       bi->server_binding,
				       &ndr_table_drsuapi,
				       credentials, tctx->ev, tctx->lp_ctx);
	torture_assert_ntstatus_ok(tctx, status, "Failed to connect to server");

	bi->drs_handle = bi->drs_pipe->binding_handle;

	status = gensec_session_key(bi->drs_pipe->conn->security_state.generic_state,
	                            &bi->gensec_skey);
	torture_assert_ntstatus_ok(tctx, status, "failed to get gensec session key");

	/* Bind to DRSUAPI interface */
	GUID_from_string(DRSUAPI_DS_BIND_GUID_W2K3, &bind_guid);

	/*
	 * Add flags that should be 1, according to MS docs.
	 * It turns out DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3
	 * is actually required in order for GetNCChanges() to
	 * return schemaInfo entry in the prefixMap returned.
	 * Use DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION so
	 * we are able to fetch sensitive data.
	 */
	supported_extensions = req_extensions
			     | DRSUAPI_SUPPORTED_EXTENSION_BASE
			     | DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION
			     | DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD
			     | DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3
			     | DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;

	ZERO_STRUCT(bind_info_ctr);
	bind_info_ctr.length = 28;
	bind_info_ctr.info.info28.supported_extensions = supported_extensions;

	r.in.bind_guid = &bind_guid;
	r.in.bind_info = &bind_info_ctr;
	r.out.bind_handle = &bi->rpc_handle;

	status = dcerpc_drsuapi_DsBind_r(bi->drs_handle, mem_ctx, &r);
	torture_drsuapi_assert_call(tctx, bi->drs_pipe, status,
				    &r, "dcerpc_drsuapi_DsBind_r");


	switch (r.out.bind_info->length) {
	case 24: {
		struct drsuapi_DsBindInfo24 *info24;
		info24 = &r.out.bind_info->info.info24;
		bi->srv_info48.supported_extensions	= info24->supported_extensions;
		bi->srv_info48.site_guid		= info24->site_guid;
		bi->srv_info48.pid			= info24->pid;
		break;
	}
	case 28: {
		struct drsuapi_DsBindInfo28 *info28;
		info28 = &r.out.bind_info->info.info28;
		bi->srv_info48.supported_extensions	= info28->supported_extensions;
		bi->srv_info48.site_guid		= info28->site_guid;
		bi->srv_info48.pid			= info28->pid;
		bi->srv_info48.repl_epoch		= info28->repl_epoch;
		break;
	}
	case 48:
		bi->srv_info48 = r.out.bind_info->info.info48;
		break;
	default:
		torture_result(tctx, TORTURE_FAIL,
		               "DsBind: unknown BindInfo length: %u",
		               r.out.bind_info->length);
		return false;
	}

	/* check if server supports extensions we've requested */
	if ((bi->srv_info48.supported_extensions & req_extensions) != req_extensions) {
		torture_result(tctx, TORTURE_FAIL,
		               "Server does not support requested extensions. "
		               "Requested: 0x%08X, Supported: 0x%08X",
		               req_extensions, bi->srv_info48.supported_extensions);
		return false;
	}

	return true;
}

static bool _test_LDAPBind(struct torture_context *tctx,
			   TALLOC_CTX *mem_ctx,
			   struct cli_credentials *credentials,
			   const char *ldap_url,
			   struct ldb_context **_ldb)
{
	bool ret = true;

	struct ldb_context *ldb;

	const char *modules_option[] = { "modules:paged_searches", NULL };
	ldb = ldb_init(mem_ctx, tctx->ev);
	if (ldb == NULL) {
		return false;
	}

	/* Despite us loading the schema from the AD server, we need
	 * the samba handlers to get the extended DN syntax stuff */
	ret = ldb_register_samba_handlers(ldb);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	ldb_set_modules_dir(ldb,
			    talloc_asprintf(ldb,
					    "%s/ldb",
					    lpcfg_modulesdir(tctx->lp_ctx)));

	if (ldb_set_opaque(ldb, "credentials", credentials) != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	if (ldb_set_opaque(ldb, "loadparm", tctx->lp_ctx) != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	ret = ldb_connect(ldb, ldap_url, 0, modules_option);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		torture_assert_int_equal(tctx, ret, LDB_SUCCESS, "Failed to make LDB connection to target");
	}

	*_ldb = ldb;

	return true;
}

static bool _test_provision(struct torture_context *tctx, struct DsIntIdTestCtx *ctx)
{
	int ret;
	char *ldif_str;
	const char *pstr;
	struct ldb_ldif *ldif;
	uint32_t attr_id;
	struct ldb_context *ldb = ctx->ldb;

	/* We must have LDB connection ready by this time */
	SMB_ASSERT(ldb != NULL);

	ctx->domain_dn = ldb_dn_get_linearized(ldb_get_default_basedn(ldb));
	torture_assert(tctx, ctx->domain_dn != NULL, "Failed to get Domain DN");

	ctx->config_dn = ldb_dn_get_linearized(ldb_get_config_basedn(ldb));
	torture_assert(tctx, ctx->config_dn != NULL, "Failed to get Domain DN");

	ctx->schema_dn = ldb_dn_get_linearized(ldb_get_schema_basedn(ldb));
	torture_assert(tctx, ctx->schema_dn != NULL, "Failed to get Domain DN");

	/* prepare LDIF to provision with */
	attr_id = generate_random() % 0xFFFF;
	pstr = ldif_str = talloc_asprintf(ctx, PROVISION_LDIF_FMT,
	                                  attr_id, ctx->domain_dn);

	/* Provision test data */
	while ((ldif = ldb_ldif_read_string(ldb, &pstr)) != NULL) {
		switch (ldif->changetype) {
		case LDB_CHANGETYPE_DELETE:
			ret = ldb_delete(ldb, ldif->msg->dn);
			break;
		case LDB_CHANGETYPE_MODIFY:
			ret = ldb_modify(ldb, ldif->msg);
			break;
		case LDB_CHANGETYPE_ADD:
		default:
			ret = ldb_add(ldb, ldif->msg);
			break;
		}
		if (ret != LDB_SUCCESS) {
			char *msg = talloc_asprintf(ctx,
			                            "Failed to apply ldif - %s (%s): \n%s",
			                            ldb_errstring(ldb),
			                            ldb_strerror(ret),
			                            ldb_ldif_write_string(ldb, ctx, ldif));
			torture_fail(tctx, msg);

		}
		ldb_ldif_read_free(ldb, ldif);
	}

	return true;
}


static bool _test_GetNCChanges(struct torture_context *tctx,
			       struct DsaBindInfo *bi,
			       const char *nc_dn_str,
			       TALLOC_CTX *mem_ctx,
			       struct drsuapi_DsGetNCChangesCtr6 **_ctr6)
{
	NTSTATUS status;
	struct drsuapi_DsGetNCChanges r;
	union drsuapi_DsGetNCChangesRequest req;
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6_chunk = NULL;
	struct drsuapi_DsGetNCChangesCtr6 ctr6;
	uint32_t _level = 0;
	union drsuapi_DsGetNCChangesCtr ctr;

	struct dom_sid null_sid;

	ZERO_STRUCT(null_sid);

	/* fill-in Naming Context */
	nc.guid	= GUID_zero();
	nc.sid	= null_sid;
	nc.dn	= nc_dn_str;

	/* fill-in request fields */
	req.req8.destination_dsa_guid		= GUID_random();
	req.req8.source_dsa_invocation_id	= GUID_zero();
	req.req8.naming_context			= &nc;
	req.req8.highwatermark.tmp_highest_usn	= 0;
	req.req8.highwatermark.reserved_usn	= 0;
	req.req8.highwatermark.highest_usn	= 0;
	req.req8.uptodateness_vector		= NULL;
	req.req8.replica_flags			= DRSUAPI_DRS_WRIT_REP
						| DRSUAPI_DRS_INIT_SYNC
						| DRSUAPI_DRS_PER_SYNC
						| DRSUAPI_DRS_GET_ANC
						| DRSUAPI_DRS_NEVER_SYNCED
						;
	req.req8.max_object_count		= 402;
	req.req8.max_ndr_size			= 402116;

	req.req8.extended_op			= DRSUAPI_EXOP_NONE;
	req.req8.fsmo_info			= 0;
	req.req8.partial_attribute_set		= NULL;
	req.req8.partial_attribute_set_ex	= NULL;
	req.req8.mapping_ctr.num_mappings	= 0;
	req.req8.mapping_ctr.mappings		= NULL;

	r.in.bind_handle	= &bi->rpc_handle;
	r.in.level		= 8;
	r.in.req		= &req;

	ZERO_STRUCT(r.out);
	r.out.level_out 	= &_level;
	r.out.ctr		= &ctr;

	ZERO_STRUCT(ctr6);
	do {
		ZERO_STRUCT(ctr);

		status = dcerpc_drsuapi_DsGetNCChanges_r(bi->drs_handle, mem_ctx, &r);
		torture_drsuapi_assert_call(tctx, bi->drs_pipe, status,
					    &r, "dcerpc_drsuapi_DsGetNCChanges_r");

		/* we expect to get level 6 reply */
		torture_assert_int_equal(tctx, _level, 6, "Expected level 6 reply");

		/* store this chunk for later use */
		ctr6_chunk = &r.out.ctr->ctr6;

		if (!ctr6.first_object) {
			ctr6 = *ctr6_chunk;
		} else {
			struct drsuapi_DsReplicaObjectListItemEx *cur;

			ctr6.object_count += ctr6_chunk->object_count;
			for (cur = ctr6.first_object; cur->next_object; cur = cur->next_object) {}
			cur->next_object = ctr6_chunk->first_object;

			/* TODO: store the chunk of linked_attributes if needed */
		}

		/* prepare for next request */
		r.in.req->req8.highwatermark = ctr6_chunk->new_highwatermark;

	} while (ctr6_chunk->more_data);

	*_ctr6 = talloc(mem_ctx, struct drsuapi_DsGetNCChangesCtr6);
	torture_assert(mem_ctx, *_ctr6, "Not enough memory");
	**_ctr6 = ctr6;

	return true;
}

static char * _make_error_message(TALLOC_CTX *mem_ctx,
				  enum drsuapi_DsAttributeId drs_attid,
				  const struct dsdb_attribute *dsdb_attr,
				  const struct drsuapi_DsReplicaObjectIdentifier *identifier)
{
	return talloc_asprintf(mem_ctx, "\nInvalid ATTID for %1$s (%2$s)\n"
			       " drs_attid:      %3$11d (0x%3$08X)\n"
			       " msDS_IntId:     %4$11d (0x%4$08X)\n"
			       " attributeId_id: %5$11d (0x%5$08X)",
			       dsdb_attr->lDAPDisplayName,
			       identifier->dn,
			       drs_attid,
			       dsdb_attr->msDS_IntId,
			       dsdb_attr->attributeID_id);
}

/**
 * Fetch Schema NC and check ATTID values returned.
 * When Schema partition is replicated, ATTID
 * should always be made using prefixMap
 */
static bool test_dsintid_schema(struct torture_context *tctx, struct DsIntIdTestCtx *ctx)
{
	uint32_t i;
	const struct dsdb_schema *ldap_schema;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;
	const struct dsdb_attribute *dsdb_attr;
	const struct drsuapi_DsReplicaAttribute *drs_attr;
	const struct drsuapi_DsReplicaAttributeCtr *attr_ctr;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	const struct drsuapi_DsReplicaLinkedAttribute *la;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(ctx);
	torture_assert(tctx, mem_ctx, "Not enough memory");

	/* fetch whole Schema partition */
	torture_comment(tctx, "Fetch partition: %s\n", ctx->schema_dn);
	if (!_test_GetNCChanges(tctx, &ctx->dsa_bind, ctx->schema_dn, mem_ctx, &ctr6)) {
		torture_fail(tctx, "_test_GetNCChanges() failed");
	}

	/* load schema if not loaded yet */
	torture_comment(tctx, "Loading schema...\n");
	if (!drs_util_dsdb_schema_load_ldb(tctx, ctx->ldb, &ctr6->mapping_ctr, false)) {
		torture_fail(tctx, "drs_util_dsdb_schema_load_ldb() failed");
	}
	ldap_schema = dsdb_get_schema(ctx->ldb, NULL);

	/* verify ATTIDs fetched */
	torture_comment(tctx, "Verify ATTIDs fetched\n");
	for (cur = ctr6->first_object; cur; cur = cur->next_object) {
		attr_ctr = &cur->object.attribute_ctr;
		for (i = 0; i < attr_ctr->num_attributes; i++) {
			drs_attr = &attr_ctr->attributes[i];
			dsdb_attr = dsdb_attribute_by_attributeID_id(ldap_schema,
			                                             drs_attr->attid);

			torture_assert(tctx,
				       drs_attr->attid == dsdb_attr->attributeID_id,
				       _make_error_message(ctx, drs_attr->attid,
							   dsdb_attr,
							   cur->object.identifier));
			if (dsdb_attr->msDS_IntId) {
				torture_assert(tctx,
					       drs_attr->attid != dsdb_attr->msDS_IntId,
					       _make_error_message(ctx, drs_attr->attid,
								   dsdb_attr,
								   cur->object.identifier));
			}
		}
	}

	/* verify ATTIDs for Linked Attributes */
	torture_comment(tctx, "Verify ATTIDs for Linked Attributes (%u)\n",
			ctr6->linked_attributes_count);
	for (i = 0; i < ctr6->linked_attributes_count; i++) {
		la = &ctr6->linked_attributes[i];
		dsdb_attr = dsdb_attribute_by_attributeID_id(ldap_schema, la->attid);

		torture_assert(tctx,
			       la->attid == dsdb_attr->attributeID_id,
			       _make_error_message(ctx, la->attid,
						   dsdb_attr,
						   la->identifier))
		if (dsdb_attr->msDS_IntId) {
			torture_assert(tctx,
				       la->attid != dsdb_attr->msDS_IntId,
				       _make_error_message(ctx, la->attid,
							   dsdb_attr,
							   la->identifier))
		}
	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Fetch non-Schema NC and check ATTID values returned.
 * When non-Schema partition is replicated, ATTID
 * should be msDS-IntId value for the attribute
 * if this value exists
 */
static bool _test_dsintid(struct torture_context *tctx,
			  struct DsIntIdTestCtx *ctx,
			  const char *nc_dn_str)
{
	uint32_t i;
	const struct dsdb_schema *ldap_schema;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;
	const struct dsdb_attribute *dsdb_attr;
	const struct drsuapi_DsReplicaAttribute *drs_attr;
	const struct drsuapi_DsReplicaAttributeCtr *attr_ctr;
	const struct drsuapi_DsReplicaObjectListItemEx *cur;
	const struct drsuapi_DsReplicaLinkedAttribute *la;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(ctx);
	torture_assert(tctx, mem_ctx, "Not enough memory");

	/* fetch whole Schema partition */
	torture_comment(tctx, "Fetch partition: %s\n", nc_dn_str);
	if (!_test_GetNCChanges(tctx, &ctx->dsa_bind, nc_dn_str, mem_ctx, &ctr6)) {
		torture_fail(tctx, "_test_GetNCChanges() failed");
	}

	/* load schema if not loaded yet */
	torture_comment(tctx, "Loading schema...\n");
	if (!drs_util_dsdb_schema_load_ldb(tctx, ctx->ldb, &ctr6->mapping_ctr, false)) {
		torture_fail(tctx, "drs_util_dsdb_schema_load_ldb() failed");
	}
	ldap_schema = dsdb_get_schema(ctx->ldb, NULL);

	/* verify ATTIDs fetched */
	torture_comment(tctx, "Verify ATTIDs fetched\n");
	for (cur = ctr6->first_object; cur; cur = cur->next_object) {
		attr_ctr = &cur->object.attribute_ctr;
		for (i = 0; i < attr_ctr->num_attributes; i++) {
			drs_attr = &attr_ctr->attributes[i];
			dsdb_attr = dsdb_attribute_by_attributeID_id(ldap_schema,
			                                             drs_attr->attid);
			if (dsdb_attr->msDS_IntId) {
				torture_assert(tctx,
				               drs_attr->attid == dsdb_attr->msDS_IntId,
					       _make_error_message(ctx, drs_attr->attid,
								   dsdb_attr,
								   cur->object.identifier));
			} else {
				torture_assert(tctx,
					       drs_attr->attid == dsdb_attr->attributeID_id,
					       _make_error_message(ctx, drs_attr->attid,
								   dsdb_attr,
								   cur->object.identifier));
			}
		}
	}

	/* verify ATTIDs for Linked Attributes */
	torture_comment(tctx, "Verify ATTIDs for Linked Attributes (%u)\n",
			ctr6->linked_attributes_count);
	for (i = 0; i < ctr6->linked_attributes_count; i++) {
		la = &ctr6->linked_attributes[i];
		dsdb_attr = dsdb_attribute_by_attributeID_id(ldap_schema, la->attid);

		if (dsdb_attr->msDS_IntId) {
			torture_assert(tctx,
				       la->attid == dsdb_attr->msDS_IntId,
				       _make_error_message(ctx, la->attid,
							   dsdb_attr,
							   la->identifier));
		} else {
			torture_assert(tctx,
				       la->attid == dsdb_attr->attributeID_id,
				       _make_error_message(ctx, la->attid,
							   dsdb_attr,
							   la->identifier));
		}
	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Fetch Domain NC and check ATTID values returned.
 * When Domain partition is replicated, ATTID
 * should be msDS-IntId value for the attribute
 * if this value exists
 */
static bool test_dsintid_configuration(struct torture_context *tctx, struct DsIntIdTestCtx *ctx)
{
	return _test_dsintid(tctx, ctx, ctx->config_dn);
}

/**
 * Fetch Configuration NC and check ATTID values returned.
 * When Configuration partition is replicated, ATTID
 * should be msDS-IntId value for the attribute
 * if this value exists
 */
static bool test_dsintid_domain(struct torture_context *tctx, struct DsIntIdTestCtx *ctx)
{
	return _test_dsintid(tctx, ctx, ctx->domain_dn);
}


/**
 * DSSYNC test case setup
 */
static bool torture_dsintid_tcase_setup(struct torture_context *tctx, void **data)
{
	bool bret;
	struct DsIntIdTestCtx *ctx;

	*data = ctx = _dsintid_create_context(tctx);
	torture_assert(tctx, ctx, "test_create_context() failed");

	bret = _test_DsaBind(tctx, ctx, ctx->creds,
	                     DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8 |
	                     DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6,
	                     &ctx->dsa_bind);
	torture_assert(tctx, bret, "_test_DsaBind() failed");

	bret = _test_LDAPBind(tctx, ctx, ctx->creds, ctx->ldap_url, &ctx->ldb);
	torture_assert(tctx, bret, "_test_LDAPBind() failed");

	bret = _test_provision(tctx, ctx);
	torture_assert(tctx, bret, "_test_provision() failed");

	return true;
}

/**
 * DSSYNC test case cleanup
 */
static bool torture_dsintid_tcase_teardown(struct torture_context *tctx, void *data)
{
	struct DsIntIdTestCtx *ctx;
	struct drsuapi_DsUnbind r;
	struct policy_handle bind_handle;

	ctx = talloc_get_type(data, struct DsIntIdTestCtx);

	ZERO_STRUCT(r);
	r.out.bind_handle = &bind_handle;

	/* Release DRSUAPI handle */
	r.in.bind_handle = &ctx->dsa_bind.rpc_handle;
	dcerpc_drsuapi_DsUnbind_r(ctx->dsa_bind.drs_handle, ctx, &r);

	talloc_free(ctx);

	return true;
}

/**
 * DSSYNC test case implementation
 */
void torture_drs_rpc_dsintid_tcase(struct torture_suite *suite)
{
	typedef bool (*run_func) (struct torture_context *test, void *tcase_data);

	struct torture_test *test;
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "msDSIntId");

	torture_tcase_set_fixture(tcase,
				  torture_dsintid_tcase_setup,
				  torture_dsintid_tcase_teardown);

	test = torture_tcase_add_simple_test(tcase, "Schema", (run_func)test_dsintid_schema);
	test = torture_tcase_add_simple_test(tcase, "Configuration", (run_func)test_dsintid_configuration);
	test = torture_tcase_add_simple_test(tcase, "Domain", (run_func)test_dsintid_domain);
}
