/* 
   Unix SMB/CIFS implementation.

   test alternate data streams

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
#include "system/locale.h"
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\teststreams"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%s) Incorrect value %s=%d - should be %d\n", \
		       __location__, #v, (int)v, (int)correct); \
		ret = false; \
	}} while (0)

#define CHECK_NTTIME(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%s) Incorrect value %s=%llu - should be %llu\n", \
		       __location__, #v, (unsigned long long)v, \
		       (unsigned long long)correct); \
		ret = false; \
	}} while (0)

#define CHECK_STR(v, correct) do { \
	bool ok; \
	if ((v) && !(correct)) { \
		ok = false; \
	} else if (!(v) && (correct)) { \
		ok = false; \
	} else if (!(v) && !(correct)) { \
		ok = true; \
	} else if (strcmp((v), (correct)) == 0) { \
		ok = true; \
	} else { \
		ok = false; \
	} \
	if (!ok) { \
		printf("(%s) Incorrect value %s='%s' - should be '%s'\n", \
		       __location__, #v, (v)?(v):"NULL", \
		       (correct)?(correct):"NULL"); \
		ret = false; \
	}} while (0)

/*
  check that a stream has the right contents
*/
static bool check_stream(struct smbcli_state *cli, const char *location,
			 TALLOC_CTX *mem_ctx,
			 const char *fname, const char *sname, 
			 const char *value)
{
	int fnum;
	const char *full_name;
	uint8_t *buf;
	ssize_t ret;

	full_name = talloc_asprintf(mem_ctx, "%s:%s", fname, sname);

	fnum = smbcli_open(cli->tree, full_name, O_RDONLY, DENY_NONE);

	if (value == NULL) {
		if (fnum != -1) {
			printf("(%s) should have failed stream open of %s\n",
			       location, full_name);
			return false;
		}
		return true;
	}
	    
	if (fnum == -1) {
		printf("(%s) Failed to open stream '%s' - %s\n",
		       location, full_name, smbcli_errstr(cli->tree));
		return false;
	}

	buf = talloc_array(mem_ctx, uint8_t, strlen(value)+11);
	
	ret = smbcli_read(cli->tree, fnum, buf, 0, strlen(value)+11);
	if (ret != strlen(value)) {
		printf("(%s) Failed to read %lu bytes from stream '%s' - got %d\n",
		       location, (long)strlen(value), full_name, (int)ret);
		return false;
	}

	if (memcmp(buf, value, strlen(value)) != 0) {
		printf("(%s) Bad data in stream\n", location);
		return false;
	}

	smbcli_close(cli->tree, fnum);
	return true;
}

static int qsort_string(const void *v1, const void *v2)
{
	char * const *s1 = v1;
	char * const *s2 = v2;
	return strcmp(*s1, *s2);
}

static int qsort_stream(const void *v1, const void *v2)
{
	const struct stream_struct * s1 = v1;
	const struct stream_struct * s2 = v2;
	return strcmp(s1->stream_name.s, s2->stream_name.s);
}

static bool check_stream_list(struct smbcli_state *cli, const char *fname,
			      int num_exp, const char **exp)
{
	union smb_fileinfo finfo;
	NTSTATUS status;
	int i;
	TALLOC_CTX *tmp_ctx = talloc_new(cli);
	char **exp_sort;
	struct stream_struct *stream_sort;
	bool ret = false;

	finfo.generic.level = RAW_FILEINFO_STREAM_INFO;
	finfo.generic.in.file.path = fname;

	status = smb_raw_pathinfo(cli->tree, tmp_ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "(%s) smb_raw_pathinfo failed: %s\n",
			  __location__, nt_errstr(status));
		goto fail;
	}

	if (finfo.stream_info.out.num_streams != num_exp) {
		d_fprintf(stderr, "(%s) expected %d streams, got %d\n",
			  __location__, num_exp,
			  finfo.stream_info.out.num_streams);
		goto fail;
	}

	if (num_exp == 0) {
		ret = true;
		goto fail;
	}

	exp_sort = talloc_memdup(tmp_ctx, exp, num_exp * sizeof(*exp));

	if (exp_sort == NULL) {
		goto fail;
	}

	qsort(exp_sort, num_exp, sizeof(*exp_sort), qsort_string);

	stream_sort = talloc_memdup(tmp_ctx, finfo.stream_info.out.streams,
				    finfo.stream_info.out.num_streams *
				    sizeof(*stream_sort));

	if (stream_sort == NULL) {
		goto fail;
	}

	qsort(stream_sort, finfo.stream_info.out.num_streams,
	      sizeof(*stream_sort), qsort_stream);

	for (i=0; i<num_exp; i++) {
		if (strcmp(exp_sort[i], stream_sort[i].stream_name.s) != 0) {
			d_fprintf(stderr, "(%s) expected stream name %s, got "
				  "%s\n", __location__, exp_sort[i],
				  stream_sort[i].stream_name.s);
			goto fail;
		}
	}

	ret = true;
 fail:
	talloc_free(tmp_ctx);
	return ret;
}

