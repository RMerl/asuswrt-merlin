/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS protocol statistics / perfcounters
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
#include <sys/isi_stats_protocol.h>
#include <sys/isi_stats_client.h>
#include <sys/isi_stats_cifs.h>

extern struct current_user current_user;

struct onefs_op_counter {
	struct isp_op_delta iod;
	struct onefs_op_counter *next;
	struct onefs_op_counter *prev;
};

struct onefs_stats_context {
	bool alloced;
	struct isp_op_delta iod;

	/* ANDX commands stats stored here */
	struct onefs_op_counter *ops_chain;
};

const char *onefs_stat_debug(struct isp_op_delta *iod);

struct onefs_stats_context g_context;

static void onefs_smb_statistics_end(struct smb_perfcount_data *pcd);

struct isp_op_delta *onefs_stats_get_op_delta(struct onefs_stats_context *ctxt)
{
	/* operate on head of chain */
	if (ctxt->ops_chain) {
#ifdef ONEFS_PERF_DEBUG
		DEBUG(0,("************* CHAINED *****\n"));
#endif
		return &ctxt->ops_chain->iod;
	} else
		return &ctxt->iod;
}

/* statistics operations */
static void onefs_smb_statistics_start(struct smb_perfcount_data *pcd)
{

#ifdef ONEFS_PERF_DEBUG
	if (g_context.iod.op) {
		DEBUG(0,("**************** OP Collision! %s(%d) \n",
			onefs_stat_debug(&g_context.iod), g_context.iod.op));
	}

#endif

        ISP_OP_BEG(&g_context.iod, ISP_PROTO_CIFS, 0);

        if (g_context.iod.enabled)
		pcd->context = &g_context;
	else
		pcd->context = NULL;


}

static void onefs_smb_statistics_add(struct smb_perfcount_data *pcd)
{
	struct onefs_op_counter *oc;
	struct onefs_stats_context *ctxt = pcd->context;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	oc = SMB_MALLOC_P(struct onefs_op_counter);

	if (oc == NULL)
		return;

#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("*********** add chained op \n"));
#endif

	DLIST_ADD(ctxt->ops_chain, oc);
        ISP_OP_BEG(&oc->iod, ISP_PROTO_CIFS, 0);
}

static void onefs_smb_statistics_set_op(struct smb_perfcount_data *pcd, int op)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct isp_op_delta *iod;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	iod = onefs_stats_get_op_delta(ctxt);
	iod->op = isp_cifs_op_id(op);

#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("***********SET op %s(%d)\n", onefs_stat_debug(iod), op));
#endif
	/* no reply required */
	if (op == SMBntcancel)
		onefs_smb_statistics_end(pcd);

}

static void onefs_smb_statistics_set_subop(struct smb_perfcount_data *pcd,
					   int subop)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct isp_op_delta *iod;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	iod = onefs_stats_get_op_delta(ctxt);
	iod->op = isp_cifs_sub_op_id(iod->op, subop);

#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("********************  SET subop %s(%d)\n",
		onefs_stat_debug(iod), subop));
#endif

	/*
	 * finalize this one - we don't need to know when it
	 * is set, but its useful as a counter
	 */
	if (subop == NT_TRANSACT_NOTIFY_CHANGE)
		onefs_smb_statistics_end(pcd);
}

static void onefs_smb_statistics_set_ioctl(struct smb_perfcount_data *pcd,
					   int io_ctl)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct isp_op_delta *iod;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	/* we only monitor shadow copy */
	if (io_ctl != FSCTL_GET_SHADOW_COPY_DATA)
		return;

	iod = onefs_stats_get_op_delta(ctxt);
	iod->op = isp_cifs_sub_op_id(iod->op, ISP_CIFS_NTRN_IOCTL_FGSCD);
#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("********************  SET ioctl %s(%d)\n",
		onefs_stat_debug(iod), io_ctl));
#endif
}

static void onefs_smb_statistics_set_msglen_in(struct smb_perfcount_data *pcd,
					       uint64_t in_bytes)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct isp_op_delta *iod;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	iod = onefs_stats_get_op_delta(ctxt);
	iod->in_bytes = in_bytes;
}

static void onefs_smb_statistics_set_msglen_out(struct smb_perfcount_data *pcd,
					        uint64_t out_bytes)
{
	struct onefs_stats_context *ctxt = pcd->context;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	if (ctxt->ops_chain)
		ctxt->ops_chain->iod.out_bytes = out_bytes;

	ctxt->iod.out_bytes = out_bytes;
}

