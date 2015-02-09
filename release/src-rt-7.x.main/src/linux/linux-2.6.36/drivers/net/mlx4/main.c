/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006, 2007, 2008 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
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
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <linux/mlx4/device.h>
#include <linux/mlx4/doorbell.h>

#include "mlx4.h"
#include "fw.h"
#include "icm.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("Mellanox ConnectX HCA low-level driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

struct workqueue_struct *mlx4_wq;

#ifdef CONFIG_MLX4_DEBUG

int mlx4_debug_level = 0;
module_param_named(debug_level, mlx4_debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Enable debug tracing if > 0");

#endif /* CONFIG_MLX4_DEBUG */

#ifdef CONFIG_PCI_MSI

static int msi_x = 1;
module_param(msi_x, int, 0444);
MODULE_PARM_DESC(msi_x, "attempt to use MSI-X if nonzero");

#else /* CONFIG_PCI_MSI */

#define msi_x (0)

#endif /* CONFIG_PCI_MSI */

static char mlx4_version[] __devinitdata =
	DRV_NAME ": Mellanox ConnectX core driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

static struct mlx4_profile default_profile = {
	.num_qp		= 1 << 17,
	.num_srq	= 1 << 16,
	.rdmarc_per_qp	= 1 << 4,
	.num_cq		= 1 << 16,
	.num_mcg	= 1 << 13,
	.num_mpt	= 1 << 17,
	.num_mtt	= 1 << 20,
};

static int log_num_mac = 2;
module_param_named(log_num_mac, log_num_mac, int, 0444);
MODULE_PARM_DESC(log_num_mac, "Log2 max number of MACs per ETH port (1-7)");

static int log_num_vlan;
module_param_named(log_num_vlan, log_num_vlan, int, 0444);
MODULE_PARM_DESC(log_num_vlan, "Log2 max number of VLANs per ETH port (0-7)");

static int use_prio;
module_param_named(use_prio, use_prio, bool, 0444);
MODULE_PARM_DESC(use_prio, "Enable steering by VLAN priority on ETH ports "
		  "(0/1, default 0)");

static int log_mtts_per_seg = ilog2(MLX4_MTT_ENTRY_PER_SEG);
module_param_named(log_mtts_per_seg, log_mtts_per_seg, int, 0444);
MODULE_PARM_DESC(log_mtts_per_seg, "Log2 number of MTT entries per segment (1-5)");

int mlx4_check_port_params(struct mlx4_dev *dev,
			   enum mlx4_port_type *port_type)
{
	int i;

	for (i = 0; i < dev->caps.num_ports - 1; i++) {
		if (port_type[i] != port_type[i + 1]) {
			if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_DPDP)) {
				mlx4_err(dev, "Only same port types supported "
					 "on this HCA, aborting.\n");
				return -EINVAL;
			}
			if (port_type[i] == MLX4_PORT_TYPE_ETH &&
			    port_type[i + 1] == MLX4_PORT_TYPE_IB)
				return -EINVAL;
		}
	}

	for (i = 0; i < dev->caps.num_ports; i++) {
		if (!(port_type[i] & dev->caps.supported_type[i+1])) {
			mlx4_err(dev, "Requested port type for port %d is not "
				      "supported on this HCA\n", i + 1);
			return -EINVAL;
		}
	}
	return 0;
}

