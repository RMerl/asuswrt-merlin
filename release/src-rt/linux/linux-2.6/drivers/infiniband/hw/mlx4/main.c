/*
 * Copyright (c) 2006, 2007 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
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
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>

#include <rdma/ib_smi.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_addr.h>

#include <linux/mlx4/driver.h>
#include <linux/mlx4/cmd.h>

#include "mlx4_ib.h"
#include "user.h"

#define DRV_NAME	"mlx4_ib"
#define DRV_VERSION	"1.0"
#define DRV_RELDATE	"April 4, 2008"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("Mellanox ConnectX HCA InfiniBand driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

static const char mlx4_ib_version[] =
	DRV_NAME ": Mellanox ConnectX InfiniBand driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

struct update_gid_work {
	struct work_struct	work;
	union ib_gid		gids[128];
	struct mlx4_ib_dev     *dev;
	int			port;
};

static struct workqueue_struct *wq;

static void init_query_mad(struct ib_smp *mad)
{
	mad->base_version  = 1;
	mad->mgmt_class    = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->class_version = 1;
	mad->method	   = IB_MGMT_METHOD_GET;
}

static union ib_gid zgid;

static int mlx4_ib_query_device(struct ib_device *ibdev,
				struct ib_device_attr *props)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memset(props, 0, sizeof *props);

	props->fw_ver = dev->dev->caps.fw_ver;
	props->device_cap_flags    = IB_DEVICE_CHANGE_PHY_PORT |
		IB_DEVICE_PORT_ACTIVE_EVENT		|
		IB_DEVICE_SYS_IMAGE_GUID		|
		IB_DEVICE_RC_RNR_NAK_GEN		|
		IB_DEVICE_BLOCK_MULTICAST_LOOPBACK;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BAD_PKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_PKEY_CNTR;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BAD_QKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_QKEY_CNTR;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_APM)
		props->device_cap_flags |= IB_DEVICE_AUTO_PATH_MIG;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_UD_AV_PORT)
		props->device_cap_flags |= IB_DEVICE_UD_AV_PORT_ENFORCE;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_IPOIB_CSUM)
		props->device_cap_flags |= IB_DEVICE_UD_IP_CSUM;
	if (dev->dev->caps.max_gso_sz && dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BLH)
		props->device_cap_flags |= IB_DEVICE_UD_TSO;
	if (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_RESERVED_LKEY)
		props->device_cap_flags |= IB_DEVICE_LOCAL_DMA_LKEY;
	if ((dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_LOCAL_INV) &&
	    (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_REMOTE_INV) &&
	    (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_FAST_REG_WR))
		props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;

	props->vendor_id	   = be32_to_cpup((__be32 *) (out_mad->data + 36)) &
		0xffffff;
	props->vendor_part_id	   = be16_to_cpup((__be16 *) (out_mad->data + 30));
	props->hw_ver		   = be32_to_cpup((__be32 *) (out_mad->data + 32));
	memcpy(&props->sys_image_guid, out_mad->data +	4, 8);

	props->max_mr_size	   = ~0ull;
	props->page_size_cap	   = dev->dev->caps.page_size_cap;
	props->max_qp		   = dev->dev->caps.num_qps - dev->dev->caps.reserved_qps;
	props->max_qp_wr	   = dev->dev->caps.max_wqes;
	props->max_sge		   = min(dev->dev->caps.max_sq_sg,
					 dev->dev->caps.max_rq_sg);
	props->max_cq		   = dev->dev->caps.num_cqs - dev->dev->caps.reserved_cqs;
	props->max_cqe		   = dev->dev->caps.max_cqes;
	props->max_mr		   = dev->dev->caps.num_mpts - dev->dev->caps.reserved_mrws;
	props->max_pd		   = dev->dev->caps.num_pds - dev->dev->caps.reserved_pds;
	props->max_qp_rd_atom	   = dev->dev->caps.max_qp_dest_rdma;
	props->max_qp_init_rd_atom = dev->dev->caps.max_qp_init_rdma;
	props->max_res_rd_atom	   = props->max_qp_rd_atom * props->max_qp;
	props->max_srq		   = dev->dev->caps.num_srqs - dev->dev->caps.reserved_srqs;
	props->max_srq_wr	   = dev->dev->caps.max_srq_wqes - 1;
	props->max_srq_sge	   = dev->dev->caps.max_srq_sge;
	props->max_fast_reg_page_list_len = MLX4_MAX_FAST_REG_PAGES;
	props->local_ca_ack_delay  = dev->dev->caps.local_ca_ack_delay;
	props->atomic_cap	   = dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_ATOMIC ?
		IB_ATOMIC_HCA : IB_ATOMIC_NONE;
	props->masked_atomic_cap   = IB_ATOMIC_HCA;
	props->max_pkeys	   = dev->dev->caps.pkey_table_len[1];
	props->max_mcast_grp	   = dev->dev->caps.num_mgms + dev->dev->caps.num_amgms;
	props->max_mcast_qp_attach = dev->dev->caps.num_qp_per_mgm;
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	props->max_map_per_fmr = (1 << (32 - ilog2(dev->dev->caps.num_mpts))) - 1;

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static enum rdma_link_layer
mlx4_ib_port_link_layer(struct ib_device *device, u8 port_num)
{
	struct mlx4_dev *dev = to_mdev(device)->dev;

	return dev->caps.port_mask & (1 << (port_num - 1)) ?
		IB_LINK_LAYER_INFINIBAND : IB_LINK_LAYER_ETHERNET;
}

static int ib_link_query_port(struct ib_device *ibdev, u8 port,
			      struct ib_port_attr *props,
			      struct ib_smp *out_mad)
{
	props->lid		= be16_to_cpup((__be16 *) (out_mad->data + 16));
	props->lmc		= out_mad->data[34] & 0x7;
	props->sm_lid		= be16_to_cpup((__be16 *) (out_mad->data + 18));
	props->sm_sl		= out_mad->data[36] & 0xf;
	props->state		= out_mad->data[32] & 0xf;
	props->phys_state	= out_mad->data[33] >> 4;
	props->port_cap_flags	= be32_to_cpup((__be32 *) (out_mad->data + 20));
	props->gid_tbl_len	= to_mdev(ibdev)->dev->caps.gid_table_len[port];
	props->max_msg_sz	= to_mdev(ibdev)->dev->caps.max_msg_sz;
	props->pkey_tbl_len	= to_mdev(ibdev)->dev->caps.pkey_table_len[port];
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 48));
	props->active_width	= out_mad->data[31] & 0xf;
	props->active_speed	= out_mad->data[35] >> 4;
	props->max_mtu		= out_mad->data[41] & 0xf;
	props->active_mtu	= out_mad->data[36] >> 4;
	props->subnet_timeout	= out_mad->data[51] & 0x1f;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= out_mad->data[41] >> 4;

	return 0;
}

static u8 state_to_phys_state(enum ib_port_state state)
{
	return state == IB_PORT_ACTIVE ? 5 : 3;
}

static int eth_link_query_port(struct ib_device *ibdev, u8 port,
			       struct ib_port_attr *props,
			       struct ib_smp *out_mad)
{
	struct mlx4_ib_iboe *iboe = &to_mdev(ibdev)->iboe;
	struct net_device *ndev;
	enum ib_mtu tmp;

	props->active_width	= IB_WIDTH_1X;
	props->active_speed	= 4;
	props->port_cap_flags	= IB_PORT_CM_SUP;
	props->gid_tbl_len	= to_mdev(ibdev)->dev->caps.gid_table_len[port];
	props->max_msg_sz	= to_mdev(ibdev)->dev->caps.max_msg_sz;
	props->pkey_tbl_len	= 1;
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 48));
	props->max_mtu		= IB_MTU_2048;
	props->subnet_timeout	= 0;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= 0;
	props->state		= IB_PORT_DOWN;
	props->phys_state	= state_to_phys_state(props->state);
	props->active_mtu	= IB_MTU_256;
	spin_lock(&iboe->lock);
	ndev = iboe->netdevs[port - 1];
	if (!ndev)
		goto out;

	tmp = iboe_get_mtu(ndev->mtu);
	props->active_mtu = tmp ? min(props->max_mtu, tmp) : IB_MTU_256;

	props->state		= (netif_running(ndev) && netif_carrier_ok(ndev)) ?
					IB_PORT_ACTIVE : IB_PORT_DOWN;
	props->phys_state	= state_to_phys_state(props->state);

out:
	spin_unlock(&iboe->lock);
	return 0;
}

static int mlx4_ib_query_port(struct ib_device *ibdev, u8 port,
			      struct ib_port_attr *props)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof *props);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	err = mlx4_ib_port_link_layer(ibdev, port) == IB_LINK_LAYER_INFINIBAND ?
		ib_link_query_port(ibdev, port, props, out_mad) :
		eth_link_query_port(ibdev, port, props, out_mad);

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static int __mlx4_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			       union ib_gid *gid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw, out_mad->data + 8, 8);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_GUID_INFO;
	in_mad->attr_mod = cpu_to_be32(index / 8);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw + 8, out_mad->data + (index % 8) * 8, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int iboe_query_gid(struct ib_device *ibdev, u8 port, int index,
			  union ib_gid *gid)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);

	*gid = dev->iboe.gid_table[port - 1][index];

	return 0;
}

static int mlx4_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			     union ib_gid *gid)
{
	if (rdma_port_get_link_layer(ibdev, port) == IB_LINK_LAYER_INFINIBAND)
		return __mlx4_ib_query_gid(ibdev, port, index, gid);
	else
		return iboe_query_gid(ibdev, port, index, gid);
}

static int mlx4_ib_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
			      u16 *pkey)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PKEY_TABLE;
	in_mad->attr_mod = cpu_to_be32(index / 32);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	*pkey = be16_to_cpu(((__be16 *) out_mad->data)[index % 32]);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mlx4_ib_modify_device(struct ib_device *ibdev, int mask,
				 struct ib_device_modify *props)
{
	struct mlx4_cmd_mailbox *mailbox;

	if (mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (!(mask & IB_DEVICE_MODIFY_NODE_DESC))
		return 0;

	spin_lock(&to_mdev(ibdev)->sm_lock);
	memcpy(ibdev->node_desc, props->node_desc, 64);
	spin_unlock(&to_mdev(ibdev)->sm_lock);

	/*
	 * If possible, pass node desc to FW, so it can generate
	 * a 144 trap.  If cmd fails, just ignore.
	 */
	mailbox = mlx4_alloc_cmd_mailbox(to_mdev(ibdev)->dev);
	if (IS_ERR(mailbox))
		return 0;

	memset(mailbox->buf, 0, 256);
	memcpy(mailbox->buf, props->node_desc, 64);
	mlx4_cmd(to_mdev(ibdev)->dev, mailbox->dma, 1, 0,
		 MLX4_CMD_SET_NODE, MLX4_CMD_TIME_CLASS_A);

	mlx4_free_cmd_mailbox(to_mdev(ibdev)->dev, mailbox);

	return 0;
}

