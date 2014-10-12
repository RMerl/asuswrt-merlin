/*
   Unix SMB/CIFS implementation.

   rootDSE ldb module

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Simo Sorce 2005-2008

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
#include <ldb.h>
#include <ldb_module.h>
#include "system/time.h"
#include "dsdb/samdb/samdb.h"
#include "version.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "libcli/security/security.h"
#include "librpc/ndr/libndr.h"
#include "auth/auth.h"
#include "param/param.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc_c.h"

struct private_data {
	unsigned int num_controls;
	char **controls;
	unsigned int num_partitions;
	struct ldb_dn **partitions;
	bool block_anonymous;
};

/*
  return 1 if a specific attribute has been requested
*/
static int do_attribute(const char * const *attrs, const char *name)
{
	return attrs == NULL ||
		ldb_attr_in_list(attrs, name) ||
		ldb_attr_in_list(attrs, "*");
}

static int do_attribute_explicit(const char * const *attrs, const char *name)
{
	return attrs != NULL && ldb_attr_in_list(attrs, name);
}


/*
  expand a DN attribute to include extended DN information if requested
 */
static int expand_dn_in_message(struct ldb_module *module, struct ldb_message *msg,
				const char *attrname, struct ldb_control *edn_control,
				struct ldb_request *req)
{
	struct ldb_dn *dn, *dn2;
	struct ldb_val *v;
	int ret;
	struct ldb_request *req2;
	char *dn_string;
	const char *no_attrs[] = { NULL };
	struct ldb_result *res;
	struct ldb_extended_dn_control *edn;
	TALLOC_CTX *tmp_ctx = talloc_new(req);
	struct ldb_context *ldb;
	int edn_type = 0;

	ldb = ldb_module_get_ctx(module);

	edn = talloc_get_type(edn_control->data, struct ldb_extended_dn_control);
	if (edn) {
		edn_type = edn->type;
	}

	v = discard_const_p(struct ldb_val, ldb_msg_find_ldb_val(msg, attrname));
	if (v == NULL) {
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}

	dn_string = talloc_strndup(tmp_ctx, (const char *)v->data, v->length);
	if (dn_string == NULL) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (res == NULL) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	dn = ldb_dn_new(tmp_ctx, ldb, dn_string);
	if (dn == NULL) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	ret = ldb_build_search_req(&req2, ldb, tmp_ctx,
				   dn,
				   LDB_SCOPE_BASE,
				   NULL,
				   no_attrs,
				   NULL,
				   res, ldb_search_default_callback,
				   req);
	LDB_REQ_SET_LOCATION(req2);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}


	ret = ldb_request_add_control(req2,
				      LDB_CONTROL_EXTENDED_DN_OID,
				      edn_control->critical, edn);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = ldb_next_request(module, req2);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req2->handle, LDB_WAIT_ALL);
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (!res || res->count != 1) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	dn2 = res->msgs[0]->dn;

	v->data = (uint8_t *)ldb_dn_get_extended_linearized(msg->elements, dn2, edn_type);
	if (v->data == NULL) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}
	v->length = strlen((char *)v->data);


	talloc_free(tmp_ctx);

	return LDB_SUCCESS;
}