static int onefs_copy_perfcount_context(struct onefs_stats_context *ctxt,
					struct onefs_stats_context **dest)
{
	struct onefs_stats_context *new_ctxt;

	/* make an alloc'd copy of the data */
	new_ctxt = SMB_MALLOC_P(struct onefs_stats_context);
	if (!new_ctxt) {
		return -1;
	}

	memcpy(new_ctxt, ctxt, sizeof(struct onefs_stats_context));
	new_ctxt->alloced = True;
	*dest = new_ctxt;
	return 0;
}

static void onefs_smb_statistics_copy_context(struct smb_perfcount_data *pcd,
					      struct smb_perfcount_data *dest)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct onefs_stats_context *new_ctxt;
	int ret;

        /* not enabled */
        if (pcd->context == NULL)
                return;

#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("********  COPYING op %s(%d)\n",
		onefs_stat_debug(&ctxt->iod), ctxt->iod.op));
#endif

	ret = onefs_copy_perfcount_context(ctxt, &new_ctxt);
	if (ret)
		return;

	/* instrumentation */
	if (ctxt == &g_context)
		ZERO_STRUCT(g_context);

	dest->context = new_ctxt;
}

/*
 * For perf reasons, we usually use the global - sometimes, though,
 * when an operation is deferred, we need to alloc a copy.
 */
static void onefs_smb_statistics_defer_op(struct smb_perfcount_data *pcd,
				          struct smb_perfcount_data *def_pcd)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct onefs_stats_context *deferred_ctxt;
	int ret;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	/* already allocated ? */
	if (ctxt->alloced)
	{
		def_pcd->context = ctxt;
		pcd->context = NULL;
		return;
	}

#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("********  DEFERRING op %s(%d)\n",
		onefs_stat_debug(&ctxt->iod), ctxt->iod.op));
#endif

	ret = onefs_copy_perfcount_context(ctxt, &deferred_ctxt);
	if (ret)
		return;

	def_pcd->context = (void*) deferred_ctxt;

	/* instrumentation */
	if (ctxt == &g_context)
		ZERO_STRUCT(g_context);

	if (pcd != def_pcd)
		pcd->context = NULL;
}

static void onefs_smb_statistics_end(struct smb_perfcount_data *pcd)
{
	struct onefs_stats_context *ctxt = pcd->context;
	struct onefs_op_counter *tmp;
	uint64_t uid;

	static in_addr_t rem_addr = 0;
	static in_addr_t loc_addr = 0;

        /* not enabled */
        if (pcd->context == NULL)
                return;

	uid = current_user.ut.uid ? current_user.ut.uid : ISC_UNKNOWN_CLIENT_ID;

	/* get address info once, doesn't change for process */
	if (rem_addr == 0) {

#error Isilon, please remove this after testing the code below

		char *addr;

		addr = talloc_sub_basic(talloc_tos(), "", "", "%I");
		if (addr != NULL) {
			rem_addr = interpret_addr(addr);
			TALLOC_FREE(addr);
		} else {
			rem_addr = ISC_MASKED_ADDR;
		}

		addr = talloc_sub_basic(talloc_tos(), "", "", "%i");
		if (addr != NULL) {
			loc_addr = interpret_addr(addr);
			TALLOC_FREE(addr);
		} else {
			loc_addr = ISC_MASKED_ADDR;
		}
	}

	/*
	 * bug here - we aren't getting the outlens right,
	 * when dealing w/ chained requests.
	 */
	for (tmp = ctxt->ops_chain; tmp; tmp = tmp->next) {
		tmp->iod.out_bytes = ctxt->iod.out_bytes;
		isc_cookie_init(&tmp->iod.cookie, rem_addr, loc_addr, uid);
		ISP_OP_END(&tmp->iod);
#ifdef ONEFS_PERF_DEBUG
		DEBUG(0,("********  Finalized CHAIN op %s uid %llu in:%llu"
			", out:%llu\n",
			onefs_stat_debug(&tmp->iod), uid,
			tmp->iod.in_bytes, tmp->iod.out_bytes));
#endif
		SAFE_FREE(DLIST_PREV(tmp));
	}

	isc_cookie_init(&ctxt->iod.cookie, rem_addr, loc_addr, uid);
	ISP_OP_END(&ctxt->iod);
#ifdef ONEFS_PERF_DEBUG
	DEBUG(0,("********  Finalized op %s uid %llu in:%llu, out:%llu\n",
		onefs_stat_debug(&ctxt->iod), uid,
		ctxt->iod.in_bytes, ctxt->iod.out_bytes));
#endif

	if (ctxt->alloced)
		SAFE_FREE(ctxt);
	else
		ZERO_STRUCTP(ctxt);

	pcd->context = NULL;
}


