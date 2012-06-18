/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007
   Copyright (C) Simo Sorce <idra@samba.org> 2008

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
 *  Component: ldb linked_attributes module
 *
 *  Description: Module to ensure linked attribute pairs remain in sync
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"
#include "dlinklist.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_misc.h"

struct la_private {
	struct la_context *la_list;
};

struct la_op_store {
	struct la_op_store *next;
	struct la_op_store *prev;
	enum la_op {LA_OP_ADD, LA_OP_DEL} op;
	struct GUID guid;
	char *name;
	char *value;
};

struct replace_context {
	struct la_context *ac;
	unsigned int num_elements;
	struct ldb_message_element *el;
};

struct la_context {
	struct la_context *next, *prev;
	const struct dsdb_schema *schema;
	struct ldb_module *module;
	struct ldb_request *req;
	struct ldb_dn *partition_dn;
	struct ldb_dn *add_dn;
	struct ldb_dn *del_dn;
	struct replace_context *rc;
	struct la_op_store *ops;
	struct ldb_extended *op_response;
	struct ldb_control **op_controls;
};

static struct la_context *linked_attributes_init(struct ldb_module *module,
						 struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct la_context *ac;
	const struct ldb_control *partition_ctrl;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct la_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return NULL;
	}

	ac->schema = dsdb_get_schema(ldb);
	ac->module = module;
	ac->req = req;

	/* remember the partition DN that came in, if given */
	partition_ctrl = ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID);
	if (partition_ctrl) {
		const struct dsdb_control_current_partition *partition;
		partition = talloc_get_type(partition_ctrl->data,
					    struct dsdb_control_current_partition);
		SMB_ASSERT(partition && partition->version == DSDB_CONTROL_CURRENT_PARTITION_VERSION);
	
		ac->partition_dn = ldb_dn_copy(ac, partition->dn);
	}

	return ac;
}

/*
  turn a DN into a GUID
 */
static int la_guid_from_dn(struct la_context *ac, struct ldb_dn *dn, struct GUID *guid)
{
	const struct ldb_val *guid_val;
	int ret;

	guid_val = ldb_dn_get_extended_component(dn, "GUID");
	if (guid_val) {
		/* there is a GUID embedded in the DN */
		enum ndr_err_code ndr_err;
		ndr_err = ndr_pull_struct_blob(guid_val, ac, NULL, guid,
					       (ndr_pull_flags_fn_t)ndr_pull_GUID);
		if (ndr_err != NDR_ERR_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to parse GUID\n"));
			return LDB_ERR_OPERATIONS_ERROR;
		}
	} else {
		ret = dsdb_find_guid_by_dn(ldb_module_get_ctx(ac->module), dn, guid);
		if (ret != LDB_SUCCESS) {
			DEBUG(4,(__location__ ": Failed to find GUID for dn %s\n",
				 ldb_dn_get_linearized(dn)));
			return ret;
		}
	}
	return LDB_SUCCESS;
}


/* Common routine to handle reading the attributes and creating a
 * series of modify requests */
