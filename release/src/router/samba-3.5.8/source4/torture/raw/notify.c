/* 
   Unix SMB/CIFS implementation.
   basic raw test suite for change notify
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "system/filesys.h"
#include "torture/util.h"

#define BASEDIR "\\test_notify"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%d) Incorrect status %s - should be %s\n", \
		       __LINE__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)


#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%d) wrong value for %s  0x%x should be 0x%x\n", \
		       __LINE__, #v, (int)v, (int)correct); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_WSTR(field, value, flags) do { \
	if (!field.s || strcmp(field.s, value) || wire_bad_flags(&field, flags, cli->transport)) { \
		printf("(%d) %s [%s] != %s\n",  __LINE__, #field, field.s, value); \
			ret = false; \
		goto done; \
	}} while (0)


/* 
   basic testing of change notify on directories
*/
static bool test_notify_dir(struct smbcli_state *cli, struct smbcli_state *cli2, 
			    TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	union smb_close cl;
	int i, count, fnum, fnum2;
	struct smbcli_request *req, *req2;
	extern int torture_numops;

	printf("TESTING CHANGE NOTIFY ON DIRECTRIES\n");
		
	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	printf("testing notify cancel\n");

	req = smb_raw_changenotify_send(cli->tree, &notify);
	smb_raw_ntcancel(req);
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	printf("testing notify mkdir\n");

	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_mkdir(cli2->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("testing notify rmdir\n");

	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_rmdir(cli2->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("testing notify mkdir - rmdir - mkdir - rmdir\n");

	smbcli_mkdir(cli2->tree, BASEDIR "\\subdir-name");
	smbcli_rmdir(cli2->tree, BASEDIR "\\subdir-name");
	smbcli_mkdir(cli2->tree, BASEDIR "\\subdir-name");
	smbcli_rmdir(cli2->tree, BASEDIR "\\subdir-name");
	msleep(200);
	req = smb_raw_changenotify_send(cli->tree, &notify);
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 4);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[1].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[1].name, "subdir-name", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[2].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[2].name, "subdir-name", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[3].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[3].name, "subdir-name", STR_UNICODE);

	count = torture_numops;
	printf("testing buffered notify on create of %d files\n", count);
	for (i=0;i<count;i++) {
		char *fname = talloc_asprintf(cli, BASEDIR "\\test%d.txt", i);
		int fnum3 = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		if (fnum3 == -1) {
			printf("Failed to create %s - %s\n", 
			       fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum3);
	}

	/* (1st notify) setup a new notify on a different directory handle.
	   This new notify won't see the events above. */
	notify.nttrans.in.file.fnum = fnum2;
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	/* (2nd notify) whereas this notify will see the above buffered events,
	   and it directly returns the buffered events */
	notify.nttrans.in.file.fnum = fnum;
	req = smb_raw_changenotify_send(cli->tree, &notify);

	status = smbcli_unlink(cli->tree, BASEDIR "\\nonexistant.txt");
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	/* (1st unlink) as the 2nd notify directly returns,
	   this unlink is only seen by the 1st notify and 
	   the 3rd notify (later) */
	printf("testing notify on unlink for the first file\n");
	status = smbcli_unlink(cli2->tree, BASEDIR "\\test0.txt");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* receive the reply from the 2nd notify */
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, count);
	for (i=1;i<count;i++) {
		CHECK_VAL(notify.nttrans.out.changes[i].action, NOTIFY_ACTION_ADDED);
	}
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "test0.txt", STR_UNICODE);

	printf("and now from the 1st notify\n");
	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "test0.txt", STR_UNICODE);

	printf("(3rd notify) this notify will only see the 1st unlink\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);

	status = smbcli_unlink(cli->tree, BASEDIR "\\nonexistant.txt");
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	printf("testing notify on wildcard unlink for %d files\n", count-1);
	/* (2nd unlink) do a wildcard unlink */
	status = smbcli_unlink(cli2->tree, BASEDIR "\\test*.txt");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* receive the 3rd notify */
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "test0.txt", STR_UNICODE);

	/* and we now see the rest of the unlink calls on both directory handles */
	notify.nttrans.in.file.fnum = fnum;
	sleep(3);
	req = smb_raw_changenotify_send(cli->tree, &notify);
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, count-1);
	for (i=0;i<notify.nttrans.out.num_changes;i++) {
		CHECK_VAL(notify.nttrans.out.changes[i].action, NOTIFY_ACTION_REMOVED);
	}
	notify.nttrans.in.file.fnum = fnum2;
	req = smb_raw_changenotify_send(cli->tree, &notify);
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, count-1);
	for (i=0;i<notify.nttrans.out.num_changes;i++) {
		CHECK_VAL(notify.nttrans.out.changes[i].action, NOTIFY_ACTION_REMOVED);
	}

	printf("testing if a close() on the dir handle triggers the notify reply\n");

	notify.nttrans.in.file.fnum = fnum;
	req = smb_raw_changenotify_send(cli->tree, &notify);

	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 0);

