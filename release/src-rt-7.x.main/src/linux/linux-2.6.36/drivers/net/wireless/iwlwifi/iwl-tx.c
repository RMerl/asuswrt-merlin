/******************************************************************************
 *
 * Copyright(c) 2003 - 2010 Intel Corporation. All rights reserved.
 *
 * Portions of this file are derived from the ipw3945 project, as well
 * as portions of the ieee80211 subsystem header files.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#include <linux/etherdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <net/mac80211.h>
#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-sta.h"
#include "iwl-io.h"
#include "iwl-helpers.h"

/**
 * iwl_txq_update_write_ptr - Send new write index to hardware
 */
void iwl_txq_update_write_ptr(struct iwl_priv *priv, struct iwl_tx_queue *txq)
{
	u32 reg = 0;
	int txq_id = txq->q.id;

	if (txq->need_update == 0)
		return;

	/* if we're trying to save power */
	if (test_bit(STATUS_POWER_PMI, &priv->status)) {
		/* wake up nic if it's powered down ...
		 * uCode will wake up, and interrupt us again, so next
		 * time we'll skip this part. */
		reg = iwl_read32(priv, CSR_UCODE_DRV_GP1);

		if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
			IWL_DEBUG_INFO(priv, "Tx queue %d requesting wakeup, GP1 = 0x%x\n",
				      txq_id, reg);
			iwl_set_bit(priv, CSR_GP_CNTRL,
				    CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
			return;
		}

		iwl_write_direct32(priv, HBUS_TARG_WRPTR,
				     txq->q.write_ptr | (txq_id << 8));

	/* else not in power-save mode, uCode will never sleep when we're
	 * trying to tx (during RFKILL, we're not trying to tx). */
	} else
		iwl_write32(priv, HBUS_TARG_WRPTR,
			    txq->q.write_ptr | (txq_id << 8));

	txq->need_update = 0;
}
EXPORT_SYMBOL(iwl_txq_update_write_ptr);

/**
 * iwl_tx_queue_free - Deallocate DMA queue.
 * @txq: Transmit queue to deallocate.
 *
 * Empty queue by removing and destroying all BD's.
 * Free all buffers.
 * 0-fill, but do not free "txq" descriptor structure.
 */
void iwl_tx_queue_free(struct iwl_priv *priv, int txq_id)
{
	struct iwl_tx_queue *txq = &priv->txq[txq_id];
	struct iwl_queue *q = &txq->q;
	struct device *dev = &priv->pci_dev->dev;
	int i;

	if (q->n_bd == 0)
		return;

	/* first, empty all BD's */
	for (; q->write_ptr != q->read_ptr;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd))
		priv->cfg->ops->lib->txq_free_tfd(priv, txq);

	/* De-alloc array of command/tx buffers */
	for (i = 0; i < TFD_TX_CMD_SLOTS; i++)
		kfree(txq->cmd[i]);

	/* De-alloc circular buffer of TFDs */
	if (txq->q.n_bd)
		dma_free_coherent(dev, priv->hw_params.tfd_size *
				  txq->q.n_bd, txq->tfds, txq->q.dma_addr);

	/* De-alloc array of per-TFD driver data */
	kfree(txq->txb);
	txq->txb = NULL;

	/* deallocate arrays */
	kfree(txq->cmd);
	kfree(txq->meta);
	txq->cmd = NULL;
	txq->meta = NULL;

	/* 0-fill queue descriptor structure */
	memset(txq, 0, sizeof(*txq));
}
EXPORT_SYMBOL(iwl_tx_queue_free);

/**
 * iwl_cmd_queue_free - Deallocate DMA queue.
 * @txq: Transmit queue to deallocate.
 *
 * Empty queue by removing and destroying all BD's.
 * Free all buffers.
 * 0-fill, but do not free "txq" descriptor structure.
 */
