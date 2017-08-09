/*
 * Unix SMB/CIFS implementation.
 * Test module for perfcounters
 *
 * Copyright (C) Todd Stecher 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"

#define PARM_PC_TEST_TYPE		"pc_test"
#define PARM_DUMPON_COUNT		"count"
#define PARM_DUMPON_COUNT_DEFAULT	50

struct perfcount_test_identity {
	uid_t uid;
	char *user;
	char *domain;
};

struct perfcount_test_counter {
	int op;
	int sub_op;
	int ioctl;
	uint64_t bytes_in;
	uint64_t bytes_out;
	int count;

	struct perfcount_test_counter *next;
	struct perfcount_test_counter *prev;
};

struct perfcount_test_context {

	/* wip:  identity */
	struct perfcount_test_identity *id;
	struct perfcount_test_counter *ops;
};

#define MAX_OP 256
struct perfcount_test_counter *g_list[MAX_OP];

int count;

/* determine frequency of dumping results */
int count_mod = 1;

static void perfcount_test_add_counters(struct perfcount_test_context *ctxt)
{
	struct perfcount_test_counter *head;
	struct perfcount_test_counter *ptc;
	struct perfcount_test_counter *tmp;
	bool found;

	for (ptc = ctxt->ops; ptc != NULL; ) {

		found = false;

		if (ptc->op > MAX_OP)
			continue;

		for (head = g_list[ptc->op]; head != NULL; head = head->next) {
			if ((ptc->sub_op == head->sub_op) &&
			    (ptc->ioctl == head->ioctl)) {
				head->bytes_in += ptc->bytes_in;
				head->bytes_out += ptc->bytes_out;
				head->count++;
				tmp = ptc->next;
				DLIST_REMOVE(ctxt->ops, ptc);
				SAFE_FREE(ptc);
				ptc = tmp;
				found = true;
				break;
			}
		}

		/* not in global tracking list - add it */
		if (!found) {
			tmp = ptc->next;
			DLIST_REMOVE(ctxt->ops, ptc);
			ptc->count = 1;
			DLIST_ADD(g_list[ptc->op], ptc);
			ptc = tmp;
		}
	}

}

#if 0

static void perfcount_test_dump_id(struct perfcount_test_identity *id, int lvl)
{
	if (!id)
		return;

	DEBUG(lvl,("uid - %d\n", id->uid));
	DEBUG(lvl,("user - %s\n", id->user));
	DEBUG(lvl,("domain - %s\n", id->domain));
}

#endif

static const char *trans_subop_table[] = {
	"unknown", "trans:create", "trans:ioctl", "trans:set sd",
	"trans:change notify", "trans: rename", "trans:get sd",
	"trans:get quota", "trans:set quota"
};

static const char *trans2_subop_table[] = {
	"trans2:open", "trans2:find first", "trans2:find next",
	"trans2:q fsinfo", "trans2:set fsinfo", "trans2:q path info",
	"trans2:set pathinfo", "trans2:fs ctl", "trans2: io ctl",
	"trans2:find notify first", "trans2:find notify next",
	"trans2:mkdir", "trans2:sess setup", "trans2:get dfs referral",
	"trans2:report dfs inconsistent"
};

static const char *smb_subop_name(int op, int subop)
{
	/* trans */
	if (op == 0x25) {
		if (subop > sizeof(trans_subop_table) /
		    sizeof(trans_subop_table[0])) {
			return "unknown";
		}
		return trans_subop_table[subop];
	} else if (op == 0x32) {
		if (subop > sizeof(trans2_subop_table) /
		    sizeof(trans2_subop_table[0])) {
			return "unknown";
		}
		return trans2_subop_table[subop];
	}

	return "unknown";
}

static void perfcount_test_dump_counter(struct perfcount_test_counter *ptc,
					int lvl)
{
	DEBUG(lvl, ("OP: %s\n", smb_fn_name(ptc->op)));
	if (ptc->sub_op > 0) {
		DEBUG(lvl, ("SUBOP: %s\n",
			smb_subop_name(ptc->op, ptc->sub_op)));
	}

	if (ptc->ioctl > 0) {
		DEBUG(lvl, ("IOCTL: %d\n", ptc->ioctl));
	}

	DEBUG(lvl, ("Count: %d\n\n", ptc->count));
}

static void perfcount_test_dump_counters(void)
{
	int i;
	struct perfcount_test_counter *head;

	count_mod = lp_parm_int(0, PARM_PC_TEST_TYPE, PARM_DUMPON_COUNT,
	    PARM_DUMPON_COUNT_DEFAULT);

	if ((count++ % count_mod) != 0)
		return;

	DEBUG(0,("#####  Dumping Performance Counters #####\n"));

	for (i=0; i < MAX_OP; i++) {
		struct perfcount_test_counter *next;
		for (head = g_list[i]; head != NULL; head = next) {
			next = head->next;
			perfcount_test_dump_counter(head, 0);
			SAFE_FREE(head);
		}
		g_list[i] = NULL;
	}
}

