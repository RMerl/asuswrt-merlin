/*
   ldb database library

   Copyright (C) Simo Sorce 2006-2008
   Copyright (C) Nadezhda Ivanova 2009
   Copyright (C) Anatoliy Atanasov  2009

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

/*
 *  Name: ldb
 *
 *  Component: ldb ACL module
 *
 *  Description: Module that performs authorisation access checks based on the
 *               account's security context and the DACL of the object being polled.
 *               Only DACL checks implemented at this point
 *
 *  Authors: Nadezhda Ivanova, Anatoliy Atanasov
 */

#include "includes.h"
#include "ldb_module.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "param/param.h"

/* acl_search helper */
struct acl_context {

	struct ldb_module *module;
	struct ldb_request *req;
	struct ldb_request *down_req;

	/*needed if we have to identify if this is SYSTEM_USER*/
	enum security_user_level user_type;

	uint32_t access_needed;
	struct ldb_dn * dn_to_check;

	/* set to true when we need to process the request as a SYSTEM_USER, regardless
	 * of the user's actual rights - for example when we need to retrieve the
	 * ntSecurityDescriptor */
	bool ignore_security;
	struct security_token *token;
	/*needed to identify if we have requested these attributes*/
	bool nTSecurityDescriptor;
	bool objectClass;
	int sec_result;
};

struct extended_access_check_attribute {
	const char *oa_name;
	const uint32_t requires_rights;
};

struct acl_private{
	bool perform_check;
};

static int acl_search_callback(struct ldb_request *req, struct ldb_reply *ares);

/*FIXME: Perhaps this should go in the .idl file*/
#define SEC_GENERIC_ACCESS_NEVER_GRANTED ( 0xFFFFFFFF )

