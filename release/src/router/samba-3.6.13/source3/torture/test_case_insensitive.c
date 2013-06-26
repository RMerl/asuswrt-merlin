/*
   Unix SMB/CIFS implementation.
   reproducer for bug 8042
   Copyright (C) Volker Lendecke 2011

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
#include "torture/proto.h"
#include "system/filesys.h"
#include "libsmb/libsmb.h"

/*
 * Regression test file creates on case insensitive file systems (e.g. OS/X)
 * https://bugzilla.samba.org/show_bug.cgi?id=8042
 */

bool run_case_insensitive_create(int dummy)
{
	struct cli_state *cli;
	uint16_t fnum;
	NTSTATUS status;

	printf("Starting case_insensitive_create\n");

	if (!torture_open_connection(&cli, 0)) {
		return false;
	}

	status = cli_mkdir(cli, "x");
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_mkdir failed: %s\n", nt_errstr(status));
		goto done;
	}
	status = cli_chkpath(cli, "X");
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_chkpath failed: %s\n", nt_errstr(status));
		goto rmdir;
	}
	status = cli_open(cli, "x\\y", O_RDWR|O_CREAT, 0, &fnum);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_open failed: %s\n", nt_errstr(status));

		if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_IS_A_DIRECTORY)) {
			printf("Bug 8042 reappeared!!\n");
		}
		goto unlink;
	}
	status = cli_close(cli, fnum);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_close failed: %s\n", nt_errstr(status));
		goto done;
	}
unlink:
	status = cli_unlink(cli, "x\\y", 0);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_unlink failed: %s\n", nt_errstr(status));
		goto done;
	}
rmdir:
	status = cli_rmdir(cli, "x");
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_close failed: %s\n", nt_errstr(status));
	}
done:
	torture_close_connection(cli);
	return NT_STATUS_IS_OK(status);
}
