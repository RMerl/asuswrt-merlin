/* Copyright 2006 Fabric7 Systems, Inc */

#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

struct regs_line {
		u32	addr;
		u32	data;
};

#define VIOC_REGS_LINE_SIZE	sizeof(struct regs_line)

int vioc_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	unsigned int	i;
	unsigned int	num_regs;
	struct regs_line *reg_info = (struct regs_line *) regs->data;

	printf("ethtool_regs\n"
		"%-20s = %04x\n"
		"%-20s = %04x\n",
		"cmd", regs->cmd,
		"version", regs->version);

	num_regs = regs->len/VIOC_REGS_LINE_SIZE;

	for (i = 0; i < num_regs; i++){
		printf("%08x = %08x\n", reg_info[i].addr, reg_info[i].data);
	}

	return 0;
}
