/* 
   ldb database library

   Copyright (C) Simo Sorce  2006-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2007

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
 *  Component: objectClass sorting module
 *
 *  Description: 
 *  - sort the objectClass attribute into the class
 *    hierarchy, 
 *  - fix DNs and attributes into 'standard' case
 *  - Add objectCategory and ntSecurityDescriptor defaults
 *
 *  Author: Andrew Bartlett
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

struct oc_context {

	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_reply *search_res;

	int (*step_fn)(struct oc_context *);
};

struct class_list {
	struct class_list *prev, *next;
	const struct dsdb_class *objectclass;
};

static struct oc_context *oc_init_context(struct ldb_module *module,
					  struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct oc_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct oc_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int objectclass_do_add(struct oc_context *ac);

/* Sort objectClasses into correct order, and validate that all
 * objectClasses specified actually exist in the schema
 */

static int objectclass_sort(struct ldb_module *module,
			    const struct dsdb_schema *schema,
			    TALLOC_CTX *mem_ctx,
			    struct ldb_message_element *objectclass_element,
			    struct class_list **sorted_out) 
{
	struct ldb_context *ldb;
	int i;
	int layer;
	struct class_list *sorted = NULL, *parent_class = NULL,
		*subclass = NULL, *unsorted = NULL, *current, *poss_subclass, *poss_parent, *new_parent;

	ldb = ldb_module_get_ctx(module);

	/* DESIGN:
	 *
	 * We work on 4 different 'bins' (implemented here as linked lists):
	 *
	 * * sorted:       the eventual list, in the order we wish to push
	 *                 into the database.  This is the only ordered list.
	 *
	 * * parent_class: The current parent class 'bin' we are
	 *                 trying to find subclasses for
	 *
	 * * subclass:     The subclasses we have found so far
	 *
	 * * unsorted:     The remaining objectClasses
	 *
	 * The process is a matter of filtering objectClasses up from
	 * unsorted into sorted.  Order is irrelevent in the later 3 'bins'.
	 * 
	 * We start with 'top' (found and promoted to parent_class
	 * initially).  Then we find (in unsorted) all the direct
	 * subclasses of 'top'.  parent_classes is concatenated onto
	 * the end of 'sorted', and subclass becomes the list in
	 * parent_class.
	 *
	 * We then repeat, until we find no more subclasses.  Any left
	 * over classes are added to the end.
	 *
	 */

	/* Firstly, dump all the objectClass elements into the
	 * unsorted bin, except for 'top', which is special */
	for (i=0; i < objectclass_element->num_values; i++) {
		current = talloc(mem_ctx, struct class_list);
		if (!current) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		current->objectclass = dsdb_class_by_lDAPDisplayName_ldb_val(schema, &objectclass_element->values[i]);
		if (!current->objectclass) {
			ldb_asprintf_errstring(ldb, "objectclass %.*s is not a valid objectClass in schema", 
					       (int)objectclass_element->values[i].length, (const char *)objectclass_element->values[i].data);
			return LDB_ERR_OBJECT_CLASS_VIOLATION;
		}

		/* this is the root of the tree.  We will start
		 * looking for subclasses from here */
		if (ldb_attr_cmp("top", current->objectclass->lDAPDisplayName) == 0) {
			DLIST_ADD_END(parent_class, current, struct class_list *);
		} else {
			DLIST_ADD_END(unsorted, current, struct class_list *);
		}
	}

	if (parent_class == NULL) {
		current = talloc(mem_ctx, struct class_list);
		current->objectclass = dsdb_class_by_lDAPDisplayName(schema, "top");
		DLIST_ADD_END(parent_class, current, struct class_list *);
	}

