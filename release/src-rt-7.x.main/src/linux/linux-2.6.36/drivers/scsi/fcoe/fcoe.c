/*
 * Copyright(c) 2007 - 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <net/rtnetlink.h>

#include <scsi/fc/fc_encaps.h>
#include <scsi/fc/fc_fip.h>

#include <scsi/libfc.h>
#include <scsi/fc_frame.h>
#include <scsi/libfcoe.h>

#include "fcoe.h"

MODULE_AUTHOR("Open-FCoE.org");
MODULE_DESCRIPTION("FCoE");
MODULE_LICENSE("GPL v2");

/* Performance tuning parameters for fcoe */
static unsigned int fcoe_ddp_min;
module_param_named(ddp_min, fcoe_ddp_min, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ddp_min, "Minimum I/O size in bytes for "	\
		 "Direct Data Placement (DDP).");

DEFINE_MUTEX(fcoe_config_mutex);

/* fcoe_percpu_clean completion.  Waiter protected by fcoe_create_mutex */
static DECLARE_COMPLETION(fcoe_flush_completion);

/* fcoe host list */
/* must only by accessed under the RTNL mutex */
LIST_HEAD(fcoe_hostlist);
DEFINE_PER_CPU(struct fcoe_percpu_s, fcoe_percpu);

/* Function Prototypes */
static int fcoe_reset(struct Scsi_Host *);
static int fcoe_xmit(struct fc_lport *, struct fc_frame *);
static int fcoe_rcv(struct sk_buff *, struct net_device *,
		    struct packet_type *, struct net_device *);
static int fcoe_percpu_receive_thread(void *);
static void fcoe_clean_pending_queue(struct fc_lport *);
static void fcoe_percpu_clean(struct fc_lport *);
static int fcoe_link_speed_update(struct fc_lport *);
static int fcoe_link_ok(struct fc_lport *);

static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *);
static int fcoe_hostlist_add(const struct fc_lport *);

static void fcoe_check_wait_queue(struct fc_lport *, struct sk_buff *);
static int fcoe_device_notification(struct notifier_block *, ulong, void *);
static void fcoe_dev_setup(void);
static void fcoe_dev_cleanup(void);
static struct fcoe_interface
*fcoe_hostlist_lookup_port(const struct net_device *);

static int fcoe_fip_recv(struct sk_buff *, struct net_device *,
			 struct packet_type *, struct net_device *);

static void fcoe_fip_send(struct fcoe_ctlr *, struct sk_buff *);
static void fcoe_update_src_mac(struct fc_lport *, u8 *);
static u8 *fcoe_get_src_mac(struct fc_lport *);
static void fcoe_destroy_work(struct work_struct *);

static int fcoe_ddp_setup(struct fc_lport *, u16, struct scatterlist *,
			  unsigned int);
static int fcoe_ddp_done(struct fc_lport *, u16);

static int fcoe_cpu_callback(struct notifier_block *, unsigned long, void *);

static int fcoe_create(const char *, struct kernel_param *);
static int fcoe_destroy(const char *, struct kernel_param *);
static int fcoe_enable(const char *, struct kernel_param *);
static int fcoe_disable(const char *, struct kernel_param *);

static struct fc_seq *fcoe_elsct_send(struct fc_lport *,
				      u32 did, struct fc_frame *,
				      unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *, u32 timeout);
static void fcoe_recv_frame(struct sk_buff *skb);

static void fcoe_get_lesb(struct fc_lport *, struct fc_els_lesb *);

module_param_call(create, fcoe_create, NULL, (void *)FIP_MODE_AUTO, S_IWUSR);
__MODULE_PARM_TYPE(create, "string");
MODULE_PARM_DESC(create, " Creates fcoe instance on a ethernet interface");
module_param_call(create_vn2vn, fcoe_create, NULL,
		  (void *)FIP_MODE_VN2VN, S_IWUSR);
__MODULE_PARM_TYPE(create_vn2vn, "string");
MODULE_PARM_DESC(create_vn2vn, " Creates a VN_node to VN_node FCoE instance "
		 "on an Ethernet interface");
module_param_call(destroy, fcoe_destroy, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(destroy, "string");
MODULE_PARM_DESC(destroy, " Destroys fcoe instance on a ethernet interface");
module_param_call(enable, fcoe_enable, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(enable, "string");
MODULE_PARM_DESC(enable, " Enables fcoe on a ethernet interface.");
module_param_call(disable, fcoe_disable, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(disable, "string");
MODULE_PARM_DESC(disable, " Disables fcoe on a ethernet interface.");

/* notification function for packets from net device */
static struct notifier_block fcoe_notifier = {
	.notifier_call = fcoe_device_notification,
};

/* notification function for CPU hotplug events */
static struct notifier_block fcoe_cpu_notifier = {
	.notifier_call = fcoe_cpu_callback,
};

static struct scsi_transport_template *fcoe_transport_template;
static struct scsi_transport_template *fcoe_vport_transport_template;

static int fcoe_vport_destroy(struct fc_vport *);
static int fcoe_vport_create(struct fc_vport *, bool disabled);
static int fcoe_vport_disable(struct fc_vport *, bool disable);
static void fcoe_set_vport_symbolic_name(struct fc_vport *);
static void fcoe_set_port_id(struct fc_lport *, u32, struct fc_frame *);

static struct libfc_function_template fcoe_libfc_fcn_templ = {
	.frame_send = fcoe_xmit,
	.ddp_setup = fcoe_ddp_setup,
	.ddp_done = fcoe_ddp_done,
	.elsct_send = fcoe_elsct_send,
	.get_lesb = fcoe_get_lesb,
	.lport_set_port_id = fcoe_set_port_id,
};

struct fc_function_template fcoe_transport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

	.vport_create = fcoe_vport_create,
	.vport_delete = fcoe_vport_destroy,
	.vport_disable = fcoe_vport_disable,
	.set_vport_symbolic_name = fcoe_set_vport_symbolic_name,

	.bsg_request = fc_lport_bsg_request,
};

struct fc_function_template fcoe_vport_transport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

	.bsg_request = fc_lport_bsg_request,
};

static struct scsi_host_template fcoe_shost_template = {
	.module = THIS_MODULE,
	.name = "FCoE Driver",
	.proc_name = FCOE_NAME,
	.queuecommand = fc_queuecommand,
	.eh_abort_handler = fc_eh_abort,
	.eh_device_reset_handler = fc_eh_device_reset,
	.eh_host_reset_handler = fc_eh_host_reset,
	.slave_alloc = fc_slave_alloc,
	.change_queue_depth = fc_change_queue_depth,
	.change_queue_type = fc_change_queue_type,
	.this_id = -1,
	.cmd_per_lun = 3,
	.can_queue = FCOE_MAX_OUTSTANDING_COMMANDS,
	.use_clustering = ENABLE_CLUSTERING,
	.sg_tablesize = SG_ALL,
	.max_sectors = 0xffff,
};

/**
 * fcoe_interface_setup() - Setup a FCoE interface
 * @fcoe:   The new FCoE interface
 * @netdev: The net device that the fcoe interface is on
 *
 * Returns : 0 for success
 * Locking: must be called with the RTNL mutex held
 */
static int fcoe_interface_setup(struct fcoe_interface *fcoe,
				struct net_device *netdev)
{
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct netdev_hw_addr *ha;
	struct net_device *real_dev;
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	fcoe->netdev = netdev;

	/* Let LLD initialize for FCoE */
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_enable) {
		if (ops->ndo_fcoe_enable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to enable FCoE"
					" specific feature for LLD.\n");
	}

	/* Do not support for bonding device */
	if ((netdev->priv_flags & IFF_MASTER_ALB) ||
	    (netdev->priv_flags & IFF_SLAVE_INACTIVE) ||
	    (netdev->priv_flags & IFF_MASTER_8023AD)) {
		FCOE_NETDEV_DBG(netdev, "Bonded interfaces not supported\n");
		return -EOPNOTSUPP;
	}

	/* look for SAN MAC address, if multiple SAN MACs exist, only
	 * use the first one for SPMA */
	real_dev = (netdev->priv_flags & IFF_802_1Q_VLAN) ?
		vlan_dev_real_dev(netdev) : netdev;
	rcu_read_lock();
	for_each_dev_addr(real_dev, ha) {
		if ((ha->type == NETDEV_HW_ADDR_T_SAN) &&
		    (is_valid_ether_addr(ha->addr))) {
			memcpy(fip->ctl_src_addr, ha->addr, ETH_ALEN);
			fip->spma = 1;
			break;
		}
	}
	rcu_read_unlock();

	/* setup Source Mac Address */
	if (!fip->spma)
		memcpy(fip->ctl_src_addr, netdev->dev_addr, netdev->addr_len);

	/*
	 * Add FCoE MAC address as second unicast MAC address
	 * or enter promiscuous mode if not capable of listening
	 * for multiple unicast MACs.
	 */
	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_uc_add(netdev, flogi_maddr);
	if (fip->spma)
		dev_uc_add(netdev, fip->ctl_src_addr);
	if (fip->mode == FIP_MODE_VN2VN) {
		dev_mc_add(netdev, FIP_ALL_VN2VN_MACS);
		dev_mc_add(netdev, FIP_ALL_P2P_MACS);
	} else
		dev_mc_add(netdev, FIP_ALL_ENODE_MACS);

	/*
	 * setup the receive function from ethernet driver
	 * on the ethertype for the given device
	 */
	fcoe->fcoe_packet_type.func = fcoe_rcv;
	fcoe->fcoe_packet_type.type = __constant_htons(ETH_P_FCOE);
	fcoe->fcoe_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fcoe_packet_type);

	fcoe->fip_packet_type.func = fcoe_fip_recv;
	fcoe->fip_packet_type.type = htons(ETH_P_FIP);
	fcoe->fip_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fip_packet_type);

	return 0;
}

