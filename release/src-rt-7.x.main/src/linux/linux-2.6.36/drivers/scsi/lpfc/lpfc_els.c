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
/* See Fibre Channel protocol T11 FC-LS for details */
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

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
#include "lpfc_debugfs.h"

static int lpfc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iocbq *);
static void lpfc_cmpl_fabric_iocb(struct lpfc_hba *, struct lpfc_iocbq *,
			struct lpfc_iocbq *);
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport);
static int lpfc_issue_els_fdisc(struct lpfc_vport *vport,
				struct lpfc_nodelist *ndlp, uint8_t retry);
static int lpfc_issue_fabric_iocb(struct lpfc_hba *phba,
				  struct lpfc_iocbq *iocb);

static int lpfc_max_els_tries = 3;

/**
 * lpfc_els_chk_latt - Check host link attention event for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there is an outstanding host link
 * attention event during the discovery process with the @vport. It is done
 * by reading the HBA's Host Attention (HA) register. If there is any host
 * link attention events during this @vport's discovery process, the @vport
 * shall be marked as FC_ABORT_DISCOVERY, a host link attention clear shall
 * be issued if the link state is not already in host link cleared state,
 * and a return code shall indicate whether the host link attention event
 * had happened.
 *
 * Note that, if either the host link is in state LPFC_LINK_DOWN or @vport
 * state in LPFC_VPORT_READY, the request for checking host link attention
 * event will be ignored and a return code shall indicate no host link
 * attention event had happened.
 *
 * Return codes
 *   0 - no host link attention event happened
 *   1 - host link attention event happened
 **/
int
lpfc_els_chk_latt(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_state >= LPFC_VPORT_READY ||
	    phba->link_state == LPFC_LINK_DOWN ||
	    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	/* Read the HBA Host Attention Register */
	ha_copy = readl(phba->HAregaddr);

	if (!(ha_copy & HA_LATT))
		return 0;

	/* Pending Link Event during Discovery */
	lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0237 Pending Link Event during "
			 "Discovery: State x%x\n",
			 phba->pport->port_state);

	/* CLEAR_LA should re-enable link attention events and
	 * we should then imediately take a LATT event. The
	 * LATT processing should call lpfc_linkdown() which
	 * will cleanup any left over in-progress discovery
	 * events.
	 */
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	if (phba->link_state != LPFC_CLEAR_LA)
		lpfc_issue_clear_la(phba, vport);

	return 1;
}

/**
 * lpfc_prep_els_iocb - Allocate and prepare a lpfc iocb data structure
 * @vport: pointer to a host virtual N_Port data structure.
 * @expectRsp: flag indicating whether response is expected.
 * @cmdSize: size of the ELS command.
 * @retry: number of retries to the command IOCB when it fails.
 * @ndlp: pointer to a node-list data structure.
 * @did: destination identifier.
 * @elscmd: the ELS command code.
 *
 * This routine is used for allocating a lpfc-IOCB data structure from
 * the driver lpfc-IOCB free-list and prepare the IOCB with the parameters
 * passed into the routine for discovery state machine to issue an Extended
 * Link Service (ELS) commands. It is a generic lpfc-IOCB allocation
 * and preparation routine that is used by all the discovery state machine
 * routines and the ELS command-specific fields will be later set up by
 * the individual discovery machine routines after calling this routine
 * allocating and preparing a generic IOCB data structure. It fills in the
 * Buffer Descriptor Entries (BDEs), allocates buffers for both command
 * payload and response payload (if expected). The reference count on the
 * ndlp is incremented by 1 and the reference to the ndlp is put into
 * context1 of the IOCB data structure for this IOCB to hold the ndlp
 * reference for the command's callback function to access later.
 *
 * Return code
 *   Pointer to the newly allocated/prepared els iocb data structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpfc_prep_els_iocb(struct lpfc_vport *vport, uint8_t expectRsp,
		   uint16_t cmdSize, uint8_t retry,
		   struct lpfc_nodelist *ndlp, uint32_t did,
		   uint32_t elscmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmabuf *pcmd, *prsp, *pbuflist;
	struct ulp_bde64 *bpl;
	IOCB_t *icmd;


	if (!lpfc_is_link_up(phba))
		return NULL;

	/* Allocate buffer for  command iocb */
	elsiocb = lpfc_sli_get_iocbq(phba);

	if (elsiocb == NULL)
		return NULL;

	/*
	 * If this command is for fabric controller and HBA running
	 * in FIP mode send FLOGI, FDISC and LOGO as FIP frames.
	 */
	if ((did == Fabric_DID) &&
		(phba->hba_flag & HBA_FIP_SUPPORT) &&
		((elscmd == ELS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))
		switch (elscmd) {
		case ELS_CMD_FLOGI:
		elsiocb->iocb_flag |= ((ELS_ID_FLOGI << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		case ELS_CMD_FDISC:
		elsiocb->iocb_flag |= ((ELS_ID_FDISC << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		case ELS_CMD_LOGO:
		elsiocb->iocb_flag |= ((ELS_ID_LOGO << LPFC_FIP_ELS_ID_SHIFT)
					& LPFC_FIP_ELS_ID_MASK);
		break;
		}
	else
		elsiocb->iocb_flag &= ~LPFC_FIP_ELS_ID_MASK;

	icmd = &elsiocb->iocb;

	/* fill in BDEs for command */
	/* Allocate buffer for command payload */
	pcmd = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd->phys);
	if (!pcmd || !pcmd->virt)
		goto els_iocb_free_pcmb_exit;

	INIT_LIST_HEAD(&pcmd->list);

	/* Allocate buffer for response payload */
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (prsp)
			prsp->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	/* Allocate buffer for Buffer ptr list */
	pbuflist = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						 &pbuflist->phys);
	if (!pbuflist || !pbuflist->virt)
		goto els_iocb_free_pbuf_exit;

	INIT_LIST_HEAD(&pbuflist->list);

	icmd->un.elsreq64.bdl.addrHigh = putPaddrHigh(pbuflist->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLow(pbuflist->phys);
	icmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	/* DID */
	if (expectRsp) {
		icmd->un.elsreq64.bdl.bdeSize = (2 * sizeof(struct ulp_bde64));
		icmd->ulpCommand = CMD_ELS_REQUEST64_CR;
		icmd->ulpTimeout = phba->fc_ratov * 2;
	} else {
		icmd->un.elsreq64.bdl.bdeSize = sizeof(struct ulp_bde64);
		icmd->ulpCommand = CMD_XMIT_ELS_RSP64_CX;
	}
	icmd->ulpBdeCount = 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		icmd->un.elsreq64.myID = vport->fc_myDID;

		/* For ELS_REQUEST64_CR, use the VPI by default */
		icmd->ulpContext = vport->vpi + phba->vpi_base;
		icmd->ulpCt_h = 0;
		/* The CT field must be 0=INVALID_RPI for the ECHO cmd */
		if (elscmd == ELS_CMD_ECHO)
			icmd->ulpCt_l = 0; /* context = invalid RPI */
		else
			icmd->ulpCt_l = 1; /* context = VPI */
	}

	bpl = (struct ulp_bde64 *) pbuflist->virt;
	bpl->addrLow = le32_to_cpu(putPaddrLow(pcmd->phys));
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pcmd->phys));
	bpl->tus.f.bdeSize = cmdSize;
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
	}

	/* prevent preparing iocb with NULL ndlp reference */
	elsiocb->context1 = lpfc_nlp_get(ndlp);
	if (!elsiocb->context1)
		goto els_iocb_free_pbuf_exit;
	elsiocb->context2 = pcmd;
	elsiocb->context3 = pbuflist;
	elsiocb->retry = retry;
	elsiocb->vport = vport;
	elsiocb->drvrTimeout = (phba->fc_ratov << 1) + LPFC_DRVR_TIMEOUT;

	if (prsp) {
		list_add(&prsp->list, &pcmd->list);
	}
	if (expectRsp) {
		/* Xmit ELS command <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0116 Xmit ELS command x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} else {
		/* Xmit ELS response <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0117 Xmit ELS response x%x to remote "
				 "NPORT x%x I/O tag: x%x, size: x%x\n",
				 elscmd, ndlp->nlp_DID, elsiocb->iotag,
				 cmdSize);
	}
	return elsiocb;

els_iocb_free_pbuf_exit:
	if (expectRsp)
		lpfc_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_iocb_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kfree(prsp);

els_iocb_free_pcmb_exit:
	kfree(pcmd);
	lpfc_sli_release_iocbq(phba, elsiocb);
	return NULL;
}

/**
 * lpfc_issue_fabric_reglogin - Issue fabric registration login for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues a fabric registration login for a @vport. An
 * active ndlp node with Fabric_DID must already exist for this @vport.
 * The routine invokes two mailbox commands to carry out fabric registration
 * login through the HBA firmware: the first mailbox command requests the
 * HBA to perform link configuration for the @vport; and the second mailbox
 * command requests the HBA to perform the actual fabric registration login
 * with the @vport.
 *
 * Return code
 *   0 - successfully issued fabric registration login for @vport
 *   -ENXIO -- failed to issue fabric registration login for @vport
 **/
int
lpfc_issue_fabric_reglogin(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	int rc;
	int err = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		err = 1;
		goto fail;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 2;
		goto fail;
	}

	vport->port_state = LPFC_FABRIC_CFG_LINK;
	lpfc_config_link(phba, mbox);
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->vport = vport;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 3;
		goto fail_free_mbox;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 4;
		goto fail;
	}
	rc = lpfc_reg_rpi(phba, vport->vpi, Fabric_DID, (uint8_t *)sp, mbox, 0);
	if (rc) {
		err = 5;
		goto fail_free_mbox;
	}

	mbox->mbox_cmpl = lpfc_mbx_cmpl_fabric_reg_login;
	mbox->vport = vport;
	/* increment the reference count on ndlp to hold reference
	 * for the callback routine.
	 */
	mbox->context2 = lpfc_nlp_get(ndlp);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 6;
		goto fail_issue_reg_login;
	}

	return 0;

fail_issue_reg_login:
	/* decrement the reference count on ndlp just incremented
	 * for the failed mbox command.
	 */
	lpfc_nlp_put(ndlp);
	mp = (struct lpfc_dmabuf *) mbox->context1;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
fail_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);

fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0249 Cannot issue Register Fabric login: Err %d\n", err);
	return -ENXIO;
}

/**
 * lpfc_issue_reg_vfi - Register VFI for this vport's fabric login
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues a REG_VFI mailbox for the vfi, vpi, fcfi triplet for
 * the @vport. This mailbox command is necessary for FCoE only.
 *
 * Return code
 *   0 - successfully issued REG_VFI for @vport
 *   A failure code otherwise.
 **/
static int
lpfc_issue_reg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	struct lpfc_dmabuf *dmabuf;
	int rc = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		rc = -ENODEV;
		goto fail;
	}

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf) {
		rc = -ENOMEM;
		goto fail;
	}
	dmabuf->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &dmabuf->phys);
	if (!dmabuf->virt) {
		rc = -ENOMEM;
		goto fail_free_dmabuf;
	}
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto fail_free_coherent;
	}
	vport->port_state = LPFC_FABRIC_CFG_LINK;
	memcpy(dmabuf->virt, &phba->fc_fabparam, sizeof(vport->fc_sparam));
	lpfc_reg_vfi(mboxq, vport, dmabuf->phys);
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_reg_vfi;
	mboxq->vport = vport;
	mboxq->context1 = dmabuf;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = -ENXIO;
		goto fail_free_mbox;
	}
	return 0;

fail_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
fail_free_coherent:
	lpfc_mbuf_free(phba, dmabuf->virt, dmabuf->phys);
fail_free_dmabuf:
	kfree(dmabuf);
fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0289 Issue Register VFI failed: Err %d\n", rc);
	return rc;
}

/**
 * lpfc_cmpl_els_flogi_fabric - Completion function for flogi to a fabric port
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @sp: pointer to service parameter data structure.
 * @irsp: pointer to the IOCB within the lpfc response IOCB.
 *
 * This routine is invoked by the lpfc_cmpl_els_flogi() completion callback
 * function to handle the completion of a Fabric Login (FLOGI) into a fabric
 * port in a fabric topology. It properly sets up the parameters to the @ndlp
 * from the IOCB response. It also check the newly assigned N_Port ID to the
 * @vport against the previously assigned N_Port ID. If it is different from
 * the previously assigned Destination ID (DID), the lpfc_unreg_rpi() routine
 * is invoked on all the remaining nodes with the @vport to unregister the
 * Remote Port Indicators (RPIs). Finally, the lpfc_issue_fabric_reglogin()
 * is invoked to register login to the fabric.
 *
 * Return code
 *   0 - Success (currently, always return 0)
 **/
static int
lpfc_cmpl_els_flogi_fabric(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			   struct serv_parm *sp, IOCB_t *irsp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovResolution)	/* E_D_TOV ticks are in nanoseconds */
		phba->fc_edtov = (phba->fc_edtov + 999999) / 1000000;

	phba->fc_ratov = (be32_to_cpu(sp->cmn.w2.r_a_tov) + 999) / 1000;

	if (phba->fc_topology == TOPOLOGY_LOOP) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PUBLIC_LOOP;
		spin_unlock_irq(shost->host_lock);
	} else {
		/*
		 * If we are a N-port connected to a Fabric, fixup sparam's so
		 * logins to devices on remote loops work.
		 */
		vport->fc_sparam.cmn.altBbCredit = 1;
	}

	vport->fc_myDID = irsp->un.ulpWord[4] & Mask_DID;
	memcpy(&ndlp->nlp_portname, &sp->portName, sizeof(struct lpfc_name));
	memcpy(&ndlp->nlp_nodename, &sp->nodeName, sizeof(struct lpfc_name));
	ndlp->nlp_class_sup = 0;
	if (sp->cls1.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS1;
	if (sp->cls2.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS2;
	if (sp->cls3.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS3;
	if (sp->cls4.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS4;
	ndlp->nlp_maxframe = ((sp->cmn.bbRcvSizeMsb & 0x0F) << 8) |
				sp->cmn.bbRcvSizeLsb;
	memcpy(&phba->fc_fabparam, sp, sizeof(struct serv_parm));

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		if (sp->cmn.response_multiple_NPort) {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV supported, "
					 "response data 0x%x\n",
					 sp->cmn.response_multiple_NPort);
			phba->link_flag |= LS_NPIV_FAB_SUPPORTED;
		} else {
			/* Because we asked f/w for NPIV it still expects us
			to call reg_vnpid atleast for the physcial host */
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1817 Fabric does not support NPIV "
					 "- configuring single port mode.\n");
			phba->link_flag &= ~LS_NPIV_FAB_SUPPORTED;
		}
	}

	if ((vport->fc_prevDID != vport->fc_myDID) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {

		/* If our NportID changed, we need to ensure all
		 * remaining NPORTs get unreg_login'ed.
		 */
		list_for_each_entry_safe(np, next_np,
					&vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(np))
				continue;
			if ((np->nlp_state != NLP_STE_NPR_NODE) ||
				   !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
		}
		/*
		 * If VPI is unreged, driver need to do INIT_VPI
		 * before re-registering
		 */
		if (phba->sli_rev == LPFC_SLI_REV4) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			spin_unlock_irq(shost->host_lock);
		}
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
			/*
			 * Driver needs to re-reg VPI in order for f/w
			 * to update the MAC address.
			 */
			lpfc_register_new_vport(phba, vport, ndlp);
			return 0;
	}

	if (phba->sli_rev < LPFC_SLI_REV4) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_REG_LOGIN_ISSUE);
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED &&
		    vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
			lpfc_register_new_vport(phba, vport, ndlp);
		else
			lpfc_issue_fabric_reglogin(vport);
	} else {
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
		if ((!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) &&
			(vport->vpi_state & LPFC_VPI_REGISTERED)) {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		} else if (vport->fc_flag & FC_VFI_REGISTERED)
			lpfc_issue_init_vpi(vport);
		else
			lpfc_issue_reg_vfi(vport);
	}
	return 0;
}
/**
 * lpfc_cmpl_els_flogi_nport - Completion function for flogi to an N_Port
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @sp: pointer to service parameter data structure.
 *
 * This routine is invoked by the lpfc_cmpl_els_flogi() completion callback
 * function to handle the completion of a Fabric Login (FLOGI) into an N_Port
 * in a point-to-point topology. First, the @vport's N_Port Name is compared
 * with the received N_Port Name: if the @vport's N_Port Name is greater than
 * the received N_Port Name lexicographically, this node shall assign local
 * N_Port ID (PT2PT_LocalID: 1) and remote N_Port ID (PT2PT_RemoteID: 2) and
 * will send out Port Login (PLOGI) with the N_Port IDs assigned. Otherwise,
 * this node shall just wait for the remote node to issue PLOGI and assign
 * N_Port IDs.
 *
 * Return code
 *   0 - Success
 *   -ENXIO - Fail
 **/
static int
lpfc_cmpl_els_flogi_nport(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			  struct serv_parm *sp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_ratov = FF_DEF_RATOV;
	rc = memcmp(&vport->fc_portname, &sp->portName,
		    sizeof(vport->fc_portname));
	if (rc >= 0) {
		/* This side will initiate the PLOGI */
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT_PLOGI;
		spin_unlock_irq(shost->host_lock);

		/*
		 * N_Port ID cannot be 0, set our to LocalID the other
		 * side will be RemoteID.
		 */

		/* not equal */
		if (rc)
			vport->fc_myDID = PT2PT_LocalID;

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			goto fail;

		lpfc_config_link(phba, mbox);

		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			goto fail;
		}
		/* Decrement ndlp reference count indicating that ndlp can be
		 * safely released when other references to it are done.
		 */
		lpfc_nlp_put(ndlp);

		ndlp = lpfc_findnode_did(vport, PT2PT_RemoteID);
		if (!ndlp) {
			/*
			 * Cannot find existing Fabric ndlp, so allocate a
			 * new one
			 */
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp)
				goto fail;
			lpfc_nlp_init(vport, ndlp, PT2PT_RemoteID);
		} else if (!NLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
			if(!ndlp)
				goto fail;
		}

		memcpy(&ndlp->nlp_portname, &sp->portName,
		       sizeof(struct lpfc_name));
		memcpy(&ndlp->nlp_nodename, &sp->nodeName,
		       sizeof(struct lpfc_name));
		/* Set state will put ndlp onto node list if not already done */
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
	} else
		/* This side will wait for the PLOGI, decrement ndlp reference
		 * count indicating that ndlp can be released when other
		 * references to it are done.
		 */
		lpfc_nlp_put(ndlp);

	/* If we are pt2pt with another NPort, force NPIV off! */
	phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_PT2PT;
	spin_unlock_irq(shost->host_lock);

	/* Start discovery - this should just do CLEAR_LA */
	lpfc_disc_start(vport);
	return 0;
fail:
	return -ENXIO;
}

/**
 * lpfc_cmpl_els_flogi - Completion callback function for flogi
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the top-level completion callback function for issuing
 * a Fabric Login (FLOGI) command. If the response IOCB reported error,
 * the lpfc_els_retry() routine shall be invoked to retry the FLOGI. If
 * retry has been made (either immediately or delayed with lpfc_els_retry()
 * returning 1), the command IOCB will be released and function returned.
 * If the retry attempt has been given up (possibly reach the maximum
 * number of retries), one additional decrement of ndlp reference shall be
 * invoked before going out after releasing the command IOCB. This will
 * actually release the remote node (Note, lpfc_els_free_iocb() will also
 * invoke one decrement of ndlp reference count). If no error reported in
 * the IOCB status, the command Port ID field is used to determine whether
 * this is a point-to-point topology or a fabric topology: if the Port ID
 * field is assigned, it is a fabric topology; otherwise, it is a
 * point-to-point topology. The routine lpfc_cmpl_els_flogi_fabric() or
 * lpfc_cmpl_els_flogi_nport() shall be invoked accordingly to handle the
 * specific topology completion conditions.
 **/
static void
lpfc_cmpl_els_flogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = cmdiocb->context2, *prsp;
	struct serv_parm *sp;
	uint16_t fcf_index;
	int rc;

	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		/* One additional decrement on node reference count to
		 * trigger the release of the node
		 */
		lpfc_nlp_put(ndlp);
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FLOGI cmpl:      status:x%x/x%x state:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		vport->port_state);

	if (irsp->ulpStatus) {
		/*
		 * In case of FIP mode, perform round robin FCF failover
		 * due to new FCF discovery
		 */
		if ((phba->hba_flag & HBA_FIP_SUPPORT) &&
		    (phba->fcf.fcf_flag & FCF_DISCOVERY) &&
		    (irsp->ulpStatus != IOSTAT_LOCAL_REJECT) &&
		    (irsp->un.ulpWord[4] != IOERR_SLI_ABORTED)) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP | LOG_ELS,
					"2611 FLOGI failed on registered "
					"FCF record fcf_index(%d), status: "
					"x%x/x%x, tmo:x%x, trying to perform "
					"round robin failover\n",
					phba->fcf.current_rec.fcf_indx,
					irsp->ulpStatus, irsp->un.ulpWord[4],
					irsp->ulpTimeout);
			fcf_index = lpfc_sli4_fcf_rr_next_index_get(phba);
			if (fcf_index == LPFC_FCOE_FCF_NEXT_NONE) {
				/*
				 * Exhausted the eligible FCF record list,
				 * fail through to retry FLOGI on current
				 * FCF record.
				 */
				lpfc_printf_log(phba, KERN_WARNING,
						LOG_FIP | LOG_ELS,
						"2760 Completed one round "
						"of FLOGI FCF round robin "
						"failover list, retry FLOGI "
						"on currently registered "
						"FCF index:%d\n",
						phba->fcf.current_rec.fcf_indx);
			} else {
				lpfc_printf_log(phba, KERN_INFO,
						LOG_FIP | LOG_ELS,
						"2794 FLOGI FCF round robin "
						"failover to FCF index x%x\n",
						fcf_index);
				rc = lpfc_sli4_fcf_rr_read_fcf_rec(phba,
								   fcf_index);
				if (rc)
					lpfc_printf_log(phba, KERN_WARNING,
							LOG_FIP | LOG_ELS,
							"2761 FLOGI round "
							"robin FCF failover "
							"read FCF failed "
							"rc:x%x, fcf_index:"
							"%d\n", rc,
						phba->fcf.current_rec.fcf_indx);
				else
					goto out;
			}
		}

		/* FLOGI failure */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"2858 FLOGI failure Status:x%x/x%x TMO:x%x\n",
				irsp->ulpStatus, irsp->un.ulpWord[4],
				irsp->ulpTimeout);

		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;

		/* FLOGI failed, so there is no fabric */
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);

		/* If private loop, then allow max outstanding els to be
		 * LPFC_MAX_DISC_THREADS (32). Scanning in the case of no
		 * alpa map would take too long otherwise.
		 */
		if (phba->alpa_map[0] == 0) {
			vport->cfg_discovery_threads = LPFC_MAX_DISC_THREADS;
		}

		/* FLOGI failure */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0100 FLOGI failure Status:x%x/x%x TMO:x%x\n",
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpTimeout);
		goto flogifail;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	spin_unlock_irq(shost->host_lock);

	/*
	 * The FLogI succeeded.  Sync the data for the CPU before
	 * accessing it.
	 */
	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);

	sp = prsp->virt + sizeof(uint32_t);

	/* FLOGI completes successfully */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0101 FLOGI completes successfully "
			 "Data: x%x x%x x%x x%x\n",
			 irsp->un.ulpWord[4], sp->cmn.e_d_tov,
			 sp->cmn.w2.r_a_tov, sp->cmn.edtovResolution);

	if (vport->port_state == LPFC_FLOGI) {
		/*
		 * If Common Service Parameters indicate Nport
		 * we are point to point, if Fport we are Fabric.
		 */
		if (sp->cmn.fPort)
			rc = lpfc_cmpl_els_flogi_fabric(vport, ndlp, sp, irsp);
		else if (!(phba->hba_flag & HBA_FCOE_SUPPORT))
			rc = lpfc_cmpl_els_flogi_nport(vport, ndlp, sp);
		else {
			lpfc_printf_vlog(vport, KERN_ERR,
				LOG_FIP | LOG_ELS,
				"2831 FLOGI response with cleared Fabric "
				"bit fcf_index 0x%x "
				"Switch Name %02x%02x%02x%02x%02x%02x%02x%02x "
				"Fabric Name "
				"%02x%02x%02x%02x%02x%02x%02x%02x\n",
				phba->fcf.current_rec.fcf_indx,
				phba->fcf.current_rec.switch_name[0],
				phba->fcf.current_rec.switch_name[1],
				phba->fcf.current_rec.switch_name[2],
				phba->fcf.current_rec.switch_name[3],
				phba->fcf.current_rec.switch_name[4],
				phba->fcf.current_rec.switch_name[5],
				phba->fcf.current_rec.switch_name[6],
				phba->fcf.current_rec.switch_name[7],
				phba->fcf.current_rec.fabric_name[0],
				phba->fcf.current_rec.fabric_name[1],
				phba->fcf.current_rec.fabric_name[2],
				phba->fcf.current_rec.fabric_name[3],
				phba->fcf.current_rec.fabric_name[4],
				phba->fcf.current_rec.fabric_name[5],
				phba->fcf.current_rec.fabric_name[6],
				phba->fcf.current_rec.fabric_name[7]);
			lpfc_nlp_put(ndlp);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			spin_unlock_irq(&phba->hbalock);
			goto out;
		}
		if (!rc) {
			/* Mark the FCF discovery process done */
			if (phba->hba_flag & HBA_FIP_SUPPORT)
				lpfc_printf_vlog(vport, KERN_INFO, LOG_FIP |
						LOG_ELS,
						"2769 FLOGI successful on FCF "
						"record: current_fcf_index:"
						"x%x, terminate FCF round "
						"robin failover process\n",
						phba->fcf.current_rec.fcf_indx);
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DISCOVERY;
			spin_unlock_irq(&phba->hbalock);
			goto out;
		}
	}

