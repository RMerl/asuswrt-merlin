/*
   Unix SMB/CIFS implementation.

   test suite for SMB2 leases

   Copyright (C) Zachary Loafman 2009

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

static inline uint32_t lease(const char *ls) {
	uint32_t val = 0;
	int i;

	for (i = 0; i < strlen(ls); i++) {
		switch (ls[i]) {
		case 'R':
			val |= SMB2_LEASE_READ;
			break;
		case 'H':
			val |= SMB2_LEASE_HANDLE;
			break;
		case 'W':
			val |= SMB2_LEASE_WRITE;
			break;
		}
	}

	return val;
}

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s): wrong value for %s got 0x%x - should be 0x%x\n", \
				__location__, #v, (int)(v), (int)(correct)); \
		ret = false; \
	}} while (0)

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, __location__": Incorrect status %s - should be %s", \
		       nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

static void smb2_generic_create(struct smb2_create *io, struct smb2_lease *ls,
                                bool dir, const char *name, uint32_t disposition,
                                uint32_t oplock, uint64_t leasekey,
                                uint32_t leasestate)
{
	ZERO_STRUCT(*io);
	io->in.security_flags		= 0x00;
	io->in.oplock_level		= oplock;
	io->in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	io->in.create_flags		= 0x00000000;
	io->in.reserved			= 0x00000000;
	io->in.desired_access		= SEC_RIGHTS_FILE_ALL;
	io->in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	io->in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	io->in.create_disposition	= disposition;
	io->in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	io->in.fname			= name;

	if (dir) {
		io->in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
		io->in.share_access &= ~NTCREATEX_SHARE_ACCESS_DELETE;
		io->in.file_attributes = FILE_ATTRIBUTE_DIRECTORY;
		io->in.create_disposition = NTCREATEX_DISP_CREATE;
	}

	if (ls) {
		ZERO_STRUCT(*ls);
		ls->lease_key.data[0] = leasekey;
		ls->lease_key.data[1] = ~leasekey;
		ls->lease_state = leasestate;
		io->in.lease_request = ls;
	}
}

static void smb2_lease_create(struct smb2_create *io, struct smb2_lease *ls,
                              bool dir, const char *name, uint64_t leasekey,
                              uint32_t leasestate)
{
	smb2_generic_create(io, ls, dir, name, NTCREATEX_DISP_OPEN_IF,
	    SMB2_OPLOCK_LEVEL_LEASE, leasekey, leasestate);
}

static void smb2_oplock_create(struct smb2_create *io, const char *name,
                               uint32_t oplock)
{
	smb2_generic_create(io, NULL, false, name, NTCREATEX_DISP_OPEN_IF,
	    oplock, 0, 0);
}

#define CHECK_CREATED(__io, __created, __attribute)			\
	do {								\
		CHECK_VAL((__io)->out.create_action, NTCREATEX_ACTION_ ## __created); \
		CHECK_VAL((__io)->out.alloc_size, 0);			\
		CHECK_VAL((__io)->out.size, 0);				\
		CHECK_VAL((__io)->out.file_attr, (__attribute));	\
		CHECK_VAL((__io)->out.reserved2, 0);			\
	} while(0)

#define CHECK_LEASE(__io, __state, __oplevel, __key)			\
	do {								\
		if (__oplevel) {					\
			CHECK_VAL((__io)->out.oplock_level, SMB2_OPLOCK_LEVEL_LEASE); \
			CHECK_VAL((__io)->out.lease_response.lease_key.data[0], (__key)); \
			CHECK_VAL((__io)->out.lease_response.lease_key.data[1], ~(__key)); \
			CHECK_VAL((__io)->out.lease_response.lease_state, lease(__state)); \
		} else {						\
			CHECK_VAL((__io)->out.oplock_level, SMB2_OPLOCK_LEVEL_NONE); \
			CHECK_VAL((__io)->out.lease_response.lease_key.data[0], 0); \
			CHECK_VAL((__io)->out.lease_response.lease_key.data[1], 0); \
			CHECK_VAL((__io)->out.lease_response.lease_state, 0); \
		}							\
									\
		CHECK_VAL((__io)->out.lease_response.lease_flags, 0);	\
		CHECK_VAL((__io)->out.lease_response.lease_duration, 0); \
	} while(0)							\

static const uint64_t LEASE1 = 0xBADC0FFEE0DDF00Dull;
static const uint64_t LEASE2 = 0xDEADBEEFFEEDBEADull;
static const uint64_t LEASE3 = 0xDAD0FFEDD00DF00Dull;

#define NREQUEST_RESULTS 8
static const char *request_results[NREQUEST_RESULTS][2] = {
	{ "", "" },
	{ "R", "R" },
	{ "H", "" },
	{ "W", "" },
	{ "RH", "RH" },
	{ "RW", "RW" },
	{ "HW", "" },
	{ "RHW", "RHW" },
};

static bool test_lease_request(struct torture_context *tctx,
	                       struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create io;
	struct smb2_lease ls;
	struct smb2_handle h1, h2;
	NTSTATUS status;
	const char *fname = "lease.dat";
	const char *fname2 = "lease2.dat";
	const char *sname = "lease.dat:stream";
	const char *dname = "lease.dir";
	bool ret = true;
	int i;

	smb2_util_unlink(tree, fname);
	smb2_util_unlink(tree, fname2);
	smb2_util_rmdir(tree, dname);

	/* Win7 is happy to grant RHW leases on files. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h1 = io.out.file.handle;
	CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RHW", true, LEASE1);

	/* But will reject leases on directories. */
	smb2_lease_create(&io, &ls, true, dname, LEASE2, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_DIRECTORY);
	CHECK_LEASE(&io, "", false, 0);
	smb2_util_close(tree, io.out.file.handle);

	/* Also rejects multiple files leased under the same key. */
	smb2_lease_create(&io, &ls, true, fname2, LEASE1, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/* And grants leases on streams (with separate leasekey). */
	smb2_lease_create(&io, &ls, false, sname, LEASE2, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	h2 = io.out.file.handle;
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RHW", true, LEASE2);
	smb2_util_close(tree, h2);

	smb2_util_close(tree, h1);

	/* Now see what combos are actually granted. */
	for (i = 0; i < NREQUEST_RESULTS; i++) {
		torture_comment(tctx, "Requesting lease type %s(%x),"
		    " expecting %s(%x)\n",
		    request_results[i][0], lease(request_results[i][0]),
		    request_results[i][1], lease(request_results[i][1]));
		smb2_lease_create(&io, &ls, false, fname, LEASE1,
		    lease(request_results[i][0]));
		status = smb2_create(tree, mem_ctx, &io);
		h2 = io.out.file.handle;
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, request_results[i][1], true, LEASE1);
		smb2_util_close(tree, io.out.file.handle);
	}

 done:
	smb2_util_close(tree, h1);
	smb2_util_close(tree, h2);

	smb2_util_unlink(tree, fname);
	smb2_util_unlink(tree, fname2);
	smb2_util_rmdir(tree, dname);

	talloc_free(mem_ctx);

	return ret;
}