void iwl_cmd_queue_free(struct iwl_priv *priv)
{
	struct iwl_tx_queue *txq = &priv->txq[IWL_CMD_QUEUE_NUM];
	struct iwl_queue *q = &txq->q;
	struct device *dev = &priv->pci_dev->dev;
	int i;
	bool huge = false;

	if (q->n_bd == 0)
		return;

	for (; q->read_ptr != q->write_ptr;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd)) {
		/* we have no way to tell if it is a huge cmd ATM */
		i = get_cmd_index(q, q->read_ptr, 0);

		if (txq->meta[i].flags & CMD_SIZE_HUGE) {
			huge = true;
			continue;
		}

		pci_unmap_single(priv->pci_dev,
				 dma_unmap_addr(&txq->meta[i], mapping),
				 dma_unmap_len(&txq->meta[i], len),
				 PCI_DMA_BIDIRECTIONAL);
	}
	if (huge) {
		i = q->n_window;
		pci_unmap_single(priv->pci_dev,
				 dma_unmap_addr(&txq->meta[i], mapping),
				 dma_unmap_len(&txq->meta[i], len),
				 PCI_DMA_BIDIRECTIONAL);
	}

	/* De-alloc array of command/tx buffers */
	for (i = 0; i <= TFD_CMD_SLOTS; i++)
		kfree(txq->cmd[i]);

	/* De-alloc circular buffer of TFDs */
	if (txq->q.n_bd)
		dma_free_coherent(dev, priv->hw_params.tfd_size * txq->q.n_bd,
				  txq->tfds, txq->q.dma_addr);

	/* deallocate arrays */
	kfree(txq->cmd);
	kfree(txq->meta);
	txq->cmd = NULL;
	txq->meta = NULL;

	/* 0-fill queue descriptor structure */
	memset(txq, 0, sizeof(*txq));
}
EXPORT_SYMBOL(iwl_cmd_queue_free);

/*************** DMA-QUEUE-GENERAL-FUNCTIONS  *****
 * DMA services
 *
 * Theory of operation
 *
 * A Tx or Rx queue resides in host DRAM, and is comprised of a circular buffer
 * of buffer descriptors, each of which points to one or more data buffers for
 * the device to read from or fill.  Driver and device exchange status of each
 * queue via "read" and "write" pointers.  Driver keeps minimum of 2 empty
 * entries in each circular buffer, to protect against confusing empty and full
 * queue states.
 *
 * The device reads or writes the data in the queues via the device's several
 * DMA/FIFO channels.  Each queue is mapped to a single DMA channel.
 *
 * For Tx queue, there are low mark and high mark limits. If, after queuing
 * the packet for Tx, free space become < low mark, Tx queue stopped. When
 * reclaiming packets (on 'tx done IRQ), if free space become > high mark,
 * Tx queue resumed.
 *
 * See more detailed info in iwl-4965-hw.h.
 ***************************************************/

int iwl_queue_space(const struct iwl_queue *q)
{
	int s = q->read_ptr - q->write_ptr;

	if (q->read_ptr > q->write_ptr)
		s -= q->n_bd;

	if (s <= 0)
		s += q->n_window;
	/* keep some reserve to not confuse empty and full situations */
	s -= 2;
	if (s < 0)
		s = 0;
	return s;
}
EXPORT_SYMBOL(iwl_queue_space);


/**
 * iwl_queue_init - Initialize queue's high/low-water and read/write indexes
 */
static int iwl_queue_init(struct iwl_priv *priv, struct iwl_queue *q,
			  int count, int slots_num, u32 id)
{
	q->n_bd = count;
	q->n_window = slots_num;
	q->id = id;

	/* count must be power-of-two size, otherwise iwl_queue_inc_wrap
	 * and iwl_queue_dec_wrap are broken. */
	BUG_ON(!is_power_of_2(count));

	/* slots_num must be power-of-two size, otherwise
	 * get_cmd_index is broken. */
	BUG_ON(!is_power_of_2(slots_num));

	q->low_mark = q->n_window / 4;
	if (q->low_mark < 4)
		q->low_mark = 4;

	q->high_mark = q->n_window / 8;
	if (q->high_mark < 2)
		q->high_mark = 2;

	q->write_ptr = q->read_ptr = 0;
	q->last_read_ptr = 0;
	q->repeat_same_read_ptr = 0;

	return 0;
}

/**
 * iwl_tx_queue_alloc - Alloc driver data and TFD CB for one Tx/cmd queue
 */
static int iwl_tx_queue_alloc(struct iwl_priv *priv,
			      struct iwl_tx_queue *txq, u32 id)
{
	struct device *dev = &priv->pci_dev->dev;
	size_t tfd_sz = priv->hw_params.tfd_size * TFD_QUEUE_SIZE_MAX;

