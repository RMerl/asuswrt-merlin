/*
   Unix SMB/CIFS implementation.
   test suite for SMB printing operations

   Copyright (C) Guenther Deschner 2010

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
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "system/filesys.h"

#include "torture/smbtorture.h"
#include "torture/util.h"
#include "libcli/rap/rap.h"
#include "torture/rap/proto.h"
#include "param/param.h"

/* TODO:

 printing half the file,
 finding job
 delete job
 try writing 2nd half

 SMBsplretq

*/

#define TORTURE_PRINT_FILE "torture_print_file"

static bool print_printjob(struct torture_context *tctx,
			   struct smbcli_tree *tree)
{
	int fnum;
	DATA_BLOB data;
	ssize_t size_written;
	const char *str;

	torture_comment(tctx, "creating printjob %s\n", TORTURE_PRINT_FILE);

	fnum = smbcli_open(tree, TORTURE_PRINT_FILE, O_RDWR|O_CREAT|O_TRUNC, DENY_NONE);
	if (fnum == -1) {
		torture_fail(tctx, "failed to open file");
	}

	str = talloc_asprintf(tctx, "TortureTestPage: %d\nData\n",0);

	data = data_blob_string_const(str);

	size_written = smbcli_write(tree, fnum, 0, data.data, 0, data.length);
	if (size_written != data.length) {
		torture_fail(tctx, "failed to write file");
	}

	torture_assert_ntstatus_ok(tctx,
		smbcli_close(tree, fnum),
		"failed to close file");

	return true;
}

static bool test_raw_print(struct torture_context *tctx,
			   struct smbcli_state *cli)
{
	return print_printjob(tctx, cli->tree);
}

static bool test_netprintqenum(struct torture_context *tctx,
			       struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i, q;
	uint16_t levels[] = { 0, 1, 2, 3, 4, 5 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_comment(tctx,
			"Testing rap_NetPrintQEnum level %d\n", r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintqenum(cli->tree, tctx, &r),
			"smbcli_rap_netprintqenum failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"failed to enum printq");

		for (q=0; q<r.out.count; q++) {
			switch (r.in.level) {
			case 0:
				printf("%s\n", r.out.info[q].info0.PrintQName);
				break;
			}
		}
	}

	return true;
}

static bool test_netprintqgetinfo(struct torture_context *tctx,
				  struct smbcli_state *cli)
{
	struct rap_NetPrintQGetInfo r;
	struct rap_NetPrintQEnum r_enum;
	int i, p;
	uint16_t levels[] = { 0, 1, 2, 3, 4, 5 };

	r.in.level = 0;
	r.in.bufsize = 0;
	r.in.PrintQueueName = "";

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqgetinfo(cli->tree, tctx, &r),
		"smbcli_rap_netprintqgetinfo failed");

	r_enum.in.level = 5;
	r_enum.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r_enum),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r_enum.out.status),
		"failed to enum printq");

	for (p=0; p < r_enum.out.count; p++) {

		for (i=0; i < ARRAY_SIZE(levels); i++) {

			r.in.level = levels[i];
			r.in.bufsize = 8192;
			r.in.PrintQueueName = r_enum.out.info[p].info5.PrintQueueName;

			torture_comment(tctx, "Testing rap_NetPrintQGetInfo(%s) level %d\n",
				r.in.PrintQueueName, r.in.level);

			torture_assert_ntstatus_ok(tctx,
				smbcli_rap_netprintqgetinfo(cli->tree, tctx, &r),
				"smbcli_rap_netprintqgetinfo failed");

			switch (r.in.level) {
			case 0:
				printf("%s\n", r.out.info.info0.PrintQName);
				break;
			}
		}
	}

	return true;
}

static bool test_netprintjob_pause(struct torture_context *tctx,
				   struct smbcli_state *cli,
				   uint16_t job_id)
{
	struct rap_NetPrintJobPause r;

	r.in.JobID = job_id;

	torture_comment(tctx, "Testing rap_NetPrintJobPause(%d)\n", r.in.JobID);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobpause(cli->tree, tctx, &r),
		"smbcli_rap_netprintjobpause failed");

	return true;
}

