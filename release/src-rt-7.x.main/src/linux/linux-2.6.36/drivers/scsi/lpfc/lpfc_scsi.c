/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_version.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"

#define LPFC_RESET_WAIT  2
#define LPFC_ABORT_WAIT  2

int _dump_buf_done;

static char *dif_op_str[] = {
	"SCSI_PROT_NORMAL",
	"SCSI_PROT_READ_INSERT",
	"SCSI_PROT_WRITE_STRIP",
	"SCSI_PROT_READ_STRIP",
	"SCSI_PROT_WRITE_INSERT",
	"SCSI_PROT_READ_PASS",
	"SCSI_PROT_WRITE_PASS",
};
static void
lpfc_release_scsi_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb);
static void
lpfc_release_scsi_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb);

static void
lpfc_debug_save_data(struct lpfc_hba *phba, struct scsi_cmnd *cmnd)
{
	void *src, *dst;
	struct scatterlist *sgde = scsi_sglist(cmnd);

	if (!_dump_buf_data) {
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9050 BLKGRD: ERROR %s _dump_buf_data is NULL\n",
				__func__);
		return;
	}


	if (!sgde) {
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9051 BLKGRD: ERROR: data scatterlist is null\n");
		return;
	}

	dst = (void *) _dump_buf_data;
	while (sgde) {
		src = sg_virt(sgde);
		memcpy(dst, src, sgde->length);
		dst += sgde->length;
		sgde = sg_next(sgde);
	}
}

static void
lpfc_debug_save_dif(struct lpfc_hba *phba, struct scsi_cmnd *cmnd)
{
	void *src, *dst;
	struct scatterlist *sgde = scsi_prot_sglist(cmnd);

	if (!_dump_buf_dif) {
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9052 BLKGRD: ERROR %s _dump_buf_data is NULL\n",
				__func__);
		return;
	}

	if (!sgde) {
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9053 BLKGRD: ERROR: prot scatterlist is null\n");
		return;
	}

	dst = _dump_buf_dif;
	while (sgde) {
		src = sg_virt(sgde);
		memcpy(dst, src, sgde->length);
		dst += sgde->length;
		sgde = sg_next(sgde);
	}
}

/**
 * lpfc_sli4_set_rsp_sgl_last - Set the last bit in the response sge.
 * @phba: Pointer to HBA object.
 * @lpfc_cmd: lpfc scsi command object pointer.
 *
 * This function is called from the lpfc_prep_task_mgmt_cmd function to
 * set the last bit in the response sge entry.
 **/
static void
lpfc_sli4_set_rsp_sgl_last(struct lpfc_hba *phba,
				struct lpfc_scsi_buf *lpfc_cmd)
{
	struct sli4_sge *sgl = (struct sli4_sge *)lpfc_cmd->fcp_bpl;
	if (sgl) {
		sgl += 1;
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
	}
}

/**
 * lpfc_update_stats - Update statistical data for the command completion
 * @phba: Pointer to HBA object.
 * @lpfc_cmd: lpfc scsi command object pointer.
 *
 * This function is called when there is a command completion and this
 * function updates the statistical data for the command completion.
 **/
static void
lpfc_update_stats(struct lpfc_hba *phba, struct  lpfc_scsi_buf *lpfc_cmd)
{
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *pnode = rdata->pnode;
	struct scsi_cmnd *cmd = lpfc_cmd->pCmd;
	unsigned long flags;
	struct Scsi_Host  *shost = cmd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	unsigned long latency;
	int i;

	if (cmd->result)
		return;

	latency = jiffies_to_msecs((long)jiffies - (long)lpfc_cmd->start_time);

	spin_lock_irqsave(shost->host_lock, flags);
	if (!vport->stat_data_enabled ||
		vport->stat_data_blocked ||
		!pnode->lat_data ||
		(phba->bucket_type == LPFC_NO_BUCKET)) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		return;
	}

	if (phba->bucket_type == LPFC_LINEAR_BUCKET) {
		i = (latency + phba->bucket_step - 1 - phba->bucket_base)/
			phba->bucket_step;
		/* check array subscript bounds */
		if (i < 0)
			i = 0;
		else if (i >= LPFC_MAX_BUCKET_COUNT)
			i = LPFC_MAX_BUCKET_COUNT - 1;
	} else {
		for (i = 0; i < LPFC_MAX_BUCKET_COUNT-1; i++)
			if (latency <= (phba->bucket_base +
				((1<<i)*phba->bucket_step)))
				break;
	}

	pnode->lat_data[i].cmd_count++;
	spin_unlock_irqrestore(shost->host_lock, flags);
}

/**
 * lpfc_send_sdev_queuedepth_change_event - Posts a queuedepth change event
 * @phba: Pointer to HBA context object.
 * @vport: Pointer to vport object.
 * @ndlp: Pointer to FC node associated with the target.
 * @lun: Lun number of the scsi device.
 * @old_val: Old value of the queue depth.
 * @new_val: New value of the queue depth.
 *
 * This function sends an event to the mgmt application indicating
 * there is a change in the scsi device queue depth.
 **/
static void
lpfc_send_sdev_queuedepth_change_event(struct lpfc_hba *phba,
		struct lpfc_vport  *vport,
		struct lpfc_nodelist *ndlp,
		uint32_t lun,
		uint32_t old_val,
		uint32_t new_val)
{
	struct lpfc_fast_path_event *fast_path_evt;
	unsigned long flags;

	fast_path_evt = lpfc_alloc_fast_evt(phba);
	if (!fast_path_evt)
		return;

	fast_path_evt->un.queue_depth_evt.scsi_event.event_type =
		FC_REG_SCSI_EVENT;
	fast_path_evt->un.queue_depth_evt.scsi_event.subcategory =
		LPFC_EVENT_VARQUEDEPTH;

	/* Report all luns with change in queue depth */
	fast_path_evt->un.queue_depth_evt.scsi_event.lun = lun;
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		memcpy(&fast_path_evt->un.queue_depth_evt.scsi_event.wwpn,
			&ndlp->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.queue_depth_evt.scsi_event.wwnn,
			&ndlp->nlp_nodename, sizeof(struct lpfc_name));
	}

	fast_path_evt->un.queue_depth_evt.oldval = old_val;
	fast_path_evt->un.queue_depth_evt.newval = new_val;
	fast_path_evt->vport = vport;

	fast_path_evt->work_evt.evt = LPFC_EVT_FASTPATH_MGMT_EVT;
	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&fast_path_evt->work_evt.evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);
	lpfc_worker_wake_up(phba);

	return;
}

/**
 * lpfc_change_queue_depth - Alter scsi device queue depth
 * @sdev: Pointer the scsi device on which to change the queue depth.
 * @qdepth: New queue depth to set the sdev to.
 * @reason: The reason for the queue depth change.
 *
 * This function is called by the midlayer and the LLD to alter the queue
 * depth for a scsi device. This function sets the queue depth to the new
 * value and sends an event out to log the queue depth change.
 **/
int
lpfc_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_rport_data *rdata;
	unsigned long new_queue_depth, old_queue_depth;

	old_queue_depth = sdev->queue_depth;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	new_queue_depth = sdev->queue_depth;
	rdata = sdev->hostdata;
	if (rdata)
		lpfc_send_sdev_queuedepth_change_event(phba, vport,
						       rdata->pnode, sdev->lun,
						       old_queue_depth,
						       new_queue_depth);
	return sdev->queue_depth;
}

/**
 * lpfc_rampdown_queue_depth - Post RAMP_DOWN_QUEUE event to worker thread
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called when there is resource error in driver or firmware.
 * This routine posts WORKER_RAMP_DOWN_QUEUE event for @phba. This routine
 * posts at most 1 event each second. This routine wakes up worker thread of
 * @phba to process WORKER_RAM_DOWN_EVENT event.
 *
 * This routine should be called with no lock held.
 **/
void
lpfc_rampdown_queue_depth(struct lpfc_hba *phba)
{
	unsigned long flags;
	uint32_t evt_posted;

	spin_lock_irqsave(&phba->hbalock, flags);
	atomic_inc(&phba->num_rsrc_err);
	phba->last_rsrc_error_time = jiffies;

	if ((phba->last_ramp_down_time + QUEUE_RAMP_DOWN_INTERVAL) > jiffies) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	phba->last_ramp_down_time = jiffies;

	spin_unlock_irqrestore(&phba->hbalock, flags);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	evt_posted = phba->pport->work_port_events & WORKER_RAMP_DOWN_QUEUE;
	if (!evt_posted)
		phba->pport->work_port_events |= WORKER_RAMP_DOWN_QUEUE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	if (!evt_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_rampup_queue_depth - Post RAMP_UP_QUEUE event for worker thread
 * @phba: The Hba for which this call is being executed.
 *
 * This routine post WORKER_RAMP_UP_QUEUE event for @phba vport. This routine
 * post at most 1 event every 5 minute after last_ramp_up_time or
 * last_rsrc_error_time.  This routine wakes up worker thread of @phba
 * to process WORKER_RAM_DOWN_EVENT event.
 *
 * This routine should be called with no lock held.
 **/
static inline void
lpfc_rampup_queue_depth(struct lpfc_vport  *vport,
			uint32_t queue_depth)
{
	unsigned long flags;
	struct lpfc_hba *phba = vport->phba;
	uint32_t evt_posted;
	atomic_inc(&phba->num_cmd_success);

	if (vport->cfg_lun_queue_depth <= queue_depth)
		return;
	spin_lock_irqsave(&phba->hbalock, flags);
	if (time_before(jiffies,
			phba->last_ramp_up_time + QUEUE_RAMP_UP_INTERVAL) ||
	    time_before(jiffies,
			phba->last_rsrc_error_time + QUEUE_RAMP_UP_INTERVAL)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}
	phba->last_ramp_up_time = jiffies;
	spin_unlock_irqrestore(&phba->hbalock, flags);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	evt_posted = phba->pport->work_port_events & WORKER_RAMP_UP_QUEUE;
	if (!evt_posted)
		phba->pport->work_port_events |= WORKER_RAMP_UP_QUEUE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	if (!evt_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_ramp_down_queue_handler - WORKER_RAMP_DOWN_QUEUE event handler
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called to  process WORKER_RAMP_DOWN_QUEUE event for worker
 * thread.This routine reduces queue depth for all scsi device on each vport
 * associated with @phba.
 **/
void
lpfc_ramp_down_queue_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	unsigned long new_queue_depth;
	unsigned long num_rsrc_err, num_cmd_success;
	int i;

	num_rsrc_err = atomic_read(&phba->num_rsrc_err);
	num_cmd_success = atomic_read(&phba->num_cmd_success);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				new_queue_depth =
					sdev->queue_depth * num_rsrc_err /
					(num_rsrc_err + num_cmd_success);
				if (!new_queue_depth)
					new_queue_depth = sdev->queue_depth - 1;
				else
					new_queue_depth = sdev->queue_depth -
								new_queue_depth;
				lpfc_change_queue_depth(sdev, new_queue_depth,
							SCSI_QDEPTH_DEFAULT);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	atomic_set(&phba->num_rsrc_err, 0);
	atomic_set(&phba->num_cmd_success, 0);
}

/**
 * lpfc_ramp_up_queue_handler - WORKER_RAMP_UP_QUEUE event handler
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called to  process WORKER_RAMP_UP_QUEUE event for worker
 * thread.This routine increases queue depth for all scsi device on each vport
 * associated with @phba by 1. This routine also sets @phba num_rsrc_err and
 * num_cmd_success to zero.
 **/
void
lpfc_ramp_up_queue_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				if (vports[i]->cfg_lun_queue_depth <=
				    sdev->queue_depth)
					continue;
				lpfc_change_queue_depth(sdev,
							sdev->queue_depth+1,
							SCSI_QDEPTH_RAMP_UP);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	atomic_set(&phba->num_rsrc_err, 0);
	atomic_set(&phba->num_cmd_success, 0);
}

/**
 * lpfc_scsi_dev_block - set all scsi hosts to block state
 * @phba: Pointer to HBA context object.
 *
 * This function walks vport list and set each SCSI host to block state
 * by invoking fc_remote_port_delete() routine. This function is invoked
 * with EEH when device's PCI slot has been permanently disabled.
 **/
void
lpfc_scsi_dev_block(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	struct fc_rport *rport;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				rport = starget_to_rport(scsi_target(sdev));
				fc_remote_port_delete(rport);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
}

/**
 * lpfc_new_scsi_buf_s3 - Scsi buffer allocator for HBA with SLI3 IF spec
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine allocates a scsi buffer for device with SLI-3 interface spec,
 * the scsi buffer contains all the necessary information needed to initiate
 * a SCSI I/O. The non-DMAable buffer region contains information to build
 * the IOCB. The DMAable region contains memory for the FCP CMND, FCP RSP,
 * and the initial BPL. In addition to allocating memory, the FCP CMND and
 * FCP RSP BDEs are setup in the BPL and the BPL BDE is setup in the IOCB.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static int
lpfc_new_scsi_buf_s3(struct lpfc_vport *vport, int num_to_alloc)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_scsi_buf *psb;
	struct ulp_bde64 *bpl;
	IOCB_t *iocb;
	dma_addr_t pdma_phys_fcp_cmd;
	dma_addr_t pdma_phys_fcp_rsp;
	dma_addr_t pdma_phys_bpl;
	uint16_t iotag;
	int bcnt;

	for (bcnt = 0; bcnt < num_to_alloc; bcnt++) {
		psb = kzalloc(sizeof(struct lpfc_scsi_buf), GFP_KERNEL);
		if (!psb)
			break;

		/*
		 * Get memory from the pci pool to map the virt space to pci
		 * bus space for an I/O.  The DMA buffer includes space for the
		 * struct fcp_cmnd, struct fcp_rsp and the number of bde's
		 * necessary to support the sg_tablesize.
		 */
		psb->data = pci_pool_alloc(phba->lpfc_scsi_dma_buf_pool,
					GFP_KERNEL, &psb->dma_handle);
		if (!psb->data) {
			kfree(psb);
			break;
		}

		/* Initialize virtual ptrs to dma_buf region. */
		memset(psb->data, 0, phba->cfg_sg_dma_buf_size);

		/* Allocate iotag for psb->cur_iocbq. */
		iotag = lpfc_sli_next_iotag(phba, &psb->cur_iocbq);
		if (iotag == 0) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
					psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}
		psb->cur_iocbq.iocb_flag |= LPFC_IO_FCP;

		psb->fcp_cmnd = psb->data;
		psb->fcp_rsp = psb->data + sizeof(struct fcp_cmnd);
		psb->fcp_bpl = psb->data + sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp);

		/* Initialize local short-hand pointers. */
		bpl = psb->fcp_bpl;
		pdma_phys_fcp_cmd = psb->dma_handle;
		pdma_phys_fcp_rsp = psb->dma_handle + sizeof(struct fcp_cmnd);
		pdma_phys_bpl = psb->dma_handle + sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp);

		/*
		 * The first two bdes are the FCP_CMD and FCP_RSP. The balance
		 * are sg list bdes.  Initialize the first two and leave the
		 * rest for queuecommand.
		 */
		bpl[0].addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys_fcp_cmd));
		bpl[0].addrLow = le32_to_cpu(putPaddrLow(pdma_phys_fcp_cmd));
		bpl[0].tus.f.bdeSize = sizeof(struct fcp_cmnd);
		bpl[0].tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl[0].tus.w = le32_to_cpu(bpl[0].tus.w);

		/* Setup the physical region for the FCP RSP */
		bpl[1].addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys_fcp_rsp));
		bpl[1].addrLow = le32_to_cpu(putPaddrLow(pdma_phys_fcp_rsp));
		bpl[1].tus.f.bdeSize = sizeof(struct fcp_rsp);
		bpl[1].tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl[1].tus.w = le32_to_cpu(bpl[1].tus.w);

		/*
		 * Since the IOCB for the FCP I/O is built into this
		 * lpfc_scsi_buf, initialize it with all known data now.
		 */
		iocb = &psb->cur_iocbq.iocb;
		iocb->un.fcpi64.bdl.ulpIoTag32 = 0;
		if ((phba->sli_rev == 3) &&
				!(phba->sli3_options & LPFC_SLI3_BG_ENABLED)) {
			/* fill in immediate fcp command BDE */
			iocb->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BDE_IMMED;
			iocb->un.fcpi64.bdl.bdeSize = sizeof(struct fcp_cmnd);
			iocb->un.fcpi64.bdl.addrLow = offsetof(IOCB_t,
					unsli3.fcp_ext.icd);
			iocb->un.fcpi64.bdl.addrHigh = 0;
			iocb->ulpBdeCount = 0;
			iocb->ulpLe = 0;
			/* fill in responce BDE */
			iocb->unsli3.fcp_ext.rbde.tus.f.bdeFlags =
							BUFF_TYPE_BDE_64;
			iocb->unsli3.fcp_ext.rbde.tus.f.bdeSize =
				sizeof(struct fcp_rsp);
			iocb->unsli3.fcp_ext.rbde.addrLow =
				putPaddrLow(pdma_phys_fcp_rsp);
			iocb->unsli3.fcp_ext.rbde.addrHigh =
				putPaddrHigh(pdma_phys_fcp_rsp);
		} else {
			iocb->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
			iocb->un.fcpi64.bdl.bdeSize =
					(2 * sizeof(struct ulp_bde64));
			iocb->un.fcpi64.bdl.addrLow =
					putPaddrLow(pdma_phys_bpl);
			iocb->un.fcpi64.bdl.addrHigh =
					putPaddrHigh(pdma_phys_bpl);
			iocb->ulpBdeCount = 1;
			iocb->ulpLe = 1;
		}
		iocb->ulpClass = CLASS3;
		psb->status = IOSTAT_SUCCESS;
		/* Put it back into the SCSI buffer list */
		lpfc_release_scsi_buf_s3(phba, psb);

	}

	return bcnt;
}

