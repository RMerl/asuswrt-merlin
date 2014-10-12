/* 
   Unix SMB/CIFS implementation.
   RAW_OPEN_* individual test suite
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
#include "libcli/raw/libcliraw.h"
#include "system/time.h"
#include "system/filesys.h"
#include "lib/events/events.h"
#include "libcli/libcli.h"
#include "torture/util.h"

/* enum for whether reads/writes are possible on a file */
enum rdwr_mode {RDWR_NONE, RDWR_RDONLY, RDWR_WRONLY, RDWR_RDWR};

#define BASEDIR "\\rawopen"

/*
  check if a open file can be read/written
*/
static enum rdwr_mode check_rdwr(struct smbcli_tree *tree, int fnum)
{
	uint8_t c = 1;
	bool can_read  = (smbcli_read(tree, fnum, &c, 0, 1) == 1);
	bool can_write = (smbcli_write(tree, fnum, 0, &c, 0, 1) == 1);
	if ( can_read &&  can_write) return RDWR_RDWR;
	if ( can_read && !can_write) return RDWR_RDONLY;
	if (!can_read &&  can_write) return RDWR_WRONLY;
	return RDWR_NONE;
}

/*
  describe a RDWR mode as a string
*/
static const char *rdwr_string(enum rdwr_mode m)
{
	switch (m) {
	case RDWR_NONE: return "NONE";
	case RDWR_RDONLY: return "RDONLY";
	case RDWR_WRONLY: return "WRONLY";
	case RDWR_RDWR: return "RDWR";
	}
	return "-";
}

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CREATE_FILE do { \
	fnum = create_complex_file(cli, tctx, fname); \
	if (fnum == -1) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Failed to create %s - %s\n", \
			 __location__, fname, smbcli_errstr(cli->tree)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_RDWR(fnum, correct) do { \
	enum rdwr_mode m = check_rdwr(cli->tree, fnum); \
	if (m != correct) { \
		torture_result(tctx, TORTURE_FAIL, \
		       "(%s) Incorrect readwrite mode %s - expected %s\n", \
		       __location__, rdwr_string(m), rdwr_string(correct)); \
		ret = false; \
	}} while (0)

#define CHECK_TIME(t, field) do { \
	time_t t1, t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.file.path = fname; \
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t1 = t & ~1; \
	t2 = nt_time_to_unix(finfo.all_info.out.field) & ~1; \
	if (abs(t1-t2) > 2) { \
		torture_result(tctx, TORTURE_FAIL, \
		       "(%s) wrong time for field %s  %s - %s\n", \
		       __location__, #field, \
		       timestring(tctx, t1), \
		       timestring(tctx, t2)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_NTTIME(t, field) do { \
	NTTIME t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.file.path = fname; \
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t2 = finfo.all_info.out.field; \
	if (t != t2) { \
		torture_result(tctx, TORTURE_FAIL, \
		       "(%s) wrong time for field %s  %s - %s\n", \
		       __location__, #field, \
		       nt_time_string(tctx, t), \
		       nt_time_string(tctx, t2)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_ALL_INFO(v, field) do { \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.file.path = fname; \
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if ((v) != (finfo.all_info.out.field)) { \
		torture_result(tctx, TORTURE_FAIL, \
		       "(%s) wrong value for field %s  0x%x - 0x%x\n", \
		       __location__, #field, (int)v, (int)(finfo.all_info.out.field)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
		       "(%s) wrong value for %s  0x%x - should be 0x%x\n", \
		       __location__, #v, (int)(v), (int)correct); \
		ret = false; \
	}} while (0)

#define SET_ATTRIB(sattrib) do { \
	union smb_setfileinfo sfinfo; \
	ZERO_STRUCT(sfinfo.basic_info.in); \
	sfinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFORMATION; \
	sfinfo.basic_info.in.file.path = fname; \
	sfinfo.basic_info.in.attrib = sattrib; \
	status = smb_raw_setpathinfo(cli->tree, &sfinfo); \
	if (!NT_STATUS_IS_OK(status)) { \
		torture_warning(tctx, "(%s) Failed to set attrib 0x%x on %s\n", \
		       __location__, sattrib, fname); \
	}} while (0)

/*
  test RAW_OPEN_OPEN
*/
static bool test_open(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_open.txt";
	NTSTATUS status;
	int fnum = -1, fnum2;
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.openold.level = RAW_OPEN_OPEN;
	io.openold.in.fname = fname;
	io.openold.in.open_mode = OPEN_FLAGS_FCB;
	io.openold.in.search_attrs = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	fnum = io.openold.out.file.fnum;

	smbcli_unlink(cli->tree, fname);
	CREATE_FILE;
	smbcli_close(cli->tree, fnum);

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.openold.out.file.fnum;
	CHECK_RDWR(fnum2, RDWR_RDWR);
	smbcli_close(cli->tree, fnum2);
	smbcli_close(cli->tree, fnum);

	/* check the read/write modes */
	io.openold.level = RAW_OPEN_OPEN;
	io.openold.in.fname = fname;
	io.openold.in.search_attrs = 0;

	io.openold.in.open_mode = OPEN_FLAGS_OPEN_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;
	CHECK_RDWR(fnum, RDWR_RDONLY);
	smbcli_close(cli->tree, fnum);

	io.openold.in.open_mode = OPEN_FLAGS_OPEN_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;
	CHECK_RDWR(fnum, RDWR_WRONLY);
	smbcli_close(cli->tree, fnum);

	io.openold.in.open_mode = OPEN_FLAGS_OPEN_RDWR;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);
	smbcli_close(cli->tree, fnum);

	/* check the share modes roughly - not a complete matrix */
	io.openold.in.open_mode = OPEN_FLAGS_OPEN_RDWR | OPEN_FLAGS_DENY_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);
	
	if (io.openold.in.open_mode != io.openold.out.rmode) {
		torture_warning(tctx, "(%s) rmode should equal open_mode - 0x%x 0x%x\n",
		       __location__, io.openold.out.rmode, io.openold.in.open_mode);
	}

	io.openold.in.open_mode = OPEN_FLAGS_OPEN_RDWR | OPEN_FLAGS_DENY_NONE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.openold.in.open_mode = OPEN_FLAGS_OPEN_READ | OPEN_FLAGS_DENY_NONE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.openold.out.file.fnum;
	CHECK_RDWR(fnum2, RDWR_RDONLY);
	smbcli_close(cli->tree, fnum);
	smbcli_close(cli->tree, fnum2);


	/* check the returned write time */
	io.openold.level = RAW_OPEN_OPEN;
	io.openold.in.fname = fname;
	io.openold.in.search_attrs = 0;
	io.openold.in.open_mode = OPEN_FLAGS_OPEN_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openold.out.file.fnum;

	/* check other reply fields */
	CHECK_TIME(io.openold.out.write_time, write_time);
	CHECK_ALL_INFO(io.openold.out.size, size);
	CHECK_ALL_INFO(io.openold.out.attrib, attrib & ~FILE_ATTRIBUTE_NONINDEXED);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test RAW_OPEN_OPENX
*/
static bool test_openx(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_openx.txt";
	const char *fname_exe = BASEDIR "\\torture_openx.exe";
	NTSTATUS status;
	int fnum = -1, fnum2;
	bool ret = true;
	int i;
	struct timeval tv;
	struct {
		uint16_t open_func;
		bool with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ OPENX_OPEN_FUNC_OPEN, 	                  true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN,  	                  false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_FAIL, 	                  true,  NT_STATUS_DOS(ERRDOS, ERRbadaccess) },
		{ OPENX_OPEN_FUNC_FAIL, 	                  false, NT_STATUS_DOS(ERRDOS, ERRbadaccess) },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OK },
	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 1024*1024;
	io.openx.in.timeout = 0;

	/* check all combinations of open_func */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = create_complex_file(cli, tctx, fname);
			if (fnum == -1) {
				torture_result(tctx, TORTURE_FAIL,
					"Failed to create file %s - %s\n",
					fname, smbcli_errstr(cli->tree));
				ret = false;
				goto done;
			}
			smbcli_close(cli->tree, fnum);
		}
		io.openx.in.open_func = open_funcs[i].open_func;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d with_file=%d open_func=0x%x)\n",
				__location__, nt_errstr(status),
				nt_errstr(open_funcs[i].correct_status),
				i, (int)open_funcs[i].with_file,
				(int)open_funcs[i].open_func);
			ret = false;
		}
		if (NT_STATUS_IS_OK(status)) {
			smbcli_close(cli->tree, io.openx.out.file.fnum);
		}
		if (open_funcs[i].with_file) {
			smbcli_unlink(cli->tree, fname);
		}
	}

	smbcli_unlink(cli->tree, fname);

	/* check the basic return fields */
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.file.fnum;

	CHECK_ALL_INFO(io.openx.out.size, size);
	CHECK_TIME(io.openx.out.write_time, write_time);
	CHECK_ALL_INFO(io.openx.out.attrib, attrib & ~FILE_ATTRIBUTE_NONINDEXED);
	CHECK_VAL(io.openx.out.access, OPENX_MODE_ACCESS_RDWR);
	CHECK_VAL(io.openx.out.ftype, 0);
	CHECK_VAL(io.openx.out.devstate, 0);
	CHECK_VAL(io.openx.out.action, OPENX_ACTION_CREATED);
	CHECK_VAL(io.openx.out.size, 1024*1024);
	CHECK_ALL_INFO(io.openx.in.size, size);
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* check the fields when the file already existed */
	fnum2 = create_complex_file(cli, tctx, fname);
	if (fnum2 == -1) {
		ret = false;
		goto done;
	}
	smbcli_close(cli->tree, fnum2);

	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.file.fnum;

	CHECK_ALL_INFO(io.openx.out.size, size);
	CHECK_TIME(io.openx.out.write_time, write_time);
	CHECK_VAL(io.openx.out.action, OPENX_ACTION_EXISTED);
	CHECK_VAL(io.openx.out.unknown, 0);
	CHECK_ALL_INFO(io.openx.out.attrib, attrib & ~FILE_ATTRIBUTE_NONINDEXED);
	smbcli_close(cli->tree, fnum);

	/* now check the search attrib for hidden files - win2003 ignores this? */
	SET_ATTRIB(FILE_ATTRIBUTE_HIDDEN);
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN, attrib);

	io.openx.in.search_attrs = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.openx.out.file.fnum);

	io.openx.in.search_attrs = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.openx.out.file.fnum);

	SET_ATTRIB(FILE_ATTRIBUTE_NORMAL);
	smbcli_unlink(cli->tree, fname);

	/* and check attrib on create */
	io.openx.in.open_func = OPENX_OPEN_FUNC_FAIL | OPENX_OPEN_FUNC_CREATE;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = FILE_ATTRIBUTE_SYSTEM;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	if (torture_setting_bool(tctx, "samba3", false)) {
		CHECK_ALL_INFO(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE, 
			       attrib & ~(FILE_ATTRIBUTE_NONINDEXED|
					  FILE_ATTRIBUTE_SPARSE));
	}
	else {
		CHECK_ALL_INFO(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE, 
			       attrib & ~(FILE_ATTRIBUTE_NONINDEXED));
	}
	smbcli_close(cli->tree, io.openx.out.file.fnum);
	smbcli_unlink(cli->tree, fname);

	/* check timeout on create - win2003 ignores the timeout! */
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.openx.in.file_attrs = 0;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.file.fnum;

	io.openx.in.timeout = 20000;
	tv = timeval_current();
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_NONE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);
	if (timeval_elapsed(&tv) > 3.0) {
		torture_result(tctx, TORTURE_FAIL,
			"(%s) Incorrect timing in openx with timeout "
			"- waited %.2f seconds\n",
			__location__, timeval_elapsed(&tv));
		ret = false;
	}
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* now this is a really weird one - open for execute implies create?! */
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_EXEC | OPENX_MODE_DENY_NONE;
	io.openx.in.search_attrs = 0;
	io.openx.in.open_func = OPENX_OPEN_FUNC_FAIL;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 0;
	io.openx.in.timeout = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.openx.out.file.fnum);

	/* check the extended return flag */
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO | OPENX_FLAGS_EXTENDED_RETURN;
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(io.openx.out.access_mask, SEC_STD_ALL);
	smbcli_close(cli->tree, io.openx.out.file.fnum);

	io.openx.in.fname = "\\A.+,;=[].B";
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	/* Check the mapping for open exec. */

	/* First create an .exe file. */
	smbcli_unlink(cli->tree, fname_exe);
	fnum = create_complex_file(cli, tctx, fname_exe);
	smbcli_close(cli->tree, fnum);

	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname_exe;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_EXEC | OPENX_MODE_DENY_NONE;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 0;
	io.openx.in.timeout = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Can we read and write ? */
	CHECK_RDWR(io.openx.out.file.fnum, RDWR_RDONLY);
	smbcli_close(cli->tree, io.openx.out.file.fnum);
	smbcli_unlink(cli->tree, fname);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test RAW_OPEN_T2OPEN

  many thanks to kukks for a sniff showing how this works with os2->w2k