flogifail:
	lpfc_nlp_put(ndlp);

	if (!lpfc_error_lost_link(irsp)) {
		/* FLOGI failed, so just use loop map to make discovery list */
		lpfc_disc_list_loopmap(vport);

		/* Start discovery */
		lpfc_disc_start(vport);
	} else if (((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] != IOERR_SLI_ABORTED) &&
			(irsp->un.ulpWord[4] != IOERR_SLI_DOWN))) &&
			(phba->link_state != LPFC_CLEAR_LA)) {
		/* If FLOGI failed enable link interrupt. */
		lpfc_issue_clear_la(phba, vport);
	}
out:
	lpfc_els_free_iocb(phba, cmdiocb);
}

/**
 * lpfc_issue_els_flogi - Issue an flogi iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Fabric Login (FLOGI) Request ELS command
 * for a @vport. The initiator service parameters are put into the payload
 * of the FLOGI Request IOCB and the top-level callback function pointer
 * to lpfc_cmpl_els_flogi() routine is put to the IOCB completion callback
 * function field. The lpfc_issue_fabric_iocb routine is invoked to send
 * out FLOGI ELS command with one outstanding fabric IOCB at a time.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the FLOGI ELS command.
 *
 * Return code
 *   0 - successfully issued flogi iocb for @vport
 *   1 - failed to issue flogi iocb for @vport
 **/
static int
lpfc_issue_els_flogi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli_ring *pring;
	uint8_t *pcmd;
	uint16_t cmdsize;
	uint32_t tmo;
	int rc;

	pring = &phba->sli.ring[LPFC_ELS_RING];

	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_FLOGI);

	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For FLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_FLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	/* Setup CSPs accordingly for Fabric */
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;
	if (sp->cmn.fcphLow < FC_PH3)
		sp->cmn.fcphLow = FC_PH3;
	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	if  (phba->sli_rev == LPFC_SLI_REV4) {
		elsiocb->iocb.ulpCt_h = ((SLI4_CT_FCFI >> 1) & 1);
		elsiocb->iocb.ulpCt_l = (SLI4_CT_FCFI & 1);
		/* FLOGI needs to be 3 for WQE FCFI */
		/* Set the fcfi to the fcfi we registered with */
		elsiocb->iocb.ulpContext = phba->fcf.fcfi;
	} else if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		sp->cmn.request_multiple_Nport = 1;
		/* For FLOGI, Let FLOGI rsp set the NPortID for VPI 0 */
		icmd->ulpCt_h = 1;
		icmd->ulpCt_l = 0;
	}

	if (phba->fc_topology != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd->un.elsreq64.fl = 1;
	}

	tmo = phba->fc_ratov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set_disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXmitFLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_flogi;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FLOGI:     opt:x%x",
		phba->sli3_options, 0, 0);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_abort_flogi - Abort all outstanding flogi iocbs
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine aborts all the outstanding Fabric Login (FLOGI) IOCBs
 * with a @phba. This routine walks all the outstanding IOCBs on the txcmplq
 * list and issues an abort IOCB commond on each outstanding IOCB that
 * contains a active Fabric_DID ndlp. Note that this function is to issue
 * the abort IOCB command on all the outstanding IOCBs, thus when this
 * function returns, it does not guarantee all the IOCBs are actually aborted.
 *
 * Return code
 *   0 - Successfully issued abort iocb on all outstanding flogis (Always 0)
 **/
int
lpfc_els_abort_flogi(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	struct lpfc_nodelist *ndlp;
	IOCB_t *icmd;

	/* Abort outstanding I/O on NPort <nlp_DID> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"0201 Abort outstanding I/O on NPort x%x\n",
			Fabric_DID);

	pring = &phba->sli.ring[LPFC_ELS_RING];

	/*
	 * Check the txcmplq for an iocb that matches the nport the driver is
	 * searching for.
	 */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		icmd = &iocb->iocb;
		if (icmd->ulpCommand == CMD_ELS_REQUEST64_CR &&
		    icmd->un.elsreq64.bdl.ulpIoTag32) {
			ndlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
			    (ndlp->nlp_DID == Fabric_DID))
				lpfc_sli_issue_abort_iotag(phba, pring, iocb);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	return 0;
}

/**
 * lpfc_initial_flogi - Issue an initial fabric login for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues an initial Fabric Login (FLOGI) for the @vport
 * specified. It first searches the ndlp with the Fabric_DID (0xfffffe) from
 * the @vport's ndlp list. If no such ndlp found, it will create an ndlp and
 * put it into the @vport's ndlp list. If an inactive ndlp found on the list,
 * it will just be enabled and made active. The lpfc_issue_els_flogi() routine
 * is then invoked with the @vport and the ndlp to perform the FLOGI for the
 * @vport.
 *
 * Return code
 *   0 - failed to issue initial flogi for @vport
 *   1 - successfully issued initial flogi for @vport
 **/
int
lpfc_initial_flogi(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	vport->port_state = LPFC_FLOGI;
	lpfc_set_disctmo(vport);

	/* First look for the Fabric ndlp */
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		/* Cannot find existing Fabric ndlp, so allocate a new one */
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		/* Set the node type */
		ndlp->nlp_type |= NLP_FABRIC;
		/* Put ndlp onto node list */
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		/* re-setup ndlp without removing from node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}

	if (lpfc_issue_els_flogi(vport, ndlp, 0))
		/* This decrement of reference count to node shall kick off
		 * the release of the node.
		 */
		lpfc_nlp_put(ndlp);

	return 1;
}

/**
 * lpfc_initial_fdisc - Issue an initial fabric discovery for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues an initial Fabric Discover (FDISC) for the @vport
 * specified. It first searches the ndlp with the Fabric_DID (0xfffffe) from
 * the @vport's ndlp list. If no such ndlp found, it will create an ndlp and
 * put it into the @vport's ndlp list. If an inactive ndlp found on the list,
 * it will just be enabled and made active. The lpfc_issue_els_fdisc() routine
 * is then invoked with the @vport and the ndlp to perform the FDISC for the
 * @vport.
 *
 * Return code
 *   0 - failed to issue initial fdisc for @vport
 *   1 - successfully issued initial fdisc for @vport
 **/
int
lpfc_initial_fdisc(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	/* First look for the Fabric ndlp */
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		/* Cannot find existing Fabric ndlp, so allocate a new one */
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		/* Put ndlp onto node list */
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		/* re-setup ndlp without removing from node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}

	if (lpfc_issue_els_fdisc(vport, ndlp, 0)) {
		/* decrement node reference count to trigger the release of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

/**
 * lpfc_more_plogi - Check and issue remaining plogis for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there are more remaining Port Logins
 * (PLOGI) to be issued for the @vport. If so, it will invoke the routine
 * lpfc_els_disc_plogi() to go through the Node Port Recovery (NPR) nodes
 * to issue ELS PLOGIs up to the configured discover threads with the
 * @vport (@vport->cfg_discovery_threads). The function also decrement
 * the @vport's num_disc_node by 1 if it is not already 0.
 **/
void
lpfc_more_plogi(struct lpfc_vport *vport)
{
	int sentplogi;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;

	/* Continue discovery with <num_disc_nodes> PLOGIs to go */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0232 Continue discovery with %d PLOGIs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_plogi_cnt,
			 vport->fc_flag, vport->port_state);
	/* Check to see if there are more PLOGIs to be sent */
	if (vport->fc_flag & FC_NLP_MORE)
		/* go thru NPR nodes and issue any remaining ELS PLOGIs */
		sentplogi = lpfc_els_disc_plogi(vport);

	return;
}

/**
 * lpfc_plogi_confirm_nport - Confirm pologi wwpn matches stored ndlp
 * @phba: pointer to lpfc hba data structure.
 * @prsp: pointer to response IOCB payload.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine checks and indicates whether the WWPN of an N_Port, retrieved
 * from a PLOGI, matches the WWPN that is stored in the @ndlp for that N_POrt.
 * The following cases are considered N_Port confirmed:
 * 1) The N_Port is a Fabric ndlp; 2) The @ndlp is on vport list and matches
 * the WWPN of the N_Port logged into; 3) The @ndlp is not on vport list but
 * it does not have WWPN assigned either. If the WWPN is confirmed, the
 * pointer to the @ndlp will be returned. If the WWPN is not confirmed:
 * 1) if there is a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the RPI associated with the node; 2) if there is
 * no node found on vport list with the same WWPN of the N_Port PLOGI logged
 * into, a new node shall be allocated (or activated). In either case, the
 * parameters of the @ndlp shall be copied to the new_ndlp, the @ndlp shall
 * be released and the new_ndlp shall be put on to the vport node list and
 * its pointer returned as the confirmed node.
 *
 * Note that before the @ndlp got "released", the keepDID from not-matching
 * or inactive "new_ndlp" on the vport node list is assigned to the nlp_DID
 * of the @ndlp. This is because the release of @ndlp is actually to put it
 * into an inactive state on the vport node list and the vport node list
 * management algorithm does not allow two node with a same DID.
 *
 * Return code
 *   pointer to the PLOGI N_Port @ndlp
 **/
static struct lpfc_nodelist *
lpfc_plogi_confirm_nport(struct lpfc_hba *phba, uint32_t *prsp,
			 struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport    *vport = ndlp->vport;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rport_data *rdata;
	struct fc_rport *rport;
	struct serv_parm *sp;
	uint8_t  name[sizeof(struct lpfc_name)];
	uint32_t rc, keepDID = 0;
	int  put_node;
	int  put_rport;

	/* Fabric nodes can have the same WWPN so we don't bother searching
	 * by WWPN.  Just return the ndlp that was given to us.
	 */
	if (ndlp->nlp_type & NLP_FABRIC)
		return ndlp;

	sp = (struct serv_parm *) ((uint8_t *) prsp + sizeof(uint32_t));
	memset(name, 0, sizeof(struct lpfc_name));

	/* Now we find out if the NPort we are logging into, matches the WWPN
	 * we have for that ndlp. If not, we have some work to do.
	 */
	new_ndlp = lpfc_findnode_wwpn(vport, &sp->portName);

	if (new_ndlp == ndlp && NLP_CHK_NODE_ACT(new_ndlp))
		return ndlp;

	if (!new_ndlp) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpfc_nlp_init(vport, new_ndlp, ndlp->nlp_DID);
	} else if (!NLP_CHK_NODE_ACT(new_ndlp)) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = lpfc_enable_node(vport, new_ndlp,
						NLP_STE_UNUSED_NODE);
		if (!new_ndlp)
			return ndlp;
		keepDID = new_ndlp->nlp_DID;
	} else
		keepDID = new_ndlp->nlp_DID;

	lpfc_unreg_rpi(vport, new_ndlp);
	new_ndlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_prev_state = ndlp->nlp_prev_state;

	if (ndlp->nlp_flag & NLP_NPR_2B_DISC)
		new_ndlp->nlp_flag |= NLP_NPR_2B_DISC;
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;

	/* Set state will put new_ndlp on to node list if not already done */
	lpfc_nlp_set_state(vport, new_ndlp, ndlp->nlp_state);

	/* Move this back to NPR state */
	if (memcmp(&ndlp->nlp_portname, name, sizeof(struct lpfc_name)) == 0) {
		/* The new_ndlp is replacing ndlp totally, so we need
		 * to put ndlp on UNUSED list and try to free it.
		 */

		/* Fix up the rport accordingly */
		rport =  ndlp->rport;
		if (rport) {
			rdata = rport->dd_data;
			if (rdata->pnode == ndlp) {
				lpfc_nlp_put(ndlp);
				ndlp->rport = NULL;
				rdata->pnode = lpfc_nlp_get(new_ndlp);
				new_ndlp->rport = rport;
			}
			new_ndlp->nlp_type = ndlp->nlp_type;
		}
		/* We shall actually free the ndlp with both nlp_DID and
		 * nlp_portname fields equals 0 to avoid any ndlp on the
		 * nodelist never to be used.
		 */
		if (ndlp->nlp_DID == 0) {
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
		}

		/* Two ndlps cannot have the same did on the nodelist */
		ndlp->nlp_DID = keepDID;
		lpfc_drop_node(vport, ndlp);
	}
	else {
		lpfc_unreg_rpi(vport, ndlp);
		/* Two ndlps cannot have the same did */
		ndlp->nlp_DID = keepDID;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		/* Since we are swapping the ndlp passed in with the new one
		 * and the did has already been swapped, copy over the
		 * state and names.
		 */
		memcpy(&new_ndlp->nlp_portname, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(&new_ndlp->nlp_nodename, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		new_ndlp->nlp_state = ndlp->nlp_state;
		/* Fix up the rport accordingly */
		rport = ndlp->rport;
		if (rport) {
			rdata = rport->dd_data;
			put_node = rdata->pnode != NULL;
			put_rport = ndlp->rport != NULL;
			rdata->pnode = NULL;
			ndlp->rport = NULL;
			if (put_node)
				lpfc_nlp_put(ndlp);
			if (put_rport)
				put_device(&rport->dev);
		}
	}
	return new_ndlp;
}

/**
 * lpfc_end_rscn - Check and handle more rscn for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether more Registration State Change
 * Notifications (RSCNs) came in while the discovery state machine was in
 * the FC_RSCN_MODE. If so, the lpfc_els_handle_rscn() routine will be
 * invoked to handle the additional RSCNs for the @vport. Otherwise, the
 * FC_RSCN_MODE bit will be cleared with the @vport to mark as the end of
 * handling the RSCNs.
 **/
void
lpfc_end_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_RSCN_MODE) {
		/*
		 * Check to see if more RSCNs came in while we were
		 * processing this one.
		 */
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	}
}

/**
 * lpfc_cmpl_els_plogi - Completion callback function for plogi
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for issuing the Port
 * Login (PLOGI) command. For PLOGI completion, there must be an active
 * ndlp on the vport node list that matches the remote node ID from the
 * PLOGI reponse IOCB. If such ndlp does not exist, the PLOGI is simply
 * ignored and command IOCB released. The PLOGI response IOCB status is
 * checked for error conditons. If there is error status reported, PLOGI
 * retry shall be attempted by invoking the lpfc_els_retry() routine.
 * Otherwise, the lpfc_plogi_confirm_nport() routine shall be invoked on
 * the ndlp and the NLP_EVT_CMPL_PLOGI state to the Discover State Machine
 * (DSM) is set for this PLOGI completion. Finally, it checks whether
 * there are additional N_Port nodes with the vport that need to perform
 * PLOGI. If so, the lpfc_more_plogi() routine is invoked to issue addition
 * PLOGIs.
 **/
static void
lpfc_cmpl_els_plogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *prsp;
	int disc, rc, did, type;

	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PLOGI cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);

	ndlp = lpfc_findnode_did(vport, irsp->un.elsreq64.remoteID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0136 PLOGI completes to NPort x%x "
				 "with no ndlp. Data: x%x x%x x%x\n",
				 irsp->un.elsreq64.remoteID,
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpIoTag);
		goto out;
	}

	/* Since ndlp can be freed in the disc state machine, note if this node
	 * is being used during discovery.
	 */
	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
	spin_unlock_irq(shost->host_lock);
	rc   = 0;

	/* PLOGI completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0102 PLOGI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	/* ndlp could be freed in DSM, save these values now */
	type = ndlp->nlp_type;
	did = ndlp->nlp_DID;

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
			}
			goto out;
		}
		/* PLOGI failed Don't print the vport to vport rjts */
		if (irsp->ulpStatus != IOSTAT_LS_RJT ||
			(((irsp->un.ulpWord[4]) >> 16 != LSRJT_INVALID_CMD) &&
			((irsp->un.ulpWord[4]) >> 16 != LSRJT_UNABLE_TPC)) ||
			(phba)->pport->cfg_log_verbose & LOG_ELS)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2753 PLOGI failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			rc = NLP_STE_FREED_NODE;
		else
			rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						     NLP_EVT_CMPL_PLOGI);
	} else {
		/* Good status, call state machine */
		prsp = list_entry(((struct lpfc_dmabuf *)
				   cmdiocb->context2)->list.next,
				  struct lpfc_dmabuf, list);
		ndlp = lpfc_plogi_confirm_nport(phba, prsp->virt, ndlp);
		rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					     NLP_EVT_CMPL_PLOGI);
	}

	if (disc && vport->num_disc_nodes) {
		/* Check to see if there are more PLOGIs to be sent */
		lpfc_more_plogi(vport);

		if (vport->num_disc_nodes == 0) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_NDISC_ACTIVE;
			spin_unlock_irq(shost->host_lock);

			lpfc_can_disctmo(vport);
			lpfc_end_rscn(vport);
		}
	}

out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_plogi - Issue an plogi iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @did: destination port identifier.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Port Login (PLOGI) command to a remote N_Port
 * (with the @did) for a @vport. Before issuing a PLOGI to a remote N_Port,
 * the ndlp with the remote N_Port DID must exist on the @vport's ndlp list.
 * This routine constructs the proper feilds of the PLOGI IOCB and invokes
 * the lpfc_sli_issue_iocb() routine to send out PLOGI ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PLOGI ELS command.
 *
 * Return code
 *   0 - Successfully issued a plogi for @vport
 *   1 - failed to issue a plogi for @vport
 **/
int
lpfc_issue_els_plogi(struct lpfc_vport *vport, uint32_t did, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;

	psli = &phba->sli;

	ndlp = lpfc_findnode_did(vport, did);
	if (ndlp && !NLP_CHK_NODE_ACT(ndlp))
		ndlp = NULL;

	/* If ndlp is not NULL, we will bump the reference count on it */
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_PLOGI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For PLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_PLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	if (sp->cmn.fcphLow < FC_PH_4_3)
		sp->cmn.fcphLow = FC_PH_4_3;

	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PLOGI:     did:x%x",
		did, 0, 0);

	phba->fc_stat.elsXmitPLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_plogi;
	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (ret == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_prli - Completion callback function for prli
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for a Process Login
 * (PRLI) ELS command. The PRLI response IOCB status is checked for error
 * status. If there is error status reported, PRLI retry shall be attempted
 * by invoking the lpfc_els_retry() routine. Otherwise, the state
 * NLP_EVT_CMPL_PRLI is sent to the Discover State Machine (DSM) for this
 * ndlp to mark the PRLI completion.
 **/
static void
lpfc_cmpl_els_prli(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PRLI cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	/* PRLI completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0103 PRLI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);

	vport->fc_prli_sent--;
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			goto out;
		}
		/* PRLI failed */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2754 PRLI failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_PRLI);
	} else
		/* Good status, call state machine */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_PRLI);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_prli - Issue a prli iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Process Login (PRLI) ELS command for the
 * @vport. The PRLI service parameters are set up in the payload of the
 * PRLI Request command and the pointer to lpfc_cmpl_els_prli() routine
 * is put to the IOCB completion callback func field before invoking the
 * routine lpfc_sli_issue_iocb() to send out PRLI command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PRLI ELS command.
 *
 * Return code
 *   0 - successfully issued prli iocb command for @vport
 *   1 - failed to issue prli iocb command for @vport
 **/
int
lpfc_issue_els_prli(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba *phba = vport->phba;
	PRLI *npr;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(PRLI));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_PRLI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For PRLI request, remainder of payload is service parameters */
	memset(pcmd, 0, (sizeof(PRLI) + sizeof(uint32_t)));
	*((uint32_t *) (pcmd)) = ELS_CMD_PRLI;
	pcmd += sizeof(uint32_t);

	/* For PRLI, remainder of payload is PRLI parameter page */
	npr = (PRLI *) pcmd;
	/*
	 * If our firmware version is 3.20 or later,
	 * set the following bits for FC-TAPE support.
	 */
	if (phba->vpd.rev.feaLevelHigh >= 0x02) {
		npr->ConfmComplAllowed = 1;
		npr->Retry = 1;
		npr->TaskRetryIdReq = 1;
	}
	npr->estabImagePair = 1;
	npr->readXferRdyDis = 1;

	/* For FCP support */
	npr->prliType = PRLI_FCP_TYPE;
	npr->initiatorFunc = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PRLI:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitPRLI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_prli;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_PRLI_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	vport->fc_prli_sent++;
	return 0;
}

/**
 * lpfc_rscn_disc - Perform rscn discovery for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine performs Registration State Change Notification (RSCN)
 * discovery for a @vport. If the @vport's node port recovery count is not
 * zero, it will invoke the lpfc_els_disc_plogi() to perform PLOGI for all
 * the nodes that need recovery. If none of the PLOGI were needed through
 * the lpfc_els_disc_plogi() routine, the lpfc_end_rscn() routine shall be
 * invoked to check and handle possible more RSCN came in during the period
 * of processing the current ones.
 **/
