/* 
   Unix SMB/CIFS implementation.
   test suite for various write operations

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
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"

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
		       __location__, #v, v, correct); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_BUFFER(buf, seed, len) do { \
	if (!check_buffer(buf, seed, len, __location__)) { \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_ALL_INFO(v, field) do { \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.file.path = fname; \
	status = smb_raw_pathinfo(cli->tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if ((v) != finfo.all_info.out.field) { \
		printf("(%s) wrong value for field %s  %.0f - %.0f\n", \
		       __location__, #field, (double)v, (double)finfo.all_info.out.field); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)


#define BASEDIR "\\testwrite"


/*
  setup a random buffer based on a seed
*/
static void setup_buffer(uint8_t *buf, uint_t seed, int len)
{
	int i;
	srandom(seed);
	for (i=0;i<len;i++) buf[i] = random();
}

/*
  check a random buffer based on a seed
*/
static bool check_buffer(uint8_t *buf, uint_t seed, int len, const char *location)
{
	int i;
	srandom(seed);
	for (i=0;i<len;i++) {
		uint8_t v = random();
		if (buf[i] != v) {
			printf("Buffer incorrect at %s! ofs=%d buf=0x%x correct=0x%x\n", 
			       location, i, buf[i], v);
			return false;
		}
	}
	return true;
}

/*
  test write ops
*/
static bool test_write(struct torture_context *tctx, 
					   struct smbcli_state *cli)
{
	union smb_write io;
	NTSTATUS status;
	bool ret = true;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	uint_t seed = time(NULL);
	union smb_fileinfo finfo;

