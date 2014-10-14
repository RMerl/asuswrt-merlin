/*
 * bnx2fc_els.c: Broadcom NetXtreme II Linux FCoE offload driver.
 * This file contains helper routines that handle ELS requests
 * and responses.
 *
 * Copyright (c) 2008 - 2010 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#include "bnx2fc.h"

static void bnx2fc_logo_resp(struct fc_seq *seq, struct fc_frame *fp,
			     void *arg);
static void bnx2fc_flogi_resp(struct fc_seq *seq, struct fc_frame *fp,
			      void *arg);
static int bnx2fc_initiate_els(struct bnx2fc_rport *tgt, unsigned int op,
			void *data, u32 data_len,
			void (*cb_func)(struct bnx2fc_els_cb_arg *cb_arg),
			struct bnx2fc_els_cb_arg *cb_arg, u32 timer_msec);

static void bnx2fc_rrq_compl(struct bnx2fc_els_cb_arg *cb_arg)
{
	struct bnx2fc_cmd *orig_io_req;
	struct bnx2fc_cmd *rrq_req;
	int rc = 0;

	BUG_ON(!cb_arg);
	rrq_req = cb_arg->io_req;
	orig_io_req = cb_arg->aborted_io_req;
	BUG_ON(!orig_io_req);
	BNX2FC_ELS_DBG("rrq_compl: orig xid = 0x%x, rrq_xid = 0x%x\n",
		   orig_io_req->xid, rrq_req->xid);

	kref_put(&orig_io_req->refcount, bnx2fc_cmd_release);

	if (test_and_clear_bit(BNX2FC_FLAG_ELS_TIMEOUT, &rrq_req->req_flags)) {
		/*
		 * els req is timed out. cleanup the IO with FW and
		 * drop the completion. Remove from active_cmd_queue.
		 */
		BNX2FC_ELS_DBG("rrq xid - 0x%x timed out, clean it up\n",
			   rrq_req->xid);

		if (rrq_req->on_active_queue) {
			list_del_init(&rrq_req->link);
			rrq_req->on_active_queue = 0;
			rc = bnx2fc_initiate_cleanup(rrq_req);
			BUG_ON(rc);
		}
	}
	kfree(cb_arg);
}
int bnx2fc_send_rrq(struct bnx2fc_cmd *aborted_io_req)
{

	struct fc_els_rrq rrq;
	struct bnx2fc_rport *tgt = aborted_io_req->tgt;
	struct fc_lport *lport = tgt->rdata->local_port;
	struct bnx2fc_els_cb_arg *cb_arg = NULL;
	u32 sid = tgt->sid;
	u32 r_a_tov = lport->r_a_tov;
	unsigned long start = jiffies;
	int rc;

	BNX2FC_ELS_DBG("Sending RRQ orig_xid = 0x%x\n",
		   aborted_io_req->xid);
	memset(&rrq, 0, sizeof(rrq));

	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_NOIO);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for RRQ\n");
		rc = -ENOMEM;
		goto rrq_err;
	}

	cb_arg->aborted_io_req = aborted_io_req;

	rrq.rrq_cmd = ELS_RRQ;
	hton24(rrq.rrq_s_id, sid);
	rrq.rrq_ox_id = htons(aborted_io_req->xid);
	rrq.rrq_rx_id = htons(aborted_io_req->task->rx_wr_tx_rd.rx_id);

retry_rrq:
	rc = bnx2fc_initiate_els(tgt, ELS_RRQ, &rrq, sizeof(rrq),
				 bnx2fc_rrq_compl, cb_arg,
				 r_a_tov);
	if (rc == -ENOMEM) {
		if (time_after(jiffies, start + (10 * HZ))) {
			BNX2FC_ELS_DBG("rrq Failed\n");
			rc = FAILED;
			goto rrq_err;
		}
		msleep(20);
		goto retry_rrq;
	}
