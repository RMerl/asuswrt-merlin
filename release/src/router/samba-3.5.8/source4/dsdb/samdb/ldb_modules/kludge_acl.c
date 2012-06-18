/* 
   ldb database library

   Copyright (C) Andrew Bartlett 2005
   Copyright (C) Simo Sorce 2006-2008

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
 *  Component: ldb kludge ACL module
 *
 *  Description: Simple module to enforce a simple form of access
 *               control, sufficient for securing a default Samba4 
 *               installation.
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"

/* Kludge ACL rules:
 *
 * - System can read passwords
 * - Administrators can write anything
 * - Users can read anything that is not a password
 *
 */

struct kludge_private_data {
	const char **password_attrs;
	bool acl_perform;
};

static enum security_user_level what_is_user(struct ldb_module *module) 
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	return security_session_user_level(session_info);
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

/* search */
struct kludge_acl_context {

	struct ldb_module *module;
	struct ldb_request *req;

	enum security_user_level user_type;
	bool allowedAttributes;
	bool allowedAttributesEffective;
	bool allowedChildClasses;
	bool allowedChildClassesEffective;
	const char * const *attrs;
};

/* read all objectClasses */

static int kludge_acl_allowedAttributes(struct ldb_context *ldb, struct ldb_message *msg,
					const char *attrName) 
{
	struct ldb_message_element *oc_el;
	struct ldb_message_element *allowedAttributes;
	const struct dsdb_schema *schema = dsdb_get_schema(ldb);
	TALLOC_CTX *mem_ctx;
	const char **attr_list;
	int i, ret;

 	/* If we don't have a schema yet, we can't do anything... */
	if (schema == NULL) {
		return LDB_SUCCESS;
	}

	/* Must remove any existing attribute, or else confusion reins */
	ldb_msg_remove_attr(msg, attrName);
	ret = ldb_msg_add_empty(msg, attrName, 0, &allowedAttributes);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	mem_ctx = talloc_new(msg);
	if (!mem_ctx) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* To ensure that oc_el is valid, we must look for it after 
	   we alter the element array in ldb_msg_add_empty() */
	oc_el = ldb_msg_find_element(msg, "objectClass");
	
	attr_list = dsdb_full_attribute_list(mem_ctx, schema, oc_el, DSDB_SCHEMA_ALL);
	if (!attr_list) {
		ldb_asprintf_errstring(ldb, "kludge_acl: Failed to get list of attributes create %s attribute", attrName);
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; attr_list && attr_list[i]; i++) {
		ldb_msg_add_string(msg, attrName, attr_list[i]);
	}
	talloc_free(mem_ctx);
	return LDB_SUCCESS;

}
/* read all objectClasses */

static int kludge_acl_childClasses(struct ldb_context *ldb, struct ldb_message *msg,
				   const char *attrName) 
{
	struct ldb_message_element *oc_el;
	struct ldb_message_element *allowedClasses;
	const struct dsdb_schema *schema = dsdb_get_schema(ldb);
	const struct dsdb_class *sclass;
	int i, j, ret;

 	/* If we don't have a schema yet, we can't do anything... */
	if (schema == NULL) {
		return LDB_SUCCESS;
	}

	/* Must remove any existing attribute, or else confusion reins */
	ldb_msg_remove_attr(msg, attrName);
	ret = ldb_msg_add_empty(msg, attrName, 0, &allowedClasses);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	/* To ensure that oc_el is valid, we must look for it after 
	   we alter the element array in ldb_msg_add_empty() */
	oc_el = ldb_msg_find_element(msg, "objectClass");

	for (i=0; oc_el && i < oc_el->num_values; i++) {
		sclass = dsdb_class_by_lDAPDisplayName_ldb_val(schema, &oc_el->values[i]);
		if (!sclass) {
			/* We don't know this class?  what is going on? */
			continue;
		}

		for (j=0; sclass->possibleInferiors && sclass->possibleInferiors[j]; j++) {
			ldb_msg_add_string(msg, attrName, sclass->possibleInferiors[j]);
		}
	}
		
	if (allowedClasses->num_values > 1) {
		qsort(allowedClasses->values, 
		      allowedClasses->num_values, 
		      sizeof(*allowedClasses->values),
		      (comparison_fn_t)data_blob_cmp);
	
		for (i=1 ; i < allowedClasses->num_values; i++) {

			struct ldb_val *val1 = &allowedClasses->values[i-1];
			struct ldb_val *val2 = &allowedClasses->values[i];
			if (data_blob_cmp(val1, val2) == 0) {
				memmove(val1, val2, (allowedClasses->num_values - i) * sizeof( struct ldb_val)); 
				allowedClasses->num_values--;
				i--;
			}
		}
	}

	return LDB_SUCCESS;

}

