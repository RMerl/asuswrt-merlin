/* 
   samdb proxy module

   Copyright (C) Andrew Tridgell 2005

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
  ldb proxy module. At startup this looks for a record like this:

   dn=@PROXYINFO
   url=destination url
   olddn = basedn to proxy in upstream server
   newdn = basedn in local server
   username = username to connect to upstream
   password = password for upstream

   NOTE: this module is a complete hack at this stage. I am committing it just
   so others can know how I am investigating mmc support
   
 */

#include "includes.h"
#include "ldb_module.h"
#include "auth/credentials/credentials.h"

struct proxy_data {
	struct ldb_context *upstream;
	struct ldb_dn *olddn;
	struct ldb_dn *newdn;
	const char **oldstr;
	const char **newstr;
};

struct proxy_ctx {
	struct ldb_module *module;
	struct ldb_request *req;

#ifdef DEBUG_PROXY
	int count;
#endif
};

/*
  load the @PROXYINFO record
*/
static int load_proxy_info(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct proxy_data *proxy = talloc_get_type(ldb_module_get_private(module), struct proxy_data);
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	int ret;
	const char *olddn, *newdn, *url, *username, *password, *oldstr, *newstr;
	struct cli_credentials *creds;

	/* see if we have already loaded it */
	if (proxy->upstream != NULL) {
		return LDB_SUCCESS;
	}

	dn = ldb_dn_new(proxy, ldb, "@PROXYINFO");
	if (dn == NULL) {
		goto failed;
	}
	ret = ldb_search(ldb, proxy, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	talloc_free(dn);
	if (ret != LDB_SUCCESS || res->count != 1) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Can't find @PROXYINFO\n");
		goto failed;
	}

	url      = ldb_msg_find_attr_as_string(res->msgs[0], "url", NULL);
	olddn    = ldb_msg_find_attr_as_string(res->msgs[0], "olddn", NULL);
	newdn    = ldb_msg_find_attr_as_string(res->msgs[0], "newdn", NULL);
	username = ldb_msg_find_attr_as_string(res->msgs[0], "username", NULL);
	password = ldb_msg_find_attr_as_string(res->msgs[0], "password", NULL);
	oldstr   = ldb_msg_find_attr_as_string(res->msgs[0], "oldstr", NULL);
	newstr   = ldb_msg_find_attr_as_string(res->msgs[0], "newstr", NULL);

	if (url == NULL || olddn == NULL || newdn == NULL || username == NULL || password == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Need url, olddn, newdn, oldstr, newstr, username and password in @PROXYINFO\n");
		goto failed;
	}

	proxy->olddn = ldb_dn_new(proxy, ldb, olddn);
	if (proxy->olddn == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Failed to explode olddn '%s'\n", olddn);
		goto failed;
	}
	
	proxy->newdn = ldb_dn_new(proxy, ldb, newdn);
	if (proxy->newdn == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Failed to explode newdn '%s'\n", newdn);
		goto failed;
	}

	proxy->upstream = ldb_init(proxy, ldb_get_event_context(ldb));
	if (proxy->upstream == NULL) {
		ldb_oom(ldb);
		goto failed;
	}

	proxy->oldstr = str_list_make(proxy, oldstr, ", ");
	if (proxy->oldstr == NULL) {
		ldb_oom(ldb);
		goto failed;
	}

	proxy->newstr = str_list_make(proxy, newstr, ", ");
	if (proxy->newstr == NULL) {
		ldb_oom(ldb);
		goto failed;
	}

	/* setup credentials for connection */
	creds = cli_credentials_init(proxy->upstream);
	if (creds == NULL) {
		ldb_oom(ldb);
		goto failed;
	}
	cli_credentials_guess(creds, ldb_get_opaque(ldb, "loadparm"));
	cli_credentials_set_username(creds, username, CRED_SPECIFIED);
	cli_credentials_set_password(creds, password, CRED_SPECIFIED);

	ldb_set_opaque(proxy->upstream, "credentials", creds);

	ret = ldb_connect(proxy->upstream, url, 0, NULL);
	if (ret != 0) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "proxy failed to connect to %s\n", url);
		goto failed;
	}

	ldb_debug(ldb, LDB_DEBUG_TRACE, "proxy connected to %s\n", url);

	talloc_free(res);

	return LDB_SUCCESS;