/**
 * lpfc_sli4_fcp_xri_aborted - Fast-path process of fcp xri abort
 * @phba: pointer to lpfc hba data structure.
 * @axri: pointer to the fcp xri abort wcqe structure.
 *
 * This routine is invoked by the worker thread to process a SLI4 fast-path
 * FCP aborted xri.
 **/
void
lpfc_sli4_fcp_xri_aborted(struct lpfc_hba *phba,
			  struct sli4_wcqe_xri_aborted *axri)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	struct lpfc_scsi_buf *psb, *next_psb;
	unsigned long iflag = 0;
	struct lpfc_iocbq *iocbq;
	int i;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irqsave(&phba->hbalock, iflag);
	spin_lock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	list_for_each_entry_safe(psb, next_psb,
		&phba->sli4_hba.lpfc_abts_scsi_buf_list, list) {
		if (psb->cur_iocbq.sli4_xritag == xri) {
			list_del(&psb->list);
			psb->exch_busy = 0;
			psb->status = IOSTAT_SUCCESS;
			spin_unlock(
				&phba->sli4_hba.abts_scsi_buf_list_lock);
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_release_scsi_buf_s4(phba, psb);
			return;
		}
	}
	spin_unlock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (!(iocbq->iocb_flag &  LPFC_IO_FCP) ||
			(iocbq->iocb_flag & LPFC_IO_LIBDFC))
			continue;
		if (iocbq->sli4_xritag != xri)
			continue;
		psb = container_of(iocbq, struct lpfc_scsi_buf, cur_iocbq);
		psb->exch_busy = 0;
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		if (pring->txq_cnt)
			lpfc_worker_wake_up(phba);
		return;

	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

/**
 * lpfc_sli4_repost_scsi_sgl_list - Repsot the Scsi buffers sgl pages as block
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine walks the list of scsi buffers that have been allocated and
 * repost them to the HBA by using SGL block post. This is needed after a
 * pci_function_reset/warm_start or start. The lpfc_hba_down_post_s4 routine
 * is responsible for moving all scsi buffers on the lpfc_abts_scsi_sgl_list
 * to the lpfc_scsi_buf_list. If the repost fails, reject all scsi buffers.
 *
 * Returns: 0 = success, non-zero failure.
 **/
int
lpfc_sli4_repost_scsi_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *psb;
	int index, status, bcnt = 0, rcnt = 0, rc = 0;
	LIST_HEAD(sblist);

	for (index = 0; index < phba->sli4_hba.scsi_xri_cnt; index++) {
		psb = phba->sli4_hba.lpfc_scsi_psb_array[index];
		if (psb) {
			/* Remove from SCSI buffer list */
			list_del(&psb->list);
			/* Add it to a local SCSI buffer list */
			list_add_tail(&psb->list, &sblist);
			if (++rcnt == LPFC_NEMBED_MBOX_SGL_CNT) {
				bcnt = rcnt;
				rcnt = 0;
			}
		} else
			/* A hole present in the XRI array, need to skip */
			bcnt = rcnt;

		if (index == phba->sli4_hba.scsi_xri_cnt - 1)
			/* End of XRI array for SCSI buffer, complete */
			bcnt = rcnt;

		/* Continue until collect up to a nembed page worth of sgls */
		if (bcnt == 0)
			continue;
		/* Now, post the SCSI buffer list sgls as a block */
		status = lpfc_sli4_post_scsi_sgl_block(phba, &sblist, bcnt);
		/* Reset SCSI buffer count for next round of posting */
		bcnt = 0;
		while (!list_empty(&sblist)) {
			list_remove_head(&sblist, psb, struct lpfc_scsi_buf,
					 list);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
				rc++;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
		}
	}
	return rc;
}

/**
 * lpfc_new_scsi_buf_s4 - Scsi buffer allocator for HBA with SLI4 IF spec
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine allocates a scsi buffer for device with SLI-4 interface spec,
 * the scsi buffer contains all the necessary information needed to initiate
 * a SCSI I/O.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static int
lpfc_new_scsi_buf_s4(struct lpfc_vport *vport, int num_to_alloc)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_scsi_buf *psb;
	struct sli4_sge *sgl;
	IOCB_t *iocb;
	dma_addr_t pdma_phys_fcp_cmd;
	dma_addr_t pdma_phys_fcp_rsp;
	dma_addr_t pdma_phys_bpl, pdma_phys_bpl1;
	uint16_t iotag, last_xritag = NO_XRI;
	int status = 0, index;
	int bcnt;
	int non_sequential_xri = 0;
	LIST_HEAD(sblist);

	for (bcnt = 0; bcnt < num_to_alloc; bcnt++) {
		psb = kzalloc(sizeof(struct lpfc_scsi_buf), GFP_KERNEL);
		if (!psb)
			break;

		/*
		 * Get memory from the pci pool to map the virt space to pci bus
		 * space for an I/O.  The DMA buffer includes space for the
		 * struct fcp_cmnd, struct fcp_rsp and the number of bde's
		 * necessary to support the sg_tablesize.
		 */
		psb->data = pci_pool_alloc(phba->lpfc_scsi_dma_buf_pool,
						GFP_KERNEL, &psb->dma_handle);
		if (!psb->data) {
			kfree(psb);
			break;
		}

		/* Initialize virtual ptrs to dma_buf region. */
		memset(psb->data, 0, phba->cfg_sg_dma_buf_size);

		/* Allocate iotag for psb->cur_iocbq. */
		iotag = lpfc_sli_next_iotag(phba, &psb->cur_iocbq);
		if (iotag == 0) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
				psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}

		psb->cur_iocbq.sli4_xritag = lpfc_sli4_next_xritag(phba);
		if (psb->cur_iocbq.sli4_xritag == NO_XRI) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
			      psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}
		if (last_xritag != NO_XRI
			&& psb->cur_iocbq.sli4_xritag != (last_xritag+1)) {
			non_sequential_xri = 1;
		} else
			list_add_tail(&psb->list, &sblist);
		last_xritag = psb->cur_iocbq.sli4_xritag;

		index = phba->sli4_hba.scsi_xri_cnt++;
		psb->cur_iocbq.iocb_flag |= LPFC_IO_FCP;

		psb->fcp_bpl = psb->data;
		psb->fcp_cmnd = (psb->data + phba->cfg_sg_dma_buf_size)
			- (sizeof(struct fcp_cmnd) + sizeof(struct fcp_rsp));
		psb->fcp_rsp = (struct fcp_rsp *)((uint8_t *)psb->fcp_cmnd +
					sizeof(struct fcp_cmnd));

		/* Initialize local short-hand pointers. */
		sgl = (struct sli4_sge *)psb->fcp_bpl;
		pdma_phys_bpl = psb->dma_handle;
		pdma_phys_fcp_cmd =
			(psb->dma_handle + phba->cfg_sg_dma_buf_size)
			 - (sizeof(struct fcp_cmnd) + sizeof(struct fcp_rsp));
		pdma_phys_fcp_rsp = pdma_phys_fcp_cmd + sizeof(struct fcp_cmnd);

		/*
		 * The first two bdes are the FCP_CMD and FCP_RSP.  The balance
		 * are sg list bdes.  Initialize the first two and leave the
		 * rest for queuecommand.
		 */
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(pdma_phys_fcp_cmd));
		sgl->addr_lo = cpu_to_le32(putPaddrLow(pdma_phys_fcp_cmd));
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(sizeof(struct fcp_cmnd));
		sgl++;

		/* Setup the physical region for the FCP RSP */
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(pdma_phys_fcp_rsp));
		sgl->addr_lo = cpu_to_le32(putPaddrLow(pdma_phys_fcp_rsp));
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(sizeof(struct fcp_rsp));

		/*
		 * Since the IOCB for the FCP I/O is built into this
		 * lpfc_scsi_buf, initialize it with all known data now.
		 */
		iocb = &psb->cur_iocbq.iocb;
		iocb->un.fcpi64.bdl.ulpIoTag32 = 0;
		iocb->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BDE_64;
		/* setting the BLP size to 2 * sizeof BDE may not be correct.
		 * We are setting the bpl to point to out sgl. An sgl's
		 * entries are 16 bytes, a bpl entries are 12 bytes.
		 */
		iocb->un.fcpi64.bdl.bdeSize = sizeof(struct fcp_cmnd);
		iocb->un.fcpi64.bdl.addrLow = putPaddrLow(pdma_phys_fcp_cmd);
		iocb->un.fcpi64.bdl.addrHigh = putPaddrHigh(pdma_phys_fcp_cmd);
		iocb->ulpBdeCount = 1;
		iocb->ulpLe = 1;
		iocb->ulpClass = CLASS3;
		if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
			pdma_phys_bpl1 = pdma_phys_bpl + SGL_PAGE_SIZE;
		else
			pdma_phys_bpl1 = 0;
		psb->dma_phys_bpl = pdma_phys_bpl;
		phba->sli4_hba.lpfc_scsi_psb_array[index] = psb;
		if (non_sequential_xri) {
			status = lpfc_sli4_post_sgl(phba, pdma_phys_bpl,
						pdma_phys_bpl1,
						psb->cur_iocbq.sli4_xritag);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
			break;
		}
	}
	if (bcnt) {
		status = lpfc_sli4_post_scsi_sgl_block(phba, &sblist, bcnt);
		/* Reset SCSI buffer count for next round of posting */
		while (!list_empty(&sblist)) {
			list_remove_head(&sblist, psb, struct lpfc_scsi_buf,
				 list);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
		}
	}

	return bcnt + non_sequential_xri;
}

/**
 * lpfc_new_scsi_buf - Wrapper funciton for scsi buffer allocator
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine wraps the actual SCSI buffer allocator function pointer from
 * the lpfc_hba struct.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static inline int
lpfc_new_scsi_buf(struct lpfc_vport *vport, int num_to_alloc)
{
	return vport->phba->lpfc_new_scsi_buf(vport, num_to_alloc);
}

/**
 * lpfc_get_scsi_buf - Get a scsi buffer from lpfc_scsi_buf_list of the HBA
 * @phba: The HBA for which this call is being executed.
 *
 * This routine removes a scsi buffer from head of @phba lpfc_scsi_buf_list list
 * and returns to caller.
 *
 * Return codes:
 *   NULL - Error
 *   Pointer to lpfc_scsi_buf - Success
 **/
static struct lpfc_scsi_buf*
lpfc_get_scsi_buf(struct lpfc_hba * phba)
{
	struct  lpfc_scsi_buf * lpfc_cmd = NULL;
	struct list_head *scsi_buf_list = &phba->lpfc_scsi_buf_list;
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	list_remove_head(scsi_buf_list, lpfc_cmd, struct lpfc_scsi_buf, list);
	if (lpfc_cmd) {
		lpfc_cmd->seg_cnt = 0;
		lpfc_cmd->nonsg_phys = 0;
		lpfc_cmd->prot_seg_cnt = 0;
	}
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	return  lpfc_cmd;
}

/**
 * lpfc_release_scsi_buf - Return a scsi buffer back to hba scsi buf list
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list.
 **/
static void
lpfc_release_scsi_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	psb->pCmd = NULL;
	list_add_tail(&psb->list, &phba->lpfc_scsi_buf_list);
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
}

/**
 * lpfc_release_scsi_buf_s4: Return a scsi buffer back to hba scsi buf list.
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list. For SLI4 XRI's are tied to the scsi buffer
 * and cannot be reused for at least RA_TOV amount of time if it was
 * aborted.
 **/
static void
lpfc_release_scsi_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	unsigned long iflag = 0;

	if (psb->exch_busy) {
		spin_lock_irqsave(&phba->sli4_hba.abts_scsi_buf_list_lock,
					iflag);
		psb->pCmd = NULL;
		list_add_tail(&psb->list,
			&phba->sli4_hba.lpfc_abts_scsi_buf_list);
		spin_unlock_irqrestore(&phba->sli4_hba.abts_scsi_buf_list_lock,
					iflag);
	} else {

		spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
		psb->pCmd = NULL;
		list_add_tail(&psb->list, &phba->lpfc_scsi_buf_list);
		spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	}
}

/**
 * lpfc_release_scsi_buf: Return a scsi buffer back to hba scsi buf list.
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list.
 **/
static void
lpfc_release_scsi_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{

	phba->lpfc_release_scsi_buf(phba, psb);
}

/**
 * lpfc_scsi_prep_dma_buf_s3 - DMA mapping for scsi buffer to SLI3 IF spec
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine does the pci dma mapping for scatter-gather list of scsi cmnd
 * field of @lpfc_cmd for device with SLI-3 interface spec. This routine scans
 * through sg elements and format the bdea. This routine also initializes all
 * IOCB fields which are dependent on scsi command request buffer.
 *
 * Return codes:
 *   1 - Error
 *   0 - Success
 **/