static int mlx4_SET_PORT(struct mlx4_ib_dev *dev, u8 port, int reset_qkey_viols,
			 u32 cap_mask)
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;
	u8 is_eth = dev->dev->caps.port_type[port] == MLX4_PORT_TYPE_ETH;

	mailbox = mlx4_alloc_cmd_mailbox(dev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	memset(mailbox->buf, 0, 256);

	if (dev->dev->flags & MLX4_FLAG_OLD_PORT_CMDS) {
		*(u8 *) mailbox->buf	     = !!reset_qkey_viols << 6;
		((__be32 *) mailbox->buf)[2] = cpu_to_be32(cap_mask);
	} else {
		((u8 *) mailbox->buf)[3]     = !!reset_qkey_viols;
		((__be32 *) mailbox->buf)[1] = cpu_to_be32(cap_mask);
	}

	err = mlx4_cmd(dev->dev, mailbox->dma, port, is_eth, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev->dev, mailbox);
	return err;
}

static int mlx4_ib_modify_port(struct ib_device *ibdev, u8 port, int mask,
			       struct ib_port_modify *props)
{
	struct ib_port_attr attr;
	u32 cap_mask;
	int err;

	mutex_lock(&to_mdev(ibdev)->cap_mask_mutex);

	err = mlx4_ib_query_port(ibdev, port, &attr);
	if (err)
		goto out;

