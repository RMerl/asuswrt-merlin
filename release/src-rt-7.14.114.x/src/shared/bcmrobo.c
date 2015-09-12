/*
 * Broadcom 53xx RoboSwitch device driver.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmrobo.c 462657 2014-03-18 11:31:42Z $
 */


#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndsoc.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmparams.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#include <bcmrobo.h>
#include <proto/ethernet.h>
#include <hndpmu.h>
#ifdef BCMFA
#include <etioctl.h>
#endif

#ifdef	BCMDBG
#define	ET_ERROR(args)	printf args
#else	/* BCMDBG */
#define	ET_ERROR(args)
#endif	/* BCMDBG */
#define	ET_MSG(args)

#define VARG(var, len) (((len) == 1) ? *((uint8 *)(var)) : \
		        ((len) == 2) ? *((uint16 *)(var)) : \
		        *((uint32 *)(var)))

/*
 * Switch can be programmed through SPI interface, which
 * has a rreg and a wreg functions to read from and write to
 * registers.
 */

/* MII access registers */
#define PSEUDO_PHYAD	0x1E	/* MII Pseudo PHY address */
#define REG_MII_CTRL    0x00    /* 53115 MII control register */
#define REG_MII_CLAUSE_45_CTL1 0xd     /* 53125 MII Clause 45 control 1 */
#define REG_MII_CLAUSE_45_CTL2 0xe     /* 53125 MII Clause 45 control 2 */
#define REG_MII_PAGE	0x10	/* MII Page register */
#define REG_MII_ADDR	0x11	/* MII Address register */
#define REG_MII_DATA0	0x18	/* MII Data register 0 */
#define REG_MII_DATA1	0x19	/* MII Data register 1 */
#define REG_MII_DATA2	0x1a	/* MII Data register 2 */
#define REG_MII_DATA3	0x1b	/* MII Data register 3 */
#define REG_MII_AUX_STATUS2	0x1b	/* Auxiliary status 2 register */
#define REG_MII_AUTO_PWRDOWN	0x1c	/* 53115 Auto power down register */
#define REG_MII_BRCM_TEST	0x1f	/* Broadcom test register */

/* Page numbers */
#define PAGE_CTRL	0x00	/* Control page */
#define PAGE_STATUS	0x01	/* Status page */
#define PAGE_MMR	0x02	/* 5397 Management/Mirroring page */
#define PAGE_VTBL	0x05	/* ARL/VLAN Table access page */
#define PAGE_FC		0x0a	/* Flow control register page */
#define PAGE_GPHY_MII_P0	0x10	/* Port0 Internal GPHY MII registers page */
#define PAGE_GPHY_MII_P4	0x14	/* Last/Port4 Internal GPHY MII registers page */
#ifdef ETAGG
#define PAGE_TRUNKING	0x32	/* MAC-base Trunking registers page */
#endif
#define PAGE_VLAN	0x34	/* VLAN page */
#define PAGE_CFPTCAM	0xa0	/* CFP TCAM registers page */
#define PAGE_CFP	0xa1	/* CFP configuration registers page */

/* Control page registers */
#define REG_CTRL_PORT0	0x00	/* Port 0 traffic control register */
#define REG_CTRL_PORT1	0x01	/* Port 1 traffic control register */
#define REG_CTRL_PORT2	0x02	/* Port 2 traffic control register */
#define REG_CTRL_PORT3	0x03	/* Port 3 traffic control register */
#define REG_CTRL_PORT4	0x04	/* Port 4 traffic control register */
#define REG_CTRL_PORT5	0x05	/* Port 5 traffic control register */
#define REG_CTRL_PORT6	0x06	/* Port 6 traffic control register */
#define REG_CTRL_PORT7	0x07	/* Port 7 traffic control register */
#define REG_CTRL_IMP	0x08	/* IMP port traffic control register */
#define REG_CTRL_MODE	0x0B	/* Switch Mode register */
#define REG_CTRL_MIIPO	0x0E	/* 5325: MII Port Override register */
#define REG_CTRL_PWRDOWN 0x0F   /* 5325: Power Down Mode register */
#ifdef ETAGG
#define REG_CTRL_RSV_MCAST 0x2F /* NS: Reserved multicast register */
#endif
#define REG_CTRL_SRST	0x79	/* Software reset control register */
#ifdef PLC
#define REG_CTRL_MIIP5O 0x5d    /* 53115: Port State Override register for port 5 */

/* Management/Mirroring Registers */
#define REG_MMR_ATCR    0x06    /* Aging Time Control register */
#define REG_MMR_MCCR    0x10    /* Mirror Capture Control register */
#define REG_MMR_IMCR    0x12    /* Ingress Mirror Control register */
#endif /* PLC */

/* Management Page registers */
#define REG_MGMT_CFG	0x00
#define REG_IMP0_PORT	0x01
#define REG_IMP1_PORT	0x02
#define REG_BRCM_HDR	0x03

/* Status Page Registers */
#define REG_STATUS_LINK	0x00	/* Link Status Summary */
#ifdef ETAGG
#define REG_STATUS_SPD	0x04	/* Port Speed register */
#define REG_STATUS_DUP	0x08	/* Duplex Status register */
#endif
#define REG_STATUS_REV	0x50	/* Revision Register */

#define REG_DEVICE_ID	0x30	/* 539x Device id: */

#ifdef ETAGG
/* MAC-base Trunking registers */
#define REG_TRUNKING_CTL	0x00
#define REG_TRUNKING_GRP0	0x10
#define REG_TRUNKING_GRP1	0x12
#endif

/* VLAN page registers */
#define REG_VLAN_CTRL0	0x00	/* VLAN Control 0 register */
#define REG_VLAN_CTRL1	0x01	/* VLAN Control 1 register */
#define REG_VLAN_CTRL2	0x02	/* VLAN Control 2 register */
#define REG_VLAN_CTRL4	0x04	/* VLAN Control 4 register */
#define REG_VLAN_CTRL5	0x05	/* VLAN Control 5 register */
#define REG_VLAN_ACCESS	0x06	/* VLAN Table Access register */
#define REG_VLAN_WRITE	0x08	/* VLAN Write register */
#define REG_VLAN_READ	0x0C	/* VLAN Read register */
#define REG_VLAN_PTAG0	0x10	/* VLAN Default Port Tag register - port 0 */
#define REG_VLAN_PTAG1	0x12	/* VLAN Default Port Tag register - port 1 */
#define REG_VLAN_PTAG2	0x14	/* VLAN Default Port Tag register - port 2 */
#define REG_VLAN_PTAG3	0x16	/* VLAN Default Port Tag register - port 3 */
#define REG_VLAN_PTAG4	0x18	/* VLAN Default Port Tag register - port 4 */
#define REG_VLAN_PTAG5	0x1a	/* VLAN Default Port Tag register - port 5 */
#define REG_VLAN_PTAG6	0x1c	/* VLAN Default Port Tag register - port 6 */
#define REG_VLAN_PTAG7	0x1e	/* VLAN Default Port Tag register - port 7 */
#define REG_VLAN_PTAG8	0x20	/* 539x: VLAN Default Port Tag register - IMP port */
#define REG_VLAN_PMAP	0x20	/* 5325: VLAN Priority Re-map register */

#define VLAN_NUMVLANS	16	/* # of VLANs */


/* ARL/VLAN Table Access page registers */
#define REG_VTBL_CTRL		0x00	/* ARL Read/Write Control */
#define REG_VTBL_MINDX		0x02	/* MAC Address Index */
#define REG_VTBL_VINDX		0x08	/* VID Table Index */
#define REG_VTBL_ARL_E0		0x10	/* ARL Entry 0 */
#define REG_VTBL_ARL_E1		0x18	/* ARL Entry 1 */
#define REG_VTBL_DAT_E0		0x18	/* ARL Table Data Entry 0 */
#define REG_VTBL_SCTRL		0x20	/* ARL Search Control */
#define REG_VTBL_SADDR		0x22	/* ARL Search Address */
#define REG_VTBL_SRES		0x24	/* ARL Search Result */
#define REG_VTBL_SREXT		0x2c	/* ARL Search Result */
#define REG_VTBL_VID_E0		0x30	/* VID Entry 0 */
#define REG_VTBL_VID_E1		0x32	/* VID Entry 1 */
#define REG_VTBL_PREG		0xFF	/* Page Register */
#define REG_VTBL_ACCESS		0x60	/* VLAN table access register */
#define REG_VTBL_INDX		0x61	/* VLAN table address index register */
#define REG_VTBL_ENTRY		0x63	/* VLAN table entry register */
#define REG_VTBL_ACCESS_5395	0x80	/* VLAN table access register */
#define REG_VTBL_INDX_5395	0x81	/* VLAN table address index register */
#define REG_VTBL_ENTRY_5395	0x83	/* VLAN table entry register */

#define REG_FC_OOBPAUSE		0xe0	/* OOB Pause Signal enable register */

/* CFP TCAM page registers */
#define REG_CFPTCAM_ACC			0x00	/* CFP access register */
#define REG_CFPTCAM_DATA0		0x10	/* CFP TCAM Data 0 register */
#define REG_CFPTCAM_DATA1		0x14	/* CFP TCAM Data 1 register */
#define REG_CFPTCAM_DATA2		0x18	/* CFP TCAM Data 2 register */
#define REG_CFPTCAM_DATA3		0x1c	/* CFP TCAM Data 3 register */
#define REG_CFPTCAM_DATA4		0x20	/* CFP TCAM Data 4 register */
#define REG_CFPTCAM_DATA5		0x24	/* CFP TCAM Data 5 register */
#define REG_CFPTCAM_DATA6		0x28	/* CFP TCAM Data 6 register */
#define REG_CFPTCAM_DATA7		0x2c	/* CFP TCAM Data 7 register */
#define REG_CFPTCAM_MASK0		0x30	/* CFP TCAM Mask 0 register */
#define REG_CFPTCAM_MASK1		0x34	/* CFP TCAM Mask 1 register */
#define REG_CFPTCAM_MASK2		0x38	/* CFP TCAM Mask 2 register */
#define REG_CFPTCAM_MASK3		0x3c	/* CFP TCAM Mask 3 register */
#define REG_CFPTCAM_MASK4		0x40	/* CFP TCAM Mask 4 register */
#define REG_CFPTCAM_MASK5		0x44	/* CFP TCAM Mask 5 register */
#define REG_CFPTCAM_MASK6		0x48	/* CFP TCAM Mask 6 register */
#define REG_CFPTCAM_MASK7		0x4c	/* CFP TCAM Mask 7 register */
#define REG_CFPTCAM_ACT_POL_DATA0	0x50	/* CFP CFP Action/Policy Data 0 Register */
#define REG_CFPTCAM_ACT_POL_DATA1	0x54	/* CFP CFP Action/Policy Data 1 Register */
#define REG_CFPTCAM_RATE_METER0		0x60	/* CFP CFP RATE METER DATA 0 Register */
#define REG_CFPTCAM_RATE_METER1		0x64	/* CFP CFP RATE METER DATA 0 Register */
#define REG_CFPTCAM_RATE_INBAND		0x70	/* CFP CFP RATE In-Band Statistic Register */
#define REG_CFPTCAM_RATE_OUTBAND	0x74	/* CFP CFP RATE In-Band Statistic Register */

#define CFP_ACC_RD_STS_SHIFT		28
#define CFP_ACC_XCESS_ADDR_SHIFT	16
#define CFP_ACC_RAM_SEL_SHIFT		10
#define CFP_ACC_OP_SEL_SHIFT		1
#define CFP_ACC_OP_STR_DONE		1
#define CFP_ACT_POL_DATA0_CFMI_SHIFT	24	/* CHANGE_FWRD_MAP_IB Shift */
#define CFP_ACT_POL_DATA0_DMI_SHIFT	14	/* DST_MAP_IB Shift */
#define CFP_ACT_POL_DATA1_CFMO_SHIFT	11	/* CHANGE_FWRD_MAP_OB Shift */
#define CFP_ACT_POL_DATA1_DMO_SHIFT	1	/* DST_MAP_OB Shift */

/* CFP Configuration page registers */
#define REG_CFP_CTL_REG			0x00	/* CFP Control Register */
#define REG_CFP_UDF_0_A_0_8		0x10	/* UDFs of slice 0 for IPv4 packet Register */
#define REG_CFP_UDF_1_A_0_8		0x20	/* UDFs of slice 1 for IPv4 packet Register */
#define REG_CFP_UDF_2_A_0_8		0x30	/* UDFs of slice 2 for IPv4 packet Register */
#define REG_CFP_UDF_0_B_0_8		0x40	/* UDFs of slice 0 for IPv6 packet Register */
#define REG_CFP_UDF_1_B_0_8		0x50	/* UDFs of slice 1 for IPv6 packet Register */
#define REG_CFP_UDF_2_B_0_8		0x60	/* UDFs of slice 2 for IPv6 packet Register */
#define REG_CFP_UDF_0_C_0_8		0x70	/* UDFs of slice 0 for none-IP Register */
#define REG_CFP_UDF_1_C_0_8		0x80	/* UDFs of slice 0 for none-IP Register */
#define REG_CFP_UDF_2_C_0_8		0x90	/* UDFs of slice 0 for none-IP Register */
#define REG_CFP_UDF_0_D_0_11		0xa0	/* UDFs for IPv6 Chain Rule Register */

#define CFP_ACC_RD_STS_WAIT(robo, mask) \
do { \
	uint32 val32 = 0; \
	(robo)->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32)); \
	if ((val32 >> CFP_ACC_RD_STS_SHIFT) & (mask)) \
		break; \
	bcm_mdelay(1); \
} while (1)

/* a flag to control Manage Mode enable/disable */
static bool mang_mode_en = FALSE;

#define RXTX_FLOW_CTRL_MASK	0x3	/* 53125 flow control capability mask */
#define RXTX_FLOW_CTRL_SHIFT	4	/* 53125 flow contorl capability offset */

#ifndef	_CFE_
/* SPI registers */
#define REG_SPI_PAGE	0xff	/* SPI Page register */

/* Access switch registers through GPIO/SPI */

/* Minimum timing constants */
#define SCK_EDGE_TIME	2	/* clock edge duration - 2us */
#define MOSI_SETUP_TIME	1	/* input setup duration - 1us */
#define SS_SETUP_TIME	1 	/* select setup duration - 1us */

/* misc. constants */
#define SPI_MAX_RETRY	100

/* Enable GPIO access to the chip */
static void
gpio_enable(robo_info_t *robo)
{
	/* Enable GPIO outputs with SCK and MOSI low, SS high */
	si_gpioout(robo->sih, robo->ss | robo->sck | robo->mosi, robo->ss, GPIO_DRV_PRIORITY);
	si_gpioouten(robo->sih, robo->ss | robo->sck | robo->mosi,
	             robo->ss | robo->sck | robo->mosi, GPIO_DRV_PRIORITY);
}

/* Disable GPIO access to the chip */
static void
gpio_disable(robo_info_t *robo)
{
	/* Disable GPIO outputs with all their current values */
	si_gpioouten(robo->sih, robo->ss | robo->sck | robo->mosi, 0, GPIO_DRV_PRIORITY);
}

