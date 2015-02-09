/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#include "ql4_def.h"
#include "ql4_glbl.h"
#include "ql4_dbg.h"
#include "ql4_inline.h"


/**
 * qla4xxx_mailbox_command - issues mailbox commands
 * @ha: Pointer to host adapter structure.
 * @inCount: number of mailbox registers to load.
 * @outCount: number of mailbox registers to return.
 * @mbx_cmd: data pointer for mailbox in registers.
 * @mbx_sts: data pointer for mailbox out registers.
 *
 * This routine isssue mailbox commands and waits for completion.
 * If outCount is 0, this routine completes successfully WITHOUT waiting
 * for the mailbox command to complete.
 **/
int qla4xxx_mailbox_command(struct scsi_qla_host *ha, uint8_t inCount,
			    uint8_t outCount, uint32_t *mbx_cmd,
			    uint32_t *mbx_sts)
{
	int status = QLA_ERROR;
	uint8_t i;
	u_long wait_count;
	uint32_t intr_status;
	unsigned long flags = 0;

	/* Make sure that pointers are valid */
	if (!mbx_cmd || !mbx_sts) {
		DEBUG2(printk("scsi%ld: %s: Invalid mbx_cmd or mbx_sts "
			      "pointer\n", ha->host_no, __func__));
		return status;
	}

	if (is_qla8022(ha) &&
	    test_bit(AF_FW_RECOVERY, &ha->flags)) {
		DEBUG2(ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: prematurely "
		    "completing mbx cmd as firmware recovery detected\n",
		    ha->host_no, __func__));
		return status;
	}

	if ((is_aer_supported(ha)) &&
	    (test_bit(AF_PCI_CHANNEL_IO_PERM_FAILURE, &ha->flags))) {
		DEBUG2(printk(KERN_WARNING "scsi%ld: %s: Perm failure on EEH, "
		    "timeout MBX Exiting.\n", ha->host_no, __func__));
		return status;
	}

	/* Mailbox code active */
	wait_count = MBOX_TOV * 100;

	while (wait_count--) {
		mutex_lock(&ha->mbox_sem);
		if (!test_bit(AF_MBOX_COMMAND, &ha->flags)) {
			set_bit(AF_MBOX_COMMAND, &ha->flags);
			mutex_unlock(&ha->mbox_sem);
			break;
		}
		mutex_unlock(&ha->mbox_sem);
		if (!wait_count) {
			DEBUG2(printk("scsi%ld: %s: mbox_sem failed\n",
				ha->host_no, __func__));
			return status;
		}
		msleep(10);
	}

	/* To prevent overwriting mailbox registers for a command that has
	 * not yet been serviced, check to see if an active command
	 * (AEN, IOCB, etc.) is interrupting, then service it.
	 * -----------------------------------------------------------------
	 */
	spin_lock_irqsave(&ha->hardware_lock, flags);

	if (is_qla8022(ha)) {
		intr_status = readl(&ha->qla4_8xxx_reg->host_int);
		if (intr_status & ISRX_82XX_RISC_INT) {
			/* Service existing interrupt */
			DEBUG2(printk("scsi%ld: %s: "
			    "servicing existing interrupt\n",
			    ha->host_no, __func__));
			intr_status = readl(&ha->qla4_8xxx_reg->host_status);
			ha->isp_ops->interrupt_service_routine(ha, intr_status);
			clear_bit(AF_MBOX_COMMAND_DONE, &ha->flags);
			if (test_bit(AF_INTERRUPTS_ON, &ha->flags) &&
			    test_bit(AF_INTx_ENABLED, &ha->flags))
				qla4_8xxx_wr_32(ha,
				    ha->nx_legacy_intr.tgt_mask_reg,
				    0xfbff);
		}
	} else {
		intr_status = readl(&ha->reg->ctrl_status);
		if (intr_status & CSR_SCSI_PROCESSOR_INTR) {
			/* Service existing interrupt */
			ha->isp_ops->interrupt_service_routine(ha, intr_status);
			clear_bit(AF_MBOX_COMMAND_DONE, &ha->flags);
		}
	}

	ha->mbox_status_count = outCount;
	for (i = 0; i < outCount; i++)
		ha->mbox_status[i] = 0;

	if (is_qla8022(ha)) {
		/* Load all mailbox registers, except mailbox 0. */
		DEBUG5(
		    printk("scsi%ld: %s: Cmd ", ha->host_no, __func__);
		    for (i = 0; i < inCount; i++)
			printk("mb%d=%04x ", i, mbx_cmd[i]);
		    printk("\n"));

		for (i = 1; i < inCount; i++)
			writel(mbx_cmd[i], &ha->qla4_8xxx_reg->mailbox_in[i]);
		writel(mbx_cmd[0], &ha->qla4_8xxx_reg->mailbox_in[0]);
		readl(&ha->qla4_8xxx_reg->mailbox_in[0]);
		writel(HINT_MBX_INT_PENDING, &ha->qla4_8xxx_reg->hint);
	} else {
		/* Load all mailbox registers, except mailbox 0. */
		for (i = 1; i < inCount; i++)
			writel(mbx_cmd[i], &ha->reg->mailbox[i]);

		/* Wakeup firmware  */
		writel(mbx_cmd[0], &ha->reg->mailbox[0]);
		readl(&ha->reg->mailbox[0]);
		writel(set_rmask(CSR_INTR_RISC), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
	}

	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	/* Wait for completion */

	/*
	 * If we don't want status, don't wait for the mailbox command to
	 * complete.  For example, MBOX_CMD_RESET_FW doesn't return status,
	 * you must poll the inbound Interrupt Mask for completion.
	 */
	if (outCount == 0) {
		status = QLA_SUCCESS;
		goto mbox_exit;
	}

	/*
	 * Wait for completion: Poll or completion queue
	 */
	if (test_bit(AF_IRQ_ATTACHED, &ha->flags) &&
	    test_bit(AF_INTERRUPTS_ON, &ha->flags) &&
	    test_bit(AF_ONLINE, &ha->flags) &&
	    !test_bit(AF_HBA_GOING_AWAY, &ha->flags)) {
		/* Do not poll for completion. Use completion queue */
		set_bit(AF_MBOX_COMMAND_NOPOLL, &ha->flags);
		wait_for_completion_timeout(&ha->mbx_intr_comp, MBOX_TOV * HZ);
		clear_bit(AF_MBOX_COMMAND_NOPOLL, &ha->flags);
	} else {
		/* Poll for command to complete */
		wait_count = jiffies + MBOX_TOV * HZ;
		while (test_bit(AF_MBOX_COMMAND_DONE, &ha->flags) == 0) {
			if (time_after_eq(jiffies, wait_count))
				break;

			/*
			 * Service the interrupt.
			 * The ISR will save the mailbox status registers
			 * to a temporary storage location in the adapter
			 * structure.
			 */

			spin_lock_irqsave(&ha->hardware_lock, flags);
			if (is_qla8022(ha)) {
				intr_status =
				    readl(&ha->qla4_8xxx_reg->host_int);
				if (intr_status & ISRX_82XX_RISC_INT) {
					ha->mbox_status_count = outCount;
					intr_status =
					 readl(&ha->qla4_8xxx_reg->host_status);
					ha->isp_ops->interrupt_service_routine(
					    ha, intr_status);
					if (test_bit(AF_INTERRUPTS_ON,
					    &ha->flags) &&
					    test_bit(AF_INTx_ENABLED,
					    &ha->flags))
						qla4_8xxx_wr_32(ha,
						ha->nx_legacy_intr.tgt_mask_reg,
						0xfbff);
				}
			} else {
				intr_status = readl(&ha->reg->ctrl_status);
				if (intr_status & INTR_PENDING) {
					/*
					 * Service the interrupt.
					 * The ISR will save the mailbox status
					 * registers to a temporary storage
					 * location in the adapter structure.
					 */
					ha->mbox_status_count = outCount;
					ha->isp_ops->interrupt_service_routine(
					    ha, intr_status);
				}
			}
			spin_unlock_irqrestore(&ha->hardware_lock, flags);
			msleep(10);
		}
	}

	/* Check for mailbox timeout. */
	if (!test_bit(AF_MBOX_COMMAND_DONE, &ha->flags)) {
		if (is_qla8022(ha) &&
		    test_bit(AF_FW_RECOVERY, &ha->flags)) {
			DEBUG2(ql4_printk(KERN_INFO, ha,
			    "scsi%ld: %s: prematurely completing mbx cmd as "
			    "firmware recovery detected\n",
			    ha->host_no, __func__));
			goto mbox_exit;
		}
		DEBUG2(printk("scsi%ld: Mailbox Cmd 0x%08X timed out ...,"
			      " Scheduling Adapter Reset\n", ha->host_no,
			      mbx_cmd[0]));
		ha->mailbox_timeout_count++;
		mbx_sts[0] = (-1);
		set_bit(DPC_RESET_HA, &ha->dpc_flags);
		goto mbox_exit;
	}

	/*
	 * Copy the mailbox out registers to the caller's mailbox in/out
	 * structure.
	 */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (i = 0; i < outCount; i++)
		mbx_sts[i] = ha->mbox_status[i];

	/* Set return status and error flags (if applicable). */
	switch (ha->mbox_status[0]) {
	case MBOX_STS_COMMAND_COMPLETE:
		status = QLA_SUCCESS;
		break;

	case MBOX_STS_INTERMEDIATE_COMPLETION:
		status = QLA_SUCCESS;
		break;

	case MBOX_STS_BUSY:
		DEBUG2( printk("scsi%ld: %s: Cmd = %08X, ISP BUSY\n",
			       ha->host_no, __func__, mbx_cmd[0]));
		ha->mailbox_timeout_count++;
		break;

	default:
		DEBUG2(printk("scsi%ld: %s: **** FAILED, cmd = %08X, "
			      "sts = %08X ****\n", ha->host_no, __func__,
			      mbx_cmd[0], mbx_sts[0]));
		break;
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

mbox_exit:
	mutex_lock(&ha->mbox_sem);
	clear_bit(AF_MBOX_COMMAND, &ha->flags);
	mutex_unlock(&ha->mbox_sem);
	clear_bit(AF_MBOX_COMMAND_DONE, &ha->flags);

	return status;
}

void qla4xxx_mailbox_premature_completion(struct scsi_qla_host *ha)
{
	set_bit(AF_FW_RECOVERY, &ha->flags);
	ql4_printk(KERN_INFO, ha, "scsi%ld: %s: set FW RECOVERY!\n",
	    ha->host_no, __func__);

	if (test_bit(AF_MBOX_COMMAND, &ha->flags)) {
		if (test_bit(AF_MBOX_COMMAND_NOPOLL, &ha->flags)) {
			complete(&ha->mbx_intr_comp);
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: Due to fw "
			    "recovery, doing premature completion of "
			    "mbx cmd\n", ha->host_no, __func__);

		} else {
			set_bit(AF_MBOX_COMMAND_DONE, &ha->flags);
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: Due to fw "
			    "recovery, doing premature completion of "
			    "polling mbx cmd\n", ha->host_no, __func__);
		}
	}
}

