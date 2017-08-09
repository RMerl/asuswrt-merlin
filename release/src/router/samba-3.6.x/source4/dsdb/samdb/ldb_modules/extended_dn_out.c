/* 
   ldb database library

   Copyright (C) Simo Sorce 2005-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007-2009

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
 *  results, and fixes some other aspects of the result (returned case issues)
 *  values.
 *
 *  Authors: Simo Sorce
 *           Andrew Bartlett
 */

#include "includes.h"
#include <ldb.h>
#include <ldb_errors.h>
#include <ldb_module.h>
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/ndr/libndr.h"
#include "dsdb/samdb/samdb.h"
#include "util.h"

struct extended_dn_out_private {
	bool dereference;
	bool normalise;
	struct dsdb_openldap_dereference_control *dereference_control;
	const char **attrs;
};

/* Do the lazy init of the derererence control */

static int extended_dn_out_dereference_setup_control(struct ldb_context *ldb, struct extended_dn_out_private *p)
{
	const struct dsdb_schema *schema;
	struct dsdb_openldap_dereference_control *dereference_control;
	struct dsdb_attribute *cur;

	unsigned int i = 0;
	if (p->dereference_control) {
		return LDB_SUCCESS;
	}

	schema = dsdb_get_schema(ldb, p);
	if (!schema) {
		/* No schema on this DB (yet) */
		return LDB_SUCCESS;
	}

	p->dereference_control = dereference_control
		= talloc_zero(p, struct dsdb_openldap_dereference_control);

	if (!p->dereference_control) {
		return ldb_oom(ldb);
	}

	for (cur = schema->attributes; cur; cur = cur->next) {
		if (dsdb_dn_oid_to_format(cur->syntax->ldap_oid) != DSDB_NORMAL_DN) {
			continue;
		}
		dereference_control->dereference
			= talloc_realloc(p, dereference_control->dereference,
					 struct dsdb_openldap_dereference *, i + 2);
		if (!dereference_control) {
			return ldb_oom(ldb);
		}
		dereference_control->dereference[i] = talloc(dereference_control->dereference,
					 struct dsdb_openldap_dereference);
		if (!dereference_control->dereference[i]) {
			return ldb_oom(ldb);
		}
		dereference_control->dereference[i]->source_attribute = cur->lDAPDisplayName;
		dereference_control->dereference[i]->dereference_attribute = p->attrs;
		i++;
		dereference_control->dereference[i] = NULL;
	}
	return LDB_SUCCESS;
}

static char **copy_attrs(void *mem_ctx, const char * const * attrs)
{
	char **nattrs;
	unsigned int i, num;

	for (num = 0; attrs[num]; num++);

	nattrs = talloc_array(mem_ctx, char *, num + 1);
	if (!nattrs) return NULL;

	for(i = 0; i < num; i++) {
		nattrs[i] = talloc_strdup(nattrs, attrs[i]);
		if (!nattrs[i]) {
			talloc_free(nattrs);
			return NULL;
		}
	}
	nattrs[i] = NULL;

	return nattrs;
}

static bool add_attrs(void *mem_ctx, char ***attrs, const char *attr)
{
	char **nattrs;
	unsigned int num;

	for (num = 0; (*attrs)[num]; num++);

	nattrs = talloc_realloc(mem_ctx, *attrs, char *, num + 2);
	if (!nattrs) return false;

	*attrs = nattrs;

	nattrs[num] = talloc_strdup(nattrs, attr);
	if (!nattrs[num]) return false;

	nattrs[num + 1] = NULL;

	return true;
}

/* Fix the DN so that the relative attribute names are in upper case so that the DN:
   cn=Adminstrator,cn=users,dc=samba,dc=example,dc=com becomes
   CN=Adminstrator,CN=users,DC=samba,DC=example,DC=com
*/