done:
	smb_raw_exit(cli->session);
	return ret;
}

/*
 * Check notify reply for a rename action. Not sure if this is a valid thing
 * to do, but depending on timing between inotify and messaging we get the
 * add/remove/modify in any order. This routines tries to find the action/name
 * pair in any of the three following notify_changes.
 */

static bool check_rename_reply(struct smbcli_state *cli,
			       int line,
			       struct notify_changes *actions,
			       uint32_t action, const char *name)
{
	int i;

	for (i=0; i<3; i++) {
		if (actions[i].action == action) {
			if ((actions[i].name.s == NULL)
			    || (strcmp(actions[i].name.s, name) != 0)
			    || (wire_bad_flags(&actions[i].name, STR_UNICODE,
					       cli->transport))) {
				printf("(%d) name [%s] != %s\n", line,
				       actions[i].name.s, name);
				return false;
			}
			return true;
		}
	}

	printf("(%d) expected action %d, not found\n", line, action);
	return false;
}

/* 
   testing of recursive change notify
*/
static bool test_notify_recursive(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req1, *req2;

	printf("TESTING CHANGE NOTIFY WITH RECURSION\n");
		
	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify, on file or directory name
	   changes. Setup both with and without recursion */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_CREATION;
	notify.nttrans.in.file.fnum = fnum;

	notify.nttrans.in.recursive = true;
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	notify.nttrans.in.recursive = false;
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	/* cancel initial requests so the buffer is setup */
	smb_raw_ntcancel(req1);
	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	smb_raw_ntcancel(req2);
	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");
	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name\\subname1");
	smbcli_close(cli->tree, 
		     smbcli_open(cli->tree, BASEDIR "\\subdir-name\\subname2", O_CREAT, 0));
	smbcli_rename(cli->tree, BASEDIR "\\subdir-name\\subname1", BASEDIR "\\subdir-name\\subname1-r");
	smbcli_rename(cli->tree, BASEDIR "\\subdir-name\\subname2", BASEDIR "\\subname2-r");
	smbcli_rename(cli->tree, BASEDIR "\\subname2-r", BASEDIR "\\subname3-r");

	notify.nttrans.in.completion_filter = 0;
	notify.nttrans.in.recursive = true;
	msleep(200);
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name\\subname1-r");
	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name");
	smbcli_unlink(cli->tree, BASEDIR "\\subname3-r");

	notify.nttrans.in.recursive = false;
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 11);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[1].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[1].name, "subdir-name\\subname1", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[2].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[2].name, "subdir-name\\subname2", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[3].action, NOTIFY_ACTION_OLD_NAME);
	CHECK_WSTR(notify.nttrans.out.changes[3].name, "subdir-name\\subname1", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[4].action, NOTIFY_ACTION_NEW_NAME);
	CHECK_WSTR(notify.nttrans.out.changes[4].name, "subdir-name\\subname1-r", STR_UNICODE);

	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[5],
		NOTIFY_ACTION_ADDED, "subname2-r");
	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[5],
		NOTIFY_ACTION_REMOVED, "subdir-name\\subname2");
	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[5],
		NOTIFY_ACTION_MODIFIED, "subname2-r");
		
	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[8],
		NOTIFY_ACTION_OLD_NAME, "subname2-r");
	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[8],
		NOTIFY_ACTION_NEW_NAME, "subname3-r");
	ret &= check_rename_reply(
		cli, __LINE__, &notify.nttrans.out.changes[8],
		NOTIFY_ACTION_MODIFIED, "subname3-r");

	if (!ret) {
		goto done;
	}

	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 3);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name\\subname1-r", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[1].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[1].name, "subdir-name", STR_UNICODE);
	CHECK_VAL(notify.nttrans.out.changes[2].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[2].name, "subname3-r", STR_UNICODE);

