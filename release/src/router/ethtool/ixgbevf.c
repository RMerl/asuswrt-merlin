/* Copyright (c) 2013 Intel Corporation */
#include <stdio.h>
#include "internal.h"

int
ixgbevf_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u8 version = (u8)(regs->version >> 24);
	u8 i;

	if (version == 0)
		return -1;

	fprintf(stdout,
		"0x00000: VFCTRL      (VF Control Register) (Write Only) N/A\n");

	fprintf(stdout,
		"0x00008: VFSTATUS    (VF Status Register)               0x%08X\n",
		regs_buff[1]);

	fprintf(stdout,
		"0x00010: VFLINKS     (VF Link Status Register)          0x%08X\n",
		regs_buff[2]);

	fprintf(stdout,
		"0x03190: VFRXMEMWRAP (Rx Packet Buffer Flush Detect)    0x%08X\n",
		regs_buff[3]);

	fprintf(stdout,
		"0x00048: VFFRTIMER   (VF Free Running Timer)            0x%08X\n",
		regs_buff[4]);

	fprintf(stdout,
		"0x00100: VFEICR      (VF Extended Interrupt Cause)      0x%08X\n",
		regs_buff[5]);

	fprintf(stdout,
		"0x00104: VFEICS      (VF Extended Interrupt Cause Set)  0x%08X\n",
		regs_buff[6]);

	fprintf(stdout,
		"0x00108: VFEIMS      (VF Extended Interrupt Mask Set)   0x%08X\n",
		regs_buff[7]);

	fprintf(stdout,
		"0x0010C: VFEIMC      (VF Extended Interrupt Mask Clear) 0x%08X\n",
		regs_buff[8]);

	fprintf(stdout,
		"0x00110: VFEIAC      (VF Extended Interrupt Auto Clear) 0x%08X\n",
		regs_buff[9]);

	fprintf(stdout,
		"0x00114: VFEIAM      (VF Extended Interrupt Auto Mask)  0x%08X\n",
		regs_buff[10]);

	fprintf(stdout,
		"0x00820: VFEITR(0)   (VF Extended Interrupt Throttle)   0x%08X\n",
		regs_buff[11]);

	fprintf(stdout,
		"0x00120: VFIVAR(0)   (VF Interrupt Vector Allocation)   0x%08X\n",
		regs_buff[12]);

	fprintf(stdout,
		"0x00140: VFIVAR_MISC (VF Interrupt Vector Misc)         0x%08X\n",
		regs_buff[13]);

	fprintf(stdout,
		"0x00104: VFPSRTYPE   (VF Replication Packet Split Type) 0x%08X\n",
		regs_buff[28]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDBAL(%d)  (VF Rx Desc. Base Addr Low %d)      0x%08X\n",
			0x1000 + 0x40*i,
			i, i,
			regs_buff[14+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDBAH(%d)  (VF Rx Desc. Base Addr High %d)     0x%08X\n",
			0x1004 + 0x40*i,
			i, i,
			regs_buff[16+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDLEN(%d)  (VF Rx Desc. Length %d)             0x%08X\n",
			0x1008 + 0x40*i,
			i, i,
			regs_buff[18+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDH(%d)    (VF Rx Desc. Head %d)               0x%08X\n",
			0x1010 + 0x40*i,
			i, i,
			regs_buff[20+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDT(%d)    (VF Rx Desc. Tail %d)               0x%08X\n",
			0x1018 + 0x40*i,
			i, i,
			regs_buff[22+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFRDT(%d)    (VF Rx Desc. Control %d),           0x%08X\n",
			0x1028 + 0x40*i,
			i, i,
			regs_buff[24+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFSRRCTL(%d) (VF Split Rx Control %d)            0x%08X\n",
			0x1014 + 0x40*i,
			i, i,
			regs_buff[26+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDBAL(%d)  (VF Tx Desc. Base Addr Low %d)      0x%08X\n",
			0x2000 + 0x40*i,
			i, i,
			regs_buff[29+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDBAH(%d)  (VF Tx Desc. Base Addr High %d)     0x%08X\n",
			0x2004 + 0x40*i,
			i, i,
			regs_buff[31+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDLEN(%d)  (VF Tx Desc. Length %d)             0x%08X\n",
			0x2008 + 0x40*i,
			i, i,
			regs_buff[33+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDH(%d)    (VF Tx Desc. Head %d)               0x%08X\n",
			0x2010 + 0x40*i,
			i, i,
			regs_buff[35+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDT(%d)    (VF Tx Desc. Tail %d)               0x%08X\n",
			0x2018 + 0x40*i,
			i, i,
			regs_buff[37+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDT(%d)    (VF Tx Desc. Control %d)            0x%08X\n",
			0x2028 + 0x40*i,
			i, i,
			regs_buff[39+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDWBAL(%d) (VF Tx Desc. Write Back Addr Lo %d) 0x%08X\n",
			0x2038 + 0x40*i,
			i, i,
			regs_buff[41+i]);

	for (i = 0; i < 2; i++)
		fprintf(stdout,
			"0x%05x: VFTDWBAH(%d) (VF Tx Desc. Write Back Addr Hi %d) 0x%08X\n",
			0x203C + 0x40*i,
			i, i,
			regs_buff[43+i]);

	return 0;
}