	/* Driver private data, only for Tx (not command) queues,
	 * not shared with device. */
	if (id != IWL_CMD_QUEUE_NUM) {
		txq->txb = kzalloc(sizeof(txq->txb[0]) *
				   TFD_QUEUE_SIZE_MAX, GFP_KERNEL);
		if (!txq->txb) {
			IWL_ERR(priv, "kmalloc for auxiliary BD "
				  "structures failed\n");
			goto error;
		}
	} else {
		txq->txb = NULL;
	}

	/* Circular buffer of transmit frame descriptors (TFDs),
	 * shared with device */
	txq->tfds = dma_alloc_coherent(dev, tfd_sz, &txq->q.dma_addr,
				       GFP_KERNEL);
	if (!txq->tfds) {
		IWL_ERR(priv, "pci_alloc_consistent(%zd) failed\n", tfd_sz);
		goto error;
	}
	txq->q.id = id;

	return 0;

 error:
	kfree(txq->txb);
	txq->txb = NULL;

	return -ENOMEM;
}

/**
 * iwl_tx_queue_init - Allocate and initialize one tx/cmd queue
 */
int iwl_tx_queue_init(struct iwl_priv *priv, struct iwl_tx_queue *txq,
		      int slots_num, u32 txq_id)
{
	int i, len;
	int ret;
	int actual_slots = slots_num;

	/*
	 * Alloc buffer array for commands (Tx or other types of commands).
	 * For the command queue (#4), allocate command space + one big
	 * command for scan, since scan command is very huge; the system will
	 * not have two scans at the same time, so only one is needed.
	 * For normal Tx queues (all other queues), no super-size command
	 * space is needed.
	 */
	if (txq_id == IWL_CMD_QUEUE_NUM)
		actual_slots++;

	txq->meta = kzalloc(sizeof(struct iwl_cmd_meta) * actual_slots,
			    GFP_KERNEL);
	txq->cmd = kzalloc(sizeof(struct iwl_device_cmd *) * actual_slots,
			   GFP_KERNEL);

	if (!txq->meta || !txq->cmd)
		goto out_free_arrays;

	len = sizeof(struct iwl_device_cmd);
	for (i = 0; i < actual_slots; i++) {
		/* only happens for cmd queue */
		if (i == slots_num)
			len = IWL_MAX_CMD_SIZE;

		txq->cmd[i] = kmalloc(len, GFP_KERNEL);
		if (!txq->cmd[i])
			goto err;
	}

	/* Alloc driver data array and TFD circular buffer */
	ret = iwl_tx_queue_alloc(priv, txq, txq_id);
	if (ret)
		goto err;

	txq->need_update = 0;

	/*
	 * Aggregation TX queues will get their ID when aggregation begins;
	 * they overwrite the setting done here. The command FIFO doesn't
	 * need an swq_id so don't set one to catch errors, all others can
	 * be set up to the identity mapping.
	 */
	if (txq_id != IWL_CMD_QUEUE_NUM)
		txq->swq_id = txq_id;

	/* TFD_QUEUE_SIZE_MAX must be power-of-two size, otherwise
	 * iwl_queue_inc_wrap and iwl_queue_dec_wrap are broken. */
	BUILD_BUG_ON(TFD_QUEUE_SIZE_MAX & (TFD_QUEUE_SIZE_MAX - 1));

	/* Initialize queue's high/low-water marks, and head/tail indexes */
	iwl_queue_init(priv, &txq->q, TFD_QUEUE_SIZE_MAX, slots_num, txq_id);

	/* Tell device where to find queue */
	priv->cfg->ops->lib->txq_init(priv, txq);

	return 0;
err:
	for (i = 0; i < actual_slots; i++)
		kfree(txq->cmd[i]);
out_free_arrays:
	kfree(txq->meta);
	kfree(txq->cmd);

	return -ENOMEM;
}
EXPORT_SYMBOL(iwl_tx_queue_init);

void iwl_tx_queue_reset(struct iwl_priv *priv, struct iwl_tx_queue *txq,
			int slots_num, u32 txq_id)
{
	int actual_slots = slots_num;

	if (txq_id == IWL_CMD_QUEUE_NUM)
		actual_slots++;

	memset(txq->meta, 0, sizeof(struct iwl_cmd_meta) * actual_slots);

	txq->need_update = 0;

	/* Initialize queue's high/low-water marks, and head/tail indexes */
	iwl_queue_init(priv, &txq->q, TFD_QUEUE_SIZE_MAX, slots_num, txq_id);

	/* Tell device where to find queue */
	priv->cfg->ops->lib->txq_init(priv, txq);
}
EXPORT_SYMBOL(iwl_tx_queue_reset);