static void
lpfc_rscn_disc(struct lpfc_vport *vport)
{
	lpfc_can_disctmo(vport);

	/* RSCN discovery */
	/* go thru NPR nodes and issue ELS PLOGIs */
	if (vport->fc_npr_cnt)
		if (lpfc_els_disc_plogi(vport))
			return;

	lpfc_end_rscn(vport);
}

/**
 * lpfc_adisc_done - Complete the adisc phase of discovery
 * @vport: pointer to lpfc_vport hba data structure that finished all ADISCs.
 *
 * This function is called when the final ADISC is completed during discovery.
 * This function handles clearing link attention or issuing reg_vpi depending
 * on whether npiv is enabled. This function also kicks off the PLOGI phase of
 * discovery.
 * This function is called with no locks held.
 **/
static void
lpfc_adisc_done(struct lpfc_vport *vport)
{
	struct Scsi_Host   *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba   *phba = vport->phba;

	/*
	 * For NPIV, cmpl_reg_vpi will set port_state to READY,
	 * and continue discovery.
	 */
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_RSCN_MODE) &&
	    (phba->sli_rev < LPFC_SLI_REV4)) {
		lpfc_issue_reg_vpi(phba, vport);
		return;
	}
	/*
	* For SLI2, we need to set port_state to READY
	* and continue discovery.
	*/
	if (vport->port_state < LPFC_VPORT_READY) {
		/* If we get here, there is nothing to ADISC */
		if (vport->port_type == LPFC_PHYSICAL_PORT)
			lpfc_issue_clear_la(phba, vport);
		if (!(vport->fc_flag & FC_ABORT_DISCOVERY)) {
			vport->num_disc_nodes = 0;
			/* go thru NPR list, issue ELS PLOGIs */
			if (vport->fc_npr_cnt)
				lpfc_els_disc_plogi(vport);
			if (!vport->num_disc_nodes) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag &= ~FC_NDISC_ACTIVE;
				spin_unlock_irq(shost->host_lock);
				lpfc_can_disctmo(vport);
				lpfc_end_rscn(vport);
			}
		}
		vport->port_state = LPFC_VPORT_READY;
	} else
		lpfc_rscn_disc(vport);
}

/**
 * lpfc_more_adisc - Issue more adisc as needed
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine determines whether there are more ndlps on a @vport
 * node list need to have Address Discover (ADISC) issued. If so, it will
 * invoke the lpfc_els_disc_adisc() routine to issue ADISC on the @vport's
 * remaining nodes which need to have ADISC sent.
 **/
void
lpfc_more_adisc(struct lpfc_vport *vport)
{
	int sentadisc;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;
	/* Continue discovery with <num_disc_nodes> ADISCs to go */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0210 Continue discovery with %d ADISCs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_adisc_cnt,
			 vport->fc_flag, vport->port_state);
	/* Check to see if there are more ADISCs to be sent */
	if (vport->fc_flag & FC_NLP_MORE) {
		lpfc_set_disctmo(vport);
		/* go thru NPR nodes and issue any remaining ELS ADISCs */
		sentadisc = lpfc_els_disc_adisc(vport);
	}
	if (!vport->num_disc_nodes)
		lpfc_adisc_done(vport);
	return;
}

/**
 * lpfc_cmpl_els_adisc - Completion callback function for adisc
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion function for issuing the Address Discover
 * (ADISC) command. It first checks to see whether link went down during
 * the discovery process. If so, the node will be marked as node port
 * recovery for issuing discover IOCB by the link attention handler and
 * exit. Otherwise, the response status is checked. If error was reported
 * in the response status, the ADISC command shall be retried by invoking
 * the lpfc_els_retry() routine. Otherwise, if no error was reported in
 * the response status, the state machine is invoked to set transition
 * with respect to NLP_EVT_CMPL_ADISC event.
 **/
static void
lpfc_cmpl_els_adisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	int  disc;

	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ADISC cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);

	/* Since ndlp can be freed in the disc state machine, note if this node
	 * is being used during discovery.
	 */
	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~(NLP_ADISC_SND | NLP_NPR_2B_DISC);
	spin_unlock_irq(shost->host_lock);
	/* ADISC completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0104 ADISC completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
				lpfc_set_disctmo(vport);
			}
			goto out;
		}
		/* ADISC failed */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2755 ADISC failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (!lpfc_error_lost_link(irsp))
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_ADISC);
	} else
		/* Good status, call state machine */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_ADISC);

	/* Check to see if there are more ADISCs to be sent */
	if (disc && vport->num_disc_nodes)
		lpfc_more_adisc(vport);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_adisc - Issue an address discover iocb to an node on a vport
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues an Address Discover (ADISC) for an @ndlp on a
 * @vport. It prepares the payload of the ADISC ELS command, updates the
 * and states of the ndlp, and invokes the lpfc_sli_issue_iocb() routine
 * to issue the ADISC ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the ADISC ELS command.
 *
 * Return code
 *   0 - successfully issued adisc
 *   1 - failed to issue adisc
 **/
int
lpfc_issue_els_adisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(ADISC));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ADISC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For ADISC request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_ADISC;
	pcmd += sizeof(uint32_t);

	/* Fill in ADISC payload */
	ap = (ADISC *) pcmd;
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue ADISC:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitADISC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_adisc;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_ADISC_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_ADISC_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_logo - Completion callback function for logo
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion function for issuing the ELS Logout (LOGO)
 * command. If no error status was reported from the LOGO response, the
 * state machine of the associated ndlp shall be invoked for transition with
 * respect to NLP_EVT_CMPL_LOGO event. Otherwise, if error status was reported,
 * the lpfc_els_retry() routine will be invoked to retry the LOGO command.
 **/
static void
lpfc_cmpl_els_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;

	psli = &phba->sli;
	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	/* LOGO completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0105 LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (ndlp->nlp_flag & NLP_TARGET_REMOVE) {
	        /* NLP_EVT_DEVICE_RM should unregister the RPI
		 * which should abort all outstanding IOs.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
		goto out;
	}

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			/* ELS command is being retried */
			goto out;
		/* LOGO failed */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2756 LOGO failure DID:%06X Status:x%x/x%x\n",
				 ndlp->nlp_DID, irsp->ulpStatus,
				 irsp->un.ulpWord[4]);
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_LOGO);
	} else
		/* Good status, call state machine.
		 * This will unregister the rpi if needed.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_LOGO);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_logo - Issue a logo to an node on a vport
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine constructs and issues an ELS Logout (LOGO) iocb command
 * to a remote node, referred by an @ndlp on a @vport. It constructs the
 * payload of the IOCB, properly sets up the @ndlp state, and invokes the
 * lpfc_sli_issue_iocb() routine to send out the LOGO ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the LOGO ELS command.
 *
 * Return code
 *   0 - successfully issued logo
 *   1 - failed to issue logo
 **/
int
lpfc_issue_els_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	spin_lock_irq(shost->host_lock);
	if (ndlp->nlp_flag & NLP_LOGO_SND) {
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	spin_unlock_irq(shost->host_lock);

	cmdsize = (2 * sizeof(uint32_t)) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	/* Fill in LOGO payload */
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitLOGO++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_logo;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (rc == IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_cmd - Completion callback function for generic els command
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is a generic completion callback function for ELS commands.
 * Specifically, it is the callback function which does not need to perform
 * any command specific operations. It is currently used by the ELS command
 * issuing routines for the ELS State Change  Request (SCR),
 * lpfc_issue_els_scr(), and the ELS Fibre Channel Address Resolution
 * Protocol Response (FARPR) routine, lpfc_issue_els_farpr(). Other than
 * certain debug loggings, this callback function simply invokes the
 * lpfc_els_chk_latt() routine to check whether link went down during the
 * discovery process.
 **/
static void
lpfc_cmpl_els_cmd(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);
	/* ELS cmd tag <ulpIoTag> completes */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0106 ELS cmd tag x%x completes Data: x%x x%x x%x\n",
			 irsp->ulpIoTag, irsp->ulpStatus,
			 irsp->un.ulpWord[4], irsp->ulpTimeout);
	/* Check to see if link went down during discovery */
	lpfc_els_chk_latt(vport);
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_scr - Issue a scr to an node on a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @nportid: N_Port identifier to the remote node.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a State Change Request (SCR) to a fabric node
 * on a @vport. The remote node @nportid is passed into the function. It
 * first search the @vport node list to find the matching ndlp. If no such
 * ndlp is found, a new ndlp shall be created for this (SCR) purpose. An
 * IOCB is allocated, payload prepared, and the lpfc_sli_issue_iocb()
 * routine is invoked to send the SCR IOCB.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the SCR ELS command.
 *
 * Return code
 *   0 - Successfully issued scr command
 *   1 - Failed to issue scr command
 **/
int
lpfc_issue_els_scr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(SCR));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_SCR);

	if (!elsiocb) {
		/* This will trigger the release of the node just
		 * allocated
		 */
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_SCR;
	pcmd += sizeof(uint32_t);

	/* For SCR, remainder of payload is SCR parameter page */
	memset(pcmd, 0, sizeof(SCR));
	((SCR *) pcmd)->Function = SCR_FUNC_FULL;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue SCR:       did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitSCR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		/* The additional lpfc_nlp_put will cause the following
		 * lpfc_els_free_iocb routine to trigger the rlease of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	/* This will cause the callback-function lpfc_cmpl_els_cmd to
	 * trigger the release of node.
	 */
	lpfc_nlp_put(ndlp);
	return 0;
}

/**
 * lpfc_issue_els_farpr - Issue a farp to an node on a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @nportid: N_Port identifier to the remote node.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Fibre Channel Address Resolution Response
 * (FARPR) to a node on a vport. The remote node N_Port identifier (@nportid)
 * is passed into the function. It first search the @vport node list to find
 * the matching ndlp. If no such ndlp is found, a new ndlp shall be created
 * for this (FARPR) purpose. An IOCB is allocated, payload prepared, and the
 * lpfc_sli_issue_iocb() routine is invoked to send the FARPR ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PARPR ELS command.
 *
 * Return code
 *   0 - Successfully issued farpr command
 *   1 - Failed to issue farpr command
 **/
static int
lpfc_issue_els_farpr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	FARP *fp;
	uint8_t *pcmd;
	uint32_t *lp;
	uint16_t cmdsize;
	struct lpfc_nodelist *ondlp;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(FARP));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_RNID);
	if (!elsiocb) {
		/* This will trigger the release of the node just
		 * allocated
		 */
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_FARPR;
	pcmd += sizeof(uint32_t);

	/* Fill in FARPR payload */
	fp = (FARP *) (pcmd);
	memset(fp, 0, sizeof(FARP));
	lp = (uint32_t *) pcmd;
	*lp++ = be32_to_cpu(nportid);
	*lp++ = be32_to_cpu(vport->fc_myDID);
	fp->Rflags = 0;
	fp->Mflags = (FARP_MATCH_PORT | FARP_MATCH_NODE);

	memcpy(&fp->RportName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&fp->RnodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ondlp = lpfc_findnode_did(vport, nportid);
	if (ondlp && NLP_CHK_NODE_ACT(ondlp)) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fp->OnodeName, &ondlp->nlp_nodename,
		       sizeof(struct lpfc_name));
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FARPR:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitFARPR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		/* The additional lpfc_nlp_put will cause the following
		 * lpfc_els_free_iocb routine to trigger the release of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	/* This will cause the callback-function lpfc_cmpl_els_cmd to
	 * trigger the release of the node.
	 */
	lpfc_nlp_put(ndlp);
	return 0;
}

/**
 * lpfc_cancel_retry_delay_tmo - Cancel the timer with delayed iocb-cmd retry
 * @vport: pointer to a host virtual N_Port data structure.
 * @nlp: pointer to a node-list data structure.
 *
 * This routine cancels the timer with a delayed IOCB-command retry for
 * a @vport's @ndlp. It stops the timer for the delayed function retrial and
 * removes the ELS retry event if it presents. In addition, if the
 * NLP_NPR_2B_DISC bit is set in the @nlp's nlp_flag bitmap, ADISC IOCB
 * commands are sent for the @vport's nodes that require issuing discovery
 * ADISC.
 **/
void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_work_evt *evtp;

	if (!(nlp->nlp_flag & NLP_DELAY_TMO))
		return;
	spin_lock_irq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	del_timer_sync(&nlp->nlp_delayfunc);
	nlp->nlp_last_elscmd = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		/* Decrement nlp reference count held for the delayed retry */
		evtp = &nlp->els_retry_evt;
		lpfc_nlp_put((struct lpfc_nodelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		spin_lock_irq(shost->host_lock);
		nlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		if (vport->num_disc_nodes) {
			if (vport->port_state < LPFC_VPORT_READY) {
				/* Check if there are more ADISCs to be sent */
				lpfc_more_adisc(vport);
			} else {
				/* Check if there are more PLOGIs to be sent */
				lpfc_more_plogi(vport);
				if (vport->num_disc_nodes == 0) {
					spin_lock_irq(shost->host_lock);
					vport->fc_flag &= ~FC_NDISC_ACTIVE;
					spin_unlock_irq(shost->host_lock);
					lpfc_can_disctmo(vport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

/**
 * lpfc_els_retry_delay - Timer function with a ndlp delayed function timer
 * @ptr: holder for the pointer to the timer function associated data (ndlp).
 *
 * This routine is invoked by the ndlp delayed-function timer to check
 * whether there is any pending ELS retry event(s) with the node. If not, it
 * simply returns. Otherwise, if there is at least one ELS delayed event, it
 * adds the delayed events to the HBA work list and invokes the
 * lpfc_worker_wake_up() routine to wake up worker thread to process the
 * event. Note that lpfc_nlp_get() is called before posting the event to
 * the work list to hold reference count of ndlp so that it guarantees the
 * reference to ndlp will still be available when the worker thread gets
 * to the event associated with the ndlp.
 **/
void
lpfc_els_retry_delay(unsigned long ptr)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) ptr;
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_hba   *phba = vport->phba;
	unsigned long flags;
	struct lpfc_work_evt  *evtp = &ndlp->els_retry_evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	if (!list_empty(&evtp->evt_listp)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	/* We need to hold the node by incrementing the reference
	 * count until the queued work is done
	 */
	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (evtp->evt_arg1) {
		evtp->evt = LPFC_EVT_ELS_RETRY;
		list_add_tail(&evtp->evt_listp, &phba->work_list);
		lpfc_worker_wake_up(phba);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return;
}

/**
 * lpfc_els_retry_delay_handler - Work thread handler for ndlp delayed function
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine is the worker-thread handler for processing the @ndlp delayed
 * event(s), posted by the lpfc_els_retry_delay() routine. It simply retrieves
 * the last ELS command from the associated ndlp and invokes the proper ELS
 * function according to the delayed ELS command to retry the command.
 **/
void
lpfc_els_retry_delay_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	uint32_t cmd, did, retry;

	spin_lock_irq(shost->host_lock);
	did = ndlp->nlp_DID;
	cmd = ndlp->nlp_last_elscmd;
	ndlp->nlp_last_elscmd = 0;

	if (!(ndlp->nlp_flag & NLP_DELAY_TMO)) {
		spin_unlock_irq(shost->host_lock);
		return;
	}

	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	/*
	 * If a discovery event readded nlp_delayfunc after timer
	 * firing and before processing the timer, cancel the
	 * nlp_delayfunc.
	 */
	del_timer_sync(&ndlp->nlp_delayfunc);
	retry = ndlp->nlp_retry;
	ndlp->nlp_retry = 0;

	switch (cmd) {
	case ELS_CMD_FLOGI:
		lpfc_issue_els_flogi(vport, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
		if (!lpfc_issue_els_plogi(vport, ndlp->nlp_DID, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		break;
	case ELS_CMD_ADISC:
		if (!lpfc_issue_els_adisc(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
		}
		break;
	case ELS_CMD_PRLI:
		if (!lpfc_issue_els_prli(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
		}
		break;
	case ELS_CMD_LOGO:
		if (!lpfc_issue_els_logo(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		}
		break;
	case ELS_CMD_FDISC:
		lpfc_issue_els_fdisc(vport, ndlp, retry);
		break;
	}
	return;
}

/**
 * lpfc_els_retry - Make retry decision on an els command iocb
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine makes a retry decision on an ELS command IOCB, which has
 * failed. The following ELS IOCBs use this function for retrying the command
 * when previously issued command responsed with error status: FLOGI, PLOGI,
 * PRLI, ADISC, LOGO, and FDISC. Based on the ELS command type and the
 * returned error status, it makes the decision whether a retry shall be
 * issued for the command, and whether a retry shall be made immediately or
 * delayed. In the former case, the corresponding ELS command issuing-function
 * is called to retry the command. In the later case, the ELS command shall
 * be posted to the ndlp delayed event and delayed function timer set to the
 * ndlp for the delayed command issusing.
 *
 * Return code
 *   0 - No retry of els command is made
 *   1 - Immediate or delayed retry of els command is made
 **/
static int
lpfc_els_retry(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	uint32_t *elscmd;
	struct ls_rjt stat;
	int retry = 0, maxretry = lpfc_max_els_tries, delay = 0;
	int logerr = 0;
	uint32_t cmd = 0;
	uint32_t did;


	/* Note: context2 may be 0 for internal driver abort
	 * of delays ELS command.
	 */

	if (pcmd && pcmd->virt) {
		elscmd = (uint32_t *) (pcmd->virt);
		cmd = *elscmd++;
	}

	if (ndlp && NLP_CHK_NODE_ACT(ndlp))
		did = ndlp->nlp_DID;
	else {
		/* We should only hit this case for retrying PLOGI */
		did = irsp->un.elsreq64.remoteID;
		ndlp = lpfc_findnode_did(vport, did);
		if ((!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		    && (cmd != ELS_CMD_PLOGI))
			return 1;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Retry ELS:       wd7:x%x wd4:x%x did:x%x",
		*(((uint32_t *) irsp) + 7), irsp->un.ulpWord[4], ndlp->nlp_DID);

	switch (irsp->ulpStatus) {
	case IOSTAT_FCP_RSP_ERROR:
	case IOSTAT_REMOTE_STOP:
		break;

	case IOSTAT_LOCAL_REJECT:
		switch ((irsp->un.ulpWord[4] & 0xff)) {
		case IOERR_LOOP_OPEN_FAILURE:
			if (cmd == ELS_CMD_FLOGI) {
				if (PCI_DEVICE_ID_HORNET ==
					phba->pcidev->device) {
					phba->fc_topology = TOPOLOGY_LOOP;
					phba->pport->fc_myDID = 0;
					phba->alpa_map[0] = 0;
					phba->alpa_map[1] = 0;
				}
			}
			if (cmd == ELS_CMD_PLOGI && cmdiocb->retry == 0)
				delay = 1000;
			retry = 1;
			break;

		case IOERR_ILLEGAL_COMMAND:
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0124 Retry illegal cmd x%x "
					 "retry:x%x delay:x%x\n",
					 cmd, cmdiocb->retry, delay);
			retry = 1;
			/* All command's retry policy */
			maxretry = 8;
			if (cmdiocb->retry > 2)
				delay = 1000;
			break;

		case IOERR_NO_RESOURCES:
			logerr = 1; /* HBA out of resources */
			retry = 1;
			if (cmdiocb->retry > 100)
				delay = 100;
			maxretry = 250;
			break;

		case IOERR_ILLEGAL_FRAME:
			delay = 100;
			retry = 1;
			break;

		case IOERR_SEQUENCE_TIMEOUT:
		case IOERR_INVALID_RPI:
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_RJT:
	case IOSTAT_FABRIC_RJT:
		if (irsp->un.ulpWord[4] & RJT_UNAVAIL_TEMP) {
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_BSY:
	case IOSTAT_FABRIC_BSY:
		logerr = 1; /* Fabric / Remote NPort out of resources */
		retry = 1;
		break;

	case IOSTAT_LS_RJT:
		stat.un.lsRjtError = be32_to_cpu(irsp->un.ulpWord[4]);
		/* Added for Vendor specifc support
		 * Just keep retrying for these Rsn / Exp codes
		 */
		switch (stat.un.b.lsRjtRsnCode) {
		case LSRJT_UNABLE_TPC:
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_CMD_IN_PROGRESS) {
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			}
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_CANT_GIVE_DATA) {
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			}
			if (cmd == ELS_CMD_PLOGI) {
				delay = 1000;
				maxretry = lpfc_max_els_tries + 1;
				retry = 1;
				break;
			}
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_OUT_OF_RESOURCE)){
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0125 FDISC Failed (x%x). "
						 "Fabric out of resources\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_NO_FABRIC_RSCS);
			}
			break;

		case LSRJT_LOGICAL_BSY:
			if ((cmd == ELS_CMD_PLOGI) ||
			    (cmd == ELS_CMD_PRLI)) {
				delay = 1000;
				maxretry = 48;
			} else if (cmd == ELS_CMD_FDISC) {
				/* FDISC retry policy */
				maxretry = 48;
				if (cmdiocb->retry >= 32)
					delay = 1000;
			}
			retry = 1;
			break;

		case LSRJT_LOGICAL_ERR:
			/* There are some cases where switches return this
			 * error when they are not ready and should be returning
			 * Logical Busy. We should delay every time.
			 */
			if (cmd == ELS_CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeExp == LSEXP_PORT_LOGIN_REQ) {
				maxretry = 3;
				delay = 1000;
				retry = 1;
				break;
			}
		case LSRJT_PROTOCOL_ERR:
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  ((stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0122 FDISC Failed (x%x). "
						 "Fabric Detected Bad WWN\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_FABRIC_REJ_WWN);
			}
			break;
		}
		break;

	case IOSTAT_INTERMED_RSP:
	case IOSTAT_BA_RJT:
		break;

	default:
		break;
	}

	if (did == FDMI_DID)
		retry = 1;

	if (((cmd == ELS_CMD_FLOGI) || (cmd == ELS_CMD_FDISC)) &&
	    (phba->fc_topology != TOPOLOGY_LOOP) &&
	    !lpfc_error_lost_link(irsp)) {
		/* FLOGI retry policy */
		retry = 1;
		/* retry forever */
		maxretry = 0;
		if (cmdiocb->retry >= 100)
			delay = 5000;
		else if (cmdiocb->retry >= 32)
			delay = 1000;
	}

	cmdiocb->retry++;
	if (maxretry && (cmdiocb->retry >= maxretry)) {
		phba->fc_stat.elsRetryExceeded++;
		retry = 0;
	}

	if ((vport->load_flag & FC_UNLOADING) != 0)
		retry = 0;

	if (retry) {
		if ((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_FDISC)) {
			/* Stop retrying PLOGI and FDISC if in FCF discovery */
			if (phba->fcf.fcf_flag & FCF_DISCOVERY) {
				lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
						 "2849 Stop retry ELS command "
						 "x%x to remote NPORT x%x, "
						 "Data: x%x x%x\n", cmd, did,
						 cmdiocb->retry, delay);
				return 0;
			}
		}

		/* Retry ELS command <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0107 Retry ELS command x%x to remote "
				 "NPORT x%x Data: x%x x%x\n",
				 cmd, did, cmdiocb->retry, delay);

		if (((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_ADISC)) &&
			((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] & 0xff) != IOERR_NO_RESOURCES))) {
			/* Don't reset timer for no resources */

			/* If discovery / RSCN timer is running, reset it */
			if (timer_pending(&vport->fc_disctmo) ||
			    (vport->fc_flag & FC_RSCN_MODE))
				lpfc_set_disctmo(vport);
		}

		phba->fc_stat.elsXmitRetry++;
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) && delay) {
			phba->fc_stat.elsDelayRetry++;
			ndlp->nlp_retry = cmdiocb->retry;

			/* delay is specified in milliseconds */
			mod_timer(&ndlp->nlp_delayfunc,
				jiffies + msecs_to_jiffies(delay));
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(shost->host_lock);

			ndlp->nlp_prev_state = ndlp->nlp_state;
			if (cmd == ELS_CMD_PRLI)
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_REG_LOGIN_ISSUE);
			else
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_NPR_NODE);
			ndlp->nlp_last_elscmd = cmd;

			return 1;
		}
		switch (cmd) {
		case ELS_CMD_FLOGI:
			lpfc_issue_els_flogi(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_FDISC:
			lpfc_issue_els_fdisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PLOGI:
			if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
			}
			lpfc_issue_els_plogi(vport, did, cmdiocb->retry);
			return 1;
		case ELS_CMD_ADISC:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PRLI:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
			lpfc_issue_els_prli(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_LOGO:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
			lpfc_issue_els_logo(vport, ndlp, cmdiocb->retry);
			return 1;
		}
	}
	/* No retry ELS command <elsCmd> to remote NPORT <did> */
	if (logerr) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "0137 No retry ELS command x%x to remote "
			 "NPORT x%x: Out of Resources: Error:x%x/%x\n",
			 cmd, did, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0108 No retry ELS command x%x to remote "
			 "NPORT x%x Retried:%d Error:x%x/%x\n",
			 cmd, did, cmdiocb->retry, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	return 0;
}

/**
 * lpfc_els_free_data - Free lpfc dma buffer and data structure with an iocb
 * @phba: pointer to lpfc hba data structure.
 * @buf_ptr1: pointer to the lpfc DMA buffer data structure.
 *
 * This routine releases the lpfc DMA (Direct Memory Access) buffer(s)
 * associated with a command IOCB back to the lpfc DMA buffer pool. It first
 * checks to see whether there is a lpfc DMA buffer associated with the
 * response of the command IOCB. If so, it will be released before releasing
 * the lpfc DMA buffer associated with the IOCB itself.
 *
 * Return code
 *   0 - Successfully released lpfc DMA buffer (currently, always return 0)
 **/
static int
lpfc_els_free_data(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr1)
{
	struct lpfc_dmabuf *buf_ptr;

	/* Free the response before processing the command. */
	if (!list_empty(&buf_ptr1->list)) {
		list_remove_head(&buf_ptr1->list, buf_ptr,
				 struct lpfc_dmabuf,
				 list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}
	lpfc_mbuf_free(phba, buf_ptr1->virt, buf_ptr1->phys);
	kfree(buf_ptr1);
	return 0;
}

/**
 * lpfc_els_free_bpl - Free lpfc dma buffer and data structure with bpl
 * @phba: pointer to lpfc hba data structure.
 * @buf_ptr: pointer to the lpfc dma buffer data structure.
 *
 * This routine releases the lpfc Direct Memory Access (DMA) buffer
 * associated with a Buffer Pointer List (BPL) back to the lpfc DMA buffer
 * pool.
 *
 * Return code
 *   0 - Successfully released lpfc DMA buffer (currently, always return 0)
 **/
static int
lpfc_els_free_bpl(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr)
{
	lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
	kfree(buf_ptr);
	return 0;
}

/**
 * lpfc_els_free_iocb - Free a command iocb and its associated resources
 * @phba: pointer to lpfc hba data structure.
 * @elsiocb: pointer to lpfc els command iocb data structure.
 *
 * This routine frees a command IOCB and its associated resources. The
 * command IOCB data structure contains the reference to various associated
 * resources, these fields must be set to NULL if the associated reference
 * not present:
 *   context1 - reference to ndlp
 *   context2 - reference to cmd
 *   context2->next - reference to rsp
 *   context3 - reference to bpl
 *
 * It first properly decrements the reference count held on ndlp for the
 * IOCB completion callback function. If LPFC_DELAY_MEM_FREE flag is not
 * set, it invokes the lpfc_els_free_data() routine to release the Direct
 * Memory Access (DMA) buffers associated with the IOCB. Otherwise, it
 * adds the DMA buffer the @phba data structure for the delayed release.
 * If reference to the Buffer Pointer List (BPL) is present, the
 * lpfc_els_free_bpl() routine is invoked to release the DMA memory
 * associated with BPL. Finally, the lpfc_sli_release_iocbq() routine is
 * invoked to release the IOCB data structure back to @phba IOCBQ list.
 *
 * Return code
 *   0 - Success (currently, always return 0)
 **/
int
lpfc_els_free_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *elsiocb)
{
	struct lpfc_dmabuf *buf_ptr, *buf_ptr1;
	struct lpfc_nodelist *ndlp;

	ndlp = (struct lpfc_nodelist *)elsiocb->context1;
	if (ndlp) {
		if (ndlp->nlp_flag & NLP_DEFER_RM) {
			lpfc_nlp_put(ndlp);

			/* If the ndlp is not being used by another discovery
			 * thread, free it.
			 */
			if (!lpfc_nlp_not_used(ndlp)) {
				/* If ndlp is being used by another discovery
				 * thread, just clear NLP_DEFER_RM
				 */
				ndlp->nlp_flag &= ~NLP_DEFER_RM;
			}
		}
		else
			lpfc_nlp_put(ndlp);
		elsiocb->context1 = NULL;
	}
	/* context2  = cmd,  context2->next = rsp, context3 = bpl */
	if (elsiocb->context2) {
		if (elsiocb->iocb_flag & LPFC_DELAY_MEM_FREE) {
			/* Firmware could still be in progress of DMAing
			 * payload, so don't free data buffer till after
			 * a hbeat.
			 */
			elsiocb->iocb_flag &= ~LPFC_DELAY_MEM_FREE;
			buf_ptr = elsiocb->context2;
			elsiocb->context2 = NULL;
			if (buf_ptr) {
				buf_ptr1 = NULL;
				spin_lock_irq(&phba->hbalock);
				if (!list_empty(&buf_ptr->list)) {
					list_remove_head(&buf_ptr->list,
						buf_ptr1, struct lpfc_dmabuf,
						list);
					INIT_LIST_HEAD(&buf_ptr1->list);
					list_add_tail(&buf_ptr1->list,
						&phba->elsbuf);
					phba->elsbuf_cnt++;
				}
				INIT_LIST_HEAD(&buf_ptr->list);
				list_add_tail(&buf_ptr->list, &phba->elsbuf);
				phba->elsbuf_cnt++;
				spin_unlock_irq(&phba->hbalock);
			}
		} else {
			buf_ptr1 = (struct lpfc_dmabuf *) elsiocb->context2;
			lpfc_els_free_data(phba, buf_ptr1);
		}
	}

	if (elsiocb->context3) {
		buf_ptr = (struct lpfc_dmabuf *) elsiocb->context3;
		lpfc_els_free_bpl(phba, buf_ptr);
	}
	lpfc_sli_release_iocbq(phba, elsiocb);
	return 0;
}

/**
 * lpfc_cmpl_els_logo_acc - Completion callback function to logo acc response
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function to the Logout (LOGO)
 * Accept (ACC) Response ELS command. This routine is invoked to indicate
 * the completion of the LOGO process. It invokes the lpfc_nlp_not_used() to
 * release the ndlp if it has the last reference remaining (reference count
 * is 1). If succeeded (meaning ndlp released), it sets the IOCB context1
 * field to NULL to inform the following lpfc_els_free_iocb() routine no
 * ndlp reference count needs to be decremented. Otherwise, the ndlp
 * reference use-count shall be decremented by the lpfc_els_free_iocb()
 * routine. Finally, the lpfc_els_free_iocb() is invoked to release the
 * IOCB data structure.
 **/
static void
lpfc_cmpl_els_logo_acc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ACC LOGO cmpl:   status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], ndlp->nlp_DID);
	/* ACC to LOGO completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0109 ACC to LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);

	if (ndlp->nlp_state == NLP_STE_NPR_NODE) {
		/* NPort Recovery mode or node is just allocated */
		if (!lpfc_nlp_not_used(ndlp)) {
			/* If the ndlp is being used by another discovery
			 * thread, just unregister the RPI.
			 */
			lpfc_unreg_rpi(vport, ndlp);
		} else {
			/* Indicate the node has already released, should
			 * not reference to it from within lpfc_els_free_iocb.
			 */
			cmdiocb->context1 = NULL;
		}
	}
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_mbx_cmpl_dflt_rpi - Completion callbk func for unreg dflt rpi mbox cmd
 * @phba: pointer to lpfc hba data structure.
 * @pmb: pointer to the driver internal queue element for mailbox command.
 *
 * This routine is the completion callback function for unregister default
 * RPI (Remote Port Index) mailbox command to the @phba. It simply releases
 * the associated lpfc Direct Memory Access (DMA) buffer back to the pool and
 * decrements the ndlp reference count held for this completion callback
 * function. After that, it invokes the lpfc_nlp_not_used() to check
 * whether there is only one reference left on the ndlp. If so, it will
 * perform one more decrement and trigger the release of the ndlp.
 **/
void
lpfc_mbx_cmpl_dflt_rpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;

	/*
	 * This routine is used to register and unregister in previous SLI
	 * modes.
	 */
	if ((pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) &&
	    (phba->sli_rev == LPFC_SLI_REV4))
		lpfc_sli4_free_rpi(phba, pmb->u.mb.un.varUnregLogin.rpi);

	pmb->context1 = NULL;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_nlp_put(ndlp);
		/* This is the end of the default RPI cleanup logic for this
		 * ndlp. If no other discovery threads are using this ndlp.
		 * we should free all resources associated with it.
		 */
		lpfc_nlp_not_used(ndlp);
	}

	return;
}

