/* 
* InterfaceIdleMode.c
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
/*
Function:				InterfaceIdleModeWakeup

Description:			This is the hardware specific Function for waking up HW device from Idle mode.
						A software abort pattern is written to the device to wake it and necessary power state
						transitions from host are performed here. 

Input parameters:		IN PMINI_ADAPTER Adapter   - Miniport Adapter Context
						

Return:				BCM_STATUS_SUCCESS - If Wakeup of the HW Interface was successful.
						Other           - If an error occured.
*/


/*
Function:				InterfaceIdleModeRespond

Description:			This is the hardware specific Function for responding to Idle mode request from target.
						Necessary power state transitions from host for idle mode or other device specific
						initializations are performed here. 

Input parameters:		IN PMINI_ADAPTER Adapter   - Miniport Adapter Context
						

Return:				BCM_STATUS_SUCCESS - If Idle mode response related HW configuration was successful.
						Other           - If an error occured.
*/

/*
"dmem bfc02f00  100" tells how many time device went in Idle mode.
this value will be at address bfc02fa4.just before value d0ea1dle.

Set time value by writing at bfc02f98 7d0

checking the Ack timer expire on kannon by running command
d qcslog .. if it shows e means host has not send response to f/w with in 200 ms. Response should be 
send to f/w with in 200 ms after the Idle/Shutdown req issued 

*/

int InterfaceIdleModeRespond(PMINI_ADAPTER Adapter,int* puiBuffer)
{
	int	status = STATUS_SUCCESS;
	unsigned int	uiRegRead = 0;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"SubType of Message :0x%X", ntohl(*puiBuffer));
	
	if(ntohl(*puiBuffer) == GO_TO_IDLE_MODE_PAYLOAD)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL," Got GO_TO_IDLE_MODE_PAYLOAD(210) Msg Subtype");
		if(ntohl(*(puiBuffer+1)) == 0 )
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Got IDLE MODE WAKE UP Response From F/W");
			
			status = wrmalt (Adapter,SW_ABORT_IDLEMODE_LOC, &uiRegRead, sizeof(uiRegRead));
			if(status)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "wrm failed while clearing Idle Mode Reg");
				return status;
			}
			
			if(Adapter->ulPowerSaveMode == DEVICE_POWERSAVE_MODE_AS_MANUAL_CLOCK_GATING)
			{
				uiRegRead = 0x00000000 ;
				status = wrmalt (Adapter,DEBUG_INTERRUPT_GENERATOR_REGISTOR, &uiRegRead, sizeof(uiRegRead));
				if(status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "wrm failed while clearing Idle Mode	Reg");
					return status;
				}
			}
			//Below Register should not br read in case of Manual and Protocol Idle mode.
			else if(Adapter->ulPowerSaveMode != DEVICE_POWERSAVE_MODE_AS_PROTOCOL_IDLE_MODE)
			{
				//clear on read Register
				status = rdmalt(Adapter, DEVICE_INT_OUT_EP_REG0, &uiRegRead, sizeof(uiRegRead));
				if(status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "rdm failed while clearing H/W Abort Reg0");
					return status;
				}	
				//clear on read Register
				status = rdmalt (Adapter, DEVICE_INT_OUT_EP_REG1, &uiRegRead, sizeof(uiRegRead));
				if(status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "rdm failed while clearing H/W Abort	Reg1");
					return status;
				}
			}	
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Device Up from Idle Mode");
			
			// Set Idle Mode Flag to False and Clear IdleMode reg.
			Adapter->IdleMode = FALSE;
			Adapter->bTriedToWakeUpFromlowPowerMode = FALSE;
			
			wake_up(&Adapter->lowpower_mode_wait_queue);
		#if 0	
			if(Adapter->LEDInfo.led_thread_running & BCM_LED_THREAD_RUNNING_ACTIVELY)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"LED Thread is Running. Hence Setting the LED Event as IDLEMODE_EXIT");
				Adapter->DriverState = IDLEMODE_EXIT;
				wake_up(&Adapter->LEDInfo.notify_led_event);
			}
		#endif	
			
		}
		else
		{			
			if(TRUE == Adapter->IdleMode)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Device is already in Idle mode....");
				return status ;
			}
				
			uiRegRead = 0;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Got Req from F/W to go in IDLE mode \n");
			
			if (Adapter->chip_id== BCS220_2 || 
				Adapter->chip_id == BCS220_2BC ||
					Adapter->chip_id== BCS250_BC ||
					Adapter->chip_id== BCS220_3) 
			{
				
				status = rdmalt(Adapter, HPM_CONFIG_MSW, &uiRegRead, sizeof(uiRegRead));
				if(status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "rdm failed while Reading HPM_CONFIG_LDO145 Reg 0\n");
					return status;
				}	
				
				
				uiRegRead |= (1<<17);

				status = wrmalt (Adapter,HPM_CONFIG_MSW, &uiRegRead, sizeof(uiRegRead));
				if(status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "wrm failed while clearing Idle Mode Reg\n");
					return status;
				}	
				
			}
			SendIdleModeResponse(Adapter);
		}
	}
	else if(ntohl(*puiBuffer) == IDLE_MODE_SF_UPDATE_MSG)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "OverRiding Service Flow Params");
		OverrideServiceFlowParams(Adapter,puiBuffer);
	}
	return status;
}


VOID InterfaceWriteIdleModeWakePattern(PMINI_ADAPTER Adapter)
{
/*	BeceemWriteMemoryUshort(Adapter, Host2CPU_Mailbox_Low, 0x1d1e);
	BeceemWriteMemoryUshort(Adapter, Host2CPU_Mailbox_Low, 0x1d1e);
	BeceemWriteMemoryUshort(Adapter, Host2CPU_Mailbox_Upp, 0xd0ea);
	BeceemWriteMemoryUshort(Adapter, Host2CPU_Mailbox_Upp, 0xd0ea);*/
	return;
}

