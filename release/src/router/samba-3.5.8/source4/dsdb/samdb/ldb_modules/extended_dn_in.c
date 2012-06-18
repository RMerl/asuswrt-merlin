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
 *  Description: this module interprets DNs of the form <SID=S-1-2-4456> into normal DNs.
 *
 *  Authors: Simo Sorce
 *           Andrew Bartlett
 */

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_errors.h"
#include "ldb/include/ldb_module.h"

/* search */
struct extended_search_context {
	struct ldb_module *module;
	struct ldb_request *req;
	struct ldb_dn *basedn;
	char *wellknown_object;
	int extended_type;
};

/* An extra layer of indirection because LDB does not allow the original request to be altered */

static int extended_final_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret = LDB_ERR_OPERATIONS_ERROR;
	struct extended_search_context *ac;
	ac = talloc_get_type(req->context, struct extended_search_context);

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

static int extended_base_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct extended_search_context *ac;
	struct ldb_request *down_req;
	struct ldb_message_element *el;
	int ret;
	size_t i;
	size_t wkn_len = 0;
	char *valstr = NULL;
	const char *found = NULL;

	ac = talloc_get_type(req->context, struct extended_search_context);

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
		if (!ac->wellknown_object) {
			ac->basedn = talloc_steal(ac, ares->message->dn);
			break;
		}

		wkn_len = strlen(ac->wellknown_object);

		el = ldb_msg_find_element(ares->message, "wellKnownObjects");
		if (!el) {
			ac->basedn = NULL;
			break;
		}

		for (i=0; i < el->num_values; i++) {
			valstr = talloc_strndup(ac,
						(const char *)el->values[i].data,
						el->values[i].length);
			if (!valstr) {
				ldb_oom(ldb_module_get_ctx(ac->module));
				return ldb_module_done(ac->req, NULL, NULL,
						       LDB_ERR_OPERATIONS_ERROR);
			}

			if (strncasecmp(valstr, ac->wellknown_object, wkn_len) != 0) {
				talloc_free(valstr);
				continue;
			}

			found = &valstr[wkn_len];
			break;
		}

		if (!found) {
			break;
		}

		ac->basedn = ldb_dn_new(ac, ldb_module_get_ctx(ac->module), found);
		talloc_free(valstr);
		if (!ac->basedn) {
			ldb_oom(ldb_module_get_ctx(ac->module));
			return ldb_module_done(ac->req, NULL, NULL,
					       LDB_ERR_OPERATIONS_ERROR);
		}

		break;

	case LDB_REPLY_REFERRAL:
		break;

	case LDB_REPLY_DONE:

		if (!ac->basedn) {
			const char *str = talloc_asprintf(req, "Base-DN '%s' not found",
							  ldb_dn_get_linearized(ac->req->op.search.base));
			ldb_set_errstring(ldb_module_get_ctx(ac->module), str);
			return ldb_module_done(ac->req, NULL, NULL,
					       LDB_ERR_NO_SUCH_OBJECT);
		}

		switch (ac->req->operation) {
		case LDB_SEARCH:
			ret = ldb_build_search_req_ex(&down_req,
						      ldb_module_get_ctx(ac->module), ac->req,
						      ac->basedn,
						      ac->req->op.search.scope,
						      ac->req->op.search.tree,
						      ac->req->op.search.attrs,
						      ac->req->controls,
						      ac, extended_final_callback, 
						      ac->req);
			break;
		case LDB_ADD:
		{
			struct ldb_message *add_msg = ldb_msg_copy_shallow(ac, ac->req->op.add.message);
			if (!add_msg) {
				ldb_oom(ldb_module_get_ctx(ac->module));
				return ldb_module_done(ac->req, NULL, NULL,
						       LDB_ERR_OPERATIONS_ERROR);
			}
			
			add_msg->dn = ac->basedn;

			ret = ldb_build_add_req(&down_req,
						ldb_module_get_ctx(ac->module), ac->req,
						add_msg, 
						ac->req->controls,
						ac, extended_final_callback, 
						ac->req);
			break;
		}
		case LDB_MODIFY:
		{
			struct ldb_message *mod_msg = ldb_msg_copy_shallow(ac, ac->req->op.mod.message);
			if (!mod_msg) {
				ldb_oom(ldb_module_get_ctx(ac->module));
				return ldb_module_done(ac->req, NULL, NULL,
						       LDB_ERR_OPERATIONS_ERROR);
			}
			
			mod_msg->dn = ac->basedn;

			ret = ldb_build_mod_req(&down_req,
						ldb_module_get_ctx(ac->module), ac->req,
						mod_msg, 
						ac->req->controls,
						ac, extended_final_callback, 
						ac->req);
			break;
		}
		case LDB_DELETE:
			ret = ldb_build_del_req(&down_req,
						ldb_module_get_ctx(ac->module), ac->req,
						ac->basedn, 
						ac->req->controls,
						ac, extended_final_callback, 
						ac->req);
			break;
		case LDB_RENAME:
			ret = ldb_build_rename_req(&down_req,
						   ldb_module_get_ctx(ac->module), ac->req,
						   ac->basedn, 
						   ac->req->op.rename.newdn,
						   ac->req->controls,
						   ac, extended_final_callback, 
						   ac->req);
			break;
		default:
			return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
		}
		
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		return ldb_next_request(ac->module, down_req);
	}
	talloc_free(ares);
	return LDB_SUCCESS;
}