/*Contains a part of the attributes - the ones that have predefined required rights*/
static const struct extended_access_check_attribute extended_access_checks_table[] =
{
	{
		.oa_name = "nTSecurityDescriptor",
		.requires_rights = SEC_FLAG_SYSTEM_SECURITY & SEC_STD_READ_CONTROL,
	},
	{
                .oa_name = "pekList",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
        },
	{
                .oa_name = "currentValue",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "dBCSPwd",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "unicodePwd",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "ntPwdHistory",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "priorValue",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "supplementalCredentials",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "trustAuthIncoming",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "trustAuthOutgoing",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "ImPwdHistory",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "initialAuthIncoming",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "initialAuthOutgoing",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
	{
                .oa_name = "msDS-ExecuteScriptPassword",
                .requires_rights = SEC_GENERIC_ACCESS_NEVER_GRANTED,
	},
};

static NTSTATUS extended_access_check(const char *attribute_name, const int access_rights, uint32_t searchFlags)
{
	int i = 0;
	if (access_rights == SEC_GENERIC_ACCESS_NEVER_GRANTED) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/*Check if the attribute is in the table first*/
	for ( i = 0; extended_access_checks_table[i].oa_name; i++ ) {
		if (ldb_attr_cmp(extended_access_checks_table[i].oa_name, attribute_name) == 0) {
			if ((access_rights & extended_access_checks_table[i].requires_rights) == access_rights) {
				return NT_STATUS_OK;
			} else {
				return NT_STATUS_ACCESS_DENIED;
			}
		}
	}

	/*Check for attribute whose attributeSchema has 0x80 set in searchFlags*/
	if ((searchFlags & SEARCH_FLAG_CONFIDENTIAL) == SEARCH_FLAG_CONFIDENTIAL) {
		if (((SEC_ADS_READ_PROP & SEC_ADS_CONTROL_ACCESS) & access_rights) == access_rights) {
			return NT_STATUS_OK;
		} else {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/*Check attributes with *special* behaviour*/
	if (ldb_attr_cmp("msDS-QuotaEffective", attribute_name) == 0 || ldb_attr_cmp("msDS-QuotaUsed", attribute_name) == 0){
		/*Rights required:
		 *
		 *(RIGHT_DS_READ_PROPERTY on the Quotas container or
		 *((the client is querying the quota for the security principal it is authenticated as) and
		 *(DS-Query-Self-Quota control access right on the Quotas container))
		 */
	}

        if (ldb_attr_cmp("userPassword", attribute_name) == 0) {
		/*When the dSHeuristics.fUserPwdSupport flag is false, the requester must be granted RIGHT_DS_READ_PROPERTY.
		 *When the dSHeuristics.fUserPwdSupport flag is true, access is never granted.
		 */
	}

	if (ldb_attr_cmp("sDRightsEffective", attribute_name) == 0) {
		/*FIXME:3.1.1.4.5.4 in MS-ADTS*/
	}

	if (ldb_attr_cmp("allowedChildClassesEffective", attribute_name) == 0) {
		/*FIXME:3.1.1.4.5.5 in MS-ADTS*/
        }

	if (ldb_attr_cmp("allowedAttributesEffective", attribute_name) == 0) {
		/*FIXME:3.1.1.4.5.7 in MS-ADTS*/
        }

	if (ldb_attr_cmp("msDS-Approx-Immed-Subordinates", attribute_name) == 0) {
		/*FIXME:3.1.1.4.5.15 in MS-ADTS*/
        }

	if (ldb_attr_cmp("msDS-QuotaEffective", attribute_name) == 0) {
		/*FIXME:3.1.1.4.5.22 in MS-ADTS*/
        }

	if (ldb_attr_cmp("msDS-ReplAttributeMetaData", attribute_name) == 0 || ldb_attr_cmp("msDS-ReplAttributeMetaData", attribute_name) == 0) {
		/*The security context of the requester must be granted the following rights on the replPropertyMetaData attribute:
		 *(RIGHT_DS_READ_PROPERTY)or (DS-Replication-Manage-Topology by ON!nTSecurityDescriptor)
		 */
        }

	if (ldb_attr_cmp("msDS-NCReplInboundNeighbors", attribute_name) == 0) {
		/*The security context of the requester must be granted the following rights on repsFrom:
		 *(RIGHT_DS_READ_PROPERTY) or (DS-Replication-Manage-Topology) or (DS-Replication-Monitor-Topology)
		 */
        }

	if (ldb_attr_cmp("msDS-NCReplOutboundNeighbors", attribute_name) == 0) {
		/*The security context of the requester must be granted the following rights on repsTo:
		 *(RIGHT_DS_READ_PROPERTY) or (DS-Replication-Manage-Topology) or (DS-Replication-Monitor-Topology)
		 */
        }

	if (ldb_attr_cmp("msDS-NCReplCursors", attribute_name) == 0) {
		/*The security context of the requester must be granted the following rights on replUpToDateVector: (RIGHT_DS_READ_PROPERTY)
		 *or (DS-Replication-Manage-Topology) or (DS-Replication-Monitor-Topology)
		 */
        }

	if (ldb_attr_cmp("msDS-IsUserCachableAtRodc", attribute_name) == 0) {
		/*The security context of the requester must be granted
		 *the DS-Replication-Secrets-Synchronize control access right on the root of the default NC.
		 */
        }

	return NT_STATUS_OK;
}

/* Builds an object tree for object specific access checks */
static struct object_tree * build_object_tree_form_attr_list(TALLOC_CTX *mem_ctx,   /* Todo this context or separate? */
							     struct ldb_context *ldb,
							     const char ** attr_names,
							     int num_attrs,
							     const char * object_class,
							     uint32_t init_access)
{
	const struct dsdb_schema *schema = dsdb_get_schema(ldb);
	const struct GUID *oc_guid = class_schemaid_guid_by_lDAPDisplayName(schema, object_class);
	struct object_tree *tree;
	int i;

	if (!oc_guid)
		return NULL;

	tree = insert_in_object_tree(mem_ctx, oc_guid, NULL, init_access, NULL);
	if (attr_names){
		for (i=0; i < num_attrs; i++){
			const struct dsdb_attribute *attribute = dsdb_attribute_by_lDAPDisplayName(schema,attr_names[i]);
			if (attribute)
				insert_in_object_tree(mem_ctx,
						      &attribute->schemaIDGUID,
						      &attribute->attributeSecurityGUID,
						      init_access,
						      tree);
		}
	}
	return tree;
}

bool is_root_base_dn(struct ldb_context *ldb, struct ldb_dn *dn_to_check)
{
	int result;
	struct ldb_dn *root_base_dn = ldb_get_root_basedn(ldb);
	result = ldb_dn_compare(root_base_dn,dn_to_check);
	return (result==0);
}

static int acl_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct acl_context *ac;

	ac = talloc_get_type(req->context, struct acl_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return ldb_module_done(ac->req, ares->controls,
				ares->response, ares->error);
}


static int acl_access_check_add(struct ldb_reply *ares,
				struct acl_context *ac,
				struct security_descriptor *sd)
{
	uint32_t access_granted = 0;
	NTSTATUS status;
	struct ldb_dn *parent;
	struct ldb_dn *grandparent;
	struct object_tree *tree = NULL;

	parent = ldb_dn_get_parent(ac->req, ac->req->op.add.message->dn);
	grandparent = ldb_dn_get_parent(ac->req, parent);
	if (ldb_dn_compare(ares->message->dn, grandparent) == 0)
		status = sec_access_check_ds(sd, ac->token,
					     SEC_ADS_LIST,
					     &access_granted,
					     NULL);
	else if (ldb_dn_compare(ares->message->dn, parent) == 0){
		struct ldb_message_element *oc_el;
		struct ldb_context *ldb = ldb_module_get_ctx(ac->module);
		const struct dsdb_schema *schema = dsdb_get_schema(ldb);
		int i;

		oc_el = ldb_msg_find_element(ares->message, "objectClass");
		if (!oc_el || oc_el->num_values == 0)
			return LDB_SUCCESS;
		for (i = 0; i < oc_el->num_values; i++){
			const struct GUID *guid = class_schemaid_guid_by_lDAPDisplayName(schema,
											  oc_el->values[i].data);
			ac->sec_result = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
			tree = insert_in_object_tree(ac->req, guid, NULL, SEC_ADS_CREATE_CHILD,
						     tree);
			status = sec_access_check_ds(sd, ac->token, SEC_ADS_CREATE_CHILD,&access_granted, tree);
			if (NT_STATUS_IS_OK(status))
				ac->sec_result = LDB_SUCCESS;
		}
	}
	else
		return LDB_SUCCESS;

	return ac->sec_result;
}

static int acl_access_check_modify(struct ldb_reply *ares, struct acl_context *ac,
				   struct security_descriptor *sd)
{
	uint32_t access_granted = 0;
	NTSTATUS status;
	struct ldb_dn *parent;
	struct object_tree *tree = NULL;

	parent = ldb_dn_get_parent(ac->req, ac->req->op.add.message->dn);
	if (ldb_dn_compare(ares->message->dn, parent) == 0)
		status = sec_access_check_ds(sd, ac->token, SEC_ADS_LIST,&access_granted, NULL);
	else if (ldb_dn_compare(ares->message->dn, ac->req->op.add.message->dn) == 0){
		struct ldb_message_element *oc_el;
		struct ldb_context *ldb = ldb_module_get_ctx(ac->module);
		const struct dsdb_schema *schema = dsdb_get_schema(ldb);
		int i;
		struct GUID *guid;
		oc_el = ldb_msg_find_element(ares->message, "objectClass");
		if (!oc_el || oc_el->num_values == 0)
			return LDB_SUCCESS;

		guid = class_schemaid_guid_by_lDAPDisplayName(schema,
							      oc_el->values[oc_el->num_values-1].data);
		tree = insert_in_object_tree(ac->req, guid, NULL, SEC_ADS_WRITE_PROP,
						     tree);
		for (i=0; i < ac->req->op.mod.message->num_elements; i++){
			const struct dsdb_attribute *attr = dsdb_attribute_by_lDAPDisplayName(schema,
											      ac->req->op.mod.message->elements[i].name);
			if (!attr)
				return LDB_ERR_OPERATIONS_ERROR; /* What should we actually return here? */
			insert_in_object_tree(ac, &attr->schemaIDGUID,
					      &attr->attributeSecurityGUID, ac->access_needed, tree);
		}
		status = sec_access_check_ds(sd, ac->token, SEC_ADS_WRITE_PROP ,&access_granted, tree);
		if (!NT_STATUS_IS_OK(status))
			ac->sec_result = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	else
		return LDB_SUCCESS;
	return ac->sec_result;
}
/*TODO*/
static int acl_access_check_rename(struct ldb_reply *ares, struct acl_context *ac,
				   struct security_descriptor *sd)
{
	return ac->sec_result;
}

static int acl_access_check_delete(struct ldb_reply *ares, struct acl_context *ac,
				   struct security_descriptor *sd)
{
	uint32_t access_granted = 0;
	NTSTATUS status;
	struct ldb_dn *parent;
	struct object_tree *tree = NULL;

	parent = ldb_dn_get_parent(ac->req, ac->req->op.del.dn);
	if (ldb_dn_compare(ares->message->dn, parent) == 0){
		status = sec_access_check_ds(sd, ac->token, SEC_ADS_LIST,&access_granted, NULL);
		if (!NT_STATUS_IS_OK(status)){
			ac->sec_result = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
			return ac->sec_result;
		}
		status = sec_access_check_ds(sd, ac->token, SEC_ADS_DELETE_CHILD,&access_granted, NULL);
		if (NT_STATUS_IS_OK(status)){
			ac->sec_result = LDB_SUCCESS;
			return ac->sec_result;
		}
	}
	else if (ldb_dn_compare(ares->message->dn, ac->req->op.del.dn) == 0){
		status = sec_access_check_ds(sd, ac->token, SEC_STD_DELETE, &access_granted, NULL);
		if (!NT_STATUS_IS_OK(status))
			ac->sec_result = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	return ac->sec_result;
}

static int acl_access_check_search(struct ldb_reply *ares, struct acl_context *ac,
				   struct security_descriptor *sd)
{
	uint32_t access_granted;
	NTSTATUS status;
	struct ldb_dn *parent;

	if (ac->user_type == SECURITY_SYSTEM || ac->user_type == SECURITY_ANONYMOUS) {
		return LDB_SUCCESS;/*FIXME: we have anonymous access*/
	}

	parent = ldb_dn_get_parent(ac->req, ac->dn_to_check);
	ac->sec_result = LDB_SUCCESS;
	if (ldb_dn_compare(ares->message->dn, parent) == 0) {
		status = sec_access_check_ds(sd, ac->token, SEC_ADS_LIST,&access_granted, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			ac->sec_result = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
		}
	}

	return ac->sec_result;
}

static int acl_perform_access_check(struct ldb_request *req, struct ldb_reply *ares,
				    struct acl_context *ac)
{
	struct ldb_message_element *oc_el;
	struct security_descriptor *sd;
	enum ndr_err_code ndr_err;

	oc_el = ldb_msg_find_element(ares->message, "ntSecurityDescriptor");
	if (!oc_el || oc_el->num_values == 0)
		return LDB_SUCCESS;

	sd = talloc(ac, struct security_descriptor);
	if(!sd) {
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	}
	ndr_err = ndr_pull_struct_blob(&oc_el->values[0], sd, NULL, sd,
				       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err))
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	switch (ac->req->operation) {
	case LDB_SEARCH:
		return acl_access_check_search(ares, ac, sd);
	case LDB_ADD:
		return acl_access_check_add(ares, ac, sd);
	case LDB_MODIFY:
		return acl_access_check_modify(ares, ac, sd);
	case LDB_DELETE:
		return acl_access_check_delete(ares, ac, sd);
	case LDB_RENAME:
		return acl_access_check_rename(ares, ac, sd);
	default:
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	}
	return LDB_SUCCESS;
}

static int acl_forward_add(struct ldb_reply *ares,
			   struct acl_context *ac)
{
  struct ldb_request *newreq;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);
	ret = ldb_build_add_req(&newreq,ldb,
				ac,
				ac->req->op.add.message,
				ac->req->controls,
				ac,
				acl_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS)
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	return ldb_next_request(ac->module, newreq);
}

static int acl_forward_modify(struct ldb_reply *ares,
			      struct acl_context *ac)
{
  struct ldb_request *newreq;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);
	ret = ldb_build_mod_req(&newreq,ldb,
				    ac,
				    ac->req->op.mod.message,
				    ac->req->controls,
				    ac,
				    acl_op_callback,
				    ac->req);
	if (ret != LDB_SUCCESS)
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	return ldb_next_request(ac->module, newreq);
}

static int acl_forward_delete(struct ldb_reply *ares,
			      struct acl_context *ac)
{
	struct ldb_request *newreq;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);
	ret = ldb_build_del_req(&newreq, ldb,
				    ac,
				    ac->req->op.del.dn,
				    ac->req->controls,
				    ac,
				    acl_op_callback,
				    ac->req);
	if (ret != LDB_SUCCESS)
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	return ldb_next_request(ac->module, newreq);
}

static int acl_forward_rename(struct ldb_reply *ares,
			      struct acl_context *ac)
{
	return LDB_SUCCESS;
}

static int acl_forward_search(struct acl_context *ac)
{
	int ret;
	const char * const *attrs;
	struct ldb_control *sd_control;
	struct ldb_control **sd_saved_controls;
	struct ldb_context *ldb;
	struct ldb_request *newreq;

	ldb = ldb_module_get_ctx(ac->module);
	attrs = ac->req->op.search.attrs;
	if (attrs) {
		ac->nTSecurityDescriptor = false;
		ac->objectClass = false;
		if (!ldb_attr_in_list(ac->req->op.search.attrs, "nTSecurityDescriptor")) {
			attrs = ldb_attr_list_copy_add(ac, attrs, "nTSecurityDescriptor");
			ac->nTSecurityDescriptor = true;
		}
		if (!ldb_attr_in_list(ac->req->op.search.attrs, "objectClass")) {
			attrs = ldb_attr_list_copy_add(ac, attrs, "objectClass");
			ac->objectClass = true;
		}
	}
	ret = ldb_build_search_req_ex(&newreq,ldb,
				      ac,
				      ac->req->op.search.base,
				      ac->req->op.search.scope,
				      ac->req->op.search.tree,
				      attrs,
				      ac->req->controls,
				      ac, acl_search_callback,
				      ac->req);
	if (ret != LDB_SUCCESS) {
                return LDB_ERR_OPERATIONS_ERROR;
	}
	/* check if there's an SD_FLAGS control */
	sd_control = ldb_request_get_control(newreq, LDB_CONTROL_SD_FLAGS_OID);
	if (sd_control) {
		/* save it locally and remove it from the list */
		/* we do not need to replace them later as we
		 * are keeping the original req intact */
		if (!save_controls(sd_control, newreq, &sd_saved_controls)) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}
	return ldb_next_request(ac->module, newreq);
}

static int acl_forward_request(struct ldb_reply *ares,
			       struct acl_context *ac)
{
	switch (ac->req->operation) {
	case LDB_SEARCH:
		return acl_forward_search(ac);
	case LDB_ADD:
		return acl_forward_add(ares,ac);
	case LDB_MODIFY:
		return acl_forward_modify(ares,ac);
	case LDB_DELETE:
		return acl_forward_delete(ares,ac);
	case LDB_RENAME:
		return acl_forward_rename(ares,ac);
	default:
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_ERR_OPERATIONS_ERROR);
	}
	return LDB_SUCCESS;
}

