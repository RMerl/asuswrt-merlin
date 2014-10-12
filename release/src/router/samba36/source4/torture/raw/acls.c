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
		ret = false; \
		torture_result(tctx, TORTURE_FAIL, "(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		goto done; \
	}} while (0)

#define FAIL_UNLESS(__cond)					\
	do {							\
		if (__cond) {} else {				\
			ret = false; \
			torture_result(tctx, TORTURE_FAIL, "%s) condition violated: %s\n", \
			    __location__, #__cond); \
			goto done; \
		}						\
	} while(0)

#define CHECK_SECURITY_DESCRIPTOR(_sd1, _sd2) do { \
	if (!security_descriptor_equal(_sd1, _sd2)) { \
		torture_warning(tctx, "%s: security descriptors don't match!\n", __location__); \
		torture_warning(tctx, "got:\n"); \
		NDR_PRINT_DEBUG(security_descriptor, _sd1); \
		torture_warning(tctx, "expected:\n"); \
		NDR_PRINT_DEBUG(security_descriptor, _sd2); \
		ret = false; \
	} \
} while (0)

/*
 * Helper function to verify a security descriptor, by querying
 * and comparing against the passed in sd.
 * Copied to smb2_util_verify_sd() for SMB2.
 */
static bool verify_sd(TALLOC_CTX *tctx, struct smbcli_state *cli,
    int fnum, struct security_descriptor *sd)
{
	NTSTATUS status;
	bool ret = true;
	union smb_fileinfo q = {};

	if (sd) {
		q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
		q.query_secdesc.in.file.fnum = fnum;
		q.query_secdesc.in.secinfo_flags =
		    SECINFO_OWNER |
		    SECINFO_GROUP |
		    SECINFO_DACL;
		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);

		/* More work is needed if we're going to check this bit. */
		sd->type &= ~SEC_DESC_DACL_AUTO_INHERITED;

		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd);
	}

 done:
	return ret;
}

/*
 * Helper function to verify attributes, by querying
 * and comparing against the passed attrib.
 * Copied to smb2_util_verify_attrib() for SMB2.
 */
static bool verify_attrib(TALLOC_CTX *tctx, struct smbcli_state *cli,
    int fnum, uint32_t attrib)
{
	NTSTATUS status;
	bool ret = true;
	union smb_fileinfo q2 = {};

	if (attrib) {
		q2.standard.level = RAW_FILEINFO_STANDARD;
		q2.standard.in.file.fnum = fnum;
		status = smb_raw_fileinfo(cli->tree, tctx, &q2);
		CHECK_STATUS(status, NT_STATUS_OK);

		q2.standard.out.attrib &= ~FILE_ATTRIBUTE_ARCHIVE;

		if (q2.standard.out.attrib != attrib) {
			torture_warning(tctx, "%s: attributes don't match! "
			    "got %x, expected %x\n", __location__,
			    (uint32_t)q2.standard.out.attrib,
			    (uint32_t)attrib);
			ret = false;
		}
	}

 done:
	return ret;
}

/**
 * Test setting and removing a DACL.
 * Test copied to torture_smb2_setinfo() for SMB2.
 */
