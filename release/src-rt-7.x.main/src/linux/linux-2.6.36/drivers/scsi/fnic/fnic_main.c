/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <linux/module.h>
#include <linux/mempool.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/if_ether.h>
#include <scsi/fc/fc_fip.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_tcq.h>
#include <scsi/libfc.h>
#include <scsi/fc_frame.h>

#include "vnic_dev.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "fnic_io.h"
#include "fnic.h"

#define PCI_DEVICE_ID_CISCO_FNIC	0x0045

/* Timer to poll notification area for events. Used for MSI interrupts */
#define FNIC_NOTIFY_TIMER_PERIOD	(2 * HZ)

static struct kmem_cache *fnic_sgl_cache[FNIC_SGL_NUM_CACHES];
static struct kmem_cache *fnic_io_req_cache;
LIST_HEAD(fnic_list);
DEFINE_SPINLOCK(fnic_list_lock);

/* Supported devices by fnic module */
static struct pci_device_id fnic_id_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_CISCO, PCI_DEVICE_ID_CISCO_FNIC) },
	{ 0, }
};

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR("Abhijeet Joglekar <abjoglek@cisco.com>, "
	      "Joseph R. Eykholt <jeykholt@cisco.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
MODULE_DEVICE_TABLE(pci, fnic_id_table);

unsigned int fnic_log_level;
module_param(fnic_log_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(fnic_log_level, "bit mask of fnic logging levels");


static struct libfc_function_template fnic_transport_template = {
	.frame_send = fnic_send,
	.lport_set_port_id = fnic_set_port_id,
	.fcp_abort_io = fnic_empty_scsi_cleanup,
	.fcp_cleanup = fnic_empty_scsi_cleanup,
	.exch_mgr_reset = fnic_exch_mgr_reset
};

static int fnic_slave_alloc(struct scsi_device *sdev)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));
	struct fc_lport *lp = shost_priv(sdev->host);
	struct fnic *fnic = lport_priv(lp);

	sdev->tagged_supported = 1;

	if (!rport || fc_remote_port_chkready(rport))
		return -ENXIO;

	scsi_activate_tcq(sdev, FNIC_DFLT_QUEUE_DEPTH);
	rport->dev_loss_tmo = fnic->config.port_down_timeout / 1000;

	return 0;
}

static struct scsi_host_template fnic_host_template = {
	.module = THIS_MODULE,
	.name = DRV_NAME,
	.queuecommand = fnic_queuecommand,
	.eh_abort_handler = fnic_abort_cmd,
	.eh_device_reset_handler = fnic_device_reset,
	.eh_host_reset_handler = fnic_host_reset,
	.slave_alloc = fnic_slave_alloc,
	.change_queue_depth = fc_change_queue_depth,
	.change_queue_type = fc_change_queue_type,
	.this_id = -1,
	.cmd_per_lun = 3,
	.can_queue = FNIC_MAX_IO_REQ,
	.use_clustering = ENABLE_CLUSTERING,
	.sg_tablesize = FNIC_MAX_SG_DESC_CNT,
	.max_sectors = 0xffff,
	.shost_attrs = fnic_attrs,
};

static void fnic_get_host_speed(struct Scsi_Host *shost);
static struct scsi_transport_template *fnic_fc_transport;
static struct fc_host_statistics *fnic_get_stats(struct Scsi_Host *);

static struct fc_function_template fnic_fc_functions = {

	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,
	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fnic_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,
	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.show_rport_dev_loss_tmo = 1,
	.issue_fc_host_lip = fnic_reset,
	.get_fc_host_stats = fnic_get_stats,
	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.terminate_rport_io = fnic_terminate_rport_io,
	.bsg_request = fc_lport_bsg_request,
};

static void fnic_get_host_speed(struct Scsi_Host *shost)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);
	u32 port_speed = vnic_dev_port_speed(fnic->vdev);

	/* Add in other values as they get defined in fw */
	switch (port_speed) {
	case 10000:
		fc_host_speed(shost) = FC_PORTSPEED_10GBIT;
		break;
	default:
		fc_host_speed(shost) = FC_PORTSPEED_10GBIT;
		break;
	}
}