static bool test_lease_upgrade(struct torture_context *tctx,
                               struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create io;
	struct smb2_lease ls;
	struct smb2_handle h, hnew;
	NTSTATUS status;
	const char *fname = "lease.dat";
	bool ret = true;

	smb2_util_unlink(tree, fname);

	/* Grab a RH lease. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RH"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RH", true, LEASE1);
	h = io.out.file.handle;

	/* Upgrades (sidegrades?) to RW leave us with an RH. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RH", true, LEASE1);
	hnew = io.out.file.handle;

	smb2_util_close(tree, hnew);

	/* Upgrade to RHW lease. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RHW", true, LEASE1);
	hnew = io.out.file.handle;

	smb2_util_close(tree, h);
	h = hnew;

	/* Attempt to downgrade - original lease state is maintained. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RH"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RHW", true, LEASE1);
	hnew = io.out.file.handle;

	smb2_util_close(tree, hnew);

 done:
	smb2_util_close(tree, h);
	smb2_util_close(tree, hnew);

	smb2_util_unlink(tree, fname);

	talloc_free(mem_ctx);

	return ret;
}

#define CHECK_LEASE_BREAK(__lb, __oldstate, __state, __key)		\
	do {								\
		CHECK_VAL((__lb)->new_lease_state, lease(__state));	\
		CHECK_VAL((__lb)->current_lease.lease_state, lease(__oldstate)); \
		CHECK_VAL((__lb)->current_lease.lease_key.data[0], (__key)); \
		CHECK_VAL((__lb)->current_lease.lease_key.data[1], ~(__key)); \
	} while(0)

#define CHECK_LEASE_BREAK_ACK(__lba, __state, __key)			\
	do {								\
		CHECK_VAL((__lba)->out.reserved, 0);			\
		CHECK_VAL((__lba)->out.lease.lease_key.data[0], (__key)); \
		CHECK_VAL((__lba)->out.lease.lease_key.data[1], ~(__key)); \
		CHECK_VAL((__lba)->out.lease.lease_state, lease(__state)); \
		CHECK_VAL((__lba)->out.lease.lease_flags, 0);		\
		CHECK_VAL((__lba)->out.lease.lease_duration, 0);	\
	} while(0)

static struct {
	struct smb2_lease_break lease_break;
	struct smb2_lease_break_ack lease_break_ack;
	int count;
	int failures;

	struct smb2_handle oplock_handle;
	int held_oplock_level;
	int oplock_level;
	int oplock_count;
	int oplock_failures;
} break_info;

#define CHECK_BREAK_INFO(__oldstate, __state, __key)			\
	do {								\
		CHECK_VAL(break_info.failures, 0);			\
		CHECK_VAL(break_info.count, 1);				\
		CHECK_LEASE_BREAK(&break_info.lease_break, (__oldstate), \
		    (__state), (__key));				\
		if (break_info.lease_break.break_flags &		\
		    SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED) {	\
			CHECK_LEASE_BREAK_ACK(&break_info.lease_break_ack, \
				              (__state), (__key));	\
		}							\
	} while(0)

static void torture_lease_break_callback(struct smb2_request *req)
{
	NTSTATUS status;

	status = smb2_lease_break_ack_recv(req, &break_info.lease_break_ack);
	if (!NT_STATUS_IS_OK(status))
		break_info.failures++;

	return;
}

/* a lease break request handler */
static bool torture_lease_handler(struct smb2_transport *transport,
				  const struct smb2_lease_break *lb,
				  void *private_data)
{
	struct smb2_tree *tree = private_data;
	struct smb2_lease_break_ack io;
	struct smb2_request *req;

	break_info.lease_break = *lb;
	break_info.count++;

	if (lb->break_flags & SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED) {
		ZERO_STRUCT(io);
		io.in.lease.lease_key = lb->current_lease.lease_key;
		io.in.lease.lease_state = lb->new_lease_state;

		req = smb2_lease_break_ack_send(tree, &io);
		req->async.fn = torture_lease_break_callback;
		req->async.private_data = NULL;
	}

	return true;
}

/*
  break_results should be read as "held lease, new lease, hold broken to, new
  grant", i.e. { "RH", "RW", "RH", "R" } means that if key1 holds RH and key2
  tries for RW, key1 will be broken to RH (in this case, not broken at all)
  and key2 will be granted R.

  Note: break_results only includes things that Win7 will actually grant (see
  request_results above).
 */
#define NBREAK_RESULTS 16
static const char *break_results[NBREAK_RESULTS][4] = {
	{"R",	"R",	"R",	"R"},
	{"R",	"RH",	"R",	"RH"},
	{"R",	"RW",	"R",	"R"},
	{"R",	"RHW",	"R",	"RH"},

	{"RH",	"R",	"RH",	"R"},
	{"RH",	"RH",	"RH",	"RH"},
	{"RH",	"RW",	"RH",	"R"},
	{"RH",	"RHW",	"RH",	"RH"},

	{"RW",	"R",	"R",	"R"},
	{"RW",	"RH",	"R",	"RH"},
	{"RW",	"RW",	"R",	"R"},
	{"RW",	"RHW",	"R",	"RH"},

	{"RHW",	"R",	"RH",	"R"},
	{"RHW",	"RH",	"RH",	"RH"},
	{"RHW",	"RW",	"RH",	"R"},
	{"RHW", "RHW",	"RH",	"RH"},
};

static bool test_lease_break(struct torture_context *tctx,
                               struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create io;
	struct smb2_lease ls;
	struct smb2_handle h, h2, h3;
	NTSTATUS status;
	const char *fname = "lease.dat";
	bool ret = true;
	int i;

	tree->session->transport->lease.handler	= torture_lease_handler;
	tree->session->transport->lease.private_data = tree;

	smb2_util_unlink(tree, fname);

	for (i = 0; i < NBREAK_RESULTS; i++) {
		const char *held = break_results[i][0];
		const char *contend = break_results[i][1];
		const char *brokento = break_results[i][2];
		const char *granted = break_results[i][3];
		torture_comment(tctx, "Hold %s(%x), requesting %s(%x), "
		    "expecting break to %s(%x) and grant of %s(%x)\n",
		    held, lease(held), contend, lease(contend),
		    brokento, lease(brokento), granted, lease(granted));

		ZERO_STRUCT(break_info);

		/* Grab lease. */
		smb2_lease_create(&io, &ls, false, fname, LEASE1, lease(held));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h = io.out.file.handle;
		CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, held, true, LEASE1);

		/* Possibly contend lease. */
		smb2_lease_create(&io, &ls, false, fname, LEASE2, lease(contend));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h2 = io.out.file.handle;
		CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, granted, true, LEASE2);

		if (lease(held) != lease(brokento)) {
			CHECK_BREAK_INFO(held, brokento, LEASE1);
		} else {
			CHECK_VAL(break_info.count, 0);
			CHECK_VAL(break_info.failures, 0);
		}

		ZERO_STRUCT(break_info);

		/*
		  Now verify that an attempt to upgrade LEASE1 results in no
		  break and no change in LEASE1.
		 */
		smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RHW"));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h3 = io.out.file.handle;
		CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, brokento, true, LEASE1);
		CHECK_VAL(break_info.count, 0);
		CHECK_VAL(break_info.failures, 0);

		smb2_util_close(tree, h);
		smb2_util_close(tree, h2);
		smb2_util_close(tree, h3);

		status = smb2_util_unlink(tree, fname);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

 done:
	smb2_util_close(tree, h);
	smb2_util_close(tree, h2);

	smb2_util_unlink(tree, fname);

	talloc_free(mem_ctx);

	return ret;
}