static bool test_sd(struct torture_context *tctx, struct smbcli_state *cli)
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING SETFILEINFO EA_SET\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "add a new ACE to the DACL\n");

	test_sid = dom_sid_parse_talloc(tctx, SID_NT_AUTHENTICATED_USERS);

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
	FAIL_UNLESS(verify_sd(tctx, cli, fnum, sd));

	torture_comment(tctx, "remove it again\n");

	status = security_descriptor_dacl_del(sd, test_sid);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);
	FAIL_UNLESS(verify_sd(tctx, cli, fnum, sd));

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/*
  test using nttrans create to create a file with an initial acl set
  Test copied to test_create_acl() for SMB2.
*/
static bool test_nttrans_create_ext(struct torture_context *tctx,
				    struct smbcli_state *cli, bool test_dir)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\acl2.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q = {};
	struct security_ace ace;
	struct security_descriptor *sd;
	struct dom_sid *test_sid;
	uint32_t attrib =
	    FILE_ATTRIBUTE_HIDDEN |
	    FILE_ATTRIBUTE_SYSTEM |
	    (test_dir ? FILE_ATTRIBUTE_DIRECTORY : 0);
	NTSTATUS (*delete_func)(struct smbcli_tree *, const char *) =
	    test_dir ? smbcli_rmdir : smbcli_unlink;

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options =
	    test_dir ? NTCREATEX_OPTIONS_DIRECTORY : 0;
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

	torture_comment(tctx, "basic create\n");

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	torture_comment(tctx, "querying ACL\n");

	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = 
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd = q.query_secdesc.out.sd;

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = delete_func(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "adding a new ACE\n");
	test_sid = dom_sid_parse_talloc(tctx, SID_NT_AUTHENTICATED_USERS);

	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.access_mask = SEC_STD_ALL;
	ace.trustee = *test_sid;

	status = security_descriptor_dacl_add(sd, &ace);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	torture_comment(tctx, "creating with an initial ACL\n");

	io.ntcreatex.in.sec_desc = sd;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	
	FAIL_UNLESS(verify_sd(tctx, cli, fnum, sd));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes\n");

	io.ntcreatex.in.sec_desc = NULL;
	io.ntcreatex.in.file_attr = attrib;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	FAIL_UNLESS(verify_attrib(tctx, cli, fnum, attrib));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = delete_func(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes and ACL\n");

	io.ntcreatex.in.sec_desc = sd;
	io.ntcreatex.in.file_attr = attrib;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	FAIL_UNLESS(verify_sd(tctx, cli, fnum, sd));
	FAIL_UNLESS(verify_attrib(tctx, cli, fnum, attrib));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes, ACL and owner\n");

	sd = security_descriptor_dacl_create(tctx,
					0, SID_WORLD, SID_BUILTIN_USERS,
					SID_WORLD,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					0,
					NULL);

	io.ntcreatex.in.sec_desc = sd;
	io.ntcreatex.in.file_attr = attrib;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	FAIL_UNLESS(verify_sd(tctx, cli, fnum, sd));
	FAIL_UNLESS(verify_attrib(tctx, cli, fnum, attrib));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

static bool test_nttrans_create_file(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	torture_comment(tctx, "Testing nttrans create with sec_desc on files\n");

	return test_nttrans_create_ext(tctx, cli, false);
}

static bool test_nttrans_create_dir(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	torture_comment(tctx, "Testing nttrans create with sec_desc on directories\n");

	return test_nttrans_create_ext(tctx, cli, true);
}

#define CHECK_ACCESS_FLAGS(_fnum, flags) do { \
	union smb_fileinfo _q; \
	_q.access_information.level = RAW_FILEINFO_ACCESS_INFORMATION; \
	_q.access_information.in.file.fnum = (_fnum); \
	status = smb_raw_fileinfo(cli->tree, tctx, &_q); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if (_q.access_information.out.access_flags != (flags)) { \
		ret = false; \
		torture_result(tctx, TORTURE_FAIL, "(%s) Incorrect access_flags 0x%08x - should be 0x%08x\n", \
		       __location__, _q.access_information.out.access_flags, (flags)); \
		goto done; \
	} \
} while (0)

/*
  test using NTTRANS CREATE to create a file with a null ACL set
  Test copied to test_create_null_dacl() for SMB2.
*/
static bool test_nttrans_create_null_dacl(struct torture_context *tctx,
					  struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\nulldacl.txt";
	bool ret = true;
	int fnum = -1;
	union smb_fileinfo q;
	union smb_setfileinfo s;
	struct security_descriptor *sd = security_descriptor_initialise(tctx);
	struct security_acl dacl;

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING SEC_DESC WITH A NULL DACL\n");

	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "creating a file with a empty sd\n");
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	torture_comment(tctx, "get the original sd\n");
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
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL_PRESENT flag not set by the server!\n");
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "no DACL has been created on the server!\n");
		goto done;
	}

	torture_comment(tctx, "set NULL DACL\n");
	sd->type |= SEC_DESC_DACL_PRESENT;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.fnum = fnum;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "get the sd\n");
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
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL_PRESENT flag not set by the server!\n");
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL has been created on the server!\n");
		goto done;
	}

	torture_comment(tctx, "try open for read control\n");
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_STD_READ_CONTROL | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_FILE_READ_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_RIGHTS_FILE_WRITE | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_RIGHTS_FILE_READ | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "set DACL with 0 aces\n");
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

	torture_comment(tctx, "get the sd\n");
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
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL_PRESENT flag not set by the server!\n");
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "no DACL has been created on the server!\n");
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl->num_aces != 0) {
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL has %u aces!\n",
		       q.query_secdesc.out.sd->dacl->num_aces);
		goto done;
	}

	torture_comment(tctx, "try open for read control\n");
	io.ntcreatex.in.access_mask = SEC_STD_READ_CONTROL;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum,
		SEC_STD_READ_CONTROL | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for write => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for read => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic write => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic read => access_denied\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "set empty sd\n");
	sd->type &= ~SEC_DESC_DACL_PRESENT;
	sd->dacl = NULL;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.fnum = fnum;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb_raw_setfileinfo(cli->tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "get the sd\n");
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
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL_PRESENT flag not set by the server!\n");
		goto done;
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		ret = false;
		torture_result(tctx, TORTURE_FAIL, "DACL has been created on the server!\n");
		goto done;
	}