static int fix_dn(struct ldb_context *ldb, struct ldb_dn *dn)
{
	int i, ret;
	char *upper_rdn_attr;

	for (i=0; i < ldb_dn_get_comp_num(dn); i++) {
		/* We need the attribute name in upper case */
		upper_rdn_attr = strupper_talloc(dn,
						 ldb_dn_get_component_name(dn, i));
		if (!upper_rdn_attr) {
			return ldb_oom(ldb);
		}
		
		/* And replace it with CN=foo (we need the attribute in upper case */
		ret = ldb_dn_set_component(dn, i, upper_rdn_attr,
					   *ldb_dn_get_component_val(dn, i));
		talloc_free(upper_rdn_attr);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/* Inject the extended DN components, so the DN cn=Adminstrator,cn=users,dc=samba,dc=example,dc=com becomes
   <GUID=541203ae-f7d6-47ef-8390-bfcf019f9583>;<SID=S-1-5-21-4177067393-1453636373-93818737-500>;cn=Adminstrator,cn=users,dc=samba,dc=example,dc=com */

static int inject_extended_dn_out(struct ldb_reply *ares,
				  struct ldb_context *ldb,
				  int type,
				  bool remove_guid,
				  bool remove_sid)
{
	int ret;
	const DATA_BLOB *guid_blob;
	const DATA_BLOB *sid_blob;

	guid_blob = ldb_msg_find_ldb_val(ares->message, "objectGUID");
	sid_blob = ldb_msg_find_ldb_val(ares->message, "objectSid");

	if (!guid_blob) {
		ldb_set_errstring(ldb, "Did not find objectGUID to inject into extended DN");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_dn_set_extended_component(ares->message->dn, "GUID", guid_blob);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	if (sid_blob) {
		ret = ldb_dn_set_extended_component(ares->message->dn, "SID", sid_blob);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (remove_guid) {
		ldb_msg_remove_attr(ares->message, "objectGUID");
	}

	if (sid_blob && remove_sid) {
		ldb_msg_remove_attr(ares->message, "objectSid");
	}

	return LDB_SUCCESS;
}

static int handle_dereference_openldap(struct ldb_dn *dn,
			      struct dsdb_openldap_dereference_result **dereference_attrs, 
			      const char *attr, const DATA_BLOB *val)
{
	const struct ldb_val *entryUUIDblob, *sid_blob;
	struct ldb_message fake_msg; /* easier to use routines that expect an ldb_message */
	unsigned int j;
	
	fake_msg.num_elements = 0;
			
	/* Look for this attribute in the returned control */
	for (j = 0; dereference_attrs && dereference_attrs[j]; j++) {
		struct ldb_val source_dn = data_blob_string_const(dereference_attrs[j]->dereferenced_dn);
		if (ldb_attr_cmp(dereference_attrs[j]->source_attribute, attr) == 0
		    && data_blob_cmp(&source_dn, val) == 0) {
			fake_msg.num_elements = dereference_attrs[j]->num_attributes;
			fake_msg.elements = dereference_attrs[j]->attributes;
			break;
		}
	}
	if (!fake_msg.num_elements) {
		return LDB_SUCCESS;
	}
	/* Look for an OpenLDAP entryUUID */
	
	entryUUIDblob = ldb_msg_find_ldb_val(&fake_msg, "entryUUID");
	if (entryUUIDblob) {
		NTSTATUS status;
		struct ldb_val guid_blob;
		struct GUID guid;
		
		status = GUID_from_data_blob(entryUUIDblob, &guid);
		
		if (!NT_STATUS_IS_OK(status)) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		status = GUID_to_ndr_blob(&guid, dn, &guid_blob);
		if (!NT_STATUS_IS_OK(status)) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		
		ldb_dn_set_extended_component(dn, "GUID", &guid_blob);
	}
	
	sid_blob = ldb_msg_find_ldb_val(&fake_msg, "objectSid");
	
	/* Look for the objectSid */
	if (sid_blob) {
		ldb_dn_set_extended_component(dn, "SID", sid_blob);
	}
	return LDB_SUCCESS;
}

static int handle_dereference_fds(struct ldb_dn *dn,
			      struct dsdb_openldap_dereference_result **dereference_attrs, 
			      const char *attr, const DATA_BLOB *val)
{
	const struct ldb_val *nsUniqueIdBlob, *sidBlob;
	struct ldb_message fake_msg; /* easier to use routines that expect an ldb_message */
	unsigned int j;
	
	fake_msg.num_elements = 0;
			
	/* Look for this attribute in the returned control */
	for (j = 0; dereference_attrs && dereference_attrs[j]; j++) {
		struct ldb_val source_dn = data_blob_string_const(dereference_attrs[j]->dereferenced_dn);
		if (ldb_attr_cmp(dereference_attrs[j]->source_attribute, attr) == 0
		    && data_blob_cmp(&source_dn, val) == 0) {
			fake_msg.num_elements = dereference_attrs[j]->num_attributes;
			fake_msg.elements = dereference_attrs[j]->attributes;
			break;
		}
	}
	if (!fake_msg.num_elements) {
		return LDB_SUCCESS;
	}

	/* Look for the nsUniqueId */
	
	nsUniqueIdBlob = ldb_msg_find_ldb_val(&fake_msg, "nsUniqueId");
	if (nsUniqueIdBlob) {
		NTSTATUS status;
		struct ldb_val guid_blob;
		struct GUID guid;
		
        	status = NS_GUID_from_string((char *)nsUniqueIdBlob->data, &guid);
		
		if (!NT_STATUS_IS_OK(status)) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		status = GUID_to_ndr_blob(&guid, dn, &guid_blob);
		if (!NT_STATUS_IS_OK(status)) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		
		ldb_dn_set_extended_component(dn, "GUID", &guid_blob);
	}
	
	/* Look for the objectSid */

	sidBlob = ldb_msg_find_ldb_val(&fake_msg, "sambaSID");
	if (sidBlob) {
		enum ndr_err_code ndr_err;

		struct ldb_val sid_blob;
        	struct dom_sid *sid;

        	sid = dom_sid_parse_length(NULL, sidBlob);

        	if (sid == NULL) {
			return LDB_ERR_INVALID_DN_SYNTAX;
        	}

        	ndr_err = ndr_push_struct_blob(&sid_blob, NULL, sid,
						(ndr_push_flags_fn_t)ndr_push_dom_sid);
        	talloc_free(sid);
        	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return LDB_ERR_INVALID_DN_SYNTAX;
        	}

		ldb_dn_set_extended_component(dn, "SID", &sid_blob);
	}
	return LDB_SUCCESS;
}

/* search */
struct extended_search_context {
	struct ldb_module *module;
	const struct dsdb_schema *schema;
	struct ldb_request *req;
	bool inject;
	bool remove_guid;
	bool remove_sid;
	int extended_type;
};

static int extended_callback(struct ldb_request *req, struct ldb_reply *ares,
		int (*handle_dereference)(struct ldb_dn *dn,
				struct dsdb_openldap_dereference_result **dereference_attrs, 
				const char *attr, const DATA_BLOB *val))
{
	struct extended_search_context *ac;
	struct ldb_control *control;
	struct dsdb_openldap_dereference_result_control *dereference_control = NULL;
	int ret;
	unsigned int i, j;
	struct ldb_message *msg;
	struct extended_dn_out_private *p;
	struct ldb_context *ldb;
	bool have_reveal_control=false, checked_reveal_control=false;

	ac = talloc_get_type(req->context, struct extended_search_context);
	p = talloc_get_type(ldb_module_get_private(ac->module), struct extended_dn_out_private);
	ldb = ldb_module_get_ctx(ac->module);
	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	msg = ares->message;

	switch (ares->type) {
	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,
					ares->response, LDB_SUCCESS);
	case LDB_REPLY_ENTRY:
		break;
	}

	if (p && p->normalise) {
		ret = fix_dn(ldb, ares->message->dn);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}
			
	if (ac->inject) {
		/* for each record returned post-process to add any derived
		   attributes that have been asked for */
		ret = inject_extended_dn_out(ares, ldb,
					     ac->extended_type, ac->remove_guid,
					     ac->remove_sid);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}

	if ((p && p->normalise) || ac->inject) {
		const struct ldb_val *val = ldb_msg_find_ldb_val(ares->message, "distinguishedName");
		if (val) {
			ldb_msg_remove_attr(ares->message, "distinguishedName");
			if (ac->inject) {
				ret = ldb_msg_add_steal_string(ares->message, "distinguishedName", 
							       ldb_dn_get_extended_linearized(ares->message, ares->message->dn, ac->extended_type));
			} else {
				ret = ldb_msg_add_linearized_dn(ares->message,
								"distinguishedName",
								ares->message->dn);
			}
			if (ret != LDB_SUCCESS) {
				return ldb_oom(ldb);
			}
		}
	}

	if (p && p->dereference) {
		control = ldb_reply_get_control(ares, DSDB_OPENLDAP_DEREFERENCE_CONTROL);
	
		if (control && control->data) {
			dereference_control = talloc_get_type(control->data, struct dsdb_openldap_dereference_result_control);
		}
	}

	/* Walk the returned elements (but only if we have a schema to
	 * interpret the list with) */
	for (i = 0; ac->schema && i < msg->num_elements; i++) {
		bool make_extended_dn;
		const struct dsdb_attribute *attribute;
		attribute = dsdb_attribute_by_lDAPDisplayName(ac->schema, msg->elements[i].name);
		if (!attribute) {
			continue;
		}

		if (p->normalise) {
			/* If we are also in 'normalise' mode, then
			 * fix the attribute names to be in the
			 * correct case */
			msg->elements[i].name = talloc_strdup(msg->elements, attribute->lDAPDisplayName);
			if (!msg->elements[i].name) {
				ldb_oom(ldb);
				return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
			}
		}

		/* distinguishedName has been dealt with above */
		if (ldb_attr_cmp(msg->elements[i].name, "distinguishedName") == 0) {
			continue;
		}

		/* Look to see if this attributeSyntax is a DN */
		if (dsdb_dn_oid_to_format(attribute->syntax->ldap_oid) == DSDB_INVALID_DN) {
			continue;
		}

		make_extended_dn = ac->inject;

		/* Always show plain DN in case of Object(OR-Name) syntax */
		if (make_extended_dn) {
			make_extended_dn = (strcmp(attribute->syntax->ldap_oid, DSDB_SYNTAX_OR_NAME) != 0);
		}

		for (j = 0; j < msg->elements[i].num_values; j++) {
			const char *dn_str;
			struct ldb_dn *dn;
			struct dsdb_dn *dsdb_dn = NULL;
			struct ldb_val *plain_dn = &msg->elements[i].values[j];		

			if (!checked_reveal_control) {
				have_reveal_control =
					ldb_request_get_control(req, LDB_CONTROL_REVEAL_INTERNALS) != NULL;
				checked_reveal_control = true;
			}

			/* this is a fast method for detecting deleted
			   linked attributes, working on the unparsed
			   ldb_val */
			if (dsdb_dn_is_deleted_val(plain_dn) && !have_reveal_control) {
				/* it's a deleted linked attribute,
				  and we don't have the reveal control */
				memmove(&msg->elements[i].values[j],
					&msg->elements[i].values[j+1],
					(msg->elements[i].num_values-(j+1))*sizeof(struct ldb_val));
				msg->elements[i].num_values--;
				j--;
				continue;
			}


			dsdb_dn = dsdb_dn_parse(msg, ldb, plain_dn, attribute->syntax->ldap_oid);

			if (!dsdb_dn || !ldb_dn_validate(dsdb_dn->dn)) {
				ldb_asprintf_errstring(ldb, 
						       "could not parse %.*s in %s on %s as a %s DN", 
						       (int)plain_dn->length, plain_dn->data,
						       msg->elements[i].name, ldb_dn_get_linearized(msg->dn),
						       attribute->syntax->ldap_oid);
				talloc_free(dsdb_dn);
				return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_INVALID_DN_SYNTAX);
			}
			dn = dsdb_dn->dn;

			/* don't let users see the internal extended
			   GUID components */
			if (!have_reveal_control) {
				const char *accept[] = { "GUID", "SID", "WKGUID", NULL };
				ldb_dn_extended_filter(dn, accept);
			}

			if (p->normalise) {
				ret = fix_dn(ldb, dn);
				if (ret != LDB_SUCCESS) {
					talloc_free(dsdb_dn);
					return ldb_module_done(ac->req, NULL, NULL, ret);
				}
			}
			
			/* If we are running in dereference mode (such
			 * as against OpenLDAP) then the DN in the msg
			 * above does not contain the extended values,
			 * and we need to look in the dereference
			 * result */

			/* Look for this value in the attribute */

			if (dereference_control) {
				ret = handle_dereference(dn, 
							 dereference_control->attributes,
							 msg->elements[i].name,
							 &msg->elements[i].values[j]);
				if (ret != LDB_SUCCESS) {
					talloc_free(dsdb_dn);
					return ldb_module_done(ac->req, NULL, NULL, ret);
				}
			}
			
			if (make_extended_dn) {
				dn_str = dsdb_dn_get_extended_linearized(msg->elements[i].values,
									 dsdb_dn, ac->extended_type);
			} else {
				dn_str = dsdb_dn_get_linearized(msg->elements[i].values, 
								dsdb_dn);
			}
			
			if (!dn_str) {
				ldb_oom(ldb);
				talloc_free(dsdb_dn);
				return ldb_module_done(ac->req, NULL, NULL, LDB_ERR_OPERATIONS_ERROR);
			}
			msg->elements[i].values[j] = data_blob_string_const(dn_str);
			talloc_free(dsdb_dn);
		}
		if (msg->elements[i].num_values == 0) {
			/* we've deleted all of the values from this
			 * element - remove the element */
			memmove(&msg->elements[i],
				&msg->elements[i+1],
				(msg->num_elements-(i+1))*sizeof(struct ldb_message_element));
			msg->num_elements--;
			i--;
		}
	}
	return ldb_module_send_entry(ac->req, msg, ares->controls);
}

static int extended_callback_ldb(struct ldb_request *req, struct ldb_reply *ares)
{
	return extended_callback(req, ares, NULL);
}

static int extended_callback_openldap(struct ldb_request *req, struct ldb_reply *ares)
{
	return extended_callback(req, ares, handle_dereference_openldap);
}

static int extended_callback_fds(struct ldb_request *req, struct ldb_reply *ares)
{
	return extended_callback(req, ares, handle_dereference_fds);
}

static int extended_dn_out_search(struct ldb_module *module, struct ldb_request *req,
		int (*callback)(struct ldb_request *req, struct ldb_reply *ares))
{
	struct ldb_control *control;
	struct ldb_control *storage_format_control;
	struct ldb_extended_dn_control *extended_ctrl = NULL;
	struct extended_search_context *ac;
	struct ldb_request *down_req;
	char **new_attrs;
	const char * const *const_attrs;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;
	bool critical;

	struct extended_dn_out_private *p = talloc_get_type(ldb_module_get_private(module), struct extended_dn_out_private);

	/* The schema manipulation does not apply to special DNs */
	if (ldb_dn_is_special(req->op.search.base)) {
		return ldb_next_request(module, req);
	}

	/* check if there's an extended dn control */
	control = ldb_request_get_control(req, LDB_CONTROL_EXTENDED_DN_OID);
	if (control && control->data) {
		extended_ctrl = talloc_get_type(control->data, struct ldb_extended_dn_control);
		if (!extended_ctrl) {
			return LDB_ERR_PROTOCOL_ERROR;
		}
	}

	/* Look to see if, as we are in 'store DN+GUID+SID' mode, the
	 * client is after the storage format (to fill in linked
	 * attributes) */
	storage_format_control = ldb_request_get_control(req, DSDB_CONTROL_DN_STORAGE_FORMAT_OID);
	if (!control && storage_format_control && storage_format_control->data) {
		extended_ctrl = talloc_get_type(storage_format_control->data, struct ldb_extended_dn_control);
		if (!extended_ctrl) {
			ldb_set_errstring(ldb, "extended_dn_out: extended_ctrl was of the wrong data type");
			return LDB_ERR_PROTOCOL_ERROR;
		}
	}

	ac = talloc_zero(req, struct extended_search_context);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}

	ac->module = module;
	ac->schema = dsdb_get_schema(ldb, ac);
	ac->req = req;
	ac->inject = false;
	ac->remove_guid = false;
	ac->remove_sid = false;
	
	const_attrs = req->op.search.attrs;

	/* We only need to do special processing if we were asked for
	 * the extended DN, or we are 'store DN+GUID+SID'
	 * (!dereference) mode.  (This is the normal mode for LDB on
	 * tdb). */
	if (control || (storage_format_control && p && !p->dereference)) {
		ac->inject = true;
		if (extended_ctrl) {
			ac->extended_type = extended_ctrl->type;
		} else {
			ac->extended_type = 0;
		}

		/* check if attrs only is specified, in that case check wether we need to modify them */
		if (req->op.search.attrs && !is_attr_in_list(req->op.search.attrs, "*")) {
			if (! is_attr_in_list(req->op.search.attrs, "objectGUID")) {
				ac->remove_guid = true;
			}
			if (! is_attr_in_list(req->op.search.attrs, "objectSid")) {
				ac->remove_sid = true;
			}
			if (ac->remove_guid || ac->remove_sid) {
				new_attrs = copy_attrs(ac, req->op.search.attrs);
				if (new_attrs == NULL) {
					return ldb_oom(ldb);
				}

				if (ac->remove_guid) {
					if (!add_attrs(ac, &new_attrs, "objectGUID"))
						return ldb_operr(ldb);
				}
				if (ac->remove_sid) {
					if (!add_attrs(ac, &new_attrs, "objectSid"))
						return ldb_operr(ldb);
				}
				const_attrs = (const char * const *)new_attrs;
			}
		}
	}

	ret = ldb_build_search_req_ex(&down_req,
				      ldb, ac,
				      req->op.search.base,
				      req->op.search.scope,
				      req->op.search.tree,
				      const_attrs,
				      req->controls,
				      ac, callback,
				      req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* mark extended DN and storage format controls as done */
	if (control) {
		critical = control->critical;
		control->critical = 0;
	}

	if (storage_format_control) {
		storage_format_control->critical = 0;
	}

	/* Add in dereference control, if we were asked to, we are
	 * using the 'dereference' mode (such as with an OpenLDAP
	 * backend) and have the control prepared */
	if (control && p && p->dereference) {
		ret = extended_dn_out_dereference_setup_control(ldb, p);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		/* We should always have this, but before the schema
		 * is with us, things get tricky */
		if (p->dereference_control) {

			/* This control must *not* be critical,
			 * because if this particular request did not
			 * return any dereferencable attributes in the
			 * end, then OpenLDAP will reply with
			 * unavailableCriticalExtension, rather than
			 * just an empty return control */
			ret = ldb_request_add_control(down_req,
						      DSDB_OPENLDAP_DEREFERENCE_CONTROL,
						      false, p->dereference_control);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
	}

	/* perform the search */
	return ldb_next_request(module, down_req);
}

static int extended_dn_out_ldb_search(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_out_search(module, req, extended_callback_ldb);
}

static int extended_dn_out_openldap_search(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_out_search(module, req, extended_callback_openldap);
}

static int extended_dn_out_fds_search(struct ldb_module *module, struct ldb_request *req)
{
	return extended_dn_out_search(module, req, extended_callback_fds);
}

static int extended_dn_out_ldb_init(struct ldb_module *module)
{
	int ret;

	struct extended_dn_out_private *p = talloc(module, struct extended_dn_out_private);
	struct dsdb_extended_dn_store_format *dn_format;

	ldb_module_set_private(module, p);

	if (!p) {
		return ldb_oom(ldb_module_get_ctx(module));
	}

	dn_format = talloc(p, struct dsdb_extended_dn_store_format);
	if (!dn_format) {
		talloc_free(p);
		return ldb_oom(ldb_module_get_ctx(module));
	}

	dn_format->store_extended_dn_in_ldb = true;
	ret = ldb_set_opaque(ldb_module_get_ctx(module), DSDB_EXTENDED_DN_STORE_FORMAT_OPAQUE_NAME, dn_format);
	if (ret != LDB_SUCCESS) {
		talloc_free(p);
		return ret;
	}

	p->dereference = false;
	p->normalise = false;

	ret = ldb_mod_register_control(module, LDB_CONTROL_EXTENDED_DN_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR,
			"extended_dn_out: Unable to register control with rootdse!\n");
		return ldb_operr(ldb_module_get_ctx(module));
	}

	return ldb_next_init(module);
}

static int extended_dn_out_dereference_init(struct ldb_module *module, const char *attrs[])
{
	int ret;
	struct extended_dn_out_private *p = talloc_zero(module, struct extended_dn_out_private);
	struct dsdb_extended_dn_store_format *dn_format;

	ldb_module_set_private(module, p);

	if (!p) {
		return ldb_module_oom(module);
	}

	dn_format = talloc(p, struct dsdb_extended_dn_store_format);
	if (!dn_format) {
		talloc_free(p);
		return ldb_module_oom(module);
	}

	dn_format->store_extended_dn_in_ldb = false;

	ret = ldb_set_opaque(ldb_module_get_ctx(module), DSDB_EXTENDED_DN_STORE_FORMAT_OPAQUE_NAME, dn_format);
	if (ret != LDB_SUCCESS) {
		talloc_free(p);
		return ret;
	}

	p->dereference = true;

	p->attrs = attrs;
	/* At the moment, servers that need dereference also need the
	 * DN and attribute names to be normalised */
	p->normalise = true;

	ret = ldb_mod_register_control(module, LDB_CONTROL_EXTENDED_DN_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR,
			  "extended_dn_out: Unable to register control with rootdse!\n");
		return ldb_operr(ldb_module_get_ctx(module));
	}

	return ldb_next_init(module);
}

