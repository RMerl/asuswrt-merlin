/*
   ldb database library

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2009

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
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"

static int resolve_oids_need_value(struct ldb_context *ldb,
				   struct dsdb_schema *schema,
				   const struct dsdb_attribute *a,
				   const struct ldb_val *valp)
{
	const struct dsdb_attribute *va = NULL;
	const struct dsdb_class *vo = NULL;
	const void *p2;
	char *str = NULL;

	if (a->syntax->oMSyntax != 6) {
		return LDB_ERR_COMPARE_FALSE;
	}

	if (valp) {
		p2 = memchr(valp->data, '.', valp->length);
	} else {
		p2 = NULL;
	}

	if (!p2) {
		return LDB_ERR_COMPARE_FALSE;
	}

	switch (a->attributeID_id) {
	case DRSUAPI_ATTID_objectClass:
	case DRSUAPI_ATTID_subClassOf:
	case DRSUAPI_ATTID_auxiliaryClass:
	case DRSUAPI_ATTID_systemPossSuperiors:
	case DRSUAPI_ATTID_possSuperiors:
		str = talloc_strndup(ldb, (char *)valp->data, valp->length);
		if (!str) {
			return ldb_oom(ldb);
		}
		vo = dsdb_class_by_governsID_oid(schema, str);
		talloc_free(str);
		if (!vo) {
			return LDB_ERR_COMPARE_FALSE;
		}
		return LDB_ERR_COMPARE_TRUE;
	case DRSUAPI_ATTID_systemMustContain:
	case DRSUAPI_ATTID_systemMayContain:
	case DRSUAPI_ATTID_mustContain:
	case DRSUAPI_ATTID_mayContain:
		str = talloc_strndup(ldb, (char *)valp->data, valp->length);
		if (!str) {
			return ldb_oom(ldb);
		}
		va = dsdb_attribute_by_attributeID_oid(schema, str);
		talloc_free(str);
		if (!va) {
			return LDB_ERR_COMPARE_FALSE;
		}
		return LDB_ERR_COMPARE_TRUE;
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
		return LDB_ERR_COMPARE_FALSE;
	}

	return LDB_ERR_COMPARE_FALSE;
}

static int resolve_oids_parse_tree_need(struct ldb_context *ldb,
					struct dsdb_schema *schema,
					const struct ldb_parse_tree *tree)
{
	unsigned int i;
	const struct dsdb_attribute *a = NULL;
	const char *attr;
	const char *p1;
	const void *p2;
	const struct ldb_val *valp = NULL;
	int ret;

	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ret = resolve_oids_parse_tree_need(ldb, schema,
						tree->u.list.elements[i]);
			if (ret != LDB_ERR_COMPARE_FALSE) {
				return ret;
			}
		}
		return LDB_ERR_COMPARE_FALSE;
	case LDB_OP_NOT:
		return resolve_oids_parse_tree_need(ldb, schema,
						tree->u.isnot.child);
	case LDB_OP_EQUALITY:
	case LDB_OP_GREATER:
	case LDB_OP_LESS:
	case LDB_OP_APPROX:
		attr = tree->u.equality.attr;
		valp = &tree->u.equality.value;
		break;
	case LDB_OP_SUBSTRING:
		attr = tree->u.substring.attr;
		break;
	case LDB_OP_PRESENT:
		attr = tree->u.present.attr;
		break;
	case LDB_OP_EXTENDED:
		attr = tree->u.extended.attr;
		valp = &tree->u.extended.value;
		break;
	default:
		return LDB_ERR_COMPARE_FALSE;
	}

	p1 = strchr(attr, '.');

	if (valp) {
		p2 = memchr(valp->data, '.', valp->length);
	} else {
		p2 = NULL;
	}

	if (!p1 && !p2) {
		return LDB_ERR_COMPARE_FALSE;
	}

	if (p1) {
		a = dsdb_attribute_by_attributeID_oid(schema, attr);
	} else {
		a = dsdb_attribute_by_lDAPDisplayName(schema, attr);
	}
	if (!a) {
		return LDB_ERR_COMPARE_FALSE;
	}

	if (!p2) {
		return LDB_ERR_COMPARE_FALSE;
	}

	return resolve_oids_need_value(ldb, schema, a, valp);
}

static int resolve_oids_element_need(struct ldb_context *ldb,
				     struct dsdb_schema *schema,
				     const struct ldb_message_element *el)
{
	unsigned int i;
	const struct dsdb_attribute *a = NULL;
	const char *p1;

	p1 = strchr(el->name, '.');

	if (p1) {
		a = dsdb_attribute_by_attributeID_oid(schema, el->name);
	} else {
		a = dsdb_attribute_by_lDAPDisplayName(schema, el->name);
	}
	if (!a) {
		return LDB_ERR_COMPARE_FALSE;
	}

	for (i=0; i < el->num_values; i++) {
		int ret;
		ret = resolve_oids_need_value(ldb, schema, a,
					      &el->values[i]);
		if (ret != LDB_ERR_COMPARE_FALSE) {
			return ret;
		}
	}

	return LDB_ERR_COMPARE_FALSE;
}

static int resolve_oids_message_need(struct ldb_context *ldb,
				     struct dsdb_schema *schema,
				     const struct ldb_message *msg)
{
	unsigned int i;

	for (i=0; i < msg->num_elements; i++) {
		int ret;
		ret = resolve_oids_element_need(ldb, schema,
						&msg->elements[i]);
		if (ret != LDB_ERR_COMPARE_FALSE) {
			return ret;
		}
	}

	return LDB_ERR_COMPARE_FALSE;
}

static int resolve_oids_replace_value(struct ldb_context *ldb,
				      struct dsdb_schema *schema,
				      const struct dsdb_attribute *a,
				      struct ldb_val *valp)
{
	const struct dsdb_attribute *va = NULL;
	const struct dsdb_class *vo = NULL;
	const void *p2;
	char *str = NULL;

	if (a->syntax->oMSyntax != 6) {
		return LDB_SUCCESS;
	}

	if (valp) {
		p2 = memchr(valp->data, '.', valp->length);
	} else {
		p2 = NULL;
	}

	if (!p2) {
		return LDB_SUCCESS;
	}

	switch (a->attributeID_id) {
	case DRSUAPI_ATTID_objectClass:
	case DRSUAPI_ATTID_subClassOf:
	case DRSUAPI_ATTID_auxiliaryClass:
	case DRSUAPI_ATTID_systemPossSuperiors:
	case DRSUAPI_ATTID_possSuperiors:
		str = talloc_strndup(schema, (char *)valp->data, valp->length);
		if (!str) {
			return ldb_oom(ldb);
		}
		vo = dsdb_class_by_governsID_oid(schema, str);
		talloc_free(str);
		if (!vo) {
			return LDB_SUCCESS;
		}
		*valp = data_blob_string_const(vo->lDAPDisplayName);
		return LDB_SUCCESS;
	case DRSUAPI_ATTID_systemMustContain:
	case DRSUAPI_ATTID_systemMayContain:
	case DRSUAPI_ATTID_mustContain:
	case DRSUAPI_ATTID_mayContain:
		str = talloc_strndup(schema, (char *)valp->data, valp->length);
		if (!str) {
			return ldb_oom(ldb);
		}
		va = dsdb_attribute_by_attributeID_oid(schema, str);
		talloc_free(str);
		if (!va) {
			return LDB_SUCCESS;
		}
		*valp = data_blob_string_const(va->lDAPDisplayName);
		return LDB_SUCCESS;
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
		return LDB_SUCCESS;
	}

	return LDB_SUCCESS;
}

static int resolve_oids_parse_tree_replace(struct ldb_context *ldb,
					   struct dsdb_schema *schema,
					   struct ldb_parse_tree *tree)
{
	unsigned int i;
	const struct dsdb_attribute *a = NULL;
	const char **attrp;
	const char *p1;
	const void *p2;
	struct ldb_val *valp = NULL;
	int ret;

	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ret = resolve_oids_parse_tree_replace(ldb, schema,
							tree->u.list.elements[i]);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
		return LDB_SUCCESS;
	case LDB_OP_NOT:
		return resolve_oids_parse_tree_replace(ldb, schema,
						tree->u.isnot.child);
	case LDB_OP_EQUALITY:
	case LDB_OP_GREATER:
	case LDB_OP_LESS:
	case LDB_OP_APPROX:
		attrp = &tree->u.equality.attr;
		valp = &tree->u.equality.value;
		break;
	case LDB_OP_SUBSTRING:
		attrp = &tree->u.substring.attr;
		break;
	case LDB_OP_PRESENT:
		attrp = &tree->u.present.attr;
		break;
	case LDB_OP_EXTENDED:
		attrp = &tree->u.extended.attr;
		valp = &tree->u.extended.value;
		break;
	default:
		return LDB_SUCCESS;
	}

	p1 = strchr(*attrp, '.');

	if (valp) {
		p2 = memchr(valp->data, '.', valp->length);
	} else {
		p2 = NULL;
	}

	if (!p1 && !p2) {
		return LDB_SUCCESS;
	}

	if (p1) {
		a = dsdb_attribute_by_attributeID_oid(schema, *attrp);
	} else {
		a = dsdb_attribute_by_lDAPDisplayName(schema, *attrp);
	}
	if (!a) {
		return LDB_SUCCESS;
	}

	*attrp = a->lDAPDisplayName;

	if (!p2) {
		return LDB_SUCCESS;
	}

	if (a->syntax->oMSyntax != 6) {
		return LDB_SUCCESS;
	}

	return resolve_oids_replace_value(ldb, schema, a, valp);
}

static int resolve_oids_element_replace(struct ldb_context *ldb,
					struct dsdb_schema *schema,
					struct ldb_message_element *el)
{
	unsigned int i;
	const struct dsdb_attribute *a = NULL;
	const char *p1;

	p1 = strchr(el->name, '.');

	if (p1) {
		a = dsdb_attribute_by_attributeID_oid(schema, el->name);
	} else {
		a = dsdb_attribute_by_lDAPDisplayName(schema, el->name);
	}
	if (!a) {
		return LDB_SUCCESS;
	}

	el->name = a->lDAPDisplayName;

	for (i=0; i < el->num_values; i++) {
		int ret;
		ret = resolve_oids_replace_value(ldb, schema, a,
						 &el->values[i]);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

static int resolve_oids_message_replace(struct ldb_context *ldb,
					struct dsdb_schema *schema,
					struct ldb_message *msg)
{
	unsigned int i;

	for (i=0; i < msg->num_elements; i++) {
		int ret;
		ret = resolve_oids_element_replace(ldb, schema,
						   &msg->elements[i]);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

struct resolve_oids_context {
	struct ldb_module *module;
	struct ldb_request *req;
};

static int resolve_oids_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct resolve_oids_context *ac;

	ac = talloc_get_type_abort(req->context, struct resolve_oids_context);
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
		return ldb_module_send_entry(ac->req, ares->message, ares->controls);

	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, LDB_SUCCESS);

	}
	return LDB_SUCCESS;
}

static int resolve_oids_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
	struct ldb_parse_tree *tree;
	struct ldb_request *down_req;
	struct resolve_oids_context *ac;
	int ret;
	bool needed = false;
	const char * const *attrs1;
	const char **attrs2;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);
	schema = dsdb_get_schema(ldb, NULL);

	if (!schema) {
		return ldb_next_request(module, req);
	}

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.search.base)) {
		return ldb_next_request(module, req);
	}

	ret = resolve_oids_parse_tree_need(ldb, schema,
					   req->op.search.tree);
	if (ret == LDB_ERR_COMPARE_TRUE) {
		needed = true;
	} else if (ret != LDB_ERR_COMPARE_FALSE) {
		return ret;
	}

	attrs1 = req->op.search.attrs;

	for (i=0; attrs1 && attrs1[i]; i++) {
		const char *p;
		const struct dsdb_attribute *a;

		p = strchr(attrs1[i], '.');
		if (p == NULL) {
			continue;
		}

		a = dsdb_attribute_by_attributeID_oid(schema, attrs1[i]);
		if (a == NULL) {
			continue;
		}

		needed = true;
		break;
	}

	if (!needed) {
		return ldb_next_request(module, req);
	}

	ac = talloc(req, struct resolve_oids_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}
	ac->module = module;
	ac->req = req;

	tree = ldb_parse_tree_copy_shallow(ac, req->op.search.tree);
	if (!tree) {
		return ldb_oom(ldb);
	}

	schema = talloc_reference(tree, schema);
	if (!schema) {
		return ldb_oom(ldb);
	}

	ret = resolve_oids_parse_tree_replace(ldb, schema,
					      tree);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	attrs2 = str_list_copy_const(ac,
				     discard_const_p(const char *, req->op.search.attrs));
	if (req->op.search.attrs && !attrs2) {
		return ldb_oom(ldb);
	}

	for (i=0; attrs2 && attrs2[i]; i++) {
		const char *p;
		const struct dsdb_attribute *a;

		p = strchr(attrs2[i], '.');
		if (p == NULL) {
			continue;
		}

		a = dsdb_attribute_by_attributeID_oid(schema, attrs2[i]);
		if (a == NULL) {
			continue;
		}

		attrs2[i] = a->lDAPDisplayName;
	}

	ret = ldb_build_search_req_ex(&down_req, ldb, ac,
				      req->op.search.base,
				      req->op.search.scope,
				      tree,
				      attrs2,
				      req->controls,
				      ac, resolve_oids_callback,
				      req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

static int resolve_oids_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
	int ret;
	struct ldb_message *msg;
	struct ldb_request *down_req;
	struct resolve_oids_context *ac;

	ldb = ldb_module_get_ctx(module);
	schema = dsdb_get_schema(ldb, NULL);

	if (!schema) {
		return ldb_next_request(module, req);
	}

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ret = resolve_oids_message_need(ldb, schema,
					req->op.add.message);
	if (ret == LDB_ERR_COMPARE_FALSE) {
		return ldb_next_request(module, req);
	} else if (ret != LDB_ERR_COMPARE_TRUE) {
		return ret;
	}

	ac = talloc(req, struct resolve_oids_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}
	ac->module = module;
	ac->req = req;

	msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);
	if (!msg) {
		return ldb_oom(ldb);
	}

	if (!talloc_reference(msg, schema)) {
		return ldb_oom(ldb);
	}

	ret = resolve_oids_message_replace(ldb, schema, msg);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, resolve_oids_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

static int resolve_oids_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
	int ret;
	struct ldb_message *msg;
	struct ldb_request *down_req;
	struct resolve_oids_context *ac;

	ldb = ldb_module_get_ctx(module);
	schema = dsdb_get_schema(ldb, NULL);

	if (!schema) {
		return ldb_next_request(module, req);
	}

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ret = resolve_oids_message_need(ldb, schema,
					req->op.mod.message);
	if (ret == LDB_ERR_COMPARE_FALSE) {
		return ldb_next_request(module, req);
	} else if (ret != LDB_ERR_COMPARE_TRUE) {
		return ret;
	}

	ac = talloc(req, struct resolve_oids_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}
	ac->module = module;
	ac->req = req;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		return ldb_oom(ldb);
	}

	if (!talloc_reference(msg, schema)) {
		return ldb_oom(ldb);
	}

	ret = resolve_oids_message_replace(ldb, schema, msg);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, resolve_oids_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

static const struct ldb_module_ops ldb_resolve_oids_module_ops = {
	.name		= "resolve_oids",
	.search		= resolve_oids_search,
	.add		= resolve_oids_add,
	.modify		= resolve_oids_modify,
};


int ldb_resolve_oids_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_resolve_oids_module_ops);
}
