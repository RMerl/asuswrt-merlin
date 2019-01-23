/* 
   Unix SMB/CIFS mplementation.
   LDAP protocol helper functions for SAMBA
   
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Simo Sorce 2004
   Copyright (C) Matthias Dieter Wallnöfer 2009-2010
    
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
#include "ldb_wrap.h"
#include "libcli/ldap/ldap_client.h"
#include "lib/cmdline/popt_common.h"

#include "torture/torture.h"
#include "torture/ldap/proto.h"


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

	printf("Testing multiple binds on a single connection as anonymous and user\n");

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

static bool test_search_rootDSE(struct ldap_connection *conn, const char **basedn,
	const char ***partitions)
{
	bool ret = true;
	struct ldap_message *msg, *result;
	struct ldap_request *req;
	int i;
	struct ldap_SearchResEntry *r;
	NTSTATUS status;

	printf("Testing RootDSE Search\n");

	*basedn = NULL;

	if (partitions != NULL) {
		*partitions = const_str_list(str_list_make_empty(conn));
	}

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
			if ((partitions != NULL) &&
			    (strcasecmp("namingContexts", r->attributes[i].name) == 0)) {
				char *entry = talloc_asprintf(conn, "%.*s",
							      (int)r->attributes[i].values[j].length,
							      (char *)r->attributes[i].values[j].data);
				*partitions = str_list_add(*partitions, entry);
			}
		}
	}

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

/* This has to be done using the LDAP API since the LDB API does only transmit
 * the error code and not the error message. */
static bool test_error_codes(struct torture_context *tctx,
	struct ldap_connection *conn, const char *basedn)
{
	struct ldap_message *msg, *rep;
	struct ldap_request *req;
	const char *err_code_str;
	char *endptr;
	WERROR err;
	NTSTATUS status;

	printf("Testing the most important error code -> error message conversions!\n");

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
		printf("error in ldap add request - %s\n", nt_errstr(status));
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
	if ((!W_ERROR_EQUAL(err, WERR_DS_REFERRAL))
			|| (rep->r.AddResponse.resultcode != LDAP_REFERRAL)) {
			return false;
	}
	if ((rep->r.AddResponse.referral == NULL)
			|| (strstr(rep->r.AddResponse.referral, basedn) == NULL)) {
			return false;
	}

	printf(" Try another wrong addition\n");

	msg->type = LDAP_TAG_AddRequest;
	msg->r.AddRequest.dn = "";
	msg->r.AddRequest.num_attributes = 0;
	msg->r.AddRequest.attributes = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_AddResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap add request - %s\n", nt_errstr(status));
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
	if ((!W_ERROR_EQUAL(err, WERR_DS_ROOT_MUST_BE_NC) &&
	     !W_ERROR_EQUAL(err, WERR_DS_NAMING_VIOLATION))
		|| (rep->r.AddResponse.resultcode != LDAP_NAMING_VIOLATION)) {
		return false;
	}

	printf(" Try a wrong modification\n");

	msg->type = LDAP_TAG_ModifyRequest;
	msg->r.ModifyRequest.dn = basedn;
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
	if ((!W_ERROR_EQUAL(err, WERR_INVALID_PARAM) &&
	     !W_ERROR_EQUAL(err, WERR_DS_UNWILLING_TO_PERFORM))
		|| (rep->r.ModifyResponse.resultcode != LDAP_UNWILLING_TO_PERFORM)) {
		return false;
	}

	printf(" Try another wrong modification\n");

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
	if ((!W_ERROR_EQUAL(err, WERR_INVALID_PARAM) &&
	     !W_ERROR_EQUAL(err, WERR_DS_UNWILLING_TO_PERFORM))
		|| (rep->r.ModifyResponse.resultcode != LDAP_UNWILLING_TO_PERFORM)) {
		return false;
	}

	printf(" Try a wrong removal\n");

	msg->type = LDAP_TAG_DelRequest;
	msg->r.DelRequest.dn = basedn;

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
	if ((!W_ERROR_EQUAL(err, WERR_DS_CANT_DELETE) &&
	     !W_ERROR_EQUAL(err, WERR_DS_UNWILLING_TO_PERFORM))
		|| (rep->r.DelResponse.resultcode != LDAP_UNWILLING_TO_PERFORM)) {
		return false;
	}