/*
  test bahavior of streams on directories
*/
static bool test_stream_dir(struct torture_context *tctx,
			   struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream.txt";
	const char *sname1;
	bool ret = true;
	const char *basedir_data;

	basedir_data = talloc_asprintf(mem_ctx, "%s::$DATA", BASEDIR);
	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "Stream One");

	printf("(%s) opening non-existant directory stream\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_NOT_A_DIRECTORY);

	printf("(%s) opening basedir  stream\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = basedir_data;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_NOT_A_DIRECTORY);

	printf("(%s) opening basedir ::$DATA stream\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0x10;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = basedir_data;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_IS_A_DIRECTORY);

	printf("(%s) list the streams on the basedir\n", __location__);
	ret &= check_stream_list(cli, BASEDIR, 0, NULL);
done:
	return ret;
}

/*
  test basic behavior of streams on directories
*/
static bool test_stream_io(struct torture_context *tctx,
			   struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream.txt";
	const char *sname1, *sname2;
	bool ret = true;
	int fnum = -1;
	ssize_t retsize;

	const char *one[] = { "::$DATA" };
	const char *two[] = { "::$DATA", ":Second Stream:$DATA" };
	const char *three[] = { "::$DATA", ":Stream One:$DATA",
				":Second Stream:$DATA" };

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "Stream One");
	sname2 = talloc_asprintf(mem_ctx, "%s:%s:$DaTa", fname, "Second Stream");

	printf("(%s) creating a stream on a non-existant file\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One", NULL);

	printf("(%s) check that open of base file is allowed\n", __location__);
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("(%s) writing to stream\n", __location__);
	retsize = smbcli_write(cli->tree, fnum, 0, "test data", 0, 9);
	CHECK_VALUE(retsize, 9);

	smbcli_close(cli->tree, fnum);

	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One", "test data");

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.fname = sname1;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("(%s) modifying stream\n", __location__);
	retsize = smbcli_write(cli->tree, fnum, 0, "MORE DATA ", 5, 10);
	CHECK_VALUE(retsize, 10);

	smbcli_close(cli->tree, fnum);

	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One:$FOO", NULL);

	printf("(%s) creating a stream2 on a existing file\n", __location__);
	io.ntcreatex.in.fname = sname2;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("(%s) modifying stream\n", __location__);
	retsize = smbcli_write(cli->tree, fnum, 0, "SECOND STREAM", 0, 13);
	CHECK_VALUE(retsize, 13);

	smbcli_close(cli->tree, fnum);

	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One", "test MORE DATA ");
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One:$DATA", "test MORE DATA ");
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Stream One:", NULL);
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Second Stream", "SECOND STREAM");
	if (!torture_setting_bool(tctx, "samba4", false)) {
		ret &= check_stream(cli, __location__, mem_ctx, fname,
				    "SECOND STREAM:$DATA", "SECOND STREAM");
	}
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Second Stream:$DATA", "SECOND STREAM");
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Second Stream:", NULL);
	ret &= check_stream(cli, __location__, mem_ctx, fname, "Second Stream:$FOO", NULL);

	check_stream_list(cli, fname, 3, three);

	printf("(%s) deleting stream\n", __location__);
	status = smbcli_unlink(cli->tree, sname1);
	CHECK_STATUS(status, NT_STATUS_OK);

	check_stream_list(cli, fname, 2, two);

	printf("(%s) delete a stream via delete-on-close\n", __location__);
	io.ntcreatex.in.fname = sname2;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.access_mask = SEC_STD_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	
	smbcli_close(cli->tree, fnum);
	status = smbcli_unlink(cli->tree, sname2);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	check_stream_list(cli, fname, 1, one);

	if (!torture_setting_bool(tctx, "samba4", false)) {
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
		io.ntcreatex.in.fname = sname1;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
		io.ntcreatex.in.fname = sname2;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	}

	printf("(%s) deleting file\n", __location__);
	status = smbcli_unlink(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/*
  test stream sharemodes
*/
static bool test_stream_sharemodes(struct torture_context *tctx,
				   struct smbcli_state *cli,
				   TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream.txt";
	const char *sname1, *sname2;
	bool ret = true;
	int fnum1 = -1;
	int fnum2 = -1;

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "Stream One");
	sname2 = talloc_asprintf(mem_ctx, "%s:%s:$DaTa", fname, "Second Stream");

	printf("(%s) testing stream share mode conflicts\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	/*
	 * A different stream does not give a sharing violation
	 */

	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	/*
	 * ... whereas the same stream does with unchanged access/share_access
	 * flags
	 */

	io.ntcreatex.in.fname = sname1;
	io.ntcreatex.in.open_disposition = 0;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

done:
	if (fnum1 != -1) smbcli_close(cli->tree, fnum1);
	if (fnum2 != -1) smbcli_close(cli->tree, fnum2);
	status = smbcli_unlink(cli->tree, fname);
	return ret;
}

/* 
 *  Test FILE_SHARE_DELETE on streams
 *
 * A stream opened with !FILE_SHARE_DELETE prevents the main file to be opened
 * with SEC_STD_DELETE.
 *
 * The main file opened with !FILE_SHARE_DELETE does *not* prevent a stream to
 * be opened with SEC_STD_DELETE.
 *
 * A stream held open with FILE_SHARE_DELETE allows the file to be
 * deleted. After the main file is deleted, access to the open file descriptor
 * still works, but all name-based access to both the main file as well as the
 * stream is denied with DELETE ending.
 *
 * This means, an open of the main file with SEC_STD_DELETE should walk all
 * streams and also open them with SEC_STD_DELETE. If any of these opens gives
 * SHARING_VIOLATION, the main open fails.
 *
 * Closing the main file after delete_on_close has been set does not really
 * unlink it but leaves the corresponding share mode entry with
 * delete_on_close being set around until all streams are closed.
 *
 * Opening a stream must also look at the main file's share mode entry, look
 * at the delete_on_close bit and potentially return DELETE_PENDING.
 */

static bool test_stream_delete(struct torture_context *tctx,
			       struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream.txt";
	const char *sname1;
	bool ret = true;
	int fnum = -1;
	uint8_t buf[9];
	ssize_t retsize;
	union smb_fileinfo finfo;

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "Stream One");

	printf("(%s) opening non-existant file stream\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	retsize = smbcli_write(cli->tree, fnum, 0, "test data", 0, 9);
	CHECK_VALUE(retsize, 9);

	/*
	 * One stream opened without FILE_SHARE_DELETE prevents the main file
	 * to be deleted or even opened with DELETE access
	 */

	status = smbcli_unlink(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.access_mask = SEC_STD_DELETE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	smbcli_close(cli->tree, fnum);

	/*
	 * ... but unlink works if a stream is opened with FILE_SHARE_DELETE
	 */

	io.ntcreatex.in.fname = sname1;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	status = smbcli_unlink(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * file access still works on the stream while the main file is closed
	 */

	retsize = smbcli_read(cli->tree, fnum, buf, 0, 9);
	CHECK_VALUE(retsize, 9);

	finfo.generic.level = RAW_FILEINFO_STANDARD;
	finfo.generic.in.file.path = fname;

	/*
	 * name-based access to both the main file and the stream does not
	 * work anymore but gives DELETE_PENDING
	 */

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_DELETE_PENDING);

	/*
	 * older S3 doesn't do this
	 */
	finfo.generic.in.file.path = sname1;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_DELETE_PENDING);

	/*
	 * fd-based qfileinfo on the stream still works, the stream does not
	 * have the delete-on-close bit set. This could mean that open on the
	 * stream first opens the main file
	 */

	finfo.all_info.level = RAW_FILEINFO_ALL_INFO;
	finfo.all_info.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	/* w2k and w2k3 return 0 and w2k8 returns 1
	CHECK_VALUE(finfo.all_info.out.delete_pending, 0);
	*/

	smbcli_close(cli->tree, fnum);

	/*
	 * After closing the stream the file is really gone.
	 */

	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA
		|SEC_STD_DELETE;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
done:
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	return ret;
}

/*
  test stream names
*/
static bool test_stream_names(struct torture_context *tctx,
			      struct smbcli_state *cli,
			      TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	union smb_fileinfo finfo;
	union smb_fileinfo stinfo;
	union smb_setfileinfo sinfo;
	const char *fname = BASEDIR "\\stream_names.txt";
	const char *sname1, *sname1b, *sname1c, *sname1d;
	const char *sname2, *snamew, *snamew2;
	const char *snamer1, *snamer2;
	bool ret = true;
	int fnum1 = -1;
	int fnum2 = -1;
	int fnum3 = -1;
	int i;
	const char *four[4] = {
		"::$DATA",
		":\x05Stream\n One:$DATA",
		":MStream Two:$DATA",
		":?Stream*:$DATA"
	};
	const char *five1[5] = {
		"::$DATA",
		":\x05Stream\n One:$DATA",
		":BeforeRename:$DATA",
		":MStream Two:$DATA",
		":?Stream*:$DATA"
	};
	const char *five2[5] = {
		"::$DATA",
		":\x05Stream\n One:$DATA",
		":AfterRename:$DATA",
		":MStream Two:$DATA",
		":?Stream*:$DATA"
	};

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "\x05Stream\n One");
	sname1b = talloc_asprintf(mem_ctx, "%s:", sname1);
	sname1c = talloc_asprintf(mem_ctx, "%s:$FOO", sname1);
	sname1d = talloc_asprintf(mem_ctx, "%s:?D*a", sname1);
	sname2 = talloc_asprintf(mem_ctx, "%s:%s:$DaTa", fname, "MStream Two");
	snamew = talloc_asprintf(mem_ctx, "%s:%s:$DATA", fname, "?Stream*");
	snamew2 = talloc_asprintf(mem_ctx, "%s\\stream*:%s:$DATA", BASEDIR, "?Stream*");
	snamer1 = talloc_asprintf(mem_ctx, "%s:%s:$DATA", fname, "BeforeRename");
	snamer2 = talloc_asprintf(mem_ctx, "%s:%s:$DATA", fname, "AfterRename");

	printf("(%s) testing stream names\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	/*
	 * A different stream does not give a sharing violation
	 */

	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	/*
	 * ... whereas the same stream does with unchanged access/share_access
	 * flags
	 */

	io.ntcreatex.in.fname = sname1;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_SUPERSEDE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.ntcreatex.in.fname = sname1b;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);

	io.ntcreatex.in.fname = sname1c;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
		/* w2k returns INVALID_PARAMETER */
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	} else {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);
	}

	io.ntcreatex.in.fname = sname1d;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
		/* w2k returns INVALID_PARAMETER */
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	} else {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);
	}

	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.ntcreatex.in.fname = snamew;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum3 = io.ntcreatex.out.file.fnum;

	io.ntcreatex.in.fname = snamew2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);

	ret &= check_stream_list(cli, fname, 4, four);

	smbcli_close(cli->tree, fnum1);
	smbcli_close(cli->tree, fnum2);
	smbcli_close(cli->tree, fnum3);

	if (torture_setting_bool(tctx, "samba4", true)) {
		goto done;
	}

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_stream_list(cli, fname, 4, four);

	for (i=0; i < 4; i++) {
		NTTIME write_time;
		uint64_t stream_size;
		char *path = talloc_asprintf(tctx, "%s%s",
					     fname, four[i]);

		char *rpath = talloc_strdup(path, path);
		char *p = strrchr(rpath, ':');
		/* eat :$DATA */
		*p = 0;
		p--;
		if (*p == ':') {
			/* eat ::$DATA */
			*p = 0;
		}
		printf("(%s): i[%u][%s]\n", __location__, i, path);
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE |
					      SEC_FILE_WRITE_ATTRIBUTE |
					    SEC_RIGHTS_FILE_ALL;
		io.ntcreatex.in.fname = path;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		fnum1 = io.ntcreatex.out.file.fnum;

		finfo.generic.level = RAW_FILEINFO_ALL_INFO;
		finfo.generic.in.file.path = fname;
		status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
		CHECK_STATUS(status, NT_STATUS_OK);

		stinfo.generic.level = RAW_FILEINFO_ALL_INFO;
		stinfo.generic.in.file.fnum = fnum1;
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &stinfo);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!torture_setting_bool(tctx, "samba3", false)) {
			CHECK_NTTIME(stinfo.all_info.out.create_time,
				     finfo.all_info.out.create_time);
			CHECK_NTTIME(stinfo.all_info.out.access_time,
				     finfo.all_info.out.access_time);
			CHECK_NTTIME(stinfo.all_info.out.write_time,
				     finfo.all_info.out.write_time);
			CHECK_NTTIME(stinfo.all_info.out.change_time,
				     finfo.all_info.out.change_time);
		}
		CHECK_VALUE(stinfo.all_info.out.attrib,
			    finfo.all_info.out.attrib);
		CHECK_VALUE(stinfo.all_info.out.size,
			    finfo.all_info.out.size);
		CHECK_VALUE(stinfo.all_info.out.delete_pending,
			    finfo.all_info.out.delete_pending);
		CHECK_VALUE(stinfo.all_info.out.directory,
			    finfo.all_info.out.directory);
		CHECK_VALUE(stinfo.all_info.out.ea_size,
			    finfo.all_info.out.ea_size);

		stinfo.generic.level = RAW_FILEINFO_NAME_INFO;
		stinfo.generic.in.file.fnum = fnum1;
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &stinfo);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!torture_setting_bool(tctx, "samba3", false)) {
			CHECK_STR(rpath, stinfo.name_info.out.fname.s);
		}

		write_time = finfo.all_info.out.write_time;
		write_time += i*1000000;
		write_time /= 1000000;
		write_time *= 1000000;

		ZERO_STRUCT(sinfo);
		sinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFO;
		sinfo.basic_info.in.file.fnum = fnum1;
		sinfo.basic_info.in.write_time = write_time;
		sinfo.basic_info.in.attrib = stinfo.all_info.out.attrib;
		status = smb_raw_setfileinfo(cli->tree, &sinfo);
		CHECK_STATUS(status, NT_STATUS_OK);

		stream_size = i*8192;

		ZERO_STRUCT(sinfo);
		sinfo.end_of_file_info.level = RAW_SFILEINFO_END_OF_FILE_INFO;
		sinfo.end_of_file_info.in.file.fnum = fnum1;
		sinfo.end_of_file_info.in.size = stream_size;
		status = smb_raw_setfileinfo(cli->tree, &sinfo);
		CHECK_STATUS(status, NT_STATUS_OK);

		stinfo.generic.level = RAW_FILEINFO_ALL_INFO;
		stinfo.generic.in.file.fnum = fnum1;
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &stinfo);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!torture_setting_bool(tctx, "samba3", false)) {
			CHECK_NTTIME(stinfo.all_info.out.write_time,
				     write_time);
			CHECK_VALUE(stinfo.all_info.out.attrib,
				    finfo.all_info.out.attrib);
		}
		CHECK_VALUE(stinfo.all_info.out.size,
			    stream_size);
		CHECK_VALUE(stinfo.all_info.out.delete_pending,
			    finfo.all_info.out.delete_pending);
		CHECK_VALUE(stinfo.all_info.out.directory,
			    finfo.all_info.out.directory);
		CHECK_VALUE(stinfo.all_info.out.ea_size,
			    finfo.all_info.out.ea_size);

		ret &= check_stream_list(cli, fname, 4, four);

		smbcli_close(cli->tree, fnum1);
		talloc_free(path);
	}

	printf("(%s): testing stream renames\n", __location__);
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE |
				      SEC_FILE_WRITE_ATTRIBUTE |
				    SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.fname = snamer1;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	ret &= check_stream_list(cli, fname, 5, five1);

	ZERO_STRUCT(sinfo);
	sinfo.rename_information.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sinfo.rename_information.in.file.fnum = fnum1;
	sinfo.rename_information.in.overwrite = true;
	sinfo.rename_information.in.root_fid = 0;
	sinfo.rename_information.in.new_name = ":AfterRename:$DATA";
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_stream_list(cli, fname, 5, five2);

	ZERO_STRUCT(sinfo);
	sinfo.rename_information.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sinfo.rename_information.in.file.fnum = fnum1;
	sinfo.rename_information.in.overwrite = false;
	sinfo.rename_information.in.root_fid = 0;
	sinfo.rename_information.in.new_name = ":MStream Two:$DATA";
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_COLLISION);

	ret &= check_stream_list(cli, fname, 5, five2);

	ZERO_STRUCT(sinfo);
	sinfo.rename_information.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sinfo.rename_information.in.file.fnum = fnum1;
	sinfo.rename_information.in.overwrite = true;
	sinfo.rename_information.in.root_fid = 0;
	sinfo.rename_information.in.new_name = ":MStream Two:$DATA";
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	ret &= check_stream_list(cli, fname, 5, five2);

	/* TODO: we need to test more rename combinations */