static bool test_netprintjob_continue(struct torture_context *tctx,
				      struct smbcli_state *cli,
				      uint16_t job_id)
{
	struct rap_NetPrintJobContinue r;

	r.in.JobID = job_id;

	torture_comment(tctx, "Testing rap_NetPrintJobContinue(%d)\n", r.in.JobID);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobcontinue(cli->tree, tctx, &r),
		"smbcli_rap_netprintjobcontinue failed");

	return true;
}

static bool test_netprintjob_delete(struct torture_context *tctx,
				    struct smbcli_state *cli,
				    uint16_t job_id)
{
	struct rap_NetPrintJobDelete r;

	r.in.JobID = job_id;

	torture_comment(tctx, "Testing rap_NetPrintJobDelete(%d)\n", r.in.JobID);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobdelete(cli->tree, tctx, &r),
		"smbcli_rap_netprintjobdelete failed");

	return true;
}

static bool test_netprintjob(struct torture_context *tctx,
			     struct smbcli_state *cli)
{
	uint16_t job_id = 400;

	torture_assert(tctx,
		test_netprintjob_pause(tctx, cli, job_id),
		"failed to pause job");
	torture_assert(tctx,
		test_netprintjob_continue(tctx, cli, job_id),
		"failed to continue job");
	torture_assert(tctx,
		test_netprintjob_delete(tctx, cli, job_id),
		"failed to delete job");

	return true;
}

static bool test_netprintq_pause(struct torture_context *tctx,
				 struct smbcli_state *cli,
				 const char *PrintQueueName)
{
	struct rap_NetPrintQueuePause r;

	r.in.PrintQueueName = PrintQueueName;

	torture_comment(tctx, "Testing rap_NetPrintQueuePause(%s)\n", r.in.PrintQueueName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqueuepause(cli->tree, tctx, &r),
		"smbcli_rap_netprintqueuepause failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"smbcli_rap_netprintqueuepause failed");

	return true;
}

static bool test_netprintq_resume(struct torture_context *tctx,
				  struct smbcli_state *cli,
				  const char *PrintQueueName)
{
	struct rap_NetPrintQueueResume r;

	r.in.PrintQueueName = PrintQueueName;

	torture_comment(tctx, "Testing rap_NetPrintQueueResume(%s)\n", r.in.PrintQueueName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqueueresume(cli->tree, tctx, &r),
		"smbcli_rap_netprintqueueresume failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"smbcli_rap_netprintqueueresume failed");

	return true;
}

static bool test_netprintq(struct torture_context *tctx,
			   struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i;

	r.in.level = 5;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"failed to enum printq");

	for (i=0; i < r.out.count; i++) {

		const char *printqname = r.out.info[i].info5.PrintQueueName;

		torture_assert(tctx,
			test_netprintq_pause(tctx, cli, printqname),
			"failed to pause print queue");

		torture_assert(tctx,
			test_netprintq_resume(tctx, cli, printqname),
			"failed to resume print queue");
	}

	return true;
}

static bool test_netprintjobenum_args(struct torture_context *tctx,
				      struct smbcli_state *cli,
				      const char *PrintQueueName,
				      uint16_t level,
				      uint16_t *count_p,
				      union rap_printj_info **info_p)
{
	struct rap_NetPrintJobEnum r;

	r.in.PrintQueueName = PrintQueueName;
	r.in.bufsize = 8192;
	r.in.level = level;

	torture_comment(tctx,
		"Testing rap_NetPrintJobEnum(%s) level %d\n", r.in.PrintQueueName, r.in.level);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobenum(cli->tree, tctx, &r),
		"smbcli_rap_netprintjobenum failed");

	if (count_p) {
		*count_p = r.out.count;
	}
	if (info_p) {
		*info_p = r.out.info;
	}

	return true;
}

static bool test_netprintjobenum_one(struct torture_context *tctx,
				     struct smbcli_state *cli,
				     const char *PrintQueueName)
{
	struct rap_NetPrintJobEnum r;
	int i;
	uint16_t levels[] = { 0, 1, 2 };

	r.in.PrintQueueName = PrintQueueName;
	r.in.bufsize = 8192;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];

		torture_comment(tctx,
			"Testing rap_NetPrintJobEnum(%s) level %d\n", r.in.PrintQueueName, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintjobenum(cli->tree, tctx, &r),
			"smbcli_rap_netprintjobenum failed");
	}

	return true;
}