	/* For each object:  find parent chain */
	for (current = unsorted; schema && current; current = current->next) {
		for (poss_parent = unsorted; poss_parent; poss_parent = poss_parent->next) {
			if (ldb_attr_cmp(poss_parent->objectclass->lDAPDisplayName, current->objectclass->subClassOf) == 0) {
				break;
			}
		}
		/* If we didn't get to the end of the list, we need to add this parent */
		if (poss_parent || (ldb_attr_cmp("top", current->objectclass->subClassOf) == 0)) {
			continue;
		}

		new_parent = talloc(mem_ctx, struct class_list);
		new_parent->objectclass = dsdb_class_by_lDAPDisplayName(schema, current->objectclass->subClassOf);
		DLIST_ADD_END(unsorted, new_parent, struct class_list *);
	}

	/* DEBUGGING aid:  how many layers are we down now? */
	layer = 0;
	do {
		layer++;
		/* Find all the subclasses of classes in the
		 * parent_classes.  Push them onto the subclass list */

		/* Ensure we don't bother if there are no unsorted entries left */
		for (current = parent_class; schema && unsorted && current; current = current->next) {
			/* Walk the list of possible subclasses in unsorted */
			for (poss_subclass = unsorted; poss_subclass; ) {
				struct class_list *next;
				
				/* Save the next pointer, as the DLIST_ macros will change poss_subclass->next */
				next = poss_subclass->next;

				if (ldb_attr_cmp(poss_subclass->objectclass->subClassOf, current->objectclass->lDAPDisplayName) == 0) {
					DLIST_REMOVE(unsorted, poss_subclass);
					DLIST_ADD(subclass, poss_subclass);
					
					break;
				}
				poss_subclass = next;
			}
		}

		/* Now push the parent_classes as sorted, we are done with
		these.  Add to the END of the list by concatenation */
		DLIST_CONCATENATE(sorted, parent_class, struct class_list *);

		/* and now find subclasses of these */
		parent_class = subclass;
		subclass = NULL;

		/* If we didn't find any subclasses we will fall out
		 * the bottom here */
	} while (parent_class);

	if (!unsorted) {
		*sorted_out = sorted;
		return LDB_SUCCESS;
	}

	if (!schema) {
		/* If we don't have schema yet, then just merge the lists again */
		DLIST_CONCATENATE(sorted, unsorted, struct class_list *);
		*sorted_out = sorted;
		return LDB_SUCCESS;
	}

	/* This shouldn't happen, and would break MMC, perhaps there
	 * was no 'top', a conflict in the objectClasses or some other
	 * schema error?
	 */
	ldb_asprintf_errstring(ldb, "objectclass %s is not a valid objectClass in objectClass chain", unsorted->objectclass->lDAPDisplayName);
	return LDB_ERR_OBJECT_CLASS_VIOLATION;
}

static int get_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct oc_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct oc_context);
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

static int oc_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct oc_context *ac;

	ac = talloc_get_type(req->context, struct oc_context);

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

/* Fix up the DN to be in the standard form, taking particular care to match the parent DN

   This should mean that if the parent is:
    CN=Users,DC=samba,DC=example,DC=com
   and a proposed child is
    cn=Admins ,cn=USERS,dc=Samba,dc=example,dc=COM

   The resulting DN should be:

    CN=Admins,CN=Users,DC=samba,DC=example,DC=com
   
 */
static int fix_dn(TALLOC_CTX *mem_ctx, 
		  struct ldb_dn *newdn, struct ldb_dn *parent_dn, 
		  struct ldb_dn **fixed_dn) 
{
	char *upper_rdn_attr;
	/* Fix up the DN to be in the standard form, taking particular care to match the parent DN */
	*fixed_dn = ldb_dn_copy(mem_ctx, parent_dn);

	/* We need the attribute name in upper case */
	upper_rdn_attr = strupper_talloc(*fixed_dn, 
					 ldb_dn_get_rdn_name(newdn));
	if (!upper_rdn_attr) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
					       
