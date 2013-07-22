/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Ethernet				File: usbeth.h
    *  
    *  Driver for USB Ethernet devices.
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

/*  *********************************************************************
    *  USB-Ethernet adapter driver includes
    ********************************************************************* */

#ifndef __usbeth_h__
#define __usbeth_h__

/* **************************************
   *  CATC Netmate adapter
   ************************************** */

#define CATC_MCAST_TBL_ADDR         0xFA80      //in Netmate's SRAM
#define CATC_GET_MAC_ADDR			0xF2
#define CATC_SET_REG		        0xFA
#define CATC_GET_REG		        0xFB
#define CATC_SET_MEM		        0xFC

#define CATC_TX_BUF_CNT_REG         0x20
#define CATC_RX_BUF_CNT_REG         0x21
#define CATC_ADV_OP_MODES_REG       0x22
#define CATC_RX_FRAME_CNT_REG       0x24

#define CATC_ETH_CTRL_REG           0x60
#define CATC_ENET_STATUS_REG        0x61
#define CATC_ETH_ADDR_0_REG         0x67        // Byte #0 (leftmost)
#define CATC_LED_CTRL_REG           0x81


/* **************************************
   *  Admtek (PEGASUS II) adapter
   ************************************** */

#define PEG_SET_REG		           0xF1
#define PEG_GET_REG		           0xF0

#define PEG_MCAST_TBL_REG          0x08
#define PEG_MAC_ADDR_0_REG         0x10
#define PEG_EEPROM_OFS_REG         0x20
#define PEG_EEPROM_DATA_REG        0x21
#define PEG_EEPROM_CTL_REG         0x23
#define PEG_PHY_ADDR_REG	       0x25
#define PEG_PHY_DATA_REG	       0x26		//& 27 for 2 bytes
#define PEG_PHY_CTRL_REG	       0x28
#define PEG_ETH_CTL0_REG           0x00
#define PEG_ETH_CTL1_REG           0x01
#define PEG_ETH_CTL2_REG           0x02
#define PEG_GPIO0_REG              0x7e
#define PEG_GPIO1_REG              0x7f
#define PEG_INT_PHY_REG			   0x7b

#define PHY_WRITE				   0x20
#define PHY_READ				   0x40


/* **************************************
   *  Realtek adapter
   ************************************** */

#define RTEK_REG_ACCESS		0x05
#define RTEK_MAC_REG		0x0120
#define RTEK_CMD_REG		0x012E
#define RTEK_RXCFG_REG		0x0130
#define RTEK_RESET			0x10
#define RTEK_AUTOLOAD		0x01



#endif //__usbeth_h_