done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test the behaviour of the well known SID_CREATOR_OWNER sid, and some generic
  mapping bits
  Test copied to smb2/acls.c for SMB2.
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING SID_CREATOR_OWNER\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	torture_comment(tctx, "set a sec desc allowing no write by CREATOR_OWNER\n");
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

	torture_comment(tctx, "try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "set a sec desc allowing no write by owner\n");
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

	torture_comment(tctx, "check that sd has been mapped correctly\n");
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd);

	torture_comment(tctx, "try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_FILE_READ_DATA|
			   SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_RIGHTS_FILE_READ);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "set a sec desc allowing generic read by owner\n");
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

	torture_comment(tctx, "check that generic read has been mapped correctly\n");
	sd2 = security_descriptor_dacl_create(tctx,
					 0, owner_sid, NULL,
					 owner_sid,
					 SEC_ACE_TYPE_ACCESS_ALLOWED,
					 SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					 0,
					 NULL);

	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

	torture_comment(tctx, "try open for write\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for read\n");
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
			   SEC_FILE_READ_DATA | 
			   SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

	torture_comment(tctx, "try open for generic write\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_WRITE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "try open for generic read\n");
	io.ntcreatex.in.access_mask = SEC_GENERIC_READ;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, SEC_RIGHTS_FILE_READ);
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);


	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);


done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test the mapping of the SEC_GENERIC_xx bits to SEC_STD_xx and
  SEC_FILE_xx bits
  Test copied to smb2/acls.c for SMB2.
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING FILE GENERIC BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n",
		    nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n",
		    nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

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

		torture_comment(tctx, "Testing generic bits 0x%08x\n",
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
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask | file_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

		if (!has_take_ownership_privilege) {
			continue;
		}

		torture_comment(tctx, "Testing generic bits 0x%08x (anonymous)\n",
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
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask_anon | file_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	}

	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);


	torture_comment(tctx, "TESTING DIR GENERIC BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n",
		    nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n",
		    nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

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

		torture_comment(tctx, "Testing generic bits 0x%08x\n",
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
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask | dir_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

		if (!has_take_ownership_privilege) {
			continue;
		}

		torture_comment(tctx, "Testing generic bits 0x%08x (anonymous)\n",
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
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

		io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		status = smb_raw_open(cli->tree, tctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, 
				   expected_mask_anon | dir_mappings[i].specific_bits);
		smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
	}

	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  see what access bits the owner of a file always gets
  Test copied to smb2/acls.c for SMB2.
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING FILE OWNER BITS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n", nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_RESTORE - %s\n", has_restore_privilege?"Yes":"No");

	status = torture_check_privilege(cli, 
					    owner_sid, 
					    sec_privilege_name(SEC_PRIV_TAKE_OWNERSHIP));
	has_take_ownership_privilege = NT_STATUS_IS_OK(status);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "torture_check_privilege - %s\n", nt_errstr(status));
	}
	torture_comment(tctx, "SEC_PRIV_TAKE_OWNERSHIP - %s\n", has_take_ownership_privilege?"Yes":"No");

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
				torture_warning(tctx, "failed with access mask 0x%08x of expected 0x%08x\n",
				       bit, expected_bits);
			}
			CHECK_STATUS(status, NT_STATUS_OK);
			CHECK_ACCESS_FLAGS(io.ntcreatex.out.file.fnum, bit | SEC_FILE_READ_ATTRIBUTE);
			smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);
		} else {
			if (NT_STATUS_IS_OK(status)) {
				torture_warning(tctx, "open succeeded with access mask 0x%08x of "
					"expected 0x%08x - should fail\n",
				       bit, expected_bits);
			}
			CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
		}
	}

	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}



