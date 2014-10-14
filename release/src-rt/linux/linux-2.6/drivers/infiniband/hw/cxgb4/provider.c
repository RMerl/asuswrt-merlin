/*
 * Copyright (c) 2009-2010 Chelsio, Inc. All rights reserved.
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
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/rtnetlink.h>
#include <linux/inetdevice.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/byteorder.h>

#include <rdma/iw_cm.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>

#include "iw_cxgb4.h"

static int fastreg_support = 1;
module_param(fastreg_support, int, 0644);
MODULE_PARM_DESC(fastreg_support, "Advertise fastreg support (default=1)");

static int c4iw_modify_port(struct ib_device *ibdev,
			    u8 port, int port_modify_mask,
			    struct ib_port_modify *props)
{
	return -ENOSYS;
}

static struct ib_ah *c4iw_ah_create(struct ib_pd *pd,
				    struct ib_ah_attr *ah_attr)
{
	return ERR_PTR(-ENOSYS);
}

static int c4iw_ah_destroy(struct ib_ah *ah)
{
	return -ENOSYS;
}

static int c4iw_multicast_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	return -ENOSYS;
}

static int c4iw_multicast_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	return -ENOSYS;
}

static int c4iw_process_mad(struct ib_device *ibdev, int mad_flags,
			    u8 port_num, struct ib_wc *in_wc,
			    struct ib_grh *in_grh, struct ib_mad *in_mad,
			    struct ib_mad *out_mad)
{
	return -ENOSYS;
}

static int c4iw_dealloc_ucontext(struct ib_ucontext *context)
{
	struct c4iw_dev *rhp = to_c4iw_dev(context->device);
	struct c4iw_ucontext *ucontext = to_c4iw_ucontext(context);
	struct c4iw_mm_entry *mm, *tmp;

	PDBG("%s context %p\n", __func__, context);
	list_for_each_entry_safe(mm, tmp, &ucontext->mmaps, entry)
		kfree(mm);
	c4iw_release_dev_ucontext(&rhp->rdev, &ucontext->uctx);
	kfree(ucontext);
	return 0;
}

static struct ib_ucontext *c4iw_alloc_ucontext(struct ib_device *ibdev,
					       struct ib_udata *udata)
{
	struct c4iw_ucontext *context;
	struct c4iw_dev *rhp = to_c4iw_dev(ibdev);

	PDBG("%s ibdev %p\n", __func__, ibdev);
	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);
	c4iw_init_dev_ucontext(&rhp->rdev, &context->uctx);
	INIT_LIST_HEAD(&context->mmaps);
	spin_lock_init(&context->mmap_lock);
	return &context->ibucontext;
}

static int c4iw_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	int len = vma->vm_end - vma->vm_start;
	u32 key = vma->vm_pgoff << PAGE_SHIFT;
	struct c4iw_rdev *rdev;
	int ret = 0;
	struct c4iw_mm_entry *mm;
	struct c4iw_ucontext *ucontext;
	u64 addr;

	PDBG("%s pgoff 0x%lx key 0x%x len %d\n", __func__, vma->vm_pgoff,
	     key, len);

	if (vma->vm_start & (PAGE_SIZE-1))
		return -EINVAL;

	rdev = &(to_c4iw_dev(context->device)->rdev);
	ucontext = to_c4iw_ucontext(context);

	mm = remove_mmap(ucontext, key, len);
	if (!mm)
		return -EINVAL;
	addr = mm->addr;
	kfree(mm);

	if ((addr >= pci_resource_start(rdev->lldi.pdev, 0)) &&
	    (addr < (pci_resource_start(rdev->lldi.pdev, 0) +
		    pci_resource_len(rdev->lldi.pdev, 0)))) {

		/*
		 * MA_SYNC register...
		 */
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = io_remap_pfn_range(vma, vma->vm_start,
					 addr >> PAGE_SHIFT,
					 len, vma->vm_page_prot);
	} else if ((addr >= pci_resource_start(rdev->lldi.pdev, 2)) &&
		   (addr < (pci_resource_start(rdev->lldi.pdev, 2) +
		    pci_resource_len(rdev->lldi.pdev, 2)))) {

		/*
		 * Map user DB or OCQP memory...
		 */
		if (addr >= rdev->oc_mw_pa)
			vma->vm_page_prot = t4_pgprot_wc(vma->vm_page_prot);
		else
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = io_remap_pfn_range(vma, vma->vm_start,
					 addr >> PAGE_SHIFT,
					 len, vma->vm_page_prot);
	} else {

		/*
		 * Map WQ or CQ contig dma memory...
		 */
		ret = remap_pfn_range(vma, vma->vm_start,
				      addr >> PAGE_SHIFT,
				      len, vma->vm_page_prot);
	}

	return ret;
}

