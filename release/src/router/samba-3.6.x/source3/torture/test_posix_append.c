/*
   Unix SMB/CIFS implementation.
   reproducer for bug 6898
   Copyright (C) Volker Lendecke 2009

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
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"

/*
 * Make sure that GENERIC_WRITE does not trigger append. See
 * https://bugzilla.samba.org/show_bug.cgi?id=6898
 */

bool run_posix_append(int dummy)
{
	struct cli_state *cli;
	const char *fname = "append";
	NTSTATUS status;
	uint16_t fnum;
	SMB_OFF_T size;
	uint8_t c = '\0';
	bool ret = false;

	printf("Starting POSIX_APPEND\n");

	if (!torture_open_connection(&cli, 0)) {
		return false;
	}

	status = torture_setup_unix_extensions(cli);
	if (!NT_STATUS_IS_OK(status)) {
		printf("torture_setup_unix_extensions failed: %s\n",
		       nt_errstr(status));
		goto fail;
	}

	status = cli_ntcreate(
		cli, fname, 0,
		GENERIC_WRITE_ACCESS|GENERIC_READ_ACCESS|DELETE_ACCESS,
		FILE_ATTRIBUTE_NORMAL|FILE_FLAG_POSIX_SEMANTICS,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE|FILE_DELETE_ON_CLOSE,
		0, &fnum);

	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_ntcreate failed: %s\n", nt_errstr(status));
		goto fail;
	}

	/*
	 * Write two bytes at offset 0. With bug 6898 we would end up
	 * with a file of 2 byte length.
	 */

	status = cli_writeall(cli, fnum, 0, &c, 0, sizeof(c), NULL);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_write failed: %s\n", nt_errstr(status));
		goto fail;
	}
	status = cli_writeall(cli, fnum, 0, &c, 0, sizeof(c), NULL);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_write failed: %s\n", nt_errstr(status));
		goto fail;
	}

	status = cli_getattrE(cli, fnum, NULL, &size, NULL, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		printf("cli_getatrE failed: %s\n", nt_errstr(status));
		goto fail;
	}

	if (size != sizeof(c)) {
		printf("BUG: Writing with O_APPEND!!\n");
		goto fail;
	}

	ret = true;
fail:
	torture_close_connection(cli);
	return ret;
}
