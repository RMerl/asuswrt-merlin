/*
   ldb database library

   Copyright (C) Simo Sorce  2006-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2007
   Copyright (C) Nadezhda Ivanova  2009

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
 *  Component: DS Security descriptor module
 *
 *  Description:
 *  - Calculate the security descriptor of a newly created object
 *  - Perform sd recalculation on a move operation
 *  - Handle sd modification invariants
 *
 *  Author: Nadezhda Ivanova
 */

#include "includes.h"
#include "ldb_module.h"
#include "dlinklist.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "auth/auth.h"
#include "param/param.h"

struct descriptor_data {
	int _dummy;
};

struct descriptor_context {
		struct ldb_module *module;
		struct ldb_request *req;
		struct ldb_reply *search_res;
		int (*step_fn)(struct descriptor_context *);
};

static const struct dsdb_class * get_last_structural_class(const struct dsdb_schema *schema, struct ldb_message_element *element)
{
	const struct dsdb_class *last_class = NULL;
	int i;
	for (i = 0; i < element->num_values; i++){
		if (!last_class) {
			last_class = dsdb_class_by_lDAPDisplayName_ldb_val(schema, &element->values[i]);
		} else {
			const struct dsdb_class *tmp_class = dsdb_class_by_lDAPDisplayName_ldb_val(schema, &element->values[i]);
			if (tmp_class->subClass_order > last_class->subClass_order)
				last_class = tmp_class;
		}
	}
	return last_class;
}

struct dom_sid *get_default_ag(TALLOC_CTX *mem_ctx,
			       struct ldb_dn *dn,
			       struct security_token *token,
			       struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct ldb_dn *root_base_dn = ldb_get_root_basedn(ldb);
	struct ldb_dn *schema_base_dn = ldb_get_schema_basedn(ldb);
	struct ldb_dn *config_base_dn = ldb_get_config_basedn(ldb);
	const struct dom_sid *domain_sid = samdb_domain_sid(ldb);
	struct dom_sid *da_sid = dom_sid_add_rid(tmp_ctx, domain_sid, DOMAIN_RID_ADMINS);
	struct dom_sid *ea_sid = dom_sid_add_rid(tmp_ctx, domain_sid, DOMAIN_RID_ENTERPRISE_ADMINS);
	struct dom_sid *sa_sid = dom_sid_add_rid(tmp_ctx, domain_sid, DOMAIN_RID_SCHEMA_ADMINS);
	struct dom_sid *dag_sid;

	if (ldb_dn_compare_base(schema_base_dn, dn) == 0){
		if (security_token_has_sid(token, sa_sid))
			dag_sid = dom_sid_dup(mem_ctx, sa_sid);
		else if (security_token_has_sid(token, ea_sid))
			dag_sid = dom_sid_dup(mem_ctx, ea_sid);
		else if (security_token_has_sid(token, da_sid))
			dag_sid = dom_sid_dup(mem_ctx, da_sid);
		else
			dag_sid = NULL;
	}
	else if (ldb_dn_compare_base(config_base_dn, dn) == 0){
		if (security_token_has_sid(token, ea_sid))
			dag_sid = dom_sid_dup(mem_ctx, ea_sid);
		else if (security_token_has_sid(token, da_sid))
			dag_sid = dom_sid_dup(mem_ctx, da_sid);
		else
			dag_sid = NULL;
	}
	else if (ldb_dn_compare_base(root_base_dn, dn) == 0){
		if (security_token_has_sid(token, da_sid))
			dag_sid = dom_sid_dup(mem_ctx, da_sid);
		else if (security_token_has_sid(token, ea_sid))
				dag_sid = dom_sid_dup(mem_ctx, ea_sid);
		else
			dag_sid = NULL;
	}
	else
		dag_sid = NULL;

	talloc_free(tmp_ctx);
	return dag_sid;
}

static struct security_descriptor *get_sd_unpacked(struct ldb_module *module, TALLOC_CTX *mem_ctx,
					    const struct dsdb_class *objectclass)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct security_descriptor *sd;
	const struct dom_sid *domain_sid = samdb_domain_sid(ldb);

	if (!objectclass->defaultSecurityDescriptor || !domain_sid) {
		return NULL;
	}

	sd = sddl_decode(mem_ctx,
			 objectclass->defaultSecurityDescriptor,
			 domain_sid);
	return sd;
}