/*
  test the inheritance of ACL flags onto new files and directories
  Test copied to smb2/acls.c for SMB2.
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
	struct security_descriptor *sd, *sd2, *sd_orig=NULL, *sd_def1, *sd_def2;
	const char *owner_sid, *group_sid;
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING ACL INHERITANCE\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER | SECINFO_GROUP;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);
	group_sid = dom_sid_string(tctx, sd_orig->group_sid);

	torture_comment(tctx, "owner_sid is %s\n", owner_sid);
	torture_comment(tctx, "group_sid is %s\n", group_sid);

	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;

	if (torture_setting_bool(tctx, "samba4", false)) {
		/* the default ACL in Samba4 includes the group and
		   other permissions */
		sd_def1 = security_descriptor_dacl_create(tctx,
							 0, owner_sid, NULL,
							 owner_sid,
							 SEC_ACE_TYPE_ACCESS_ALLOWED,
							 SEC_RIGHTS_FILE_ALL,
							 0,
							 group_sid,
							 SEC_ACE_TYPE_ACCESS_ALLOWED,
							 SEC_RIGHTS_FILE_READ | SEC_FILE_EXECUTE,
							 0,
							 SID_WORLD,
							 SEC_ACE_TYPE_ACCESS_ALLOWED,
							 SEC_RIGHTS_FILE_READ | SEC_FILE_EXECUTE,
							 0,
							 SID_NT_SYSTEM,
							 SEC_ACE_TYPE_ACCESS_ALLOWED,
							 SEC_RIGHTS_FILE_ALL,
							 0,
							 NULL);
	} else {
		/*
		 * The Windows Default ACL for a new file, when there is no ACL to be
		 * inherited: FullControl for the owner and SYSTEM.
		 */
		sd_def1 = security_descriptor_dacl_create(tctx,
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
	}

	/*
	 * Use this in the case the system being tested does not add an ACE for
	 * the SYSTEM SID.
	 */
	sd_def2 = security_descriptor_dacl_create(tctx,
					    0, owner_sid, NULL,
					    owner_sid,
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
			if (!security_descriptor_equal(q.query_secdesc.out.sd, sd_def1) &&
			    !security_descriptor_equal(q.query_secdesc.out.sd, sd_def2)) {
				torture_warning(tctx, "Expected default sd "
				    "for i=%d:\n", i);
				NDR_PRINT_DEBUG(security_descriptor, sd_def1);
				torture_warning(tctx, "at %d - got:\n", i);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			}
			goto check_dir;
		}

		if (q.query_secdesc.out.sd->dacl == NULL ||
		    q.query_secdesc.out.sd->dacl->num_aces != 1 ||
		    q.query_secdesc.out.sd->dacl->aces[0].access_mask != SEC_FILE_WRITE_DATA ||
		    !dom_sid_equal(&q.query_secdesc.out.sd->dacl->aces[0].trustee,
				   sd_orig->owner_sid)) {
			ret = false;
			torture_warning(tctx, "Bad sd in child file at %d\n", i);
			NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
			goto check_dir;
		}

		if (q.query_secdesc.out.sd->dacl->aces[0].flags != 
		    test_flags[i].file_flags) {
			torture_warning(tctx, "incorrect file_flags 0x%x - expected 0x%x for parent 0x%x with (i=%d)\n",
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
			if (!security_descriptor_equal(q.query_secdesc.out.sd, sd_def1) &&
			    !security_descriptor_equal(q.query_secdesc.out.sd, sd_def2)) {
				torture_warning(tctx, "Expected default sd for dir at %d:\n", i);
				NDR_PRINT_DEBUG(security_descriptor, sd_def1);
				torture_warning(tctx, "got:\n");
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
				torture_warning(tctx, "(CI & NP) Bad sd in child dir - expected 0x%x for parent 0x%x (i=%d)\n",
				       test_flags[i].dir_flags,
				       test_flags[i].parent_flags, i);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				torture_comment(tctx, "FYI, here is the parent sd:\n");
				NDR_PRINT_DEBUG(security_descriptor, sd);
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
				torture_warning(tctx, "(CI) Bad sd in child dir - expected 0x%x for parent 0x%x (i=%d)\n",
				       test_flags[i].dir_flags,
				       test_flags[i].parent_flags, i);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				torture_comment(tctx, "FYI, here is the parent sd:\n");
				NDR_PRINT_DEBUG(security_descriptor, sd);
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
				torture_warning(tctx, "(0) Bad sd in child dir - expected 0x%x for parent 0x%x (i=%d)\n",
				       test_flags[i].dir_flags,
				       test_flags[i].parent_flags, i);
				NDR_PRINT_DEBUG(security_descriptor, q.query_secdesc.out.sd);
				torture_comment(tctx, "FYI, here is the parent sd:\n");
				NDR_PRINT_DEBUG(security_descriptor, sd);
				ret = false;
				continue;
			}
		}
	}

	torture_comment(tctx, "Testing access checks on inherited create with %s\n", fname1);
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

	/* Check DACL we just set. */
	torture_comment(tctx, "checking new sd\n");
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd);

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
	CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "failed: w2k3 ACL bug (allowed open when ACL should deny)\n");
		ret = false;
		fnum2 = io.ntcreatex.out.file.fnum;
		CHECK_ACCESS_FLAGS(fnum2, SEC_RIGHTS_FILE_ALL);
		smbcli_close(cli->tree, fnum2);
	} else {
		if (TARGET_IS_WIN7(tctx)) {
			CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		} else {
			CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
		}
	}

	torture_comment(tctx, "trying without execute\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL & ~SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "and with full permissions again\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_ACCESS_FLAGS(fnum2, SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, fnum2);

	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (TARGET_IS_WIN7(tctx)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_ACCESS_FLAGS(fnum2, SEC_FILE_WRITE_DATA | SEC_FILE_READ_ATTRIBUTE);
	smbcli_close(cli->tree, fnum2);

done:
	if (sd_orig != NULL) {
		set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
		set.set_secdesc.in.file.fnum = fnum;
		set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
		set.set_secdesc.in.sd = sd_orig;
		status = smb_raw_setfileinfo(cli->tree, &set);
	}

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname1);
	smbcli_rmdir(cli->tree, dname);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

static bool test_inheritance_flags(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *dname = BASEDIR "\\inheritance";
	const char *fname1 = BASEDIR "\\inheritance\\testfile";
	bool ret = true;
	int fnum=0, fnum2, i, j;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd2, *sd_orig=NULL;
	const char *owner_sid;
	struct {
		uint32_t parent_set_sd_type; /* 3 options */
		uint32_t parent_set_ace_inherit; /* 1 option */
		uint32_t parent_get_sd_type;
		uint32_t parent_get_ace_inherit;
		uint32_t child_get_sd_type;
		uint32_t child_get_ace_inherit;
	} tflags[16]; /* 2^4 */

	for (i = 0; i < 15; i++) {
		torture_comment(tctx, "i=%d:", i);

		ZERO_STRUCT(tflags[i]);

		if (i & 1) {
			tflags[i].parent_set_sd_type |=
			    SEC_DESC_DACL_AUTO_INHERITED;
			torture_comment(tctx, "AUTO_INHERITED, ");
		}
		if (i & 2) {
			tflags[i].parent_set_sd_type |=
			    SEC_DESC_DACL_AUTO_INHERIT_REQ;
			torture_comment(tctx, "AUTO_INHERIT_REQ, ");
		}
		if (i & 4) {
			tflags[i].parent_set_sd_type |=
			    SEC_DESC_DACL_PROTECTED;
			tflags[i].parent_get_sd_type |=
			    SEC_DESC_DACL_PROTECTED;
			torture_comment(tctx, "PROTECTED, ");
		}
		if (i & 8) {
			tflags[i].parent_set_ace_inherit |=
			    SEC_ACE_FLAG_INHERITED_ACE;
			tflags[i].parent_get_ace_inherit |=
			    SEC_ACE_FLAG_INHERITED_ACE;
			torture_comment(tctx, "INHERITED, ");
		}

		if ((tflags[i].parent_set_sd_type &
		    (SEC_DESC_DACL_AUTO_INHERITED | SEC_DESC_DACL_AUTO_INHERIT_REQ)) ==
		    (SEC_DESC_DACL_AUTO_INHERITED | SEC_DESC_DACL_AUTO_INHERIT_REQ)) {
			tflags[i].parent_get_sd_type |=
			    SEC_DESC_DACL_AUTO_INHERITED;
			tflags[i].child_get_sd_type |=
			    SEC_DESC_DACL_AUTO_INHERITED;
			tflags[i].child_get_ace_inherit |=
			    SEC_ACE_FLAG_INHERITED_ACE;
			torture_comment(tctx, "  ... parent is AUTO INHERITED");
		}

		if (tflags[i].parent_set_ace_inherit &
		    SEC_ACE_FLAG_INHERITED_ACE) {
			tflags[i].parent_get_ace_inherit =
			    SEC_ACE_FLAG_INHERITED_ACE;
			torture_comment(tctx, "  ... parent ACE is INHERITED");
		}

		torture_comment(tctx, "\n");
	}

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING ACL INHERITANCE FLAGS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_MASK;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	torture_comment(tctx, "creating initial directory %s\n", dname);
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	torture_comment(tctx, "getting original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);
	torture_comment(tctx, "owner_sid is %s\n", owner_sid);

	for (i=0; i < ARRAY_SIZE(tflags); i++) {
		torture_comment(tctx, "setting a new sd on directory, pass #%d\n", i);

		sd = security_descriptor_dacl_create(tctx,
						tflags[i].parent_set_sd_type,
						NULL, NULL,
						owner_sid,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						SEC_FILE_WRITE_DATA | SEC_STD_WRITE_DAC,
						SEC_ACE_FLAG_OBJECT_INHERIT |
						SEC_ACE_FLAG_CONTAINER_INHERIT |
						tflags[i].parent_set_ace_inherit,
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

		/*
		 * Check DACL we just set, except change the bits to what they
		 * should be.
		 */
		torture_comment(tctx, "  checking new sd\n");

		/* REQ bit should always be false. */
		sd->type &= ~SEC_DESC_DACL_AUTO_INHERIT_REQ;

		if ((tflags[i].parent_get_sd_type & SEC_DESC_DACL_AUTO_INHERITED) == 0)
			sd->type &= ~SEC_DESC_DACL_AUTO_INHERITED;

		q.query_secdesc.in.file.fnum = fnum;
		q.query_secdesc.in.secinfo_flags = SECINFO_DACL;
		status = smb_raw_fileinfo(cli->tree, tctx, &q);
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd);

		/* Create file. */
		torture_comment(tctx, "  creating file %s\n", fname1);
		io.ntcreatex.in.fname = fname1;
		io.ntcreatex.in.create_options = 0;
		io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
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

		torture_comment(tctx, "  checking sd on file %s\n", fname1);
		sd2 = security_descriptor_dacl_create(tctx,
						 tflags[i].child_get_sd_type,
						 owner_sid, NULL,
						 owner_sid,
						 SEC_ACE_TYPE_ACCESS_ALLOWED,
						 SEC_FILE_WRITE_DATA | SEC_STD_WRITE_DAC,
						 tflags[i].child_get_ace_inherit,
						 NULL);
		CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);

		/*
		 * Set new sd on file ... prove that the bits have nothing to
		 * do with the parents bits when manually setting an ACL. The
		 * _AUTO_INHERITED bit comes directly from the ACL set.
		 */
		for (j = 0; j < ARRAY_SIZE(tflags); j++) {
			torture_comment(tctx, "  setting new file sd, pass #%d\n", j);

			/* Change sd type. */
			sd2->type &= ~(SEC_DESC_DACL_AUTO_INHERITED |
			    SEC_DESC_DACL_AUTO_INHERIT_REQ |
			    SEC_DESC_DACL_PROTECTED);
			sd2->type |= tflags[j].parent_set_sd_type;

			sd2->dacl->aces[0].flags &=
			    ~SEC_ACE_FLAG_INHERITED_ACE;
			sd2->dacl->aces[0].flags |=
			    tflags[j].parent_set_ace_inherit;

			set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
			set.set_secdesc.in.file.fnum = fnum2;
			set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
			set.set_secdesc.in.sd = sd2;
			status = smb_raw_setfileinfo(cli->tree, &set);
			CHECK_STATUS(status, NT_STATUS_OK);

			/* Check DACL we just set. */
			sd2->type &= ~SEC_DESC_DACL_AUTO_INHERIT_REQ;
			if ((tflags[j].parent_get_sd_type & SEC_DESC_DACL_AUTO_INHERITED) == 0)
				sd2->type &= ~SEC_DESC_DACL_AUTO_INHERITED;

			q.query_secdesc.in.file.fnum = fnum2;
			q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
			status = smb_raw_fileinfo(cli->tree, tctx, &q);
			CHECK_STATUS(status, NT_STATUS_OK);

			CHECK_SECURITY_DESCRIPTOR(q.query_secdesc.out.sd, sd2);
		}

		smbcli_close(cli->tree, fnum2);
		smbcli_unlink(cli->tree, fname1);
	}

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test dynamic acl inheritance
  Test copied to smb2/acls.c for SMB2.
*/
static bool test_inheritance_dynamic(struct torture_context *tctx, 
									 struct smbcli_state *cli)
{
	NTSTATUS status;
	union smb_open io;
	const char *dname = BASEDIR "\\inheritance2";
	const char *fname1 = BASEDIR "\\inheritance2\\testfile";
	bool ret = true;
	int fnum=0, fnum2;
	union smb_fileinfo q;
	union smb_setfileinfo set;
	struct security_descriptor *sd, *sd_orig=NULL;
	const char *owner_sid;
	
	torture_comment(tctx, "TESTING DYNAMIC ACL INHERITANCE\n");

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
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

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	torture_comment(tctx, "owner_sid is %s\n", owner_sid);

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

	torture_comment(tctx, "create a file with an inherited acl\n");
	io.ntcreatex.in.fname = fname1;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);

	torture_comment(tctx, "try and access file with base rights - should be OK\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);

	torture_comment(tctx, "try and access file with extra rights - should be denied\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA | SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	torture_comment(tctx, "update parent sd\n");
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

	torture_comment(tctx, "try and access file with base rights - should be OK\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	smbcli_close(cli->tree, fnum2);


	torture_comment(tctx, "try and access now - should be OK if dynamic inheritance works\n");
	io.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA | SEC_FILE_EXECUTE;
	status = smb_raw_open(cli->tree, tctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		torture_comment(tctx, "Server does not have dynamic inheritance\n");
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		torture_comment(tctx, "Server does have dynamic inheritance\n");
	}
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	smbcli_unlink(cli->tree, fname1);

done:
	torture_comment(tctx, "put back original sd\n");
	set.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	set.set_secdesc.in.file.fnum = fnum;
	set.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	set.set_secdesc.in.sd = sd_orig;
	status = smb_raw_setfileinfo(cli->tree, &set);

	smbcli_close(cli->tree, fnum);
	smbcli_rmdir(cli->tree, dname);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

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
			ret = false; \
			torture_result(tctx, TORTURE_FAIL, "(%s) %s but flags 0x%08X are not granted! granted[0x%08X] desired[0x%08X]\n", \
			       __location__, nt_errstr(status), access, granted, desired); \
			goto done; \
		} \
	} else { \
		if (granted & access) {\
			ret = false; \
			torture_result(tctx, TORTURE_FAIL, "(%s) %s but flags 0x%08X are granted! granted[0x%08X] desired[0x%08X]\n", \
			       __location__, nt_errstr(status), access, granted, desired); \
			goto done; \
		} \
	} \
	CHECK_STATUS_FOR_BIT_ACTION(status, bits, do {} while (0)); \
} while (0)

/* test what access mask is needed for getting and setting security_descriptors
  Test copied to smb2/acls.c for SMB2. */
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

	if (!torture_setup_dir(cli, BASEDIR))
		return false;

	torture_comment(tctx, "TESTING ACCESS MASKS FOR SD GET/SET\n");

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
	io.ntcreatex.in.root_fid.fnum = 0;
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
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/* 
   basic testing of security descriptor calls
*/
struct torture_suite *torture_raw_acls(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "acls");

	torture_suite_add_1smb_test(suite, "sd", test_sd);
	torture_suite_add_1smb_test(suite, "create_file", test_nttrans_create_file);
	torture_suite_add_1smb_test(suite, "create_dir", test_nttrans_create_dir);
	torture_suite_add_1smb_test(suite, "nulldacl", test_nttrans_create_null_dacl);
	torture_suite_add_1smb_test(suite, "creator", test_creator_sid);
	torture_suite_add_1smb_test(suite, "generic", test_generic_bits);
	torture_suite_add_1smb_test(suite, "owner", test_owner_bits);
	torture_suite_add_1smb_test(suite, "inheritance", test_inheritance);

	/* torture_suite_add_1smb_test(suite, "INHERITFLAGS", test_inheritance_flags); */
	torture_suite_add_1smb_test(suite, "dynamic", test_inheritance_dynamic);
	/* XXX This test does not work against XP or Vista.
	torture_suite_add_1smb_test(suite, "GETSET", test_sd_get_set);
	*/

	return suite;
}