/**
 * lpfc_cmpl_els_rsp - Completion callback function for els response iocb cmd
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for ELS Response IOCB
 * command. In normal case, this callback function just properly sets the
 * nlp_flag bitmap in the ndlp data structure, if the mbox command reference
 * field in the command IOCB is not NULL, the referred mailbox command will
 * be send out, and then invokes the lpfc_els_free_iocb() routine to release
 * the IOCB. Under error conditions, such as when a LS_RJT is returned or a
 * link down event occurred during the discovery, the lpfc_nlp_not_used()
 * routine shall be invoked trying to release the ndlp if no other threads
 * are currently referring it.
 **/
static void
lpfc_cmpl_els_rsp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp ? ndlp->vport : NULL;
	struct Scsi_Host  *shost = vport ? lpfc_shost_from_vport(vport) : NULL;
	IOCB_t  *irsp;
	uint8_t *pcmd;
	LPFC_MBOXQ_t *mbox = NULL;
	struct lpfc_dmabuf *mp = NULL;
	uint32_t ls_rjt = 0;

	irsp = &rspiocb->iocb;

	if (cmdiocb->context_un.mbox)
		mbox = cmdiocb->context_un.mbox;

	/* First determine if this is a LS_RJT cmpl. Note, this callback
	 * function can have cmdiocb->contest1 (ndlp) field set to NULL.
	 */
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) cmdiocb->context2)->virt);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    (*((uint32_t *) (pcmd)) == ELS_CMD_LS_RJT)) {
		/* A LS_RJT associated with Default RPI cleanup has its own
		 * separate code path.
		 */
		if (!(ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			ls_rjt = 1;
	}

	/* Check to see if link went down during discovery */
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) || lpfc_els_chk_latt(vport)) {
		if (mbox) {
			mp = (struct lpfc_dmabuf *) mbox->context1;
			if (mp) {
				lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			mempool_free(mbox, phba->mbox_mem_pool);
		}
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
		    (ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			if (lpfc_nlp_not_used(ndlp)) {
				ndlp = NULL;
				/* Indicate the node has already released,
				 * should not reference to it from within
				 * the routine lpfc_els_free_iocb.
				 */
				cmdiocb->context1 = NULL;
			}
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ELS rsp cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		cmdiocb->iocb.un.elsreq64.remoteID);
	/* ELS response tag <ulpIoTag> completes */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0110 ELS response tag x%x completes "
			 "Data: x%x x%x x%x x%x x%x x%x x%x\n",
			 cmdiocb->iocb.ulpIoTag, rspiocb->iocb.ulpStatus,
			 rspiocb->iocb.un.ulpWord[4], rspiocb->iocb.ulpTimeout,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	if (mbox) {
		if ((rspiocb->iocb.ulpStatus == 0)
		    && (ndlp->nlp_flag & NLP_ACC_REGLOGIN)) {
			lpfc_unreg_rpi(vport, ndlp);
			/* Increment reference count to ndlp to hold the
			 * reference to ndlp for the callback function.
			 */
			mbox->context2 = lpfc_nlp_get(ndlp);
			mbox->vport = vport;
			if (ndlp->nlp_flag & NLP_RM_DFLT_RPI) {
				mbox->mbox_flag |= LPFC_MBX_IMED_UNREG;
				mbox->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			}
			else {
				mbox->mbox_cmpl = lpfc_mbx_cmpl_reg_login;
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
					   NLP_STE_REG_LOGIN_ISSUE);
			}
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			    != MBX_NOT_FINISHED)
				goto out;
			else
				/* Decrement the ndlp reference count we
				 * set for this failed mailbox command.
				 */
				lpfc_nlp_put(ndlp);

			/* ELS rsp: Cannot issue reg_login for <NPortid> */
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0138 ELS rsp: Cannot issue reg_login for x%x "
				"Data: x%x x%x x%x\n",
				ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
				ndlp->nlp_rpi);

			if (lpfc_nlp_not_used(ndlp)) {
				ndlp = NULL;
				/* Indicate node has already been released,
				 * should not reference to it from within
				 * the routine lpfc_els_free_iocb.
				 */
				cmdiocb->context1 = NULL;
			}
		} else {
			/* Do not drop node for lpfc_els_abort'ed ELS cmds */
			if (!lpfc_error_lost_link(irsp) &&
			    ndlp->nlp_flag & NLP_ACC_REGLOGIN) {
				if (lpfc_nlp_not_used(ndlp)) {
					ndlp = NULL;
					/* Indicate node has already been
					 * released, should not reference
					 * to it from within the routine
					 * lpfc_els_free_iocb.
					 */
					cmdiocb->context1 = NULL;
				}
			}
		}
		mp = (struct lpfc_dmabuf *) mbox->context1;
		if (mp) {
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
		mempool_free(mbox, phba->mbox_mem_pool);
	}
out:
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~(NLP_ACC_REGLOGIN | NLP_RM_DFLT_RPI);
		spin_unlock_irq(shost->host_lock);

		/* If the node is not being used by another discovery thread,
		 * and we are sending a reject, we are done with it.
		 * Release driver reference count here and free associated
		 * resources.
		 */
		if (ls_rjt)
			if (lpfc_nlp_not_used(ndlp))
				/* Indicate node has already been released,
				 * should not reference to it from within
				 * the routine lpfc_els_free_iocb.
				 */
				cmdiocb->context1 = NULL;
	}

	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_els_rsp_acc - Prepare and issue an acc response iocb command
 * @vport: pointer to a host virtual N_Port data structure.
 * @flag: the els command code to be accepted.
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 * @mbox: pointer to the driver internal queue element for mailbox command.
 *
 * This routine prepares and issues an Accept (ACC) response IOCB
 * command. It uses the @flag to properly set up the IOCB field for the
 * specific ACC response command to be issued and invokes the
 * lpfc_sli_issue_iocb() routine to send out ACC response IOCB. If a
 * @mbox pointer is passed in, it will be put into the context_un.mbox
 * field of the IOCB for the completion callback function to issue the
 * mailbox command to the HBA later when callback is invoked.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the corresponding response ELS IOCB command.
 *
 * Return code
 *   0 - Successfully issued acc response
 *   1 - Failed to issue acc response
 **/
int
lpfc_els_rsp_acc(struct lpfc_vport *vport, uint32_t flag,
		 struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		 LPFC_MBOXQ_t *mbox)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;
	ELS_PKT *els_pkt_ptr;

	psli = &phba->sli;
	oldcmd = &oldiocb->iocb;

	switch (flag) {
	case ELS_CMD_ACC:
		cmdsize = sizeof(uint32_t);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb) {
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag &= ~NLP_LOGO_ACC;
			spin_unlock_irq(shost->host_lock);
			return 1;
		}

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;	/* Xri */
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC:       did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_PLOGI:
		cmdsize = (sizeof(struct serv_parm) + sizeof(uint32_t));
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_ACC);
		if (!elsiocb)
			return 1;

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext;	/* Xri */
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

		if (mbox)
			elsiocb->context_un.mbox = mbox;

		*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
		pcmd += sizeof(uint32_t);
		memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC PLOGI: did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	case ELS_CMD_PRLO:
		cmdsize = sizeof(uint32_t) + sizeof(PRLO);
		elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry,
					     ndlp, ndlp->nlp_DID, ELS_CMD_PRLO);
		if (!elsiocb)
			return 1;

		icmd = &elsiocb->iocb;
		icmd->ulpContext = oldcmd->ulpContext; /* Xri */
		pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

		memcpy(pcmd, ((struct lpfc_dmabuf *) oldiocb->context2)->virt,
		       sizeof(uint32_t) + sizeof(PRLO));
		*((uint32_t *) (pcmd)) = ELS_CMD_PRLO_ACC;
		els_pkt_ptr = (ELS_PKT *) pcmd;
		els_pkt_ptr->un.prlo.acceptRspCode = PRLO_REQ_EXECUTED;

		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
			"Issue ACC PRLO:  did:x%x flg:x%x",
			ndlp->nlp_DID, ndlp->nlp_flag, 0);
		break;
	default:
		return 1;
	}
	/* Xmit ELS ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0128 Xmit ELS ACC response tag x%x, XRI: x%x, "
			 "DID: x%x, nlp_flag: x%x nlp_state: x%x RPI: x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	if (ndlp->nlp_flag & NLP_LOGO_ACC) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_ACC;
		spin_unlock_irq(shost->host_lock);
		elsiocb->iocb_cmpl = lpfc_cmpl_els_logo_acc;
	} else {
		elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	}

	phba->fc_stat.elsXmitACC++;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_rsp_reject - Propare and issue a rjt response iocb command
 * @vport: pointer to a virtual N_Port data structure.
 * @rejectError:
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 * @mbox: pointer to the driver internal queue element for mailbox command.
 *
 * This routine prepares and issue an Reject (RJT) response IOCB
 * command. If a @mbox pointer is passed in, it will be put into the
 * context_un.mbox field of the IOCB for the completion callback function
 * to issue to the HBA later.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the reject response ELS IOCB command.
 *
 * Return code
 *   0 - Successfully issued reject response
 *   1 - Failed to issue reject response
 **/
int
lpfc_els_rsp_reject(struct lpfc_vport *vport, uint32_t rejectError,
		    struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp,
		    LPFC_MBOXQ_t *mbox)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;
	cmdsize = 2 * sizeof(uint32_t);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LS_RJT);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	/* Xri */
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_LS_RJT;
	pcmd += sizeof(uint32_t);
	*((uint32_t *) (pcmd)) = rejectError;

	if (mbox)
		elsiocb->context_un.mbox = mbox;

	/* Xmit ELS RJT <err> response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0129 Xmit ELS RJT x%x response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 rejectError, elsiocb->iotag,
			 elsiocb->iocb.ulpContext, ndlp->nlp_DID,
			 ndlp->nlp_flag, ndlp->nlp_state, ndlp->nlp_rpi);
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue LS_RJT:    did:x%x flg:x%x err:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, rejectError);

	phba->fc_stat.elsXmitLSRJT++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_rsp_adisc_acc - Prepare and issue acc response to adisc iocb cmd
 * @vport: pointer to a virtual N_Port data structure.
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine prepares and issues an Accept (ACC) response to Address
 * Discover (ADISC) ELS command. It simply prepares the payload of the IOCB
 * and invokes the lpfc_sli_issue_iocb() routine to send out the command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the ADISC Accept response ELS IOCB command.
 *
 * Return code
 *   0 - Successfully issued acc adisc response
 *   1 - Failed to issue adisc acc response
 **/
int
lpfc_els_rsp_adisc_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		       struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd, *oldcmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	cmdsize = sizeof(uint32_t) + sizeof(ADISC);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	/* Xri */

	/* Xmit ADISC ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0130 Xmit ADISC ACC response iotag x%x xri: "
			 "x%x, did x%x, nlp_flag x%x, nlp_state x%x rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	ap = (ADISC *) (pcmd);
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC ADISC: did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_rsp_prli_acc - Prepare and issue acc response to prli iocb cmd
 * @vport: pointer to a virtual N_Port data structure.
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine prepares and issues an Accept (ACC) response to Process
 * Login (PRLI) ELS command. It simply prepares the payload of the IOCB
 * and invokes the lpfc_sli_issue_iocb() routine to send out the command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PRLI Accept response ELS IOCB command.
 *
 * Return code
 *   0 - Successfully issued acc prli response
 *   1 - Failed to issue acc prli response
 **/
int
lpfc_els_rsp_prli_acc(struct lpfc_vport *vport, struct lpfc_iocbq *oldiocb,
		      struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	PRLI *npr;
	lpfc_vpd_t *vpd;
	IOCB_t *icmd;
	IOCB_t *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;

	cmdsize = sizeof(uint32_t) + sizeof(PRLI);
	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
		ndlp->nlp_DID, (ELS_CMD_ACC | (ELS_CMD_PRLI & ~ELS_RSP_MASK)));
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	/* Xri */
	/* Xmit PRLI ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0131 Xmit PRLI ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = (ELS_CMD_ACC | (ELS_CMD_PRLI & ~ELS_RSP_MASK));
	pcmd += sizeof(uint32_t);

	/* For PRLI, remainder of payload is PRLI parameter page */
	memset(pcmd, 0, sizeof(PRLI));

	npr = (PRLI *) pcmd;
	vpd = &phba->vpd;
	/*
	 * If the remote port is a target and our firmware version is 3.20 or
	 * later, set the following bits for FC-TAPE support.
	 */
	if ((ndlp->nlp_type & NLP_FCP_TARGET) &&
	    (vpd->rev.feaLevelHigh >= 0x02)) {
		npr->ConfmComplAllowed = 1;
		npr->Retry = 1;
		npr->TaskRetryIdReq = 1;
	}

	npr->acceptRspCode = PRLI_REQ_EXECUTED;
	npr->estabImagePair = 1;
	npr->readXferRdyDis = 1;
	npr->ConfmComplAllowed = 1;

	npr->prliType = PRLI_FCP_TYPE;
	npr->initiatorFunc = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC PRLI:  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_rsp_rnid_acc - Issue rnid acc response iocb command
 * @vport: pointer to a virtual N_Port data structure.
 * @format: rnid command format.
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine issues a Request Node Identification Data (RNID) Accept
 * (ACC) response. It constructs the RNID ACC response command according to
 * the proper @format and then calls the lpfc_sli_issue_iocb() routine to
 * issue the response. Note that this command does not need to hold the ndlp
 * reference count for the callback. So, the ndlp reference count taken by
 * the lpfc_prep_els_iocb() routine is put back and the context1 field of
 * IOCB is set to NULL to indicate to the lpfc_els_free_iocb() routine that
 * there is no ndlp reference available.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function. However, for the RNID Accept Response ELS command,
 * this is undone later by this routine after the IOCB is allocated.
 *
 * Return code
 *   0 - Successfully issued acc rnid response
 *   1 - Failed to issue acc rnid response
 **/