int InterfaceAbortIdlemode(PMINI_ADAPTER Adapter, unsigned int Pattern)
{
	int 	status = STATUS_SUCCESS;
	unsigned int value;
	unsigned int chip_id ;
	unsigned long timeout = 0 ,itr = 0;

	int 	lenwritten = 0;
	unsigned char aucAbortPattern[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	PS_INTERFACE_ADAPTER psInterfaceAdapter = Adapter->pvInterfaceAdapter;

	//Abort Bus suspend if its already suspended
	if((TRUE == psInterfaceAdapter->bSuspended) && (TRUE == Adapter->bDoSuspend)) 
	{	
		status = usb_autopm_get_interface(psInterfaceAdapter->interface);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Bus got wakeup..Aborting Idle mode... status:%d \n",status);
				
	}

	if((Adapter->ulPowerSaveMode == DEVICE_POWERSAVE_MODE_AS_MANUAL_CLOCK_GATING)
									||
	   (Adapter->ulPowerSaveMode == DEVICE_POWERSAVE_MODE_AS_PROTOCOL_IDLE_MODE))
	{
		//write the SW abort pattern.
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Writing pattern<%d> to SW_ABORT_IDLEMODE_LOC\n", Pattern);
		status = wrmalt(Adapter,SW_ABORT_IDLEMODE_LOC, &Pattern, sizeof(Pattern));
		if(status)
		{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"WRM to Register SW_ABORT_IDLEMODE_LOC failed..");
				return status;
		}
	}
	
	if(Adapter->ulPowerSaveMode == DEVICE_POWERSAVE_MODE_AS_MANUAL_CLOCK_GATING)
	{
		value = 0x80000000;
		status = wrmalt(Adapter,DEBUG_INTERRUPT_GENERATOR_REGISTOR, &value, sizeof(value));
		if(status) 
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"WRM to DEBUG_INTERRUPT_GENERATOR_REGISTOR Register failed");
			return status;
		}	
	}
	else if(Adapter->ulPowerSaveMode != DEVICE_POWERSAVE_MODE_AS_PROTOCOL_IDLE_MODE)
	{
		/*
		 * Get a Interrupt Out URB and send 8 Bytes Down
		 * To be Done in Thread Context.
		 * Not using Asynchronous Mechanism.
		 */
		status = usb_interrupt_msg (psInterfaceAdapter->udev, 
			usb_sndintpipe(psInterfaceAdapter->udev,
			psInterfaceAdapter->sIntrOut.int_out_endpointAddr),
			aucAbortPattern, 
			8, 
			&lenwritten, 
			5000);
		if(status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Sending Abort pattern down fails with status:%d..\n",status);
			return status;
		}
		else
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "NOB Sent down :%d", lenwritten);	
		}

		//mdelay(25);
		
		timeout= jiffies +  msecs_to_jiffies(50) ; 
		while( timeout > jiffies )
		{
			itr++ ;
			rdmalt(Adapter, CHIP_ID_REG, &chip_id, sizeof(UINT));
			if(0xbece3200==(chip_id&~(0xF0)))
			{
				chip_id = chip_id&~(0xF0);
			}
			if(chip_id == Adapter->chip_id)
				break;
		}
		if(timeout < jiffies )
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Not able to read chip-id even after 25 msec");
		}
		else
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Number of completed iteration to read chip-id :%lu", itr);
		}
						
		status = wrmalt(Adapter,SW_ABORT_IDLEMODE_LOC, &Pattern, sizeof(status));
		if(status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"WRM to Register SW_ABORT_IDLEMODE_LOC failed..");
			return status;
		}	
	}
	return status;
}
int InterfaceIdleModeWakeup(PMINI_ADAPTER Adapter)
{
	ULONG	Status = 0; 
	if(Adapter->bTriedToWakeUpFromlowPowerMode)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL, "Wake up already attempted.. ignoring\n");
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, IDLE_MODE, DBG_LVL_ALL,"Writing Low Power Mode Abort pattern to the Device\n");
		Adapter->bTriedToWakeUpFromlowPowerMode = TRUE;
		InterfaceAbortIdlemode(Adapter, Adapter->usIdleModePattern);

	}
	return Status;
}

void InterfaceHandleShutdownModeWakeup(PMINI_ADAPTER Adapter)
{
	unsigned int uiRegVal = 0;
	INT Status = 0;
	if(Adapter->ulPowerSaveMode == DEVICE_POWERSAVE_MODE_AS_MANUAL_CLOCK_GATING)
	{
		// clear idlemode interrupt.
		uiRegVal = 0;
		Status =wrmalt(Adapter,DEBUG_INTERRUPT_GENERATOR_REGISTOR, &uiRegVal, sizeof(uiRegVal));
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"WRM to DEBUG_INTERRUPT_GENERATOR_REGISTOR Failed with err :%d", Status);
			return;
		}
	}

    else
	{

        //clear Interrupt EP registers.
		Status = rdmalt(Adapter,DEVICE_INT_OUT_EP_REG0, &uiRegVal, sizeof(uiRegVal));
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"RDM of DEVICE_INT_OUT_EP_REG0 failed with Err :%d", Status);
			return;
		}

        Status = rdmalt(Adapter,DEVICE_INT_OUT_EP_REG1, &uiRegVal, sizeof(uiRegVal));
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"RDM of DEVICE_INT_OUT_EP_REG1 failed with Err :%d", Status);
			return;
		}
	}
}

