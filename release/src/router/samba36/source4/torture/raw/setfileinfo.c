/* 
   Unix SMB/CIFS implementation.
   RAW_SFILEINFO_* individual test suite
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
#include "system/time.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\testsfileinfo"

/* basic testing of all RAW_SFILEINFO_* calls 
   for each call we test that it succeeds, and where possible test 
   for consistency between the calls. 
*/
static bool
torture_raw_sfileinfo_base(struct torture_context *torture, struct smbcli_state *cli)
{
	bool ret = true;
	int fnum = -1;
	char *fnum_fname;
	char *fnum_fname_new;
	char *path_fname;
	char *path_fname_new;
	union smb_fileinfo finfo1, finfo2;
	union smb_setfileinfo sfinfo;
	NTSTATUS status, status2;
	const char *call_name;
	time_t basetime = (time(NULL) - 86400) & ~1;
	bool check_fnum;
	int n = time(NULL) % 100;
	
	asprintf(&path_fname, BASEDIR "\\fname_test_%d.txt", n);
	asprintf(&path_fname_new, BASEDIR "\\fname_test_new_%d.txt", n);
	asprintf(&fnum_fname, BASEDIR "\\fnum_test_%d.txt", n);
	asprintf(&fnum_fname_new, BASEDIR "\\fnum_test_new_%d.txt", n);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

#define RECREATE_FILE(fname) do { \
	if (fnum != -1) smbcli_close(cli->tree, fnum); \
	fnum = create_complex_file(cli, torture, fname); \
	if (fnum == -1) { \
		printf("(%s) ERROR: open of %s failed (%s)\n", \
		       __location__, fname, smbcli_errstr(cli->tree)); \
		ret = false; \
		goto done; \
	}} while (0)

#define RECREATE_BOTH do { \
		RECREATE_FILE(path_fname); \
		smbcli_close(cli->tree, fnum); \
		RECREATE_FILE(fnum_fname); \
	} while (0)

	RECREATE_BOTH;
	
#define CHECK_CALL_FNUM(call, rightstatus) do { \
	check_fnum = true; \
	call_name = #call; \
	sfinfo.generic.level = RAW_SFILEINFO_ ## call; \
	sfinfo.generic.in.file.fnum = fnum; \
	status = smb_raw_setfileinfo(cli->tree, &sfinfo); \
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) { \
                torture_warning(torture, \
			"(%s) %s - %s", __location__, #call, \
                        nt_errstr(status)); \
        } else if (!NT_STATUS_EQUAL(status, rightstatus)) { \
		printf("(%s) %s - %s (should be %s)\n", __location__, #call, \
			nt_errstr(status), nt_errstr(rightstatus)); \
		ret = false; \
	} \
	finfo1.generic.level = RAW_FILEINFO_ALL_INFO; \
	finfo1.generic.in.file.fnum = fnum; \
	status2 = smb_raw_fileinfo(cli->tree, torture, &finfo1); \
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) { \
                torture_warning(torture, \
			"(%s) %s - %s", __location__, #call, \
                        nt_errstr(status)); \
        } else if (!NT_STATUS_IS_OK(status2)) { \
		printf("(%s) %s pathinfo - %s\n", __location__, #call, nt_errstr(status)); \
		ret = false; \
	}} while (0)

#define CHECK_CALL_PATH(call, rightstatus) do { \
	check_fnum = false; \
	call_name = #call; \
	sfinfo.generic.level = RAW_SFILEINFO_ ## call; \
	sfinfo.generic.in.file.path = path_fname; \
	status = smb_raw_setpathinfo(cli->tree, &sfinfo); \
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) { \
		sfinfo.generic.in.file.path = path_fname_new; \
		status = smb_raw_setpathinfo(cli->tree, &sfinfo); \
	} \
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) { \
                torture_warning(torture, \
			"(%s) %s - %s", __location__, #call, \
                        nt_errstr(status)); \
        } else if (!NT_STATUS_EQUAL(status, rightstatus)) { \
		printf("(%s) %s - %s (should be %s)\n", __location__, #call, \
			nt_errstr(status), nt_errstr(rightstatus)); \
		ret = false; \
	} \
	finfo1.generic.level = RAW_FILEINFO_ALL_INFO; \
	finfo1.generic.in.file.path = path_fname; \
	status2 = smb_raw_pathinfo(cli->tree, torture, &finfo1); \
	if (NT_STATUS_EQUAL(status2, NT_STATUS_OBJECT_NAME_NOT_FOUND)) { \
		finfo1.generic.in.file.path = path_fname_new; \
		status2 = smb_raw_pathinfo(cli->tree, torture, &finfo1); \
	} \
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) { \
                torture_warning(torture, \
			"(%s) %s - %s", __location__, #call, \
                        nt_errstr(status)); \
        } else if (!NT_STATUS_IS_OK(status2)) { \
		printf("(%s) %s pathinfo - %s\n", __location__, #call, nt_errstr(status2)); \
		ret = false; \
	}} while (0)