static int acl_visible_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct acl_context *ac;

	ac = talloc_get_type(req->context, struct acl_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		return acl_perform_access_check(req, ares, ac);
	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral); /* what to do here actually? */
	case LDB_REPLY_DONE:
		if (ac->sec_result != LDB_SUCCESS) {
			return ldb_module_done(ac->req, ares->controls,
                                               ares->response, ac->sec_result);
		}
		return acl_forward_request(ares,ac);
	default:
		break;
	}
	return LDB_SUCCESS;
}

static enum security_user_level what_is_user(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	return security_session_user_level(session_info);
}

static struct security_token * user_token(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if(!session_info) {
		return NULL;
	}
	return session_info->security_token;
}


static int make_req_access_check(struct ldb_module *module, struct ldb_request *req,
				 struct acl_context *ac, const char *filter)
{
	struct ldb_context *ldb;
	int ret;
	const char **attrs = talloc_array(ac, const char *, 3);
	struct ldb_parse_tree *tree = ldb_parse_tree(req, filter);

	attrs[0] = talloc_strdup(attrs, "ntSecurityDescriptor");
	attrs[1] = talloc_strdup(attrs, "objectClass");
	attrs[2] = NULL;

	ldb = ldb_module_get_ctx(module);
	ret = ldb_build_search_req_ex(&ac->down_req,
				      ldb, ac,
				      ac->dn_to_check,
				      LDB_SCOPE_SUBTREE,
				      tree,
				      attrs,
				      NULL,
				      ac, acl_visible_callback,
				      req);
	return ret;
}

