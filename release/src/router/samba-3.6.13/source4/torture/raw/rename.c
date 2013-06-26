/* 
   Unix SMB/CIFS implementation.
   rename test suite
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
#include "libcli/libcli.h"
#include "torture/util.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect %s %d - should be %d\n", \
			__location__, #v, (int)v, (int)correct); \
		ret = false; \
	}} while (0)

#define BASEDIR "\\testrename"

/*
  test SMBmv ops
*/
static bool test_mv(struct torture_context *tctx, 
					struct smbcli_state *cli)
{
	union smb_rename io;
	NTSTATUS status;
	bool ret = true;
	int fnum = -1;
	const char *fname1 = BASEDIR "\\test1.txt";
	const char *fname2 = BASEDIR "\\test2.txt";
	const char *Fname1 = BASEDIR "\\Test1.txt";
	union smb_fileinfo finfo;
	union smb_open op;

	torture_comment(tctx, "Testing SMBmv\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Trying simple rename\n");

	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid.fnum = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	op.ntcreatex.in.create_options = 0;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = fname1;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	io.generic.level = RAW_RENAME_RENAME;
	io.rename.in.pattern1 = fname1;
	io.rename.in.pattern2 = fname2;
	io.rename.in.attrib = 0;
	
	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	smbcli_close(cli->tree, fnum);

	op.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	op.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE | 
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	torture_comment(tctx, "trying rename while first file open with SHARE_ACCESS_DELETE\n");
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.rename.in.pattern1 = fname2;
	io.rename.in.pattern2 = fname1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Trying case-changing rename\n");
	io.rename.in.pattern1 = fname1;
	io.rename.in.pattern2 = Fname1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.all_info.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	if (strcmp(finfo.all_info.out.fname.s, Fname1) != 0) {
		torture_warning(tctx, "(%s) Incorrect filename [%s] after case-changing "
		       "rename, should be [%s]\n", __location__,
		       finfo.all_info.out.fname.s, Fname1);
	}

	io.rename.in.pattern1 = fname1;
	io.rename.in.pattern2 = fname2;

	torture_comment(tctx, "trying rename while not open\n");
	smb_raw_exit(cli->session);
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	torture_comment(tctx, "Trying self rename\n");
	io.rename.in.pattern1 = fname2;
	io.rename.in.pattern2 = fname2;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.rename.in.pattern1 = fname1;
	io.rename.in.pattern2 = fname1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);


	torture_comment(tctx, "trying wildcard rename\n");
	io.rename.in.pattern1 = BASEDIR "\\*.txt";
	io.rename.in.pattern2 = fname1;
	
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "and again\n");
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Trying extension change\n");
	io.rename.in.pattern1 = BASEDIR "\\*.txt";
	io.rename.in.pattern2 = BASEDIR "\\*.bak";
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	torture_comment(tctx, "Checking attrib handling\n");
	torture_set_file_attribute(cli->tree, BASEDIR "\\test1.bak", FILE_ATTRIBUTE_HIDDEN);
	io.rename.in.pattern1 = BASEDIR "\\test1.bak";
	io.rename.in.pattern2 = BASEDIR "\\*.txt";
	io.rename.in.attrib = 0;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	io.rename.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


static bool test_osxrename(struct torture_context *tctx,
			   struct smbcli_state *cli)
{
	union smb_rename io;
	union smb_unlink io_un;
	NTSTATUS status;
	bool ret = true;
	int fnum = -1;
	const char *fname1 = BASEDIR "\\test1";
	const char *FNAME1 = BASEDIR "\\TEST1";
	union smb_fileinfo finfo;
	union smb_open op;