*/
static bool test_t2open(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname1 = BASEDIR "\\torture_t2open_yes.txt";
	const char *fname2 = BASEDIR "\\torture_t2open_no.txt";
	const char *fname = BASEDIR "\\torture_t2open_3.txt";
	NTSTATUS status;
	int fnum;
	bool ret = true;
	int i;
	struct {
		uint16_t open_func;
		bool with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ OPENX_OPEN_FUNC_OPEN, 	                  true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN,  	                  false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_FAIL, 	                  true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL, 	                  false, NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  false, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, true,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, false, NT_STATUS_OK },
	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	fnum = create_complex_file(cli, tctx, fname1);
	if (fnum == -1) {
		torture_result(tctx, TORTURE_FAIL,
			"(%s): Failed to create file %s - %s\n",
			__location__, fname1, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}
	smbcli_close(cli->tree, fnum);

	io.t2open.level = RAW_OPEN_T2OPEN;
	io.t2open.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.t2open.in.open_mode = OPENX_MODE_DENY_NONE | OPENX_MODE_ACCESS_RDWR;
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.search_attrs = 0;
	io.t2open.in.file_attrs = 0;
	io.t2open.in.write_time = 0;
	io.t2open.in.size = 0;
	io.t2open.in.timeout = 0;

	io.t2open.in.num_eas = 3;
	io.t2open.in.eas = talloc_array(tctx, struct ea_struct, io.t2open.in.num_eas);
	io.t2open.in.eas[0].flags = 0;
	io.t2open.in.eas[0].name.s = ".CLASSINFO";
	io.t2open.in.eas[0].value = data_blob_talloc(tctx, "first value", 11);
	io.t2open.in.eas[1].flags = 0;
	io.t2open.in.eas[1].name.s = "EA TWO";
	io.t2open.in.eas[1].value = data_blob_talloc(tctx, "foo", 3);
	io.t2open.in.eas[2].flags = 0;
	io.t2open.in.eas[2].name.s = "X THIRD";
	io.t2open.in.eas[2].value = data_blob_talloc(tctx, "xy", 2);

	/* check all combinations of open_func */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
	again:
		if (open_funcs[i].with_file) {
			io.t2open.in.fname = fname1;
		} else {
			io.t2open.in.fname = fname2;
		}
		io.t2open.in.open_func = open_funcs[i].open_func;
		status = smb_raw_open(cli->tree, tctx, &io);
		if ((io.t2open.in.num_eas != 0)
		    && NT_STATUS_EQUAL(status, NT_STATUS_EAS_NOT_SUPPORTED)
		    && torture_setting_bool(tctx, "samba3", false)) {
			torture_warning(tctx, "(%s) EAs not supported, not "
				"treating as fatal in Samba3 test\n",
				__location__);
			io.t2open.in.num_eas = 0;
			goto again;
		}

		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d with_file=%d open_func=0x%x)\n",
				 __location__, nt_errstr(status),
				nt_errstr(open_funcs[i].correct_status),
				i, (int)open_funcs[i].with_file,
				(int)open_funcs[i].open_func);
			ret = false;
		}
		if (NT_STATUS_IS_OK(status)) {
			smbcli_close(cli->tree, io.t2open.out.file.fnum);
		}
	}

	smbcli_unlink(cli->tree, fname1);
	smbcli_unlink(cli->tree, fname2);

	/* check the basic return fields */
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.write_time = 0;
	io.t2open.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.t2open.out.file.fnum;

	CHECK_ALL_INFO(io.t2open.out.size, size);
