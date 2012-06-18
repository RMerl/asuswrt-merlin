/* 
   ldb database library

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Simo Sorce 2006

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
/*
  handle operational attributes
 */

/*
  createTimestamp: HIDDEN, searchable, ldaptime, alias for whenCreated
  modifyTimestamp: HIDDEN, searchable, ldaptime, alias for whenChanged

     for the above two, we do the search as normal, and if
     createTimestamp or modifyTimestamp is asked for, then do
     additional searches for whenCreated and whenChanged and fill in
     the resulting values

     we also need to replace these with the whenCreated/whenChanged
     equivalent in the search expression trees

  whenCreated: not-HIDDEN, CONSTRUCTED, SEARCHABLE
  whenChanged: not-HIDDEN, CONSTRUCTED, SEARCHABLE

     on init we need to setup attribute handlers for these so
     comparisons are done correctly. The resolution is 1 second.

     on add we need to add both the above, for current time

     on modify we need to change whenChanged


  subschemaSubentry: HIDDEN, not-searchable, 
                     points at DN CN=Aggregate,CN=Schema,CN=Configuration,$BASEDN

     for this one we do the search as normal, then add the static
     value if requested. How do we work out the $BASEDN from inside a
     module?
     

  structuralObjectClass: HIDDEN, CONSTRUCTED, not-searchable. always same as objectclass?

     for this one we do the search as normal, then if requested ask
     for objectclass, change the attribute name, and add it

  allowedAttributesEffective: HIDDEN, CONSTRUCTED, not-searchable, 
     list of attributes that can be modified - requires schema lookup


  attributeTypes: in schema only
  objectClasses: in schema only
  matchingRules: in schema only
  matchingRuleUse: in schema only
  creatorsName: not supported by w2k3?
  modifiersName: not supported by w2k3?
*/

#include "includes.h"
#include "ldb/include/includes.h"

/*
  construct a canonical name from a message
*/
static int construct_canonical_name(struct ldb_module *module, struct ldb_message *msg)
{
	char *canonicalName;
	canonicalName = ldb_dn_canonical_string(msg, msg->dn);
	if (canonicalName == NULL) {
		return -1;
	}
	return ldb_msg_add_steal_string(msg, "canonicalName", canonicalName);
}

/*
  a list of attribute names that should be substituted in the parse
  tree before the search is done
*/
static const struct {
	const char *attr;
	const char *replace;
} parse_tree_sub[] = {
	{ "createTimestamp", "whenCreated" },
	{ "modifyTimestamp", "whenChanged" }
};


/*
  a list of attribute names that are hidden, but can be searched for
  using another (non-hidden) name to produce the correct result
*/
static const struct {
	const char *attr;
	const char *replace;
	int (*constructor)(struct ldb_module *, struct ldb_message *);
} search_sub[] = {
	{ "createTimestamp", "whenCreated", NULL },
	{ "modifyTimestamp", "whenChanged", NULL },
	{ "structuralObjectClass", "objectClass", NULL },
	{ "canonicalName", "distinguishedName", construct_canonical_name }
};

/*
  post process a search result record. For any search_sub[] attributes that were
  asked for, we need to call the appropriate copy routine to copy the result
  into the message, then remove any attributes that we added to the search but were
  not asked for by the user
*/
static int operational_search_post_process(struct ldb_module *module,
					   struct ldb_message *msg, 
					   const char * const *attrs)
{
	int i, a=0;

	for (a=0;attrs && attrs[a];a++) {
		for (i=0;i<ARRAY_SIZE(search_sub);i++) {
			if (ldb_attr_cmp(attrs[a], search_sub[i].attr) != 0) {
				continue;
			}

			/* construct the new attribute, using either a supplied 
			   constructor or a simple copy */
			if (search_sub[i].constructor) {
				if (search_sub[i].constructor(module, msg) != 0) {
					goto failed;
				}
			} else if (ldb_msg_copy_attr(msg,
						     search_sub[i].replace,
						     search_sub[i].attr) != 0) {
				goto failed;
			}

			/* remove the added search attribute, unless it was asked for 
			   by the user */
			if (search_sub[i].replace == NULL ||
			    ldb_attr_in_list(attrs, search_sub[i].replace) ||
			    ldb_attr_in_list(attrs, "*")) {
				continue;
			}

			ldb_msg_remove_attr(msg, search_sub[i].replace);
		}
	}

	return 0;

failed:
	ldb_debug_set(module->ldb, LDB_DEBUG_WARNING, 
		      "operational_search_post_process failed for attribute '%s'\n", 
		      attrs[a]);
	return -1;
}