#define CHECK1(call) \
	do { if (NT_STATUS_IS_OK(status)) { \
		finfo2.generic.level = RAW_FILEINFO_ ## call; \
		if (check_fnum) { \
			finfo2.generic.in.file.fnum = fnum; \
			status2 = smb_raw_fileinfo(cli->tree, torture, &finfo2); \
		} else { \
			finfo2.generic.in.file.path = path_fname; \
			status2 = smb_raw_pathinfo(cli->tree, torture, &finfo2); \
			if (NT_STATUS_EQUAL(status2, NT_STATUS_OBJECT_NAME_NOT_FOUND)) { \
				finfo2.generic.in.file.path = path_fname_new; \
				status2 = smb_raw_pathinfo(cli->tree, torture, &finfo2); \
			} \
		} \
		if (!NT_STATUS_IS_OK(status2)) { \
			printf("%s - %s\n", #call, nt_errstr(status2)); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_VALUE(call, stype, field, value) do { \
 	CHECK1(call); \
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(status2) && finfo2.stype.out.field != value) { \
		printf("(%s) %s - %s/%s should be 0x%x - 0x%x\n", __location__, \
		       call_name, #stype, #field, \
		       (unsigned int)value, (unsigned int)finfo2.stype.out.field); \
		dump_all_info(torture, &finfo1); \
		ret = false; \
	}} while (0)

#define CHECK_TIME(call, stype, field, value) do { \
 	CHECK1(call); \
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(status2) && nt_time_to_unix(finfo2.stype.out.field) != value) { \
		printf("(%s) %s - %s/%s should be 0x%x - 0x%x\n", __location__, \
		        call_name, #stype, #field, \
		        (unsigned int)value, \
			(unsigned int)nt_time_to_unix(finfo2.stype.out.field)); \
		printf("\t%s", timestring(torture, value)); \
		printf("\t%s\n", nt_time_string(torture, finfo2.stype.out.field)); \
		dump_all_info(torture, &finfo1); \
		ret = false; \
	}} while (0)

