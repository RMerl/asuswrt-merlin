/* 
   Unix SMB/CIFS implementation.

   openattr tester

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
#include "system/filesys.h"
#include "libcli/security/secace.h"

extern int torture_failures;

#define CHECK_MAX_FAILURES(label) do { if (++failures >= torture_failures) goto label; } while (0)


static const uint32_t open_attrs_table[] = {
		FILE_ATTRIBUTE_NORMAL,
		FILE_ATTRIBUTE_ARCHIVE,
		FILE_ATTRIBUTE_READONLY,
		FILE_ATTRIBUTE_HIDDEN,
		FILE_ATTRIBUTE_SYSTEM,

		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY,
		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN,
		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM,
		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN,
		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM,
		FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM,

		FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN,
		FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM,
		FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM,
		FILE_ATTRIBUTE_HIDDEN,FILE_ATTRIBUTE_SYSTEM,
};

struct trunc_open_results {
	unsigned int num;
	uint32_t init_attr;
	uint32_t trunc_attr;
	uint32_t result_attr;
};

static const struct trunc_open_results attr_results[] = {
	{ 0, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE },
	{ 1, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE },
	{ 2, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY },
	{ 16, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE },
	{ 17, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE },
	{ 18, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY },
	{ 51, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 54, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 56, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN },
	{ 68, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 71, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 73, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM },
	{ 99, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_HIDDEN,FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 102, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 104, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN },
	{ 116, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 119,  FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM,  FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 121, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM },
	{ 170, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN },
	{ 173, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM },
	{ 227, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 230, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN },
	{ 232, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN },
	{ 244, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 247, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM },
	{ 249, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM }
};


bool torture_openattrtest(struct torture_context *tctx, 
			  struct smbcli_state *cli1)
{
	const char *fname = "\\openattr.file";
	int fnum1;
	uint16_t attr;
	unsigned int i, j, k, l;
	int failures = 0;

	for (k = 0, i = 0; i < sizeof(open_attrs_table)/sizeof(uint32_t); i++) {
		smbcli_setatr(cli1->tree, fname, 0, 0);
		smbcli_unlink(cli1->tree, fname);
		fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
					      SEC_FILE_WRITE_DATA, 
					      open_attrs_table[i],
					      NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
		
		torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open %d (1) of %s failed (%s)", i, 
					   fname, smbcli_errstr(cli1->tree)));

		torture_assert_ntstatus_ok(tctx, 
							smbcli_close(cli1->tree, fnum1),
							talloc_asprintf(tctx, "close %d (1) of %s failed (%s)", i, fname, 
							smbcli_errstr(cli1->tree)));

		for (j = 0; j < ARRAY_SIZE(open_attrs_table); j++) {
			fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
						      SEC_FILE_READ_DATA|
						      SEC_FILE_WRITE_DATA, 
						      open_attrs_table[j],
						      NTCREATEX_SHARE_ACCESS_NONE, 
						      NTCREATEX_DISP_OVERWRITE, 0, 0);

			if (fnum1 == -1) {
				for (l = 0; l < ARRAY_SIZE(attr_results); l++) {
					if (attr_results[l].num == k) {
						torture_result(tctx, TORTURE_FAIL,
								"[%d] trunc open 0x%x -> 0x%x of %s failed - should have succeeded !(%s)",
								k, open_attrs_table[i],
								open_attrs_table[j],
								fname, smbcli_errstr(cli1->tree));
						CHECK_MAX_FAILURES(error_exit);
					}
				}
				torture_assert_ntstatus_equal(tctx, 
					smbcli_nt_error(cli1->tree), NT_STATUS_ACCESS_DENIED, 
					talloc_asprintf(tctx, "[%d] trunc open 0x%x -> 0x%x failed with wrong error code %s",
							k, open_attrs_table[i], open_attrs_table[j],
							smbcli_errstr(cli1->tree)));
					CHECK_MAX_FAILURES(error_exit);
#if 0
				torture_comment(tctx, "[%d] trunc open 0x%x -> 0x%x failed\n", k, open_attrs_table[i], open_attrs_table[j]);
#endif
				k++;
				continue;
			}

			torture_assert_ntstatus_ok(tctx, 
									   smbcli_close(cli1->tree, fnum1),
									talloc_asprintf(tctx, "close %d (2) of %s failed (%s)", j, 
									fname, smbcli_errstr(cli1->tree)));

			torture_assert_ntstatus_ok(tctx, 
						smbcli_getatr(cli1->tree, fname, &attr, NULL, NULL),
						talloc_asprintf(tctx, "getatr(2) failed (%s)", smbcli_errstr(cli1->tree)));

#if 0
			torture_comment(tctx, "[%d] getatr check [0x%x] trunc [0x%x] got attr 0x%x\n",
					k,  open_attrs_table[i],  open_attrs_table[j], attr );
#endif

			for (l = 0; l < ARRAY_SIZE(attr_results); l++) {
				if (attr_results[l].num == k) {
					if (attr != attr_results[l].result_attr ||
					    open_attrs_table[i] != attr_results[l].init_attr ||
					    open_attrs_table[j] != attr_results[l].trunc_attr) {
						torture_result(tctx, TORTURE_FAIL,
							"[%d] getatr check failed. [0x%x] trunc [0x%x] got attr 0x%x, should be 0x%x",
						       k, open_attrs_table[i],
						       open_attrs_table[j],
						       (unsigned int)attr,
						       attr_results[l].result_attr);
						CHECK_MAX_FAILURES(error_exit);
					}
					break;
				}
			}
			k++;
		}
	}
error_exit:
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);

	return true;
}

bool torture_winattrtest(struct torture_context *tctx,
			  struct smbcli_state *cli1)
{
	const char *fname = "\\winattr1.file";
	const char *dname = "\\winattr1.dir";
	int fnum1;
	uint16_t attr;
	uint16_t j;
	uint32_t aceno;
	int failures = 0;
	union smb_fileinfo query, query_org;
	NTSTATUS status;
	struct security_descriptor *sd1, *sd2;


	/* Test winattrs for file */
	smbcli_unlink(cli1->tree, fname);

	/* Open a file*/
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR | O_CREAT | O_TRUNC,
			DENY_NONE);
	torture_assert(tctx, fnum1 != -1,
		       talloc_asprintf(tctx, "open(1) of %s failed (%s)\n",
		       fname, smbcli_errstr(cli1->tree)));


	/* Get security descriptor and store it*/
	query_org.generic.level = RAW_FILEINFO_SEC_DESC;
	query_org.generic.in.file.fnum = fnum1;
	status = smb_raw_fileinfo(cli1->tree, tctx, &query_org);
	if(!NT_STATUS_IS_OK(status)){
		torture_comment(tctx, "smb_raw_fileinfo(1) of %s failed (%s)\n",
				fname, nt_errstr(status));
		torture_assert_ntstatus_ok(tctx,
				smbcli_close(cli1->tree, fnum1),
				talloc_asprintf(tctx,
					"close(1) of %s failed (%s)\n",
			                fname, smbcli_errstr(cli1->tree)));
		CHECK_MAX_FAILURES(error_exit_file);
	}
	sd1 = query_org.query_secdesc.out.sd;

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
	               talloc_asprintf(tctx, "close(1) of %s failed (%s)\n",
			               fname, smbcli_errstr(cli1->tree)));

	/*Set and get attributes*/
	for (j = 0; j < ARRAY_SIZE(open_attrs_table); j++) {
		torture_assert_ntstatus_ok(tctx,
			smbcli_setatr(cli1->tree, fname, open_attrs_table[j],0),
			talloc_asprintf(tctx, "setatr(2) failed (%s)",
				smbcli_errstr(cli1->tree)));

		torture_assert_ntstatus_ok(tctx,
			smbcli_getatr(cli1->tree, fname, &attr, NULL, NULL),
			talloc_asprintf(tctx, "getatr(2) failed (%s)",
			smbcli_errstr(cli1->tree)));

		/* Check the result */
		if((j == 0)&&(attr != FILE_ATTRIBUTE_ARCHIVE)){
			torture_comment(tctx, "getatr check failed. \
					Attr applied [0x%x], got attr [0x%x], \
					should be [0x%x]",
					open_attrs_table[j],
					(uint16_t)attr,open_attrs_table[j +1]);
			CHECK_MAX_FAILURES(error_exit_file);
		}else{

			if((j != 0) &&(attr != open_attrs_table[j])){
				torture_comment(tctx, "getatr check failed. \
					Attr applied [0x%x],got attr 0x%x, \
					should be 0x%x ",
					open_attrs_table[j], (uint16_t)attr,
					open_attrs_table[j]);
				CHECK_MAX_FAILURES(error_exit_file);
			}

		}

		fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY | O_CREAT,
				DENY_NONE);
		torture_assert(tctx, fnum1 != -1,
		       talloc_asprintf(tctx, "open(2) of %s failed (%s)\n",
		       fname, smbcli_errstr(cli1->tree)));
		/*Get security descriptor */
		query.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
		query.query_secdesc.in.file.fnum = fnum1;
		status = smb_raw_fileinfo(cli1->tree, tctx, &query);
		if(!NT_STATUS_IS_OK(status)){
			torture_comment(tctx,
				"smb_raw_fileinfo(2) of %s failed (%s)\n",
				fname, nt_errstr(status));
			torture_assert_ntstatus_ok(tctx,
				smbcli_close(cli1->tree, fnum1),
				talloc_asprintf(tctx,
					"close(2) of %s failed (%s)\n",
					fname, smbcli_errstr(cli1->tree)));
			CHECK_MAX_FAILURES(error_exit_file);
		}
		sd2 = query.query_secdesc.out.sd;

		torture_assert_ntstatus_ok(tctx,smbcli_close(cli1->tree,fnum1),
	               talloc_asprintf(tctx, "close(2) of %s failed (%s)\n",
			               fname, smbcli_errstr(cli1->tree)));

		/*Compare security descriptors -- Must be same*/
		for (aceno=0;(sd1->dacl&&aceno < sd1->dacl->num_aces);aceno++){
			struct security_ace *ace1 = &sd1->dacl->aces[aceno];
			struct security_ace *ace2 = &sd2->dacl->aces[aceno];

			if(!sec_ace_equal(ace1,ace2)){
				torture_comment(tctx,
					"ACLs changed! Not expected!\n");
				CHECK_MAX_FAILURES(error_exit_file);
			}
		}

		torture_comment(tctx, "[%d] setattr = [0x%x] got attr 0x%x\n",
			j,  open_attrs_table[j], attr );

	}

