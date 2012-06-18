/* 
   Unix SMB/CIFS implementation.

   SMB2 notify test suite

   Copyright (C) Stefan Metzmacher 2006
   
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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"

#include "libcli/raw/libcliraw.h"
#include "lib/events/events.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%s) Incorrect value %s=%d - should be %d\n", \
		       __location__, #v, v, correct); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_WIRE_STR(field, value) do { \
	if (!field.s || strcmp(field.s, value)) { \
		printf("(%s) %s [%s] != %s\n", \
			  __location__, #field, field.s, value); \
		ret = false; \
		goto done; \
	}} while (0)

#define FNAME "smb2-notify01.dat"

#define TARGET_IS_WIN7(_tctx) (torture_setting_bool(_tctx, "win7", false))

static bool test_valid_request(struct torture_context *tctx, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle dh;
	struct smb2_notify n;
	struct smb2_request *req;
	uint32_t max_buffer_size = 0x00080000;

	if (TARGET_IS_WIN7(tctx)) {
		max_buffer_size = 0x00010000;
	}

	smb2_util_unlink(tree, FNAME);

	status = smb2_util_roothandle(tree, &dh);
	CHECK_STATUS(status, NT_STATUS_OK);

	n.in.recursive		= 0x0000;
	n.in.buffer_size	= max_buffer_size;
	n.in.file.handle	= dh;
	n.in.completion_filter	= 0x00000FFF;
	n.in.unknown		= 0x00000000;
	req = smb2_notify_send(tree, &n);

	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			break;
		}
	}

	status = torture_setup_complex_file(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(n.out.num_changes, 1);
	CHECK_VALUE(n.out.changes[0].action, NOTIFY_ACTION_ADDED);
	CHECK_WIRE_STR(n.out.changes[0].name, FNAME);

	/* 
	 * if the change response doesn't fit in the buffer
	 * NOTIFY_ENUM_DIR is returned.
	 */
	n.in.buffer_size	= 0x00000000;
	req = smb2_notify_send(tree, &n);

	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			break;
		}
	}

	status = torture_setup_complex_file(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, STATUS_NOTIFY_ENUM_DIR);

	/* 
	 * if the change response fits in the buffer we get
	 * NT_STATUS_OK again
	 */
	n.in.buffer_size	= max_buffer_size;
	req = smb2_notify_send(tree, &n);

	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			break;
		}
	}

	status = torture_setup_complex_file(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(n.out.num_changes, 3);
	CHECK_VALUE(n.out.changes[0].action, NOTIFY_ACTION_REMOVED);
	CHECK_WIRE_STR(n.out.changes[0].name, FNAME);
	CHECK_VALUE(n.out.changes[1].action, NOTIFY_ACTION_ADDED);
	CHECK_WIRE_STR(n.out.changes[1].name, FNAME);
	CHECK_VALUE(n.out.changes[2].action, NOTIFY_ACTION_MODIFIED);
	CHECK_WIRE_STR(n.out.changes[2].name, FNAME);

	/* if the first notify returns NOTIFY_ENUM_DIR, all do */
	status = smb2_util_close(tree, dh);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_util_roothandle(tree, &dh);
	CHECK_STATUS(status, NT_STATUS_OK);

	n.in.recursive		= 0x0000;
	n.in.buffer_size	= 0x00000001;
	n.in.file.handle	= dh;
	n.in.completion_filter	= 0x00000FFF;
	n.in.unknown		= 0x00000000;
	req = smb2_notify_send(tree, &n);

	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			break;
		}
	}

	status = torture_setup_complex_file(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, STATUS_NOTIFY_ENUM_DIR);

	n.in.buffer_size	= max_buffer_size;
	req = smb2_notify_send(tree, &n);
	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			break;
		}
	}

	status = torture_setup_complex_file(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, STATUS_NOTIFY_ENUM_DIR);

	/* if the buffer size is too large, we get invalid parameter */
	n.in.recursive		= 0x0000;
	n.in.buffer_size	= max_buffer_size + 1;
	n.in.file.handle	= dh;
	n.in.completion_filter	= 0x00000FFF;
	n.in.unknown		= 0x00000000;
	req = smb2_notify_send(tree, &n);
	status = smb2_notify_recv(req, tctx, &n);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

done:
	return ret;
}

/* basic testing of SMB2 notify
*/
bool torture_smb2_notify(struct torture_context *torture)
{
	struct smb2_tree *tree;
	bool ret = true;

	if (!torture_smb2_connection(torture, &tree)) {
		return false;
	}

	ret &= test_valid_request(torture, tree);

	return ret;
}
