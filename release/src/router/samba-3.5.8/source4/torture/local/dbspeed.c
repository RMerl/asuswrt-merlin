/* 
   Unix SMB/CIFS implementation.

   local test for tdb/ldb speed

   Copyright (C) Andrew Tridgell 2004
   
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
#include "system/filesys.h"
#include "../tdb/include/tdb.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb_wrap.h"
#include "lib/tdb_wrap.h"
#include "torture/smbtorture.h"
#include "param/param.h"

float tdb_speed;

static bool tdb_add_record(struct tdb_wrap *tdbw, const char *fmt1, 
			   const char *fmt2, int i)
{
	TDB_DATA key, data;
	int ret;
	key.dptr = (uint8_t *)talloc_asprintf(tdbw, fmt1, i);
	key.dsize = strlen((char *)key.dptr)+1;
	data.dptr = (uint8_t *)talloc_asprintf(tdbw, fmt2, i+10000);
	data.dsize = strlen((char *)data.dptr)+1;

	ret = tdb_store(tdbw->tdb, key, data, TDB_INSERT);

	talloc_free(key.dptr);
	talloc_free(data.dptr);
	return ret == 0;
}

/*
  test tdb speed
*/
static bool test_tdb_speed(struct torture_context *torture, const void *_data)
{
	struct timeval tv;
	struct tdb_wrap *tdbw;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	int i, count;
	TALLOC_CTX *tmp_ctx = talloc_new(torture);

	unlink("test.tdb");

	torture_comment(torture, "Testing tdb speed for sidmap\n");

	tdbw = tdb_wrap_open(tmp_ctx, "test.tdb", 
			     10000, 0, O_RDWR|O_CREAT|O_TRUNC, 0600);
	if (!tdbw) {
		unlink("test.tdb");
		talloc_free(tmp_ctx);
		torture_fail(torture, "Failed to open test.tdb");
	}

	torture_comment(torture, "Adding %d SID records\n", torture_entries);

	for (i=0;i<torture_entries;i++) {
		if (!tdb_add_record(tdbw, 
				    "S-1-5-21-53173311-3623041448-2049097239-%u",
				    "UID %u", i)) {
			torture_result(torture, TORTURE_FAIL, "Failed to add SID %d\n", i);
			goto failed;
		}
		if (!tdb_add_record(tdbw, 
				    "UID %u",
				    "S-1-5-21-53173311-3623041448-2049097239-%u", i)) {
			torture_result(torture, TORTURE_FAIL, "Failed to add UID %d\n", i);
			goto failed;
		}
	}

	torture_comment(torture, "Testing for %d seconds\n", timelimit);

	tv = timeval_current();

	for (count=0;timeval_elapsed(&tv) < timelimit;count++) {
		TDB_DATA key, data;
		i = random() % torture_entries;
		key.dptr = (uint8_t *)talloc_asprintf(tmp_ctx, "S-1-5-21-53173311-3623041448-2049097239-%u", i);
		key.dsize = strlen((char *)key.dptr)+1;
		data = tdb_fetch(tdbw->tdb, key);
		talloc_free(key.dptr);
		if (data.dptr == NULL) {
			torture_result(torture, TORTURE_FAIL, "Failed to fetch SID %d\n", i);
			goto failed;
		}
		free(data.dptr);
		key.dptr = (uint8_t *)talloc_asprintf(tmp_ctx, "UID %u", i);
		key.dsize = strlen((char *)key.dptr)+1;
		data = tdb_fetch(tdbw->tdb, key);
		talloc_free(key.dptr);
		if (data.dptr == NULL) {
			torture_result(torture, TORTURE_FAIL, "Failed to fetch UID %d\n", i);
			goto failed;
		}
		free(data.dptr);
	}

	tdb_speed = count/timeval_elapsed(&tv);
	torture_comment(torture, "tdb speed %.2f ops/sec\n", tdb_speed);
	

	unlink("test.tdb");
	talloc_free(tmp_ctx);
	return true;

failed:
	unlink("test.tdb");
	talloc_free(tmp_ctx);
	return false;
}