static int c4iw_deallocate_pd(struct ib_pd *pd)
{
	struct c4iw_dev *rhp;
	struct c4iw_pd *php;

	php = to_c4iw_pd(pd);
	rhp = php->rhp;
	PDBG("%s ibpd %p pdid 0x%x\n", __func__, pd, php->pdid);
	c4iw_put_resource(&rhp->rdev.resource.pdid_fifo, php->pdid,
			  &rhp->rdev.resource.pdid_fifo_lock);
	kfree(php);
	return 0;
}

static struct ib_pd *c4iw_allocate_pd(struct ib_device *ibdev,
				      struct ib_ucontext *context,
				      struct ib_udata *udata)
{
	struct c4iw_pd *php;
	u32 pdid;
	struct c4iw_dev *rhp;

	PDBG("%s ibdev %p\n", __func__, ibdev);
	rhp = (struct c4iw_dev *) ibdev;
	pdid =  c4iw_get_resource(&rhp->rdev.resource.pdid_fifo,
				  &rhp->rdev.resource.pdid_fifo_lock);
	if (!pdid)
		return ERR_PTR(-EINVAL);
	php = kzalloc(sizeof(*php), GFP_KERNEL);
	if (!php) {
		c4iw_put_resource(&rhp->rdev.resource.pdid_fifo, pdid,
				  &rhp->rdev.resource.pdid_fifo_lock);
		return ERR_PTR(-ENOMEM);
	}
	php->pdid = pdid;
	php->rhp = rhp;
	if (context) {
		if (ib_copy_to_udata(udata, &php->pdid, sizeof(u32))) {
			c4iw_deallocate_pd(&php->ibpd);
			return ERR_PTR(-EFAULT);
		}
	}
	PDBG("%s pdid 0x%0x ptr 0x%p\n", __func__, pdid, php);
	return &php->ibpd;
}

static int c4iw_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
			   u16 *pkey)
{
	PDBG("%s ibdev %p\n", __func__, ibdev);
	*pkey = 0;
	return 0;
}

static int c4iw_query_gid(struct ib_device *ibdev, u8 port, int index,
			  union ib_gid *gid)
{
	struct c4iw_dev *dev;

	PDBG("%s ibdev %p, port %d, index %d, gid %p\n",
	       __func__, ibdev, port, index, gid);
	dev = to_c4iw_dev(ibdev);
	BUG_ON(port == 0);
	memset(&(gid->raw[0]), 0, sizeof(gid->raw));
	memcpy(&(gid->raw[0]), dev->rdev.lldi.ports[port-1]->dev_addr, 6);
	return 0;
}

static int c4iw_query_device(struct ib_device *ibdev,
			     struct ib_device_attr *props)
{

	struct c4iw_dev *dev;
	PDBG("%s ibdev %p\n", __func__, ibdev);

	dev = to_c4iw_dev(ibdev);
	memset(props, 0, sizeof *props);
	memcpy(&props->sys_image_guid, dev->rdev.lldi.ports[0]->dev_addr, 6);
	props->hw_ver = dev->rdev.lldi.adapter_type;
	props->fw_ver = dev->rdev.lldi.fw_vers;
	props->device_cap_flags = dev->device_cap_flags;
	props->page_size_cap = T4_PAGESIZE_MASK;
	props->vendor_id = (u32)dev->rdev.lldi.pdev->vendor;
	props->vendor_part_id = (u32)dev->rdev.lldi.pdev->device;
	props->max_mr_size = T4_MAX_MR_SIZE;
	props->max_qp = T4_MAX_NUM_QP;
	props->max_qp_wr = T4_MAX_QP_DEPTH;
	props->max_sge = T4_MAX_RECV_SGE;
	props->max_sge_rd = 1;
	props->max_qp_rd_atom = c4iw_max_read_depth;
	props->max_qp_init_rd_atom = c4iw_max_read_depth;
	props->max_cq = T4_MAX_NUM_CQ;
	props->max_cqe = T4_MAX_CQ_DEPTH;
	props->max_mr = c4iw_num_stags(&dev->rdev);
	props->max_pd = T4_MAX_NUM_PD;
	props->local_ca_ack_delay = 0;
	props->max_fast_reg_page_list_len = T4_MAX_FR_DEPTH;