/**
 * fcoe_interface_create() - Create a FCoE interface on a net device
 * @netdev: The net device to create the FCoE interface on
 * @fip_mode: The mode to use for FIP
 *
 * Returns: pointer to a struct fcoe_interface or NULL on error
 */
static struct fcoe_interface *fcoe_interface_create(struct net_device *netdev,
						    enum fip_state fip_mode)
{
	struct fcoe_interface *fcoe;
	int err;

	fcoe = kzalloc(sizeof(*fcoe), GFP_KERNEL);
	if (!fcoe) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate fcoe structure\n");
		return NULL;
	}

	dev_hold(netdev);
	kref_init(&fcoe->kref);

	/*
	 * Initialize FIP.
	 */
	fcoe_ctlr_init(&fcoe->ctlr, fip_mode);
	fcoe->ctlr.send = fcoe_fip_send;
	fcoe->ctlr.update_mac = fcoe_update_src_mac;
	fcoe->ctlr.get_src_addr = fcoe_get_src_mac;

	err = fcoe_interface_setup(fcoe, netdev);
	if (err) {
		fcoe_ctlr_destroy(&fcoe->ctlr);
		kfree(fcoe);
		dev_put(netdev);
		return NULL;
	}

	return fcoe;
}

/**
 * fcoe_interface_cleanup() - Clean up a FCoE interface
 * @fcoe: The FCoE interface to be cleaned up
 *
 * Caller must be holding the RTNL mutex
 */
void fcoe_interface_cleanup(struct fcoe_interface *fcoe)
{
	struct net_device *netdev = fcoe->netdev;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	/*
	 * Don't listen for Ethernet packets anymore.
	 * synchronize_net() ensures that the packet handlers are not running
	 * on another CPU. dev_remove_pack() would do that, this calls the
	 * unsyncronized version __dev_remove_pack() to avoid multiple delays.
	 */
	__dev_remove_pack(&fcoe->fcoe_packet_type);
	__dev_remove_pack(&fcoe->fip_packet_type);
	synchronize_net();

	/* Delete secondary MAC addresses */
	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_uc_del(netdev, flogi_maddr);
	if (fip->spma)
		dev_uc_del(netdev, fip->ctl_src_addr);
	if (fip->mode == FIP_MODE_VN2VN) {
		dev_mc_del(netdev, FIP_ALL_VN2VN_MACS);
		dev_mc_del(netdev, FIP_ALL_P2P_MACS);
	} else
		dev_mc_del(netdev, FIP_ALL_ENODE_MACS);

	/* Tell the LLD we are done w/ FCoE */
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_disable) {
		if (ops->ndo_fcoe_disable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to disable FCoE"
					" specific feature for LLD.\n");
	}
}

/**
 * fcoe_interface_release() - fcoe_port kref release function
 * @kref: Embedded reference count in an fcoe_interface struct
 */
static void fcoe_interface_release(struct kref *kref)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;

	fcoe = container_of(kref, struct fcoe_interface, kref);
	netdev = fcoe->netdev;
	/* tear-down the FCoE controller */
	fcoe_ctlr_destroy(&fcoe->ctlr);
	kfree(fcoe);
	dev_put(netdev);
}

/**
 * fcoe_interface_get() - Get a reference to a FCoE interface
 * @fcoe: The FCoE interface to be held
 */
static inline void fcoe_interface_get(struct fcoe_interface *fcoe)
{
	kref_get(&fcoe->kref);
}

/**
 * fcoe_interface_put() - Put a reference to a FCoE interface
 * @fcoe: The FCoE interface to be released
 */
static inline void fcoe_interface_put(struct fcoe_interface *fcoe)
{
	kref_put(&fcoe->kref, fcoe_interface_release);
}

/**
 * fcoe_fip_recv() - Handler for received FIP frames
 * @skb:      The receive skb
 * @netdev:   The associated net device
 * @ptype:    The packet_type structure which was used to register this handler
 * @orig_dev: The original net_device the the skb was received on.
 *	      (in case dev is a bond)
 *
 * Returns: 0 for success
 */
static int fcoe_fip_recv(struct sk_buff *skb, struct net_device *netdev,
			 struct packet_type *ptype,
			 struct net_device *orig_dev)
{
	struct fcoe_interface *fcoe;

	fcoe = container_of(ptype, struct fcoe_interface, fip_packet_type);
	fcoe_ctlr_recv(&fcoe->ctlr, skb);
	return 0;
}

/**
 * fcoe_fip_send() - Send an Ethernet-encapsulated FIP frame
 * @fip: The FCoE controller
 * @skb: The FIP packet to be sent
 */
static void fcoe_fip_send(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	skb->dev = fcoe_from_ctlr(fip)->netdev;
	dev_queue_xmit(skb);
}

/**
 * fcoe_update_src_mac() - Update the Ethernet MAC filters
 * @lport: The local port to update the source MAC on
 * @addr:  Unicast MAC address to add
 *
 * Remove any previously-set unicast MAC filter.
 * Add secondary FCoE MAC address filter for our OUI.
 */
static void fcoe_update_src_mac(struct fc_lport *lport, u8 *addr)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;

	rtnl_lock();
	if (!is_zero_ether_addr(port->data_src_addr))
		dev_uc_del(fcoe->netdev, port->data_src_addr);
	if (!is_zero_ether_addr(addr))
		dev_uc_add(fcoe->netdev, addr);
	memcpy(port->data_src_addr, addr, ETH_ALEN);
	rtnl_unlock();
}

/**
 * fcoe_get_src_mac() - return the Ethernet source address for an lport
 * @lport: libfc lport
 */
static u8 *fcoe_get_src_mac(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);

	return port->data_src_addr;
}

/**
 * fcoe_lport_config() - Set up a local port
 * @lport: The local port to be setup
 *
 * Returns: 0 for success
 */
static int fcoe_lport_config(struct fc_lport *lport)
{
	lport->link_up = 0;
	lport->qfull = 0;
	lport->max_retry_count = 3;
	lport->max_rport_retry_count = 3;
	lport->e_d_tov = 2 * 1000;	/* FC-FS default */
	lport->r_a_tov = 2 * 2 * 1000;
	lport->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS |
				 FCP_SPPF_RETRY | FCP_SPPF_CONF_COMPL);
	lport->does_npiv = 1;

	fc_lport_init_stats(lport);

	/* lport fc_lport related configuration */
	fc_lport_config(lport);

	/* offload related configuration */
	lport->crc_offload = 0;
	lport->seq_offload = 0;
	lport->lro_enabled = 0;
	lport->lro_xid = 0;
	lport->lso_max = 0;

	return 0;
}

/**
 * fcoe_queue_timer() - The fcoe queue timer
 * @lport: The local port
 *
 * Calls fcoe_check_wait_queue on timeout
 */
static void fcoe_queue_timer(ulong lport)
{
	fcoe_check_wait_queue((struct fc_lport *)lport, NULL);
}

/**
 * fcoe_get_wwn() - Get the world wide name from LLD if it supports it
 * @netdev: the associated net device
 * @wwn: the output WWN
 * @type: the type of WWN (WWPN or WWNN)
 *
 * Returns: 0 for success
 */
static int fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type)
{
	const struct net_device_ops *ops = netdev->netdev_ops;

	if (ops->ndo_fcoe_get_wwn)
		return ops->ndo_fcoe_get_wwn(netdev, wwn, type);
	return -EINVAL;
}

/**
 * fcoe_netdev_features_change - Updates the lport's offload flags based
 * on the LLD netdev's FCoE feature flags
 */
static void fcoe_netdev_features_change(struct fc_lport *lport,
					struct net_device *netdev)
{
	mutex_lock(&lport->lp_mutex);

	if (netdev->features & NETIF_F_SG)
		lport->sg_supp = 1;
	else
		lport->sg_supp = 0;

	if (netdev->features & NETIF_F_FCOE_CRC) {
		lport->crc_offload = 1;
		FCOE_NETDEV_DBG(netdev, "Supports FCCRC offload\n");
	} else {
		lport->crc_offload = 0;
	}

	if (netdev->features & NETIF_F_FSO) {
		lport->seq_offload = 1;
		lport->lso_max = netdev->gso_max_size;
		FCOE_NETDEV_DBG(netdev, "Supports LSO for max len 0x%x\n",
				lport->lso_max);
	} else {
		lport->seq_offload = 0;
		lport->lso_max = 0;
	}

	if (netdev->fcoe_ddp_xid) {
		lport->lro_enabled = 1;
		lport->lro_xid = netdev->fcoe_ddp_xid;
		FCOE_NETDEV_DBG(netdev, "Supports LRO for max xid 0x%x\n",
				lport->lro_xid);
	} else {
		lport->lro_enabled = 0;
		lport->lro_xid = 0;
	}

	mutex_unlock(&lport->lp_mutex);
}

/**
 * fcoe_netdev_config() - Set up net devive for SW FCoE
 * @lport:  The local port that is associated with the net device
 * @netdev: The associated net device
 *
 * Must be called after fcoe_lport_config() as it will use local port mutex
 *
 * Returns: 0 for success
 */
static int fcoe_netdev_config(struct fc_lport *lport, struct net_device *netdev)
{
	u32 mfs;
	u64 wwnn, wwpn;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	/* Setup lport private data to point to fcoe softc */
	port = lport_priv(lport);
	fcoe = port->fcoe;

	/*
	 * Determine max frame size based on underlying device and optional
	 * user-configured limit.  If the MFS is too low, fcoe_link_ok()
	 * will return 0, so do this first.
	 */
	mfs = netdev->mtu;
	if (netdev->features & NETIF_F_FCOE_MTU) {
		mfs = FCOE_MTU;
		FCOE_NETDEV_DBG(netdev, "Supports FCOE_MTU of %d bytes\n", mfs);
	}
	mfs -= (sizeof(struct fcoe_hdr) + sizeof(struct fcoe_crc_eof));
	if (fc_set_mfs(lport, mfs))
		return -EINVAL;

	/* offload features support */
	fcoe_netdev_features_change(lport, netdev);

	skb_queue_head_init(&port->fcoe_pending_queue);
	port->fcoe_pending_queue_active = 0;
	setup_timer(&port->timer, fcoe_queue_timer, (unsigned long)lport);

	fcoe_link_speed_update(lport);

	if (!lport->vport) {
		if (fcoe_get_wwn(netdev, &wwnn, NETDEV_FCOE_WWNN))
			wwnn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr, 1, 0);
		fc_set_wwnn(lport, wwnn);
		if (fcoe_get_wwn(netdev, &wwpn, NETDEV_FCOE_WWPN))
			wwpn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr,
						 2, 0);
		fc_set_wwpn(lport, wwpn);
	}

	return 0;
}

