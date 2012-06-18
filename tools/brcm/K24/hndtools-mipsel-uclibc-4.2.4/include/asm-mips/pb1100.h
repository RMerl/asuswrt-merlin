/*
 * AMD Alchemy PB1100 Reference Boards
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 *
 */
#ifndef __ASM_PB1100_H
#define __ASM_PB1100_H

#define BCSR_KSEG1_ADDR 0xAE000000

/*
 * Overlay data structure of the Pb1100 board registers.
 * Registers located at physical 0E0000xx, KSEG1 0xAE0000xx
 */
typedef volatile struct
{
	/*00*/	unsigned short whoami;
			unsigned short reserved0;
	/*04*/	unsigned short status;
			unsigned short reserved1;
	/*08*/	unsigned short switches;
			unsigned short reserved2;
	/*0C*/	unsigned short resets;
			unsigned short reserved3;
	/*10*/	unsigned short pcmcia;
			unsigned short reserved4;
	/*14*/	unsigned short graphics; 
			unsigned short reserved5;
	/*18*/	unsigned short leds;
			unsigned short reserved6;
	/*1C*/	unsigned short swreset;
			unsigned short reserved7;

} BCSR;


/*
 * Register/mask bit definitions for the BCSRs
 */
#define BCSR_WHOAMI_DCID		0x000F	
#define BCSR_WHOAMI_CPLD		0x00F0
#define BCSR_WHOAMI_BOARD		0x0F00 

#define BCSR_STATUS_RS232_RI	    	0x0001 
#define BCSR_STATUS_RS232_DSR	 	0x0002 
#define BCSR_STATUS_RS232_CTS    	0x0004	
#define BCSR_STATUS_RS232_CD	   	0x0008	 
#define BCSR_STATUS_PCMCIA_VS_MASK  	0x0030 
#define BCSR_STATUS_TSC_BUSY        	0x0040 
#define BCSR_STATUS_SRAM_SIZ		0x0080 
#define BCSR_STATUS_FLASH_L_STS 	0x0100 
#define BCSR_STATUS_FLASH_H_STS 	0x0200	
#define BCSR_STATUS_ROM_H_STS   	0x0400 
#define BCSR_STATUS_ROM_L_STS   	0x0800	
#define BCSR_STATUS_FLASH_WP	    	0x1000 
#define BCSR_STATUS_SWAP_BOOT		0x2000
#define BCSR_STATUS_ROM_SIZ    		0x4000 
#define BCSR_STATUS_ROM_SEL      	0x8000	

#define BCSR_SWITCHES_DIP		0x00FF
#define BCSR_SWITCHES_DIP_1		0x0080
#define BCSR_SWITCHES_DIP_2		0x0040
#define BCSR_SWITCHES_DIP_3		0x0020
#define BCSR_SWITCHES_DIP_4		0x0010
#define BCSR_SWITCHES_DIP_5		0x0008
#define BCSR_SWITCHES_DIP_6		0x0004
#define BCSR_SWITCHES_DIP_7		0x0002
#define BCSR_SWITCHES_DIP_8		0x0001
#define BCSR_SWITCHES_ROTARY    	0x0F00
#define BCSR_SWITCHES_SDO_CL     	0x8000

#define BCSR_RESETS_PHY0		0x0001
#define BCSR_RESETS_PHY1		0x0002
#define BCSR_RESETS_DC			0x0004
#define BCSR_RESETS_RS232_RTS		0x0100
#define BCSR_RESETS_RS232_DTR   	0x0200
#define BCSR_RESETS_FIR_SEL		0x2000
#define BCSR_RESETS_IRDA_MODE_MASK	0xC000
#define BCSR_RESETS_IRDA_MODE_FULL	0x0000
#define BCSR_RESETS_IRDA_MODE_OFF	0x4000
#define BCSR_RESETS_IRDA_MODE_2_3	0x8000
#define BCSR_RESETS_IRDA_MODE_1_3	0xC000

#define BCSR_PCMCIA_PC0VPP		0x0003
#define BCSR_PCMCIA_PC0VCC		0x000C
#define BCSR_PCMCIA_PC0_DR_VEN		0x0010
#define BCSR_PCMCIA_PC0RST		0x0080
#define BCSR_PCMCIA_SEL_SD_CON0   	0x0100
#define BCSR_PCMCIA_SEL_SD_CON1   	0x0200
#define BCSR_PCMCIA_SD0_PWR		0x0400
#define BCSR_PCMCIA_SD1_PWR		0x0800
#define BCSR_PCMCIA_SD0_WP		0x4000
#define BCSR_PCMCIA_SD1_WP		0x8000

#define PB1100_G_CONTROL		0xAE000014
#define BCSR_GRAPHICS_GPX_SMPASS    	0x0010
#define BCSR_GRAPHICS_GPX_BIG_ENDIAN	0x0020
#define BCSR_GRAPHICS_GPX_RST		0x0040

#define BCSR_LEDS_DECIMALS		0x00FF
#define BCSR_LEDS_LED0			0x0100
#define BCSR_LEDS_LED1			0x0200
#define BCSR_LEDS_LED2			0x0400
#define BCSR_LEDS_LED3			0x0800

#define BCSR_SWRESET_RESET		0x0080
#define BCSR_VDDI_VDI			0x001F


 /* PCMCIA Pb1x00 specific defines */
#define PCMCIA_MAX_SOCK 0
#define PCMCIA_NUM_SOCKS (PCMCIA_MAX_SOCK+1)

/* VPP/VCC */
#define SET_VCC_VPP(VCC, VPP) (((VCC)<<2) | ((VPP)<<0))

#endif /* __ASM_PB1100_H */