/*  operations */
static void perfcount_test_start(struct smb_perfcount_data *pcd)
{
	struct perfcount_test_context *ctxt;
	struct perfcount_test_counter *ctr;
	/*
	 * there shouldn't already be a context here - if so,
	 * there's an unbalanced call to start / end.
	 */
	if (pcd->context) {
		DEBUG(0,("perfcount_test_start - starting "
			 "initialized context - %p\n", pcd));
		return;
	}

	ctxt = SMB_MALLOC_P(struct perfcount_test_context);
	if (!ctxt)
		return;

	ZERO_STRUCTP(ctxt);

	/* create 'default' context */
	ctr = SMB_MALLOC_P(struct perfcount_test_counter);
	if (!ctr) {
		SAFE_FREE(ctxt);
		return;
	}

	ZERO_STRUCTP(ctr);
	ctr->op = ctr->sub_op = ctr->ioctl = -1;
	DLIST_ADD(ctxt->ops, ctr);

	pcd->context = (void*)ctxt;
}

static void perfcount_test_add(struct smb_perfcount_data *pcd)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;
	struct perfcount_test_counter *ctr;

        if (pcd->context == NULL)
                return;

	ctr = SMB_MALLOC_P(struct perfcount_test_counter);
	if (!ctr) {
		return;
	}

	DLIST_ADD(ctxt->ops, ctr);

}

static void perfcount_test_set_op(struct smb_perfcount_data *pcd, int op)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;

        if (pcd->context == NULL)
                return;

	ctxt->ops->op = op;
}

static void perfcount_test_set_subop(struct smb_perfcount_data *pcd, int sub_op)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;

        if (pcd->context == NULL)
                return;

	ctxt->ops->sub_op = sub_op;
}

static void perfcount_test_set_ioctl(struct smb_perfcount_data *pcd, int io_ctl)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;
        if (pcd->context == NULL)
                return;

	ctxt->ops->ioctl = io_ctl;
}

static void perfcount_test_set_msglen_in(struct smb_perfcount_data *pcd,
					 uint64_t bytes_in)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;
        if (pcd->context == NULL)
                return;

	ctxt->ops->bytes_in = bytes_in;
}

static void perfcount_test_set_msglen_out(struct smb_perfcount_data *pcd,
					  uint64_t bytes_out)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;

        if (pcd->context == NULL)
                return;

	ctxt->ops->bytes_out = bytes_out;
}

static void perfcount_test_copy_context(struct smb_perfcount_data *pcd,
				        struct smb_perfcount_data *new_pcd)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;
	struct perfcount_test_context *new_ctxt;

	struct perfcount_test_counter *ctr;
	struct perfcount_test_counter *new_ctr;

        if (pcd->context == NULL)
                return;

	new_ctxt = SMB_MALLOC_P(struct perfcount_test_context);
	if (!new_ctxt) {
		return;
	}

	memcpy(new_ctxt, ctxt, sizeof(struct perfcount_test_context));

	for (ctr = ctxt->ops; ctr != NULL; ctr = ctr->next) {
		new_ctr = SMB_MALLOC_P(struct perfcount_test_counter);
		if (!new_ctr) {
			goto error;
		}

		memcpy(new_ctr, ctr, sizeof(struct perfcount_test_counter));
		new_ctr->next = NULL;
		new_ctr->prev = NULL;
		DLIST_ADD(new_ctxt->ops, new_ctr);
	}

	new_pcd->context = new_ctxt;
	return;

error:

	for (ctr = new_ctxt->ops; ctr != NULL; ) {
		new_ctr = ctr->next;
		SAFE_FREE(ctr);
		ctr = new_ctr;
	}

	SAFE_FREE(new_ctxt);
}

/*
 * For perf reasons, its best to use some global state
 * when an operation is deferred, we need to alloc a copy.
 */
static void perfcount_test_defer_op(struct smb_perfcount_data *pcd,
				    struct smb_perfcount_data *def_pcd)
{
	/* we don't do anything special to deferred ops */
	return;
}

static void perfcount_test_end(struct smb_perfcount_data *pcd)
{
	struct perfcount_test_context *ctxt =
		(struct perfcount_test_context *)pcd->context;
        if (pcd->context == NULL)
                return;

	/* @bug - we don't store outbytes right for chained cmds */
	perfcount_test_add_counters(ctxt);
	perfcount_test_dump_counters();
	pcd->context = NULL;
	SAFE_FREE(ctxt);
}


static struct smb_perfcount_handlers perfcount_test_handlers = {
	perfcount_test_start,
	perfcount_test_add,
	perfcount_test_set_op,
	perfcount_test_set_subop,
	perfcount_test_set_ioctl,
	perfcount_test_set_msglen_in,
	perfcount_test_set_msglen_out,
	perfcount_test_copy_context,
	perfcount_test_defer_op,
	perfcount_test_end
};

NTSTATUS perfcount_test_init(void)
{
	return smb_register_perfcounter(SMB_PERFCOUNTER_INTERFACE_VERSION,
					"pc_test", &perfcount_test_handlers);
}