static int
lpfc_els_rsp_rnid_acc(struct lpfc_vport *vport, uint8_t format,
		      struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	RNID *rn;
	IOCB_t *icmd, *oldcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	psli = &phba->sli;
	cmdsize = sizeof(uint32_t) + sizeof(uint32_t)
					+ (2 * sizeof(struct lpfc_name));
	if (format)
		cmdsize += sizeof(RNID_TOP_DISC);

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	/* Xri */
	/* Xmit RNID ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0132 Xmit RNID ACC response tag x%x xri x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext);
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t);

	memset(pcmd, 0, sizeof(RNID));
	rn = (RNID *) (pcmd);
	rn->Format = format;
	rn->CommonLen = (2 * sizeof(struct lpfc_name));
	memcpy(&rn->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&rn->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	switch (format) {
	case 0:
		rn->SpecificLen = 0;
		break;
	case RNID_TOPOLOGY_DISC:
		rn->SpecificLen = sizeof(RNID_TOP_DISC);
		memcpy(&rn->un.topologyDisc.portName,
		       &vport->fc_portname, sizeof(struct lpfc_name));
		rn->un.topologyDisc.unitType = RNID_HBA;
		rn->un.topologyDisc.physPort = 0;
		rn->un.topologyDisc.attachedNodes = 0;
		break;
	default:
		rn->CommonLen = 0;
		rn->SpecificLen = 0;
		break;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"Issue ACC RNID:  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	phba->fc_stat.elsXmitACC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	lpfc_nlp_put(ndlp);
	elsiocb->context1 = NULL;  /* Don't need ndlp for cmpl,
				    * it could be freed */

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_disc_adisc - Issue remaining adisc iocbs to npr nodes of a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues Address Discover (ADISC) ELS commands to those
 * N_Ports which are in node port recovery state and ADISC has not been issued
 * for the @vport. Each time an ELS ADISC IOCB is issued by invoking the
 * lpfc_issue_els_adisc() routine, the per @vport number of discover count
 * (num_disc_nodes) shall be incremented. If the num_disc_nodes reaches a
 * pre-configured threshold (cfg_discovery_threads), the @vport fc_flag will
 * be marked with FC_NLP_MORE bit and the process of issuing remaining ADISC
 * IOCBs quit for later pick up. On the other hand, after walking through
 * all the ndlps with the @vport and there is none ADISC IOCB issued, the
 * @vport fc_flag shall be cleared with FC_NLP_MORE bit indicating there is
 * no more ADISC need to be sent.
 *
 * Return code
 *    The number of N_Ports with adisc issued.
 **/
int
lpfc_els_disc_adisc(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentadisc = 0;

	/* go thru NPR nodes and issue any remaining ELS ADISCs */
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_NPR_NODE &&
		    (ndlp->nlp_flag & NLP_NPR_2B_DISC) != 0 &&
		    (ndlp->nlp_flag & NLP_NPR_ADISC) != 0) {
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, 0);
			sentadisc++;
			vport->num_disc_nodes++;
			if (vport->num_disc_nodes >=
			    vport->cfg_discovery_threads) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag |= FC_NLP_MORE;
				spin_unlock_irq(shost->host_lock);
				break;
			}
		}
	}
	if (sentadisc == 0) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentadisc;
}

/**
 * lpfc_els_disc_plogi - Issue plogi for all npr nodes of a vport before adisc
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues Port Login (PLOGI) ELS commands to all the N_Ports
 * which are in node port recovery state, with a @vport. Each time an ELS
 * ADISC PLOGI IOCB is issued by invoking the lpfc_issue_els_plogi() routine,
 * the per @vport number of discover count (num_disc_nodes) shall be
 * incremented. If the num_disc_nodes reaches a pre-configured threshold
 * (cfg_discovery_threads), the @vport fc_flag will be marked with FC_NLP_MORE
 * bit set and quit the process of issuing remaining ADISC PLOGIN IOCBs for
 * later pick up. On the other hand, after walking through all the ndlps with
 * the @vport and there is none ADISC PLOGI IOCB issued, the @vport fc_flag
 * shall be cleared with the FC_NLP_MORE bit indicating there is no more ADISC
 * PLOGI need to be sent.
 *
 * Return code
 *   The number of N_Ports with plogi issued.
 **/
int
lpfc_els_disc_plogi(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int sentplogi = 0;

	/* go thru NPR nodes and issue any remaining ELS PLOGIs */
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_NPR_NODE &&
		    (ndlp->nlp_flag & NLP_NPR_2B_DISC) != 0 &&
		    (ndlp->nlp_flag & NLP_DELAY_TMO) == 0 &&
		    (ndlp->nlp_flag & NLP_NPR_ADISC) == 0) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
			lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			sentplogi++;
			vport->num_disc_nodes++;
			if (vport->num_disc_nodes >=
			    vport->cfg_discovery_threads) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag |= FC_NLP_MORE;
				spin_unlock_irq(shost->host_lock);
				break;
			}
		}
	}
	if (sentplogi) {
		lpfc_set_disctmo(vport);
	}
	else {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NLP_MORE;
		spin_unlock_irq(shost->host_lock);
	}
	return sentplogi;
}

/**
 * lpfc_els_flush_rscn - Clean up any rscn activities with a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine cleans up any Registration State Change Notification
 * (RSCN) activity with a @vport. Note that the fc_rscn_flush flag of the
 * @vport together with the host_lock is used to prevent multiple thread
 * trying to access the RSCN array on a same @vport at the same time.
 **/
void
lpfc_els_flush_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	int i;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		/* Another thread is walking fc_rscn_id_list on this vport */
		spin_unlock_irq(shost->host_lock);
		return;
	}
	/* Indicate we are walking lpfc_els_flush_rscn on this vport */
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);

	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lpfc_in_buf_free(phba, vport->fc_rscn_id_list[i]);
		vport->fc_rscn_id_list[i] = NULL;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_rscn_id_cnt = 0;
	vport->fc_flag &= ~(FC_RSCN_MODE | FC_RSCN_DISCOVERY);
	spin_unlock_irq(shost->host_lock);
	lpfc_can_disctmo(vport);
	/* Indicate we are done walking this fc_rscn_id_list */
	vport->fc_rscn_flush = 0;
}

/**
 * lpfc_rscn_payload_check - Check whether there is a pending rscn to a did
 * @vport: pointer to a host virtual N_Port data structure.
 * @did: remote destination port identifier.
 *
 * This routine checks whether there is any pending Registration State
 * Configuration Notification (RSCN) to a @did on @vport.
 *
 * Return code
 *   None zero - The @did matched with a pending rscn
 *   0 - not able to match @did with a pending rscn
 **/
int
lpfc_rscn_payload_check(struct lpfc_vport *vport, uint32_t did)
{
	D_ID ns_did;
	D_ID rscn_did;
	uint32_t *lp;
	uint32_t payload_len, i;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	ns_did.un.word = did;

	/* Never match fabric nodes for RSCNs */
	if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
		return 0;

	/* If we are doing a FULL RSCN rediscovery, match everything */
	if (vport->fc_flag & FC_RSCN_DISCOVERY)
		return did;

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		/* Another thread is walking fc_rscn_id_list on this vport */
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	/* Indicate we are walking fc_rscn_id_list on this vport */
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	for (i = 0; i < vport->fc_rscn_id_cnt; i++) {
		lp = vport->fc_rscn_id_list[i]->virt;
		payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
		payload_len -= sizeof(uint32_t);	/* take off word 0 */
		while (payload_len) {
			rscn_did.un.word = be32_to_cpu(*lp++);
			payload_len -= sizeof(uint32_t);
			switch (rscn_did.un.b.resv & RSCN_ADDRESS_FORMAT_MASK) {
			case RSCN_ADDRESS_FORMAT_PORT:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area)
				    && (ns_did.un.b.id == rscn_did.un.b.id))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_AREA:
				if ((ns_did.un.b.domain == rscn_did.un.b.domain)
				    && (ns_did.un.b.area == rscn_did.un.b.area))
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_DOMAIN:
				if (ns_did.un.b.domain == rscn_did.un.b.domain)
					goto return_did_out;
				break;
			case RSCN_ADDRESS_FORMAT_FABRIC:
				goto return_did_out;
			}
		}
	}
	/* Indicate we are done with walking fc_rscn_id_list on this vport */
	vport->fc_rscn_flush = 0;
	return 0;
return_did_out:
	/* Indicate we are done with walking fc_rscn_id_list on this vport */
	vport->fc_rscn_flush = 0;
	return did;
}

/**
 * lpfc_rscn_recovery_check - Send recovery event to vport nodes matching rscn
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine sends recovery (NLP_EVT_DEVICE_RECOVERY) event to the
 * state machine for a @vport's nodes that are with pending RSCN (Registration
 * State Change Notification).
 *
 * Return code
 *   0 - Successful (currently alway return 0)
 **/
static int
lpfc_rscn_recovery_check(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL;

	/* Move all affected nodes by pending RSCNs to NPR state. */
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp) ||
		    (ndlp->nlp_state == NLP_STE_UNUSED_NODE) ||
		    !lpfc_rscn_payload_check(vport, ndlp->nlp_DID))
			continue;
		lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
	}
	return 0;
}

/**
 * lpfc_send_rscn_event - Send an RSCN event to management application
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 *
 * lpfc_send_rscn_event sends an RSCN netlink event to management
 * applications.
 */
static void
lpfc_send_rscn_event(struct lpfc_vport *vport,
		struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_dmabuf *pcmd;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	uint32_t *payload_ptr;
	uint32_t payload_len;
	struct lpfc_rscn_event_header *rscn_event_data;

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	payload_ptr = (uint32_t *) pcmd->virt;
	payload_len = be32_to_cpu(*payload_ptr & ~ELS_CMD_MASK);

	rscn_event_data = kmalloc(sizeof(struct lpfc_rscn_event_header) +
		payload_len, GFP_KERNEL);
	if (!rscn_event_data) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			"0147 Failed to allocate memory for RSCN event\n");
		return;
	}
	rscn_event_data->event_type = FC_REG_RSCN_EVENT;
	rscn_event_data->payload_length = payload_len;
	memcpy(rscn_event_data->rscn_payload, payload_ptr,
		payload_len);

	fc_host_post_vendor_event(shost,
		fc_get_event_number(),
		sizeof(struct lpfc_els_event_header) + payload_len,
		(char *)rscn_event_data,
		LPFC_NL_VENDOR_ID);

	kfree(rscn_event_data);
}

/**
 * lpfc_els_rcv_rscn - Process an unsolicited rscn iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes an unsolicited RSCN (Registration State Change
 * Notification) IOCB. First, the payload of the unsolicited RSCN is walked
 * to invoke fc_host_post_event() routine to the FC transport layer. If the
 * discover state machine is about to begin discovery, it just accepts the
 * RSCN and the discovery process will satisfy the RSCN. If this RSCN only
 * contains N_Port IDs for other vports on this HBA, it just accepts the
 * RSCN and ignore processing it. If the state machine is in the recovery
 * state, the fc_rscn_id_list of this @vport is walked and the
 * lpfc_rscn_recovery_check() routine is invoked to send recovery event for
 * all nodes that match RSCN payload. Otherwise, the lpfc_els_handle_rscn()
 * routine is invoked to handle the RSCN event.
 *
 * Return code
 *   0 - Just sent the acc response
 *   1 - Sent the acc response and waited for name server completion
 **/
static int
lpfc_els_rcv_rscn(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp, *datap;
	IOCB_t *icmd;
	uint32_t payload_len, length, nportid, *cmd;
	int rscn_cnt;
	int rscn_id = 0, hba_id = 0;
	int i;

	icmd = &cmdiocb->iocb;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	payload_len = be32_to_cpu(*lp++ & ~ELS_CMD_MASK);
	payload_len -= sizeof(uint32_t);	/* take off word 0 */
	/* RSCN received */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0214 RSCN received Data: x%x x%x x%x x%x\n",
			 vport->fc_flag, payload_len, *lp,
			 vport->fc_rscn_id_cnt);

	/* Send an RSCN event to the management application */
	lpfc_send_rscn_event(vport, cmdiocb);

	for (i = 0; i < payload_len/sizeof(uint32_t); i++)
		fc_host_post_event(shost, fc_get_event_number(),
			FCH_EVT_RSCN, lp[i]);

	/* If we are about to begin discovery, just ACC the RSCN.
	 * Discovery processing will satisfy it.
	 */
	if (vport->port_state <= LPFC_NS_QRY) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN ignore: did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}

	/* If this RSCN just contains NPortIDs for other vports on this HBA,
	 * just ACC and ignore it.
	 */
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		!(vport->cfg_peer_port_login)) {
		i = payload_len;
		datap = lp;
		while (i > 0) {
			nportid = *datap++;
			nportid = ((be32_to_cpu(nportid)) & Mask_DID);
			i -= sizeof(uint32_t);
			rscn_id++;
			if (lpfc_find_vport_by_did(phba, nportid))
				hba_id++;
		}
		if (rscn_id == hba_id) {
			/* ALL NPortIDs in RSCN are on HBA */
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0219 Ignore RSCN "
					 "Data: x%x x%x x%x x%x\n",
					 vport->fc_flag, payload_len,
					 *lp, vport->fc_rscn_id_cnt);
			lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
				"RCV RSCN vport:  did:x%x/ste:x%x flg:x%x",
				ndlp->nlp_DID, vport->port_state,
				ndlp->nlp_flag);

			lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb,
				ndlp, NULL);
			return 0;
		}
	}

	spin_lock_irq(shost->host_lock);
	if (vport->fc_rscn_flush) {
		/* Another thread is walking fc_rscn_id_list on this vport */
		vport->fc_flag |= FC_RSCN_DISCOVERY;
		spin_unlock_irq(shost->host_lock);
		/* Send back ACC */
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		return 0;
	}
	/* Indicate we are walking fc_rscn_id_list on this vport */
	vport->fc_rscn_flush = 1;
	spin_unlock_irq(shost->host_lock);
	/* Get the array count after successfully have the token */
	rscn_cnt = vport->fc_rscn_id_cnt;
	/* If we are already processing an RSCN, save the received
	 * RSCN payload buffer, cmdiocb->context2 to process later.
	 */
	if (vport->fc_flag & (FC_RSCN_MODE | FC_NDISC_ACTIVE)) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RSCN defer:  did:x%x/ste:x%x flg:x%x",
			ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_RSCN_DEFERRED;
		if ((rscn_cnt < FC_MAX_HOLD_RSCN) &&
		    !(vport->fc_flag & FC_RSCN_DISCOVERY)) {
			vport->fc_flag |= FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
			if (rscn_cnt) {
				cmd = vport->fc_rscn_id_list[rscn_cnt-1]->virt;
				length = be32_to_cpu(*cmd & ~ELS_CMD_MASK);
			}
			if ((rscn_cnt) &&
			    (payload_len + length <= LPFC_BPL_SIZE)) {
				*cmd &= ELS_CMD_MASK;
				*cmd |= cpu_to_be32(payload_len + length);
				memcpy(((uint8_t *)cmd) + length, lp,
				       payload_len);
			} else {
				vport->fc_rscn_id_list[rscn_cnt] = pcmd;
				vport->fc_rscn_id_cnt++;
				/* If we zero, cmdiocb->context2, the calling
				 * routine will not try to free it.
				 */
				cmdiocb->context2 = NULL;
			}
			/* Deferred RSCN */
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0235 Deferred RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		} else {
			vport->fc_flag |= FC_RSCN_DISCOVERY;
			spin_unlock_irq(shost->host_lock);
			/* ReDiscovery RSCN */
			lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
					 "0234 ReDiscovery RSCN "
					 "Data: x%x x%x x%x\n",
					 vport->fc_rscn_id_cnt, vport->fc_flag,
					 vport->port_state);
		}
		/* Indicate we are done walking fc_rscn_id_list on this vport */
		vport->fc_rscn_flush = 0;
		/* Send back ACC */
		lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
		/* send RECOVERY event for ALL nodes that match RSCN payload */
		lpfc_rscn_recovery_check(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_RSCN_DEFERRED;
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
		"RCV RSCN:        did:x%x/ste:x%x flg:x%x",
		ndlp->nlp_DID, vport->port_state, ndlp->nlp_flag);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_RSCN_MODE;
	spin_unlock_irq(shost->host_lock);
	vport->fc_rscn_id_list[vport->fc_rscn_id_cnt++] = pcmd;
	/* Indicate we are done walking fc_rscn_id_list on this vport */
	vport->fc_rscn_flush = 0;
	/*
	 * If we zero, cmdiocb->context2, the calling routine will
	 * not try to free it.
	 */
	cmdiocb->context2 = NULL;
	lpfc_set_disctmo(vport);
	/* Send back ACC */
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
	/* send RECOVERY event for ALL nodes that match RSCN payload */
	lpfc_rscn_recovery_check(vport);
	return lpfc_els_handle_rscn(vport);
}

/**
 * lpfc_els_handle_rscn - Handle rscn for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine handles the Registration State Configuration Notification
 * (RSCN) for a @vport. If login to NameServer does not exist, a new ndlp shall
 * be created and a Port Login (PLOGI) to the NameServer is issued. Otherwise,
 * if the ndlp to NameServer exists, a Common Transport (CT) command to the
 * NameServer shall be issued. If CT command to the NameServer fails to be
 * issued, the lpfc_els_flush_rscn() routine shall be invoked to clean up any
 * RSCN activities with the @vport.
 *
 * Return code
 *   0 - Cleaned up rscn on the @vport
 *   1 - Wait for plogi to name server before proceed
 **/
int
lpfc_els_handle_rscn(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_hba *phba = vport->phba;

	/* Ignore RSCN if the port is being torn down. */
	if (vport->load_flag & FC_UNLOADING) {
		lpfc_els_flush_rscn(vport);
		return 0;
	}

	/* Start timer for RSCN processing */
	lpfc_set_disctmo(vport);

	/* RSCN processed */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0215 RSCN processed Data: x%x x%x x%x x%x\n",
			 vport->fc_flag, 0, vport->fc_rscn_id_cnt,
			 vport->port_state);

	/* To process RSCN, first compare RSCN data with NameServer */
	vport->fc_ns_retry = 0;
	vport->num_disc_nodes = 0;

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		/* Good ndlp, issue CT Request to NameServer */
		if (lpfc_ns_cmd(vport, SLI_CTNS_GID_FT, 0, 0) == 0)
			/* Wait for NameServer query cmpl before we can
			   continue */
			return 1;
	} else {
		/* If login to NameServer does not exist, issue one */
		/* Good status, issue PLOGI to NameServer */
		ndlp = lpfc_findnode_did(vport, NameServer_DID);
		if (ndlp && NLP_CHK_NODE_ACT(ndlp))
			/* Wait for NameServer login cmpl before we can
			   continue */
			return 1;

		if (ndlp) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_PLOGI_ISSUE);
			if (!ndlp) {
				lpfc_els_flush_rscn(vport);
				return 0;
			}
			ndlp->nlp_prev_state = NLP_STE_UNUSED_NODE;
		} else {
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp) {
				lpfc_els_flush_rscn(vport);
				return 0;
			}
			lpfc_nlp_init(vport, ndlp, NameServer_DID);
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_issue_els_plogi(vport, NameServer_DID, 0);
		/* Wait for NameServer login cmpl before we can
		 * continue
		 */
		return 1;
	}

	lpfc_els_flush_rscn(vport);
	return 0;
}

/**
 * lpfc_els_rcv_flogi - Process an unsolicited flogi iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Fabric Login (FLOGI) IOCB received as an ELS
 * unsolicited event. An unsolicited FLOGI can be received in a point-to-
 * point topology. As an unsolicited FLOGI should not be received in a loop
 * mode, any unsolicited FLOGI received in loop mode shall be ignored. The
 * lpfc_check_sparm() routine is invoked to check the parameters in the
 * unsolicited FLOGI. If parameters validation failed, the routine
 * lpfc_els_rsp_reject() shall be called with reject reason code set to
 * LSEXP_SPARM_OPTIONS to reject the FLOGI. Otherwise, the Port WWN in the
 * FLOGI shall be compared with the Port WWN of the @vport to determine who
 * will initiate PLOGI. The higher lexicographical value party shall has
 * higher priority (as the winning port) and will initiate PLOGI and
 * communicate Port_IDs (Addresses) for both nodes in PLOGI. The result
 * of this will be marked in the @vport fc_flag field with FC_PT2PT_PLOGI
 * and then the lpfc_els_rsp_acc() routine is invoked to accept the FLOGI.
 *
 * Return code
 *   0 - Successfully processed the unsolicited flogi
 *   1 - Failed to process the unsolicited flogi
 **/
static int
lpfc_els_rcv_flogi(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_dmabuf *pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	uint32_t *lp = (uint32_t *) pcmd->virt;
	IOCB_t *icmd = &cmdiocb->iocb;
	struct serv_parm *sp;
	LPFC_MBOXQ_t *mbox;
	struct ls_rjt stat;
	uint32_t cmd, did;
	int rc;

	cmd = *lp++;
	sp = (struct serv_parm *) lp;

	/* FLOGI received */

	lpfc_set_disctmo(vport);

	if (phba->fc_topology == TOPOLOGY_LOOP) {
		/* We should never receive a FLOGI in loop mode, ignore it */
		did = icmd->un.elsreq64.remoteID;

		/* An FLOGI ELS command <elsCmd> was received from DID <did> in
		   Loop Mode */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0113 An FLOGI ELS command x%x was "
				 "received from DID x%x in Loop Mode\n",
				 cmd, did);
		return 1;
	}

	did = Fabric_DID;

	if ((lpfc_check_sparm(vport, ndlp, sp, CLASS3, 1))) {
		/* For a FLOGI we accept, then if our portname is greater
		 * then the remote portname we initiate Nport login.
		 */

		rc = memcmp(&vport->fc_portname, &sp->portName,
			    sizeof(struct lpfc_name));

		if (!rc) {
			mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
			if (!mbox)
				return 1;

			lpfc_linkdown(phba);
			lpfc_init_link(phba, mbox,
				       phba->cfg_topology,
				       phba->cfg_link_speed);
			mbox->u.mb.un.varInitLnk.lipsr_AL_PA = 0;
			mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			mbox->vport = vport;
			rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
			lpfc_set_loopback_flag(phba);
			if (rc == MBX_NOT_FINISHED) {
				mempool_free(mbox, phba->mbox_mem_pool);
			}
			return 1;
		} else if (rc > 0) {	/* greater than */
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_PT2PT_PLOGI;
			spin_unlock_irq(shost->host_lock);
		}
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT;
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);
	} else {
		/* Reject this request because invalid parameters */
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_SPARM_OPTIONS;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
		return 1;
	}

	/* Send back ACC */
	lpfc_els_rsp_acc(vport, ELS_CMD_PLOGI, cmdiocb, ndlp, NULL);

	return 0;
}