static void torture_oplock_break_callback(struct smb2_request *req)
{
	NTSTATUS status;
	struct smb2_break br;

	ZERO_STRUCT(br);
	status = smb2_break_recv(req, &br);
	if (!NT_STATUS_IS_OK(status))
		break_info.oplock_failures++;

	return;
}

/* a oplock break request handler */
static bool torture_oplock_handler(struct smb2_transport *transport,
				   const struct smb2_handle *handle,
				   uint8_t level, void *private_data)
{
	struct smb2_tree *tree = private_data;
	struct smb2_request *req;
	struct smb2_break br;

	break_info.oplock_handle = *handle;
	break_info.oplock_level	= level;
	break_info.oplock_count++;

	ZERO_STRUCT(br);
	br.in.file.handle = *handle;
	br.in.oplock_level = level;

	if (break_info.held_oplock_level > SMB2_OPLOCK_LEVEL_II) {
		req = smb2_break_send(tree, &br);
		req->async.fn = torture_oplock_break_callback;
		req->async.private_data = NULL;
	}
	break_info.held_oplock_level = level;

	return true;
}

static inline uint32_t oplock(const char *op) {
	uint32_t val = SMB2_OPLOCK_LEVEL_NONE;
	int i;

	for (i = 0; i < strlen(op); i++) {
		switch (op[i]) {
		case 's':
			return SMB2_OPLOCK_LEVEL_II;
		case 'x':
			return SMB2_OPLOCK_LEVEL_EXCLUSIVE;
		case 'b':
			return SMB2_OPLOCK_LEVEL_EXCLUSIVE;
		default:
			continue;
		}
	}

	return val;
}