static struct dom_sid *get_default_group(TALLOC_CTX *mem_ctx,
					 struct ldb_context *ldb,
					 struct dom_sid *dag)
{
	int *domainFunctionality;

	domainFunctionality = talloc_get_type(
		ldb_get_opaque(ldb, "domainFunctionality"), int);

	if (*domainFunctionality
			&& (*domainFunctionality >= DS_DOMAIN_FUNCTION_2008)) {
		return dag;
	}

	return NULL;
}

static DATA_BLOB *get_new_descriptor(struct ldb_module *module,
				     struct ldb_dn *dn,
				     TALLOC_CTX *mem_ctx,
				     const struct dsdb_class *objectclass,
				     const struct ldb_val *parent,
				     struct ldb_val *object)
{
	struct security_descriptor *user_descriptor = NULL, *parent_descriptor = NULL;
	struct security_descriptor *new_sd;
	DATA_BLOB *linear_sd;
	enum ndr_err_code ndr_err;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= ldb_get_opaque(ldb, "sessionInfo");
	const struct dom_sid *domain_sid = samdb_domain_sid(ldb);
	char *sddl_sd;
	struct dom_sid *default_owner;
	struct dom_sid *default_group;

	if (object) {
		user_descriptor = talloc(mem_ctx, struct security_descriptor);
		if(!user_descriptor)
			return NULL;
		ndr_err = ndr_pull_struct_blob(object, user_descriptor, NULL,
					       user_descriptor,
					       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);

		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)){
			talloc_free(user_descriptor);
			return NULL;
		}
	} else {
		user_descriptor = get_sd_unpacked(module, mem_ctx, objectclass);
	}

	if (parent){
		parent_descriptor = talloc(mem_ctx, struct security_descriptor);
		if(!parent_descriptor)
			return NULL;
		ndr_err = ndr_pull_struct_blob(parent, parent_descriptor, NULL,
					       parent_descriptor,
					       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);

		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)){
			talloc_free(parent_descriptor);
			return NULL;
		}
	}
	default_owner = get_default_ag(mem_ctx, dn,
				       session_info->security_token, ldb);
	default_group = get_default_group(mem_ctx, ldb, default_owner);
	new_sd = create_security_descriptor(mem_ctx, parent_descriptor, user_descriptor, true,
					    NULL, SEC_DACL_AUTO_INHERIT|SEC_SACL_AUTO_INHERIT,
					    session_info->security_token,
					    default_owner, default_group,
					    map_generic_rights_ds);
	if (!new_sd)
		return NULL;


	sddl_sd = sddl_encode(mem_ctx, new_sd, domain_sid);
	DEBUG(10, ("Object %s created with desriptor %s", ldb_dn_get_linearized(dn), sddl_sd));

	linear_sd = talloc(mem_ctx, DATA_BLOB);
	if (!linear_sd) {
		return NULL;
	}

	ndr_err = ndr_push_struct_blob(linear_sd, mem_ctx,
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       new_sd,
				       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NULL;
	}

	return linear_sd;
}

static struct descriptor_context *descriptor_init_context(struct ldb_module *module,
							  struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct descriptor_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct descriptor_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;
	return ac;
}

static int get_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct descriptor_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct descriptor_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS &&
	    ares->error != LDB_ERR_NO_SUCH_OBJECT) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	ldb_reset_err_string(ldb);

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		if (ac->search_res != NULL) {
			ldb_set_errstring(ldb, "Too many results");
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->search_res = talloc_steal(ac, ares);
		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:
		talloc_free(ares);
		ret = ac->step_fn(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		break;
	}

	return LDB_SUCCESS;
}
static int descriptor_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct descriptor_context *ac;

	ac = talloc_get_type(req->context, struct descriptor_context);

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

