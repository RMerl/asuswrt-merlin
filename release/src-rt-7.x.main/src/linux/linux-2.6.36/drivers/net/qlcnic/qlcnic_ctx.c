/*
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called "COPYING".
 *
 */

#include "qlcnic.h"

static u32
qlcnic_poll_rsp(struct qlcnic_adapter *adapter)
{
	u32 rsp;
	int timeout = 0;

	do {
		/* give atleast 1ms for firmware to respond */
		msleep(1);

		if (++timeout > QLCNIC_OS_CRB_RETRY_COUNT)
			return QLCNIC_CDRP_RSP_TIMEOUT;

		rsp = QLCRD32(adapter, QLCNIC_CDRP_CRB_OFFSET);
	} while (!QLCNIC_CDRP_IS_RSP(rsp));

	return rsp;
}

u32
qlcnic_issue_cmd(struct qlcnic_adapter *adapter,
	u32 pci_fn, u32 version, u32 arg1, u32 arg2, u32 arg3, u32 cmd)
{
	u32 rsp;
	u32 signature;
	u32 rcode = QLCNIC_RCODE_SUCCESS;
	struct pci_dev *pdev = adapter->pdev;

	signature = QLCNIC_CDRP_SIGNATURE_MAKE(pci_fn, version);

	/* Acquire semaphore before accessing CRB */
	if (qlcnic_api_lock(adapter))
		return QLCNIC_RCODE_TIMEOUT;

	QLCWR32(adapter, QLCNIC_SIGN_CRB_OFFSET, signature);
	QLCWR32(adapter, QLCNIC_ARG1_CRB_OFFSET, arg1);
	QLCWR32(adapter, QLCNIC_ARG2_CRB_OFFSET, arg2);
	QLCWR32(adapter, QLCNIC_ARG3_CRB_OFFSET, arg3);
	QLCWR32(adapter, QLCNIC_CDRP_CRB_OFFSET, QLCNIC_CDRP_FORM_CMD(cmd));

	rsp = qlcnic_poll_rsp(adapter);

	if (rsp == QLCNIC_CDRP_RSP_TIMEOUT) {
		dev_err(&pdev->dev, "card response timeout.\n");
		rcode = QLCNIC_RCODE_TIMEOUT;
	} else if (rsp == QLCNIC_CDRP_RSP_FAIL) {
		rcode = QLCRD32(adapter, QLCNIC_ARG1_CRB_OFFSET);
		dev_err(&pdev->dev, "failed card response code:0x%x\n",
				rcode);
	}

	/* Release semaphore */
	qlcnic_api_unlock(adapter);

	return rcode;
}

int
qlcnic_fw_cmd_set_mtu(struct qlcnic_adapter *adapter, int mtu)
{
	struct qlcnic_recv_context *recv_ctx = &adapter->recv_ctx;

	if (recv_ctx->state == QLCNIC_HOST_CTX_STATE_ACTIVE) {
		if (qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			recv_ctx->context_id,
			mtu,
			0,
			QLCNIC_CDRP_CMD_SET_MTU)) {

			dev_err(&adapter->pdev->dev, "Failed to set mtu\n");
			return -EIO;
		}
	}

	return 0;
}