#define NOPLOCK_RESULTS 12
static const char *oplock_results[NOPLOCK_RESULTS][4] = {
	{"R",	"s",	"R",	"s"},
	{"R",	"x",	"R",	"s"},
	{"R",	"b",	"R",	"s"},

	{"RH",	"s",	"RH",	""},
	{"RH",	"x",	"RH",	""},
	{"RH",	"b",	"RH",	""},

	{"RW",	"s",	"R",	"s"},
	{"RW",	"x",	"R",	"s"},
	{"RW",	"b",	"R",	"s"},

	{"RHW",	"s",	"RH",	""},
	{"RHW",	"x",	"RH",	""},
	{"RHW",	"b",	"RH",	""},
};

static const char *oplock_results_2[NOPLOCK_RESULTS][4] = {
	{"s",	"R",	"s",	"R"},
	{"s",	"RH",	"s",	"R"},
	{"s",	"RW",	"s",	"R"},
	{"s",	"RHW",	"s",	"R"},

	{"x",	"R",	"s",	"R"},
	{"x",	"RH",	"s",	"R"},
	{"x",	"RW",	"s",	"R"},
	{"x",	"RHW",	"s",	"R"},

	{"b",	"R",	"s",	"R"},
	{"b",	"RH",	"s",	"R"},
	{"b",	"RW",	"s",	"R"},
	{"b",	"RHW",	"s",	"R"},
};

