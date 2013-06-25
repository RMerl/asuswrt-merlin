/* 
* Bcmnet.c
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


#include<headers.h>

static INT bcm_notify_event(struct notifier_block *nb, ULONG event, PVOID dev)
{
	struct net_device *ndev = (struct net_device*)dev;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	//PMINI_ADAPTER 	Adapter = (PMINI_ADAPTER)ndev->priv;
	if(strncmp(ndev->name,gblpnetdev->name,5)==0)
	{
		switch(event)
		{
			case NETDEV_CHANGEADDR:
			case NETDEV_GOING_DOWN:
				/*ignore this */
					break;	
			case NETDEV_DOWN:
				break;
				
			case NETDEV_UP:
				break; 

			case NETDEV_REGISTER:
				 /* Increment the Reference Count for "veth0" */
				 BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Register RefCount: %x\n",
									atomic_read(&ndev->refcnt));
				 atomic_inc(&ndev->refcnt);
				 break;

			case NETDEV_UNREGISTER:
				 /* Decrement the Reference Count for "veth0" */
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Unregister RefCnt: %x\n",
									atomic_read(&ndev->refcnt));
				atomic_dec(&ndev->refcnt);
				if((int)atomic_read(&ndev->refcnt) < 0)
					atomic_set(&ndev->refcnt, 0);
				break;
		};
	}
	return NOTIFY_DONE;
}

/* Notifier block to receive netdevice events */
static struct notifier_block bcm_notifier_block = 
{
	.notifier_call = bcm_notify_event,
};

struct net_device *gblpnetdev;
/***************************************************************************************/
/* proto-type of lower function */
#ifdef BCM_SHM_INTERFACE
const char *bcmVirtDeviceName="bcmeth";
#endif

static INT bcm_open(struct net_device *dev)
{
    PMINI_ADAPTER Adapter = NULL ; //(PMINI_ADAPTER)dev->priv;
	Adapter = GET_BCM_ADAPTER(dev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "======>");
    if(Adapter->fw_download_done==FALSE)
        return -EINVAL;
	Adapter->if_up=1;
	if(Adapter->LinkUpStatus == 1){
		if(netif_queue_stopped(Adapter->dev)){
			netif_carrier_on(Adapter->dev);
			netif_start_queue(Adapter->dev);
		}
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "<======");
    return 0;
}

static INT bcm_close(struct net_device *dev)
{
   PMINI_ADAPTER Adapter = NULL ;//gpadapter ;
   Adapter = GET_BCM_ADAPTER(dev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "=====>");
	Adapter->if_up=0;
	if(!netif_queue_stopped(dev)) {
		netif_carrier_off(dev);
	    netif_stop_queue(dev);
	}
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,"<=====");
    return 0;
}

static struct net_device_stats *bcm_get_stats(struct net_device *dev)
{
    PLINUX_DEP_DATA pLinuxData=NULL;
	PMINI_ADAPTER Adapter = NULL ;// gpadapter ;
	Adapter = GET_BCM_ADAPTER(dev);
    pLinuxData = (PLINUX_DEP_DATA)(Adapter->pvOsDepData);
	
    //BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Dev = %p, pLinuxData = %p", dev, pLinuxData);
	pLinuxData->netstats.rx_packets=atomic_read(&Adapter->RxRollOverCount)*64*1024+Adapter->PrevNumRecvDescs;
	pLinuxData->netstats.rx_bytes=atomic_read(&Adapter->GoodRxByteCount)+atomic_read(&Adapter->BadRxByteCount);
	pLinuxData->netstats.rx_dropped=atomic_read(&Adapter->RxPacketDroppedCount);
	pLinuxData->netstats.rx_errors=atomic_read(&Adapter->RxPacketDroppedCount);
	pLinuxData->netstats.rx_length_errors=0;
	pLinuxData->netstats.rx_frame_errors=0;
	pLinuxData->netstats.rx_crc_errors=0;
	pLinuxData->netstats.tx_bytes=atomic_read(&Adapter->GoodTxByteCount);
	pLinuxData->netstats.tx_packets=atomic_read(&Adapter->TxTotalPacketCount);
	pLinuxData->netstats.tx_dropped=atomic_read(&Adapter->TxDroppedPacketCount);
	
    return &(pLinuxData->netstats);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31) 
static const struct net_device_ops bcm_netdev_ops = {
.ndo_open 	= bcm_open,
.ndo_stop	= bcm_close,
.ndo_get_stats 	= bcm_get_stats,
.ndo_start_xmit = bcm_transmit,
};
#endif


/**
@ingroup init_functions
Register other driver entry points with the kernel
*/
int register_networkdev(PMINI_ADAPTER Adapter)
{
	int result=0, reg_to_os = TRUE;
#ifdef BCM_SHM_INTERFACE
	int value = 0;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) 	
	int *temp = NULL ;
