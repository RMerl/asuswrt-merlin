/* 
* InterfaceMacros.h
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


#ifndef _INTERFACE_MACROS_H
#define _INTERFACE_MACROS_H

#define BCM_USB_MAX_READ_LENGTH 2048

#define MAXIMUM_USB_TCB      128
#define MAXIMUM_USB_RCB 	 128

#define MAX_BUFFERS_PER_QUEUE   256

#define MAX_DATA_BUFFER_SIZE    2048

//Num of Asynchronous reads pending
#define NUM_RX_DESC 64

#define SYS_CFG 0x0F000C00

#endif