static bool test_netprintjobgetinfo_byid(struct torture_context *tctx,
					 struct smbcli_state *cli,
					 uint16_t JobID)
{
	struct rap_NetPrintJobGetInfo r;
	uint16_t levels[] = { 0, 1, 2 };
	int i;

	r.in.JobID = JobID;
	r.in.bufsize = 8192;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];

		torture_comment(tctx, "Testing rap_NetPrintJobGetInfo(%d) level %d\n", r.in.JobID, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintjobgetinfo(cli->tree, tctx, &r),
			"smbcli_rap_netprintjobgetinfo failed");
	}

	return true;
}

static bool test_netprintjobsetinfo_byid(struct torture_context *tctx,
					 struct smbcli_state *cli,
					 uint16_t JobID)
{
	struct rap_NetPrintJobSetInfo r;
	uint16_t levels[] = { 0, 1, 2 };
	int i;
	const char *comment = "tortured by samba";

	r.in.JobID = JobID;
	r.in.bufsize = strlen(comment);
	r.in.ParamNum = RAP_PARAM_JOBCOMMENT;
	r.in.Param.string = comment;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];

		torture_comment(tctx, "Testing rap_NetPrintJobSetInfo(%d) level %d\n", r.in.JobID, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintjobsetinfo(cli->tree, tctx, &r),
			"smbcli_rap_netprintjobsetinfo failed");
	}

	return true;
}


static bool test_netprintjobgetinfo_byqueue(struct torture_context *tctx,
					    struct smbcli_state *cli,
					    const char *PrintQueueName)
{
	struct rap_NetPrintJobEnum r;
	int i;

	r.in.PrintQueueName = PrintQueueName;
	r.in.bufsize = 8192;
	r.in.level = 0;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobenum(cli->tree, tctx, &r),
		"failed to enumerate jobs");

	for (i=0; i < r.out.count; i++) {

		torture_assert(tctx,
			test_netprintjobgetinfo_byid(tctx, cli, r.out.info[i].info0.JobID),
			"failed to get job info");
	}

	return true;
}

static bool test_netprintjobsetinfo_byqueue(struct torture_context *tctx,
					    struct smbcli_state *cli,
					    const char *PrintQueueName)
{
	struct rap_NetPrintJobEnum r;
	int i;

	r.in.PrintQueueName = PrintQueueName;
	r.in.bufsize = 8192;
	r.in.level = 0;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintjobenum(cli->tree, tctx, &r),
		"failed to enumerate jobs");

	for (i=0; i < r.out.count; i++) {

		torture_assert(tctx,
			test_netprintjobsetinfo_byid(tctx, cli, r.out.info[i].info0.JobID),
			"failed to set job info");
	}

	return true;
}

static bool test_netprintjobenum(struct torture_context *tctx,
				 struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i;

	r.in.level = 5;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"failed to enum printq");

	for (i=0; i < r.out.count; i++) {

		const char *printqname = r.out.info[i].info5.PrintQueueName;

		torture_assert(tctx,
			test_netprintjobenum_one(tctx, cli, printqname),
			"failed to enumerate printjobs on print queue");
	}

	return true;
}

static bool test_netprintjobgetinfo(struct torture_context *tctx,
				    struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i;

	r.in.level = 5;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"failed to enum printq");

	for (i=0; i < r.out.count; i++) {

		const char *printqname = r.out.info[i].info5.PrintQueueName;

		torture_assert(tctx,
			test_netprintjobgetinfo_byqueue(tctx, cli, printqname),
			"failed to enumerate printjobs on print queue");
	}

	return true;
}

static bool test_netprintjobsetinfo(struct torture_context *tctx,
				    struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i;

	r.in.level = 5;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"failed to enum printq");

	for (i=0; i < r.out.count; i++) {

		const char *printqname = r.out.info[i].info5.PrintQueueName;

		torture_assert(tctx,
			test_netprintjobsetinfo_byqueue(tctx, cli, printqname),
			"failed to set printjobs on print queue");
	}

	return true;
}

static bool test_netprintdestenum(struct torture_context *tctx,
				  struct smbcli_state *cli)
{
	struct rap_NetPrintDestEnum r;
	int i;
	uint16_t levels[] = { 0, 1, 2, 3 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_comment(tctx,
			"Testing rap_NetPrintDestEnum level %d\n", r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintdestenum(cli->tree, tctx, &r),
			"smbcli_rap_netprintdestenum failed");
	}