/**
 * fcoe_shost_config() - Set up the SCSI host associated with a local port
 * @lport: The local port
 * @dev:   The device associated with the SCSI host
 *
 * Must be called after fcoe_lport_config() and fcoe_netdev_config()
 *
 * Returns: 0 for success
 */
static int fcoe_shost_config(struct fc_lport *lport, struct device *dev)
{
	int rc = 0;

	/* lport scsi host config */
	lport->host->max_lun = FCOE_MAX_LUN;
	lport->host->max_id = FCOE_MAX_FCP_TARGET;
	lport->host->max_channel = 0;
	lport->host->max_cmd_len = FCOE_MAX_CMD_LEN;

	if (lport->vport)
		lport->host->transportt = fcoe_vport_transport_template;
	else
		lport->host->transportt = fcoe_transport_template;

	/* add the new host to the SCSI-ml */
	rc = scsi_add_host(lport->host, dev);
	if (rc) {
		FCOE_NETDEV_DBG(fcoe_netdev(lport), "fcoe_shost_config: "
				"error on scsi_add_host\n");
		return rc;
	}

	if (!lport->vport)
		fc_host_max_npiv_vports(lport->host) = USHRT_MAX;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name);

	return 0;
}

/**
 * fcoe_oem_match() - The match routine for the offloaded exchange manager
 * @fp: The I/O frame
 *
 * This routine will be associated with an exchange manager (EM). When
 * the libfc exchange handling code is looking for an EM to use it will
 * call this routine and pass it the frame that it wishes to send. This
 * routine will return True if the associated EM is to be used and False
 * if the echange code should continue looking for an EM.
 *
 * The offload EM that this routine is associated with will handle any
 * packets that are for SCSI read requests.
 *
 * Returns: True for read types I/O, otherwise returns false.
 */
bool fcoe_oem_match(struct fc_frame *fp)
{
	return fc_fcp_is_read(fr_fsp(fp)) &&
		(fr_fsp(fp)->data_len > fcoe_ddp_min);
}

/**
 * fcoe_em_config() - Allocate and configure an exchange manager
 * @lport: The local port that the new EM will be associated with
 *
 * Returns: 0 on success
 */
static inline int fcoe_em_config(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct fcoe_interface *oldfcoe = NULL;
	struct net_device *old_real_dev, *cur_real_dev;
	u16 min_xid = FCOE_MIN_XID;
	u16 max_xid = FCOE_MAX_XID;

	/*
	 * Check if need to allocate an em instance for
	 * offload exchange ids to be shared across all VN_PORTs/lport.
	 */
	if (!lport->lro_enabled || !lport->lro_xid ||
	    (lport->lro_xid >= max_xid)) {
		lport->lro_xid = 0;
		goto skip_oem;
	}

	/*
	 * Reuse existing offload em instance in case
	 * it is already allocated on real eth device
	 */
	if (fcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
		cur_real_dev = vlan_dev_real_dev(fcoe->netdev);
	else
		cur_real_dev = fcoe->netdev;

	list_for_each_entry(oldfcoe, &fcoe_hostlist, list) {
		if (oldfcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
			old_real_dev = vlan_dev_real_dev(oldfcoe->netdev);
		else
			old_real_dev = oldfcoe->netdev;

		if (cur_real_dev == old_real_dev) {
			fcoe->oem = oldfcoe->oem;
			break;
		}
	}

	if (fcoe->oem) {
		if (!fc_exch_mgr_add(lport, fcoe->oem, fcoe_oem_match)) {
			printk(KERN_ERR "fcoe_em_config: failed to add "
			       "offload em:%p on interface:%s\n",
			       fcoe->oem, fcoe->netdev->name);
			return -ENOMEM;
		}
	} else {
		fcoe->oem = fc_exch_mgr_alloc(lport, FC_CLASS_3,
					      FCOE_MIN_XID, lport->lro_xid,
					      fcoe_oem_match);
		if (!fcoe->oem) {
			printk(KERN_ERR "fcoe_em_config: failed to allocate "
			       "em for offload exches on interface:%s\n",
			       fcoe->netdev->name);
			return -ENOMEM;
		}
	}

	/*
	 * Exclude offload EM xid range from next EM xid range.
	 */
	min_xid += lport->lro_xid + 1;

skip_oem:
	if (!fc_exch_mgr_alloc(lport, FC_CLASS_3, min_xid, max_xid, NULL)) {
		printk(KERN_ERR "fcoe_em_config: failed to "
		       "allocate em on interface %s\n", fcoe->netdev->name);
		return -ENOMEM;
	}

	return 0;
}

/**
 * fcoe_if_destroy() - Tear down a SW FCoE instance
 * @lport: The local port to be destroyed
 *
 * Locking: must be called with the RTNL mutex held and RTNL mutex
 * needed to be dropped by this function since not dropping RTNL
 * would cause circular locking warning on synchronous fip worker
 * cancelling thru fcoe_interface_put invoked by this function.
 *
 */
static void fcoe_if_destroy(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct net_device *netdev = fcoe->netdev;

	FCOE_NETDEV_DBG(netdev, "Destroying interface\n");

	/* Logout of the fabric */
	fc_fabric_logoff(lport);

	/* Cleanup the fc_lport */
	fc_lport_destroy(lport);
	fc_fcp_destroy(lport);

	/* Stop the transmit retry timer */
	del_timer_sync(&port->timer);

	/* Free existing transmit skbs */
	fcoe_clean_pending_queue(lport);

	if (!is_zero_ether_addr(port->data_src_addr))
		dev_uc_del(netdev, port->data_src_addr);
	rtnl_unlock();

	/* receives may not be stopped until after this */
	fcoe_interface_put(fcoe);

	/* Free queued packets for the per-CPU receive threads */
	fcoe_percpu_clean(lport);

	/* Detach from the scsi-ml */
	fc_remove_host(lport->host);
	scsi_remove_host(lport->host);

	/* There are no more rports or I/O, free the EM */
	fc_exch_mgr_free(lport);

	/* Free memory used by statistical counters */
	fc_lport_free_stats(lport);

	/* Release the Scsi_Host */
	scsi_host_put(lport->host);
	module_put(THIS_MODULE);
}

/**
 * fcoe_ddp_setup() - Call a LLD's ddp_setup through the net device
 * @lport: The local port to setup DDP for
 * @xid:   The exchange ID for this DDP transfer
 * @sgl:   The scatterlist describing this transfer
 * @sgc:   The number of sg items
 *
 * Returns: 0 if the DDP context was not configured
 */
static int fcoe_ddp_setup(struct fc_lport *lport, u16 xid,
			  struct scatterlist *sgl, unsigned int sgc)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netdev->netdev_ops->ndo_fcoe_ddp_setup)
		return netdev->netdev_ops->ndo_fcoe_ddp_setup(netdev,
							      xid, sgl,
							      sgc);

	return 0;
}

/**
 * fcoe_ddp_done() - Call a LLD's ddp_done through the net device
 * @lport: The local port to complete DDP on
 * @xid:   The exchange ID for this DDP transfer
 *
 * Returns: the length of data that have been completed by DDP
 */
static int fcoe_ddp_done(struct fc_lport *lport, u16 xid)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netdev->netdev_ops->ndo_fcoe_ddp_done)
		return netdev->netdev_ops->ndo_fcoe_ddp_done(netdev, xid);
	return 0;
}

/**
 * fcoe_if_create() - Create a FCoE instance on an interface
 * @fcoe:   The FCoE interface to create a local port on
 * @parent: The device pointer to be the parent in sysfs for the SCSI host
 * @npiv:   Indicates if the port is a vport or not
 *
 * Creates a fc_lport instance and a Scsi_Host instance and configure them.
 *
 * Returns: The allocated fc_lport or an error pointer
 */
static struct fc_lport *fcoe_if_create(struct fcoe_interface *fcoe,
				       struct device *parent, int npiv)
{
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *lport = NULL;
	struct fcoe_port *port;
	int rc;
	/*
	 * parent is only a vport if npiv is 1,
	 * but we'll only use vport in that case so go ahead and set it
	 */
	struct fc_vport *vport = dev_to_vport(parent);

	FCOE_NETDEV_DBG(netdev, "Create Interface\n");

