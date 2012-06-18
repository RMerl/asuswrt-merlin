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
#include "torture/torture.h"
#include "libcli/libcli.h"
#include "torture/util.h"

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
	uint_t num;
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
	uint_t i, j, k, l;
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
						torture_comment(tctx, "[%d] trunc open 0x%x -> 0x%x of %s failed - should have succeeded !(%s)\n",
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
						torture_comment(tctx, "[%d] getatr check failed. [0x%x] trunc [0x%x] got attr 0x%x, should be 0x%x\n",
						       k, open_attrs_table[i],
						       open_attrs_table[j],
						       (uint_t)attr,
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