static int la_store_op(struct la_context *ac,
		       enum la_op op, struct ldb_val *dn,
		       const char *name)
{
	struct ldb_context *ldb;
	struct la_op_store *os;
	struct ldb_dn *op_dn;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	op_dn = ldb_dn_from_ldb_val(ac, ldb, dn);
	if (!op_dn) {
		ldb_asprintf_errstring(ldb, 
				       "could not parse attribute as a DN");
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	os = talloc_zero(ac, struct la_op_store);
	if (!os) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	os->op = op;

	ret = la_guid_from_dn(ac, op_dn, &os->guid);
	if (ret == LDB_ERR_NO_SUCH_OBJECT && ac->req->operation == LDB_DELETE) {
		/* we are deleting an object, and we've found it has a
		 * forward link to a target that no longer
		 * exists. This is not an error in the delete, and we
		 * should just not do the deferred delete of the
		 * target attribute
		 */
		talloc_free(os);
		return LDB_SUCCESS;
	}
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	os->name = talloc_strdup(os, name);
	if (!os->name) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Do deletes before adds */
	if (op == LA_OP_ADD) {
		DLIST_ADD_END(ac->ops, os, struct la_op_store *);
	} else {
		/* By adding to the head of the list, we do deletes before
		 * adds when processing a replace */
		DLIST_ADD(ac->ops, os);
	}

	return LDB_SUCCESS;
}

static int la_op_search_callback(struct ldb_request *req,
				 struct ldb_reply *ares);
static int la_queue_mod_request(struct la_context *ac);
static int la_down_req(struct la_context *ac);



/* add */
static int linked_attributes_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	const struct dsdb_attribute *target_attr;
	struct la_context *ac;
	const char *attr_name;
	int ret;
	int i, j;

	ldb = ldb_module_get_ctx(module);

	if (ldb_dn_is_special(req->op.add.message->dn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ac = linked_attributes_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	/* Need to ensure we only have forward links being specified */
	for (i=0; i < req->op.add.message->num_elements; i++) {
		const struct ldb_message_element *el = &req->op.add.message->elements[i];
		const struct dsdb_attribute *schema_attr
			= dsdb_attribute_by_lDAPDisplayName(ac->schema, el->name);
		if (!schema_attr) {
			ldb_asprintf_errstring(ldb, 
					       "attribute %s is not a valid attribute in schema", el->name);
			return LDB_ERR_OBJECT_CLASS_VIOLATION;			
		}
		/* We have a valid attribute, now find out if it is linked */
		if (schema_attr->linkID == 0) {
			continue;
		}
		
		if ((schema_attr->linkID & 1) == 1) {
			/* Odd is for the target.  Illegal to modify */
			ldb_asprintf_errstring(ldb, 
					       "attribute %s must not be modified directly, it is a linked attribute", el->name);
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		
		/* Even link IDs are for the originating attribute */
		target_attr = dsdb_attribute_by_linkID(ac->schema, schema_attr->linkID + 1);
		if (!target_attr) {
			/*
			 * windows 2003 has a broken schema where
			 * the definition of msDS-IsDomainFor
			 * is missing (which is supposed to be
			 * the backlink of the msDS-HasDomainNCs
			 * attribute
			 */
			continue;
		}

		attr_name = target_attr->lDAPDisplayName;

		for (j = 0; j < el->num_values; j++) {
			ret = la_store_op(ac, LA_OP_ADD,
					  &el->values[j],
					  attr_name);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
	}

	/* if no linked attributes are present continue */
	if (ac->ops == NULL) {
		/* nothing to do for this module, proceed */
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	/* start with the original request */
	return la_down_req(ac);
}

/* For a delete or rename, we need to find out what linked attributes
 * are currently on this DN, and then deal with them.  This is the
 * callback to the base search */

static int la_mod_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	const struct dsdb_attribute *schema_attr;
	const struct dsdb_attribute *target_attr;
	struct ldb_message_element *search_el;
	struct replace_context *rc;
	struct la_context *ac;
	const char *attr_name;
	int i, j;
	int ret = LDB_SUCCESS;

	ac = talloc_get_type(req->context, struct la_context);
	ldb = ldb_module_get_ctx(ac->module);
	rc = ac->rc;

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* Only entries are interesting, and we only want the olddn */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:

		if (ldb_dn_compare(ares->message->dn, ac->req->op.mod.message->dn) != 0) {
			ldb_asprintf_errstring(ldb, 
					       "linked_attributes: %s is not the DN we were looking for", ldb_dn_get_linearized(ares->message->dn));
			/* Guh?  We only asked for this DN */
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		ac->add_dn = ac->del_dn = talloc_steal(ac, ares->message->dn);

		/* We don't populate 'rc' for ADD - it can't be deleting elements anyway */
		for (i = 0; rc && i < rc->num_elements; i++) {

			schema_attr = dsdb_attribute_by_lDAPDisplayName(ac->schema, rc->el[i].name);
			if (!schema_attr) {
				ldb_asprintf_errstring(ldb,
					"attribute %s is not a valid attribute in schema",
					rc->el[i].name);
				talloc_free(ares);
				return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OBJECT_CLASS_VIOLATION);
			}

			search_el = ldb_msg_find_element(ares->message,
							 rc->el[i].name);

			/* See if this element already exists */
			/* otherwise just ignore as
			 * the add has already been scheduled */
			if ( ! search_el) {
				continue;
			}

			target_attr = dsdb_attribute_by_linkID(ac->schema, schema_attr->linkID + 1);
			if (!target_attr) {
				/*
				 * windows 2003 has a broken schema where
				 * the definition of msDS-IsDomainFor
				 * is missing (which is supposed to be
				 * the backlink of the msDS-HasDomainNCs
				 * attribute
				 */
				continue;
			}
			attr_name = target_attr->lDAPDisplayName;

			/* Now we know what was there, we can remove it for the re-add */
			for (j = 0; j < search_el->num_values; j++) {
				ret = la_store_op(ac, LA_OP_DEL,
						  &search_el->values[j],
						  attr_name);
				if (ret != LDB_SUCCESS) {
					talloc_free(ares);
					return ldb_module_done(ac->req,
							       NULL, NULL, ret);
				}
			}
		}

		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);

		if (ac->req->operation == LDB_ADD) {
			/* Start the modifies to the backlinks */
			ret = la_queue_mod_request(ac);

			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL,
						       ret);
			}
		} else {
			/* Start with the original request */
			ret = la_down_req(ac);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL, ret);
			}
		}
		return LDB_SUCCESS;
	}

	talloc_free(ares);
	return ret;
}