static const char *user_name(TALLOC_CTX *mem_ctx, struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if (!session_info) {
		return "UNKNOWN (NULL)";
	}

	return talloc_asprintf(mem_ctx, "%s\\%s",
			       session_info->server_info->domain_name,
			       session_info->server_info->account_name);
}

static int acl_module_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct acl_private *data;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module, LDB_CONTROL_SD_FLAGS_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "acl_module_init: Unable to register control with rootdse!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data = talloc(module, struct acl_private);
	data->perform_check = lp_parm_bool(ldb_get_opaque(ldb, "loadparm"),
				  NULL, "acl", "perform", false);
	ldb_module_set_private(module, data);

	return ldb_next_init(module);
}

static int acl_add(struct ldb_module *module, struct ldb_request *req)
{
	int ret;
	struct acl_context *ac;
	struct ldb_dn * parent = ldb_dn_get_parent(req, req->op.add.message->dn);
	char * filter;
	struct ldb_context *ldb;
	struct acl_private *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data->perform_check)
		return ldb_next_request(module, req);

	ac = talloc(req, struct acl_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (what_is_user(module) == SECURITY_SYSTEM)
		return ldb_next_request(module, req);

	ac->module = module;
	ac->req = req;
	ac->ignore_security = true;
	ac->dn_to_check = ldb_dn_get_parent(req, parent);
	ac->token = user_token(module);
	ac->user_type = what_is_user(module);
	ac->sec_result = LDB_SUCCESS;
	if (!is_root_base_dn(ldb, req->op.add.message->dn) && parent && !is_root_base_dn(ldb, parent)){
		filter = talloc_asprintf(req,"(&(objectClass=*)(|(%s=%s)(%s=%s))))",
					 ldb_dn_get_component_name(parent,0),
					 ldb_dn_get_component_val(parent,0)->data,
					 ldb_dn_get_component_name(ac->dn_to_check,0),
					 ldb_dn_get_component_val(ac->dn_to_check,0)->data);

		ret = make_req_access_check(module, req, ac, filter);
		if (ret != LDB_SUCCESS){
			return ret;
		}
		return ldb_next_request(module, ac->down_req);
	}
	return ldb_next_request(module, req);
}