	/* Create a new child */
	if (ldb_dn_add_child_fmt(*fixed_dn, "X=X") == false) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* And replace it with CN=foo (we need the attribute in upper case */
	return ldb_dn_set_component(*fixed_dn, 0, upper_rdn_attr,
				    *ldb_dn_get_rdn_val(newdn));
}

/* Fix all attribute names to be in the correct case, and check they are all valid per the schema */
static int fix_attributes(struct ldb_context *ldb, const struct dsdb_schema *schema, struct ldb_message *msg) 
{
	int i;
	for (i=0; i < msg->num_elements; i++) {
		const struct dsdb_attribute *attribute = dsdb_attribute_by_lDAPDisplayName(schema, msg->elements[i].name);
		/* Add in a very special case for 'clearTextPassword',
		 * which is used for internal processing only, and is
		 * not presented in the schema */
		if (!attribute) {
			if (strcasecmp(msg->elements[i].name, "clearTextPassword") != 0) {
				ldb_asprintf_errstring(ldb, "attribute %s is not a valid attribute in schema", msg->elements[i].name);
				return LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE;
			}
		} else {
			msg->elements[i].name = attribute->lDAPDisplayName;
		}
	}

	return LDB_SUCCESS;
}

static int objectclass_do_add(struct oc_context *ac);

static int objectclass_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_request *search_req;
	struct oc_context *ac;
	struct ldb_dn *parent_dn;
	int ret;
	static const char * const parent_attrs[] = { "objectGUID", NULL };

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectclass_add\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	/* the objectClass must be specified on add */
	if (ldb_msg_find_element(req->op.add.message, 
				 "objectClass") == NULL) {
		return LDB_ERR_OBJECT_CLASS_VIOLATION;
	}

	ac = oc_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* If there isn't a parent, just go on to the add processing */
	if (ldb_dn_get_comp_num(ac->req->op.add.message->dn) == 1) {
		return objectclass_do_add(ac);
	}

	/* get copy of parent DN */
	parent_dn = ldb_dn_get_parent(ac, ac->req->op.add.message->dn);
	if (parent_dn == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&search_req, ldb,
				   ac, parent_dn, LDB_SCOPE_BASE,
				   "(objectClass=*)", parent_attrs,
				   NULL,
				   ac, get_search_callback,
				   req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	talloc_steal(search_req, parent_dn);

	ac->step_fn = objectclass_do_add;

	return ldb_next_request(ac->module, search_req);
}

static int objectclass_do_add(struct oc_context *ac)
{
	struct ldb_context *ldb;
	const struct dsdb_schema *schema;
	struct ldb_request *add_req;
	char *value;
	struct ldb_message_element *objectclass_element;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	struct class_list *sorted, *current;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);
	schema = dsdb_get_schema(ldb);

	mem_ctx = talloc_new(ac);
	if (mem_ctx == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);