done:
	if (fnum1 != -1) smbcli_close(cli->tree, fnum1);
	if (fnum2 != -1) smbcli_close(cli->tree, fnum2);
	if (fnum3 != -1) smbcli_close(cli->tree, fnum3);
	status = smbcli_unlink(cli->tree, fname);
	return ret;
}

/*
  test stream names
*/
static bool test_stream_names2(struct torture_context *tctx,
			       struct smbcli_state *cli,
			       TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream_names2.txt";
	bool ret = true;
	int fnum1 = -1;
	uint8_t i;

	printf("(%s) testing stream names\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	for (i=0x01; i < 0x7F; i++) {
		char *path = talloc_asprintf(tctx, "%s:Stream%c0x%02X:$DATA",
					     fname, i, i);
		NTSTATUS expected;

		switch (i) {
		case '/':/*0x2F*/
		case ':':/*0x3A*/
		case '\\':/*0x5C*/
			expected = NT_STATUS_OBJECT_NAME_INVALID;
			break;
		default:
			expected = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			break;
		}


		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		io.ntcreatex.in.fname = path;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		if (!NT_STATUS_EQUAL(status, expected)) {
			printf("(%s) %s:Stream%c0x%02X:$DATA%s => expected[%s]\n",
				__location__, fname, isprint(i)?(char)i:' ', i,
				isprint(i)?"":" (not printable)",
				nt_errstr(expected));
		}
		CHECK_STATUS(status, expected);

		talloc_free(path);
	}

done:
	if (fnum1 != -1) smbcli_close(cli->tree, fnum1);
	status = smbcli_unlink(cli->tree, fname);
	return ret;
}

