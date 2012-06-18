/* 
   Unix SMB/CIFS mplementation.
   LDAP protocol helper functions for SAMBA
   
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Simo Sorce 2004
   Copyright (C) Matthias Dieter Wallnöfer 2009
    
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
#include "libcli/ldap/ldap_client.h"
#include "lib/cmdline/popt_common.h"

#include "torture/torture.h"
#include "torture/ldap/proto.h"

#include "param/param.h"

static bool test_bind_simple(struct ldap_connection *conn, const char *userdn, const char *password)
{
	NTSTATUS status;
	bool ret = true;

	status = torture_ldap_bind(conn, userdn, password);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
	}

	return ret;
}

static bool test_bind_sasl(struct torture_context *tctx,
			   struct ldap_connection *conn, struct cli_credentials *creds)
{
	NTSTATUS status;
	bool ret = true;

	printf("Testing sasl bind as user\n");

	status = torture_ldap_bind_sasl(conn, creds, tctx->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
	}

	return ret;
}

static bool test_multibind(struct ldap_connection *conn, const char *userdn, const char *password)
{
	bool ret = true;

	printf("Testing multiple binds on a single connnection as anonymous and user\n");

	ret = test_bind_simple(conn, NULL, NULL);
	if (!ret) {
		printf("1st bind as anonymous failed\n");
		return ret;
	}

	ret = test_bind_simple(conn, userdn, password);
	if (!ret) {
		printf("2nd bind as authenticated user failed\n");
	}

	return ret;
}

static bool test_search_rootDSE(struct ldap_connection *conn, char **basedn)
{
	bool ret = true;
	struct ldap_message *msg, *result;
	struct ldap_request *req;
	int i;
	struct ldap_SearchResEntry *r;
	NTSTATUS status;

	printf("Testing RootDSE Search\n");

	*basedn = NULL;

	msg = new_ldap_message(conn);
	if (!msg) {
		return false;
	}

	msg->type = LDAP_TAG_SearchRequest;
	msg->r.SearchRequest.basedn = "";
	msg->r.SearchRequest.scope = LDAP_SEARCH_SCOPE_BASE;
	msg->r.SearchRequest.deref = LDAP_DEREFERENCE_NEVER;
	msg->r.SearchRequest.timelimit = 0;
	msg->r.SearchRequest.sizelimit = 0;
	msg->r.SearchRequest.attributesonly = false;
	msg->r.SearchRequest.tree = ldb_parse_tree(msg, "(objectclass=*)");
	msg->r.SearchRequest.num_attributes = 0;
	msg->r.SearchRequest.attributes = NULL;

	req = ldap_request_send(conn, msg);
	if (req == NULL) {
		printf("Could not setup ldap search\n");
		return false;
	}

	status = ldap_result_one(req, &result, LDAP_TAG_SearchResultEntry);
	if (!NT_STATUS_IS_OK(status)) {
		printf("search failed - %s\n", nt_errstr(status));
		return false;
	}

	printf("received %d replies\n", req->num_replies);

	r = &result->r.SearchResultEntry;
		
	DEBUG(1,("\tdn: %s\n", r->dn));
	for (i=0; i<r->num_attributes; i++) {
		int j;
		for (j=0; j<r->attributes[i].num_values; j++) {
			DEBUG(1,("\t%s: %d %.*s\n", r->attributes[i].name,
				 (int)r->attributes[i].values[j].length,
				 (int)r->attributes[i].values[j].length,
				 (char *)r->attributes[i].values[j].data));
			if (!(*basedn) && 
			    strcasecmp("defaultNamingContext",r->attributes[i].name)==0) {
				*basedn = talloc_asprintf(conn, "%.*s",
							  (int)r->attributes[i].values[j].length,
							  (char *)r->attributes[i].values[j].data);
			}
		}
	}

	talloc_free(req);

	return ret;
}

static bool test_compare_sasl(struct ldap_connection *conn, const char *basedn)
{
	struct ldap_message *msg, *rep;
	struct ldap_request *req;
	const char *val;
	NTSTATUS status;

	printf("Testing SASL Compare: %s\n", basedn);

	if (!basedn) {
		return false;
	}

	msg = new_ldap_message(conn);
	if (!msg) {
		return false;
	}

	msg->type = LDAP_TAG_CompareRequest;
	msg->r.CompareRequest.dn = basedn;
	msg->r.CompareRequest.attribute = talloc_strdup(msg, "objectClass");
	val = "domain";
	msg->r.CompareRequest.value = data_blob_talloc(msg, val, strlen(val));

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_CompareResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap compare request - %s\n", nt_errstr(status));
		return false;
	}

	DEBUG(5,("Code: %d DN: [%s] ERROR:[%s] REFERRAL:[%s]\n",
		rep->r.CompareResponse.resultcode,
		rep->r.CompareResponse.dn,
		rep->r.CompareResponse.errormessage,
		rep->r.CompareResponse.referral));

	return true;
}

/*
 * This takes an AD error message and splits it into the WERROR code
 * (WERR_DS_GENERIC if none found) and the reason (remaining string).
 */