static int descriptor_do_add(struct descriptor_context *ac)
{
	struct ldb_context *ldb;
	const struct dsdb_schema *schema;
	struct ldb_request *add_req;
	struct ldb_message_element *objectclass_element, *sd_element = NULL;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	int ret;
	struct ldb_val *sd_val = NULL;
	const struct ldb_val *parentsd_val = NULL;
	DATA_BLOB *sd;
	const struct dsdb_class *objectclass;

	ldb = ldb_module_get_ctx(ac->module);
	schema = dsdb_get_schema(ldb);

	mem_ctx = talloc_new(ac);
	if (mem_ctx == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);

	/* get the security descriptor values*/
	sd_element = ldb_msg_find_element(msg, "nTSecurityDescriptor");
	objectclass_element = ldb_msg_find_element(msg, "objectClass");
	objectclass = get_last_structural_class(schema, objectclass_element);

	if (!objectclass) {
		ldb_asprintf_errstring(ldb, "No last structural objectclass found on %s", ldb_dn_get_linearized(msg->dn));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (sd_element)
		sd_val = &sd_element->values[0];
	/* NC's have no parent */
	if ((ldb_dn_compare(msg->dn, (ldb_get_schema_basedn(ldb))) == 0) ||
	    (ldb_dn_compare(msg->dn, (ldb_get_config_basedn(ldb))) == 0) ||
	    (ldb_dn_compare(msg->dn, (ldb_get_root_basedn(ldb))) == 0)) {
		parentsd_val = NULL;
	} else if (ac->search_res != NULL){
		parentsd_val = ldb_msg_find_ldb_val(ac->search_res->message, "nTSecurityDescriptor");
	}

	/* get the parent descriptor and the one provided. If not provided, get the default.*/
	/* convert to security descriptor and calculate */
	sd = get_new_descriptor(ac->module, msg->dn, mem_ctx, objectclass,
			parentsd_val, sd_val);
	if (sd_val) {
		ldb_msg_remove_attr(msg, "nTSecurityDescriptor");
	}

	if (sd) {
		ret = ldb_msg_add_steal_value(msg, "nTSecurityDescriptor", sd);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	talloc_free(mem_ctx);
	ret = ldb_msg_sanity_check(ldb, msg);

	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, "No last structural objectclass found on %s", ldb_dn_get_linearized(msg->dn));
		return ret;
	}

	ret = ldb_build_add_req(&add_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, descriptor_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* perform the add */
	return ldb_next_request(ac->module, add_req);
}

static int descriptor_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *search_req;
	struct descriptor_context *ac;
	struct ldb_dn *parent_dn;
	int ret;
	struct descriptor_data *data;
	static const char * const descr_attrs[] = { "nTSecurityDescriptor", NULL };

	data = talloc_get_type(ldb_module_get_private(module), struct descriptor_data);
	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "descriptor_add\n");

	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ac = descriptor_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* If there isn't a parent, just go on to the add processing */
	if (ldb_dn_get_comp_num(ac->req->op.add.message->dn) == 1) {
		return descriptor_do_add(ac);
	}

	/* get copy of parent DN */
	parent_dn = ldb_dn_get_parent(ac, ac->req->op.add.message->dn);
	if (parent_dn == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&search_req, ldb,
				   ac, parent_dn, LDB_SCOPE_BASE,
				   "(objectClass=*)", descr_attrs,
				   NULL,
				   ac, get_search_callback,
				   req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	talloc_steal(search_req, parent_dn);

	ac->step_fn = descriptor_do_add;

	return ldb_next_request(ac->module, search_req);
}
/* TODO */
static int descriptor_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "descriptor_modify\n");
	return ldb_next_request(module, req);
}
/* TODO */
static int descriptor_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	ldb_debug(ldb, LDB_DEBUG_TRACE, "descriptor_rename\n");
	return ldb_next_request(module, req);
}

static int descriptor_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct descriptor_data *data;

	ldb = ldb_module_get_ctx(module);
	data = talloc(module, struct descriptor_data);
	if (data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_module_set_private(module, data);
	return ldb_next_init(module);
}


_PUBLIC_ const struct ldb_module_ops ldb_descriptor_module_ops = {
	.name		   = "descriptor",
	.add           = descriptor_add,
	.modify        = descriptor_modify,
	.rename        = descriptor_rename,
	.init_context  = descriptor_init
};