static struct smb_perfcount_handlers onefs_pc_handlers = {
	onefs_smb_statistics_start,
	onefs_smb_statistics_add,
	onefs_smb_statistics_set_op,
	onefs_smb_statistics_set_subop,
	onefs_smb_statistics_set_ioctl,
	onefs_smb_statistics_set_msglen_in,
	onefs_smb_statistics_set_msglen_out,
	onefs_smb_statistics_copy_context,
	onefs_smb_statistics_defer_op,
	onefs_smb_statistics_end
};

NTSTATUS perfcount_onefs_init(void)
{
	return smb_register_perfcounter(SMB_PERFCOUNTER_INTERFACE_VERSION,
					"pc_onefs", &onefs_pc_handlers);
}

#ifdef ONEFS_PERF_DEBUG
/* debug helper */
struct op_debug {
	int type;
	const char *name;
};

struct op_debug op_debug_table[] = {
	{ 0x00, "mkdir"}, { 0x01, "rmdir"}, { 0x02, "open"}, { 0x03, "create"},
	{ 0x04, "close"}, { 0x05, "flush"}, { 0x06, "unlink"}, { 0x07, "mv"},
	{ 0x08, "getatr"}, { 0x09, "setatr"}, { 0x0a, "read"}, { 0x0b, "write"},
	{ 0x0c, "lock"}, { 0x0d, "unlock"}, { 0x0e, "ctemp"}, { 0x0f, "mknew"},
	{ 0x10, "chkpth"}, { 0x11, "exit"}, { 0x12, "lseek"}, { 0x13, "lockread"},
	{ 0x14, "writeunlock"}, { 0x1a, "readbraw"}, { 0x1b, "readbmpx"},
	{ 0x1c, "readbs"}, { 0x1d, "writebraw"}, { 0x1e, "writebmpx"},
	{ 0x1f, "writebs"}, { 0x20, "writec"}, { 0x22, "setattre"},
	{ 0x23, "getattre"}, { 0x24, "lockingx"}, { 0x25, "trans"},
	{ 0x26, "transs"}, { 0x27, "ioctl"}, { 0x28, "ioctls"}, { 0x29, "copy"},
	{ 0x2a, "move"}, { 0x2b, "echo"}, { 0x2c, "writeclose"}, { 0x2d, "openx"},
	{ 0x2e, "readx"}, { 0x2f, "writex"}, { 0x34, "findclose"},
	{ 0x35, "findnclose"}, { 0x70, "tcon"}, { 0x71, "tdis"},
	{ 0x72, "negprot"}, { 0x73, "sesssetupx"}, { 0x74, "ulogoffx"},
	{ 0x75, "tconx"}, { 0x80, "dskattr"}, { 0x81, "search"},
	{ 0x82, "ffirst"}, { 0x83, "funique"}, { 0x84, "fclose"},
	{ 0x400, "nttrans"},{ 0x500, "nttranss"},
	{ 0xa2, "ntcreatex"}, { 0xa4, "ntcancel"}, { 0xa5, "ntrename"},
	{ 0xc0, "splopen"}, { 0xc1, "splwr"}, { 0xc2, "splclose"},
	{ 0xc3, "splretq"}, { 0xd0, "sends"}, { 0xd1, "sendb"},
	{ 0xd2, "fwdname"}, { 0xd3, "cancelf"}, { 0xd4, "getmac"},
	{ 0xd5, "sendstrt"}, { 0xd6, "sendend"}, { 0xd7, "sendtxt"},
	{ ISP_CIFS_INVALID_OP, "unknown"},
	{ ISP_CIFS_TRNS2 + 0x00,  "trans2:open"},
	{ ISP_CIFS_TRNS2 + 0x01,  "trans2:findfirst"},
	{ ISP_CIFS_TRNS2 + 0x02,  "trans2:findnext"},
	{ ISP_CIFS_TRNS2 + 0x03,  "trans2:qfsinfo"},
	{ ISP_CIFS_TRNS2 + 0x04,  "trans2:setfsinfo"},
	{ ISP_CIFS_TRNS2 + 0x05,  "trans2:qpathinfo"},
	{ ISP_CIFS_TRNS2 + 0x06,  "trans2:setpathinfo"},
	{ ISP_CIFS_TRNS2 + 0x07,  "trans2:qfileinfo"},
	{ ISP_CIFS_TRNS2 + 0x08,  "trans2:setfileinfo"},
	{ ISP_CIFS_TRNS2 + 0x0a,  "trans2:ioctl"},
	{ ISP_CIFS_TRNS2 + 0x0b,  "trans2:findnotifyfirst"},
	{ ISP_CIFS_TRNS2 + 0x0c,  "trans2:findnotifynext"},
	{ ISP_CIFS_TRNS2 + 0x0d,  "trans2:mkdir"},
	{ ISP_CIFS_TRNS2 + 0x10,  "trans2:get_dfs_ref"},
	{ ISP_CIFS_TRNS2 + ISP_CIFS_SUBOP_UNKNOWN, "trans2:unknown"},
	{ ISP_CIFS_TRNSS2 +0x00, "transs2:open"},
	{ ISP_CIFS_TRNSS2 +0x01, "transs2:findfirst"},
	{ ISP_CIFS_TRNSS2 +0x02, "transs2:findnext"},
	{ ISP_CIFS_TRNSS2 +0x03, "transs2:qfsinfo"},
	{ ISP_CIFS_TRNSS2 +0x04, "transs2:setfsinfo"},
	{ ISP_CIFS_TRNSS2 +0x05, "transs2:qpathinfo"},
	{ ISP_CIFS_TRNSS2 +0x06, "transs2:setpathinfo"},
	{ ISP_CIFS_TRNSS2 +0x07, "transs2:qfileinfo"},
	{ ISP_CIFS_TRNSS2 +0x08, "transs2:setfileinfo"},
	{ ISP_CIFS_TRNSS2 +0x0a, "transs2:ioctl"},
	{ ISP_CIFS_TRNSS2 +0x0b, "transs2:findnotifyfirst"},
	{ ISP_CIFS_TRNSS2 +0x0c, "transs2:findnotifynext"},
	{ ISP_CIFS_TRNSS2 +0x0d, "transs2:mkdir"},
	{ ISP_CIFS_TRNSS2 +0x10, "transs2:get_dfs_referral"},
	{ ISP_CIFS_TRNSS2 + ISP_CIFS_SUBOP_UNKNOWN, "transs2:unknown"},
	{ ISP_CIFS_NTRNS + 0x1, "nttrans:create"},
	{ ISP_CIFS_NTRNS + 0x2, "nttrans:ioctl"},
	{ ISP_CIFS_NTRNS + 0x3, "nttrans:set_security_desc"},
	{ ISP_CIFS_NTRNS + 0x4, "nttrans:notify_change"},
	{ ISP_CIFS_NTRNS + 0x5, "nttrans:rename"},
	{ ISP_CIFS_NTRNS + 0x6, "nttrans:qry_security_desc"},
	{ ISP_CIFS_NTRNS + 0x7, "nttrans:get_user_quota"},
	{ ISP_CIFS_NTRNS + 0x8, "nttrans:set_user_quota"},
	{ ISP_CIFS_NTRNS + ISP_CIFS_NTRN_IOCTL_FGSCD,
		"nttrans:ioctl:get_shadow_copy_data"},
	{ ISP_CIFS_NTRNS + ISP_CIFS_SUBOP_UNKNOWN,
		"nttrans:unknown"},
	{ ISP_CIFS_NTRNSS + 0x1, "nttranss:create"},
	{ ISP_CIFS_NTRNSS + 0x2, "nttranss:ioctl"},
	{ ISP_CIFS_NTRNSS + 0x3, "nttranss:set_security_desc"},
	{ ISP_CIFS_NTRNSS + 0x4, "nttranss:notify_change"},
	{ ISP_CIFS_NTRNSS + 0x5, "nttranss:rename"},
	{ ISP_CIFS_NTRNSS + 0x6, "nttranss:qry_security_desc"},
	{ ISP_CIFS_NTRNSS + 0x7, "nttranss:get_user_quota"},
	{ ISP_CIFS_NTRNSS + 0x8, "nttranss:set_user_quota"},
	{ ISP_CIFS_NTRNSS + ISP_CIFS_NTRN_IOCTL_FGSCD,
		"nttranss:ioctl:get_shadow_copy_data"},
	{ ISP_CIFS_NTRNSS + ISP_CIFS_SUBOP_UNKNOWN,
		"nttranss:unknown"},
};

int op_debug_table_count = sizeof(op_debug_table) / sizeof(op_debug_table[0]);

const char *onefs_stat_debug(struct isp_op_delta *iod)
{
	int i;
	const char *unk = "unknown";
	for (i=0; i < op_debug_table_count;i++) {
		if (iod->op == op_debug_table[i].type)
			return op_debug_table[i].name;
	}

	return unk;
}
#endif
