/* 
   Unix SMB/CIFS implementation.

   SMB2 setinfo individual test suite

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"

#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"

#define BASEDIR ""

/* basic testing of all SMB2 setinfo calls 
   for each call we test that it succeeds, and where possible test 
   for consistency between the calls. 
*/
bool torture_smb2_setinfo(struct torture_context *torture)
{
	struct smb2_tree *tree;
	bool ret = true;
	struct smb2_handle handle;
	char *fname;
	char *fname_new;
	union smb_fileinfo finfo2;
	union smb_setfileinfo sfinfo;
	struct security_ace ace;
	struct security_descriptor *sd;
	struct dom_sid *test_sid;
	NTSTATUS status, status2=NT_STATUS_OK;
	const char *call_name;
	time_t basetime = (time(NULL) - 86400) & ~1;
	int n = time(NULL) % 100;
	
	ZERO_STRUCT(handle);
	
	fname = talloc_asprintf(torture, BASEDIR "fnum_test_%d.txt", n);
	fname_new = talloc_asprintf(torture, BASEDIR "fnum_test_new_%d.txt", n);

	if (!torture_smb2_connection(torture, &tree)) {
		return false;
	}

#define RECREATE_FILE(fname) do { \
	smb2_util_close(tree, handle); \
	status = smb2_create_complex_file(tree, fname, &handle); \
	if (!NT_STATUS_IS_OK(status)) { \
		printf("(%s) ERROR: open of %s failed (%s)\n", \
		       __location__, fname, nt_errstr(status)); \
		ret = false; \
		goto done; \
	}} while (0)

#define RECREATE_BOTH do { \
		RECREATE_FILE(fname); \
	} while (0)

	RECREATE_BOTH;
	
#define CHECK_CALL(call, rightstatus) do { \
	call_name = #call; \
	sfinfo.generic.level = RAW_SFILEINFO_ ## call; \
	sfinfo.generic.in.file.handle = handle; \
	status = smb2_setinfo_file(tree, &sfinfo); \
	if (!NT_STATUS_EQUAL(status, rightstatus)) { \
		printf("(%s) %s - %s (should be %s)\n", __location__, #call, \
			nt_errstr(status), nt_errstr(rightstatus)); \
		ret = false; \
		goto done; \
	} \
	} while (0)