static int acl_modify(struct ldb_module *module, struct ldb_request *req)
{
	int ret;
	struct acl_context *ac;
	struct ldb_dn * parent = ldb_dn_get_parent(req, req->op.mod.message->dn);
	char * filter;
	struct ldb_context *ldb;
	struct acl_private *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data->perform_check)
		return ldb_next_request(module, req);

	ac = talloc(req, struct acl_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

/*	if (what_is_user(module) == SECURITY_SYSTEM) */
		return ldb_next_request(module, req);

	ac->module = module;
	ac->req = req;
	ac->ignore_security = true;
	ac->dn_to_check = req->op.mod.message->dn;
	ac->token = user_token(module);
	ac->user_type = what_is_user(module);
	ac->sec_result = LDB_SUCCESS;
	if (!is_root_base_dn(ldb, req->op.mod.message->dn) && parent && !is_root_base_dn(ldb, parent)){
	  filter = talloc_asprintf(req,"(&(objectClass=*)(|(%s=%s)(%s=%s))))",
				   ldb_dn_get_component_name(parent,0),
				   ldb_dn_get_component_val(parent,0)->data,
				   ldb_dn_get_component_name(req->op.mod.message->dn,0),
				   ldb_dn_get_component_val(req->op.mod.message->dn,0)->data);

		ret = make_req_access_check(module, req, ac, filter);
		if (ret != LDB_SUCCESS){
			return ret;
		}
		return ldb_next_request(module, ac->down_req);
	}
	return ldb_next_request(module, req);
}