static struct fc_host_statistics *fnic_get_stats(struct Scsi_Host *host)
{
	int ret;
	struct fc_lport *lp = shost_priv(host);
	struct fnic *fnic = lport_priv(lp);
	struct fc_host_statistics *stats = &lp->host_stats;
	struct vnic_stats *vs;
	unsigned long flags;

	if (time_before(jiffies, fnic->stats_time + HZ / FNIC_STATS_RATE_LIMIT))
		return stats;
	fnic->stats_time = jiffies;

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	ret = vnic_dev_stats_dump(fnic->vdev, &fnic->stats);
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	if (ret) {
		FNIC_MAIN_DBG(KERN_DEBUG, fnic->lport->host,
			      "fnic: Get vnic stats failed"
			      " 0x%x", ret);
		return stats;
	}
	vs = fnic->stats;
	stats->tx_frames = vs->tx.tx_unicast_frames_ok;
	stats->tx_words  = vs->tx.tx_unicast_bytes_ok / 4;
	stats->rx_frames = vs->rx.rx_unicast_frames_ok;
	stats->rx_words  = vs->rx.rx_unicast_bytes_ok / 4;
	stats->error_frames = vs->tx.tx_errors + vs->rx.rx_errors;
	stats->dumped_frames = vs->tx.tx_drops + vs->rx.rx_drop;
	stats->invalid_crc_count = vs->rx.rx_crc_errors;
	stats->seconds_since_last_reset = (jiffies - lp->boot_time) / HZ;
	stats->fcp_input_megabytes = div_u64(fnic->fcp_input_bytes, 1000000);
	stats->fcp_output_megabytes = div_u64(fnic->fcp_output_bytes, 1000000);

	return stats;
}

void fnic_log_q_error(struct fnic *fnic)
{
	unsigned int i;
	u32 error_status;

	for (i = 0; i < fnic->raw_wq_count; i++) {
		error_status = ioread32(&fnic->wq[i].ctrl->error_status);
		if (error_status)
			shost_printk(KERN_ERR, fnic->lport->host,
				     "WQ[%d] error_status"
				     " %d\n", i, error_status);
	}

	for (i = 0; i < fnic->rq_count; i++) {
		error_status = ioread32(&fnic->rq[i].ctrl->error_status);
		if (error_status)
			shost_printk(KERN_ERR, fnic->lport->host,
				     "RQ[%d] error_status"
				     " %d\n", i, error_status);
	}

	for (i = 0; i < fnic->wq_copy_count; i++) {
		error_status = ioread32(&fnic->wq_copy[i].ctrl->error_status);
		if (error_status)
			shost_printk(KERN_ERR, fnic->lport->host,
				     "CWQ[%d] error_status"
				     " %d\n", i, error_status);
	}
}

void fnic_handle_link_event(struct fnic *fnic)
{
	unsigned long flags;

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	if (fnic->stop_rx_link_events) {
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	queue_work(fnic_event_queue, &fnic->link_work);

}

static int fnic_notify_set(struct fnic *fnic)
{
	int err;

	switch (vnic_dev_get_intr_mode(fnic->vdev)) {
	case VNIC_DEV_INTR_MODE_INTX:
		err = vnic_dev_notify_set(fnic->vdev, FNIC_INTX_NOTIFY);
		break;
	case VNIC_DEV_INTR_MODE_MSI:
		err = vnic_dev_notify_set(fnic->vdev, -1);
		break;
	case VNIC_DEV_INTR_MODE_MSIX:
		err = vnic_dev_notify_set(fnic->vdev, FNIC_MSIX_ERR_NOTIFY);
		break;
	default:
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Interrupt mode should be set up"
			     " before devcmd notify set %d\n",
			     vnic_dev_get_intr_mode(fnic->vdev));
		err = -1;
		break;
	}

	return err;
}