static int
lpfc_scsi_prep_dma_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct scatterlist *sgel = NULL;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	struct ulp_bde64 *bpl = lpfc_cmd->fcp_bpl;
	struct lpfc_iocbq *iocbq = &lpfc_cmd->cur_iocbq;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	struct ulp_bde64 *data_bde = iocb_cmd->unsli3.fcp_ext.dbde;
	dma_addr_t physaddr;
	uint32_t num_bde = 0;
	int nseg, datadir = scsi_cmnd->sc_data_direction;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	bpl += 2;
	if (scsi_sg_count(scsi_cmnd)) {
		/*
		 * The driver stores the segment count returned from pci_map_sg
		 * because this a count of dma-mappings used to map the use_sg
		 * pages.  They are not guaranteed to be the same for those
		 * architectures that implement an IOMMU.
		 */

		nseg = dma_map_sg(&phba->pcidev->dev, scsi_sglist(scsi_cmnd),
				  scsi_sg_count(scsi_cmnd), datadir);
		if (unlikely(!nseg))
			return 1;

		lpfc_cmd->seg_cnt = nseg;
		if (lpfc_cmd->seg_cnt > phba->cfg_sg_seg_cnt) {
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9064 BLKGRD: %s: Too many sg segments from "
			       "dma_map_sg.  Config %d, seg_cnt %d\n",
			       __func__, phba->cfg_sg_seg_cnt,
			       lpfc_cmd->seg_cnt);
			scsi_dma_unmap(scsi_cmnd);
			return 1;
		}

		/*
		 * The driver established a maximum scatter-gather segment count
		 * during probe that limits the number of sg elements in any
		 * single scsi command.  Just run through the seg_cnt and format
		 * the bde's.
		 * When using SLI-3 the driver will try to fit all the BDEs into
		 * the IOCB. If it can't then the BDEs get added to a BPL as it
		 * does for SLI-2 mode.
		 */
		scsi_for_each_sg(scsi_cmnd, sgel, nseg, num_bde) {
			physaddr = sg_dma_address(sgel);
			if (phba->sli_rev == 3 &&
			    !(phba->sli3_options & LPFC_SLI3_BG_ENABLED) &&
			    !(iocbq->iocb_flag & DSS_SECURITY_OP) &&
			    nseg <= LPFC_EXT_DATA_BDE_COUNT) {
				data_bde->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
				data_bde->tus.f.bdeSize = sg_dma_len(sgel);
				data_bde->addrLow = putPaddrLow(physaddr);
				data_bde->addrHigh = putPaddrHigh(physaddr);
				data_bde++;
			} else {
				bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
				bpl->tus.f.bdeSize = sg_dma_len(sgel);
				bpl->tus.w = le32_to_cpu(bpl->tus.w);
				bpl->addrLow =
					le32_to_cpu(putPaddrLow(physaddr));
				bpl->addrHigh =
					le32_to_cpu(putPaddrHigh(physaddr));
				bpl++;
			}
		}
	}

	/*
	 * Finish initializing those IOCB fields that are dependent on the
	 * scsi_cmnd request_buffer.  Note that for SLI-2 the bdeSize is
	 * explicitly reinitialized and for SLI-3 the extended bde count is
	 * explicitly reinitialized since all iocb memory resources are reused.
	 */
	if (phba->sli_rev == 3 &&
	    !(phba->sli3_options & LPFC_SLI3_BG_ENABLED) &&
	    !(iocbq->iocb_flag & DSS_SECURITY_OP)) {
		if (num_bde > LPFC_EXT_DATA_BDE_COUNT) {
			/*
			 * The extended IOCB format can only fit 3 BDE or a BPL.
			 * This I/O has more than 3 BDE so the 1st data bde will
			 * be a BPL that is filled in here.
			 */
			physaddr = lpfc_cmd->dma_handle;
			data_bde->tus.f.bdeFlags = BUFF_TYPE_BLP_64;
			data_bde->tus.f.bdeSize = (num_bde *
						   sizeof(struct ulp_bde64));
			physaddr += (sizeof(struct fcp_cmnd) +
				     sizeof(struct fcp_rsp) +
				     (2 * sizeof(struct ulp_bde64)));
			data_bde->addrHigh = putPaddrHigh(physaddr);
			data_bde->addrLow = putPaddrLow(physaddr);
			/* ebde count includes the responce bde and data bpl */
			iocb_cmd->unsli3.fcp_ext.ebde_count = 2;
		} else {
			/* ebde count includes the responce bde and data bdes */
			iocb_cmd->unsli3.fcp_ext.ebde_count = (num_bde + 1);
		}
	} else {
		iocb_cmd->un.fcpi64.bdl.bdeSize =
			((num_bde + 2) * sizeof(struct ulp_bde64));
		iocb_cmd->unsli3.fcp_ext.ebde_count = (num_bde + 1);
	}
	fcp_cmnd->fcpDl = cpu_to_be32(scsi_bufflen(scsi_cmnd));

	/*
	 * Due to difference in data length between DIF/non-DIF paths,
	 * we need to set word 4 of IOCB here
	 */
	iocb_cmd->un.fcpi.fcpi_parm = scsi_bufflen(scsi_cmnd);
	return 0;
}

/*
 * Given a scsi cmnd, determine the BlockGuard opcodes to be used with it
 * @sc: The SCSI command to examine
 * @txopt: (out) BlockGuard operation for transmitted data
 * @rxopt: (out) BlockGuard operation for received data
 *
 * Returns: zero on success; non-zero if tx and/or rx op cannot be determined
 *
 */
static int
lpfc_sc_to_bg_opcodes(struct lpfc_hba *phba, struct scsi_cmnd *sc,
		uint8_t *txop, uint8_t *rxop)
{
	uint8_t guard_type = scsi_host_get_guard(sc->device->host);
	uint8_t ret = 0;

	if (guard_type == SHOST_DIX_GUARD_IP) {
		switch (scsi_get_prot_op(sc)) {
		case SCSI_PROT_READ_INSERT:
		case SCSI_PROT_WRITE_STRIP:
			*txop = BG_OP_IN_CSUM_OUT_NODIF;
			*rxop = BG_OP_IN_NODIF_OUT_CSUM;
			break;

		case SCSI_PROT_READ_STRIP:
		case SCSI_PROT_WRITE_INSERT:
			*txop = BG_OP_IN_NODIF_OUT_CRC;
			*rxop = BG_OP_IN_CRC_OUT_NODIF;
			break;

		case SCSI_PROT_READ_PASS:
		case SCSI_PROT_WRITE_PASS:
			*txop = BG_OP_IN_CSUM_OUT_CRC;
			*rxop = BG_OP_IN_CRC_OUT_CSUM;
			break;

		case SCSI_PROT_NORMAL:
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9063 BLKGRD: Bad op/guard:%d/%d combination\n",
					scsi_get_prot_op(sc), guard_type);
			ret = 1;
			break;

		}
	} else if (guard_type == SHOST_DIX_GUARD_CRC) {
		switch (scsi_get_prot_op(sc)) {
		case SCSI_PROT_READ_STRIP:
		case SCSI_PROT_WRITE_INSERT:
			*txop = BG_OP_IN_NODIF_OUT_CRC;
			*rxop = BG_OP_IN_CRC_OUT_NODIF;
			break;

		case SCSI_PROT_READ_PASS:
		case SCSI_PROT_WRITE_PASS:
			*txop = BG_OP_IN_CRC_OUT_CRC;
			*rxop = BG_OP_IN_CRC_OUT_CRC;
			break;

		case SCSI_PROT_READ_INSERT:
		case SCSI_PROT_WRITE_STRIP:
		case SCSI_PROT_NORMAL:
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9075 BLKGRD: Bad op/guard:%d/%d combination\n",
					scsi_get_prot_op(sc), guard_type);
			ret = 1;
			break;
		}
	} else {
		/* unsupported format */
		BUG();
	}

	return ret;
}

struct scsi_dif_tuple {
	__be16 guard_tag;       /* Checksum */
	__be16 app_tag;         /* Opaque storage */
	__be32 ref_tag;         /* Target LBA or indirect LBA */
};

static inline unsigned
lpfc_cmd_blksize(struct scsi_cmnd *sc)
{
	return sc->device->sector_size;
}

/**
 * lpfc_get_cmd_dif_parms - Extract DIF parameters from SCSI command
 * @sc:             in: SCSI command
 * @apptagmask:     out: app tag mask
 * @apptagval:      out: app tag value
 * @reftag:         out: ref tag (reference tag)
 *
 * Description:
 *   Extract DIF parameters from the command if possible.  Otherwise,
 *   use default parameters.
 *
 **/
static inline void
lpfc_get_cmd_dif_parms(struct scsi_cmnd *sc, uint16_t *apptagmask,
		uint16_t *apptagval, uint32_t *reftag)
{
	struct  scsi_dif_tuple *spt;
	unsigned char op = scsi_get_prot_op(sc);
	unsigned int protcnt = scsi_prot_sg_count(sc);
	static int cnt;

	if (protcnt && (op == SCSI_PROT_WRITE_STRIP ||
				op == SCSI_PROT_WRITE_PASS)) {

		cnt++;
		spt = page_address(sg_page(scsi_prot_sglist(sc))) +
			scsi_prot_sglist(sc)[0].offset;
		*apptagmask = 0;
		*apptagval = 0;
		*reftag = cpu_to_be32(spt->ref_tag);

	} else {
		/* SBC defines ref tag to be lower 32bits of LBA */
		*reftag = (uint32_t) (0xffffffff & scsi_get_lba(sc));
		*apptagmask = 0;
		*apptagval = 0;
	}
}

/*
 * This function sets up buffer list for protection groups of
 * type LPFC_PG_TYPE_NO_DIF
 *
 * This is usually used when the HBA is instructed to generate
 * DIFs and insert them into data stream (or strip DIF from
 * incoming data stream)
 *
 * The buffer list consists of just one protection group described
 * below:
 *                                +-------------------------+
 *   start of prot group  -->     |          PDE_5          |
 *                                +-------------------------+
 *                                |          PDE_6          |
 *                                +-------------------------+
 *                                |         Data BDE        |
 *                                +-------------------------+
 *                                |more Data BDE's ... (opt)|
 *                                +-------------------------+
 *
 * @sc: pointer to scsi command we're working on
 * @bpl: pointer to buffer list for protection groups
 * @datacnt: number of segments of data that have been dma mapped
 *
 * Note: Data s/g buffers have been dma mapped
 */
static int
lpfc_bg_setup_bpl(struct lpfc_hba *phba, struct scsi_cmnd *sc,
		struct ulp_bde64 *bpl, int datasegcnt)
{
	struct scatterlist *sgde = NULL; /* s/g data entry */
	struct lpfc_pde5 *pde5 = NULL;
	struct lpfc_pde6 *pde6 = NULL;
	dma_addr_t physaddr;
	int i = 0, num_bde = 0, status;
	int datadir = sc->sc_data_direction;
	unsigned blksize;
	uint32_t reftag;
	uint16_t apptagmask, apptagval;
	uint8_t txop, rxop;

	status  = lpfc_sc_to_bg_opcodes(phba, sc, &txop, &rxop);
	if (status)
		goto out;

	/* extract some info from the scsi command for pde*/
	blksize = lpfc_cmd_blksize(sc);
	lpfc_get_cmd_dif_parms(sc, &apptagmask, &apptagval, &reftag);

	/* setup PDE5 with what we have */
	pde5 = (struct lpfc_pde5 *) bpl;
	memset(pde5, 0, sizeof(struct lpfc_pde5));
	bf_set(pde5_type, pde5, LPFC_PDE5_DESCRIPTOR);
	pde5->reftag = reftag;

	/* Endianness conversion if necessary for PDE5 */
	pde5->word0 = cpu_to_le32(pde5->word0);
	pde5->reftag = cpu_to_le32(pde5->reftag);

	/* advance bpl and increment bde count */
	num_bde++;
	bpl++;
	pde6 = (struct lpfc_pde6 *) bpl;

	/* setup PDE6 with the rest of the info */
	memset(pde6, 0, sizeof(struct lpfc_pde6));
	bf_set(pde6_type, pde6, LPFC_PDE6_DESCRIPTOR);
	bf_set(pde6_optx, pde6, txop);
	bf_set(pde6_oprx, pde6, rxop);
	if (datadir == DMA_FROM_DEVICE) {
		bf_set(pde6_ce, pde6, 1);
		bf_set(pde6_re, pde6, 1);
		bf_set(pde6_ae, pde6, 1);
	}
	bf_set(pde6_ai, pde6, 1);
	bf_set(pde6_apptagval, pde6, apptagval);

	/* Endianness conversion if necessary for PDE6 */
	pde6->word0 = cpu_to_le32(pde6->word0);
	pde6->word1 = cpu_to_le32(pde6->word1);
	pde6->word2 = cpu_to_le32(pde6->word2);

	/* advance bpl and increment bde count */
	num_bde++;
	bpl++;

	/* assumption: caller has already run dma_map_sg on command data */
	scsi_for_each_sg(sc, sgde, datasegcnt, i) {
		physaddr = sg_dma_address(sgde);
		bpl->addrLow = le32_to_cpu(putPaddrLow(physaddr));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		bpl->tus.f.bdeSize = sg_dma_len(sgde);
		if (datadir == DMA_TO_DEVICE)
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		else
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
		bpl++;
		num_bde++;
	}

out:
	return num_bde;
}

/*
 * This function sets up buffer list for protection groups of
 * type LPFC_PG_TYPE_DIF_BUF
 *
 * This is usually used when DIFs are in their own buffers,
 * separate from the data. The HBA can then by instructed
 * to place the DIFs in the outgoing stream.  For read operations,
 * The HBA could extract the DIFs and place it in DIF buffers.
 *
 * The buffer list for this type consists of one or more of the
 * protection groups described below:
 *                                    +-------------------------+
 *   start of first prot group  -->   |          PDE_5          |
 *                                    +-------------------------+
 *                                    |          PDE_6          |
 *                                    +-------------------------+
 *                                    |      PDE_7 (Prot BDE)   |
 *                                    +-------------------------+
 *                                    |        Data BDE         |
 *                                    +-------------------------+
 *                                    |more Data BDE's ... (opt)|
 *                                    +-------------------------+
 *   start of new  prot group  -->    |          PDE_5          |
 *                                    +-------------------------+
 *                                    |          ...            |
 *                                    +-------------------------+
 *
 * @sc: pointer to scsi command we're working on
 * @bpl: pointer to buffer list for protection groups
 * @datacnt: number of segments of data that have been dma mapped
 * @protcnt: number of segment of protection data that have been dma mapped
 *
 * Note: It is assumed that both data and protection s/g buffers have been
 *       mapped for DMA
 */
static int
lpfc_bg_setup_bpl_prot(struct lpfc_hba *phba, struct scsi_cmnd *sc,
		struct ulp_bde64 *bpl, int datacnt, int protcnt)
{
	struct scatterlist *sgde = NULL; /* s/g data entry */
	struct scatterlist *sgpe = NULL; /* s/g prot entry */
	struct lpfc_pde5 *pde5 = NULL;
	struct lpfc_pde6 *pde6 = NULL;
	struct ulp_bde64 *prot_bde = NULL;
	dma_addr_t dataphysaddr, protphysaddr;
	unsigned short curr_data = 0, curr_prot = 0;
	unsigned int split_offset, protgroup_len;
	unsigned int protgrp_blks, protgrp_bytes;
	unsigned int remainder, subtotal;
	int status;
	int datadir = sc->sc_data_direction;
	unsigned char pgdone = 0, alldone = 0;
	unsigned blksize;
	uint32_t reftag;
	uint16_t apptagmask, apptagval;
	uint8_t txop, rxop;
	int num_bde = 0;

	sgpe = scsi_prot_sglist(sc);
	sgde = scsi_sglist(sc);