	buf = talloc_zero_array(tctx, uint8_t, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing RAW_WRITE_WRITE\n");
	io.generic.level = RAW_WRITE_WRITE;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("Trying zero write\n");
	io.write.in.file.fnum = fnum;
	io.write.in.count = 0;
	io.write.in.offset = 0;
	io.write.in.remaining = 0;
	io.write.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.write.out.nwritten, 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying small write\n");
	io.write.in.count = 9;
	io.write.in.offset = 4;
	io.write.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.write.out.nwritten, io.write.in.count);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying large write\n");
	io.write.in.count = 4000;
	io.write.in.offset = 0;
	io.write.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.write.out.nwritten, 4000);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

	printf("Trying bad fnum\n");
	io.write.in.file.fnum = fnum+1;
	io.write.in.count = 4000;
	io.write.in.offset = 0;
	io.write.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("Setting file as sparse\n");
	status = torture_set_sparse(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (!(cli->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
		printf("skipping large file tests - CAP_LARGE_FILES not set\n");
		goto done;
	}
	
	if (!(cli->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
		printf("skipping large file tests - CAP_LARGE_FILES not set\n");
		goto done;
	}

	printf("Trying 2^32 offset\n");
	setup_buffer(buf, seed, maxsize);
	io.write.in.file.fnum = fnum;
	io.write.in.count = 4000;
	io.write.in.offset = 0xFFFFFFFF - 2000;
	io.write.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.write.out.nwritten, 4000);
	CHECK_ALL_INFO(io.write.in.count + (uint64_t)io.write.in.offset, size);
	
	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, io.write.in.offset, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test writex ops
*/
static bool test_writex(struct torture_context *tctx, 
						struct smbcli_state *cli)
{
	union smb_write io;
	NTSTATUS status;
	bool ret = true;
	int fnum, i;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	uint_t seed = time(NULL);
	union smb_fileinfo finfo;
	int max_bits=63;

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		max_bits=33;
		torture_comment(tctx, "dangerous not set - limiting range of test to 2^%d\n", max_bits);
	}

	buf = talloc_zero_array(tctx, uint8_t, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing RAW_WRITE_WRITEX\n");
	io.generic.level = RAW_WRITE_WRITEX;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("Trying zero write\n");
	io.writex.in.file.fnum = fnum;
	io.writex.in.offset = 0;
	io.writex.in.wmode = 0;
	io.writex.in.remaining = 0;
	io.writex.in.count = 0;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying small write\n");
	io.writex.in.count = 9;
	io.writex.in.offset = 4;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, io.writex.in.count);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying large write\n");
	io.writex.in.count = 4000;
	io.writex.in.offset = 0;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, 4000);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

	printf("Trying bad fnum\n");
	io.writex.in.file.fnum = fnum+1;
	io.writex.in.count = 4000;
	io.writex.in.offset = 0;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("Testing wmode\n");
	io.writex.in.file.fnum = fnum;
	io.writex.in.count = 1;
	io.writex.in.offset = 0;
	io.writex.in.wmode = 1;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, io.writex.in.count);

	io.writex.in.wmode = 2;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, io.writex.in.count);


	printf("Trying locked region\n");
	cli->session->pid++;
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 3, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %s\n", __location__);
		ret = false;
		goto done;
	}
	cli->session->pid--;
	io.writex.in.wmode = 0;
	io.writex.in.count = 4;
	io.writex.in.offset = 0;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	printf("Setting file as sparse\n");
	status = torture_set_sparse(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	if (!(cli->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
		printf("skipping large file tests - CAP_LARGE_FILES not set\n");
		goto done;
	}

	printf("Trying 2^32 offset\n");
	setup_buffer(buf, seed, maxsize);
	io.writex.in.file.fnum = fnum;
	io.writex.in.count = 4000;
	io.writex.in.offset = 0xFFFFFFFF - 2000;
	io.writex.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writex.out.nwritten, 4000);
	CHECK_ALL_INFO(io.writex.in.count + (uint64_t)io.writex.in.offset, size);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, io.writex.in.offset, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

	for (i=33;i<max_bits;i++) {
		printf("Trying 2^%d offset\n", i);
		setup_buffer(buf, seed+1, maxsize);
		io.writex.in.file.fnum = fnum;
		io.writex.in.count = 4000;
		io.writex.in.offset = ((uint64_t)1) << i;
		io.writex.in.data = buf;
		status = smb_raw_write(cli->tree, &io);
		if (i>33 &&
		    NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
			break;
		}
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_VALUE(io.writex.out.nwritten, 4000);
		CHECK_ALL_INFO(io.writex.in.count + (uint64_t)io.writex.in.offset, size);

		memset(buf, 0, maxsize);
		if (smbcli_read(cli->tree, fnum, buf, io.writex.in.offset, 4000) != 4000) {
			printf("read failed at %s\n", __location__);
			ret = false;
			goto done;
		}
		CHECK_BUFFER(buf, seed+1, 4000);
	}
	printf("limit is 2^%d\n", i);

	setup_buffer(buf, seed, maxsize);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test write unlock ops
*/
static bool test_writeunlock(struct torture_context *tctx, 
							 struct smbcli_state *cli)
{
	union smb_write io;
	NTSTATUS status;
	bool ret = true;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	uint_t seed = time(NULL);
	union smb_fileinfo finfo;

	buf = talloc_zero_array(tctx, uint8_t, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing RAW_WRITE_WRITEUNLOCK\n");
	io.generic.level = RAW_WRITE_WRITEUNLOCK;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("Trying zero write\n");
	io.writeunlock.in.file.fnum = fnum;
	io.writeunlock.in.count = 0;
	io.writeunlock.in.offset = 0;
	io.writeunlock.in.remaining = 0;
	io.writeunlock.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeunlock.out.nwritten, io.writeunlock.in.count);

	setup_buffer(buf, seed, maxsize);

	printf("Trying small write\n");
	io.writeunlock.in.count = 9;
	io.writeunlock.in.offset = 4;
	io.writeunlock.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);
	smbcli_lock(cli->tree, fnum, io.writeunlock.in.offset, io.writeunlock.in.count, 
		 0, WRITE_LOCK);
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeunlock.out.nwritten, io.writeunlock.in.count);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying large write\n");
	io.writeunlock.in.count = 4000;
	io.writeunlock.in.offset = 0;
	io.writeunlock.in.data = buf;
	smbcli_lock(cli->tree, fnum, io.writeunlock.in.offset, io.writeunlock.in.count, 
		 0, WRITE_LOCK);
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeunlock.out.nwritten, 4000);

	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

	printf("Trying bad fnum\n");
	io.writeunlock.in.file.fnum = fnum+1;
	io.writeunlock.in.count = 4000;
	io.writeunlock.in.offset = 0;
	io.writeunlock.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("Setting file as sparse\n");
	status = torture_set_sparse(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	if (!(cli->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
		printf("skipping large file tests - CAP_LARGE_FILES not set\n");
		goto done;
	}

	printf("Trying 2^32 offset\n");
	setup_buffer(buf, seed, maxsize);
	io.writeunlock.in.file.fnum = fnum;
	io.writeunlock.in.count = 4000;
	io.writeunlock.in.offset = 0xFFFFFFFF - 2000;
	io.writeunlock.in.data = buf;
	smbcli_lock(cli->tree, fnum, io.writeunlock.in.offset, io.writeunlock.in.count, 
		 0, WRITE_LOCK);
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeunlock.out.nwritten, 4000);
	CHECK_ALL_INFO(io.writeunlock.in.count + (uint64_t)io.writeunlock.in.offset, size);

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, io.writeunlock.in.offset, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test write close ops
*/
static bool test_writeclose(struct torture_context *tctx, 
							struct smbcli_state *cli)
{
	union smb_write io;
	NTSTATUS status;
	bool ret = true;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	uint_t seed = time(NULL);
	union smb_fileinfo finfo;

	buf = talloc_zero_array(tctx, uint8_t, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing RAW_WRITE_WRITECLOSE\n");
	io.generic.level = RAW_WRITE_WRITECLOSE;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("Trying zero write\n");
	io.writeclose.in.file.fnum = fnum;
	io.writeclose.in.count = 0;
	io.writeclose.in.offset = 0;
	io.writeclose.in.mtime = 0;
	io.writeclose.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeclose.out.nwritten, io.writeclose.in.count);

	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeclose.out.nwritten, io.writeclose.in.count);

	setup_buffer(buf, seed, maxsize);

	printf("Trying small write\n");
	io.writeclose.in.count = 9;
	io.writeclose.in.offset = 4;
	io.writeclose.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	fnum = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	io.writeclose.in.file.fnum = fnum;

	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeclose.out.nwritten, io.writeclose.in.count);

	fnum = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	io.writeclose.in.file.fnum = fnum;

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 13) != 13) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf+4, seed, 9);
	CHECK_VALUE(IVAL(buf,0), 0);

	setup_buffer(buf, seed, maxsize);

	printf("Trying large write\n");
	io.writeclose.in.count = 4000;
	io.writeclose.in.offset = 0;
	io.writeclose.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeclose.out.nwritten, 4000);

	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	fnum = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	io.writeclose.in.file.fnum = fnum;

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, 0, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

	printf("Trying bad fnum\n");
	io.writeclose.in.file.fnum = fnum+1;
	io.writeclose.in.count = 4000;
	io.writeclose.in.offset = 0;
	io.writeclose.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("Setting file as sparse\n");
	status = torture_set_sparse(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	if (!(cli->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
		printf("skipping large file tests - CAP_LARGE_FILES not set\n");
		goto done;
	}

	printf("Trying 2^32 offset\n");
	setup_buffer(buf, seed, maxsize);
	io.writeclose.in.file.fnum = fnum;
	io.writeclose.in.count = 4000;
	io.writeclose.in.offset = 0xFFFFFFFF - 2000;
	io.writeclose.in.data = buf;
	status = smb_raw_write(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.writeclose.out.nwritten, 4000);
	CHECK_ALL_INFO(io.writeclose.in.count + (uint64_t)io.writeclose.in.offset, size);

	fnum = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	io.writeclose.in.file.fnum = fnum;

	memset(buf, 0, maxsize);
	if (smbcli_read(cli->tree, fnum, buf, io.writeclose.in.offset, 4000) != 4000) {
		printf("read failed at %s\n", __location__);
		ret = false;
		goto done;
	}
	CHECK_BUFFER(buf, seed, 4000);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/* 
   basic testing of write calls
*/
struct torture_suite *torture_raw_write(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "WRITE");

	torture_suite_add_1smb_test(suite, "write", test_write);
	torture_suite_add_1smb_test(suite, "write unlock", test_writeunlock);
	torture_suite_add_1smb_test(suite, "write close", test_writeclose);
	torture_suite_add_1smb_test(suite, "writex", test_writex);

	return suite;
}