static uint8_t
qla4xxx_set_ifcb(struct scsi_qla_host *ha, uint32_t *mbox_cmd,
		 uint32_t *mbox_sts, dma_addr_t init_fw_cb_dma)
{
	memset(mbox_cmd, 0, sizeof(mbox_cmd[0]) * MBOX_REG_COUNT);
	memset(mbox_sts, 0, sizeof(mbox_sts[0]) * MBOX_REG_COUNT);
	mbox_cmd[0] = MBOX_CMD_INITIALIZE_FIRMWARE;
	mbox_cmd[1] = 0;
	mbox_cmd[2] = LSDW(init_fw_cb_dma);
	mbox_cmd[3] = MSDW(init_fw_cb_dma);
	mbox_cmd[4] = sizeof(struct addr_ctrl_blk);
	mbox_cmd[5] = (IFCB_VER_MAX << 8) | IFCB_VER_MIN;

	if (qla4xxx_mailbox_command(ha, 6, 6, mbox_cmd, mbox_sts) !=
	    QLA_SUCCESS) {
		DEBUG2(printk(KERN_WARNING "scsi%ld: %s: "
			      "MBOX_CMD_INITIALIZE_FIRMWARE"
			      " failed w/ status %04X\n",
			      ha->host_no, __func__, mbox_sts[0]));
		return QLA_ERROR;
	}
	return QLA_SUCCESS;
}

