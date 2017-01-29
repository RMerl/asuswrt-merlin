/****************************************************************************
 * Support for the Altera Triple Speed Ethernet 10/100/1000 Controller
 *
 * Copyright (C) 2014 Altera Corporation
 *
 * Author: Vince Bridgers <vbridgers2013@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <stdio.h>
#include <string.h>
#include "internal.h"

#define ALTERA_VERSION_MASK	0xffff
#define ALTERA_ETHTOOL_V1	1

int
bitset(u32 val, int bit)
{
	if (val & (1 << bit))
		return 1;
	return 0;
}

int altera_tse_dump_regs(struct ethtool_drvinfo *info,
			 struct ethtool_regs *regs)
{
	int i;
	u32 *tsereg = (unsigned int *)regs->data;
	u32 version = regs->version;

	if ((version & ALTERA_VERSION_MASK) > ALTERA_ETHTOOL_V1)
		return -1;

	/*
	 * Version 1: Initial TSE driver release. No feature information
	 *	      available, 32-bits of version is equal to 1.
	 *
	 * Version 2: Lower 16-bits of version is 2, upper 16 bits are:
	 *	Bit 16 - SGMDA or MSGDMA Registers
	 *	Bit 17 - PCS Present
	 *	Bit 18 - Supplementary MAC Address Filter Registers Present
	 *	Bit 19 - Multicast Hash Filter Present
	 *	Bit 20 - IEEE 1588 Feature Present
	 */
	fprintf(stdout, "Altera TSE 10/100/1000 Registers, Version %d\n",
		version);
	fprintf(stdout, "---------------------------------------------\n");
	fprintf(stdout, "Revision               0x%08X\n", tsereg[0]);
	fprintf(stdout, "    Core Version       %d.%d\n",
		(tsereg[0] & 0xffff) >> 8,
		tsereg[0] & 0xff);
	fprintf(stdout, "    CustVersion        0x%08X\n", tsereg[0] >> 16);
	fprintf(stdout, "Scratch                0x%08X\n", tsereg[1]);
	fprintf(stdout, "Command/Config         0x%08X\n", tsereg[2]);
	fprintf(stdout, "    (0)TX_EN           %d\n", bitset(tsereg[2], 0));
	fprintf(stdout, "    (1)RX_EN           %d\n", bitset(tsereg[2], 1));
	fprintf(stdout, "    (2)XON_GEN         %d\n", bitset(tsereg[2], 2));
	fprintf(stdout, "    (3)ETH_SPEED       %d\n", bitset(tsereg[2], 3));
	fprintf(stdout, "    (4)PROMIS_EN       %d\n", bitset(tsereg[2], 4));
	fprintf(stdout, "    (5)PAD_EN          %d\n", bitset(tsereg[2], 5));
	fprintf(stdout, "    (6)CRC_FWD         %d\n", bitset(tsereg[2], 6));
	fprintf(stdout, "    (7)PAUSE_FWD       %d\n", bitset(tsereg[2], 7));
	fprintf(stdout, "    (8)PAUSE_IGN       %d\n", bitset(tsereg[2], 8));
	fprintf(stdout, "    (9)TXADDR_INS      %d\n", bitset(tsereg[2], 9));
	fprintf(stdout, "    (10)HD_EN          %d\n", bitset(tsereg[2], 10));
	fprintf(stdout, "    (11)EXCESS_COL     %d\n", bitset(tsereg[2], 11));
	fprintf(stdout, "    (12)LATE_COL       %d\n", bitset(tsereg[2], 12));
	fprintf(stdout, "    (13)SW_RESET       %d\n", bitset(tsereg[2], 13));
	fprintf(stdout, "    (14)MHASH_SEL      %d\n", bitset(tsereg[2], 14));
	fprintf(stdout, "    (15)LOOP_EN        %d\n", bitset(tsereg[2], 15));
	fprintf(stdout, "    (16-18)TX_ADDR_SEL %d\n",
		(tsereg[2] & 0x30000) >> 16);
	fprintf(stdout, "    (19)MAGIC_EN       %d\n", bitset(tsereg[2], 19));
	fprintf(stdout, "    (20)SLEEP          %d\n", bitset(tsereg[2], 20));
	fprintf(stdout, "    (21)WAKEUP         %d\n", bitset(tsereg[2], 21));
	fprintf(stdout, "    (22)XOFF_GEN       %d\n", bitset(tsereg[2], 22));
	fprintf(stdout, "    (23)CTRL_FRAME_EN  %d\n", bitset(tsereg[2], 23));
	fprintf(stdout, "    (24)NO_LEN_CHECK   %d\n", bitset(tsereg[2], 24));
	fprintf(stdout, "    (25)ENA_10         %d\n", bitset(tsereg[2], 25));
	fprintf(stdout, "    (26)RX_ERR_DISC    %d\n", bitset(tsereg[2], 26));
	fprintf(stdout, "    (31)CTRL_RESET     %d\n", bitset(tsereg[2], 31));
	fprintf(stdout, "mac_0                  0x%08X\n", tsereg[3]);
	fprintf(stdout, "mac_1                  0x%08X\n", tsereg[4]);
	fprintf(stdout, "frm_length             0x%08X\n", tsereg[5]);
	fprintf(stdout, "pause_quant            0x%08X\n", tsereg[6]);
	fprintf(stdout, "rx_section_empty       0x%08X\n", tsereg[7]);
	fprintf(stdout, "rx_section_full        0x%08X\n", tsereg[8]);
	fprintf(stdout, "tx_section_empty       0x%08X\n", tsereg[9]);
	fprintf(stdout, "tx_section_full        0x%08X\n", tsereg[0xa]);
	fprintf(stdout, "rx_almost_empty        0x%08X\n", tsereg[0xb]);
	fprintf(stdout, "rx_almost_full         0x%08X\n", tsereg[0xc]);
	fprintf(stdout, "tx_almost_empty        0x%08X\n", tsereg[0xd]);
	fprintf(stdout, "tx_almost_full         0x%08X\n", tsereg[0xe]);
	fprintf(stdout, "mdio_addr0             0x%08X\n", tsereg[0xf]);
	fprintf(stdout, "mdio_addr1             0x%08X\n", tsereg[0x10]);
	fprintf(stdout, "holdoff_quant          0x%08X\n", tsereg[0x11]);

	fprintf(stdout, "tx_ipg_length          0x%08X\n", tsereg[0x17]);
	fprintf(stdout, "Transmit Command       0x%08X\n", tsereg[0x3a]);
	fprintf(stdout, "Receive Command        0x%08X\n", tsereg[0x3b]);

	for (i = 0; i < 64; i++)
		fprintf(stdout, "Multicast Hash[%02d]     0x%08X\n",
			i,
			tsereg[0x40 + i]);
	return 0;
}