#define CHECK_STR(call, stype, field, value) do { \
 	CHECK1(call); \
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(status2) && strcmp(finfo2.stype.out.field, value) != 0) { \
		printf("(%s) %s - %s/%s should be '%s' - '%s'\n", __location__, \
		        call_name, #stype, #field, \
		        value, \
			finfo2.stype.out.field); \
		dump_all_info(torture, &finfo1); \
		ret = false; \
	}} while (0)

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

	
	printf("Test setattr\n");
	sfinfo.setattr.in.attrib = FILE_ATTRIBUTE_READONLY;
	sfinfo.setattr.in.write_time = basetime;
	CHECK_CALL_PATH(SETATTR, NT_STATUS_OK);
	CHECK_VALUE  (ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_READONLY);
	CHECK_TIME   (ALL_INFO, all_info, write_time, basetime);

	printf("setting to NORMAL doesn't do anything\n");
	sfinfo.setattr.in.attrib = FILE_ATTRIBUTE_NORMAL;
	sfinfo.setattr.in.write_time = 0;
	CHECK_CALL_PATH(SETATTR, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_READONLY);
	CHECK_TIME (ALL_INFO, all_info, write_time, basetime);

	printf("a zero write_time means don't change\n");
	sfinfo.setattr.in.attrib = 0;
	sfinfo.setattr.in.write_time = 0;
	CHECK_CALL_PATH(SETATTR, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_NORMAL);
	CHECK_TIME (ALL_INFO, all_info, write_time, basetime);

	printf("Test setattre\n");
	sfinfo.setattre.in.create_time = basetime + 20;
	sfinfo.setattre.in.access_time = basetime + 30;
	sfinfo.setattre.in.write_time  = basetime + 40;
	CHECK_CALL_FNUM(SETATTRE, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 20);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 30);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 40);

	sfinfo.setattre.in.create_time = 0;
	sfinfo.setattre.in.access_time = 0;
	sfinfo.setattre.in.write_time  = 0;
	CHECK_CALL_FNUM(SETATTRE, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 20);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 30);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 40);

	printf("Test standard level\n");
	sfinfo.standard.in.create_time = basetime + 100;
	sfinfo.standard.in.access_time = basetime + 200;
	sfinfo.standard.in.write_time  = basetime + 300;
	CHECK_CALL_FNUM(STANDARD, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);

	printf("Test basic_info level\n");
	basetime += 86400;
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, basetime + 100);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, basetime + 200);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  basetime + 300);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, basetime + 400);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_READONLY;
	CHECK_CALL_FNUM(BASIC_INFO, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);
	CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_READONLY);

	printf("a zero time means don't change\n");
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  0);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, 0);
	sfinfo.basic_info.in.attrib = 0;
	CHECK_CALL_FNUM(BASIC_INFO, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);
	CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_READONLY);

	printf("Test basic_information level\n");
	basetime += 86400;
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, basetime + 100);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, basetime + 200);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  basetime + 300);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, basetime + 400);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_NORMAL;
	CHECK_CALL_FNUM(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);
	CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_NORMAL);

	CHECK_CALL_PATH(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);
	CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_NORMAL);

	torture_comment(torture, "try to change a file to a directory\n");
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	CHECK_CALL_FNUM(BASIC_INFO, NT_STATUS_INVALID_PARAMETER);

	printf("a zero time means don't change\n");
	unix_to_nt_time(&sfinfo.basic_info.in.create_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.access_time, 0);
	unix_to_nt_time(&sfinfo.basic_info.in.write_time,  0);
	unix_to_nt_time(&sfinfo.basic_info.in.change_time, 0);
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_NORMAL;
	CHECK_CALL_FNUM(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);
	CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_NORMAL);

	CHECK_CALL_PATH(BASIC_INFORMATION, NT_STATUS_OK);
	CHECK_TIME(ALL_INFO, all_info, create_time, basetime + 100);
	CHECK_TIME(ALL_INFO, all_info, access_time, basetime + 200);
	CHECK_TIME(ALL_INFO, all_info, write_time,  basetime + 300);

	/* interesting - w2k3 leaves change_time as current time for 0 change time
	   in setpathinfo
	  CHECK_TIME(ALL_INFO, all_info, change_time, basetime + 400);
	*/
	CHECK_VALUE(ALL_INFO, all_info, attrib,     FILE_ATTRIBUTE_NORMAL);

	printf("Test disposition_info level\n");
	sfinfo.disposition_info.in.delete_on_close = 1;
	CHECK_CALL_FNUM(DISPOSITION_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, delete_pending, 1);
	CHECK_VALUE(ALL_INFO, all_info, nlink, 0);

	sfinfo.disposition_info.in.delete_on_close = 0;
	CHECK_CALL_FNUM(DISPOSITION_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, delete_pending, 0);
	CHECK_VALUE(ALL_INFO, all_info, nlink, 1);

	printf("Test disposition_information level\n");
	sfinfo.disposition_info.in.delete_on_close = 1;
	CHECK_CALL_FNUM(DISPOSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, delete_pending, 1);
	CHECK_VALUE(ALL_INFO, all_info, nlink, 0);

	/* this would delete the file! */
	/*
	  CHECK_CALL_PATH(DISPOSITION_INFORMATION, NT_STATUS_OK);
	  CHECK_VALUE(ALL_INFO, all_info, delete_pending, 1);
	  CHECK_VALUE(ALL_INFO, all_info, nlink, 0);
	*/

	sfinfo.disposition_info.in.delete_on_close = 0;
	CHECK_CALL_FNUM(DISPOSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, delete_pending, 0);
	CHECK_VALUE(ALL_INFO, all_info, nlink, 1);

	CHECK_CALL_PATH(DISPOSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, delete_pending, 0);
	CHECK_VALUE(ALL_INFO, all_info, nlink, 1);

	printf("Test allocation_info level\n");
	sfinfo.allocation_info.in.alloc_size = 0;
	CHECK_CALL_FNUM(ALLOCATION_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 0);

	sfinfo.allocation_info.in.alloc_size = 4096;
	CHECK_CALL_FNUM(ALLOCATION_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 4096);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);

	RECREATE_BOTH;
	sfinfo.allocation_info.in.alloc_size = 0;
	CHECK_CALL_FNUM(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 0);

	CHECK_CALL_PATH(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 0);

	sfinfo.allocation_info.in.alloc_size = 4096;
	CHECK_CALL_FNUM(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 4096);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);

	/* setting the allocation size up via setpathinfo seems
	   to be broken in w2k3 */
	CHECK_CALL_PATH(ALLOCATION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, alloc_size, 0);
	CHECK_VALUE(ALL_INFO, all_info, size, 0);

	printf("Test end_of_file_info level\n");
	sfinfo.end_of_file_info.in.size = 37;
	CHECK_CALL_FNUM(END_OF_FILE_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 37);

	sfinfo.end_of_file_info.in.size = 7;
	CHECK_CALL_FNUM(END_OF_FILE_INFO, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 7);

	sfinfo.end_of_file_info.in.size = 37;
	CHECK_CALL_FNUM(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 37);

	CHECK_CALL_PATH(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 37);

	sfinfo.end_of_file_info.in.size = 7;
	CHECK_CALL_FNUM(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 7);

	CHECK_CALL_PATH(END_OF_FILE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(ALL_INFO, all_info, size, 7);

	printf("Test position_information level\n");
	sfinfo.position_information.in.position = 123456;
	CHECK_CALL_FNUM(POSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(POSITION_INFORMATION, position_information, position, 123456);

	CHECK_CALL_PATH(POSITION_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(POSITION_INFORMATION, position_information, position, 0);

	printf("Test mode_information level\n");
	sfinfo.mode_information.in.mode = 2;
	CHECK_CALL_FNUM(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 2);

	CHECK_CALL_PATH(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 0);

	sfinfo.mode_information.in.mode = 1;
	CHECK_CALL_FNUM(MODE_INFORMATION, NT_STATUS_INVALID_PARAMETER);
	CHECK_CALL_PATH(MODE_INFORMATION, NT_STATUS_INVALID_PARAMETER);

	sfinfo.mode_information.in.mode = 0;
	CHECK_CALL_FNUM(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 0);

	CHECK_CALL_PATH(MODE_INFORMATION, NT_STATUS_OK);
	CHECK_VALUE(MODE_INFORMATION, mode_information, mode, 0);

#if 0
	printf("Test unix_basic level\n");
	CHECK_CALL_FNUM(UNIX_BASIC, NT_STATUS_OK);
	CHECK_CALL_PATH(UNIX_BASIC, NT_STATUS_OK);

	printf("Test unix_link level\n");
	CHECK_CALL_FNUM(UNIX_LINK, NT_STATUS_OK);
	CHECK_CALL_PATH(UNIX_LINK, NT_STATUS_OK);
#endif

done:
	smb_raw_exit(cli->session);
	smbcli_close(cli->tree, fnum);
	if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fnum_fname))) {
		printf("Failed to delete %s - %s\n", fnum_fname, smbcli_errstr(cli->tree));
	}
	if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, path_fname))) {
		printf("Failed to delete %s - %s\n", path_fname, smbcli_errstr(cli->tree));
	}

	return ret;
}

