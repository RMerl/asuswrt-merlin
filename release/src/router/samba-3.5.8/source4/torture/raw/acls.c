/* 
   Unix SMB/CIFS implementation.

   test security descriptor operations

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
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "librpc/gen_ndr/lsa.h"
#include "libcli/util/clilsa.h"
#include "libcli/security/security.h"
#include "torture/util.h"
#include "librpc/gen_ndr/ndr_security.h"

#define BASEDIR "\\testsd"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)


static bool test_sd(struct torture_context *tctx, 
					struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\sd.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_ace ace;
	struct security_descriptor *sd;
	struct dom_sid *test_sid;

	printf("TESTING SETFILEINFO EA_SET\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = 
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd = q.query_secdesc.out.sd;

	printf("add a new ACE to the DACL\n");

	test_sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1234-5432");

	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.access_mask = SEC_STD_ALL;
	ace.trustee = *test_sid;

	status = security_descriptor_dacl_add(sd, &ace);
	CHECK_STATUS(status, NT_STATUS_OK);

	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = q.query_secdesc.in.secinfo_flags;
	set.set_secdesc.in.sd = sd;

	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (!security_acl_equal(q.query_secdesc.out.sd->dacl, sd->dacl)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

	printf("remove it again\n");

	status = security_descriptor_dacl_del(sd, test_sid);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (!security_acl_equal(q.query_secdesc.out.sd->dacl, sd->dacl)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}


/*
  test using nttrans create to create a file with an initial acl set
*/
static bool test_nttrans_create(struct torture_context *tctx, 
								struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\acl2.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q;
	struct security_ace ace;
	struct security_descriptor *sd;
	struct dom_sid *test_sid;

	printf("testing nttrans create with sec_desc\n");

	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.sec_desc = NULL;
	io.ntcreatex.in.ea_list = NULL;

	printf("creating normal file\n");

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("querying ACL\n");

	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = 
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd = q.query_secdesc.out.sd;

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	printf("adding a new ACE\n");
	test_sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1234-54321");

	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.access_mask = SEC_STD_ALL;
	ace.trustee = *test_sid;

	status = security_descriptor_dacl_add(sd, &ace);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	printf("creating a file with an initial ACL\n");

	io.ntcreatex.in.sec_desc = sd;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	
	q.query_secdesc.in.file.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (!security_acl_equal(q.query_secdesc.out.sd->dacl, sd->dacl)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

#define CHECK_ACCESS_FLAGS(_fnum, flags) do { \
	union smb_fileinfo _q; \
	_q.access_information.level = RAW_FILEINFO_ACCESS_INFORMATION; \
	_q.access_information.in.file.fnum = (_fnum); \
	status = smb_raw_fileinfo(cli->tree, tctx, &_q); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if (_q.access_information.out.access_flags != (flags)) { \
		printf("(%s) Incorrect access_flags 0x%08x - should be 0x%08x\n", \
		       __location__, _q.access_information.out.access_flags, (flags)); \
		ret = false; \
		goto done; \
	} \
} while (0)

/*
  test using NTTRANS CREATE to create a file with a null ACL set
*/
static bool test_nttrans_create_null_dacl(struct torture_context *tctx,
					  struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\acl3.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q;
	union smb_setfileinfo s;
	struct security_descriptor *sd = security_descriptor_initialise(tctx);
	struct security_acl dacl;

	printf("TESTING SEC_DESC WITH A NULL DACL\n");

	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL | SEC_STD_WRITE_DAC
		| SEC_STD_WRITE_OWNER;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.sec_desc = sd;
	io.ntcreatex.in.ea_list = NULL;

	printf("creating a file with a empty sd\n");
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * Testing the created DACL,
	 * the server should add the inherited DACL
	 * when SEC_DESC_DACL_PRESENT isn't specified
	 */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		printf("DACL_PRESENT flag not set by the server!\n");
		ret = false;
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		printf("no DACL has been created on the server!\n");
		ret = false;
		goto done;
	}

	printf("set NULL DACL\n");
	sd->type |= SEC_DESC_DACL_PRESENT;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.fnum = fnum;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		printf("DACL_PRESENT flag not set by the server!\n");
		ret = false;
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		printf("DACL has been created on the server!\n");
		ret = false;
		goto done;
	}

	printf("try open for read control\n");
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_STD_READ_CONTROL | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_FILE_READ_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_RIGHTS_FILE_WRITE | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_RIGHTS_FILE_READ | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("set DACL with 0 aces\n");
	ZERO_STRUCT(dacl);
	dacl.revision = SECURITY_ACL_REVISION_NT4;
	dacl.num_aces = 0;
	sd->dacl = &dacl;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.fnum = fnum;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		printf("DACL_PRESENT flag not set by the server!\n");
		ret = false;
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		printf("no DACL has been created on the server!\n");
		ret = false;
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl->num_aces != 0) {
		printf("DACL has %u aces!\n",
		       q.query_secdesc.out.sd->dacl->num_aces);
		ret = false;
		goto done;
	}

	printf("try open for read control\n");
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_STD_READ_CONTROL | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for write => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for read => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic write => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic read => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("set empty sd\n");
	sd->type &= ~SEC_DESC_DACL_PRESENT;
	sd->dacl = NULL;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.fnum = fnum;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		printf("DACL_PRESENT flag not set by the server!\n");
		ret = false;
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		printf("DACL has been created on the server!\n");
		ret = false;
		goto done;
	}