/* similar to the modify for the time being.
 * We need to concider the special delete tree case, though - TODO */
static int acl_delete(struct ldb_module *module, struct ldb_request *req)
{
	int ret;
	struct acl_context *ac;
	struct ldb_dn * parent = ldb_dn_get_parent(req, req->op.del.dn);
	char * filter;
	struct ldb_context *ldb;
	struct acl_private *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data->perform_check)
		return ldb_next_request(module, req);

	ac = talloc(req, struct acl_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ac->user_type == SECURITY_SYSTEM)
		return ldb_next_request(module, req);

	ac->module = module;
	ac->req = req;
	ac->ignore_security = true;
	ac->dn_to_check = req->op.del.dn;
	ac->token = user_token(module);
	ac->user_type = what_is_user(module);
	ac->sec_result = LDB_SUCCESS;
	if (parent) {
		filter = talloc_asprintf(req,"(&(objectClass=*)(|(%s=%s)(%s=%s))))",
					 ldb_dn_get_component_name(parent,0),
					 ldb_dn_get_component_val(parent,0)->data,
					 ldb_dn_get_component_name(req->op.del.dn,0),
					 ldb_dn_get_component_val(req->op.del.dn,0)->data);
		ret = make_req_access_check(module, req, ac, filter);

		if (ret != LDB_SUCCESS){
			return ret;
                }
		return ldb_next_request(module, ac->down_req);
	}

	return ldb_next_request(module, req);
}