	if (!sgpe || !sgde) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
				"9020 Invalid s/g entry: data=0x%p prot=0x%p\n",
				sgpe, sgde);
		return 0;
	}

	status = lpfc_sc_to_bg_opcodes(phba, sc, &txop, &rxop);
	if (status)
		goto out;

	/* extract some info from the scsi command */
	blksize = lpfc_cmd_blksize(sc);
	lpfc_get_cmd_dif_parms(sc, &apptagmask, &apptagval, &reftag);

	split_offset = 0;
	do {
		/* setup PDE5 with what we have */
		pde5 = (struct lpfc_pde5 *) bpl;
		memset(pde5, 0, sizeof(struct lpfc_pde5));
		bf_set(pde5_type, pde5, LPFC_PDE5_DESCRIPTOR);
		pde5->reftag = reftag;

		/* Endianness conversion if necessary for PDE5 */
		pde5->word0 = cpu_to_le32(pde5->word0);
		pde5->reftag = cpu_to_le32(pde5->reftag);

		/* advance bpl and increment bde count */
		num_bde++;
		bpl++;
		pde6 = (struct lpfc_pde6 *) bpl;

		/* setup PDE6 with the rest of the info */
		memset(pde6, 0, sizeof(struct lpfc_pde6));
		bf_set(pde6_type, pde6, LPFC_PDE6_DESCRIPTOR);
		bf_set(pde6_optx, pde6, txop);
		bf_set(pde6_oprx, pde6, rxop);
		bf_set(pde6_ce, pde6, 1);
		bf_set(pde6_re, pde6, 1);
		bf_set(pde6_ae, pde6, 1);
		bf_set(pde6_ai, pde6, 1);
		bf_set(pde6_apptagval, pde6, apptagval);

		/* Endianness conversion if necessary for PDE6 */
		pde6->word0 = cpu_to_le32(pde6->word0);
		pde6->word1 = cpu_to_le32(pde6->word1);
		pde6->word2 = cpu_to_le32(pde6->word2);

		/* advance bpl and increment bde count */
		num_bde++;
		bpl++;

		/* setup the first BDE that points to protection buffer */
		prot_bde = (struct ulp_bde64 *) bpl;
		protphysaddr = sg_dma_address(sgpe);
		prot_bde->addrHigh = le32_to_cpu(putPaddrLow(protphysaddr));
		prot_bde->addrLow = le32_to_cpu(putPaddrHigh(protphysaddr));
		protgroup_len = sg_dma_len(sgpe);

		/* must be integer multiple of the DIF block length */
		BUG_ON(protgroup_len % 8);

		protgrp_blks = protgroup_len / 8;
		protgrp_bytes = protgrp_blks * blksize;

		prot_bde->tus.f.bdeSize = protgroup_len;
		prot_bde->tus.f.bdeFlags = LPFC_PDE7_DESCRIPTOR;
		prot_bde->tus.w = le32_to_cpu(bpl->tus.w);

		curr_prot++;
		num_bde++;

		/* setup BDE's for data blocks associated with DIF data */
		pgdone = 0;
		subtotal = 0; /* total bytes processed for current prot grp */
		while (!pgdone) {
			if (!sgde) {
				lpfc_printf_log(phba, KERN_ERR, LOG_BG,
					"9065 BLKGRD:%s Invalid data segment\n",
						__func__);
				return 0;
			}
			bpl++;
			dataphysaddr = sg_dma_address(sgde) + split_offset;
			bpl->addrLow = le32_to_cpu(putPaddrLow(dataphysaddr));
			bpl->addrHigh = le32_to_cpu(putPaddrHigh(dataphysaddr));

			remainder = sg_dma_len(sgde) - split_offset;

			if ((subtotal + remainder) <= protgrp_bytes) {
				/* we can use this whole buffer */
				bpl->tus.f.bdeSize = remainder;
				split_offset = 0;

				if ((subtotal + remainder) == protgrp_bytes)
					pgdone = 1;
			} else {
				/* must split this buffer with next prot grp */
				bpl->tus.f.bdeSize = protgrp_bytes - subtotal;
				split_offset += bpl->tus.f.bdeSize;
			}

			subtotal += bpl->tus.f.bdeSize;

			if (datadir == DMA_TO_DEVICE)
				bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
			else
				bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
			bpl->tus.w = le32_to_cpu(bpl->tus.w);

			num_bde++;
			curr_data++;

			if (split_offset)
				break;

			/* Move to the next s/g segment if possible */
			sgde = sg_next(sgde);

		}

		/* are we done ? */
		if (curr_prot == protcnt) {
			alldone = 1;
		} else if (curr_prot < protcnt) {
			/* advance to next prot buffer */
			sgpe = sg_next(sgpe);
			bpl++;

			/* update the reference tag */
			reftag += protgrp_blks;
		} else {
			/* if we're here, we have a bug */
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9054 BLKGRD: bug in %s\n", __func__);
		}

	} while (!alldone);

out:

	return num_bde;
}
/*
 * Given a SCSI command that supports DIF, determine composition of protection
 * groups involved in setting up buffer lists
 *
 * Returns:
 *			      for DIF (for both read and write)
 * */
static int
lpfc_prot_group_type(struct lpfc_hba *phba, struct scsi_cmnd *sc)
{
	int ret = LPFC_PG_TYPE_INVALID;
	unsigned char op = scsi_get_prot_op(sc);

	switch (op) {
	case SCSI_PROT_READ_STRIP:
	case SCSI_PROT_WRITE_INSERT:
		ret = LPFC_PG_TYPE_NO_DIF;
		break;
	case SCSI_PROT_READ_INSERT:
	case SCSI_PROT_WRITE_STRIP:
	case SCSI_PROT_READ_PASS:
	case SCSI_PROT_WRITE_PASS:
		ret = LPFC_PG_TYPE_DIF_BUF;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
				"9021 Unsupported protection op:%d\n", op);
		break;
	}

	return ret;
}

/*
 * This is the protection/DIF aware version of
 * lpfc_scsi_prep_dma_buf(). It may be a good idea to combine the
 * two functions eventually, but for now, it's here
 */
static int
lpfc_bg_scsi_prep_dma_buf(struct lpfc_hba *phba,
		struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	struct ulp_bde64 *bpl = lpfc_cmd->fcp_bpl;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	uint32_t num_bde = 0;
	int datasegcnt, protsegcnt, datadir = scsi_cmnd->sc_data_direction;
	int prot_group_type = 0;
	int diflen, fcpdl;
	unsigned blksize;

	/*
	 * Start the lpfc command prep by bumping the bpl beyond fcp_cmnd
	 *  fcp_rsp regions to the first data bde entry
	 */
	bpl += 2;
	if (scsi_sg_count(scsi_cmnd)) {
		/*
		 * The driver stores the segment count returned from pci_map_sg
		 * because this a count of dma-mappings used to map the use_sg
		 * pages.  They are not guaranteed to be the same for those
		 * architectures that implement an IOMMU.
		 */
		datasegcnt = dma_map_sg(&phba->pcidev->dev,
					scsi_sglist(scsi_cmnd),
					scsi_sg_count(scsi_cmnd), datadir);
		if (unlikely(!datasegcnt))
			return 1;

		lpfc_cmd->seg_cnt = datasegcnt;
		if (lpfc_cmd->seg_cnt > phba->cfg_sg_seg_cnt) {
			lpfc_printf_log(phba, KERN_ERR, LOG_BG,
					"9067 BLKGRD: %s: Too many sg segments"
					" from dma_map_sg.  Config %d, seg_cnt"
					" %d\n",
					__func__, phba->cfg_sg_seg_cnt,
					lpfc_cmd->seg_cnt);
			scsi_dma_unmap(scsi_cmnd);
			return 1;
		}

		prot_group_type = lpfc_prot_group_type(phba, scsi_cmnd);

		switch (prot_group_type) {
		case LPFC_PG_TYPE_NO_DIF:
			num_bde = lpfc_bg_setup_bpl(phba, scsi_cmnd, bpl,
					datasegcnt);
			/* we should have 2 or more entries in buffer list */
			if (num_bde < 2)
				goto err;
			break;
		case LPFC_PG_TYPE_DIF_BUF:{
			/*
			 * This type indicates that protection buffers are
			 * passed to the driver, so that needs to be prepared
			 * for DMA
			 */
			protsegcnt = dma_map_sg(&phba->pcidev->dev,
					scsi_prot_sglist(scsi_cmnd),
					scsi_prot_sg_count(scsi_cmnd), datadir);
			if (unlikely(!protsegcnt)) {
				scsi_dma_unmap(scsi_cmnd);
				return 1;
			}

			lpfc_cmd->prot_seg_cnt = protsegcnt;
			if (lpfc_cmd->prot_seg_cnt
			    > phba->cfg_prot_sg_seg_cnt) {
				lpfc_printf_log(phba, KERN_ERR, LOG_BG,
					"9068 BLKGRD: %s: Too many prot sg "
					"segments from dma_map_sg.  Config %d,"
						"prot_seg_cnt %d\n", __func__,
						phba->cfg_prot_sg_seg_cnt,
						lpfc_cmd->prot_seg_cnt);
				dma_unmap_sg(&phba->pcidev->dev,
					     scsi_prot_sglist(scsi_cmnd),
					     scsi_prot_sg_count(scsi_cmnd),
					     datadir);
				scsi_dma_unmap(scsi_cmnd);
				return 1;
			}

			num_bde = lpfc_bg_setup_bpl_prot(phba, scsi_cmnd, bpl,
					datasegcnt, protsegcnt);
			/* we should have 3 or more entries in buffer list */
			if (num_bde < 3)
				goto err;
			break;
		}
		case LPFC_PG_TYPE_INVALID:
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"9022 Unexpected protection group %i\n",
					prot_group_type);
			return 1;
		}
	}

	/*
	 * Finish initializing those IOCB fields that are dependent on the
	 * scsi_cmnd request_buffer.  Note that the bdeSize is explicitly
	 * reinitialized since all iocb memory resources are used many times
	 * for transmit, receive, and continuation bpl's.
	 */
	iocb_cmd->un.fcpi64.bdl.bdeSize = (2 * sizeof(struct ulp_bde64));
	iocb_cmd->un.fcpi64.bdl.bdeSize += (num_bde * sizeof(struct ulp_bde64));
	iocb_cmd->ulpBdeCount = 1;
	iocb_cmd->ulpLe = 1;

	fcpdl = scsi_bufflen(scsi_cmnd);

	if (scsi_get_prot_type(scsi_cmnd) == SCSI_PROT_DIF_TYPE1) {
		/*
		 * We are in DIF Type 1 mode
		 * Every data block has a 8 byte DIF (trailer)
		 * attached to it.  Must ajust FCP data length
		 */
		blksize = lpfc_cmd_blksize(scsi_cmnd);
		diflen = (fcpdl / blksize) * 8;
		fcpdl += diflen;
	}
	fcp_cmnd->fcpDl = be32_to_cpu(fcpdl);

	/*
	 * Due to difference in data length between DIF/non-DIF paths,
	 * we need to set word 4 of IOCB here
	 */
	iocb_cmd->un.fcpi.fcpi_parm = fcpdl;

	return 0;
err:
	lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
			"9023 Could not setup all needed BDE's"
			"prot_group_type=%d, num_bde=%d\n",
			prot_group_type, num_bde);
	return 1;
}

/*
 * This function checks for BlockGuard errors detected by
 * the HBA.  In case of errors, the ASC/ASCQ fields in the
 * sense buffer will be set accordingly, paired with
 * ILLEGAL_REQUEST to signal to the kernel that the HBA
 * detected corruption.
 *
 * Returns:
 *  0 - No error found
 *  1 - BlockGuard error found
 * -1 - Internal error (bad profile, ...etc)
 */
static int
lpfc_parse_bg_err(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd,
			struct lpfc_iocbq *pIocbOut)
{
	struct scsi_cmnd *cmd = lpfc_cmd->pCmd;
	struct sli3_bg_fields *bgf = &pIocbOut->iocb.unsli3.sli3_bg;
	int ret = 0;
	uint32_t bghm = bgf->bghm;
	uint32_t bgstat = bgf->bgstat;
	uint64_t failing_sector = 0;

	lpfc_printf_log(phba, KERN_ERR, LOG_BG, "9069 BLKGRD: BG ERROR in cmd"
			" 0x%x lba 0x%llx blk cnt 0x%x "
			"bgstat=0x%x bghm=0x%x\n",
			cmd->cmnd[0], (unsigned long long)scsi_get_lba(cmd),
			blk_rq_sectors(cmd->request), bgstat, bghm);

	spin_lock(&_dump_buf_lock);
	if (!_dump_buf_done) {
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,  "9070 BLKGRD: Saving"
			" Data for %u blocks to debugfs\n",
				(cmd->cmnd[7] << 8 | cmd->cmnd[8]));
		lpfc_debug_save_data(phba, cmd);

		/* If we have a prot sgl, save the DIF buffer */
		if (lpfc_prot_group_type(phba, cmd) ==
				LPFC_PG_TYPE_DIF_BUF) {
			lpfc_printf_log(phba, KERN_ERR, LOG_BG, "9071 BLKGRD: "
				"Saving DIF for %u blocks to debugfs\n",
				(cmd->cmnd[7] << 8 | cmd->cmnd[8]));
			lpfc_debug_save_dif(phba, cmd);
		}

		_dump_buf_done = 1;
	}
	spin_unlock(&_dump_buf_lock);

	if (lpfc_bgs_get_invalid_prof(bgstat)) {
		cmd->result = ScsiResult(DID_ERROR, 0);
		lpfc_printf_log(phba, KERN_ERR, LOG_BG, "9072 BLKGRD: Invalid"
			" BlockGuard profile. bgstat:0x%x\n",
			bgstat);
		ret = (-1);
		goto out;
	}

	if (lpfc_bgs_get_uninit_dif_block(bgstat)) {
		cmd->result = ScsiResult(DID_ERROR, 0);
		lpfc_printf_log(phba, KERN_ERR, LOG_BG, "9073 BLKGRD: "
				"Invalid BlockGuard DIF Block. bgstat:0x%x\n",
				bgstat);
		ret = (-1);
		goto out;
	}

	if (lpfc_bgs_get_guard_err(bgstat)) {
		ret = 1;

		scsi_build_sense_buffer(1, cmd->sense_buffer, ILLEGAL_REQUEST,
				0x10, 0x1);
		cmd->result = DRIVER_SENSE << 24
			| ScsiResult(DID_ABORT, SAM_STAT_CHECK_CONDITION);
		phba->bg_guard_err_cnt++;
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9055 BLKGRD: guard_tag error\n");
	}

	if (lpfc_bgs_get_reftag_err(bgstat)) {
		ret = 1;

		scsi_build_sense_buffer(1, cmd->sense_buffer, ILLEGAL_REQUEST,
				0x10, 0x3);
		cmd->result = DRIVER_SENSE << 24
			| ScsiResult(DID_ABORT, SAM_STAT_CHECK_CONDITION);

		phba->bg_reftag_err_cnt++;
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9056 BLKGRD: ref_tag error\n");
	}

	if (lpfc_bgs_get_apptag_err(bgstat)) {
		ret = 1;

		scsi_build_sense_buffer(1, cmd->sense_buffer, ILLEGAL_REQUEST,
				0x10, 0x2);
		cmd->result = DRIVER_SENSE << 24
			| ScsiResult(DID_ABORT, SAM_STAT_CHECK_CONDITION);

		phba->bg_apptag_err_cnt++;
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9061 BLKGRD: app_tag error\n");
	}

	if (lpfc_bgs_get_hi_water_mark_present(bgstat)) {
		/*
		 * setup sense data descriptor 0 per SPC-4 as an information
		 * field, and put the failing LBA in it
		 */
		cmd->sense_buffer[8] = 0;     /* Information */
		cmd->sense_buffer[9] = 0xa;   /* Add. length */
		bghm /= cmd->device->sector_size;

		failing_sector = scsi_get_lba(cmd);
		failing_sector += bghm;

		put_unaligned_be64(failing_sector, &cmd->sense_buffer[10]);
	}

	if (!ret) {
		/* No error was reported - problem in FW? */
		cmd->result = ScsiResult(DID_ERROR, 0);
		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
			"9057 BLKGRD: no errors reported!\n");
	}

out:
	return ret;
}

/**
 * lpfc_scsi_prep_dma_buf_s4 - DMA mapping for scsi buffer to SLI4 IF spec
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine does the pci dma mapping for scatter-gather list of scsi cmnd
 * field of @lpfc_cmd for device with SLI-4 interface spec.
 *
 * Return codes:
 *	1 - Error
 *	0 - Success
 **/