done:
	smb_raw_exit(cli->session);
	return ret;
}

/* 
   testing of change notify mask change
*/
static bool test_notify_mask_change(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req1, *req2;

	printf("TESTING CHANGE NOTIFY WITH MASK CHANGE\n");

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify, on file or directory name
	   changes. Setup both with and without recursion */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_ATTRIBUTES;
	notify.nttrans.in.file.fnum = fnum;

	notify.nttrans.in.recursive = true;
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	notify.nttrans.in.recursive = false;
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	/* cancel initial requests so the buffer is setup */
	smb_raw_ntcancel(req1);
	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	smb_raw_ntcancel(req2);
	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	notify.nttrans.in.recursive = true;
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	/* Set to hidden then back again. */
	smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));
	smbcli_setatr(cli->tree, BASEDIR "\\tname1", FILE_ATTRIBUTE_HIDDEN, 0);
	smbcli_unlink(cli->tree, BASEDIR "\\tname1");

	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_MODIFIED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "tname1", STR_UNICODE);

	/* Now try and change the mask to include other events.
	 * This should not work - once the mask is set on a directory
	 * fnum it seems to be fixed until the fnum is closed. */

	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_CREATION;
	notify.nttrans.in.recursive = true;
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	notify.nttrans.in.recursive = false;
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");
	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name\\subname1");
	smbcli_close(cli->tree, 
		     smbcli_open(cli->tree, BASEDIR "\\subdir-name\\subname2", O_CREAT, 0));
	smbcli_rename(cli->tree, BASEDIR "\\subdir-name\\subname1", BASEDIR "\\subdir-name\\subname1-r");
	smbcli_rename(cli->tree, BASEDIR "\\subdir-name\\subname2", BASEDIR "\\subname2-r");
	smbcli_rename(cli->tree, BASEDIR "\\subname2-r", BASEDIR "\\subname3-r");

	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name\\subname1-r");
	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name");
	smbcli_unlink(cli->tree, BASEDIR "\\subname3-r");

	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_MODIFIED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subname2-r", STR_UNICODE);

	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_MODIFIED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subname3-r", STR_UNICODE);

	if (!ret) {
		goto done;
	}

done:
	smb_raw_exit(cli->session);
	return ret;
}