static void mlx4_set_port_mask(struct mlx4_dev *dev)
{
	int i;

	dev->caps.port_mask = 0;
	for (i = 1; i <= dev->caps.num_ports; ++i)
		if (dev->caps.port_type[i] == MLX4_PORT_TYPE_IB)
			dev->caps.port_mask |= 1 << (i - 1);
}
static int mlx4_dev_cap(struct mlx4_dev *dev, struct mlx4_dev_cap *dev_cap)
{
	int err;
	int i;

	err = mlx4_QUERY_DEV_CAP(dev, dev_cap);
	if (err) {
		mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
		return err;
	}

	if (dev_cap->min_page_sz > PAGE_SIZE) {
		mlx4_err(dev, "HCA minimum page size of %d bigger than "
			 "kernel PAGE_SIZE of %ld, aborting.\n",
			 dev_cap->min_page_sz, PAGE_SIZE);
		return -ENODEV;
	}
	if (dev_cap->num_ports > MLX4_MAX_PORTS) {
		mlx4_err(dev, "HCA has %d ports, but we only support %d, "
			 "aborting.\n",
			 dev_cap->num_ports, MLX4_MAX_PORTS);
		return -ENODEV;
	}

	if (dev_cap->uar_size > pci_resource_len(dev->pdev, 2)) {
		mlx4_err(dev, "HCA reported UAR size of 0x%x bigger than "
			 "PCI resource 2 size of 0x%llx, aborting.\n",
			 dev_cap->uar_size,
			 (unsigned long long) pci_resource_len(dev->pdev, 2));
		return -ENODEV;
	}

	dev->caps.num_ports	     = dev_cap->num_ports;
	for (i = 1; i <= dev->caps.num_ports; ++i) {
		dev->caps.vl_cap[i]	    = dev_cap->max_vl[i];
		dev->caps.ib_mtu_cap[i]	    = dev_cap->ib_mtu[i];
		dev->caps.gid_table_len[i]  = dev_cap->max_gids[i];
		dev->caps.pkey_table_len[i] = dev_cap->max_pkeys[i];
		dev->caps.port_width_cap[i] = dev_cap->max_port_width[i];
		dev->caps.eth_mtu_cap[i]    = dev_cap->eth_mtu[i];
		dev->caps.def_mac[i]        = dev_cap->def_mac[i];
		dev->caps.supported_type[i] = dev_cap->supported_port_types[i];
	}

	dev->caps.num_uars	     = dev_cap->uar_size / PAGE_SIZE;
	dev->caps.local_ca_ack_delay = dev_cap->local_ca_ack_delay;
	dev->caps.bf_reg_size	     = dev_cap->bf_reg_size;
	dev->caps.bf_regs_per_page   = dev_cap->bf_regs_per_page;
	dev->caps.max_sq_sg	     = dev_cap->max_sq_sg;
	dev->caps.max_rq_sg	     = dev_cap->max_rq_sg;
	dev->caps.max_wqes	     = dev_cap->max_qp_sz;
	dev->caps.max_qp_init_rdma   = dev_cap->max_requester_per_qp;
	dev->caps.max_srq_wqes	     = dev_cap->max_srq_sz;
	dev->caps.max_srq_sge	     = dev_cap->max_rq_sg - 1;
	dev->caps.reserved_srqs	     = dev_cap->reserved_srqs;
	dev->caps.max_sq_desc_sz     = dev_cap->max_sq_desc_sz;
	dev->caps.max_rq_desc_sz     = dev_cap->max_rq_desc_sz;
	dev->caps.num_qp_per_mgm     = MLX4_QP_PER_MGM;
	/*
	 * Subtract 1 from the limit because we need to allocate a
	 * spare CQE so the HCA HW can tell the difference between an
	 * empty CQ and a full CQ.
	 */
	dev->caps.max_cqes	     = dev_cap->max_cq_sz - 1;
	dev->caps.reserved_cqs	     = dev_cap->reserved_cqs;
	dev->caps.reserved_eqs	     = dev_cap->reserved_eqs;
	dev->caps.mtts_per_seg	     = 1 << log_mtts_per_seg;
	dev->caps.reserved_mtts	     = DIV_ROUND_UP(dev_cap->reserved_mtts,
						    dev->caps.mtts_per_seg);
	dev->caps.reserved_mrws	     = dev_cap->reserved_mrws;
	dev->caps.reserved_uars	     = dev_cap->reserved_uars;
	dev->caps.reserved_pds	     = dev_cap->reserved_pds;
	dev->caps.mtt_entry_sz	     = dev->caps.mtts_per_seg * dev_cap->mtt_entry_sz;
	dev->caps.max_msg_sz         = dev_cap->max_msg_sz;
	dev->caps.page_size_cap	     = ~(u32) (dev_cap->min_page_sz - 1);
	dev->caps.flags		     = dev_cap->flags;
	dev->caps.bmme_flags	     = dev_cap->bmme_flags;
	dev->caps.reserved_lkey	     = dev_cap->reserved_lkey;
	dev->caps.stat_rate_support  = dev_cap->stat_rate_support;
	dev->caps.max_gso_sz	     = dev_cap->max_gso_sz;

	dev->caps.log_num_macs  = log_num_mac;
	dev->caps.log_num_vlans = log_num_vlan;
	dev->caps.log_num_prios = use_prio ? 3 : 0;

	for (i = 1; i <= dev->caps.num_ports; ++i) {
		if (dev->caps.supported_type[i] != MLX4_PORT_TYPE_ETH)
			dev->caps.port_type[i] = MLX4_PORT_TYPE_IB;
		else
			dev->caps.port_type[i] = MLX4_PORT_TYPE_ETH;
		dev->caps.possible_type[i] = dev->caps.port_type[i];
		mlx4_priv(dev)->sense.sense_allowed[i] =
			dev->caps.supported_type[i] == MLX4_PORT_TYPE_AUTO;

		if (dev->caps.log_num_macs > dev_cap->log_max_macs[i]) {
			dev->caps.log_num_macs = dev_cap->log_max_macs[i];
			mlx4_warn(dev, "Requested number of MACs is too much "
				  "for port %d, reducing to %d.\n",
				  i, 1 << dev->caps.log_num_macs);
		}
		if (dev->caps.log_num_vlans > dev_cap->log_max_vlans[i]) {
			dev->caps.log_num_vlans = dev_cap->log_max_vlans[i];
			mlx4_warn(dev, "Requested number of VLANs is too much "
				  "for port %d, reducing to %d.\n",
				  i, 1 << dev->caps.log_num_vlans);
		}
	}

	mlx4_set_port_mask(dev);

	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW] = dev_cap->reserved_qps;
	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_ETH_ADDR] =
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_ADDR] =
		(1 << dev->caps.log_num_macs) *
		(1 << dev->caps.log_num_vlans) *
		(1 << dev->caps.log_num_prios) *
		dev->caps.num_ports;
	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_EXCH] = MLX4_NUM_FEXCH;

	dev->caps.reserved_qps = dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_ETH_ADDR] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_ADDR] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_EXCH];

	return 0;
}