/* Write a byte stream to the chip thru SPI */
static int
spi_write(robo_info_t *robo, uint8 *buf, uint len)
{
	uint i;
	uint8 mask;

	/* Byte bang from LSB to MSB */
	for (i = 0; i < len; i++) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80; mask; mask >>= 1) {
			/* Clock low */
			si_gpioout(robo->sih, robo->sck, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);

			/* Sample on rising edge */
			if (mask & buf[i])
				si_gpioout(robo->sih, robo->mosi, robo->mosi, GPIO_DRV_PRIORITY);
			else
				si_gpioout(robo->sih, robo->mosi, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(MOSI_SETUP_TIME);

			/* Clock high */
			si_gpioout(robo->sih, robo->sck, robo->sck, GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);
		}
	}

	return 0;
}

/* Read a byte stream from the chip thru SPI */
static int
spi_read(robo_info_t *robo, uint8 *buf, uint len)
{
	uint i, timeout;
	uint8 rack, mask, byte;

	/* Timeout after 100 tries without RACK */
	for (i = 0, rack = 0, timeout = SPI_MAX_RETRY; i < len && timeout;) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80, byte = 0; mask; mask >>= 1) {
			/* Clock low */
			si_gpioout(robo->sih, robo->sck, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);

			/* Sample on falling edge */
			if (si_gpioin(robo->sih) & robo->miso)
				byte |= mask;

			/* Clock high */
			si_gpioout(robo->sih, robo->sck, robo->sck, GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);
		}
		/* RACK when bit 0 is high */
		if (!rack) {
			rack = (byte & 1);
			timeout--;
			continue;
		}
		/* Byte bang from LSB to MSB */
		buf[i] = byte;
		i++;
	}

	if (timeout == 0) {
		ET_ERROR(("spi_read: timeout"));
		return -1;
	}

	return 0;
}

/* Enable/disable SPI access */
static void
spi_select(robo_info_t *robo, uint8 spi)
{
	if (spi) {
		/* Enable SPI access */
		si_gpioout(robo->sih, robo->ss, 0, GPIO_DRV_PRIORITY);
	} else {
		/* Disable SPI access */
		si_gpioout(robo->sih, robo->ss, robo->ss, GPIO_DRV_PRIORITY);
	}
	OSL_DELAY(SS_SETUP_TIME);
}


/* Select chip and page */
static void
spi_goto(robo_info_t *robo, uint8 page)
{
	uint8 reg8 = REG_SPI_PAGE;	/* page select register */
	uint8 cmd8;

	/* Issue the command only when we are on a different page */
	if (robo->page == page)
		return;

	robo->page = page;

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Select new page with CID 0 */
	cmd8 = ((6 << 4) |		/* normal SPI */
	        1);			/* write */
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &reg8, 1);
	spi_write(robo, &page, 1);

	/* Disable SPI access */
	spi_select(robo, 0);
}

/* Write register thru SPI */
static int
spi_wreg(robo_info_t *robo, uint8 page, uint8 addr, void *val, int len)
{
	int status = 0;
	uint8 cmd8;
	union {
		uint8 val8;
		uint16 val16;
		uint32 val32;
	} bytes;

	if ((len != 1) && (len != 2) && (len != 4)) {
		printf("Invalid length. For SPI mode, the length can only be 1, 2, and 4 bytes.\n");
		return -1;
	}

	/* validate value length and buffer address */
	ASSERT(len == 1 || (len == 2 && !((int)val & 1)) ||
	       (len == 4 && !((int)val & 3)));

	/* Select chip and page */
	spi_goto(robo, page);

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Write with CID 0 */
	cmd8 = ((6 << 4) |		/* normal SPI */
	        1);			/* write */
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &addr, 1);
	switch (len) {
	case 1:
		bytes.val8 = *(uint8 *)val;
		break;
	case 2:
		bytes.val16 = htol16(*(uint16 *)val);
		break;
	case 4:
		bytes.val32 = htol32(*(uint32 *)val);
		break;
	}
	spi_write(robo, (uint8 *)val, len);

	ET_MSG(("%s: [0x%x-0x%x] := 0x%x (len %d)\n", __FUNCTION__, page, addr,
	        *(uint16 *)val, len));
	/* Disable SPI access */
	spi_select(robo, 0);
	return status;
}

/* Read register thru SPI in fast SPI mode */
static int
spi_rreg(robo_info_t *robo, uint8 page, uint8 addr, void *val, int len)
{
	int status = 0;
	uint8 cmd8;
	union {
		uint8 val8;
		uint16 val16;
		uint32 val32;
	} bytes;

	if ((len != 1) && (len != 2) && (len != 4)) {
		printf("Invalid length. For SPI mode, the length can only be 1, 2, and 4 bytes.\n");
		return -1;
	}

	/* validate value length and buffer address */
	ASSERT(len == 1 || (len == 2 && !((int)val & 1)) ||
	       (len == 4 && !((int)val & 3)));

	/* Select chip and page */
	spi_goto(robo, page);

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Fast SPI read with CID 0 and byte offset 0 */
	cmd8 = (1 << 4);		/* fast SPI */
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &addr, 1);
	status = spi_read(robo, (uint8 *)&bytes, len);
	switch (len) {
	case 1:
		*(uint8 *)val = bytes.val8;
		break;
	case 2:
		*(uint16 *)val = ltoh16(bytes.val16);
		break;
	case 4:
		*(uint32 *)val = ltoh32(bytes.val32);
		break;
	}

	ET_MSG(("%s: [0x%x-0x%x] => 0x%x (len %d)\n", __FUNCTION__, page, addr,
	        *(uint16 *)val, len));

	/* Disable SPI access */
	spi_select(robo, 0);
	return status;
}

/* SPI/gpio interface functions */
static dev_ops_t spigpio = {
	gpio_enable,
	gpio_disable,
	spi_wreg,
	spi_rreg,
	"SPI (GPIO)"
};
#endif /* _CFE_ */


/* Access switch registers through MII (MDC/MDIO) */

#define MII_MAX_RETRY	100

/* Write register thru MDC/MDIO */
static int
mii_wreg(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len)
{
	uint16 cmd16, val16;
	void *h = robo->h;
	int i;
	uint8 *ptr = (uint8 *)val;

	/* do not allow access to internal PHY MII registers */
	if ((page >= PAGE_GPHY_MII_P0) && (page <= PAGE_GPHY_MII_P4)) {
		ET_ERROR(("mii_wreg: cannot access MII registers through pseudo phy interface\n"));
		return -1;
	}

	/* validate value length and buffer address */
	ASSERT(len == 1 || len == 6 || len == 8 ||
	       ((len == 2) && !((int)val & 1)) || ((len == 4) && !((int)val & 3)));

	ET_MSG(("%s: [0x%x-0x%x] := 0x%x (len %d)\n", __FUNCTION__, page, reg,
	       VARG(val, len), len));

	/* set page number - MII register 0x10 */
	if (robo->page != page) {
		cmd16 = ((page << 8) |		/* page number */
		         1);			/* mdc/mdio access enable */
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_PAGE, cmd16);
		robo->page = page;
	}

	switch (len) {
	case 8:
		val16 = ptr[7];
		val16 = ((val16 << 8) | ptr[6]);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA3, val16);
		/* FALLTHRU */

	case 6:
		val16 = ptr[5];
		val16 = ((val16 << 8) | ptr[4]);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA2, val16);
		val16 = ptr[3];
		val16 = ((val16 << 8) | ptr[2]);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA1, val16);
		val16 = ptr[1];
		val16 = ((val16 << 8) | ptr[0]);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		break;

	case 4:
		val16 = (uint16)((*(uint32 *)val) >> 16);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA1, val16);
		val16 = (uint16)(*(uint32 *)val);
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		break;

	case 2:
		val16 = *(uint16 *)val;
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		break;

	case 1:
		val16 = *(uint8 *)val;
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		break;
	}

	/* set register address - MII register 0x11 */
	cmd16 = ((reg << 8) |		/* register address */
	         1);		/* opcode write */
	robo->miiwr(h, PSEUDO_PHYAD, REG_MII_ADDR, cmd16);

	/* is operation finished? */
	for (i = MII_MAX_RETRY; i > 0; i --) {
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_ADDR);
		if ((val16 & 3) == 0)
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("mii_wreg: timeout"));
		return -1;
	}
	return 0;
}

/* Read register thru MDC/MDIO */
static int
mii_rreg(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len)
{
	uint16 cmd16, val16;
	void *h = robo->h;
	int i;
	uint8 *ptr = (uint8 *)val;

	/* do not allow access to internal PHY MII registers */
	if ((page >= PAGE_GPHY_MII_P0) && (page <= PAGE_GPHY_MII_P4)) {
		ET_ERROR(("mii_rreg: cannot access MII registers through pseudo phy interface\n"));
		return -1;
	}

	/* validate value length and buffer address */
	ASSERT(len == 1 || len == 6 || len == 8 ||
	       ((len == 2) && !((int)val & 1)) || ((len == 4) && !((int)val & 3)));

	/* set page number - MII register 0x10 */
	if (robo->page != page) {
		cmd16 = ((page << 8) |		/* page number */
		         1);			/* mdc/mdio access enable */
		robo->miiwr(h, PSEUDO_PHYAD, REG_MII_PAGE, cmd16);
		robo->page = page;
	}

	/* set register address - MII register 0x11 */
	cmd16 = ((reg << 8) |		/* register address */
	         2);			/* opcode read */
	robo->miiwr(h, PSEUDO_PHYAD, REG_MII_ADDR, cmd16);

	/* is operation finished? */
	for (i = MII_MAX_RETRY; i > 0; i --) {
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_ADDR);
		if ((val16 & 3) == 0)
			break;
	}
	/* timed out */
	if (!i) {
		ET_ERROR(("mii_rreg: timeout"));
		return -1;
	}

	switch (len) {
	case 8:
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA3);
		ptr[7] = (val16 >> 8);
		ptr[6] = (val16 & 0xff);
		/* FALLTHRU */

	case 6:
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA2);
		ptr[5] = (val16 >> 8);
		ptr[4] = (val16 & 0xff);
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA1);
		ptr[3] = (val16 >> 8);
		ptr[2] = (val16 & 0xff);
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA0);
		ptr[1] = (val16 >> 8);
		ptr[0] = (val16 & 0xff);
		break;

	case 4:
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA1);
		*(uint32 *)val = (((uint32)val16) << 16);
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA0);
		*(uint32 *)val |= val16;
		break;

	case 2:
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA0);
		*(uint16 *)val = val16;
		break;

	case 1:
		val16 = robo->miird(h, PSEUDO_PHYAD, REG_MII_DATA0);
		*(uint8 *)val = (uint8)(val16 & 0xff);
		break;
	}

	ET_MSG(("%s: [0x%x-0x%x] => 0x%x (len %d)\n", __FUNCTION__, page, reg,
	       VARG(val, len), len));

	return 0;
}

/* MII interface functions */
static dev_ops_t mdcmdio = {
	NULL,
	NULL,
	mii_wreg,
	mii_rreg,
	"MII (MDC/MDIO)"
};


/*
 * BCM4707, 4708 and 4709 use ChipcommonB Switch Register Access Bridge Registers (SRAB)
 * to access the switch registers
 */

#ifdef ROBO_SRAB
#define SRAB_ENAB()	(1)
#define NS_CHIPCB_SRAB		0x18007000	/* NorthStar Chip Common B SRAB base */
#define SET_ROBO_SRABREGS(robo)	((robo)->srabregs = \
	(srabregs_t *)REG_MAP(NS_CHIPCB_SRAB, SI_CORE_SIZE))
#define SET_ROBO_SRABOPS(robo)	((robo)->ops = &srab)
#else
#define SRAB_ENAB()	(0)
#define SET_ROBO_SRABREGS(robo)
#define SET_ROBO_SRABOPS(robo)
#endif /* ROBO_SRAB */

#ifdef ROBO_SRAB
#define SRAB_MAX_RETRY		1000
static int
srab_request_grant(robo_info_t *robo)
{
	int i, ret = 0;
	uint32 val32;

	val32 = R_REG(si_osh(robo->sih), &robo->srabregs->ctrls);
	val32 |= CFG_F_rcareq_MASK;
	W_REG(si_osh(robo->sih), &robo->srabregs->ctrls, val32);

	/* Wait for command complete */
	for (i = SRAB_MAX_RETRY * 10; i > 0; i --) {
		val32 = R_REG(si_osh(robo->sih), &robo->srabregs->ctrls);
		if ((val32 & CFG_F_rcagnt_MASK))
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("srab_request_grant: timeout"));
		ret = -1;
	}

	return ret;
}

static void
srab_release_grant(robo_info_t *robo)
{
	uint32 val32;

	val32 = R_REG(si_osh(robo->sih), &robo->srabregs->ctrls);
	val32 &= ~CFG_F_rcareq_MASK;
	W_REG(si_osh(robo->sih), &robo->srabregs->ctrls, val32);
}

static int
srab_interface_reset(robo_info_t *robo)
{
	int i, ret = 0;
	uint32 val32;

	/* Wait for switch initialization complete */
	for (i = SRAB_MAX_RETRY * 10; i > 0; i --) {
		val32 = R_REG(si_osh(robo->sih), &robo->srabregs->ctrls);
		if ((val32 & CFG_F_sw_init_done_MASK))
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("srab_interface_reset: timeout sw_init_done"));
		ret = -1;
	}

	/* Set the SRAU reset bit */
	W_REG(si_osh(robo->sih), &robo->srabregs->cmdstat, CFG_F_sra_rst_MASK);

	/* Wait for it to auto-clear */
	for (i = SRAB_MAX_RETRY * 10; i > 0; i --) {
		val32 = R_REG(si_osh(robo->sih), &robo->srabregs->cmdstat);
		if ((val32 & CFG_F_sra_rst_MASK) == 0)
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("srab_interface_reset: timeout sra_rst"));
		ret |= -2;
	}

	return ret;
}

static int
srab_wreg(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len)
{
	uint16 val16;
	uint32 val32;
	uint32 val_h = 0, val_l = 0;
	int i, ret = 0;
	uint8 *ptr = (uint8 *)val;

	/* validate value length and buffer address */
	ASSERT(len == 1 || len == 6 || len == 8 ||
	       ((len == 2) && !((int)val & 1)) || ((len == 4) && !((int)val & 3)));

	ET_MSG(("%s: [0x%x-0x%x] := 0x%x (len %d)\n", __FUNCTION__, page, reg,
	       VARG(val, len), len));

	srab_request_grant(robo);

	/* Load the value to write */
	switch (len) {
	case 8:
		val16 = ptr[7];
		val16 = ((val16 << 8) | ptr[6]);
		val_h = val16 << 16;
		/* FALLTHRU */

	case 6:
		val16 = ptr[5];
		val16 = ((val16 << 8) | ptr[4]);
		val_h |= val16;

		val16 = ptr[3];
		val16 = ((val16 << 8) | ptr[2]);
		val_l = val16 << 16;
		val16 = ptr[1];
		val16 = ((val16 << 8) | ptr[0]);
		val_l |= val16;
		break;

	case 4:
		val_l = *(uint32 *)val;
		break;

	case 2:
		val_l = *(uint16 *)val;
		break;

	case 1:
		val_l = *(uint8 *)val;
		break;
	}
	W_REG(si_osh(robo->sih), &robo->srabregs->wd_h, val_h);
	W_REG(si_osh(robo->sih), &robo->srabregs->wd_l, val_l);

	/* We don't need this variable */
	if (robo->page != page)
		robo->page = page;

	/* Issue the write command */
	val32 = ((page << CFG_F_sra_page_R)
		| (reg << CFG_F_sra_offset_R)
		| CFG_F_sra_gordyn_MASK
		| CFG_F_sra_write_MASK);
	W_REG(si_osh(robo->sih), &robo->srabregs->cmdstat, val32);

	/* Wait for command complete */
	for (i = SRAB_MAX_RETRY; i > 0; i --) {
		val32 = R_REG(si_osh(robo->sih), &robo->srabregs->cmdstat);
		if ((val32 & CFG_F_sra_gordyn_MASK) == 0)
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("srab_wreg: timeout"));
		srab_interface_reset(robo);
		ret = -1;
	}

	srab_release_grant(robo);

	return ret;
}