	if (!npiv) {
		lport = libfc_host_alloc(&fcoe_shost_template,
					 sizeof(struct fcoe_port));
	} else	{
		lport = libfc_vport_create(vport,
					   sizeof(struct fcoe_port));
	}
	if (!lport) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate host structure\n");
		rc = -ENOMEM;
		goto out;
	}
	port = lport_priv(lport);
	port->lport = lport;
	port->fcoe = fcoe;
	INIT_WORK(&port->destroy_work, fcoe_destroy_work);

	/* configure a fc_lport including the exchange manager */
	rc = fcoe_lport_config(lport);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure lport for the "
				"interface\n");
		goto out_host_put;
	}

	if (npiv) {
		FCOE_NETDEV_DBG(netdev, "Setting vport names, "
				"%16.16llx %16.16llx\n",
				vport->node_name, vport->port_name);
		fc_set_wwnn(lport, vport->node_name);
		fc_set_wwpn(lport, vport->port_name);
	}

	/* configure lport network properties */
	rc = fcoe_netdev_config(lport, netdev);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure netdev for the "
				"interface\n");
		goto out_lp_destroy;
	}

	/* configure lport scsi host properties */
	rc = fcoe_shost_config(lport, parent);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure shost for the "
				"interface\n");
		goto out_lp_destroy;
	}

	/* Initialize the library */
	rc = fcoe_libfc_config(lport, &fcoe->ctlr, &fcoe_libfc_fcn_templ, 1);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure libfc for the "
				"interface\n");
		goto out_lp_destroy;
	}

	if (!npiv) {
		/*
		 * fcoe_em_alloc() and fcoe_hostlist_add() both
		 * need to be atomic with respect to other changes to the
		 * hostlist since fcoe_em_alloc() looks for an existing EM
		 * instance on host list updated by fcoe_hostlist_add().
		 *
		 * This is currently handled through the fcoe_config_mutex
		 * begin held.
		 */

		/* lport exch manager allocation */
		rc = fcoe_em_config(lport);
		if (rc) {
			FCOE_NETDEV_DBG(netdev, "Could not configure the EM "
					"for the interface\n");
			goto out_lp_destroy;
		}
	}

	fcoe_interface_get(fcoe);
	return lport;

out_lp_destroy:
	fc_exch_mgr_free(lport);
out_host_put:
	scsi_host_put(lport->host);
out:
	return ERR_PTR(rc);
}

/**
 * fcoe_if_init() - Initialization routine for fcoe.ko
 *
 * Attaches the SW FCoE transport to the FC transport
 *
 * Returns: 0 on success
 */