static uint8_t
qla4xxx_get_ifcb(struct scsi_qla_host *ha, uint32_t *mbox_cmd,
		 uint32_t *mbox_sts, dma_addr_t init_fw_cb_dma)
{
	memset(mbox_cmd, 0, sizeof(mbox_cmd[0]) * MBOX_REG_COUNT);
	memset(mbox_sts, 0, sizeof(mbox_sts[0]) * MBOX_REG_COUNT);
	mbox_cmd[0] = MBOX_CMD_GET_INIT_FW_CTRL_BLOCK;
	mbox_cmd[2] = LSDW(init_fw_cb_dma);
	mbox_cmd[3] = MSDW(init_fw_cb_dma);
	mbox_cmd[4] = sizeof(struct addr_ctrl_blk);

	if (qla4xxx_mailbox_command(ha, 5, 5, mbox_cmd, mbox_sts) !=
	    QLA_SUCCESS) {
		DEBUG2(printk(KERN_WARNING "scsi%ld: %s: "
			      "MBOX_CMD_GET_INIT_FW_CTRL_BLOCK"
			      " failed w/ status %04X\n",
			      ha->host_no, __func__, mbox_sts[0]));
		return QLA_ERROR;
	}
	return QLA_SUCCESS;
}

static void
qla4xxx_update_local_ip(struct scsi_qla_host *ha,
			 struct addr_ctrl_blk  *init_fw_cb)
{
	/* Save IPv4 Address Info */
	memcpy(ha->ip_address, init_fw_cb->ipv4_addr,
		min(sizeof(ha->ip_address), sizeof(init_fw_cb->ipv4_addr)));
	memcpy(ha->subnet_mask, init_fw_cb->ipv4_subnet,
		min(sizeof(ha->subnet_mask), sizeof(init_fw_cb->ipv4_subnet)));
	memcpy(ha->gateway, init_fw_cb->ipv4_gw_addr,
		min(sizeof(ha->gateway), sizeof(init_fw_cb->ipv4_gw_addr)));

	if (is_ipv6_enabled(ha)) {
		/* Save IPv6 Address */
		ha->ipv6_link_local_state = init_fw_cb->ipv6_lnk_lcl_addr_state;
		ha->ipv6_addr0_state = init_fw_cb->ipv6_addr0_state;
		ha->ipv6_addr1_state = init_fw_cb->ipv6_addr1_state;
		ha->ipv6_default_router_state = init_fw_cb->ipv6_dflt_rtr_state;
		ha->ipv6_link_local_addr.in6_u.u6_addr8[0] = 0xFE;
		ha->ipv6_link_local_addr.in6_u.u6_addr8[1] = 0x80;

		memcpy(&ha->ipv6_link_local_addr.in6_u.u6_addr8[8],
			init_fw_cb->ipv6_if_id,
			min(sizeof(ha->ipv6_link_local_addr)/2,
			sizeof(init_fw_cb->ipv6_if_id)));
		memcpy(&ha->ipv6_addr0, init_fw_cb->ipv6_addr0,
			min(sizeof(ha->ipv6_addr0),
			sizeof(init_fw_cb->ipv6_addr0)));
		memcpy(&ha->ipv6_addr1, init_fw_cb->ipv6_addr1,
			min(sizeof(ha->ipv6_addr1),
			sizeof(init_fw_cb->ipv6_addr1)));
		memcpy(&ha->ipv6_default_router_addr,
			init_fw_cb->ipv6_dflt_rtr_addr,
			min(sizeof(ha->ipv6_default_router_addr),
			sizeof(init_fw_cb->ipv6_dflt_rtr_addr)));
	}
}