#if 0
	/* windows appears to leak uninitialised memory here */
	CHECK_VAL(io.t2open.out.write_time, 0);
#endif
	CHECK_ALL_INFO(io.t2open.out.attrib, attrib & ~FILE_ATTRIBUTE_NONINDEXED);
	CHECK_VAL(io.t2open.out.access, OPENX_MODE_DENY_NONE | OPENX_MODE_ACCESS_RDWR);
	CHECK_VAL(io.t2open.out.ftype, 0);
	CHECK_VAL(io.t2open.out.devstate, 0);
	CHECK_VAL(io.t2open.out.action, OPENX_ACTION_CREATED);
	smbcli_close(cli->tree, fnum);

	status = torture_check_ea(cli, fname, ".CLASSINFO", "first value");
	CHECK_STATUS(status, io.t2open.in.num_eas
		     ? NT_STATUS_OK : NT_STATUS_EAS_NOT_SUPPORTED);
	status = torture_check_ea(cli, fname, "EA TWO", "foo");
	CHECK_STATUS(status, io.t2open.in.num_eas
		     ? NT_STATUS_OK : NT_STATUS_EAS_NOT_SUPPORTED);
	status = torture_check_ea(cli, fname, "X THIRD", "xy");
	CHECK_STATUS(status, io.t2open.in.num_eas
		     ? NT_STATUS_OK : NT_STATUS_EAS_NOT_SUPPORTED);

	/* now check the search attrib for hidden files - win2003 ignores this? */
	SET_ATTRIB(FILE_ATTRIBUTE_HIDDEN);
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN, attrib);

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.t2open.out.file.fnum);

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.t2open.out.file.fnum);

	SET_ATTRIB(FILE_ATTRIBUTE_NORMAL);
	smbcli_unlink(cli->tree, fname);

	/* and check attrib on create */
	io.t2open.in.open_func = OPENX_OPEN_FUNC_FAIL | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.file_attrs = FILE_ATTRIBUTE_SYSTEM;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* check timeout on create - win2003 ignores the timeout! */
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.file_attrs = 0;
	io.t2open.in.timeout = 20000;
	io.t2open.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
	