static int __init fcoe_if_init(void)
{
	/* attach to scsi transport */
	fcoe_transport_template = fc_attach_transport(&fcoe_transport_function);
	fcoe_vport_transport_template =
		fc_attach_transport(&fcoe_vport_transport_function);

	if (!fcoe_transport_template) {
		printk(KERN_ERR "fcoe: Failed to attach to the FC transport\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * fcoe_if_exit() - Tear down fcoe.ko
 *
 * Detaches the SW FCoE transport from the FC transport
 *
 * Returns: 0 on success
 */
int __exit fcoe_if_exit(void)
{
	fc_release_transport(fcoe_transport_template);
	fc_release_transport(fcoe_vport_transport_template);
	fcoe_transport_template = NULL;
	fcoe_vport_transport_template = NULL;
	return 0;
}

/**
 * fcoe_percpu_thread_create() - Create a receive thread for an online CPU
 * @cpu: The CPU index of the CPU to create a receive thread for
 */
static void fcoe_percpu_thread_create(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(fcoe_percpu, cpu);

	thread = kthread_create(fcoe_percpu_receive_thread,
				(void *)p, "fcoethread/%d", cpu);

	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		wake_up_process(thread);

		spin_lock_bh(&p->fcoe_rx_list.lock);
		p->thread = thread;
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
}

/**
 * fcoe_percpu_thread_destroy() - Remove the receive thread of a CPU
 * @cpu: The CPU index of the CPU whose receive thread is to be destroyed
 *
 * Destroys a per-CPU Rx thread. Any pending skbs are moved to the
 * current CPU's Rx thread. If the thread being destroyed is bound to
 * the CPU processing this context the skbs will be freed.
 */
static void fcoe_percpu_thread_destroy(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;
	struct page *crc_eof;
	struct sk_buff *skb;
#ifdef CONFIG_SMP
	struct fcoe_percpu_s *p0;
	unsigned targ_cpu = get_cpu();
#endif /* CONFIG_SMP */

	FCOE_DBG("Destroying receive thread for CPU %d\n", cpu);

	/* Prevent any new skbs from being queued for this CPU. */
	p = &per_cpu(fcoe_percpu, cpu);
	spin_lock_bh(&p->fcoe_rx_list.lock);
	thread = p->thread;
	p->thread = NULL;
	crc_eof = p->crc_eof_page;
	p->crc_eof_page = NULL;
	p->crc_eof_offset = 0;
	spin_unlock_bh(&p->fcoe_rx_list.lock);

#ifdef CONFIG_SMP
	/*
	 * Don't bother moving the skb's if this context is running
	 * on the same CPU that is having its thread destroyed. This
	 * can easily happen when the module is removed.
	 */
	if (cpu != targ_cpu) {
		p0 = &per_cpu(fcoe_percpu, targ_cpu);
		spin_lock_bh(&p0->fcoe_rx_list.lock);
		if (p0->thread) {
			FCOE_DBG("Moving frames from CPU %d to CPU %d\n",
				 cpu, targ_cpu);

			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				__skb_queue_tail(&p0->fcoe_rx_list, skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		} else {
			/*
			 * The targeted CPU is not initialized and cannot accept
			 * new	skbs. Unlock the targeted CPU and drop the skbs
			 * on the CPU that is going offline.
			 */
			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				kfree_skb(skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		}
	} else {
		/*
		 * This scenario occurs when the module is being removed
		 * and all threads are being destroyed. skbs will continue
		 * to be shifted from the CPU thread that is being removed
		 * to the CPU thread associated with the CPU that is processing
		 * the module removal. Once there is only one CPU Rx thread it
		 * will reach this case and we will drop all skbs and later
		 * stop the thread.
		 */
		spin_lock_bh(&p->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
			kfree_skb(skb);
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
	put_cpu();
#else
	/*
	 * This a non-SMP scenario where the singular Rx thread is
	 * being removed. Free all skbs and stop the thread.
	 */
	spin_lock_bh(&p->fcoe_rx_list.lock);
	while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
		kfree_skb(skb);
	spin_unlock_bh(&p->fcoe_rx_list.lock);
#endif

	if (thread)
		kthread_stop(thread);

	if (crc_eof)
		put_page(crc_eof);
}

/**
 * fcoe_cpu_callback() - Handler for CPU hotplug events
 * @nfb:    The callback data block
 * @action: The event triggering the callback
 * @hcpu:   The index of the CPU that the event is for
 *
 * This creates or destroys per-CPU data for fcoe
 *
 * Returns NOTIFY_OK always.
 */
static int fcoe_cpu_callback(struct notifier_block *nfb,
			     unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		FCOE_DBG("CPU %x online: Create Rx thread\n", cpu);
		fcoe_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		FCOE_DBG("CPU %x offline: Remove Rx thread\n", cpu);
		fcoe_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

/**
 * fcoe_rcv() - Receive packets from a net device
 * @skb:    The received packet
 * @netdev: The net device that the packet was received on
 * @ptype:  The packet type context
 * @olddev: The last device net device
 *
 * This routine is called by NET_RX_SOFTIRQ. It receives a packet, builds a
 * FC frame and passes the frame to libfc.
 *
 * Returns: 0 for success
 */
int fcoe_rcv(struct sk_buff *skb, struct net_device *netdev,
	     struct packet_type *ptype, struct net_device *olddev)
{
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_interface *fcoe;
	struct fc_frame_header *fh;
	struct fcoe_percpu_s *fps;
	struct fcoe_port *port;
	struct ethhdr *eh;
	unsigned int cpu;

	fcoe = container_of(ptype, struct fcoe_interface, fcoe_packet_type);
	lport = fcoe->ctlr.lp;
	if (unlikely(!lport)) {
		FCOE_NETDEV_DBG(netdev, "Cannot find hba structure");
		goto err2;
	}
	if (!lport->link_up)
		goto err2;

	FCOE_NETDEV_DBG(netdev, "skb_info: len:%d data_len:%d head:%p "
			"data:%p tail:%p end:%p sum:%d dev:%s",
			skb->len, skb->data_len, skb->head, skb->data,
			skb_tail_pointer(skb), skb_end_pointer(skb),
			skb->csum, skb->dev ? skb->dev->name : "<NULL>");

	/* check for mac addresses */
	eh = eth_hdr(skb);
	port = lport_priv(lport);
	if (compare_ether_addr(eh->h_dest, port->data_src_addr) &&
	    compare_ether_addr(eh->h_dest, fcoe->ctlr.ctl_src_addr) &&
	    compare_ether_addr(eh->h_dest, (u8[6])FC_FCOE_FLOGI_MAC)) {
		FCOE_NETDEV_DBG(netdev, "wrong destination mac address:%pM\n",
				eh->h_dest);
		goto err;
	}

	if (is_fip_mode(&fcoe->ctlr) &&
	    compare_ether_addr(eh->h_source, fcoe->ctlr.dest_addr)) {
		FCOE_NETDEV_DBG(netdev, "wrong source mac address:%pM\n",
				eh->h_source);
		goto err;
	}

	/*
	 * Check for minimum frame length, and make sure required FCoE
	 * and FC headers are pulled into the linear data area.
	 */
	if (unlikely((skb->len < FCOE_MIN_FRAME) ||
		     !pskb_may_pull(skb, FCOE_HEADER_LEN)))
		goto err;

	skb_set_transport_header(skb, sizeof(struct fcoe_hdr));
	fh = (struct fc_frame_header *) skb_transport_header(skb);

	fr = fcoe_dev_from_skb(skb);
	fr->fr_dev = lport;
	fr->ptype = ptype;

	/*
	 * In case the incoming frame's exchange is originated from
	 * the initiator, then received frame's exchange id is ANDed
	 * with fc_cpu_mask bits to get the same cpu on which exchange
	 * was originated, otherwise just use the current cpu.
	 */
	if (ntoh24(fh->fh_f_ctl) & FC_FC_EX_CTX)
		cpu = ntohs(fh->fh_ox_id) & fc_cpu_mask;
	else
		cpu = smp_processor_id();

	fps = &per_cpu(fcoe_percpu, cpu);
	spin_lock_bh(&fps->fcoe_rx_list.lock);
	if (unlikely(!fps->thread)) {
		/*
		 * The targeted CPU is not ready, let's target
		 * the first CPU now. For non-SMP systems this
		 * will check the same CPU twice.
		 */
		FCOE_NETDEV_DBG(netdev, "CPU is online, but no receive thread "
				"ready for incoming skb- using first online "
				"CPU.\n");

		spin_unlock_bh(&fps->fcoe_rx_list.lock);
		cpu = cpumask_first(cpu_online_mask);
		fps = &per_cpu(fcoe_percpu, cpu);
		spin_lock_bh(&fps->fcoe_rx_list.lock);
		if (!fps->thread) {
			spin_unlock_bh(&fps->fcoe_rx_list.lock);
			goto err;
		}
	}

	/*
	 * We now have a valid CPU that we're targeting for
	 * this skb. We also have this receive thread locked,
	 * so we're free to queue skbs into it's queue.
	 */

	/* If this is a SCSI-FCP frame, and this is already executing on the
	 * correct CPU, and the queue for this CPU is empty, then go ahead
	 * and process the frame directly in the softirq context.
	 * This lets us process completions without context switching from the
	 * NET_RX softirq, to our receive processing thread, and then back to
	 * BLOCK softirq context.
	 */
	if (fh->fh_type == FC_TYPE_FCP &&
	    cpu == smp_processor_id() &&
	    skb_queue_empty(&fps->fcoe_rx_list)) {
		spin_unlock_bh(&fps->fcoe_rx_list.lock);
		fcoe_recv_frame(skb);
	} else {
		__skb_queue_tail(&fps->fcoe_rx_list, skb);
		if (fps->fcoe_rx_list.qlen == 1)
			wake_up_process(fps->thread);
		spin_unlock_bh(&fps->fcoe_rx_list.lock);
	}

	return 0;
err:
	per_cpu_ptr(lport->dev_stats, get_cpu())->ErrorFrames++;
	put_cpu();
err2:
	kfree_skb(skb);
	return -1;
}

/**
 * fcoe_start_io() - Start FCoE I/O
 * @skb: The packet to be transmitted
 *
 * This routine is called from the net device to start transmitting
 * FCoE packets.
 *
 * Returns: 0 for success
 */
static inline int fcoe_start_io(struct sk_buff *skb)
{
	struct sk_buff *nskb;
	int rc;

	nskb = skb_clone(skb, GFP_ATOMIC);
	rc = dev_queue_xmit(nskb);
	if (rc != 0)
		return rc;
	kfree_skb(skb);
	return 0;
}

/**
 * fcoe_get_paged_crc_eof() - Allocate a page to be used for the trailer CRC
 * @skb:  The packet to be transmitted
 * @tlen: The total length of the trailer
 *
 * This routine allocates a page for frame trailers. The page is re-used if
 * there is enough room left on it for the current trailer. If there isn't
 * enough buffer left a new page is allocated for the trailer. Reference to
 * the page from this function as well as the skbs using the page fragments
 * ensure that the page is freed at the appropriate time.
 *
 * Returns: 0 for success
 */
static int fcoe_get_paged_crc_eof(struct sk_buff *skb, int tlen)
{
	struct fcoe_percpu_s *fps;
	struct page *page;

	fps = &get_cpu_var(fcoe_percpu);
	page = fps->crc_eof_page;
	if (!page) {
		page = alloc_page(GFP_ATOMIC);
		if (!page) {
			put_cpu_var(fcoe_percpu);
			return -ENOMEM;
		}
		fps->crc_eof_page = page;
		fps->crc_eof_offset = 0;
	}

	get_page(page);
	skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags, page,
			   fps->crc_eof_offset, tlen);
	skb->len += tlen;
	skb->data_len += tlen;
	skb->truesize += tlen;
	fps->crc_eof_offset += sizeof(struct fcoe_crc_eof);

	if (fps->crc_eof_offset >= PAGE_SIZE) {
		fps->crc_eof_page = NULL;
		fps->crc_eof_offset = 0;
		put_page(page);
	}
	put_cpu_var(fcoe_percpu);
	return 0;
}

/**
 * fcoe_fc_crc() - Calculates the CRC for a given frame
 * @fp: The frame to be checksumed
 *
 * This uses crc32() routine to calculate the CRC for a frame
 *
 * Return: The 32 bit CRC value
 */
u32 fcoe_fc_crc(struct fc_frame *fp)
{
	struct sk_buff *skb = fp_skb(fp);
	struct skb_frag_struct *frag;
	unsigned char *data;
	unsigned long off, len, clen;
	u32 crc;
	unsigned i;

	crc = crc32(~0, skb->data, skb_headlen(skb));

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		off = frag->page_offset;
		len = frag->size;
		while (len > 0) {
			clen = min(len, PAGE_SIZE - (off & ~PAGE_MASK));
			data = kmap_atomic(frag->page + (off >> PAGE_SHIFT),
					   KM_SKB_DATA_SOFTIRQ);
			crc = crc32(crc, data + (off & ~PAGE_MASK), clen);
			kunmap_atomic(data, KM_SKB_DATA_SOFTIRQ);
			off += clen;
			len -= clen;
		}
	}
	return crc;
}

/**
 * fcoe_xmit() - Transmit a FCoE frame
 * @lport: The local port that the frame is to be transmitted for
 * @fp:	   The frame to be transmitted
 *
 * Return: 0 for success
 */
int fcoe_xmit(struct fc_lport *lport, struct fc_frame *fp)
{
	int wlen;
	u32 crc;
	struct ethhdr *eh;
	struct fcoe_crc_eof *cp;
	struct sk_buff *skb;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	unsigned int hlen;		/* header length implies the version */
	unsigned int tlen;		/* trailer length */
	unsigned int elen;		/* eth header, may include vlan */
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	u8 sof, eof;
	struct fcoe_hdr *hp;

	WARN_ON((fr_len(fp) % sizeof(u32)) != 0);

	fh = fc_frame_header_get(fp);
	skb = fp_skb(fp);
	wlen = skb->len / FCOE_WORD_TO_BYTE;

	if (!lport->link_up) {
		kfree_skb(skb);
		return 0;
	}

	if (unlikely(fh->fh_type == FC_TYPE_ELS) &&
	    fcoe_ctlr_els_send(&fcoe->ctlr, lport, skb))
		return 0;

	sof = fr_sof(fp);
	eof = fr_eof(fp);

	elen = sizeof(struct ethhdr);
	hlen = sizeof(struct fcoe_hdr);
	tlen = sizeof(struct fcoe_crc_eof);
	wlen = (skb->len - tlen + sizeof(crc)) / FCOE_WORD_TO_BYTE;

	/* crc offload */
	if (likely(lport->crc_offload)) {
		skb->ip_summed = CHECKSUM_PARTIAL;
		skb->csum_start = skb_headroom(skb);
		skb->csum_offset = skb->len;
		crc = 0;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
		crc = fcoe_fc_crc(fp);
	}

	/* copy port crc and eof to the skb buff */
	if (skb_is_nonlinear(skb)) {
		skb_frag_t *frag;
		if (fcoe_get_paged_crc_eof(skb, tlen)) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		frag = &skb_shinfo(skb)->frags[skb_shinfo(skb)->nr_frags - 1];
		cp = kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ)
			+ frag->page_offset;
	} else {
		cp = (struct fcoe_crc_eof *)skb_put(skb, tlen);
	}

	memset(cp, 0, sizeof(*cp));
	cp->fcoe_eof = eof;
	cp->fcoe_crc32 = cpu_to_le32(~crc);

	if (skb_is_nonlinear(skb)) {
		kunmap_atomic(cp, KM_SKB_DATA_SOFTIRQ);
		cp = NULL;
	}

	/* adjust skb network/transport offsets to match mac/fcoe/port */
	skb_push(skb, elen + hlen);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb->mac_len = elen;
	skb->protocol = htons(ETH_P_FCOE);
	skb->dev = fcoe->netdev;

	/* fill up mac and fcoe headers */
	eh = eth_hdr(skb);
	eh->h_proto = htons(ETH_P_FCOE);
	memcpy(eh->h_dest, fcoe->ctlr.dest_addr, ETH_ALEN);
	if (fcoe->ctlr.map_dest)
		memcpy(eh->h_dest + 3, fh->fh_d_id, 3);

	if (unlikely(fcoe->ctlr.flogi_oxid != FC_XID_UNKNOWN))
		memcpy(eh->h_source, fcoe->ctlr.ctl_src_addr, ETH_ALEN);
	else
		memcpy(eh->h_source, port->data_src_addr, ETH_ALEN);

	hp = (struct fcoe_hdr *)(eh + 1);
	memset(hp, 0, sizeof(*hp));
	if (FC_FCOE_VER)
		FC_FCOE_ENCAPS_VER(hp, FC_FCOE_VER);
	hp->fcoe_sof = sof;

	/* fcoe lso, mss is in max_payload which is non-zero for FCP data */
	if (lport->seq_offload && fr_max_payload(fp)) {
		skb_shinfo(skb)->gso_type = SKB_GSO_FCOE;
		skb_shinfo(skb)->gso_size = fr_max_payload(fp);
	} else {
		skb_shinfo(skb)->gso_type = 0;
		skb_shinfo(skb)->gso_size = 0;
	}
	/* update tx stats: regardless if LLD fails */
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	stats->TxFrames++;
	stats->TxWords += wlen;
	put_cpu();

	/* send down to lld */
	fr_dev(fp) = lport;
	if (port->fcoe_pending_queue.qlen)
		fcoe_check_wait_queue(lport, skb);
	else if (fcoe_start_io(skb))
		fcoe_check_wait_queue(lport, skb);

	return 0;
}

/**
 * fcoe_percpu_flush_done() - Indicate per-CPU queue flush completion
 * @skb: The completed skb (argument required by destructor)
 */
static void fcoe_percpu_flush_done(struct sk_buff *skb)
{
	complete(&fcoe_flush_completion);
}

/**
 * fcoe_recv_frame() - process a single received frame
 * @skb: frame to process
 */
static void fcoe_recv_frame(struct sk_buff *skb)
{
	u32 fr_len;
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	struct fcoe_crc_eof crc_eof;
	struct fc_frame *fp;
	struct fcoe_port *port;
	struct fcoe_hdr *hp;

	fr = fcoe_dev_from_skb(skb);
	lport = fr->fr_dev;
	if (unlikely(!lport)) {
		if (skb->destructor != fcoe_percpu_flush_done)
			FCOE_NETDEV_DBG(skb->dev, "NULL lport in skb");
		kfree_skb(skb);
		return;
	}

	FCOE_NETDEV_DBG(skb->dev, "skb_info: len:%d data_len:%d "
			"head:%p data:%p tail:%p end:%p sum:%d dev:%s",
			skb->len, skb->data_len,
			skb->head, skb->data, skb_tail_pointer(skb),
			skb_end_pointer(skb), skb->csum,
			skb->dev ? skb->dev->name : "<NULL>");

	port = lport_priv(lport);
	if (skb_is_nonlinear(skb))
		skb_linearize(skb);	/* not ideal */

	/*
	 * Frame length checks and setting up the header pointers
	 * was done in fcoe_rcv already.
	 */
	hp = (struct fcoe_hdr *) skb_network_header(skb);
	fh = (struct fc_frame_header *) skb_transport_header(skb);

	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	if (unlikely(FC_FCOE_DECAPS_VER(hp) != FC_FCOE_VER)) {
		if (stats->ErrorFrames < 5)
			printk(KERN_WARNING "fcoe: FCoE version "
			       "mismatch: The frame has "
			       "version %x, but the "
			       "initiator supports version "
			       "%x\n", FC_FCOE_DECAPS_VER(hp),
			       FC_FCOE_VER);
		goto drop;
	}

	skb_pull(skb, sizeof(struct fcoe_hdr));
	fr_len = skb->len - sizeof(struct fcoe_crc_eof);

	stats->RxFrames++;
	stats->RxWords += fr_len / FCOE_WORD_TO_BYTE;

	fp = (struct fc_frame *)skb;
	fc_frame_init(fp);
	fr_dev(fp) = lport;
	fr_sof(fp) = hp->fcoe_sof;

	/* Copy out the CRC and EOF trailer for access */
	if (skb_copy_bits(skb, fr_len, &crc_eof, sizeof(crc_eof)))
		goto drop;
	fr_eof(fp) = crc_eof.fcoe_eof;
	fr_crc(fp) = crc_eof.fcoe_crc32;
	if (pskb_trim(skb, fr_len))
		goto drop;

	/*
	 * We only check CRC if no offload is available and if it is
	 * it's solicited data, in which case, the FCP layer would
	 * check it during the copy.
	 */
	if (lport->crc_offload &&
	    skb->ip_summed == CHECKSUM_UNNECESSARY)
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
	else
		fr_flags(fp) |= FCPHF_CRC_UNCHECKED;

	fh = fc_frame_header_get(fp);
	if ((fh->fh_r_ctl != FC_RCTL_DD_SOL_DATA ||
	    fh->fh_type != FC_TYPE_FCP) &&
	    (fr_flags(fp) & FCPHF_CRC_UNCHECKED)) {
		if (le32_to_cpu(fr_crc(fp)) !=
		    ~crc32(~0, skb->data, fr_len)) {
			if (stats->InvalidCRCCount < 5)
				printk(KERN_WARNING "fcoe: dropping "
				       "frame with CRC error\n");
			stats->InvalidCRCCount++;
			goto drop;
		}
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
	}
	put_cpu();
	fc_exch_recv(lport, fp);
	return;

drop:
	stats->ErrorFrames++;
	put_cpu();
	kfree_skb(skb);
}

/**
 * fcoe_percpu_receive_thread() - The per-CPU packet receive thread
 * @arg: The per-CPU context
 *
 * Return: 0 for success
 */
int fcoe_percpu_receive_thread(void *arg)
{
	struct fcoe_percpu_s *p = arg;
	struct sk_buff *skb;

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {

		spin_lock_bh(&p->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&p->fcoe_rx_list)) == NULL) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_bh(&p->fcoe_rx_list.lock);
			schedule();
			set_current_state(TASK_RUNNING);
			if (kthread_should_stop())
				return 0;
			spin_lock_bh(&p->fcoe_rx_list.lock);
		}
		spin_unlock_bh(&p->fcoe_rx_list.lock);
		fcoe_recv_frame(skb);
	}
	return 0;
}

/**
 * fcoe_check_wait_queue() - Attempt to clear the transmit backlog
 * @lport: The local port whose backlog is to be cleared
 *
 * This empties the wait_queue, dequeues the head of the wait_queue queue
 * and calls fcoe_start_io() for each packet. If all skb have been
 * transmitted it returns the qlen. If an error occurs it restores
 * wait_queue (to try again later) and returns -1.
 *
 * The wait_queue is used when the skb transmit fails. The failed skb
 * will go in the wait_queue which will be emptied by the timer function or
 * by the next skb transmit.
 */
static void fcoe_check_wait_queue(struct fc_lport *lport, struct sk_buff *skb)
{
	struct fcoe_port *port = lport_priv(lport);
	int rc;

	spin_lock_bh(&port->fcoe_pending_queue.lock);

	if (skb)
		__skb_queue_tail(&port->fcoe_pending_queue, skb);

	if (port->fcoe_pending_queue_active)
		goto out;
	port->fcoe_pending_queue_active = 1;

	while (port->fcoe_pending_queue.qlen) {
		/* keep qlen > 0 until fcoe_start_io succeeds */
		port->fcoe_pending_queue.qlen++;
		skb = __skb_dequeue(&port->fcoe_pending_queue);

		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		rc = fcoe_start_io(skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);

		if (rc) {
			__skb_queue_head(&port->fcoe_pending_queue, skb);
			/* undo temporary increment above */
			port->fcoe_pending_queue.qlen--;
			break;
		}
		/* undo temporary increment above */
		port->fcoe_pending_queue.qlen--;
	}

	if (port->fcoe_pending_queue.qlen < FCOE_LOW_QUEUE_DEPTH)
		lport->qfull = 0;
	if (port->fcoe_pending_queue.qlen && !timer_pending(&port->timer))
		mod_timer(&port->timer, jiffies + 2);
	port->fcoe_pending_queue_active = 0;
out:
	if (port->fcoe_pending_queue.qlen > FCOE_MAX_QUEUE_DEPTH)
		lport->qfull = 1;
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
	return;
}

/**
 * fcoe_dev_setup() - Setup the link change notification interface
 */
static void fcoe_dev_setup(void)
{
	register_netdevice_notifier(&fcoe_notifier);
}

/**
 * fcoe_dev_cleanup() - Cleanup the link change notification interface
 */
static void fcoe_dev_cleanup(void)
{
	unregister_netdevice_notifier(&fcoe_notifier);
}

/**
 * fcoe_device_notification() - Handler for net device events
 * @notifier: The context of the notification
 * @event:    The type of event
 * @ptr:      The net device that the event was on
 *
 * This function is called by the Ethernet driver in case of link change event.
 *
 * Returns: 0 for success
 */
static int fcoe_device_notification(struct notifier_block *notifier,
				    ulong event, void *ptr)
{
	struct fc_lport *lport = NULL;
	struct net_device *netdev = ptr;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;
	struct fcoe_dev_stats *stats;
	u32 link_possible = 1;
	u32 mfs;
	int rc = NOTIFY_OK;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev == netdev) {
			lport = fcoe->ctlr.lp;
			break;
		}
	}
	if (!lport) {
		rc = NOTIFY_DONE;
		goto out;
	}

	switch (event) {
	case NETDEV_DOWN:
	case NETDEV_GOING_DOWN:
		link_possible = 0;
		break;
	case NETDEV_UP:
	case NETDEV_CHANGE:
		break;
	case NETDEV_CHANGEMTU:
		if (netdev->features & NETIF_F_FCOE_MTU)
			break;
		mfs = netdev->mtu - (sizeof(struct fcoe_hdr) +
				     sizeof(struct fcoe_crc_eof));
		if (mfs >= FC_MIN_MAX_FRAME)
			fc_set_mfs(lport, mfs);
		break;
	case NETDEV_REGISTER:
		break;
	case NETDEV_UNREGISTER:
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
		fcoe_interface_cleanup(fcoe);
		schedule_work(&port->destroy_work);
		goto out;
		break;
	case NETDEV_FEAT_CHANGE:
		fcoe_netdev_features_change(lport, netdev);
		break;
	default:
		FCOE_NETDEV_DBG(netdev, "Unknown event %ld "
				"from netdev netlink\n", event);
	}

	fcoe_link_speed_update(lport);

	if (link_possible && !fcoe_link_ok(lport))
		fcoe_ctlr_link_up(&fcoe->ctlr);
	else if (fcoe_ctlr_link_down(&fcoe->ctlr)) {
		stats = per_cpu_ptr(lport->dev_stats, get_cpu());
		stats->LinkFailureCount++;
		put_cpu();
		fcoe_clean_pending_queue(lport);
	}
out:
	return rc;
}