/*************** HOST COMMAND QUEUE FUNCTIONS   *****/

/**
 * iwl_enqueue_hcmd - enqueue a uCode command
 * @priv: device private data point
 * @cmd: a point to the ucode command structure
 *
 * The function returns < 0 values to indicate the operation is
 * failed. On success, it turns the index (> 0) of command in the
 * command queue.
 */
int iwl_enqueue_hcmd(struct iwl_priv *priv, struct iwl_host_cmd *cmd)
{
	struct iwl_tx_queue *txq = &priv->txq[IWL_CMD_QUEUE_NUM];
	struct iwl_queue *q = &txq->q;
	struct iwl_device_cmd *out_cmd;
	struct iwl_cmd_meta *out_meta;
	dma_addr_t phys_addr;
	unsigned long flags;
	int len;
	u32 idx;
	u16 fix_size;

	cmd->len = priv->cfg->ops->utils->get_hcmd_size(cmd->id, cmd->len);
	fix_size = (u16)(cmd->len + sizeof(out_cmd->hdr));

	/* If any of the command structures end up being larger than
	 * the TFD_MAX_PAYLOAD_SIZE, and it sent as a 'small' command then
	 * we will need to increase the size of the TFD entries
	 * Also, check to see if command buffer should not exceed the size
	 * of device_cmd and max_cmd_size. */
	BUG_ON((fix_size > TFD_MAX_PAYLOAD_SIZE) &&
	       !(cmd->flags & CMD_SIZE_HUGE));
	BUG_ON(fix_size > IWL_MAX_CMD_SIZE);

	if (iwl_is_rfkill(priv) || iwl_is_ctkill(priv)) {
		IWL_WARN(priv, "Not sending command - %s KILL\n",
			 iwl_is_rfkill(priv) ? "RF" : "CT");
		return -EIO;
	}

	if (iwl_queue_space(q) < ((cmd->flags & CMD_ASYNC) ? 2 : 1)) {
		IWL_ERR(priv, "No space in command queue\n");
		if (iwl_within_ct_kill_margin(priv))
			iwl_tt_enter_ct_kill(priv);
		else {
			IWL_ERR(priv, "Restarting adapter due to queue full\n");
			queue_work(priv->workqueue, &priv->restart);
		}
		return -ENOSPC;
	}

	spin_lock_irqsave(&priv->hcmd_lock, flags);

	/* If this is a huge cmd, mark the huge flag also on the meta.flags
	 * of the _original_ cmd. This is used for DMA mapping clean up.
	 */
	if (cmd->flags & CMD_SIZE_HUGE) {
		idx = get_cmd_index(q, q->write_ptr, 0);
		txq->meta[idx].flags = CMD_SIZE_HUGE;
	}

	idx = get_cmd_index(q, q->write_ptr, cmd->flags & CMD_SIZE_HUGE);
	out_cmd = txq->cmd[idx];
	out_meta = &txq->meta[idx];

	memset(out_meta, 0, sizeof(*out_meta));	/* re-initialize to NULL */
	out_meta->flags = cmd->flags;
	if (cmd->flags & CMD_WANT_SKB)
		out_meta->source = cmd;
	if (cmd->flags & CMD_ASYNC)
		out_meta->callback = cmd->callback;

	out_cmd->hdr.cmd = cmd->id;
	memcpy(&out_cmd->cmd.payload, cmd->data, cmd->len);

	/* At this point, the out_cmd now has all of the incoming cmd
	 * information */

	out_cmd->hdr.flags = 0;
	out_cmd->hdr.sequence = cpu_to_le16(QUEUE_TO_SEQ(IWL_CMD_QUEUE_NUM) |
			INDEX_TO_SEQ(q->write_ptr));
	if (cmd->flags & CMD_SIZE_HUGE)
		out_cmd->hdr.sequence |= SEQ_HUGE_FRAME;
	len = sizeof(struct iwl_device_cmd);
	if (idx == TFD_CMD_SLOTS)
		len = IWL_MAX_CMD_SIZE;

#ifdef CONFIG_IWLWIFI_DEBUG
	switch (out_cmd->hdr.cmd) {
	case REPLY_TX_LINK_QUALITY_CMD:
	case SENSITIVITY_CMD:
		IWL_DEBUG_HC_DUMP(priv, "Sending command %s (#%x), seq: 0x%04X, "
				"%d bytes at %d[%d]:%d\n",
				get_cmd_string(out_cmd->hdr.cmd),
				out_cmd->hdr.cmd,
				le16_to_cpu(out_cmd->hdr.sequence), fix_size,
				q->write_ptr, idx, IWL_CMD_QUEUE_NUM);
				break;
	default:
		IWL_DEBUG_HC(priv, "Sending command %s (#%x), seq: 0x%04X, "
				"%d bytes at %d[%d]:%d\n",
				get_cmd_string(out_cmd->hdr.cmd),
				out_cmd->hdr.cmd,
				le16_to_cpu(out_cmd->hdr.sequence), fix_size,
				q->write_ptr, idx, IWL_CMD_QUEUE_NUM);
	}
#endif
	txq->need_update = 1;

	if (priv->cfg->ops->lib->txq_update_byte_cnt_tbl)
		/* Set up entry in queue's byte count circular buffer */
		priv->cfg->ops->lib->txq_update_byte_cnt_tbl(priv, txq, 0);

	phys_addr = pci_map_single(priv->pci_dev, &out_cmd->hdr,
				   fix_size, PCI_DMA_BIDIRECTIONAL);
	dma_unmap_addr_set(out_meta, mapping, phys_addr);
	dma_unmap_len_set(out_meta, len, fix_size);

	trace_iwlwifi_dev_hcmd(priv, &out_cmd->hdr, fix_size, cmd->flags);

	priv->cfg->ops->lib->txq_attach_buf_to_tfd(priv, txq,
						   phys_addr, fix_size, 1,
						   U32_PAD(cmd->len));

	/* Increment and update queue's write index */
	q->write_ptr = iwl_queue_inc_wrap(q->write_ptr, q->n_bd);
	iwl_txq_update_write_ptr(priv, txq);

	spin_unlock_irqrestore(&priv->hcmd_lock, flags);
	return idx;
}