/*
  test RAW_OPEN_NTCREATEX
*/
static bool test_ntcreatex(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	const char *dname = BASEDIR "\\torture_ntcreatex.dir";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	int i;
	struct {
		uint32_t open_disp;
		bool with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ NTCREATEX_DISP_SUPERSEDE, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_SUPERSEDE, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE, 	true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ NTCREATEX_DISP_CREATE, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	false, NT_STATUS_OK },
		{ 6, 			        true,  NT_STATUS_INVALID_PARAMETER },
		{ 6, 	                        false, NT_STATUS_INVALID_PARAMETER },
	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 1024*1024;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* test the open disposition */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR|O_TRUNC, DENY_NONE);
			if (fnum == -1) {
				torture_result(tctx, TORTURE_FAIL,
					"Failed to create file %s - %s\n",
					fname, smbcli_errstr(cli->tree));
				ret = false;
				goto done;
			}
			smbcli_close(cli->tree, fnum);
		}
		io.ntcreatex.in.open_disposition = open_funcs[i].open_disp;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d with_file=%d open_disp=%d)\n",
				__location__, nt_errstr(status),
				nt_errstr(open_funcs[i].correct_status),
				i, (int)open_funcs[i].with_file,
				(int)open_funcs[i].open_disp);
			ret = false;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
			smbcli_unlink(cli->tree, fname);
		}
	}

	/* basic field testing */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);

	/* check fields when the file already existed */
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, tctx, fname);
	if (fnum == -1) {
		ret = false;
		goto done;
	}
	smbcli_close(cli->tree, fnum);

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);


	/* create a directory */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.fname = dname;
	fname = dname;

	smbcli_rmdir(cli->tree, fname);
	smbcli_unlink(cli->tree, fname);

	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_VAL(io.ntcreatex.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED, 
		  FILE_ATTRIBUTE_DIRECTORY);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.is_directory, 1);
	CHECK_VAL(io.ntcreatex.out.size, 0);
	CHECK_VAL(io.ntcreatex.out.alloc_size, 0);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	smbcli_unlink(cli->tree, fname);
	

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test RAW_OPEN_NTTRANS_CREATE
*/
static bool test_nttrans_create(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	const char *dname = BASEDIR "\\torture_ntcreatex.dir";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	int i;
	uint32_t ok_mask, not_supported_mask, invalid_parameter_mask;
	uint32_t not_a_directory_mask, unexpected_mask;
	struct {
		uint32_t open_disp;
		bool with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ NTCREATEX_DISP_SUPERSEDE, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_SUPERSEDE, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE, 	true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ NTCREATEX_DISP_CREATE, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	false, NT_STATUS_OK },
		{ 6, 			        true,  NT_STATUS_INVALID_PARAMETER },
		{ 6, 	                        false, NT_STATUS_INVALID_PARAMETER },
	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 1024*1024;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.sec_desc = NULL;
	io.ntcreatex.in.ea_list = NULL;

	/* test the open disposition */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR|O_TRUNC, DENY_NONE);
			if (fnum == -1) {
				torture_result(tctx, TORTURE_FAIL,
					"Failed to create file %s - %s\n",
					fname, smbcli_errstr(cli->tree));
				ret = false;
				goto done;
			}
			smbcli_close(cli->tree, fnum);
		}
		io.ntcreatex.in.open_disposition = open_funcs[i].open_disp;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d with_file=%d open_disp=%d)\n",
				__location__, nt_errstr(status),
				nt_errstr(open_funcs[i].correct_status),
				i, (int)open_funcs[i].with_file,
				(int)open_funcs[i].open_disp);
			ret = false;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
			smbcli_unlink(cli->tree, fname);
		}
	}

	/* basic field testing */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);

	/* check fields when the file already existed */
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, tctx, fname);
	if (fnum == -1) {
		ret = false;
		goto done;
	}
	smbcli_close(cli->tree, fnum);

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	smbcli_close(cli->tree, fnum);

	/* check no-recall - don't pull a file from tape on a HSM */
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NO_RECALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	smbcli_close(cli->tree, fnum);

	/* Check some create options (these all should be ignored) */
	for (i=0; i < 32; i++) {
		uint32_t create_option = (1 << i) & NTCREATEX_OPTIONS_MUST_IGNORE_MASK;
		if (create_option == 0) {
			continue;
		}
		io.ntcreatex.in.create_options = create_option;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "ntcreatex create option 0x%08x "
				"gave %s - should give NT_STATUS_OK\n",
				create_option, nt_errstr(status));
		}
		CHECK_STATUS(status, NT_STATUS_OK);
		fnum = io.ntcreatex.out.file.fnum;

		CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
		CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_EXISTED);
		CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
		CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
		CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
		CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
		CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
		CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
		CHECK_ALL_INFO(io.ntcreatex.out.size, size);
		CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
		CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
		smbcli_close(cli->tree, fnum);
	}

	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.access_mask     = SEC_FLAG_MAXIMUM_ALLOWED;

	/* Check for options that should return NOT_SUPPORTED, OK or INVALID_PARAMETER */
	ok_mask = 0;
	not_supported_mask = 0;
	invalid_parameter_mask = 0;
	not_a_directory_mask = 0;
	unexpected_mask = 0;
	for (i=0; i < 32; i++) {
		uint32_t create_option = 1<<i;
		if (create_option & NTCREATEX_OPTIONS_DELETE_ON_CLOSE) {
			continue;
		}
		io.ntcreatex.in.create_options = create_option;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
			not_supported_mask |= create_option;
		} else if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			ok_mask |= create_option;
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
		} else if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
			invalid_parameter_mask |= create_option;
		} else if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_A_DIRECTORY)) {
			not_a_directory_mask |= 1<<i;
		} else {
			unexpected_mask |= 1<<i;
			torture_comment(tctx, "create option 0x%08x returned %s\n",
					create_option, nt_errstr(status));
		}
	}

	CHECK_VAL(ok_mask,                0x00efcfce);
	CHECK_VAL(not_a_directory_mask,   0x00000001);
	CHECK_VAL(not_supported_mask,     0x00002000);
	CHECK_VAL(invalid_parameter_mask, 0xff100030);
	CHECK_VAL(unexpected_mask,        0x00000000);

	smbcli_unlink(cli->tree, fname);


	/* create a directory */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.fname = dname;
	fname = dname;

	smbcli_rmdir(cli->tree, fname);
	smbcli_unlink(cli->tree, fname);

	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_VAL(io.ntcreatex.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED, 
		  FILE_ATTRIBUTE_DIRECTORY);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.is_directory, 1);
	CHECK_VAL(io.ntcreatex.out.size, 0);
	CHECK_VAL(io.ntcreatex.out.alloc_size, 0);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	smbcli_unlink(cli->tree, fname);
	

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
  test RAW_OPEN_NTCREATEX with an already opened and byte range locked file

  I've got an application that does a similar sequence of ntcreate&x,
  locking&x and another ntcreate&x with
  open_disposition==NTCREATEX_DISP_OVERWRITE_IF. Windows 2003 allows the
  second open.
