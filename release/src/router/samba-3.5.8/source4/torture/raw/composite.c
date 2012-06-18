/* 
   Unix SMB/CIFS implementation.

   libcli composite function testing

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
#include "torture/torture.h"
#include "lib/events/events.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "libcli/security/security.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "lib/cmdline/popt_common.h"
#include "torture/util.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

#define BASEDIR "\\composite"

static void loadfile_complete(struct composite_context *c)
{
	int *count = talloc_get_type(c->async.private_data, int);
	(*count)++;
}

/*
  test a simple savefile/loadfile combination
*/
static bool test_loadfile(struct smbcli_state *cli, struct torture_context *tctx)
{
	const char *fname = BASEDIR "\\test.txt";
	NTSTATUS status;
	struct smb_composite_savefile io1;
	struct smb_composite_loadfile io2;
	struct composite_context **c;
	uint8_t *data;
	size_t len = random() % 100000;
	const int num_ops = 50;
	int i;
	int *count = talloc_zero(tctx, int);

	data = talloc_array(tctx, uint8_t, len);

	generate_random_buffer(data, len);

	io1.in.fname = fname;
	io1.in.data  = data;
	io1.in.size  = len;

	printf("testing savefile\n");

	status = smb_composite_savefile(cli->tree, &io1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("(%s) savefile failed: %s\n", __location__,nt_errstr(status));
		return false;
	}

	io2.in.fname = fname;

	printf("testing parallel loadfile with %d ops\n", num_ops);

	c = talloc_array(tctx, struct composite_context *, num_ops);

	for (i=0;i<num_ops;i++) {
		c[i] = smb_composite_loadfile_send(cli->tree, &io2);
		c[i]->async.fn = loadfile_complete;
		c[i]->async.private_data = count;
	}

	printf("waiting for completion\n");
	while (*count != num_ops) {
		event_loop_once(cli->transport->socket->event.ctx);
		if (torture_setting_bool(tctx, "progress", true)) {
			printf("(%s) count=%d\r", __location__, *count);
			fflush(stdout);
		}
	}
	printf("count=%d\n", *count);
	
	for (i=0;i<num_ops;i++) {
		status = smb_composite_loadfile_recv(c[i], tctx);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) loadfile[%d] failed - %s\n", __location__, i, nt_errstr(status));
			return false;
		}

		if (io2.out.size != len) {
			printf("(%s) wrong length in returned data - %d should be %d\n",__location__,
			       io2.out.size, (int)len);
			return false;
		}
		
		if (memcmp(io2.out.data, data, len) != 0) {
			printf("(%s) wrong data in loadfile!\n",__location__);
			return false;
		}
	}

	talloc_free(data);

	return true;
}

