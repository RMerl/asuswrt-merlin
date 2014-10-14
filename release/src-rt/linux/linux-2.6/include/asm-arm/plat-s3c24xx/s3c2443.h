/* linux/include/asm-arm/plat-s3c24xx/s3c2443.h
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Header file for s3c2443 cpu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifdef CONFIG_CPU_S3C2443

struct s3c2410_uartcfg;

extern  int s3c2443_init(void);

extern void s3c2443_map_io(struct map_desc *mach_desc, int size);

extern void s3c2443_init_uarts(struct s3c2410_uartcfg *cfg, int no);

extern void s3c2443_init_clocks(int xtal);

extern  int s3c2443_baseclk_add(void);

#else
#define s3c2443_init_clocks NULL
#define s3c2443_init_uarts NULL
#define s3c2443_map_io NULL
#define s3c2443_init NULL
#endif
