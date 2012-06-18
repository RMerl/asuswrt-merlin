/*
 * FIC MMP
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
#ifndef __ASM_FICMMP_H
#define __ASM_FICMMP_H

#include <linux/types.h>
#include <asm/au1000.h>
#include <asm/au1xxx_gpio.h>

// This is defined in au1000.h with bogus value
#undef AU1X00_EXTERNAL_INT

#define DBDMA_AC97_TX_CHAN DSCR_CMD0_PSC1_TX
#define DBDMA_AC97_RX_CHAN DSCR_CMD0_PSC1_RX
#define DBDMA_I2S_TX_CHAN DSCR_CMD0_PSC1_TX
#define DBDMA_I2S_RX_CHAN DSCR_CMD0_PSC1_RX
/* SPI and SMB are muxed on the Pb1200 board.
   Refer to board documentation.
 */
#define SPI_PSC_BASE        PSC0_BASE_ADDR
#define SMBUS_PSC_BASE      PSC0_BASE_ADDR
/* AC97 and I2S are muxed on the Pb1200 board.
   Refer to board documentation.
 */
#define AC97_PSC_BASE       PSC1_BASE_ADDR
#define I2S_PSC_BASE		PSC1_BASE_ADDR


/*
 * SMSC LAN91C111
 */
#define AU1XXX_SMC91111_PHYS_ADDR	(0xAC000300)
#define AU1XXX_SMC91111_IRQ			AU1000_GPIO_5

/* DC_IDE and DC_ETHERNET */
#define FICMMP_IDE_INT	AU1000_GPIO_4

#define AU1XXX_ATA_PHYS_ADDR	(0x0C800000)
#define AU1XXX_ATA_REG_OFFSET	(5)
/*
#define AU1XXX_ATA_BASE		(0x0C800000)
#define AU1XXX_ATA_END			(0x0CFFFFFF)
#define AU1XXX_ATA_MEM_SIZE		(AU1XXX_ATA_END - AU1XXX_ATA_BASE +1)

#define AU1XXX_ATA_REG_OFFSET		(5)
*/
/* VPP/VCC */
#define SET_VCC_VPP(VCC, VPP, SLOT)\
	((((VCC)<<2) | ((VPP)<<0)) << ((SLOT)*8))

	
#define FICMMP_CONFIG_BASE		0xAD000000
#define FICMMP_CONFIG_ENABLE	13

#define FICMMP_CONFIG_I2SFREQ(N)	(N<<0)
#define FICMMP_CONFIG_I2SXTAL0		(1<<0)
#define FICMMP_CONFIG_I2SXTAL1		(1<<1)
#define FICMMP_CONFIG_I2SXTAL2		(1<<2)
#define FICMMP_CONFIG_I2SXTAL3		(1<<3)
#define FICMMP_CONFIG_ADV1			(1<<4)
#define FICMMP_CONFIG_IDERST		(1<<5)
#define FICMMP_CONFIG_LCMEN			(1<<6)
#define FICMMP_CONFIG_CAMPWDN		(1<<7)
#define FICMMP_CONFIG_USBPWREN		(1<<8)
#define FICMMP_CONFIG_LCMPWREN		(1<<9)
#define FICMMP_CONFIG_TVOUTPWREN	(1<<10)
#define FICMMP_CONFIG_RS232PWREN	(1<<11)
#define FICMMP_CONFIG_LCMDATAOUT	(1<<12)
#define FICMMP_CONFIG_TVODATAOUT	(1<<13)
#define FICMMP_CONFIG_ADV3			(1<<14)
#define FICMMP_CONFIG_ADV4			(1<<15)

#define I2S_FREQ_8_192				(0x0)
#define I2S_FREQ_11_2896			(0x1)
#define I2S_FREQ_12_288				(0x2)
#define I2S_FREQ_24_576				(0x3)
//#define I2S_FREQ_12_288			(0x4)
#define I2S_FREQ_16_9344			(0x5)
#define I2S_FREQ_18_432				(0x6)
#define I2S_FREQ_36_864				(0x7)
#define I2S_FREQ_16_384				(0x8)
#define I2S_FREQ_22_5792			(0x9)
//#define I2S_FREQ_24_576			(0x10)
#define I2S_FREQ_49_152				(0x11)
//#define I2S_FREQ_24_576			(0x12)
#define I2S_FREQ_33_8688			(0x13)
//#define I2S_FREQ_36_864			(0x14)
#define I2S_FREQ_73_728				(0x15)

#define FICMMP_IDE_PWR				9
#define FICMMP_FOCUS_RST			2

static __inline void ficmmp_config_set(u16 bits)
{
	extern u16 ficmmp_config;
	//printk("set_config: %X, Old: %X, New: %X\n", bits, ficmmp_config, ficmmp_config | bits);
	ficmmp_config |= bits;
	*((u16*)FICMMP_CONFIG_BASE) = ficmmp_config;
}

static __inline void ficmmp_config_clear(u16 bits)
{
	extern u16 ficmmp_config;
//	printk("clear_config: %X, Old: %X, New: %X\n", bits, ficmmp_config, ficmmp_config & ~bits);
	ficmmp_config &= ~bits;
	*((u16*)FICMMP_CONFIG_BASE) = ficmmp_config;
}

static __inline void ficmmp_config_init(void)
{
	au1xxx_gpio_write(FICMMP_CONFIG_ENABLE, 0);	//Enable configuration latch
	ficmmp_config_set(FICMMP_CONFIG_LCMDATAOUT | FICMMP_CONFIG_TVODATAOUT | FICMMP_CONFIG_IDERST);  //Disable display data buffers
	ficmmp_config_set(FICMMP_CONFIG_I2SFREQ(I2S_FREQ_36_864));
}

static __inline u32 ficmmp_set_i2s_sample_rate(u32 rate)
{
	u32 freq;
	
	switch(rate)
	{
	case 88200: 
	case 44100:
	case  8018: freq = I2S_FREQ_11_2896; break;
	case 48000:
	case 32000: //freq = I2S_FREQ_18_432; break;
	case  8000: freq = I2S_FREQ_12_288; break;
	default:    freq = I2S_FREQ_12_288; rate = 8000; 
	}
	ficmmp_config_clear(FICMMP_CONFIG_I2SFREQ(0xF));
	ficmmp_config_set(FICMMP_CONFIG_I2SFREQ(freq));
	return rate;
}

#endif /* __ASM_FICMMP_H */