	cap_mask = (attr.port_cap_flags | props->set_port_cap_mask) &
		~props->clr_port_cap_mask;

	err = mlx4_SET_PORT(to_mdev(ibdev), port,
			    !!(mask & IB_PORT_RESET_QKEY_CNTR),
			    cap_mask);

out:
	mutex_unlock(&to_mdev(ibdev)->cap_mask_mutex);
	return err;
}

static struct ib_ucontext *mlx4_ib_alloc_ucontext(struct ib_device *ibdev,
						  struct ib_udata *udata)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct mlx4_ib_ucontext *context;
	struct mlx4_ib_alloc_ucontext_resp resp;
	int err;

	if (!dev->ib_active)
		return ERR_PTR(-EAGAIN);

	resp.qp_tab_size      = dev->dev->caps.num_qps;
	resp.bf_reg_size      = dev->dev->caps.bf_reg_size;
	resp.bf_regs_per_page = dev->dev->caps.bf_regs_per_page;

	context = kmalloc(sizeof *context, GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);

	err = mlx4_uar_alloc(to_mdev(ibdev)->dev, &context->uar);
	if (err) {
		kfree(context);
		return ERR_PTR(err);
	}

	INIT_LIST_HEAD(&context->db_page_list);
	mutex_init(&context->db_page_mutex);

	err = ib_copy_to_udata(udata, &resp, sizeof resp);
	if (err) {
		mlx4_uar_free(to_mdev(ibdev)->dev, &context->uar);
		kfree(context);
		return ERR_PTR(-EFAULT);
	}

	return &context->ibucontext;
}