static int extended_dn_in_fix(struct ldb_module *module, struct ldb_request *req, struct ldb_dn *dn)
{
	struct extended_search_context *ac;
	struct ldb_request *down_req;
	int ret;
	struct ldb_dn *base_dn = NULL;
	enum ldb_scope base_dn_scope = LDB_SCOPE_BASE;
	const char *base_dn_filter = NULL;
	const char * const *base_dn_attrs = NULL;
	char *wellknown_object = NULL;
	static const char *no_attr[] = {
		NULL
	};
	static const char *wkattr[] = {
		"wellKnownObjects",
		NULL
	};
	bool all_partitions = false;

	if (!ldb_dn_has_extended(dn)) {
		/* Move along there isn't anything to see here */
		return ldb_next_request(module, req);
	} else {
		/* It looks like we need to map the DN */
		const struct ldb_val *sid_val, *guid_val, *wkguid_val;

		sid_val = ldb_dn_get_extended_component(dn, "SID");
		guid_val = ldb_dn_get_extended_component(dn, "GUID");
		wkguid_val = ldb_dn_get_extended_component(dn, "WKGUID");

		if (sid_val) {
			all_partitions = true;
			base_dn = ldb_get_default_basedn(ldb_module_get_ctx(module));
			base_dn_filter = talloc_asprintf(req, "(objectSid=%s)", 
							 ldb_binary_encode(req, *sid_val));
			if (!base_dn_filter) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
			base_dn_scope = LDB_SCOPE_SUBTREE;
			base_dn_attrs = no_attr;

		} else if (guid_val) {

			all_partitions = true;
			base_dn = ldb_get_default_basedn(ldb_module_get_ctx(module));
			base_dn_filter = talloc_asprintf(req, "(objectGUID=%s)", 
							 ldb_binary_encode(req, *guid_val));
			if (!base_dn_filter) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
			base_dn_scope = LDB_SCOPE_SUBTREE;
			base_dn_attrs = no_attr;


		} else if (wkguid_val) {
			char *wkguid_dup;
			char *tail_str;
			char *p;

			wkguid_dup = talloc_strndup(req, (char *)wkguid_val->data, wkguid_val->length);

			p = strchr(wkguid_dup, ',');
			if (!p) {
				return LDB_ERR_INVALID_DN_SYNTAX;
			}

			p[0] = '\0';
			p++;

			wellknown_object = talloc_asprintf(req, "B:32:%s:", wkguid_dup);
			if (!wellknown_object) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}

			tail_str = p;

			base_dn = ldb_dn_new(req, ldb_module_get_ctx(module), tail_str);
			talloc_free(wkguid_dup);
			if (!base_dn) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
			base_dn_filter = talloc_strdup(req, "(objectClass=*)");
			if (!base_dn_filter) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
			base_dn_scope = LDB_SCOPE_BASE;
			base_dn_attrs = wkattr;
		} else {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}

		ac = talloc_zero(req, struct extended_search_context);
		if (ac == NULL) {
			ldb_oom(ldb_module_get_ctx(module));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		ac->module = module;
		ac->req = req;
		ac->basedn = NULL;  /* Filled in if the search finds the DN by SID/GUID etc */
		ac->wellknown_object = wellknown_object;
		
		/* If the base DN was an extended DN (perhaps a well known
		 * GUID) then search for that, so we can proceed with the original operation */

		ret = ldb_build_search_req(&down_req,
					   ldb_module_get_ctx(module), ac,
					   base_dn,
					   base_dn_scope,
					   base_dn_filter,
					   base_dn_attrs,
					   NULL,
					   ac, extended_base_callback,
					   req);
		if (ret != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (all_partitions) {
			struct ldb_search_options_control *control;
			control = talloc(down_req, struct ldb_search_options_control);
			control->search_options = 2;
			ret = ldb_request_add_control(down_req,
						      LDB_CONTROL_SEARCH_OPTIONS_OID,
						      true, control);
			if (ret != LDB_SUCCESS) {
				ldb_oom(ldb_module_get_ctx(module));
				return ret;
			}
		}

		/* perform the search */
		return ldb_next_request(module, down_req);
	}
}

static int extended_dn_in_search(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_in_fix(module, req, req->op.search.base);
}

static int extended_dn_in_modify(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_in_fix(module, req, req->op.mod.message->dn);
}

static int extended_dn_in_del(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_in_fix(module, req, req->op.del.dn);
}

static int extended_dn_in_rename(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_in_fix(module, req, req->op.rename.olddn);
}

_PUBLIC_ const struct ldb_module_ops ldb_extended_dn_in_module_ops = {
	.name		   = "extended_dn_in",
	.search            = extended_dn_in_search,
	.modify            = extended_dn_in_modify,
	.del               = extended_dn_in_del,
	.rename            = extended_dn_in_rename,
};