/*
  test a simple savefile/loadfile combination
*/
static bool test_fetchfile(struct smbcli_state *cli, struct torture_context *tctx)
{
	const char *fname = BASEDIR "\\test.txt";
	NTSTATUS status;
	struct smb_composite_savefile io1;
	struct smb_composite_fetchfile io2;
	struct composite_context **c;
	uint8_t *data;
	int i;
	size_t len = random() % 10000;
	extern int torture_numops;
	struct tevent_context *event_ctx;
	int *count = talloc_zero(tctx, int);
	bool ret = true;

	data = talloc_array(tctx, uint8_t, len);

	generate_random_buffer(data, len);

	io1.in.fname = fname;
	io1.in.data  = data;
	io1.in.size  = len;

	printf("testing savefile\n");

	status = smb_composite_savefile(cli->tree, &io1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("(%s) savefile failed: %s\n",__location__, nt_errstr(status));
		return false;
	}

	io2.in.dest_host = torture_setting_string(tctx, "host", NULL);
	io2.in.ports = lp_smb_ports(tctx->lp_ctx);
	io2.in.called_name = torture_setting_string(tctx, "host", NULL);
	io2.in.service = torture_setting_string(tctx, "share", NULL);
	io2.in.service_type = "A:";

	io2.in.credentials = cmdline_credentials;
	io2.in.workgroup  = lp_workgroup(tctx->lp_ctx);
	io2.in.filename = fname;
	io2.in.resolve_ctx = lp_resolve_context(tctx->lp_ctx);
	io2.in.iconv_convenience = lp_iconv_convenience(tctx->lp_ctx);
	io2.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);
	lp_smbcli_options(tctx->lp_ctx, &io2.in.options);
	lp_smbcli_session_options(tctx->lp_ctx, &io2.in.session_options);

	printf("testing parallel fetchfile with %d ops\n", torture_numops);

	event_ctx = cli->transport->socket->event.ctx;
	c = talloc_array(tctx, struct composite_context *, torture_numops);

	for (i=0; i<torture_numops; i++) {
		c[i] = smb_composite_fetchfile_send(&io2, event_ctx);
		c[i]->async.fn = loadfile_complete;
		c[i]->async.private_data = count;
	}

	printf("waiting for completion\n");

	while (*count != torture_numops) {
		event_loop_once(event_ctx);
		if (torture_setting_bool(tctx, "progress", true)) {
			printf("(%s) count=%d\r", __location__, *count);
			fflush(stdout);
		}
	}
	printf("count=%d\n", *count);

	for (i=0;i<torture_numops;i++) {
		status = smb_composite_fetchfile_recv(c[i], tctx);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) loadfile[%d] failed - %s\n", __location__, i,
			       nt_errstr(status));
			ret = false;
			continue;
		}

		if (io2.out.size != len) {
			printf("(%s) wrong length in returned data - %d "
			       "should be %d\n", __location__,
			       io2.out.size, (int)len);
			ret = false;
			continue;
		}
		
		if (memcmp(io2.out.data, data, len) != 0) {
			printf("(%s) wrong data in loadfile!\n", __location__);
			ret = false;
			continue;
		}
	}

	return ret;
}

/*
  test setfileacl
*/
static bool test_appendacl(struct smbcli_state *cli, struct torture_context *tctx)
{
	struct smb_composite_appendacl **io;
	struct smb_composite_appendacl **io_orig;
	struct composite_context **c;
	struct tevent_context *event_ctx;

	struct security_descriptor *test_sd;
	struct security_ace *ace;
	struct dom_sid *test_sid;

	const int num_ops = 50;
	int *count = talloc_zero(tctx, int);
	struct smb_composite_savefile io1;

	NTSTATUS status;
	int i;

	io_orig = talloc_array(tctx, struct smb_composite_appendacl *, num_ops);

	printf ("creating %d empty files and getting their acls with appendacl\n", num_ops);

	for (i = 0; i < num_ops; i++) {
		io1.in.fname = talloc_asprintf(io_orig, BASEDIR "\\test%d.txt", i);
		io1.in.data  = NULL;
		io1.in.size  = 0;
	  
		status = smb_composite_savefile(cli->tree, &io1);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) savefile failed: %s\n", __location__, nt_errstr(status));
			return false;
		}

		io_orig[i] = talloc (io_orig, struct smb_composite_appendacl);
		io_orig[i]->in.fname = talloc_steal(io_orig[i], io1.in.fname);
		io_orig[i]->in.sd = security_descriptor_initialise(io_orig[i]);
		status = smb_composite_appendacl(cli->tree, io_orig[i], io_orig[i]);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) appendacl failed: %s\n", __location__, nt_errstr(status));
			return false;
		}
	}
	

	/* fill Security Descriptor with aces to be added */

	test_sd = security_descriptor_initialise(tctx);
	test_sid = dom_sid_parse_talloc (tctx, "S-1-5-32-1234-5432");

	ace = talloc_zero(tctx, struct security_ace);

	ace->type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace->flags = 0;
	ace->access_mask = SEC_STD_ALL;
	ace->trustee = *test_sid;

	status = security_descriptor_dacl_add(test_sd, ace);
	if (!NT_STATUS_IS_OK(status)) {
		printf("(%s) appendacl failed: %s\n", __location__, nt_errstr(status));
		return false;
	}

	/* set parameters for appendacl async call */

	printf("testing parallel appendacl with %d ops\n", num_ops);

	c = talloc_array(tctx, struct composite_context *, num_ops);
	io = talloc_array(tctx, struct  smb_composite_appendacl *, num_ops);

	for (i=0; i < num_ops; i++) {
		io[i] = talloc (io, struct smb_composite_appendacl);
		io[i]->in.sd = test_sd;
		io[i]->in.fname = talloc_asprintf(io[i], BASEDIR "\\test%d.txt", i);

		c[i] = smb_composite_appendacl_send(cli->tree, io[i]);
		c[i]->async.fn = loadfile_complete;
		c[i]->async.private_data = count;
	}

	event_ctx = tctx->ev;
	printf("waiting for completion\n");
	while (*count != num_ops) {
		event_loop_once(event_ctx);
		if (torture_setting_bool(tctx, "progress", true)) {
			printf("(%s) count=%d\r", __location__, *count);
			fflush(stdout);
		}
	}
	printf("count=%d\n", *count);

	for (i=0; i < num_ops; i++) {
		status = smb_composite_appendacl_recv(c[i], io[i]);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) appendacl[%d] failed - %s\n", __location__, i, nt_errstr(status));
			return false;
		}
		
		security_descriptor_dacl_add(io_orig[i]->out.sd, ace);
		if (!security_acl_equal(io_orig[i]->out.sd->dacl, io[i]->out.sd->dacl)) {
			printf("(%s) appendacl[%d] failed - needed acl isn't set\n", __location__, i);
			return false;
		}
	}
	

	talloc_free (ace);
	talloc_free (test_sid);
	talloc_free (test_sd);
		
	return true;
}