static void fnic_notify_timer(unsigned long data)
{
	struct fnic *fnic = (struct fnic *)data;

	fnic_handle_link_event(fnic);
	mod_timer(&fnic->notify_timer,
		  round_jiffies(jiffies + FNIC_NOTIFY_TIMER_PERIOD));
}

static void fnic_notify_timer_start(struct fnic *fnic)
{
	switch (vnic_dev_get_intr_mode(fnic->vdev)) {
	case VNIC_DEV_INTR_MODE_MSI:
		/*
		 * Schedule first timeout immediately. The driver is
		 * initiatialized and ready to look for link up notification
		 */
		mod_timer(&fnic->notify_timer, jiffies);
		break;
	default:
		/* Using intr for notification for INTx/MSI-X */
		break;
	};
}

static int fnic_dev_wait(struct vnic_dev *vdev,
			 int (*start)(struct vnic_dev *, int),
			 int (*finished)(struct vnic_dev *, int *),
			 int arg)
{
	unsigned long time;
	int done;
	int err;

	err = start(vdev, arg);
	if (err)
		return err;

	/* Wait for func to complete...2 seconds max */
	time = jiffies + (HZ * 2);
	do {
		err = finished(vdev, &done);
		if (err)
			return err;
		if (done)
			return 0;
		schedule_timeout_uninterruptible(HZ / 10);
	} while (time_after(time, jiffies));

	return -ETIMEDOUT;
}

static int fnic_cleanup(struct fnic *fnic)
{
	unsigned int i;
	int err;

	vnic_dev_disable(fnic->vdev);
	for (i = 0; i < fnic->intr_count; i++)
		vnic_intr_mask(&fnic->intr[i]);

	for (i = 0; i < fnic->rq_count; i++) {
		err = vnic_rq_disable(&fnic->rq[i]);
		if (err)
			return err;
	}
	for (i = 0; i < fnic->raw_wq_count; i++) {
		err = vnic_wq_disable(&fnic->wq[i]);
		if (err)
			return err;
	}
	for (i = 0; i < fnic->wq_copy_count; i++) {
		err = vnic_wq_copy_disable(&fnic->wq_copy[i]);
		if (err)
			return err;
	}

	/* Clean up completed IOs and FCS frames */
	fnic_wq_copy_cmpl_handler(fnic, -1);
	fnic_wq_cmpl_handler(fnic, -1);
	fnic_rq_cmpl_handler(fnic, -1);

	/* Clean up the IOs and FCS frames that have not completed */
	for (i = 0; i < fnic->raw_wq_count; i++)
		vnic_wq_clean(&fnic->wq[i], fnic_free_wq_buf);
	for (i = 0; i < fnic->rq_count; i++)
		vnic_rq_clean(&fnic->rq[i], fnic_free_rq_buf);
	for (i = 0; i < fnic->wq_copy_count; i++)
		vnic_wq_copy_clean(&fnic->wq_copy[i],
				   fnic_wq_copy_cleanup_handler);

	for (i = 0; i < fnic->cq_count; i++)
		vnic_cq_clean(&fnic->cq[i]);
	for (i = 0; i < fnic->intr_count; i++)
		vnic_intr_clean(&fnic->intr[i]);

	mempool_destroy(fnic->io_req_pool);
	for (i = 0; i < FNIC_SGL_NUM_CACHES; i++)
		mempool_destroy(fnic->io_sgl_pool[i]);

	return 0;
}

static void fnic_iounmap(struct fnic *fnic)
{
	if (fnic->bar0.vaddr)
		iounmap(fnic->bar0.vaddr);
}

/*
 * Allocate element for mempools requiring GFP_DMA flag.
 * Otherwise, checks in kmem_flagcheck() hit BUG_ON().
 */
static void *fnic_alloc_slab_dma(gfp_t gfp_mask, void *pool_data)
{
	struct kmem_cache *mem = pool_data;

	return kmem_cache_alloc(mem, gfp_mask | GFP_ATOMIC | GFP_DMA);
}