done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/*
  test the behaviour of the well known SID_CREATOR_OWNER sid, and some generic
  mapping bits
*/
static bool test_creator_sid(struct torture_context *tctx, 
							 struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\creator.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd_orig, *sd2;
	const char *owner_sid;

	printf("TESTING SID_CREATOR_OWNER\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL | SEC_STD_WRITE_DAC | SEC_STD_WRITE_OWNER;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	printf("set a sec desc allowing no write by CREATOR_OWNER\n");
	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					SID_CREATOR_OWNER,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					0,
					NULL);

	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd;

	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("set a sec desc allowing no write by owner\n");
	sd = security_descriptor_dacl_create(tctx,
					0, owner_sid, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					0,
					NULL);

	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("check that sd has been mapped correctly\n");
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	if (!security_descriptor_equal(q.query_secdesc.out.sd, sd)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

	printf("try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_FILE_READ_DATA|
			   SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_RIGHTS_FILE_READ);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("set a sec desc allowing generic read by owner\n");
	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_GENERIC_READ | SEC_STD_ALL,
					0,
					NULL);

	set.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("check that generic read has been mapped correctly\n");
	sd2 = security_descriptor_dacl_create(tctx,
					 0, owner_sid, NULL,
					 owner_sid,
					 SEC_ACE_TYPE_ACCESS_ALLOWED,
					 SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					 0,
					 NULL);

	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd2);
		ret = false;
	}
	

	printf("try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_FILE_READ_DATA | 
			   SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	printf("try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, SEC_RIGHTS_FILE_READ);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);


	printf("put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);


done:
	smbcli_close(cli->tree, fnum);
	return ret;
}


/*
  test the mapping of the SEC_GENERIC_xx bits to SEC_STD_xx and
  SEC_FILE_xx bits
*/
static bool test_generic_bits(struct torture_context *tctx, 
							  struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\generic.txt";
	bool ret = true;
	int fnum = -1, i;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd_orig, *sd2;
	const char *owner_sid;
	const struct {
		uint32_t gen_bits;
		uint32_t specific_bits;
	} file_mappings[] = {
		{ 0,                       0 },
		{ SEC_GENERIC_READ,        SEC_RIGHTS_FILE_READ },
		{ SEC_GENERIC_WRITE,       SEC_RIGHTS_FILE_WRITE },
		{ SEC_GENERIC_EXECUTE,     SEC_RIGHTS_FILE_EXECUTE },
		{ SEC_GENERIC_ALL,         SEC_RIGHTS_FILE_ALL },
		{ SEC_FILE_READ_DATA,      SEC_FILE_READ_DATA },
		{ SEC_FILE_READ_ATTRIBUTE, SEC_FILE_READ_ATTRIBUTE }
	};
	const struct {
		uint32_t gen_bits;
		uint32_t specific_bits;
	} dir_mappings[] = {
		{ 0,                   0 },
		{ SEC_GENERIC_READ,    SEC_RIGHTS_DIR_READ },
		{ SEC_GENERIC_WRITE,   SEC_RIGHTS_DIR_WRITE },
		{ SEC_GENERIC_EXECUTE, SEC_RIGHTS_DIR_EXECUTE },
		{ SEC_GENERIC_ALL,     SEC_RIGHTS_DIR_ALL }
	};
	bool has_restore_privilege;
	bool has_take_ownership_privilege;

	printf("TESTING FILE GENERIC BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = 
		SEC_STD_READ_CONTROL | 
		SEC_STD_WRITE_DAC | 
		SEC_STD_WRITE_OWNER;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

	for (i=0;i<ARRAY_SIZE(file_mappings);i++) {
		uint32_t expected_mask = 
			SEC_STD_WRITE_DAC | 
			SEC_STD_READ_CONTROL | 
			SEC_FILE_READ_ATTRIBUTE |
			SEC_STD_DELETE;
		uint32_t expected_mask_anon = SEC_FILE_READ_ATTRIBUTE;

		if (has_restore_privilege) {
			expected_mask_anon |= SEC_STD_DELETE;
		}

		printf("testing generic bits 0x%08x\n", 
		       file_mappings[i].gen_bits);
		sd = security_descriptor_dacl_create(tctx,
						0, owner_sid, NULL,
						owner_sid,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						file_mappings[i].gen_bits,
						0,
						NULL);

		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
		set.set_secdesc.in.sd = sd;

		status = smb_raw_setfileinfo(cli->tree, &set);
		CHECK_STATUS(status, NT_STATUS_OK);

		sd2 = security_descriptor_dacl_create(tctx,
						 0, owner_sid, NULL,
						 owner_sid,
						 SEC_ACE_TYPE_ACCESS_ALLOWED,
						 file_mappings[i].specific_bits,
						 0,
						 NULL);

		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
			printf("%s: security descriptors don't match!\n", __location__);
			printf("got:\n");
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			printf("expected:\n");
			NDR_PRINT_DEBUG(security_descriptor, sd2);
			ret = false;
		}

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask | file_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

		if (!has_take_ownership_privilege) {
			continue;
		}

		printf("testing generic bits 0x%08x (anonymous)\n", 
		       file_mappings[i].gen_bits);
		sd = security_descriptor_dacl_create(tctx,
						0, SID_NT_ANONYMOUS, NULL,
						owner_sid,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						file_mappings[i].gen_bits,
						0,
						NULL);

		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
		set.set_secdesc.in.sd = sd;

		status = smb_raw_setfileinfo(cli->tree, &set);
		CHECK_STATUS(status, NT_STATUS_OK);

		sd2 = security_descriptor_dacl_create(tctx,
						 0, SID_NT_ANONYMOUS, NULL,
						 owner_sid,
						 SEC_ACE_TYPE_ACCESS_ALLOWED,
						 file_mappings[i].specific_bits,
						 0,
						 NULL);

		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
			printf("%s: security descriptors don't match!\n", __location__);
			printf("got:\n");
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			printf("expected:\n");
			NDR_PRINT_DEBUG(security_descriptor, sd2);
			ret = false;
		}

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask_anon | file_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	}

	printf("put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);


	printf("TESTING DIR GENERIC BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = 
		SEC_STD_READ_CONTROL | 
		SEC_STD_WRITE_DAC | 
		SEC_STD_WRITE_OWNER;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

	for (i=0;i<ARRAY_SIZE(dir_mappings);i++) {
		uint32_t expected_mask = 
			SEC_STD_WRITE_DAC | 
			SEC_STD_READ_CONTROL | 
			SEC_FILE_READ_ATTRIBUTE |
			SEC_STD_DELETE;
		uint32_t expected_mask_anon = SEC_FILE_READ_ATTRIBUTE;

		if (has_restore_privilege) {
			expected_mask_anon |= SEC_STD_DELETE;
		}

		printf("testing generic bits 0x%08x\n", 
		       file_mappings[i].gen_bits);
		sd = security_descriptor_dacl_create(tctx,
						0, owner_sid, NULL,
						owner_sid,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						dir_mappings[i].gen_bits,
						0,
						NULL);

		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
		set.set_secdesc.in.sd = sd;

		status = smb_raw_setfileinfo(cli->tree, &set);
		CHECK_STATUS(status, NT_STATUS_OK);

		sd2 = security_descriptor_dacl_create(tctx,
						 0, owner_sid, NULL,
						 owner_sid,
						 SEC_ACE_TYPE_ACCESS_ALLOWED,
						 dir_mappings[i].specific_bits,
						 0,
						 NULL);

		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
			printf("%s: security descriptors don't match!\n", __location__);
			printf("got:\n");
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			printf("expected:\n");
			NDR_PRINT_DEBUG(security_descriptor, sd2);
			ret = false;
		}

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask | dir_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

		if (!has_take_ownership_privilege) {
			continue;
		}

		printf("testing generic bits 0x%08x (anonymous)\n", 
		       file_mappings[i].gen_bits);
		sd = security_descriptor_dacl_create(tctx,
						0, SID_NT_ANONYMOUS, NULL,
						owner_sid,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						file_mappings[i].gen_bits,
						0,
						NULL);

		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
		set.set_secdesc.in.sd = sd;

		status = smb_raw_setfileinfo(cli->tree, &set);
		CHECK_STATUS(status, NT_STATUS_OK);

		sd2 = security_descriptor_dacl_create(tctx,
						 0, SID_NT_ANONYMOUS, NULL,
						 owner_sid,
						 SEC_ACE_TYPE_ACCESS_ALLOWED,
						 file_mappings[i].specific_bits,
						 0,
						 NULL);

		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);
		if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
			printf("%s: security descriptors don't match!\n", __location__);
			printf("got:\n");
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			printf("expected:\n");
			NDR_PRINT_DEBUG(security_descriptor, sd2);
			ret = false;
		}

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask_anon | dir_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	}

	printf("put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}


/*
  see what access bits the owner of a file always gets
*/
static bool test_owner_bits(struct torture_context *tctx, 
							struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\test_owner_bits.txt";
	bool ret = true;
	int fnum = -1, i;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd_orig;
	const char *owner_sid;
	bool has_restore_privilege;
	bool has_take_ownership_privilege;
	uint32_t expected_bits;

	printf("TESTING FILE OWNER BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = 
		SEC_STD_READ_CONTROL | 
		SEC_STD_WRITE_DAC | 
		SEC_STD_WRITE_OWNER;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ | 
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = smblsa_sid_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		printf("smblsa_sid_check_privilege - %s\n", nt_errstr(status));
	}
	printf("SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_FILE_WRITE_DATA,
					0,
					NULL);

	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd;

	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	expected_bits = SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE;

	for (i=0;i<16;i++) {
		uint32_t bit = (1<<i);
		io.ntcreatex.in.access_mask = bit;
		status = smb_raw_open(cli->tree, tctx, &io);
		if (expected_bits & bit) {
			if (!NT_STATUS_IS_OK(status)) {
				printf("failed with access mask 0x%08x of expected 0x%08x\n",
				       bit, expected_bits);
			}
			CHECK_STATUS(status, NT_STATUS_OK);
			CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, bit | SEC_FILE_READ_ATTRIBUTE);
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
		} else {
			if (NT_STATUS_IS_OK(status)) {
				printf("open succeeded with access mask 0x%08x of "
					"expected 0x%08x - should fail\n",
				       bit, expected_bits);
			}
			CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
		}
	}

	printf("put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	return ret;
}



/*
  test the inheritance of ACL flags onto new files and directories
*/
static bool test_inheritance(struct torture_context *tctx, 
							 struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *dname = BASEDIR "\\inheritance";
	const char *fname1 = BASEDIR "\\inheritance\\testfile";
	const char *fname2 = BASEDIR "\\inheritance\\testdir";
	bool ret = true;
	int fnum=0, fnum2, i;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd2, *sd_orig=NULL, *sd_def;
	const char *owner_sid;
	const struct dom_sid *creator_owner;
	const struct {
		uint32_t parent_flags;
		uint32_t file_flags;
		uint32_t dir_flags;
	} test_flags[] = {
		{
			0, 
			0,
			0
		},
		{
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			SEC_ACE_FLAG_OBJECT_INHERIT | 
			SEC_ACE_FLAG_INHERIT_ONLY,
		},
		{
			SEC_ACE_FLAG_CONTAINER_INHERIT,
			0,
			SEC_ACE_FLAG_CONTAINER_INHERIT,
		},
		{
			SEC_ACE_FLAG_OBJECT_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT,
			0,
			SEC_ACE_FLAG_OBJECT_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT,
		},
		{
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			SEC_ACE_FLAG_OBJECT_INHERIT | 
			SEC_ACE_FLAG_INHERIT_ONLY,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_CONTAINER_INHERIT,
			0,
			SEC_ACE_FLAG_CONTAINER_INHERIT,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_CONTAINER_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			SEC_ACE_FLAG_CONTAINER_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT,
			0,
			0,
		},
		{
			SEC_ACE_FLAG_INHERIT_ONLY | 
			SEC_ACE_FLAG_NO_PROPAGATE_INHERIT | 
			SEC_ACE_FLAG_CONTAINER_INHERIT | 
			SEC_ACE_FLAG_OBJECT_INHERIT,
			0,
			0,
		}
	};

	smbcli_rmdir(cli->tree, dname);

	printf("TESTING ACL INHERITANCE\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	printf("owner_sid is %s\n", owner_sid);

	sd_def = security_descriptor_dacl_create(tctx,
					    0, owner_sid, NULL,
					    owner_sid,
					    SEC_ACE_TYPE_ACCESS_ALLOWED,
					    SEC_RIGHTS_FILE_ALL,
					    0,
					    SID_NT_SYSTEM,
					    SEC_ACE_TYPE_ACCESS_ALLOWED,
					    SEC_RIGHTS_FILE_ALL,
					    0,
					    NULL);

	creator_owner = dom_sid_parse_talloc(tctx, SID_CREATOR_OWNER);

	for (i=0;i<ARRAY_SIZE(test_flags);i++) {
		sd = security_descriptor_dacl_create(tctx,
						0, NULL, NULL,
						SID_CREATOR_OWNER,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						SEC_FILE_WRITE_DATA,
						test_flags[i].parent_flags,
						SID_WORLD,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						SEC_FILE_ALL | SEC_STD_ALL,
						0,
						NULL);
		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
		set.set_secdesc.in.sd = sd;
		status = smb_raw_setfileinfo(cli->tree, &set);
		CHECK_STATUS(status, NT_STATUS_OK);

		io.ntcreatex.in.fname = fname1;
		io.ntcreatex.in.create_options = 0;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		fnum2 = io.ntcreatex.out.file.fnum;

		q.query_secdesc.in.file.fnum = fnum2;
		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);

		smbcli_close(cli->tree, fnum2);
		smbcli_unlink(cli->tree, fname1);

		if (!(test_flags[i].parent_flags & SEC_ACE_FLAG_OBJECT_INHERIT)) {
			if (!security_descriptor_equal(q.query_secdesc.out.sd, sd_def)) {
				printf("Expected default sd:\n");
				NDR_PRINT_DEBUG(security_descriptor, sd_def);
				printf("at %d - got:\n", i);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			}
			goto check_dir;
		}

		if (q.query_secdesc.out.sd->dacl == NULL ||
		    q.query_secdesc.out.sd->dacl->num_aces != 1 ||
		    q.query_secdesc.out.sd->dacl->aces[0].access_mask != SEC_FILE_WRITE_DATA ||
		    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[0].trustee,
				   sd_orig->owner_sid)) {
			printf("Bad sd in child file at %d\n", i);
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			ret = false;
			goto check_dir;
		}

		if (q.query_secdesc.out.sd->dacl->aces[0].flags != 
		    test_flags[i].file_flags) {
			printf("incorrect file_flags 0x%x - expected 0x%x for parent 0x%x with (i=%d)\n",
			       q.query_secdesc.out.sd->dacl->aces[0].flags,
			       test_flags[i].file_flags,
			       test_flags[i].parent_flags,
			       i);
			ret = false;
		}

	check_dir:
		io.ntcreatex.in.fname = fname2;
		io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		fnum2 = io.ntcreatex.out.file.fnum;

		q.query_secdesc.in.file.fnum = fnum2;
		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);

		smbcli_close(cli->tree, fnum2);
		smbcli_rmdir(cli->tree, fname2);

		if (!(test_flags[i].parent_flags & SEC_ACE_FLAG_CONTAINER_INHERIT) &&
		    (!(test_flags[i].parent_flags & SEC_ACE_FLAG_OBJECT_INHERIT) ||
		     (test_flags[i].parent_flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT))) {
			if (!security_descriptor_equal(q.query_secdesc.out.sd, sd_def)) {
				printf("Expected default sd for dir at %d:\n", i);
				NDR_PRINT_DEBUG(security_descriptor, sd_def);
				printf("got:\n");
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			}
			continue;
		}

		if ((test_flags[i].parent_flags & SEC_ACE_FLAG_CONTAINER_INHERIT) && 
		    (test_flags[i].parent_flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)) {
			if (q.query_secdesc.out.sd->dacl == NULL ||
			    q.query_secdesc.out.sd->dacl->num_aces != 1 ||
			    q.query_secdesc.out.sd->dacl->aces[0].access_mask != SEC_FILE_WRITE_DATA ||
			    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[0].trustee,
					   sd_orig->owner_sid) ||
			    q.query_secdesc.out.sd->dacl->aces[0].flags != test_flags[i].dir_flags) {
				printf("(CI & NP) Bad sd in child dir at %d (parent 0x%x)\n", 
				       i, test_flags[i].parent_flags);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				ret = false;
				continue;
			}
		} else if (test_flags[i].parent_flags & SEC_ACE_FLAG_CONTAINER_INHERIT) {
			if (q.query_secdesc.out.sd->dacl == NULL ||
			    q.query_secdesc.out.sd->dacl->num_aces != 2 ||
			    q.query_secdesc.out.sd->dacl->aces[0].access_mask != SEC_FILE_WRITE_DATA ||
			    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[0].trustee,
					   sd_orig->owner_sid) ||
			    q.query_secdesc.out.sd->dacl->aces[1].access_mask != SEC_FILE_WRITE_DATA ||
			    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[1].trustee,
					   creator_owner) ||
			    q.query_secdesc.out.sd->dacl->aces[0].flags != 0 ||
			    q.query_secdesc.out.sd->dacl->aces[1].flags != 
			    (test_flags[i].dir_flags | SEC_ACE_FLAG_INHERIT_ONLY)) {
				printf("(CI) Bad sd in child dir at %d (parent 0x%x)\n", 
				       i, test_flags[i].parent_flags);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				ret = false;
				continue;
			}
		} else {
			if (q.query_secdesc.out.sd->dacl == NULL ||
			    q.query_secdesc.out.sd->dacl->num_aces != 1 ||
			    q.query_secdesc.out.sd->dacl->aces[0].access_mask != SEC_FILE_WRITE_DATA ||
			    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[0].trustee,
					   creator_owner) ||
			    q.query_secdesc.out.sd->dacl->aces[0].flags != test_flags[i].dir_flags) {
				printf("(0) Bad sd in child dir at %d (parent 0x%x)\n", 
					i, test_flags[i].parent_flags);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				ret = false;
				continue;
			}
		}
	}

	printf("testing access checks on inherited create with %s\n", fname1);
	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_FILE_WRITE_DATA | SEC_STD_WRITE_DAC,
					SEC_ACE_FLAG_OBJECT_INHERIT,
					SID_WORLD,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_FILE_ALL | SEC_STD_ALL,
					0,
					NULL);
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.ntcreatex.in.fname = fname1;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_ACCESS_FLAGS(fnum2, SEC_RIGHTS_FILE_ALL);

	q.query_secdesc.in.file.fnum = fnum2;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, fnum2);

	sd2 = security_descriptor_dacl_create(tctx,
					 0, owner_sid, NULL,
					 owner_sid,
					 SEC_ACE_TYPE_ACCESS_ALLOWED,
					 SEC_FILE_WRITE_DATA | SEC_STD_WRITE_DAC,
					 0,
					 NULL);
	if (!security_descriptor_equal(q.query_secdesc.out.sd, sd2)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd2);
		ret = false;
	}

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (NT_STATUS_IS_OK(status)) {
		printf("failed: w2k3 ACL bug (allowed open when ACL should deny)\n");
		ret = false;
		fnum2 = io.ntcreatex.out.file.fnum;
		CHECK_ACCESS_FLAGS(fnum2, SEC_RIGHTS_FILE_ALL);
		smbcli_close(cli->tree, fnum2);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	printf("trying without execute\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL & ~SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("and with full permissions again\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_ACCESS_FLAGS(fnum2, SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, fnum2);

	printf("put back original sd\n");
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_ACCESS_FLAGS(fnum2, SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, fnum2);

	smbcli_unlink(cli->tree, fname1);
	smbcli_rmdir(cli->tree, dname);

done:
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);

	smbcli_close(cli->tree, fnum);
	return ret;
}


