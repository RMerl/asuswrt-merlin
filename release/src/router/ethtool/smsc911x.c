#include <stdio.h>
#include <string.h>
#include "internal.h"

int smsc911x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	unsigned int *smsc_reg = (unsigned int *)regs->data;

	fprintf(stdout, "LAN911x Registers\n");
	fprintf(stdout, "offset 0x50, ID_REV       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x54, INT_CFG      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x58, INT_STS      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x5C, INT_EN       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x60, RESERVED     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x64, BYTE_TEST    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x68, FIFO_INT     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x6C, RX_CFG       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x70, TX_CFG       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x74, HW_CFG       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x78, RX_DP_CTRL   = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x7C, RX_FIFO_INF  = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x80, TX_FIFO_INF  = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x84, PMT_CTRL     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x88, GPIO_CFG     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x8C, GPT_CFG      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x90, GPT_CNT      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x94, FPGA_REV     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x98, ENDIAN       = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0x9C, FREE_RUN     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xA0, RX_DROP      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xA4, MAC_CSR_CMD  = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xA8, MAC_CSR_DATA = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xAC, AFC_CFG      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xB0, E2P_CMD      = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "offset 0xB4, E2P_DATA     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "\n");

	fprintf(stdout, "MAC Registers\n");
	fprintf(stdout, "index 1, MAC_CR   = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 2, ADDRH    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 3, ADDRL    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 4, HASHH    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 5, HASHL    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 6, MII_ACC  = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 7, MII_DATA = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 8, FLOW     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index 9, VLAN1    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index A, VLAN2    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index B, WUFF     = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "index C, WUCSR    = 0x%08X\n",*smsc_reg++);
	fprintf(stdout, "\n");

	fprintf(stdout, "PHY Registers\n");
	fprintf(stdout, "index 0, Basic Control Reg = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 1, Basic Status Reg  = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 2, PHY identifier 1  = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 3, PHY identifier 2  = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 4, Auto Negotiation Advertisement Reg = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 5, Auto Negotiation Link Partner Ability Reg = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 6, Auto Negotiation Expansion Register = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 7, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 8, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 9, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 10, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 11, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 12, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 13, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 14, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 15, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 16, Silicon Revision Reg = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 17, Mode Control/Status Reg = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 18, Special Modes = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 19, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 20, TSTCNTL = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 21, TSTREAD1 = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 22, TSTREAD2 = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 23, TSTWRITE = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 24, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 25, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 26, Reserved = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 27, Control/Status Indication = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 28, Special internal testability = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 29, Interrupt Source Register = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 30, Interrupt Mask Register = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "index 31, PHY Special Control/Status Register = 0x%04X\n",*smsc_reg++);
	fprintf(stdout, "\n");

	return 0;
}