*/
static bool test_ntcreatex_brlocked(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io, io1;
	union smb_lock io2;
	struct smb_lock_entry lock[1];
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	NTSTATUS status;
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing ntcreatex with a byte range locked file\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = 0x2019f;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io.ntcreatex.in.security_flags = NTCREATEX_SECURITY_DYNAMIC |
		NTCREATEX_SECURITY_ALL;
	io.ntcreatex.in.fname = fname;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io2.lockx.level = RAW_LOCK_LOCKX;
	io2.lockx.in.file.fnum = io.ntcreatex.out.file.fnum;
	io2.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io2.lockx.in.timeout = 0;
	io2.lockx.in.ulock_cnt = 0;
	io2.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 0;
	lock[0].count = 0x1;
	io2.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io2);
	CHECK_STATUS(status, NT_STATUS_OK);

	io1.generic.level = RAW_OPEN_NTCREATEX;
	io1.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io1.ntcreatex.in.root_fid.fnum = 0;
	io1.ntcreatex.in.access_mask = 0x20196;
	io1.ntcreatex.in.alloc_size = 0;
	io1.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io1.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io1.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io1.ntcreatex.in.create_options = 0;
	io1.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io1.ntcreatex.in.security_flags = NTCREATEX_SECURITY_DYNAMIC |
		NTCREATEX_SECURITY_ALL;
	io1.ntcreatex.in.fname = fname;

	status = smb_raw_open(cli->tree, tctx, &io1);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	smbcli_close(cli->tree, io1.ntcreatex.out.file.fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test RAW_OPEN_MKNEW
*/
static bool test_mknew(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_mknew.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.mknew.level = RAW_OPEN_MKNEW;
	io.mknew.in.attrib = 0;
	io.mknew.in.write_time = 0;
	io.mknew.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.file.fnum;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_COLLISION);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* make sure write_time works */
	io.mknew.in.write_time = basetime;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.file.fnum;
	CHECK_TIME(basetime, write_time);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* make sure file_attrs works */
	io.mknew.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.file.fnum;
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE, 
		       attrib & ~FILE_ATTRIBUTE_NONINDEXED);
	
done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test RAW_OPEN_CREATE
*/
static bool test_create(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_create.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.create.level = RAW_OPEN_CREATE;
	io.create.in.attrib = 0;
	io.create.in.write_time = 0;
	io.create.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.file.fnum;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, io.create.out.file.fnum);
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* make sure write_time works */
	io.create.in.write_time = basetime;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.file.fnum;
	CHECK_TIME(basetime, write_time);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	/* make sure file_attrs works */
	io.create.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.file.fnum;
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE, 
		       attrib & ~FILE_ATTRIBUTE_NONINDEXED);
	