	return 0;
}

static int c4iw_query_port(struct ib_device *ibdev, u8 port,
			   struct ib_port_attr *props)
{
	struct c4iw_dev *dev;
	struct net_device *netdev;
	struct in_device *inetdev;

	PDBG("%s ibdev %p\n", __func__, ibdev);

	dev = to_c4iw_dev(ibdev);
	netdev = dev->rdev.lldi.ports[port-1];

	memset(props, 0, sizeof(struct ib_port_attr));
	props->max_mtu = IB_MTU_4096;
	if (netdev->mtu >= 4096)
		props->active_mtu = IB_MTU_4096;
	else if (netdev->mtu >= 2048)
		props->active_mtu = IB_MTU_2048;
	else if (netdev->mtu >= 1024)
		props->active_mtu = IB_MTU_1024;
	else if (netdev->mtu >= 512)
		props->active_mtu = IB_MTU_512;
	else
		props->active_mtu = IB_MTU_256;

	if (!netif_carrier_ok(netdev))
		props->state = IB_PORT_DOWN;
	else {
		inetdev = in_dev_get(netdev);
		if (inetdev) {
			if (inetdev->ifa_list)
				props->state = IB_PORT_ACTIVE;
			else
				props->state = IB_PORT_INIT;
			in_dev_put(inetdev);
		} else
			props->state = IB_PORT_INIT;
	}

	props->port_cap_flags =
	    IB_PORT_CM_SUP |
	    IB_PORT_SNMP_TUNNEL_SUP |
	    IB_PORT_REINIT_SUP |
	    IB_PORT_DEVICE_MGMT_SUP |
	    IB_PORT_VENDOR_CLASS_SUP | IB_PORT_BOOT_MGMT_SUP;
	props->gid_tbl_len = 1;
	props->pkey_tbl_len = 1;
	props->active_width = 2;
	props->active_speed = 2;
	props->max_msg_sz = -1;

	return 0;
}

static ssize_t show_rev(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct c4iw_dev *c4iw_dev = container_of(dev, struct c4iw_dev,
						 ibdev.dev);
	PDBG("%s dev 0x%p\n", __func__, dev);
	return sprintf(buf, "%d\n", c4iw_dev->rdev.lldi.adapter_type);
}

static ssize_t show_fw_ver(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct c4iw_dev *c4iw_dev = container_of(dev, struct c4iw_dev,
						 ibdev.dev);
	PDBG("%s dev 0x%p\n", __func__, dev);

	return sprintf(buf, "%u.%u.%u.%u\n",
			FW_HDR_FW_VER_MAJOR_GET(c4iw_dev->rdev.lldi.fw_vers),
			FW_HDR_FW_VER_MINOR_GET(c4iw_dev->rdev.lldi.fw_vers),
			FW_HDR_FW_VER_MICRO_GET(c4iw_dev->rdev.lldi.fw_vers),
			FW_HDR_FW_VER_BUILD_GET(c4iw_dev->rdev.lldi.fw_vers));
}

static ssize_t show_hca(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct c4iw_dev *c4iw_dev = container_of(dev, struct c4iw_dev,
						 ibdev.dev);
	struct ethtool_drvinfo info;
	struct net_device *lldev = c4iw_dev->rdev.lldi.ports[0];

	PDBG("%s dev 0x%p\n", __func__, dev);
	lldev->ethtool_ops->get_drvinfo(lldev, &info);
	return sprintf(buf, "%s\n", info.driver);
}

static ssize_t show_board(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct c4iw_dev *c4iw_dev = container_of(dev, struct c4iw_dev,
						 ibdev.dev);
	PDBG("%s dev 0x%p\n", __func__, dev);
	return sprintf(buf, "%x.%x\n", c4iw_dev->rdev.lldi.pdev->vendor,
		       c4iw_dev->rdev.lldi.pdev->device);
}