static int
lpfc_scsi_prep_dma_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct scatterlist *sgel = NULL;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	struct sli4_sge *sgl = (struct sli4_sge *)lpfc_cmd->fcp_bpl;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	dma_addr_t physaddr;
	uint32_t num_bde = 0;
	uint32_t dma_len;
	uint32_t dma_offset = 0;
	int nseg;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	if (scsi_sg_count(scsi_cmnd)) {
		/*
		 * The driver stores the segment count returned from pci_map_sg
		 * because this a count of dma-mappings used to map the use_sg
		 * pages.  They are not guaranteed to be the same for those
		 * architectures that implement an IOMMU.
		 */

		nseg = scsi_dma_map(scsi_cmnd);
		if (unlikely(!nseg))
			return 1;
		sgl += 1;
		/* clear the last flag in the fcp_rsp map entry */
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl += 1;

		lpfc_cmd->seg_cnt = nseg;
		if (lpfc_cmd->seg_cnt > phba->cfg_sg_seg_cnt) {
			lpfc_printf_log(phba, KERN_ERR, LOG_BG, "9074 BLKGRD:"
				" %s: Too many sg segments from "
				"dma_map_sg.  Config %d, seg_cnt %d\n",
				__func__, phba->cfg_sg_seg_cnt,
			       lpfc_cmd->seg_cnt);
			scsi_dma_unmap(scsi_cmnd);
			return 1;
		}

		/*
		 * The driver established a maximum scatter-gather segment count
		 * during probe that limits the number of sg elements in any
		 * single scsi command.  Just run through the seg_cnt and format
		 * the sge's.
		 * When using SLI-3 the driver will try to fit all the BDEs into
		 * the IOCB. If it can't then the BDEs get added to a BPL as it
		 * does for SLI-2 mode.
		 */
		scsi_for_each_sg(scsi_cmnd, sgel, nseg, num_bde) {
			physaddr = sg_dma_address(sgel);
			dma_len = sg_dma_len(sgel);
			sgl->addr_lo = cpu_to_le32(putPaddrLow(physaddr));
			sgl->addr_hi = cpu_to_le32(putPaddrHigh(physaddr));
			if ((num_bde + 1) == nseg)
				bf_set(lpfc_sli4_sge_last, sgl, 1);
			else
				bf_set(lpfc_sli4_sge_last, sgl, 0);
			bf_set(lpfc_sli4_sge_offset, sgl, dma_offset);
			sgl->word2 = cpu_to_le32(sgl->word2);
			sgl->sge_len = cpu_to_le32(dma_len);
			dma_offset += dma_len;
			sgl++;
		}
	} else {
		sgl += 1;
		/* clear the last flag in the fcp_rsp map entry */
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
	}

	/*
	 * Finish initializing those IOCB fields that are dependent on the
	 * scsi_cmnd request_buffer.  Note that for SLI-2 the bdeSize is
	 * explicitly reinitialized.
	 * all iocb memory resources are reused.
	 */
	fcp_cmnd->fcpDl = cpu_to_be32(scsi_bufflen(scsi_cmnd));

	/*
	 * Due to difference in data length between DIF/non-DIF paths,
	 * we need to set word 4 of IOCB here
	 */
	iocb_cmd->un.fcpi.fcpi_parm = scsi_bufflen(scsi_cmnd);
	return 0;
}

/**
 * lpfc_scsi_prep_dma_buf - Wrapper function for DMA mapping of scsi buffer
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine wraps the actual DMA mapping function pointer from the
 * lpfc_hba struct.
 *
 * Return codes:
 *	1 - Error
 *	0 - Success
 **/
static inline int
lpfc_scsi_prep_dma_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	return phba->lpfc_scsi_prep_dma_buf(phba, lpfc_cmd);
}

/**
 * lpfc_send_scsi_error_event - Posts an event when there is SCSI error
 * @phba: Pointer to hba context object.
 * @vport: Pointer to vport object.
 * @lpfc_cmd: Pointer to lpfc scsi command which reported the error.
 * @rsp_iocb: Pointer to response iocb object which reported error.
 *
 * This function posts an event when there is a SCSI command reporting
 * error from the scsi device.
 **/
static void
lpfc_send_scsi_error_event(struct lpfc_hba *phba, struct lpfc_vport *vport,
		struct lpfc_scsi_buf *lpfc_cmd, struct lpfc_iocbq *rsp_iocb) {
	struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
	struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
	uint32_t resp_info = fcprsp->rspStatus2;
	uint32_t scsi_status = fcprsp->rspStatus3;
	uint32_t fcpi_parm = rsp_iocb->iocb.un.fcpi.fcpi_parm;
	struct lpfc_fast_path_event *fast_path_evt = NULL;
	struct lpfc_nodelist *pnode = lpfc_cmd->rdata->pnode;
	unsigned long flags;

	/* If there is queuefull or busy condition send a scsi event */
	if ((cmnd->result == SAM_STAT_TASK_SET_FULL) ||
		(cmnd->result == SAM_STAT_BUSY)) {
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.scsi_evt.event_type =
			FC_REG_SCSI_EVENT;
		fast_path_evt->un.scsi_evt.subcategory =
		(cmnd->result == SAM_STAT_TASK_SET_FULL) ?
		LPFC_EVENT_QFULL : LPFC_EVENT_DEVBSY;
		fast_path_evt->un.scsi_evt.lun = cmnd->device->lun;
		memcpy(&fast_path_evt->un.scsi_evt.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.scsi_evt.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
	} else if ((resp_info & SNS_LEN_VALID) && fcprsp->rspSnsLen &&
		((cmnd->cmnd[0] == READ_10) || (cmnd->cmnd[0] == WRITE_10))) {
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.check_cond_evt.scsi_event.event_type =
			FC_REG_SCSI_EVENT;
		fast_path_evt->un.check_cond_evt.scsi_event.subcategory =
			LPFC_EVENT_CHECK_COND;
		fast_path_evt->un.check_cond_evt.scsi_event.lun =
			cmnd->device->lun;
		memcpy(&fast_path_evt->un.check_cond_evt.scsi_event.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.check_cond_evt.scsi_event.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
		fast_path_evt->un.check_cond_evt.sense_key =
			cmnd->sense_buffer[2] & 0xf;
		fast_path_evt->un.check_cond_evt.asc = cmnd->sense_buffer[12];
		fast_path_evt->un.check_cond_evt.ascq = cmnd->sense_buffer[13];
	} else if ((cmnd->sc_data_direction == DMA_FROM_DEVICE) &&
		     fcpi_parm &&
		     ((be32_to_cpu(fcprsp->rspResId) != fcpi_parm) ||
			((scsi_status == SAM_STAT_GOOD) &&
			!(resp_info & (RESID_UNDER | RESID_OVER))))) {
		/*
		 * If status is good or resid does not match with fcp_param and
		 * there is valid fcpi_parm, then there is a read_check error
		 */
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.read_check_error.header.event_type =
			FC_REG_FABRIC_EVENT;
		fast_path_evt->un.read_check_error.header.subcategory =
			LPFC_EVENT_FCPRDCHKERR;
		memcpy(&fast_path_evt->un.read_check_error.header.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.read_check_error.header.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
		fast_path_evt->un.read_check_error.lun = cmnd->device->lun;
		fast_path_evt->un.read_check_error.opcode = cmnd->cmnd[0];
		fast_path_evt->un.read_check_error.fcpiparam =
			fcpi_parm;
	} else
		return;

	fast_path_evt->vport = vport;
	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&fast_path_evt->work_evt.evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);
	lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_scsi_unprep_dma_buf - Un-map DMA mapping of SG-list for dev
 * @phba: The HBA for which this call is being executed.
 * @psb: The scsi buffer which is going to be un-mapped.
 *
 * This routine does DMA un-mapping of scatter gather list of scsi command
 * field of @lpfc_cmd for device with SLI-3 interface spec.
 **/
static void
lpfc_scsi_unprep_dma_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	/*
	 * There are only two special cases to consider.  (1) the scsi command
	 * requested scatter-gather usage or (2) the scsi command allocated
	 * a request buffer, but did not request use_sg.  There is a third
	 * case, but it does not require resource deallocation.
	 */
	if (psb->seg_cnt > 0)
		scsi_dma_unmap(psb->pCmd);
	if (psb->prot_seg_cnt > 0)
		dma_unmap_sg(&phba->pcidev->dev, scsi_prot_sglist(psb->pCmd),
				scsi_prot_sg_count(psb->pCmd),
				psb->pCmd->sc_data_direction);
}

/**
 * lpfc_handler_fcp_err - FCP response handler
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: Pointer to lpfc_scsi_buf data structure.
 * @rsp_iocb: The response IOCB which contains FCP error.
 *
 * This routine is called to process response IOCB with status field
 * IOSTAT_FCP_RSP_ERROR. This routine sets result field of scsi command
 * based upon SCSI and FCP error.
 **/
static void
lpfc_handle_fcp_err(struct lpfc_vport *vport, struct lpfc_scsi_buf *lpfc_cmd,
		    struct lpfc_iocbq *rsp_iocb)
{
	struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcpcmd = lpfc_cmd->fcp_cmnd;
	struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
	uint32_t fcpi_parm = rsp_iocb->iocb.un.fcpi.fcpi_parm;
	uint32_t resp_info = fcprsp->rspStatus2;
	uint32_t scsi_status = fcprsp->rspStatus3;
	uint32_t *lp;
	uint32_t host_status = DID_OK;
	uint32_t rsplen = 0;
	uint32_t logit = LOG_FCP | LOG_FCP_ERROR;


	/*
	 *  If this is a task management command, there is no
	 *  scsi packet associated with this lpfc_cmd.  The driver
	 *  consumes it.
	 */
	if (fcpcmd->fcpCntl2) {
		scsi_status = 0;
		goto out;
	}

	if (resp_info & RSP_LEN_VALID) {
		rsplen = be32_to_cpu(fcprsp->rspRspLen);
		if (rsplen != 0 && rsplen != 4 && rsplen != 8) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "2719 Invalid response length: "
				 "tgt x%x lun x%x cmnd x%x rsplen x%x\n",
				 cmnd->device->id,
				 cmnd->device->lun, cmnd->cmnd[0],
				 rsplen);
			host_status = DID_ERROR;
			goto out;
		}
		if (fcprsp->rspInfo3 != RSP_NO_FAILURE) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "2757 Protocol failure detected during "
				 "processing of FCP I/O op: "
				 "tgt x%x lun x%x cmnd x%x rspInfo3 x%x\n",
				 cmnd->device->id,
				 cmnd->device->lun, cmnd->cmnd[0],
				 fcprsp->rspInfo3);
			host_status = DID_ERROR;
			goto out;
		}
	}

	if ((resp_info & SNS_LEN_VALID) && fcprsp->rspSnsLen) {
		uint32_t snslen = be32_to_cpu(fcprsp->rspSnsLen);
		if (snslen > SCSI_SENSE_BUFFERSIZE)
			snslen = SCSI_SENSE_BUFFERSIZE;

		if (resp_info & RSP_LEN_VALID)
		  rsplen = be32_to_cpu(fcprsp->rspRspLen);
		memcpy(cmnd->sense_buffer, &fcprsp->rspInfo0 + rsplen, snslen);
	}
	lp = (uint32_t *)cmnd->sense_buffer;

	if (!scsi_status && (resp_info & RESID_UNDER))
		logit = LOG_FCP;

	lpfc_printf_vlog(vport, KERN_WARNING, logit,
			 "9024 FCP command x%x failed: x%x SNS x%x x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 cmnd->cmnd[0], scsi_status,
			 be32_to_cpu(*lp), be32_to_cpu(*(lp + 3)), resp_info,
			 be32_to_cpu(fcprsp->rspResId),
			 be32_to_cpu(fcprsp->rspSnsLen),
			 be32_to_cpu(fcprsp->rspRspLen),
			 fcprsp->rspInfo3);

	scsi_set_resid(cmnd, 0);
	if (resp_info & RESID_UNDER) {
		scsi_set_resid(cmnd, be32_to_cpu(fcprsp->rspResId));

		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
				 "9025 FCP Read Underrun, expected %d, "
				 "residual %d Data: x%x x%x x%x\n",
				 be32_to_cpu(fcpcmd->fcpDl),
				 scsi_get_resid(cmnd), fcpi_parm, cmnd->cmnd[0],
				 cmnd->underflow);

		/*
		 * If there is an under run check if under run reported by
		 * storage array is same as the under run reported by HBA.
		 * If this is not same, there is a dropped frame.
		 */
		if ((cmnd->sc_data_direction == DMA_FROM_DEVICE) &&
			fcpi_parm &&
			(scsi_get_resid(cmnd) != fcpi_parm)) {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_FCP | LOG_FCP_ERROR,
					 "9026 FCP Read Check Error "
					 "and Underrun Data: x%x x%x x%x x%x\n",
					 be32_to_cpu(fcpcmd->fcpDl),
					 scsi_get_resid(cmnd), fcpi_parm,
					 cmnd->cmnd[0]);
			scsi_set_resid(cmnd, scsi_bufflen(cmnd));
			host_status = DID_ERROR;
		}
		/*
		 * The cmnd->underflow is the minimum number of bytes that must
		 * be transfered for this command.  Provided a sense condition
		 * is not present, make sure the actual amount transferred is at
		 * least the underflow value or fail.
		 */
		if (!(resp_info & SNS_LEN_VALID) &&
		    (scsi_status == SAM_STAT_GOOD) &&
		    (scsi_bufflen(cmnd) - scsi_get_resid(cmnd)
		     < cmnd->underflow)) {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
					 "9027 FCP command x%x residual "
					 "underrun converted to error "
					 "Data: x%x x%x x%x\n",
					 cmnd->cmnd[0], scsi_bufflen(cmnd),
					 scsi_get_resid(cmnd), cmnd->underflow);
			host_status = DID_ERROR;
		}
	} else if (resp_info & RESID_OVER) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "9028 FCP command x%x residual overrun error. "
				 "Data: x%x x%x\n", cmnd->cmnd[0],
				 scsi_bufflen(cmnd), scsi_get_resid(cmnd));
		host_status = DID_ERROR;

	/*
	 * Check SLI validation that all the transfer was actually done
	 * (fcpi_parm should be zero). Apply check only to reads.
	 */
	} else if ((scsi_status == SAM_STAT_GOOD) && fcpi_parm &&
			(cmnd->sc_data_direction == DMA_FROM_DEVICE)) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP | LOG_FCP_ERROR,
				 "9029 FCP Read Check Error Data: "
				 "x%x x%x x%x x%x\n",
				 be32_to_cpu(fcpcmd->fcpDl),
				 be32_to_cpu(fcprsp->rspResId),
				 fcpi_parm, cmnd->cmnd[0]);
		host_status = DID_ERROR;
		scsi_set_resid(cmnd, scsi_bufflen(cmnd));
	}

 out:
	cmnd->result = ScsiResult(host_status, scsi_status);
	lpfc_send_scsi_error_event(vport->phba, vport, lpfc_cmd, rsp_iocb);
}

/**
 * lpfc_scsi_cmd_iocb_cmpl - Scsi cmnd IOCB completion routine
 * @phba: The Hba for which this call is being executed.
 * @pIocbIn: The command IOCBQ for the scsi cmnd.
 * @pIocbOut: The response IOCBQ for the scsi cmnd.
 *
 * This routine assigns scsi command result by looking into response IOCB
 * status field appropriately. This routine handles QUEUE FULL condition as
 * well by ramping down device queue depth.
 **/
