/*
 *  linux/drivers/mmc/host/mmci.h - ARM PrimeCell MMCI PL180/1 driver
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define MMCIPOWER		0x000
#define MCI_PWR_OFF		0x00
#define MCI_PWR_UP		0x02
#define MCI_PWR_ON		0x03
#define MCI_DATA2DIREN		(1 << 2)
#define MCI_CMDDIREN		(1 << 3)
#define MCI_DATA0DIREN		(1 << 4)
#define MCI_DATA31DIREN		(1 << 5)
#define MCI_OD			(1 << 6)
#define MCI_ROD			(1 << 7)
/* The ST Micro version does not have ROD */
#define MCI_FBCLKEN		(1 << 7)
#define MCI_DATA74DIREN		(1 << 8)

#define MMCICLOCK		0x004
#define MCI_CLK_ENABLE		(1 << 8)
#define MCI_CLK_PWRSAVE		(1 << 9)
#define MCI_CLK_BYPASS		(1 << 10)
#define MCI_4BIT_BUS		(1 << 11)
/* 8bit wide buses supported in ST Micro versions */
#define MCI_ST_8BIT_BUS		(1 << 12)

#define MMCIARGUMENT		0x008
#define MMCICOMMAND		0x00c
#define MCI_CPSM_RESPONSE	(1 << 6)
#define MCI_CPSM_LONGRSP	(1 << 7)
#define MCI_CPSM_INTERRUPT	(1 << 8)
#define MCI_CPSM_PENDING	(1 << 9)
#define MCI_CPSM_ENABLE		(1 << 10)
#define MCI_SDIO_SUSP		(1 << 11)
#define MCI_ENCMD_COMPL		(1 << 12)
#define MCI_NIEN		(1 << 13)
#define MCI_CE_ATACMD		(1 << 14)

#define MMCIRESPCMD		0x010
#define MMCIRESPONSE0		0x014
#define MMCIRESPONSE1		0x018
#define MMCIRESPONSE2		0x01c
#define MMCIRESPONSE3		0x020
#define MMCIDATATIMER		0x024
#define MMCIDATALENGTH		0x028
#define MMCIDATACTRL		0x02c
#define MCI_DPSM_ENABLE		(1 << 0)
#define MCI_DPSM_DIRECTION	(1 << 1)
#define MCI_DPSM_MODE		(1 << 2)
#define MCI_DPSM_DMAENABLE	(1 << 3)
#define MCI_DPSM_BLOCKSIZE	(1 << 4)
#define MCI_DPSM_RWSTART	(1 << 8)
#define MCI_DPSM_RWSTOP		(1 << 9)
#define MCI_DPSM_RWMOD		(1 << 10)
#define MCI_DPSM_SDIOEN		(1 << 11)

#define MMCIDATACNT		0x030
#define MMCISTATUS		0x034
#define MCI_CMDCRCFAIL		(1 << 0)
#define MCI_DATACRCFAIL		(1 << 1)
#define MCI_CMDTIMEOUT		(1 << 2)
#define MCI_DATATIMEOUT		(1 << 3)
#define MCI_TXUNDERRUN		(1 << 4)
#define MCI_RXOVERRUN		(1 << 5)
#define MCI_CMDRESPEND		(1 << 6)
#define MCI_CMDSENT		(1 << 7)
#define MCI_DATAEND		(1 << 8)
#define MCI_DATABLOCKEND	(1 << 10)
#define MCI_CMDACTIVE		(1 << 11)
#define MCI_TXACTIVE		(1 << 12)
#define MCI_RXACTIVE		(1 << 13)
#define MCI_TXFIFOHALFEMPTY	(1 << 14)
#define MCI_RXFIFOHALFFULL	(1 << 15)
#define MCI_TXFIFOFULL		(1 << 16)
#define MCI_RXFIFOFULL		(1 << 17)
#define MCI_TXFIFOEMPTY		(1 << 18)
#define MCI_RXFIFOEMPTY		(1 << 19)
#define MCI_TXDATAAVLBL		(1 << 20)
#define MCI_RXDATAAVLBL		(1 << 21)
#define MCI_SDIOIT		(1 << 22)
#define MCI_CEATAEND		(1 << 23)