#define CHECK_CALL_FNUM(call, rightstatus) do { \
        check_fnum = true; \
        call_name = #call; \
        sfinfo.generic.level = RAW_SFILEINFO_ ## call; \
        sfinfo.generic.in.file.fnum = fnum; \
        status = smb_raw_setfileinfo(cli->tree, &sfinfo); \
        if (!NT_STATUS_EQUAL(status, rightstatus)) { \
                printf("(%s) %s - %s (should be %s)\n", __location__, #call, \
                        nt_errstr(status), nt_errstr(rightstatus)); \
                ret = false; \
        } \
        finfo1.generic.level = RAW_FILEINFO_ALL_INFO; \
        finfo1.generic.in.file.fnum = fnum; \
        status2 = smb_raw_fileinfo(cli->tree, tctx, &finfo1); \
        if (!NT_STATUS_IS_OK(status2)) { \
                printf("(%s) %s pathinfo - %s\n", __location__, #call, nt_errstr(status)); \
                ret = false; \
        }} while (0)

/*
  test stream renames
*/
static bool test_stream_rename(struct torture_context *tctx,
				   struct smbcli_state *cli,
				   TALLOC_CTX *mem_ctx)
{
	NTSTATUS status, status2;
	union smb_open io;
	const char *fname = BASEDIR "\\stream_rename.txt";
	const char *sname1, *sname2;
	union smb_fileinfo finfo1;
	union smb_setfileinfo sfinfo;
	bool ret = true;
	int fnum = -1;
	bool check_fnum;
	const char *call_name;

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname, "Stream One");
	sname2 = talloc_asprintf(mem_ctx, "%s:%s:$DaTa", fname, "Second Stream");

	printf("(%s) testing stream renames\n", __location__);
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE |
				      SEC_FILE_WRITE_ATTRIBUTE |
				    SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;

	/* Create two streams. */
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	if (fnum != -1) smbcli_close(cli->tree, fnum);

	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	if (fnum != -1) smbcli_close(cli->tree, fnum);

	/*
	 * Open the second stream.
	 */

	io.ntcreatex.in.access_mask = SEC_STD_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/*
	 * Now rename the second stream onto the first.
	 */

	ZERO_STRUCT(sfinfo);

	sfinfo.rename_information.in.overwrite = 1;
	sfinfo.rename_information.in.root_fid  = 0;
	sfinfo.rename_information.in.new_name  = ":Stream One";
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);