static void
lpfc_scsi_cmd_iocb_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *pIocbIn,
			struct lpfc_iocbq *pIocbOut)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) pIocbIn->context1;
	struct lpfc_vport      *vport = pIocbIn->vport;
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *pnode = rdata->pnode;
	struct scsi_cmnd *cmd;
	int result;
	struct scsi_device *tmp_sdev;
	int depth;
	unsigned long flags;
	struct lpfc_fast_path_event *fast_path_evt;
	struct Scsi_Host *shost;
	uint32_t queue_depth, scsi_id;

	/* Sanity check on return of outstanding command */
	if (!(lpfc_cmd->pCmd))
		return;
	cmd = lpfc_cmd->pCmd;
	shost = cmd->device->host;

	lpfc_cmd->result = pIocbOut->iocb.un.ulpWord[4];
	lpfc_cmd->status = pIocbOut->iocb.ulpStatus;
	/* pick up SLI4 exhange busy status from HBA */
	lpfc_cmd->exch_busy = pIocbOut->iocb_flag & LPFC_EXCHANGE_BUSY;

	if (pnode && NLP_CHK_NODE_ACT(pnode))
		atomic_dec(&pnode->cmd_pending);

	if (lpfc_cmd->status) {
		if (lpfc_cmd->status == IOSTAT_LOCAL_REJECT &&
		    (lpfc_cmd->result & IOERR_DRVR_MASK))
			lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
		else if (lpfc_cmd->status >= IOSTAT_CNT)
			lpfc_cmd->status = IOSTAT_DEFAULT;

		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "9030 FCP cmd x%x failed <%d/%d> "
				 "status: x%x result: x%x Data: x%x x%x\n",
				 cmd->cmnd[0],
				 cmd->device ? cmd->device->id : 0xffff,
				 cmd->device ? cmd->device->lun : 0xffff,
				 lpfc_cmd->status, lpfc_cmd->result,
				 pIocbOut->iocb.ulpContext,
				 lpfc_cmd->cur_iocbq.iocb.ulpIoTag);

		switch (lpfc_cmd->status) {
		case IOSTAT_FCP_RSP_ERROR:
			/* Call FCP RSP handler to determine result */
			lpfc_handle_fcp_err(vport, lpfc_cmd, pIocbOut);
			break;
		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			cmd->result = ScsiResult(DID_TRANSPORT_DISRUPTED, 0);
			fast_path_evt = lpfc_alloc_fast_evt(phba);
			if (!fast_path_evt)
				break;
			fast_path_evt->un.fabric_evt.event_type =
				FC_REG_FABRIC_EVENT;
			fast_path_evt->un.fabric_evt.subcategory =
				(lpfc_cmd->status == IOSTAT_NPORT_BSY) ?
				LPFC_EVENT_PORT_BUSY : LPFC_EVENT_FABRIC_BUSY;
			if (pnode && NLP_CHK_NODE_ACT(pnode)) {
				memcpy(&fast_path_evt->un.fabric_evt.wwpn,
					&pnode->nlp_portname,
					sizeof(struct lpfc_name));
				memcpy(&fast_path_evt->un.fabric_evt.wwnn,
					&pnode->nlp_nodename,
					sizeof(struct lpfc_name));
			}
			fast_path_evt->vport = vport;
			fast_path_evt->work_evt.evt =
				LPFC_EVT_FASTPATH_MGMT_EVT;
			spin_lock_irqsave(&phba->hbalock, flags);
			list_add_tail(&fast_path_evt->work_evt.evt_listp,
				&phba->work_list);
			spin_unlock_irqrestore(&phba->hbalock, flags);
			lpfc_worker_wake_up(phba);
			break;
		case IOSTAT_LOCAL_REJECT:
			if (lpfc_cmd->result == IOERR_INVALID_RPI ||
			    lpfc_cmd->result == IOERR_NO_RESOURCES ||
			    lpfc_cmd->result == IOERR_ABORT_REQUESTED ||
			    lpfc_cmd->result == IOERR_SLER_CMD_RCV_FAILURE) {
				cmd->result = ScsiResult(DID_REQUEUE, 0);
				break;
			}

			if ((lpfc_cmd->result == IOERR_RX_DMA_FAILED ||
			     lpfc_cmd->result == IOERR_TX_DMA_FAILED) &&
			     pIocbOut->iocb.unsli3.sli3_bg.bgstat) {
				if (scsi_get_prot_op(cmd) != SCSI_PROT_NORMAL) {
					/*
					 * This is a response for a BG enabled
					 * cmd. Parse BG error
					 */
					lpfc_parse_bg_err(phba, lpfc_cmd,
							pIocbOut);
					break;
				} else {
					lpfc_printf_vlog(vport, KERN_WARNING,
							LOG_BG,
							"9031 non-zero BGSTAT "
							"on unprotected cmd\n");
				}
			}

		/* else: fall through */
		default:
			cmd->result = ScsiResult(DID_ERROR, 0);
			break;
		}

		if (!pnode || !NLP_CHK_NODE_ACT(pnode)
		    || (pnode->nlp_state != NLP_STE_MAPPED_NODE))
			cmd->result = ScsiResult(DID_TRANSPORT_DISRUPTED,
						 SAM_STAT_BUSY);
	} else {
		cmd->result = ScsiResult(DID_OK, 0);
	}

	if (cmd->result || lpfc_cmd->fcp_rsp->rspSnsLen) {
		uint32_t *lp = (uint32_t *)cmd->sense_buffer;

		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
				 "0710 Iodone <%d/%d> cmd %p, error "
				 "x%x SNS x%x x%x Data: x%x x%x\n",
				 cmd->device->id, cmd->device->lun, cmd,
				 cmd->result, *lp, *(lp + 3), cmd->retries,
				 scsi_get_resid(cmd));
	}

	lpfc_update_stats(phba, lpfc_cmd);
	result = cmd->result;
	if (vport->cfg_max_scsicmpl_time &&
	   time_after(jiffies, lpfc_cmd->start_time +
		msecs_to_jiffies(vport->cfg_max_scsicmpl_time))) {
		spin_lock_irqsave(shost->host_lock, flags);
		if (pnode && NLP_CHK_NODE_ACT(pnode)) {
			if (pnode->cmd_qdepth >
				atomic_read(&pnode->cmd_pending) &&
				(atomic_read(&pnode->cmd_pending) >
				LPFC_MIN_TGT_QDEPTH) &&
				((cmd->cmnd[0] == READ_10) ||
				(cmd->cmnd[0] == WRITE_10)))
				pnode->cmd_qdepth =
					atomic_read(&pnode->cmd_pending);

			pnode->last_change_time = jiffies;
		}
		spin_unlock_irqrestore(shost->host_lock, flags);
	} else if (pnode && NLP_CHK_NODE_ACT(pnode)) {
		if ((pnode->cmd_qdepth < vport->cfg_tgt_queue_depth) &&
		   time_after(jiffies, pnode->last_change_time +
			      msecs_to_jiffies(LPFC_TGTQ_INTERVAL))) {
			spin_lock_irqsave(shost->host_lock, flags);
			depth = pnode->cmd_qdepth * LPFC_TGTQ_RAMPUP_PCENT
				/ 100;
			depth = depth ? depth : 1;
			pnode->cmd_qdepth += depth;
			if (pnode->cmd_qdepth > vport->cfg_tgt_queue_depth)
				pnode->cmd_qdepth = vport->cfg_tgt_queue_depth;
			pnode->last_change_time = jiffies;
			spin_unlock_irqrestore(shost->host_lock, flags);
		}
	}

	lpfc_scsi_unprep_dma_buf(phba, lpfc_cmd);

	/* The sdev is not guaranteed to be valid post scsi_done upcall. */
	queue_depth = cmd->device->queue_depth;
	scsi_id = cmd->device->id;
	cmd->scsi_done(cmd);

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		/*
		 * If there is a thread waiting for command completion
		 * wake up the thread.
		 */
		spin_lock_irqsave(shost->host_lock, flags);
		lpfc_cmd->pCmd = NULL;
		if (lpfc_cmd->waitq)
			wake_up(lpfc_cmd->waitq);
		spin_unlock_irqrestore(shost->host_lock, flags);
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return;
	}

	if (!result)
		lpfc_rampup_queue_depth(vport, queue_depth);

	/*
	 * Check for queue full.  If the lun is reporting queue full, then
	 * back off the lun queue depth to prevent target overloads.
	 */
	if (result == SAM_STAT_TASK_SET_FULL && pnode &&
	    NLP_CHK_NODE_ACT(pnode)) {
		shost_for_each_device(tmp_sdev, shost) {
			if (tmp_sdev->id != scsi_id)
				continue;
			depth = scsi_track_queue_full(tmp_sdev,
						      tmp_sdev->queue_depth-1);
			if (depth <= 0)
				continue;
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
					 "0711 detected queue full - lun queue "
					 "depth adjusted to %d.\n", depth);
			lpfc_send_sdev_queuedepth_change_event(phba, vport,
							       pnode,
							       tmp_sdev->lun,
							       depth+1, depth);
		}
	}

	/*
	 * If there is a thread waiting for command completion
	 * wake up the thread.
	 */
	spin_lock_irqsave(shost->host_lock, flags);
	lpfc_cmd->pCmd = NULL;
	if (lpfc_cmd->waitq)
		wake_up(lpfc_cmd->waitq);
	spin_unlock_irqrestore(shost->host_lock, flags);

	lpfc_release_scsi_buf(phba, lpfc_cmd);
}

/**
 * lpfc_fcpcmd_to_iocb - copy the fcp_cmd data into the IOCB
 * @data: A pointer to the immediate command data portion of the IOCB.
 * @fcp_cmnd: The FCP Command that is provided by the SCSI layer.
 *
 * The routine copies the entire FCP command from @fcp_cmnd to @data while
 * byte swapping the data to big endian format for transmission on the wire.
 **/
static void
lpfc_fcpcmd_to_iocb(uint8_t *data, struct fcp_cmnd *fcp_cmnd)
{
	int i, j;
	for (i = 0, j = 0; i < sizeof(struct fcp_cmnd);
	     i += sizeof(uint32_t), j++) {
		((uint32_t *)data)[j] = cpu_to_be32(((uint32_t *)fcp_cmnd)[j]);
	}
}

/**
 * lpfc_scsi_prep_cmnd - Wrapper func for convert scsi cmnd to FCP info unit
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: The scsi command which needs to send.
 * @pnode: Pointer to lpfc_nodelist.
 *
 * This routine initializes fcp_cmnd and iocb data structure from scsi command
 * to transfer for device with SLI3 interface spec.
 **/
static void
lpfc_scsi_prep_cmnd(struct lpfc_vport *vport, struct lpfc_scsi_buf *lpfc_cmd,
		    struct lpfc_nodelist *pnode)
{
	struct lpfc_hba *phba = vport->phba;
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	struct lpfc_iocbq *piocbq = &(lpfc_cmd->cur_iocbq);
	int datadir = scsi_cmnd->sc_data_direction;
	char tag[2];

	if (!pnode || !NLP_CHK_NODE_ACT(pnode))
		return;

	lpfc_cmd->fcp_rsp->rspSnsLen = 0;
	/* clear task management bits */
	lpfc_cmd->fcp_cmnd->fcpCntl2 = 0;

	int_to_scsilun(lpfc_cmd->pCmd->device->lun,
			&lpfc_cmd->fcp_cmnd->fcp_lun);

	memcpy(&fcp_cmnd->fcpCdb[0], scsi_cmnd->cmnd, 16);

	if (scsi_populate_tag_msg(scsi_cmnd, tag)) {
		switch (tag[0]) {
		case HEAD_OF_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = HEAD_OF_Q;
			break;
		case ORDERED_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = ORDERED_Q;
			break;
		default:
			fcp_cmnd->fcpCntl1 = SIMPLE_Q;
			break;
		}
	} else
		fcp_cmnd->fcpCntl1 = 0;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	if (scsi_sg_count(scsi_cmnd)) {
		if (datadir == DMA_TO_DEVICE) {
			iocb_cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
			if (phba->sli_rev < LPFC_SLI_REV4) {
				iocb_cmd->un.fcpi.fcpi_parm = 0;
				iocb_cmd->ulpPU = 0;
			} else
				iocb_cmd->ulpPU = PARM_READ_CHECK;
			fcp_cmnd->fcpCntl3 = WRITE_DATA;
			phba->fc4OutputRequests++;
		} else {
			iocb_cmd->ulpCommand = CMD_FCP_IREAD64_CR;
			iocb_cmd->ulpPU = PARM_READ_CHECK;
			fcp_cmnd->fcpCntl3 = READ_DATA;
			phba->fc4InputRequests++;
		}
	} else {
		iocb_cmd->ulpCommand = CMD_FCP_ICMND64_CR;
		iocb_cmd->un.fcpi.fcpi_parm = 0;
		iocb_cmd->ulpPU = 0;
		fcp_cmnd->fcpCntl3 = 0;
		phba->fc4ControlRequests++;
	}
	if (phba->sli_rev == 3 &&
	    !(phba->sli3_options & LPFC_SLI3_BG_ENABLED))
		lpfc_fcpcmd_to_iocb(iocb_cmd->unsli3.fcp_ext.icd, fcp_cmnd);
	/*
	 * Finish initializing those IOCB fields that are independent
	 * of the scsi_cmnd request_buffer
	 */
	piocbq->iocb.ulpContext = pnode->nlp_rpi;
	if (pnode->nlp_fcp_info & NLP_FCP_2_DEVICE)
		piocbq->iocb.ulpFCP2Rcvy = 1;
	else
		piocbq->iocb.ulpFCP2Rcvy = 0;

	piocbq->iocb.ulpClass = (pnode->nlp_fcp_info & 0x0f);
	piocbq->context1  = lpfc_cmd;
	piocbq->iocb_cmpl = lpfc_scsi_cmd_iocb_cmpl;
	piocbq->iocb.ulpTimeout = lpfc_cmd->timeout;
	piocbq->vport = vport;
}

/**
 * lpfc_scsi_prep_task_mgmt_cmnd - Convert SLI3 scsi TM cmd to FCP info unit
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: Pointer to lpfc_scsi_buf data structure.
 * @lun: Logical unit number.
 * @task_mgmt_cmd: SCSI task management command.
 *
 * This routine creates FCP information unit corresponding to @task_mgmt_cmd
 * for device with SLI-3 interface spec.
 *
 * Return codes:
 *   0 - Error
 *   1 - Success
 **/
static int
lpfc_scsi_prep_task_mgmt_cmd(struct lpfc_vport *vport,
			     struct lpfc_scsi_buf *lpfc_cmd,
			     unsigned int lun,
			     uint8_t task_mgmt_cmd)
{
	struct lpfc_iocbq *piocbq;
	IOCB_t *piocb;
	struct fcp_cmnd *fcp_cmnd;
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *ndlp = rdata->pnode;

	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) ||
	    ndlp->nlp_state != NLP_STE_MAPPED_NODE)
		return 0;

	piocbq = &(lpfc_cmd->cur_iocbq);
	piocbq->vport = vport;

	piocb = &piocbq->iocb;

	fcp_cmnd = lpfc_cmd->fcp_cmnd;
	/* Clear out any old data in the FCP command area */
	memset(fcp_cmnd, 0, sizeof(struct fcp_cmnd));
	int_to_scsilun(lun, &fcp_cmnd->fcp_lun);
	fcp_cmnd->fcpCntl2 = task_mgmt_cmd;
	if (vport->phba->sli_rev == 3 &&
	    !(vport->phba->sli3_options & LPFC_SLI3_BG_ENABLED))
		lpfc_fcpcmd_to_iocb(piocb->unsli3.fcp_ext.icd, fcp_cmnd);
	piocb->ulpCommand = CMD_FCP_ICMND64_CR;
	piocb->ulpContext = ndlp->nlp_rpi;
	if (ndlp->nlp_fcp_info & NLP_FCP_2_DEVICE) {
		piocb->ulpFCP2Rcvy = 1;
	}
	piocb->ulpClass = (ndlp->nlp_fcp_info & 0x0f);

	/* ulpTimeout is only one byte */
	if (lpfc_cmd->timeout > 0xff) {
		/*
		 * Do not timeout the command at the firmware level.
		 * The driver will provide the timeout mechanism.
		 */
		piocb->ulpTimeout = 0;
	} else
		piocb->ulpTimeout = lpfc_cmd->timeout;

	if (vport->phba->sli_rev == LPFC_SLI_REV4)
		lpfc_sli4_set_rsp_sgl_last(vport->phba, lpfc_cmd);

	return 1;
}

/**
 * lpfc_scsi_api_table_setup - Set up scsi api fucntion jump table
 * @phba: The hba struct for which this call is being executed.
 * @dev_grp: The HBA PCI-Device group number.
 *
 * This routine sets up the SCSI interface API function jump table in @phba
 * struct.
 * Returns: 0 - success, -ENODEV - failure.
 **/
int
lpfc_scsi_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	phba->lpfc_scsi_unprep_dma_buf = lpfc_scsi_unprep_dma_buf;
	phba->lpfc_scsi_prep_cmnd = lpfc_scsi_prep_cmnd;
	phba->lpfc_get_scsi_buf = lpfc_get_scsi_buf;

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_new_scsi_buf = lpfc_new_scsi_buf_s3;
		phba->lpfc_scsi_prep_dma_buf = lpfc_scsi_prep_dma_buf_s3;
		phba->lpfc_release_scsi_buf = lpfc_release_scsi_buf_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_new_scsi_buf = lpfc_new_scsi_buf_s4;
		phba->lpfc_scsi_prep_dma_buf = lpfc_scsi_prep_dma_buf_s4;
		phba->lpfc_release_scsi_buf = lpfc_release_scsi_buf_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1418 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	phba->lpfc_get_scsi_buf = lpfc_get_scsi_buf;
	phba->lpfc_rampdown_queue_depth = lpfc_rampdown_queue_depth;
	phba->lpfc_scsi_cmd_iocb_cmpl = lpfc_scsi_cmd_iocb_cmpl;
	return 0;
}