/* modify */
static int linked_attributes_modify(struct ldb_module *module, struct ldb_request *req)
{
	/* Look over list of modifications */
	/* Find if any are for linked attributes */
	/* Determine the effect of the modification */
	/* Apply the modify to the linked entry */

	struct ldb_context *ldb;
	int i, j;
	struct la_context *ac;
	struct ldb_request *search_req;
	const char **attrs;

	int ret;

	ldb = ldb_module_get_ctx(module);

	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ac = linked_attributes_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		return ldb_next_request(module, req);
	}

	ac->rc = talloc_zero(ac, struct replace_context);
	if (!ac->rc) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i < req->op.mod.message->num_elements; i++) {
		bool store_el = false;
		const char *attr_name;
		const struct dsdb_attribute *target_attr;
		const struct ldb_message_element *el = &req->op.mod.message->elements[i];
		const struct dsdb_attribute *schema_attr
			= dsdb_attribute_by_lDAPDisplayName(ac->schema, el->name);
		if (!schema_attr) {
			ldb_asprintf_errstring(ldb, 
					       "attribute %s is not a valid attribute in schema", el->name);
			return LDB_ERR_OBJECT_CLASS_VIOLATION;			
		}
		/* We have a valid attribute, now find out if it is linked */
		if (schema_attr->linkID == 0) {
			continue;
		}
		
		if ((schema_attr->linkID & 1) == 1) {
			/* Odd is for the target.  Illegal to modify */
			ldb_asprintf_errstring(ldb, 
					       "attribute %s must not be modified directly, it is a linked attribute", el->name);
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		
		/* Even link IDs are for the originating attribute */
		
		/* Now find the target attribute */
		target_attr = dsdb_attribute_by_linkID(ac->schema, schema_attr->linkID + 1);
		if (!target_attr) {
			/*
			 * windows 2003 has a broken schema where
			 * the definition of msDS-IsDomainFor
			 * is missing (which is supposed to be
			 * the backlink of the msDS-HasDomainNCs
			 * attribute
			 */
			continue;
		}

		attr_name = target_attr->lDAPDisplayName;
	
		switch (el->flags & LDB_FLAG_MOD_MASK) {
		case LDB_FLAG_MOD_REPLACE:
			/* treat as just a normal add the delete part is handled by the callback */
			store_el = true;

			/* break intentionally missing */

		case LDB_FLAG_MOD_ADD:

			/* For each value being added, we need to setup the adds */
			for (j = 0; j < el->num_values; j++) {
				ret = la_store_op(ac, LA_OP_ADD,
						  &el->values[j],
						  attr_name);
				if (ret != LDB_SUCCESS) {
					return ret;
				}
			}
			break;

		case LDB_FLAG_MOD_DELETE:

			if (el->num_values) {
				/* For each value being deleted, we need to setup the delete */
				for (j = 0; j < el->num_values; j++) {
					ret = la_store_op(ac, LA_OP_DEL,
							  &el->values[j],
							  attr_name);
					if (ret != LDB_SUCCESS) {
						return ret;
					}
				}
			} else {
				/* Flag that there was a DELETE
				 * without a value specified, so we
				 * need to look for the old value */
				store_el = true;
			}

			break;
		}

		if (store_el) {
			struct ldb_message_element *search_el;

			search_el = talloc_realloc(ac->rc, ac->rc->el,
						   struct ldb_message_element,
						   ac->rc->num_elements +1);
			if (!search_el) {
				ldb_oom(ldb);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ac->rc->el = search_el;

			ac->rc->el[ac->rc->num_elements] = *el;
			ac->rc->num_elements++;
		}
	}
	
	if (ac->ops || ac->rc->el) {
		/* both replace and delete without values are handled in the callback
		 * after the search on the entry to be modified is performed */
		
		attrs = talloc_array(ac->rc, const char *, ac->rc->num_elements + 1);
		if (!attrs) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		for (i = 0; ac->rc && i < ac->rc->num_elements; i++) {
			attrs[i] = ac->rc->el[i].name;
		}
		attrs[i] = NULL;
		
		/* The callback does all the hard work here */
		ret = ldb_build_search_req(&search_req, ldb, ac,
					   req->op.mod.message->dn,
					   LDB_SCOPE_BASE,
					   "(objectClass=*)", attrs,
					   NULL,
					   ac, la_mod_search_callback,
					   req);

		/* We need to figure out our own extended DN, to fill in as the backlink target */
		if (ret == LDB_SUCCESS) {
			ret = ldb_request_add_control(search_req,
						      LDB_CONTROL_EXTENDED_DN_OID,
						      false, NULL);
		}
		if (ret == LDB_SUCCESS) {
			talloc_steal(search_req, attrs);
			
			ret = ldb_next_request(module, search_req);
		}

	} else {
		/* nothing to do for this module, proceed */
		talloc_free(ac);
		ret = ldb_next_request(module, req);
	}

	return ret;
}

/* delete */
static int linked_attributes_del(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *search_req;
	struct la_context *ac;
	const char **attrs;
	WERROR werr;
	int ret;

	/* This gets complex:  We need to:
	   - Do a search for the entry
	   - Wait for these result to appear
	   - In the callback for the result, issue a modify
		request based on the linked attributes found
	   - Wait for each modify result
	   - Regain our sainity
	*/

	ldb = ldb_module_get_ctx(module);

	ac = linked_attributes_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		return ldb_next_request(module, req);
	}

	werr = dsdb_linked_attribute_lDAPDisplayName_list(ac->schema, ac, &attrs);
	if (!W_ERROR_IS_OK(werr)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&search_req, ldb, req,
				   req->op.del.dn, LDB_SCOPE_BASE,
				   "(objectClass=*)", attrs,
				   NULL,
				   ac, la_op_search_callback,
				   req);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	talloc_steal(search_req, attrs);

	return ldb_next_request(module, search_req);
}

