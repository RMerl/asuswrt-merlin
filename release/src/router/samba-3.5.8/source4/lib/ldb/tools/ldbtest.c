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
 *  Component: ldbtest
 *
 *  Description: utility to test ldb
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_includes.h"
#include "ldb.h"
#include "tools/cmdline.h"

static struct timeval tp1,tp2;
static struct ldb_cmdline *options;

static void _start_timer(void)
{
	gettimeofday(&tp1,NULL);
}

static double _end_timer(void)
{
	gettimeofday(&tp2,NULL);
	return((tp2.tv_sec - tp1.tv_sec) + 
	       (tp2.tv_usec - tp1.tv_usec)*1.0e-6);
}

static void add_records(struct ldb_context *ldb,
			struct ldb_dn *basedn,
			int count)
{
	struct ldb_message msg;
	int i;

#if 0
        if (ldb_lock(ldb, "transaction") != 0) {
                printf("transaction lock failed\n");
                exit(1);
        }
#endif
	for (i=0;i<count;i++) {
		struct ldb_message_element el[6];
		struct ldb_val vals[6][1];
		char *name;
		TALLOC_CTX *tmp_ctx = talloc_new(ldb);

		name = talloc_asprintf(tmp_ctx, "Test%d", i);

		msg.dn = ldb_dn_copy(tmp_ctx, basedn);
		ldb_dn_add_child_fmt(msg.dn, "cn=%s", name);
		msg.num_elements = 6;
		msg.elements = el;

		el[0].flags = 0;
		el[0].name = talloc_strdup(tmp_ctx, "cn");
		el[0].num_values = 1;
		el[0].values = vals[0];
		vals[0][0].data = (uint8_t *)name;
		vals[0][0].length = strlen(name);

		el[1].flags = 0;
		el[1].name = "title";
		el[1].num_values = 1;
		el[1].values = vals[1];
		vals[1][0].data = (uint8_t *)talloc_asprintf(tmp_ctx, "The title of %s", name);
		vals[1][0].length = strlen((char *)vals[1][0].data);

		el[2].flags = 0;
		el[2].name = talloc_strdup(tmp_ctx, "uid");
		el[2].num_values = 1;
		el[2].values = vals[2];
		vals[2][0].data = (uint8_t *)ldb_casefold(ldb, tmp_ctx, name, strlen(name));
		vals[2][0].length = strlen((char *)vals[2][0].data);

		el[3].flags = 0;
		el[3].name = talloc_strdup(tmp_ctx, "mail");
		el[3].num_values = 1;
		el[3].values = vals[3];
		vals[3][0].data = (uint8_t *)talloc_asprintf(tmp_ctx, "%s@example.com", name);
		vals[3][0].length = strlen((char *)vals[3][0].data);

		el[4].flags = 0;
		el[4].name = talloc_strdup(tmp_ctx, "objectClass");
		el[4].num_values = 1;
		el[4].values = vals[4];
		vals[4][0].data = (uint8_t *)talloc_strdup(tmp_ctx, "OpenLDAPperson");
		vals[4][0].length = strlen((char *)vals[4][0].data);

		el[5].flags = 0;
		el[5].name = talloc_strdup(tmp_ctx, "sn");
		el[5].num_values = 1;
		el[5].values = vals[5];
		vals[5][0].data = (uint8_t *)name;
		vals[5][0].length = strlen((char *)vals[5][0].data);

		ldb_delete(ldb, msg.dn);

		if (ldb_add(ldb, &msg) != 0) {
			printf("Add of %s failed - %s\n", name, ldb_errstring(ldb));
			exit(1);
		}

		printf("adding uid %s\r", name);
		fflush(stdout);

		talloc_free(tmp_ctx);
	}
#if 0
        if (ldb_unlock(ldb, "transaction") != 0) {
                printf("transaction unlock failed\n");
                exit(1);
        }
#endif
	printf("\n");
}