failed:
	talloc_free(res);
	talloc_free(proxy->olddn);
	talloc_free(proxy->newdn);
	talloc_free(proxy->upstream);
	proxy->upstream = NULL;
	return ldb_operr(ldb);
}


/*
  convert a binary blob
*/
static void proxy_convert_blob(TALLOC_CTX *mem_ctx, struct ldb_val *v,
			       const char *oldstr, const char *newstr)
{
	size_t len1, len2, len3;
	uint8_t *olddata = v->data;
	char *p = strcasestr((char *)v->data, oldstr);

	len1 = (p - (char *)v->data);
	len2 = strlen(newstr);
	len3 = v->length - (p+strlen(oldstr) - (char *)v->data);
	v->length = len1+len2+len3;
	v->data = talloc_size(mem_ctx, v->length);
	memcpy(v->data, olddata, len1);
	memcpy(v->data+len1, newstr, len2);
	memcpy(v->data+len1+len2, olddata + len1 + strlen(oldstr), len3);
}

/*
  convert a returned value
*/
static void proxy_convert_value(struct proxy_data *proxy, struct ldb_message *msg, struct ldb_val *v)
{
	size_t i;

	for (i=0;proxy->oldstr[i];i++) {
		char *p = strcasestr((char *)v->data, proxy->oldstr[i]);
		if (p == NULL) continue;
		proxy_convert_blob(msg, v, proxy->oldstr[i], proxy->newstr[i]);
	}
}


/*
  convert a returned value
*/
static struct ldb_parse_tree *proxy_convert_tree(TALLOC_CTX *mem_ctx,
						 struct proxy_data *proxy,
						 struct ldb_parse_tree *tree)
{
	size_t i;
	char *expression = ldb_filter_from_tree(mem_ctx, tree);

	for (i=0;proxy->newstr[i];i++) {
		struct ldb_val v;
		char *p = strcasestr(expression, proxy->newstr[i]);
		if (p == NULL) continue;
		v.data = (uint8_t *)expression;
		v.length = strlen(expression)+1;
		proxy_convert_blob(mem_ctx, &v, proxy->newstr[i], proxy->oldstr[i]);
		return ldb_parse_tree(mem_ctx, (const char *)v.data);
	}
	return tree;
}



/*
  convert a returned record
*/
static void proxy_convert_record(struct ldb_context *ldb,
				 struct proxy_data *proxy,
				 struct ldb_message *msg)
{
	unsigned int attr, v;

	/* fix the message DN */
	if (ldb_dn_compare_base(proxy->olddn, msg->dn) == 0) {
		ldb_dn_remove_base_components(msg->dn, ldb_dn_get_comp_num(proxy->olddn));
		ldb_dn_add_base(msg->dn, proxy->newdn);
	}

	/* fix any attributes */
	for (attr=0;attr<msg->num_elements;attr++) {
		for (v=0;v<msg->elements[attr].num_values;v++) {
			proxy_convert_value(proxy, msg, &msg->elements[attr].values[v]);
		}
	}

	/* fix any DN components */
	for (attr=0;attr<msg->num_elements;attr++) {
		for (v=0;v<msg->elements[attr].num_values;v++) {
			proxy_convert_value(proxy, msg, &msg->elements[attr].values[v]);
		}
	}
}