static int
srab_rreg(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len)
{
	uint32 val32;
	uint32 val_h = 0, val_l = 0;
	int i, ret = 0;
	uint8 *ptr = (uint8 *)val;

	/* validate value length and buffer address */
	ASSERT(len == 1 || len == 6 || len == 8 ||
	       ((len == 2) && !((int)val & 1)) || ((len == 4) && !((int)val & 3)));

	srab_request_grant(robo);

	/* We don't need this variable */
	if (robo->page != page)
		robo->page = page;

	/* Assemble read command */
	srab_request_grant(robo);

	val32 = ((page << CFG_F_sra_page_R)
		| (reg << CFG_F_sra_offset_R)
		| CFG_F_sra_gordyn_MASK);
	W_REG(si_osh(robo->sih), &robo->srabregs->cmdstat, val32);

	/* is operation finished? */
	for (i = SRAB_MAX_RETRY; i > 0; i --) {
		val32 = R_REG(si_osh(robo->sih), &robo->srabregs->cmdstat);
		if ((val32 & CFG_F_sra_gordyn_MASK) == 0)
			break;
	}

	/* timed out */
	if (!i) {
		ET_ERROR(("srab_read: timeout"));
		srab_interface_reset(robo);
		ret = -1;
		goto err;
	}

	/* Didn't time out, read and return the value */
	val_h = R_REG(si_osh(robo->sih), &robo->srabregs->rd_h);
	val_l = R_REG(si_osh(robo->sih), &robo->srabregs->rd_l);

	switch (len) {
	case 8:
		ptr[7] = (val_h >> 24);
		ptr[6] = ((val_h >> 16) & 0xff);
		/* FALLTHRU */

	case 6:
		ptr[5] = ((val_h >> 8) & 0xff);
		ptr[4] = (val_h & 0xff);
		ptr[3] = (val_l >> 24);
		ptr[2] = ((val_l >> 16) & 0xff);
		ptr[1] = ((val_l >> 8) & 0xff);
		ptr[0] = (val_l & 0xff);
		break;

	case 4:
		*(uint32 *)val = val_l;
		break;

	case 2:
		*(uint16 *)val = (uint16)(val_l & 0xffff);
		break;

	case 1:
		*(uint8 *)val = (uint8)(val_l & 0xff);
		break;
	}

	ET_MSG(("%s: [0x%x-0x%x] => 0x%x (len %d)\n", __FUNCTION__, page, reg,
	       VARG(val, len), len));

err:
	srab_release_grant(robo);

	return ret;
}

/* SRAB interface functions */
static dev_ops_t srab = {
	NULL,
	NULL,
	srab_wreg,
	srab_rreg,
	"SRAB"
};
#else
#define srab_interface_reset(a) do {} while (0)
#define srab_rreg(a, b, c, d, e) 0
#endif /* ROBO_SRAB */

/* High level switch configuration functions. */

/* Get access to the RoboSwitch */
robo_info_t *
bcm_robo_attach(si_t *sih, void *h, char *vars, miird_f miird, miiwr_f miiwr)
{
	robo_info_t *robo;
	uint32 reset, idx;
//#ifndef	_CFE_
	const char *et1port, *et1phyaddr;
	int mdcport = 0, phyaddr = 0;
	uint8 val8;
	uint16 reg_val; 
//#endif /* _CFE_ */
	int lan_portenable = 0;
	int rc;

	/* Allocate and init private state */
	if (!(robo = MALLOC(si_osh(sih), sizeof(robo_info_t)))) {
		ET_ERROR(("robo_attach: out of memory, malloced %d bytes",
		          MALLOCED(si_osh(sih))));
		return NULL;
	}
	bzero(robo, sizeof(robo_info_t));

	robo->h = h;
	robo->sih = sih;
	robo->vars = vars;
	robo->miird = miird;
	robo->miiwr = miiwr;
	robo->page = -1;

	if (SRAB_ENAB() && BCM4707_CHIP(sih->chip)) {
		SET_ROBO_SRABREGS(robo);
	}

	/* Enable center tap voltage for LAN ports using gpio23. Usefull in case when
	 * romboot CFE loads linux over WAN port and Linux enables LAN ports later
	 */
	if ((lan_portenable = getgpiopin(robo->vars, "lanports_enable", GPIO_PIN_NOTDEFINED)) !=
	    GPIO_PIN_NOTDEFINED) {
		lan_portenable = 1 << lan_portenable;
		si_gpioouten(sih, lan_portenable, lan_portenable, GPIO_DRV_PRIORITY);
		si_gpioout(sih, lan_portenable, lan_portenable, GPIO_DRV_PRIORITY);
		bcm_mdelay(5);
	}

	/* Trigger external reset by nvram variable existance */
	if ((reset = getgpiopin(robo->vars, "robo_reset", GPIO_PIN_NOTDEFINED)) !=
	    GPIO_PIN_NOTDEFINED) {
		/*
		 * Reset sequence: RESET low(50ms)->high(20ms)
		 *
		 * We have to perform a full sequence for we don't know how long
		 * it has been from power on till now.
		 */
		ET_MSG(("%s: Using external reset in gpio pin %d\n", __FUNCTION__, reset));
		reset = 1 << reset;

		/* Keep RESET low for 50 ms */
		si_gpioout(sih, reset, 0, GPIO_DRV_PRIORITY);
		si_gpioouten(sih, reset, reset, GPIO_DRV_PRIORITY);
		bcm_mdelay(50);

		/* Keep RESET high for at least 20 ms */
		si_gpioout(sih, reset, reset, GPIO_DRV_PRIORITY);
		bcm_mdelay(20);
	} else {
		/* In case we need it */
		idx = si_coreidx(sih);

		if (si_setcore(sih, ROBO_CORE_ID, 0)) {
			/* If we have an internal robo core, reset it using si_core_reset */
			ET_MSG(("%s: Resetting internal robo core\n", __FUNCTION__));
			si_core_reset(sih, 0, 0);
			robo->corerev = si_corerev(sih);
		}
		else if (sih->chip == BCM5356_CHIP_ID) {
			/* Testing chipid is a temporary hack. We need to really
			 * figure out how to treat non-cores in ai chips.
			 */
			robo->corerev = 3;
		}
		else if (SRAB_ENAB() && BCM4707_CHIP(sih->chip)) {
			srab_interface_reset(robo);
			rc = srab_rreg(robo, PAGE_MMR, REG_VERSION_ID, &robo->corerev, 1);
		}
		else {
			mii_rreg(robo, PAGE_STATUS, REG_STATUS_REV, &robo->corerev, 1);
		}
		si_setcoreidx(sih, idx);
		ET_MSG(("%s: Internal robo rev %d\n", __FUNCTION__, robo->corerev));
	}

	if (SRAB_ENAB() && BCM4707_CHIP(sih->chip)) {
		rc = srab_rreg(robo, PAGE_MMR, REG_DEVICE_ID, &robo->devid, sizeof(uint32));

		ET_MSG(("%s: devid read %ssuccesfully via srab: 0x%x\n",
			__FUNCTION__, rc ? "un" : "", robo->devid));

		SET_ROBO_SRABOPS(robo);
		if ((rc != 0) || (robo->devid == 0)) {
			ET_ERROR(("%s: error reading devid\n", __FUNCTION__));
			MFREE(si_osh(robo->sih), robo, sizeof(robo_info_t));
			return NULL;
		}
		ET_MSG(("%s: devid: 0x%x\n", __FUNCTION__, robo->devid));
	}
	else if (miird && miiwr) {
		uint16 tmp;
		int retry_count = 0;

		/* Read the PHY ID */
		tmp = miird(h, PSEUDO_PHYAD, 2);

		/* WAR: Enable mdc/mdio access to the switch registers. Unless
		 * a write to bit 0 of pseudo phy register 16 is done we are
		 * unable to talk to the switch on a customer ref design.
		 */
		if (tmp == 0xffff) {
			miiwr(h, PSEUDO_PHYAD, 16, 1);
			tmp = miird(h, PSEUDO_PHYAD, 2);
		}

		if (tmp != 0xffff) {
			do {
				rc = mii_rreg(robo, PAGE_MMR, REG_DEVICE_ID,
				              &robo->devid, sizeof(uint16));
				if (rc != 0)
					break;
				retry_count++;
			} while ((robo->devid == 0) && (retry_count < 10));

			ET_MSG(("%s: devid read %ssuccesfully via mii: 0x%x\n",
			        __FUNCTION__, rc ? "un" : "", robo->devid));
			ET_MSG(("%s: mii access to switch works\n", __FUNCTION__));
			robo->ops = &mdcmdio;
			if ((rc != 0) || (robo->devid == 0)) {
				ET_MSG(("%s: error reading devid, assuming 5325e\n",
				        __FUNCTION__));
				robo->devid = DEVID5325;
			}
		}
		ET_MSG(("%s: devid: 0x%x\n", __FUNCTION__, robo->devid));
	}

	if ((robo->devid == DEVID5395) ||
	    (robo->devid == DEVID5397) ||
	    (robo->devid == DEVID5398)) {
		uint8 srst_ctrl;

		/* If it is a 539x switch, use the soft reset register */
		ET_MSG(("%s: Resetting 539x robo switch\n", __FUNCTION__));

		/* Reset the 539x switch core and register file */
		srst_ctrl = 0x83;
		mii_wreg(robo, PAGE_CTRL, REG_CTRL_SRST, &srst_ctrl, sizeof(uint8));
		srst_ctrl = 0x00;
		mii_wreg(robo, PAGE_CTRL, REG_CTRL_SRST, &srst_ctrl, sizeof(uint8));
	}

	/* Enable switch leds */
	if (sih->chip == BCM5356_CHIP_ID) {
		if (PMUCTL_ENAB(sih)) {
			si_pmu_chipcontrol(sih, 2, (1 << 25), (1 << 25));
			/* also enable fast MII clocks */
			si_pmu_chipcontrol(sih, 0, (1 << 1), (1 << 1));
		}
	} else if ((sih->chip == BCM5357_CHIP_ID) || (sih->chip == BCM53572_CHIP_ID)) {
		uint32 led_gpios = 0;
		const char *var;

		if (((sih->chip == BCM5357_CHIP_ID) && (sih->chippkg != BCM47186_PKG_ID)) ||
		    ((sih->chip == BCM53572_CHIP_ID) && (sih->chippkg != BCM47188_PKG_ID)))
			led_gpios = 0x1f;
		var = getvar(vars, "et_swleds");
		if (var)
			led_gpios = bcm_strtoul(var, NULL, 0);
		if (PMUCTL_ENAB(sih) && led_gpios)
			si_pmu_chipcontrol(sih, 2, (0x3ff << 8), (led_gpios << 8));
	}

#ifndef	_CFE_
	if (!robo->ops) {
		int mosi, miso, ss, sck;

		robo->ops = &spigpio;
		robo->devid = DEVID5325;

		/* Init GPIO mapping. Default 2, 3, 4, 5 */
		ss = getgpiopin(vars, "robo_ss", 2);
		if (ss == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_ss gpio fail: GPIO 2 in use"));
			goto error;
		}
		robo->ss = 1 << ss;
		sck = getgpiopin(vars, "robo_sck", 3);
		if (sck == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_sck gpio fail: GPIO 3 in use"));
			goto error;
		}
		robo->sck = 1 << sck;
		mosi = getgpiopin(vars, "robo_mosi", 4);
		if (mosi == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_mosi gpio fail: GPIO 4 in use"));
			goto error;
		}
		robo->mosi = 1 << mosi;
		miso = getgpiopin(vars, "robo_miso", 5);
		if (miso == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_miso gpio fail: GPIO 5 in use"));
			goto error;
		}
		robo->miso = 1 << miso;
		ET_MSG(("%s: ss %d sck %d mosi %d miso %d\n", __FUNCTION__,
		        ss, sck, mosi, miso));
	}
#endif /* _CFE_ */

	/* sanity check */
	ASSERT(robo->ops);
	ASSERT(robo->ops->write_reg);
	ASSERT(robo->ops->read_reg);
	ASSERT((robo->devid == DEVID5325) ||
	       (robo->devid == DEVID5395) ||
	       (robo->devid == DEVID5397) ||
	       (robo->devid == DEVID5398) ||
	       (robo->devid == DEVID53115) ||
	       (robo->devid == DEVID53125) ||
	       ROBO_IS_BCM5301X(robo->devid));

#ifndef	_CFE_
	/* nvram variable switch_mode controls the power save mode on the switch
	 * set the default value in the beginning
	 */
	robo->pwrsave_mode_manual = getintvar(robo->vars, "switch_mode_manual");
	robo->pwrsave_sleep_time = getintvar(robo->vars, "switch_pwrsave_sleep");
	if (robo->pwrsave_sleep_time == 0)
		robo->pwrsave_sleep_time = PWRSAVE_SLEEP_TIME;
	robo->pwrsave_wake_time = getintvar(robo->vars, "switch_pwrsave_wake");
	if (robo->pwrsave_wake_time == 0)
		robo->pwrsave_wake_time = PWRSAVE_WAKE_TIME;
	robo->pwrsave_mode_auto = getintvar(robo->vars, "switch_mode_auto");
#endif /* _CFE_ */

	/* Determining what all phys need to be included in
	 * power save operation
	 */
	et1port = getvar(vars, "et1mdcport");
	if (et1port)
		mdcport = bcm_atoi(et1port);

	et1phyaddr = getvar(vars, "et1phyaddr");
	if (et1phyaddr)
		phyaddr = bcm_atoi(et1phyaddr);

#ifndef        _CFE_
	if ((mdcport == 0) && (phyaddr == 4))
		/* For 5325F switch we need to do only phys 0-3 */
		robo->pwrsave_phys = 0xf;
	else
		/* By default all 5 phys are put into power save if there is no link */
		robo->pwrsave_phys = 0x1f;
#endif /* _CFE_ */

#ifdef PLC
	/* See if one of the ports is connected to plc chipset */
	robo->plc_hw = (getvar(vars, "plc_vifs") != NULL);
#endif /* PLC */

	/* reset p5 reg when needs to link up it in ac88u/ac87u */
	if (ROBO_IS_BCM5301X(robo->devid) && mdcport == 0 && phyaddr == 30) {
		val8 = 0xfb;
		robo->ops->write_reg(robo, PAGE_CTRL, 0x5d, &val8, sizeof(val8));
	}

	/* set led mode OFF in cfe */
	/* map0 */
	robo->ops->read_reg(robo, PAGE_CTRL, 0x18, &reg_val, sizeof(reg_val));
#ifdef _CFE_
	reg_val &= 0xfe00;
#else
	reg_val |= 0x1ff;
#endif
	robo->ops->write_reg(robo, PAGE_CTRL, 0x18, &reg_val, sizeof(reg_val));

	/* map1 */
	robo->ops->read_reg(robo, PAGE_CTRL, 0x1a, &reg_val, sizeof(reg_val));
#ifdef _CFE_
	reg_val &= 0xfe00;
#else
	reg_val |= 0x1ff;
#endif
	robo->ops->write_reg(robo, PAGE_CTRL, 0x1a, &reg_val, sizeof(reg_val));

#ifdef BCMFA
	robo->aux_pid = -1;
	if (BCM4707_CHIP(CHIPID(robo->sih->chip))) {
		if (getvar(robo->vars, "et0macaddr"))
			robo->aux_pid = 5;	/* unit 0 maps to port 5 */
		else if (getvar(robo->vars, "et1macaddr"))
			robo->aux_pid = 7;	/* unit 1 maps to port 7 */

		ASSERT(robo->aux_pid != -1);
	}
#endif /* BCMFA */

	return robo;

#ifndef	_CFE_
error:
	bcm_robo_detach(robo);
	return NULL;
#endif /* _CFE_ */
}