/*
  test dynamic acl inheritance
*/
static bool test_inheritance_dynamic(struct torture_context *tctx, 
									 struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *dname = BASEDIR "\\inheritance";
	const char *fname1 = BASEDIR "\\inheritance\\testfile";
	bool ret = true;
	int fnum=0, fnum2;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd_orig=NULL;
	const char *owner_sid;
	
	printf("TESTING DYNAMIC ACL INHERITANCE\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	printf("owner_sid is %s\n", owner_sid);

	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_FILE_WRITE_DATA | SEC_STD_DELETE | SEC_FILE_READ_ATTRIBUTE,
					SEC_ACE_FLAG_OBJECT_INHERIT,
					NULL);
	sd->type |= SEC_DESC_DACL_AUTO_INHERITED | SEC_DESC_DACL_AUTO_INHERIT_REQ;

	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("create a file with an inherited acl\n");
	io.ntcreatex.in.fname = fname1;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);

	printf("try and access file with base rights - should be OK\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);

	printf("try and access file with extra rights - should be denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA | SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	printf("update parent sd\n");
	sd = security_descriptor_dacl_create(tctx,
					0, NULL, NULL,
					owner_sid,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_FILE_WRITE_DATA | SEC_STD_DELETE | SEC_FILE_READ_ATTRIBUTE | SEC_FILE_EXECUTE,
					SEC_ACE_FLAG_OBJECT_INHERIT,
					NULL);
	sd->type |= SEC_DESC_DACL_AUTO_INHERITED | SEC_DESC_DACL_AUTO_INHERIT_REQ;

	set.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("try and access file with base rights - should be OK\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);


	printf("try and access now - should be OK if dynamic inheritance works\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA | SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		printf("Server does not have dynamic inheritance\n");
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		printf("Server does have dynamic inheritance\n");
	}
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	smbcli_unlink(cli->tree, fname1);

done:
	printf("put back original sd\n");
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);

	smbcli_close(cli->tree, fnum);
	smbcli_rmdir(cli->tree, dname);

	return ret;
}

#define CHECK_STATUS_FOR_BIT_ACTION(status, bits, action) do { \
	if (!(bits & desired_64)) {\
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED); \
		action; \
	} else { \
		CHECK_STATUS(status, NT_STATUS_OK); \
	} \
} while (0)