/**
 * lpfc_els_rcv_rnid - Process an unsolicited rnid iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Request Node Identification Data (RNID) IOCB
 * received as an ELS unsolicited event. Only when the RNID specified format
 * 0x0 or 0xDF (Topology Discovery Specific Node Identification Data)
 * present, this routine will invoke the lpfc_els_rsp_rnid_acc() routine to
 * Accept (ACC) the RNID ELS command. All the other RNID formats are
 * rejected by invoking the lpfc_els_rsp_reject() routine.
 *
 * Return code
 *   0 - Successfully processed rnid iocb (currently always return 0)
 **/
static int
lpfc_els_rcv_rnid(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	RNID *rn;
	struct ls_rjt stat;
	uint32_t cmd, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	rn = (RNID *) lp;

	/* RNID received */

	switch (rn->Format) {
	case 0:
	case RNID_TOPOLOGY_DISC:
		/* Send back ACC */
		lpfc_els_rsp_rnid_acc(vport, rn->Format, cmdiocb, ndlp);
		break;
	default:
		/* Reject this request because format not supported */
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
	}
	return 0;
}

/**
 * lpfc_els_rcv_lirr - Process an unsolicited lirr iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes a Link Incident Report Registration(LIRR) IOCB
 * received as an ELS unsolicited event. Currently, this function just invokes
 * the lpfc_els_rsp_reject() routine to reject the LIRR IOCB unconditionally.
 *
 * Return code
 *   0 - Successfully processed lirr iocb (currently always return 0)
 **/
static int
lpfc_els_rcv_lirr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct ls_rjt stat;

	/* For now, unconditionally reject this command */
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

/**
 * lpfc_els_rcv_rrq - Process an unsolicited rrq iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes a Reinstate Recovery Qualifier (RRQ) IOCB
 * received as an ELS unsolicited event. A request to RRQ shall only
 * be accepted if the Originator Nx_Port N_Port_ID or the Responder
 * Nx_Port N_Port_ID of the target Exchange is the same as the
 * N_Port_ID of the Nx_Port that makes the request. If the RRQ is
 * not accepted, an LS_RJT with reason code "Unable to perform
 * command request" and reason code explanation "Invalid Originator
 * S_ID" shall be returned. For now, we just unconditionally accept
 * RRQ from the target.
 **/
static void
lpfc_els_rcv_rrq(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);
}

/**
 * lpfc_els_rsp_rps_acc - Completion callbk func for MBX_READ_LNK_STAT mbox cmd
 * @phba: pointer to lpfc hba data structure.
 * @pmb: pointer to the driver internal queue element for mailbox command.
 *
 * This routine is the completion callback function for the MBX_READ_LNK_STAT
 * mailbox command. This callback function is to actually send the Accept
 * (ACC) response to a Read Port Status (RPS) unsolicited IOCB event. It
 * collects the link statistics from the completion of the MBX_READ_LNK_STAT
 * mailbox command, constructs the RPS response with the link statistics
 * collected, and then invokes the lpfc_sli_issue_iocb() routine to send ACC
 * response to the RPS.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the RPS Accept Response ELS IOCB command.
 *
 **/
static void
lpfc_els_rsp_rps_acc(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb;
	IOCB_t *icmd;
	RPS_RSP *rps_rsp;
	uint8_t *pcmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_nodelist *ndlp;
	uint16_t xri, status;
	uint32_t cmdsize;

	mb = &pmb->u.mb;

	ndlp = (struct lpfc_nodelist *) pmb->context2;
	xri = (uint16_t) ((unsigned long)(pmb->context1));
	pmb->context1 = NULL;
	pmb->context2 = NULL;

	if (mb->mbxStatus) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	cmdsize = sizeof(RPS_RSP) + sizeof(uint32_t);
	mempool_free(pmb, phba->mbox_mem_pool);
	elsiocb = lpfc_prep_els_iocb(phba->pport, 0, cmdsize,
				     lpfc_max_els_tries, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	/* Decrement the ndlp reference count from previous mbox command */
	lpfc_nlp_put(ndlp);

	if (!elsiocb)
		return;

	icmd = &elsiocb->iocb;
	icmd->ulpContext = xri;

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint32_t); /* Skip past command */
	rps_rsp = (RPS_RSP *)pcmd;

	if (phba->fc_topology != TOPOLOGY_LOOP)
		status = 0x10;
	else
		status = 0x8;
	if (phba->pport->fc_flag & FC_FABRIC)
		status |= 0x4;

	rps_rsp->rsvd1 = 0;
	rps_rsp->portStatus = cpu_to_be16(status);
	rps_rsp->linkFailureCnt = cpu_to_be32(mb->un.varRdLnk.linkFailureCnt);
	rps_rsp->lossSyncCnt = cpu_to_be32(mb->un.varRdLnk.lossSyncCnt);
	rps_rsp->lossSignalCnt = cpu_to_be32(mb->un.varRdLnk.lossSignalCnt);
	rps_rsp->primSeqErrCnt = cpu_to_be32(mb->un.varRdLnk.primSeqErrCnt);
	rps_rsp->invalidXmitWord = cpu_to_be32(mb->un.varRdLnk.invalidXmitWord);
	rps_rsp->crcCnt = cpu_to_be32(mb->un.varRdLnk.crcCnt);
	/* Xmit ELS RPS ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(ndlp->vport, KERN_INFO, LOG_ELS,
			 "0118 Xmit ELS RPS ACC response tag x%x xri x%x, "
			 "did x%x, nlp_flag x%x, nlp_state x%x, rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) == IOCB_ERROR)
		lpfc_els_free_iocb(phba, elsiocb);
	return;
}

/**
 * lpfc_els_rcv_rps - Process an unsolicited rps iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Read Port Status (RPS) IOCB received as an
 * ELS unsolicited event. It first checks the remote port state. If the
 * remote port is not in NLP_STE_UNMAPPED_NODE state or NLP_STE_MAPPED_NODE
 * state, it invokes the lpfc_els_rsp_reject() routine to send the reject
 * response. Otherwise, it issue the MBX_READ_LNK_STAT mailbox command
 * for reading the HBA link statistics. It is for the callback function,
 * lpfc_els_rsp_rps_acc(), set to the MBX_READ_LNK_STAT mailbox command
 * to actually sending out RPS Accept (ACC) response.
 *
 * Return codes
 *   0 - Successfully processed rps iocb (currently always return 0)
 **/
static int
lpfc_els_rcv_rps(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t *lp;
	uint8_t flag;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *pcmd;
	RPS *rps;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE))
		/* reject the unsolicited RPS request and done with it */
		goto reject_out;

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;
	flag = (be32_to_cpu(*lp++) & 0xf);
	rps = (RPS *) lp;

	if ((flag == 0) ||
	    ((flag == 1) && (be32_to_cpu(rps->un.portNum) == 0)) ||
	    ((flag == 2) && (memcmp(&rps->un.portName, &vport->fc_portname,
				    sizeof(struct lpfc_name)) == 0))) {

		printk("Fix me....\n");
		dump_stack();
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_ATOMIC);
		if (mbox) {
			lpfc_read_lnk_stat(phba, mbox);
			mbox->context1 =
			    (void *)((unsigned long) cmdiocb->iocb.ulpContext);
			mbox->context2 = lpfc_nlp_get(ndlp);
			mbox->vport = vport;
			mbox->mbox_cmpl = lpfc_els_rsp_rps_acc;
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
				!= MBX_NOT_FINISHED)
				/* Mbox completion will send ELS Response */
				return 0;
			/* Decrement reference count used for the failed mbox
			 * command.
			 */
			lpfc_nlp_put(ndlp);
			mempool_free(mbox, phba->mbox_mem_pool);
		}
	}

reject_out:
	/* issue rejection response */
	stat.un.b.lsRjtRsvd0 = 0;
	stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
	stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
	stat.un.b.vendorUnique = 0;
	lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp, NULL);
	return 0;
}

/**
 * lpfc_els_rsp_rpl_acc - Issue an accept rpl els command
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdsize: size of the ELS command.
 * @oldiocb: pointer to the original lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine issuees an Accept (ACC) Read Port List (RPL) ELS command.
 * It is to be called by the lpfc_els_rcv_rpl() routine to accept the RPL.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the RPL Accept Response ELS command.
 *
 * Return code
 *   0 - Successfully issued ACC RPL ELS command
 *   1 - Failed to issue ACC RPL ELS command
 **/
static int
lpfc_els_rsp_rpl_acc(struct lpfc_vport *vport, uint16_t cmdsize,
		     struct lpfc_iocbq *oldiocb, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd, *oldcmd;
	RPL_RSP rpl_rsp;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;

	elsiocb = lpfc_prep_els_iocb(vport, 0, cmdsize, oldiocb->retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ACC);

	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	oldcmd = &oldiocb->iocb;
	icmd->ulpContext = oldcmd->ulpContext;	/* Xri */

	pcmd = (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_ACC;
	pcmd += sizeof(uint16_t);
	*((uint16_t *)(pcmd)) = be16_to_cpu(cmdsize);
	pcmd += sizeof(uint16_t);

	/* Setup the RPL ACC payload */
	rpl_rsp.listLen = be32_to_cpu(1);
	rpl_rsp.index = 0;
	rpl_rsp.port_num_blk.portNum = 0;
	rpl_rsp.port_num_blk.portID = be32_to_cpu(vport->fc_myDID);
	memcpy(&rpl_rsp.port_num_blk.portName, &vport->fc_portname,
	    sizeof(struct lpfc_name));
	memcpy(pcmd, &rpl_rsp, cmdsize - sizeof(uint32_t));
	/* Xmit ELS RPL ACC response tag <ulpIoTag> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0120 Xmit ELS RPL ACC response tag x%x "
			 "xri x%x, did x%x, nlp_flag x%x, nlp_state x%x, "
			 "rpi x%x\n",
			 elsiocb->iotag, elsiocb->iocb.ulpContext,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	elsiocb->iocb_cmpl = lpfc_cmpl_els_rsp;
	phba->fc_stat.elsXmitACC++;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_rcv_rpl - Process an unsolicited rpl iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Read Port List (RPL) IOCB received as an ELS
 * unsolicited event. It first checks the remote port state. If the remote
 * port is not in NLP_STE_UNMAPPED_NODE and NLP_STE_MAPPED_NODE states, it
 * invokes the lpfc_els_rsp_reject() routine to send reject response.
 * Otherwise, this routine then invokes the lpfc_els_rsp_rpl_acc() routine
 * to accept the RPL.
 *
 * Return code
 *   0 - Successfully processed rpl iocb (currently always return 0)
 **/
static int
lpfc_els_rcv_rpl(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	uint32_t maxsize;
	uint16_t cmdsize;
	RPL *rpl;
	struct ls_rjt stat;

	if ((ndlp->nlp_state != NLP_STE_UNMAPPED_NODE) &&
	    (ndlp->nlp_state != NLP_STE_MAPPED_NODE)) {
		/* issue rejection response */
		stat.un.b.lsRjtRsvd0 = 0;
		stat.un.b.lsRjtRsnCode = LSRJT_UNABLE_TPC;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_CANT_GIVE_DATA;
		stat.un.b.vendorUnique = 0;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, cmdiocb, ndlp,
			NULL);
		/* rejected the unsolicited RPL request and done with it */
		return 0;
	}

	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;
	rpl = (RPL *) (lp + 1);

	maxsize = be32_to_cpu(rpl->maxsize);

	/* We support only one port */
	if ((rpl->index == 0) &&
	    ((maxsize == 0) ||
	     ((maxsize * sizeof(uint32_t)) >= sizeof(RPL_RSP)))) {
		cmdsize = sizeof(uint32_t) + sizeof(RPL_RSP);
	} else {
		cmdsize = sizeof(uint32_t) + maxsize * sizeof(uint32_t);
	}
	lpfc_els_rsp_rpl_acc(vport, cmdsize, cmdiocb, ndlp);

	return 0;
}

/**
 * lpfc_els_rcv_farp - Process an unsolicited farp request els command
 * @vport: pointer to a virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Fibre Channel Address Resolution Protocol
 * (FARP) Request IOCB received as an ELS unsolicited event. Currently,
 * the lpfc driver only supports matching on WWPN or WWNN for FARP. As such,
 * FARP_MATCH_PORT flag and FARP_MATCH_NODE flag are checked against the
 * Match Flag in the FARP request IOCB: if FARP_MATCH_PORT flag is set, the
 * remote PortName is compared against the FC PortName stored in the @vport
 * data structure; if FARP_MATCH_NODE flag is set, the remote NodeName is
 * compared against the FC NodeName stored in the @vport data structure.
 * If any of these matches and the FARP_REQUEST_FARPR flag is set in the
 * FARP request IOCB Response Flag, the lpfc_issue_els_farpr() routine is
 * invoked to send out FARP Response to the remote node. Before sending the
 * FARP Response, however, the FARP_REQUEST_PLOGI flag is check in the FARP
 * request IOCB Response Flag and, if it is set, the lpfc_issue_els_plogi()
 * routine is invoked to log into the remote port first.
 *
 * Return code
 *   0 - Either the FARP Match Mode not supported or successfully processed
 **/
static int
lpfc_els_rcv_farp(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_nodelist *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	FARP *fp;
	uint32_t cmd, cnt, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	fp = (FARP *) lp;
	/* FARP-REQ received from DID <did> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0601 FARP-REQ received from DID x%x\n", did);
	/* We will only support match on WWPN or WWNN */
	if (fp->Mflags & ~(FARP_MATCH_NODE | FARP_MATCH_PORT)) {
		return 0;
	}

	cnt = 0;
	/* If this FARP command is searching for my portname */
	if (fp->Mflags & FARP_MATCH_PORT) {
		if (memcmp(&fp->RportName, &vport->fc_portname,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	/* If this FARP command is searching for my nodename */
	if (fp->Mflags & FARP_MATCH_NODE) {
		if (memcmp(&fp->RnodeName, &vport->fc_nodename,
			   sizeof(struct lpfc_name)) == 0)
			cnt = 1;
	}

	if (cnt) {
		if ((ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) ||
		   (ndlp->nlp_state == NLP_STE_MAPPED_NODE)) {
			/* Log back into the node before sending the FARP. */
			if (fp->Rflags & FARP_REQUEST_PLOGI) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
				lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0);
			}

			/* Send a FARP response to that node */
			if (fp->Rflags & FARP_REQUEST_FARPR)
				lpfc_issue_els_farpr(vport, did, 0);
		}
	}
	return 0;
}

/**
 * lpfc_els_rcv_farpr - Process an unsolicited farp response iocb
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine processes Fibre Channel Address Resolution Protocol
 * Response (FARPR) IOCB received as an ELS unsolicited event. It simply
 * invokes the lpfc_els_rsp_acc() routine to the remote node to accept
 * the FARP response request.
 *
 * Return code
 *   0 - Successfully processed FARPR IOCB (currently always return 0)
 **/
static int
lpfc_els_rcv_farpr(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_nodelist  *ndlp)
{
	struct lpfc_dmabuf *pcmd;
	uint32_t *lp;
	IOCB_t *icmd;
	uint32_t cmd, did;

	icmd = &cmdiocb->iocb;
	did = icmd->un.elsreq64.remoteID;
	pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	lp = (uint32_t *) pcmd->virt;

	cmd = *lp++;
	/* FARP-RSP received from DID <did> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0600 FARP-RSP received from DID x%x\n", did);
	/* ACCEPT the Farp resp request */
	lpfc_els_rsp_acc(vport, ELS_CMD_ACC, cmdiocb, ndlp, NULL);

	return 0;
}

/**
 * lpfc_els_rcv_fan - Process an unsolicited fan iocb command
 * @vport: pointer to a host virtual N_Port data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @fan_ndlp: pointer to a node-list data structure.
 *
 * This routine processes a Fabric Address Notification (FAN) IOCB
 * command received as an ELS unsolicited event. The FAN ELS command will
 * only be processed on a physical port (i.e., the @vport represents the
 * physical port). The fabric NodeName and PortName from the FAN IOCB are
 * compared against those in the phba data structure. If any of those is
 * different, the lpfc_initial_flogi() routine is invoked to initialize
 * Fabric Login (FLOGI) to the fabric to start the discover over. Otherwise,
 * if both of those are identical, the lpfc_issue_fabric_reglogin() routine
 * is invoked to register login to the fabric.
 *
 * Return code
 *   0 - Successfully processed fan iocb (currently always return 0).
 **/
static int
lpfc_els_rcv_fan(struct lpfc_vport *vport, struct lpfc_iocbq *cmdiocb,
		 struct lpfc_nodelist *fan_ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t *lp;
	FAN *fp;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS, "0265 FAN received\n");
	lp = (uint32_t *)((struct lpfc_dmabuf *)cmdiocb->context2)->virt;
	fp = (FAN *) ++lp;
	/* FAN received; Fan does not have a reply sequence */
	if ((vport == phba->pport) &&
	    (vport->port_state == LPFC_LOCAL_CFG_LINK)) {
		if ((memcmp(&phba->fc_fabparam.nodeName, &fp->FnodeName,
			    sizeof(struct lpfc_name))) ||
		    (memcmp(&phba->fc_fabparam.portName, &fp->FportName,
			    sizeof(struct lpfc_name)))) {
			/* This port has switched fabrics. FLOGI is required */
			lpfc_initial_flogi(vport);
		} else {
			/* FAN verified - skip FLOGI */
			vport->fc_myDID = vport->fc_prevDID;
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else
				lpfc_issue_reg_vfi(vport);
		}
	}
	return 0;
}

/**
 * lpfc_els_timeout - Handler funciton to the els timer
 * @ptr: holder for the timer function associated data.
 *
 * This routine is invoked by the ELS timer after timeout. It posts the ELS
 * timer timeout event by setting the WORKER_ELS_TMO bit to the work port
 * event bitmap and then invokes the lpfc_worker_wake_up() routine to wake
 * up the worker thread. It is for the worker thread to invoke the routine
 * lpfc_els_timeout_handler() to work on the posted event WORKER_ELS_TMO.
 **/
void
lpfc_els_timeout(unsigned long ptr)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) ptr;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	spin_lock_irqsave(&vport->work_port_lock, iflag);
	tmo_posted = vport->work_port_events & WORKER_ELS_TMO;
	if (!tmo_posted)
		vport->work_port_events |= WORKER_ELS_TMO;
	spin_unlock_irqrestore(&vport->work_port_lock, iflag);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}


/**
 * lpfc_els_timeout_handler - Process an els timeout event
 * @vport: pointer to a virtual N_Port data structure.
 *
 * This routine is the actual handler function that processes an ELS timeout
 * event. It walks the ELS ring to get and abort all the IOCBs (except the
 * ABORT/CLOSE/FARP/FARPR/FDISC), which are associated with the @vport by
 * invoking the lpfc_sli_issue_abort_iotag() routine.
 **/
void
lpfc_els_timeout_handler(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;
	struct lpfc_dmabuf *pcmd;
	uint32_t els_command = 0;
	uint32_t timeout;
	uint32_t remote_ID = 0xffffffff;
	LIST_HEAD(txcmplq_completions);
	LIST_HEAD(abort_list);


	timeout = (uint32_t)(phba->fc_ratov << 1);

	pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&pring->txcmplq, &txcmplq_completions);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(piocb, tmp_iocb, &txcmplq_completions, list) {
		cmd = &piocb->iocb;

		if ((piocb->iocb_flag & LPFC_IO_LIBDFC) != 0 ||
		    piocb->iocb.ulpCommand == CMD_ABORT_XRI_CN ||
		    piocb->iocb.ulpCommand == CMD_CLOSE_XRI_CN)
			continue;

		if (piocb->vport != vport)
			continue;

		pcmd = (struct lpfc_dmabuf *) piocb->context2;
		if (pcmd)
			els_command = *(uint32_t *) (pcmd->virt);

		if (els_command == ELS_CMD_FARP ||
		    els_command == ELS_CMD_FARPR ||
		    els_command == ELS_CMD_FDISC)
			continue;

		if (piocb->drvrTimeout > 0) {
			if (piocb->drvrTimeout >= timeout)
				piocb->drvrTimeout -= timeout;
			else
				piocb->drvrTimeout = 0;
			continue;
		}

		remote_ID = 0xffffffff;
		if (cmd->ulpCommand != CMD_GEN_REQUEST64_CR)
			remote_ID = cmd->un.elsreq64.remoteID;
		else {
			struct lpfc_nodelist *ndlp;
			ndlp = __lpfc_findnode_rpi(vport, cmd->ulpContext);
			if (ndlp && NLP_CHK_NODE_ACT(ndlp))
				remote_ID = ndlp->nlp_DID;
		}
		list_add_tail(&piocb->dlist, &abort_list);
	}
	spin_lock_irq(&phba->hbalock);
	list_splice(&txcmplq_completions, &pring->txcmplq);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(piocb, tmp_iocb, &abort_list, dlist) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "0127 ELS timeout Data: x%x x%x x%x "
			 "x%x\n", els_command,
			 remote_ID, cmd->ulpCommand, cmd->ulpIoTag);
		spin_lock_irq(&phba->hbalock);
		list_del_init(&piocb->dlist);
		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
		spin_unlock_irq(&phba->hbalock);
	}

	if (phba->sli.ring[LPFC_ELS_RING].txcmplq_cnt)
		mod_timer(&vport->els_tmofunc, jiffies + HZ * timeout);
}

/**
 * lpfc_els_flush_cmd - Clean up the outstanding els commands to a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine is used to clean up all the outstanding ELS commands on a
 * @vport. It first aborts the @vport by invoking lpfc_fabric_abort_vport()
 * routine. After that, it walks the ELS transmit queue to remove all the
 * IOCBs with the @vport other than the QUE_RING and ABORT/CLOSE IOCBs. For
 * the IOCBs with a non-NULL completion callback function, the callback
 * function will be invoked with the status set to IOSTAT_LOCAL_REJECT and
 * un.ulpWord[4] set to IOERR_SLI_ABORTED. For IOCBs with a NULL completion
 * callback function, the IOCB will simply be released. Finally, it walks
 * the ELS transmit completion queue to issue an abort IOCB to any transmit
 * completion queue IOCB that is associated with the @vport and is not
 * an IOCB from libdfc (i.e., the management plane IOCBs that are not
 * part of the discovery state machine) out to HBA by invoking the
 * lpfc_sli_issue_abort_iotag() routine. Note that this function issues the
 * abort IOCB to any transmit completion queueed IOCB, it does not guarantee
 * the IOCBs are aborted when this function returns.
 **/