/* Release access to the RoboSwitch */
void
bcm_robo_detach(robo_info_t *robo)
{
	if (SRAB_ENAB() && robo->srabregs)
		REG_UNMAP(robo->srabregs);

	MFREE(si_osh(robo->sih), robo, sizeof(robo_info_t));
}

/* Enable the device and set it to a known good state */
int
bcm_robo_enable_device(robo_info_t *robo)
{
	uint8 reg_offset, reg_val;
	int ret = 0;
#ifdef PLC
	uint32 val32;
#endif /* PLC */

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	if (robo->devid == DEVID5398) {
		/* Disable unused ports: port 6 and 7 */
		for (reg_offset = REG_CTRL_PORT6; reg_offset <= REG_CTRL_PORT7; reg_offset ++) {
			/* Set bits [1:0] to disable RX and TX */
			reg_val = 0x03;
			robo->ops->write_reg(robo, PAGE_CTRL, reg_offset, &reg_val,
			                     sizeof(reg_val));
		}
	}

	if (robo->devid == DEVID5325) {
		/* Must put the switch into Reverse MII mode! */

		/* MII port state override (page 0 register 14) */
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &reg_val, sizeof(reg_val));

		/* Bit 4 enables reverse MII mode */
		if (!(reg_val & (1 << 4))) {
			/* Enable RvMII */
			reg_val |= (1 << 4);
			robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &reg_val,
			                     sizeof(reg_val));

			/* Read back */
			robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &reg_val,
			                    sizeof(reg_val));
			if (!(reg_val & (1 << 4))) {
				ET_ERROR(("robo_enable_device: enabling RvMII mode failed\n"));
				ret = -1;
			}
		}
	}

#ifdef PLC
	if (robo->plc_hw) {
		val32 = 0x100002;
		robo->ops->write_reg(robo, PAGE_MMR, REG_MMR_ATCR, &val32, sizeof(val32));
	}
#endif /* PLC */

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return ret;
}

/* Port flags */
#define FLAG_TAGGED	't'	/* output tagged (external ports only) */
#define FLAG_UNTAG	'u'	/* input & output untagged (CPU port only, for OS (linux, ...) */
#define FLAG_LAN	'*'	/* input & output untagged (CPU port only, for CFE */

/* port descriptor */
typedef	struct {
	uint32 untag;	/* untag enable bit (Page 0x05 Address 0x63-0x66 Bit[17:9]) */
	uint32 member;	/* vlan member bit (Page 0x05 Address 0x63-0x66 Bit[7:0]) */
	uint8 ptagr;	/* port tag register address (Page 0x34 Address 0x10-0x1F) */
	uint8 cpu;	/* is this cpu port? */
} pdesc_t;

pdesc_t pdesc97[] = {
	/* 5395/5397/5398/53115S is 0 ~ 7.  port 8 is IMP port. */
	/* port 0 */ {1 << 9, 1 << 0, REG_VLAN_PTAG0, 0},
	/* port 1 */ {1 << 10, 1 << 1, REG_VLAN_PTAG1, 0},
	/* port 2 */ {1 << 11, 1 << 2, REG_VLAN_PTAG2, 0},
	/* port 3 */ {1 << 12, 1 << 3, REG_VLAN_PTAG3, 0},
	/* port 4 */ {1 << 13, 1 << 4, REG_VLAN_PTAG4, 0},
	/* port 5 */ {1 << 14, 1 << 5, REG_VLAN_PTAG5, 0},
	/* port 6 */ {1 << 15, 1 << 6, REG_VLAN_PTAG6, 0},
	/* port 7 */ {1 << 16, 1 << 7, REG_VLAN_PTAG7, 0},
	/* mii port */ {1 << 17, 1 << 8, REG_VLAN_PTAG8, 1},
};

pdesc_t pdesc25[] = {
	/* port 0 */ {1 << 6, 1 << 0, REG_VLAN_PTAG0, 0},
	/* port 1 */ {1 << 7, 1 << 1, REG_VLAN_PTAG1, 0},
	/* port 2 */ {1 << 8, 1 << 2, REG_VLAN_PTAG2, 0},
	/* port 3 */ {1 << 9, 1 << 3, REG_VLAN_PTAG3, 0},
	/* port 4 */ {1 << 10, 1 << 4, REG_VLAN_PTAG4, 0},
	/* mii port */ {1 << 11, 1 << 5, REG_VLAN_PTAG5, 1},
};

#if !defined(_CFE_) && defined(BCMFA)
/* For FA feature can be worked with old/new CFE which keep using et0mac */
static int
robo_fa_imp_port_upd(robo_info_t *robo, char *port, int pid, int vid, int pdescsz)
{
	int newpid = pid;

	if (pid == robo->aux_pid) {
		if (BCM4707_CHIP(CHIPID(robo->sih->chip)) && (pid != pdescsz - 1) &&
		    FA_ON(getintvar(robo->vars, "ctf_fa_mode"))) {
			newpid = pdescsz - 1;
		}
	}

	return newpid;
}
#endif /* !_CFE_ && BCMFA */

/* Find the first vlanXXXXports which the last port include '*' or 'u' */
static void
robo_cpu_port_upd(robo_info_t *robo, pdesc_t *pdesc, int pdescsz)
{
	int vid;

	for (vid = 0; vid < VLAN_NUMVLANS; vid ++) {
		char vlanports[] = "vlanXXXXports";
		char port[] = "XXXX", *next;
		const char *ports, *cur;
		int pid, len;

		/* no members if VLAN id is out of limitation */
		if (vid > VLAN_MAXVID)
			return;

		/* get vlan member ports from nvram */
		sprintf(vlanports, "vlan%dports", vid);
		ports = getvar(robo->vars, vlanports);
		if (!ports)
			continue;

		/* search last port include '*' or 'u' */
		for (cur = ports; cur; cur = next) {
			/* tokenize the port list */
			while (*cur == ' ')
				cur ++;
			next = bcmstrstr(cur, " ");
			len = next ? next - cur : strlen(cur);
			if (!len)
				break;
			if (len > sizeof(port) - 1)
				len = sizeof(port) - 1;
			strncpy(port, cur, len);
			port[len] = 0;

			/* make sure port # is within the range */
			pid = bcm_atoi(port);
			if (pid >= pdescsz) {
				ET_ERROR(("robo_cpu_upd: port %d in vlan%dports is out "
				          "of range[0-%d]\n", pid, vid, pdescsz));
				continue;
			}

#if !defined(_CFE_) && defined(BCMFA)
			pid = robo_fa_imp_port_upd(robo, port, pid, vid, pdescsz);
#endif
			if (strchr(port, FLAG_LAN) || strchr(port, FLAG_UNTAG)) {
				/* Change it and return */
				pdesc[pid].cpu = 1;
				return;
			}
		}
	}
}

#ifdef ETAGG
#if defined(BCMFA) || defined(BCMAGG)
int32
robo_bhdr_register(robo_info_t *robo, bhdr_ports_t bhdr_port, uint8 registrant)
{
	bool bhdr_enabled;
	uint8 val8;

	if (!robo)
		return -1;

	if (!BCM4707_CHIP(CHIPID(robo->sih->chip)))
		return -1;

	if ((bhdr_port >= MAX_BHDR_PORTS) || ((registrant & BHDR_REGISTRANTS) == 0))
		return -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* BCM header is already enabled on the unit or not */
	bhdr_enabled = (robo->bhdr_registered[bhdr_port] ? TRUE : FALSE);

	/* Add registrant for BCM header */
	robo->bhdr_registered[bhdr_port] |= registrant;

	/* Enable BCM header if it's not yet enabled */
	if (!bhdr_enabled) {
		robo->ops->read_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
		val8 |= (0x1 << bhdr_port);
		robo->ops->write_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}

int32
robo_bhdr_unregister(robo_info_t *robo, bhdr_ports_t bhdr_port, uint8 registrant)
{
	bool bhdr_enabled;
	uint8 val8;

	if (!robo)
		return -1;

	if (!BCM4707_CHIP(CHIPID(robo->sih->chip)))
		return -1;

	if ((bhdr_port >= MAX_BHDR_PORTS) || ((registrant & BHDR_REGISTRANTS) == 0))
		return -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* BCM header is already enabled on the unit or not */
	bhdr_enabled = (robo->bhdr_registered[bhdr_port] ? TRUE : FALSE);
	robo->bhdr_registered[bhdr_port] &= ~registrant;

	/* Disable BCM header if no one registered now */
	if (bhdr_enabled && (robo->bhdr_registered[bhdr_port] == 0)) {
		robo->ops->read_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
		val8 &= ~(0x1 << bhdr_port);
		robo->ops->write_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}
#endif /* BCMFA || BCMAGG */
#endif	/* ETAGG */

#if !defined(_CFE_) && defined(BCMFA)
/* Default assume: Do copy to aux port */
static void
robo_fa_aux_set_action_policy(robo_info_t *robo, uint32 index)
{
	uint32 val32;
	uint32 aux_portmap;

	/* bit[5:0]=p5~p0, bit6=p7, bit7=p8 */
	if (robo->aux_pid > 5)
		aux_portmap = (1 << (robo->aux_pid - 1));
	else
		aux_portmap = (1 << robo->aux_pid);

	/* Set to both in-band and out-band */

	/* In-Band */
	val32 = ((3 << CFP_ACT_POL_DATA0_CFMI_SHIFT) | /* do copy */
		 (aux_portmap << CFP_ACT_POL_DATA0_DMI_SHIFT) | /* aux port */
		 7); /* STP_BYP | EAP_BYP | VLAN_BYP */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA0, &val32, sizeof(val32));
	/* Out-Band */
	val32 = ((3 << CFP_ACT_POL_DATA1_CFMO_SHIFT) | /* do copy */
		 (aux_portmap << CFP_ACT_POL_DATA1_DMO_SHIFT)); /* aux port */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA1, &val32, sizeof(val32));
	/* Issue write command */
	val32 = ((index << CFP_ACC_XCESS_ADDR_SHIFT) | /* index */
		 (2 << CFP_ACC_RAM_SEL_SHIFT) | /* action/policy */
		 (2 << CFP_ACC_OP_SEL_SHIFT) | /* write */
		  CFP_ACC_OP_STR_DONE); /* operation start */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32));
}

static void
robo_fa_aux_set_tcam(robo_info_t *robo, bool ipv6, bool tcp_rst, uint32 index)
{
	uint32 val32;
	uint8 l3_framing_bit, tcp_flags_bit;
	uint8 phy_portmap = 0x1F; /* port 0~4 by default */
	char *rgmii_port;

	/* Add rgmii port into phy_portmap */
	rgmii_port = getvar(robo->vars, "rgmii_port");
	if (rgmii_port)
		phy_portmap |= (1 << bcm_strtoul(rgmii_port, NULL, 0));

	if (ipv6)
		l3_framing_bit = 1; /* IPv6 */
	else
		l3_framing_bit = 0; /* IPv4 */

	if (tcp_rst)
		tcp_flags_bit = 4; /* RST */
	else
		tcp_flags_bit = 1; /* FIN */

	/* Both Data and Mask */
	/* DATA0 = UDF_n_A1(Low 8) + UDF_n_A0(16) + Reserved(4) + Slice_ID(2) + Vaild(2) */
	/* TCP flags bit (Host order) + Slice 0 + Vaild */
	val32 = ((tcp_flags_bit << 8) | (0 << 2) | 3);
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_DATA0, &val32, sizeof(val32));
	val32 = ((tcp_flags_bit << 8) | (3 << 2) | 3);
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_MASK0, &val32, sizeof(val32));
	/* DATA1 = UDF_n_A3(Low 8) + UDF_n_A2(16) + UDF_n_A1(High 8) */
	/* DATA2 = UDF_n_A5(Low 8) + UDF_n_A4(16) + UDF_n_A3(High 8) */
	/* DATA3 = UDF_n_A7(Low 8) + UDF_n_A6(16) + UDF_n_A5(High 8) */
	/* DATA4 = C_Tag(Low 8) + UDF_n_A8(16) + UDF_n_A7(High 8) */
	/* DATA5 = UDF_Valid [7:0](8) + S_Tag(16) + C_Tag(High 8) */
	val32 = (1 << 24); /* UDF_Valid[0] */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_DATA5, &val32, sizeof(val32));
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_MASK5, &val32, sizeof(val32));
	/* DATA6 = STag_Status(2) + CTag_Status(2) + L2_Framing(2) + L3_Framing(2) + IP_TOS(8) +
	 *              IP_Protocol(8) + IP_Fragmentation(1) + NonFirst_Fragment(1) +
	 *              IP_Authentication(1) + TTL_Range(2) + Reserved(2) + UDF_Valid[8](1)
	*/
	val32 = ((l3_framing_bit << 24) | (6 << 8)); /* IPv4/6 TCP */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_DATA6, &val32, sizeof(val32));
	val32 = ((3 << 24) | (6 << 8));
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_MASK6, &val32, sizeof(val32));
	/* DATA7 = SRC_PortMap */
	val32 = 0;
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_DATA7, &val32, sizeof(val32));
	val32 = 0xFF & ~phy_portmap; /* ~phy_portmap */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_MASK7, &val32, sizeof(val32));

	/* Issue write command */
	val32 = ((index << CFP_ACC_XCESS_ADDR_SHIFT) | /* index */
		 (1 << CFP_ACC_RAM_SEL_SHIFT) | /* TCAM */
		 (2 << CFP_ACC_OP_SEL_SHIFT) | /* write */
		  CFP_ACC_OP_STR_DONE); /* operation start */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32));
}

