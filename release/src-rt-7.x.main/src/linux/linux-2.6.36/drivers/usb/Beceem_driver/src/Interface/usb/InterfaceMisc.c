/* 
* InterfaceMisc.c
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


#include <headers.h>

#ifndef BCM_SHM_INTERFACE

PS_INTERFACE_ADAPTER
InterfaceAdapterGet(PMINI_ADAPTER psAdapter)
{
	if(psAdapter == NULL)
	{
		return NULL;
	}
	return (PS_INTERFACE_ADAPTER)(psAdapter->pvInterfaceAdapter);
}

INT
InterfaceRDM(PS_INTERFACE_ADAPTER psIntfAdapter,
            UINT addr,
            PVOID buff,
            INT len)
{
	int retval = 0;
	USHORT usRetries = 0 ; 
	if(psIntfAdapter == NULL )
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0,"Interface Adapter is NULL");
		return -EINVAL ;
	}

	if(psIntfAdapter->psAdapter->device_removed == TRUE)
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0,"Device got removed");
		return -ENODEV;
	}

	if((psIntfAdapter->psAdapter->StopAllXaction == TRUE) && (psIntfAdapter->psAdapter->chip_id >= T3LPB))
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL,"Currently Xaction is not allowed on the bus");
		return -EACCES;
	}
	
	if(psIntfAdapter->bSuspended ==TRUE || psIntfAdapter->bPreparingForBusSuspend == TRUE)
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL,"Bus is in suspended states hence RDM not allowed..");
		return -EACCES;		
	}
	psIntfAdapter->psAdapter->DeviceAccess = TRUE ;
	do {
		retval = usb_control_msg(psIntfAdapter->udev,
				usb_rcvctrlpipe(psIntfAdapter->udev,0),
	            0x02, 
	            0xC2, 
	            (addr & 0xFFFF), 
	            ((addr >> 16) & 0xFFFF),
				buff,
	            len, 
	            5000); 

		usRetries++ ;
		if(-ENODEV == retval)
		{
			psIntfAdapter->psAdapter->device_removed =TRUE;
			break;
		}	
		
	}while((retval < 0) && (usRetries < MAX_RDM_WRM_RETIRES ) );	

	if(retval < 0)
	{	
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "RDM failed status :%d, retires :%d", retval,usRetries);
			psIntfAdapter->psAdapter->DeviceAccess = FALSE ;
			return retval;
	}
	else
	{	
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "RDM sent %d", retval);
			psIntfAdapter->psAdapter->DeviceAccess = FALSE ;
			return STATUS_SUCCESS;
	}
}

INT
InterfaceWRM(PS_INTERFACE_ADAPTER psIntfAdapter,
            UINT addr,
            PVOID buff,
            INT len)
{
	int retval = 0;
	USHORT usRetries = 0 ;
	
	if(psIntfAdapter == NULL )
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0, "Interface Adapter  is NULL");
		return -EINVAL;
	}
	if(psIntfAdapter->psAdapter->device_removed == TRUE)
	{
		
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0,"Device got removed");
		return -ENODEV;
	}

	if((psIntfAdapter->psAdapter->StopAllXaction == TRUE) && (psIntfAdapter->psAdapter->chip_id >= T3LPB))
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL,"Currently Xaction is not allowed on the bus...");
		return EACCES;
	}

	if(psIntfAdapter->bSuspended ==TRUE || psIntfAdapter->bPreparingForBusSuspend == TRUE)
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL,"Bus is in suspended states hence RDM not allowed..");
		return -EACCES; 
	}
	psIntfAdapter->psAdapter->DeviceAccess = TRUE ;
	do{
		retval = usb_control_msg(psIntfAdapter->udev,
				usb_sndctrlpipe(psIntfAdapter->udev,0),
	            0x01, 
	            0x42, 
	            (addr & 0xFFFF), 
	            ((addr >> 16) & 0xFFFF),
				buff,
	            len, 
	            5000);
		
		usRetries++ ;
		if(-ENODEV == retval)
		{
			psIntfAdapter->psAdapter->device_removed = TRUE ;
			break;
		}
		
	}while((retval < 0) && ( usRetries < MAX_RDM_WRM_RETIRES));

	if(retval < 0)
	{	
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "WRM failed status :%d, retires :%d", retval, usRetries);
		psIntfAdapter->psAdapter->DeviceAccess = FALSE ;
		return retval;
	}
	else
	{
		psIntfAdapter->psAdapter->DeviceAccess = FALSE ;
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "WRM sent %d", retval);
		return STATUS_SUCCESS;
			
	}
	
}

INT
BcmRDM(PVOID arg,
			UINT addr,
			PVOID buff,
			INT len)
{
	return InterfaceRDM((PS_INTERFACE_ADAPTER)arg, addr, buff, len);
}

INT
BcmWRM(PVOID arg,
			UINT addr,
			PVOID buff,
			INT len)
{
	return InterfaceWRM((PS_INTERFACE_ADAPTER)arg, addr, buff, len);
}



INT Bcm_clear_halt_of_endpoints(PMINI_ADAPTER Adapter)
{
	PS_INTERFACE_ADAPTER psIntfAdapter = (PS_INTERFACE_ADAPTER)(Adapter->pvInterfaceAdapter);
	INT status = STATUS_SUCCESS ;

	/*
		 usb_clear_halt - tells device to clear endpoint halt/stall condition
		 @dev: device whose endpoint is halted
		 @pipe: endpoint "pipe" being cleared
		 @ Context: !in_interrupt ()

		usb_clear_halt is the synchrnous call and returns 0 on success else returns with error code.
		This is used to clear halt conditions for bulk and interrupt endpoints only.
		 Control and isochronous endpoints never halts.
		 
		Any URBs  queued for such an endpoint should normally be unlinked by the driver
		before clearing the halt condition.

	*/

	//Killing all the submitted urbs to different end points.	
	Bcm_kill_all_URBs(psIntfAdapter);	


	//clear the halted/stalled state for every end point
	status = usb_clear_halt(psIntfAdapter->udev,psIntfAdapter->sIntrIn.int_in_pipe);
	if(status != STATUS_SUCCESS)
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Interrupt IN end point. :%d ", status);

	status = usb_clear_halt(psIntfAdapter->udev,psIntfAdapter->sBulkIn.bulk_in_pipe);
	if(status != STATUS_SUCCESS)
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Bulk IN end point. :%d ", status);
	
	status = usb_clear_halt(psIntfAdapter->udev,psIntfAdapter->sBulkOut.bulk_out_pipe);
	if(status != STATUS_SUCCESS)
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Bulk OUT end point. :%d ", status);

	return status ;
}