/* 
   testing of mask bits for change notify
*/
static bool test_notify_mask(struct smbcli_state *cli, struct torture_context *tctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum, fnum2;
	uint32_t mask;
	int i;
	char c = 1;
	struct timeval tv;
	NTTIME t;

	printf("TESTING CHANGE NOTIFY COMPLETION FILTERS\n");

	tv = timeval_current_ofs(1000, 0);
	t = timeval_to_nttime(&tv);

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.recursive = true;

#define NOTIFY_MASK_TEST(test_name, setup, op, cleanup, Action, expected, nchanges) \
	do { \
	smbcli_getatr(cli->tree, test_name, NULL, NULL, NULL); \
	do { for (mask=i=0;i<32;i++) { \
		struct smbcli_request *req; \
		status = smb_raw_open(cli->tree, tctx, &io); \
		CHECK_STATUS(status, NT_STATUS_OK); \
		fnum = io.ntcreatex.out.file.fnum; \
		setup \
		notify.nttrans.in.file.fnum = fnum;	\
		notify.nttrans.in.completion_filter = (1<<i); \
		req = smb_raw_changenotify_send(cli->tree, &notify); \
		op \
		msleep(200); smb_raw_ntcancel(req); \
		status = smb_raw_changenotify_recv(req, tctx, &notify); \
		cleanup \
		smbcli_close(cli->tree, fnum); \
		if (NT_STATUS_EQUAL(status, NT_STATUS_CANCELLED)) continue; \
		CHECK_STATUS(status, NT_STATUS_OK); \
		/* special case to cope with file rename behaviour */ \
		if (nchanges == 2 && notify.nttrans.out.num_changes == 1 && \
		    notify.nttrans.out.changes[0].action == NOTIFY_ACTION_MODIFIED && \
		    ((expected) & FILE_NOTIFY_CHANGE_ATTRIBUTES) && \
		    Action == NOTIFY_ACTION_OLD_NAME) { \
			printf("(rename file special handling OK)\n"); \
		} else if (nchanges != notify.nttrans.out.num_changes) { \
			printf("ERROR: nchanges=%d expected=%d action=%d filter=0x%08x\n", \
			       notify.nttrans.out.num_changes, \
			       nchanges, \
			       notify.nttrans.out.changes[0].action, \
			       notify.nttrans.in.completion_filter); \
			ret = false; \
		} else if (notify.nttrans.out.changes[0].action != Action) { \
			printf("ERROR: nchanges=%d action=%d expectedAction=%d filter=0x%08x\n", \
			       notify.nttrans.out.num_changes, \
			       notify.nttrans.out.changes[0].action, \
			       Action, \
			       notify.nttrans.in.completion_filter); \
			ret = false; \
		} else if (strcmp(notify.nttrans.out.changes[0].name.s, "tname1") != 0) { \
			printf("ERROR: nchanges=%d action=%d filter=0x%08x name=%s\n", \
			       notify.nttrans.out.num_changes, \
			       notify.nttrans.out.changes[0].action, \
			       notify.nttrans.in.completion_filter, \
			       notify.nttrans.out.changes[0].name.s);	\
			ret = false; \
		} \
		mask |= (1<<i); \
	} \
	if ((expected) != mask) { \
		if (((expected) & ~mask) != 0) { \
			printf("ERROR: trigger on too few bits. mask=0x%08x expected=0x%08x\n", \
			       mask, expected); \
			ret = false; \
		} else { \
			printf("WARNING: trigger on too many bits. mask=0x%08x expected=0x%08x\n", \
			       mask, expected); \
		} \
	} \
	} while (0); \
	} while (0);

	printf("testing mkdir\n");
	NOTIFY_MASK_TEST("testing mkdir",;,
			 smbcli_mkdir(cli->tree, BASEDIR "\\tname1");,
			 smbcli_rmdir(cli->tree, BASEDIR "\\tname1");,
			 NOTIFY_ACTION_ADDED,
			 FILE_NOTIFY_CHANGE_DIR_NAME, 1);

	printf("testing create file\n");
	NOTIFY_MASK_TEST("testing create file",;,
			 smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));,
			 smbcli_unlink(cli->tree, BASEDIR "\\tname1");,
			 NOTIFY_ACTION_ADDED,
			 FILE_NOTIFY_CHANGE_FILE_NAME, 1);

	printf("testing unlink\n");
	NOTIFY_MASK_TEST("testing unlink",
			 smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));,
			 smbcli_unlink(cli->tree, BASEDIR "\\tname1");,
			 ;,
			 NOTIFY_ACTION_REMOVED,
			 FILE_NOTIFY_CHANGE_FILE_NAME, 1);

	printf("testing rmdir\n");
	NOTIFY_MASK_TEST("testing rmdir",
			 smbcli_mkdir(cli->tree, BASEDIR "\\tname1");,
			 smbcli_rmdir(cli->tree, BASEDIR "\\tname1");,
			 ;,
			 NOTIFY_ACTION_REMOVED,
			 FILE_NOTIFY_CHANGE_DIR_NAME, 1);

	printf("testing rename file\n");
	NOTIFY_MASK_TEST("testing rename file",
			 smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));,
			 smbcli_rename(cli->tree, BASEDIR "\\tname1", BASEDIR "\\tname2");,
			 smbcli_unlink(cli->tree, BASEDIR "\\tname2");,
			 NOTIFY_ACTION_OLD_NAME,
			 FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_CREATION, 2);

	printf("testing rename dir\n");
	NOTIFY_MASK_TEST("testing rename dir",
		smbcli_mkdir(cli->tree, BASEDIR "\\tname1");,
		smbcli_rename(cli->tree, BASEDIR "\\tname1", BASEDIR "\\tname2");,
		smbcli_rmdir(cli->tree, BASEDIR "\\tname2");,
		NOTIFY_ACTION_OLD_NAME,
		FILE_NOTIFY_CHANGE_DIR_NAME, 2);

	printf("testing set path attribute\n");
	NOTIFY_MASK_TEST("testing set path attribute",
		smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));,
		smbcli_setatr(cli->tree, BASEDIR "\\tname1", FILE_ATTRIBUTE_HIDDEN, 0);,
		smbcli_unlink(cli->tree, BASEDIR "\\tname1");,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_ATTRIBUTES, 1);

	printf("testing set path write time\n");
	NOTIFY_MASK_TEST("testing set path write time",
		smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1", O_CREAT, 0));,
		smbcli_setatr(cli->tree, BASEDIR "\\tname1", FILE_ATTRIBUTE_NORMAL, 1000);,
		smbcli_unlink(cli->tree, BASEDIR "\\tname1");,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_LAST_WRITE, 1);

	printf("testing set file attribute\n");
	NOTIFY_MASK_TEST("testing set file attribute",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_fsetatr(cli->tree, fnum2, FILE_ATTRIBUTE_HIDDEN, 0, 0, 0, 0);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_ATTRIBUTES, 1);

	if (torture_setting_bool(tctx, "samba3", false)) {
		printf("Samba3 does not yet support create times "
		       "everywhere\n");
	}
	else {
		printf("testing set file create time\n");
		NOTIFY_MASK_TEST("testing set file create time",
			fnum2 = create_complex_file(cli, tctx,
						    BASEDIR "\\tname1");,
			smbcli_fsetatr(cli->tree, fnum2, 0, t, 0, 0, 0);,
			(smbcli_close(cli->tree, fnum2),
			 smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
			NOTIFY_ACTION_MODIFIED,
			FILE_NOTIFY_CHANGE_CREATION, 1);
	}

	printf("testing set file access time\n");
	NOTIFY_MASK_TEST("testing set file access time",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_fsetatr(cli->tree, fnum2, 0, 0, t, 0, 0);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_LAST_ACCESS, 1);

	printf("testing set file write time\n");
	NOTIFY_MASK_TEST("testing set file write time",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_fsetatr(cli->tree, fnum2, 0, 0, 0, t, 0);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_LAST_WRITE, 1);

	printf("testing set file change time\n");
	NOTIFY_MASK_TEST("testing set file change time",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_fsetatr(cli->tree, fnum2, 0, 0, 0, 0, t);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		0, 1);


	printf("testing write\n");
	NOTIFY_MASK_TEST("testing write",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_write(cli->tree, fnum2, 1, &c, 10000, 1);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		0, 1);

	printf("testing truncate\n");
	NOTIFY_MASK_TEST("testing truncate",
		fnum2 = create_complex_file(cli, tctx, BASEDIR "\\tname1");,
		smbcli_ftruncate(cli->tree, fnum2, 10000);,
		(smbcli_close(cli->tree, fnum2), smbcli_unlink(cli->tree, BASEDIR "\\tname1"));,
		NOTIFY_ACTION_MODIFIED,
		FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES, 1);

done:
	smb_raw_exit(cli->session);
	return ret;
}

