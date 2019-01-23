/* 
   Unix SMB/CIFS implementation.
   Test samba3 hide unreadable/unwriteable
   Copyright (C) Volker Lendecke 2006
   
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
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"

static void init_unixinfo_nochange(union smb_setfileinfo *info)
{
	ZERO_STRUCTP(info);
	info->unix_basic.level = RAW_SFILEINFO_UNIX_BASIC;
	info->unix_basic.in.mode = SMB_MODE_NO_CHANGE;

	info->unix_basic.in.end_of_file = SMB_SIZE_NO_CHANGE_HI;
	info->unix_basic.in.end_of_file <<= 32;
	info->unix_basic.in.end_of_file |= SMB_SIZE_NO_CHANGE_LO;
	
	info->unix_basic.in.num_bytes = SMB_SIZE_NO_CHANGE_HI;
	info->unix_basic.in.num_bytes <<= 32;
	info->unix_basic.in.num_bytes |= SMB_SIZE_NO_CHANGE_LO;
	
	info->unix_basic.in.status_change_time = SMB_TIME_NO_CHANGE_HI;
	info->unix_basic.in.status_change_time <<= 32;
	info->unix_basic.in.status_change_time |= SMB_TIME_NO_CHANGE_LO;

	info->unix_basic.in.access_time = SMB_TIME_NO_CHANGE_HI;
	info->unix_basic.in.access_time <<= 32;
	info->unix_basic.in.access_time |= SMB_TIME_NO_CHANGE_LO;

	info->unix_basic.in.change_time = SMB_TIME_NO_CHANGE_HI;
	info->unix_basic.in.change_time <<= 32;
	info->unix_basic.in.change_time |= SMB_TIME_NO_CHANGE_LO;

	info->unix_basic.in.uid = SMB_UID_NO_CHANGE;
	info->unix_basic.in.gid = SMB_GID_NO_CHANGE;
}

struct list_state {
	const char *fname;
	bool visible;
};

static void set_visible(struct clilist_file_info *i, const char *mask,
			void *priv)
{
	struct list_state *state = (struct list_state *)priv;

	if (strcasecmp_m(state->fname, i->name) == 0)
		state->visible = true;
}

static bool is_visible(struct smbcli_tree *tree, const char *fname)
{
	struct list_state state;

	state.visible = false;
	state.fname = fname;

	if (smbcli_list(tree, "*.*", 0, set_visible, &state) < 0) {
		return false;
	}
	return state.visible;
}

static bool is_readable(struct smbcli_tree *tree, const char *fname)
{
	int fnum;
	fnum = smbcli_open(tree, fname, O_RDONLY, DENY_NONE);
	if (fnum < 0) {
		return false;
	}
	smbcli_close(tree, fnum);
	return true;
}

static bool is_writeable(TALLOC_CTX *mem_ctx, struct smbcli_tree *tree,
			 const char *fname)
{
	int fnum;
	fnum = smbcli_open(tree, fname, O_WRONLY, DENY_NONE);
	if (fnum < 0) {
		return false;
	}
	smbcli_close(tree, fnum);
	return true;
}

/*
 * This is not an exact method because there's a ton of reasons why a getatr
 * might fail. But for our purposes it's sufficient.
 */

static bool smbcli_file_exists(struct smbcli_tree *tree, const char *fname)
{
	return NT_STATUS_IS_OK(smbcli_getatr(tree, fname, NULL, NULL, NULL));
}

static NTSTATUS smbcli_chmod(struct smbcli_tree *tree, const char *fname,
			     uint64_t permissions)
{
	union smb_setfileinfo sfinfo;
	init_unixinfo_nochange(&sfinfo);
	sfinfo.unix_basic.in.file.path = fname;
	sfinfo.unix_basic.in.permissions = permissions;
	return smb_raw_setpathinfo(tree, &sfinfo);
}