static int acl_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_dn *source_parent;
	struct ldb_dn *dest_parent;
	int ret;
	struct acl_context *ac;
	char * filter;
	struct ldb_context *ldb;
	struct acl_private *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data->perform_check)
		return ldb_next_request(module, req);

	ac = talloc(req, struct acl_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ac->user_type == SECURITY_SYSTEM)
		return ldb_next_request(module, req);

	ac->module = module;
	ac->req = req;
	ac->ignore_security = true;
	ac->token = user_token(module);
	ac->user_type = what_is_user(module);
	ac->sec_result = LDB_SUCCESS;

	/* We need to know if it is a simple rename or a move operation */
	source_parent = ldb_dn_get_parent(req, req->op.rename.olddn);
	dest_parent = ldb_dn_get_parent(req, req->op.rename.newdn);

	if (ldb_dn_compare(source_parent, dest_parent) == 0){
		/*Not a move, just rename*/
		filter = talloc_asprintf(req,"(&(objectClass=*)(|(%s=%s)(%s=%s))))",
					 ldb_dn_get_component_name(dest_parent,0),
					 ldb_dn_get_component_val(dest_parent,0)->data,
					 ldb_dn_get_component_name(req->op.rename.olddn,0),
					 ldb_dn_get_component_val(req->op.rename.olddn,0)->data);
	}
	else{
		filter = talloc_asprintf(req,"(&(objectClass=*)(|(%s=%s)(%s=%s))))",
					 ldb_dn_get_component_name(dest_parent,0),
					 ldb_dn_get_component_val(dest_parent,0)->data,
					 ldb_dn_get_component_name(source_parent,0),
					 ldb_dn_get_component_val(source_parent,0)->data);
	}

	ret = make_req_access_check(module, req, ac, filter);

	if (ret != LDB_SUCCESS){
		return ret;
		}
	return ldb_next_request(module, ac->down_req);
}

static int acl_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct acl_context *ac;
	struct security_descriptor *sd;
	uint32_t searchFlags;
	uint32_t access_mask;
	struct object_tree *ot;
	int i, ret;
	NTSTATUS status;
	struct ldb_message_element *element_security_descriptor;
	struct ldb_message_element *element_object_class;
	const struct dsdb_attribute *attr;
	const struct dsdb_schema *schema;
	struct GUID *oc_guid;

	ac = talloc_get_type(req->context, struct acl_context);
	ldb = ldb_module_get_ctx(ac->module);
	schema = dsdb_get_schema(ldb);
	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls, ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		switch (ac->user_type) {
		case SECURITY_SYSTEM:
		case SECURITY_ANONYMOUS:/*FIXME: should we let anonymous have system access*/
			break;
		default:
			/* Access checks
			 *
			 * 0. If we do not have nTSecurityDescriptor, we do not have an object in the response,
			 *    so check the parent dn.
			 * 1. Call sec_access_check on empty tree
			 * 2. For each attribute call extended_access_check
			 * 3. For each attribute call build_object_tree_form_attr_list and then check with sec_access_check
			 *
			 */
			element_security_descriptor = ldb_msg_find_element(ares->message, "nTSecurityDescriptor");
			element_object_class = ldb_msg_find_element(ares->message, "objectClass");
			if (!element_security_descriptor || !element_object_class)
				break;

			sd = talloc(ldb, struct security_descriptor);
			if(!sd) {
			  return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
			}
			if(!NDR_ERR_CODE_IS_SUCCESS(ndr_pull_struct_blob(&element_security_descriptor->values[0],
									 ldb,
									 NULL,
									 sd,
									 (ndr_pull_flags_fn_t)ndr_pull_security_descriptor))) {
				DEBUG(0, ("acl_search_callback: Error parsing security descriptor\n"));
				return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
			}

			oc_guid = class_schemaid_guid_by_lDAPDisplayName(schema, element_object_class->values[0].data);
			for (i=0; i<ares->message->num_elements; i++) {
				attr = dsdb_attribute_by_lDAPDisplayName(schema, ares->message->elements[i].name);
				if (attr) {
					searchFlags = attr->searchFlags;
				} else {
					searchFlags = 0x0;
				}

				/*status = extended_access_check(ares->message->elements[i].name, access_mask, searchFlags); */ /* Todo FIXME */
				ac->access_needed = SEC_ADS_READ_PROP;
				if (NT_STATUS_IS_OK(status)) {
					ot = insert_in_object_tree(req, oc_guid, NULL, ac->access_needed, NULL);

					insert_in_object_tree(req,
							      &attr->schemaIDGUID,
							      &attr->attributeSecurityGUID,
							      ac->access_needed,
							      ot);

					status = sec_access_check_ds(sd,
								     ac->token,
								     ac->access_needed,
								     &access_mask,
								     ot);

					if (NT_STATUS_IS_OK(status)) {
						continue;
					}
				}
				ldb_msg_remove_attr(ares->message, ares->message->elements[i].name);
			}
			break;
		}
		if (ac->nTSecurityDescriptor) {
			ldb_msg_remove_attr(ares->message, "nTSecurityDescriptor");
		} else if (ac->objectClass) {
			ldb_msg_remove_attr(ares->message, "objectClass");
		}

		return ldb_module_send_entry(ac->req, ares->message, ares->controls);
	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,ares->response, LDB_SUCCESS);
	}

        return LDB_SUCCESS;
}