VOID Bcm_kill_all_URBs(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	struct urb *tempUrb = NULL;
	UINT i;
    /**
	  *     usb_kill_urb - cancel a transfer request and wait for it to finish
 	  *     @urb: pointer to URB describing a previously submitted request,
 	  *	  returns nothing as it is void returned API.
 	  *     
	  * 	 This routine cancels an in-progress request. It is guaranteed that
 	  *     upon return all completion handlers will have finished and the URB
 	  *     will be totally idle and available for reuse

 	  *	This routine may not be used in an interrupt context (such as a bottom
 	  *	 half or a completion handler), or when holding a spinlock, or in other
 	  *	 situations where the caller can't schedule().	
	  *		  
	**/

	/* Cancel submitted Interrupt-URB's */
	if(psIntfAdapter->psInterruptUrb != NULL)
	{
		if(psIntfAdapter->psInterruptUrb->status == -EINPROGRESS)
				 usb_kill_urb(psIntfAdapter->psInterruptUrb);
	}
	
	/* Cancel All submitted TX URB's */
	BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0, "Cancelling All Submitted TX Urbs \n");

    for(i = 0; i < MAXIMUM_USB_TCB; i++)
	{
		tempUrb = psIntfAdapter->asUsbTcb[i].urb;
		if(tempUrb) 
		{
			if(tempUrb->status == -EINPROGRESS)
				usb_kill_urb(tempUrb);
		}
	}

	
    BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0, "Cancelling All submitted Rx Urbs \n");
    
	for(i = 0; i < MAXIMUM_USB_RCB; i++)
	{
		tempUrb = psIntfAdapter->asUsbRcb[i].urb;
		if(tempUrb) 
		{
			if(tempUrb->status == -EINPROGRESS)
					usb_kill_urb(tempUrb);
		}
	}

//waiting for 10 ms or until all previously submitted tx urb Callback got called.
// as for each submitted urb there is callback and we are decreasing the uNumTcbUsed count it will reach to Zero.

	i=0;

    while(i < 10)
    {
		printk("\n<<<<No of tries in the tx side:%d>>>\n",i);
		if (atomic_read(&psIntfAdapter->uNumTcbUsed) == 0)
            break;
        
        msleep(1);
		
        i++;
    }
    

	atomic_set(&psIntfAdapter->uCurrTcb, 0);

	i=0;

	while(i < 10)
    {
		printk("\n<<<<No of tries in the Rx side:%d>>>\n",i);
    	if (atomic_read(&psIntfAdapter->uNumRcbUsed) == 0)
            break;
        
        msleep(1);
	
        i++;
    }
    
	atomic_set(&psIntfAdapter->uCurrRcb, 0);	

	BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0, "TCB: used- %d cur-%d\n", atomic_read(&psIntfAdapter->uNumTcbUsed), atomic_read(&psIntfAdapter->uCurrTcb));	
	BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_PRINTK, 0, 0, "RCB: used- %d cur-%d\n", atomic_read(&psIntfAdapter->uNumRcbUsed), atomic_read(&psIntfAdapter->uCurrRcb));
	
}