	/* Check we have a valid parent */
	if (ac->search_res == NULL) {
		if (ldb_dn_compare(ldb_get_root_basedn(ldb),
								msg->dn) == 0) {
			/* Allow the tree to be started */
			
			/* but don't keep any error string, it's meaningless */
			ldb_set_errstring(ldb, NULL);
		} else {
			ldb_asprintf_errstring(ldb, "objectclass: Cannot add %s, parent does not exist!", 
					       ldb_dn_get_linearized(msg->dn));
			talloc_free(mem_ctx);
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
	} else {
		const struct ldb_val *parent_guid;

		/* Fix up the DN to be in the standard form, taking particular care to match the parent DN */
		ret = fix_dn(msg, 
			     ac->req->op.add.message->dn,
			     ac->search_res->message->dn,
			     &msg->dn);

		if (ret != LDB_SUCCESS) {
			ldb_asprintf_errstring(ldb, "Could not munge DN %s into normal form", 
					       ldb_dn_get_linearized(ac->req->op.add.message->dn));
			talloc_free(mem_ctx);
			return ret;
		}

		parent_guid = ldb_msg_find_ldb_val(ac->search_res->message, "objectGUID");
		if (parent_guid == NULL) {
			ldb_asprintf_errstring(ldb, "objectclass: Cannot add %s, parent does not have an objectGUID!", 
					       ldb_dn_get_linearized(msg->dn));
			talloc_free(mem_ctx);
			return LDB_ERR_UNWILLING_TO_PERFORM;			
		}

		/* TODO: Check this is a valid child to this parent,
		 * by reading the allowedChildClasses and
		 * allowedChildClasssesEffective attributes */
		ret = ldb_msg_add_steal_value(msg, "parentGUID", discard_const(parent_guid));
		if (ret != LDB_SUCCESS) {
			ldb_asprintf_errstring(ldb, "objectclass: Cannot add %s, failed to add parentGUID", 
					       ldb_dn_get_linearized(msg->dn));
			talloc_free(mem_ctx);
			return LDB_ERR_UNWILLING_TO_PERFORM;						
		}
	}
	if (schema) {
		ret = fix_attributes(ldb, schema, msg);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}

		/* This is now the objectClass list from the database */
		objectclass_element = ldb_msg_find_element(msg, "objectClass");

		if (!objectclass_element) {
			/* Where did it go?  bail now... */
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ret = objectclass_sort(ac->module, schema, mem_ctx, objectclass_element, &sorted);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
		
		ldb_msg_remove_attr(msg, "objectClass");
		ret = ldb_msg_add_empty(msg, "objectClass", 0, NULL);
		
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}

		/* We must completely replace the existing objectClass entry,
		 * because we need it sorted */

		/* Move from the linked list back into an ldb msg */
		for (current = sorted; current; current = current->next) {
			value = talloc_strdup(msg, current->objectclass->lDAPDisplayName);
			if (value == NULL) {
				ldb_oom(ldb);
				talloc_free(mem_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = ldb_msg_add_string(msg, "objectClass", value);
			if (ret != LDB_SUCCESS) {
				ldb_set_errstring(ldb,
						  "objectclass: could not re-add sorted "
						  "objectclass to modify msg");
				talloc_free(mem_ctx);
				return ret;
			}
			/* Last one is the critical one */
			if (!current->next) {
				struct ldb_message_element *el;
				int32_t systemFlags = 0;
				DATA_BLOB *sd;
				if (!ldb_msg_find_element(msg, "objectCategory")) {
					value = talloc_strdup(msg, current->objectclass->defaultObjectCategory);
					if (value == NULL) {
						ldb_oom(ldb);
						talloc_free(mem_ctx);
						return LDB_ERR_OPERATIONS_ERROR;
					}
					ldb_msg_add_string(msg, "objectCategory", value);
				}
				if (!ldb_msg_find_element(msg, "showInAdvancedViewOnly") && (current->objectclass->defaultHidingValue == true)) {
					ldb_msg_add_string(msg, "showInAdvancedViewOnly",
							   "TRUE");
				}

				/* There are very special rules for systemFlags, see MS-ADTS 3.1.1.5.2.4 */
				el = ldb_msg_find_element(msg, "systemFlags");

				systemFlags = ldb_msg_find_attr_as_int(msg, "systemFlags", 0);

				if (el) {
					/* Only these flags may be set by a client, but we can't tell between a client and our provision at this point */
					/* systemFlags &= ( SYSTEM_FLAG_CONFIG_ALLOW_RENAME | SYSTEM_FLAG_CONFIG_ALLOW_MOVE | SYSTEM_FLAG_CONFIG_LIMITED_MOVE); */
					ldb_msg_remove_element(msg, el);
				}

				/* This flag is only allowed on attributeSchema objects */
				if (ldb_attr_cmp(current->objectclass->lDAPDisplayName, "attributeSchema") == 0) {
					systemFlags &= ~SYSTEM_FLAG_ATTR_IS_RDN;
				}

				if (ldb_attr_cmp(current->objectclass->lDAPDisplayName, "server") == 0) {
					systemFlags |= (int32_t)(SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE | SYSTEM_FLAG_CONFIG_ALLOW_RENAME | SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE);
				} else if (ldb_attr_cmp(current->objectclass->lDAPDisplayName, "site") == 0
					   || ldb_attr_cmp(current->objectclass->lDAPDisplayName, "serverContainer") == 0
					   || ldb_attr_cmp(current->objectclass->lDAPDisplayName, "ntDSDSA") == 0) {
					systemFlags |= (int32_t)(SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE);

				} else if (ldb_attr_cmp(current->objectclass->lDAPDisplayName, "siteLink") == 0
					   || ldb_attr_cmp(current->objectclass->lDAPDisplayName, "siteLinkBridge") == 0
					   || ldb_attr_cmp(current->objectclass->lDAPDisplayName, "nTDSConnection") == 0) {
					systemFlags |= (int32_t)(SYSTEM_FLAG_CONFIG_ALLOW_RENAME);
				}

				/* TODO: If parent object is site or subnet, also add (SYSTEM_FLAG_CONFIG_ALLOW_RENAME) */

				if (el || systemFlags != 0) {
					samdb_msg_add_int(ldb, msg, msg, "systemFlags", systemFlags);
				}
			}
		}
	}

	talloc_free(mem_ctx);
	ret = ldb_msg_sanity_check(ldb, msg);


	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&add_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, oc_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* perform the add */
	return ldb_next_request(ac->module, add_req);
}

static int oc_modify_callback(struct ldb_request *req,
				struct ldb_reply *ares);
static int objectclass_do_mod(struct oc_context *ac);

static int objectclass_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_message_element *objectclass_element;
	struct ldb_message *msg;
	const struct dsdb_schema *schema = dsdb_get_schema(ldb);
	struct class_list *sorted, *current;
	struct ldb_request *down_req;
	struct oc_context *ac;
	TALLOC_CTX *mem_ctx;
	char *value;
	int ret;

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectclass_modify\n");

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}
	