static int c4iw_get_mib(struct ib_device *ibdev,
			union rdma_protocol_stats *stats)
{
	struct tp_tcp_stats v4, v6;
	struct c4iw_dev *c4iw_dev = to_c4iw_dev(ibdev);

	cxgb4_get_tcp_stats(c4iw_dev->rdev.lldi.pdev, &v4, &v6);
	memset(stats, 0, sizeof *stats);
	stats->iw.tcpInSegs = v4.tcpInSegs + v6.tcpInSegs;
	stats->iw.tcpOutSegs = v4.tcpOutSegs + v6.tcpOutSegs;
	stats->iw.tcpRetransSegs = v4.tcpRetransSegs + v6.tcpRetransSegs;
	stats->iw.tcpOutRsts = v4.tcpOutRsts + v6.tcpOutSegs;

	return 0;
}

static DEVICE_ATTR(hw_rev, S_IRUGO, show_rev, NULL);
static DEVICE_ATTR(fw_ver, S_IRUGO, show_fw_ver, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca, NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board, NULL);

static struct device_attribute *c4iw_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_fw_ver,
	&dev_attr_hca_type,
	&dev_attr_board_id,
};

int c4iw_register_device(struct c4iw_dev *dev)
{
	int ret;
	int i;

	PDBG("%s c4iw_dev %p\n", __func__, dev);
	BUG_ON(!dev->rdev.lldi.ports[0]);
	strlcpy(dev->ibdev.name, "cxgb4_%d", IB_DEVICE_NAME_MAX);
	memset(&dev->ibdev.node_guid, 0, sizeof(dev->ibdev.node_guid));
	memcpy(&dev->ibdev.node_guid, dev->rdev.lldi.ports[0]->dev_addr, 6);
	dev->ibdev.owner = THIS_MODULE;
	dev->device_cap_flags = IB_DEVICE_LOCAL_DMA_LKEY | IB_DEVICE_MEM_WINDOW;
	if (fastreg_support)
		dev->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	dev->ibdev.local_dma_lkey = 0;
	dev->ibdev.uverbs_cmd_mask =
	    (1ull << IB_USER_VERBS_CMD_GET_CONTEXT) |
	    (1ull << IB_USER_VERBS_CMD_QUERY_DEVICE) |
	    (1ull << IB_USER_VERBS_CMD_QUERY_PORT) |
	    (1ull << IB_USER_VERBS_CMD_ALLOC_PD) |
	    (1ull << IB_USER_VERBS_CMD_DEALLOC_PD) |
	    (1ull << IB_USER_VERBS_CMD_REG_MR) |
	    (1ull << IB_USER_VERBS_CMD_DEREG_MR) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_CQ) |
	    (1ull << IB_USER_VERBS_CMD_DESTROY_CQ) |
	    (1ull << IB_USER_VERBS_CMD_REQ_NOTIFY_CQ) |
	    (1ull << IB_USER_VERBS_CMD_CREATE_QP) |
	    (1ull << IB_USER_VERBS_CMD_MODIFY_QP) |
	    (1ull << IB_USER_VERBS_CMD_POLL_CQ) |
	    (1ull << IB_USER_VERBS_CMD_DESTROY_QP) |
	    (1ull << IB_USER_VERBS_CMD_POST_SEND) |
	    (1ull << IB_USER_VERBS_CMD_POST_RECV);
	dev->ibdev.node_type = RDMA_NODE_RNIC;
	memcpy(dev->ibdev.node_desc, C4IW_NODE_DESC, sizeof(C4IW_NODE_DESC));
	dev->ibdev.phys_port_cnt = dev->rdev.lldi.nports;
	dev->ibdev.num_comp_vectors = 1;
	dev->ibdev.dma_device = &(dev->rdev.lldi.pdev->dev);
	dev->ibdev.query_device = c4iw_query_device;
	dev->ibdev.query_port = c4iw_query_port;
	dev->ibdev.modify_port = c4iw_modify_port;
	dev->ibdev.query_pkey = c4iw_query_pkey;
	dev->ibdev.query_gid = c4iw_query_gid;
	dev->ibdev.alloc_ucontext = c4iw_alloc_ucontext;
	dev->ibdev.dealloc_ucontext = c4iw_dealloc_ucontext;
	dev->ibdev.mmap = c4iw_mmap;
	dev->ibdev.alloc_pd = c4iw_allocate_pd;
	dev->ibdev.dealloc_pd = c4iw_deallocate_pd;
	dev->ibdev.create_ah = c4iw_ah_create;
	dev->ibdev.destroy_ah = c4iw_ah_destroy;
	dev->ibdev.create_qp = c4iw_create_qp;
	dev->ibdev.modify_qp = c4iw_ib_modify_qp;
	dev->ibdev.destroy_qp = c4iw_destroy_qp;
	dev->ibdev.create_cq = c4iw_create_cq;
	dev->ibdev.destroy_cq = c4iw_destroy_cq;
	dev->ibdev.resize_cq = c4iw_resize_cq;
	dev->ibdev.poll_cq = c4iw_poll_cq;
	dev->ibdev.get_dma_mr = c4iw_get_dma_mr;
	dev->ibdev.reg_phys_mr = c4iw_register_phys_mem;
	dev->ibdev.rereg_phys_mr = c4iw_reregister_phys_mem;
	dev->ibdev.reg_user_mr = c4iw_reg_user_mr;
	dev->ibdev.dereg_mr = c4iw_dereg_mr;
	dev->ibdev.alloc_mw = c4iw_alloc_mw;
	dev->ibdev.bind_mw = c4iw_bind_mw;
	dev->ibdev.dealloc_mw = c4iw_dealloc_mw;
	dev->ibdev.alloc_fast_reg_mr = c4iw_alloc_fast_reg_mr;
	dev->ibdev.alloc_fast_reg_page_list = c4iw_alloc_fastreg_pbl;
	dev->ibdev.free_fast_reg_page_list = c4iw_free_fastreg_pbl;
	dev->ibdev.attach_mcast = c4iw_multicast_attach;
	dev->ibdev.detach_mcast = c4iw_multicast_detach;
	dev->ibdev.process_mad = c4iw_process_mad;
	dev->ibdev.req_notify_cq = c4iw_arm_cq;
	dev->ibdev.post_send = c4iw_post_send;
	dev->ibdev.post_recv = c4iw_post_receive;
	dev->ibdev.get_protocol_stats = c4iw_get_mib;
	dev->ibdev.uverbs_abi_ver = C4IW_UVERBS_ABI_VERSION;

	dev->ibdev.iwcm = kmalloc(sizeof(struct iw_cm_verbs), GFP_KERNEL);
	if (!dev->ibdev.iwcm)
		return -ENOMEM;

	dev->ibdev.iwcm->connect = c4iw_connect;
	dev->ibdev.iwcm->accept = c4iw_accept_cr;
	dev->ibdev.iwcm->reject = c4iw_reject_cr;
	dev->ibdev.iwcm->create_listen = c4iw_create_listen;
	dev->ibdev.iwcm->destroy_listen = c4iw_destroy_listen;
	dev->ibdev.iwcm->add_ref = c4iw_qp_add_ref;
	dev->ibdev.iwcm->rem_ref = c4iw_qp_rem_ref;
	dev->ibdev.iwcm->get_qp = c4iw_get_qp;

	ret = ib_register_device(&dev->ibdev, NULL);
	if (ret)
		goto bail1;

	for (i = 0; i < ARRAY_SIZE(c4iw_class_attributes); ++i) {
		ret = device_create_file(&dev->ibdev.dev,
					 c4iw_class_attributes[i]);
		if (ret)
			goto bail2;
	}
	dev->registered = 1;
	return 0;
bail2:
	ib_unregister_device(&dev->ibdev);
bail1:
	kfree(dev->ibdev.iwcm);
	return ret;
}

void c4iw_unregister_device(struct c4iw_dev *dev)
{
	int i;

	PDBG("%s c4iw_dev %p\n", __func__, dev);
	for (i = 0; i < ARRAY_SIZE(c4iw_class_attributes); ++i)
		device_remove_file(&dev->ibdev.dev,
				   c4iw_class_attributes[i]);
	ib_unregister_device(&dev->ibdev);
	kfree(dev->ibdev.iwcm);
	dev->registered = 0;
	return;
}