/* find all attributes allowed by all these objectClasses */

static int kludge_acl_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct kludge_acl_context *ac;
	struct kludge_private_data *data;
	int i, ret;

	ac = talloc_get_type(req->context, struct kludge_acl_context);
	data = talloc_get_type(ldb_module_get_private(ac->module), struct kludge_private_data);
	ldb = ldb_module_get_ctx(ac->module);

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
		if (ac->allowedAttributes) {
			ret = kludge_acl_allowedAttributes(ldb,
						   ares->message,
						   "allowedAttributes");
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL, ret);
			}
		}
		if (ac->allowedChildClasses) {
			ret = kludge_acl_childClasses(ldb,
						ares->message,
						"allowedChildClasses");
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL, ret);
			}
		}

		if (data && data->password_attrs) /* if we are not initialized just get through */
		{
			switch (ac->user_type) {
			case SECURITY_SYSTEM:
				if (ac->allowedAttributesEffective) {
					ret = kludge_acl_allowedAttributes(ldb, ares->message,
									"allowedAttributesEffective");
					if (ret != LDB_SUCCESS) {
						return ldb_module_done(ac->req, NULL, NULL, ret);
					}
				}
				if (ac->allowedChildClassesEffective) {
					ret = kludge_acl_childClasses(ldb, ares->message,
									"allowedChildClassesEffective");
					if (ret != LDB_SUCCESS) {
						return ldb_module_done(ac->req, NULL, NULL, ret);
					}
				}
				break;

			case SECURITY_ADMINISTRATOR:
				if (ac->allowedAttributesEffective) {
					ret = kludge_acl_allowedAttributes(ldb, ares->message,
									"allowedAttributesEffective");
					if (ret != LDB_SUCCESS) {
						return ldb_module_done(ac->req, NULL, NULL, ret);
					}
				}
				if (ac->allowedChildClassesEffective) {
					ret = kludge_acl_childClasses(ldb, ares->message,
									"allowedChildClassesEffective");
					if (ret != LDB_SUCCESS) {
						return ldb_module_done(ac->req, NULL, NULL, ret);
					}
				}
				/* fall through */
			default:
				/* remove password attributes */
				for (i = 0; data->password_attrs[i]; i++) {
					ldb_msg_remove_attr(ares->message, data->password_attrs[i]);
				}
			}
		}

		if (ac->allowedAttributes ||
		    ac->allowedAttributesEffective ||
		    ac->allowedChildClasses ||
		    ac->allowedChildClassesEffective) {

			if (!ldb_attr_in_list(ac->attrs, "objectClass") &&
		            !ldb_attr_in_list(ac->attrs, "*")) {

				ldb_msg_remove_attr(ares->message,
						    "objectClass");
			}
		}

		return ldb_module_send_entry(ac->req, ares->message, ares->controls);

	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,
					ares->response, LDB_SUCCESS);

	}
	return LDB_SUCCESS;
}