static int mlx4_ib_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
	struct mlx4_ib_ucontext *context = to_mucontext(ibcontext);

	mlx4_uar_free(to_mdev(ibcontext->device)->dev, &context->uar);
	kfree(context);

	return 0;
}

static int mlx4_ib_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	struct mlx4_ib_dev *dev = to_mdev(context->device);

	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	if (vma->vm_pgoff == 0) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

		if (io_remap_pfn_range(vma, vma->vm_start,
				       to_mucontext(context)->uar.pfn,
				       PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
	} else if (vma->vm_pgoff == 1 && dev->dev->caps.bf_reg_size != 0) {
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

		if (io_remap_pfn_range(vma, vma->vm_start,
				       to_mucontext(context)->uar.pfn +
				       dev->dev->caps.num_uars,
				       PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
	} else
		return -EINVAL;

	return 0;
}

static struct ib_pd *mlx4_ib_alloc_pd(struct ib_device *ibdev,
				      struct ib_ucontext *context,
				      struct ib_udata *udata)
{
	struct mlx4_ib_pd *pd;
	int err;

	pd = kmalloc(sizeof *pd, GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	err = mlx4_pd_alloc(to_mdev(ibdev)->dev, &pd->pdn);
	if (err) {
		kfree(pd);
		return ERR_PTR(err);
	}

	if (context)
		if (ib_copy_to_udata(udata, &pd->pdn, sizeof (__u32))) {
			mlx4_pd_free(to_mdev(ibdev)->dev, pd->pdn);
			kfree(pd);
			return ERR_PTR(-EFAULT);
		}

	return &pd->ibpd;
}

static int mlx4_ib_dealloc_pd(struct ib_pd *pd)
{
	mlx4_pd_free(to_mdev(pd->device)->dev, to_mpd(pd)->pdn);
	kfree(pd);

	return 0;
}

static int add_gid_entry(struct ib_qp *ibqp, union ib_gid *gid)
{
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct mlx4_ib_gid_entry *ge;

	ge = kzalloc(sizeof *ge, GFP_KERNEL);
	if (!ge)
		return -ENOMEM;

	ge->gid = *gid;
	if (mlx4_ib_add_mc(mdev, mqp, gid)) {
		ge->port = mqp->port;
		ge->added = 1;
	}

	mutex_lock(&mqp->mutex);
	list_add_tail(&ge->list, &mqp->gid_list);
	mutex_unlock(&mqp->mutex);

	return 0;
}

int mlx4_ib_add_mc(struct mlx4_ib_dev *mdev, struct mlx4_ib_qp *mqp,
		   union ib_gid *gid)
{
	u8 mac[6];
	struct net_device *ndev;
	int ret = 0;

	if (!mqp->port)
		return 0;

	spin_lock(&mdev->iboe.lock);
	ndev = mdev->iboe.netdevs[mqp->port - 1];
	if (ndev)
		dev_hold(ndev);
	spin_unlock(&mdev->iboe.lock);

	if (ndev) {
		rdma_get_mcast_mac((struct in6_addr *)gid, mac);
		rtnl_lock();
		dev_mc_add(mdev->iboe.netdevs[mqp->port - 1], mac);
		ret = 1;
		rtnl_unlock();
		dev_put(ndev);
	}

	return ret;
}

static int mlx4_ib_mcg_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	int err;
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);

	err = mlx4_multicast_attach(mdev->dev, &mqp->mqp, gid->raw,
				    !!(mqp->flags & MLX4_IB_QP_BLOCK_MULTICAST_LOOPBACK),
				    MLX4_PROT_IB_IPV6);
	if (err)
		return err;

	err = add_gid_entry(ibqp, gid);
	if (err)
		goto err_add;

	return 0;

err_add:
	mlx4_multicast_detach(mdev->dev, &mqp->mqp, gid->raw, MLX4_PROT_IB_IPV6);
	return err;
}

static struct mlx4_ib_gid_entry *find_gid_entry(struct mlx4_ib_qp *qp, u8 *raw)
{
	struct mlx4_ib_gid_entry *ge;
	struct mlx4_ib_gid_entry *tmp;
	struct mlx4_ib_gid_entry *ret = NULL;

	list_for_each_entry_safe(ge, tmp, &qp->gid_list, list) {
		if (!memcmp(raw, ge->gid.raw, 16)) {
			ret = ge;
			break;
		}
	}

	return ret;
}

static int mlx4_ib_mcg_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	int err;
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);
	u8 mac[6];
	struct net_device *ndev;
	struct mlx4_ib_gid_entry *ge;

	err = mlx4_multicast_detach(mdev->dev,
				    &mqp->mqp, gid->raw, MLX4_PROT_IB_IPV6);
	if (err)
		return err;

	mutex_lock(&mqp->mutex);
	ge = find_gid_entry(mqp, gid->raw);
	if (ge) {
		spin_lock(&mdev->iboe.lock);
		ndev = ge->added ? mdev->iboe.netdevs[ge->port - 1] : NULL;
		if (ndev)
			dev_hold(ndev);
		spin_unlock(&mdev->iboe.lock);
		rdma_get_mcast_mac((struct in6_addr *)gid, mac);
		if (ndev) {
			rtnl_lock();
			dev_mc_del(mdev->iboe.netdevs[ge->port - 1], mac);
			rtnl_unlock();
			dev_put(ndev);
		}
		list_del(&ge->list);
		kfree(ge);
	} else
		printk(KERN_WARNING "could not find mgid entry\n");

	mutex_unlock(&mqp->mutex);

	return 0;
}