/*
 * basic testing of all RAW_SFILEINFO_RENAME call
 */
static bool
torture_raw_sfileinfo_rename(struct torture_context *torture,
    struct smbcli_state *cli)
{
	bool ret = true;
	int fnum_saved, d_fnum, fnum2, fnum = -1;
	char *fnum_fname;
	char *fnum_fname_new;
	char *path_fname;
	char *path_fname_new;
	char *path_dname;
	char *path_dname_new;
	char *saved_name;
	char *saved_name_new;
	union smb_fileinfo finfo1, finfo2;
	union smb_setfileinfo sfinfo;
	NTSTATUS status, status2;
	const char *call_name;
	bool check_fnum;
	int n = time(NULL) % 100;
	
	asprintf(&path_fname, BASEDIR "\\fname_test_%d.txt", n);
	asprintf(&path_fname_new, BASEDIR "\\fname_test_new_%d.txt", n);
	asprintf(&fnum_fname, BASEDIR "\\fnum_test_%d.txt", n);
	asprintf(&fnum_fname_new, BASEDIR "\\fnum_test_new_%d.txt", n);
	asprintf(&path_dname, BASEDIR "\\dname_test_%d", n);
	asprintf(&path_dname_new, BASEDIR "\\dname_test_new_%d", n);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	RECREATE_BOTH;

