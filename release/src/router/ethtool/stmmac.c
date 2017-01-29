/****************************************************************************
 * Support for the Synopsys MAC 10/100/1000 on-chip Ethernet controllers
 *
 * Copyright (C) 2007-2009  STMicroelectronics Ltd
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <stdio.h>
#include <string.h>
#include "internal.h"

int st_mac100_dump_regs(struct ethtool_drvinfo *info,
			struct ethtool_regs *regs)
{
	int i;
	unsigned int *stmmac_reg = (unsigned int *)regs->data;

	fprintf(stdout, "ST MAC 10/100 Registers\n");
	fprintf(stdout, "control reg  0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "addr HI 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "addr LO 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "multicast hash HI 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "multicast hash LO 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "MII addr 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "MII data %08X\n", *stmmac_reg++);
	fprintf(stdout, "flow control 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "VLAN1 tag 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "VLAN2 tag 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "mac wakeup frame 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "mac wakeup crtl 0x%08X\n", *stmmac_reg++);

	fprintf(stdout, "\n");
	fprintf(stdout, "DMA Registers\n");
	for (i = 0; i < 9; i++)
		fprintf(stdout, "CSR%d  0x%08X\n", i, *stmmac_reg++);

	fprintf(stdout, "DMA cur tx buf addr 0x%08X\n", *stmmac_reg++);
	fprintf(stdout, "DMA cur rx buf addr 0x%08X\n", *stmmac_reg++);

	fprintf(stdout, "\n");

	return 0;
}

int st_gmac_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	int i;
	unsigned int *stmmac_reg = (unsigned int *)regs->data;

	fprintf(stdout, "ST GMAC Registers\n");
	fprintf(stdout, "GMAC Registers\n");
	for (i = 0; i < 55; i++)
		fprintf(stdout, "Reg%d  0x%08X\n", i, *stmmac_reg++);

	fprintf(stdout, "\n");
	fprintf(stdout, "DMA Registers\n");
	for (i = 0; i < 22; i++)
		fprintf(stdout, "Reg%d  0x%08X\n", i, *stmmac_reg++);

	return 0;
}