static int init_node_data(struct mlx4_ib_dev *dev)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;

	err = mlx4_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(dev->ib_dev.node_desc, out_mad->data, 64);

	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx4_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(&dev->ib_dev.node_guid, out_mad->data + 12, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "MT%d\n", dev->dev->pdev->device);
}

static ssize_t show_fw_ver(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%d.%d.%d\n", (int) (dev->dev->caps.fw_ver >> 32),
		       (int) (dev->dev->caps.fw_ver >> 16) & 0xffff,
		       (int) dev->dev->caps.fw_ver & 0xffff);
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", dev->dev->rev_id);
}

static ssize_t show_board(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%.*s\n", MLX4_BOARD_ID_LEN,
		       dev->dev->board_id);
}

static DEVICE_ATTR(hw_rev,   S_IRUGO, show_rev,    NULL);
static DEVICE_ATTR(fw_ver,   S_IRUGO, show_fw_ver, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);

static struct device_attribute *mlx4_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_fw_ver,
	&dev_attr_hca_type,
	&dev_attr_board_id
};

static void mlx4_addrconf_ifid_eui48(u8 *eui, u16 vlan_id, struct net_device *dev)
{
	memcpy(eui, dev->dev_addr, 3);
	memcpy(eui + 5, dev->dev_addr + 3, 3);
	if (vlan_id < 0x1000) {
		eui[3] = vlan_id >> 8;
		eui[4] = vlan_id & 0xff;
	} else {
		eui[3] = 0xff;
		eui[4] = 0xfe;
	}
	eui[0] ^= 2;
}

static void update_gids_task(struct work_struct *work)
{
	struct update_gid_work *gw = container_of(work, struct update_gid_work, work);
	struct mlx4_cmd_mailbox *mailbox;
	union ib_gid *gids;
	int err;
	struct mlx4_dev	*dev = gw->dev->dev;
	struct ib_event event;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox)) {
		printk(KERN_WARNING "update gid table failed %ld\n", PTR_ERR(mailbox));
		return;
	}

	gids = mailbox->buf;
	memcpy(gids, gw->gids, sizeof gw->gids);

	err = mlx4_cmd(dev, mailbox->dma, MLX4_SET_PORT_GID_TABLE << 8 | gw->port,
		       1, MLX4_CMD_SET_PORT, MLX4_CMD_TIME_CLASS_B);
	if (err)
		printk(KERN_WARNING "set port command failed\n");
	else {
		memcpy(gw->dev->iboe.gid_table[gw->port - 1], gw->gids, sizeof gw->gids);
		event.device = &gw->dev->ib_dev;
		event.element.port_num = gw->port;
		event.event    = IB_EVENT_LID_CHANGE;
		ib_dispatch_event(&event);
	}

	mlx4_free_cmd_mailbox(dev, mailbox);
	kfree(gw);
}