static uint8_t
qla4xxx_update_local_ifcb(struct scsi_qla_host *ha,
			  uint32_t *mbox_cmd,
			  uint32_t *mbox_sts,
			  struct addr_ctrl_blk  *init_fw_cb,
			  dma_addr_t init_fw_cb_dma)
{
	if (qla4xxx_get_ifcb(ha, mbox_cmd, mbox_sts, init_fw_cb_dma)
	    != QLA_SUCCESS) {
		DEBUG2(printk(KERN_WARNING
			      "scsi%ld: %s: Failed to get init_fw_ctrl_blk\n",
			      ha->host_no, __func__));
		return QLA_ERROR;
	}

	DEBUG2(qla4xxx_dump_buffer(init_fw_cb, sizeof(struct addr_ctrl_blk)));

	/* Save some info in adapter structure. */
	ha->acb_version = init_fw_cb->acb_version;
	ha->firmware_options = le16_to_cpu(init_fw_cb->fw_options);
	ha->tcp_options = le16_to_cpu(init_fw_cb->ipv4_tcp_opts);
	ha->ipv4_options = le16_to_cpu(init_fw_cb->ipv4_ip_opts);
	ha->ipv4_addr_state = le16_to_cpu(init_fw_cb->ipv4_addr_state);
	ha->heartbeat_interval = init_fw_cb->hb_interval;
	memcpy(ha->name_string, init_fw_cb->iscsi_name,
		min(sizeof(ha->name_string),
		sizeof(init_fw_cb->iscsi_name)));
	/*memcpy(ha->alias, init_fw_cb->Alias,
	       min(sizeof(ha->alias), sizeof(init_fw_cb->Alias)));*/

	/* Save Command Line Paramater info */
	ha->discovery_wait = ql4xdiscoverywait;

	if (ha->acb_version == ACB_SUPPORTED) {
		ha->ipv6_options = init_fw_cb->ipv6_opts;
		ha->ipv6_addl_options = init_fw_cb->ipv6_addtl_opts;
	}
	qla4xxx_update_local_ip(ha, init_fw_cb);

	return QLA_SUCCESS;
}

/**
 * qla4xxx_initialize_fw_cb - initializes firmware control block.
 * @ha: Pointer to host adapter structure.
 **/
int qla4xxx_initialize_fw_cb(struct scsi_qla_host * ha)
{
	struct addr_ctrl_blk *init_fw_cb;
	dma_addr_t init_fw_cb_dma;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	int status = QLA_ERROR;

	init_fw_cb = dma_alloc_coherent(&ha->pdev->dev,
					sizeof(struct addr_ctrl_blk),
					&init_fw_cb_dma, GFP_KERNEL);
	if (init_fw_cb == NULL) {
		DEBUG2(printk("scsi%ld: %s: Unable to alloc init_cb\n",
			      ha->host_no, __func__));
		goto exit_init_fw_cb_no_free;
	}
	memset(init_fw_cb, 0, sizeof(struct addr_ctrl_blk));

	/* Get Initialize Firmware Control Block. */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	if (qla4xxx_get_ifcb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb_dma) !=
	    QLA_SUCCESS) {
		dma_free_coherent(&ha->pdev->dev,
				  sizeof(struct addr_ctrl_blk),
				  init_fw_cb, init_fw_cb_dma);
		goto exit_init_fw_cb;
	}

	/* Initialize request and response queues. */
	qla4xxx_init_rings(ha);

	/* Fill in the request and response queue information. */
	init_fw_cb->rqq_consumer_idx = cpu_to_le16(ha->request_out);
	init_fw_cb->compq_producer_idx = cpu_to_le16(ha->response_in);
	init_fw_cb->rqq_len = __constant_cpu_to_le16(REQUEST_QUEUE_DEPTH);
	init_fw_cb->compq_len = __constant_cpu_to_le16(RESPONSE_QUEUE_DEPTH);
	init_fw_cb->rqq_addr_lo = cpu_to_le32(LSDW(ha->request_dma));
	init_fw_cb->rqq_addr_hi = cpu_to_le32(MSDW(ha->request_dma));
	init_fw_cb->compq_addr_lo = cpu_to_le32(LSDW(ha->response_dma));
	init_fw_cb->compq_addr_hi = cpu_to_le32(MSDW(ha->response_dma));
	init_fw_cb->shdwreg_addr_lo = cpu_to_le32(LSDW(ha->shadow_regs_dma));
	init_fw_cb->shdwreg_addr_hi = cpu_to_le32(MSDW(ha->shadow_regs_dma));

	/* Set up required options. */
	init_fw_cb->fw_options |=
		__constant_cpu_to_le16(FWOPT_SESSION_MODE |
				       FWOPT_INITIATOR_MODE);
	init_fw_cb->fw_options &= __constant_cpu_to_le16(~FWOPT_TARGET_MODE);

	if (qla4xxx_set_ifcb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb_dma)
		!= QLA_SUCCESS) {
		DEBUG2(printk(KERN_WARNING
			      "scsi%ld: %s: Failed to set init_fw_ctrl_blk\n",
			      ha->host_no, __func__));
		goto exit_init_fw_cb;
	}

	if (qla4xxx_update_local_ifcb(ha, &mbox_cmd[0], &mbox_sts[0],
		init_fw_cb, init_fw_cb_dma) != QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: Failed to update local ifcb\n",
				ha->host_no, __func__));
		goto exit_init_fw_cb;
	}
	status = QLA_SUCCESS;

exit_init_fw_cb:
	dma_free_coherent(&ha->pdev->dev, sizeof(struct addr_ctrl_blk),
				init_fw_cb, init_fw_cb_dma);
exit_init_fw_cb_no_free:
	return status;
}

/**
 * qla4xxx_get_dhcp_ip_address - gets HBA ip address via DHCP
 * @ha: Pointer to host adapter structure.
 **/
