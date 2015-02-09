/******************************************************************************/
/*                                                                            */
/* Broadcom BCM5700 Linux Network Driver, Copyright (c) 2000 - 2004 Broadcom  */
/* Corporation.                                                               */
/* All rights reserved.                                                       */
/*                                                                            */
/* This program is free software; you can redistribute it and/or modify       */
/* it under the terms of the GNU General Public License as published by       */
/* the Free Software Foundation, located in the file LICENSE.                 */
/*                                                                            */
/* /proc file system handling code.                                           */
/*                                                                            */
/******************************************************************************/

#include "mm.h"
#ifdef BCM_PROC_FS

#define NICINFO_PROC_DIR "nicinfo"

static struct proc_dir_entry *bcm5700_procfs_dir;

extern char bcm5700_driver[], bcm5700_version[];
extern int bcm5700_open(struct net_device *dev);

extern uint64_t bcm5700_crc_count(PUM_DEVICE_BLOCK pUmDevice);
extern uint64_t bcm5700_rx_err_count(PUM_DEVICE_BLOCK pUmDevice);

static char *na_str = "n/a";
static char *pause_str = "pause ";
static char *asym_pause_str = "asym_pause ";
static char *on_str = "on";
static char *off_str = "off";
static char *up_str = "up";
static char *down_str = "down";

int bcm5700_proc_create_dev(struct net_device *dev);
int bcm5700_proc_remove_dev(struct net_device *dev);