rrq_err:
	if (rc) {
		BNX2FC_ELS_DBG("RRQ failed - release orig io req 0x%x\n",
			aborted_io_req->xid);
		kfree(cb_arg);
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&aborted_io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
	}
	return rc;
}

static void bnx2fc_l2_els_compl(struct bnx2fc_els_cb_arg *cb_arg)
{
	struct bnx2fc_cmd *els_req;
	struct bnx2fc_rport *tgt;
	struct bnx2fc_mp_req *mp_req;
	struct fc_frame_header *fc_hdr;
	unsigned char *buf;
	void *resp_buf;
	u32 resp_len, hdr_len;
	u16 l2_oxid;
	int frame_len;
	int rc = 0;

	l2_oxid = cb_arg->l2_oxid;
	BNX2FC_ELS_DBG("ELS COMPL - l2_oxid = 0x%x\n", l2_oxid);

	els_req = cb_arg->io_req;
	if (test_and_clear_bit(BNX2FC_FLAG_ELS_TIMEOUT, &els_req->req_flags)) {
		/*
		 * els req is timed out. cleanup the IO with FW and
		 * drop the completion. libfc will handle the els timeout
		 */
		if (els_req->on_active_queue) {
			list_del_init(&els_req->link);
			els_req->on_active_queue = 0;
			rc = bnx2fc_initiate_cleanup(els_req);
			BUG_ON(rc);
		}
		goto free_arg;
	}

	tgt = els_req->tgt;
	mp_req = &(els_req->mp_req);
	fc_hdr = &(mp_req->resp_fc_hdr);
	resp_len = mp_req->resp_len;
	resp_buf = mp_req->resp_buf;

	buf = kzalloc(PAGE_SIZE, GFP_ATOMIC);
	if (!buf) {
		printk(KERN_ERR PFX "Unable to alloc mp buf\n");
		goto free_arg;
	}
	hdr_len = sizeof(*fc_hdr);
	if (hdr_len + resp_len > PAGE_SIZE) {
		printk(KERN_ERR PFX "l2_els_compl: resp len is "
				    "beyond page size\n");
		goto free_buf;
	}
	memcpy(buf, fc_hdr, hdr_len);
	memcpy(buf + hdr_len, resp_buf, resp_len);
	frame_len = hdr_len + resp_len;

	bnx2fc_process_l2_frame_compl(tgt, buf, frame_len, l2_oxid);

free_buf:
	kfree(buf);
free_arg:
	kfree(cb_arg);
}