/**
 * fnic_get_mac() - get assigned data MAC address for FIP code.
 * @lport: 	local port.
 */
static u8 *fnic_get_mac(struct fc_lport *lport)
{
	struct fnic *fnic = lport_priv(lport);

	return fnic->data_src_addr;
}

static int __devinit fnic_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct Scsi_Host *host;
	struct fc_lport *lp;
	struct fnic *fnic;
	mempool_t *pool;
	int err;
	int i;
	unsigned long flags;

	/*
	 * Allocate SCSI Host and set up association between host,
	 * local port, and fnic
	 */
	lp = libfc_host_alloc(&fnic_host_template, sizeof(struct fnic));
	if (!lp) {
		printk(KERN_ERR PFX "Unable to alloc libfc local port\n");
		err = -ENOMEM;
		goto err_out;
	}
	host = lp->host;
	fnic = lport_priv(lp);
	fnic->lport = lp;
	fnic->ctlr.lp = lp;

	snprintf(fnic->name, sizeof(fnic->name) - 1, "%s%d", DRV_NAME,
		 host->host_no);

	host->transportt = fnic_fc_transport;

	err = scsi_init_shared_tag_map(host, FNIC_MAX_IO_REQ);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Unable to alloc shared tag map\n");
		goto err_out_free_hba;
	}

	/* Setup PCI resources */
	pci_set_drvdata(pdev, fnic);

	fnic->pdev = pdev;

	err = pci_enable_device(pdev);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Cannot enable PCI device, aborting.\n");
		goto err_out_free_hba;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Cannot enable PCI resources, aborting\n");
		goto err_out_disable_device;
	}

	pci_set_master(pdev);

	/* Query PCI controller on system for DMA addressing
	 * limitation for the device.  Try 40-bit first, and
	 * fail to 32-bit.
	 */
	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(40));
	if (err) {
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			shost_printk(KERN_ERR, fnic->lport->host,
				     "No usable DMA configuration "
				     "aborting\n");
			goto err_out_release_regions;
		}
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			shost_printk(KERN_ERR, fnic->lport->host,
				     "Unable to obtain 32-bit DMA "
				     "for consistent allocations, aborting.\n");
			goto err_out_release_regions;
		}
	} else {
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(40));
		if (err) {
			shost_printk(KERN_ERR, fnic->lport->host,
				     "Unable to obtain 40-bit DMA "
				     "for consistent allocations, aborting.\n");
			goto err_out_release_regions;
		}
	}

	/* Map vNIC resources from BAR0 */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "BAR0 not memory-map'able, aborting.\n");
		err = -ENODEV;
		goto err_out_release_regions;
	}

	fnic->bar0.vaddr = pci_iomap(pdev, 0, 0);
	fnic->bar0.bus_addr = pci_resource_start(pdev, 0);
	fnic->bar0.len = pci_resource_len(pdev, 0);

	if (!fnic->bar0.vaddr) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Cannot memory-map BAR0 res hdr, "
			     "aborting.\n");
		err = -ENODEV;
		goto err_out_release_regions;
	}

	fnic->vdev = vnic_dev_register(NULL, fnic, pdev, &fnic->bar0);
	if (!fnic->vdev) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "vNIC registration failed, "
			     "aborting.\n");
		err = -ENODEV;
		goto err_out_iounmap;
	}

	err = fnic_dev_wait(fnic->vdev, vnic_dev_open,
			    vnic_dev_open_done, 0);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "vNIC dev open failed, aborting.\n");
		goto err_out_vnic_unregister;
	}

	err = vnic_dev_init(fnic->vdev, 0);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "vNIC dev init failed, aborting.\n");
		goto err_out_dev_close;
	}

	err = vnic_dev_mac_addr(fnic->vdev, fnic->ctlr.ctl_src_addr);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "vNIC get MAC addr failed \n");
		goto err_out_dev_close;
	}
	/* set data_src for point-to-point mode and to keep it non-zero */
	memcpy(fnic->data_src_addr, fnic->ctlr.ctl_src_addr, ETH_ALEN);

	/* Get vNIC configuration */
	err = fnic_get_vnic_config(fnic);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Get vNIC configuration failed, "
			     "aborting.\n");
		goto err_out_dev_close;
	}
	host->max_lun = fnic->config.luns_per_tgt;
	host->max_id = FNIC_MAX_FCP_TARGET;
	host->max_cmd_len = FCOE_MAX_CMD_LEN;

	fnic_get_res_counts(fnic);

	err = fnic_set_intr_mode(fnic);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Failed to set intr mode, "
			     "aborting.\n");
		goto err_out_dev_close;
	}

	err = fnic_alloc_vnic_resources(fnic);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Failed to alloc vNIC resources, "
			     "aborting.\n");
		goto err_out_clear_intr;
	}


	/* initialize all fnic locks */
	spin_lock_init(&fnic->fnic_lock);

	for (i = 0; i < FNIC_WQ_MAX; i++)
		spin_lock_init(&fnic->wq_lock[i]);

	for (i = 0; i < FNIC_WQ_COPY_MAX; i++) {
		spin_lock_init(&fnic->wq_copy_lock[i]);
		fnic->wq_copy_desc_low[i] = DESC_CLEAN_LOW_WATERMARK;
		fnic->fw_ack_recd[i] = 0;
		fnic->fw_ack_index[i] = -1;
	}

	for (i = 0; i < FNIC_IO_LOCKS; i++)
		spin_lock_init(&fnic->io_req_lock[i]);

	fnic->io_req_pool = mempool_create_slab_pool(2, fnic_io_req_cache);
	if (!fnic->io_req_pool)
		goto err_out_free_resources;

	pool = mempool_create(2, fnic_alloc_slab_dma, mempool_free_slab,
			      fnic_sgl_cache[FNIC_SGL_CACHE_DFLT]);
	if (!pool)
		goto err_out_free_ioreq_pool;
	fnic->io_sgl_pool[FNIC_SGL_CACHE_DFLT] = pool;

	pool = mempool_create(2, fnic_alloc_slab_dma, mempool_free_slab,
			      fnic_sgl_cache[FNIC_SGL_CACHE_MAX]);
	if (!pool)
		goto err_out_free_dflt_pool;
	fnic->io_sgl_pool[FNIC_SGL_CACHE_MAX] = pool;

	/* setup vlan config, hw inserts vlan header */
	fnic->vlan_hw_insert = 1;
	fnic->vlan_id = 0;

	/* Initialize the FIP fcoe_ctrl struct */
	fnic->ctlr.send = fnic_eth_send;
	fnic->ctlr.update_mac = fnic_update_mac;
	fnic->ctlr.get_src_addr = fnic_get_mac;
	if (fnic->config.flags & VFCF_FIP_CAPABLE) {
		shost_printk(KERN_INFO, fnic->lport->host,
			     "firmware supports FIP\n");
		/* enable directed and multicast */
		vnic_dev_packet_filter(fnic->vdev, 1, 1, 0, 0, 0);
		vnic_dev_add_addr(fnic->vdev, FIP_ALL_ENODE_MACS);
		vnic_dev_add_addr(fnic->vdev, fnic->ctlr.ctl_src_addr);
		fcoe_ctlr_init(&fnic->ctlr, FIP_MODE_AUTO);
	} else {
		shost_printk(KERN_INFO, fnic->lport->host,
			     "firmware uses non-FIP mode\n");
		fcoe_ctlr_init(&fnic->ctlr, FIP_MODE_NON_FIP);
	}
	fnic->state = FNIC_IN_FC_MODE;

	/* Enable hardware stripping of vlan header on ingress */
	fnic_set_nic_config(fnic, 0, 0, 0, 0, 0, 0, 1);

	/* Setup notification buffer area */
	err = fnic_notify_set(fnic);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Failed to alloc notify buffer, aborting.\n");
		goto err_out_free_max_pool;
	}

	/* Setup notify timer when using MSI interrupts */
	if (vnic_dev_get_intr_mode(fnic->vdev) == VNIC_DEV_INTR_MODE_MSI)
		setup_timer(&fnic->notify_timer,
			    fnic_notify_timer, (unsigned long)fnic);

	/* allocate RQ buffers and post them to RQ*/
	for (i = 0; i < fnic->rq_count; i++) {
		err = vnic_rq_fill(&fnic->rq[i], fnic_alloc_rq_frame);
		if (err) {
			shost_printk(KERN_ERR, fnic->lport->host,
				     "fnic_alloc_rq_frame can't alloc "
				     "frame\n");
			goto err_out_free_rq_buf;
		}
	}

	/*
	 * Initialization done with PCI system, hardware, firmware.
	 * Add host to SCSI
	 */
	err = scsi_add_host(lp->host, &pdev->dev);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "fnic: scsi_add_host failed...exiting\n");
		goto err_out_free_rq_buf;
	}

	/* Start local port initiatialization */

	lp->link_up = 0;

	lp->max_retry_count = fnic->config.flogi_retries;
	lp->max_rport_retry_count = fnic->config.plogi_retries;
	lp->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS |
			      FCP_SPPF_CONF_COMPL);
	if (fnic->config.flags & VFCF_FCP_SEQ_LVL_ERR)
		lp->service_params |= FCP_SPPF_RETRY;

	lp->boot_time = jiffies;
	lp->e_d_tov = fnic->config.ed_tov;
	lp->r_a_tov = fnic->config.ra_tov;
	lp->link_supported_speeds = FC_PORTSPEED_10GBIT;
	fc_set_wwnn(lp, fnic->config.node_wwn);
	fc_set_wwpn(lp, fnic->config.port_wwn);

	fcoe_libfc_config(lp, &fnic->ctlr, &fnic_transport_template, 0);

	if (!fc_exch_mgr_alloc(lp, FC_CLASS_3, FCPIO_HOST_EXCH_RANGE_START,
			       FCPIO_HOST_EXCH_RANGE_END, NULL)) {
		err = -ENOMEM;
		goto err_out_remove_scsi_host;
	}

	fc_lport_init_stats(lp);

	fc_lport_config(lp);

	if (fc_set_mfs(lp, fnic->config.maxdatafieldsize +
		       sizeof(struct fc_frame_header))) {
		err = -EINVAL;
		goto err_out_free_exch_mgr;
	}
	fc_host_maxframe_size(lp->host) = lp->mfs;

	sprintf(fc_host_symbolic_name(lp->host),
		DRV_NAME " v" DRV_VERSION " over %s", fnic->name);

	spin_lock_irqsave(&fnic_list_lock, flags);
	list_add_tail(&fnic->list, &fnic_list);
	spin_unlock_irqrestore(&fnic_list_lock, flags);

	INIT_WORK(&fnic->link_work, fnic_handle_link);
	INIT_WORK(&fnic->frame_work, fnic_handle_frame);
	skb_queue_head_init(&fnic->frame_queue);
	skb_queue_head_init(&fnic->tx_queue);

	/* Enable all queues */
	for (i = 0; i < fnic->raw_wq_count; i++)
		vnic_wq_enable(&fnic->wq[i]);
	for (i = 0; i < fnic->rq_count; i++)
		vnic_rq_enable(&fnic->rq[i]);
	for (i = 0; i < fnic->wq_copy_count; i++)
		vnic_wq_copy_enable(&fnic->wq_copy[i]);

	fc_fabric_login(lp);

	vnic_dev_enable(fnic->vdev);

	err = fnic_request_intr(fnic);
	if (err) {
		shost_printk(KERN_ERR, fnic->lport->host,
			     "Unable to request irq.\n");
		goto err_out_free_exch_mgr;
	}

	for (i = 0; i < fnic->intr_count; i++)
		vnic_intr_unmask(&fnic->intr[i]);

	fnic_notify_timer_start(fnic);

	return 0;