#ifdef BCMDBG
static void
robo_fa_aux_dump_action_policy(robo_info_t *robo, struct bcmstrbuf *b, uint32 index, char *note)
{
	uint32 val32;

	/* Issue read command */
	val32 = ((index << CFP_ACC_XCESS_ADDR_SHIFT) | /* index */
		 (2 << CFP_ACC_RAM_SEL_SHIFT) | /* action/policy */
		 (1 << CFP_ACC_OP_SEL_SHIFT) | /* read */
		  CFP_ACC_OP_STR_DONE); /* operation start */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32));
	CFP_ACC_RD_STS_WAIT(robo, 2 /* action policy */);

	/* Dump both in-band and out-band */
	/* In-Band */
	val32 = 0;
	robo->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA0, &val32, sizeof(val32));
	bcm_bprintf(b, "(0x%02x,0x%02x) CFP Action Policy In-Band index %d%s: 0x%08x\n",
		PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA0, index,
		note ? note : "", val32);
	/* Out-Band */
	val32 = 0;
	robo->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA1, &val32, sizeof(val32));
	bcm_bprintf(b, "(0x%02x,0x%02x) CFP Action Policy Out-Band index %d%s: 0x%08x\n",
		PAGE_CFPTCAM, REG_CFPTCAM_ACT_POL_DATA1, index,
		note ? note : "", val32);
}

static void
robo_fa_aux_dump_tcam(robo_info_t *robo, struct bcmstrbuf *b, uint32 index, char *note)
{
	int i;
	uint32 val32;
	uint32 tcam32[8];

	/* Issue read command */
	val32 = ((index << CFP_ACC_XCESS_ADDR_SHIFT) | /* index */
		 (1 << CFP_ACC_RAM_SEL_SHIFT) | /* TCAM */
		 (1 << CFP_ACC_OP_SEL_SHIFT) | /* read */
		  CFP_ACC_OP_STR_DONE); /* operation start */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32));
	CFP_ACC_RD_STS_WAIT(robo, 1 /* TCAM */);

	/* Dump both Data and Mask */
	memset(tcam32, 0, sizeof(tcam32));
	for (i = 0; i < 8; i++)
		robo->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_DATA0+(4*i), &tcam32[i],
			sizeof(tcam32[i]));
	bcm_bprintf(b, "(0x%02x,0x%02x) CFP TCAM Data index %d%s: 0x%08x 0x%08x 0x%08x "
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x (LSB)\n",
		 PAGE_CFPTCAM, REG_CFPTCAM_DATA0, index, note ? note : "",
		tcam32[7], tcam32[6], tcam32[5], tcam32[4],
		tcam32[3], tcam32[2], tcam32[1], tcam32[0]);
	memset(tcam32, 0, sizeof(tcam32));
	for (i = 0; i < 8; i++)
		robo->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_MASK0+(4*i), &tcam32[i],
			sizeof(tcam32[i]));
	bcm_bprintf(b, "(0x%02x,0x%02x) CFP TCAM Mask index %d%s: 0x%08x 0x%08x 0x%08x "
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x (LSB)\n",
		PAGE_CFPTCAM, REG_CFPTCAM_MASK0, index, note ? note : "",
		tcam32[7], tcam32[6], tcam32[5], tcam32[4],
		tcam32[3], tcam32[2], tcam32[1], tcam32[0]);
}

static void
robo_fa_aux_dump_rate_counter(robo_info_t *robo, struct bcmstrbuf *b, uint32 index, char *note)
{
	uint32 val32;

	/* Issue read command */
	val32 = ((index << CFP_ACC_XCESS_ADDR_SHIFT) | /* index */
		 (0x10 << CFP_ACC_RAM_SEL_SHIFT) | /* out-band statistic */
		 (1 << CFP_ACC_OP_SEL_SHIFT) | /* read */
		  CFP_ACC_OP_STR_DONE); /* operation start */
	robo->ops->write_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_ACC, &val32, sizeof(val32));
	CFP_ACC_RD_STS_WAIT(robo, 8 /* band statistic */);

	/* Dump Out-Band Statistic */
	val32 = 0;
	robo->ops->read_reg(robo, PAGE_CFPTCAM, REG_CFPTCAM_RATE_OUTBAND, &val32, sizeof(val32));
	bcm_bprintf(b, "(0x%02x,0x%02x) CFP Rate Out-Band Statistic index %d%s: 0x%08x\n",
		PAGE_CFPTCAM, REG_CFPTCAM_RATE_OUTBAND, index,
		note ? note : "", val32);
}
#endif /* BCMDBG */

void
robo_fa_aux_init(robo_info_t *robo)
{
	uint8 val8;

	if (!robo)
		return;

	if (robo->aux_pid == -1) {
		ET_ERROR(("Invalid aux_pid\n"));
		return;
	}

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* AUX Port GMII Port States Override Register */
	val8 = 0;
	robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT0_GMIIPO + robo->aux_pid,
		&val8, sizeof(val8));
	val8 |=
		(1 << 7) |	/* GMII_SPEED_UP_2G */
		(1 << 6) |	/* SW_OVERRIDE */
		(1 << 5) |	/* TXFLOW_CNTL */
		(1 << 4) |	/* RXFLOW_CNTL */
				/* default(2 << 2) SPEED :
				 * 2b10 1000/2000Mbps
				 */
				/* default(1 << 1) DUPLX_MODE:
				 * Full Duplex
				 */
		(1 << 0);	/* LINK_STS: Link up */
	robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PORT0_GMIIPO + robo->aux_pid,
		&val8, sizeof(val8));

	/* CFP: filter TCP FIN/RST and copy to AUX port */
	/* 1. Action policy */
	/* Index 0~4 (IPv4/6 TCP FIN/RST) */
	robo_fa_aux_set_action_policy(robo, 0);
	robo_fa_aux_set_action_policy(robo, 1);
	robo_fa_aux_set_action_policy(robo, 2);
	robo_fa_aux_set_action_policy(robo, 3);

	/* 2. Data and Mask */
	/* Index 0~4 (IPv4/6 TCP FIN/RST) */
	robo_fa_aux_set_tcam(robo, FALSE, FALSE, 0);
	robo_fa_aux_set_tcam(robo, FALSE, TRUE, 1);
	robo_fa_aux_set_tcam(robo, TRUE, FALSE, 2);
	robo_fa_aux_set_tcam(robo, TRUE, TRUE, 3);

	/* 3. UDFs */
	/* Slice 0 for IPv4/6 packet -FIN/RST */
	/* TCP flags offset 14 --> UDF offset start from 12 = 2N (N=6) */
	val8 = 0x60 | 0x6;
	robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_UDF_0_A_0_8, &val8,
		sizeof(val8));
	robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_UDF_0_A_0_8+1, &val8,
		sizeof(val8));
	robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_UDF_0_B_0_8, &val8,
		sizeof(val8));
	robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_UDF_0_B_0_8+1, &val8,
		sizeof(val8));

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);
}

void
robo_fa_aux_enable(robo_info_t *robo, bool enable)
{
	uint8 val8 = 0x0;
	char *rgmii_port;
	uint8 phy_portmap = 0x1F; /* port 0~4 by default */

	if (!robo)
		return;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Add rgmii port into phy_portmap */
	rgmii_port = getvar(robo->vars, "rgmii_port");
	if (rgmii_port)
		phy_portmap |= (1 << bcm_strtoul(rgmii_port, NULL, 0));

	/* Enable CFP on phy ports */
	if (enable)
		val8 = phy_portmap;

	robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_CTL_REG, &val8, sizeof(val8));

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);
}

void
robo_fa_enable(robo_info_t *robo, bool on, bool bhdr)
{
	uint16 val16;
	uint8 val8;
	int32 err;

	if (!robo)
		return;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* BCM_HDR and OOB PAUSE */
	if (on) {
		/* Enable BCM_HDR Tag on IMP port if need it. */
#ifdef ETAGG
		if (bhdr)
			err = robo_bhdr_register(robo, BHDR_PORT8, BHDR_FA);
		else
			err = robo_bhdr_unregister(robo, BHDR_PORT8, BHDR_FA);
		if (err) {
			ET_ERROR(("%s: enabling FA but failed to %s bhdr\n",
				__FUNCTION__, bhdr ? "register" : "unregister"));
		}
#else
		val8 = (bhdr ? 0x1 : 0x0);
		robo->ops->write_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
#endif	/* ETAGG */

		/* Use out-of-band signal for Switch and SOC flow control */
		robo->ops->read_reg(robo, PAGE_FC, REG_FC_OOBPAUSE, &val16, sizeof(val16));
		val16 |= (1 << 8);
		robo->ops->write_reg(robo, PAGE_FC, REG_FC_OOBPAUSE, &val16, sizeof(val16));
	}
	else {
		/* Disable BRCM HDR */
#ifdef ETAGG
		err = robo_bhdr_unregister(robo, BHDR_PORT8, BHDR_FA);
		if (err) {
			ET_ERROR(("%s: disabling FA but failed to unregister bhdr\n",
				__FUNCTION__));
		}
#else
		val8 = 0x0;
		robo->ops->write_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
#endif	/* ETAGG */

		/* Default value: Use pause frame for Switch and SOC flow control. */
		robo->ops->read_reg(robo, PAGE_FC, REG_FC_OOBPAUSE, &val16, sizeof(val16));
		val16 &= ~(1 << 8);
		robo->ops->write_reg(robo, PAGE_FC, REG_FC_OOBPAUSE, &val16, sizeof(val16));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);
}
#endif /* !_CFE_ && BCMFA */

/* Configure the VLANs */
int
bcm_robo_config_vlan(robo_info_t *robo, uint8 *mac_addr)
{
	uint8 val8;
	uint16 val16;
	uint32 val32;
	pdesc_t *pdesc;
	int pdescsz;
	uint16 vid;
	uint8 arl_entry[8] = { 0 }, arl_entry1[8] = { 0 };

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* setup global vlan configuration */
	/* VLAN Control 0 Register (Page 0x34, Address 0) */
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL0, &val8, sizeof(val8));
	val8 |= ((1 << 7) |		/* enable 802.1Q VLAN */
	         (3 << 5));		/* individual VLAN learning mode */
	if (robo->devid == DEVID5325)
		val8 &= ~(1 << 1);	/* must clear reserved bit 1 */
	robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL0, &val8, sizeof(val8));

	/* VLAN Control 1 Register (Page 0x34, Address 1) */
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL1, &val8, sizeof(val8));
	val8 |= ((1 << 2) |		/* enable RSV multicast V Fwdmap */
		 (1 << 3));		/* enable RSV multicast V Untagmap */
	if (robo->devid == DEVID5325)
		val8 |= (1 << 1);	/* enable RSV multicast V Tagging */
	robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL1, &val8, sizeof(val8));

	arl_entry[0] = mac_addr[5];
	arl_entry[1] = mac_addr[4];
	arl_entry[2] = mac_addr[3];
	arl_entry[3] = mac_addr[2];
	arl_entry[4] = mac_addr[1];
	arl_entry[5] = mac_addr[0];

	if (robo->devid == DEVID5325) {
		/* Init the entry 1 of the bin */
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E1,
		                     arl_entry1, sizeof(arl_entry1));
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VID_E1,
		                     arl_entry1, 1);

		/* Init the entry 0 of the bin */
		arl_entry[6] = 0x8;		/* Port Id: MII */
		arl_entry[7] = 0xc0;	/* Static Entry, Valid */

		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E0,
		                     arl_entry, sizeof(arl_entry));
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_MINDX,
		                     arl_entry, ETHER_ADDR_LEN);

		/* VLAN Control 4 Register (Page 0x34, Address 4) */
		val8 = (1 << 6);		/* drop frame with VID violation */
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL4, &val8, sizeof(val8));

		/* VLAN Control 5 Register (Page 0x34, Address 5) */
		val8 = (1 << 3);		/* drop frame when miss V table */
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL5, &val8, sizeof(val8));

		pdesc = pdesc25;
		pdescsz = sizeof(pdesc25) / sizeof(pdesc_t);
	} else {
		/* Initialize the MAC Addr Index Register */
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_MINDX,
		                     arl_entry, ETHER_ADDR_LEN);

		pdesc = pdesc97;
		pdescsz = sizeof(pdesc97) / sizeof(pdesc_t);

		if (SRAB_ENAB() && ROBO_IS_BCM5301X(robo->devid))
			robo_cpu_port_upd(robo, pdesc97, pdescsz);
	}

	/* setup each vlan. max. 16 vlans. */
	/* force vlan id to be equal to vlan number */
	for (vid = 0; vid < VLAN_NUMVLANS; vid ++) {
		char vlanports[] = "vlanXXXXports";
		char port[] = "XXXX", *next;
		const char *ports, *cur;
		uint32 untag = 0;
		uint32 member = 0;
		int pid, len;
		int cpuport = 0;

		/* no members if VLAN id is out of limitation */
		if (vid > VLAN_MAXVID)
			goto vlan_setup;

		/* get vlan member ports from nvram */
		sprintf(vlanports, "vlan%dports", vid);
		ports = getvar(robo->vars, vlanports);

		/* In 539x vid == 0 us invalid?? */
		if ((robo->devid != DEVID5325) && (vid == 0)) {
			if (ports)
				ET_ERROR(("VID 0 is set in nvram, Ignoring\n"));
			continue;
		}

		/* disable this vlan if not defined */
		if (!ports)
			goto vlan_setup;

		/*
		 * setup each port in the vlan. cpu port needs special handing
		 * (with or without output tagging) to support linux/pmon/cfe.
		 */
		for (cur = ports; cur; cur = next) {
			/* tokenize the port list */
			while (*cur == ' ')
				cur ++;
			next = bcmstrstr(cur, " ");
			len = next ? next - cur : strlen(cur);
			if (!len)
				break;
			if (len > sizeof(port) - 1)
				len = sizeof(port) - 1;
			strncpy(port, cur, len);
			port[len] = 0;

			/* make sure port # is within the range */
			pid = bcm_atoi(port);
			if (pid >= pdescsz) {
				ET_ERROR(("robo_config_vlan: port %d in vlan%dports is out "
				          "of range[0-%d]\n", pid, vid, pdescsz));
				continue;
			}

			/* build VLAN registers values */
#ifndef	_CFE_
#ifdef BCMFA
			pid = robo_fa_imp_port_upd(robo, port, pid, vid, pdescsz);
#endif
			if ((!pdesc[pid].cpu && !strchr(port, FLAG_TAGGED)) ||
			    (pdesc[pid].cpu && strchr(port, FLAG_UNTAG)))
#endif /* !_CFE_ */
				untag |= pdesc[pid].untag;

			member |= pdesc[pid].member;

			/* set port tag - applies to untagged ingress frames */
			/* Default Port Tag Register (Page 0x34, Address 0x10-0x1D) */
#ifdef	_CFE_
#define	FL	FLAG_LAN
#else
#define	FL	FLAG_UNTAG
#endif /* _CFE_ */
			if (!pdesc[pid].cpu || strchr(port, FL)) {
				val16 = ((0 << 13) |		/* priority - always 0 */
				         vid);			/* vlan id */
				robo->ops->write_reg(robo, PAGE_VLAN, pdesc[pid].ptagr,
				                     &val16, sizeof(val16));
			}

			if (pdesc[pid].cpu)
				cpuport = pid;
		}

		/* Add static ARL entries */
		if (robo->devid == DEVID5325) {
			val8 = vid;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VID_E0,
			                     &val8, sizeof(val8));
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VINDX,
			                     &val8, sizeof(val8));

			/* Write the entry */
			val8 = 0x80;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			                     &val8, sizeof(val8));
			/* Wait for write to complete */
			SPINWAIT((robo->ops->read_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			         &val8, sizeof(val8)), ((val8 & 0x80) != 0)),
			         100 /* usec */);
		} else {
			/* Set the VLAN Id in VLAN ID Index Register */
			val8 = vid;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VINDX,
			                     &val8, sizeof(val8));

			/* Set the MAC addr and VLAN Id in ARL Table MAC/VID Entry 0
			 * Register.
			 */
			arl_entry[6] = vid;
			arl_entry[7] = 0x0;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E0,
			                     arl_entry, sizeof(arl_entry));

			/* Set the Static bit , Valid bit and Port ID fields in
			 * ARL Table Data Entry 0 Register
			 */
			val32 = 0x18000 + cpuport;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_DAT_E0,
			                     &val32, sizeof(val32));

			/* Clear the ARL_R/W bit and set the START/DONE bit in
			 * the ARL Read/Write Control Register.
			 */
			val8 = 0x80;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			                     &val8, sizeof(val8));
			/* Wait for write to complete */
			SPINWAIT((robo->ops->read_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			         &val8, sizeof(val8)), ((val8 & 0x80) != 0)),
			         100 /* usec */);
		}