static int update_ipv6_gids(struct mlx4_ib_dev *dev, int port, int clear)
{
	struct net_device *ndev = dev->iboe.netdevs[port - 1];
	struct update_gid_work *work;
	struct net_device *tmp;
	int i;
	u8 *hits;
	int ret;
	union ib_gid gid;
	int free;
	int found;
	int need_update = 0;
	u16 vid;

	work = kzalloc(sizeof *work, GFP_ATOMIC);
	if (!work)
		return -ENOMEM;

	hits = kzalloc(128, GFP_ATOMIC);
	if (!hits) {
		ret = -ENOMEM;
		goto out;
	}

	rcu_read_lock();
	for_each_netdev_rcu(&init_net, tmp) {
		if (ndev && (tmp == ndev || rdma_vlan_dev_real_dev(tmp) == ndev)) {
			gid.global.subnet_prefix = cpu_to_be64(0xfe80000000000000LL);
			vid = rdma_vlan_dev_vlan_id(tmp);
			mlx4_addrconf_ifid_eui48(&gid.raw[8], vid, ndev);
			found = 0;
			free = -1;
			for (i = 0; i < 128; ++i) {
				if (free < 0 &&
				    !memcmp(&dev->iboe.gid_table[port - 1][i], &zgid, sizeof zgid))
					free = i;
				if (!memcmp(&dev->iboe.gid_table[port - 1][i], &gid, sizeof gid)) {
					hits[i] = 1;
					found = 1;
					break;
				}
			}

			if (!found) {
				if (tmp == ndev &&
				    (memcmp(&dev->iboe.gid_table[port - 1][0],
					    &gid, sizeof gid) ||
				     !memcmp(&dev->iboe.gid_table[port - 1][0],
					     &zgid, sizeof gid))) {
					dev->iboe.gid_table[port - 1][0] = gid;
					++need_update;
					hits[0] = 1;
				} else if (free >= 0) {
					dev->iboe.gid_table[port - 1][free] = gid;
					hits[free] = 1;
					++need_update;
				}
			}
		}
	}
	rcu_read_unlock();

	for (i = 0; i < 128; ++i)
		if (!hits[i]) {
			if (memcmp(&dev->iboe.gid_table[port - 1][i], &zgid, sizeof zgid))
				++need_update;
			dev->iboe.gid_table[port - 1][i] = zgid;
		}

	if (need_update) {
		memcpy(work->gids, dev->iboe.gid_table[port - 1], sizeof work->gids);
		INIT_WORK(&work->work, update_gids_task);
		work->port = port;
		work->dev = dev;
		queue_work(wq, &work->work);
	} else
		kfree(work);

	kfree(hits);
	return 0;

out:
	kfree(work);
	return ret;
}

static void handle_en_event(struct mlx4_ib_dev *dev, int port, unsigned long event)
{
	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGEADDR:
		update_ipv6_gids(dev, port, 0);
		break;

	case NETDEV_DOWN:
		update_ipv6_gids(dev, port, 1);
		dev->iboe.netdevs[port - 1] = NULL;
	}
}

static void netdev_added(struct mlx4_ib_dev *dev, int port)
{
	update_ipv6_gids(dev, port, 0);
}

static void netdev_removed(struct mlx4_ib_dev *dev, int port)
{
	update_ipv6_gids(dev, port, 1);
}

static int mlx4_ib_netdev_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	struct net_device *dev = ptr;
	struct mlx4_ib_dev *ibdev;
	struct net_device *oldnd;
	struct mlx4_ib_iboe *iboe;
	int port;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	ibdev = container_of(this, struct mlx4_ib_dev, iboe.nb);
	iboe = &ibdev->iboe;

	spin_lock(&iboe->lock);
	mlx4_foreach_ib_transport_port(port, ibdev->dev) {
		oldnd = iboe->netdevs[port - 1];
		iboe->netdevs[port - 1] =
			mlx4_get_protocol_dev(ibdev->dev, MLX4_PROT_ETH, port);
		if (oldnd != iboe->netdevs[port - 1]) {
			if (iboe->netdevs[port - 1])
				netdev_added(ibdev, port);
			else
				netdev_removed(ibdev, port);
		}
	}

	if (dev == iboe->netdevs[0] ||
	    (iboe->netdevs[0] && rdma_vlan_dev_real_dev(dev) == iboe->netdevs[0]))
		handle_en_event(ibdev, 1, event);
	else if (dev == iboe->netdevs[1]
		 || (iboe->netdevs[1] && rdma_vlan_dev_real_dev(dev) == iboe->netdevs[1]))
		handle_en_event(ibdev, 2, event);

	spin_unlock(&iboe->lock);

	return NOTIFY_DONE;
}