void
lpfc_els_flush_cmd(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;

	lpfc_fabric_abort_vport(vport);

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txq, list) {
		cmd = &piocb->iocb;

		if (piocb->iocb_flag & LPFC_IO_LIBDFC) {
			continue;
		}

		/* Do not flush out the QUE_RING and ABORT/CLOSE iocbs */
		if (cmd->ulpCommand == CMD_QUE_RING_BUF_CN ||
		    cmd->ulpCommand == CMD_QUE_RING_BUF64_CN ||
		    cmd->ulpCommand == CMD_CLOSE_XRI_CN ||
		    cmd->ulpCommand == CMD_ABORT_XRI_CN)
			continue;

		if (piocb->vport != vport)
			continue;

		list_move_tail(&piocb->list, &completions);
		pring->txq_cnt--;
	}

	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		if (piocb->iocb_flag & LPFC_IO_LIBDFC) {
			continue;
		}

		if (piocb->vport != vport)
			continue;

		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
	}
	spin_unlock_irq(&phba->hbalock);

	/* Cancell all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return;
}

/**
 * lpfc_els_flush_all_cmd - Clean up all the outstanding els commands to a HBA
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is used to clean up all the outstanding ELS commands on a
 * @phba. It first aborts the @phba by invoking the lpfc_fabric_abort_hba()
 * routine. After that, it walks the ELS transmit queue to remove all the
 * IOCBs to the @phba other than the QUE_RING and ABORT/CLOSE IOCBs. For
 * the IOCBs with the completion callback function associated, the callback
 * function will be invoked with the status set to IOSTAT_LOCAL_REJECT and
 * un.ulpWord[4] set to IOERR_SLI_ABORTED. For IOCBs without the completion
 * callback function associated, the IOCB will simply be released. Finally,
 * it walks the ELS transmit completion queue to issue an abort IOCB to any
 * transmit completion queue IOCB that is not an IOCB from libdfc (i.e., the
 * management plane IOCBs that are not part of the discovery state machine)
 * out to HBA by invoking the lpfc_sli_issue_abort_iotag() routine.
 **/
void
lpfc_els_flush_all_cmd(struct lpfc_hba  *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *tmp_iocb, *piocb;
	IOCB_t *cmd = NULL;

	lpfc_fabric_abort_hba(phba);
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txq, list) {
		cmd = &piocb->iocb;
		if (piocb->iocb_flag & LPFC_IO_LIBDFC)
			continue;
		/* Do not flush out the QUE_RING and ABORT/CLOSE iocbs */
		if (cmd->ulpCommand == CMD_QUE_RING_BUF_CN ||
		    cmd->ulpCommand == CMD_QUE_RING_BUF64_CN ||
		    cmd->ulpCommand == CMD_CLOSE_XRI_CN ||
		    cmd->ulpCommand == CMD_ABORT_XRI_CN)
			continue;
		list_move_tail(&piocb->list, &completions);
		pring->txq_cnt--;
	}
	list_for_each_entry_safe(piocb, tmp_iocb, &pring->txcmplq, list) {
		if (piocb->iocb_flag & LPFC_IO_LIBDFC)
			continue;
		lpfc_sli_issue_abort_iotag(phba, pring, piocb);
	}
	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return;
}

/**
 * lpfc_send_els_failure_event - Posts an ELS command failure event
 * @phba: Pointer to hba context object.
 * @cmdiocbp: Pointer to command iocb which reported error.
 * @rspiocbp: Pointer to response iocb which reported error.
 *
 * This function sends an event when there is an ELS command
 * failure.
 **/
void
lpfc_send_els_failure_event(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbp,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_vport *vport = cmdiocbp->vport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_lsrjt_event lsrjt_event;
	struct lpfc_fabric_event_header fabric_event;
	struct ls_rjt stat;
	struct lpfc_nodelist *ndlp;
	uint32_t *pcmd;

	ndlp = cmdiocbp->context1;
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return;

	if (rspiocbp->iocb.ulpStatus == IOSTAT_LS_RJT) {
		lsrjt_event.header.event_type = FC_REG_ELS_EVENT;
		lsrjt_event.header.subcategory = LPFC_EVENT_LSRJT_RCV;
		memcpy(lsrjt_event.header.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(lsrjt_event.header.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
			cmdiocbp->context2)->virt);
		lsrjt_event.command = (pcmd != NULL) ? *pcmd : 0;
		stat.un.lsRjtError = be32_to_cpu(rspiocbp->iocb.un.ulpWord[4]);
		lsrjt_event.reason_code = stat.un.b.lsRjtRsnCode;
		lsrjt_event.explanation = stat.un.b.lsRjtRsnCodeExp;
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(lsrjt_event),
			(char *)&lsrjt_event,
			LPFC_NL_VENDOR_ID);
		return;
	}
	if ((rspiocbp->iocb.ulpStatus == IOSTAT_NPORT_BSY) ||
		(rspiocbp->iocb.ulpStatus == IOSTAT_FABRIC_BSY)) {
		fabric_event.event_type = FC_REG_FABRIC_EVENT;
		if (rspiocbp->iocb.ulpStatus == IOSTAT_NPORT_BSY)
			fabric_event.subcategory = LPFC_EVENT_PORT_BUSY;
		else
			fabric_event.subcategory = LPFC_EVENT_FABRIC_BUSY;
		memcpy(fabric_event.wwpn, &ndlp->nlp_portname,
			sizeof(struct lpfc_name));
		memcpy(fabric_event.wwnn, &ndlp->nlp_nodename,
			sizeof(struct lpfc_name));
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(fabric_event),
			(char *)&fabric_event,
			LPFC_NL_VENDOR_ID);
		return;
	}

}

/**
 * lpfc_send_els_event - Posts unsolicited els event
 * @vport: Pointer to vport object.
 * @ndlp: Pointer FC node object.
 * @cmd: ELS command code.
 *
 * This function posts an event when there is an incoming
 * unsolicited ELS command.
 **/
static void
lpfc_send_els_event(struct lpfc_vport *vport,
		    struct lpfc_nodelist *ndlp,
		    uint32_t *payload)
{
	struct lpfc_els_event_header *els_data = NULL;
	struct lpfc_logo_event *logo_data = NULL;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (*payload == ELS_CMD_LOGO) {
		logo_data = kmalloc(sizeof(struct lpfc_logo_event), GFP_KERNEL);
		if (!logo_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0148 Failed to allocate memory "
				"for LOGO event\n");
			return;
		}
		els_data = &logo_data->header;
	} else {
		els_data = kmalloc(sizeof(struct lpfc_els_event_header),
			GFP_KERNEL);
		if (!els_data) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0149 Failed to allocate memory "
				"for ELS event\n");
			return;
		}
	}
	els_data->event_type = FC_REG_ELS_EVENT;
	switch (*payload) {
	case ELS_CMD_PLOGI:
		els_data->subcategory = LPFC_EVENT_PLOGI_RCV;
		break;
	case ELS_CMD_PRLO:
		els_data->subcategory = LPFC_EVENT_PRLO_RCV;
		break;
	case ELS_CMD_ADISC:
		els_data->subcategory = LPFC_EVENT_ADISC_RCV;
		break;
	case ELS_CMD_LOGO:
		els_data->subcategory = LPFC_EVENT_LOGO_RCV;
		/* Copy the WWPN in the LOGO payload */
		memcpy(logo_data->logo_wwpn, &payload[2],
			sizeof(struct lpfc_name));
		break;
	default:
		kfree(els_data);
		return;
	}
	memcpy(els_data->wwpn, &ndlp->nlp_portname, sizeof(struct lpfc_name));
	memcpy(els_data->wwnn, &ndlp->nlp_nodename, sizeof(struct lpfc_name));
	if (*payload == ELS_CMD_LOGO) {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_logo_event),
			(char *)logo_data,
			LPFC_NL_VENDOR_ID);
		kfree(logo_data);
	} else {
		fc_host_post_vendor_event(shost,
			fc_get_event_number(),
			sizeof(struct lpfc_els_event_header),
			(char *)els_data,
			LPFC_NL_VENDOR_ID);
		kfree(els_data);
	}

	return;
}


/**
 * lpfc_els_unsol_buffer - Process an unsolicited event data buffer
 * @phba: pointer to lpfc hba data structure.
 * @pring: pointer to a SLI ring.
 * @vport: pointer to a host virtual N_Port data structure.
 * @elsiocb: pointer to lpfc els command iocb data structure.
 *
 * This routine is used for processing the IOCB associated with a unsolicited
 * event. It first determines whether there is an existing ndlp that matches
 * the DID from the unsolicited IOCB. If not, it will create a new one with
 * the DID from the unsolicited IOCB. The ELS command from the unsolicited
 * IOCB is then used to invoke the proper routine and to set up proper state
 * of the discovery state machine.
 **/
static void
lpfc_els_unsol_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		      struct lpfc_vport *vport, struct lpfc_iocbq *elsiocb)
{
	struct Scsi_Host  *shost;
	struct lpfc_nodelist *ndlp;
	struct ls_rjt stat;
	uint32_t *payload;
	uint32_t cmd, did, newnode, rjt_err = 0;
	IOCB_t *icmd = &elsiocb->iocb;

	if (!vport || !(elsiocb->context2))
		goto dropit;

	newnode = 0;
	payload = ((struct lpfc_dmabuf *)elsiocb->context2)->virt;
	cmd = *payload;
	if ((phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) == 0)
		lpfc_post_buffer(phba, pring, 1);

	did = icmd->un.rcvels.remoteID;
	if (icmd->ulpStatus) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV Unsol ELS:  status:x%x/x%x did:x%x",
			icmd->ulpStatus, icmd->un.ulpWord[4], did);
		goto dropit;
	}

	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport))
		goto dropit;

	/* Ignore traffic received during vport shutdown. */
	if (vport->load_flag & FC_UNLOADING)
		goto dropit;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		/* Cannot find existing Fabric ndlp, so allocate a new one */
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			goto dropit;

		lpfc_nlp_init(vport, ndlp, did);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
		if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
			ndlp->nlp_type |= NLP_FABRIC;
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp,
					NLP_STE_UNUSED_NODE);
		if (!ndlp)
			goto dropit;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
		if ((did & Fabric_DID_MASK) == Fabric_DID_MASK)
			ndlp->nlp_type |= NLP_FABRIC;
	} else if (ndlp->nlp_state == NLP_STE_UNUSED_NODE) {
		/* This is similar to the new node path */
		ndlp = lpfc_nlp_get(ndlp);
		if (!ndlp)
			goto dropit;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		newnode = 1;
	}

	phba->fc_stat.elsRcvFrame++;

	elsiocb->context1 = lpfc_nlp_get(ndlp);
	elsiocb->vport = vport;

	if ((cmd & ELS_CMD_MASK) == ELS_CMD_RSCN) {
		cmd &= ELS_CMD_MASK;
	}
	/* ELS command <elsCmd> received from NPORT <did> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0112 ELS command x%x received from NPORT x%x "
			 "Data: x%x\n", cmd, did, vport->port_state);
	switch (cmd) {
	case ELS_CMD_PLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPLOGI++;
		ndlp = lpfc_plogi_confirm_nport(phba, payload, ndlp);

		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			if (!(phba->pport->fc_flag & FC_PT2PT) ||
				(phba->pport->fc_flag & FC_PT2PT_PLOGI)) {
				rjt_err = LSRJT_UNABLE_TPC;
				break;
			}
			/* We get here, and drop thru, if we are PT2PT with
			 * another NPort and the other side has initiated
			 * the PLOGI before responding to our FLOGI.
			 */
		}

		shost = lpfc_shost_from_vport(vport);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_TARGET_REMOVE;
		spin_unlock_irq(shost->host_lock);

		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PLOGI);

		break;
	case ELS_CMD_FLOGI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FLOGI:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFLOGI++;
		lpfc_els_rcv_flogi(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_LOGO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LOGO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLOGO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_LOGO);
		break;
	case ELS_CMD_PRLO:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLO:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLO++;
		lpfc_send_els_event(vport, ndlp, payload);
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLO);
		break;
	case ELS_CMD_RSCN:
		phba->fc_stat.elsRcvRSCN++;
		lpfc_els_rcv_rscn(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_ADISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ADISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		lpfc_send_els_event(vport, ndlp, payload);
		phba->fc_stat.elsRcvADISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_ADISC);
		break;
	case ELS_CMD_PDISC:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PDISC:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPDISC++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb,
					NLP_EVT_RCV_PDISC);
		break;
	case ELS_CMD_FARPR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARPR:       did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARPR++;
		lpfc_els_rcv_farpr(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FARP:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FARP:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFARP++;
		lpfc_els_rcv_farp(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_FAN:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV FAN:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvFAN++;
		lpfc_els_rcv_fan(vport, elsiocb, ndlp);
		break;
	case ELS_CMD_PRLI:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV PRLI:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvPRLI++;
		if (vport->port_state < LPFC_DISC_AUTH) {
			rjt_err = LSRJT_UNABLE_TPC;
			break;
		}
		lpfc_disc_state_machine(vport, ndlp, elsiocb, NLP_EVT_RCV_PRLI);
		break;
	case ELS_CMD_LIRR:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV LIRR:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvLIRR++;
		lpfc_els_rcv_lirr(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RPS:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RPS:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRPS++;
		lpfc_els_rcv_rps(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RPL:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RPL:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRPL++;
		lpfc_els_rcv_rpl(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RNID:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RNID:        did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRNID++;
		lpfc_els_rcv_rnid(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	case ELS_CMD_RRQ:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV RRQ:         did:x%x/ste:x%x flg:x%x",
			did, vport->port_state, ndlp->nlp_flag);

		phba->fc_stat.elsRcvRRQ++;
		lpfc_els_rcv_rrq(vport, elsiocb, ndlp);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	default:
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_UNSOL,
			"RCV ELS cmd:     cmd:x%x did:x%x/ste:x%x",
			cmd, did, vport->port_state);

		/* Unsupported ELS command, reject */
		rjt_err = LSRJT_INVALID_CMD;

		/* Unknown ELS command <elsCmd> received from NPORT <did> */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0115 Unknown ELS command x%x "
				 "received from NPORT x%x\n", cmd, did);
		if (newnode)
			lpfc_nlp_put(ndlp);
		break;
	}

	/* check if need to LS_RJT received ELS cmd */
	if (rjt_err) {
		memset(&stat, 0, sizeof(stat));
		stat.un.b.lsRjtRsnCode = rjt_err;
		stat.un.b.lsRjtRsnCodeExp = LSEXP_NOTHING_MORE;
		lpfc_els_rsp_reject(vport, stat.un.lsRjtError, elsiocb, ndlp,
			NULL);
	}

	lpfc_nlp_put(elsiocb->context1);
	elsiocb->context1 = NULL;
	return;

dropit:
	if (vport && !(vport->load_flag & FC_UNLOADING))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			"0111 Dropping received ELS cmd "
			"Data: x%x x%x x%x\n",
			icmd->ulpStatus, icmd->un.ulpWord[4], icmd->ulpTimeout);
	phba->fc_stat.elsRcvDrop++;
}

/**
 * lpfc_find_vport_by_vpid - Find a vport on a HBA through vport identifier
 * @phba: pointer to lpfc hba data structure.
 * @vpi: host virtual N_Port identifier.
 *
 * This routine finds a vport on a HBA (referred by @phba) through a
 * @vpi. The function walks the HBA's vport list and returns the address
 * of the vport with the matching @vpi.
 *
 * Return code
 *    NULL - No vport with the matching @vpi found
 *    Otherwise - Address to the vport with the matching @vpi.
 **/
struct lpfc_vport *
lpfc_find_vport_by_vpid(struct lpfc_hba *phba, uint16_t vpi)
{
	struct lpfc_vport *vport;
	unsigned long flags;

	spin_lock_irqsave(&phba->hbalock, flags);
	list_for_each_entry(vport, &phba->port_list, listentry) {
		if (vport->vpi == vpi) {
			spin_unlock_irqrestore(&phba->hbalock, flags);
			return vport;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return NULL;
}

/**
 * lpfc_els_unsol_event - Process an unsolicited event from an els sli ring
 * @phba: pointer to lpfc hba data structure.
 * @pring: pointer to a SLI ring.
 * @elsiocb: pointer to lpfc els iocb data structure.
 *
 * This routine is used to process an unsolicited event received from a SLI
 * (Service Level Interface) ring. The actual processing of the data buffer
 * associated with the unsolicited event is done by invoking the routine
 * lpfc_els_unsol_buffer() after properly set up the iocb buffer from the
 * SLI ring on which the unsolicited event was received.
 **/
void
lpfc_els_unsol_event(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		     struct lpfc_iocbq *elsiocb)
{
	struct lpfc_vport *vport = phba->pport;
	IOCB_t *icmd = &elsiocb->iocb;
	dma_addr_t paddr;
	struct lpfc_dmabuf *bdeBuf1 = elsiocb->context2;
	struct lpfc_dmabuf *bdeBuf2 = elsiocb->context3;

	elsiocb->context1 = NULL;
	elsiocb->context2 = NULL;
	elsiocb->context3 = NULL;

	if (icmd->ulpStatus == IOSTAT_NEED_BUFFER) {
		lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);
	} else if (icmd->ulpStatus == IOSTAT_LOCAL_REJECT &&
	    (icmd->un.ulpWord[4] & 0xff) == IOERR_RCV_BUFFER_WAITING) {
		phba->fc_stat.NoRcvBuf++;
		/* Not enough posted buffers; Try posting more buffers */
		if (!(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED))
			lpfc_post_buffer(phba, pring, 0);
		return;
	}

	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    (icmd->ulpCommand == CMD_IOCB_RCV_ELS64_CX ||
	     icmd->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
		if (icmd->unsli3.rcvsli3.vpi == 0xffff)
			vport = phba->pport;
		else
			vport = lpfc_find_vport_by_vpid(phba,
				icmd->unsli3.rcvsli3.vpi - phba->vpi_base);
	}
	/* If there are no BDEs associated
	 * with this IOCB, there is nothing to do.
	 */
	if (icmd->ulpBdeCount == 0)
		return;

	/* type of ELS cmd is first 32bit word
	 * in packet
	 */
	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		elsiocb->context2 = bdeBuf1;
	} else {
		paddr = getPaddr(icmd->un.cont64[0].addrHigh,
				 icmd->un.cont64[0].addrLow);
		elsiocb->context2 = lpfc_sli_ringpostbuf_get(phba, pring,
							     paddr);
	}

	lpfc_els_unsol_buffer(phba, pring, vport, elsiocb);
	/*
	 * The different unsolicited event handlers would tell us
	 * if they are done with "mp" by setting context2 to NULL.
	 */
	if (elsiocb->context2) {
		lpfc_in_buf_free(phba, (struct lpfc_dmabuf *)elsiocb->context2);
		elsiocb->context2 = NULL;
	}

	/* RCV_ELS64_CX provide for 2 BDEs - process 2nd if included */
	if ((phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) &&
	    icmd->ulpBdeCount == 2) {
		elsiocb->context2 = bdeBuf2;
		lpfc_els_unsol_buffer(phba, pring, vport, elsiocb);
		/* free mp if we are done with it */
		if (elsiocb->context2) {
			lpfc_in_buf_free(phba, elsiocb->context2);
			elsiocb->context2 = NULL;
		}
	}
}

/**
 * lpfc_do_scr_ns_plogi - Issue a plogi to the name server for scr
 * @phba: pointer to lpfc hba data structure.
 * @vport: pointer to a virtual N_Port data structure.
 *
 * This routine issues a Port Login (PLOGI) to the Name Server with
 * State Change Request (SCR) for a @vport. This routine will create an
 * ndlp for the Name Server associated to the @vport if such node does
 * not already exist. The PLOGI to Name Server is issued by invoking the
 * lpfc_issue_els_plogi() routine. If Fabric-Device Management Interface
 * (FDMI) is configured to the @vport, a FDMI node will be created and
 * the PLOGI to FDMI is issued by invoking lpfc_issue_els_plogi() routine.
 **/
void
lpfc_do_scr_ns_plogi(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp, *ndlp_fdmi;

	ndlp = lpfc_findnode_did(vport, NameServer_DID);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp) {
			if (phba->fc_topology == TOPOLOGY_LOOP) {
				lpfc_disc_start(vport);
				return;
			}
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0251 NameServer login: no memory\n");
			return;
		}
		lpfc_nlp_init(vport, ndlp, NameServer_DID);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp) {
			if (phba->fc_topology == TOPOLOGY_LOOP) {
				lpfc_disc_start(vport);
				return;
			}
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					"0348 NameServer login: node freed\n");
			return;
		}
	}
	ndlp->nlp_type |= NLP_FABRIC;

	lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);

	if (lpfc_issue_els_plogi(vport, ndlp->nlp_DID, 0)) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0252 Cannot issue NameServer login\n");
		return;
	}

	if (vport->cfg_fdmi_on) {
		ndlp_fdmi = mempool_alloc(phba->nlp_mem_pool,
					  GFP_KERNEL);
		if (ndlp_fdmi) {
			lpfc_nlp_init(vport, ndlp_fdmi, FDMI_DID);
			ndlp_fdmi->nlp_type |= NLP_FABRIC;
			lpfc_nlp_set_state(vport, ndlp_fdmi,
				NLP_STE_PLOGI_ISSUE);
			lpfc_issue_els_plogi(vport, ndlp_fdmi->nlp_DID,
					     0);
		}
	}
	return;
}

/**
 * lpfc_cmpl_reg_new_vport - Completion callback function to register new vport
 * @phba: pointer to lpfc hba data structure.
 * @pmb: pointer to the driver internal queue element for mailbox command.
 *
 * This routine is the completion callback function to register new vport
 * mailbox command. If the new vport mailbox command completes successfully,
 * the fabric registration login shall be performed on physical port (the
 * new vport created is actually a physical port, with VPI 0) or the port
 * login to Name Server for State Change Request (SCR) will be performed
 * on virtual port (real virtual port, with VPI greater than 0).
 **/
static void
lpfc_cmpl_reg_new_vport(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;
	MAILBOX_t *mb = &pmb->u.mb;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);

	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				"0915 Register VPI failed : Status: x%x"
				" upd bit: x%x \n", mb->mbxStatus,
				 mb->un.varRegVpi.upd);
		if (phba->sli_rev == LPFC_SLI_REV4 &&
			mb->un.varRegVpi.upd)
			goto mbox_err_exit ;

		switch (mb->mbxStatus) {
		case 0x11:	/* unsupported feature */
		case 0x9603:	/* max_vpi exceeded */
		case 0x9602:	/* Link event since CLEAR_LA */
			/* giving up on vport registration */
			lpfc_vport_set_state(vport, FC_VPORT_FAILED);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
			spin_unlock_irq(shost->host_lock);
			lpfc_can_disctmo(vport);
			break;
		/* If reg_vpi fail with invalid VPI status, re-init VPI */
		case 0x20:
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			lpfc_init_vpi(phba, pmb, vport->vpi);
			pmb->vport = vport;
			pmb->mbox_cmpl = lpfc_init_vpi_cmpl;
			rc = lpfc_sli_issue_mbox(phba, pmb,
				MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED) {
				lpfc_printf_vlog(vport,
					KERN_ERR, LOG_MBOX,
					"2732 Failed to issue INIT_VPI"
					" mailbox command\n");
			} else {
				lpfc_nlp_put(ndlp);
				return;
			}

		default:
			/* Try to recover from this error */
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
			if (vport->port_type == LPFC_PHYSICAL_PORT
				&& !(vport->fc_flag & FC_LOGO_RCVD_DID_CHNG))
				lpfc_initial_flogi(vport);
			else
				lpfc_initial_fdisc(vport);
			break;
		}
	} else {
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		spin_unlock_irq(shost->host_lock);
		if (vport == phba->pport) {
			if (phba->sli_rev < LPFC_SLI_REV4)
				lpfc_issue_fabric_reglogin(vport);
			else {
				/*
				 * If the physical port is instantiated using
				 * FDISC, do not start vport discovery.
				 */
				if (vport->port_state != LPFC_FDISC)
					lpfc_start_fdiscs(phba);
				lpfc_do_scr_ns_plogi(phba, vport);
			}
		} else
			lpfc_do_scr_ns_plogi(phba, vport);
	}