err_out_free_exch_mgr:
	fc_exch_mgr_free(lp);
err_out_remove_scsi_host:
	fc_remove_host(lp->host);
	scsi_remove_host(lp->host);
err_out_free_rq_buf:
	for (i = 0; i < fnic->rq_count; i++)
		vnic_rq_clean(&fnic->rq[i], fnic_free_rq_buf);
	vnic_dev_notify_unset(fnic->vdev);
err_out_free_max_pool:
	mempool_destroy(fnic->io_sgl_pool[FNIC_SGL_CACHE_MAX]);
err_out_free_dflt_pool:
	mempool_destroy(fnic->io_sgl_pool[FNIC_SGL_CACHE_DFLT]);
err_out_free_ioreq_pool:
	mempool_destroy(fnic->io_req_pool);
err_out_free_resources:
	fnic_free_vnic_resources(fnic);
err_out_clear_intr:
	fnic_clear_intr_mode(fnic);
err_out_dev_close:
	vnic_dev_close(fnic->vdev);
err_out_vnic_unregister:
	vnic_dev_unregister(fnic->vdev);
err_out_iounmap:
	fnic_iounmap(fnic);
err_out_release_regions:
	pci_release_regions(pdev);
err_out_disable_device:
	pci_disable_device(pdev);
err_out_free_hba:
	scsi_host_put(lp->host);