static int
qlcnic_fw_cmd_create_rx_ctx(struct qlcnic_adapter *adapter)
{
	void *addr;
	struct qlcnic_hostrq_rx_ctx *prq;
	struct qlcnic_cardrsp_rx_ctx *prsp;
	struct qlcnic_hostrq_rds_ring *prq_rds;
	struct qlcnic_hostrq_sds_ring *prq_sds;
	struct qlcnic_cardrsp_rds_ring *prsp_rds;
	struct qlcnic_cardrsp_sds_ring *prsp_sds;
	struct qlcnic_host_rds_ring *rds_ring;
	struct qlcnic_host_sds_ring *sds_ring;

	dma_addr_t hostrq_phys_addr, cardrsp_phys_addr;
	u64 phys_addr;

	int i, nrds_rings, nsds_rings;
	size_t rq_size, rsp_size;
	u32 cap, reg, val, reg2;
	int err;

	struct qlcnic_recv_context *recv_ctx = &adapter->recv_ctx;

	nrds_rings = adapter->max_rds_rings;
	nsds_rings = adapter->max_sds_rings;

	rq_size =
		SIZEOF_HOSTRQ_RX(struct qlcnic_hostrq_rx_ctx, nrds_rings,
						nsds_rings);
	rsp_size =
		SIZEOF_CARDRSP_RX(struct qlcnic_cardrsp_rx_ctx, nrds_rings,
						nsds_rings);

	addr = pci_alloc_consistent(adapter->pdev,
				rq_size, &hostrq_phys_addr);
	if (addr == NULL)
		return -ENOMEM;
	prq = (struct qlcnic_hostrq_rx_ctx *)addr;

	addr = pci_alloc_consistent(adapter->pdev,
			rsp_size, &cardrsp_phys_addr);
	if (addr == NULL) {
		err = -ENOMEM;
		goto out_free_rq;
	}
	prsp = (struct qlcnic_cardrsp_rx_ctx *)addr;

	prq->host_rsp_dma_addr = cpu_to_le64(cardrsp_phys_addr);

	cap = (QLCNIC_CAP0_LEGACY_CONTEXT | QLCNIC_CAP0_LEGACY_MN
						| QLCNIC_CAP0_VALIDOFF);
	cap |= (QLCNIC_CAP0_JUMBO_CONTIGUOUS | QLCNIC_CAP0_LRO_CONTIGUOUS);

	prq->valid_field_offset = offsetof(struct qlcnic_hostrq_rx_ctx,
							 msix_handler);
	prq->txrx_sds_binding = nsds_rings - 1;

	prq->capabilities[0] = cpu_to_le32(cap);
	prq->host_int_crb_mode =
		cpu_to_le32(QLCNIC_HOST_INT_CRB_MODE_SHARED);
	prq->host_rds_crb_mode =
		cpu_to_le32(QLCNIC_HOST_RDS_CRB_MODE_UNIQUE);

	prq->num_rds_rings = cpu_to_le16(nrds_rings);
	prq->num_sds_rings = cpu_to_le16(nsds_rings);
	prq->rds_ring_offset = cpu_to_le32(0);

	val = le32_to_cpu(prq->rds_ring_offset) +
		(sizeof(struct qlcnic_hostrq_rds_ring) * nrds_rings);
	prq->sds_ring_offset = cpu_to_le32(val);

	prq_rds = (struct qlcnic_hostrq_rds_ring *)(prq->data +
			le32_to_cpu(prq->rds_ring_offset));

	for (i = 0; i < nrds_rings; i++) {

		rds_ring = &recv_ctx->rds_rings[i];
		rds_ring->producer = 0;

		prq_rds[i].host_phys_addr = cpu_to_le64(rds_ring->phys_addr);
		prq_rds[i].ring_size = cpu_to_le32(rds_ring->num_desc);
		prq_rds[i].ring_kind = cpu_to_le32(i);
		prq_rds[i].buff_size = cpu_to_le64(rds_ring->dma_size);
	}

	prq_sds = (struct qlcnic_hostrq_sds_ring *)(prq->data +
			le32_to_cpu(prq->sds_ring_offset));

	for (i = 0; i < nsds_rings; i++) {

		sds_ring = &recv_ctx->sds_rings[i];
		sds_ring->consumer = 0;
		memset(sds_ring->desc_head, 0, STATUS_DESC_RINGSIZE(sds_ring));

		prq_sds[i].host_phys_addr = cpu_to_le64(sds_ring->phys_addr);
		prq_sds[i].ring_size = cpu_to_le32(sds_ring->num_desc);
		prq_sds[i].msi_index = cpu_to_le16(i);
	}

	phys_addr = hostrq_phys_addr;
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			(u32)(phys_addr >> 32),
			(u32)(phys_addr & 0xffffffff),
			rq_size,
			QLCNIC_CDRP_CMD_CREATE_RX_CTX);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Failed to create rx ctx in firmware%d\n", err);
		goto out_free_rsp;
	}


	prsp_rds = ((struct qlcnic_cardrsp_rds_ring *)
			 &prsp->data[le32_to_cpu(prsp->rds_ring_offset)]);

	for (i = 0; i < le16_to_cpu(prsp->num_rds_rings); i++) {
		rds_ring = &recv_ctx->rds_rings[i];

		reg = le32_to_cpu(prsp_rds[i].host_producer_crb);
		rds_ring->crb_rcv_producer = adapter->ahw.pci_base0 + reg;
	}

	prsp_sds = ((struct qlcnic_cardrsp_sds_ring *)
			&prsp->data[le32_to_cpu(prsp->sds_ring_offset)]);

	for (i = 0; i < le16_to_cpu(prsp->num_sds_rings); i++) {
		sds_ring = &recv_ctx->sds_rings[i];

		reg = le32_to_cpu(prsp_sds[i].host_consumer_crb);
		reg2 = le32_to_cpu(prsp_sds[i].interrupt_crb);

		sds_ring->crb_sts_consumer = adapter->ahw.pci_base0 + reg;
		sds_ring->crb_intr_mask = adapter->ahw.pci_base0 + reg2;
	}

	recv_ctx->state = le32_to_cpu(prsp->host_ctx_state);
	recv_ctx->context_id = le16_to_cpu(prsp->context_id);
	recv_ctx->virt_port = prsp->virt_port;

