/* linux/include/asm-arm/arch-s3c2410/uncompress.h
 *
 * Copyright (c) 2003 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 - uncompress code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

typedef unsigned int upf_t;	/* cannot include linux/serial_core.h */

/* defines for UART registers */
#include "asm/arch/regs-serial.h"
#include "asm/arch/regs-gpio.h"
#include "asm/arch/regs-watchdog.h"

#include <asm/arch/map.h>

/* working in physical space... */
#undef S3C2410_GPIOREG
#undef S3C2410_WDOGREG

#define S3C2410_GPIOREG(x) ((S3C24XX_PA_GPIO + (x)))
#define S3C2410_WDOGREG(x) ((S3C24XX_PA_WATCHDOG + (x)))

/* how many bytes we allow into the FIFO at a time in FIFO mode */
#define FIFO_MAX	 (14)

#define uart_base S3C24XX_PA_UART + (0x4000*CONFIG_S3C2410_LOWLEVEL_UART_PORT)

static __inline__ void
uart_wr(unsigned int reg, unsigned int val)
{
	volatile unsigned int *ptr;

	ptr = (volatile unsigned int *)(reg + uart_base);
	*ptr = val;
}

static __inline__ unsigned int
uart_rd(unsigned int reg)
{
	volatile unsigned int *ptr;

	ptr = (volatile unsigned int *)(reg + uart_base);
	return *ptr;
}


/* we can deal with the case the UARTs are being run
 * in FIFO mode, so that we don't hold up our execution
 * waiting for tx to happen...
*/

static void putc(int ch)
{
	int cpuid = S3C2410_GSTATUS1_2410;

#ifndef CONFIG_CPU_S3C2400
	cpuid = *((volatile unsigned int *)S3C2410_GSTATUS1);
	cpuid &= S3C2410_GSTATUS1_IDMASK;
#endif

	if (uart_rd(S3C2410_UFCON) & S3C2410_UFCON_FIFOMODE) {
		int level;

		while (1) {
			level = uart_rd(S3C2410_UFSTAT);

			if (cpuid == S3C2410_GSTATUS1_2440 ||
			    cpuid == S3C2410_GSTATUS1_2442) {
				level &= S3C2440_UFSTAT_TXMASK;
				level >>= S3C2440_UFSTAT_TXSHIFT;
			} else {
				level &= S3C2410_UFSTAT_TXMASK;
				level >>= S3C2410_UFSTAT_TXSHIFT;
			}

			if (level < FIFO_MAX)
				break;
		}

	} else {
		/* not using fifos */

		while ((uart_rd(S3C2410_UTRSTAT) & S3C2410_UTRSTAT_TXE) != S3C2410_UTRSTAT_TXE)
			barrier();
	}

	/* write byte to transmission register */
	uart_wr(S3C2410_UTXH, ch);
}

static inline void flush(void)
{
}

#define __raw_writel(d,ad) do { *((volatile unsigned int *)(ad)) = (d); } while(0)

/* CONFIG_S3C2410_BOOT_WATCHDOG
 *
 * Simple boot-time watchdog setup, to reboot the system if there is
 * any problem with the boot process
*/

#ifdef CONFIG_S3C2410_BOOT_WATCHDOG

#define WDOG_COUNT (0xff00)

static inline void arch_decomp_wdog(void)
{
	__raw_writel(WDOG_COUNT, S3C2410_WTCNT);
}

static void arch_decomp_wdog_start(void)
{
	__raw_writel(WDOG_COUNT, S3C2410_WTDAT);
	__raw_writel(WDOG_COUNT, S3C2410_WTCNT);
	__raw_writel(S3C2410_WTCON_ENABLE | S3C2410_WTCON_DIV128 | S3C2410_WTCON_RSTEN | S3C2410_WTCON_PRESCALE(0x80), S3C2410_WTCON);
}

#else
#define arch_decomp_wdog_start()
#define arch_decomp_wdog()
#endif

#ifdef CONFIG_S3C2410_BOOT_ERROR_RESET

static void arch_decomp_error(const char *x)
{
	putstr("\n\n");
	putstr(x);
	putstr("\n\n -- System resetting\n");

	__raw_writel(0x4000, S3C2410_WTDAT);
	__raw_writel(0x4000, S3C2410_WTCNT);
	__raw_writel(S3C2410_WTCON_ENABLE | S3C2410_WTCON_DIV128 | S3C2410_WTCON_RSTEN | S3C2410_WTCON_PRESCALE(0x40), S3C2410_WTCON);

	while(1);
}

#define arch_error arch_decomp_error
#endif

static void error(char *err);

static void
arch_decomp_setup(void)
{
	/* we may need to setup the uart(s) here if we are not running
	 * on an BAST... the BAST will have left the uarts configured
	 * after calling linux.
	 */

	arch_decomp_wdog_start();
}


#endif /* __ASM_ARCH_UNCOMPRESS_H */
