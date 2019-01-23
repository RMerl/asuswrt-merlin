/* 
   Unix SMB/CIFS implementation.
   SMB trans2 alias scanner
   Copyright (C) Andrew Tridgell 2003
   
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
#include "../lib/util/dlinklist.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"

int create_complex_file(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *fname);

struct trans2_blobs {
	struct trans2_blobs *next, *prev;
	uint16_t level;
	DATA_BLOB params, data;
};

/* look for aliases for a query */
static bool gen_aliases(struct torture_context *tctx, 
						struct smbcli_state *cli, struct smb_trans2 *t2, 
						int level_offset)
{
	uint16_t level;
	struct trans2_blobs *alias_blobs = NULL;
	struct trans2_blobs *t2b, *t2b2;
	int count=0, alias_count=0;

	for (level=0;level<2000;level++) {
		NTSTATUS status;

		SSVAL(t2->in.params.data, level_offset, level);
		
		status = smb_raw_trans2(cli->tree, tctx, t2);
		if (!NT_STATUS_IS_OK(status)) continue;

		t2b = talloc(tctx, struct trans2_blobs);
		t2b->level = level;
		t2b->params = t2->out.params;
		t2b->data = t2->out.data;
		DLIST_ADD(alias_blobs, t2b);
		torture_comment(tctx, 
						"\tFound level %4u (0x%03x) of size %3d (0x%02x)\n", 
			 level, level,
			 (int)t2b->data.length, (int)t2b->data.length);
		count++;
	}

	torture_comment(tctx, "Found %d levels with success status\n", count);

	for (t2b=alias_blobs; t2b; t2b=t2b->next) {
		for (t2b2=alias_blobs; t2b2; t2b2=t2b2->next) {
			if (t2b->level >= t2b2->level) continue;
			if (data_blob_cmp(&t2b->params, &t2b2->params) == 0 &&
			    data_blob_cmp(&t2b->data, &t2b2->data) == 0) {
				torture_comment(tctx, 
				"\tLevel %u (0x%x) and level %u (0x%x) are possible aliases\n", 
				       t2b->level, t2b->level, t2b2->level, t2b2->level);
				alias_count++;
			}
		}
	}

	torture_comment(tctx, "Found %d aliased levels\n", alias_count);

	return true;
}

/* look for qfsinfo aliases */
static bool qfsinfo_aliases(struct torture_context *tctx, struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_QFSINFO;

	t2.in.max_param = 0;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 2);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	return gen_aliases(tctx, cli, &t2, 0);
}

/* look for qfileinfo aliases */
static bool qfileinfo_aliases(struct torture_context *tctx, struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_QFILEINFO;
	const char *fname = "\\qfileinfo_aliases.txt";
	int fnum;

	t2.in.max_param = 2;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 4);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, cli, fname);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, 
					"open of %s failed (%s)", fname, 
				   smbcli_errstr(cli->tree)));

	smbcli_write(cli->tree, fnum, 0, &t2, 0, sizeof(t2));

	SSVAL(t2.in.params.data, 0, fnum);

	if (!gen_aliases(tctx, cli, &t2, 2))
		return false;

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	return true;
}


/* look for qpathinfo aliases */
static bool qpathinfo_aliases(struct torture_context *tctx, struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_QPATHINFO;
	const char *fname = "\\qpathinfo_aliases.txt";
	int fnum;

	t2.in.max_param = 2;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 6);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, cli, fname);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, 
					"open of %s failed (%s)", fname, 
				   smbcli_errstr(cli->tree)));

	smbcli_write(cli->tree, fnum, 0, &t2, 0, sizeof(t2));
	smbcli_close(cli->tree, fnum);

	SIVAL(t2.in.params.data, 2, 0);

	smbcli_blob_append_string(cli->session, tctx, &t2.in.params, 
			       fname, STR_TERMINATE);

	if (!gen_aliases(tctx, cli, &t2, 0))
		return false;

	smbcli_unlink(cli->tree, fname);

	return true;
}


/* look for trans2 findfirst aliases */
static bool findfirst_aliases(struct torture_context *tctx, struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_FINDFIRST;
	const char *fname = "\\findfirst_aliases.txt";
	int fnum;

	t2.in.max_param = 16;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 12);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, cli, fname);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, 
					"open of %s failed (%s)", fname, 
				   smbcli_errstr(cli->tree)));

	smbcli_write(cli->tree, fnum, 0, &t2, 0, sizeof(t2));
	smbcli_close(cli->tree, fnum);

	SSVAL(t2.in.params.data, 0, 0);
	SSVAL(t2.in.params.data, 2, 1);
	SSVAL(t2.in.params.data, 4, FLAG_TRANS2_FIND_CLOSE);
	SSVAL(t2.in.params.data, 6, 0);
	SIVAL(t2.in.params.data, 8, 0);

	smbcli_blob_append_string(cli->session, tctx, &t2.in.params, 
			       fname, STR_TERMINATE);

	if (!gen_aliases(tctx, cli, &t2, 6))
		return false;

	smbcli_unlink(cli->tree, fname);

	return true;
}