static int bcm5700_netdev_event(struct notifier_block * this, unsigned long event, void * ptr)
{
	struct net_device * event_dev = (struct net_device *)ptr;

	if (event == NETDEV_CHANGENAME) {
		if (event_dev->open == bcm5700_open) {
			bcm5700_proc_remove_dev(event_dev);
			bcm5700_proc_create_dev(event_dev);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block bcm5700_netdev_notifier = {
	.notifier_call = bcm5700_netdev_event
};

static struct proc_dir_entry *
proc_getdir(char *name, struct proc_dir_entry *proc_dir)
{
	struct proc_dir_entry *pde = proc_dir;

	lock_kernel();
	for (pde=pde->subdir; pde; pde = pde->next) {
		if (pde->namelen && (strcmp(name, pde->name) == 0)) {
			/* directory exists */
			break;
		}
	}
	if (pde == (struct proc_dir_entry *) 0)
	{
		/* create the directory */
#if (LINUX_VERSION_CODE > 0x20300)
		pde = proc_mkdir(name, proc_dir);
#else
		pde = create_proc_entry(name, S_IFDIR, proc_dir);
#endif
		if (pde == (struct proc_dir_entry *) 0) {
			unlock_kernel();
			return (pde);
		}
	}
	unlock_kernel();
	return (pde);
}

int
bcm5700_proc_create(void)
{
	bcm5700_procfs_dir = proc_getdir(NICINFO_PROC_DIR, proc_net);

	if (bcm5700_procfs_dir == (struct proc_dir_entry *) 0) {
		printk(KERN_DEBUG "Could not create procfs nicinfo directory %s\n", NICINFO_PROC_DIR);
		return -1;
	}
	register_netdevice_notifier(&bcm5700_netdev_notifier);
	return 0;
}

int
bcm5700_proc_remove_notifier(void)
{
	unregister_netdevice_notifier(&bcm5700_netdev_notifier);
	return 0;
}

void
b57_get_speed_adv(PUM_DEVICE_BLOCK pUmDevice, char *str)
{
	PLM_DEVICE_BLOCK pDevice = &pUmDevice->lm_dev;

	if (pDevice->DisableAutoNeg == TRUE) {
		strcpy(str, na_str);
		return;
	}
	if (pDevice->TbiFlags & ENABLE_TBI_FLAG) {
		strcpy(str, "1000full");
		return;
	}
	if (pDevice->PhyFlags & PHY_IS_FIBER) {
		if (pDevice->RequestedDuplexMode != LM_DUPLEX_MODE_HALF)
			strcpy(str, "1000full");
		else
			strcpy(str, "1000half");
		return;
	}
	str[0] = 0;
	if (pDevice->advertising & PHY_AN_AD_10BASET_HALF) {
		strcat(str, "10half ");
	}
	if (pDevice->advertising & PHY_AN_AD_10BASET_FULL) {
		strcat(str, "10full ");
	}
	if (pDevice->advertising & PHY_AN_AD_100BASETX_HALF) {
		strcat(str, "100half ");
	}
	if (pDevice->advertising & PHY_AN_AD_100BASETX_FULL) {
		strcat(str, "100full ");
	}
	if (pDevice->advertising1000 & BCM540X_AN_AD_1000BASET_HALF) {
		strcat(str, "1000half ");
	}
	if (pDevice->advertising1000 & BCM540X_AN_AD_1000BASET_FULL) {
		strcat(str, "1000full ");
	}
}

void
b57_get_fc_adv(PUM_DEVICE_BLOCK pUmDevice, char *str)
{
	PLM_DEVICE_BLOCK pDevice = &pUmDevice->lm_dev;

	if (pDevice->DisableAutoNeg == TRUE) {
		strcpy(str, na_str);
		return;
	}
	str[0] = 0;
	if (pDevice->TbiFlags & ENABLE_TBI_FLAG) {
		if(pDevice->DisableAutoNeg == FALSE ||
			pDevice->RequestedLineSpeed == LM_LINE_SPEED_AUTO) {
			if (pDevice->FlowControlCap &
				LM_FLOW_CONTROL_RECEIVE_PAUSE) {

				strcpy(str, pause_str);
				if (!(pDevice->FlowControlCap &
					LM_FLOW_CONTROL_TRANSMIT_PAUSE)) {

					strcpy(str, asym_pause_str);
				}
			}
			else if (pDevice->FlowControlCap &
				LM_FLOW_CONTROL_TRANSMIT_PAUSE) {

				strcpy(str, asym_pause_str);
			}
		}
		return;
	}
	if (pDevice->advertising & PHY_AN_AD_PAUSE_CAPABLE) {
		strcat(str, pause_str);
	}
	if (pDevice->advertising & PHY_AN_AD_ASYM_PAUSE) {
		strcat(str, asym_pause_str);
	}
}

int
bcm5700_read_pfs(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct net_device *dev = (struct net_device *) data;
	PUM_DEVICE_BLOCK pUmDevice = (PUM_DEVICE_BLOCK) dev->priv;
	PLM_DEVICE_BLOCK pDevice = &pUmDevice->lm_dev;
	PT3_STATS_BLOCK pStats = (PT3_STATS_BLOCK) pDevice->pStatsBlkVirt;
	int len = 0;
	unsigned long rx_mac_errors, rx_crc_errors, rx_align_errors;
	unsigned long rx_runt_errors, rx_frag_errors, rx_long_errors;
	unsigned long rx_overrun_errors, rx_jabber_errors;
	char str[64];

	if (pUmDevice->opened == 0)
		pStats = 0;

	len += sprintf(page+len, "Description\t\t\t%s\n", pUmDevice->name);
	len += sprintf(page+len, "Driver_Name\t\t\t%s\n", bcm5700_driver);
	len += sprintf(page+len, "Driver_Version\t\t\t%s\n", bcm5700_version);
	len += sprintf(page+len, "Bootcode_Version\t\t%s\n", pDevice->BootCodeVer);
	if( pDevice->IPMICodeVer[0] != 0 )
		len += sprintf(page+len, "ASF_IPMI_Version\t\t%s\n", pDevice->IPMICodeVer);
	len += sprintf(page+len, "PCI_Vendor\t\t\t0x%04x\n", pDevice->PciVendorId);
	len += sprintf(page+len, "PCI_Device_ID\t\t\t0x%04x\n",
		pDevice->PciDeviceId);
	len += sprintf(page+len, "PCI_Subsystem_Vendor\t\t0x%04x\n",
		pDevice->SubsystemVendorId);
	len += sprintf(page+len, "PCI_Subsystem_ID\t\t0x%04x\n",
		pDevice->SubsystemId);
	len += sprintf(page+len, "PCI_Revision_ID\t\t\t0x%02x\n",
		pDevice->PciRevId);
	len += sprintf(page+len, "PCI_Slot\t\t\t%d\n",
		PCI_SLOT(pUmDevice->pdev->devfn));
	if (T3_ASIC_REV(pDevice->ChipRevId) == T3_ASIC_REV_5704)
        {
		len += sprintf(page+len, "PCI_Function\t\t\t%d\n",
			pDevice->FunctNum);
	}
	len += sprintf(page+len, "PCI_Bus\t\t\t\t%d\n",
		pUmDevice->pdev->bus->number);

	len += sprintf(page+len, "PCI_Bus_Speed\t\t\t%s\n",
		pDevice->BusSpeedStr);

	len += sprintf(page+len, "Memory\t\t\t\t0x%lx\n", pUmDevice->dev->base_addr);
	len += sprintf(page+len, "IRQ\t\t\t\t%d\n", dev->irq);
	len += sprintf(page+len, "System_Device_Name\t\t%s\n", dev->name);
	len += sprintf(page+len, "Current_HWaddr\t\t\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
	len += sprintf(page+len,
		"Permanent_HWaddr\t\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		pDevice->NodeAddress[0], pDevice->NodeAddress[1],
		pDevice->NodeAddress[2], pDevice->NodeAddress[3],
		pDevice->NodeAddress[4], pDevice->NodeAddress[5]);
	len += sprintf(page+len, "Part_Number\t\t\t%s\n\n", pDevice->PartNo);

	len += sprintf(page+len, "Link\t\t\t\t%s\n", 
		(pUmDevice->opened == 0) ? "unknown" :
    		((pDevice->LinkStatus == LM_STATUS_LINK_ACTIVE) ? up_str :
		down_str));
	len += sprintf(page+len, "Auto_Negotiate\t\t\t%s\n", 
    		(pDevice->DisableAutoNeg == TRUE) ? off_str : on_str);
	b57_get_speed_adv(pUmDevice, str);
	len += sprintf(page+len, "Speed_Advertisement\t\t%s\n", str);
	b57_get_fc_adv(pUmDevice, str);
	len += sprintf(page+len, "Flow_Control_Advertisement\t%s\n", str);
	len += sprintf(page+len, "Speed\t\t\t\t%s\n", 
    		((pDevice->LinkStatus == LM_STATUS_LINK_DOWN) ||
		(pUmDevice->opened == 0)) ? na_str :
    		((pDevice->LineSpeed == LM_LINE_SPEED_1000MBPS) ? "1000" :
    		((pDevice->LineSpeed == LM_LINE_SPEED_100MBPS) ? "100" :
    		(pDevice->LineSpeed == LM_LINE_SPEED_10MBPS) ? "10" : na_str)));
	len += sprintf(page+len, "Duplex\t\t\t\t%s\n", 
    		((pDevice->LinkStatus == LM_STATUS_LINK_DOWN) ||
		(pUmDevice->opened == 0)) ? na_str :
		((pDevice->DuplexMode == LM_DUPLEX_MODE_FULL) ? "full" :
			"half"));
	len += sprintf(page+len, "Flow_Control\t\t\t%s\n", 
    		((pDevice->LinkStatus == LM_STATUS_LINK_DOWN) ||
		(pUmDevice->opened == 0)) ? na_str :
		((pDevice->FlowControl == LM_FLOW_CONTROL_NONE) ? off_str :
		(((pDevice->FlowControl & LM_FLOW_CONTROL_RX_TX_PAUSE) ==
			LM_FLOW_CONTROL_RX_TX_PAUSE) ? "receive/transmit" :
		(pDevice->FlowControl & LM_FLOW_CONTROL_RECEIVE_PAUSE) ?
			"receive" : "transmit")));
	len += sprintf(page+len, "State\t\t\t\t%s\n", 
		(pUmDevice->suspended ? "suspended" :
    		((dev->flags & IFF_UP) ? up_str : down_str)));
	len += sprintf(page+len, "MTU_Size\t\t\t%d\n\n", dev->mtu);
	len += sprintf(page+len, "Rx_Packets\t\t\t%lu\n", 
			((pStats == 0) ? 0 :
			MM_GETSTATS(pStats->ifHCInUcastPkts) +
			MM_GETSTATS(pStats->ifHCInMulticastPkts) +
			MM_GETSTATS(pStats->ifHCInBroadcastPkts)));
#if T3_JUMBO_RCV_RCB_ENTRY_COUNT
	if ((dev->mtu > 1500) && !T3_ASIC_IS_5705_BEYOND(pDevice->ChipRevId)) {
		len += sprintf(page+len, "Rx_Jumbo_Packets\t\t%lu\n", 
			((pStats == 0) ? 0 :
			MM_GETSTATS(
				pStats->etherStatsPkts1523Octetsto2047Octets) +
			MM_GETSTATS(
				pStats->etherStatsPkts2048Octetsto4095Octets) +
			MM_GETSTATS(
				pStats->etherStatsPkts4096Octetsto8191Octets) +
			MM_GETSTATS(
				pStats->etherStatsPkts8192Octetsto9022Octets)));
	}
#endif
	len += sprintf(page+len, "Tx_Packets\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->ifHCOutUcastPkts) +
		MM_GETSTATS(pStats->ifHCOutMulticastPkts) +
		MM_GETSTATS(pStats->ifHCOutBroadcastPkts)));
#ifdef BCM_TSO
	len += sprintf(page+len, "TSO_Large_Packets\t\t%lu\n",
		pUmDevice->tso_pkt_count);
#endif
	len += sprintf(page+len, "Rx_Bytes\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->ifHCInOctets)));
	len += sprintf(page+len, "Tx_Bytes\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->ifHCOutOctets)));
	if (pStats == 0) {
		rx_crc_errors = 0;
		rx_align_errors = 0;
		rx_runt_errors = 0;
		rx_frag_errors = 0;
		rx_long_errors = 0;
		rx_overrun_errors = 0;
		rx_jabber_errors = 0;
	}
	else {
		rx_crc_errors = (unsigned long) bcm5700_crc_count(pUmDevice);
		rx_align_errors = MM_GETSTATS(pStats->dot3StatsAlignmentErrors);
		rx_runt_errors = MM_GETSTATS(pStats->etherStatsUndersizePkts);
		rx_frag_errors = MM_GETSTATS(pStats->etherStatsFragments);
		rx_long_errors = MM_GETSTATS(pStats->dot3StatsFramesTooLong);
		rx_overrun_errors = MM_GETSTATS(pStats->nicNoMoreRxBDs);
		rx_jabber_errors = MM_GETSTATS(pStats->etherStatsJabbers);
	}
	rx_mac_errors = (unsigned long) bcm5700_rx_err_count(pUmDevice);
	len += sprintf(page+len, "Rx_Errors\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		rx_mac_errors + rx_overrun_errors + pUmDevice->rx_misc_errors));
	len += sprintf(page+len, "Tx_Errors\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->ifOutErrors)));
	len += sprintf(page+len, "\nTx_Carrier_Errors\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->dot3StatsCarrierSenseErrors)));
	len += sprintf(page+len, "Tx_Abort_Excess_Coll\t\t%lu\n",
		((pStats == 0) ? 0 :
    		MM_GETSTATS(pStats->dot3StatsExcessiveCollisions)));
	len += sprintf(page+len, "Tx_Abort_Late_Coll\t\t%lu\n",
		((pStats == 0) ? 0 :
    		MM_GETSTATS(pStats->dot3StatsLateCollisions)));
	len += sprintf(page+len, "Tx_Deferred_Ok\t\t\t%lu\n",
		((pStats == 0) ? 0 :
    		MM_GETSTATS(pStats->dot3StatsDeferredTransmissions)));
	len += sprintf(page+len, "Tx_Single_Coll_Ok\t\t%lu\n",
		((pStats == 0) ? 0 :
    		MM_GETSTATS(pStats->dot3StatsSingleCollisionFrames)));
	len += sprintf(page+len, "Tx_Multi_Coll_Ok\t\t%lu\n",
		((pStats == 0) ? 0 :
    		MM_GETSTATS(pStats->dot3StatsMultipleCollisionFrames)));
	len += sprintf(page+len, "Tx_Total_Coll_Ok\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->etherStatsCollisions)));
	len += sprintf(page+len, "Tx_XON_Pause_Frames\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->outXonSent)));
	len += sprintf(page+len, "Tx_XOFF_Pause_Frames\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->outXoffSent)));
	len += sprintf(page+len, "\nRx_CRC_Errors\t\t\t%lu\n", rx_crc_errors);
	len += sprintf(page+len, "Rx_Short_Fragment_Errors\t%lu\n",
		rx_frag_errors);
	len += sprintf(page+len, "Rx_Short_Length_Errors\t\t%lu\n",
		rx_runt_errors);
	len += sprintf(page+len, "Rx_Long_Length_Errors\t\t%lu\n",
		rx_long_errors);
	len += sprintf(page+len, "Rx_Align_Errors\t\t\t%lu\n",
		rx_align_errors);
	len += sprintf(page+len, "Rx_Overrun_Errors\t\t%lu\n",
		rx_overrun_errors);
	len += sprintf(page+len, "Rx_XON_Pause_Frames\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->xonPauseFramesReceived)));
	len += sprintf(page+len, "Rx_XOFF_Pause_Frames\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->xoffPauseFramesReceived)));
	len += sprintf(page+len, "\nTx_MAC_Errors\t\t\t%lu\n",
		((pStats == 0) ? 0 :
		MM_GETSTATS(pStats->dot3StatsInternalMacTransmitErrors)));
	len += sprintf(page+len, "Rx_MAC_Errors\t\t\t%lu\n\n",
		rx_mac_errors);

	len += sprintf(page+len, "Tx_Checksum\t\t\t%s\n",
		((pDevice->TaskToOffload & LM_TASK_OFFLOAD_TX_TCP_CHECKSUM) ?
		on_str : off_str));
	len += sprintf(page+len, "Rx_Checksum\t\t\t%s\n",
		((pDevice->TaskToOffload & LM_TASK_OFFLOAD_RX_TCP_CHECKSUM) ?
		on_str : off_str));
	len += sprintf(page+len, "Scatter_Gather\t\t\t%s\n",
#if (LINUX_VERSION_CODE >= 0x20400)
		((dev->features & NETIF_F_SG) ? on_str : off_str));
#else
		off_str);
#endif
#ifdef BCM_TSO
	len += sprintf(page+len, "TSO\t\t\t\t%s\n",
		((dev->features & NETIF_F_TSO) ? on_str : off_str));
#endif
	len += sprintf(page+len, "VLAN\t\t\t\t%s\n\n",
		((pDevice->RxMode & RX_MODE_KEEP_VLAN_TAG) ? off_str : on_str));

#ifdef BCM_NIC_SEND_BD
	len += sprintf(page+len, "NIC_Tx_BDs\t\t\t%s\n",
		(pDevice->Flags & NIC_SEND_BD_FLAG) ? on_str : off_str);
#endif
	len += sprintf(page+len, "Tx_Desc_Count\t\t\t%u\n",
		pDevice->TxPacketDescCnt);
	len += sprintf(page+len, "Rx_Desc_Count\t\t\t%u\n",
		pDevice->RxStdDescCnt);
#if T3_JUMBO_RCV_RCB_ENTRY_COUNT
	len += sprintf(page+len, "Rx_Jumbo_Desc_Count\t\t%u\n",
		pDevice->RxJumboDescCnt);
#endif
#ifdef BCM_INT_COAL
	len += sprintf(page+len, "Adaptive_Coalescing\t\t%s\n",
		(pUmDevice->adaptive_coalesce ? on_str : off_str));
	len += sprintf(page+len, "Rx_Coalescing_Ticks\t\t%u\n",
		pUmDevice->rx_curr_coalesce_ticks);
	len += sprintf(page+len, "Rx_Coalesced_Frames\t\t%u\n",
		pUmDevice->rx_curr_coalesce_frames);
	len += sprintf(page+len, "Tx_Coalescing_Ticks\t\t%u\n",
		pDevice->TxCoalescingTicks);
	len += sprintf(page+len, "Tx_Coalesced_Frames\t\t%u\n",
		pUmDevice->tx_curr_coalesce_frames);
	len += sprintf(page+len, "Stats_Coalescing_Ticks\t\t%u\n",
		pDevice->StatsCoalescingTicks);
#endif
#ifdef BCM_WOL
	len += sprintf(page+len, "Wake_On_LAN\t\t\t%s\n",
        	((pDevice->WakeUpMode & LM_WAKE_UP_MODE_MAGIC_PACKET) ?
		on_str : off_str));
#endif
#if TIGON3_DEBUG
	len += sprintf(page+len, "\nDmaReadWriteCtrl\t\t%x\n",
		pDevice->DmaReadWriteCtrl);
	len += sprintf(page+len, "\nTx_Zero_Copy_Packets\t\t%lu\n",
		pUmDevice->tx_zc_count);
	len += sprintf(page+len, "Tx_Chksum_Packets\t\t%lu\n",
		pUmDevice->tx_chksum_count);
	len += sprintf(page+len, "Tx_Highmem_Fragments\t\t%lu\n",
		pUmDevice->tx_himem_count);
	len += sprintf(page+len, "Rx_Good_Chksum_Packets\t\t%lu\n",
		pUmDevice->rx_good_chksum_count);
	len += sprintf(page+len, "Rx_Bad_Chksum_Packets\t\t%lu\n",
		pUmDevice->rx_bad_chksum_count);

#endif

	*eof = 1;
	return len;
}

int
bcm5700_proc_create_dev(struct net_device *dev)
{
	PUM_DEVICE_BLOCK pUmDevice = (PUM_DEVICE_BLOCK) dev->priv;

	if (!bcm5700_procfs_dir)
		return -1;

	sprintf(pUmDevice->pfs_name, "%s.info", dev->name);
	pUmDevice->pfs_entry = create_proc_entry(pUmDevice->pfs_name,
		S_IFREG, bcm5700_procfs_dir);
	if (pUmDevice->pfs_entry == 0)
		return -1;
	pUmDevice->pfs_entry->read_proc = bcm5700_read_pfs;
	pUmDevice->pfs_entry->data = dev;
	return 0;
}
int
bcm5700_proc_remove_dev(struct net_device *dev)
{
	PUM_DEVICE_BLOCK pUmDevice = (PUM_DEVICE_BLOCK) dev->priv;

	remove_proc_entry(pUmDevice->pfs_name, bcm5700_procfs_dir);
	return 0;
}

#endif