int qla4xxx_get_dhcp_ip_address(struct scsi_qla_host * ha)
{
	struct addr_ctrl_blk *init_fw_cb;
	dma_addr_t init_fw_cb_dma;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	init_fw_cb = dma_alloc_coherent(&ha->pdev->dev,
					sizeof(struct addr_ctrl_blk),
					&init_fw_cb_dma, GFP_KERNEL);
	if (init_fw_cb == NULL) {
		printk("scsi%ld: %s: Unable to alloc init_cb\n", ha->host_no,
		       __func__);
		return QLA_ERROR;
	}

	/* Get Initialize Firmware Control Block. */
	memset(init_fw_cb, 0, sizeof(struct addr_ctrl_blk));
	if (qla4xxx_get_ifcb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb_dma) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: Failed to get init_fw_ctrl_blk\n",
			      ha->host_no, __func__));
		dma_free_coherent(&ha->pdev->dev,
				  sizeof(struct addr_ctrl_blk),
				  init_fw_cb, init_fw_cb_dma);
		return QLA_ERROR;
	}

	/* Save IP Address. */
	qla4xxx_update_local_ip(ha, init_fw_cb);
	dma_free_coherent(&ha->pdev->dev, sizeof(struct addr_ctrl_blk),
				init_fw_cb, init_fw_cb_dma);

	return QLA_SUCCESS;
}

/**
 * qla4xxx_get_firmware_state - gets firmware state of HBA
 * @ha: Pointer to host adapter structure.
 **/
int qla4xxx_get_firmware_state(struct scsi_qla_host * ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	/* Get firmware version */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_FW_STATE;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 4, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: MBOX_CMD_GET_FW_STATE failed w/ "
			      "status %04X\n", ha->host_no, __func__,
			      mbox_sts[0]));
		return QLA_ERROR;
	}
	ha->firmware_state = mbox_sts[1];
	ha->board_id = mbox_sts[2];
	ha->addl_fw_state = mbox_sts[3];
	DEBUG2(printk("scsi%ld: %s firmware_state=0x%x\n",
		      ha->host_no, __func__, ha->firmware_state);)

	return QLA_SUCCESS;
}

/**
 * qla4xxx_get_firmware_status - retrieves firmware status
 * @ha: Pointer to host adapter structure.
 **/
int qla4xxx_get_firmware_status(struct scsi_qla_host * ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	/* Get firmware version */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_FW_STATUS;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 3, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: MBOX_CMD_GET_FW_STATUS failed w/ "
			      "status %04X\n", ha->host_no, __func__,
			      mbox_sts[0]));
		return QLA_ERROR;
	}

	ql4_printk(KERN_INFO, ha, "%ld firmare IOCBs available (%d).\n",
	    ha->host_no, mbox_cmd[2]);

	return QLA_SUCCESS;
}

/**
 * qla4xxx_get_fwddb_entry - retrieves firmware ddb entry
 * @ha: Pointer to host adapter structure.
 * @fw_ddb_index: Firmware's device database index
 * @fw_ddb_entry: Pointer to firmware's device database entry structure
 * @num_valid_ddb_entries: Pointer to number of valid ddb entries
 * @next_ddb_index: Pointer to next valid device database index
 * @fw_ddb_device_state: Pointer to device state
 **/
int qla4xxx_get_fwddb_entry(struct scsi_qla_host *ha,
			    uint16_t fw_ddb_index,
			    struct dev_db_entry *fw_ddb_entry,
			    dma_addr_t fw_ddb_entry_dma,
			    uint32_t *num_valid_ddb_entries,
			    uint32_t *next_ddb_index,
			    uint32_t *fw_ddb_device_state,
			    uint32_t *conn_err_detail,
			    uint16_t *tcp_source_port_num,
			    uint16_t *connection_id)
{
	int status = QLA_ERROR;
	uint16_t options;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	/* Make sure the device index is valid */
	if (fw_ddb_index >= MAX_DDB_ENTRIES) {
		DEBUG2(printk("scsi%ld: %s: ddb [%d] out of range.\n",
			      ha->host_no, __func__, fw_ddb_index));
		goto exit_get_fwddb;
	}
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY;
	mbox_cmd[1] = (uint32_t) fw_ddb_index;
	mbox_cmd[2] = LSDW(fw_ddb_entry_dma);
	mbox_cmd[3] = MSDW(fw_ddb_entry_dma);
	mbox_cmd[4] = sizeof(struct dev_db_entry);

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 7, &mbox_cmd[0], &mbox_sts[0]) ==
	    QLA_ERROR) {
		DEBUG2(printk("scsi%ld: %s: MBOX_CMD_GET_DATABASE_ENTRY failed"
			      " with status 0x%04X\n", ha->host_no, __func__,
			      mbox_sts[0]));
		goto exit_get_fwddb;
	}
	if (fw_ddb_index != mbox_sts[1]) {
		DEBUG2(printk("scsi%ld: %s: ddb mismatch [%d] != [%d].\n",
			      ha->host_no, __func__, fw_ddb_index,
			      mbox_sts[1]));
		goto exit_get_fwddb;
	}
	if (fw_ddb_entry) {
		options = le16_to_cpu(fw_ddb_entry->options);
		if (options & DDB_OPT_IPV6_DEVICE) {
			ql4_printk(KERN_INFO, ha, "%s: DDB[%d] MB0 %04x Tot %d "
				"Next %d State %04x ConnErr %08x %pI6 "
				":%04d \"%s\"\n", __func__, fw_ddb_index,
				mbox_sts[0], mbox_sts[2], mbox_sts[3],
				mbox_sts[4], mbox_sts[5],
				fw_ddb_entry->ip_addr,
				le16_to_cpu(fw_ddb_entry->port),
				fw_ddb_entry->iscsi_name);
		} else {
			ql4_printk(KERN_INFO, ha, "%s: DDB[%d] MB0 %04x Tot %d "
				"Next %d State %04x ConnErr %08x %pI4 "
				":%04d \"%s\"\n", __func__, fw_ddb_index,
				mbox_sts[0], mbox_sts[2], mbox_sts[3],
				mbox_sts[4], mbox_sts[5],
				fw_ddb_entry->ip_addr,
				le16_to_cpu(fw_ddb_entry->port),
				fw_ddb_entry->iscsi_name);
		}
	}
	if (num_valid_ddb_entries)
		*num_valid_ddb_entries = mbox_sts[2];
	if (next_ddb_index)
		*next_ddb_index = mbox_sts[3];
	if (fw_ddb_device_state)
		*fw_ddb_device_state = mbox_sts[4];

	/*
	 * RA: This mailbox has been changed to pass connection error and
	 * details.  Its true for ISP4010 as per Version E - Not sure when it
	 * was changed.	 Get the time2wait from the fw_dd_entry field :
	 * default_time2wait which we call it as minTime2Wait DEV_DB_ENTRY
	 * struct.
	 */
	if (conn_err_detail)
		*conn_err_detail = mbox_sts[5];
	if (tcp_source_port_num)
		*tcp_source_port_num = (uint16_t) (mbox_sts[6] >> 16);
	if (connection_id)
		*connection_id = (uint16_t) mbox_sts[6] & 0x00FF;
	status = QLA_SUCCESS;