/*
 * Change the port configuration of the device.
 * Every user of this function must hold the port mutex.
 */
int mlx4_change_port_types(struct mlx4_dev *dev,
			   enum mlx4_port_type *port_types)
{
	int err = 0;
	int change = 0;
	int port;

	for (port = 0; port <  dev->caps.num_ports; port++) {
		/* Change the port type only if the new type is different
		 * from the current, and not set to Auto */
		if (port_types[port] != dev->caps.port_type[port + 1]) {
			change = 1;
			dev->caps.port_type[port + 1] = port_types[port];
		}
	}
	if (change) {
		mlx4_unregister_device(dev);
		for (port = 1; port <= dev->caps.num_ports; port++) {
			mlx4_CLOSE_PORT(dev, port);
			err = mlx4_SET_PORT(dev, port);
			if (err) {
				mlx4_err(dev, "Failed to set port %d, "
					      "aborting\n", port);
				goto out;
			}
		}
		mlx4_set_port_mask(dev);
		err = mlx4_register_device(dev);
	}

out:
	return err;
}

static ssize_t show_port_type(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_attr);
	struct mlx4_dev *mdev = info->dev;
	char type[8];

	sprintf(type, "%s",
		(mdev->caps.port_type[info->port] == MLX4_PORT_TYPE_IB) ?
		"ib" : "eth");
	if (mdev->caps.possible_type[info->port] == MLX4_PORT_TYPE_AUTO)
		sprintf(buf, "auto (%s)\n", type);
	else
		sprintf(buf, "%s\n", type);

	return strlen(buf);
}

static ssize_t set_port_type(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_attr);
	struct mlx4_dev *mdev = info->dev;
	struct mlx4_priv *priv = mlx4_priv(mdev);
	enum mlx4_port_type types[MLX4_MAX_PORTS];
	enum mlx4_port_type new_types[MLX4_MAX_PORTS];
	int i;
	int err = 0;

	if (!strcmp(buf, "ib\n"))
		info->tmp_type = MLX4_PORT_TYPE_IB;
	else if (!strcmp(buf, "eth\n"))
		info->tmp_type = MLX4_PORT_TYPE_ETH;
	else if (!strcmp(buf, "auto\n"))
		info->tmp_type = MLX4_PORT_TYPE_AUTO;
	else {
		mlx4_err(mdev, "%s is not supported port type\n", buf);
		return -EINVAL;
	}

	mlx4_stop_sense(mdev);
	mutex_lock(&priv->port_mutex);
	/* Possible type is always the one that was delivered */
	mdev->caps.possible_type[info->port] = info->tmp_type;

	for (i = 0; i < mdev->caps.num_ports; i++) {
		types[i] = priv->port[i+1].tmp_type ? priv->port[i+1].tmp_type :
					mdev->caps.possible_type[i+1];
		if (types[i] == MLX4_PORT_TYPE_AUTO)
			types[i] = mdev->caps.port_type[i+1];
	}

	if (!(mdev->caps.flags & MLX4_DEV_CAP_FLAG_DPDP)) {
		for (i = 1; i <= mdev->caps.num_ports; i++) {
			if (mdev->caps.possible_type[i] == MLX4_PORT_TYPE_AUTO) {
				mdev->caps.possible_type[i] = mdev->caps.port_type[i];
				err = -EINVAL;
			}
		}
	}
	if (err) {
		mlx4_err(mdev, "Auto sensing is not supported on this HCA. "
			       "Set only 'eth' or 'ib' for both ports "
			       "(should be the same)\n");
		goto out;
	}

	mlx4_do_sense_ports(mdev, new_types, types);

	err = mlx4_check_port_params(mdev, new_types);
	if (err)
		goto out;

	/* We are about to apply the changes after the configuration
	 * was verified, no need to remember the temporary types
	 * any more */
	for (i = 0; i < mdev->caps.num_ports; i++)
		priv->port[i + 1].tmp_type = 0;

	err = mlx4_change_port_types(mdev, new_types);

out:
	mlx4_start_sense(mdev);
	mutex_unlock(&priv->port_mutex);
	return err ? err : count;
}

static int mlx4_load_fw(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;

	priv->fw.fw_icm = mlx4_alloc_icm(dev, priv->fw.fw_pages,
					 GFP_HIGHUSER | __GFP_NOWARN, 0);
	if (!priv->fw.fw_icm) {
		mlx4_err(dev, "Couldn't allocate FW area, aborting.\n");
		return -ENOMEM;
	}

	err = mlx4_MAP_FA(dev, priv->fw.fw_icm);
	if (err) {
		mlx4_err(dev, "MAP_FA command failed, aborting.\n");
		goto err_free;
	}

	err = mlx4_RUN_FW(dev);
	if (err) {
		mlx4_err(dev, "RUN_FW command failed, aborting.\n");
		goto err_unmap_fa;
	}

	return 0;

err_unmap_fa:
	mlx4_UNMAP_FA(dev);

err_free:
	mlx4_free_icm(dev, priv->fw.fw_icm, 0);
	return err;
}