out_free_rsp:
	pci_free_consistent(adapter->pdev, rsp_size, prsp, cardrsp_phys_addr);
out_free_rq:
	pci_free_consistent(adapter->pdev, rq_size, prq, hostrq_phys_addr);
	return err;
}

static void
qlcnic_fw_cmd_destroy_rx_ctx(struct qlcnic_adapter *adapter)
{
	struct qlcnic_recv_context *recv_ctx = &adapter->recv_ctx;

	if (qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			recv_ctx->context_id,
			QLCNIC_DESTROY_CTX_RESET,
			0,
			QLCNIC_CDRP_CMD_DESTROY_RX_CTX)) {

		dev_err(&adapter->pdev->dev,
			"Failed to destroy rx ctx in firmware\n");
	}

	recv_ctx->state = QLCNIC_HOST_CTX_STATE_FREED;
}

static int
qlcnic_fw_cmd_create_tx_ctx(struct qlcnic_adapter *adapter)
{
	struct qlcnic_hostrq_tx_ctx	*prq;
	struct qlcnic_hostrq_cds_ring	*prq_cds;
	struct qlcnic_cardrsp_tx_ctx	*prsp;
	void	*rq_addr, *rsp_addr;
	size_t	rq_size, rsp_size;
	u32	temp;
	int	err;
	u64	phys_addr;
	dma_addr_t	rq_phys_addr, rsp_phys_addr;
	struct qlcnic_host_tx_ring *tx_ring = adapter->tx_ring;

	/* reset host resources */
	tx_ring->producer = 0;
	tx_ring->sw_consumer = 0;
	*(tx_ring->hw_consumer) = 0;

	rq_size = SIZEOF_HOSTRQ_TX(struct qlcnic_hostrq_tx_ctx);
	rq_addr = pci_alloc_consistent(adapter->pdev,
		rq_size, &rq_phys_addr);
	if (!rq_addr)
		return -ENOMEM;

	rsp_size = SIZEOF_CARDRSP_TX(struct qlcnic_cardrsp_tx_ctx);
	rsp_addr = pci_alloc_consistent(adapter->pdev,
		rsp_size, &rsp_phys_addr);
	if (!rsp_addr) {
		err = -ENOMEM;
		goto out_free_rq;
	}

	memset(rq_addr, 0, rq_size);
	prq = (struct qlcnic_hostrq_tx_ctx *)rq_addr;

	memset(rsp_addr, 0, rsp_size);
	prsp = (struct qlcnic_cardrsp_tx_ctx *)rsp_addr;

	prq->host_rsp_dma_addr = cpu_to_le64(rsp_phys_addr);

	temp = (QLCNIC_CAP0_LEGACY_CONTEXT | QLCNIC_CAP0_LEGACY_MN |
					QLCNIC_CAP0_LSO);
	prq->capabilities[0] = cpu_to_le32(temp);

	prq->host_int_crb_mode =
		cpu_to_le32(QLCNIC_HOST_INT_CRB_MODE_SHARED);

	prq->interrupt_ctl = 0;
	prq->msi_index = 0;
	prq->cmd_cons_dma_addr = cpu_to_le64(tx_ring->hw_cons_phys_addr);

	prq_cds = &prq->cds_ring;

	prq_cds->host_phys_addr = cpu_to_le64(tx_ring->phys_addr);
	prq_cds->ring_size = cpu_to_le32(tx_ring->num_desc);

	phys_addr = rq_phys_addr;
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			(u32)(phys_addr >> 32),
			((u32)phys_addr & 0xffffffff),
			rq_size,
			QLCNIC_CDRP_CMD_CREATE_TX_CTX);

	if (err == QLCNIC_RCODE_SUCCESS) {
		temp = le32_to_cpu(prsp->cds_ring.host_producer_crb);
		tx_ring->crb_cmd_producer = adapter->ahw.pci_base0 + temp;

		adapter->tx_context_id =
			le16_to_cpu(prsp->context_id);
	} else {
		dev_err(&adapter->pdev->dev,
			"Failed to create tx ctx in firmware%d\n", err);
		err = -EIO;
	}

	pci_free_consistent(adapter->pdev, rsp_size, rsp_addr, rsp_phys_addr);