done:
	if (fnum != -1) smbcli_close(cli->tree, fnum);
	status = smbcli_unlink(cli->tree, fname);
	return ret;
}

static bool test_stream_rename2(struct torture_context *tctx,
			       struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname1 = BASEDIR "\\stream.txt";
	const char *fname2 = BASEDIR "\\stream2.txt";
	const char *stream_name1 = ":Stream One:$DATA";
	const char *stream_name2 = ":Stream Two:$DATA";
	const char *stream_name_default = "::$DATA";
	const char *sname1;
	const char *sname2;
	bool ret = true;
	int fnum = -1;
	union smb_setfileinfo sinfo;
	union smb_rename rio;

	sname1 = talloc_asprintf(mem_ctx, "%s:%s", fname1, "Stream One");
	sname2 = talloc_asprintf(mem_ctx, "%s:%s", fname1, "Stream Two");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = (SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|
	    SEC_STD_DELETE|SEC_FILE_APPEND_DATA|SEC_STD_READ_CONTROL);
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = (NTCREATEX_SHARE_ACCESS_READ |
					NTCREATEX_SHARE_ACCESS_WRITE |
					NTCREATEX_SHARE_ACCESS_DELETE);
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = sname1;

	/* Open/create new stream. */
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/*
	 * Check raw rename with <base>:<stream>.
	 */
	printf("(%s) Checking NTRENAME of a stream using <base>:<stream>\n",
	       __location__);
	rio.generic.level = RAW_RENAME_NTRENAME;
	rio.ntrename.in.old_name = sname1;
	rio.ntrename.in.new_name = sname2;
	rio.ntrename.in.attrib = 0;
	rio.ntrename.in.cluster_size = 0;
	rio.ntrename.in.flags = RENAME_FLAG_RENAME;
	status = smb_raw_rename(cli->tree, &rio);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/*
	 * Check raw rename to the default stream using :<stream>.
	 */
	printf("(%s) Checking NTRENAME to default stream using :<stream>\n",
	       __location__);
	rio.ntrename.in.new_name = stream_name_default;
	status = smb_raw_rename(cli->tree, &rio);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_COLLISION);

	/*
	 * Check raw rename using :<stream>.
	 */
	printf("(%s) Checking NTRENAME of a stream using :<stream>\n",
	       __location__);
	rio.ntrename.in.new_name = stream_name2;
	status = smb_raw_rename(cli->tree, &rio);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * Check raw rename of a stream to a file.
	 */
	printf("(%s) Checking NTRENAME of a stream to a file\n",
	       __location__);
	rio.ntrename.in.old_name = sname2;
	rio.ntrename.in.new_name = fname2;
	status = smb_raw_rename(cli->tree, &rio);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/*
	 * Check raw rename of a file to a stream.
	 */
	printf("(%s) Checking NTRENAME of a file to a stream\n",
	       __location__);

	/* Create the file. */
	io.ntcreatex.in.fname = fname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Try the rename. */
	rio.ntrename.in.old_name = fname2;
	rio.ntrename.in.new_name = sname1;
	status = smb_raw_rename(cli->tree, &rio);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);

	/*
	 * Reopen the stream for trans2 renames.
	 */
	io.ntcreatex.in.fname = sname2;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/*
	 * Check trans2 rename of a stream using :<stream>.
	 */
	printf("(%s) Checking trans2 rename of a stream using :<stream>\n",
	       __location__);
	ZERO_STRUCT(sinfo);
	sinfo.rename_information.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sinfo.rename_information.in.file.fnum = fnum;
	sinfo.rename_information.in.overwrite = 1;
	sinfo.rename_information.in.root_fid = 0;
	sinfo.rename_information.in.new_name = stream_name1;
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * Check trans2 rename of an overwriting stream using :<stream>.
	 */
	printf("(%s) Checking trans2 rename of an overwriting stream using "
	       ":<stream>\n", __location__);

	/* Create second stream. */
	io.ntcreatex.in.fname = sname2;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	/* Rename the first stream onto the second. */
	sinfo.rename_information.in.file.fnum = fnum;
	sinfo.rename_information.in.new_name = stream_name2;
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	/*
	 * Reopen the stream with the new name.
	 */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.fname = sname2;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/*
	 * Check trans2 rename of a stream using <base>:<stream>.
	 */
	printf("(%s) Checking trans2 rename of a stream using "
	       "<base>:<stream>\n", __location__);
	sinfo.rename_information.in.file.fnum = fnum;
	sinfo.rename_information.in.new_name = sname1;
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_NOT_SUPPORTED);

	/*
	 * Samba3 doesn't currently support renaming a stream to the default
	 * stream.  This test does pass on windows.
	 */
	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		goto done;
	}

	/*
	 * Check trans2 rename to the default stream using :<stream>.
	 */
	printf("(%s) Checking trans2 rename to defaualt stream using "
	       ":<stream>\n", __location__);
	sinfo.rename_information.in.file.fnum = fnum;
	sinfo.rename_information.in.new_name = stream_name_default;
	status = smb_raw_setfileinfo(cli->tree, &sinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

 done:
	smbcli_close(cli->tree, fnum);
	status = smbcli_unlink(cli->tree, fname1);
	status = smbcli_unlink(cli->tree, fname2);
	return ret;
}