vlan_setup:
		/* setup VLAN ID and VLAN memberships */

		val32 = (untag |			/* untag enable */
		         member);			/* vlan members */
		if (robo->devid == DEVID5325) {
			if (robo->corerev < 3) {
				val32 |= ((1 << 20) |		/* valid write */
				          ((vid >> 4) << 12));	/* vlan id bit[11:4] */
			} else {
				val32 |= ((1 << 24) |		/* valid write */
				          (vid << 12));	/* vlan id bit[11:4] */
			}
			ET_MSG(("bcm_robo_config_vlan: programming REG_VLAN_WRITE %08x\n", val32));

			/* VLAN Write Register (Page 0x34, Address 0x08-0x0B) */
			robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_WRITE, &val32,
			                     sizeof(val32));
			/* VLAN Table Access Register (Page 0x34, Address 0x06-0x07) */
			val16 = ((1 << 13) |	/* start command */
			         (1 << 12) |	/* write state */
			         vid);		/* vlan id */
			robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_ACCESS, &val16,
			                     sizeof(val16));
		} else {
			uint8 vtble, vtbli, vtbla;

			if ((robo->devid == DEVID5395) ||
				(robo->devid == DEVID53115) ||
				(robo->devid == DEVID53125) ||
				ROBO_IS_BCM5301X(robo->devid)) {
				vtble = REG_VTBL_ENTRY_5395;
				vtbli = REG_VTBL_INDX_5395;
				vtbla = REG_VTBL_ACCESS_5395;
			} else {
				vtble = REG_VTBL_ENTRY;
				vtbli = REG_VTBL_INDX;
				vtbla = REG_VTBL_ACCESS;
			}

			/* VLAN Table Entry Register (Page 0x05, Address 0x63-0x66/0x83-0x86) */
			robo->ops->write_reg(robo, PAGE_VTBL, vtble, &val32,
			                     sizeof(val32));
			/* VLAN Table Address Index Reg (Page 0x05, Address 0x61-0x62/0x81-0x82) */
			val16 = vid;        /* vlan id */
			robo->ops->write_reg(robo, PAGE_VTBL, vtbli, &val16,
			                     sizeof(val16));

			/* VLAN Table Access Register (Page 0x34, Address 0x60/0x80) */
			val8 = ((1 << 7) | 	/* start command */
			        0);	        /* write */
			robo->ops->write_reg(robo, PAGE_VTBL, vtbla, &val8,
			                     sizeof(val8));
		}
	}

	if (robo->devid == DEVID5325) {
		/* setup priority mapping - applies to tagged ingress frames */
		/* Priority Re-map Register (Page 0x34, Address 0x20-0x23) */
		val32 = ((0 << 0) |	/* 0 -> 0 */
		         (1 << 3) |	/* 1 -> 1 */
		         (2 << 6) |	/* 2 -> 2 */
		         (3 << 9) |	/* 3 -> 3 */
		         (4 << 12) |	/* 4 -> 4 */
		         (5 << 15) |	/* 5 -> 5 */
		         (6 << 18) |	/* 6 -> 6 */
		         (7 << 21));	/* 7 -> 7 */
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_PMAP, &val32, sizeof(val32));
	}

	if (robo->devid == DEVID53115) {
		/* Configure the priority system to use to determine the TC of
		 * ingress frames. Use DiffServ TC mapping, otherwise 802.1p
		 * TC mapping, otherwise MAC based TC mapping.
		 */
		val8 = ((0 << 6) |		/* Disable port based QoS */
	                (2 << 2));		/* QoS priority selection */
		robo->ops->write_reg(robo, 0x30, 0, &val8, sizeof(val8));

		/* Configure tx queues scheduling mechanism */
		val8 = (3 << 0);		/* Strict priority */
		robo->ops->write_reg(robo, 0x30, 0x80, &val8, sizeof(val8));

		/* Enable 802.1p Priority to TC mapping for individual ports */
		val16 = 0x11f;
		robo->ops->write_reg(robo, 0x30, 0x4, &val16, sizeof(val16));

		/* Configure the TC to COS mapping. This determines the egress
		 * transmit queue.
		 */
		val16 = ((1 << 0)  |	/* Pri 0 mapped to TXQ 1 */
			 (0 << 2)  |	/* Pri 1 mapped to TXQ 0 */
			 (0 << 4)  |	/* Pri 2 mapped to TXQ 0 */
			 (1 << 6)  |	/* Pri 3 mapped to TXQ 1 */
			 (2 << 8)  |	/* Pri 4 mapped to TXQ 2 */
			 (2 << 10) |	/* Pri 5 mapped to TXQ 2 */
			 (3 << 12) |	/* Pri 6 mapped to TXQ 3 */
			 (3 << 14));	/* Pri 7 mapped to TXQ 3 */
		robo->ops->write_reg(robo, 0x30, 0x62, &val16, sizeof(val16));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}

static void
bcm_robo_enable_rgmii_port(robo_info_t *robo)
{
        char *var;
        int rgmii_port = -1;
        uint8 val8;

        var = getvar(robo->vars, "rgmii_port");
        if (var)
                rgmii_port = bcm_strtoul(var, NULL, 0);

        if (SRAB_ENAB() && ROBO_IS_BCM5301X(robo->devid)) {
                if (rgmii_port == 5) {
                        val8 = 0;
                        robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT5_GMIIPO,
                                &val8, sizeof(val8));
                                val8 |=
                                (1 << 7) |      /* GMII_SPEED_UP_2G */
                                (1 << 6) |      /* SW_OVERRIDE */
                                (1 << 5) |      /* TXFLOW_CNTL */
                                (1 << 4) |      /* RXFLOW_CNTL */
                                                /* default(2 << 2) SPEED :
						 * 2b10 1000/2000Mbps
						 */
						/* default(1 << 1) DUPLX_MODE:
						 * Full Duplex
						 */
                                (1 << 0);       /* LINK_STS: Link up */
                        robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PORT5_GMIIPO,
                        &val8, sizeof(val8));
                }
        }
}

/* Enable switching/forwarding */
int
bcm_robo_enable_switch(robo_info_t *robo)
{
	int i, max_port_ind, ret = 0;
	uint8 val8;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Switch Mode register (Page 0, Address 0x0B) */
	robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));

	if (!mang_mode_en) {
		/* Set unmanaged mode if no any other GMAC enable mang mode */
		val8 &= (~(1 << 0));
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));
	}

	/* Bit 1 enables switching/forwarding */
	if (!(val8 & (1 << 1))) {
		/* Set unmanaged mode */
		val8 &= (~(1 << 0));

		/* Enable forwarding */
		val8 |= (1 << 1);
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));

		/* Read back */
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));
		if (!(val8 & (1 << 1))) {
			ET_ERROR(("robo_enable_switch: enabling forwarding failed\n"));
			ret = -1;
		}

		/* No spanning tree for unmanaged mode */
		val8 = 0;
		if (robo->devid == DEVID5398 || ROBO_IS_BCM5301X(robo->devid))
			max_port_ind = REG_CTRL_PORT7;
		else
			max_port_ind = REG_CTRL_PORT4;

		for (i = REG_CTRL_PORT0; i <= max_port_ind; i++) {
			if (ROBO_IS_BCM5301X(robo->devid) && i == REG_CTRL_PORT6)
				continue;
			robo->ops->write_reg(robo, PAGE_CTRL, i, &val8, sizeof(val8));
		}

		/* No spanning tree on IMP port too */
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_IMP, &val8, sizeof(val8));
	}

	if (robo->devid == DEVID53125) {
		/* Over ride IMP port status to make it link by default */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		val8 |= 0xb1;	/* Make Link pass and override it. */
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		/* Init the EEE feature */
		robo_eee_advertise_init(robo);
	}

	if (SRAB_ENAB() && ROBO_IS_BCM5301X(robo->devid)) {
		int pdescsz = sizeof(pdesc97) / sizeof(pdesc_t);

#ifdef CFG_SIM
		/* Over ride Port0 ~ Port4 status to make it link by default */
		/* (Port0 ~ Port4) LINK_STS bit default is 0x1(link up), do it anyway */
		for (i = REG_CTRL_PORT0_GMIIPO; i <= REG_CTRL_PORT4_GMIIPO; i++) {
			val8 = 0;
			robo->ops->read_reg(robo, PAGE_CTRL, i, &val8, sizeof(val8));
			val8 |= 0x71;	/* Make it link by default. */
			robo->ops->write_reg(robo, PAGE_CTRL, i, &val8, sizeof(val8));
		}
#endif

		/* Find the first cpu port and do software override */
		for (i = 0; i < pdescsz; i++) {
			/* Port 6 is not able to use */
			if (i == 6 || !pdesc97[i].cpu)
				continue;

			if (i == pdescsz - 1) {	/* Port8: REG_CTRL_MIIPO */
				/* Port 8 IMP Port States Override Register (Page 0, Address 0xe) */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO,
					&val8, sizeof(val8));
				val8 |=
					(1 << 7) |	/* MII_SW_OR MII Software Override */
					(1 << 6) |	/* GMII_SPEED_UP_2G */
					(1 << 5) |	/* TXFLOW_CNTL */
					(1 << 4) |	/* RXFLOW_CNTL */
							/* default(2 << 2) SPEED :
							 * 2b10 1000/2000Mbps
							 */
							/* default(1 << 1) DUPLX_MODE:
							 * Full Duplex
							 */
					(1 << 0);	/* LINK_STS: Link up */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO,
					&val8, sizeof(val8));

				/* Port8 IMP0 Control Register (Page 0, Address 0x08) */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_IMP,
					&val8, sizeof(val8));
				val8 |=
					(1 << 4) |	/* RX_UCST_EN Receive Unicast Enable */
					(1 << 3) |	/* RX_MCST_EN Receive Multicast Enable */
					(1 << 2);	/* RX_BCST_EN Receive Broadcast Enable */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_IMP,
					&val8, sizeof(val8));

				/* Global Management Configuration Register (Page 2, Address 0x0) */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_MMR, REG_MGMT_CFG,
					&val8, sizeof(val8));
				val8 |=
					(2 << 6);	/* FRM_MNGP: Enable IMP0 only */
				robo->ops->write_reg(robo, PAGE_MMR, REG_MGMT_CFG,
					&val8, sizeof(val8));

				/* 802.1Q VLAN Control 5 Register (Page 0x34, Address 0x06) */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL5,
					&val8, sizeof(val8));
				val8 |= (1 << 0);	/* EN_CPU_RX_BYP_INNER_CRC_CHK:
							 * IMP port ignore CRC
							 */
				robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL5,
					&val8, sizeof(val8));

				/* Switch Mode Register (Page 0, Address 0x0B): Set Managed Mode */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE,
					&val8, sizeof(val8));
				val8 |= (1 << 0);	/* SW_FWDG_MODE Managed Mode */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MODE,
					&val8, sizeof(val8));
				mang_mode_en = TRUE;

				/* BCM_GMAC3: Enable ports 5 and 7 for SMP dual core 3 GMAC setup */

				/* Port 5 GMII Port States Override Register
				 * (Page 0, Address 0x5d)
				 */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT5_GMIIPO,
					&val8, sizeof(val8));
#if defined(BCM_GMAC3)
				val8 |=
					(1 << 7) |	/* GMII_SPEED_UP_2G */
					(1 << 6) |	/* SW_OVERRIDE */
					(1 << 5) |	/* TXFLOW_CNTL */
					(1 << 4) |	/* RXFLOW_CNTL */
							/* default(2 << 2) SPEED :
							 * 2b10 1000/2000Mbps
							 */
							/* default(1 << 1) DUPLX_MODE:
							 * Full Duplex
							 */
					(1 << 0);	/* LINK_STS: Link up */
#else  /* ! BCM_GMAC3 */
				val8 &= ~(1 << 0);
#endif /* ! BCM_GMAC3 */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PORT5_GMIIPO,
					&val8, sizeof(val8));

				/* Port 7 GMII Port States Override Register
				 * (Page 0, Address 0x5f)
				 */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT7_GMIIPO,
					&val8, sizeof(val8));
#if defined(BCM_GMAC3)
				val8 |=
					(1 << 7) |	/* GMII_SPEED_UP_2G */
					(1 << 6) |	/* SW_OVERRIDE */
					(1 << 5) |	/* TXFLOW_CNTL */
					(1 << 4) |	/* RXFLOW_CNTL */
							/* default(2 << 2) SPEED :
							 * 2b10 1000/2000Mbps
							 */
							/* default(1 << 1) DUPLX_MODE:
							 * Full Duplex
							 */
					(1 << 0);	/* LINK_STS: Link up */
#else  /* ! BCM_GMAC3 */
				val8 &= ~(1 << 0);