static int mlx4_init_cmpt_table(struct mlx4_dev *dev, u64 cmpt_base,
				int cmpt_entry_sz)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;

	err = mlx4_init_icm_table(dev, &priv->qp_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_QP *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err)
		goto err;

	err = mlx4_init_icm_table(dev, &priv->srq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_SRQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_srqs,
				  dev->caps.reserved_srqs, 0, 0);
	if (err)
		goto err_qp;

	err = mlx4_init_icm_table(dev, &priv->cq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_CQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_cqs,
				  dev->caps.reserved_cqs, 0, 0);
	if (err)
		goto err_srq;

	err = mlx4_init_icm_table(dev, &priv->eq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_EQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz,
				  dev->caps.num_eqs, dev->caps.num_eqs, 0, 0);
	if (err)
		goto err_cq;

	return 0;

err_cq:
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);

err_srq:
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);

err_qp:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

err:
	return err;
}

static int mlx4_init_icm(struct mlx4_dev *dev, struct mlx4_dev_cap *dev_cap,
			 struct mlx4_init_hca_param *init_hca, u64 icm_size)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	u64 aux_pages;
	int err;

	err = mlx4_SET_ICM_SIZE(dev, icm_size, &aux_pages);
	if (err) {
		mlx4_err(dev, "SET_ICM_SIZE command failed, aborting.\n");
		return err;
	}

	mlx4_dbg(dev, "%lld KB of HCA context requires %lld KB aux memory.\n",
		 (unsigned long long) icm_size >> 10,
		 (unsigned long long) aux_pages << 2);

	priv->fw.aux_icm = mlx4_alloc_icm(dev, aux_pages,
					  GFP_HIGHUSER | __GFP_NOWARN, 0);
	if (!priv->fw.aux_icm) {
		mlx4_err(dev, "Couldn't allocate aux memory, aborting.\n");
		return -ENOMEM;
	}

	err = mlx4_MAP_ICM_AUX(dev, priv->fw.aux_icm);
	if (err) {
		mlx4_err(dev, "MAP_ICM_AUX command failed, aborting.\n");
		goto err_free_aux;
	}

	err = mlx4_init_cmpt_table(dev, init_hca->cmpt_base, dev_cap->cmpt_entry_sz);
	if (err) {
		mlx4_err(dev, "Failed to map cMPT context memory, aborting.\n");
		goto err_unmap_aux;
	}

	err = mlx4_init_icm_table(dev, &priv->eq_table.table,
				  init_hca->eqc_base, dev_cap->eqc_entry_sz,
				  dev->caps.num_eqs, dev->caps.num_eqs,
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map EQ context memory, aborting.\n");
		goto err_unmap_cmpt;
	}

	/*
	 * Reserved MTT entries must be aligned up to a cacheline
	 * boundary, since the FW will write to them, while the driver
	 * writes to all other MTT entries. (The variable
	 * dev->caps.mtt_entry_sz below is really the MTT segment
	 * size, not the raw entry size)
	 */
	dev->caps.reserved_mtts =
		ALIGN(dev->caps.reserved_mtts * dev->caps.mtt_entry_sz,
		      dma_get_cache_alignment()) / dev->caps.mtt_entry_sz;

	err = mlx4_init_icm_table(dev, &priv->mr_table.mtt_table,
				  init_hca->mtt_base,
				  dev->caps.mtt_entry_sz,
				  dev->caps.num_mtt_segs,
				  dev->caps.reserved_mtts, 1, 0);
	if (err) {
		mlx4_err(dev, "Failed to map MTT context memory, aborting.\n");
		goto err_unmap_eq;
	}

	err = mlx4_init_icm_table(dev, &priv->mr_table.dmpt_table,
				  init_hca->dmpt_base,
				  dev_cap->dmpt_entry_sz,
				  dev->caps.num_mpts,
				  dev->caps.reserved_mrws, 1, 1);
	if (err) {
		mlx4_err(dev, "Failed to map dMPT context memory, aborting.\n");
		goto err_unmap_mtt;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.qp_table,
				  init_hca->qpc_base,
				  dev_cap->qpc_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map QP context memory, aborting.\n");
		goto err_unmap_dmpt;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.auxc_table,
				  init_hca->auxc_base,
				  dev_cap->aux_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map AUXC context memory, aborting.\n");
		goto err_unmap_qp;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.altc_table,
				  init_hca->altc_base,
				  dev_cap->altc_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map ALTC context memory, aborting.\n");
		goto err_unmap_auxc;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.rdmarc_table,
				  init_hca->rdmarc_base,
				  dev_cap->rdmarc_entry_sz << priv->qp_table.rdmarc_shift,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map RDMARC context memory, aborting\n");
		goto err_unmap_altc;
	}

	err = mlx4_init_icm_table(dev, &priv->cq_table.table,
				  init_hca->cqc_base,
				  dev_cap->cqc_entry_sz,
				  dev->caps.num_cqs,
				  dev->caps.reserved_cqs, 0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map CQ context memory, aborting.\n");
		goto err_unmap_rdmarc;
	}

	err = mlx4_init_icm_table(dev, &priv->srq_table.table,
				  init_hca->srqc_base,
				  dev_cap->srq_entry_sz,
				  dev->caps.num_srqs,
				  dev->caps.reserved_srqs, 0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map SRQ context memory, aborting.\n");
		goto err_unmap_cq;
	}

	/*
	 * It's not strictly required, but for simplicity just map the
	 * whole multicast group table now.  The table isn't very big
	 * and it's a lot easier than trying to track ref counts.
	 */
	err = mlx4_init_icm_table(dev, &priv->mcg_table.table,
				  init_hca->mc_base, MLX4_MGM_ENTRY_SIZE,
				  dev->caps.num_mgms + dev->caps.num_amgms,
				  dev->caps.num_mgms + dev->caps.num_amgms,
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map MCG context memory, aborting.\n");
		goto err_unmap_srq;
	}

	return 0;