out_free_rq:
	pci_free_consistent(adapter->pdev, rq_size, rq_addr, rq_phys_addr);

	return err;
}

static void
qlcnic_fw_cmd_destroy_tx_ctx(struct qlcnic_adapter *adapter)
{
	if (qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			adapter->tx_context_id,
			QLCNIC_DESTROY_CTX_RESET,
			0,
			QLCNIC_CDRP_CMD_DESTROY_TX_CTX)) {

		dev_err(&adapter->pdev->dev,
			"Failed to destroy tx ctx in firmware\n");
	}
}

int
qlcnic_fw_cmd_query_phy(struct qlcnic_adapter *adapter, u32 reg, u32 *val)
{

	if (qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			reg,
			0,
			0,
			QLCNIC_CDRP_CMD_READ_PHY)) {

		return -EIO;
	}

	return QLCRD32(adapter, QLCNIC_ARG1_CRB_OFFSET);
}

int
qlcnic_fw_cmd_set_phy(struct qlcnic_adapter *adapter, u32 reg, u32 val)
{
	return qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			reg,
			val,
			0,
			QLCNIC_CDRP_CMD_WRITE_PHY);
}

int qlcnic_alloc_hw_resources(struct qlcnic_adapter *adapter)
{
	void *addr;
	int err;
	int ring;
	struct qlcnic_recv_context *recv_ctx;
	struct qlcnic_host_rds_ring *rds_ring;
	struct qlcnic_host_sds_ring *sds_ring;
	struct qlcnic_host_tx_ring *tx_ring;

	struct pci_dev *pdev = adapter->pdev;

	recv_ctx = &adapter->recv_ctx;
	tx_ring = adapter->tx_ring;

	tx_ring->hw_consumer = (__le32 *)pci_alloc_consistent(pdev, sizeof(u32),
						&tx_ring->hw_cons_phys_addr);
	if (tx_ring->hw_consumer == NULL) {
		dev_err(&pdev->dev, "failed to allocate tx consumer\n");
		return -ENOMEM;
	}
	*(tx_ring->hw_consumer) = 0;

	/* cmd desc ring */
	addr = pci_alloc_consistent(pdev, TX_DESC_RINGSIZE(tx_ring),
			&tx_ring->phys_addr);

	if (addr == NULL) {
		dev_err(&pdev->dev, "failed to allocate tx desc ring\n");
		err = -ENOMEM;
		goto err_out_free;
	}

	tx_ring->desc_head = (struct cmd_desc_type0 *)addr;

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		rds_ring = &recv_ctx->rds_rings[ring];
		addr = pci_alloc_consistent(adapter->pdev,
				RCV_DESC_RINGSIZE(rds_ring),
				&rds_ring->phys_addr);
		if (addr == NULL) {
			dev_err(&pdev->dev,
				"failed to allocate rds ring [%d]\n", ring);
			err = -ENOMEM;
			goto err_out_free;
		}
		rds_ring->desc_head = (struct rcv_desc *)addr;

	}

	for (ring = 0; ring < adapter->max_sds_rings; ring++) {
		sds_ring = &recv_ctx->sds_rings[ring];

		addr = pci_alloc_consistent(adapter->pdev,
				STATUS_DESC_RINGSIZE(sds_ring),
				&sds_ring->phys_addr);
		if (addr == NULL) {
			dev_err(&pdev->dev,
				"failed to allocate sds ring [%d]\n", ring);
			err = -ENOMEM;
			goto err_out_free;
		}
		sds_ring->desc_head = (struct status_desc *)addr;
	}

	return 0;