int InterruptOut(PMINI_ADAPTER Adapter, PUCHAR pData)
{
    
    int 	status = STATUS_SUCCESS;
	int 	lenwritten = 0;
    PS_INTERFACE_ADAPTER psInterfaceAdapter = Adapter->pvInterfaceAdapter;
    
    if( FALSE == Adapter->bShutStatus &&
		FALSE == Adapter->IdleMode &&
		FALSE == Adapter->bPreparingForLowPowerMode &&
        FALSE == Adapter->device_removed)   
    {

	    status = usb_interrupt_msg (psInterfaceAdapter->udev, 
			     usb_sndintpipe(psInterfaceAdapter->udev,
            	 psInterfaceAdapter->sIntrOut.int_out_endpointAddr),
       			 pData, 
		    	 8, 
			     &lenwritten, 
			     5000);
    }
	if(status)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Sending clear pattern down fails with status:%d..\n",status);
		return status;
	}
	else
	{
	    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "NOB Sent down :%d", lenwritten);	
        return STATUS_SUCCESS;
	}


} /* InterruptOut() */

void shiftDetect(PMINI_ADAPTER Adapter) 
{   

    ULONG CurrentTime = 0;
    ULONG ulResetPattern[] = {HANDSHAKE_PATTERN1,HANDSHAKE_PATTERN2};
    UINT  uiRegRead = 0;
    int   status = 0;
    static ULONG uiHandShakeTime = 0;

    if( TRUE == Adapter->fw_download_done &&
	    FALSE == Adapter->bShutStatus &&
		FALSE == Adapter->IdleMode &&
        FALSE == Adapter->device_removed)   
    {

        //Need to read the one register to produce issue....

        status = rdmaltWithLock(Adapter,DUMMY_REG, &uiRegRead, sizeof(uiRegRead));
                
        if(status < 0) 
        {
        	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "%s:%d RDM failed\n", __FUNCTION__, __LINE__);
    	}
		else 
        {
            status = rdmaltWithLock(Adapter,CHIP_ID_REG, &uiRegRead, sizeof(uiRegRead));
            if(status < 0) 
            {
			    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "%s:%d RDM failed\n", __FUNCTION__, __LINE__);
	   		}
            else  
            { 
                if(Adapter->chip_id != uiRegRead)   
                {
                    CurrentTime = jiffies*1000/HZ;

					/* 
						Something wrong, not able to read chip id, lets try to reset and recover. 
						if does not recover after reset, try reset in TIME_TO_RESET intervals.
					*/
					
                    if((CurrentTime - uiHandShakeTime) >= TIME_TO_RESET)
					{
                        UINT uiRetries = 0;

						// Hold the RDMWRM Mutex so that RDM/WRM cannot happen while reset.
  					    
						/* 
						To ensure that InterruptOut doesn't get called 
						when device is in Idle mode...
						*/
                        			
						down(&Adapter->rdmwrmsync);  

						Adapter->StopAllXaction = TRUE;

						Bcm_kill_all_URBs(Adapter->pvInterfaceAdapter);	

						// As we are reseting the UDMA, existing mailbox counters wont be valid.
						// So mark the existing counters as used. 
						//  

						atomic_set(&Adapter->CurrNumFreeTxDesc, 0);

						((PS_INTERFACE_ADAPTER)(Adapter->pvInterfaceAdapter))->bUDMAResetDone = FALSE;

						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Resetting UDMA: Sending the pattern to INT OUT\n");

						/*  Send InterruptOut pattern to reset  */
						InterruptOut(Adapter,(PUCHAR)&ulResetPattern[0]);

						Adapter->StopAllXaction = FALSE;

						if(StartInterruptUrb((PS_INTERFACE_ADAPTER)Adapter->pvInterfaceAdapter))
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL, "Cannot send interrupt in URB");
						}

						while(FALSE == (((PS_INTERFACE_ADAPTER)(Adapter->pvInterfaceAdapter))->bUDMAResetDone))
						{
							uiRetries++;
							if(uiRetries >= UDMA_RST_HNDSHK_RETRY_CNT)
							{
							//
							// Handshake failed...perhaps older firmware.
							//						
								BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Resetting UDMA: Handshake failed...perhaps old fw\n");
								break;
							}
							msleep(10);
						}

						uiHandShakeTime = CurrentTime; 
						up(&Adapter->rdmwrmsync); 

					}

				}
			} 
		}
	}          
}

#ifdef CONFIG_USB_SUSPEND
VOID putUsbSuspend(struct work_struct *work)
{
	PS_INTERFACE_ADAPTER psIntfAdapter = NULL ;
	struct usb_interface *intf = NULL ;
	psIntfAdapter = container_of(work, S_INTERFACE_ADAPTER,usbSuspendWork);
	intf=psIntfAdapter->interface ;
		
	if(psIntfAdapter->bSuspended == FALSE) 
	{
		usb_autopm_put_interface(intf);
		if(intf->is_active == 0)	
		{
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_TX, NEXT_SEND, DBG_LVL_ALL, "Interface Suspend Completely\n");
		}
		else
		{
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_TX, NEXT_SEND, DBG_LVL_ALL,"Interface suspend failed..");
		}
		
	}
	else
	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter,DBG_TYPE_TX, NEXT_SEND, DBG_LVL_ALL, "Interface Resumed Completely\n");
	}
	
}
#else
VOID putUsbSuspend(struct work_struct *work)
{
	return;
}
#endif
#endif