/**
 * fcoe_if_to_netdev() - Parse a name buffer to get a net device
 * @buffer: The name of the net device
 *
 * Returns: NULL or a ptr to net_device
 */
static struct net_device *fcoe_if_to_netdev(const char *buffer)
{
	char *cp;
	char ifname[IFNAMSIZ + 2];

	if (buffer) {
		strlcpy(ifname, buffer, IFNAMSIZ);
		cp = ifname + strlen(ifname);
		while (--cp >= ifname && *cp == '\n')
			*cp = '\0';
		return dev_get_by_name(&init_net, ifname);
	}
	return NULL;
}

/**
 * fcoe_disable() - Disables a FCoE interface
 * @buffer: The name of the Ethernet interface to be disabled
 * @kp:	    The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_disable(const char *buffer, struct kernel_param *kp)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	if (!rtnl_trylock()) {
		dev_put(netdev);
		mutex_unlock(&fcoe_config_mutex);
		return restart_syscall();
	}

	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (fcoe) {
		fcoe_ctlr_link_down(&fcoe->ctlr);
		fcoe_clean_pending_queue(fcoe->ctlr.lp);
	} else
		rc = -ENODEV;

	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_enable() - Enables a FCoE interface
 * @buffer: The name of the Ethernet interface to be enabled
 * @kp:     The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_enable(const char *buffer, struct kernel_param *kp)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	if (!rtnl_trylock()) {
		dev_put(netdev);
		mutex_unlock(&fcoe_config_mutex);
		return restart_syscall();
	}

	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (!fcoe)
		rc = -ENODEV;
	else if (!fcoe_link_ok(fcoe->ctlr.lp))
		fcoe_ctlr_link_up(&fcoe->ctlr);

	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_destroy() - Destroy a FCoE interface
 * @buffer: The name of the Ethernet interface to be destroyed
 * @kp:	    The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_destroy(const char *buffer, struct kernel_param *kp)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	if (!rtnl_trylock()) {
		dev_put(netdev);
		mutex_unlock(&fcoe_config_mutex);
		return restart_syscall();
	}

	fcoe = fcoe_hostlist_lookup_port(netdev);
	if (!fcoe) {
		rtnl_unlock();
		rc = -ENODEV;
		goto out_putdev;
	}
	fcoe_interface_cleanup(fcoe);
	list_del(&fcoe->list);
	/* RTNL mutex is dropped by fcoe_if_destroy */
	fcoe_if_destroy(fcoe->ctlr.lp);