#endif /* ! BCM_GMAC3 */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PORT7_GMIIPO,
					&val8, sizeof(val8));
			} else {	/* Port5|7: REG_CTRL_PORT0_GMIIPO + i */

				/* Port 5|7 GMII Port States Override Register
				 * (Page 0, Address 0x5d|0x5f)
				 */
				val8 = 0;
				robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT0_GMIIPO + i,
					&val8, sizeof(val8));
				val8 |=
					(1 << 7) |	/* GMII_SPEED_UP_2G */
					(1 << 6) |	/* SW_OVERRIDE */
					(1 << 5) |	/* TXFLOW_CNTL */
					(1 << 4) |	/* RXFLOW_CNTL */
							/* default(2 << 2) SPEED :
							 * 2b10 1000/2000Mbps
							 */
							/* default(1 << 1) DUPLX_MODE:
							 * Full Duplex
							 */
					(1 << 0);	/* LINK_STS: Link up */
				robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PORT0_GMIIPO + i,
					&val8, sizeof(val8));
			}
			break;
		}

		/* BRCM HDR Control Register (Page 2, Address 0x03) */
		val8 = 0x0; /* Disable BRCM HDR by default */
		robo->ops->write_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));

		/* Disable CFP by default */
		val8 = 0x0;
		robo->ops->write_reg(robo, PAGE_CFP, REG_CFP_CTL_REG, &val8, sizeof(val8));
	}

	bcm_robo_enable_rgmii_port(robo);

	/* Drop reserved bit, if any */
	robo->ops->read_reg(robo, PAGE_CTRL, 0x2f, &val8, sizeof(val8));
	if (robo->devid != DEVID5325 && val8 & (1 << 1)) {
		val8 &= ~(1 << 1);
		robo->ops->write_reg(robo, PAGE_CTRL, 0x2f, &val8, sizeof(val8));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return ret;
}

#ifdef BCMDBG
void
robo_dump_regs(robo_info_t *robo, struct bcmstrbuf *b)
{
	uint8 val8;
	uint16 val16;
	uint32 val32;
	pdesc_t *pdesc;
	int pdescsz;
	int i;

	bcm_bprintf(b, "%s:\n", robo->ops->desc);
	if (robo->miird == NULL) {
		bcm_bprintf(b, "SPI gpio pins: ss %d sck %d mosi %d miso %d\n",
		            robo->ss, robo->sck, robo->mosi, robo->miso);
	}

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Dump registers interested */
	robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));
	bcm_bprintf(b, "(0x00,0x0B)Switch mode regsiter: 0x%02x\n", val8);
	if (robo->devid == DEVID5325) {
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x00,0x0E)MII port state override regsiter: 0x%02x\n", val8);
	}
	if (robo->miird == NULL)
		goto exit;
	if (robo->devid == DEVID5325) {
		pdesc = pdesc25;
		pdescsz = sizeof(pdesc25) / sizeof(pdesc_t);
	} else {
		pdesc = pdesc97;
		pdescsz = sizeof(pdesc97) / sizeof(pdesc_t);
	}

	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL0, &val8, sizeof(val8));
	bcm_bprintf(b, "(0x34,0x00)VLAN control 0 register: 0x%02x\n", val8);
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL1, &val8, sizeof(val8));
	bcm_bprintf(b, "(0x34,0x01)VLAN control 1 register: 0x%02x\n", val8);
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL4, &val8, sizeof(val8));
	if (robo->devid == DEVID5325) {
		bcm_bprintf(b, "(0x34,0x04)VLAN control 4 register: 0x%02x\n", val8);
		robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL5, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x34,0x05)VLAN control 5 register: 0x%02x\n", val8);

		robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_PMAP, &val32, sizeof(val32));
		bcm_bprintf(b, "(0x34,0x20)Prio Re-map: 0x%08x\n", val32);

		for (i = 0; i <= VLAN_MAXVID; i++) {
			val16 = (1 << 13)	/* start command */
			        | i;		/* vlan id */
			robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_ACCESS, &val16,
			                     sizeof(val16));
			robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_READ, &val32, sizeof(val32));
			bcm_bprintf(b, "(0x34,0xc)VLAN %d untag bits: 0x%02x member bits: 0x%02x\n",
			            i, (val32 & 0x0fc0) >> 6, (val32 & 0x003f));
		}

	} else {
		uint8 vtble, vtbli, vtbla;

		if ((robo->devid == DEVID5395) ||
			(robo->devid == DEVID53115) ||
			(robo->devid == DEVID53125) ||
			ROBO_IS_BCM5301X(robo->devid)) {
			vtble = REG_VTBL_ENTRY_5395;
			vtbli = REG_VTBL_INDX_5395;
			vtbla = REG_VTBL_ACCESS_5395;
		} else {
			vtble = REG_VTBL_ENTRY;
			vtbli = REG_VTBL_INDX;
			vtbla = REG_VTBL_ACCESS;
		}

		for (i = 0; i <= VLAN_MAXVID; i++) {
			/*
			 * VLAN Table Address Index Register
			 * (Page 0x05, Address 0x61-0x62/0x81-0x82)
			 */
			val16 = i;		/* vlan id */
			robo->ops->write_reg(robo, PAGE_VTBL, vtbli, &val16,
			                     sizeof(val16));
			/* VLAN Table Access Register (Page 0x34, Address 0x60/0x80) */
			val8 = ((1 << 7) | 	/* start command */
			        1);		/* read */
			robo->ops->write_reg(robo, PAGE_VTBL, vtbla, &val8,
			                     sizeof(val8));
			/* VLAN Table Entry Register (Page 0x05, Address 0x63-0x66/0x83-0x86) */
			robo->ops->read_reg(robo, PAGE_VTBL, vtble, &val32,
			                    sizeof(val32));
			bcm_bprintf(b, "VLAN %d untag bits: 0x%02x member bits: 0x%02x\n",
			            i, (val32 & 0x3fe00) >> 9, (val32 & 0x1ff));
		}
	}
	for (i = 0; i < pdescsz; i++) {
		robo->ops->read_reg(robo, PAGE_VLAN, pdesc[i].ptagr, &val16, sizeof(val16));
		bcm_bprintf(b, "(0x34,0x%02x)Port %d Tag: 0x%04x\n", pdesc[i].ptagr, i, val16);
	}

	if (SRAB_ENAB() && ROBO_IS_BCM5301X(robo->devid)) {
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT5_GMIIPO, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x)Port 5 States Override: 0x%02x\n",
			PAGE_CTRL, REG_CTRL_PORT5_GMIIPO, val8);
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PORT7_GMIIPO, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x)Port 7 States Override: 0x%02x\n",
			PAGE_CTRL, REG_CTRL_PORT7_GMIIPO, val8);
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x)Port 8 States Override: 0x%02x\n",
			PAGE_CTRL, REG_CTRL_MIIPO, val8);
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_IMP, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x)Port 8 IMP Control Register: 0x%02x\n",
			PAGE_CTRL, REG_CTRL_IMP, val8);
		robo->ops->read_reg(robo, PAGE_MMR, REG_MGMT_CFG, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) Global Management Config: 0x%02x\n",
			PAGE_MMR, REG_MGMT_CFG, val8);
		robo->ops->read_reg(robo, PAGE_MMR, REG_MGMT_CFG, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) 802.1Q VLAN Control 5: 0x%02x\n",
			PAGE_MMR, REG_MGMT_CFG, val8);
		robo->ops->read_reg(robo, PAGE_MMR, REG_BRCM_HDR, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) BRCM HDR Control Register: 0x%02x\n",
			PAGE_MMR, REG_BRCM_HDR, val8);
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) Switch Mode Register: 0x%02x\n",
			PAGE_CTRL, REG_CTRL_MODE, val8);

#ifdef BCMFA
		/* Dump CFP */
		bcm_bprintf(b, "\n Switch CFP Dump\n");

		/* Port CFP Enabled */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CFP, REG_CFP_CTL_REG, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) CFP Enable Map: 0x%02x\n",
			PAGE_CFP, REG_CFP_CTL_REG, val8);

		/* Action policy */
		/* Index 0~4 (IPv4/6 TCP FIN/RST) */
		robo_fa_aux_dump_action_policy(robo, b, 0, " (IPv4 TCP FIN)");
		robo_fa_aux_dump_action_policy(robo, b, 1, " (IPv4 TCP RST)");
		robo_fa_aux_dump_action_policy(robo, b, 2, " (IPv6 TCP FIN)");
		robo_fa_aux_dump_action_policy(robo, b, 3, " (IPv6 TCP RST)");

		/* Data and Mask */
		/* IPv4 index 0~4 (IPv4/6 TCP FIN/RST) */
		robo_fa_aux_dump_tcam(robo, b, 0, " (IPv4 TCP FIN)");
		robo_fa_aux_dump_tcam(robo, b, 1, " (IPv4 TCP RST)");
		robo_fa_aux_dump_tcam(robo, b, 2, " (IPv6 TCP FIN)");
		robo_fa_aux_dump_tcam(robo, b, 3, " (IPv6 TCP RST)");

		/* UDFs */
		/* Slice 0 for IPv4 packet -FIN */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CFP, REG_CFP_UDF_0_A_0_8, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) CFP UDF_0_A_0_0 Offset (IPv4 TCP FIN): 0x%02x\n",
			PAGE_CFP, REG_CFP_UDF_0_A_0_8, val8);
		/* Slice 0 for IPv4 packet -RST */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CFP, REG_CFP_UDF_0_A_0_8+1, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) CFP UDF_0_A_0_1 Offset (IPv4 TCP RST): 0x%02x\n",
			PAGE_CFP, REG_CFP_UDF_0_A_0_8+1, val8);
		/* Slice 0 for IPv6 packet -FIN */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CFP, REG_CFP_UDF_0_B_0_8, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) CFP UDF_0_B_0_0 Offset (IPv6 TCP FIN): 0x%02x\n",
			PAGE_CFP, REG_CFP_UDF_0_B_0_8, val8);
		/* Slice 0 for IPv6 packet -RST */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CFP, REG_CFP_UDF_0_B_0_8+1, &val8, sizeof(val8));
		bcm_bprintf(b, "(0x%02x,0x%02x) CFP UDF_0_B_0_1 Offset (IPv6 TCP RST): 0x%02x\n",
			PAGE_CFP, REG_CFP_UDF_0_B_0_8+1, val8);

		/* Statistic */
		robo_fa_aux_dump_rate_counter(robo, b, 0, " (IPv4 TCP FIN)");
		robo_fa_aux_dump_rate_counter(robo, b, 1, " (IPv4 TCP RST)");
		robo_fa_aux_dump_rate_counter(robo, b, 2, " (IPv6 TCP FIN)");
		robo_fa_aux_dump_rate_counter(robo, b, 3, " (IPv6 TCP RST)");
#endif /* BCMFA */
	}

exit:
	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);
}
#endif /* BCMDBG */

#ifndef	_CFE_
/*
 * Update the power save configuration for ports that changed link status.
 */
void
robo_power_save_mode_update(robo_info_t *robo)
{
	uint phy;

	for (phy = 0; phy < MAX_NO_PHYS; phy++) {
		if (robo->pwrsave_mode_auto & (1 << phy)) {
			ET_MSG(("%s: set port %d to auto mode\n",
				__FUNCTION__, phy));
			robo_power_save_mode(robo, ROBO_PWRSAVE_AUTO, phy);
		}
	}

	return;
}

static int32
robo_power_save_mode_clear_auto(robo_info_t *robo, int32 phy)
{
	uint16 val16;

	if (robo->devid == DEVID53115) {
		/* For 53115 0x1C is the MII address of the auto power
		 * down register. Bit 5 is enabling the mode
		 * bits has the following purpose
		 * 15 - write enable 10-14 shadow register select 01010 for
		 * auto power 6-9 reserved 5 auto power mode enable
		 * 4 sleep timer select : 1 means 5.4 sec
		 * 0-3 wake up timer select: 0xF 1.26 sec
		 */
		val16 = 0xa800;
		robo->miiwr(robo->h, phy, REG_MII_AUTO_PWRDOWN, val16);
	} else if ((robo->sih->chip == BCM5356_CHIP_ID) || (robo->sih->chip == BCM5357_CHIP_ID)) {
		/* To disable auto power down mode
		 * clear bit 5 of Aux Status 2 register
		 * (Shadow reg 0x1b). Shadow register
		 * access is enabled by writing
		 * 1 to bit 7 of MII register 0x1f.
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_BRCM_TEST);
		robo->miiwr(robo->h, phy, REG_MII_BRCM_TEST,
		            (val16 | (1 << 7)));

		/* Disable auto power down by clearing
		 * bit 5 of to Aux Status 2 reg.
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_AUX_STATUS2);
		robo->miiwr(robo->h, phy, REG_MII_AUX_STATUS2,
		            (val16 & ~(1 << 5)));

		/* Undo shadow access */
		val16 = robo->miird(robo->h, phy, REG_MII_BRCM_TEST);
		robo->miiwr(robo->h, phy, REG_MII_BRCM_TEST,
		            (val16 & ~(1 << 7)));
	} else
		return -1;

	robo->pwrsave_mode_phys[phy] &= ~ROBO_PWRSAVE_AUTO;

	return 0;
}

static int32
robo_power_save_mode_clear_manual(robo_info_t *robo, int32 phy)
{
	uint8 val8;
	uint16 val16;

	if ((robo->devid == DEVID53115) ||
	    (robo->sih->chip == BCM5356_CHIP_ID)) {
		/* For 53115 0x0 is the MII control register
		 * Bit 11 is the power down mode bit
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_CTRL);
		val16 &= 0xf7ff;
		robo->miiwr(robo->h, phy, REG_MII_CTRL, val16);
	} else if (ROBO_IS_BCM5301X(robo->devid)) {
		robo->ops->read_reg(robo, 0x10+phy, 0x00, &val16, sizeof(val16));
		val16 &= 0xf7ff;
		robo->ops->write_reg(robo, 0x10+phy, 0x00, &val16, sizeof(val16));
	} else if (robo->devid == DEVID5325) {
		if ((robo->sih->chip == BCM5357_CHIP_ID) ||
		    (robo->sih->chip == BCM4749_CHIP_ID) ||
		    (robo->sih->chip == BCM53572_CHIP_ID)) {
			robo->miiwr(robo->h, phy, 0x1f, 0x8b);
			robo->miiwr(robo->h, phy, 0x14, 0x0008);
			robo->miiwr(robo->h, phy, 0x10, 0x0400);
			robo->miiwr(robo->h, phy, 0x11, 0x0000);
			robo->miiwr(robo->h, phy, 0x1f, 0x0b);
		} else {
			if (phy == 0)
				return -1;
			/* For 5325 page 0x00 address 0x0F is the power down
			 * mode register. Bits 1-4 determines which of the
			 * phys are enabled for this mode
			 */
			robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PWRDOWN,
			                    &val8, sizeof(val8));
			val8 &= ~(0x1  << phy);
			robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PWRDOWN,
				&val8, sizeof(val8));
		}
	} else
		return -1;

	robo->pwrsave_mode_phys[phy] &= ~ROBO_PWRSAVE_MANUAL;

	return 0;
}

/*
 * Function which periodically checks the power save mode on the switch
 */
int32
robo_power_save_toggle(robo_info_t *robo, int32 normal)
{
	int32 phy;
	uint16 link_status;


	/* read the link status of all ports */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_LINK,
		&link_status, sizeof(uint16));
	link_status &= 0x1f;

	/* Take the phys out of the manual mode first so that link status
	 * can be checked. Once out of that  mode check the link status
	 * and if any of the link is up do not put that phy into
	 * manual power save mode
	 */
	for (phy = 0; phy < MAX_NO_PHYS; phy++) {
		/* When auto+manual modes are enabled we toggle between
		 * manual and auto modes. When only manual mode is enabled
		 * we toggle between manual and normal modes. When only
		 * auto mode is enabled there is no need to do anything
		 * here since auto mode is one time config.
		 */
		if ((robo->pwrsave_phys & (1 << phy)) &&
		    (robo->pwrsave_mode_manual & (1 << phy))) {
			if (!normal) {
				/* Take the port out of the manual mode */
				robo_power_save_mode_clear_manual(robo, phy);
			} else {
				/* If the link is down put it back to manual else
				 * remain in the current state
				 */
				if (!(link_status & (1 << phy))) {
					ET_MSG(("%s: link down, set port %d to man mode\n",
						__FUNCTION__, phy));
					robo_power_save_mode(robo, ROBO_PWRSAVE_MANUAL, phy);
				}
			}
		}
	}

	return 0;
}