int bnx2fc_send_adisc(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_adisc *adisc;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	int rc;

	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for ADISC\n");
		return -ENOMEM;
	}

	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	BNX2FC_ELS_DBG("send ADISC: l2_oxid = 0x%x\n", cb_arg->l2_oxid);
	adisc = fc_frame_payload_get(fp, sizeof(*adisc));
	/* adisc is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_ADISC, adisc, sizeof(*adisc),
				 bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

int bnx2fc_send_logo(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_logo *logo;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	int rc;

	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for LOGO\n");
		return -ENOMEM;
	}

	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	BNX2FC_ELS_DBG("Send LOGO: l2_oxid = 0x%x\n", cb_arg->l2_oxid);
	logo = fc_frame_payload_get(fp, sizeof(*logo));
	/* logo is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_LOGO, logo, sizeof(*logo),
				 bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

int bnx2fc_send_rls(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_rls *rls;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	int rc;

	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for LOGO\n");
		return -ENOMEM;
	}

	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	rls = fc_frame_payload_get(fp, sizeof(*rls));
	/* rls is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_RLS, rls, sizeof(*rls),
				  bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

static int bnx2fc_initiate_els(struct bnx2fc_rport *tgt, unsigned int op,
			void *data, u32 data_len,
			void (*cb_func)(struct bnx2fc_els_cb_arg *cb_arg),
			struct bnx2fc_els_cb_arg *cb_arg, u32 timer_msec)
{
	struct fcoe_port *port = tgt->port;
	struct bnx2fc_hba *hba = port->priv;
	struct fc_rport *rport = tgt->rport;
	struct fc_lport *lport = port->lport;
	struct bnx2fc_cmd *els_req;
	struct bnx2fc_mp_req *mp_req;
	struct fc_frame_header *fc_hdr;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	int rc = 0;
	int task_idx, index;
	u32 did, sid;
	u16 xid;

	rc = fc_remote_port_chkready(rport);
	if (rc) {
		printk(KERN_ALERT PFX "els 0x%x: rport not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
	if (lport->state != LPORT_ST_READY || !(lport->link_up)) {
		printk(KERN_ALERT PFX "els 0x%x: link is not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
	if (!(test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) ||
	     (test_bit(BNX2FC_FLAG_EXPL_LOGO, &tgt->flags))) {
		printk(KERN_ERR PFX "els 0x%x: tgt not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
	els_req = bnx2fc_elstm_alloc(tgt, BNX2FC_ELS);
	if (!els_req) {
		rc = -ENOMEM;
		goto els_err;
	}

	els_req->sc_cmd = NULL;
	els_req->port = port;
	els_req->tgt = tgt;
	els_req->cb_func = cb_func;
	cb_arg->io_req = els_req;
	els_req->cb_arg = cb_arg;

	mp_req = (struct bnx2fc_mp_req *)&(els_req->mp_req);
	rc = bnx2fc_init_mp_req(els_req);
	if (rc == FAILED) {
		printk(KERN_ALERT PFX "ELS MP request init failed\n");
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		rc = -ENOMEM;
		goto els_err;
	} else {
		/* rc SUCCESS */
		rc = 0;
	}

	/* Set the data_xfer_len to the size of ELS payload */
	mp_req->req_len = data_len;
	els_req->data_xfer_len = mp_req->req_len;

	/* Fill ELS Payload */
	if ((op >= ELS_LS_RJT) && (op <= ELS_AUTH_ELS)) {
		memcpy(mp_req->req_buf, data, data_len);
	} else {
		printk(KERN_ALERT PFX "Invalid ELS op 0x%x\n", op);
		els_req->cb_func = NULL;
		els_req->cb_arg = NULL;
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		rc = -EINVAL;
	}

	if (rc)
		goto els_err;

	/* Fill FC header */
	fc_hdr = &(mp_req->req_fc_hdr);

	did = tgt->rport->port_id;
	sid = tgt->sid;

	__fc_fill_fc_hdr(fc_hdr, FC_RCTL_ELS_REQ, did, sid,
			   FC_TYPE_ELS, FC_FC_FIRST_SEQ | FC_FC_END_SEQ |
			   FC_FC_SEQ_INIT, 0);

	/* Obtain exchange id */
	xid = els_req->xid;
	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	/* Initialize task context for this IO request */
	task_page = (struct fcoe_task_ctx_entry *) hba->task_ctx[task_idx];
	task = &(task_page[index]);
	bnx2fc_init_mp_task(els_req, task);

	spin_lock_bh(&tgt->tgt_lock);

	if (!test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) {
		printk(KERN_ERR PFX "initiate_els.. session not ready\n");
		els_req->cb_func = NULL;
		els_req->cb_arg = NULL;
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return -EINVAL;
	}

	if (timer_msec)
		bnx2fc_cmd_timer_set(els_req, timer_msec);
	bnx2fc_add_2_sq(tgt, xid);

	els_req->on_active_queue = 1;
	list_add_tail(&els_req->link, &tgt->els_queue);

	/* Ring doorbell */
	bnx2fc_ring_doorbell(tgt);
	spin_unlock_bh(&tgt->tgt_lock);

els_err:
	return rc;
}

void bnx2fc_process_els_compl(struct bnx2fc_cmd *els_req,
			      struct fcoe_task_ctx_entry *task, u8 num_rq)
{
	struct bnx2fc_mp_req *mp_req;
	struct fc_frame_header *fc_hdr;
	u64 *hdr;
	u64 *temp_hdr;