err_out_free:
	qlcnic_free_hw_resources(adapter);
	return err;
}


int qlcnic_fw_create_ctx(struct qlcnic_adapter *adapter)
{
	int err;

	err = qlcnic_fw_cmd_create_rx_ctx(adapter);
	if (err)
		return err;

	err = qlcnic_fw_cmd_create_tx_ctx(adapter);
	if (err) {
		qlcnic_fw_cmd_destroy_rx_ctx(adapter);
		return err;
	}

	set_bit(__QLCNIC_FW_ATTACHED, &adapter->state);
	return 0;
}

void qlcnic_fw_destroy_ctx(struct qlcnic_adapter *adapter)
{
	if (test_and_clear_bit(__QLCNIC_FW_ATTACHED, &adapter->state)) {
		qlcnic_fw_cmd_destroy_rx_ctx(adapter);
		qlcnic_fw_cmd_destroy_tx_ctx(adapter);

		/* Allow dma queues to drain after context reset */
		msleep(20);
	}
}

void qlcnic_free_hw_resources(struct qlcnic_adapter *adapter)
{
	struct qlcnic_recv_context *recv_ctx;
	struct qlcnic_host_rds_ring *rds_ring;
	struct qlcnic_host_sds_ring *sds_ring;
	struct qlcnic_host_tx_ring *tx_ring;
	int ring;

	recv_ctx = &adapter->recv_ctx;

	tx_ring = adapter->tx_ring;
	if (tx_ring->hw_consumer != NULL) {
		pci_free_consistent(adapter->pdev,
				sizeof(u32),
				tx_ring->hw_consumer,
				tx_ring->hw_cons_phys_addr);
		tx_ring->hw_consumer = NULL;
	}

	if (tx_ring->desc_head != NULL) {
		pci_free_consistent(adapter->pdev,
				TX_DESC_RINGSIZE(tx_ring),
				tx_ring->desc_head, tx_ring->phys_addr);
		tx_ring->desc_head = NULL;
	}

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		rds_ring = &recv_ctx->rds_rings[ring];

		if (rds_ring->desc_head != NULL) {
			pci_free_consistent(adapter->pdev,
					RCV_DESC_RINGSIZE(rds_ring),
					rds_ring->desc_head,
					rds_ring->phys_addr);
			rds_ring->desc_head = NULL;
		}
	}

	for (ring = 0; ring < adapter->max_sds_rings; ring++) {
		sds_ring = &recv_ctx->sds_rings[ring];

		if (sds_ring->desc_head != NULL) {
			pci_free_consistent(adapter->pdev,
				STATUS_DESC_RINGSIZE(sds_ring),
				sds_ring->desc_head,
				sds_ring->phys_addr);
			sds_ring->desc_head = NULL;
		}
	}
}

/* Set MAC address of a NIC partition */
int qlcnic_set_mac_address(struct qlcnic_adapter *adapter, u8* mac)
{
	int err = 0;
	u32 arg1, arg2, arg3;

	arg1 = adapter->ahw.pci_func | BIT_9;
	arg2 = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	arg3 = mac[4] | (mac[5] << 16);

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			arg1,
			arg2,
			arg3,
			QLCNIC_CDRP_CMD_MAC_ADDRESS);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to set mac address%d\n", err);
		err = -EIO;
	}

	return err;
}

/* Get MAC address of a NIC partition */
int qlcnic_get_mac_address(struct qlcnic_adapter *adapter, u8 *mac)
{
	int err;
	u32 arg1;

	arg1 = adapter->ahw.pci_func | BIT_8;
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			arg1,
			0,
			0,
			QLCNIC_CDRP_CMD_MAC_ADDRESS);

	if (err == QLCNIC_RCODE_SUCCESS)
		qlcnic_fetch_mac(adapter, QLCNIC_ARG1_CRB_OFFSET,
				QLCNIC_ARG2_CRB_OFFSET, 0, mac);
	else {
		dev_err(&adapter->pdev->dev,
			"Failed to get mac address%d\n", err);
		err = -EIO;
	}

	return err;
}

/* Get info of a NIC partition */
int qlcnic_get_nic_info(struct qlcnic_adapter *adapter,
				struct qlcnic_info *npar_info, u8 func_id)
{
	int	err;
	dma_addr_t nic_dma_t;
	struct qlcnic_info *nic_info;
	void *nic_info_addr;
	size_t	nic_size = sizeof(struct qlcnic_info);