bool torture_samba3_hide(struct torture_context *torture)
{
	struct smbcli_state *cli;
	const char *fname = "test.txt";
	int fnum;
	NTSTATUS status;
	struct smbcli_tree *hideunread;
	struct smbcli_tree *hideunwrite;

	if (!torture_open_connection_share(
		    torture, &cli, torture, torture_setting_string(torture, "host", NULL),
		    torture_setting_string(torture, "share", NULL), torture->ev)) {
		torture_fail(torture, "torture_open_connection_share failed\n");
	}

	status = torture_second_tcon(torture, cli->session, "hideunread",
				     &hideunread);
	torture_assert_ntstatus_ok(torture, status, "second_tcon(hideunread) failed\n");

	status = torture_second_tcon(torture, cli->session, "hideunwrite",
				     &hideunwrite);
	torture_assert_ntstatus_ok(torture, status, "second_tcon(hideunwrite) failed\n");

	status = smbcli_unlink(cli->tree, fname);
	if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
		smbcli_setatr(cli->tree, fname, 0, -1);
		smbcli_unlink(cli->tree, fname);
	}

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		torture_fail(torture,
			talloc_asprintf(torture, "Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree)));
	}

	smbcli_close(cli->tree, fnum);

	if (!smbcli_file_exists(cli->tree, fname)) {
		torture_fail(torture, talloc_asprintf(torture, "%s does not exist\n", fname));
	}

	/* R/W file should be visible everywhere */

	status = smbcli_chmod(cli->tree, fname, UNIX_R_USR|UNIX_W_USR);
	torture_assert_ntstatus_ok(torture, status, "smbcli_chmod failed\n");

	if (!is_writeable(torture, cli->tree, fname)) {
		torture_fail(torture, "File not writable\n");
	}
	if (!is_readable(cli->tree, fname)) {
		torture_fail(torture, "File not readable\n");
	}
	if (!is_visible(cli->tree, fname)) {
		torture_fail(torture, "r/w file not visible via normal share\n");
	}
	if (!is_visible(hideunread, fname)) {
		torture_fail(torture, "r/w file not visible via hide unreadable\n");
	}
	if (!is_visible(hideunwrite, fname)) {
		torture_fail(torture, "r/w file not visible via hide unwriteable\n");
	}

	/* R/O file should not be visible via hide unwriteable files */

	status = smbcli_chmod(cli->tree, fname, UNIX_R_USR);
	torture_assert_ntstatus_ok(torture, status, "smbcli_chmod failed\n");

	if (is_writeable(torture, cli->tree, fname)) {
		torture_fail(torture, "r/o is writable\n");
	}
	if (!is_readable(cli->tree, fname)) {
		torture_fail(torture, "r/o not readable\n");
	}
	if (!is_visible(cli->tree, fname)) {
		torture_fail(torture, "r/o file not visible via normal share\n");
	}
	if (!is_visible(hideunread, fname)) {
		torture_fail(torture, "r/o file not visible via hide unreadable\n");
	}
	if (is_visible(hideunwrite, fname)) {
		torture_fail(torture, "r/o file visible via hide unwriteable\n");
	}

	/* inaccessible file should be only visible on normal share */

	status = smbcli_chmod(cli->tree, fname, 0);
	torture_assert_ntstatus_ok(torture, status, "smbcli_chmod failed\n");

	if (is_writeable(torture, cli->tree, fname)) {
		torture_fail(torture, "inaccessible file is writable\n");
	}
	if (is_readable(cli->tree, fname)) {
		torture_fail(torture, "inaccessible file is readable\n");
	}
	if (!is_visible(cli->tree, fname)) {
		torture_fail(torture, "inaccessible file not visible via normal share\n");
	}
	if (is_visible(hideunread, fname)) {
		torture_fail(torture, "inaccessible file visible via hide unreadable\n");
	}
	if (is_visible(hideunwrite, fname)) {
		torture_fail(torture, "inaccessible file visible via hide unwriteable\n");
	}

	smbcli_chmod(cli->tree, fname, UNIX_R_USR|UNIX_W_USR);
	smbcli_unlink(cli->tree, fname);
	
	return true;
}

/*
 * Try to force smb_close to return an error. The only way I can think of is
 * to open a file with delete on close, chmod the parent dir to 000 and then
 * close. smb_close should return NT_STATUS_ACCESS_DENIED.
 */

bool torture_samba3_closeerr(struct torture_context *tctx)
{
	struct smbcli_state *cli = NULL;
	bool result = false;
	NTSTATUS status;
	const char *dname = "closeerr.dir";
	const char *fname = "closeerr.dir\\closerr.txt";
	int fnum;

	if (!torture_open_connection(&cli, tctx, 0)) {
		goto fail;
	}

	smbcli_deltree(cli->tree, dname);

	torture_assert_ntstatus_ok(
		tctx, smbcli_mkdir(cli->tree, dname),
		talloc_asprintf(tctx, "smbcli_mdir failed: (%s)\n",
				smbcli_errstr(cli->tree)));

	fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR,
			    DENY_NONE);
	torture_assert(tctx, fnum != -1, 
		       talloc_asprintf(tctx, "smbcli_open failed: %s\n",
				       smbcli_errstr(cli->tree)));
	smbcli_close(cli->tree, fnum);

	fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 0, 0);

	torture_assert(tctx, fnum != -1, 
		       talloc_asprintf(tctx, "smbcli_open failed: %s\n",
				       smbcli_errstr(cli->tree)));

	status = smbcli_nt_delete_on_close(cli->tree, fnum, true);

	torture_assert_ntstatus_ok(tctx, status, 
				   "setting delete_on_close on file failed !");

	status = smbcli_chmod(cli->tree, dname, 0);

	torture_assert_ntstatus_ok(tctx, status, 
				   "smbcli_chmod on file failed !");

	status = smbcli_close(cli->tree, fnum);

	smbcli_chmod(cli->tree, dname, UNIX_R_USR|UNIX_W_USR|UNIX_X_USR);
	smbcli_deltree(cli->tree, dname);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_ACCESS_DENIED,
				      "smbcli_close");

	result = true;
	
 fail:
	if (cli) {
		torture_close_connection(cli);
	}
	return result;
}
