/* 
   ldb database library

   Copyright (C) Simo Sorce 2005-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007-2008

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
 *  Component: ldb extended dn control module
 *
 *  Description: this module builds a special dn for returned search
 *  results nad creates the special DN in the backend store for new
 *  values.
 *
 *  This also has the curious result that we convert <SID=S-1-2-345>
 *  in an attribute value into a normal DN for the rest of the stack
 *  to process
 *
 *  Authors: Simo Sorce
 *           Andrew Bartlett
 */

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_errors.h"
#include "ldb/include/ldb_module.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"

#include <time.h>

struct extended_dn_replace_list {
	struct extended_dn_replace_list *next;
	struct ldb_dn *dn;
	TALLOC_CTX *mem_ctx;
	struct ldb_val *replace_dn;
	struct extended_dn_context *ac;
	struct ldb_request *search_req;
};


struct extended_dn_context {
	const struct dsdb_schema *schema;
	struct ldb_module *module;
	struct ldb_request *req;
	struct ldb_request *new_req;

	struct extended_dn_replace_list *ops;
	struct extended_dn_replace_list *cur;
};


static struct extended_dn_context *extended_dn_context_init(struct ldb_module *module,
							    struct ldb_request *req)
{
	struct extended_dn_context *ac;

	ac = talloc_zero(req, struct extended_dn_context);
	if (ac == NULL) {
		ldb_oom(ldb_module_get_ctx(module));
		return NULL;
	}

	ac->schema = dsdb_get_schema(ldb_module_get_ctx(module));
	ac->module = module;
	ac->req = req;

	return ac;
}

/* An extra layer of indirection because LDB does not allow the original request to be altered */

static int extended_final_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret = LDB_ERR_OPERATIONS_ERROR;
	struct extended_dn_context *ac;
	ac = talloc_get_type(req->context, struct extended_dn_context);

	if (ares->error != LDB_SUCCESS) {
		ret = ldb_module_done(ac->req, ares->controls,
				      ares->response, ares->error);
	} else {
		switch (ares->type) {
		case LDB_REPLY_ENTRY:
			
			ret = ldb_module_send_entry(ac->req, ares->message, ares->controls);
			break;
		case LDB_REPLY_REFERRAL:
			
			ret = ldb_module_send_referral(ac->req, ares->referral);
			break;
		case LDB_REPLY_DONE:
			
			ret = ldb_module_done(ac->req, ares->controls,
					      ares->response, ares->error);
			break;
		}
	}
	return ret;
}