error_exit_file:
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);

/* Check for Directory. */

	smbcli_deltree(cli1->tree, dname);
	smbcli_rmdir(cli1->tree,dname);

	/* Open a directory */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_RIGHTS_DIR_ALL,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_NONE,
				      NTCREATEX_DISP_OPEN_IF,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	/*smbcli_mkdir(cli1->tree,dname);*/

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx,
			"open (1) of %s failed (%s)",
			  dname, smbcli_errstr(cli1->tree)));


	/* Get Security Descriptor */
	query_org.generic.level = RAW_FILEINFO_SEC_DESC;
        query_org.generic.in.file.fnum = fnum1;
        status = smb_raw_fileinfo(cli1->tree, tctx, &query_org);
	if(!NT_STATUS_IS_OK(status)){
		torture_comment(tctx, "smb_raw_fileinfo(1) of %s failed (%s)\n",
				dname, nt_errstr(status));
		torture_assert_ntstatus_ok(tctx,
				smbcli_close(cli1->tree, fnum1),
				talloc_asprintf(tctx,
					"close(1) of %s failed (%s)\n",
			                dname, smbcli_errstr(cli1->tree)));
		CHECK_MAX_FAILURES(error_exit_dir);
	}
	sd1 = query_org.query_secdesc.out.sd;

	torture_assert_ntstatus_ok(tctx,
				smbcli_close(cli1->tree, fnum1),
				talloc_asprintf(tctx,
				"close (1) of %s failed (%s)", dname,
				smbcli_errstr(cli1->tree)));

	/* Set and get win attributes*/
	for (j = 1; j < ARRAY_SIZE(open_attrs_table); j++) {

		torture_assert_ntstatus_ok(tctx,
		smbcli_setatr(cli1->tree, dname, open_attrs_table[j], 0),
		talloc_asprintf(tctx, "setatr(2) failed (%s)",
		smbcli_errstr(cli1->tree)));

		torture_assert_ntstatus_ok(tctx,
		smbcli_getatr(cli1->tree, dname, &attr, NULL, NULL),
		talloc_asprintf(tctx, "getatr(2) failed (%s)",
		smbcli_errstr(cli1->tree)));

		torture_comment(tctx, "[%d] setatt = [0x%x] got attr 0x%x\n",
			j,  open_attrs_table[j], attr );

		/* Check the result */
		if(attr != (open_attrs_table[j]|FILE_ATTRIBUTE_DIRECTORY)){
			torture_comment(tctx, "getatr check failed. set attr \
				[0x%x], got attr 0x%x, should be 0x%x\n",
			        open_attrs_table[j],
			        (uint16_t)attr,
			        open_attrs_table[j]|FILE_ATTRIBUTE_DIRECTORY);
			CHECK_MAX_FAILURES(error_exit_dir);
		}

		fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_RIGHTS_DIR_READ,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_NONE,
				      NTCREATEX_DISP_OPEN,
				      0,0);

		torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx,
			"open (2) of %s failed (%s)",
			  dname, smbcli_errstr(cli1->tree)));
		/* Get security descriptor */
		query.generic.level = RAW_FILEINFO_SEC_DESC;
		query.generic.in.file.fnum = fnum1;
		status = smb_raw_fileinfo(cli1->tree, tctx, &query);
		if(!NT_STATUS_IS_OK(status)){
			torture_comment(tctx, "smb_raw_fileinfo(2) of %s failed\
					(%s)\n", dname, nt_errstr(status));
			torture_assert_ntstatus_ok(tctx,
					smbcli_close(cli1->tree, fnum1),
					talloc_asprintf(tctx,
					"close (2) of %s failed (%s)", dname,
					smbcli_errstr(cli1->tree)));
			CHECK_MAX_FAILURES(error_exit_dir);
		}
		sd2 = query.query_secdesc.out.sd;
		torture_assert_ntstatus_ok(tctx,
				smbcli_close(cli1->tree, fnum1),
				talloc_asprintf(tctx,
				"close (2) of %s failed (%s)", dname,
				smbcli_errstr(cli1->tree)));

		/* Security descriptor must be same*/
		for (aceno=0;(sd1->dacl&&aceno < sd1->dacl->num_aces);aceno++){
			struct security_ace *ace1 = &sd1->dacl->aces[aceno];
			struct security_ace *ace2 = &sd2->dacl->aces[aceno];

			if(!sec_ace_equal(ace1,ace2)){
				torture_comment(tctx,
					"ACLs changed! Not expected!\n");
				CHECK_MAX_FAILURES(error_exit_dir);
			}
		}

	}
error_exit_dir:
	smbcli_deltree(cli1->tree, dname);
	smbcli_rmdir(cli1->tree,dname);

	if(failures)
		return false;
	return true;
}
