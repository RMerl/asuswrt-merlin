/*
 *  linux/arch/arm/mach-pxa/generic.h
 *
 * Author:	Nicolas Pitre
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct irq_data;
struct sys_timer;

extern struct sys_timer pxa_timer;
extern void __init pxa_init_irq(int irq_nr,
				int (*set_wake)(struct irq_data *,
						unsigned int));
extern void __init pxa25x_init_irq(void);
#ifdef CONFIG_CPU_PXA26x
extern void __init pxa26x_init_irq(void);
#endif
extern void __init pxa27x_init_irq(void);
extern void __init pxa3xx_init_irq(void);
extern void __init pxa95x_init_irq(void);

extern void __init pxa_map_io(void);
extern void __init pxa25x_map_io(void);
extern void __init pxa27x_map_io(void);
extern void __init pxa3xx_map_io(void);

extern unsigned int get_clk_frequency_khz(int info);

#define SET_BANK(__nr,__start,__size) \
	mi->bank[__nr].start = (__start), \
	mi->bank[__nr].size = (__size)

#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

#ifdef CONFIG_PXA25x
extern unsigned pxa25x_get_clk_frequency_khz(int);
#else
#define pxa25x_get_clk_frequency_khz(x)		(0)
#endif

#ifdef CONFIG_PXA27x
extern unsigned pxa27x_get_clk_frequency_khz(int);
#else
#define pxa27x_get_clk_frequency_khz(x)		(0)
#endif

#if defined(CONFIG_PXA25x) || defined(CONFIG_PXA27x)
extern void pxa2xx_clear_reset_status(unsigned int);
#else
static inline void pxa2xx_clear_reset_status(unsigned int mask) {}
#endif

#ifdef CONFIG_PXA3xx
extern unsigned pxa3xx_get_clk_frequency_khz(int);
#else
#define pxa3xx_get_clk_frequency_khz(x)		(0)
#endif

extern struct sysdev_class pxa_irq_sysclass;
extern struct sysdev_class pxa_gpio_sysclass;
extern struct sysdev_class pxa2xx_mfp_sysclass;
extern struct sysdev_class pxa3xx_mfp_sysclass;

void __init pxa_set_ffuart_info(void *info);
void __init pxa_set_btuart_info(void *info);
void __init pxa_set_stuart_info(void *info);
void __init pxa_set_hwuart_info(void *info);
