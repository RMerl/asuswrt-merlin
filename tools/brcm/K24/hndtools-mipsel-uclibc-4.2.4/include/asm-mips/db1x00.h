/*
 * AMD Alchemy DB1x00 Reference Boards (BUT NOT DB1200)
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
#ifndef __ASM_DB1X00_H
#define __ASM_DB1X00_H

#ifdef CONFIG_MIPS_DB1550
#define BCSR_KSEG1_ADDR 0xAF000000
#define NAND_PHYS_ADDR   0x20000000
#define DBDMA_AC97_TX_CHAN DSCR_CMD0_PSC1_TX
#define DBDMA_AC97_RX_CHAN DSCR_CMD0_PSC1_RX
#define SPI_PSC_BASE        PSC0_BASE_ADDR
#define AC97_PSC_BASE       PSC1_BASE_ADDR
#define SMBUS_PSC_BASE      PSC2_BASE_ADDR
#define I2S_PSC_BASE        PSC3_BASE_ADDR
#define NAND_CS 1
/* for drivers/pcmcia/au1000_db1x00.c */
#define BOARD_PC0_INT AU1000_GPIO_3
#define BOARD_PC1_INT AU1000_GPIO_5
#define BOARD_CARD_INSERTED(SOCKET) !(bcsr->status & (1<<(4+SOCKET)))

#else
#define BCSR_KSEG1_ADDR 0xAE000000
/* for drivers/pcmcia/au1000_db1x00.c */
#define BOARD_PC0_INT AU1000_GPIO_2
#define BOARD_PC1_INT AU1000_GPIO_5
#define BOARD_CARD_INSERTED(SOCKET) !(bcsr->status & (1<<(4+SOCKET)))
#endif

/*
 * Overlay data structure of the Db1x00 board registers.
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
	/*14*/	unsigned short specific;
			unsigned short reserved5;
	/*18*/	unsigned short leds;
			unsigned short reserved6;
	/*1C*/	unsigned short swreset;
			unsigned short reserved7;

} BCSR;

static BCSR * const bcsr = (BCSR *)BCSR_KSEG1_ADDR;

/*
 * Register/mask bit definitions for the BCSRs
 */
#define BCSR_WHOAMI_DCID		0x000F
#define BCSR_WHOAMI_CPLD		0x00F0
#define BCSR_WHOAMI_BOARD		0x0F00

#define BCSR_STATUS_PC0VS		0x0003
#define BCSR_STATUS_PC1VS		0x000C
#define BCSR_STATUS_PC0FI		0x0010
#define BCSR_STATUS_PC1FI		0x0020
#define BCSR_STATUS_FLASHBUSY		0x0100
#define BCSR_STATUS_ROMBUSY		0x0400
#define BCSR_STATUS_SWAPBOOT		0x2000
#define BCSR_STATUS_FLASHDEN		0xC000

#define BCSR_SWITCHES_DIP		0x00FF
#define BCSR_SWITCHES_DIP_1		0x0080
#define BCSR_SWITCHES_DIP_2		0x0040
#define BCSR_SWITCHES_DIP_3		0x0020
#define BCSR_SWITCHES_DIP_4		0x0010
#define BCSR_SWITCHES_DIP_5		0x0008
#define BCSR_SWITCHES_DIP_6		0x0004
#define BCSR_SWITCHES_DIP_7		0x0002
#define BCSR_SWITCHES_DIP_8		0x0001
#define BCSR_SWITCHES_ROTARY		0x0F00

#define BCSR_RESETS_PHY0		0x0001
#define BCSR_RESETS_PHY1		0x0002
#define BCSR_RESETS_DC			0x0004
#define BCSR_RESETS_FIR_SEL		0x2000
#define BCSR_RESETS_IRDA_MODE_MASK	0xC000
#define BCSR_RESETS_IRDA_MODE_FULL	0x0000
#define BCSR_RESETS_IRDA_MODE_OFF	0x4000
#define BCSR_RESETS_IRDA_MODE_2_3	0x8000
#define BCSR_RESETS_IRDA_MODE_1_3	0xC000

#define BCSR_PCMCIA_PC0VPP		0x0003
#define BCSR_PCMCIA_PC0VCC		0x000C
#define BCSR_PCMCIA_PC0DRVEN		0x0010
#define BCSR_PCMCIA_PC0RST		0x0080
#define BCSR_PCMCIA_PC1VPP		0x0300
#define BCSR_PCMCIA_PC1VCC		0x0C00
#define BCSR_PCMCIA_PC1DRVEN		0x1000
#define BCSR_PCMCIA_PC1RST		0x8000

#define BCSR_BOARD_PCIM66EN		0x0001
#define BCSR_BOARD_SD0_PWR		0x0040
#define BCSR_BOARD_SD1_PWR		0x0080
#define BCSR_BOARD_PCIM33		0x0100
#define BCSR_BOARD_GPIO200RST		0x0400
#define BCSR_BOARD_PCICFG		0x1000
#define BCSR_BOARD_SD0_WP		0x4000
#define BCSR_BOARD_SD1_WP		0x8000

#define BCSR_LEDS_DECIMALS		0x0003
#define BCSR_LEDS_LED0			0x0100
#define BCSR_LEDS_LED1			0x0200
#define BCSR_LEDS_LED2			0x0400
#define BCSR_LEDS_LED3			0x0800

#define BCSR_SWRESET_RESET		0x0080

/* MTD CONFIG OPTIONS */
#if defined(CONFIG_MTD_DB1X00_BOOT) && defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_BOTH_BANKS
#elif defined(CONFIG_MTD_DB1X00_BOOT) && !defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_BOOT_ONLY
#elif !defined(CONFIG_MTD_DB1X00_BOOT) && defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_USER_ONLY
#endif

#if defined(CONFIG_BLK_DEV_IDE_AU1XXX) && defined(CONFIG_MIPS_DB1550)
/*
 * Daughter card information.
 */
#define DAUGHTER_CARD_IRQ		(AU1000_GPIO_8)
/* DC_IDE */
#define AU1XXX_ATA_PHYS_ADDR		(0x0C000000)
#define AU1XXX_ATA_REG_OFFSET		(5)	
#endif /* CONFIG_MIPS_DB1550 */

#endif /* __ASM_DB1X00_H */

