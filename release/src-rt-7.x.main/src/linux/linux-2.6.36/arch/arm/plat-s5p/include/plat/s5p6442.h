/* arch/arm/plat-s5p/include/plat/s5p6442.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for s5p6442 cpu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* Common init code for S5P6442 related SoCs */

extern void s5p6442_common_init_uarts(struct s3c2410_uartcfg *cfg, int no);
extern void s5p6442_register_clocks(void);
extern void s5p6442_setup_clocks(void);

#ifdef CONFIG_CPU_S5P6442

extern  int s5p6442_init(void);
extern void s5p6442_init_irq(void);
extern void s5p6442_map_io(void);
extern void s5p6442_init_clocks(int xtal);

#define s5p6442_init_uarts s5p6442_common_init_uarts

#else
#define s5p6442_init_clocks NULL
#define s5p6442_init_uarts NULL
#define s5p6442_map_io NULL
#define s5p6442_init NULL
#endif