static void *mlx4_ib_add(struct mlx4_dev *dev)
{
	struct mlx4_ib_dev *ibdev;
	int num_ports = 0;
	int i;
	int err;
	struct mlx4_ib_iboe *iboe;

	printk_once(KERN_INFO "%s", mlx4_ib_version);

	mlx4_foreach_ib_transport_port(i, dev)
		num_ports++;

	/* No point in registering a device with no ports... */
	if (num_ports == 0)
		return NULL;

	ibdev = (struct mlx4_ib_dev *) ib_alloc_device(sizeof *ibdev);
	if (!ibdev) {
		dev_err(&dev->pdev->dev, "Device struct alloc failed\n");
		return NULL;
	}

	iboe = &ibdev->iboe;

	if (mlx4_pd_alloc(dev, &ibdev->priv_pdn))
		goto err_dealloc;

	if (mlx4_uar_alloc(dev, &ibdev->priv_uar))
		goto err_pd;

	ibdev->uar_map = ioremap((phys_addr_t) ibdev->priv_uar.pfn << PAGE_SHIFT,
				 PAGE_SIZE);
	if (!ibdev->uar_map)
		goto err_uar;
	MLX4_INIT_DOORBELL_LOCK(&ibdev->uar_lock);

	ibdev->dev = dev;

	strlcpy(ibdev->ib_dev.name, "mlx4_%d", IB_DEVICE_NAME_MAX);
	ibdev->ib_dev.owner		= THIS_MODULE;
	ibdev->ib_dev.node_type		= RDMA_NODE_IB_CA;
	ibdev->ib_dev.local_dma_lkey	= dev->caps.reserved_lkey;
	ibdev->num_ports		= num_ports;
	ibdev->ib_dev.phys_port_cnt     = ibdev->num_ports;
	ibdev->ib_dev.num_comp_vectors	= dev->caps.num_comp_vectors;
	ibdev->ib_dev.dma_device	= &dev->pdev->dev;

	ibdev->ib_dev.uverbs_abi_ver	= MLX4_IB_UVERBS_ABI_VERSION;
	ibdev->ib_dev.uverbs_cmd_mask	=
		(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
		(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
		(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
		(1ull << IB_USER_VERBS_CMD_ATTACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_DETACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ);

	ibdev->ib_dev.query_device	= mlx4_ib_query_device;
	ibdev->ib_dev.query_port	= mlx4_ib_query_port;
	ibdev->ib_dev.get_link_layer	= mlx4_ib_port_link_layer;
	ibdev->ib_dev.query_gid		= mlx4_ib_query_gid;
	ibdev->ib_dev.query_pkey	= mlx4_ib_query_pkey;
	ibdev->ib_dev.modify_device	= mlx4_ib_modify_device;
	ibdev->ib_dev.modify_port	= mlx4_ib_modify_port;
	ibdev->ib_dev.alloc_ucontext	= mlx4_ib_alloc_ucontext;
	ibdev->ib_dev.dealloc_ucontext	= mlx4_ib_dealloc_ucontext;
	ibdev->ib_dev.mmap		= mlx4_ib_mmap;
	ibdev->ib_dev.alloc_pd		= mlx4_ib_alloc_pd;
	ibdev->ib_dev.dealloc_pd	= mlx4_ib_dealloc_pd;
	ibdev->ib_dev.create_ah		= mlx4_ib_create_ah;
	ibdev->ib_dev.query_ah		= mlx4_ib_query_ah;
	ibdev->ib_dev.destroy_ah	= mlx4_ib_destroy_ah;
	ibdev->ib_dev.create_srq	= mlx4_ib_create_srq;
	ibdev->ib_dev.modify_srq	= mlx4_ib_modify_srq;
	ibdev->ib_dev.query_srq		= mlx4_ib_query_srq;
	ibdev->ib_dev.destroy_srq	= mlx4_ib_destroy_srq;
	ibdev->ib_dev.post_srq_recv	= mlx4_ib_post_srq_recv;
	ibdev->ib_dev.create_qp		= mlx4_ib_create_qp;
	ibdev->ib_dev.modify_qp		= mlx4_ib_modify_qp;
	ibdev->ib_dev.query_qp		= mlx4_ib_query_qp;
	ibdev->ib_dev.destroy_qp	= mlx4_ib_destroy_qp;
	ibdev->ib_dev.post_send		= mlx4_ib_post_send;
	ibdev->ib_dev.post_recv		= mlx4_ib_post_recv;
	ibdev->ib_dev.create_cq		= mlx4_ib_create_cq;
	ibdev->ib_dev.modify_cq		= mlx4_ib_modify_cq;
	ibdev->ib_dev.resize_cq		= mlx4_ib_resize_cq;
	ibdev->ib_dev.destroy_cq	= mlx4_ib_destroy_cq;
	ibdev->ib_dev.poll_cq		= mlx4_ib_poll_cq;
	ibdev->ib_dev.req_notify_cq	= mlx4_ib_arm_cq;
	ibdev->ib_dev.get_dma_mr	= mlx4_ib_get_dma_mr;
	ibdev->ib_dev.reg_user_mr	= mlx4_ib_reg_user_mr;
	ibdev->ib_dev.dereg_mr		= mlx4_ib_dereg_mr;
	ibdev->ib_dev.alloc_fast_reg_mr = mlx4_ib_alloc_fast_reg_mr;
	ibdev->ib_dev.alloc_fast_reg_page_list = mlx4_ib_alloc_fast_reg_page_list;
	ibdev->ib_dev.free_fast_reg_page_list  = mlx4_ib_free_fast_reg_page_list;
	ibdev->ib_dev.attach_mcast	= mlx4_ib_mcg_attach;
	ibdev->ib_dev.detach_mcast	= mlx4_ib_mcg_detach;
	ibdev->ib_dev.process_mad	= mlx4_ib_process_mad;

	ibdev->ib_dev.alloc_fmr		= mlx4_ib_fmr_alloc;
	ibdev->ib_dev.map_phys_fmr	= mlx4_ib_map_phys_fmr;
	ibdev->ib_dev.unmap_fmr		= mlx4_ib_unmap_fmr;
	ibdev->ib_dev.dealloc_fmr	= mlx4_ib_fmr_dealloc;

	spin_lock_init(&iboe->lock);

	if (init_node_data(ibdev))
		goto err_map;

	spin_lock_init(&ibdev->sm_lock);
	mutex_init(&ibdev->cap_mask_mutex);

	if (ib_register_device(&ibdev->ib_dev, NULL))
		goto err_map;

	if (mlx4_ib_mad_init(ibdev))
		goto err_reg;

	if (dev->caps.flags & MLX4_DEV_CAP_FLAG_IBOE && !iboe->nb.notifier_call) {
		iboe->nb.notifier_call = mlx4_ib_netdev_event;
		err = register_netdevice_notifier(&iboe->nb);
		if (err)
			goto err_reg;
	}

	for (i = 0; i < ARRAY_SIZE(mlx4_class_attributes); ++i) {
		if (device_create_file(&ibdev->ib_dev.dev,
				       mlx4_class_attributes[i]))
			goto err_notif;
	}

	ibdev->ib_active = true;

	return ibdev;

err_notif:
	if (unregister_netdevice_notifier(&ibdev->iboe.nb))
		printk(KERN_WARNING "failure unregistering notifier\n");
	flush_workqueue(wq);

err_reg:
	ib_unregister_device(&ibdev->ib_dev);

err_map:
	iounmap(ibdev->uar_map);

err_uar:
	mlx4_uar_free(dev, &ibdev->priv_uar);

err_pd:
	mlx4_pd_free(dev, ibdev->priv_pdn);

err_dealloc:
	ib_dealloc_device(&ibdev->ib_dev);

	return NULL;
}

static void mlx4_ib_remove(struct mlx4_dev *dev, void *ibdev_ptr)
{
	struct mlx4_ib_dev *ibdev = ibdev_ptr;
	int p;

	mlx4_ib_mad_cleanup(ibdev);
	ib_unregister_device(&ibdev->ib_dev);
	if (ibdev->iboe.nb.notifier_call) {
		if (unregister_netdevice_notifier(&ibdev->iboe.nb))
			printk(KERN_WARNING "failure unregistering notifier\n");
		ibdev->iboe.nb.notifier_call = NULL;
	}
	iounmap(ibdev->uar_map);

	mlx4_foreach_port(p, dev, MLX4_PORT_TYPE_IB)
		mlx4_CLOSE_PORT(dev, p);

	mlx4_uar_free(dev, &ibdev->priv_uar);
	mlx4_pd_free(dev, ibdev->priv_pdn);
	ib_dealloc_device(&ibdev->ib_dev);
}

static void mlx4_ib_event(struct mlx4_dev *dev, void *ibdev_ptr,
			  enum mlx4_dev_event event, int port)
{
	struct ib_event ibev;
	struct mlx4_ib_dev *ibdev = to_mdev((struct ib_device *) ibdev_ptr);

	if (port > ibdev->num_ports)
		return;

	switch (event) {
	case MLX4_DEV_EVENT_PORT_UP:
		ibev.event = IB_EVENT_PORT_ACTIVE;
		break;

	case MLX4_DEV_EVENT_PORT_DOWN:
		ibev.event = IB_EVENT_PORT_ERR;
		break;

	case MLX4_DEV_EVENT_CATASTROPHIC_ERROR:
		ibdev->ib_active = false;
		ibev.event = IB_EVENT_DEVICE_FATAL;
		break;

	default:
		return;
	}

	ibev.device	      = ibdev_ptr;
	ibev.element.port_num = port;

	ib_dispatch_event(&ibev);
}

static struct mlx4_interface mlx4_ib_interface = {
	.add		= mlx4_ib_add,
	.remove		= mlx4_ib_remove,
	.event		= mlx4_ib_event,
	.protocol	= MLX4_PROT_IB_IPV6
};

static int __init mlx4_ib_init(void)
{
	int err;

	wq = create_singlethread_workqueue("mlx4_ib");
	if (!wq)
		return -ENOMEM;

	err = mlx4_register_interface(&mlx4_ib_interface);
	if (err) {
		destroy_workqueue(wq);
		return err;
	}

	return 0;
}

static void __exit mlx4_ib_cleanup(void)
{
	mlx4_unregister_interface(&mlx4_ib_interface);
	destroy_workqueue(wq);
}

module_init(mlx4_ib_init);
module_exit(mlx4_ib_cleanup);