/*
 * Switch the ports to normal mode.
 */
static int32
robo_power_save_mode_normal(robo_info_t *robo, int32 phy)
{
	int32 error = 0;

	/* If the phy in the power save mode come out of it */
	switch (robo->pwrsave_mode_phys[phy]) {
		case ROBO_PWRSAVE_AUTO_MANUAL:
		case ROBO_PWRSAVE_AUTO:
			error = robo_power_save_mode_clear_auto(robo, phy);
			if ((error == -1) ||
			    (robo->pwrsave_mode_phys[phy] == ROBO_PWRSAVE_AUTO))
				break;

		case ROBO_PWRSAVE_MANUAL:
			error = robo_power_save_mode_clear_manual(robo, phy);
			break;

		default:
			break;
	}

	return error;
}

/*
 * Switch all the inactive ports to auto power down mode.
 */
static int32
robo_power_save_mode_auto(robo_info_t *robo, int32 phy)
{
	uint16 val16;

	/* If the switch supports auto power down enable that */
	if (robo->devid ==  DEVID53115) {
		/* For 53115 0x1C is the MII address of the auto power
		 * down register. Bit 5 is enabling the mode
		 * bits has the following purpose
		 * 15 - write enable 10-14 shadow register select 01010 for
		 * auto power 6-9 reserved 5 auto power mode enable
		 * 4 sleep timer select : 1 means 5.4 sec
		 * 0-3 wake up timer select: 0xF 1.26 sec
		 */
		robo->miiwr(robo->h, phy, REG_MII_AUTO_PWRDOWN, 0xA83F);
	} else if ((robo->sih->chip == BCM5356_CHIP_ID) || (robo->sih->chip == BCM5357_CHIP_ID)) {
		/* To enable auto power down mode set bit 5 of
		 * Auxillary Status 2 register (Shadow reg 0x1b)
		 * Shadow register access is enabled by writing
		 * 1 to bit 7 of MII register 0x1f.
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_BRCM_TEST);
		robo->miiwr(robo->h, phy, REG_MII_BRCM_TEST,
		            (val16 | (1 << 7)));

		/* Enable auto power down by writing to Auxillary
		 * Status 2 reg.
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_AUX_STATUS2);
		robo->miiwr(robo->h, phy, REG_MII_AUX_STATUS2,
		            (val16 | (1 << 5)));

		/* Undo shadow access */
		val16 = robo->miird(robo->h, phy, REG_MII_BRCM_TEST);
		robo->miiwr(robo->h, phy, REG_MII_BRCM_TEST,
		            (val16 & ~(1 << 7)));
	} else
		return -1;

	robo->pwrsave_mode_phys[phy] |= ROBO_PWRSAVE_AUTO;

	return 0;
}

/*
 * Switch all the inactive ports to manual power down mode.
 */
static int32
robo_power_save_mode_manual(robo_info_t *robo, int32 phy)
{
	uint8 val8;
	uint16 val16;

	/* For both 5325 and 53115 the link status register is the same */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_LINK,
	                    &val16, sizeof(val16));
	if (val16 & (0x1 << phy))
		return 0;

	/* If the switch supports manual power down enable that */
	if ((robo->devid ==  DEVID53115) ||
	    (robo->sih->chip == BCM5356_CHIP_ID)) {
		/* For 53115 0x0 is the MII control register bit 11 is the
		 * power down mode bit
		 */
		val16 = robo->miird(robo->h, phy, REG_MII_CTRL);
		robo->miiwr(robo->h, phy, REG_MII_CTRL, val16 | 0x800);
	} else if (ROBO_IS_BCM5301X(robo->devid)) {
		robo->ops->read_reg(robo, 0x10+phy, 0x00, &val16, sizeof(val16));
		val16 |= 0x800;
		robo->ops->write_reg(robo, 0x10+phy, 0x00, &val16, sizeof(val16));
	} else  if (robo->devid == DEVID5325) {
		if ((robo->sih->chip == BCM5357_CHIP_ID) ||
		    (robo->sih->chip == BCM4749_CHIP_ID)||
		    (robo->sih->chip == BCM53572_CHIP_ID)) {
			robo->miiwr(robo->h, phy, 0x1f, 0x8b);
			robo->miiwr(robo->h, phy, 0x14, 0x6000);
			robo->miiwr(robo->h, phy, 0x10, 0x700);
			robo->miiwr(robo->h, phy, 0x11, 0x1000);
			robo->miiwr(robo->h, phy, 0x1f, 0x0b);
		} else {
			if (phy == 0)
				return -1;
			/* For 5325 page 0x00 address 0x0F is the power down mode
			 * register. Bits 1-4 determines which of the phys are enabled
			 * for this mode
			 */
			robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_PWRDOWN, &val8,
				sizeof(val8));
			val8 |= (1 << phy);
			robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_PWRDOWN, &val8,
				sizeof(val8));
		}
	} else
		return -1;

	robo->pwrsave_mode_phys[phy] |= ROBO_PWRSAVE_MANUAL;

	return 0;
}

/*
 * Set power save modes on the robo switch
 */
int32
robo_power_save_mode(robo_info_t *robo, int32 mode, int32 phy)
{
	int32 error = -1;

	if (phy > MAX_NO_PHYS) {
		ET_ERROR(("Passed parameter phy is out of range\n"));
		return -1;
	}

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	switch (mode) {
		case ROBO_PWRSAVE_NORMAL:
			/* If the phy in the power save mode come out of it */
			error = robo_power_save_mode_normal(robo, phy);
			break;

		case ROBO_PWRSAVE_AUTO_MANUAL:
			/* If the switch supports auto and manual power down
			 * enable both of them
			 */
		case ROBO_PWRSAVE_AUTO:
			error = robo_power_save_mode_auto(robo, phy);
			if ((error == -1) || (mode == ROBO_PWRSAVE_AUTO))
				break;

		case ROBO_PWRSAVE_MANUAL:
			error = robo_power_save_mode_manual(robo, phy);
			break;

		default:
			break;
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return error;
}

/*
 * Get the current power save mode of the switch ports.
 */
int32
robo_power_save_mode_get(robo_info_t *robo, int32 phy)
{
	ASSERT(robo);

	if (phy >= MAX_NO_PHYS)
		return -1;

	return robo->pwrsave_mode_phys[phy];
}

/*
 * Configure the power save mode for the switch ports.
 */
int32
robo_power_save_mode_set(robo_info_t *robo, int32 mode, int32 phy)
{
	int32 error;

	ASSERT(robo);

	if (phy >= MAX_NO_PHYS)
		return -1;

	error = robo_power_save_mode(robo, mode, phy);

	if (error)
		return error;

	if (mode == ROBO_PWRSAVE_NORMAL) {
		robo->pwrsave_mode_manual &= ~(1 << phy);
		robo->pwrsave_mode_auto &= ~(1 << phy);
	} else if (mode == ROBO_PWRSAVE_AUTO) {
		robo->pwrsave_mode_auto |= (1 << phy);
		robo->pwrsave_mode_manual &= ~(1 << phy);
		robo_power_save_mode_clear_manual(robo, phy);
	} else if (mode == ROBO_PWRSAVE_MANUAL) {
		robo->pwrsave_mode_manual |= (1 << phy);
		robo->pwrsave_mode_auto &= ~(1 << phy);
		robo_power_save_mode_clear_auto(robo, phy);
	} else {
		robo->pwrsave_mode_auto |= (1 << phy);
		robo->pwrsave_mode_manual |= (1 << phy);
	}

	return 0;
}
#endif /* _CFE_ */

#ifdef PLC
void
robo_plc_hw_init(robo_info_t *robo)
{
	uint8 val8;

	ASSERT(robo);

	if (!robo->plc_hw)
		return;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	if (robo->devid == DEVID53115) {
		/* Fix the duplex mode and speed for Port 5 */
		val8 = ((1 << 6) | (1 << 2) | 3);
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIP5O, &val8, sizeof(val8));
	} else if ((robo->sih->chip == BCM5357_CHIP_ID) &&
	           (robo->sih->chippkg == BCM5358_PKG_ID)) {
		/* Fix the duplex mode and speed for Port 4 (MII port). Force
		 * full duplex mode and set speed to 100.
		 */
		si_pmu_chipcontrol(robo->sih, 2, (1 << 1) | (1 << 2), (1 << 1) | (1 << 2));
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	ET_MSG(("%s: Configured PLC MII interface\n", __FUNCTION__));
}
#endif /* PLC */

/* BCM53125 EEE IOP WAR for some other vendor's wrong EEE implementation. */

static void
robo_link_down(robo_info_t *robo, int32 phy)
{
	if (robo->devid != DEVID53125)
		return;

	printf("robo_link_down: applying EEE WAR for 53125 port %d link-down\n", phy);

	robo->miiwr(robo->h, phy, 0x18, 0x0c00);
	robo->miiwr(robo->h, phy, 0x17, 0x001a);
	robo->miiwr(robo->h, phy, 0x15, 0x0007);
	robo->miiwr(robo->h, phy, 0x18, 0x0400);
}

static void
robo_link_up(robo_info_t *robo, int32 phy)
{
	if (robo->devid != DEVID53125)
		return;

	printf("robo_link_down: applying EEE WAR for 53125 port %d link-up\n", phy);

	robo->miiwr(robo->h, phy, 0x18, 0x0c00);
	robo->miiwr(robo->h, phy, 0x17, 0x001a);
	robo->miiwr(robo->h, phy, 0x15, 0x0003);
	robo->miiwr(robo->h, phy, 0x18, 0x0400);
}

void
robo_watchdog(robo_info_t *robo)
{
	int32 phy;
	uint16 link_status;
	static int first = 1;

	if (robo->devid != DEVID53125)
		return;

	if (!robo->eee_status)
		return;

	/* read the link status of all ports */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_LINK,
		&link_status, sizeof(uint16));
	link_status &= 0x1f;
	if (first || (link_status != robo->prev_status)) {
		for (phy = 0; phy < MAX_NO_PHYS; phy++) {
			if (first) {
				if (!(link_status & (1 << phy)))
					robo_link_down(robo, phy);
			} else if ((link_status & (1 << phy)) != (robo->prev_status & (1 << phy))) {
				if (!(link_status & (1 << phy)))
					robo_link_down(robo, phy);
				else
					robo_link_up(robo, phy);
			}
		}
		robo->prev_status = link_status;
		first = 0;
	}
}

void
robo_eee_advertise_init(robo_info_t *robo)
{
	uint16 val16;
	int32 phy;

	ASSERT(robo);

	val16 = 0;
	robo->ops->read_reg(robo, 0x92, 0x00, &val16, sizeof(val16));
	if (val16 == 0x1f) {
		robo->eee_status = TRUE;
		printf("bcm_robo_enable_switch: EEE is enabled\n");
	} else {
		for (phy = 0; phy < MAX_NO_PHYS; phy++) {
			/* select DEVAD 7 */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL1, 0x0007);
			/* select register 3Ch */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL2, 0x003c);
			/* read command */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL1, 0xc007);
			/* read data */
			val16 = robo->miird(robo->h, phy, REG_MII_CLAUSE_45_CTL2);
			val16 &= ~0x6;
			/* select DEVAD 7 */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL1, 0x0007);
			/* select register 3Ch */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL2, 0x003c);
			/* write command */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL1, 0x4007);
			/* write data */
			robo->miiwr(robo->h, phy, REG_MII_CLAUSE_45_CTL2, val16);
		}
		robo->eee_status = FALSE;
		printf("bcm_robo_enable_switch: EEE is disabled\n");
	}
}

int
bcm_robo_flow_control(robo_info_t *robo, bool set)
{
	uint8 val8;
	int ret = -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Only 53125 family is supported for now */
	if (robo->devid == DEVID53125) {
		/* Over ride IMP port flow control RX/TX capability */
		val8 = 0;
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		if (set)
			val8 |= (RXTX_FLOW_CTRL_MASK << RXTX_FLOW_CTRL_SHIFT);
		else
			val8 &= ~(RXTX_FLOW_CTRL_MASK << RXTX_FLOW_CTRL_SHIFT);
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &val8, sizeof(val8));
		ret = 0;
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return ret;
}

#ifdef BCMAGG
int32
robo_update_agg_group(robo_info_t *robo, int group, uint32 portmap)
{
	uint8 val8;
	uint16 val16;

	if (!robo)
		return -1;

	if (!BCM4707_CHIP(CHIPID(robo->sih->chip)))
		return -1;

	if ((group >= MAX_AGG_GROUP) || (portmap >> MAX_NO_PHYS))
		return -1;

	ET_MSG(("%s: goup %d portmap 0x%x\n", __FUNCTION__, group, portmap));

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	val8 =	(1 << 3) |	/* Enable Trunking */
		(0 << 0);	/* HASH_SEL = 0 */
	robo->ops->write_reg(robo, PAGE_TRUNKING, REG_TRUNKING_CTL,
		&val8, sizeof(val8));

	/* Enable Trunking Group according to portmap */
	val16 = (uint16)portmap;
	robo->ops->write_reg(robo, PAGE_TRUNKING, REG_TRUNKING_GRP0 + (group * 2),
		&val16, sizeof(val16));

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}

uint16
robo_get_linksts(robo_info_t *robo)
{
	uint16 link_status;

	if (!robo)
		return -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Read the link status of all ports */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_LINK,
		&link_status, sizeof(uint16));

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return link_status;
}

int32
robo_get_portsts(robo_info_t *robo, int32 pid, uint32 *link, uint32 *speed,
	uint32 *duplex)
{
	uint16 val16;
	uint32 val32;

	if (!robo || !link || !speed || !duplex)
		return -1;

	if (!BCM4707_CHIP(CHIPID(robo->sih->chip)))
		return -1;

	if (pid >= MAX_NO_PHYS)
		return -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Read link status */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_LINK,
		&val16, sizeof(uint16));
	*link = (uint32)((val16 >> pid) & 0x1);

	/* Read speed status */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_SPD,
		&val32, sizeof(uint32));
	*speed = (uint32)((val32 >> (pid * 2)) & 0x3);

	/* Read duplex status */
	robo->ops->read_reg(robo, PAGE_STATUS, REG_STATUS_DUP,
		&val16, sizeof(uint16));
	*duplex = (uint32)((val16 >> pid) & 0x1);

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}

int32
robo_permit_fwd_rsv_mcast(robo_info_t *robo)
{
	uint8 val8;

	if (!robo)
		return -1;

	if (!BCM4707_CHIP(CHIPID(robo->sih->chip)))
		return -1;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Permit to forward all reserved multicast */
	val8 = 0;
	robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_RSV_MCAST,
		&val8, sizeof(val8));

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}
#endif /* BCMAGG */