static int extended_dn_out_openldap_init(struct ldb_module *module)
{
	static const char *attrs[] = {
		"entryUUID",
		"objectSid",
		NULL
	};

	return extended_dn_out_dereference_init(module, attrs);
}

static int extended_dn_out_fds_init(struct ldb_module *module)
{
	static const char *attrs[] = {
		"nsUniqueId",
		"sambaSID",
		NULL
	};

	return extended_dn_out_dereference_init(module, attrs);
}

static const struct ldb_module_ops ldb_extended_dn_out_ldb_module_ops = {
	.name		   = "extended_dn_out_ldb",
	.search            = extended_dn_out_ldb_search,
	.init_context	   = extended_dn_out_ldb_init,
};

static const struct ldb_module_ops ldb_extended_dn_out_openldap_module_ops = {
	.name		   = "extended_dn_out_openldap",
	.search            = extended_dn_out_openldap_search,
	.init_context	   = extended_dn_out_openldap_init,
};

static const struct ldb_module_ops ldb_extended_dn_out_fds_module_ops = {
	.name		   = "extended_dn_out_fds",
	.search            = extended_dn_out_fds_search,
	.init_context	   = extended_dn_out_fds_init,
};

/*
  initialise the module
 */
_PUBLIC_ int ldb_extended_dn_out_module_init(const char *version)
{
	int ret;
	LDB_MODULE_CHECK_VERSION(version);
	ret = ldb_register_module(&ldb_extended_dn_out_ldb_module_ops);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = ldb_register_module(&ldb_extended_dn_out_openldap_module_ops);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = ldb_register_module(&ldb_extended_dn_out_fds_module_ops);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	return LDB_SUCCESS;
}