	torture_comment(tctx, "\nTesting OSX Rename\n");
	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}
	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid.fnum = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	op.ntcreatex.in.create_options = 0;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = fname1;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	io.generic.level = RAW_RENAME_RENAME;
	io.rename.in.attrib = 0;

	smbcli_close(cli->tree, fnum);

	/* Rename by changing case. First check for the
	 * existence of the file with the "newname".
	 * If we find one and both the output and input are same case,
	 * delete it. */

	torture_comment(tctx, "Checking os X rename (case changing)\n");

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.all_info.in.file.path = FNAME1;
	torture_comment(tctx, "Looking for file %s \n",FNAME1);
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);

	if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		torture_comment(tctx, "Name of the file found %s \n", finfo.all_info.out.fname.s);
		if (strcmp(finfo.all_info.out.fname.s, finfo.all_info.in.file.path) == 0) {
			/* If file is found with the same case delete it */
			torture_comment(tctx, "Deleting File %s \n", finfo.all_info.out.fname.s);
			io_un.unlink.in.pattern = finfo.all_info.out.fname.s;
			io_un.unlink.in.attrib = 0;
			status = smb_raw_unlink(cli->tree, &io_un);
			CHECK_STATUS(status, NT_STATUS_OK);
		}
	}

	io.rename.in.pattern1 = fname1;
	io.rename.in.pattern2 = FNAME1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.all_info.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	torture_comment(tctx, "File name after rename %s \n",finfo.all_info.out.fname.s);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test SMBntrename ops
*/
static bool test_ntrename(struct torture_context *tctx, 
						  struct smbcli_state *cli)
{
	union smb_rename io;
	NTSTATUS status;
	bool ret = true;
	int fnum, i;
	const char *fname1 = BASEDIR "\\test1.txt";
	const char *fname2 = BASEDIR "\\test2.txt";
	union smb_fileinfo finfo;

	torture_comment(tctx, "Testing SMBntrename\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Trying simple rename\n");

	fnum = create_complex_file(cli, tctx, fname1);
	
	io.generic.level = RAW_RENAME_NTRENAME;
	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname2;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.cluster_size = 0;
	io.ntrename.in.flags = RENAME_FLAG_RENAME;
	
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);
	
	smbcli_close(cli->tree, fnum);
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Trying self rename\n");
	io.ntrename.in.old_name = fname2;
	io.ntrename.in.new_name = fname2;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	torture_comment(tctx, "trying wildcard rename\n");
	io.ntrename.in.old_name = BASEDIR "\\*.txt";
	io.ntrename.in.new_name = fname1;
	
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_PATH_SYNTAX_BAD);

	torture_comment(tctx, "Checking attrib handling\n");
	torture_set_file_attribute(cli->tree, fname2, FILE_ATTRIBUTE_HIDDEN);
	io.ntrename.in.old_name = fname2;
	io.ntrename.in.new_name = fname1;
	io.ntrename.in.attrib = 0;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	io.ntrename.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_set_file_attribute(cli->tree, fname1, FILE_ATTRIBUTE_NORMAL);

	torture_comment(tctx, "Checking hard link\n");
	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname2;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.flags = RENAME_FLAG_HARD_LINK;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_set_file_attribute(cli->tree, fname1, FILE_ATTRIBUTE_SYSTEM);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname2;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 2);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_SYSTEM);

	finfo.generic.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 2);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_SYSTEM);

	torture_set_file_attribute(cli->tree, fname1, FILE_ATTRIBUTE_NORMAL);

	smbcli_unlink(cli->tree, fname2);

	finfo.generic.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_NORMAL);

	torture_comment(tctx, "Checking copy\n");
	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname2;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.flags = RENAME_FLAG_COPY;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_NORMAL);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname2;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_NORMAL);

	torture_set_file_attribute(cli->tree, fname1, FILE_ATTRIBUTE_SYSTEM);

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname2;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_NORMAL);

	finfo.generic.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);
	CHECK_VALUE(finfo.all_info.out.attrib, FILE_ATTRIBUTE_SYSTEM);

	torture_set_file_attribute(cli->tree, fname1, FILE_ATTRIBUTE_NORMAL);

	smbcli_unlink(cli->tree, fname2);

	finfo.generic.in.file.path = fname1;
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.all_info.out.nlink, 1);

	torture_comment(tctx, "Checking invalid flags\n");
	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname2;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.flags = 0;
	status = smb_raw_rename(cli->tree, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	io.ntrename.in.flags = 300;
	status = smb_raw_rename(cli->tree, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	io.ntrename.in.flags = 0x106;
	status = smb_raw_rename(cli->tree, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "Checking unknown field\n");
	io.ntrename.in.old_name = fname1;
	io.ntrename.in.new_name = fname2;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.flags = RENAME_FLAG_RENAME;
	io.ntrename.in.cluster_size = 0xff;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Trying RENAME_FLAG_MOVE_CLUSTER_INFORMATION\n");

	io.ntrename.in.old_name = fname2;
	io.ntrename.in.new_name = fname1;
	io.ntrename.in.attrib = 0;
	io.ntrename.in.flags = RENAME_FLAG_MOVE_CLUSTER_INFORMATION;
	io.ntrename.in.cluster_size = 1;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	io.ntrename.in.flags = RENAME_FLAG_COPY;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

#if 0
	{
		char buf[16384];
		fnum = smbcli_open(cli->tree, fname1, O_RDWR, DENY_NONE);
		memset(buf, 1, sizeof(buf));
		smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));
		smbcli_close(cli->tree, fnum);

		fnum = smbcli_open(cli->tree, fname2, O_RDWR, DENY_NONE);
		memset(buf, 1, sizeof(buf));
		smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf)-1);
		smbcli_close(cli->tree, fnum);

		torture_all_info(cli->tree, fname1);
		torture_all_info(cli->tree, fname2);
	}
	

	io.ntrename.in.flags = RENAME_FLAG_MOVE_CLUSTER_INFORMATION;
	status = smb_raw_rename(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	for (i=0;i<20000;i++) {
		io.ntrename.in.cluster_size = i;
		status = smb_raw_rename(cli->tree, &io);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
			torture_warning(tctx, "i=%d status=%s\n", i, nt_errstr(status));
		}
	}