#endif
	Adapter->dev = alloc_etherdev(sizeof(PMINI_ADAPTER));
	if(!Adapter->dev)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "ERR: No Dev");
		return -ENOMEM;
	} 
	gblpnetdev							= Adapter->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) 
	Adapter->dev->priv      			= Adapter;
#else
	temp = netdev_priv(Adapter->dev);
	*temp = (UINT)Adapter;
#endif	
	//BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "init adapterptr: %x %x\n", (UINT)Adapter, temp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31) 
	Adapter->dev->open      			= bcm_open;
	Adapter->dev->stop               	= bcm_close;
	Adapter->dev->get_stats          	= bcm_get_stats;
	Adapter->dev->hard_start_xmit    	= bcm_transmit;
#else
    Adapter->dev->netdev_ops = &bcm_netdev_ops;
#endif	

	Adapter->dev->hard_header_len    	= ETH_HLEN + LEADER_SIZE; 
	Adapter->dev->mtu					= MTU_SIZE;

	/* Read the MAC Address from EEPROM */
	ReadMacAddressFromNVM(Adapter);


#ifndef BCM_SHM_INTERFACE

	/* Register the notifier block for getting netdevice events */	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Registering netdevice notifier\n");
	result = register_netdevice_notifier(&bcm_notifier_block);
	if(result)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "BCM Notifier Block did not get registered");
		Adapter->bNetdeviceNotifierRegistered = FALSE;
		return result;
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "BCM Notifier got Registered");
		Adapter->bNetdeviceNotifierRegistered = TRUE;
	}

#else

	strcpy(Adapter->dev->name, bcmVirtDeviceName); //Copy the device name
	rdmalt(Adapter,VEOSHM_OLD_SCHEME_START, &value, sizeof(value)); 

	if((Adapter->CPUId == WP_CPU_ID) && (value != CPE_O_SCHEME_MAGIC_PATTERN))
		reg_to_os = FALSE;

#endif

	if(reg_to_os == FALSE)
	{
		printk("Not registering to OS\n");
		Adapter->if_up = 1;
	}
	else
	{
		result = register_netdev(Adapter->dev);
		if (!result)
		{
			Adapter->bNetworkInterfaceRegistered = TRUE ;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Beceem Network device name is %s!", Adapter->dev->name);
		}
		else
		{
	    	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Network device can not be registered!");
			Adapter->bNetworkInterfaceRegistered = FALSE ;
			return result;
		}
	}
#if 0
 Adapter->stDebugState.debug_level = DBG_LVL_CURR;
 Adapter->stDebugState.type =(UINT)0xffffffff;
 Adapter->stDebugState.subtype[DBG_TYPE_OTHERS] = 0xffffffff;
 Adapter->stDebugState.subtype[DBG_TYPE_RX] = 0xffffffff;
 Adapter->stDebugState.subtype[DBG_TYPE_TX] = 0xffffffff;
 Adapter->stDebugState.subtype[DBG_TYPE_INITEXIT] = 0xffffffff;

 printk("-------ps_adapter->stDebugState.type=%x\n",Adapter->stDebugState.type);
 printk("-------ps_adapter->stDebugState.subtype[DBG_TYPE_OTHERS]=%x\n",Adapter->stDebugState.subtype[DBG_TYPE_OTHERS]);
 printk("-------ps_adapter->stDebugState.subtype[DBG_TYPE_RX]=%x\n",Adapter->stDebugState.subtype[DBG_TYPE_RX]);
 printk("-------ps_adapter->stDebugState.subtype[DBG_TYPE_TX]=%x\n",Adapter->stDebugState.subtype[DBG_TYPE_TX]);
#endif
    
	return 0;
}

void bcm_unregister_networkdev(PMINI_ADAPTER Adapter)
{
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Unregistering the Net Dev...\n");
	if(Adapter->dev && !IS_ERR(Adapter->dev) && Adapter->bNetworkInterfaceRegistered)
		unregister_netdev(Adapter->dev);
		/* Unregister the notifier block */
	if(Adapter->bNetdeviceNotifierRegistered == TRUE)
	{
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Unregistering netdevice notifier\n");
			unregister_netdevice_notifier(&bcm_notifier_block);
  }
}

static int bcm_init(void) 
{
	int result;
   	result = InterfaceInitialize();
	if(result)
	{
 		printk("Initialisation failed for usbbcm");
	}
	else
	{
		printk("Initialised usbbcm");
	}
	return result;
}


static void bcm_exit(void) 
{
    printk("%s %s Calling InterfaceExit\n",__FILE__, __FUNCTION__);
	InterfaceExit();
    printk("%s %s InterfaceExit returned\n",__FILE__, __FUNCTION__);
}

module_init(bcm_init);
module_exit(bcm_exit);
MODULE_LICENSE ("GPL");