/*
  basic testing of change notify on files
*/
static bool test_notify_file(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_close cl;
	union smb_notify notify;
	struct smbcli_request *req;
	int fnum;
	const char *fname = BASEDIR "\\file.txt";

	printf("TESTING CHANGE NOTIFY ON FILES\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_STREAM_NAME;
	notify.nttrans.in.recursive = false;

	printf("testing if notifies on file handles are invalid (should be)\n");

	req = smb_raw_changenotify_send(cli->tree, &notify);
	status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_unlink(cli->tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb_raw_exit(cli->session);
	return ret;
}

/*
  basic testing of change notifies followed by a tdis
*/
static bool test_notify_tdis(struct torture_context *tctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req;
	struct smbcli_state *cli = NULL;

	printf("TESTING CHANGE NOTIFY FOLLOWED BY TDIS\n");

	if (!torture_open_connection(&cli, tctx, 0)) {
		return false;
	}

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req = smb_raw_changenotify_send(cli->tree, &notify);

	status = smbcli_tdis(cli);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->tree = NULL;

	status = smb_raw_changenotify_recv(req, tctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 0);

done:
	torture_close_connection(cli);
	return ret;
}

/*
  basic testing of change notifies followed by a exit
*/
static bool test_notify_exit(struct torture_context *tctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req;
	struct smbcli_state *cli = NULL;

	printf("TESTING CHANGE NOTIFY FOLLOWED BY EXIT\n");

	if (!torture_open_connection(&cli, tctx, 0)) {
		return false;
	}

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req = smb_raw_changenotify_send(cli->tree, &notify);

	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_changenotify_recv(req, tctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 0);

done:
	torture_close_connection(cli);
	return ret;
}

/*
  basic testing of change notifies followed by a ulogoff
*/
static bool test_notify_ulogoff(struct torture_context *tctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req;
	struct smbcli_state *cli = NULL;

	printf("TESTING CHANGE NOTIFY FOLLOWED BY ULOGOFF\n");

	if (!torture_open_connection(&cli, tctx, 0)) {
		return false;
	}

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req = smb_raw_changenotify_send(cli->tree, &notify);

	status = smb_raw_ulogoff(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_changenotify_recv(req, tctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 0);

done:
	torture_close_connection(cli);
	return ret;
}

static void tcp_dis_handler(struct smbcli_transport *t, void *p)
{
	struct smbcli_state *cli = (struct smbcli_state *)p;
	smbcli_transport_dead(cli->transport, NT_STATUS_LOCAL_DISCONNECT);
	cli->transport = NULL;
	cli->tree = NULL;
}
/*
  basic testing of change notifies followed by tcp disconnect
*/
static bool test_notify_tcp_dis(struct torture_context *tctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req;
	struct smbcli_state *cli = NULL;

	printf("TESTING CHANGE NOTIFY FOLLOWED BY TCP DISCONNECT\n");

	if (!torture_open_connection(&cli, tctx, 0)) {
		return false;
	}

	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req = smb_raw_changenotify_send(cli->tree, &notify);

	smbcli_transport_idle_handler(cli->transport, tcp_dis_handler, 250, cli);

	status = smb_raw_changenotify_recv(req, tctx, &notify);
	CHECK_STATUS(status, NT_STATUS_LOCAL_DISCONNECT);

done:
	torture_close_connection(cli);
	return ret;
}

/* 
   test setting up two change notify requests on one handle
*/
static bool test_notify_double(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum;
	struct smbcli_request *req1, *req2;

	printf("TESTING CHANGE NOTIFY TWICE ON ONE DIRECTORY\n");
		
	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req1 = smb_raw_changenotify_send(cli->tree, &notify);
	req2 = smb_raw_changenotify_send(cli->tree, &notify);

	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name2");

	status = smb_raw_changenotify_recv(req2, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name2", STR_UNICODE);

done:
	smb_raw_exit(cli->session);
	return ret;
}


/* 
   test multiple change notifies at different depths and with/without recursion
*/
static bool test_notify_tree(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	union smb_notify notify;
	union smb_open io;
	struct smbcli_request *req;
	struct timeval tv;
	struct {
		const char *path;
		bool recursive;
		uint32_t filter;
		int expected;
		int fnum;
		int counted;
	} dirs[] = {
		{BASEDIR "\\abc",               true, FILE_NOTIFY_CHANGE_NAME, 30 },
		{BASEDIR "\\zqy",               true, FILE_NOTIFY_CHANGE_NAME, 8 },
		{BASEDIR "\\atsy",              true, FILE_NOTIFY_CHANGE_NAME, 4 },
		{BASEDIR "\\abc\\foo",          true,  FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\abc\\blah",         true,  FILE_NOTIFY_CHANGE_NAME, 13 },
		{BASEDIR "\\abc\\blah",         false, FILE_NOTIFY_CHANGE_NAME, 7 },
		{BASEDIR "\\abc\\blah\\a",      true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\abc\\blah\\b",      true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\abc\\blah\\c",      true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\abc\\fooblah",      true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\zqy\\xx",           true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\zqy\\yyy",          true, FILE_NOTIFY_CHANGE_NAME, 2 },
		{BASEDIR "\\zqy\\..",           true, FILE_NOTIFY_CHANGE_NAME, 40 },
		{BASEDIR,                       true, FILE_NOTIFY_CHANGE_NAME, 40 },
		{BASEDIR,                       false,FILE_NOTIFY_CHANGE_NAME, 6 },
		{BASEDIR "\\atsy",              false,FILE_NOTIFY_CHANGE_NAME, 4 },
		{BASEDIR "\\abc",               true, FILE_NOTIFY_CHANGE_NAME, 24 },
		{BASEDIR "\\abc",               false,FILE_NOTIFY_CHANGE_FILE_NAME, 0 },
		{BASEDIR "\\abc",               true, FILE_NOTIFY_CHANGE_FILE_NAME, 0 },
		{BASEDIR "\\abc",               true, FILE_NOTIFY_CHANGE_NAME, 24 },
	};
	int i;
	NTSTATUS status;
	bool all_done = false;

	printf("TESTING CHANGE NOTIFY FOR DIFFERENT DEPTHS\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;

	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 20000;

	/*
	  setup the directory tree, and the notify buffer on each directory
	*/
	for (i=0;i<ARRAY_SIZE(dirs);i++) {
		io.ntcreatex.in.fname = dirs[i].path;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		dirs[i].fnum = io.ntcreatex.out.file.fnum;

		notify.nttrans.in.completion_filter = dirs[i].filter;
		notify.nttrans.in.file.fnum = dirs[i].fnum;
		notify.nttrans.in.recursive = dirs[i].recursive;
		req = smb_raw_changenotify_send(cli->tree, &notify);
		smb_raw_ntcancel(req);
		status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
		CHECK_STATUS(status, NT_STATUS_CANCELLED);
	}

	/* trigger 2 events in each dir */
	for (i=0;i<ARRAY_SIZE(dirs);i++) {
		char *path = talloc_asprintf(mem_ctx, "%s\\test.dir", dirs[i].path);
		smbcli_mkdir(cli->tree, path);
		smbcli_rmdir(cli->tree, path);
		talloc_free(path);
	}

	/* give a bit of time for the events to propogate */
	tv = timeval_current();

	do {
		/* count events that have happened in each dir */
		for (i=0;i<ARRAY_SIZE(dirs);i++) {
			notify.nttrans.in.file.fnum = dirs[i].fnum;
			req = smb_raw_changenotify_send(cli->tree, &notify);
			smb_raw_ntcancel(req);
			notify.nttrans.out.num_changes = 0;
			status = smb_raw_changenotify_recv(req, mem_ctx, &notify);
			dirs[i].counted += notify.nttrans.out.num_changes;
		}
		
		all_done = true;

		for (i=0;i<ARRAY_SIZE(dirs);i++) {
			if (dirs[i].counted != dirs[i].expected) {
				all_done = false;
			}
		}
	} while (!all_done && timeval_elapsed(&tv) < 20);

	printf("took %.4f seconds to propogate all events\n", timeval_elapsed(&tv));

	for (i=0;i<ARRAY_SIZE(dirs);i++) {
		if (dirs[i].counted != dirs[i].expected) {
			printf("ERROR: i=%d expected %d got %d for '%s'\n",
			       i, dirs[i].expected, dirs[i].counted, dirs[i].path);
			ret = false;
		}
	}

	/*
	  run from the back, closing and deleting
	*/
	for (i=ARRAY_SIZE(dirs)-1;i>=0;i--) {
		smbcli_close(cli->tree, dirs[i].fnum);
		smbcli_rmdir(cli->tree, dirs[i].path);
	}

done:
	smb_raw_exit(cli->session);
	return ret;
}

/*
   Test response when cached server events exceed single NT NOTFIY response
   packet size.
*/
static bool test_notify_overflow(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum, fnum2;
	int count = 100;
	struct smbcli_request *req1;
	int i;

	printf("TESTING CHANGE NOTIFY EVENT OVERFLOW\n");

	/* get a handle on the directory */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* ask for a change notify, on name changes. */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;

	notify.nttrans.in.recursive = true;
	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	/* cancel initial requests so the buffer is setup */
	smb_raw_ntcancel(req1);
	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	/* open a lot of files, filling up the server side notify buffer */
	printf("testing overflowed buffer notify on create of %d files\n",
	       count);
	for (i=0;i<count;i++) {
		char *fname = talloc_asprintf(cli, BASEDIR "\\test%d.txt", i);
		int fnum2 = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR,
					DENY_NONE);
		if (fnum2 == -1) {
			printf("Failed to create %s - %s\n",
			       fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum2);
	}

	/* expect that 0 events will be returned with NT_STATUS_OK */
	req1 = smb_raw_changenotify_send(cli->tree, &notify);
	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 0);

done:
	smb_raw_exit(cli->session);
	return ret;
}

/*
   Test if notifications are returned for changes to the base directory.
   They shouldn't be.
*/
static bool test_notify_basedir(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum, fnum2;
	int count = 100;
	struct smbcli_request *req1;
	int i;

	printf("TESTING CHANGE NOTIFY BASEDIR EVENTS\n");

	/* get a handle on the directory */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* create a test file that will also be modified */
	smbcli_close(cli->tree, smbcli_open(cli->tree, BASEDIR "\\tname1",
					    O_CREAT, 0));

	/* ask for a change notify, on attribute changes. */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_ATTRIBUTES;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	req1 = smb_raw_changenotify_send(cli->tree, &notify);

	/* set attribute on the base dir */
	smbcli_setatr(cli->tree, BASEDIR, FILE_ATTRIBUTE_HIDDEN, 0);

	/* set attribute on a file to assure we receive a notification */
	smbcli_setatr(cli->tree, BASEDIR "\\tname1", FILE_ATTRIBUTE_HIDDEN, 0);
	msleep(200);

	/* check how many responses were given, expect only 1 for the file */
	status = smb_raw_changenotify_recv(req1, mem_ctx, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_MODIFIED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "tname1", STR_UNICODE);

done:
	smb_raw_exit(cli->session);
	return ret;
}


/*
  create a secondary tree connect - used to test for a bug in Samba3 messaging
  with change notify
*/
static struct smbcli_tree *secondary_tcon(struct smbcli_state *cli, 
					  struct torture_context *tctx)
{
	NTSTATUS status;
	const char *share, *host;
	struct smbcli_tree *tree;
	union smb_tcon tcon;

	share = torture_setting_string(tctx, "share", NULL);
	host  = torture_setting_string(tctx, "host", NULL);
	
	printf("create a second tree context on the same session\n");
	tree = smbcli_tree_init(cli->session, tctx, false);

	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = talloc_asprintf(tctx, "\\\\%s\\%s", host, share);
	tcon.tconx.in.device = "A:";	
	status = smb_raw_tcon(tree, tctx, &tcon);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tree);
		printf("Failed to create secondary tree\n");
		return NULL;
	}

	tree->tid = tcon.tconx.out.tid;
	printf("tid1=%d tid2=%d\n", cli->tree->tid, tree->tid);

	return tree;
}


/* 
   very simple change notify test
*/
static bool test_notify_tcon(struct smbcli_state *cli, struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	union smb_notify notify;
	union smb_open io;
	int fnum, fnum2;
	struct smbcli_request *req;
	extern int torture_numops;
	struct smbcli_tree *tree = NULL;
		
	printf("TESTING SIMPLE CHANGE NOTIFY\n");
		
	/*
	  get a handle on the directory
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR;

	status = smb_raw_open(cli->tree, torture, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	status = smb_raw_open(cli->tree, torture, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	/* ask for a change notify,
	   on file or directory name changes */
	notify.nttrans.level = RAW_NOTIFY_NTTRANS;
	notify.nttrans.in.buffer_size = 1000;
	notify.nttrans.in.completion_filter = FILE_NOTIFY_CHANGE_NAME;
	notify.nttrans.in.file.fnum = fnum;
	notify.nttrans.in.recursive = true;

	printf("testing notify mkdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("testing notify rmdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("SIMPLE CHANGE NOTIFY OK\n");

	printf("TESTING WITH SECONDARY TCON\n");
	tree = secondary_tcon(cli, torture);

	printf("testing notify mkdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("testing notify rmdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("CHANGE NOTIFY WITH TCON OK\n");

	printf("Disconnecting secondary tree\n");
	status = smb_tree_disconnect(tree);
	CHECK_STATUS(status, NT_STATUS_OK);
	talloc_free(tree);

	printf("testing notify mkdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_mkdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("testing notify rmdir\n");
	req = smb_raw_changenotify_send(cli->tree, &notify);
	smbcli_rmdir(cli->tree, BASEDIR "\\subdir-name");

	status = smb_raw_changenotify_recv(req, torture, &notify);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(notify.nttrans.out.num_changes, 1);
	CHECK_VAL(notify.nttrans.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WSTR(notify.nttrans.out.changes[0].name, "subdir-name", STR_UNICODE);

	printf("CHANGE NOTIFY WITH TDIS OK\n");
done:
	smb_raw_exit(cli->session);
	return ret;
}


/* 
   basic testing of change notify
*/
bool torture_raw_notify(struct torture_context *torture, 
			struct smbcli_state *cli, 
			struct smbcli_state *cli2)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_notify_tcon(cli, torture);
	ret &= test_notify_dir(cli, cli2, torture);
	ret &= test_notify_mask(cli, torture);
	ret &= test_notify_recursive(cli, torture);
	ret &= test_notify_mask_change(cli, torture);
	ret &= test_notify_file(cli, torture);
	ret &= test_notify_tdis(torture);
	ret &= test_notify_exit(torture);
	ret &= test_notify_ulogoff(torture);
	ret &= test_notify_tcp_dis(torture);
	ret &= test_notify_double(cli, torture);
	ret &= test_notify_tree(cli, torture);
	ret &= test_notify_overflow(cli, torture);
	ret &= test_notify_basedir(cli, torture);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}