	BNX2FC_ELS_DBG("Entered process_els_compl xid = 0x%x"
			"cmd_type = %d\n", els_req->xid, els_req->cmd_type);

	if (test_and_set_bit(BNX2FC_FLAG_ELS_DONE,
			     &els_req->req_flags)) {
		BNX2FC_ELS_DBG("Timer context finished processing this "
			   "els - 0x%x\n", els_req->xid);
		/* This IO doesn't receive cleanup completion */
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		return;
	}

	/* Cancel the timeout_work, as we received the response */
	if (cancel_delayed_work(&els_req->timeout_work))
		kref_put(&els_req->refcount,
			 bnx2fc_cmd_release); /* drop timer hold */

	if (els_req->on_active_queue) {
		list_del_init(&els_req->link);
		els_req->on_active_queue = 0;
	}

	mp_req = &(els_req->mp_req);
	fc_hdr = &(mp_req->resp_fc_hdr);

	hdr = (u64 *)fc_hdr;
	temp_hdr = (u64 *)
		&task->cmn.general.cmd_info.mp_fc_frame.fc_hdr;
	hdr[0] = cpu_to_be64(temp_hdr[0]);
	hdr[1] = cpu_to_be64(temp_hdr[1]);
	hdr[2] = cpu_to_be64(temp_hdr[2]);

	mp_req->resp_len = task->rx_wr_only.sgl_ctx.mul_sges.cur_sge_off;

	/* Parse ELS response */
	if ((els_req->cb_func) && (els_req->cb_arg)) {
		els_req->cb_func(els_req->cb_arg);
		els_req->cb_arg = NULL;
	}

	kref_put(&els_req->refcount, bnx2fc_cmd_release);
}

static void bnx2fc_flogi_resp(struct fc_seq *seq, struct fc_frame *fp,
			      void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	u8 *mac;
	struct fc_frame_header *fh;
	u8 op;

	if (IS_ERR(fp))
		goto done;

	mac = fr_cb(fp)->granted_mac;
	if (is_zero_ether_addr(mac)) {
		fh = fc_frame_header_get(fp);
		if (fh->fh_type != FC_TYPE_ELS) {
			printk(KERN_ERR PFX "bnx2fc_flogi_resp:"
				"fh_type != FC_TYPE_ELS\n");
			fc_frame_free(fp);
			return;
		}
		op = fc_frame_payload_op(fp);
		if (lport->vport) {
			if (op == ELS_LS_RJT) {
				printk(KERN_ERR PFX "bnx2fc_flogi_resp is LS_RJT\n");
				fc_vport_terminate(lport->vport);
				fc_frame_free(fp);
				return;
			}
		}
		if (fcoe_ctlr_recv_flogi(fip, lport, fp)) {
			fc_frame_free(fp);
			return;
		}
	}
	fip->update_mac(lport, mac);
done:
	fc_lport_flogi_resp(seq, fp, lport);
}

static void bnx2fc_logo_resp(struct fc_seq *seq, struct fc_frame *fp,
			     void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	static u8 zero_mac[ETH_ALEN] = { 0 };

	if (!IS_ERR(fp))
		fip->update_mac(lport, zero_mac);
	fc_lport_logo_resp(seq, fp, lport);
}

struct fc_seq *bnx2fc_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout)
{
	struct fcoe_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->priv;
	struct fcoe_ctlr *fip = &hba->ctlr;
	struct fc_frame_header *fh = fc_frame_header_get(fp);

	switch (op) {
	case ELS_FLOGI:
	case ELS_FDISC:
		return fc_elsct_send(lport, did, fp, op, bnx2fc_flogi_resp,
				     fip, timeout);
	case ELS_LOGO:
		/* only hook onto fabric logouts, not port logouts */
		if (ntoh24(fh->fh_d_id) != FC_FID_FLOGI)
			break;
		return fc_elsct_send(lport, did, fp, op, bnx2fc_logo_resp,
				     fip, timeout);
	}
	return fc_elsct_send(lport, did, fp, op, resp, arg, timeout);
}