	nic_info_addr = pci_alloc_consistent(adapter->pdev,
		nic_size, &nic_dma_t);
	if (!nic_info_addr)
		return -ENOMEM;
	memset(nic_info_addr, 0, nic_size);

	nic_info = (struct qlcnic_info *) nic_info_addr;
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			MSD(nic_dma_t),
			LSD(nic_dma_t),
			(func_id << 16 | nic_size),
			QLCNIC_CDRP_CMD_GET_NIC_INFO);

	if (err == QLCNIC_RCODE_SUCCESS) {
		npar_info->pci_func = le16_to_cpu(nic_info->pci_func);
		npar_info->op_mode = le16_to_cpu(nic_info->op_mode);
		npar_info->phys_port = le16_to_cpu(nic_info->phys_port);
		npar_info->switch_mode = le16_to_cpu(nic_info->switch_mode);
		npar_info->max_tx_ques = le16_to_cpu(nic_info->max_tx_ques);
		npar_info->max_rx_ques = le16_to_cpu(nic_info->max_rx_ques);
		npar_info->min_tx_bw = le16_to_cpu(nic_info->min_tx_bw);
		npar_info->max_tx_bw = le16_to_cpu(nic_info->max_tx_bw);
		npar_info->capabilities = le32_to_cpu(nic_info->capabilities);
		npar_info->max_mtu = le16_to_cpu(nic_info->max_mtu);

		dev_info(&adapter->pdev->dev,
			"phy port: %d switch_mode: %d,\n"
			"\tmax_tx_q: %d max_rx_q: %d min_tx_bw: 0x%x,\n"
			"\tmax_tx_bw: 0x%x max_mtu:0x%x, capabilities: 0x%x\n",
			npar_info->phys_port, npar_info->switch_mode,
			npar_info->max_tx_ques, npar_info->max_rx_ques,
			npar_info->min_tx_bw, npar_info->max_tx_bw,
			npar_info->max_mtu, npar_info->capabilities);
	} else {
		dev_err(&adapter->pdev->dev,
			"Failed to get nic info%d\n", err);
		err = -EIO;
	}

	pci_free_consistent(adapter->pdev, nic_size, nic_info_addr, nic_dma_t);
	return err;
}

/* Configure a NIC partition */
int qlcnic_set_nic_info(struct qlcnic_adapter *adapter, struct qlcnic_info *nic)
{
	int err = -EIO;
	dma_addr_t nic_dma_t;
	void *nic_info_addr;
	struct qlcnic_info *nic_info;
	size_t nic_size = sizeof(struct qlcnic_info);

	if (adapter->op_mode != QLCNIC_MGMT_FUNC)
		return err;

	nic_info_addr = pci_alloc_consistent(adapter->pdev, nic_size,
			&nic_dma_t);
	if (!nic_info_addr)
		return -ENOMEM;

	memset(nic_info_addr, 0, nic_size);
	nic_info = (struct qlcnic_info *)nic_info_addr;

	nic_info->pci_func = cpu_to_le16(nic->pci_func);
	nic_info->op_mode = cpu_to_le16(nic->op_mode);
	nic_info->phys_port = cpu_to_le16(nic->phys_port);
	nic_info->switch_mode = cpu_to_le16(nic->switch_mode);
	nic_info->capabilities = cpu_to_le32(nic->capabilities);
	nic_info->max_mac_filters = nic->max_mac_filters;
	nic_info->max_tx_ques = cpu_to_le16(nic->max_tx_ques);
	nic_info->max_rx_ques = cpu_to_le16(nic->max_rx_ques);
	nic_info->min_tx_bw = cpu_to_le16(nic->min_tx_bw);
	nic_info->max_tx_bw = cpu_to_le16(nic->max_tx_bw);

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			MSD(nic_dma_t),
			LSD(nic_dma_t),
			((nic->pci_func << 16) | nic_size),
			QLCNIC_CDRP_CMD_SET_NIC_INFO);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to set nic info%d\n", err);
		err = -EIO;
	}

	pci_free_consistent(adapter->pdev, nic_size, nic_info_addr, nic_dma_t);
	return err;
}