static int acl_search(struct ldb_module *module, struct ldb_request *req)
{
	int ret;
	struct ldb_context *ldb;
	struct acl_context *ac;
	const char **attrs;
	struct ldb_control *sd_control;
	struct ldb_control **sd_saved_controls;
	struct ldb_dn * parent;
	struct acl_private *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data || !data->perform_check)
		return ldb_next_request(module, req);

	if (what_is_user(module) == SECURITY_SYSTEM)
		return ldb_next_request(module, req);

	ac = talloc_get_type(req->context, struct acl_context);
	if ( ac == NULL ) {
		ac = talloc(req, struct acl_context);
		if (ac == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ac->module = module;
		ac->req = req;
		ac->ignore_security = false;
		ac->user_type = what_is_user(module);
		ac->token = user_token(module);
		ac->dn_to_check = req->op.search.base;
		ac->sec_result = LDB_SUCCESS;

		attrs = talloc_array(ac, const char*, 2);
		attrs[0] = talloc_strdup(attrs, "nTSecurityDescriptor");
		attrs[1] = NULL;
		parent = ldb_dn_get_parent(req, ac->dn_to_check);
		if (!is_root_base_dn(ldb, req->op.search.base) && parent && !is_root_base_dn(ldb, parent)) {
			/*we have parent so check for visibility*/
			ret = ldb_build_search_req(&ac->down_req,
						   ldb, ac,
						   parent,
						   LDB_SCOPE_BASE,
						   "(objectClass=*)",
						   attrs,
						   req->controls,
						   ac, acl_visible_callback,
						   req);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
			return ldb_next_request(module, ac->down_req);
		} else {
			return acl_forward_search(ac);
		}
	}

	return ldb_next_request(module, req);
}

static int acl_extended(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	enum security_user_level user_type;
	struct acl_private *data;

	data = talloc_get_type(ldb_module_get_private(module), struct acl_private);

	if (!data->perform_check)
		return ldb_next_request(module, req);

	/* allow everybody to read the sequence number */
	if (strcmp(req->op.extended.oid, LDB_EXTENDED_SEQUENCE_NUMBER) == 0) {
		return ldb_next_request(module, req);
	}

	user_type = what_is_user(module);
	switch (user_type) {
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		return ldb_next_request(module, req);
	default:
		ldb_asprintf_errstring(ldb,
				       "acl_extended: attempted database modify not permitted."
				       "User %s is not SYSTEM or an Administrator",
				       user_name(req, module));
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
}

_PUBLIC_ const struct ldb_module_ops ldb_acl_module_ops = {
	.name		   = "acl",
	.search            = acl_search,
	.add               = acl_add,
	.modify            = acl_modify,
	.del               = acl_delete,
	.rename            = acl_rename,
	.extended          = acl_extended,
	.init_context	   = acl_module_init
};