exit_get_fwddb:
	return status;
}

/**
 * qla4xxx_set_fwddb_entry - sets a ddb entry.
 * @ha: Pointer to host adapter structure.
 * @fw_ddb_index: Firmware's device database index
 * @fw_ddb_entry: Pointer to firmware's ddb entry structure, or NULL.
 *
 * This routine initializes or updates the adapter's device database
 * entry for the specified device. It also triggers a login for the
 * specified device. Therefore, it may also be used as a secondary
 * login routine when a NULL pointer is specified for the fw_ddb_entry.
 **/
int qla4xxx_set_ddb_entry(struct scsi_qla_host * ha, uint16_t fw_ddb_index,
			  dma_addr_t fw_ddb_entry_dma)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	int status;

	/* Do not wait for completion. The firmware will send us an
	 * ASTS_DATABASE_CHANGED (0x8014) to notify us of the login status.
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_SET_DATABASE_ENTRY;
	mbox_cmd[1] = (uint32_t) fw_ddb_index;
	mbox_cmd[2] = LSDW(fw_ddb_entry_dma);
	mbox_cmd[3] = MSDW(fw_ddb_entry_dma);
	mbox_cmd[4] = sizeof(struct dev_db_entry);

	status = qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0],
	    &mbox_sts[0]);
	DEBUG2(printk("scsi%ld: %s: status=%d mbx0=0x%x mbx4=0x%x\n",
	    ha->host_no, __func__, status, mbox_sts[0], mbox_sts[4]);)

	return status;
}

/**
 * qla4xxx_get_crash_record - retrieves crash record.
 * @ha: Pointer to host adapter structure.
 *
 * This routine retrieves a crash record from the QLA4010 after an 8002h aen.
 **/
void qla4xxx_get_crash_record(struct scsi_qla_host * ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct crash_record *crash_record = NULL;
	dma_addr_t crash_record_dma = 0;
	uint32_t crash_record_size = 0;

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_cmd));

	/* Get size of crash record. */
	mbox_cmd[0] = MBOX_CMD_GET_CRASH_RECORD;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: ERROR: Unable to retrieve size!\n",
			      ha->host_no, __func__));
		goto exit_get_crash_record;
	}
	crash_record_size = mbox_sts[4];
	if (crash_record_size == 0) {
		DEBUG2(printk("scsi%ld: %s: ERROR: Crash record size is 0!\n",
			      ha->host_no, __func__));
		goto exit_get_crash_record;
	}

	/* Alloc Memory for Crash Record. */
	crash_record = dma_alloc_coherent(&ha->pdev->dev, crash_record_size,
					  &crash_record_dma, GFP_KERNEL);
	if (crash_record == NULL)
		goto exit_get_crash_record;

	/* Get Crash Record. */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_cmd));

	mbox_cmd[0] = MBOX_CMD_GET_CRASH_RECORD;
	mbox_cmd[2] = LSDW(crash_record_dma);
	mbox_cmd[3] = MSDW(crash_record_dma);
	mbox_cmd[4] = crash_record_size;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS)
		goto exit_get_crash_record;

	/* Dump Crash Record. */

exit_get_crash_record:
	if (crash_record)
		dma_free_coherent(&ha->pdev->dev, crash_record_size,
				  crash_record, crash_record_dma);
}

/**
 * qla4xxx_get_conn_event_log - retrieves connection event log
 * @ha: Pointer to host adapter structure.
 **/