/* rename */
static int linked_attributes_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct la_context *ac;

	/* This gets complex:  We need to:
	   - Do a search for the entry
	   - Wait for these result to appear
	   - In the callback for the result, issue a modify
		request based on the linked attributes found
	   - Wait for each modify result
	   - Regain our sainity
	*/

	ac = linked_attributes_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		return ldb_next_request(module, req);
	}

	/* start with the original request */
	return la_down_req(ac);
}


static int la_op_search_callback(struct ldb_request *req,
				 struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct la_context *ac;
	const struct dsdb_attribute *schema_attr;
	const struct dsdb_attribute *target_attr;
	const struct ldb_message_element *el;
	const char *attr_name;
	int i, j;
	int ret;

	ac = talloc_get_type(req->context, struct la_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* Only entries are interesting, and we only want the olddn */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ret = ldb_dn_compare(ares->message->dn, req->op.search.base);
		if (ret != 0) {
			/* Guh?  We only asked for this DN */
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		if (ares->message->num_elements == 0) {
			/* only bother at all if there were some
			 * linked attributes found */
			talloc_free(ares);
			return LDB_SUCCESS;
		}

		switch (ac->req->operation) {
		case LDB_DELETE:
			ac->del_dn = talloc_steal(ac, ares->message->dn);
			break;
		case LDB_RENAME:
			ac->add_dn = talloc_steal(ac, ares->message->dn); 
			ac->del_dn = talloc_steal(ac, ac->req->op.rename.olddn);
			break;
		default:
			talloc_free(ares);
			ldb_set_errstring(ldb,
					  "operations must be delete or rename");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		for (i = 0; i < ares->message->num_elements; i++) {
			el = &ares->message->elements[i];

			schema_attr = dsdb_attribute_by_lDAPDisplayName(ac->schema, el->name);
			if (!schema_attr) {
				ldb_asprintf_errstring(ldb,
					"attribute %s is not a valid attribute"
					" in schema", el->name);
				talloc_free(ares);
				return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OBJECT_CLASS_VIOLATION);
			}

			/* Valid attribute, now find out if it is linked */
			if (schema_attr->linkID == 0) {
				/* Not a linked attribute, skip */
				continue;
			}

			if ((schema_attr->linkID & 1) == 0) {
				/* Odd is for the target. */
				target_attr = dsdb_attribute_by_linkID(ac->schema, schema_attr->linkID + 1);
				if (!target_attr) {
					continue;
				}
				attr_name = target_attr->lDAPDisplayName;
			} else {
				target_attr = dsdb_attribute_by_linkID(ac->schema, schema_attr->linkID - 1);
				if (!target_attr) {
					continue;
				}
				attr_name = target_attr->lDAPDisplayName;
			}
			for (j = 0; j < el->num_values; j++) {
				ret = la_store_op(ac, LA_OP_DEL,
						  &el->values[j],
						  attr_name);

				/* for renames, ensure we add it back */
				if (ret == LDB_SUCCESS
				    && ac->req->operation == LDB_RENAME) {
					ret = la_store_op(ac, LA_OP_ADD,
							  &el->values[j],
							  attr_name);
				}
				if (ret != LDB_SUCCESS) {
					talloc_free(ares);
					return ldb_module_done(ac->req,
							       NULL, NULL, ret);
				}
			}
		}

		break;

	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);


		switch (ac->req->operation) {
		case LDB_DELETE:
			/* start the mod requests chain */
			ret = la_down_req(ac);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL, ret);
			}
			return ret;

		case LDB_RENAME:	
			/* start the mod requests chain */
			ret = la_queue_mod_request(ac);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL,
						       ret);
			}	
			return ret;
			
		default:
			talloc_free(ares);
			ldb_set_errstring(ldb,
					  "operations must be delete or rename");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