/* Get PCI Info of a partition */
int qlcnic_get_pci_info(struct qlcnic_adapter *adapter,
				struct qlcnic_pci_info *pci_info)
{
	int err = 0, i;
	dma_addr_t pci_info_dma_t;
	struct qlcnic_pci_info *npar;
	void *pci_info_addr;
	size_t npar_size = sizeof(struct qlcnic_pci_info);
	size_t pci_size = npar_size * QLCNIC_MAX_PCI_FUNC;

	pci_info_addr = pci_alloc_consistent(adapter->pdev, pci_size,
			&pci_info_dma_t);
	if (!pci_info_addr)
		return -ENOMEM;
	memset(pci_info_addr, 0, pci_size);

	npar = (struct qlcnic_pci_info *) pci_info_addr;
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			MSD(pci_info_dma_t),
			LSD(pci_info_dma_t),
			pci_size,
			QLCNIC_CDRP_CMD_GET_PCI_INFO);

	if (err == QLCNIC_RCODE_SUCCESS) {
		for (i = 0; i < QLCNIC_MAX_PCI_FUNC; i++, npar++, pci_info++) {
			pci_info->id = le32_to_cpu(npar->id);
			pci_info->active = le32_to_cpu(npar->active);
			pci_info->type = le32_to_cpu(npar->type);
			pci_info->default_port =
				le32_to_cpu(npar->default_port);
			pci_info->tx_min_bw =
				le32_to_cpu(npar->tx_min_bw);
			pci_info->tx_max_bw =
				le32_to_cpu(npar->tx_max_bw);
			memcpy(pci_info->mac, npar->mac, ETH_ALEN);
		}
	} else {
		dev_err(&adapter->pdev->dev,
			"Failed to get PCI Info%d\n", err);
		err = -EIO;
	}

	pci_free_consistent(adapter->pdev, pci_size, pci_info_addr,
		pci_info_dma_t);
	return err;
}

/* Reset a NIC partition */

int qlcnic_reset_partition(struct qlcnic_adapter *adapter, u8 func_no)
{
	int err = -EIO;

	if (adapter->op_mode != QLCNIC_MGMT_FUNC)
		return err;

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			func_no,
			0,
			0,
			QLCNIC_CDRP_CMD_RESET_NPAR);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to issue reset partition%d\n", err);
		err = -EIO;
	}

	return err;
}

/* Get eSwitch Capabilities */
int qlcnic_get_eswitch_capabilities(struct qlcnic_adapter *adapter, u8 port,
					struct qlcnic_eswitch *eswitch)
{
	int err = -EIO;
	u32 arg1, arg2;

	if (adapter->op_mode == QLCNIC_NON_PRIV_FUNC)
		return err;

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			port,
			0,
			0,
			QLCNIC_CDRP_CMD_GET_ESWITCH_CAPABILITY);

	if (err == QLCNIC_RCODE_SUCCESS) {
		arg1 = QLCRD32(adapter, QLCNIC_ARG1_CRB_OFFSET);
		arg2 = QLCRD32(adapter, QLCNIC_ARG2_CRB_OFFSET);

		eswitch->port = arg1 & 0xf;
		eswitch->active_vports = LSB(arg2);
		eswitch->max_ucast_filters = MSB(arg2);
		eswitch->max_active_vlans = LSB(MSW(arg2));
		if (arg1 & BIT_6)
			eswitch->flags |= QLCNIC_SWITCH_VLAN_FILTERING;
		if (arg1 & BIT_7)
			eswitch->flags |= QLCNIC_SWITCH_PROMISC_MODE;
		if (arg1 & BIT_8)
			eswitch->flags |= QLCNIC_SWITCH_PORT_MIRRORING;
	} else {
		dev_err(&adapter->pdev->dev,
			"Failed to get eswitch capabilities%d\n", err);
	}

	return err;
}

/* Get current status of eswitch */
int qlcnic_get_eswitch_status(struct qlcnic_adapter *adapter, u8 port,
				struct qlcnic_eswitch *eswitch)
{
	int err = -EIO;
	u32 arg1, arg2;