/* test a query FS info by asking for share's GUID */
static bool test_fsinfo(struct smbcli_state *cli, struct torture_context *tctx)
{
	char *guid = NULL;
	NTSTATUS status;
	struct smb_composite_fsinfo io1;
	struct composite_context **c;

	int i;
	extern int torture_numops;
	struct tevent_context *event_ctx;
	int *count = talloc_zero(tctx, int);
	bool ret = true;

	io1.in.dest_host = torture_setting_string(tctx, "host", NULL);
	io1.in.dest_ports = lp_smb_ports(tctx->lp_ctx);
	io1.in.socket_options = lp_socket_options(tctx->lp_ctx);
	io1.in.called_name = torture_setting_string(tctx, "host", NULL);
	io1.in.service = torture_setting_string(tctx, "share", NULL);
	io1.in.service_type = "A:";
	io1.in.credentials = cmdline_credentials;
	io1.in.workgroup = lp_workgroup(tctx->lp_ctx);
	io1.in.level = RAW_QFS_OBJECTID_INFORMATION;
	io1.in.iconv_convenience = lp_iconv_convenience(tctx->lp_ctx);
	io1.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);

	printf("testing parallel queryfsinfo [Object ID] with %d ops\n", torture_numops);

	event_ctx = tctx->ev;
	c = talloc_array(tctx, struct composite_context *, torture_numops);

	for (i=0; i<torture_numops; i++) {
		c[i] = smb_composite_fsinfo_send(cli->tree, &io1, lp_resolve_context(tctx->lp_ctx));
		c[i]->async.fn = loadfile_complete;
		c[i]->async.private_data = count;
	}

	printf("waiting for completion\n");

	while (*count < torture_numops) {
		event_loop_once(event_ctx);
		if (torture_setting_bool(tctx, "progress", true)) {
			printf("(%s) count=%d\r", __location__, *count);
			fflush(stdout);
		}
	}
	printf("count=%d\n", *count);

	for (i=0;i<torture_numops;i++) {
		status = smb_composite_fsinfo_recv(c[i], tctx);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) fsinfo[%d] failed - %s\n", __location__, i, nt_errstr(status));
			ret = false;
			continue;
		}

		if (io1.out.fsinfo->generic.level != RAW_QFS_OBJECTID_INFORMATION) {
			printf("(%s) wrong level in returned info - %d "
			       "should be %d\n", __location__,
			       io1.out.fsinfo->generic.level, RAW_QFS_OBJECTID_INFORMATION);
			ret = false;
			continue;
		}

		guid=GUID_string(tctx, &io1.out.fsinfo->objectid_information.out.guid);
		printf("[%d] GUID: %s\n", i, guid);

		
	}

	return ret;
}


/* 
   basic testing of libcli composite calls
*/
bool torture_raw_composite(struct torture_context *tctx, 
			   struct smbcli_state *cli)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_fetchfile(cli, tctx);
	ret &= test_loadfile(cli, tctx);
 	ret &= test_appendacl(cli, tctx);
	ret &= test_fsinfo(cli, tctx);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