out_putdev:
	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_destroy_work() - Destroy a FCoE port in a deferred work context
 * @work: Handle to the FCoE port to be destroyed
 */
static void fcoe_destroy_work(struct work_struct *work)
{
	struct fcoe_port *port;

	port = container_of(work, struct fcoe_port, destroy_work);
	mutex_lock(&fcoe_config_mutex);
	rtnl_lock();
	/* RTNL mutex is dropped by fcoe_if_destroy */
	fcoe_if_destroy(port->lport);
	mutex_unlock(&fcoe_config_mutex);
}

/**
 * fcoe_create() - Create a fcoe interface
 * @buffer: The name of the Ethernet interface to create on
 * @kp:	    The associated kernel param
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_create(const char *buffer, struct kernel_param *kp)
{
	enum fip_state fip_mode = (enum fip_state)(long)kp->arg;
	int rc;
	struct fcoe_interface *fcoe;
	struct fc_lport *lport;
	struct net_device *netdev;

	mutex_lock(&fcoe_config_mutex);

	if (!rtnl_trylock()) {
		mutex_unlock(&fcoe_config_mutex);
		return restart_syscall();
	}

#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nomod;
	}
#endif

	if (!try_module_get(THIS_MODULE)) {
		rc = -EINVAL;
		goto out_nomod;
	}

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	/* look for existing lport */
	if (fcoe_hostlist_lookup(netdev)) {
		rc = -EEXIST;
		goto out_putdev;
	}

	fcoe = fcoe_interface_create(netdev, fip_mode);
	if (!fcoe) {
		rc = -ENOMEM;
		goto out_putdev;
	}

	lport = fcoe_if_create(fcoe, &netdev->dev, 0);
	if (IS_ERR(lport)) {
		printk(KERN_ERR "fcoe: Failed to create interface (%s)\n",
		       netdev->name);
		rc = -EIO;
		fcoe_interface_cleanup(fcoe);
		goto out_free;
	}

	/* Make this the "master" N_Port */
	fcoe->ctlr.lp = lport;

	/* add to lports list */
	fcoe_hostlist_add(lport);

	/* start FIP Discovery and FLOGI */
	lport->boot_time = jiffies;
	fc_fabric_login(lport);
	if (!fcoe_link_ok(lport))
		fcoe_ctlr_link_up(&fcoe->ctlr);

	/*
	 * Release from init in fcoe_interface_create(), on success lport
	 * should be holding a reference taken in fcoe_if_create().
	 */
	fcoe_interface_put(fcoe);
	dev_put(netdev);
	rtnl_unlock();
	mutex_unlock(&fcoe_config_mutex);

	return 0;
out_free:
	fcoe_interface_put(fcoe);
out_putdev:
	dev_put(netdev);
out_nodev:
	module_put(THIS_MODULE);
out_nomod:
	rtnl_unlock();
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_link_speed_update() - Update the supported and actual link speeds
 * @lport: The local port to update speeds for
 *
 * Returns: 0 if the ethtool query was successful
 *          -1 if the ethtool query failed
 */
int fcoe_link_speed_update(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct net_device *netdev = port->fcoe->netdev;
	struct ethtool_cmd ecmd = { ETHTOOL_GSET };

	if (!dev_ethtool_get_settings(netdev, &ecmd)) {
		lport->link_supported_speeds &=
			~(FC_PORTSPEED_1GBIT | FC_PORTSPEED_10GBIT);
		if (ecmd.supported & (SUPPORTED_1000baseT_Half |
				      SUPPORTED_1000baseT_Full))
			lport->link_supported_speeds |= FC_PORTSPEED_1GBIT;
		if (ecmd.supported & SUPPORTED_10000baseT_Full)
			lport->link_supported_speeds |=
				FC_PORTSPEED_10GBIT;
		if (ecmd.speed == SPEED_1000)
			lport->link_speed = FC_PORTSPEED_1GBIT;
		if (ecmd.speed == SPEED_10000)
			lport->link_speed = FC_PORTSPEED_10GBIT;

		return 0;
	}
	return -1;
}

/**
 * fcoe_link_ok() - Check if the link is OK for a local port
 * @lport: The local port to check link on
 *
 * Returns: 0 if link is UP and OK, -1 if not
 *
 */
int fcoe_link_ok(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct net_device *netdev = port->fcoe->netdev;

	if (netif_oper_up(netdev))
		return 0;
	return -1;
}

/**
 * fcoe_percpu_clean() - Clear all pending skbs for an local port
 * @lport: The local port whose skbs are to be cleared
 *
 * Must be called with fcoe_create_mutex held to single-thread completion.
 *
 * This flushes the pending skbs by adding a new skb to each queue and
 * waiting until they are all freed.  This assures us that not only are
 * there no packets that will be handled by the lport, but also that any
 * threads already handling packet have returned.
 */
void fcoe_percpu_clean(struct fc_lport *lport)
{
	struct fcoe_percpu_s *pp;
	struct fcoe_rcv_info *fr;
	struct sk_buff_head *list;
	struct sk_buff *skb, *next;
	struct sk_buff *head;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		pp = &per_cpu(fcoe_percpu, cpu);
		spin_lock_bh(&pp->fcoe_rx_list.lock);
		list = &pp->fcoe_rx_list;
		head = list->next;
		for (skb = head; skb != (struct sk_buff *)list;
		     skb = next) {
			next = skb->next;
			fr = fcoe_dev_from_skb(skb);
			if (fr->fr_dev == lport) {
				__skb_unlink(skb, list);
				kfree_skb(skb);
			}
		}

		if (!pp->thread || !cpu_online(cpu)) {
			spin_unlock_bh(&pp->fcoe_rx_list.lock);
			continue;
		}

		skb = dev_alloc_skb(0);
		if (!skb) {
			spin_unlock_bh(&pp->fcoe_rx_list.lock);
			continue;
		}
		skb->destructor = fcoe_percpu_flush_done;

		__skb_queue_tail(&pp->fcoe_rx_list, skb);
		if (pp->fcoe_rx_list.qlen == 1)
			wake_up_process(pp->thread);
		spin_unlock_bh(&pp->fcoe_rx_list.lock);

		wait_for_completion(&fcoe_flush_completion);
	}
}

/**
 * fcoe_clean_pending_queue() - Dequeue a skb and free it
 * @lport: The local port to dequeue a skb on
 */
void fcoe_clean_pending_queue(struct fc_lport *lport)
{
	struct fcoe_port  *port = lport_priv(lport);
	struct sk_buff *skb;

	spin_lock_bh(&port->fcoe_pending_queue.lock);
	while ((skb = __skb_dequeue(&port->fcoe_pending_queue)) != NULL) {
		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		kfree_skb(skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);
	}
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
}

/**
 * fcoe_reset() - Reset a local port
 * @shost: The SCSI host associated with the local port to be reset
 *
 * Returns: Always 0 (return value required by FC transport template)
 */
int fcoe_reset(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);
	fc_lport_reset(lport);
	return 0;
}

/**
 * fcoe_hostlist_lookup_port() - Find the FCoE interface associated with a net device
 * @netdev: The net device used as a key
 *
 * Locking: Must be called with the RNL mutex held.
 *
 * Returns: NULL or the FCoE interface
 */
static struct fcoe_interface *
fcoe_hostlist_lookup_port(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev == netdev)
			return fcoe;
	}
	return NULL;
}

/**
 * fcoe_hostlist_lookup() - Find the local port associated with a
 *			    given net device
 * @netdev: The netdevice used as a key
 *
 * Locking: Must be called with the RTNL mutex held
 *
 * Returns: NULL or the local port
 */
static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	fcoe = fcoe_hostlist_lookup_port(netdev);
	return (fcoe) ? fcoe->ctlr.lp : NULL;
}

/**
 * fcoe_hostlist_add() - Add the FCoE interface identified by a local
 *			 port to the hostlist
 * @lport: The local port that identifies the FCoE interface to be added
 *
 * Locking: must be called with the RTNL mutex held
 *
 * Returns: 0 for success
 */
static int fcoe_hostlist_add(const struct fc_lport *lport)
{
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	fcoe = fcoe_hostlist_lookup_port(fcoe_netdev(lport));
	if (!fcoe) {
		port = lport_priv(lport);
		fcoe = port->fcoe;
		list_add_tail(&fcoe->list, &fcoe_hostlist);
	}
	return 0;
}

