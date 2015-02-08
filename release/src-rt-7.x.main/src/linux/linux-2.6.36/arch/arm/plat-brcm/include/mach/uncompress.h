/*
 * These are macros and inlines needed for the kernel decompressor
 * prinarily for debugging messages.
 *
 * The NorthStar chip has two UART channels in ChipcommonA which are
 * byte-spaced and must be byte addressed, while the UART in ChipcommonB
 * is 32-bit word spaced and addressed.
 */


#ifndef    __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

#ifndef	CONFIG_DEBUG_LL		/* Enable debug UART offset calculations */
#define	CONFIG_DEBUG_LL	
#endif

#include <linux/io.h>
#include <mach/io_map.h>

#define	PLAT_LLDEBUG_UART_PA	CONFIG_DEBUG_UART_ADDR

#ifndef	PLAT_LLDEBUG_UART_SH
#error	Internal header error
#endif

#define UART_BASE_ADDR           PLAT_LLDEBUG_UART_PA
#define UART_RBR_THR_DLL_OFFSET 0x00
#define UART_LSR_OFFSET         (0x14 >> 2 << PLAT_LLDEBUG_UART_SH)
#define UART_LSR_THRE_MASK      0x60
#define UART_LSR_TEMT_MASK      0x40

#if PLAT_LLDEBUG_UART_SH == 2
#define	UART_READ_REG	__raw_readl
#define	UART_WRITE_REG	__raw_writel
#elif PLAT_LLDEBUG_UART_SH == 0
#define	UART_READ_REG	__raw_readb
#define	UART_WRITE_REG	__raw_writeb
#else
#error	UART register shift value unsupported
#endif

static inline void putc(int c)
{
	unsigned count = 1 << 20;

	/*
	* data should be written to THR register only 
	* if THRE (LSR bit5) is set)
	*/
	while (0 == (UART_READ_REG( UART_BASE_ADDR + UART_LSR_OFFSET) 
		& UART_LSR_THRE_MASK ))
		{
		if( --count == 0 ) break;
		}

	UART_WRITE_REG(c, UART_BASE_ADDR + UART_RBR_THR_DLL_OFFSET);
}

static inline void flush(void)
{
	unsigned count = 1 << 20;
	/* Wait for the tx fifo to be empty and last char to be sent */
	while (0 == (UART_READ_REG(UART_BASE_ADDR + UART_LSR_OFFSET) 
		& UART_LSR_TEMT_MASK ))
		{
		if( --count == 0 ) break;
		}
}

static inline void arch_decomp_setup(void)
{
}

#define arch_decomp_wdog()

#endif /* __ASM_ARCH_UNCOMPRESS_H */