/* look for aliases for a set function */
static bool gen_set_aliases(struct torture_context *tctx, 
							struct smbcli_state *cli, 
							struct smb_trans2 *t2, int level_offset)
{
	uint16_t level;
	struct trans2_blobs *alias_blobs = NULL;
	struct trans2_blobs *t2b;
	int count=0, dsize;

	for (level=1;level<1100;level++) {
		NTSTATUS status, status1;
		SSVAL(t2->in.params.data, level_offset, level);

		status1 = NT_STATUS_OK;

		for (dsize=2; dsize<1024; dsize += 2) {
			data_blob_free(&t2->in.data);
			t2->in.data = data_blob(NULL, dsize);
			data_blob_clear(&t2->in.data);
			status = smb_raw_trans2(cli->tree, tctx, t2);
			/* some error codes mean that this whole level doesn't exist */
			if (NT_STATUS_EQUAL(NT_STATUS_INVALID_LEVEL, status) ||
			    NT_STATUS_EQUAL(NT_STATUS_INVALID_INFO_CLASS, status) ||
			    NT_STATUS_EQUAL(NT_STATUS_NOT_SUPPORTED, status)) {
				break;
			}
			if (NT_STATUS_IS_OK(status)) break;

			/* invalid parameter means that the level exists at this 
			   size, but the contents are wrong (not surprising with
			   all zeros!) */
			if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) break;

			/* this is the usual code for 'wrong size' */
			if (NT_STATUS_EQUAL(status, NT_STATUS_INFO_LENGTH_MISMATCH)) {
				continue;
			}

			if (!NT_STATUS_EQUAL(status, status1)) {
				torture_comment(tctx, "level=%d size=%d %s\n", level, dsize, nt_errstr(status));
			}
			status1 = status;
		}

		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) continue;

		t2b = talloc(tctx, struct trans2_blobs);
		t2b->level = level;
		t2b->params = t2->out.params;
		t2b->data = t2->out.data;
		DLIST_ADD(alias_blobs, t2b);
		torture_comment(tctx, 
					"\tFound level %4u (0x%03x) of size %3d (0x%02x)\n", 
			 level, level,
			 (int)t2->in.data.length, (int)t2->in.data.length);
		count++;
	}

	torture_comment(tctx, "Found %d valid levels\n", count);

	return true;
}



/* look for setfileinfo aliases */
static bool setfileinfo_aliases(struct torture_context *tctx, struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_SETFILEINFO;
	const char *fname = "\\setfileinfo_aliases.txt";
	int fnum;

	t2.in.max_param = 2;
	t2.in.max_data = 0;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 6);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, cli, fname);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, 
				   "open of %s failed (%s)", fname, 
				   smbcli_errstr(cli->tree)));

	smbcli_write(cli->tree, fnum, 0, &t2, 0, sizeof(t2));

	SSVAL(t2.in.params.data, 0, fnum);
	SSVAL(t2.in.params.data, 4, 0);

	gen_set_aliases(tctx, cli, &t2, 2);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	return true;
}

/* look for setpathinfo aliases */
static bool setpathinfo_aliases(struct torture_context *tctx, 
								struct smbcli_state *cli)
{
	struct smb_trans2 t2;
	uint16_t setup = TRANSACT2_SETPATHINFO;
	const char *fname = "\\setpathinfo_aliases.txt";
	int fnum;

	t2.in.max_param = 32;
	t2.in.max_data = UINT16_MAX;
	t2.in.max_setup = 0;
	t2.in.flags = 0;
	t2.in.timeout = 0;
	t2.in.setup_count = 1;
	t2.in.setup = &setup;
	t2.in.params = data_blob_talloc_zero(tctx, 4);
	t2.in.data = data_blob(NULL, 0);
	ZERO_STRUCT(t2.out);

	smbcli_unlink(cli->tree, fname);

	fnum = create_complex_file(cli, cli, fname);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, 
					"open of %s failed (%s)", fname, 
				   smbcli_errstr(cli->tree)));

	smbcli_write(cli->tree, fnum, 0, &t2, 0, sizeof(t2));
	smbcli_close(cli->tree, fnum);

	SSVAL(t2.in.params.data, 2, 0);

	smbcli_blob_append_string(cli->session, tctx, &t2.in.params, 
			       fname, STR_TERMINATE);

	if (!gen_set_aliases(tctx, cli, &t2, 0))
		return false;

	torture_assert_ntstatus_ok(tctx, smbcli_unlink(cli->tree, fname),
		talloc_asprintf(tctx, "unlink: %s", smbcli_errstr(cli->tree)));

	return true;
}


/* look for aliased info levels in trans2 calls */
struct torture_suite *torture_trans2_aliases(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "aliases");

	torture_suite_add_1smb_test(suite, "QFILEINFO aliases", qfsinfo_aliases);
	torture_suite_add_1smb_test(suite, "QFSINFO aliases", qfileinfo_aliases);
	torture_suite_add_1smb_test(suite, "QPATHINFO aliases", qpathinfo_aliases);
	torture_suite_add_1smb_test(suite, "FINDFIRST aliases", findfirst_aliases);
	torture_suite_add_1smb_test(suite, "setfileinfo_aliases", setfileinfo_aliases);
	torture_suite_add_1smb_test(suite, "setpathinfo_aliases", setpathinfo_aliases);

	return suite;
}