done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test RAW_OPEN_CTEMP
*/
static bool test_ctemp(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;
	const char *name, *fname = NULL;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.ctemp.level = RAW_OPEN_CTEMP;
	io.ctemp.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	io.ctemp.in.write_time = basetime;
	io.ctemp.in.directory = BASEDIR;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ctemp.out.file.fnum;

	name = io.ctemp.out.name;

	finfo.generic.level = RAW_FILEINFO_NAME_INFO;
	finfo.generic.in.file.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, tctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	fname = finfo.name_info.out.fname.s;
	torture_comment(tctx, "ctemp name=%s  real name=%s\n", name, fname);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test chained RAW_OPEN_OPENX_READX
*/
static bool test_chained(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_chained.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	const char *buf = "test";
	char buf2[4];

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	fnum = create_complex_file(cli, tctx, fname);

	smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));

	smbcli_close(cli->tree, fnum);	

	io.openxreadx.level = RAW_OPEN_OPENX_READX;
	io.openxreadx.in.fname = fname;
	io.openxreadx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openxreadx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openxreadx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	io.openxreadx.in.search_attrs = 0;
	io.openxreadx.in.file_attrs = 0;
	io.openxreadx.in.write_time = 0;
	io.openxreadx.in.size = 1024*1024;
	io.openxreadx.in.timeout = 0;
	
	io.openxreadx.in.offset = 0;
	io.openxreadx.in.mincnt = sizeof(buf);
	io.openxreadx.in.maxcnt = sizeof(buf);
	io.openxreadx.in.remaining = 0;
	io.openxreadx.out.data = (uint8_t *)buf2;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openxreadx.out.file.fnum;

	if (memcmp(buf, buf2, sizeof(buf)) != 0) {
		torture_result(tctx, TORTURE_FAIL,
			"wrong data in reply buffer\n");
		ret = false;
	}

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
  test RAW_OPEN_OPENX without a leading slash on the path.
  NetApp filers are known to fail on this.
  
*/
static bool test_no_leading_slash(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_no_leading_slash.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	const char *buf = "test";

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	smbcli_unlink(cli->tree, fname);

	/* Create the file */
	fnum = create_complex_file(cli, tctx, fname);
	smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));
	smbcli_close(cli->tree, fnum);	

	/* Prepare to open the file using path without leading slash */
	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname + 1;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 1024*1024;
	io.openx.in.timeout = 0;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.file.fnum;

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
  test RAW_OPEN_OPENX against an existing directory to
  ensure it returns NT_STATUS_FILE_IS_A_DIRECTORY.
  Samba 3.2.0 - 3.2.6 are known to fail this.
  
*/
static bool test_openx_over_dir(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\openx_over_dir";
	NTSTATUS status;
	int d_fnum = -1;
	int fnum = -1;
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* Create the Directory */
	status = create_directory_handle(cli->tree, fname, &d_fnum);
	smbcli_close(cli->tree, d_fnum);	

	/* Prepare to open the file over the directory. */
	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 1024*1024;
	io.openx.in.timeout = 0;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_IS_A_DIRECTORY);
	fnum = io.openx.out.file.fnum;

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/* A little torture test to expose a race condition in Samba 3.0.20 ... :-) */

static bool test_raw_open_multi(struct torture_context *tctx, struct smbcli_state *cli_ignored)
{
	struct smbcli_state *cli;
	TALLOC_CTX *mem_ctx = talloc_init("torture_test_oplock_multi");
	const char *fname = "\\test_oplock.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smbcli_state **clients;
	struct smbcli_request **requests;
	union smb_open *ios;
	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);
	int i, num_files = 3;
	int num_ok = 0;
	int num_collision = 0;
	
	clients = talloc_array(mem_ctx, struct smbcli_state *, num_files);
	requests = talloc_array(mem_ctx, struct smbcli_request *, num_files);
	ios = talloc_array(mem_ctx, union smb_open, num_files);
	if ((tctx->ev == NULL) || (clients == NULL) || (requests == NULL) ||
	    (ios == NULL)) {
		torture_result(tctx, TORTURE_FAIL, "(%s): talloc failed\n",
				__location__);
		return false;
	}

	if (!torture_open_connection_share(mem_ctx, &cli, tctx, host, share, tctx->ev)) {
		return false;
	}

	cli->tree->session->transport->options.request_timeout = 60;

	for (i=0; i<num_files; i++) {
		if (!torture_open_connection_share(mem_ctx, &(clients[i]),
						   tctx, host, share, tctx->ev)) {
			torture_result(tctx, TORTURE_FAIL,
				       "(%s): Could not open %d'th connection\n",
				       __location__, i);
			return false;
		}
		clients[i]->tree->session->transport->options.request_timeout = 60;
	}

	/* cleanup */
	smbcli_unlink(cli->tree, fname);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.flags = 0;

	for (i=0; i<num_files; i++) {
		ios[i] = io;
		requests[i] = smb_raw_open_send(clients[i]->tree, &ios[i]);
		if (requests[i] == NULL) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s): could not send %d'th request\n",
				__location__, i);
			return false;
		}
	}

	torture_comment(tctx, "waiting for replies\n");
	while (1) {
		bool unreplied = false;
		for (i=0; i<num_files; i++) {
			if (requests[i] == NULL) {
				continue;
			}
			if (requests[i]->state < SMBCLI_REQUEST_DONE) {
				unreplied = true;
				break;
			}
			status = smb_raw_open_recv(requests[i], mem_ctx,
						   &ios[i]);

			torture_comment(tctx, "File %d returned status %s\n", i,
				  nt_errstr(status));

			if (NT_STATUS_IS_OK(status)) {
				num_ok += 1;
			} 

			if (NT_STATUS_EQUAL(status,
					    NT_STATUS_OBJECT_NAME_COLLISION)) {
				num_collision += 1;
			}

			requests[i] = NULL;
		}
		if (!unreplied) {
			break;
		}

		if (event_loop_once(tctx->ev) != 0) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s): event_loop_once failed\n", __location__);
			return false;
		}
	}

	if ((num_ok != 1) || (num_ok + num_collision != num_files)) {
		ret = false;
	}

	for (i=0; i<num_files; i++) {
		torture_close_connection(clients[i]);
	}
	talloc_free(mem_ctx);
	return ret;
}