/**
 * iwl_hcmd_queue_reclaim - Reclaim TX command queue entries already Tx'd
 *
 * When FW advances 'R' index, all entries between old and new 'R' index
 * need to be reclaimed. As result, some free space forms.  If there is
 * enough free space (> low mark), wake the stack that feeds us.
 */
static void iwl_hcmd_queue_reclaim(struct iwl_priv *priv, int txq_id,
				   int idx, int cmd_idx)
{
	struct iwl_tx_queue *txq = &priv->txq[txq_id];
	struct iwl_queue *q = &txq->q;
	int nfreed = 0;

	if ((idx >= q->n_bd) || (iwl_queue_used(q, idx) == 0)) {
		IWL_ERR(priv, "Read index for DMA queue txq id (%d), index %d, "
			  "is out of range [0-%d] %d %d.\n", txq_id,
			  idx, q->n_bd, q->write_ptr, q->read_ptr);
		return;
	}

	for (idx = iwl_queue_inc_wrap(idx, q->n_bd); q->read_ptr != idx;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd)) {

		if (nfreed++ > 0) {
			IWL_ERR(priv, "HCMD skipped: index (%d) %d %d\n", idx,
					q->write_ptr, q->read_ptr);
			queue_work(priv->workqueue, &priv->restart);
		}

	}
}

/**
 * iwl_tx_cmd_complete - Pull unused buffers off the queue and reclaim them
 * @rxb: Rx buffer to reclaim
 *
 * If an Rx buffer has an async callback associated with it the callback
 * will be executed.  The attached skb (if present) will only be freed
 * if the callback returns 1
 */