	ZERO_STRUCT(sfinfo);

	smbcli_close(cli->tree, create_complex_file(cli, torture, fnum_fname_new));
	smbcli_close(cli->tree, create_complex_file(cli, torture, path_fname_new));

	sfinfo.rename_information.in.overwrite = 0;
	sfinfo.rename_information.in.root_fid  = 0;
	sfinfo.rename_information.in.new_name  = fnum_fname_new+strlen(BASEDIR)+1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OBJECT_NAME_COLLISION);

	sfinfo.rename_information.in.new_name  = path_fname_new+strlen(BASEDIR)+1;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OBJECT_NAME_COLLISION);

	sfinfo.rename_information.in.new_name  = fnum_fname_new;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_NOT_SUPPORTED);

	sfinfo.rename_information.in.new_name  = fnum_fname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname_new);

	printf("Trying rename with dest file open\n");
	fnum2 = create_complex_file(cli, torture, fnum_fname);
	sfinfo.rename_information.in.new_name  = fnum_fname+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_ACCESS_DENIED);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname_new);

	fnum_saved = fnum;
	fnum = fnum2;
	sfinfo.disposition_info.in.delete_on_close = 1;
	CHECK_CALL_FNUM(DISPOSITION_INFO, NT_STATUS_OK);
	fnum = fnum_saved;

	printf("Trying rename with dest file open and delete_on_close\n");
	sfinfo.rename_information.in.new_name  = fnum_fname+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_ACCESS_DENIED);

	smbcli_close(cli->tree, fnum2);
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname);

	printf("Trying rename with source file open twice\n");
	sfinfo.rename_information.in.new_name  = fnum_fname+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname);

	fnum2 = create_complex_file(cli, torture, fnum_fname);
	sfinfo.rename_information.in.new_name  = fnum_fname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname_new);
	smbcli_close(cli->tree, fnum2);

	sfinfo.rename_information.in.new_name  = fnum_fname+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname);

	sfinfo.rename_information.in.new_name  = path_fname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 1;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname_new);

	sfinfo.rename_information.in.new_name  = fnum_fname+strlen(BASEDIR)+1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname);

	sfinfo.rename_information.in.new_name  = path_fname+strlen(BASEDIR)+1;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname);

	printf("Trying rename with a root fid\n");
	status = create_directory_handle(cli->tree, BASEDIR, &d_fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	sfinfo.rename_information.in.new_name  = fnum_fname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.root_fid = d_fnum;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_INVALID_PARAMETER);
	CHECK_STR(NAME_INFO, name_info, fname.s, fnum_fname);
	smbcli_close(cli->tree, d_fnum);

	printf("Trying rename directory\n");
	if (!torture_setup_dir(cli, path_dname)) {
		ret = false;
		goto done;
	}
	saved_name = path_fname;
	saved_name_new = path_fname_new;
	path_fname = path_dname;
	path_fname_new = path_dname_new;
	sfinfo.rename_information.in.new_name  = path_dname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	sfinfo.rename_information.in.root_fid  = 0;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname_new);
	path_fname = saved_name;
	path_fname_new = saved_name_new;

	if (torture_setting_bool(torture, "samba3", false)) {
		printf("SKIP: Trying rename directory with a handle\n");
		printf("SKIP: Trying rename by path while a handle is open\n");
		printf("SKIP: Trying rename directory by path while a handle is open\n");
		goto done;
	}

	printf("Trying rename directory with a handle\n");
	status = create_directory_handle(cli->tree, path_dname_new, &d_fnum);
	fnum_saved = fnum;
	fnum = d_fnum;
	saved_name = fnum_fname;
	saved_name_new = fnum_fname_new;
	fnum_fname = path_dname;
	fnum_fname_new = path_dname_new;
	sfinfo.rename_information.in.new_name  = path_dname+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	sfinfo.rename_information.in.root_fid  = 0;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname);
	smbcli_close(cli->tree, d_fnum);
	fnum = fnum_saved;
	fnum_fname = saved_name;
	fnum_fname_new = saved_name_new;

	printf("Trying rename by path while a handle is open\n");
	fnum_saved = fnum;
	fnum = create_complex_file(cli, torture, path_fname);
	sfinfo.rename_information.in.new_name  = path_fname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	sfinfo.rename_information.in.root_fid  = 0;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname_new);
	/* check that the handle returns the same name */
	check_fnum = true;
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname_new);
	/* rename it back on the handle */
	sfinfo.rename_information.in.new_name  = path_fname+strlen(BASEDIR)+1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname);
	check_fnum = false;
	CHECK_STR(NAME_INFO, name_info, fname.s, path_fname);
	smbcli_close(cli->tree, fnum);
	fnum = fnum_saved;

	printf("Trying rename directory by path while a handle is open\n");
	status = create_directory_handle(cli->tree, path_dname, &d_fnum);
	fnum_saved = fnum;
	fnum = d_fnum;
	saved_name = path_fname;
	saved_name_new = path_fname_new;
	path_fname = path_dname;
	path_fname_new = path_dname_new;
	sfinfo.rename_information.in.new_name  = path_dname_new+strlen(BASEDIR)+1;
	sfinfo.rename_information.in.overwrite = 0;
	sfinfo.rename_information.in.root_fid  = 0;
	CHECK_CALL_PATH(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname_new);
	path_fname = saved_name;
	path_fname_new = saved_name_new;
	saved_name = fnum_fname;
	saved_name_new = fnum_fname_new;
	fnum_fname = path_dname;
	fnum_fname_new = path_dname_new;
	/* check that the handle returns the same name */
	check_fnum = true;
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname_new);
	/* rename it back on the handle */
	sfinfo.rename_information.in.new_name  = path_dname+strlen(BASEDIR)+1;
	CHECK_CALL_FNUM(RENAME_INFORMATION, NT_STATUS_OK);
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname);
	fnum_fname = saved_name;
	fnum_fname_new = saved_name_new;
	saved_name = path_fname;
	saved_name_new = path_fname_new;
	path_fname = path_dname;
	path_fname_new = path_dname_new;
	check_fnum = false;
	CHECK_STR(NAME_INFO, name_info, fname.s, path_dname);
	smbcli_close(cli->tree, d_fnum);
	fnum = fnum_saved;
	path_fname = saved_name;
	path_fname_new = saved_name_new;

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/* 
   look for the w2k3 setpathinfo STANDARD bug
*/
static bool torture_raw_sfileinfo_bug(struct torture_context *torture,
    struct smbcli_state *cli)
{
	const char *fname = "\\bug3.txt";
	union smb_setfileinfo sfinfo;
	NTSTATUS status;
	int fnum;