err_out:
	return err;
}

static void __devexit fnic_remove(struct pci_dev *pdev)
{
	struct fnic *fnic = pci_get_drvdata(pdev);
	struct fc_lport *lp = fnic->lport;
	unsigned long flags;

	/*
	 * Mark state so that the workqueue thread stops forwarding
	 * received frames and link events to the local port. ISR and
	 * other threads that can queue work items will also stop
	 * creating work items on the fnic workqueue
	 */
	spin_lock_irqsave(&fnic->fnic_lock, flags);
	fnic->stop_rx_link_events = 1;
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	if (vnic_dev_get_intr_mode(fnic->vdev) == VNIC_DEV_INTR_MODE_MSI)
		del_timer_sync(&fnic->notify_timer);

	/*
	 * Flush the fnic event queue. After this call, there should
	 * be no event queued for this fnic device in the workqueue
	 */
	flush_workqueue(fnic_event_queue);
	skb_queue_purge(&fnic->frame_queue);
	skb_queue_purge(&fnic->tx_queue);

	/*
	 * Log off the fabric. This stops all remote ports, dns port,
	 * logs off the fabric. This flushes all rport, disc, lport work
	 * before returning
	 */
	fc_fabric_logoff(fnic->lport);

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	fnic->in_remove = 1;
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	fcoe_ctlr_destroy(&fnic->ctlr);
	fc_lport_destroy(lp);

	/*
	 * This stops the fnic device, masks all interrupts. Completed
	 * CQ entries are drained. Posted WQ/RQ/Copy-WQ entries are
	 * cleaned up
	 */
	fnic_cleanup(fnic);

	BUG_ON(!skb_queue_empty(&fnic->frame_queue));
	BUG_ON(!skb_queue_empty(&fnic->tx_queue));

	spin_lock_irqsave(&fnic_list_lock, flags);
	list_del(&fnic->list);
	spin_unlock_irqrestore(&fnic_list_lock, flags);

	fc_remove_host(fnic->lport->host);
	scsi_remove_host(fnic->lport->host);
	fc_exch_mgr_free(fnic->lport);
	vnic_dev_notify_unset(fnic->vdev);
	fnic_free_intr(fnic);
	fnic_free_vnic_resources(fnic);
	fnic_clear_intr_mode(fnic);
	vnic_dev_close(fnic->vdev);
	vnic_dev_unregister(fnic->vdev);
	fnic_iounmap(fnic);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	scsi_host_put(lp->host);
}