/*
  hook search operations
*/

struct operational_context {

	struct ldb_module *module;
	void *up_context;
	int (*up_callback)(struct ldb_context *, void *, struct ldb_reply *);

	const char * const *attrs;
};

static int operational_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct operational_context *ac;

	if (!context || !ares) {
		ldb_set_errstring(ldb, "NULL Context or Result in callback");
		goto error;
	}

	ac = talloc_get_type(context, struct operational_context);

	if (ares->type == LDB_REPLY_ENTRY) {
		/* for each record returned post-process to add any derived
		   attributes that have been asked for */
		if (operational_search_post_process(ac->module, ares->message, ac->attrs) != 0) {
			goto error;
		}
	}

	return ac->up_callback(ldb, ac->up_context, ares);

error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}

static int operational_search(struct ldb_module *module, struct ldb_request *req)
{
	struct operational_context *ac;
	struct ldb_request *down_req;
	const char **search_attrs = NULL;
	int i, a, ret;

	req->handle = NULL;

	ac = talloc(req, struct operational_context);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->up_context = req->context;
	ac->up_callback = req->callback;
	ac->attrs = req->op.search.attrs;

	down_req = talloc_zero(req, struct ldb_request);
	if (down_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	down_req->operation = req->operation;
	down_req->op.search.base = req->op.search.base;
	down_req->op.search.scope = req->op.search.scope;
	down_req->op.search.tree = req->op.search.tree;

	/*  FIXME: I hink we should copy the tree and keep the original
	 *  unmodified. SSS */
	/* replace any attributes in the parse tree that are
	   searchable, but are stored using a different name in the
	   backend */
	for (i=0;i<ARRAY_SIZE(parse_tree_sub);i++) {
		ldb_parse_tree_attr_replace(discard_const_p(struct ldb_parse_tree, req->op.search.tree), 
					    parse_tree_sub[i].attr, 
					    parse_tree_sub[i].replace);
	}

	/* in the list of attributes we are looking for, rename any
	   attributes to the alias for any hidden attributes that can
	   be fetched directly using non-hidden names */
	for (a=0;ac->attrs && ac->attrs[a];a++) {
		for (i=0;i<ARRAY_SIZE(search_sub);i++) {
			if (ldb_attr_cmp(ac->attrs[a], search_sub[i].attr) == 0 &&
			    search_sub[i].replace) {
				if (!search_attrs) {
					search_attrs = ldb_attr_list_copy(req, ac->attrs);
					if (search_attrs == NULL) {
						return LDB_ERR_OPERATIONS_ERROR;
					}
				}
				search_attrs[a] = search_sub[i].replace;
			}
		}
	}
	
	/* use new set of attrs if any */
	if (search_attrs) down_req->op.search.attrs = search_attrs;
	else down_req->op.search.attrs = req->op.search.attrs;
	
	down_req->controls = req->controls;

	down_req->context = ac;
	down_req->callback = operational_callback;
	ldb_set_timeout_from_prev_req(module->ldb, req, down_req);

	/* perform the search */
	ret = ldb_next_request(module, down_req);

	/* do not free down_req as the call results may be linked to it,
	 * it will be freed when the upper level request get freed */
	if (ret == LDB_SUCCESS) {
		req->handle = down_req->handle;
	}

	return ret;
}

static int operational_init(struct ldb_module *ctx)
{
	/* setup some standard attribute handlers */
	ldb_set_attrib_handler_syntax(ctx->ldb, "whenCreated", LDB_SYNTAX_UTC_TIME);
	ldb_set_attrib_handler_syntax(ctx->ldb, "whenChanged", LDB_SYNTAX_UTC_TIME);
	ldb_set_attrib_handler_syntax(ctx->ldb, "subschemaSubentry", LDB_SYNTAX_DN);
	ldb_set_attrib_handler_syntax(ctx->ldb, "structuralObjectClass", LDB_SYNTAX_OBJECTCLASS);

	return ldb_next_init(ctx);
}

static const struct ldb_module_ops operational_ops = {
	.name              = "operational",
	.search            = operational_search,
	.init_context	   = operational_init
};

int ldb_operational_init(void)
{
	return ldb_register_module(&operational_ops);
}