	/* Without schema, there isn't much to do here */
	if (!schema) {
		return ldb_next_request(module, req);
	}
	objectclass_element = ldb_msg_find_element(req->op.mod.message, "objectClass");

	ac = oc_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* If no part of this touches the objectClass, then we don't
	 * need to make any changes.  */

	/* If the only operation is the deletion of the objectClass
	 * then go on with just fixing the attribute case */
	if (!objectclass_element) {
		msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
		if (msg == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		ret = fix_attributes(ldb, schema, msg);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ret = ldb_build_mod_req(&down_req, ldb, ac,
					msg,
					req->controls,
					ac, oc_op_callback,
					req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		/* go on with the call chain */
		return ldb_next_request(module, down_req);
	}

	switch (objectclass_element->flags & LDB_FLAG_MOD_MASK) {
	case LDB_FLAG_MOD_DELETE:
		if (objectclass_element->num_values == 0) {
			return LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED;
		}
		break;

	case LDB_FLAG_MOD_REPLACE:
		mem_ctx = talloc_new(ac);
		if (mem_ctx == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
		if (msg == NULL) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = fix_attributes(ldb, schema, msg);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}

		ret = objectclass_sort(module, schema, mem_ctx, objectclass_element, &sorted);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}

		/* We must completely replace the existing objectClass entry,
		 * because we need it sorted */
		
		ldb_msg_remove_attr(msg, "objectClass");
		ret = ldb_msg_add_empty(msg, "objectClass", LDB_FLAG_MOD_REPLACE, NULL);
		
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}

		/* Move from the linked list back into an ldb msg */
		for (current = sorted; current; current = current->next) {
			/* copy the value as this string is on the schema
			 * context and we can't rely on it not changing
			 * before the operation is over */
			value = talloc_strdup(msg,
					current->objectclass->lDAPDisplayName);
			if (value == NULL) {
				ldb_oom(ldb);
				talloc_free(mem_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = ldb_msg_add_string(msg, "objectClass", value);
			if (ret != LDB_SUCCESS) {
				ldb_set_errstring(ldb,
					"objectclass: could not re-add sorted "
					"objectclass to modify msg");
				talloc_free(mem_ctx);
				return ret;
			}
		}
		
		talloc_free(mem_ctx);

		ret = ldb_msg_sanity_check(ldb, msg);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ret = ldb_build_mod_req(&down_req, ldb, ac,
					msg,
					req->controls,
					ac, oc_op_callback,
					req);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		/* go on with the call chain */
		return ldb_next_request(module, down_req);
	}

	/* This isn't the default branch of the switch, but a 'in any
	 * other case'.  When a delete isn't for all objectClasses for
	 * example
	 */

	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = fix_attributes(ldb, schema, msg);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return ret;
	}

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, oc_modify_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

static int oc_modify_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	static const char * const attrs[] = { "objectClass", NULL };
	struct ldb_request *search_req;
	struct oc_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct oc_context);
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
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	ret = ldb_build_search_req(&search_req, ldb, ac,
				   ac->req->op.mod.message->dn, LDB_SCOPE_BASE,
				   "(objectClass=*)",
				   attrs, NULL, 
				   ac, get_search_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}