static void modify_records(struct ldb_context *ldb,
			   struct ldb_dn *basedn,
			   int count)
{
	struct ldb_message msg;
	int i;

	for (i=0;i<count;i++) {
		struct ldb_message_element el[3];
		struct ldb_val vals[3];
		char *name;
		TALLOC_CTX *tmp_ctx = talloc_new(ldb);
		
		name = talloc_asprintf(tmp_ctx, "Test%d", i);
		msg.dn = ldb_dn_copy(tmp_ctx, basedn);
		ldb_dn_add_child_fmt(msg.dn, "cn=%s", name);

		msg.num_elements = 3;
		msg.elements = el;

		el[0].flags = LDB_FLAG_MOD_DELETE;
		el[0].name = talloc_strdup(tmp_ctx, "mail");
		el[0].num_values = 0;

		el[1].flags = LDB_FLAG_MOD_ADD;
		el[1].name = talloc_strdup(tmp_ctx, "mail");
		el[1].num_values = 1;
		el[1].values = &vals[1];
		vals[1].data = (uint8_t *)talloc_asprintf(tmp_ctx, "%s@other.example.com", name);
		vals[1].length = strlen((char *)vals[1].data);

		el[2].flags = LDB_FLAG_MOD_REPLACE;
		el[2].name = talloc_strdup(tmp_ctx, "mail");
		el[2].num_values = 1;
		el[2].values = &vals[2];
		vals[2].data = (uint8_t *)talloc_asprintf(tmp_ctx, "%s@other2.example.com", name);
		vals[2].length = strlen((char *)vals[2].data);

		if (ldb_modify(ldb, &msg) != 0) {
			printf("Modify of %s failed - %s\n", name, ldb_errstring(ldb));
			exit(1);
		}

		printf("Modifying uid %s\r", name);
		fflush(stdout);

		talloc_free(tmp_ctx);
	}

	printf("\n");
}


static void delete_records(struct ldb_context *ldb,
			   struct ldb_dn *basedn,
			   int count)
{
	int i;

	for (i=0;i<count;i++) {
		struct ldb_dn *dn;
		char *name = talloc_asprintf(ldb, "Test%d", i);
		dn = ldb_dn_copy(name, basedn);
		ldb_dn_add_child_fmt(dn, "cn=%s", name);

		printf("Deleting uid Test%d\r", i);
		fflush(stdout);

		if (ldb_delete(ldb, dn) != 0) {
			printf("Delete of %s failed - %s\n", ldb_dn_get_linearized(dn), ldb_errstring(ldb));
			exit(1);
		}
		talloc_free(name);
	}

	printf("\n");
}

static void search_uid(struct ldb_context *ldb, struct ldb_dn *basedn, int nrecords, int nsearches)
{
	int i;

	for (i=0;i<nsearches;i++) {
		int uid = (i * 700 + 17) % (nrecords * 2);
		char *expr;
		struct ldb_result *res = NULL;
		int ret;

		expr = talloc_asprintf(ldb, "(uid=TEST%d)", uid);
		ret = ldb_search(ldb, ldb, &res, basedn, LDB_SCOPE_SUBTREE, NULL, "%s", expr);

		if (ret != LDB_SUCCESS || (uid < nrecords && res->count != 1)) {
			printf("Failed to find %s - %s\n", expr, ldb_errstring(ldb));
			exit(1);
		}

		if (uid >= nrecords && res->count > 0) {
			printf("Found %s !? - %d\n", expr, ret);
			exit(1);
		}

		printf("testing uid %d/%d - %d  \r", i, uid, res->count);
		fflush(stdout);

		talloc_free(res);
		talloc_free(expr);
	}

	printf("\n");
}

static void start_test(struct ldb_context *ldb, int nrecords, int nsearches)
{
	struct ldb_dn *basedn;

	basedn = ldb_dn_new(ldb, ldb, options->basedn);
	if ( ! ldb_dn_validate(basedn)) {
		printf("Invalid base DN\n");
		exit(1);
	}

	printf("Adding %d records\n", nrecords);
	add_records(ldb, basedn, nrecords);

	printf("Starting search on uid\n");
	_start_timer();
	search_uid(ldb, basedn, nrecords, nsearches);
	printf("uid search took %.2f seconds\n", _end_timer());

	printf("Modifying records\n");
	modify_records(ldb, basedn, nrecords);

	printf("Deleting records\n");
	delete_records(ldb, basedn, nrecords);
}