err_unmap_srq:
	mlx4_cleanup_icm_table(dev, &priv->srq_table.table);

err_unmap_cq:
	mlx4_cleanup_icm_table(dev, &priv->cq_table.table);

err_unmap_rdmarc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.rdmarc_table);

err_unmap_altc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.altc_table);

err_unmap_auxc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.auxc_table);

err_unmap_qp:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.qp_table);

err_unmap_dmpt:
	mlx4_cleanup_icm_table(dev, &priv->mr_table.dmpt_table);

err_unmap_mtt:
	mlx4_cleanup_icm_table(dev, &priv->mr_table.mtt_table);

err_unmap_eq:
	mlx4_cleanup_icm_table(dev, &priv->eq_table.table);

err_unmap_cmpt:
	mlx4_cleanup_icm_table(dev, &priv->eq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

err_unmap_aux:
	mlx4_UNMAP_ICM_AUX(dev);

err_free_aux:
	mlx4_free_icm(dev, priv->fw.aux_icm, 0);

	return err;
}

static void mlx4_free_icms(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	mlx4_cleanup_icm_table(dev, &priv->mcg_table.table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.rdmarc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.altc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.auxc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.qp_table);
	mlx4_cleanup_icm_table(dev, &priv->mr_table.dmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->mr_table.mtt_table);
	mlx4_cleanup_icm_table(dev, &priv->eq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->eq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

	mlx4_UNMAP_ICM_AUX(dev);
	mlx4_free_icm(dev, priv->fw.aux_icm, 0);
}

static void mlx4_close_hca(struct mlx4_dev *dev)
{
	mlx4_CLOSE_HCA(dev, 0);
	mlx4_free_icms(dev);
	mlx4_UNMAP_FA(dev);
	mlx4_free_icm(dev, mlx4_priv(dev)->fw.fw_icm, 0);
}

static int mlx4_init_hca(struct mlx4_dev *dev)
{
	struct mlx4_priv	  *priv = mlx4_priv(dev);
	struct mlx4_adapter	   adapter;
	struct mlx4_dev_cap	   dev_cap;
	struct mlx4_mod_stat_cfg   mlx4_cfg;
	struct mlx4_profile	   profile;
	struct mlx4_init_hca_param init_hca;
	u64 icm_size;
	int err;

	err = mlx4_QUERY_FW(dev);
	if (err) {
		if (err == -EACCES)
			mlx4_info(dev, "non-primary physical function, skipping.\n");
		else
			mlx4_err(dev, "QUERY_FW command failed, aborting.\n");
		return err;
	}

	err = mlx4_load_fw(dev);
	if (err) {
		mlx4_err(dev, "Failed to start FW, aborting.\n");
		return err;
	}

	mlx4_cfg.log_pg_sz_m = 1;
	mlx4_cfg.log_pg_sz = 0;
	err = mlx4_MOD_STAT_CFG(dev, &mlx4_cfg);
	if (err)
		mlx4_warn(dev, "Failed to override log_pg_sz parameter\n");

	err = mlx4_dev_cap(dev, &dev_cap);
	if (err) {
		mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
		goto err_stop_fw;
	}

	profile = default_profile;

	icm_size = mlx4_make_profile(dev, &profile, &dev_cap, &init_hca);
	if ((long long) icm_size < 0) {
		err = icm_size;
		goto err_stop_fw;
	}

	init_hca.log_uar_sz = ilog2(dev->caps.num_uars);

	err = mlx4_init_icm(dev, &dev_cap, &init_hca, icm_size);
	if (err)
		goto err_stop_fw;

	err = mlx4_INIT_HCA(dev, &init_hca);
	if (err) {
		mlx4_err(dev, "INIT_HCA command failed, aborting.\n");
		goto err_free_icm;
	}

	err = mlx4_QUERY_ADAPTER(dev, &adapter);
	if (err) {
		mlx4_err(dev, "QUERY_ADAPTER command failed, aborting.\n");
		goto err_close;
	}

	priv->eq_table.inta_pin = adapter.inta_pin;
	memcpy(dev->board_id, adapter.board_id, sizeof dev->board_id);

	return 0;

err_close:
	mlx4_CLOSE_HCA(dev, 0);

err_free_icm:
	mlx4_free_icms(dev);

err_stop_fw:
	mlx4_UNMAP_FA(dev);
	mlx4_free_icm(dev, priv->fw.fw_icm, 0);

	return err;
}

static int mlx4_setup_hca(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;
	int port;
	__be32 ib_port_default_caps;

	err = mlx4_init_uar_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "user access region table, aborting.\n");
		return err;
	}

	err = mlx4_uar_alloc(dev, &priv->driver_uar);
	if (err) {
		mlx4_err(dev, "Failed to allocate driver access region, "
			 "aborting.\n");
		goto err_uar_table_free;
	}

	priv->kar = ioremap(priv->driver_uar.pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!priv->kar) {
		mlx4_err(dev, "Couldn't map kernel access region, "
			 "aborting.\n");
		err = -ENOMEM;
		goto err_uar_free;
	}

	err = mlx4_init_pd_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "protection domain table, aborting.\n");
		goto err_kar_unmap;
	}

	err = mlx4_init_mr_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "memory region table, aborting.\n");
		goto err_pd_table_free;
	}

	err = mlx4_init_eq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "event queue table, aborting.\n");
		goto err_mr_table_free;
	}

	err = mlx4_cmd_use_events(dev);
	if (err) {
		mlx4_err(dev, "Failed to switch to event-driven "
			 "firmware commands, aborting.\n");
		goto err_eq_table_free;
	}

	err = mlx4_NOP(dev);
	if (err) {
		if (dev->flags & MLX4_FLAG_MSI_X) {
			mlx4_warn(dev, "NOP command failed to generate MSI-X "
				  "interrupt IRQ %d).\n",
				  priv->eq_table.eq[dev->caps.num_comp_vectors].irq);
			mlx4_warn(dev, "Trying again without MSI-X.\n");
		} else {
			mlx4_err(dev, "NOP command failed to generate interrupt "
				 "(IRQ %d), aborting.\n",
				 priv->eq_table.eq[dev->caps.num_comp_vectors].irq);
			mlx4_err(dev, "BIOS or ACPI interrupt routing problem?\n");
		}

		goto err_cmd_poll;
	}

	mlx4_dbg(dev, "NOP command IRQ test passed\n");

	err = mlx4_init_cq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "completion queue table, aborting.\n");
		goto err_cmd_poll;
	}

	err = mlx4_init_srq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "shared receive queue table, aborting.\n");
		goto err_cq_table_free;
	}

	err = mlx4_init_qp_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "queue pair table, aborting.\n");
		goto err_srq_table_free;
	}

	err = mlx4_init_mcg_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "multicast group table, aborting.\n");
		goto err_qp_table_free;
	}

	for (port = 1; port <= dev->caps.num_ports; port++) {
		ib_port_default_caps = 0;
		err = mlx4_get_port_ib_caps(dev, port, &ib_port_default_caps);
		if (err)
			mlx4_warn(dev, "failed to get port %d default "
				  "ib capabilities (%d). Continuing with "
				  "caps = 0\n", port, err);
		dev->caps.ib_port_def_cap[port] = ib_port_default_caps;
		err = mlx4_SET_PORT(dev, port);
		if (err) {
			mlx4_err(dev, "Failed to set port %d, aborting\n",
				port);
			goto err_mcg_table_free;
		}
	}

	return 0;