#define CHECK1(call) \
	do { if (NT_STATUS_IS_OK(status)) { \
		finfo2.generic.level = RAW_FILEINFO_ ## call; \
		finfo2.generic.in.file.handle = handle; \
		status2 = smb2_getinfo_file(tree, torture, &finfo2); \
		if (!NT_STATUS_IS_OK(status2)) { \
			printf("(%s) %s - %s\n", __location__, #call, nt_errstr(status2)); \
		ret = false; \
		goto done; \
		} \
	}} while (0)

#define CHECK_VALUE(call, stype, field, value) do { \
 	CHECK1(call); \
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(status2) && finfo2.stype.out.field != value) { \
		printf("(%s) %s - %s/%s should be 0x%x - 0x%x\n", __location__, \
		       call_name, #stype, #field, \
		       (uint_t)value, (uint_t)finfo2.stype.out.field); \
		torture_smb2_all_info(tree, handle); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_TIME(call, stype, field, value) do { \
 	CHECK1(call); \
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(status2) && nt_time_to_unix(finfo2.stype.out.field) != value) { \
		printf("(%s) %s - %s/%s should be 0x%x - 0x%x\n", __location__, \
		        call_name, #stype, #field, \
		        (uint_t)value, \
			(uint_t)nt_time_to_unix(finfo2.stype.out.field)); \
		printf("\t%s", timestring(torture, value)); \
		printf("\t%s\n", nt_time_string(torture, finfo2.stype.out.field)); \
		torture_smb2_all_info(tree, handle); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

	torture_smb2_all_info(tree, handle);
	
	printf("test basic_information level\n");
	basetime += 86400;
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, basetime + 100);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, basetime + 200);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  basetime + 300);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, basetime + 400);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_READONLY;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, create_time, basetime + 100);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, access_time, basetime + 200);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, write_time,  basetime + 300);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, change_time, basetime + 400);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, attrib,     FILE_ATTRIBUTE_READONLY);

	printf("a zero time means don't change\n");
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  0);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, 0);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_NORMAL;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, create_time, basetime + 100);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, access_time, basetime + 200);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, write_time,  basetime + 300);
	CHECK_TIME(SMB2_ALL_INFORMATION, all_info2, change_time, basetime + 400);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, attrib,     FILE_ATTRIBUTE_NORMAL);

	printf("change the attribute\n");
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, attrib, FILE_ATTRIBUTE_HIDDEN);

	printf("zero attrib means don't change\n");
	sfinfo.basic_info.in.attrib = 0;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, attrib, FILE_ATTRIBUTE_HIDDEN);

	printf("can't change a file to a directory\n");
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_INVALID_PARAMETER);

	printf("restore attribute\n");
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_NORMAL;
	CHECK_CALL(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, attrib, FILE_ATTRIBUTE_NORMAL);

	printf("test disposition_information level\n");
	sfinfo.disposition_info.in.delete_on_close = 1;
	CHECK_CALL(DISPOSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, delete_pending, 1);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, nlink, 0);

	sfinfo.disposition_info.in.delete_on_close = 0;
	CHECK_CALL(DISPOSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, delete_pending, 0);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, nlink, 1);

	printf("test allocation_information level\n");
	sfinfo.allocation_info.in.alloc_size = 0;
	CHECK_CALL(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, size, 0);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, alloc_size, 0);

	sfinfo.allocation_info.in.alloc_size = 4096;
	CHECK_CALL(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, alloc_size, 4096);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, size, 0);

	printf("test end_of_file_info level\n");
	sfinfo.end_of_file_info.in.size = 37;
	CHECK_CALL(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, size, 37);

	sfinfo.end_of_file_info.in.size = 7;
	CHECK_CALL(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, size, 7);

	printf("test position_information level\n");
	sfinfo.position_information.in.position = 123456;
	CHECK_CALL(POSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(POSITION_INFORMATION, position_information, position, 123456);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, position, 123456);

	printf("test mode_information level\n");
	sfinfo.mode_information.in.mode = 2;
	CHECK_CALL(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 2);
	CHECK_VALUE(SMB2_ALL_INFORMATION, all_info2, mode, 2);

	sfinfo.mode_information.in.mode = 1;
	CHECK_CALL(MODE_INFORMATION, NT_STATUS_INVALID_PARAMETER);

	sfinfo.mode_information.in.mode = 0;
	CHECK_CALL(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 0);

	printf("test sec_desc level\n");
	ZERO_STRUCT(finfo2);
	finfo2.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
 	CHECK1(SEC_DESC);
	sd = finfo2.query_secdesc.out.sd;

	test_sid = dom_sid_parse_talloc(torture, "S-1-5-32-1234-5432");
	ZERO_STRUCT(ace);
	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.access_mask = SEC_STD_ALL;
	ace.trustee = *test_sid;
	status = security_descriptor_dacl_add(sd, &ace);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("add a new ACE to the DACL\n");

	sfinfo.set_secdesc.in.secinfo_flags = finfo2.query_secdesc.in.secinfo_flags;
	sfinfo.set_secdesc.in.sd = sd;
	CHECK_CALL(SEC_DESC, NT_STATUS_OK);
 	CHECK1(SEC_DESC);

	if (!security_acl_equal(finfo2.query_secdesc.out.sd->dacl, sd->dacl)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, finfo2.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

	printf("remove it again\n");

	status = security_descriptor_dacl_del(sd, test_sid);
	CHECK_STATUS(status, NT_STATUS_OK);

	sfinfo.set_secdesc.in.secinfo_flags = finfo2.query_secdesc.in.secinfo_flags;
	sfinfo.set_secdesc.in.sd = sd;
	CHECK_CALL(SEC_DESC, NT_STATUS_OK);
 	CHECK1(SEC_DESC);

	if (!security_acl_equal(finfo2.query_secdesc.out.sd->dacl, sd->dacl)) {
		printf("%s: security descriptors don't match!\n", __location__);
		printf("got:\n");
		NDR_PRINT_DEBUG(security_descriptor, finfo2.query_secdesc.out.sd);
		printf("expected:\n");
		NDR_PRINT_DEBUG(security_descriptor, sd);
		ret = false;
	}

done:
	status = smb2_util_close(tree, handle);
	if (NT_STATUS_IS_ERR(status)) {
		printf("Failed to delete %s - %s\n", fname, nt_errstr(status));
	}
	smb2_util_unlink(tree, fname);

	return ret;
}