/* queue a linked attributes modify request in the la_private
   structure */
static int la_queue_mod_request(struct la_context *ac)
{
	struct la_private *la_private = 
		talloc_get_type(ldb_module_get_private(ac->module), struct la_private);

	if (la_private == NULL) {
		ldb_debug(ldb_module_get_ctx(ac->module), LDB_DEBUG_ERROR, __location__ ": No la_private transaction setup\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_steal(la_private, ac);
	DLIST_ADD(la_private->la_list, ac);

	return ldb_module_done(ac->req, ac->op_controls,
			       ac->op_response, LDB_SUCCESS);
}

/* Having done the original operation, then try to fix up all the linked attributes for modify and delete */
static int la_mod_del_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;
	struct la_context *ac;
	struct ldb_context *ldb;

	ac = talloc_get_type(req->context, struct la_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb,
				  "invalid ldb_reply_type in callback");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	
	ac->op_controls = talloc_steal(ac, ares->controls);
	ac->op_response = talloc_steal(ac, ares->response);

	/* If we have modfies to make, this is the time to do them for modify and delete */
	ret = la_queue_mod_request(ac);
	
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}
	talloc_free(ares);

	/* la_queue_mod_request has already sent the callbacks */
	return LDB_SUCCESS;

}

/* Having done the original rename try to fix up all the linked attributes */
static int la_rename_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;
	struct la_context *ac;
	struct ldb_request *search_req;
	const char **attrs;
	WERROR werr;
	struct ldb_context *ldb;

	ac = talloc_get_type(req->context, struct la_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb,
				  "invalid ldb_reply_type in callback");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	
	werr = dsdb_linked_attribute_lDAPDisplayName_list(ac->schema, ac, &attrs);
	if (!W_ERROR_IS_OK(werr)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	ret = ldb_build_search_req(&search_req, ldb, req,
				   ac->req->op.rename.newdn, LDB_SCOPE_BASE,
				   "(objectClass=*)", attrs,
				   NULL,
				   ac, la_op_search_callback,
				   req);
	
	if (ret != LDB_SUCCESS) {
		return ret;
	}
		
	talloc_steal(search_req, attrs);

	if (ret == LDB_SUCCESS) {
		ret = ldb_request_add_control(search_req,
					      LDB_CONTROL_EXTENDED_DN_OID,
					      false, NULL);
	}
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
				       ret);
	}
	
	ac->op_controls = talloc_steal(ac, ares->controls);
	ac->op_response = talloc_steal(ac, ares->response);

	return ldb_next_request(ac->module, search_req);
}