void qla4xxx_get_conn_event_log(struct scsi_qla_host * ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct conn_event_log_entry *event_log = NULL;
	dma_addr_t event_log_dma = 0;
	uint32_t event_log_size = 0;
	uint32_t num_valid_entries;
	uint32_t      oldest_entry = 0;
	uint32_t	max_event_log_entries;
	uint8_t		i;


	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_cmd));

	/* Get size of crash record. */
	mbox_cmd[0] = MBOX_CMD_GET_CONN_EVENT_LOG;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS)
		goto exit_get_event_log;

	event_log_size = mbox_sts[4];
	if (event_log_size == 0)
		goto exit_get_event_log;

	/* Alloc Memory for Crash Record. */
	event_log = dma_alloc_coherent(&ha->pdev->dev, event_log_size,
				       &event_log_dma, GFP_KERNEL);
	if (event_log == NULL)
		goto exit_get_event_log;

	/* Get Crash Record. */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_cmd));

	mbox_cmd[0] = MBOX_CMD_GET_CONN_EVENT_LOG;
	mbox_cmd[2] = LSDW(event_log_dma);
	mbox_cmd[3] = MSDW(event_log_dma);

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: ERROR: Unable to retrieve event "
			      "log!\n", ha->host_no, __func__));
		goto exit_get_event_log;
	}

	/* Dump Event Log. */
	num_valid_entries = mbox_sts[1];

	max_event_log_entries = event_log_size /
		sizeof(struct conn_event_log_entry);

	if (num_valid_entries > max_event_log_entries)
		oldest_entry = num_valid_entries % max_event_log_entries;

	DEBUG3(printk("scsi%ld: Connection Event Log Dump (%d entries):\n",
		      ha->host_no, num_valid_entries));

	if (ql4xextended_error_logging == 3) {
		if (oldest_entry == 0) {
			/* Circular Buffer has not wrapped around */
			for (i=0; i < num_valid_entries; i++) {
				qla4xxx_dump_buffer((uint8_t *)event_log+
						    (i*sizeof(*event_log)),
						    sizeof(*event_log));
			}
		}
		else {
			/* Circular Buffer has wrapped around -
			 * display accordingly*/
			for (i=oldest_entry; i < max_event_log_entries; i++) {
				qla4xxx_dump_buffer((uint8_t *)event_log+
						    (i*sizeof(*event_log)),
						    sizeof(*event_log));
			}
			for (i=0; i < oldest_entry; i++) {
				qla4xxx_dump_buffer((uint8_t *)event_log+
						    (i*sizeof(*event_log)),
						    sizeof(*event_log));
			}
		}
	}

exit_get_event_log:
	if (event_log)
		dma_free_coherent(&ha->pdev->dev, event_log_size, event_log,
				  event_log_dma);
}

/**
 * qla4xxx_abort_task - issues Abort Task
 * @ha: Pointer to host adapter structure.
 * @srb: Pointer to srb entry
 *
 * This routine performs a LUN RESET on the specified target/lun.
 * The caller must ensure that the ddb_entry and lun_entry pointers
 * are valid before calling this routine.
 **/
int qla4xxx_abort_task(struct scsi_qla_host *ha, struct srb *srb)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_cmnd *cmd = srb->cmd;
	int status = QLA_SUCCESS;
	unsigned long flags = 0;
	uint32_t index;

	/*
	 * Send abort task command to ISP, so that the ISP will return
	 * request with ABORT status
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	spin_lock_irqsave(&ha->hardware_lock, flags);
	index = (unsigned long)(unsigned char *)cmd->host_scribble;
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	/* Firmware already posted completion on response queue */
	if (index == MAX_SRBS)
		return status;

	mbox_cmd[0] = MBOX_CMD_ABORT_TASK;
	mbox_cmd[1] = srb->fw_ddb_index;
	mbox_cmd[2] = index;
	/* Immediate Command Enable */
	mbox_cmd[5] = 0x01;

	qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0],
	    &mbox_sts[0]);
	if (mbox_sts[0] != MBOX_STS_COMMAND_COMPLETE) {
		status = QLA_ERROR;

		DEBUG2(printk(KERN_WARNING "scsi%ld:%d:%d: abort task FAILED: "
		    "mbx0=%04X, mb1=%04X, mb2=%04X, mb3=%04X, mb4=%04X\n",
		    ha->host_no, cmd->device->id, cmd->device->lun, mbox_sts[0],
		    mbox_sts[1], mbox_sts[2], mbox_sts[3], mbox_sts[4]));
	}

	return status;
}

/**
 * qla4xxx_reset_lun - issues LUN Reset
 * @ha: Pointer to host adapter structure.
 * @ddb_entry: Pointer to device database entry
 * @lun: lun number
 *
 * This routine performs a LUN RESET on the specified target/lun.
 * The caller must ensure that the ddb_entry and lun_entry pointers
 * are valid before calling this routine.
 **/
int qla4xxx_reset_lun(struct scsi_qla_host * ha, struct ddb_entry * ddb_entry,
		      int lun)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	int status = QLA_SUCCESS;

	DEBUG2(printk("scsi%ld:%d:%d: lun reset issued\n", ha->host_no,
		      ddb_entry->fw_ddb_index, lun));

	/*
	 * Send lun reset command to ISP, so that the ISP will return all
	 * outstanding requests with RESET status
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_LUN_RESET;
	mbox_cmd[1] = ddb_entry->fw_ddb_index;
	mbox_cmd[2] = lun << 8;
	mbox_cmd[5] = 0x01;	/* Immediate Command Enable */

	qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]);
	if (mbox_sts[0] != MBOX_STS_COMMAND_COMPLETE &&
	    mbox_sts[0] != MBOX_STS_COMMAND_ERROR)
		status = QLA_ERROR;

	return status;
}

/**
 * qla4xxx_reset_target - issues target Reset
 * @ha: Pointer to host adapter structure.
 * @db_entry: Pointer to device database entry
 * @un_entry: Pointer to lun entry structure
 *
 * This routine performs a TARGET RESET on the specified target.
 * The caller must ensure that the ddb_entry pointers
 * are valid before calling this routine.
 **/
int qla4xxx_reset_target(struct scsi_qla_host *ha,
			 struct ddb_entry *ddb_entry)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	int status = QLA_SUCCESS;

	DEBUG2(printk("scsi%ld:%d: target reset issued\n", ha->host_no,
		      ddb_entry->fw_ddb_index));

	/*
	 * Send target reset command to ISP, so that the ISP will return all
	 * outstanding requests with RESET status
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_TARGET_WARM_RESET;
	mbox_cmd[1] = ddb_entry->fw_ddb_index;
	mbox_cmd[5] = 0x01;	/* Immediate Command Enable */

	qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0],
				&mbox_sts[0]);
	if (mbox_sts[0] != MBOX_STS_COMMAND_COMPLETE &&
	    mbox_sts[0] != MBOX_STS_COMMAND_ERROR)
		status = QLA_ERROR;

	return status;
}