err_mcg_table_free:
	mlx4_cleanup_mcg_table(dev);

err_qp_table_free:
	mlx4_cleanup_qp_table(dev);

err_srq_table_free:
	mlx4_cleanup_srq_table(dev);

err_cq_table_free:
	mlx4_cleanup_cq_table(dev);

err_cmd_poll:
	mlx4_cmd_use_polling(dev);

err_eq_table_free:
	mlx4_cleanup_eq_table(dev);

err_mr_table_free:
	mlx4_cleanup_mr_table(dev);

err_pd_table_free:
	mlx4_cleanup_pd_table(dev);

err_kar_unmap:
	iounmap(priv->kar);

err_uar_free:
	mlx4_uar_free(dev, &priv->driver_uar);

err_uar_table_free:
	mlx4_cleanup_uar_table(dev);
	return err;
}

static void mlx4_enable_msi_x(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct msix_entry *entries;
	int nreq;
	int err;
	int i;

	if (msi_x) {
		nreq = min_t(int, dev->caps.num_eqs - dev->caps.reserved_eqs,
			     num_possible_cpus() + 1);
		entries = kcalloc(nreq, sizeof *entries, GFP_KERNEL);
		if (!entries)
			goto no_msi;

		for (i = 0; i < nreq; ++i)
			entries[i].entry = i;

	retry:
		err = pci_enable_msix(dev->pdev, entries, nreq);
		if (err) {
			/* Try again if at least 2 vectors are available */
			if (err > 1) {
				mlx4_info(dev, "Requested %d vectors, "
					  "but only %d MSI-X vectors available, "
					  "trying again\n", nreq, err);
				nreq = err;
				goto retry;
			}
			kfree(entries);
			goto no_msi;
		}

		dev->caps.num_comp_vectors = nreq - 1;
		for (i = 0; i < nreq; ++i)
			priv->eq_table.eq[i].irq = entries[i].vector;

		dev->flags |= MLX4_FLAG_MSI_X;

		kfree(entries);
		return;
	}

no_msi:
	dev->caps.num_comp_vectors = 1;

	for (i = 0; i < 2; ++i)
		priv->eq_table.eq[i].irq = dev->pdev->irq;
}