	if (!torture_setting_bool(torture, "dangerous", false))
		torture_skip(torture, 
			"torture_raw_sfileinfo_bug disabled - enable dangerous tests to use\n");

	fnum = create_complex_file(cli, torture, fname);
	smbcli_close(cli->tree, fnum);

	sfinfo.generic.level = RAW_SFILEINFO_STANDARD;
	sfinfo.generic.in.file.path = fname;

	sfinfo.standard.in.create_time = 0;
	sfinfo.standard.in.access_time = 0;
	sfinfo.standard.in.write_time  = 0;

	status = smb_raw_setpathinfo(cli->tree, &sfinfo);
	printf("%s - %s\n", fname, nt_errstr(status));

	printf("now try and delete %s\n", fname);

	return true;
}

/**
 * Test both the snia cifs RAW_SFILEINFO_END_OF_FILE_INFO and the undocumented
 * pass-through RAW_SFILEINFO_END_OF_FILE_INFORMATION in the context of
 * trans2setpathinfo.
 */
static bool
torture_raw_sfileinfo_eof(struct torture_context *tctx,
    struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_sfileinfo_end_of_file.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	union smb_fileinfo qfi;
	uint16_t fnum = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.flags = 0;

	/* Open the file sharing none. */
	status = smb_raw_open(cli1->tree, tctx, &io);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");
	fnum = io.ntcreatex.out.file.fnum;

	/* Try to sfileinfo to extend the file. */
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFO;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;
	status = smb_raw_setpathinfo(cli2->tree, &sfi);

	/* There should be share mode contention in this case. */
	torture_assert_ntstatus_equal_goto(tctx, status,
	    NT_STATUS_SHARING_VIOLATION, ret, done, "Status should be "
	    "SHARING_VIOLATION");

	/* Make sure the size is still 0. */
	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_STANDARD_INFO;
	qfi.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli2->tree, tctx, &qfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");

	torture_assert_u64_equal(tctx, qfi.standard_info.out.size, 0,
	    "alloc_size should be 0 since the setpathinfo failed.");

	/* Try again with the pass through instead of documented version. */
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;
	status = smb_raw_setpathinfo(cli2->tree, &sfi);

	/*
	 * Looks like a windows bug:
	 * http://lists.samba.org/archive/cifs-protocol/2009-November/001130.html
	 */
	if (TARGET_IS_W2K8(tctx) || TARGET_IS_WIN7(tctx)) {
		/* It succeeds! This is just weird! */
		torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
		    ret, done, "Status should be OK");

		/* Verify that the file was actually extended to 100. */
		ZERO_STRUCT(qfi);
		qfi.generic.level = RAW_FILEINFO_STANDARD_INFO;
		qfi.generic.in.file.path = fname;
		status = smb_raw_pathinfo(cli2->tree, tctx, &qfi);
		torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
		    ret, done, "Status should be OK");

		torture_assert_u64_equal(tctx, qfi.standard_info.out.size, 100,
		    "alloc_size should be 100 since the setpathinfo "
		    "succeeded.");
	} else {
		torture_assert_ntstatus_equal_goto(tctx, status,
		    NT_STATUS_SHARING_VIOLATION, ret, done, "Status should be "
		    "SHARING_VIOLATION");
	}

	/* close the first file. */
	smbcli_close(cli1->tree, fnum);
	fnum = 0;

	/* Try to sfileinfo to extend the file again (non-pass-through). */
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFO;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 200;
	status = smb_raw_setpathinfo(cli2->tree, &sfi);

	/* This should cause the client to retun invalid level. */
	if (TARGET_IS_W2K8(tctx) || TARGET_IS_WIN7(tctx)) {
		/*
		 * Windows sends back an invalid packet that smbclient sees
		 * and returns INTERNAL_ERROR.
		 */
		torture_assert_ntstatus_equal_goto(tctx, status,
		    NT_STATUS_INTERNAL_ERROR, ret, done, "Status should be "
		    "INTERNAL_ERROR");
	} else {
		torture_assert_ntstatus_equal_goto(tctx, status,
		    NT_STATUS_INVALID_LEVEL, ret, done, "Status should be "
		    "INVALID_LEVEL");
	}

	/* Try to extend the file now with the passthrough level. */
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	status = smb_raw_setpathinfo(cli2->tree, &sfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");

	/* Verify that the file was actually extended to 200. */
	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_STANDARD_INFO;
	qfi.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli2->tree, tctx, &qfi);

	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");
	torture_assert_u64_equal(tctx, qfi.standard_info.out.size, 200,
	    "alloc_size should be 200 since the setpathinfo succeeded.");

	/* Open the file so end of file can be set by handle. */
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_WRITE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");
	fnum = io.ntcreatex.out.file.fnum;

	/* Try sfileinfo to extend the file by handle (non-pass-through). */
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFO;
	sfi.generic.in.file.fnum = fnum;
	sfi.end_of_file_info.in.size = 300;
	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");

	/* Verify that the file was actually extended to 300. */
	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_STANDARD_INFO;
	qfi.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli1->tree, tctx, &qfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");
	torture_assert_u64_equal(tctx, qfi.standard_info.out.size, 300,
	    "alloc_size should be 300 since the setpathinfo succeeded.");

	/* Try sfileinfo to extend the file by handle (pass-through). */
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.fnum = fnum;
	sfi.end_of_file_info.in.size = 400;
	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");

	/* Verify that the file was actually extended to 300. */
	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_STANDARD_INFO;
	qfi.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli1->tree, tctx, &qfi);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK, ret,
	    done, "Status should be OK");
	torture_assert_u64_equal(tctx, qfi.standard_info.out.size, 400,
	    "alloc_size should be 400 since the setpathinfo succeeded.");
 done:
	if (fnum > 0) {
		smbcli_close(cli1->tree, fnum);
		fnum = 0;
	}

	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool
torture_raw_sfileinfo_eof_access(struct torture_context *tctx,
    struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive3.dat";
	NTSTATUS status, expected_status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;
	uint32_t access_mask = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	/*
	 * base ntcreatex parms
	 */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.flags = 0;


	for (access_mask = 1; access_mask <= 0x00001FF; access_mask++) {
		io.ntcreatex.in.access_mask = access_mask;

		status = smb_raw_open(cli1->tree, tctx, &io);
		if (!NT_STATUS_IS_OK(status)) {
			continue;
		}

		fnum = io.ntcreatex.out.file.fnum;

		ZERO_STRUCT(sfi);
		sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFO;
		sfi.generic.in.file.fnum = fnum;
		sfi.end_of_file_info.in.size = 100;

		status = smb_raw_setfileinfo(cli1->tree, &sfi);

		expected_status = (access_mask & SEC_FILE_WRITE_DATA) ?
		    NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;

		if (!NT_STATUS_EQUAL(expected_status, status)) {
			torture_comment(tctx, "0x%x wrong\n", access_mask);
		}

		torture_assert_ntstatus_equal_goto(tctx, status,
		    expected_status, ret, done, "Status Wrong");

		smbcli_close(cli1->tree, fnum);
	}

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool
torture_raw_sfileinfo_archive(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	const char *fname = BASEDIR "\\test_archive.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfinfo;
	union smb_fileinfo finfo;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli->tree, fname);

	/*
	 * create a normal file, verify archive bit
	 */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.flags = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "open failed");
	fnum = io.ntcreatex.out.file.fnum;

	torture_assert_int_equal(tctx,
	    io.ntcreatex.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED,
	    FILE_ATTRIBUTE_ARCHIVE,
	    "archive bit not set");

	/*
	 * try to turn off archive bit
	 */
	ZERO_STRUCT(sfinfo);
	sfinfo.generic.level = RAW_SFILEINFO_BASIC_INFO;
	sfinfo.generic.in.file.fnum = fnum;
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_NORMAL;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "setfileinfo failed");

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, tctx, &finfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "fileinfo failed");

	torture_assert_int_equal(tctx,
	    finfo.all_info.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED,
	    FILE_ATTRIBUTE_NORMAL,
	    "archive bit set");

	status = smbcli_close(cli->tree, fnum);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "close failed");

	status = smbcli_unlink(cli->tree, fname);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "unlink failed");

	/*
	 * create a directory, verify no archive bit
	 */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_DIR_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.flags = 0;
	status = smb_raw_open(cli->tree, tctx, &io);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "directory open failed");
	fnum = io.ntcreatex.out.file.fnum;

	torture_assert_int_equal(tctx,
	    io.ntcreatex.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED,
	    FILE_ATTRIBUTE_DIRECTORY,
	    "archive bit set");

	/*
	 * verify you can turn on archive bit
	 */
	sfinfo.generic.level = RAW_SFILEINFO_BASIC_INFO;
	sfinfo.generic.in.file.fnum = fnum;
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ARCHIVE;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "setfileinfo failed");

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, tctx, &finfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "fileinfo failed");

	torture_assert_int_equal(tctx,
	    finfo.all_info.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED,
	    FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ARCHIVE,
	    "archive bit not set");

	/*
	 * and try to turn it back off
	 */
	sfinfo.generic.level = RAW_SFILEINFO_BASIC_INFO;
	sfinfo.generic.in.file.fnum = fnum;
	sfinfo.basic_info.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "setfileinfo failed");

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, tctx, &finfo);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "fileinfo failed");

	torture_assert_int_equal(tctx,
	    finfo.all_info.out.attrib & ~FILE_ATTRIBUTE_NONINDEXED,
	    FILE_ATTRIBUTE_DIRECTORY,
	    "archive bit set");

	status = smbcli_close(cli->tree, fnum);
	torture_assert_ntstatus_equal_goto(tctx, status, NT_STATUS_OK,
	    ret, done, "close failed");

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

struct torture_suite *torture_raw_sfileinfo(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "sfileinfo");

	torture_suite_add_1smb_test(suite, "base", torture_raw_sfileinfo_base);
	torture_suite_add_1smb_test(suite, "rename", torture_raw_sfileinfo_rename);
	torture_suite_add_1smb_test(suite, "bug", torture_raw_sfileinfo_bug);
	torture_suite_add_2smb_test(suite, "end-of-file",
	    torture_raw_sfileinfo_eof);
	torture_suite_add_2smb_test(suite, "end-of-file-access",
	    torture_raw_sfileinfo_eof_access);
	torture_suite_add_1smb_test(suite, "archive",
								torture_raw_sfileinfo_archive);

	return suite;
}