static bool create_file_with_stream(struct torture_context *tctx,
				    struct smbcli_state *cli,
				    TALLOC_CTX *mem_ctx,
				    const char *stream)
{
	NTSTATUS status;
	bool ret = true;
	union smb_open io;

	/* Create a file with a stream */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = (SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|
	    SEC_FILE_APPEND_DATA|SEC_STD_READ_CONTROL);
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = stream;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	return ret;
}

/* Test how streams interact with create dispositions */
static bool test_stream_create_disposition(struct torture_context *tctx,
					   struct smbcli_state *cli,
					   TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream.txt";
	const char *stream = "Stream One:$DATA";
	const char *fname_stream;
	const char *default_stream_name = "::$DATA";
	const char *stream_list[2];
	bool ret = true;
	int fnum = -1;

	fname_stream = talloc_asprintf(mem_ctx, "%s:%s", fname, stream);

	stream_list[0] = talloc_asprintf(mem_ctx, ":%s", stream);
	stream_list[1] = default_stream_name;

	if (!create_file_with_stream(tctx, cli, mem_ctx, fname_stream)) {
		goto done;
	}

	/* Open the base file with OPEN */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = (SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|
	    SEC_FILE_APPEND_DATA|SEC_STD_READ_CONTROL);
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	 * check ntcreatex open: sanity check
	 */
	printf("(%s) Checking ntcreatex disp: open\n", __location__);
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	if (!check_stream_list(cli, fname, 2, stream_list)) {
		goto done;
	}

	/*
	 * check ntcreatex overwrite
	 */
	printf("(%s) Checking ntcreatex disp: overwrite\n", __location__);
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	if (!check_stream_list(cli, fname, 1, &default_stream_name)) {
		goto done;
	}

	/*
	 * check ntcreatex overwrite_if
	 */
	printf("(%s) Checking ntcreatex disp: overwrite_if\n", __location__);
	smbcli_unlink(cli->tree, fname);
	if (!create_file_with_stream(tctx, cli, mem_ctx, fname_stream)) {
		goto done;
	}

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	if (!check_stream_list(cli, fname, 1, &default_stream_name)) {
		goto done;
	}

	/*
	 * check ntcreatex supersede
	 */
	printf("(%s) Checking ntcreatex disp: supersede\n", __location__);
	smbcli_unlink(cli->tree, fname);
	if (!create_file_with_stream(tctx, cli, mem_ctx, fname_stream)) {
		goto done;
	}

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_SUPERSEDE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	if (!check_stream_list(cli, fname, 1, &default_stream_name)) {
		goto done;
	}

	/*
	 * check ntcreatex overwrite_if on a stream.
	 */
	printf("(%s) Checking ntcreatex disp: overwrite_if on stream\n",
	       __location__);
	smbcli_unlink(cli->tree, fname);
	if (!create_file_with_stream(tctx, cli, mem_ctx, fname_stream)) {
		goto done;
	}

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.ntcreatex.in.fname = fname_stream;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	if (!check_stream_list(cli, fname, 1, &default_stream_name)) {
		goto done;
	}

	/*
	 * check openx overwrite_if
	 */
	printf("(%s) Checking openx disp: overwrite_if\n", __location__);
	smbcli_unlink(cli->tree, fname);
	if (!create_file_with_stream(tctx, cli, mem_ctx, fname_stream)) {
		goto done;
	}

	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 1024*1024;
	io.openx.in.timeout = 0;
	io.openx.in.fname = fname;

	io.openx.in.open_func = OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, io.openx.out.file.fnum);
	if (!check_stream_list(cli, fname, 1, &default_stream_name)) {
		goto done;
	}

 done:
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	return ret;
}