	ac->step_fn = objectclass_do_mod;

	ret = ldb_next_request(ac->module, search_req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL, ret);
	}
	return LDB_SUCCESS;
}

static int objectclass_do_mod(struct oc_context *ac)
{
	struct ldb_context *ldb;
	const struct dsdb_schema *schema;
	struct ldb_request *mod_req;
	char *value;
	struct ldb_message_element *objectclass_element;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	struct class_list *sorted, *current;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	if (ac->search_res == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	schema = dsdb_get_schema(ldb);

	mem_ctx = talloc_new(ac);
	if (mem_ctx == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* use a new message structure */
	msg = ldb_msg_new(ac);
	if (msg == NULL) {
		ldb_set_errstring(ldb,
			"objectclass: could not create new modify msg");
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* This is now the objectClass list from the database */
	objectclass_element = ldb_msg_find_element(ac->search_res->message, 
						   "objectClass");
	if (!objectclass_element) {
		/* Where did it go?  bail now... */
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	/* modify dn */
	msg->dn = ac->req->op.mod.message->dn;

	ret = objectclass_sort(ac->module, schema, mem_ctx, objectclass_element, &sorted);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* We must completely replace the existing objectClass entry.
	 * We could do a constrained add/del, but we are meant to be
	 * in a transaction... */

	ret = ldb_msg_add_empty(msg, "objectClass", LDB_FLAG_MOD_REPLACE, NULL);
	if (ret != LDB_SUCCESS) {
		ldb_set_errstring(ldb, "objectclass: could not clear objectclass in modify msg");
		talloc_free(mem_ctx);
		return ret;
	}
	
	/* Move from the linked list back into an ldb msg */
	for (current = sorted; current; current = current->next) {
		value = talloc_strdup(msg, current->objectclass->lDAPDisplayName);
		if (value == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ret = ldb_msg_add_string(msg, "objectClass", value);
		if (ret != LDB_SUCCESS) {
			ldb_set_errstring(ldb, "objectclass: could not re-add sorted objectclass to modify msg");
			talloc_free(mem_ctx);
			return ret;
		}
	}

	ret = ldb_msg_sanity_check(ldb, msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}

	ret = ldb_build_mod_req(&mod_req, ldb, ac,
				msg,
				ac->req->controls,
				ac, oc_op_callback,
				ac->req);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}

	talloc_free(mem_ctx);
	/* perform the modify */
	return ldb_next_request(ac->module, mod_req);
}

static int objectclass_do_rename(struct oc_context *ac);

static int objectclass_rename(struct ldb_module *module, struct ldb_request *req)
{
	static const char * const attrs[] = { "objectGUID", NULL };
	struct ldb_context *ldb;
	struct ldb_request *search_req;
	struct oc_context *ac;
	struct ldb_dn *parent_dn;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "objectclass_rename\n");

	if (ldb_dn_is_special(req->op.rename.newdn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	/* Firstly ensure we are not trying to rename it to be a child of itself */
	if ((ldb_dn_compare_base(req->op.rename.olddn, req->op.rename.newdn) == 0) 
	    && (ldb_dn_compare(req->op.rename.olddn, req->op.rename.newdn) != 0)) {
		ldb_asprintf_errstring(ldb, "Cannot rename %s to be a child of itself",
				       ldb_dn_get_linearized(req->op.rename.olddn));
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	ac = oc_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	parent_dn = ldb_dn_get_parent(ac, req->op.rename.newdn);
	if (parent_dn == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* note that the results of this search are kept and used to
	   update the parentGUID in objectclass_rename_callback() */
	ret = ldb_build_search_req(&search_req, ldb,
				   ac, parent_dn, LDB_SCOPE_BASE,
				   "(objectClass=*)",
				   attrs, NULL, 
				   ac, get_search_callback,
				   req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* we have to add the show deleted control, as otherwise DRS
	   deletes will be refused as we will think the target parent
	   does not exist */
	ret = ldb_request_add_control(search_req, LDB_CONTROL_SHOW_DELETED_OID, false, NULL);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ac->step_fn = objectclass_do_rename;

	return ldb_next_request(ac->module, search_req);
}

/* 
   called after the rename happens. 
   We now need to fix the parentGUID of the object to be the objectGUID of
   the new parent 
*/
static int objectclass_rename_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct oc_context *ac;
	const struct ldb_val *parent_guid;
	struct ldb_request *mod_req = NULL;
	int ret;
	struct ldb_message *msg;
	struct ldb_message_element *el = NULL;

	ac = talloc_get_type(req->context, struct oc_context);
	ldb = ldb_module_get_ctx(ac->module);

	/* make sure the rename succeeded */
	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}


	/* the ac->search_res should contain the new parents objectGUID */
	parent_guid = ldb_msg_find_ldb_val(ac->search_res->message, "objectGUID");
	if (parent_guid == NULL) {
		ldb_asprintf_errstring(ldb, "objectclass: Cannot rename %s, new parent does not have an objectGUID!", 
				       ldb_dn_get_linearized(ac->req->op.rename.newdn));
		return LDB_ERR_UNWILLING_TO_PERFORM;

	}

	/* construct the modify message */
	msg = ldb_msg_new(ac);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = ac->req->op.rename.newdn;

	ret = ldb_msg_add_value(msg, "parentGUID", parent_guid, &el);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	el->flags = LDB_FLAG_MOD_REPLACE;

	ret = ldb_build_mod_req(&mod_req, ldb, ac, msg,
				NULL, ac, oc_op_callback, req);

	return ldb_next_request(ac->module, mod_req);
}

static int objectclass_do_rename(struct oc_context *ac)
{
	struct ldb_context *ldb;
	struct ldb_request *rename_req;
	struct ldb_dn *fixed_dn;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* Check we have a valid parent */
	if (ac->search_res == NULL) {
		ldb_asprintf_errstring(ldb, "objectclass: Cannot rename %s, parent does not exist!", 
				       ldb_dn_get_linearized(ac->req->op.rename.newdn));
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	
	/* Fix up the DN to be in the standard form,
	 * taking particular care to match the parent DN */
	ret = fix_dn(ac,
		     ac->req->op.rename.newdn,
		     ac->search_res->message->dn,
		     &fixed_dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* TODO: Check this is a valid child to this parent,
	 * by reading the allowedChildClasses and
	 * allowedChildClasssesEffective attributes */

	ret = ldb_build_rename_req(&rename_req, ldb, ac,
				   ac->req->op.rename.olddn, fixed_dn,
				   ac->req->controls,
				   ac, objectclass_rename_callback,
				   ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* perform the rename */
	return ldb_next_request(ac->module, rename_req);
}

_PUBLIC_ const struct ldb_module_ops ldb_objectclass_module_ops = {
	.name		   = "objectclass",
	.add           = objectclass_add,
	.modify        = objectclass_modify,
	.rename        = objectclass_rename,
};