static int extended_replace_dn(struct ldb_request *req, struct ldb_reply *ares)
{
	struct extended_dn_replace_list *os = talloc_get_type(req->context, 
							   struct extended_dn_replace_list);

	if (!ares) {
		return ldb_module_done(os->ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error == LDB_ERR_NO_SUCH_OBJECT) {
		/* Don't worry too much about dangling references */

		ldb_reset_err_string(ldb_module_get_ctx(os->ac->module));
		if (os->next) {
			struct extended_dn_replace_list *next;

			next = os->next;

			talloc_free(os);

			os = next;
			return ldb_next_request(os->ac->module, next->search_req);
		} else {
			/* Otherwise, we are done - let's run the
			 * request now we have swapped the DNs for the
			 * full versions */
			return ldb_next_request(os->ac->module, os->ac->req);
		}
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(os->ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* Only entries are interesting, and we only want the olddn */
	switch (ares->type) {
	case LDB_REPLY_ENTRY:
	{
		/* This *must* be the right DN, as this is a base
		 * search.  We can't check, as it could be an extended
		 * DN, so a module below will resolve it */
		struct ldb_dn *dn = ares->message->dn;

		/* Replace the DN with the extended version of the DN
		 * (ie, add SID and GUID) */
		*os->replace_dn = data_blob_string_const(
			ldb_dn_get_extended_linearized(os->mem_ctx, 
						       dn, 1));
		if (os->replace_dn->data == NULL) {
			return ldb_module_done(os->ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		break;
	}
	case LDB_REPLY_REFERRAL:
		/* ignore */
		break;

	case LDB_REPLY_DONE:

		talloc_free(ares);
		
		/* Run the next search */

		if (os->next) {
			struct extended_dn_replace_list *next;

			next = os->next;

			talloc_free(os);

			os = next;
			return ldb_next_request(os->ac->module, next->search_req);
		} else {
			/* Otherwise, we are done - let's run the
			 * request now we have swapped the DNs for the
			 * full versions */
			return ldb_next_request(os->ac->module, os->ac->new_req);
		}
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

/* We have a 'normal' DN in the inbound request.  We need to find out
 * what the GUID and SID are on the DN it points to, so we can
 * construct an extended DN for storage.
 *
 * This creates a list of DNs to look up, and the plain DN to replace
 */

static int extended_store_replace(struct extended_dn_context *ac,
				  TALLOC_CTX *callback_mem_ctx,
				  struct ldb_val *plain_dn)
{
	int ret;
	struct extended_dn_replace_list *os;
	static const char *attrs[] = {
		"objectSid",
		"objectGUID",
		NULL
	};

	os = talloc_zero(ac, struct extended_dn_replace_list);
	if (!os) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	os->ac = ac;
	
	os->mem_ctx = callback_mem_ctx;

	os->dn = ldb_dn_from_ldb_val(os, ldb_module_get_ctx(ac->module), plain_dn);
	if (!os->dn || !ldb_dn_validate(os->dn)) {
		talloc_free(os);
		ldb_asprintf_errstring(ldb_module_get_ctx(ac->module), 
				       "could not parse %.*s as a DN", (int)plain_dn->length, plain_dn->data);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	os->replace_dn = plain_dn;

	/* The search request here might happen to be for an
	 * 'extended' style DN, such as <GUID=abced...>.  The next
	 * module in the stack will convert this into a normal DN for
	 * processing */
	ret = ldb_build_search_req(&os->search_req,
				   ldb_module_get_ctx(ac->module), os, os->dn, LDB_SCOPE_BASE, NULL, 
				   attrs, NULL, os, extended_replace_dn,
				   ac->req);

	if (ret != LDB_SUCCESS) {
		talloc_free(os);
		return ret;
	}

	ret = ldb_request_add_control(os->search_req,
				      DSDB_CONTROL_DN_STORAGE_FORMAT_OID,
				      true, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(os);
		return ret;
	}

	if (ac->ops) {
		ac->cur->next = os;
	} else {
		ac->ops = os;
	}
	ac->cur = os;

	return LDB_SUCCESS;
}


/* add */
static int extended_dn_add(struct ldb_module *module, struct ldb_request *req)
{
	struct extended_dn_context *ac;
	int ret;
	int i, j;

	if (ldb_dn_is_special(req->op.add.message->dn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ac = extended_dn_context_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	for (i=0; i < req->op.add.message->num_elements; i++) {
		const struct ldb_message_element *el = &req->op.add.message->elements[i];
		const struct dsdb_attribute *schema_attr
			= dsdb_attribute_by_lDAPDisplayName(ac->schema, el->name);
		if (!schema_attr) {
			continue;
		}

		/* We only setup an extended DN GUID on these particular DN objects */
		if (strcmp(schema_attr->attributeSyntax_oid, "2.5.5.1") != 0) {
			continue;
		}

		/* Before we setup a procedure to modify the incoming message, we must copy it */
		if (!ac->new_req) {
			struct ldb_message *msg = ldb_msg_copy(ac, req->op.add.message);
			if (!msg) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
		   
			ret = ldb_build_add_req(&ac->new_req, ldb_module_get_ctx(module), ac, msg, req->controls, ac, extended_final_callback, req);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
		/* Re-calculate el */
		el = &ac->new_req->op.add.message->elements[i];
		for (j = 0; j < el->num_values; j++) {
			ret = extended_store_replace(ac, ac->new_req->op.add.message->elements, &el->values[j]);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
	}

	/* if DNs were set continue */
	if (ac->ops == NULL) {
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	/* start with the searches */
	return ldb_next_request(module, ac->ops->search_req);
}

/* modify */
static int extended_dn_modify(struct ldb_module *module, struct ldb_request *req)
{
	/* Look over list of modifications */
	/* Find if any are for linked attributes */
	/* Determine the effect of the modification */
	/* Apply the modify to the linked entry */

	int i, j;
	struct extended_dn_context *ac;
	int ret;

	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		/* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}

	ac = extended_dn_context_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!ac->schema) {
		/* without schema, this doesn't make any sense */
		return ldb_next_request(module, req);
	}

	for (i=0; i < req->op.mod.message->num_elements; i++) {
		const struct ldb_message_element *el = &req->op.mod.message->elements[i];
		const struct dsdb_attribute *schema_attr
			= dsdb_attribute_by_lDAPDisplayName(ac->schema, el->name);
		if (!schema_attr) {
			continue;
		}

		/* We only setup an extended DN GUID on these particular DN objects */
		if (strcmp(schema_attr->attributeSyntax_oid, "2.5.5.1") != 0) {
			continue;
		}
		
		/* Before we setup a procedure to modify the incoming message, we must copy it */
		if (!ac->new_req) {
			struct ldb_message *msg = ldb_msg_copy(ac, req->op.mod.message);
			if (!msg) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
		   
			ret = ldb_build_mod_req(&ac->new_req, ldb_module_get_ctx(module), ac, msg, req->controls, ac, extended_final_callback, req);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
		/* Re-calculate el */
		el = &ac->new_req->op.mod.message->elements[i];
		/* For each value being added, we need to setup the lookups to fill in the extended DN */
		for (j = 0; j < el->num_values; j++) {
			struct ldb_dn *dn = ldb_dn_from_ldb_val(ac, ldb_module_get_ctx(module), &el->values[j]);
			if (!dn || !ldb_dn_validate(dn)) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						       "could not parse attribute %s as a DN", el->name);
				return LDB_ERR_INVALID_DN_SYNTAX;
			}
			if (((el->flags & LDB_FLAG_MOD_MASK) == LDB_FLAG_MOD_DELETE) && !ldb_dn_has_extended(dn)) {
				/* NO need to figure this DN out, it's going to be deleted anyway */
				continue;
			}
			ret = extended_store_replace(ac, req->op.mod.message->elements, &el->values[j]);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
	}

	/* if DNs were set continue */
	if (ac->ops == NULL) {
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	/* start with the searches */
	return ldb_next_request(module, ac->ops->search_req);
}

_PUBLIC_ const struct ldb_module_ops ldb_extended_dn_store_module_ops = {
	.name		   = "extended_dn_store",
	.add               = extended_dn_add,
	.modify            = extended_dn_modify,
};