/*
  test opening for delete on a read-only attribute file.
*/
static bool test_open_for_delete(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_open_for_delete.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_READONLY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* Create the readonly file. */

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	io.ntcreatex.in.create_options = 0;
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	smbcli_close(cli->tree, fnum);

	/* Now try and open for delete only - should succeed. */
	io.ntcreatex.in.access_mask = SEC_STD_DELETE;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_unlink(cli->tree, fname);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
  test chained RAW_OPEN_NTCREATEX_READX
  Send chained NTCREATEX_READX on a file that doesn't exist, then create
  the file and try again.
*/
static bool test_chained_ntcreatex_readx(struct torture_context *tctx, struct smbcli_state *cli)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	union smb_open io;
	const char *fname = BASEDIR "\\torture_chained.txt";
	NTSTATUS status;
	int fnum = -1;
	bool ret = true;
	const char *buf = "test";
	char buf2[4];

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Checking RAW_NTCREATEX_READX chained on "
			      "non-existant file \n");

	/* ntcreatex parameters */
	io.generic.level = RAW_OPEN_NTCREATEX_READX;
	io.ntcreatexreadx.in.flags = 0;
	io.ntcreatexreadx.in.root_fid.fnum = 0;
	io.ntcreatexreadx.in.access_mask = SEC_FILE_READ_DATA;
	io.ntcreatexreadx.in.alloc_size = 0;
	io.ntcreatexreadx.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatexreadx.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatexreadx.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatexreadx.in.create_options = 0;
	io.ntcreatexreadx.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io.ntcreatexreadx.in.security_flags = 0;
	io.ntcreatexreadx.in.fname = fname;

	/* readx parameters */
	io.ntcreatexreadx.in.offset = 0;
	io.ntcreatexreadx.in.mincnt = sizeof(buf);
	io.ntcreatexreadx.in.maxcnt = sizeof(buf);
	io.ntcreatexreadx.in.remaining = 0;
	io.ntcreatexreadx.out.data = (uint8_t *)buf2;

	/* try to open the non-existant file */
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	fnum = io.ntcreatexreadx.out.file.fnum;

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	torture_comment(tctx, "Checking RAW_NTCREATEX_READX chained on "
			      "existing file \n");

	fnum = create_complex_file(cli, mem_ctx, fname);
	smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));
	smbcli_close(cli->tree, fnum);

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatexreadx.out.file.fnum;

	if (memcmp(buf, buf2, sizeof(buf)) != 0) {
		torture_result(tctx, TORTURE_FAIL,
			"(%s): wrong data in reply buffer\n", __location__);
		ret = false;
	}

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	talloc_free(mem_ctx);

	return ret;
}

static bool test_ntcreatex_opendisp_dir(struct torture_context *tctx,
					struct smbcli_state *cli)
{
	const char *dname = BASEDIR "\\torture_ntcreatex_opendisp_dir";
	NTSTATUS status;
	bool ret = true;
	int i;
	struct {
		uint32_t open_disp;
		bool dir_exists;
		NTSTATUS correct_status;
	} open_funcs_dir[] = {
		{ NTCREATEX_DISP_SUPERSEDE,     true,  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_SUPERSEDE,     false, NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OPEN,          true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN,          false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE,        true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ NTCREATEX_DISP_CREATE,        false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF,       true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF,       false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE,     true,  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OVERWRITE,     false, NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OVERWRITE_IF,  true,  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OVERWRITE_IF,  false, NT_STATUS_INVALID_PARAMETER },
		{ 6,                            true,  NT_STATUS_INVALID_PARAMETER },
		{ 6,                            false, NT_STATUS_INVALID_PARAMETER },
	};
	union smb_open io;

	ZERO_STRUCT(io);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.fname = dname;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	smbcli_rmdir(cli->tree, dname);
	smbcli_unlink(cli->tree, dname);

	/* test the open disposition for directories */
	torture_comment(tctx, "Testing open dispositions for directories...\n");

	for (i=0; i<ARRAY_SIZE(open_funcs_dir); i++) {
		if (open_funcs_dir[i].dir_exists) {
			status = smbcli_mkdir(cli->tree, dname);
			if (!NT_STATUS_IS_OK(status)) {
				torture_result(tctx, TORTURE_FAIL,
					"(%s): Failed to make directory "
					"%s - %s\n", __location__, dname,
					smbcli_errstr(cli->tree));
				ret = false;
				goto done;
			}
		}

		io.ntcreatex.in.open_disposition = open_funcs_dir[i].open_disp;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs_dir[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d dir_exists=%d open_disp=%d)\n",
				__location__, nt_errstr(status),
				nt_errstr(open_funcs_dir[i].correct_status),
				i, (int)open_funcs_dir[i].dir_exists,
				(int)open_funcs_dir[i].open_disp);
			ret = false;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs_dir[i].dir_exists) {
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
			smbcli_rmdir(cli->tree, dname);
		}
	}

done:
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/**
 * Test what happens when trying to open a file with directory parameters and
 * vice-versa.  Also test that NTCREATEX_OPTIONS_DIRECTORY is treated as
 * mandatory and FILE_ATTRIBUTE_DIRECTORY is advisory for directory
 * creation/opening.
 */