static bool ldb_add_record(struct ldb_context *ldb, unsigned rid)
{
	struct ldb_message *msg;	
	int ret;

	msg = ldb_msg_new(ldb);
	if (msg == NULL) {
		return false;
	}

	msg->dn = ldb_dn_new_fmt(msg, ldb, "SID=S-1-5-21-53173311-3623041448-2049097239-%u", rid);
	if (msg->dn == NULL) {
		return false;
	}

	if (ldb_msg_add_fmt(msg, "UID", "%u", rid) != 0) {
		return false;
	}

	ret = ldb_add(ldb, msg);

	talloc_free(msg);

	return ret == LDB_SUCCESS;
}


/*
  test ldb speed
*/
static bool test_ldb_speed(struct torture_context *torture, const void *_data)
{
	struct timeval tv;
	struct ldb_context *ldb;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	int i, count;
	TALLOC_CTX *tmp_ctx = talloc_new(torture);
	struct ldb_ldif *ldif;
	const char *init_ldif = "dn: @INDEXLIST\n" \
		"@IDXATTR: UID\n";
	float ldb_speed;

	unlink("./test.ldb");

	torture_comment(torture, "Testing ldb speed for sidmap\n");

	ldb = ldb_wrap_connect(tmp_ctx, torture->ev, torture->lp_ctx, "tdb://test.ldb", 
				NULL, NULL, LDB_FLG_NOSYNC, NULL);
	if (!ldb) {
		unlink("./test.ldb");
		talloc_free(tmp_ctx);
		torture_fail(torture, "Failed to open test.ldb");
	}

	/* add an index */
	ldif = ldb_ldif_read_string(ldb, &init_ldif);
	if (ldif == NULL) goto failed;
	if (ldb_add(ldb, ldif->msg) != LDB_SUCCESS) goto failed;
	talloc_free(ldif);

	torture_comment(torture, "Adding %d SID records\n", torture_entries);

	for (i=0;i<torture_entries;i++) {
		if (!ldb_add_record(ldb, i)) {
			torture_result(torture, TORTURE_FAIL, "Failed to add SID %d\n", i);
			goto failed;
		}
	}

	if (talloc_total_blocks(torture) > 100) {
		torture_result(torture, TORTURE_FAIL, "memory leak in ldb add\n");
		goto failed;
	}

	torture_comment(torture, "Testing for %d seconds\n", timelimit);

	tv = timeval_current();

	for (count=0;timeval_elapsed(&tv) < timelimit;count++) {
		struct ldb_dn *dn;
		struct ldb_result *res;

		i = random() % torture_entries;
		dn = ldb_dn_new_fmt(tmp_ctx, ldb, "SID=S-1-5-21-53173311-3623041448-2049097239-%u", i);
		if (ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL) != LDB_SUCCESS || res->count != 1) {
			torture_fail(torture, talloc_asprintf(torture, "Failed to find SID %d", i));
		}
		talloc_free(res);
		talloc_free(dn);
		if (ldb_search(ldb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE, NULL, "(UID=%u)", i) != LDB_SUCCESS || res->count != 1) {
			torture_fail(torture, talloc_asprintf(torture, "Failed to find UID %d", i));
		}
		talloc_free(res);
	}

	if (talloc_total_blocks(torture) > 100) {
		unlink("./test.ldb");
		talloc_free(tmp_ctx);
		torture_fail(torture, "memory leak in ldb search");
	}

	ldb_speed = count/timeval_elapsed(&tv);
	torture_comment(torture, "ldb speed %.2f ops/sec\n", ldb_speed);

	torture_comment(torture, "ldb/tdb speed ratio is %.2f%%\n", (100*ldb_speed/tdb_speed));
	

	unlink("./test.ldb");
	talloc_free(tmp_ctx);
	return true;

failed:
	unlink("./test.ldb");
	talloc_free(tmp_ctx);
	return false;
}

struct torture_suite *torture_local_dbspeed(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *s = torture_suite_create(mem_ctx, "DBSPEED");
	torture_suite_add_simple_tcase_const(s, "tdb_speed", test_tdb_speed,
			NULL);
	torture_suite_add_simple_tcase_const(s, "ldb_speed", test_ldb_speed,
			NULL);
	return s;
}