static int kludge_acl_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct kludge_acl_context *ac;
	struct ldb_request *down_req;
	struct kludge_private_data *data;
	const char * const *attrs;
	int ret, i;
	struct ldb_control *sd_control;
	struct ldb_control **sd_saved_controls;

	ldb = ldb_module_get_ctx(module);

	ac = talloc(req, struct kludge_acl_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data = talloc_get_type(ldb_module_get_private(module), struct kludge_private_data);

	if (data && data->acl_perform)
		return ldb_next_request(module, req);

	ac->module = module;
	ac->req = req;
	ac->user_type = what_is_user(module);
	ac->attrs = req->op.search.attrs;

	ac->allowedAttributes = ldb_attr_in_list(req->op.search.attrs, "allowedAttributes");

	ac->allowedAttributesEffective = ldb_attr_in_list(req->op.search.attrs, "allowedAttributesEffective");

	ac->allowedChildClasses = ldb_attr_in_list(req->op.search.attrs, "allowedChildClasses");

	ac->allowedChildClassesEffective = ldb_attr_in_list(req->op.search.attrs, "allowedChildClassesEffective");

	if (ac->allowedAttributes || ac->allowedAttributesEffective || ac->allowedChildClasses || ac->allowedChildClassesEffective) {
		attrs = ldb_attr_list_copy_add(ac, req->op.search.attrs, "objectClass");
	} else {
		attrs = req->op.search.attrs;
	}

	/* replace any attributes in the parse tree that are private,
	   so we don't allow a search for 'userPassword=penguin',
	   just as we would not allow that attribute to be returned */
	switch (ac->user_type) {
	case SECURITY_SYSTEM:
		break;
	default:
	/* FIXME: We should copy the tree and keep the original unmodified. */
		/* remove password attributes */

		if (!data || !data->password_attrs) {
			break;
		}
		for (i = 0; data->password_attrs[i]; i++) {
			ldb_parse_tree_attr_replace(req->op.search.tree,
						    data->password_attrs[i],
						    "kludgeACLredactedattribute");
		}
	}

	ret = ldb_build_search_req_ex(&down_req,
					ldb, ac,
					req->op.search.base,
					req->op.search.scope,
					req->op.search.tree,
					attrs,
					req->controls,
					ac, kludge_acl_callback,
					req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* check if there's an SD_FLAGS control */
	sd_control = ldb_request_get_control(down_req, LDB_CONTROL_SD_FLAGS_OID);
	if (sd_control) {
		/* save it locally and remove it from the list */
		/* we do not need to replace them later as we
		 * are keeping the original req intact */
		if (!save_controls(sd_control, down_req, &sd_saved_controls)) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	/* perform the search */
	return ldb_next_request(module, down_req);
}

/* ANY change type */
static int kludge_acl_change(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	enum security_user_level user_type = what_is_user(module);
	struct kludge_private_data *data = talloc_get_type(ldb_module_get_private(module),
							   struct kludge_private_data);

	if (data->acl_perform)
		return ldb_next_request(module, req);

	switch (user_type) {
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		return ldb_next_request(module, req);
	default:
		ldb_asprintf_errstring(ldb,
				       "kludge_acl_change: "
				       "attempted database modify not permitted. "
				       "User %s is not SYSTEM or an administrator",
				       user_name(req, module));
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
}

static int kludge_acl_extended(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	enum security_user_level user_type;

	/* allow everybody to read the sequence number */
	if (strcmp(req->op.extended.oid,
		   LDB_EXTENDED_SEQUENCE_NUMBER) == 0) {
		return ldb_next_request(module, req);
	}

	user_type = what_is_user(module);

	switch (user_type) {
	case SECURITY_SYSTEM:
	case SECURITY_ADMINISTRATOR:
		return ldb_next_request(module, req);
	default:
		ldb_asprintf_errstring(ldb,
				       "kludge_acl_change: "
				       "attempted database modify not permitted. "
				       "User %s is not SYSTEM or an administrator",
				       user_name(req, module));
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
}

static int kludge_acl_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret, i;
	TALLOC_CTX *mem_ctx = talloc_new(module);
	static const char *attrs[] = { "passwordAttribute", NULL };
	struct ldb_result *res;
	struct ldb_message *msg;
	struct ldb_message_element *password_attributes;

	struct kludge_private_data *data;

	ldb = ldb_module_get_ctx(module);

	data = talloc(module, struct kludge_private_data);
	if (data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data->password_attrs = NULL;
	data->acl_perform = lp_parm_bool(ldb_get_opaque(ldb, "loadparm"),
					 NULL, "acl", "perform", false);
	ldb_module_set_private(module, data);

	if (!mem_ctx) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search(ldb, mem_ctx, &res,
			 ldb_dn_new(mem_ctx, ldb, "@KLUDGEACL"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		goto done;
	}
	if (res->count == 0) {
		goto done;
	}

	if (res->count > 1) {
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	msg = res->msgs[0];

	password_attributes = ldb_msg_find_element(msg, "passwordAttribute");
	if (!password_attributes) {
		goto done;
	}
	data->password_attrs = talloc_array(data, const char *, password_attributes->num_values + 1);
	if (!data->password_attrs) {
		talloc_free(mem_ctx);
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	for (i=0; i < password_attributes->num_values; i++) {
		data->password_attrs[i] = (const char *)password_attributes->values[i].data;	
		talloc_steal(data->password_attrs, password_attributes->values[i].data);
	}
	data->password_attrs[i] = NULL;

	ret = ldb_mod_register_control(module, LDB_CONTROL_SD_FLAGS_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			"kludge_acl: Unable to register control with rootdse!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

done:
	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_kludge_acl_module_ops = {
	.name		   = "kludge_acl",
	.search            = kludge_acl_search,
	.add               = kludge_acl_change,
	.modify            = kludge_acl_change,
	.del               = kludge_acl_change,
	.rename            = kludge_acl_change,
	.extended          = kludge_acl_extended,
	.init_context	   = kludge_acl_init
};