static bool test_ntcreatexdir(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	const char *dname = BASEDIR "\\torture_ntcreatex_dir";
	NTSTATUS status;
	int i;

	struct {
		uint32_t open_disp;
		uint32_t file_attr;
		uint32_t create_options;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ NTCREATEX_DISP_SUPERSEDE, 	0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OPEN, 	        0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE, 	0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	0, NTCREATEX_OPTIONS_DIRECTORY,
		  NT_STATUS_INVALID_PARAMETER },
		{ NTCREATEX_DISP_SUPERSEDE, 	FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE, 	FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	FILE_ATTRIBUTE_DIRECTORY, 0,
		  NT_STATUS_OK },

	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* setup some base params. */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	 * Test the validity checking for create dispositions, which is done
	 * against the requested parameters rather than what's actually on
	 * disk.
	 */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		io.ntcreatex.in.open_disposition = open_funcs[i].open_disp;
		io.ntcreatex.in.file_attr = open_funcs[i].file_attr;
		io.ntcreatex.in.create_options = open_funcs[i].create_options;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s) incorrect status %s should be %s "
				"(i=%d open_disp=%d)\n",
				__location__, nt_errstr(status),
				nt_errstr(open_funcs[i].correct_status),
				i, (int)open_funcs[i].open_disp);
			return false;
		}
		/* Close and delete the file. */
		if (NT_STATUS_IS_OK(status)) {
			if (open_funcs[i].create_options != 0) {
				/* out attrib should be a directory. */
				torture_assert_int_equal(tctx,
				    io.ntcreatex.out.attrib,
				    FILE_ATTRIBUTE_DIRECTORY, "should have "
				    "created a directory");

				smbcli_close(cli->tree,
				    io.ntcreatex.out.file.fnum);

				/* Make sure unlink fails. */
				status = smbcli_unlink(cli->tree, fname);
				torture_assert_ntstatus_equal(tctx, status,
				    NT_STATUS_FILE_IS_A_DIRECTORY,
				    "unlink should fail for a directory");

				status = smbcli_rmdir(cli->tree, fname);
				torture_assert_ntstatus_ok(tctx, status,
				    "rmdir failed");
			} else {
				torture_assert_int_equal(tctx,
				    io.ntcreatex.out.attrib,
				    FILE_ATTRIBUTE_ARCHIVE, "should not have "
				    "created a directory");

				smbcli_close(cli->tree,
				    io.ntcreatex.out.file.fnum);

				/* Make sure rmdir fails. */
				status = smbcli_rmdir(cli->tree, fname);
				torture_assert_ntstatus_equal(tctx, status,
				    NT_STATUS_NOT_A_DIRECTORY,
				    "rmdir should fail for a file");

				status = smbcli_unlink(cli->tree, fname);
				torture_assert_ntstatus_ok(tctx, status,
				    "unlink failed");
			}
		}
	}

	/* Create a file. */
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create file.");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Try and open the file with file_attr_dir and check the error. */
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;

	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_ok(tctx, status, "FILE_ATTRIBUTE_DIRECTORY "
	    "doesn't produce a hard failure.");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Try and open file with createx_option_dir and check the error. */
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;

	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_NOT_A_DIRECTORY,
	    "NTCREATEX_OPTIONS_DIRECTORY will a file from being opened.");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Delete the file and move onto directory testing. */
	smbcli_unlink(cli->tree, fname);

	/* Now try some tests on a directory. */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create dir.");

	/* out attrib should be a directory. */
	torture_assert_int_equal(tctx, io.ntcreatex.out.attrib,
	    FILE_ATTRIBUTE_DIRECTORY, "should have created a directory");

	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Try and open it with normal attr and check the error. */
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;

	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_ok(tctx, status, "FILE_ATTRIBUTE_NORMAL "
	    "doesn't produce a hard failure.");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Try and open it with file create_options and check the error. */
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;

	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_equal(tctx, status,
	    NT_STATUS_FILE_IS_A_DIRECTORY,
	    "NTCREATEX_OPTIONS_NON_DIRECTORY_FILE should be returned ");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	smbcli_deltree(cli->tree, BASEDIR);

	return true;
}

/* basic testing of all RAW_OPEN_* calls
*/
struct torture_suite *torture_raw_open(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "open");

	torture_suite_add_1smb_test(suite, "brlocked", test_ntcreatex_brlocked);
	torture_suite_add_1smb_test(suite, "open", test_open);
	torture_suite_add_1smb_test(suite, "open-multi", test_raw_open_multi);
	torture_suite_add_1smb_test(suite, "openx", test_openx);
	torture_suite_add_1smb_test(suite, "ntcreatex", test_ntcreatex);
	torture_suite_add_1smb_test(suite, "nttrans-create", test_nttrans_create);
	torture_suite_add_1smb_test(suite, "t2open", test_t2open);
	torture_suite_add_1smb_test(suite, "mknew", test_mknew);
	torture_suite_add_1smb_test(suite, "create", test_create);
	torture_suite_add_1smb_test(suite, "ctemp", test_ctemp);
	torture_suite_add_1smb_test(suite, "chained-openx", test_chained);
	torture_suite_add_1smb_test(suite, "chained-ntcreatex", test_chained_ntcreatex_readx);
	torture_suite_add_1smb_test(suite, "no-leading-slash", test_no_leading_slash);
	torture_suite_add_1smb_test(suite, "openx-over-dir", test_openx_over_dir);
	torture_suite_add_1smb_test(suite, "open-for-delete", test_open_for_delete);
	torture_suite_add_1smb_test(suite, "opendisp-dir", test_ntcreatex_opendisp_dir);
	torture_suite_add_1smb_test(suite, "ntcreatedir", test_ntcreatexdir);

	return suite;
}