int qla4xxx_get_flash(struct scsi_qla_host * ha, dma_addr_t dma_addr,
		      uint32_t offset, uint32_t len)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_READ_FLASH;
	mbox_cmd[1] = LSDW(dma_addr);
	mbox_cmd[2] = MSDW(dma_addr);
	mbox_cmd[3] = offset;
	mbox_cmd[4] = len;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: MBOX_CMD_READ_FLASH, failed w/ "
		    "status %04X %04X, offset %08x, len %08x\n", ha->host_no,
		    __func__, mbox_sts[0], mbox_sts[1], offset, len));
		return QLA_ERROR;
	}
	return QLA_SUCCESS;
}

/**
 * qla4xxx_get_fw_version - gets firmware version
 * @ha: Pointer to host adapter structure.
 *
 * Retrieves the firmware version on HBA. In QLA4010, mailboxes 2 & 3 may
 * hold an address for data.  Make sure that we write 0 to those mailboxes,
 * if unused.
 **/
int qla4xxx_get_fw_version(struct scsi_qla_host * ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	/* Get firmware version. */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_ABOUT_FW;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: MBOX_CMD_ABOUT_FW failed w/ "
		    "status %04X\n", ha->host_no, __func__, mbox_sts[0]));
		return QLA_ERROR;
	}

	/* Save firmware version information. */
	ha->firmware_version[0] = mbox_sts[1];
	ha->firmware_version[1] = mbox_sts[2];
	ha->patch_number = mbox_sts[3];
	ha->build_number = mbox_sts[4];

	return QLA_SUCCESS;
}

static int qla4xxx_get_default_ddb(struct scsi_qla_host *ha,
				   dma_addr_t dma_addr)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY_DEFAULTS;
	mbox_cmd[2] = LSDW(dma_addr);
	mbox_cmd[3] = MSDW(dma_addr);

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld: %s: failed status %04X\n",
		     ha->host_no, __func__, mbox_sts[0]));
		return QLA_ERROR;
	}
	return QLA_SUCCESS;
}

static int qla4xxx_req_ddb_entry(struct scsi_qla_host *ha, uint32_t *ddb_index)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_REQUEST_DATABASE_ENTRY;
	mbox_cmd[1] = MAX_PRST_DEV_DB_ENTRIES;

	if (qla4xxx_mailbox_command(ha, MBOX_REG_COUNT, 3, &mbox_cmd[0], &mbox_sts[0]) !=
	    QLA_SUCCESS) {
		if (mbox_sts[0] == MBOX_STS_COMMAND_ERROR) {
			*ddb_index = mbox_sts[2];
		} else {
			DEBUG2(printk("scsi%ld: %s: failed status %04X\n",
			     ha->host_no, __func__, mbox_sts[0]));
			return QLA_ERROR;
		}
	} else {
		*ddb_index = MAX_PRST_DEV_DB_ENTRIES;
	}

	return QLA_SUCCESS;
}


int qla4xxx_send_tgts(struct scsi_qla_host *ha, char *ip, uint16_t port)
{
	struct dev_db_entry *fw_ddb_entry;
	dma_addr_t fw_ddb_entry_dma;
	uint32_t ddb_index;
	int ret_val = QLA_SUCCESS;


	fw_ddb_entry = dma_alloc_coherent(&ha->pdev->dev,
					  sizeof(*fw_ddb_entry),
					  &fw_ddb_entry_dma, GFP_KERNEL);
	if (!fw_ddb_entry) {
		DEBUG2(printk("scsi%ld: %s: Unable to allocate dma buffer.\n",
			      ha->host_no, __func__));
		ret_val = QLA_ERROR;
		goto exit_send_tgts_no_free;
	}

	ret_val = qla4xxx_get_default_ddb(ha, fw_ddb_entry_dma);
	if (ret_val != QLA_SUCCESS)
		goto exit_send_tgts;

	ret_val = qla4xxx_req_ddb_entry(ha, &ddb_index);
	if (ret_val != QLA_SUCCESS)
		goto exit_send_tgts;

	memset(fw_ddb_entry->iscsi_alias, 0,
	       sizeof(fw_ddb_entry->iscsi_alias));

	memset(fw_ddb_entry->iscsi_name, 0,
	       sizeof(fw_ddb_entry->iscsi_name));

	memset(fw_ddb_entry->ip_addr, 0, sizeof(fw_ddb_entry->ip_addr));
	memset(fw_ddb_entry->tgt_addr, 0,
	       sizeof(fw_ddb_entry->tgt_addr));

	fw_ddb_entry->options = (DDB_OPT_DISC_SESSION | DDB_OPT_TARGET);
	fw_ddb_entry->port = cpu_to_le16(ntohs(port));

	fw_ddb_entry->ip_addr[0] = *ip;
	fw_ddb_entry->ip_addr[1] = *(ip + 1);
	fw_ddb_entry->ip_addr[2] = *(ip + 2);
	fw_ddb_entry->ip_addr[3] = *(ip + 3);

	ret_val = qla4xxx_set_ddb_entry(ha, ddb_index, fw_ddb_entry_dma);

exit_send_tgts:
	dma_free_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
			  fw_ddb_entry, fw_ddb_entry_dma);
exit_send_tgts_no_free:
	return ret_val;
}