/*
  add dynamically generated attributes to rootDSE result
*/
static int rootdse_add_dynamic(struct ldb_module *module, struct ldb_message *msg,
			       const char * const *attrs, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct private_data *priv = talloc_get_type(ldb_module_get_private(module), struct private_data);
	char **server_sasl;
	const struct dsdb_schema *schema;
	int *val;
	struct ldb_control *edn_control;
	const char *dn_attrs[] = {
		"configurationNamingContext",
		"defaultNamingContext",
		"dsServiceName",
		"rootDomainNamingContext",
		"schemaNamingContext",
		"serverName",
		NULL
	};

	ldb = ldb_module_get_ctx(module);
	schema = dsdb_get_schema(ldb, NULL);

	msg->dn = ldb_dn_new(msg, ldb, NULL);

	/* don't return the distinguishedName, cn and name attributes */
	ldb_msg_remove_attr(msg, "distinguishedName");
	ldb_msg_remove_attr(msg, "cn");
	ldb_msg_remove_attr(msg, "name");

	if (do_attribute(attrs, "serverName")) {
		if (ldb_msg_add_linearized_dn(msg, "serverName",
			samdb_server_dn(ldb, msg)) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "dnsHostName")) {
		struct ldb_result *res;
		int ret;
		const char *dns_attrs[] = { "dNSHostName", NULL };
		ret = dsdb_module_search_dn(module, msg, &res, samdb_server_dn(ldb, msg),
					    dns_attrs, DSDB_FLAG_NEXT_MODULE, req);
		if (ret == LDB_SUCCESS) {
			const char *hostname = ldb_msg_find_attr_as_string(res->msgs[0], "dNSHostName", NULL);
			if (hostname != NULL) {
				if (ldb_msg_add_string(msg, "dNSHostName", hostname)) {
					goto failed;
				}
			}
		}
	}

	if (do_attribute(attrs, "ldapServiceName")) {
		struct loadparm_context *lp_ctx
			= talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
					  struct loadparm_context);
		char *ldap_service_name, *hostname;

		hostname = talloc_strdup(msg, lpcfg_netbios_name(lp_ctx));
		if (hostname == NULL) {
			goto failed;
		}
		strlower_m(hostname);

		ldap_service_name = talloc_asprintf(msg, "%s:%s$@%s",
						    samdb_forest_name(ldb, msg),
						    hostname, lpcfg_realm(lp_ctx));
		if (ldap_service_name == NULL) {
			goto failed;
		}

		if (ldb_msg_add_string(msg, "ldapServiceName",
				       ldap_service_name) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "currentTime")) {
		if (ldb_msg_add_steal_string(msg, "currentTime",
					     ldb_timestring(msg, time(NULL))) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (priv && do_attribute(attrs, "supportedControl")) {
		unsigned int i;
		for (i = 0; i < priv->num_controls; i++) {
			char *control = talloc_strdup(msg, priv->controls[i]);
			if (!control) {
				goto failed;
			}
			if (ldb_msg_add_steal_string(msg, "supportedControl",
						     control) != LDB_SUCCESS) {
				goto failed;
 			}
 		}
 	}

	if (priv && do_attribute(attrs, "namingContexts")) {
		unsigned int i;
		for (i = 0; i < priv->num_partitions; i++) {
			struct ldb_dn *dn = priv->partitions[i];
			if (ldb_msg_add_steal_string(msg, "namingContexts",
						     ldb_dn_alloc_linearized(msg, dn)) != LDB_SUCCESS) {
				goto failed;
 			}
 		}
	}

	server_sasl = talloc_get_type(ldb_get_opaque(ldb, "supportedSASLMechanisms"),
				       char *);
	if (server_sasl && do_attribute(attrs, "supportedSASLMechanisms")) {
		unsigned int i;
		for (i = 0; server_sasl && server_sasl[i]; i++) {
			char *sasl_name = talloc_strdup(msg, server_sasl[i]);
			if (!sasl_name) {
				goto failed;
			}
			if (ldb_msg_add_steal_string(msg, "supportedSASLMechanisms",
						     sasl_name) != LDB_SUCCESS) {
				goto failed;
			}
		}
	}

	if (do_attribute(attrs, "highestCommittedUSN")) {
		uint64_t seq_num;
		int ret = ldb_sequence_number(ldb, LDB_SEQ_HIGHEST_SEQ, &seq_num);
		if (ret == LDB_SUCCESS) {
			if (samdb_msg_add_uint64(ldb, msg, msg,
						 "highestCommittedUSN",
						 seq_num) != LDB_SUCCESS) {
				goto failed;
			}
		}
	}

	if (schema && do_attribute_explicit(attrs, "dsSchemaAttrCount")) {
		struct dsdb_attribute *cur;
		unsigned int n = 0;

		for (cur = schema->attributes; cur; cur = cur->next) {
			n++;
		}

		if (samdb_msg_add_uint(ldb, msg, msg, "dsSchemaAttrCount",
				       n) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (schema && do_attribute_explicit(attrs, "dsSchemaClassCount")) {
		struct dsdb_class *cur;
		unsigned int n = 0;

		for (cur = schema->classes; cur; cur = cur->next) {
			n++;
		}

		if (samdb_msg_add_uint(ldb, msg, msg, "dsSchemaClassCount",
				       n) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (schema && do_attribute_explicit(attrs, "dsSchemaPrefixCount")) {
		if (samdb_msg_add_uint(ldb, msg, msg, "dsSchemaPrefixCount",
				       schema->prefixmap->length) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute_explicit(attrs, "validFSMOs")) {
		const struct dsdb_naming_fsmo *naming_fsmo;
		const struct dsdb_pdc_fsmo *pdc_fsmo;
		const char *dn_str;

		if (schema && schema->fsmo.we_are_master) {
			dn_str = ldb_dn_get_linearized(ldb_get_schema_basedn(ldb));
			if (dn_str && dn_str[0]) {
				if (ldb_msg_add_fmt(msg, "validFSMOs", "%s", dn_str) != LDB_SUCCESS) {
					goto failed;
				}
			}
		}

		naming_fsmo = talloc_get_type(ldb_get_opaque(ldb, "dsdb_naming_fsmo"),
					      struct dsdb_naming_fsmo);
		if (naming_fsmo && naming_fsmo->we_are_master) {
			dn_str = ldb_dn_get_linearized(samdb_partitions_dn(ldb, msg));
			if (dn_str && dn_str[0]) {
				if (ldb_msg_add_fmt(msg, "validFSMOs", "%s", dn_str) != LDB_SUCCESS) {
					goto failed;
				}
			}
		}

		pdc_fsmo = talloc_get_type(ldb_get_opaque(ldb, "dsdb_pdc_fsmo"),
					   struct dsdb_pdc_fsmo);
		if (pdc_fsmo && pdc_fsmo->we_are_master) {
			dn_str = ldb_dn_get_linearized(ldb_get_default_basedn(ldb));
			if (dn_str && dn_str[0]) {
				if (ldb_msg_add_fmt(msg, "validFSMOs", "%s", dn_str) != LDB_SUCCESS) {
					goto failed;
				}
			}
		}
	}

	if (do_attribute_explicit(attrs, "vendorVersion")) {
		if (ldb_msg_add_fmt(msg, "vendorVersion",
				    "%s", SAMBA_VERSION_STRING) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "domainFunctionality")) {
		if (samdb_msg_add_int(ldb, msg, msg, "domainFunctionality",
				      dsdb_functional_level(ldb)) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "forestFunctionality")) {
		if (samdb_msg_add_int(ldb, msg, msg, "forestFunctionality",
				      dsdb_forest_functional_level(ldb)) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "domainControllerFunctionality")
	    && (val = talloc_get_type(ldb_get_opaque(ldb, "domainControllerFunctionality"), int))) {
		if (samdb_msg_add_int(ldb, msg, msg,
				      "domainControllerFunctionality",
				      *val) != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute(attrs, "isGlobalCatalogReady")) {
		/* MS-ADTS 3.1.1.3.2.10
		   Note, we should only return true here is we have
		   completed at least one synchronisation. As both
		   provision and vampire do a full sync, this means we
		   can return true is the gc bit is set in the NTDSDSA
		   options */
		if (ldb_msg_add_fmt(msg, "isGlobalCatalogReady",
				    "%s", samdb_is_gc(ldb)?"TRUE":"FALSE") != LDB_SUCCESS) {
			goto failed;
		}
	}

	if (do_attribute_explicit(attrs, "tokenGroups")) {
		unsigned int i;
		/* Obtain the user's session_info */
		struct auth_session_info *session_info
			= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
		if (session_info && session_info->security_token) {
			/* The list of groups this user is in */
			for (i = 0; i < session_info->security_token->num_sids; i++) {
				if (samdb_msg_add_dom_sid(ldb, msg, msg,
							  "tokenGroups",
							  &session_info->security_token->sids[i]) != LDB_SUCCESS) {
					goto failed;
				}
			}
		}
	}

	/* TODO: lots more dynamic attributes should be added here */

	edn_control = ldb_request_get_control(req, LDB_CONTROL_EXTENDED_DN_OID);

	/* if the client sent us the EXTENDED_DN control then we need
	   to expand the DNs to have GUID and SID. W2K8 join relies on
	   this */
	if (edn_control) {
		unsigned int i;
		int ret;
		for (i=0; dn_attrs[i]; i++) {
			if (!do_attribute(attrs, dn_attrs[i])) continue;
			ret = expand_dn_in_message(module, msg, dn_attrs[i],
						   edn_control, req);
			if (ret != LDB_SUCCESS) {
				DEBUG(0,(__location__ ": Failed to expand DN in rootDSE for %s\n",
					 dn_attrs[i]));
				goto failed;
			}
		}
	}

	return LDB_SUCCESS;

failed:
	return ldb_operr(ldb);
}

/*
  handle search requests
*/

struct rootdse_context {
	struct ldb_module *module;
	struct ldb_request *req;
};

static struct rootdse_context *rootdse_init_context(struct ldb_module *module,
						    struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct rootdse_context *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct rootdse_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

static int rootdse_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct rootdse_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct rootdse_context);

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
		/*
		 * if the client explicit asks for the 'netlogon' attribute
		 * the reply_entry needs to be skipped
		 */
		if (ac->req->op.search.attrs &&
		    ldb_attr_in_list(ac->req->op.search.attrs, "netlogon")) {
			talloc_free(ares);
			return LDB_SUCCESS;
		}

		/* for each record returned post-process to add any dynamic
		   attributes that have been asked for */
		ret = rootdse_add_dynamic(ac->module, ares->message,
					  ac->req->op.search.attrs, ac->req);
		if (ret != LDB_SUCCESS) {
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		return ldb_module_send_entry(ac->req, ares->message, ares->controls);

	case LDB_REPLY_REFERRAL:
		/* should we allow the backend to return referrals in this case
		 * ?? */
		break;

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

/*
  filter from controls from clients in several ways

  1) mark our registered controls as non-critical in the request

    This is needed as clients may mark controls as critical even if
    they are not needed at all in a request. For example, the centrify
    client sets the SD_FLAGS control as critical on ldap modify
    requests which are setting the dNSHostName attribute on the
    machine account. That request doesn't need SD_FLAGS at all, but
    centrify adds it on all ldap requests.

  2) if this request is untrusted then remove any non-registered
     controls that are non-critical

    This is used on ldap:// connections to prevent remote users from
    setting an internal control that may be dangerous

  3) if this request is untrusted then fail any request that includes
     a critical non-registered control
 */
static int rootdse_filter_controls(struct ldb_module *module, struct ldb_request *req)
{
	unsigned int i, j;
	struct private_data *priv = talloc_get_type(ldb_module_get_private(module), struct private_data);
	bool is_untrusted;

	if (!req->controls) {
		return LDB_SUCCESS;
	}

	is_untrusted = ldb_req_is_untrusted(req);

	for (i=0; req->controls[i]; i++) {
		bool is_registered = false;
		bool is_critical = (req->controls[i]->critical != 0);

		if (req->controls[i]->oid == NULL) {
			continue;
		}

		if (is_untrusted || is_critical) {
			for (j=0; j<priv->num_controls; j++) {
				if (strcasecmp(priv->controls[j], req->controls[i]->oid) == 0) {
					is_registered = true;
					break;
				}
			}
		}

		if (is_untrusted && !is_registered) {
			if (!is_critical) {
				/* remove it by marking the oid NULL */
				req->controls[i]->oid = NULL;
				req->controls[i]->data = NULL;
				req->controls[i]->critical = 0;
				continue;
			}
			/* its a critical unregistered control - give
			   an error */
			ldb_asprintf_errstring(ldb_module_get_ctx(module),
					       "Attempt to use critical non-registered control '%s'",
					       req->controls[i]->oid);
			return LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
		}

		if (!is_critical) {
			continue;
		}

		if (is_registered) {
			req->controls[i]->critical = 0;
		}
	}

	return LDB_SUCCESS;
}

/* Ensure that anonymous users are not allowed to make anything other than rootDSE search operations */

static int rootdse_filter_operations(struct ldb_module *module, struct ldb_request *req)
{
	struct auth_session_info *session_info;
	struct private_data *priv = talloc_get_type(ldb_module_get_private(module), struct private_data);
	bool is_untrusted = ldb_req_is_untrusted(req);
	bool is_anonymous = true;
	if (is_untrusted == false) {
		return LDB_SUCCESS;
	}

	session_info = (struct auth_session_info *)ldb_get_opaque(ldb_module_get_ctx(module), "sessionInfo");
	if (session_info) {
		is_anonymous = security_token_is_anonymous(session_info->security_token);
	}
	
	if (is_anonymous == false || (priv && priv->block_anonymous == false)) {
		return LDB_SUCCESS;
	}
	
	if (req->operation == LDB_SEARCH) {
		if (req->op.search.scope == LDB_SCOPE_BASE && ldb_dn_is_null(req->op.search.base)) {
			return LDB_SUCCESS;
		}
	}
	ldb_set_errstring(ldb_module_get_ctx(module), "Operation unavailable without authentication");
	return LDB_ERR_OPERATIONS_ERROR;
}

static int rootdse_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct rootdse_context *ac;
	struct ldb_request *down_req;
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ldb = ldb_module_get_ctx(module);

	/* see if its for the rootDSE - only a base search on the "" DN qualifies */
	if (!(req->op.search.scope == LDB_SCOPE_BASE && ldb_dn_is_null(req->op.search.base))) {
		/* Otherwise, pass down to the rest of the stack */
		return ldb_next_request(module, req);
	}

	ac = rootdse_init_context(module, req);
	if (ac == NULL) {
		return ldb_operr(ldb);
	}

	/* in our db we store the rootDSE with a DN of @ROOTDSE */
	ret = ldb_build_search_req(&down_req, ldb, ac,
					ldb_dn_new(ac, ldb, "@ROOTDSE"),
					LDB_SCOPE_BASE,
					NULL,
					req->op.search.attrs,
					NULL,/* for now skip the controls from the client */
					ac, rootdse_callback,
					req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, down_req);
}

static int rootdse_register_control(struct ldb_module *module, struct ldb_request *req)
{
	struct private_data *priv = talloc_get_type(ldb_module_get_private(module), struct private_data);
	char **list;

	list = talloc_realloc(priv, priv->controls, char *, priv->num_controls + 1);
	if (!list) {
		return ldb_oom(ldb_module_get_ctx(module));
	}

	list[priv->num_controls] = talloc_strdup(list, req->op.reg_control.oid);
	if (!list[priv->num_controls]) {
		return ldb_oom(ldb_module_get_ctx(module));
	}

	priv->num_controls += 1;
	priv->controls = list;

	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);
}

static int rootdse_register_partition(struct ldb_module *module, struct ldb_request *req)
{
	struct private_data *priv = talloc_get_type(ldb_module_get_private(module), struct private_data);
	struct ldb_dn **list;

	list = talloc_realloc(priv, priv->partitions, struct ldb_dn *, priv->num_partitions + 1);
	if (!list) {
		return ldb_oom(ldb_module_get_ctx(module));
	}

	list[priv->num_partitions] = ldb_dn_copy(list, req->op.reg_partition.dn);
	if (!list[priv->num_partitions]) {
		return ldb_operr(ldb_module_get_ctx(module));
	}

	priv->num_partitions += 1;
	priv->partitions = list;

	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);
}


static int rootdse_request(struct ldb_module *module, struct ldb_request *req)
{
	switch (req->operation) {

	case LDB_REQ_REGISTER_CONTROL:
		return rootdse_register_control(module, req);
	case LDB_REQ_REGISTER_PARTITION:
		return rootdse_register_partition(module, req);

	default:
		break;
	}
	return ldb_next_request(module, req);
}

static int rootdse_init(struct ldb_module *module)
{
	int ret;
	struct ldb_context *ldb;
	struct ldb_result *res;
	struct private_data *data;
	const char *attrs[] = { "msDS-Behavior-Version", NULL };
	const char *ds_attrs[] = { "dsServiceName", NULL };
	TALLOC_CTX *mem_ctx;

	ldb = ldb_module_get_ctx(module);

	data = talloc_zero(module, struct private_data);
	if (data == NULL) {
		return ldb_oom(ldb);
	}

	data->num_controls = 0;
	data->controls = NULL;
	data->num_partitions = 0;
	data->partitions = NULL;
	data->block_anonymous = true;

	ldb_module_set_private(module, data);

	ldb_set_default_dns(ldb);

	ret = ldb_next_init(module);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	mem_ctx = talloc_new(data);
	if (!mem_ctx) {
		return ldb_oom(ldb);
	}

	/* Now that the partitions are set up, do a search for:
	   - domainControllerFunctionality
	   - domainFunctionality
	   - forestFunctionality

	   Then stuff these values into an opaque
	*/
	ret = dsdb_module_search(module, mem_ctx, &res,
				 ldb_get_default_basedn(ldb),
				 LDB_SCOPE_BASE, attrs, DSDB_FLAG_NEXT_MODULE, NULL, NULL);
	if (ret == LDB_SUCCESS && res->count == 1) {
		int domain_behaviour_version
			= ldb_msg_find_attr_as_int(res->msgs[0],
						   "msDS-Behavior-Version", -1);
		if (domain_behaviour_version != -1) {
			int *val = talloc(ldb, int);
			if (!val) {
				talloc_free(mem_ctx);
				return ldb_oom(ldb);
			}
			*val = domain_behaviour_version;
			ret = ldb_set_opaque(ldb, "domainFunctionality", val);
			if (ret != LDB_SUCCESS) {
				talloc_free(mem_ctx);
				return ret;
			}
		}
	}

	ret = dsdb_module_search(module, mem_ctx, &res,
				 samdb_partitions_dn(ldb, mem_ctx),
				 LDB_SCOPE_BASE, attrs, DSDB_FLAG_NEXT_MODULE, NULL, NULL);
	if (ret == LDB_SUCCESS && res->count == 1) {
		int forest_behaviour_version
			= ldb_msg_find_attr_as_int(res->msgs[0],
						   "msDS-Behavior-Version", -1);
		if (forest_behaviour_version != -1) {
			int *val = talloc(ldb, int);
			if (!val) {
				talloc_free(mem_ctx);
				return ldb_oom(ldb);
			}
			*val = forest_behaviour_version;
			ret = ldb_set_opaque(ldb, "forestFunctionality", val);
			if (ret != LDB_SUCCESS) {
				talloc_free(mem_ctx);
				return ret;
			}
		}
	}

	/* For now, our own server's location in the DB is recorded in
	 * the @ROOTDSE record */
	ret = dsdb_module_search(module, mem_ctx, &res,
				 ldb_dn_new(mem_ctx, ldb, "@ROOTDSE"),
				 LDB_SCOPE_BASE, ds_attrs, DSDB_FLAG_NEXT_MODULE, NULL, NULL);
	if (ret == LDB_SUCCESS && res->count == 1) {
		struct ldb_dn *ds_dn
			= ldb_msg_find_attr_as_dn(ldb, mem_ctx, res->msgs[0],
						  "dsServiceName");
		if (ds_dn) {
			ret = dsdb_module_search(module, mem_ctx, &res, ds_dn,
						 LDB_SCOPE_BASE, attrs, DSDB_FLAG_NEXT_MODULE, NULL, NULL);
			if (ret == LDB_SUCCESS && res->count == 1) {
				int domain_controller_behaviour_version
					= ldb_msg_find_attr_as_int(res->msgs[0],
								   "msDS-Behavior-Version", -1);
				if (domain_controller_behaviour_version != -1) {
					int *val = talloc(ldb, int);
					if (!val) {
						talloc_free(mem_ctx);
						return ldb_oom(ldb);
					}
					*val = domain_controller_behaviour_version;
					ret = ldb_set_opaque(ldb,
							     "domainControllerFunctionality", val);
					if (ret != LDB_SUCCESS) {
						talloc_free(mem_ctx);
						return ret;
					}
				}
			}
		}
	}

	data->block_anonymous = dsdb_block_anonymous_ops(module, NULL);

	talloc_free(mem_ctx);

	return LDB_SUCCESS;
}

/*
 * This function gets the string SCOPE_DN:OPTIONAL_FEATURE_GUID and parse it
 * to a DN and a GUID object
 */
static int get_optional_feature_dn_guid(struct ldb_request *req, struct ldb_context *ldb,
						TALLOC_CTX *mem_ctx,
						struct ldb_dn **op_feature_scope_dn,
						struct GUID *op_feature_guid)
{
	const struct ldb_message *msg = req->op.mod.message;
	const char *ldb_val_str;
	char *dn, *guid;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NTSTATUS status;

	ldb_val_str = ldb_msg_find_attr_as_string(msg, "enableOptionalFeature", NULL);
	if (!ldb_val_str) {
		ldb_set_errstring(ldb,
				  "rootdse: unable to find 'enableOptionalFeature'!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	guid = strchr(ldb_val_str, ':');
	if (!guid) {
		ldb_set_errstring(ldb,
				  "rootdse: unable to find GUID in 'enableOptionalFeature'!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	status = GUID_from_string(guid+1, op_feature_guid);
	if (!NT_STATUS_IS_OK(status)) {
		ldb_set_errstring(ldb,
				  "rootdse: bad GUID in 'enableOptionalFeature'!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	dn = talloc_strndup(tmp_ctx, ldb_val_str, guid-ldb_val_str);
	if (!dn) {
		ldb_set_errstring(ldb,
				  "rootdse: bad DN in 'enableOptionalFeature'!");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	*op_feature_scope_dn = ldb_dn_new(mem_ctx, ldb, dn);

	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

/*
 * This function gets the OPTIONAL_FEATURE_GUID and looks for the optional feature
 * ldb_message object.
 */
static int dsdb_find_optional_feature(struct ldb_module *module, struct ldb_context *ldb,
				      TALLOC_CTX *mem_ctx, struct GUID op_feature_guid, struct ldb_message **msg,
				      struct ldb_request *parent)
{
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	int ret;

	ret = dsdb_module_search(module, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
				 NULL,
				 DSDB_FLAG_NEXT_MODULE |
				 DSDB_SEARCH_SEARCH_ALL_PARTITIONS,
				 parent,
				 "(&(objectClass=msDS-OptionalFeature)"
				 "(msDS-OptionalFeatureGUID=%s))",GUID_string(tmp_ctx, &op_feature_guid));

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count == 0) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	if (res->count != 1) {
		ldb_asprintf_errstring(ldb,
				       "More than one object found matching optional feature GUID %s\n",
				       GUID_string(tmp_ctx, &op_feature_guid));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*msg = talloc_steal(mem_ctx, res->msgs[0]);

	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

static int rootdse_enable_recycle_bin(struct ldb_module *module,struct ldb_context *ldb,
				      TALLOC_CTX *mem_ctx, struct ldb_dn *op_feature_scope_dn,
				      struct ldb_message *op_feature_msg, struct ldb_request *parent)
{
	int ret;
	const int domain_func_level = dsdb_functional_level(ldb);
	struct ldb_dn *ntds_settings_dn;
	TALLOC_CTX *tmp_ctx;
	unsigned int el_count = 0;
	struct ldb_message *msg;

	ret = ldb_msg_find_attr_as_int(op_feature_msg, "msDS-RequiredForestBehaviorVersion", 0);
	if (domain_func_level < ret){
		ldb_asprintf_errstring(ldb,
				       "rootdse_enable_recycle_bin: Domain functional level must be at least %d\n",
				       ret);
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	tmp_ctx = talloc_new(mem_ctx);
	ntds_settings_dn = samdb_ntds_settings_dn(ldb);
	if (!ntds_settings_dn) {
		DEBUG(0, (__location__ ": Failed to find NTDS settings DN\n"));
		ret = LDB_ERR_OPERATIONS_ERROR;
		talloc_free(tmp_ctx);
		return ret;
	}

	ntds_settings_dn = ldb_dn_copy(tmp_ctx, ntds_settings_dn);
	if (!ntds_settings_dn) {
		DEBUG(0, (__location__ ": Failed to copy NTDS settings DN\n"));
		ret = LDB_ERR_OPERATIONS_ERROR;
		talloc_free(tmp_ctx);
		return ret;
	}

	msg = ldb_msg_new(tmp_ctx);
	msg->dn = ntds_settings_dn;

	ldb_msg_add_linearized_dn(msg, "msDS-EnabledFeature", op_feature_msg->dn);
	msg->elements[el_count++].flags = LDB_FLAG_MOD_ADD;

	ret = dsdb_module_modify(module, msg, DSDB_FLAG_NEXT_MODULE, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       "rootdse_enable_recycle_bin: Failed to modify object %s - %s",
				       ldb_dn_get_linearized(ntds_settings_dn),
				       ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	msg->dn = op_feature_scope_dn;
	ret = dsdb_module_modify(module, msg, DSDB_FLAG_NEXT_MODULE, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       "rootdse_enable_recycle_bin: Failed to modify object %s - %s",
				       ldb_dn_get_linearized(op_feature_scope_dn),
				       ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	return LDB_SUCCESS;
}

static int rootdse_enableoptionalfeature(struct ldb_module *module, struct ldb_request *req)
{
	/*
	  steps:
	       - check for system (only system can enable features)
	       - extract GUID from the request
	       - find the feature object
	       - check functional level, must be at least msDS-RequiredForestBehaviorVersion
	       - check if it is already enabled (if enabled return LDAP_ATTRIBUTE_OR_VALUE_EXISTS) - probably not needed, just return error from the add/modify
	       - add/modify objects (see ntdsconnection code for an example)
	 */

	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct GUID op_feature_guid;
	struct ldb_dn *op_feature_scope_dn;
	struct ldb_message *op_feature_msg;
	struct auth_session_info *session_info =
				(struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	int ret;
	const char *guid_string;

	if (security_session_user_level(session_info, NULL) != SECURITY_SYSTEM) {
		ldb_set_errstring(ldb, "rootdse: Insufficient rights for enableoptionalfeature");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	ret = get_optional_feature_dn_guid(req, ldb, tmp_ctx, &op_feature_scope_dn, &op_feature_guid);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	guid_string = GUID_string(tmp_ctx, &op_feature_guid);
	if (!guid_string) {
		ldb_set_errstring(ldb, "rootdse: bad optional feature GUID");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	ret = dsdb_find_optional_feature(module, ldb, tmp_ctx, op_feature_guid, &op_feature_msg, req);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       "rootdse: unable to find optional feature for %s - %s",
				       guid_string, ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	if (strcasecmp(DS_GUID_FEATURE_RECYCLE_BIN, guid_string) == 0) {
			ret = rootdse_enable_recycle_bin(module, ldb,
							 tmp_ctx, op_feature_scope_dn,
							 op_feature_msg, req);
	} else {
		ldb_asprintf_errstring(ldb,
				       "rootdse: unknown optional feature %s",
				       guid_string);
		talloc_free(tmp_ctx);
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       "rootdse: failed to set optional feature for %s - %s",
				       guid_string, ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	talloc_free(tmp_ctx);
	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);;
}

static int rootdse_schemaupdatenow(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_result *ext_res;
	int ret;
	struct ldb_dn *schema_dn;

	schema_dn = ldb_get_schema_basedn(ldb);
	if (!schema_dn) {
		ldb_reset_err_string(ldb);
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "rootdse_modify: no schema dn present: (skip ldb_extended call)\n");
		return ldb_next_request(module, req);
	}

	ret = ldb_extended(ldb, DSDB_EXTENDED_SCHEMA_UPDATE_NOW_OID, schema_dn, &ext_res);
	if (ret != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}

	talloc_free(ext_res);
	return ldb_module_done(req, NULL, NULL, ret);
}

static int rootdse_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
		If dn is not "" we should let it pass through
	*/
	if (!ldb_dn_is_null(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb_set_errstring(ldb, "rootdse_add: you cannot add a new rootdse entry!");
	return LDB_ERR_NAMING_VIOLATION;
}

static int rootdse_become_master(struct ldb_module *module,
				 struct ldb_request *req,
				 enum drepl_role_master role)
{
	struct drepl_takeFSMORole r;
	struct messaging_context *msg;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	TALLOC_CTX *tmp_ctx = talloc_new(req);
	struct loadparm_context *lp_ctx = ldb_get_opaque(ldb, "loadparm");
	NTSTATUS status_call;
	WERROR status_fn;
	bool am_rodc;
	struct dcerpc_binding_handle *irpc_handle;
	int ret;

	ret = samdb_rodc(ldb, &am_rodc);
	if (ret != LDB_SUCCESS) {
		return ldb_error(ldb, ret, "Could not determine if server is RODC.");
	}

	if (am_rodc) {
		return ldb_error(ldb, LDB_ERR_UNWILLING_TO_PERFORM,
				 "RODC cannot become a role master.");
	}

	msg = messaging_client_init(tmp_ctx, lpcfg_messaging_path(tmp_ctx, lp_ctx),
				    ldb_get_event_context(ldb));
	if (!msg) {
		ldb_asprintf_errstring(ldb, "Failed to generate client messaging context in %s", lpcfg_messaging_path(tmp_ctx, lp_ctx));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	irpc_handle = irpc_binding_handle_by_name(tmp_ctx, msg,
						  "dreplsrv",
						  &ndr_table_irpc);
	if (irpc_handle == NULL) {
		return ldb_oom(ldb);
	}
	r.in.role = role;

	status_call = dcerpc_drepl_takeFSMORole_r(irpc_handle, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status_call)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	status_fn = r.out.result;
	if (!W_ERROR_IS_OK(status_fn)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);
}

static int rootdse_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
		If dn is not "" we should let it pass through
	*/
	if (!ldb_dn_is_null(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	/*
		dn is empty so check for schemaUpdateNow attribute
		"The type of modification and values specified in the LDAP modify operation do not matter." MSDN
	*/
	if (ldb_msg_find_element(req->op.mod.message, "schemaUpdateNow")) {
		return rootdse_schemaupdatenow(module, req);
	}
	if (ldb_msg_find_element(req->op.mod.message, "becomeDomainMaster")) {
		return rootdse_become_master(module, req, DREPL_NAMING_MASTER);
	}
	if (ldb_msg_find_element(req->op.mod.message, "becomeInfrastructureMaster")) {
		return rootdse_become_master(module, req, DREPL_INFRASTRUCTURE_MASTER);
	}
	if (ldb_msg_find_element(req->op.mod.message, "becomeRidMaster")) {
		return rootdse_become_master(module, req, DREPL_RID_MASTER);
	}
	if (ldb_msg_find_element(req->op.mod.message, "becomeSchemaMaster")) {
		return rootdse_become_master(module, req, DREPL_SCHEMA_MASTER);
	}
	if (ldb_msg_find_element(req->op.mod.message, "becomePdc")) {
		return rootdse_become_master(module, req, DREPL_PDC_MASTER);
	}
	if (ldb_msg_find_element(req->op.mod.message, "enableOptionalFeature")) {
		return rootdse_enableoptionalfeature(module, req);
	}

	ldb_set_errstring(ldb, "rootdse_modify: unknown attribute to change!");
	return LDB_ERR_UNWILLING_TO_PERFORM;
}

static int rootdse_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
		If dn is not "" we should let it pass through
	*/
	if (!ldb_dn_is_null(req->op.rename.olddn)) {
		return ldb_next_request(module, req);
	}

	ldb_set_errstring(ldb, "rootdse_remove: you cannot rename the rootdse entry!");
	return LDB_ERR_NO_SUCH_OBJECT;
}

static int rootdse_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
		If dn is not "" we should let it pass through
	*/
	if (!ldb_dn_is_null(req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	ldb_set_errstring(ldb, "rootdse_remove: you cannot delete the rootdse entry!");
	return LDB_ERR_NO_SUCH_OBJECT;
}

static int rootdse_extended(struct ldb_module *module, struct ldb_request *req)
{
	int ret;

	ret = rootdse_filter_operations(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = rootdse_filter_controls(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, req);
}

static const struct ldb_module_ops ldb_rootdse_module_ops = {
	.name		= "rootdse",
	.init_context   = rootdse_init,
	.search         = rootdse_search,
	.request	= rootdse_request,
	.add		= rootdse_add,
	.modify         = rootdse_modify,
	.rename         = rootdse_rename,
	.extended       = rootdse_extended,
	.del		= rootdse_delete
};

int ldb_rootdse_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_rootdse_module_ops);
}