	if (adapter->op_mode != QLCNIC_MGMT_FUNC)
		return err;

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			port,
			0,
			0,
			QLCNIC_CDRP_CMD_GET_ESWITCH_STATUS);

	if (err == QLCNIC_RCODE_SUCCESS) {
		arg1 = QLCRD32(adapter, QLCNIC_ARG1_CRB_OFFSET);
		arg2 = QLCRD32(adapter, QLCNIC_ARG2_CRB_OFFSET);

		eswitch->port = arg1 & 0xf;
		eswitch->active_vports = LSB(arg2);
		eswitch->active_ucast_filters = MSB(arg2);
		eswitch->active_vlans = LSB(MSW(arg2));
		if (arg1 & BIT_6)
			eswitch->flags |= QLCNIC_SWITCH_VLAN_FILTERING;
		if (arg1 & BIT_8)
			eswitch->flags |= QLCNIC_SWITCH_PORT_MIRRORING;

	} else {
		dev_err(&adapter->pdev->dev,
			"Failed to get eswitch status%d\n", err);
	}

	return err;
}

/* Enable/Disable eSwitch */
int qlcnic_toggle_eswitch(struct qlcnic_adapter *adapter, u8 id, u8 enable)
{
	int err = -EIO;
	u32 arg1, arg2;
	struct qlcnic_eswitch *eswitch;

	if (adapter->op_mode != QLCNIC_MGMT_FUNC)
		return err;

	eswitch = &adapter->eswitch[id];
	if (!eswitch)
		return err;

	arg1 = eswitch->port | (enable ? BIT_4 : 0);
	arg2 = eswitch->active_vports | (eswitch->max_ucast_filters << 8) |
		(eswitch->max_active_vlans << 16);
	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			arg1,
			arg2,
			0,
			QLCNIC_CDRP_CMD_TOGGLE_ESWITCH);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to enable eswitch%d\n", eswitch->port);
		eswitch->flags &= ~QLCNIC_SWITCH_ENABLE;
		err = -EIO;
	} else {
		eswitch->flags |= QLCNIC_SWITCH_ENABLE;
		dev_info(&adapter->pdev->dev,
			"Enabled eSwitch for port %d\n", eswitch->port);
	}

	return err;
}

/* Configure eSwitch for port mirroring */
int qlcnic_config_port_mirroring(struct qlcnic_adapter *adapter, u8 id,
				u8 enable_mirroring, u8 pci_func)
{
	int err = -EIO;
	u32 arg1;

	if (adapter->op_mode != QLCNIC_MGMT_FUNC ||
		!(adapter->eswitch[id].flags & QLCNIC_SWITCH_ENABLE))
		return err;

	arg1 = id | (enable_mirroring ? BIT_4 : 0);
	arg1 |= pci_func << 8;

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			arg1,
			0,
			0,
			QLCNIC_CDRP_CMD_SET_PORTMIRRORING);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to configure port mirroring%d on eswitch:%d\n",
			pci_func, id);
	} else {
		dev_info(&adapter->pdev->dev,
			"Configured eSwitch %d for port mirroring:%d\n",
			id, pci_func);
	}

	return err;
}

/* Configure eSwitch port */
int qlcnic_config_switch_port(struct qlcnic_adapter *adapter, u8 id,
		int vlan_tagging, u8 discard_tagged, u8 promsc_mode,
		u8 mac_learn, u8 pci_func, u16 vlan_id)
{
	int err = -EIO;
	u32 arg1;
	struct qlcnic_eswitch *eswitch;

	if (adapter->op_mode != QLCNIC_MGMT_FUNC)
		return err;

	eswitch = &adapter->eswitch[id];
	if (!(eswitch->flags & QLCNIC_SWITCH_ENABLE))
		return err;

	arg1 = eswitch->port | (discard_tagged ? BIT_4 : 0);
	arg1 |= (promsc_mode ? BIT_6 : 0) | (mac_learn ? BIT_7 : 0);
	arg1 |= pci_func << 8;
	if (vlan_tagging)
		arg1 |= BIT_5 | (vlan_id << 16);

	err = qlcnic_issue_cmd(adapter,
			adapter->ahw.pci_func,
			adapter->fw_hal_version,
			arg1,
			0,
			0,
			QLCNIC_CDRP_CMD_CONFIGURE_ESWITCH);

	if (err != QLCNIC_RCODE_SUCCESS) {
		dev_err(&adapter->pdev->dev,
			"Failed to configure eswitch port%d\n", eswitch->port);
	} else {
		dev_info(&adapter->pdev->dev,
			"Configured eSwitch for port %d\n", eswitch->port);
	}

	return err;
}