void iwl_tx_cmd_complete(struct iwl_priv *priv, struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	u16 sequence = le16_to_cpu(pkt->hdr.sequence);
	int txq_id = SEQ_TO_QUEUE(sequence);
	int index = SEQ_TO_INDEX(sequence);
	int cmd_index;
	bool huge = !!(pkt->hdr.sequence & SEQ_HUGE_FRAME);
	struct iwl_device_cmd *cmd;
	struct iwl_cmd_meta *meta;
	struct iwl_tx_queue *txq = &priv->txq[IWL_CMD_QUEUE_NUM];

	/* If a Tx command is being handled and it isn't in the actual
	 * command queue then there a command routing bug has been introduced
	 * in the queue management code. */
	if (WARN(txq_id != IWL_CMD_QUEUE_NUM,
		 "wrong command queue %d, sequence 0x%X readp=%d writep=%d\n",
		  txq_id, sequence,
		  priv->txq[IWL_CMD_QUEUE_NUM].q.read_ptr,
		  priv->txq[IWL_CMD_QUEUE_NUM].q.write_ptr)) {
		iwl_print_hex_error(priv, pkt, 32);
		return;
	}

	/* If this is a huge cmd, clear the huge flag on the meta.flags
	 * of the _original_ cmd. So that iwl_cmd_queue_free won't unmap
	 * the DMA buffer for the scan (huge) command.
	 */
	if (huge) {
		cmd_index = get_cmd_index(&txq->q, index, 0);
		txq->meta[cmd_index].flags = 0;
	}
	cmd_index = get_cmd_index(&txq->q, index, huge);
	cmd = txq->cmd[cmd_index];
	meta = &txq->meta[cmd_index];

	pci_unmap_single(priv->pci_dev,
			 dma_unmap_addr(meta, mapping),
			 dma_unmap_len(meta, len),
			 PCI_DMA_BIDIRECTIONAL);

	/* Input error checking is done when commands are added to queue. */
	if (meta->flags & CMD_WANT_SKB) {
		meta->source->reply_page = (unsigned long)rxb_addr(rxb);
		rxb->page = NULL;
	} else if (meta->callback)
		meta->callback(priv, cmd, pkt);

	iwl_hcmd_queue_reclaim(priv, txq_id, index, cmd_index);

	if (!(meta->flags & CMD_ASYNC)) {
		clear_bit(STATUS_HCMD_ACTIVE, &priv->status);
		IWL_DEBUG_INFO(priv, "Clearing HCMD_ACTIVE for command %s\n",
			       get_cmd_string(cmd->hdr.cmd));
		wake_up_interruptible(&priv->wait_command_queue);
	}
	meta->flags = 0;
}
EXPORT_SYMBOL(iwl_tx_cmd_complete);

#ifdef CONFIG_IWLWIFI_DEBUG
#define TX_STATUS_FAIL(x) case TX_STATUS_FAIL_ ## x: return #x
#define TX_STATUS_POSTPONE(x) case TX_STATUS_POSTPONE_ ## x: return #x

const char *iwl_get_tx_fail_reason(u32 status)
{
	switch (status & TX_STATUS_MSK) {
	case TX_STATUS_SUCCESS:
		return "SUCCESS";
		TX_STATUS_POSTPONE(DELAY);
		TX_STATUS_POSTPONE(FEW_BYTES);
		TX_STATUS_POSTPONE(BT_PRIO);
		TX_STATUS_POSTPONE(QUIET_PERIOD);
		TX_STATUS_POSTPONE(CALC_TTAK);
		TX_STATUS_FAIL(INTERNAL_CROSSED_RETRY);
		TX_STATUS_FAIL(SHORT_LIMIT);
		TX_STATUS_FAIL(LONG_LIMIT);
		TX_STATUS_FAIL(FIFO_UNDERRUN);
		TX_STATUS_FAIL(DRAIN_FLOW);
		TX_STATUS_FAIL(RFKILL_FLUSH);
		TX_STATUS_FAIL(LIFE_EXPIRE);
		TX_STATUS_FAIL(DEST_PS);
		TX_STATUS_FAIL(HOST_ABORTED);
		TX_STATUS_FAIL(BT_RETRY);
		TX_STATUS_FAIL(STA_INVALID);
		TX_STATUS_FAIL(FRAG_DROPPED);
		TX_STATUS_FAIL(TID_DISABLE);
		TX_STATUS_FAIL(FIFO_FLUSHED);
		TX_STATUS_FAIL(INSUFFICIENT_CF_POLL);
		TX_STATUS_FAIL(FW_DROP);
		TX_STATUS_FAIL(STA_COLOR_MISMATCH_DROP);
	}

	return "UNKNOWN";
}
EXPORT_SYMBOL(iwl_get_tx_fail_reason);
#endif /* CONFIG_IWLWIFI_DEBUG */