/* Test streaminfo with enough streams on a file to fill up the buffer.  */
static bool test_stream_large_streaminfo(struct torture_context *tctx,
					 struct smbcli_state *cli,
					 TALLOC_CTX *mem_ctx)
{
#define LONG_STREAM_SIZE 2
	char *lstream_name;
	const char *fname = BASEDIR "\\stream.txt";
	const char *fname_stream;
	NTSTATUS status;
	bool ret = true;
	int i;
	union smb_fileinfo finfo;

	lstream_name = talloc_array(mem_ctx, char, LONG_STREAM_SIZE);

	for (i = 0; i < LONG_STREAM_SIZE - 1; i++) {
		lstream_name[i] = (char)('a' + i%26);
	}
	lstream_name[LONG_STREAM_SIZE - 1] = '\0';

	printf("(%s) Creating a file with a lot of streams\n", __location__);
	for (i = 0; i < 10000; i++) {
		fname_stream = talloc_asprintf(mem_ctx, "%s:%s%d", fname,
					       lstream_name, i);
		ret = create_file_with_stream(tctx, cli, mem_ctx,
					      fname_stream);
		if (!ret) {
			goto done;
		}
	}

	finfo.generic.level = RAW_FILEINFO_STREAM_INFO;
	finfo.generic.in.file.path = fname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, STATUS_BUFFER_OVERFLOW);

 done:
	smbcli_unlink(cli->tree, fname);
	return ret;
}

