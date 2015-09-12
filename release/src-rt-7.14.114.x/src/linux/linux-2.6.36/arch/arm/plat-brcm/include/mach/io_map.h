#ifndef __MACH_IO_MAP_H
#define __MACH_IO_MAP_H	__FILE__

#include <asm/memory.h>
#include <asm/page.h>
#include <mach/vmalloc.h>
#include <plat/plat-bcm5301x.h>

/*
 * This is the address of the UART used for low-level debug,
 * a.k.a. EARLY_PRINTK, and it must be known at compile time,
 * so it can be used in early uncompression and assembly code.
 * It is now defined in platform Kconfig, and verified in
 * SoC-level platform "C" file.
 */
#ifdef	CONFIG_DEBUG_LL
#ifdef	CONFIG_DEBUG_UART_ADDR
#define	PLAT_LLDEBUG_UART_PA	CONFIG_DEBUG_UART_ADDR
#else
#error	Define CONFIG_DEBUG_UART_ADDR in Kconfig to use EARLY_PRINTK
#endif
#endif

/*
 * There are only a few fixed virtual I/O mappings we need.
 * We shall place them after VMALLOC_END, and hope they do
 * not conflict with CONSISTENT_BASE, the start of mapping for
 * DMA memory.
 */
#define	PLAT_FIXMAP_BASE	(VMALLOC_END + (1<<20))
#define	IO_BASE_VA		0xf1000000	/* Temp - K.I.S.S. */
#define	IO_BASE_PA		(0xff000000 & PLAT_UART0_PA)

#define	SOC_CHIPCOMON_A_BASE_VA	(SOC_CHIPCOMON_A_BASE_PA-IO_BASE_PA+IO_BASE_VA)
#define	SOC_DMU_BASE_VA		(SOC_DMU_BASE_PA -IO_BASE_PA+IO_BASE_VA)
#define	SOC_IDM_BASE_VA		(SOC_IDM_BASE_PA -IO_BASE_PA+IO_BASE_VA)

#define	PLAT_UART0_OFF		(PLAT_UART0_PA-IO_BASE_PA)
#define	PLAT_UART1_OFF		(PLAT_UART1_PA-IO_BASE_PA)
#define	PLAT_UART2_OFF		(PLAT_UART2_PA-IO_BASE_PA)

#define	PLAT_UART0_VA		(IO_BASE_VA + PLAT_UART0_OFF)
#define	PLAT_UART1_VA		(IO_BASE_VA + PLAT_UART1_OFF)
#define	PLAT_UART2_VA		(IO_BASE_VA + PLAT_UART2_OFF)

#ifdef	CONFIG_DEBUG_LL
#if	PLAT_LLDEBUG_UART_PA==PLAT_UART0_PA
#define	PLAT_LLDEBUG_UART_VA		PLAT_UART0_VA
#define	PLAT_LLDEBUG_UART_SH		2
#elif	PLAT_LLDEBUG_UART_PA==PLAT_UART1_PA
#define	PLAT_LLDEBUG_UART_VA		PLAT_UART1_VA
#define	PLAT_LLDEBUG_UART_SH		0
#elif	PLAT_LLDEBUG_UART_PA==PLAT_UART2_PA
#define	PLAT_LLDEBUG_UART_VA		PLAT_UART2_VA
#define	PLAT_LLDEBUG_UART_SH		0
#else
#error	Configured debug UART address does not match known devices
#endif
#endif	/* CONFIG_DEBUG_LL */



/* Fixed virtual address for MPCORE registers 
 * (SCU, Gtimer, Ptimer, GIC) : 2 pages 
 */
#define	MPCORE_BASE_VA		(PLAT_FIXMAP_BASE + (0<<20))

/*
 * This is only used in entry-macros.S
 * Since we will map the MPCORE base at the same virtual address
 * on all supported SoCs, then it should be acceptable to use
 * the compile-time defined base address for the interrupt dispatcher
 * assembly code using the above macros.
 */
#define	MPCORE_GIC_CPUIF_VA	(MPCORE_BASE_VA+MPCORE_GIC_CPUIF_OFF)
#define	MPCORE_GIC_CPUIF_VA_CA7	(MPCORE_BASE_VA+MPCORE_GIC_CPUIF_OFF_CA7)

/* PMU base address (VA) for BCM53573 */
#define	SOC_PMU_BASE_VA		(SOC_PMU_BASE_PA - IO_BASE_PA + IO_BASE_VA)
#define PMU_CONTROL_ADDR_OFF	0x660
#define PMU_CONTROL_DATA_OFF	0x664
#define PMU_XTALFREQ_RATIO_OFF	0x66c

#endif /*__MACH_IO_MAP_H */