/* Having done the original add, then try to fix up all the linked attributes

  This is done after the add so the links can get the extended DNs correctly.
 */
static int la_add_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;
	struct la_context *ac;
	struct ldb_context *ldb;

	ac = talloc_get_type(req->context, struct la_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb,
				  "invalid ldb_reply_type in callback");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	
	if (ac->ops) {
		struct ldb_request *search_req;
		static const char *attrs[] = { NULL };
		
		/* The callback does all the hard work here - we need
		 * the objectGUID and SID of the added record */
		ret = ldb_build_search_req(&search_req, ldb, ac,
					   ac->req->op.add.message->dn,
					   LDB_SCOPE_BASE,
					   "(objectClass=*)", attrs,
					   NULL,
					   ac, la_mod_search_callback,
					   ac->req);
		
		if (ret == LDB_SUCCESS) {
			ret = ldb_request_add_control(search_req,
						      LDB_CONTROL_EXTENDED_DN_OID,
						      false, NULL);
		}
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL,
					       ret);
		}

		ac->op_controls = talloc_steal(ac, ares->controls);
		ac->op_response = talloc_steal(ac, ares->response);

		return ldb_next_request(ac->module, search_req);
		
	} else {
		return ldb_module_done(ac->req, ares->controls,
				       ares->response, ares->error);
	}
}

/* Reconstruct the original request, but pointing at our local callback to finish things off */
static int la_down_req(struct la_context *ac)
{
	struct ldb_request *down_req;
	int ret;
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(ac->module);

	switch (ac->req->operation) {
	case LDB_ADD:
		ret = ldb_build_add_req(&down_req, ldb, ac,
					ac->req->op.add.message,
					ac->req->controls,
					ac, la_add_callback,
					ac->req);
		break;
	case LDB_MODIFY:
		ret = ldb_build_mod_req(&down_req, ldb, ac,
					ac->req->op.mod.message,
					ac->req->controls,
					ac, la_mod_del_callback,
					ac->req);
		break;
	case LDB_DELETE:
		ret = ldb_build_del_req(&down_req, ldb, ac,
					ac->req->op.del.dn,
					ac->req->controls,
					ac, la_mod_del_callback,
					ac->req);
		break;
	case LDB_RENAME:
		ret = ldb_build_rename_req(&down_req, ldb, ac,
					   ac->req->op.rename.olddn,
					   ac->req->op.rename.newdn,
					   ac->req->controls,
					   ac, la_rename_callback,
					   ac->req);
		break;
	default:
		ret = LDB_ERR_OPERATIONS_ERROR;
	}
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ac->module, down_req);
}

/*
  use the GUID part of an extended DN to find the target DN, in case
  it has moved
 */
static int la_find_dn_target(struct ldb_module *module, struct la_context *ac, 
			     struct GUID *guid, struct ldb_dn **dn)
{
	return dsdb_find_dn_by_guid(ldb_module_get_ctx(ac->module), ac, GUID_string(ac, guid), dn);
}