#define CHECK_STATUS_FOR_BIT(status, bits, access) do { \
	if (NT_STATUS_IS_OK(status)) { \
		if (!(granted & access)) {\
			printf("(%s) %s but flags 0x%08X are not granted! granted[0x%08X] desired[0x%08X]\n", \
			       __location__, nt_errstr(status), access, granted, desired); \
			ret = false; \
			goto done; \
		} \
	} else { \
		if (granted & access) {\
			printf("(%s) %s but flags 0x%08X are granted! granted[0x%08X] desired[0x%08X]\n", \
			       __location__, nt_errstr(status), access, granted, desired); \
			ret = false; \
			goto done; \
		} \
	} \
	CHECK_STATUS_FOR_BIT_ACTION(status, bits, do {} while (0)); \
} while (0)

/* test what access mask is needed for getting and setting security_descriptors */
static bool test_sd_get_set(struct torture_context *tctx, 
							struct smbcli_state *cli)
{
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo fi;
	union smb_setfileinfo si;
	struct security_descriptor *sd;
	struct security_descriptor *sd_owner = NULL;
	struct security_descriptor *sd_group = NULL;
	struct security_descriptor *sd_dacl = NULL;
	struct security_descriptor *sd_sacl = NULL;
	int fnum=0;
	const char *fname = BASEDIR "\\sd_get_set.txt";
	uint64_t desired_64;
	uint32_t desired = 0, granted;
	int i = 0;
#define NO_BITS_HACK (((uint64_t)1)<<32)
	uint64_t open_bits =
		SEC_MASK_GENERIC |
		SEC_FLAG_SYSTEM_SECURITY |
		SEC_FLAG_MAXIMUM_ALLOWED |
		SEC_STD_ALL |
		SEC_FILE_ALL | 
		NO_BITS_HACK;
	uint64_t get_owner_bits = SEC_MASK_GENERIC | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_READ_CONTROL;
	uint64_t set_owner_bits = SEC_GENERIC_ALL  | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_WRITE_OWNER;
	uint64_t get_group_bits = SEC_MASK_GENERIC | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_READ_CONTROL;
	uint64_t set_group_bits = SEC_GENERIC_ALL  | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_WRITE_OWNER;
	uint64_t get_dacl_bits  = SEC_MASK_GENERIC | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_READ_CONTROL;
	uint64_t set_dacl_bits  = SEC_GENERIC_ALL  | SEC_FLAG_MAXIMUM_ALLOWED | SEC_STD_WRITE_DAC;
	uint64_t get_sacl_bits  = SEC_FLAG_SYSTEM_SECURITY;
	uint64_t set_sacl_bits  = SEC_FLAG_SYSTEM_SECURITY;