/**
 * lpfc_taskmgmt_def_cmpl - IOCB completion routine for task management command
 * @phba: The Hba for which this call is being executed.
 * @cmdiocbq: Pointer to lpfc_iocbq data structure.
 * @rspiocbq: Pointer to lpfc_iocbq data structure.
 *
 * This routine is IOCB completion routine for device reset and target reset
 * routine. This routine release scsi buffer associated with lpfc_cmd.
 **/
static void
lpfc_tskmgmt_def_cmpl(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbq,
			struct lpfc_iocbq *rspiocbq)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) cmdiocbq->context1;
	if (lpfc_cmd)
		lpfc_release_scsi_buf(phba, lpfc_cmd);
	return;
}

/**
 * lpfc_info - Info entry point of scsi_host_template data structure
 * @host: The scsi host for which this call is being executed.
 *
 * This routine provides module information about hba.
 *
 * Reutrn code:
 *   Pointer to char - Success.
 **/
const char *
lpfc_info(struct Scsi_Host *host)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int len;
	static char  lpfcinfobuf[384];

	memset(lpfcinfobuf,0,384);
	if (phba && phba->pcidev){
		strncpy(lpfcinfobuf, phba->ModelDesc, 256);
		len = strlen(lpfcinfobuf);
		snprintf(lpfcinfobuf + len,
			384-len,
			" on PCI bus %02x device %02x irq %d",
			phba->pcidev->bus->number,
			phba->pcidev->devfn,
			phba->pcidev->irq);
		len = strlen(lpfcinfobuf);
		if (phba->Port[0]) {
			snprintf(lpfcinfobuf + len,
				 384-len,
				 " port %s",
				 phba->Port);
		}
		len = strlen(lpfcinfobuf);
		if (phba->sli4_hba.link_state.logical_speed) {
			snprintf(lpfcinfobuf + len,
				 384-len,
				 " Logical Link Speed: %d Mbps",
				 phba->sli4_hba.link_state.logical_speed * 10);
		}
	}
	return lpfcinfobuf;
}

/**
 * lpfc_poll_rearm_time - Routine to modify fcp_poll timer of hba
 * @phba: The Hba for which this call is being executed.
 *
 * This routine modifies fcp_poll_timer  field of @phba by cfg_poll_tmo.
 * The default value of cfg_poll_tmo is 10 milliseconds.
 **/
static __inline__ void lpfc_poll_rearm_timer(struct lpfc_hba * phba)
{
	unsigned long  poll_tmo_expires =
		(jiffies + msecs_to_jiffies(phba->cfg_poll_tmo));

	if (phba->sli.ring[LPFC_FCP_RING].txcmplq_cnt)
		mod_timer(&phba->fcp_poll_timer,
			  poll_tmo_expires);
}

/**
 * lpfc_poll_start_timer - Routine to start fcp_poll_timer of HBA
 * @phba: The Hba for which this call is being executed.
 *
 * This routine starts the fcp_poll_timer of @phba.
 **/
void lpfc_poll_start_timer(struct lpfc_hba * phba)
{
	lpfc_poll_rearm_timer(phba);
}

/**
 * lpfc_poll_timeout - Restart polling timer
 * @ptr: Map to lpfc_hba data structure pointer.
 *
 * This routine restarts fcp_poll timer, when FCP ring  polling is enable
 * and FCP Ring interrupt is disable.
 **/

void lpfc_poll_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba = (struct lpfc_hba *) ptr;

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);

		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}
}

/**
 * lpfc_queuecommand - scsi_host_template queuecommand entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 * @done: Pointer to done routine.
 *
 * Driver registers this routine to scsi midlayer to submit a @cmd to process.
 * This routine prepares an IOCB from scsi command and provides to firmware.
 * The @done callback is invoked after driver finished processing the command.
 *
 * Return value :
 *   0 - Success
 *   SCSI_MLQUEUE_HOST_BUSY - Block all devices served by this host temporarily.
 **/
static int
lpfc_queuecommand(struct scsi_cmnd *cmnd, void (*done) (struct scsi_cmnd *))
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *ndlp;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmnd->device));
	int err;

	err = fc_remote_port_chkready(rport);
	if (err) {
		cmnd->result = err;
		goto out_fail_command;
	}
	ndlp = rdata->pnode;

	if (!(phba->sli3_options & LPFC_SLI3_BG_ENABLED) &&
		scsi_get_prot_op(cmnd) != SCSI_PROT_NORMAL) {

		lpfc_printf_log(phba, KERN_ERR, LOG_BG,
				"9058 BLKGRD: ERROR: rcvd protected cmd:%02x"
				" op:%02x str=%s without registering for"
				" BlockGuard - Rejecting command\n",
				cmnd->cmnd[0], scsi_get_prot_op(cmnd),
				dif_op_str[scsi_get_prot_op(cmnd)]);
		goto out_fail_command;
	}

	/*
	 * Catch race where our node has transitioned, but the
	 * transport is still transitioning.
	 */
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		cmnd->result = ScsiResult(DID_TRANSPORT_DISRUPTED, 0);
		goto out_fail_command;
	}
	if (atomic_read(&ndlp->cmd_pending) >= ndlp->cmd_qdepth)
		goto out_host_busy;

	lpfc_cmd = lpfc_get_scsi_buf(phba);
	if (lpfc_cmd == NULL) {
		lpfc_rampdown_queue_depth(phba);

		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
				 "0707 driver's buffer pool is empty, "
				 "IO busied\n");
		goto out_host_busy;
	}

	/*
	 * Store the midlayer's command structure for the completion phase
	 * and complete the command initialization.
	 */
	lpfc_cmd->pCmd  = cmnd;
	lpfc_cmd->rdata = rdata;
	lpfc_cmd->timeout = 0;
	lpfc_cmd->start_time = jiffies;
	cmnd->host_scribble = (unsigned char *)lpfc_cmd;
	cmnd->scsi_done = done;

	if (scsi_get_prot_op(cmnd) != SCSI_PROT_NORMAL) {
		if (vport->phba->cfg_enable_bg) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
				"9033 BLKGRD: rcvd protected cmd:%02x op:%02x "
				"str=%s\n",
				cmnd->cmnd[0], scsi_get_prot_op(cmnd),
				dif_op_str[scsi_get_prot_op(cmnd)]);
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
				"9034 BLKGRD: CDB: %02x %02x %02x %02x %02x "
				"%02x %02x %02x %02x %02x\n",
				cmnd->cmnd[0], cmnd->cmnd[1], cmnd->cmnd[2],
				cmnd->cmnd[3], cmnd->cmnd[4], cmnd->cmnd[5],
				cmnd->cmnd[6], cmnd->cmnd[7], cmnd->cmnd[8],
				cmnd->cmnd[9]);
			if (cmnd->cmnd[0] == READ_10)
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					"9035 BLKGRD: READ @ sector %llu, "
					"count %u\n",
					(unsigned long long)scsi_get_lba(cmnd),
					blk_rq_sectors(cmnd->request));
			else if (cmnd->cmnd[0] == WRITE_10)
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					"9036 BLKGRD: WRITE @ sector %llu, "
					"count %u cmd=%p\n",
					(unsigned long long)scsi_get_lba(cmnd),
					blk_rq_sectors(cmnd->request),
					cmnd);
		}

		err = lpfc_bg_scsi_prep_dma_buf(phba, lpfc_cmd);
	} else {
		if (vport->phba->cfg_enable_bg) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					"9038 BLKGRD: rcvd unprotected cmd:"
					"%02x op:%02x str=%s\n",
					cmnd->cmnd[0], scsi_get_prot_op(cmnd),
					dif_op_str[scsi_get_prot_op(cmnd)]);
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					"9039 BLKGRD: CDB: %02x %02x %02x "
					"%02x %02x %02x %02x %02x %02x %02x\n",
					cmnd->cmnd[0], cmnd->cmnd[1],
					cmnd->cmnd[2], cmnd->cmnd[3],
					cmnd->cmnd[4], cmnd->cmnd[5],
					cmnd->cmnd[6], cmnd->cmnd[7],
					cmnd->cmnd[8], cmnd->cmnd[9]);
			if (cmnd->cmnd[0] == READ_10)
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					"9040 dbg: READ @ sector %llu, "
					"count %u\n",
					(unsigned long long)scsi_get_lba(cmnd),
					 blk_rq_sectors(cmnd->request));
			else if (cmnd->cmnd[0] == WRITE_10)
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					 "9041 dbg: WRITE @ sector %llu, "
					 "count %u cmd=%p\n",
					 (unsigned long long)scsi_get_lba(cmnd),
					 blk_rq_sectors(cmnd->request), cmnd);
			else
				lpfc_printf_vlog(vport, KERN_WARNING, LOG_BG,
					 "9042 dbg: parser not implemented\n");
		}
		err = lpfc_scsi_prep_dma_buf(phba, lpfc_cmd);
	}

	if (err)
		goto out_host_busy_free_buf;

	lpfc_scsi_prep_cmnd(vport, lpfc_cmd, ndlp);

	atomic_inc(&ndlp->cmd_pending);
	err = lpfc_sli_issue_iocb(phba, LPFC_FCP_RING,
				  &lpfc_cmd->cur_iocbq, SLI_IOCB_RET_IOCB);
	if (err) {
		atomic_dec(&ndlp->cmd_pending);
		goto out_host_busy_free_buf;
	}
	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		spin_unlock(shost->host_lock);
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);

		spin_lock(shost->host_lock);
		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}

	return 0;

 out_host_busy_free_buf:
	lpfc_scsi_unprep_dma_buf(phba, lpfc_cmd);
	lpfc_release_scsi_buf(phba, lpfc_cmd);
 out_host_busy:
	return SCSI_MLQUEUE_HOST_BUSY;

 out_fail_command:
	done(cmnd);
	return 0;
}

/**
 * lpfc_abort_handler - scsi_host_template eh_abort_handler entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine aborts @cmnd pending in base driver.
 *
 * Return code :
 *   0x2003 - Error
 *   0x2002 - Success
 **/
static int
lpfc_abort_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_iocbq *iocb;
	struct lpfc_iocbq *abtsiocb;
	struct lpfc_scsi_buf *lpfc_cmd;
	IOCB_t *cmd, *icmd;
	int ret = SUCCESS;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(waitq);

	ret = fc_block_scsi_eh(cmnd);
	if (ret)
		return ret;
	lpfc_cmd = (struct lpfc_scsi_buf *)cmnd->host_scribble;
	BUG_ON(!lpfc_cmd);

	/*
	 * If pCmd field of the corresponding lpfc_scsi_buf structure
	 * points to a different SCSI command, then the driver has
	 * already completed this command, but the midlayer did not
	 * see the completion before the eh fired.  Just return
	 * SUCCESS.
	 */
	iocb = &lpfc_cmd->cur_iocbq;
	if (lpfc_cmd->pCmd != cmnd)
		goto out;

	BUG_ON(iocb->context1 != lpfc_cmd);

	abtsiocb = lpfc_sli_get_iocbq(phba);
	if (abtsiocb == NULL) {
		ret = FAILED;
		goto out;
	}

	/*
	 * The scsi command can not be in txq and it is in flight because the
	 * pCmd is still pointig at the SCSI command we have to abort. There
	 * is no need to search the txcmplq. Just send an abort to the FW.
	 */

	cmd = &iocb->iocb;
	icmd = &abtsiocb->iocb;
	icmd->un.acxri.abortType = ABORT_TYPE_ABTS;
	icmd->un.acxri.abortContextTag = cmd->ulpContext;
	if (phba->sli_rev == LPFC_SLI_REV4)
		icmd->un.acxri.abortIoTag = iocb->sli4_xritag;
	else
		icmd->un.acxri.abortIoTag = cmd->ulpIoTag;

	icmd->ulpLe = 1;
	icmd->ulpClass = cmd->ulpClass;

	/* ABTS WQE must go to the same WQ as the WQE to be aborted */
	abtsiocb->fcp_wqidx = iocb->fcp_wqidx;
	abtsiocb->iocb_flag |= LPFC_USE_FCPWQIDX;

	if (lpfc_is_link_up(phba))
		icmd->ulpCommand = CMD_ABORT_XRI_CN;
	else
		icmd->ulpCommand = CMD_CLOSE_XRI_CN;

	abtsiocb->iocb_cmpl = lpfc_sli_abort_fcp_cmpl;
	abtsiocb->vport = vport;
	if (lpfc_sli_issue_iocb(phba, LPFC_FCP_RING, abtsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_sli_release_iocbq(phba, abtsiocb);
		ret = FAILED;
		goto out;
	}

	if (phba->cfg_poll & DISABLE_FCP_RING_INT)
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);

	lpfc_cmd->waitq = &waitq;
	/* Wait for abort to complete */
	wait_event_timeout(waitq,
			  (lpfc_cmd->pCmd != cmnd),
			   (2*vport->cfg_devloss_tmo*HZ));

	spin_lock_irq(shost->host_lock);
	lpfc_cmd->waitq = NULL;
	spin_unlock_irq(shost->host_lock);

	if (lpfc_cmd->pCmd == cmnd) {
		ret = FAILED;
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "0748 abort handler timed out waiting "
				 "for abort to complete: ret %#x, ID %d, "
				 "LUN %d, snum %#lx\n",
				 ret, cmnd->device->id, cmnd->device->lun,
				 cmnd->serial_number);
	}

 out:
	lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
			 "0749 SCSI Layer I/O Abort Request Status x%x ID %d "
			 "LUN %d snum %#lx\n", ret, cmnd->device->id,
			 cmnd->device->lun, cmnd->serial_number);
	return ret;
}

static char *
lpfc_taskmgmt_name(uint8_t task_mgmt_cmd)
{
	switch (task_mgmt_cmd) {
	case FCP_ABORT_TASK_SET:
		return "ABORT_TASK_SET";
	case FCP_CLEAR_TASK_SET:
		return "FCP_CLEAR_TASK_SET";
	case FCP_BUS_RESET:
		return "FCP_BUS_RESET";
	case FCP_LUN_RESET:
		return "FCP_LUN_RESET";
	case FCP_TARGET_RESET:
		return "FCP_TARGET_RESET";
	case FCP_CLEAR_ACA:
		return "FCP_CLEAR_ACA";
	case FCP_TERMINATE_TASK:
		return "FCP_TERMINATE_TASK";
	default:
		return "unknown";
	}
}

/**
 * lpfc_send_taskmgmt - Generic SCSI Task Mgmt Handler
 * @vport: The virtual port for which this call is being executed.
 * @rdata: Pointer to remote port local data
 * @tgt_id: Target ID of remote device.
 * @lun_id: Lun number for the TMF
 * @task_mgmt_cmd: type of TMF to send
 *
 * This routine builds and sends a TMF (SCSI Task Mgmt Function) to
 * a remote port.
 *
 * Return Code:
 *   0x2003 - Error
 *   0x2002 - Success.
 **/
static int
lpfc_send_taskmgmt(struct lpfc_vport *vport, struct lpfc_rport_data *rdata,
		    unsigned  tgt_id, unsigned int lun_id,
		    uint8_t task_mgmt_cmd)
{
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_iocbq *iocbq;
	struct lpfc_iocbq *iocbqrsp;
	int ret;
	int status;

	if (!rdata->pnode || !NLP_CHK_NODE_ACT(rdata->pnode))
		return FAILED;

	lpfc_cmd = lpfc_get_scsi_buf(phba);
	if (lpfc_cmd == NULL)
		return FAILED;
	lpfc_cmd->timeout = 60;
	lpfc_cmd->rdata = rdata;

	status = lpfc_scsi_prep_task_mgmt_cmd(vport, lpfc_cmd, lun_id,
					   task_mgmt_cmd);
	if (!status) {
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return FAILED;
	}

	iocbq = &lpfc_cmd->cur_iocbq;
	iocbqrsp = lpfc_sli_get_iocbq(phba);
	if (iocbqrsp == NULL) {
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return FAILED;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
			 "0702 Issue %s to TGT %d LUN %d "
			 "rpi x%x nlp_flag x%x\n",
			 lpfc_taskmgmt_name(task_mgmt_cmd), tgt_id, lun_id,
			 rdata->pnode->nlp_rpi, rdata->pnode->nlp_flag);

	status = lpfc_sli_issue_iocb_wait(phba, LPFC_FCP_RING,
					  iocbq, iocbqrsp, lpfc_cmd->timeout);
	if (status != IOCB_SUCCESS) {
		if (status == IOCB_TIMEDOUT) {
			iocbq->iocb_cmpl = lpfc_tskmgmt_def_cmpl;
			ret = TIMEOUT_ERROR;
		} else
			ret = FAILED;
		lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0727 TMF %s to TGT %d LUN %d failed (%d, %d)\n",
			 lpfc_taskmgmt_name(task_mgmt_cmd),
			 tgt_id, lun_id, iocbqrsp->iocb.ulpStatus,
			 iocbqrsp->iocb.un.ulpWord[4]);
	} else if (status == IOCB_BUSY)
		ret = FAILED;
	else
		ret = SUCCESS;

	lpfc_sli_release_iocbq(phba, iocbqrsp);

	if (ret != TIMEOUT_ERROR)
		lpfc_release_scsi_buf(phba, lpfc_cmd);

	return ret;
}