#define MMCICLEAR		0x038
#define MCI_CMDCRCFAILCLR	(1 << 0)
#define MCI_DATACRCFAILCLR	(1 << 1)
#define MCI_CMDTIMEOUTCLR	(1 << 2)
#define MCI_DATATIMEOUTCLR	(1 << 3)
#define MCI_TXUNDERRUNCLR	(1 << 4)
#define MCI_RXOVERRUNCLR	(1 << 5)
#define MCI_CMDRESPENDCLR	(1 << 6)
#define MCI_CMDSENTCLR		(1 << 7)
#define MCI_DATAENDCLR		(1 << 8)
#define MCI_DATABLOCKENDCLR	(1 << 10)
#define MCI_SDIOITC		(1 << 22)
#define MCI_CEATAENDC		(1 << 23)

#define MMCIMASK0		0x03c
#define MCI_CMDCRCFAILMASK	(1 << 0)
#define MCI_DATACRCFAILMASK	(1 << 1)
#define MCI_CMDTIMEOUTMASK	(1 << 2)
#define MCI_DATATIMEOUTMASK	(1 << 3)
#define MCI_TXUNDERRUNMASK	(1 << 4)
#define MCI_RXOVERRUNMASK	(1 << 5)
#define MCI_CMDRESPENDMASK	(1 << 6)
#define MCI_CMDSENTMASK		(1 << 7)
#define MCI_DATAENDMASK		(1 << 8)
#define MCI_DATABLOCKENDMASK	(1 << 10)
#define MCI_CMDACTIVEMASK	(1 << 11)
#define MCI_TXACTIVEMASK	(1 << 12)
#define MCI_RXACTIVEMASK	(1 << 13)
#define MCI_TXFIFOHALFEMPTYMASK	(1 << 14)
#define MCI_RXFIFOHALFFULLMASK	(1 << 15)
#define MCI_TXFIFOFULLMASK	(1 << 16)
#define MCI_RXFIFOFULLMASK	(1 << 17)
#define MCI_TXFIFOEMPTYMASK	(1 << 18)
#define MCI_RXFIFOEMPTYMASK	(1 << 19)
#define MCI_TXDATAAVLBLMASK	(1 << 20)
#define MCI_RXDATAAVLBLMASK	(1 << 21)
#define MCI_SDIOITMASK		(1 << 22)
#define MCI_CEATAENDMASK	(1 << 23)

#define MMCIMASK1		0x040
#define MMCIFIFOCNT		0x048
#define MMCIFIFO		0x080 /* to 0x0bc */

#define MCI_IRQENABLE	\
	(MCI_CMDCRCFAILMASK|MCI_DATACRCFAILMASK|MCI_CMDTIMEOUTMASK|	\
	MCI_DATATIMEOUTMASK|MCI_TXUNDERRUNMASK|MCI_RXOVERRUNMASK|	\
	MCI_CMDRESPENDMASK|MCI_CMDSENTMASK|MCI_DATABLOCKENDMASK)

/*
 * The size of the FIFO in bytes.
 */
#define MCI_FIFOSIZE	(16*4)
	
#define MCI_FIFOHALFSIZE (MCI_FIFOSIZE / 2)

#define NR_SG		16

struct clk;
struct variant_data;

struct mmci_host {
	void __iomem		*base;
	struct mmc_request	*mrq;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	struct mmc_host		*mmc;
	struct clk		*clk;
	int			gpio_cd;
	int			gpio_wp;

	unsigned int		data_xfered;

	spinlock_t		lock;

	unsigned int		mclk;
	unsigned int		cclk;
	u32			pwr;
	struct mmci_platform_data *plat;
	struct variant_data	*variant;

	u8			hw_designer;
	u8			hw_revision:4;

	struct timer_list	timer;
	unsigned int		oldstat;

	/* pio stuff */
	struct sg_mapping_iter	sg_miter;
	unsigned int		size;
	struct regulator	*vcc;
};