/*
      2) Store an @indexlist record

      3) Store a record that contains fields that should be index according
to @index

      4) disconnection from database

      5) connect to same database

      6) search for record added in step 3 using a search key that should
be indexed
*/
static void start_test_index(struct ldb_context **ldb)
{
	struct ldb_message *msg;
	struct ldb_result *res = NULL;
	struct ldb_dn *indexlist;
	struct ldb_dn *basedn;
	int ret;
	int flags = 0;
	const char *specials;

	specials = getenv("LDB_SPECIALS");
	if (specials && atoi(specials) == 0) {
		printf("LDB_SPECIALS disabled - skipping index test\n");
		return;
	}

	if (options->nosync) {
		flags |= LDB_FLG_NOSYNC;
	}

	printf("Starting index test\n");

	indexlist = ldb_dn_new(*ldb, *ldb, "@INDEXLIST");

	ldb_delete(*ldb, indexlist);

	msg = ldb_msg_new(NULL);

	msg->dn = indexlist;
	ldb_msg_add_string(msg, "@IDXATTR", strdup("uid"));

	if (ldb_add(*ldb, msg) != 0) {
		printf("Add of %s failed - %s\n", ldb_dn_get_linearized(msg->dn), ldb_errstring(*ldb));
		exit(1);
	}

	basedn = ldb_dn_new(*ldb, *ldb, options->basedn);

	memset(msg, 0, sizeof(*msg));
	msg->dn = ldb_dn_copy(msg, basedn);
	ldb_dn_add_child_fmt(msg->dn, "cn=test");
	ldb_msg_add_string(msg, "cn", strdup("test"));
	ldb_msg_add_string(msg, "sn", strdup("test"));
	ldb_msg_add_string(msg, "uid", strdup("test"));
	ldb_msg_add_string(msg, "objectClass", strdup("OpenLDAPperson"));

	if (ldb_add(*ldb, msg) != 0) {
		printf("Add of %s failed - %s\n", ldb_dn_get_linearized(msg->dn), ldb_errstring(*ldb));
		exit(1);
	}

	if (talloc_free(*ldb) != 0) {
		printf("failed to free/close ldb database");
		exit(1);
	}

	(*ldb) = ldb_init(options, NULL);

	ret = ldb_connect(*ldb, options->url, flags, NULL);
	if (ret != 0) {
		printf("failed to connect to %s\n", options->url);
		exit(1);
	}

	basedn = ldb_dn_new(*ldb, *ldb, options->basedn);

	ret = ldb_search(*ldb, *ldb, &res, basedn, LDB_SCOPE_SUBTREE, NULL, "uid=test");
	if (ret != LDB_SUCCESS) { 
		printf("Search with (uid=test) filter failed!\n");
		exit(1);
	}
	if(res->count != 1) {
		printf("Should have found 1 record - found %d\n", res->count);
		exit(1);
	}

	indexlist = ldb_dn_new(*ldb, *ldb, "@INDEXLIST");

	if (ldb_delete(*ldb, msg->dn) != 0 ||
	    ldb_delete(*ldb, indexlist) != 0) {
		printf("cleanup failed - %s\n", ldb_errstring(*ldb));
		exit(1);
	}

	printf("Finished index test\n");
}


static void usage(void)
{
	printf("Usage: ldbtest <options>\n");
	printf("Options:\n");
	printf("  -H ldb_url       choose the database (or $LDB_URL)\n");
	printf("  --num-records  nrecords      database size to use\n");
	printf("  --num-searches nsearches     number of searches to do\n");
	printf("\n");
	printf("tests ldb API\n\n");
	exit(1);
}

int main(int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	struct ldb_context *ldb;

	ldb = ldb_init(mem_ctx, NULL);

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	talloc_steal(mem_ctx, options);

	if (options->basedn == NULL) {
		options->basedn = "ou=Ldb Test,ou=People,o=University of Michigan,c=TEST";
	}

	srandom(1);

	printf("Testing with num-records=%d and num-searches=%d\n", 
	       options->num_records, options->num_searches);

	start_test(ldb, options->num_records, options->num_searches);

	start_test_index(&ldb);

	talloc_free(mem_ctx);

	return 0;
}