	return true;
}

static bool test_netprintdestgetinfo_bydest(struct torture_context *tctx,
					    struct smbcli_state *cli,
					    const char *PrintDestName)
{
	struct rap_NetPrintDestGetInfo r;
	int i;
	uint16_t levels[] = { 0, 1, 2, 3 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.PrintDestName = PrintDestName;
		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_comment(tctx,
			"Testing rap_NetPrintDestGetInfo(%s) level %d\n", r.in.PrintDestName, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netprintdestgetinfo(cli->tree, tctx, &r),
			"smbcli_rap_netprintdestgetinfo failed");
	}

	return true;
}


static bool test_netprintdestgetinfo(struct torture_context *tctx,
				     struct smbcli_state *cli)
{
	struct rap_NetPrintDestEnum r;
	int i;

	r.in.level = 2;
	r.in.bufsize = 8192;

	torture_comment(tctx,
		"Testing rap_NetPrintDestEnum level %d\n", r.in.level);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintdestenum(cli->tree, tctx, &r),
		"smbcli_rap_netprintdestenum failed");

	for (i=0; i < r.out.count; i++) {

		torture_assert(tctx,
			test_netprintdestgetinfo_bydest(tctx, cli, r.out.info[i].info2.PrinterName),
			"failed to get printdest info");

	}

	return true;
}

static bool test_rap_print(struct torture_context *tctx,
			   struct smbcli_state *cli)
{
	struct rap_NetPrintQEnum r;
	int i;

	r.in.level = 5;
	r.in.bufsize = 8192;

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netprintqenum(cli->tree, tctx, &r),
		"failed to enum printq");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"failed to enum printq");

	for (i=0; i < r.out.count; i++) {

		const char *printqname = r.out.info[i].info5.PrintQueueName;
		struct smbcli_tree *res_queue = NULL;
		uint16_t num_jobs;
		union rap_printj_info *job_info;
		int j;

		torture_assert(tctx,
			test_netprintq_pause(tctx, cli, printqname),
			"failed to set printjobs on print queue");

		torture_assert_ntstatus_ok(tctx,
			torture_second_tcon(tctx, cli->session, printqname, &res_queue),
			"failed to open 2nd connection");

		torture_assert(tctx,
			print_printjob(tctx, res_queue),
			"failed to print job on 2nd connection");

		talloc_free(res_queue);

		torture_assert(tctx,
			test_netprintjobenum_args(tctx, cli, printqname, 1,
			&num_jobs, &job_info),
			"failed to enum printjobs on print queue");

		for (j=0; j < num_jobs; j++) {

			uint16_t job_id = job_info[j].info1.JobID;

			torture_assert(tctx,
				test_netprintjobgetinfo_byid(tctx, cli, job_id),
				"failed to getinfo on new printjob");

			torture_assert(tctx,
				test_netprintjob_delete(tctx, cli, job_id),
				"failed to delete job");
		}

		torture_assert(tctx,
			test_netprintq_resume(tctx, cli, printqname),
			"failed to resume print queue");

	}

	return true;
}

struct torture_suite *torture_rap_printing(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "printing");

	torture_suite_add_1smb_test(suite, "raw_print", test_raw_print);
	torture_suite_add_1smb_test(suite, "rap_print", test_rap_print);
	torture_suite_add_1smb_test(suite, "rap_printq_enum", test_netprintqenum);
	torture_suite_add_1smb_test(suite, "rap_printq_getinfo", test_netprintqgetinfo);
	torture_suite_add_1smb_test(suite, "rap_printq", test_netprintq);
	torture_suite_add_1smb_test(suite, "rap_printjob_enum", test_netprintjobenum);
	torture_suite_add_1smb_test(suite, "rap_printjob_getinfo", test_netprintjobgetinfo);
	torture_suite_add_1smb_test(suite, "rap_printjob_setinfo", test_netprintjobsetinfo);
	torture_suite_add_1smb_test(suite, "rap_printjob", test_netprintjob);
	torture_suite_add_1smb_test(suite, "rap_printdest_enum", test_netprintdestenum);
	torture_suite_add_1smb_test(suite, "rap_printdest_getinfo", test_netprintdestgetinfo);

	return suite;
}
