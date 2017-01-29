/* Copyright (c) 2015 VMware Inc.*/
#include <stdio.h>
#include "internal.h"

int
vmxnet3_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *regs_buff = (u32 *)regs->data;
	u32 version = regs->version;
	int i = 0, j = 0, cnt;

	if (version != 2)
		return -1;

	fprintf(stdout, "Control Registers\n");
	fprintf(stdout, "=================\n");

	fprintf(stdout,
		"    VRRS (Vmxnet3 Revision Report and Selection)    0x%x\n",
		regs_buff[j++]);
	fprintf(stdout,
		"    UVRS (UPT Version Report and Selection)         0x%x\n",
		regs_buff[j++]);
	fprintf(stdout,
		"    DSA  (Driver Shared Address)                    0x%08x%08x\n",
		regs_buff[j+1], regs_buff[j]);
	j += 2;
	fprintf(stdout,
		"    CMD  (Command Register)                         0x%x\n",
		regs_buff[j++]);
	fprintf(stdout,
		"    MAC  (Media Access Control address)             %02x:%02x:%02x:%02x:%02x:%02x\n",
		regs_buff[j] & 0xff,
		(regs_buff[j] >> 8)  & 0xff,
		(regs_buff[j] >> 16) & 0xff,
		(regs_buff[j] >> 24) & 0xff,
		regs_buff[j + 1] & 0xff,
		(regs_buff[j + 1] >> 8)  & 0xff);
	j += 2;
	fprintf(stdout,
		"    ICR  (Interrupt Cause Register)                 0x%x\n",
		regs_buff[j++]);
	fprintf(stdout,
		"    ECR  (Event Cause Register)                     0x%x\n",
		regs_buff[j++]);

	fprintf(stdout, "Datapath Registers\n");
	fprintf(stdout, "==================\n");

	/* Interrupt Mask Registers */
	cnt = regs_buff[j++];
	for (i = 0; i < cnt; i++) {
		fprintf(stdout,
			"    IMR (Interrupt Mask Register) %d                 0x%x\n",
			i, regs_buff[j++]);
	}

	/* Transmit Queue Registers */
	cnt = regs_buff[j++];
	for (i = 0; i < cnt; i++) {
		fprintf(stdout, "    Transmit Queue %d\n", i);
		fprintf(stdout, "    ----------------\n");
		fprintf(stdout,
			"        TXPROD (Transmit Ring Producer Register)    0x%x\n",
			regs_buff[j++]);
		fprintf(stdout,
			"        Transmit Ring\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            Size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2fill                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2comp                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            gen                                     %u\n",
			regs_buff[j++]);

		fprintf(stdout,
			"        Transmit Data Ring\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            Size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            Buffer Size                             %u\n",
			regs_buff[j++]);

		fprintf(stdout,
			"        Transmit Completion Ring\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2proc                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            gen                                     %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"        stopped                                     %u\n",
			regs_buff[j++]);
	}

	/* Receive Queue Registers */
	cnt = regs_buff[j++];
	for (i = 0; i < cnt; i++) {
		fprintf(stdout, "    Receive Queue %d\n", i);
		fprintf(stdout, "    ----------------\n");
		fprintf(stdout,
			"        RXPROD1 (Receive Ring Producer Register) 1  0x%x\n",
			regs_buff[j++]);
		fprintf(stdout,
			"        RXPROD2 (Receive Ring Producer Register) 2  0x%x\n",
			regs_buff[j++]);
		fprintf(stdout,
			"        Receive Ring 0\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            Size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2fill                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2comp                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            gen                                     %u\n",
			regs_buff[j++]);

		fprintf(stdout,
			"        Receive Ring 1\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            Size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2fill                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2comp                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            gen                                     %u\n",
			regs_buff[j++]);

		fprintf(stdout,
			"        Receive Data Ring\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            Size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            Buffer Size                             %u\n",
			regs_buff[j++]);

		fprintf(stdout,
			"        Receive Completion Ring\n");
		fprintf(stdout,
			"            Base Address                            0x%08x%08x\n",
			regs_buff[j+1], regs_buff[j]);
		j += 2;
		fprintf(stdout,
			"            size                                    %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            next2proc                               %u\n",
			regs_buff[j++]);
		fprintf(stdout,
			"            gen                                     %u\n",
			regs_buff[j++]);
	}

	return 0;
}