/**
 * lpfc_chk_tgt_mapped -
 * @vport: The virtual port to check on
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine delays until the scsi target (aka rport) for the
 * command exists (is present and logged in) or we declare it non-existent.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_chk_tgt_mapped(struct lpfc_vport *vport, struct scsi_cmnd *cmnd)
{
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *pnode;
	unsigned long later;

	if (!rdata) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
			"0797 Tgt Map rport failure: rdata x%p\n", rdata);
		return FAILED;
	}
	pnode = rdata->pnode;
	/*
	 * If target is not in a MAPPED state, delay until
	 * target is rediscovered or devloss timeout expires.
	 */
	later = msecs_to_jiffies(2 * vport->cfg_devloss_tmo * 1000) + jiffies;
	while (time_after(later, jiffies)) {
		if (!pnode || !NLP_CHK_NODE_ACT(pnode))
			return FAILED;
		if (pnode->nlp_state == NLP_STE_MAPPED_NODE)
			return SUCCESS;
		schedule_timeout_uninterruptible(msecs_to_jiffies(500));
		rdata = cmnd->device->hostdata;
		if (!rdata)
			return FAILED;
		pnode = rdata->pnode;
	}
	if (!pnode || !NLP_CHK_NODE_ACT(pnode) ||
	    (pnode->nlp_state != NLP_STE_MAPPED_NODE))
		return FAILED;
	return SUCCESS;
}

/**
 * lpfc_reset_flush_io_context -
 * @vport: The virtual port (scsi_host) for the flush context
 * @tgt_id: If aborting by Target contect - specifies the target id
 * @lun_id: If aborting by Lun context - specifies the lun id
 * @context: specifies the context level to flush at.
 *
 * After a reset condition via TMF, we need to flush orphaned i/o
 * contexts from the adapter. This routine aborts any contexts
 * outstanding, then waits for their completions. The wait is
 * bounded by devloss_tmo though.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_reset_flush_io_context(struct lpfc_vport *vport, uint16_t tgt_id,
			uint64_t lun_id, lpfc_ctx_cmd context)
{
	struct lpfc_hba   *phba = vport->phba;
	unsigned long later;
	int cnt;

	cnt = lpfc_sli_sum_iocb(vport, tgt_id, lun_id, context);
	if (cnt)
		lpfc_sli_abort_iocb(vport, &phba->sli.ring[phba->sli.fcp_ring],
				    tgt_id, lun_id, context);
	later = msecs_to_jiffies(2 * vport->cfg_devloss_tmo * 1000) + jiffies;
	while (time_after(later, jiffies) && cnt) {
		schedule_timeout_uninterruptible(msecs_to_jiffies(20));
		cnt = lpfc_sli_sum_iocb(vport, tgt_id, lun_id, context);
	}
	if (cnt) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0724 I/O flush failure for context %s : cnt x%x\n",
			((context == LPFC_CTX_LUN) ? "LUN" :
			 ((context == LPFC_CTX_TGT) ? "TGT" :
			  ((context == LPFC_CTX_HOST) ? "HOST" : "Unknown"))),
			cnt);
		return FAILED;
	}
	return SUCCESS;
}

/**
 * lpfc_device_reset_handler - scsi_host_template eh_device_reset entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine does a device reset by sending a LUN_RESET task management
 * command.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_device_reset_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *pnode;
	unsigned tgt_id = cmnd->device->id;
	unsigned int lun_id = cmnd->device->lun;
	struct lpfc_scsi_event_header scsi_event;
	int status;

	if (!rdata) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0798 Device Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}
	pnode = rdata->pnode;
	status = fc_block_scsi_eh(cmnd);
	if (status)
		return status;

	status = lpfc_chk_tgt_mapped(vport, cmnd);
	if (status == FAILED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0721 Device Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}

	scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_event.subcategory = LPFC_EVENT_LUNRESET;
	scsi_event.lun = lun_id;
	memcpy(scsi_event.wwpn, &pnode->nlp_portname, sizeof(struct lpfc_name));
	memcpy(scsi_event.wwnn, &pnode->nlp_nodename, sizeof(struct lpfc_name));

	fc_host_post_vendor_event(shost, fc_get_event_number(),
		sizeof(scsi_event), (char *)&scsi_event, LPFC_NL_VENDOR_ID);

	status = lpfc_send_taskmgmt(vport, rdata, tgt_id, lun_id,
						FCP_LUN_RESET);

	lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0713 SCSI layer issued Device Reset (%d, %d) "
			 "return x%x\n", tgt_id, lun_id, status);

	/*
	 * We have to clean up i/o as : they may be orphaned by the TMF;
	 * or if the TMF failed, they may be in an indeterminate state.
	 * So, continue on.
	 * We will report success if all the i/o aborts successfully.
	 */
	status = lpfc_reset_flush_io_context(vport, tgt_id, lun_id,
						LPFC_CTX_LUN);
	return status;
}

/**
 * lpfc_target_reset_handler - scsi_host_template eh_target_reset entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine does a target reset by sending a TARGET_RESET task management
 * command.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_target_reset_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *pnode;
	unsigned tgt_id = cmnd->device->id;
	unsigned int lun_id = cmnd->device->lun;
	struct lpfc_scsi_event_header scsi_event;
	int status;

	if (!rdata) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0799 Target Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}
	pnode = rdata->pnode;
	status = fc_block_scsi_eh(cmnd);
	if (status)
		return status;

	status = lpfc_chk_tgt_mapped(vport, cmnd);
	if (status == FAILED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0722 Target Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}

	scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_event.subcategory = LPFC_EVENT_TGTRESET;
	scsi_event.lun = 0;
	memcpy(scsi_event.wwpn, &pnode->nlp_portname, sizeof(struct lpfc_name));
	memcpy(scsi_event.wwnn, &pnode->nlp_nodename, sizeof(struct lpfc_name));

	fc_host_post_vendor_event(shost, fc_get_event_number(),
		sizeof(scsi_event), (char *)&scsi_event, LPFC_NL_VENDOR_ID);

	status = lpfc_send_taskmgmt(vport, rdata, tgt_id, lun_id,
					FCP_TARGET_RESET);

	lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0723 SCSI layer issued Target Reset (%d, %d) "
			 "return x%x\n", tgt_id, lun_id, status);

	/*
	 * We have to clean up i/o as : they may be orphaned by the TMF;
	 * or if the TMF failed, they may be in an indeterminate state.
	 * So, continue on.
	 * We will report success if all the i/o aborts successfully.
	 */
	status = lpfc_reset_flush_io_context(vport, tgt_id, lun_id,
					LPFC_CTX_TGT);
	return status;
}

/**
 * lpfc_bus_reset_handler - scsi_host_template eh_bus_reset_handler entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine does target reset to all targets on @cmnd->device->host.
 * This emulates Parallel SCSI Bus Reset Semantics.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_bus_reset_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_nodelist *ndlp = NULL;
	struct lpfc_scsi_event_header scsi_event;
	int match;
	int ret = SUCCESS, status, i;

	scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_event.subcategory = LPFC_EVENT_BUSRESET;
	scsi_event.lun = 0;
	memcpy(scsi_event.wwpn, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(scsi_event.wwnn, &vport->fc_nodename, sizeof(struct lpfc_name));

	fc_host_post_vendor_event(shost, fc_get_event_number(),
		sizeof(scsi_event), (char *)&scsi_event, LPFC_NL_VENDOR_ID);

	ret = fc_block_scsi_eh(cmnd);
	if (ret)
		return ret;

	/*
	 * Since the driver manages a single bus device, reset all
	 * targets known to the driver.  Should any target reset
	 * fail, this routine returns failure to the midlayer.
	 */
	for (i = 0; i < LPFC_MAX_TARGET; i++) {
		/* Search for mapped node by target ID */
		match = 0;
		spin_lock_irq(shost->host_lock);
		list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(ndlp))
				continue;
			if (ndlp->nlp_state == NLP_STE_MAPPED_NODE &&
			    ndlp->nlp_sid == i &&
			    ndlp->rport) {
				match = 1;
				break;
			}
		}
		spin_unlock_irq(shost->host_lock);
		if (!match)
			continue;

		status = lpfc_send_taskmgmt(vport, ndlp->rport->dd_data,
					i, 0, FCP_TARGET_RESET);

		if (status != SUCCESS) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
					 "0700 Bus Reset on target %d failed\n",
					 i);
			ret = FAILED;
		}
	}
	/*
	 * We have to clean up i/o as : they may be orphaned by the TMFs
	 * above; or if any of the TMFs failed, they may be in an
	 * indeterminate state.
	 * We will report success if all the i/o aborts successfully.
	 */

	status = lpfc_reset_flush_io_context(vport, 0, 0, LPFC_CTX_HOST);
	if (status != SUCCESS)
		ret = FAILED;

	lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0714 SCSI layer issued Bus Reset Data: x%x\n", ret);
	return ret;
}

/**
 * lpfc_slave_alloc - scsi_host_template slave_alloc entry point
 * @sdev: Pointer to scsi_device.
 *
 * This routine populates the cmds_per_lun count + 2 scsi_bufs into  this host's
 * globally available list of scsi buffers. This routine also makes sure scsi
 * buffer is not allocated more than HBA limit conveyed to midlayer. This list
 * of scsi buffer exists for the lifetime of the driver.
 *
 * Return codes:
 *   non-0 - Error
 *   0 - Success
 **/
static int
lpfc_slave_alloc(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));
	uint32_t total = 0;
	uint32_t num_to_alloc = 0;
	int num_allocated = 0;
	uint32_t sdev_cnt;

	if (!rport || fc_remote_port_chkready(rport))
		return -ENXIO;

	sdev->hostdata = rport->dd_data;
	sdev_cnt = atomic_inc_return(&phba->sdev_cnt);

	/*
	 * Populate the cmds_per_lun count scsi_bufs into this host's globally
	 * available list of scsi buffers.  Don't allocate more than the
	 * HBA limit conveyed to the midlayer via the host structure.  The
	 * formula accounts for the lun_queue_depth + error handlers + 1
	 * extra.  This list of scsi bufs exists for the lifetime of the driver.
	 */
	total = phba->total_scsi_bufs;
	num_to_alloc = vport->cfg_lun_queue_depth + 2;

	/* If allocated buffers are enough do nothing */
	if ((sdev_cnt * (vport->cfg_lun_queue_depth + 2)) < total)
		return 0;

	/* Allow some exchanges to be available always to complete discovery */
	if (total >= phba->cfg_hba_queue_depth - LPFC_DISC_IOCB_BUFF_COUNT ) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0704 At limitation of %d preallocated "
				 "command buffers\n", total);
		return 0;
	/* Allow some exchanges to be available always to complete discovery */
	} else if (total + num_to_alloc >
		phba->cfg_hba_queue_depth - LPFC_DISC_IOCB_BUFF_COUNT ) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0705 Allocation request of %d "
				 "command buffers will exceed max of %d.  "
				 "Reducing allocation request to %d.\n",
				 num_to_alloc, phba->cfg_hba_queue_depth,
				 (phba->cfg_hba_queue_depth - total));
		num_to_alloc = phba->cfg_hba_queue_depth - total;
	}
	num_allocated = lpfc_new_scsi_buf(vport, num_to_alloc);
	if (num_to_alloc != num_allocated) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0708 Allocation request of %d "
				 "command buffers did not succeed.  "
				 "Allocated %d buffers.\n",
				 num_to_alloc, num_allocated);
	}
	if (num_allocated > 0)
		phba->total_scsi_bufs += num_allocated;
	return 0;
}

/**
 * lpfc_slave_configure - scsi_host_template slave_configure entry point
 * @sdev: Pointer to scsi_device.
 *
 * This routine configures following items
 *   - Tag command queuing support for @sdev if supported.
 *   - Dev loss time out value of fc_rport.
 *   - Enable SLI polling for fcp ring if ENABLE_FCP_RING_POLLING flag is set.
 *
 * Return codes:
 *   0 - Success
 **/
static int
lpfc_slave_configure(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct fc_rport   *rport = starget_to_rport(sdev->sdev_target);

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, vport->cfg_lun_queue_depth);
	else
		scsi_deactivate_tcq(sdev, vport->cfg_lun_queue_depth);

	/*
	 * Initialize the fc transport attributes for the target
	 * containing this scsi device.  Also note that the driver's
	 * target pointer is stored in the starget_data for the
	 * driver's sysfs entry point functions.
	 */
	rport->dev_loss_tmo = vport->cfg_devloss_tmo;

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);
		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}

	return 0;
}

/**
 * lpfc_slave_destroy - slave_destroy entry point of SHT data structure
 * @sdev: Pointer to scsi_device.
 *
 * This routine sets @sdev hostatdata filed to null.
 **/
static void
lpfc_slave_destroy(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	atomic_dec(&phba->sdev_cnt);
	sdev->hostdata = NULL;
	return;
}


struct scsi_host_template lpfc_template = {
	.module			= THIS_MODULE,
	.name			= LPFC_DRIVER_NAME,
	.info			= lpfc_info,
	.queuecommand		= lpfc_queuecommand,
	.eh_abort_handler	= lpfc_abort_handler,
	.eh_device_reset_handler = lpfc_device_reset_handler,
	.eh_target_reset_handler = lpfc_target_reset_handler,
	.eh_bus_reset_handler	= lpfc_bus_reset_handler,
	.slave_alloc		= lpfc_slave_alloc,
	.slave_configure	= lpfc_slave_configure,
	.slave_destroy		= lpfc_slave_destroy,
	.scan_finished		= lpfc_scan_finished,
	.this_id		= -1,
	.sg_tablesize		= LPFC_DEFAULT_SG_SEG_CNT,
	.cmd_per_lun		= LPFC_CMD_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING,
	.shost_attrs		= lpfc_hba_attrs,
	.max_sectors		= 0xFFFF,
	.vendor_id		= LPFC_NL_VENDOR_ID,
	.change_queue_depth	= lpfc_change_queue_depth,
};

struct scsi_host_template lpfc_vport_template = {
	.module			= THIS_MODULE,
	.name			= LPFC_DRIVER_NAME,
	.info			= lpfc_info,
	.queuecommand		= lpfc_queuecommand,
	.eh_abort_handler	= lpfc_abort_handler,
	.eh_device_reset_handler = lpfc_device_reset_handler,
	.eh_target_reset_handler = lpfc_target_reset_handler,
	.eh_bus_reset_handler	= lpfc_bus_reset_handler,
	.slave_alloc		= lpfc_slave_alloc,
	.slave_configure	= lpfc_slave_configure,
	.slave_destroy		= lpfc_slave_destroy,
	.scan_finished		= lpfc_scan_finished,
	.this_id		= -1,
	.sg_tablesize		= LPFC_DEFAULT_SG_SEG_CNT,
	.cmd_per_lun		= LPFC_CMD_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING,
	.shost_attrs		= lpfc_vport_attrs,
	.max_sectors		= 0xFFFF,
	.change_queue_depth	= lpfc_change_queue_depth,
};