	printf("TESTING ACCESS MASKS FOR SD GET/SET\n");

	/* first create a file with full access for everyone */
	sd = security_descriptor_dacl_create(tctx,
					0, SID_NT_ANONYMOUS, SID_BUILTIN_USERS,
					SID_WORLD,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_GENERIC_ALL,
					0,
					NULL);
	sd->type |= SEC_DESC_SACL_PRESENT;
	sd->sacl = NULL;
	io.ntcreatex.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_GENERIC_ALL;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.sec_desc = sd;
	io.ntcreatex.in.ea_list = NULL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* 
	 * now try each access_mask bit and no bit at all in a loop
	 * and see what's allowed
	 * NOTE: if i == 32 it means access_mask = 0 (see NO_BITS_HACK above)
	 */
	for (i=0; i <= 32; i++) {
		desired_64 = ((uint64_t)1) << i;
		desired = (uint32_t)desired_64;

		/* first open the file with the desired access */
		io.ntcreatex.level = RAW_OPEN_NTCREATEX;
		io.ntcreatex.in.access_mask = desired;
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS_FOR_BIT_ACTION(status, open_bits, goto next);
		fnum = io.ntcreatex.out.file.fnum;

		/* then check what access was granted */
		fi.access_information.level		= RAW_FILEINFO_ACCESS_INFORMATION;
		fi.access_information.in.file.fnum	= fnum;
		status = smb_raw_fileinfo(cli->tree, tctx, &fi);
		CHECK_STATUS(status, NT_STATUS_OK);
		granted = fi.access_information.out.access_flags;

		/* test the owner */
		ZERO_STRUCT(fi);
		fi.query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
		fi.query_secdesc.in.file.fnum		= fnum;
		fi.query_secdesc.in.secinfo_flags	= SECINFO_OWNER;
		status = smb_raw_fileinfo(cli->tree, tctx, &fi);
		CHECK_STATUS_FOR_BIT(status, get_owner_bits, SEC_STD_READ_CONTROL);
		if (fi.query_secdesc.out.sd) {
			sd_owner = fi.query_secdesc.out.sd;
		} else if (!sd_owner) {
			sd_owner = sd;
		}
		si.set_secdesc.level			= RAW_SFILEINFO_SEC_DESC;
		si.set_secdesc.in.file.fnum		= fnum;
		si.set_secdesc.in.secinfo_flags		= SECINFO_OWNER;
		si.set_secdesc.in.sd			= sd_owner;
		status = smb_raw_setfileinfo(cli->tree, &si);
		CHECK_STATUS_FOR_BIT(status, set_owner_bits, SEC_STD_WRITE_OWNER);

		/* test the group */
		ZERO_STRUCT(fi);
		fi.query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
		fi.query_secdesc.in.file.fnum		= fnum;
		fi.query_secdesc.in.secinfo_flags	= SECINFO_GROUP;
		status = smb_raw_fileinfo(cli->tree, tctx, &fi);
		CHECK_STATUS_FOR_BIT(status, get_group_bits, SEC_STD_READ_CONTROL);
		if (fi.query_secdesc.out.sd) {
			sd_group = fi.query_secdesc.out.sd;
		} else if (!sd_group) {
			sd_group = sd;
		}
		si.set_secdesc.level			= RAW_SFILEINFO_SEC_DESC;
		si.set_secdesc.in.file.fnum		= fnum;
		si.set_secdesc.in.secinfo_flags		= SECINFO_GROUP;
		si.set_secdesc.in.sd			= sd_group;
		status = smb_raw_setfileinfo(cli->tree, &si);
		CHECK_STATUS_FOR_BIT(status, set_group_bits, SEC_STD_WRITE_OWNER);

		/* test the DACL */
		ZERO_STRUCT(fi);
		fi.query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
		fi.query_secdesc.in.file.fnum		= fnum;
		fi.query_secdesc.in.secinfo_flags	= SECINFO_DACL;
		status = smb_raw_fileinfo(cli->tree, tctx, &fi);
		CHECK_STATUS_FOR_BIT(status, get_dacl_bits, SEC_STD_READ_CONTROL);
		if (fi.query_secdesc.out.sd) {
			sd_dacl = fi.query_secdesc.out.sd;
		} else if (!sd_dacl) {
			sd_dacl = sd;
		}
		si.set_secdesc.level			= RAW_SFILEINFO_SEC_DESC;
		si.set_secdesc.in.file.fnum		= fnum;
		si.set_secdesc.in.secinfo_flags		= SECINFO_DACL;
		si.set_secdesc.in.sd			= sd_dacl;
		status = smb_raw_setfileinfo(cli->tree, &si);
		CHECK_STATUS_FOR_BIT(status, set_dacl_bits, SEC_STD_WRITE_DAC);

		/* test the SACL */
		ZERO_STRUCT(fi);
		fi.query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
		fi.query_secdesc.in.file.fnum		= fnum;
		fi.query_secdesc.in.secinfo_flags	= SECINFO_SACL;
		status = smb_raw_fileinfo(cli->tree, tctx, &fi);
		CHECK_STATUS_FOR_BIT(status, get_sacl_bits, SEC_FLAG_SYSTEM_SECURITY);
		if (fi.query_secdesc.out.sd) {
			sd_sacl = fi.query_secdesc.out.sd;
		} else if (!sd_sacl) {
			sd_sacl = sd;
		}
		si.set_secdesc.level			= RAW_SFILEINFO_SEC_DESC;
		si.set_secdesc.in.file.fnum		= fnum;
		si.set_secdesc.in.secinfo_flags		= SECINFO_SACL;
		si.set_secdesc.in.sd			= sd_sacl;
		status = smb_raw_setfileinfo(cli->tree, &si);
		CHECK_STATUS_FOR_BIT(status, set_sacl_bits, SEC_FLAG_SYSTEM_SECURITY);

		/* close the handle */
		status = smbcli_close(cli->tree, fnum);
		CHECK_STATUS(status, NT_STATUS_OK);
next:
		continue;
	}

done:
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	return ret;
}


/* 
   basic testing of security descriptor calls
*/
bool torture_raw_acls(struct torture_context *tctx, struct smbcli_state *cli)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_sd(tctx, cli);
	ret &= test_nttrans_create(tctx, cli);
	ret &= test_nttrans_create_null_dacl(tctx, cli);
	ret &= test_creator_sid(tctx, cli);
	ret &= test_generic_bits(tctx, cli);
	ret &= test_owner_bits(tctx, cli);
	ret &= test_inheritance(tctx, cli);
	ret &= test_inheritance_dynamic(tctx, cli);
	ret &= test_sd_get_set(tctx, cli);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