static int proxy_search_callback(struct ldb_request *req,
				  struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct proxy_data *proxy;
	struct proxy_ctx *ac;
	int ret;

	ac = talloc_get_type(req->context, struct proxy_ctx);
	ldb = ldb_module_get_ctx(ac->module);
	proxy = talloc_get_type(ldb_module_get_private(module), struct proxy_data);

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

#ifdef DEBUG_PROXY
		ac->count++;
#endif
		proxy_convert_record(ldb, proxy, ares->message);
		ret = ldb_module_send_entry(ac->req, ares->message, ares->controls);
		break;

	case LDB_REPLY_REFERRAL:

		/* ignore remote referrals */
		break;

	case LDB_REPLY_DONE:

#ifdef DEBUG_PROXY
		printf("# record %d\n", ac->count+1);
#endif

		return ldb_module_done(ac->req, NULL, NULL, LDB_SUCCESS);
	}

	talloc_free(ares);
	return ret;
}

/* search */
static int proxy_search_bytree(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct proxy_ctx *ac;
	struct ldb_parse_tree *newtree;
	struct proxy_data *proxy = talloc_get_type(ldb_module_get_private(module), struct proxy_data);
	struct ldb_request *newreq;
	struct ldb_dn *base;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(module);

	if (req->op.search.base == NULL ||
		(req->op.search.base->comp_num == 1 &&
			req->op.search.base->components[0].name[0] == '@')) {
		goto passthru;
	}

	if (load_proxy_info(module) != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}

	/* see if the dn is within olddn */
	if (ldb_dn_compare_base(proxy->newdn, req->op.search.base) != 0) {
		goto passthru;
	}

	ac = talloc(req, struct proxy_ctx);
	if (ac == NULL) {
		return ldb_oom(ldb);
	}

	ac->module = module;
	ac->req = req;
#ifdef DEBUG_PROXY
	ac->count = 0;
#endif

	newtree = proxy_convert_tree(ac, proxy, req->op.search.tree);

	/* convert the basedn of this search */
	base = ldb_dn_copy(ac, req->op.search.base);
	if (base == NULL) {
		goto failed;
	}
	ldb_dn_remove_base_components(base, ldb_dn_get_comp_num(proxy->newdn));
	ldb_dn_add_base(base, proxy->olddn);

	ldb_debug(ldb, LDB_DEBUG_FATAL, "proxying: '%s' with dn '%s' \n", 
		  ldb_filter_from_tree(ac, newreq->op.search.tree), ldb_dn_get_linearized(newreq->op.search.base));
	for (i = 0; req->op.search.attrs && req->op.search.attrs[i]; i++) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "attr: '%s'\n", req->op.search.attrs[i]);
	}

	ret = ldb_build_search_req_ex(&newreq, ldb, ac,
				      base, req->op.search.scope,
				      newtree, req->op.search.attrs,
				      req->controls,
				      ac, proxy_search_callback,
				      req);
	LDB_REQ_SET_LOCATION(newreq);
	/* FIXME: warning, need a real event system hooked up for this to work properly,
	 * 	  for now this makes the module *not* ASYNC */
	ret = ldb_request(proxy->upstream, newreq);
	if (ret != LDB_SUCCESS) {
		ldb_set_errstring(ldb, ldb_errstring(proxy->upstream));
	}
	ret = ldb_wait(newreq->handle, LDB_WAIT_ALL);
	if (ret != LDB_SUCCESS) {
		ldb_set_errstring(ldb, ldb_errstring(proxy->upstream));
	}
	return ret;

failed:
	ldb_debug(ldb, LDB_DEBUG_TRACE, "proxy failed for %s\n", 
		  ldb_dn_get_linearized(req->op.search.base));

passthru:
	return ldb_next_request(module, req); 
}

static int proxy_request(struct ldb_module *module, struct ldb_request *req)
{
	switch (req->operation) {

	case LDB_REQ_SEARCH:
		return proxy_search_bytree(module, req);

	default:
		return ldb_next_request(module, req);

	}
}

static const struct ldb_module_ops ldb_proxy_module_ops = {
	.name		= "proxy",
	.request	= proxy_request
};

int ldb_proxy_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_proxy_module_ops);
}