/* Test the effect of setting attributes on a stream. */
static bool test_stream_attributes(struct torture_context *tctx,
					 struct smbcli_state *cli,
					 TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\stream_attr.txt";
	const char *stream = "Stream One:$DATA";
	const char *fname_stream;
	int fnum = -1;
	union smb_fileinfo finfo;
	union smb_setfileinfo sfinfo;
	time_t basetime = (time(NULL) - 86400) & ~1;

	printf ("(%s) testing attribute setting on stream\n", __location__);

	fname_stream = talloc_asprintf(mem_ctx, "%s:%s", fname, stream);

	/* Create a file with a stream with attribute FILE_ATTRIBUTE_ARCHIVE. */
	ret = create_file_with_stream(tctx, cli, mem_ctx, fname_stream);
	if (!ret) {
		goto done;
	}

	ZERO_STRUCT(finfo);
	finfo.generic.level = RAW_FILEINFO_BASIC_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (finfo.basic_info.out.attrib != FILE_ATTRIBUTE_ARCHIVE) {
		printf("(%s) Incorrect attrib %x - should be %x\n", \
		       __location__, (unsigned int)finfo.basic_info.out.attrib,
			(unsigned int)FILE_ATTRIBUTE_ARCHIVE);
		ret = false;
		goto done;
	}

	/* Now open the stream name. */

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = (SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|
	    SEC_FILE_APPEND_DATA|SEC_STD_READ_CONTROL|SEC_FILE_WRITE_ATTRIBUTE);
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname_stream;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	fnum = io.ntcreatex.out.file.fnum;

	/* Change the attributes + time on the stream fnum. */
	ZERO_STRUCT(sfinfo);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_READONLY;
	unix_to_nt_time(&sfinfo.basic_info.in.write_time, basetime);

        sfinfo.generic.level = RAW_SFILEINFO_BASIC_INFORMATION;
        sfinfo.generic.in.file.fnum = fnum;
        status = smb_raw_setfileinfo(cli->tree, &sfinfo);
        if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) { 
                printf("(%s) %s - %s (should be %s)\n", __location__, "SETATTR", 
                        nt_errstr(status), nt_errstr(NT_STATUS_OK));
                ret = false;
		goto done;
        }

	smbcli_close(cli->tree, fnum);
	fnum = -1;

	ZERO_STRUCT(finfo);
	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("(%s) %s pathinfo - %s\n", __location__, "SETATTRE", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (finfo.all_info.out.attrib != FILE_ATTRIBUTE_READONLY) {
		printf("(%s) attrib incorrect. Was 0x%x, should be 0x%x\n",
			__location__,
			(unsigned int)finfo.all_info.out.attrib,
			(unsigned int)FILE_ATTRIBUTE_READONLY);
		ret = false;
		goto done;
	}

	if (nt_time_to_unix(finfo.all_info.out.write_time) != basetime) {
		printf("(%s) time incorrect.\n",
			__location__);
		ret = false;
		goto done;
	}

 done:

	if (fnum != -1) {
		smbcli_close(cli->tree, fnum);
	}
	smbcli_unlink(cli->tree, fname);
	return ret;
}

/* 
   basic testing of streams calls
*/
bool torture_raw_streams(struct torture_context *torture, 
			 struct smbcli_state *cli)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_stream_dir(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_io(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_sharemodes(torture, cli, torture);
	smb_raw_exit(cli->session);
	if (!torture_setting_bool(torture, "samba4", false)) {
		ret &= test_stream_delete(torture, cli, torture);
	}
	ret &= test_stream_names(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_names2(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_rename(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_rename2(torture, cli, torture);
	smb_raw_exit(cli->session);
	ret &= test_stream_create_disposition(torture, cli, torture);
	smb_raw_exit(cli->session);

	ret &= test_stream_attributes(torture, cli, torture);
	smb_raw_exit(cli->session);

	/* ret &= test_stream_large_streaminfo(torture, cli, torture); */
/* 	smb_raw_exit(cli->session); */

	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
