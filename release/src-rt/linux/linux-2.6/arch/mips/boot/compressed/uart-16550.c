/*
 * 16550 compatible uart based serial debug support for zboot
 */

#include <linux/types.h>
#include <linux/serial_reg.h>
#include <linux/init.h>

#include <asm/addrspace.h>

#if defined(CONFIG_MACH_LOONGSON) || defined(CONFIG_MIPS_MALTA)
#define UART_BASE 0x1fd003f8
#define PORT(offset) (CKSEG1ADDR(UART_BASE) + (offset))
#endif

#ifdef CONFIG_AR7
#include <ar7.h>
#define PORT(offset) (CKSEG1ADDR(AR7_REGS_UART0) + (4 * offset))
#endif

#ifndef PORT
#error please define the serial port address for your own machine
#endif

static inline unsigned int serial_in(int offset)
{
	return *((char *)PORT(offset));
}

static inline void serial_out(int offset, int value)
{
	*((char *)PORT(offset)) = value;
}

void putc(char c)
{
	int timeout = 1024;

	while (((serial_in(UART_LSR) & UART_LSR_THRE) == 0) && (timeout-- > 0))
		;

	serial_out(UART_TX, c);
}