static WERROR ad_error(const char *err_msg, char **reason)
{
	WERROR err = W_ERROR(strtol(err_msg, reason, 16));

	if ((reason != NULL) && (*reason[0] != ':')) {
		return WERR_DS_GENERIC_ERROR; /* not an AD std error message */
	}
		
	if (reason != NULL) {
		*reason += 2; /* skip ": " */
	}
	return err;
}

static bool test_error_codes(struct torture_context *tctx,
	struct ldap_connection *conn, const char *basedn)
{
	struct ldap_message *msg, *rep;
	struct ldap_request *req;
	char *err_code_str, *endptr;
	WERROR err;
	NTSTATUS status;

	printf("Testing error codes - to make this test pass against SAMBA 4 you have to specify the target!\n");

	if (!basedn) {
		return false;
	}

	msg = new_ldap_message(conn);
	if (!msg) {
		return false;
	}

	printf(" Try a wrong addition\n");

	msg->type = LDAP_TAG_AddRequest;
	msg->r.AddRequest.dn = basedn;
	msg->r.AddRequest.num_attributes = 0;
	msg->r.AddRequest.attributes = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_AddResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap addition request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.AddResponse.resultcode == 0)
		|| (rep->r.AddResponse.errormessage == NULL)
		|| (strtol(rep->r.AddResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.AddResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if (!torture_setting_bool(tctx, "samba4", false)) {
		if ((!W_ERROR_EQUAL(err, WERR_DS_REFERRAL))
			|| (rep->r.AddResponse.resultcode != 10)) {
			return false;
		}
	} else {
		if ((!W_ERROR_EQUAL(err, WERR_DS_GENERIC_ERROR))
			|| (rep->r.AddResponse.resultcode != 80)) {
			return false;
		}
	}

	printf(" Try a wrong modification\n");

	msg->type = LDAP_TAG_ModifyRequest;
	msg->r.ModifyRequest.dn = "";
	msg->r.ModifyRequest.num_mods = 0;
	msg->r.ModifyRequest.mods = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_ModifyResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap modifification request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.ModifyResponse.resultcode == 0)
		|| (rep->r.ModifyResponse.errormessage == NULL)
		|| (strtol(rep->r.ModifyResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.ModifyResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if (!torture_setting_bool(tctx, "samba4", false)) {
		if ((!W_ERROR_EQUAL(err, WERR_INVALID_PARAM))
			|| (rep->r.ModifyResponse.resultcode != 53)) {
			return false;
		}
	} else {
		if ((!W_ERROR_EQUAL(err, WERR_DS_GENERIC_ERROR))
			|| (rep->r.ModifyResponse.resultcode != 80)) {
			return false;
		}
	}

	printf(" Try a wrong removal\n");

	msg->type = LDAP_TAG_DelRequest;
	msg->r.DelRequest.dn = "";

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_DelResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap removal request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.DelResponse.resultcode == 0)
		|| (rep->r.DelResponse.errormessage == NULL)
		|| (strtol(rep->r.DelResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}
	
	err = ad_error(rep->r.DelResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if (!torture_setting_bool(tctx, "samba4", false)) {
		if ((!W_ERROR_EQUAL(err, WERR_DS_OBJ_NOT_FOUND))
			|| (rep->r.DelResponse.resultcode != 32)) {
			return false;
		}
	} else {
		if ((!W_ERROR_EQUAL(err, WERR_DS_INVALID_DN_SYNTAX))
			|| (rep->r.DelResponse.resultcode != 34)) {
			return false;
		}
	}

	return true;
}

bool torture_ldap_basic(struct torture_context *torture)
{
        NTSTATUS status;
        struct ldap_connection *conn;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	const char *host = torture_setting_string(torture, "host", NULL);
	const char *userdn = torture_setting_string(torture, "ldap_userdn", NULL);
	const char *secret = torture_setting_string(torture, "ldap_secret", NULL);
	char *url;
	char *basedn;

	mem_ctx = talloc_init("torture_ldap_basic");

	url = talloc_asprintf(mem_ctx, "ldap://%s/", host);

	status = torture_ldap_connection(torture, &conn, url);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	if (!test_search_rootDSE(conn, &basedn)) {
		ret = false;
	}

	/* other bind tests here */

	if (!test_multibind(conn, userdn, secret)) {
		ret = false;
	}

	if (!test_bind_sasl(torture, conn, cmdline_credentials)) {
		ret = false;
	}

	if (!test_compare_sasl(conn, basedn)) {
		ret = false;
	}

	/* error codes test here */

	if (!test_error_codes(torture, conn, basedn)) {
		ret = false;
	}

	/* if there are no more tests we are closing */
	torture_ldap_close(conn);
	talloc_free(mem_ctx);

	return ret;
}

