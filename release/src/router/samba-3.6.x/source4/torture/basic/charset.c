/* 
   Unix SMB/CIFS implementation.

   SMB torture tester - charset test routines

   Copyright (C) Andrew Tridgell 2001
   
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
#include "libcli/libcli.h"
#include "torture/util.h"
#include "param/param.h"

#define BASEDIR "\\chartest\\"

/* 
   open a file using a set of unicode code points for the name

   the prefix BASEDIR is added before the name
*/
static NTSTATUS unicode_open(struct torture_context *tctx,
			     struct smbcli_tree *tree,
			     TALLOC_CTX *mem_ctx,
			     uint32_t open_disposition, 
			     const uint32_t *u_name, 
			     size_t u_name_len)
{
	union smb_open io;
	char *fname, *fname2=NULL, *ucs_name;
	size_t i;
	NTSTATUS status;

	ucs_name = talloc_size(mem_ctx, (1+u_name_len)*2);
	if (!ucs_name) {
		printf("Failed to create UCS2 Name - talloc() failure\n");
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<u_name_len;i++) {
		SSVAL(ucs_name, i*2, u_name[i]);
	}
	SSVAL(ucs_name, i*2, 0);

	if (!convert_string_talloc_convenience(ucs_name, lpcfg_iconv_convenience(tctx->lp_ctx), CH_UTF16, CH_UNIX, ucs_name, (1+u_name_len)*2, (void **)&fname, &i, false)) {
		torture_comment(tctx, "Failed to convert UCS2 Name into unix - convert_string_talloc() failure\n");
		talloc_free(ucs_name);
		return NT_STATUS_NO_MEMORY;
	}

	fname2 = talloc_asprintf(ucs_name, "%s%s", BASEDIR, fname);
	if (!fname2) {
		talloc_free(ucs_name);
		return NT_STATUS_NO_MEMORY;
	}

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname2;
	io.ntcreatex.in.open_disposition = open_disposition;

	status = smb_raw_open(tree, tctx, &io);

	talloc_free(ucs_name);

	return status;
}


/*
  see if the server recognises composed characters
*/
static bool test_composed(struct torture_context *tctx, 
			  struct smbcli_state *cli)
{
	const uint32_t name1[] = {0x61, 0x308};
	const uint32_t name2[] = {0xe4};
	NTSTATUS status1, status2;

	torture_assert(tctx, torture_setup_dir(cli, BASEDIR), 
		       "setting up basedir");

	status1 = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name1, 2);
	torture_assert_ntstatus_ok(tctx, status1, "Failed to create composed name");

	status2 = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name2, 1);

	torture_assert_ntstatus_ok(tctx, status2, "Failed to create accented character");

	return true;
}

/*
  see if the server recognises a naked diacritical
*/
static bool test_diacritical(struct torture_context *tctx, 
			     struct smbcli_state *cli)
{
	const uint32_t name1[] = {0x308};
	const uint32_t name2[] = {0x308, 0x308};
	NTSTATUS status1, status2;

	torture_assert(tctx, torture_setup_dir(cli, BASEDIR), 
		       "setting up basedir");

	status1 = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name1, 1);

	torture_assert_ntstatus_ok(tctx, status1, "Failed to create naked diacritical");

	/* try a double diacritical */
	status2 = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name2, 2);

	torture_assert_ntstatus_ok(tctx, status2, "Failed to create double naked diacritical");

	return true;
}

/*
  see if the server recognises a partial surrogate pair
*/
static bool test_surrogate(struct torture_context *tctx, 
			   struct smbcli_state *cli)
{
	const uint32_t name1[] = {0xd800};
	const uint32_t name2[] = {0xdc00};
	const uint32_t name3[] = {0xd800, 0xdc00};
	NTSTATUS status;

	torture_assert(tctx, torture_setup_dir(cli, BASEDIR), 
		       "setting up basedir");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name1, 1);

	torture_assert_ntstatus_ok(tctx, status, "Failed to create partial surrogate 1");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name2, 1);

	torture_assert_ntstatus_ok(tctx, status, "Failed to create partial surrogate 2");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name3, 2);

	torture_assert_ntstatus_ok(tctx, status, "Failed to create full surrogate");

	return true;
}

/*
  see if the server recognises wide-a characters
*/
static bool test_widea(struct torture_context *tctx, 
		       struct smbcli_state *cli)
{
	const uint32_t name1[] = {'a'};
	const uint32_t name2[] = {0xff41};
	const uint32_t name3[] = {0xff21};
	NTSTATUS status;

	torture_assert(tctx, torture_setup_dir(cli, BASEDIR), 
		       "setting up basedir");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name1, 1);

	torture_assert_ntstatus_ok(tctx, status, "Failed to create 'a'");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name2, 1);

	torture_assert_ntstatus_ok(tctx, status, "Failed to create wide-a");

	status = unicode_open(tctx, cli->tree, tctx, NTCREATEX_DISP_CREATE, name3, 1);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OBJECT_NAME_COLLISION, 
		"Failed to create wide-A");

	return true;
}

struct torture_suite *torture_charset(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "charset");

	torture_suite_add_1smb_test(suite, "Testing composite character (a umlaut)", test_composed); 
	torture_suite_add_1smb_test(suite, "Testing naked diacritical (umlaut)", test_diacritical);
	torture_suite_add_1smb_test(suite, "Testing partial surrogate", test_surrogate);
	torture_suite_add_1smb_test(suite, "Testing wide-a", test_widea);

	return suite;
}
