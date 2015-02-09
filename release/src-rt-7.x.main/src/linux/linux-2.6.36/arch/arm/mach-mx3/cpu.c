/*
 * MX3 CPU type detection
 *
 * Copyright (c) 2009 Daniel Mack <daniel@caiaq.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/iim.h>

unsigned int mx31_cpu_rev;
EXPORT_SYMBOL(mx31_cpu_rev);

struct mx3_cpu_type {
	u8 srev;
	const char *name;
	const char *v;
	unsigned int rev;
};

static struct mx3_cpu_type mx31_cpu_type[] __initdata = {
	{ .srev = 0x00, .name = "i.MX31(L)", .v = "1.0",  .rev = CHIP_REV_1_0	},
	{ .srev = 0x10, .name = "i.MX31",    .v = "1.1",  .rev = CHIP_REV_1_1	},
	{ .srev = 0x11, .name = "i.MX31L",   .v = "1.1",  .rev = CHIP_REV_1_1	},
	{ .srev = 0x12, .name = "i.MX31",    .v = "1.15", .rev = CHIP_REV_1_1	},
	{ .srev = 0x13, .name = "i.MX31L",   .v = "1.15", .rev = CHIP_REV_1_1	},
	{ .srev = 0x14, .name = "i.MX31",    .v = "1.2",  .rev = CHIP_REV_1_2	},
	{ .srev = 0x15, .name = "i.MX31L",   .v = "1.2",  .rev = CHIP_REV_1_2	},
	{ .srev = 0x28, .name = "i.MX31",    .v = "2.0",  .rev = CHIP_REV_2_0	},
	{ .srev = 0x29, .name = "i.MX31L",   .v = "2.0",  .rev = CHIP_REV_2_0	},
};

void __init mx31_read_cpu_rev(void)
{
	u32 i, srev;

	/* read SREV register from IIM module */
	srev = __raw_readl(IO_ADDRESS(IIM_BASE_ADDR + MXC_IIMSREV));

	for (i = 0; i < ARRAY_SIZE(mx31_cpu_type); i++)
		if (srev == mx31_cpu_type[i].srev) {
			printk(KERN_INFO
				"CPU identified as %s, silicon rev %s\n",
				mx31_cpu_type[i].name, mx31_cpu_type[i].v);

			mx31_cpu_rev = mx31_cpu_type[i].rev;
			return;
		}

	printk(KERN_WARNING "Unknown CPU identifier. srev = %02x\n", srev);
}
