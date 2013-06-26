/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

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
 *  Name: ldb
 *
 *  Component: ldbsearch
 *
 *  Description: utility for ldb search - modelled on ldapsearch
 *
 *  Author: Andrew Tridgell
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb.h"
#include "tools/cmdline.h"

static void usage(struct ldb_context *ldb)
{
	printf("Usage: ldbsearch <options> <expression> <attrs...>\n");
	ldb_cmdline_help(ldb, "ldbsearch", stdout);
	exit(LDB_ERR_OPERATIONS_ERROR);
}

static int do_compare_msg(struct ldb_message **el1,
			  struct ldb_message **el2,
			  void *opaque)
{
	return ldb_dn_compare((*el1)->dn, (*el2)->dn);
}

struct search_context {
	struct ldb_context *ldb;
	struct ldb_control **req_ctrls;

	int sort;
	unsigned int num_stored;
	struct ldb_message **store;
	unsigned int refs_stored;
	char **refs_store;

	unsigned int entries;
	unsigned int refs;

	unsigned int pending;
	int status;
};

static int store_message(struct ldb_message *msg, struct search_context *sctx) {

	sctx->store = talloc_realloc(sctx, sctx->store, struct ldb_message *, sctx->num_stored + 2);
	if (!sctx->store) {
		fprintf(stderr, "talloc_realloc failed while storing messages\n");
		return -1;
	}

	sctx->store[sctx->num_stored] = talloc_move(sctx->store, &msg);
	sctx->num_stored++;
	sctx->store[sctx->num_stored] = NULL;

	return 0;
}

static int store_referral(char *referral, struct search_context *sctx) {

	sctx->refs_store = talloc_realloc(sctx, sctx->refs_store, char *, sctx->refs_stored + 2);
	if (!sctx->refs_store) {
		fprintf(stderr, "talloc_realloc failed while storing referrals\n");
		return -1;
	}

	sctx->refs_store[sctx->refs_stored] = talloc_move(sctx->refs_store, &referral);
	sctx->refs_stored++;
	sctx->refs_store[sctx->refs_stored] = NULL;

	return 0;
}

static int display_message(struct ldb_message *msg, struct search_context *sctx) {
	struct ldb_ldif ldif;

	sctx->entries++;
	printf("# record %d\n", sctx->entries);

	ldif.changetype = LDB_CHANGETYPE_NONE;
	ldif.msg = msg;

       	if (sctx->sort) {
	/*
	 * Ensure attributes are always returned in the same
	 * order.  For testing, this makes comparison of old
	 * vs. new much easier.
	 */
        	ldb_msg_sort_elements(ldif.msg);
       	}

	ldb_ldif_write_file(sctx->ldb, stdout, &ldif);

	return 0;
}

static int display_referral(char *referral, struct search_context *sctx)
{

	sctx->refs++;
	printf("# Referral\nref: %s\n\n", referral);

	return 0;
}

static int search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct search_context *sctx;
	int ret = LDB_SUCCESS;

	sctx = talloc_get_type(req->context, struct search_context);

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_request_done(req, ares->error);
	}
	
	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		if (sctx->sort) {
			ret = store_message(ares->message, sctx);
		} else {
			ret = display_message(ares->message, sctx);
		}
		break;

	case LDB_REPLY_REFERRAL:
		if (sctx->sort) {
			ret = store_referral(ares->referral, sctx);
		} else {
			ret = display_referral(ares->referral, sctx);
		}
		if (ret) {
			return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
		}
		break;

	case LDB_REPLY_DONE:
		if (ares->controls) {
			if (handle_controls_reply(ares->controls, sctx->req_ctrls) == 1)
				sctx->pending = 1;
		}
		talloc_free(ares);
		return ldb_request_done(req, LDB_SUCCESS);
	}

	talloc_free(ares);
	if (ret != LDB_SUCCESS) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	return LDB_SUCCESS;
}

static int do_search(struct ldb_context *ldb,
		     struct ldb_dn *basedn,
		     struct ldb_cmdline *options,
		     const char *expression,
		     const char * const *attrs)
{
	struct ldb_request *req;
	struct search_context *sctx;
	int ret;

	req = NULL;
	
	sctx = talloc_zero(ldb, struct search_context);
	if (!sctx) return LDB_ERR_OPERATIONS_ERROR;

	sctx->ldb = ldb;
	sctx->sort = options->sorted;
	sctx->req_ctrls = ldb_parse_control_strings(ldb, sctx, (const char **)options->controls);
	if (options->controls != NULL &&  sctx->req_ctrls== NULL) {
		printf("parsing controls failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (basedn == NULL) {
		basedn = ldb_get_default_basedn(ldb);
	}

again:
	/* free any previous requests */
	if (req) talloc_free(req);

	ret = ldb_build_search_req(&req, ldb, ldb,
				   basedn, options->scope,
				   expression, attrs,
				   sctx->req_ctrls,
				   sctx, search_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(sctx);
		printf("allocating request failed: %s\n", ldb_errstring(ldb));
		return ret;
	}

	sctx->pending = 0;

	ret = ldb_request(ldb, req);
	if (ret != LDB_SUCCESS) {
		printf("search failed - %s\n", ldb_errstring(ldb));
		return ret;
	}

	ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	if (ret != LDB_SUCCESS) {
		printf("search error - %s\n", ldb_errstring(ldb));
		return ret;
	}

	if (sctx->pending)
		goto again;

	if (sctx->sort && (sctx->num_stored != 0 || sctx->refs != 0)) {
		unsigned int i;

		if (sctx->num_stored) {
			LDB_TYPESAFE_QSORT(sctx->store, sctx->num_stored, ldb, do_compare_msg);
		}
		for (i = 0; i < sctx->num_stored; i++) {
			display_message(sctx->store[i], sctx);
		}

		for (i = 0; i < sctx->refs_stored; i++) {
			display_referral(sctx->refs_store[i], sctx);
		}
	}

	printf("# returned %u records\n# %u entries\n# %u referrals\n",
		sctx->entries + sctx->refs, sctx->entries, sctx->refs);

	talloc_free(sctx);
	talloc_free(req);

	return LDB_SUCCESS;
}

int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	struct ldb_dn *basedn = NULL;
	const char * const * attrs = NULL;
	struct ldb_cmdline *options;
	int ret = -1;
	const char *expression = "(|(objectClass=*)(distinguishedName=*))";
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	ldb = ldb_init(mem_ctx, NULL);
	if (ldb == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	/* the check for '=' is for compatibility with ldapsearch */
	if (!options->interactive &&
	    options->argc > 0 && 
	    strchr(options->argv[0], '=')) {
		expression = options->argv[0];
		options->argv++;
		options->argc--;
	}

	if (options->argc > 0) {
		attrs = (const char * const *)(options->argv);
	}

	if (options->basedn != NULL) {
		basedn = ldb_dn_new(ldb, ldb, options->basedn);
		if (basedn == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	if (options->interactive) {
		char line[1024];
		while (fgets(line, sizeof(line), stdin)) {
			ret = do_search(ldb, basedn, options, line, attrs);
		}
	} else {
		ret = do_search(ldb, basedn, options, expression, attrs);
	}

	talloc_free(mem_ctx);

	return ret;
}