static bool test_lease_oplock(struct torture_context *tctx,
                              struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create io;
	struct smb2_lease ls;
	struct smb2_handle h, h2;
	NTSTATUS status;
	const char *fname = "lease.dat";
	bool ret = true;
	int i;

	tree->session->transport->lease.handler	= torture_lease_handler;
	tree->session->transport->lease.private_data = tree;
	tree->session->transport->oplock.handler = torture_oplock_handler;
	tree->session->transport->oplock.private_data = tree;

	smb2_util_unlink(tree, fname);

	for (i = 0; i < NOPLOCK_RESULTS; i++) {
		const char *held = oplock_results[i][0];
		const char *contend = oplock_results[i][1];
		const char *brokento = oplock_results[i][2];
		const char *granted = oplock_results[i][3];
		torture_comment(tctx, "Hold %s(%x), requesting %s(%x), "
		    "expecting break to %s(%x) and grant of %s(%x)\n",
		    held, lease(held), contend, oplock(contend),
		    brokento, lease(brokento), granted, oplock(granted));

		ZERO_STRUCT(break_info);

		/* Grab lease. */
		smb2_lease_create(&io, &ls, false, fname, LEASE1, lease(held));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h = io.out.file.handle;
		CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, held, true, LEASE1);

		/* Does an oplock contend the lease? */
		smb2_oplock_create(&io, fname, oplock(contend));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h2 = io.out.file.handle;
		CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_VAL(io.out.oplock_level, oplock(granted));
		break_info.held_oplock_level = io.out.oplock_level;

		if (lease(held) != lease(brokento)) {
			CHECK_BREAK_INFO(held, brokento, LEASE1);
		} else {
			CHECK_VAL(break_info.count, 0);
			CHECK_VAL(break_info.failures, 0);
		}

		smb2_util_close(tree, h);
		smb2_util_close(tree, h2);

		status = smb2_util_unlink(tree, fname);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	for (i = 0; i < NOPLOCK_RESULTS; i++) {
		const char *held = oplock_results_2[i][0];
		const char *contend = oplock_results_2[i][1];
		const char *brokento = oplock_results_2[i][2];
		const char *granted = oplock_results_2[i][3];
		torture_comment(tctx, "Hold %s(%x), requesting %s(%x), "
		    "expecting break to %s(%x) and grant of %s(%x)\n",
		    held, oplock(held), contend, lease(contend),
		    brokento, oplock(brokento), granted, lease(granted));

		ZERO_STRUCT(break_info);

		/* Grab an oplock. */
		smb2_oplock_create(&io, fname, oplock(held));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h = io.out.file.handle;
		CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_VAL(io.out.oplock_level, oplock(held));
		break_info.held_oplock_level = io.out.oplock_level;

		/* Grab lease. */
		smb2_lease_create(&io, &ls, false, fname, LEASE1, lease(contend));
		status = smb2_create(tree, mem_ctx, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
		h2 = io.out.file.handle;
		CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
		CHECK_LEASE(&io, granted, true, LEASE1);

		if (oplock(held) != oplock(brokento)) {
			CHECK_VAL(break_info.oplock_count, 1);
			CHECK_VAL(break_info.oplock_failures, 0);
			CHECK_VAL(break_info.oplock_level, oplock(brokento));
			break_info.held_oplock_level = break_info.oplock_level;
		} else {
			CHECK_VAL(break_info.oplock_count, 0);
			CHECK_VAL(break_info.oplock_failures, 0);
		}

		smb2_util_close(tree, h);
		smb2_util_close(tree, h2);

		status = smb2_util_unlink(tree, fname);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

 done:
	smb2_util_close(tree, h);
	smb2_util_close(tree, h2);

	smb2_util_unlink(tree, fname);

	talloc_free(mem_ctx);

	return ret;
}

static bool test_lease_multibreak(struct torture_context *tctx,
                                  struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create io;
	struct smb2_lease ls;
	struct smb2_handle h, h2, h3;
	struct smb2_write w;
	NTSTATUS status;
	const char *fname = "lease.dat";
	bool ret = true;

	tree->session->transport->lease.handler	= torture_lease_handler;
	tree->session->transport->lease.private_data = tree;
	tree->session->transport->oplock.handler = torture_oplock_handler;
	tree->session->transport->oplock.private_data = tree;

	smb2_util_unlink(tree, fname);

	ZERO_STRUCT(break_info);

	/* Grab lease, upgrade to RHW .. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RH"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h = io.out.file.handle;
	CHECK_CREATED(&io, CREATED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RH", true, LEASE1);

	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h2 = io.out.file.handle;
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RHW", true, LEASE1);

	/* Contend with LEASE2. */
	smb2_lease_create(&io, &ls, false, fname, LEASE2, lease("RHW"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h3 = io.out.file.handle;
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "RH", true, LEASE2);

	/* Verify that we were only sent one break. */
	CHECK_BREAK_INFO("RHW", "RH", LEASE1);

	/* Drop LEASE1 / LEASE2 */
	status = smb2_util_close(tree, h);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_util_close(tree, h2);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_util_close(tree, h3);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(break_info);

	/* Grab an R lease. */
	smb2_lease_create(&io, &ls, false, fname, LEASE1, lease("R"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h = io.out.file.handle;
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_LEASE(&io, "R", true, LEASE1);

	/* Grab a level-II oplock. */
	smb2_oplock_create(&io, fname, oplock("s"));
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h2 = io.out.file.handle;
	CHECK_CREATED(&io, EXISTED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_VAL(io.out.oplock_level, oplock("s"));
	break_info.held_oplock_level = io.out.oplock_level;

	/* Verify no breaks. */
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	/* Open for truncate, force a break. */
	smb2_generic_create(&io, NULL, false, fname,
	    NTCREATEX_DISP_OVERWRITE_IF, oplock(""), 0, 0);
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h3 = io.out.file.handle;
	CHECK_CREATED(&io, TRUNCATED, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_VAL(io.out.oplock_level, oplock(""));
	break_info.held_oplock_level = io.out.oplock_level;

	/* Sleep, use a write to clear the recv queue. */
	smb_msleep(250);
	ZERO_STRUCT(w);
	w.in.file.handle = h3;
	w.in.offset      = 0;
	w.in.data        = data_blob_talloc(mem_ctx, NULL, 4096);
	status = smb2_write(tree, &w);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Verify one oplock break, one lease break. */
	CHECK_VAL(break_info.oplock_count, 1);
	CHECK_VAL(break_info.oplock_failures, 0);
	CHECK_VAL(break_info.oplock_level, oplock(""));
	CHECK_BREAK_INFO("R", "", LEASE1);

 done:
	smb2_util_close(tree, h);
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h3);

	smb2_util_unlink(tree, fname);

	talloc_free(mem_ctx);

	return ret;
}

struct torture_suite *torture_smb2_lease_init(void)
{
	struct torture_suite *suite =
	    torture_suite_create(talloc_autofree_context(), "lease");

	torture_suite_add_1smb2_test(suite, "request", test_lease_request);
	torture_suite_add_1smb2_test(suite, "upgrade", test_lease_upgrade);
	torture_suite_add_1smb2_test(suite, "break", test_lease_break);
	torture_suite_add_1smb2_test(suite, "oplock", test_lease_oplock);
	torture_suite_add_1smb2_test(suite, "multibreak", test_lease_multibreak);

	suite->description = talloc_strdup(suite, "SMB2-LEASE tests");

	return suite;
}