/**
 * fcoe_init() - Initialize fcoe.ko
 *
 * Returns: 0 on success, or a negative value on failure
 */
static int __init fcoe_init(void)
{
	struct fcoe_percpu_s *p;
	unsigned int cpu;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);

	for_each_possible_cpu(cpu) {
		p = &per_cpu(fcoe_percpu, cpu);
		skb_queue_head_init(&p->fcoe_rx_list);
	}

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_create(cpu);

	/* Initialize per CPU interrupt thread */
	rc = register_hotcpu_notifier(&fcoe_cpu_notifier);
	if (rc)
		goto out_free;

	/* Setup link change notification */
	fcoe_dev_setup();

	rc = fcoe_if_init();
	if (rc)
		goto out_free;

	mutex_unlock(&fcoe_config_mutex);
	return 0;

out_free:
	for_each_online_cpu(cpu) {
		fcoe_percpu_thread_destroy(cpu);
	}
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}
module_init(fcoe_init);

/**
 * fcoe_exit() - Clean up fcoe.ko
 *
 * Returns: 0 on success or a  negative value on failure
 */
static void __exit fcoe_exit(void)
{
	struct fcoe_interface *fcoe, *tmp;
	struct fcoe_port *port;
	unsigned int cpu;

	mutex_lock(&fcoe_config_mutex);

	fcoe_dev_cleanup();

	/* releases the associated fcoe hosts */
	rtnl_lock();
	list_for_each_entry_safe(fcoe, tmp, &fcoe_hostlist, list) {
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
		fcoe_interface_cleanup(fcoe);
		schedule_work(&port->destroy_work);
	}
	rtnl_unlock();

	unregister_hotcpu_notifier(&fcoe_cpu_notifier);

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_destroy(cpu);

	mutex_unlock(&fcoe_config_mutex);

	/* flush any asyncronous interface destroys,
	 * this should happen after the netdev notifier is unregistered */
	flush_scheduled_work();
	/* That will flush out all the N_Ports on the hostlist, but now we
	 * may have NPIV VN_Ports scheduled for destruction */
	flush_scheduled_work();

	/* detach from scsi transport
	 * must happen after all destroys are done, therefor after the flush */
	fcoe_if_exit();
}
module_exit(fcoe_exit);

/**
 * fcoe_flogi_resp() - FCoE specific FLOGI and FDISC response handler
 * @seq: active sequence in the FLOGI or FDISC exchange
 * @fp: response frame, or error encoded in a pointer (timeout)
 * @arg: pointer the the fcoe_ctlr structure
 *
 * This handles MAC address management for FCoE, then passes control on to
 * the libfc FLOGI response handler.
 */
static void fcoe_flogi_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	u8 *mac;

	if (IS_ERR(fp))
		goto done;

	mac = fr_cb(fp)->granted_mac;
	if (is_zero_ether_addr(mac)) {
		/* pre-FIP */
		if (fcoe_ctlr_recv_flogi(fip, lport, fp)) {
			fc_frame_free(fp);
			return;
		}
	}
	fcoe_update_src_mac(lport, mac);
done:
	fc_lport_flogi_resp(seq, fp, lport);
}

/**
 * fcoe_logo_resp() - FCoE specific LOGO response handler
 * @seq: active sequence in the LOGO exchange
 * @fp: response frame, or error encoded in a pointer (timeout)
 * @arg: pointer the the fcoe_ctlr structure
 *
 * This handles MAC address management for FCoE, then passes control on to
 * the libfc LOGO response handler.
 */
static void fcoe_logo_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_lport *lport = arg;
	static u8 zero_mac[ETH_ALEN] = { 0 };

	if (!IS_ERR(fp))
		fcoe_update_src_mac(lport, zero_mac);
	fc_lport_logo_resp(seq, fp, lport);
}

/**
 * fcoe_elsct_send - FCoE specific ELS handler
 *
 * This does special case handling of FIP encapsualted ELS exchanges for FCoE,
 * using FCoE specific response handlers and passing the FIP controller as
 * the argument (the lport is still available from the exchange).
 *
 * Most of the work here is just handed off to the libfc routine.
 */
static struct fc_seq *fcoe_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct fc_frame_header *fh = fc_frame_header_get(fp);

	switch (op) {
	case ELS_FLOGI:
	case ELS_FDISC:
		if (lport->point_to_multipoint)
			break;
		return fc_elsct_send(lport, did, fp, op, fcoe_flogi_resp,
				     fip, timeout);
	case ELS_LOGO:
		/* only hook onto fabric logouts, not port logouts */
		if (ntoh24(fh->fh_d_id) != FC_FID_FLOGI)
			break;
		return fc_elsct_send(lport, did, fp, op, fcoe_logo_resp,
				     lport, timeout);
	}
	return fc_elsct_send(lport, did, fp, op, resp, arg, timeout);
}

/**
 * fcoe_vport_create() - create an fc_host/scsi_host for a vport
 * @vport: fc_vport object to create a new fc_host for
 * @disabled: start the new fc_host in a disabled state by default?
 *
 * Returns: 0 for success
 */
static int fcoe_vport_create(struct fc_vport *vport, bool disabled)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fcoe_port *port = lport_priv(n_port);
	struct fcoe_interface *fcoe = port->fcoe;
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *vn_port;

	mutex_lock(&fcoe_config_mutex);
	vn_port = fcoe_if_create(fcoe, &vport->dev, 1);
	mutex_unlock(&fcoe_config_mutex);

	if (IS_ERR(vn_port)) {
		printk(KERN_ERR "fcoe: fcoe_vport_create(%s) failed\n",
		       netdev->name);
		return -EIO;
	}

	if (disabled) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
	} else {
		vn_port->boot_time = jiffies;
		fc_fabric_login(vn_port);
		fc_vport_setlink(vn_port);
	}
	return 0;
}

/**
 * fcoe_vport_destroy() - destroy the fc_host/scsi_host for a vport
 * @vport: fc_vport object that is being destroyed
 *
 * Returns: 0 for success
 */
static int fcoe_vport_destroy(struct fc_vport *vport)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fc_lport *vn_port = vport->dd_data;
	struct fcoe_port *port = lport_priv(vn_port);

	mutex_lock(&n_port->lp_mutex);
	list_del(&vn_port->list);
	mutex_unlock(&n_port->lp_mutex);
	schedule_work(&port->destroy_work);
	return 0;
}

/**
 * fcoe_vport_disable() - change vport state
 * @vport: vport to bring online/offline
 * @disable: should the vport be disabled?
 */
static int fcoe_vport_disable(struct fc_vport *vport, bool disable)
{
	struct fc_lport *lport = vport->dd_data;

	if (disable) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
		fc_fabric_logoff(lport);
	} else {
		lport->boot_time = jiffies;
		fc_fabric_login(lport);
		fc_vport_setlink(lport);
	}

	return 0;
}

/**
 * fcoe_vport_set_symbolic_name() - append vport string to symbolic name
 * @vport: fc_vport with a new symbolic name string
 *
 * After generating a new symbolic name string, a new RSPN_ID request is
 * sent to the name server.  There is no response handler, so if it fails
 * for some reason it will not be retried.
 */
static void fcoe_set_vport_symbolic_name(struct fc_vport *vport)
{
	struct fc_lport *lport = vport->dd_data;
	struct fc_frame *fp;
	size_t len;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s : %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name, vport->symbolic_name);

	if (lport->state != LPORT_ST_READY)
		return;

	len = strnlen(fc_host_symbolic_name(lport->host), 255);
	fp = fc_frame_alloc(lport,
			    sizeof(struct fc_ct_hdr) +
			    sizeof(struct fc_ns_rspn) + len);
	if (!fp)
		return;
	lport->tt.elsct_send(lport, FC_FID_DIR_SERV, fp, FC_NS_RSPN_ID,
			     NULL, NULL, 3 * lport->r_a_tov);
}

/**
 * fcoe_get_lesb() - Fill the FCoE Link Error Status Block
 * @lport: the local port
 * @fc_lesb: the link error status block
 */
static void fcoe_get_lesb(struct fc_lport *lport,
			 struct fc_els_lesb *fc_lesb)
{
	unsigned int cpu;
	u32 lfc, vlfc, mdac;
	struct fcoe_dev_stats *devst;
	struct fcoe_fc_els_lesb *lesb;
	struct rtnl_link_stats64 temp;
	struct net_device *netdev = fcoe_netdev(lport);

	lfc = 0;
	vlfc = 0;
	mdac = 0;
	lesb = (struct fcoe_fc_els_lesb *)fc_lesb;
	memset(lesb, 0, sizeof(*lesb));
	for_each_possible_cpu(cpu) {
		devst = per_cpu_ptr(lport->dev_stats, cpu);
		lfc += devst->LinkFailureCount;
		vlfc += devst->VLinkFailureCount;
		mdac += devst->MissDiscAdvCount;
	}
	lesb->lesb_link_fail = htonl(lfc);
	lesb->lesb_vlink_fail = htonl(vlfc);
	lesb->lesb_miss_fka = htonl(mdac);
	lesb->lesb_fcs_error = htonl(dev_get_stats(netdev, &temp)->rx_crc_errors);
}

/**
 * fcoe_set_port_id() - Callback from libfc when Port_ID is set.
 * @lport: the local port
 * @port_id: the port ID
 * @fp: the received frame, if any, that caused the port_id to be set.
 *
 * This routine handles the case where we received a FLOGI and are
 * entering point-to-point mode.  We need to call fcoe_ctlr_recv_flogi()
 * so it can set the non-mapped mode and gateway address.
 *
 * The FLOGI LS_ACC is handled by fcoe_flogi_resp().
 */
static void fcoe_set_port_id(struct fc_lport *lport,
			     u32 port_id, struct fc_frame *fp)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;

	if (fp && fc_frame_payload_op(fp) == ELS_FLOGI)
		fcoe_ctlr_recv_flogi(&fcoe->ctlr, lport, fp);
}
