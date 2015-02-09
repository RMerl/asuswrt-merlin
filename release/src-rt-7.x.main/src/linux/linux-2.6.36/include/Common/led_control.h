/* 
* led_control.h
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


#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

/*************************TYPE DEF**********************/
#define NUM_OF_LEDS 4

#define DSD_START_OFFSET                     0x0200
#define EEPROM_VERSION_OFFSET           	 0x020E
#define EEPROM_HW_PARAM_POINTER_ADDRESS 	 0x0218
#define EEPROM_HW_PARAM_POINTER_ADDRRES_MAP5 0x0220
#define GPIO_SECTION_START_OFFSET        	 0x03

#define COMPATIBILITY_SECTION_LENGTH         42
#define COMPATIBILITY_SECTION_LENGTH_MAP5    84


#define EEPROM_MAP5_MAJORVERSION             5
#define EEPROM_MAP5_MINORVERSION             0


#define MAX_NUM_OF_BLINKS 					10
#define NUM_OF_GPIO_PINS 					16

#define DISABLE_GPIO_NUM 					0xFF
#define EVENT_SIGNALED 						1

#define MAX_FILE_NAME_BUFFER_SIZE 			100

#define TURN_ON_LED(GPIO, index) do{ \
							UINT gpio_val = GPIO; \
							(Adapter->LEDInfo.LEDState[index].BitPolarity == 1) ? \
							wrmaltWithLock(Adapter,BCM_GPIO_OUTPUT_SET_REG, &gpio_val ,sizeof(gpio_val)) : \
							wrmaltWithLock(Adapter,BCM_GPIO_OUTPUT_CLR_REG, &gpio_val, sizeof(gpio_val)); \
						}while(0);
		
#define TURN_OFF_LED(GPIO, index)  do { \
							UINT gpio_val = GPIO; \
							(Adapter->LEDInfo.LEDState[index].BitPolarity == 1) ? \
							wrmaltWithLock(Adapter,BCM_GPIO_OUTPUT_CLR_REG,&gpio_val ,sizeof(gpio_val)) : \
							wrmaltWithLock(Adapter,BCM_GPIO_OUTPUT_SET_REG,&gpio_val ,sizeof(gpio_val));  \
						}while(0);

#define B_ULONG32 unsigned long

/*******************************************************/


typedef enum _LEDColors{
	RED_LED = 1, 
	BLUE_LED = 2,
	YELLOW_LED = 3,
	GREEN_LED = 4
} LEDColors; 				/*Enumerated values of different LED types*/

typedef enum LedEvents {
	SHUTDOWN_EXIT = 0x00,	
	DRIVER_INIT = 0x1, 
	FW_DOWNLOAD = 0x2,
	FW_DOWNLOAD_DONE = 0x4,
	NO_NETWORK_ENTRY = 0x8,
	NORMAL_OPERATION = 0x10,
	LOWPOWER_MODE_ENTER = 0x20,
	IDLEMODE_CONTINUE = 0x40,
	IDLEMODE_EXIT = 0x80,
	LED_THREAD_INACTIVE = 0x100,  //Makes the LED thread Inactivce. It wil be equivallent to putting the thread on hold.
	LED_THREAD_ACTIVE = 0x200    //Makes the LED Thread Active back. 
} LedEventInfo_t;			/*Enumerated values of different driver states*/

#define DRIVER_HALT 0xff


/*Structure which stores the information of different LED types
 *  and corresponding LED state information of driver states*/
typedef struct LedStateInfo_t
{
	UCHAR LED_Type; /* specify GPIO number - use 0xFF if not used */
	UCHAR LED_On_State; /* Bits set or reset for different states */
	UCHAR LED_Blink_State; /* Bits set or reset for blinking LEDs for different states */
	UCHAR GPIO_Num;
	UCHAR 			BitPolarity;				/*To represent whether H/W is normal polarity or reverse
											  polarity*/
}LEDStateInfo, *pLEDStateInfo; 


typedef struct _LED_INFO_STRUCT
{
	LEDStateInfo	LEDState[NUM_OF_LEDS];
	BOOLEAN 		bIdleMode_tx_from_host; /*Variable to notify whether driver came out 
											from idlemode due to Host or target*/
	BOOLEAN			bIdle_led_off;
	wait_queue_head_t   notify_led_event;
	wait_queue_head_t	idleModeSyncEvent;											  
 	struct task_struct  *led_cntrl_threadid;
    int                 led_thread_running;
	BOOLEAN			bLedInitDone;
	
} LED_INFO_STRUCT, *PLED_INFO_STRUCT;
//LED Thread state.
#define BCM_LED_THREAD_DISABLED			 	 0 //LED Thread is not running.
#define BCM_LED_THREAD_RUNNING_ACTIVELY  	 1 //LED thread is running.
#define BCM_LED_THREAD_RUNNING_INACTIVELY	 2 //LED thread has been put on hold



#endif