/* apply one la_context op change */
static int la_do_op_request(struct ldb_module *module, struct la_context *ac, struct la_op_store *op)
{
	struct ldb_message_element *ret_el;
	struct ldb_request *mod_req;
	struct ldb_message *new_msg;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* Create the modify request */
	new_msg = ldb_msg_new(ac);
	if (!new_msg) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = la_find_dn_target(module, ac, &op->guid, &new_msg->dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (op->op == LA_OP_ADD) {
		ret = ldb_msg_add_empty(new_msg, op->name,
					LDB_FLAG_MOD_ADD, &ret_el);
	} else {
		ret = ldb_msg_add_empty(new_msg, op->name,
					LDB_FLAG_MOD_DELETE, &ret_el);
	}
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret_el->values = talloc_array(new_msg, struct ldb_val, 1);
	if (!ret_el->values) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret_el->num_values = 1;
	if (op->op == LA_OP_ADD) {
		ret_el->values[0] = data_blob_string_const(ldb_dn_get_extended_linearized(new_msg, ac->add_dn, 1));
	} else {
		ret_el->values[0] = data_blob_string_const(ldb_dn_get_extended_linearized(new_msg, ac->del_dn, 1));
	}

#if 0
	ldb_debug(ldb, LDB_DEBUG_WARNING,
		  "link on %s %s: %s %s\n", 
		  ldb_dn_get_linearized(new_msg->dn), ret_el->name, 
		  ret_el->values[0].data, ac->ops->op == LA_OP_ADD ? "added" : "deleted");
#endif	

	ret = ldb_build_mod_req(&mod_req, ldb, op,
				new_msg,
				NULL,
				NULL, 
				ldb_op_default_callback,
				NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	talloc_steal(mod_req, new_msg);

	if (DEBUGLVL(4)) {
		DEBUG(4,("Applying linked attribute change:\n%s\n",
			 ldb_ldif_message_string(ldb, op, LDB_CHANGETYPE_MODIFY, new_msg)));
	}

	/* Run the new request */
	ret = ldb_next_request(module, mod_req);

	/* we need to wait for this to finish, as we are being called
	   from the synchronous end_transaction hook of this module */
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(mod_req->handle, LDB_WAIT_ALL);
	}

	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "Failed to apply linked attribute change '%s' %s\n",
			  ldb_errstring(ldb),
			  ldb_ldif_message_string(ldb, op, LDB_CHANGETYPE_MODIFY, new_msg));
	}

	return ret;
}

/* apply one set of la_context changes */
static int la_do_mod_request(struct ldb_module *module, struct la_context *ac)
{
	struct la_op_store *op;

	for (op = ac->ops; op; op=op->next) {
		int ret = la_do_op_request(module, ac, op);
		if (ret != LDB_SUCCESS) {
			if (ret != LDB_ERR_NO_SUCH_OBJECT) {
				return ret;
			}
		}
	}

	return LDB_SUCCESS;
}


/*
  we hook into the transaction operations to allow us to 
  perform the linked attribute updates at the end of the whole
  transaction. This allows a forward linked attribute to be created
  before the target is created, as long as the target is created
  in the same transaction
 */
static int linked_attributes_start_transaction(struct ldb_module *module)
{
	/* create our private structure for this transaction */
	struct la_private *la_private = talloc_get_type(ldb_module_get_private(module),
							struct la_private);
	talloc_free(la_private);
	la_private = talloc(module, struct la_private);
	if (la_private == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	la_private->la_list = NULL;
	ldb_module_set_private(module, la_private);
	return ldb_next_start_trans(module);
}

/*
  on prepare commit we loop over our queued la_context structures
  and apply each of them
 */
static int linked_attributes_prepare_commit(struct ldb_module *module)
{
	struct la_private *la_private = 
		talloc_get_type(ldb_module_get_private(module), struct la_private);
	struct la_context *ac;

	/* walk the list backwards, to do the first entry first, as we
	 * added the entries with DLIST_ADD() which puts them at the
	 * start of the list */
	for (ac = la_private->la_list; ac && ac->next; ac=ac->next) ;

	for (; ac; ac=ac->prev) {
		int ret;
		ac->req = NULL;
		ret = la_do_mod_request(module, ac);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed mod request ret=%d\n", ret));
			talloc_free(la_private);
			ldb_module_set_private(module, NULL);	
			return ret;
		}
	}

	talloc_free(la_private);
	ldb_module_set_private(module, NULL);	

	return ldb_next_prepare_commit(module);
}

static int linked_attributes_del_transaction(struct ldb_module *module)
{
	struct la_private *la_private = 
		talloc_get_type(ldb_module_get_private(module), struct la_private);
	talloc_free(la_private);
	ldb_module_set_private(module, NULL);
	return ldb_next_del_trans(module);
}


_PUBLIC_ const struct ldb_module_ops ldb_linked_attributes_module_ops = {
	.name		   = "linked_attributes",
	.add               = linked_attributes_add,
	.modify            = linked_attributes_modify,
	.del               = linked_attributes_del,
	.rename            = linked_attributes_rename,
	.start_transaction = linked_attributes_start_transaction,
	.prepare_commit    = linked_attributes_prepare_commit,
	.del_transaction   = linked_attributes_del_transaction,
};