mbox_err_exit:
	/* Now, we decrement the ndlp reference count held for this
	 * callback function
	 */
	lpfc_nlp_put(ndlp);

	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_register_new_vport - Register a new vport with a HBA
 * @phba: pointer to lpfc hba data structure.
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine registers the @vport as a new virtual port with a HBA.
 * It is done through a registering vpi mailbox command.
 **/
void
lpfc_register_new_vport(struct lpfc_hba *phba, struct lpfc_vport *vport,
			struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *mbox;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_reg_vpi(vport, mbox);
		mbox->vport = vport;
		mbox->context2 = lpfc_nlp_get(ndlp);
		mbox->mbox_cmpl = lpfc_cmpl_reg_new_vport;
		if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
		    == MBX_NOT_FINISHED) {
			/* mailbox command not success, decrement ndlp
			 * reference count for this command
			 */
			lpfc_nlp_put(ndlp);
			mempool_free(mbox, phba->mbox_mem_pool);

			lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				"0253 Register VPI: Can't send mbox\n");
			goto mbox_err_exit;
		}
	} else {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				 "0254 Register VPI: no memory\n");
		goto mbox_err_exit;
	}
	return;

mbox_err_exit:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);
	return;
}

/**
 * lpfc_cancel_all_vport_retry_delay_timer - Cancel all vport retry delay timer
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine cancels the retry delay timers to all the vports.
 **/
void
lpfc_cancel_all_vport_retry_delay_timer(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct lpfc_nodelist *ndlp;
	uint32_t link_state;
	int i;

	/* Treat this failure as linkdown for all vports */
	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	vports = lpfc_create_vport_work_array(phba);

	if (vports) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			ndlp = lpfc_findnode_did(vports[i], Fabric_DID);
			if (ndlp)
				lpfc_cancel_retry_delay_tmo(vports[i], ndlp);
			lpfc_els_flush_cmd(vports[i]);
		}
		lpfc_destroy_vport_work_array(phba, vports);
	}
}

/**
 * lpfc_retry_pport_discovery - Start timer to retry FLOGI.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine abort all pending discovery commands and
 * start a timer to retry FLOGI for the physical port
 * discovery.
 **/
void
lpfc_retry_pport_discovery(struct lpfc_hba *phba)
{
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host  *shost;

	/* Cancel the all vports retry delay retry timers */
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	/* If fabric require FLOGI, then re-instantiate physical login */
	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (!ndlp)
		return;

	shost = lpfc_shost_from_vport(phba->pport);
	mod_timer(&ndlp->nlp_delayfunc, jiffies + HZ);
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	ndlp->nlp_last_elscmd = ELS_CMD_FLOGI;
	phba->pport->port_state = LPFC_FLOGI;
	return;
}

/**
 * lpfc_fabric_login_reqd - Check if FLOGI required.
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to FDISC command iocb.
 * @rspiocb: pointer to FDISC response iocb.
 *
 * This routine checks if a FLOGI is reguired for FDISC
 * to succeed.
 **/
static int
lpfc_fabric_login_reqd(struct lpfc_hba *phba,
		struct lpfc_iocbq *cmdiocb,
		struct lpfc_iocbq *rspiocb)
{

	if ((rspiocb->iocb.ulpStatus != IOSTAT_FABRIC_RJT) ||
		(rspiocb->iocb.un.ulpWord[4] != RJT_LOGIN_REQUIRED))
		return 0;
	else
		return 1;
}

/**
 * lpfc_cmpl_els_fdisc - Completion function for fdisc iocb command
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function to a Fabric Discover
 * (FDISC) ELS command. Since all the FDISC ELS commands are issued
 * single threaded, each FDISC completion callback function will reset
 * the discovery timer for all vports such that the timers will not get
 * unnecessary timeout. The function checks the FDISC IOCB status. If error
 * detected, the vport will be set to FC_VPORT_FAILED state. Otherwise,the
 * vport will set to FC_VPORT_ACTIVE state. It then checks whether the DID
 * assigned to the vport has been changed with the completion of the FDISC
 * command. If so, both RPI (Remote Port Index) and VPI (Virtual Port Index)
 * are unregistered from the HBA, and then the lpfc_register_new_vport()
 * routine is invoked to register new vport with the HBA. Otherwise, the
 * lpfc_do_scr_ns_plogi() routine is invoked to issue a PLOGI to the Name
 * Server for State Change Request (SCR).
 **/
static void
lpfc_cmpl_els_fdisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_iocbq *piocb;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0123 FDISC completes. x%x/x%x prevDID: x%x\n",
			 irsp->ulpStatus, irsp->un.ulpWord[4],
			 vport->fc_prevDID);
	/* Since all FDISCs are being single threaded, we
	 * must reset the discovery timer for ALL vports
	 * waiting to send FDISC when one completes.
	 */
	list_for_each_entry(piocb, &phba->fabric_iocb_list, list) {
		lpfc_set_disctmo(piocb->vport);
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FDISC cmpl:      status:x%x/x%x prevdid:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], vport->fc_prevDID);

	if (irsp->ulpStatus) {

		if (lpfc_fabric_login_reqd(phba, cmdiocb, rspiocb)) {
			lpfc_retry_pport_discovery(phba);
			goto out;
		}

		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;
		/* FDISC failed */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0126 FDISC failed. (%d/%d)\n",
				 irsp->ulpStatus, irsp->un.ulpWord[4]);
		goto fdisc_failed;
	}
	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_VPORT_CVL_RCVD;
	vport->fc_flag &= ~FC_VPORT_LOGO_RCVD;
	vport->fc_flag |= FC_FABRIC;
	if (vport->phba->fc_topology == TOPOLOGY_LOOP)
		vport->fc_flag |=  FC_PUBLIC_LOOP;
	spin_unlock_irq(shost->host_lock);

	vport->fc_myDID = irsp->un.ulpWord[4] & Mask_DID;
	lpfc_vport_set_state(vport, FC_VPORT_ACTIVE);
	if ((vport->fc_prevDID != vport->fc_myDID) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		/* If our NportID changed, we need to ensure all
		 * remaining NPORTs get unreg_login'ed so we can
		 * issue unreg_vpi.
		 */
		list_for_each_entry_safe(np, next_np,
			&vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(ndlp) ||
			    (np->nlp_state != NLP_STE_NPR_NODE) ||
			    !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unreg_rpi(vport, np);
		}
		lpfc_cleanup_pending_mbox(vport);
		lpfc_mbx_unreg_vpi(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		if (phba->sli_rev == LPFC_SLI_REV4)
			vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		else
			vport->fc_flag |= FC_LOGO_RCVD_DID_CHNG;
		spin_unlock_irq(shost->host_lock);
	} else if ((phba->sli_rev == LPFC_SLI_REV4) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {
		/*
		 * Driver needs to re-reg VPI in order for f/w
		 * to update the MAC address.
		 */
		lpfc_register_new_vport(phba, vport, ndlp);
		return ;
	}

	if (vport->fc_flag & FC_VPORT_NEEDS_INIT_VPI)
		lpfc_issue_init_vpi(vport);
	else if (vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
		lpfc_register_new_vport(phba, vport, ndlp);
	else
		lpfc_do_scr_ns_plogi(phba, vport);
	goto out;
fdisc_failed:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	/* Cancel discovery timer */
	lpfc_can_disctmo(vport);
	lpfc_nlp_put(ndlp);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
}

/**
 * lpfc_issue_els_fdisc - Issue a fdisc iocb command
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine prepares and issues a Fabric Discover (FDISC) IOCB to
 * a remote node (@ndlp) off a @vport. It uses the lpfc_issue_fabric_iocb()
 * routine to issue the IOCB, which makes sure only one outstanding fabric
 * IOCB will be sent off HBA at any given time.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the FDISC ELS command.
 *
 * Return code
 *   0 - Successfully issued fdisc iocb command
 *   1 - Failed to issue fdisc iocb command
 **/
static int
lpfc_issue_els_fdisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct serv_parm *sp;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int did = ndlp->nlp_DID;
	int rc;

	vport->port_state = LPFC_FDISC;
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_FDISC);
	if (!elsiocb) {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0255 Issue FDISC: no IOCB\n");
		return 1;
	}

	icmd = &elsiocb->iocb;
	icmd->un.elsreq64.myID = 0;
	icmd->un.elsreq64.fl = 1;

	if  (phba->sli_rev == LPFC_SLI_REV4) {
		/* FDISC needs to be 1 for WQE VPI */
		elsiocb->iocb.ulpCt_h = (SLI4_CT_VPI >> 1) & 1;
		elsiocb->iocb.ulpCt_l = SLI4_CT_VPI & 1 ;
		/* Set the ulpContext to the vpi */
		elsiocb->iocb.ulpContext = vport->vpi + phba->vpi_base;
	} else {
		/* For FDISC, Let FDISC rsp set the NPortID for this VPI */
		icmd->ulpCt_h = 1;
		icmd->ulpCt_l = 0;
	}

	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_FDISC;
	pcmd += sizeof(uint32_t); /* CSP Word 1 */
	memcpy(pcmd, &vport->phba->pport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;
	/* Setup CSPs accordingly for Fabric */
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;

	pcmd += sizeof(uint32_t); /* CSP Word 2 */
	pcmd += sizeof(uint32_t); /* CSP Word 3 */
	pcmd += sizeof(uint32_t); /* CSP Word 4 */
	pcmd += sizeof(uint32_t); /* Port Name */
	memcpy(pcmd, &vport->fc_portname, 8);
	pcmd += sizeof(uint32_t); /* Node Name */
	pcmd += sizeof(uint32_t); /* Node Name */
	memcpy(pcmd, &vport->fc_nodename, 8);

	lpfc_set_disctmo(vport);

	phba->fc_stat.elsXmitFDISC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_fdisc;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FDISC:     did:x%x",
		did, 0, 0);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0256 Issue FDISC: Cannot send IOCB\n");
		return 1;
	}
	lpfc_vport_set_state(vport, FC_VPORT_INITIALIZING);
	return 0;
}

/**
 * lpfc_cmpl_els_npiv_logo - Completion function with vport logo
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function to the issuing of a LOGO
 * ELS command off a vport. It frees the command IOCB and then decrement the
 * reference count held on ndlp for this completion function, indicating that
 * the reference to the ndlp is no long needed. Note that the
 * lpfc_els_free_iocb() routine decrements the ndlp reference held for this
 * callback function and an additional explicit ndlp reference decrementation
 * will trigger the actual release of the ndlp.
 **/
static void
lpfc_cmpl_els_npiv_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	ndlp = (struct lpfc_nodelist *)cmdiocb->context1;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO npiv cmpl:  status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], irsp->un.rcvels.remoteID);

	lpfc_els_free_iocb(phba, cmdiocb);
	vport->unreg_vpi_cmpl = VPORT_ERROR;

	/* Trigger the release of the ndlp after logo */
	lpfc_nlp_put(ndlp);
}

/**
 * lpfc_issue_els_npiv_logo - Issue a logo off a vport
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine issues a LOGO ELS command to an @ndlp off a @vport.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the LOGO ELS command.
 *
 * Return codes
 *   0 - Successfully issued logo off the @vport
 *   1 - Failed to issue logo off the @vport
 **/
int
lpfc_issue_els_npiv_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = 2 * sizeof(uint32_t) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, 0, ndlp, ndlp->nlp_DID,
				     ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	/* Fill in LOGO payload */
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO npiv  did:x%x flg:x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, 0);

	elsiocb->iocb_cmpl = lpfc_cmpl_els_npiv_logo;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_fabric_block_timeout - Handler function to the fabric block timer
 * @ptr: holder for the timer function associated data.
 *
 * This routine is invoked by the fabric iocb block timer after
 * timeout. It posts the fabric iocb block timeout event by setting the
 * WORKER_FABRIC_BLOCK_TMO bit to work port event bitmap and then invokes
 * lpfc_worker_wake_up() routine to wake up the worker thread. It is for
 * the worker thread to invoke the lpfc_unblock_fabric_iocbs() on the
 * posted event WORKER_FABRIC_BLOCK_TMO.
 **/
void
lpfc_fabric_block_timeout(unsigned long ptr)
{
	struct lpfc_hba  *phba = (struct lpfc_hba *) ptr;
	unsigned long iflags;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	tmo_posted = phba->pport->work_port_events & WORKER_FABRIC_BLOCK_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_FABRIC_BLOCK_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_resume_fabric_iocbs - Issue a fabric iocb from driver internal list
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine issues one fabric iocb from the driver internal list to
 * the HBA. It first checks whether it's ready to issue one fabric iocb to
 * the HBA (whether there is no outstanding fabric iocb). If so, it shall
 * remove one pending fabric iocb from the driver internal list and invokes
 * lpfc_sli_issue_iocb() routine to send the fabric iocb to the HBA.
 **/
static void
lpfc_resume_fabric_iocbs(struct lpfc_hba *phba)
{
	struct lpfc_iocbq *iocb;
	unsigned long iflags;
	int ret;
	IOCB_t *cmd;

repeat:
	iocb = NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	/* Post any pending iocb to the SLI layer */
	if (atomic_read(&phba->fabric_iocb_count) == 0) {
		list_remove_head(&phba->fabric_iocb_list, iocb, typeof(*iocb),
				 list);
		if (iocb)
			/* Increment fabric iocb count to hold the position */
			atomic_inc(&phba->fabric_iocb_count);
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (iocb) {
		iocb->fabric_iocb_cmpl = iocb->iocb_cmpl;
		iocb->iocb_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->iocb_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
			"Fabric sched1:   ste:x%x",
			iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->iocb_cmpl = iocb->fabric_iocb_cmpl;
			iocb->fabric_iocb_cmpl = NULL;
			iocb->iocb_flag &= ~LPFC_IO_FABRIC;
			cmd = &iocb->iocb;
			cmd->ulpStatus = IOSTAT_LOCAL_REJECT;
			cmd->un.ulpWord[4] = IOERR_SLI_ABORTED;
			iocb->iocb_cmpl(phba, iocb, iocb);

			atomic_dec(&phba->fabric_iocb_count);
			goto repeat;
		}
	}

	return;
}

/**
 * lpfc_unblock_fabric_iocbs - Unblock issuing fabric iocb command
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine unblocks the  issuing fabric iocb command. The function
 * will clear the fabric iocb block bit and then invoke the routine
 * lpfc_resume_fabric_iocbs() to issue one of the pending fabric iocb
 * from the driver internal fabric iocb list.
 **/
void
lpfc_unblock_fabric_iocbs(struct lpfc_hba *phba)
{
	clear_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	lpfc_resume_fabric_iocbs(phba);
	return;
}

/**
 * lpfc_block_fabric_iocbs - Block issuing fabric iocb command
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine blocks the issuing fabric iocb for a specified amount of
 * time (currently 100 ms). This is done by set the fabric iocb block bit
 * and set up a timeout timer for 100ms. When the block bit is set, no more
 * fabric iocb will be issued out of the HBA.
 **/
static void
lpfc_block_fabric_iocbs(struct lpfc_hba *phba)
{
	int blocked;

	blocked = test_and_set_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);
	/* Start a timer to unblock fabric iocbs after 100ms */
	if (!blocked)
		mod_timer(&phba->fabric_block_timer, jiffies + HZ/10 );

	return;
}

/**
 * lpfc_cmpl_fabric_iocb - Completion callback function for fabric iocb
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the callback function that is put to the fabric iocb's
 * callback function pointer (iocb->iocb_cmpl). The original iocb's callback
 * function pointer has been stored in iocb->fabric_iocb_cmpl. This callback
 * function first restores and invokes the original iocb's callback function
 * and then invokes the lpfc_resume_fabric_iocbs() routine to issue the next
 * fabric bound iocb from the driver internal fabric iocb list onto the wire.
 **/
static void
lpfc_cmpl_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	struct lpfc_iocbq *rspiocb)
{
	struct ls_rjt stat;

	if ((cmdiocb->iocb_flag & LPFC_IO_FABRIC) != LPFC_IO_FABRIC)
		BUG();

	switch (rspiocb->iocb.ulpStatus) {
		case IOSTAT_NPORT_RJT:
		case IOSTAT_FABRIC_RJT:
			if (rspiocb->iocb.un.ulpWord[4] & RJT_UNAVAIL_TEMP) {
				lpfc_block_fabric_iocbs(phba);
			}
			break;

		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			lpfc_block_fabric_iocbs(phba);
			break;

		case IOSTAT_LS_RJT:
			stat.un.lsRjtError =
				be32_to_cpu(rspiocb->iocb.un.ulpWord[4]);
			if ((stat.un.b.lsRjtRsnCode == LSRJT_UNABLE_TPC) ||
				(stat.un.b.lsRjtRsnCode == LSRJT_LOGICAL_BSY))
				lpfc_block_fabric_iocbs(phba);
			break;
	}

	if (atomic_read(&phba->fabric_iocb_count) == 0)
		BUG();

	cmdiocb->iocb_cmpl = cmdiocb->fabric_iocb_cmpl;
	cmdiocb->fabric_iocb_cmpl = NULL;
	cmdiocb->iocb_flag &= ~LPFC_IO_FABRIC;
	cmdiocb->iocb_cmpl(phba, cmdiocb, rspiocb);

	atomic_dec(&phba->fabric_iocb_count);
	if (!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags)) {
		/* Post any pending iocbs to HBA */
		lpfc_resume_fabric_iocbs(phba);
	}
}

/**
 * lpfc_issue_fabric_iocb - Issue a fabric iocb command
 * @phba: pointer to lpfc hba data structure.
 * @iocb: pointer to lpfc command iocb data structure.
 *
 * This routine is used as the top-level API for issuing a fabric iocb command
 * such as FLOGI and FDISC. To accommodate certain switch fabric, this driver
 * function makes sure that only one fabric bound iocb will be outstanding at
 * any given time. As such, this function will first check to see whether there
 * is already an outstanding fabric iocb on the wire. If so, it will put the
 * newly issued iocb onto the driver internal fabric iocb list, waiting to be
 * issued later. Otherwise, it will issue the iocb on the wire and update the
 * fabric iocb count it indicate that there is one fabric iocb on the wire.
 *
 * Note, this implementation has a potential sending out fabric IOCBs out of
 * order. The problem is caused by the construction of the "ready" boolen does
 * not include the condition that the internal fabric IOCB list is empty. As
 * such, it is possible a fabric IOCB issued by this routine might be "jump"
 * ahead of the fabric IOCBs in the internal list.
 *
 * Return code
 *   IOCB_SUCCESS - either fabric iocb put on the list or issued successfully
 *   IOCB_ERROR - failed to issue fabric iocb
 **/
static int
lpfc_issue_fabric_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *iocb)
{
	unsigned long iflags;
	int ready;
	int ret;

	if (atomic_read(&phba->fabric_iocb_count) > 1)
		BUG();

	spin_lock_irqsave(&phba->hbalock, iflags);
	ready = atomic_read(&phba->fabric_iocb_count) == 0 &&
		!test_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);

	if (ready)
		/* Increment fabric iocb count to hold the position */
		atomic_inc(&phba->fabric_iocb_count);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (ready) {
		iocb->fabric_iocb_cmpl = iocb->iocb_cmpl;
		iocb->iocb_cmpl = lpfc_cmpl_fabric_iocb;
		iocb->iocb_flag |= LPFC_IO_FABRIC;

		lpfc_debugfs_disc_trc(iocb->vport, LPFC_DISC_TRC_ELS_CMD,
			"Fabric sched2:   ste:x%x",
			iocb->vport->port_state, 0, 0);

		ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, iocb, 0);

		if (ret == IOCB_ERROR) {
			iocb->iocb_cmpl = iocb->fabric_iocb_cmpl;
			iocb->fabric_iocb_cmpl = NULL;
			iocb->iocb_flag &= ~LPFC_IO_FABRIC;
			atomic_dec(&phba->fabric_iocb_count);
		}
	} else {
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&iocb->list, &phba->fabric_iocb_list);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		ret = IOCB_SUCCESS;
	}
	return ret;
}

/**
 * lpfc_fabric_abort_vport - Abort a vport's iocbs from driver fabric iocb list
 * @vport: pointer to a virtual N_Port data structure.
 *
 * This routine aborts all the IOCBs associated with a @vport from the
 * driver internal fabric IOCB list. The list contains fabric IOCBs to be
 * issued to the ELS IOCB ring. This abort function walks the fabric IOCB
 * list, removes each IOCB associated with the @vport off the list, set the
 * status feild to IOSTAT_LOCAL_REJECT, and invokes the callback function
 * associated with the IOCB.
 **/
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {

		if (piocb->vport != vport)
			continue;

		list_move_tail(&piocb->list, &completions);
	}
	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_fabric_abort_nport - Abort a ndlp's iocbs from driver fabric iocb list
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine aborts all the IOCBs associated with an @ndlp from the
 * driver internal fabric IOCB list. The list contains fabric IOCBs to be
 * issued to the ELS IOCB ring. This abort function walks the fabric IOCB
 * list, removes each IOCB associated with the @ndlp off the list, set the
 * status feild to IOSTAT_LOCAL_REJECT, and invokes the callback function
 * associated with the IOCB.
 **/
void lpfc_fabric_abort_nport(struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);
	struct lpfc_hba  *phba = ndlp->phba;
	struct lpfc_iocbq *tmp_iocb, *piocb;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(piocb, tmp_iocb, &phba->fabric_iocb_list,
				 list) {
		if ((lpfc_check_sli_ndlp(phba, pring, piocb, ndlp))) {

			list_move_tail(&piocb->list, &completions);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_fabric_abort_hba - Abort all iocbs on driver fabric iocb list
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine aborts all the IOCBs currently on the driver internal
 * fabric IOCB list. The list contains fabric IOCBs to be issued to the ELS
 * IOCB ring. This function takes the entire IOCB list off the fabric IOCB
 * list, removes IOCBs off the list, set the status feild to
 * IOSTAT_LOCAL_REJECT, and invokes the callback function associated with
 * the IOCB.
 **/
void lpfc_fabric_abort_hba(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->fabric_iocb_list, &completions);
	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_sli4_els_xri_aborted - Slow-path process of els xri abort
 * @phba: pointer to lpfc hba data structure.
 * @axri: pointer to the els xri abort wcqe structure.
 *
 * This routine is invoked by the worker thread to process a SLI4 slow-path
 * ELS aborted xri.
 **/
void
lpfc_sli4_els_xri_aborted(struct lpfc_hba *phba,
			  struct sli4_wcqe_xri_aborted *axri)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	unsigned long iflag = 0;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irqsave(&phba->hbalock, iflag);
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_for_each_entry_safe(sglq_entry, sglq_next,
			&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->sli4_xritag == xri) {
			list_del(&sglq_entry->list);
			list_add_tail(&sglq_entry->list,
				&phba->sli4_hba.lpfc_sgl_list);
			sglq_entry->state = SGL_FREED;
			spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
			spin_unlock_irqrestore(&phba->hbalock, iflag);

			/* Check if TXQ queue needs to be serviced */
			if (pring->txq_cnt)
				lpfc_worker_wake_up(phba);
			return;
		}
	}
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	sglq_entry = __lpfc_get_active_sglq(phba, xri);
	if (!sglq_entry || (sglq_entry->sli4_xritag != xri)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return;
	}
	sglq_entry->state = SGL_XRI_ABORTED;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}