static int mlx4_init_port_info(struct mlx4_dev *dev, int port)
{
	struct mlx4_port_info *info = &mlx4_priv(dev)->port[port];
	int err = 0;

	info->dev = dev;
	info->port = port;
	mlx4_init_mac_table(dev, &info->mac_table);
	mlx4_init_vlan_table(dev, &info->vlan_table);

	sprintf(info->dev_name, "mlx4_port%d", port);
	info->port_attr.attr.name = info->dev_name;
	info->port_attr.attr.mode = S_IRUGO | S_IWUSR;
	info->port_attr.show      = show_port_type;
	info->port_attr.store     = set_port_type;
	sysfs_attr_init(&info->port_attr.attr);

	err = device_create_file(&dev->pdev->dev, &info->port_attr);
	if (err) {
		mlx4_err(dev, "Failed to create file for port %d\n", port);
		info->port = -1;
	}

	return err;
}

static void mlx4_cleanup_port_info(struct mlx4_port_info *info)
{
	if (info->port < 0)
		return;

	device_remove_file(&info->dev->pdev->dev, &info->port_attr);
}

static int __mlx4_init_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mlx4_priv *priv;
	struct mlx4_dev *dev;
	int err;
	int port;

	pr_info(DRV_NAME ": Initializing %s\n", pci_name(pdev));

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device, "
			"aborting.\n");
		return err;
	}

	/*
	 * Check for BARs.  We expect 0: 1MB
	 */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM) ||
	    pci_resource_len(pdev, 0) != 1 << 20) {
		dev_err(&pdev->dev, "Missing DCS, aborting.\n");
		err = -ENODEV;
		goto err_disable_pdev;
	}
	if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "Missing UAR, aborting.\n");
		err = -ENODEV;
		goto err_disable_pdev;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		dev_err(&pdev->dev, "Couldn't get PCI resources, aborting\n");
		goto err_disable_pdev;
	}

	pci_set_master(pdev);

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit PCI DMA mask.\n");
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set PCI DMA mask, aborting.\n");
			goto err_release_regions;
		}
	}
	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit "
			 "consistent PCI DMA mask.\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set consistent PCI DMA mask, "
				"aborting.\n");
			goto err_release_regions;
		}
	}

	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Device struct alloc failed, "
			"aborting.\n");
		err = -ENOMEM;
		goto err_release_regions;
	}

	dev       = &priv->dev;
	dev->pdev = pdev;
	INIT_LIST_HEAD(&priv->ctx_list);
	spin_lock_init(&priv->ctx_lock);

	mutex_init(&priv->port_mutex);

	INIT_LIST_HEAD(&priv->pgdir_list);
	mutex_init(&priv->pgdir_mutex);

	/*
	 * Now reset the HCA before we touch the PCI capabilities or
	 * attempt a firmware command, since a boot ROM may have left
	 * the HCA in an undefined state.
	 */
	err = mlx4_reset(dev);
	if (err) {
		mlx4_err(dev, "Failed to reset HCA, aborting.\n");
		goto err_free_dev;
	}

	if (mlx4_cmd_init(dev)) {
		mlx4_err(dev, "Failed to init command interface, aborting.\n");
		goto err_free_dev;
	}

	err = mlx4_init_hca(dev);
	if (err)
		goto err_cmd;

	err = mlx4_alloc_eq_table(dev);
	if (err)
		goto err_close;

	mlx4_enable_msi_x(dev);

	err = mlx4_setup_hca(dev);
	if (err == -EBUSY && (dev->flags & MLX4_FLAG_MSI_X)) {
		dev->flags &= ~MLX4_FLAG_MSI_X;
		pci_disable_msix(pdev);
		err = mlx4_setup_hca(dev);
	}

	if (err)
		goto err_free_eq;

	for (port = 1; port <= dev->caps.num_ports; port++) {
		err = mlx4_init_port_info(dev, port);
		if (err)
			goto err_port;
	}

	err = mlx4_register_device(dev);
	if (err)
		goto err_port;

	mlx4_sense_init(dev);
	mlx4_start_sense(dev);

	pci_set_drvdata(pdev, dev);

	return 0;

err_port:
	for (--port; port >= 1; --port)
		mlx4_cleanup_port_info(&priv->port[port]);

	mlx4_cleanup_mcg_table(dev);
	mlx4_cleanup_qp_table(dev);
	mlx4_cleanup_srq_table(dev);
	mlx4_cleanup_cq_table(dev);
	mlx4_cmd_use_polling(dev);
	mlx4_cleanup_eq_table(dev);
	mlx4_cleanup_mr_table(dev);
	mlx4_cleanup_pd_table(dev);
	mlx4_cleanup_uar_table(dev);