#endif

	torture_comment(tctx, "Checking other flags\n");

	for (i=0;i<0xFFF;i++) {
		if (i == RENAME_FLAG_RENAME ||
		    i == RENAME_FLAG_HARD_LINK ||
		    i == RENAME_FLAG_COPY) {
			continue;
		}

		io.ntrename.in.old_name = fname2;
		io.ntrename.in.new_name = fname1;
		io.ntrename.in.flags = i;
		io.ntrename.in.attrib = 0;
		io.ntrename.in.cluster_size = 0;
		status = smb_raw_rename(cli->tree, &io);
		if (TARGET_IS_WIN7(tctx)){
			if (!NT_STATUS_EQUAL(status,
					     NT_STATUS_INVALID_PARAMETER)) {
				torture_warning(tctx, "flags=0x%x status=%s\n",
						i, nt_errstr(status));
			}
		} else {
			if (!NT_STATUS_EQUAL(status,
					     NT_STATUS_ACCESS_DENIED)) {
				torture_warning(tctx, "flags=0x%x status=%s\n",
						i, nt_errstr(status));
			}
		}
	}
	
done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test dir rename.
*/
static bool test_dir_rename(struct torture_context *tctx, struct smbcli_state *cli)
{
        union smb_open io;
	union smb_rename ren_io;
	NTSTATUS status;
        const char *dname1 = BASEDIR "\\dir_for_rename";
        const char *dname2 = BASEDIR "\\renamed_dir";
        const char *dname1_long = BASEDIR "\\dir_for_rename_long";
        const char *fname = BASEDIR "\\dir_for_rename\\file.txt";
	const char *sname = BASEDIR "\\renamed_dir:a stream:$DATA";
	bool ret = true;
	int fnum = -1;

	torture_comment(tctx, "Checking rename on a directory containing an open file.\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

        /* create a directory */
        smbcli_rmdir(cli->tree, dname1);
        smbcli_rmdir(cli->tree, dname2);
        smbcli_rmdir(cli->tree, dname1_long);
        smbcli_unlink(cli->tree, dname1);
        smbcli_unlink(cli->tree, dname2);
        smbcli_unlink(cli->tree, dname1_long);

        ZERO_STRUCT(io);
        io.generic.level = RAW_OPEN_NTCREATEX;
        io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
        io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
        io.ntcreatex.in.alloc_size = 0;
        io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
        io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
        io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
        io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
        io.ntcreatex.in.fname = dname1;
        status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);

        fnum = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum);

        /* create the longname directory */
        io.ntcreatex.in.fname = dname1_long;
        status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);

        fnum = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum);

        /* Now create and hold open a file. */
        ZERO_STRUCT(io);

        io.generic.level = RAW_OPEN_NTCREATEX;
        io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
        io.ntcreatex.in.root_fid.fnum = 0;
        io.ntcreatex.in.alloc_size = 0;
        io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
        io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
        io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
        io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
        io.ntcreatex.in.create_options = 0;
        io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
        io.ntcreatex.in.security_flags = 0;
        io.ntcreatex.in.fname = fname;

        /* Create the file. */

        status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);
        fnum = io.ntcreatex.out.file.fnum;

        /* Now try and rename the directory. */

        ZERO_STRUCT(ren_io);
	ren_io.generic.level = RAW_RENAME_RENAME;
	ren_io.rename.in.pattern1 = dname1;
	ren_io.rename.in.pattern2 = dname2;
	ren_io.rename.in.attrib = 0;
	
	status = smb_raw_rename(cli->tree, &ren_io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	/* Close the file and try the rename. */
	smbcli_close(cli->tree, fnum);

	status = smb_raw_rename(cli->tree, &ren_io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * Now try just holding a second handle on the directory and holding
	 * it open across a rename.  This should be allowed.
	 */
	io.ntcreatex.in.fname = dname2;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;

	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL |
	    SEC_FILE_READ_ATTRIBUTE | SEC_FILE_READ_EA | SEC_FILE_READ_DATA;

	status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);
        fnum = io.ntcreatex.out.file.fnum;

	ren_io.generic.level = RAW_RENAME_RENAME;
	ren_io.rename.in.pattern1 = dname2;
	ren_io.rename.in.pattern2 = dname1;
	ren_io.rename.in.attrib = 0;

	status = smb_raw_rename(cli->tree, &ren_io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* close our handle to the directory. */
	smbcli_close(cli->tree, fnum);

	/* Open a handle on the long name, and then
	 * try a rename. This would catch a regression
	 * in bug #6781.
	 */
	io.ntcreatex.in.fname = dname1_long;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;

	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL |
	    SEC_FILE_READ_ATTRIBUTE | SEC_FILE_READ_EA | SEC_FILE_READ_DATA;

	status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);
        fnum = io.ntcreatex.out.file.fnum;

	ren_io.generic.level = RAW_RENAME_RENAME;
	ren_io.rename.in.pattern1 = dname1;
	ren_io.rename.in.pattern2 = dname2;
	ren_io.rename.in.attrib = 0;

	status = smb_raw_rename(cli->tree, &ren_io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* close our handle to the longname directory. */
	smbcli_close(cli->tree, fnum);

	/*
	 * Now try opening a stream on the directory and holding it open
	 * across a rename.  This should be allowed.
	 */
	io.ntcreatex.in.fname = sname;

	status = smb_raw_open(cli->tree, tctx, &io);
        CHECK_STATUS(status, NT_STATUS_OK);
        fnum = io.ntcreatex.out.file.fnum;

	ren_io.generic.level = RAW_RENAME_RENAME;
	ren_io.rename.in.pattern1 = dname2;
	ren_io.rename.in.pattern2 = dname1;
	ren_io.rename.in.attrib = 0;

	status = smb_raw_rename(cli->tree, &ren_io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:

	if (fnum != -1) {
		smbcli_close(cli->tree, fnum);
	}
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

extern bool test_trans2rename(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2);
extern bool test_nttransrename(struct torture_context *tctx, struct smbcli_state *cli1);

/* 
   basic testing of rename calls
*/
struct torture_suite *torture_raw_rename(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "rename");

	torture_suite_add_1smb_test(suite, "mv", test_mv);
	/* test_trans2rename and test_nttransrename are actually in torture/raw/oplock.c to
	   use the handlers and macros there. */
	torture_suite_add_2smb_test(suite, "trans2rename", test_trans2rename);
	torture_suite_add_1smb_test(suite, "nttransrename", test_nttransrename);
	torture_suite_add_1smb_test(suite, "ntrename", test_ntrename);
	torture_suite_add_1smb_test(suite, "osxrename", test_osxrename);
	torture_suite_add_1smb_test(suite, "directory rename", test_dir_rename);

	return suite;
}