static struct pci_driver fnic_driver = {
	.name = DRV_NAME,
	.id_table = fnic_id_table,
	.probe = fnic_probe,
	.remove = __devexit_p(fnic_remove),
};

static int __init fnic_init_module(void)
{
	size_t len;
	int err = 0;

	printk(KERN_INFO PFX "%s, ver %s\n", DRV_DESCRIPTION, DRV_VERSION);

	/* Create a cache for allocation of default size sgls */
	len = sizeof(struct fnic_dflt_sgl_list);
	fnic_sgl_cache[FNIC_SGL_CACHE_DFLT] = kmem_cache_create
		("fnic_sgl_dflt", len + FNIC_SG_DESC_ALIGN, FNIC_SG_DESC_ALIGN,
		 SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA,
		 NULL);
	if (!fnic_sgl_cache[FNIC_SGL_CACHE_DFLT]) {
		printk(KERN_ERR PFX "failed to create fnic dflt sgl slab\n");
		err = -ENOMEM;
		goto err_create_fnic_sgl_slab_dflt;
	}

	/* Create a cache for allocation of max size sgls*/
	len = sizeof(struct fnic_sgl_list);
	fnic_sgl_cache[FNIC_SGL_CACHE_MAX] = kmem_cache_create
		("fnic_sgl_max", len + FNIC_SG_DESC_ALIGN, FNIC_SG_DESC_ALIGN,
		 SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA,
		 NULL);
	if (!fnic_sgl_cache[FNIC_SGL_CACHE_MAX]) {
		printk(KERN_ERR PFX "failed to create fnic max sgl slab\n");
		err = -ENOMEM;
		goto err_create_fnic_sgl_slab_max;
	}

	/* Create a cache of io_req structs for use via mempool */
	fnic_io_req_cache = kmem_cache_create("fnic_io_req",
					      sizeof(struct fnic_io_req),
					      0, SLAB_HWCACHE_ALIGN, NULL);
	if (!fnic_io_req_cache) {
		printk(KERN_ERR PFX "failed to create fnic io_req slab\n");
		err = -ENOMEM;
		goto err_create_fnic_ioreq_slab;
	}

	fnic_event_queue = create_singlethread_workqueue("fnic_event_wq");
	if (!fnic_event_queue) {
		printk(KERN_ERR PFX "fnic work queue create failed\n");
		err = -ENOMEM;
		goto err_create_fnic_workq;
	}

	spin_lock_init(&fnic_list_lock);
	INIT_LIST_HEAD(&fnic_list);

	fnic_fc_transport = fc_attach_transport(&fnic_fc_functions);
	if (!fnic_fc_transport) {
		printk(KERN_ERR PFX "fc_attach_transport error\n");
		err = -ENOMEM;
		goto err_fc_transport;
	}

	/* register the driver with PCI system */
	err = pci_register_driver(&fnic_driver);
	if (err < 0) {
		printk(KERN_ERR PFX "pci register error\n");
		goto err_pci_register;
	}
	return err;

err_pci_register:
	fc_release_transport(fnic_fc_transport);
err_fc_transport:
	destroy_workqueue(fnic_event_queue);
err_create_fnic_workq:
	kmem_cache_destroy(fnic_io_req_cache);
err_create_fnic_ioreq_slab:
	kmem_cache_destroy(fnic_sgl_cache[FNIC_SGL_CACHE_MAX]);
err_create_fnic_sgl_slab_max:
	kmem_cache_destroy(fnic_sgl_cache[FNIC_SGL_CACHE_DFLT]);
err_create_fnic_sgl_slab_dflt:
	return err;
}

static void __exit fnic_cleanup_module(void)
{
	pci_unregister_driver(&fnic_driver);
	destroy_workqueue(fnic_event_queue);
	kmem_cache_destroy(fnic_sgl_cache[FNIC_SGL_CACHE_MAX]);
	kmem_cache_destroy(fnic_sgl_cache[FNIC_SGL_CACHE_DFLT]);
	kmem_cache_destroy(fnic_io_req_cache);
	fc_release_transport(fnic_fc_transport);
}

module_init(fnic_init_module);
module_exit(fnic_cleanup_module);