	printf(" Try another wrong removal\n");

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
	if ((!W_ERROR_EQUAL(err, WERR_DS_OBJ_NOT_FOUND) &&
	     !W_ERROR_EQUAL(err, WERR_DS_NO_SUCH_OBJECT))
		|| (rep->r.DelResponse.resultcode != LDAP_NO_SUCH_OBJECT)) {
		return false;
	}

	printf(" Try a wrong rename\n");

	msg->type = LDAP_TAG_ModifyDNRequest;
	msg->r.ModifyDNRequest.dn = basedn;
	msg->r.ModifyDNRequest.newrdn = "dc=test";
	msg->r.ModifyDNRequest.deleteolddn = true;
	msg->r.ModifyDNRequest.newsuperior = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_ModifyDNResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap rename request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.ModifyDNResponse.resultcode == 0)
		|| (rep->r.ModifyDNResponse.errormessage == NULL)
		|| (strtol(rep->r.ModifyDNResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.ModifyDNResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if ((!W_ERROR_EQUAL(err, WERR_DS_NO_PARENT_OBJECT) &&
	     !W_ERROR_EQUAL(err, WERR_DS_GENERIC_ERROR))
		|| (rep->r.ModifyDNResponse.resultcode != LDAP_OTHER)) {
		return false;
	}

	printf(" Try another wrong rename\n");

	msg->type = LDAP_TAG_ModifyDNRequest;
	msg->r.ModifyDNRequest.dn = basedn;
	msg->r.ModifyDNRequest.newrdn = basedn;
	msg->r.ModifyDNRequest.deleteolddn = true;
	msg->r.ModifyDNRequest.newsuperior = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_ModifyDNResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap rename request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.ModifyDNResponse.resultcode == 0)
		|| (rep->r.ModifyDNResponse.errormessage == NULL)
		|| (strtol(rep->r.ModifyDNResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.ModifyDNResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if ((!W_ERROR_EQUAL(err, WERR_INVALID_PARAM) &&
	     !W_ERROR_EQUAL(err, WERR_DS_NAMING_VIOLATION))
		|| (rep->r.ModifyDNResponse.resultcode != LDAP_NAMING_VIOLATION)) {
		return false;
	}

	printf(" Try another wrong rename\n");

	msg->type = LDAP_TAG_ModifyDNRequest;
	msg->r.ModifyDNRequest.dn = basedn;
	msg->r.ModifyDNRequest.newrdn = "";
	msg->r.ModifyDNRequest.deleteolddn = true;
	msg->r.ModifyDNRequest.newsuperior = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_ModifyDNResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap rename request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.ModifyDNResponse.resultcode == 0)
		|| (rep->r.ModifyDNResponse.errormessage == NULL)
		|| (strtol(rep->r.ModifyDNResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.ModifyDNResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if ((!W_ERROR_EQUAL(err, WERR_INVALID_PARAM) &&
	     !W_ERROR_EQUAL(err, WERR_DS_PROTOCOL_ERROR))
		|| (rep->r.ModifyDNResponse.resultcode != LDAP_PROTOCOL_ERROR)) {
		return false;
	}

	printf(" Try another wrong rename\n");

	msg->type = LDAP_TAG_ModifyDNRequest;
	msg->r.ModifyDNRequest.dn = "";
	msg->r.ModifyDNRequest.newrdn = "cn=temp";
	msg->r.ModifyDNRequest.deleteolddn = true;
	msg->r.ModifyDNRequest.newsuperior = NULL;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_result_one(req, &rep, LDAP_TAG_ModifyDNResponse);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap rename request - %s\n", nt_errstr(status));
		return false;
	}

	if ((rep->r.ModifyDNResponse.resultcode == 0)
		|| (rep->r.ModifyDNResponse.errormessage == NULL)
		|| (strtol(rep->r.ModifyDNResponse.errormessage, &endptr,16) <= 0)
		|| (*endptr != ':')) {
		printf("Invalid error message!\n");
		return false;
	}

	err = ad_error(rep->r.ModifyDNResponse.errormessage, &endptr);
	err_code_str = win_errstr(err);
	printf(" - Errorcode: %s; Reason: %s\n", err_code_str, endptr);
	if ((!W_ERROR_EQUAL(err, WERR_DS_OBJ_NOT_FOUND) &&
	     !W_ERROR_EQUAL(err, WERR_DS_NO_SUCH_OBJECT))
		|| (rep->r.ModifyDNResponse.resultcode != LDAP_NO_SUCH_OBJECT)) {
		return false;
	}

	return true;
}

static bool test_referrals(struct torture_context *tctx, TALLOC_CTX *mem_ctx,
	const char *url, const char *basedn, const char **partitions)
{
	struct ldb_context *ldb;
	struct ldb_result *res;
	const char * const *attrs = { NULL };
	struct ldb_dn *dn1, *dn2;
	int ret;
	int i, j, k;
	char *tempstr;
	bool found, l_found;

	printf("Testing referrals\n");

	if (partitions[0] == NULL) {
		printf("Partitions list empty!\n");
		return false;
	}

	if (strcmp(partitions[0], basedn) != 0) {
		printf("The first (root) partition DN should be the base DN!\n");
		return false;
	}

	ldb = ldb_wrap_connect(mem_ctx, tctx->ev, tctx->lp_ctx, url,
			       NULL, cmdline_credentials, 0);

	/* "partitions[i]" are the partitions for which we search the parents */
	for (i = 1; partitions[i] != NULL; i++) {
		dn1 = ldb_dn_new(mem_ctx, ldb, partitions[i]);
		if (dn1 == NULL) {
			printf("Out of memory\n");
			talloc_free(ldb);
			return false;
		}

		/* search using base scope */
		/* "partitions[j]" are the parent candidates */
		for (j = str_list_length(partitions) - 1; j >= 0; --j) {
			dn2 = ldb_dn_new(mem_ctx, ldb, partitions[j]);
			if (dn2 == NULL) {
				printf("Out of memory\n");
				talloc_free(ldb);
				return false;
			}

			ret = ldb_search(ldb, mem_ctx, &res, dn2,
					 LDB_SCOPE_BASE, attrs,
					 "(foo=bar)");
			if (ret != LDB_SUCCESS) {
				printf("%s", ldb_errstring(ldb));
				talloc_free(ldb);
				return false;
			}

			if (res->refs != NULL) {
				printf("There shouldn't be generated any referrals in the base scope!\n");
				talloc_free(ldb);
				return false;
			}

			talloc_free(res);
			talloc_free(dn2);
		}

		/* search using onelevel scope */
		found = false;
		/* "partitions[j]" are the parent candidates */
		for (j = str_list_length(partitions) - 1; j >= 0; --j) {
			dn2 = ldb_dn_new(mem_ctx, ldb, partitions[j]);
			if (dn2 == NULL) {
				printf("Out of memory\n");
				talloc_free(ldb);
				return false;
			}

			ret = ldb_search(ldb, mem_ctx, &res, dn2,
					 LDB_SCOPE_ONELEVEL, attrs,
					 "(foo=bar)");
			if (ret != LDB_SUCCESS) {
				printf("%s", ldb_errstring(ldb));
				talloc_free(ldb);
				return false;
			}

			tempstr = talloc_asprintf(mem_ctx, "/%s??base",
						  partitions[i]);
			if (tempstr == NULL) {
				printf("Out of memory\n");
				talloc_free(ldb);
				return false;
			}

			/* Try to find or find not a matching referral */
			l_found = false;
			for (k = 0; (!l_found) && (res->refs != NULL)
			    && (res->refs[k] != NULL); k++) {
				if (strstr(res->refs[k], tempstr) != NULL) {
					l_found = true;
				}
			}

			talloc_free(tempstr);

			if ((!found) && (ldb_dn_compare_base(dn2, dn1) == 0)
			    && (ldb_dn_compare(dn2, dn1) != 0)) {
				/* This is a referral candidate */
				if (!l_found) {
					printf("A required referral hasn't been found on onelevel scope (%s -> %s)!\n", partitions[j], partitions[i]);
					talloc_free(ldb);
					return false;
				}
				found = true;
			} else {
				/* This isn't a referral candidate */
				if (l_found) {
					printf("A unrequired referral has been found on onelevel scope (%s -> %s)!\n", partitions[j], partitions[i]);
					talloc_free(ldb);
					return false;
				}
			}

			talloc_free(res);
			talloc_free(dn2);
		}

		/* search using subtree scope */
		found = false;
		/* "partitions[j]" are the parent candidates */
		for (j = str_list_length(partitions) - 1; j >= 0; --j) {
			dn2 = ldb_dn_new(mem_ctx, ldb, partitions[j]);
			if (dn2 == NULL) {
				printf("Out of memory\n");
				talloc_free(ldb);
				return false;
			}

			ret = ldb_search(ldb, mem_ctx, &res, dn2,
					 LDB_SCOPE_SUBTREE, attrs,
					 "(foo=bar)");
			if (ret != LDB_SUCCESS) {
				printf("%s", ldb_errstring(ldb));
				talloc_free(ldb);
				return false;
			}

			tempstr = talloc_asprintf(mem_ctx, "/%s",
						  partitions[i]);
			if (tempstr == NULL) {
				printf("Out of memory\n");
				talloc_free(ldb);
				return false;
			}

			/* Try to find or find not a matching referral */
			l_found = false;
			for (k = 0; (!l_found) && (res->refs != NULL)
			    && (res->refs[k] != NULL); k++) {
				if (strstr(res->refs[k], tempstr) != NULL) {
					l_found = true;
				}
			}

			talloc_free(tempstr);

			if ((!found) && (ldb_dn_compare_base(dn2, dn1) == 0)
			    && (ldb_dn_compare(dn2, dn1) != 0)) {
				/* This is a referral candidate */
				if (!l_found) {
					printf("A required referral hasn't been found on subtree scope (%s -> %s)!\n", partitions[j], partitions[i]);
					talloc_free(ldb);
					return false;
				}
				found = true;
			} else {
				/* This isn't a referral candidate */
				if (l_found) {
					printf("A unrequired referral has been found on subtree scope (%s -> %s)!\n", partitions[j], partitions[i]);
					talloc_free(ldb);
					return false;
				}
			}

			talloc_free(res);
			talloc_free(dn2);
		}

		talloc_free(dn1);
	}

	talloc_free(ldb);

	return true;
}

static bool test_abandom_request(struct torture_context *tctx,
	struct ldap_connection *conn, const char *basedn)
{
	struct ldap_message *msg;
	struct ldap_request *req;
	NTSTATUS status;

	printf("Testing the AbandonRequest with an old message id!\n");

	if (!basedn) {
		return false;
	}

	msg = new_ldap_message(conn);
	if (!msg) {
		return false;
	}

	printf(" Try a AbandonRequest for an old message id\n");

	msg->type = LDAP_TAG_AbandonRequest;
	msg->r.AbandonRequest.messageid = 1;

	req = ldap_request_send(conn, msg);
	if (!req) {
		return false;
	}

	status = ldap_request_wait(req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("error in ldap abandon request - %s\n", nt_errstr(status));
		return false;
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
	const char *url;
	const char *basedn;
	const char **partitions;

	mem_ctx = talloc_init("torture_ldap_basic");

	url = talloc_asprintf(mem_ctx, "ldap://%s/", host);

	status = torture_ldap_connection(torture, &conn, url);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	if (!test_search_rootDSE(conn, &basedn, &partitions)) {
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

	/* referrals test here */

	if (!test_referrals(torture, mem_ctx, url, basedn, partitions)) {
		ret = false;
	}

	if (!test_abandom_request(torture, conn, basedn)) {
		ret = false;
	}

	/* if there are no more tests we are closing */
	torture_ldap_close(conn);
	talloc_free(mem_ctx);

	return ret;
}