err_free_eq:
	mlx4_free_eq_table(dev);

err_close:
	if (dev->flags & MLX4_FLAG_MSI_X)
		pci_disable_msix(pdev);

	mlx4_close_hca(dev);

err_cmd:
	mlx4_cmd_cleanup(dev);

err_free_dev:
	kfree(priv);

err_release_regions:
	pci_release_regions(pdev);

err_disable_pdev:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	return err;
}

static int __devinit mlx4_init_one(struct pci_dev *pdev,
				   const struct pci_device_id *id)
{
	printk_once(KERN_INFO "%s", mlx4_version);

	return __mlx4_init_one(pdev, id);
}

static void mlx4_remove_one(struct pci_dev *pdev)
{
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);
	struct mlx4_priv *priv = mlx4_priv(dev);
	int p;

	if (dev) {
		mlx4_stop_sense(dev);
		mlx4_unregister_device(dev);

		for (p = 1; p <= dev->caps.num_ports; p++) {
			mlx4_cleanup_port_info(&priv->port[p]);
			mlx4_CLOSE_PORT(dev, p);
		}

		mlx4_cleanup_mcg_table(dev);
		mlx4_cleanup_qp_table(dev);
		mlx4_cleanup_srq_table(dev);
		mlx4_cleanup_cq_table(dev);
		mlx4_cmd_use_polling(dev);
		mlx4_cleanup_eq_table(dev);
		mlx4_cleanup_mr_table(dev);
		mlx4_cleanup_pd_table(dev);

		iounmap(priv->kar);
		mlx4_uar_free(dev, &priv->driver_uar);
		mlx4_cleanup_uar_table(dev);
		mlx4_free_eq_table(dev);
		mlx4_close_hca(dev);
		mlx4_cmd_cleanup(dev);

		if (dev->flags & MLX4_FLAG_MSI_X)
			pci_disable_msix(pdev);

		kfree(priv);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
	}
}

int mlx4_restart_one(struct pci_dev *pdev)
{
	mlx4_remove_one(pdev);
	return __mlx4_init_one(pdev, NULL);
}

static DEFINE_PCI_DEVICE_TABLE(mlx4_pci_table) = {
	{ PCI_VDEVICE(MELLANOX, 0x6340) }, /* MT25408 "Hermon" SDR */
	{ PCI_VDEVICE(MELLANOX, 0x634a) }, /* MT25408 "Hermon" DDR */
	{ PCI_VDEVICE(MELLANOX, 0x6354) }, /* MT25408 "Hermon" QDR */
	{ PCI_VDEVICE(MELLANOX, 0x6732) }, /* MT25408 "Hermon" DDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x673c) }, /* MT25408 "Hermon" QDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6368) }, /* MT25408 "Hermon" EN 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6750) }, /* MT25408 "Hermon" EN 10GigE PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6372) }, /* MT25458 ConnectX EN 10GBASE-T 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x675a) }, /* MT25458 ConnectX EN 10GBASE-T+Gen2 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6764) }, /* MT26468 ConnectX EN 10GigE PCIe gen2*/
	{ PCI_VDEVICE(MELLANOX, 0x6746) }, /* MT26438 ConnectX EN 40GigE PCIe gen2 5GT/s */
	{ PCI_VDEVICE(MELLANOX, 0x676e) }, /* MT26478 ConnectX2 40GigE PCIe gen2 */
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mlx4_pci_table);

static struct pci_driver mlx4_driver = {
	.name		= DRV_NAME,
	.id_table	= mlx4_pci_table,
	.probe		= mlx4_init_one,
	.remove		= __devexit_p(mlx4_remove_one)
};

static int __init mlx4_verify_params(void)
{
	if ((log_num_mac < 0) || (log_num_mac > 7)) {
		pr_warning("mlx4_core: bad num_mac: %d\n", log_num_mac);
		return -1;
	}

	if ((log_num_vlan < 0) || (log_num_vlan > 7)) {
		pr_warning("mlx4_core: bad num_vlan: %d\n", log_num_vlan);
		return -1;
	}

	if ((log_mtts_per_seg < 1) || (log_mtts_per_seg > 5)) {
		pr_warning("mlx4_core: bad log_mtts_per_seg: %d\n", log_mtts_per_seg);
		return -1;
	}

	return 0;
}

static int __init mlx4_init(void)
{
	int ret;

	if (mlx4_verify_params())
		return -EINVAL;

	mlx4_catas_init();

	mlx4_wq = create_singlethread_workqueue("mlx4");
	if (!mlx4_wq)
		return -ENOMEM;

	ret = pci_register_driver(&mlx4_driver);
	return ret < 0 ? ret : 0;
}

static void __exit mlx4_cleanup(void)
{
	pci_unregister_driver(&mlx4_driver);
	destroy_workqueue(mlx4_wq);
}

module_init(mlx4_init);
module_exit(mlx4_cleanup);
