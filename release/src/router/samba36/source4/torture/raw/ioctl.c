/* 
   Unix SMB/CIFS implementation.
   ioctl individual test suite
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   
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
#include "libcli/raw/ioctl.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\rawioctl"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%d) Incorrect status %s - should be %s\n", \
		       __LINE__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)


/* test some ioctls */
static bool test_ioctl(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_ioctl ctl;
	int fnum;
	NTSTATUS status;
	bool ret = true;
	const char *fname = BASEDIR "\\test.dat";

	printf("TESTING IOCTL FUNCTIONS\n");

	fnum = create_complex_file(cli, mem_ctx, fname);
	if (fnum == -1) {
		printf("Failed to create test.dat - %s\n", smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

 	printf("Trying 0xFFFF\n");
 	ctl.ioctl.level = RAW_IOCTL_IOCTL;
	ctl.ioctl.in.file.fnum = fnum;
	ctl.ioctl.in.request = 0xFFFF;

	status = smb_raw_ioctl(cli->tree, mem_ctx, &ctl);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRSRV, ERRerror));

 	printf("Trying QUERY_JOB_INFO\n");
 	ctl.ioctl.level = RAW_IOCTL_IOCTL;
	ctl.ioctl.in.file.fnum = fnum;
	ctl.ioctl.in.request = IOCTL_QUERY_JOB_INFO;

	status = smb_raw_ioctl(cli->tree, mem_ctx, &ctl);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRSRV, ERRerror));

 	printf("Trying bad handle\n");
	ctl.ioctl.in.file.fnum = fnum+1;
	status = smb_raw_ioctl(cli->tree, mem_ctx, &ctl);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRSRV, ERRerror));

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/* test some filesystem control functions */
static bool test_fsctl(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	int fnum;
	NTSTATUS status;
	bool ret = true;
	const char *fname = BASEDIR "\\test.dat";
	union smb_ioctl nt;

	printf("\nTESTING FSCTL FUNCTIONS\n");

	fnum = create_complex_file(cli, mem_ctx, fname);
	if (fnum == -1) {
		printf("Failed to create test.dat - %s\n", smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("Trying FSCTL_FIND_FILES_BY_SID\n");
	nt.ioctl.level = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function = FSCTL_FIND_FILES_BY_SID;
	nt.ntioctl.in.file.fnum = fnum;
	nt.ntioctl.in.fsctl = true;
	nt.ntioctl.in.filter = 0;
	nt.ntioctl.in.max_data = 0;
	nt.ntioctl.in.blob = data_blob(NULL, 1024);
	/* definitely not a sid... */
	generate_random_buffer(nt.ntioctl.in.blob.data,
			       nt.ntioctl.in.blob.length);
	nt.ntioctl.in.blob.data[1] = 15+1;
	status = smb_raw_ioctl(cli->tree, mem_ctx, &nt);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
		printf("Got unexpected error code: %s\n",
			nt_errstr(status));
		ret = false;
		goto done;
	}

	printf("trying sparse file\n");
	nt.ioctl.level = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function = FSCTL_SET_SPARSE;
	nt.ntioctl.in.file.fnum = fnum;
	nt.ntioctl.in.fsctl = true;
	nt.ntioctl.in.filter = 0;
	nt.ntioctl.in.max_data = 0;
	nt.ntioctl.in.blob = data_blob(NULL, 0);

	status = smb_raw_ioctl(cli->tree, mem_ctx, &nt);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("trying batch oplock\n");
	nt.ioctl.level = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function = FSCTL_REQUEST_BATCH_OPLOCK;
	nt.ntioctl.in.file.fnum = fnum;
	nt.ntioctl.in.fsctl = true;
	nt.ntioctl.in.filter = 0;
	nt.ntioctl.in.max_data = 0;
	nt.ntioctl.in.blob = data_blob(NULL, 0);

	status = smb_raw_ioctl(cli->tree, mem_ctx, &nt);
	if (NT_STATUS_IS_OK(status)) {
		printf("Server supports batch oplock upgrades on open files\n");
	} else {
		printf("Server does not support batch oplock upgrades on open files\n");
	}

 	printf("Trying bad handle\n");
	nt.ntioctl.in.file.fnum = fnum+1;
	status = smb_raw_ioctl(cli->tree, mem_ctx, &nt);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

#if 0
	nt.ntioctl.in.file.fnum = fnum;
	for (i=0;i<100;i++) {
		nt.ntioctl.in.function = FSCTL_FILESYSTEM + (i<<2);
		status = smb_raw_ioctl(cli->tree, mem_ctx, &nt);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
			printf("filesystem fsctl 0x%x - %s\n",
			       i, nt_errstr(status));
		}
	}
#endif

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/* 
   basic testing of some ioctl calls 
*/
bool torture_raw_ioctl(struct torture_context *torture, 
		       struct smbcli_state *cli)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_ioctl(cli, torture);
	ret &= test_fsctl(cli, torture);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
