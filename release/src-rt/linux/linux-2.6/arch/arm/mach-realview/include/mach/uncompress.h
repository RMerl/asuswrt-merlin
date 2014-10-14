/*
 *  arch/arm/mach-realview/include/mach/uncompress.h
 *
 *  Copyright (C) 2003 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <mach/hardware.h>
#include <asm/mach-types.h>

#include <mach/board-eb.h>
#include <mach/board-pb11mp.h>
#include <mach/board-pb1176.h>
#include <mach/board-pba8.h>
#include <mach/board-pbx.h>

#define AMBA_UART_DR(base)	(*(volatile unsigned char *)((base) + 0x00))
#define AMBA_UART_LCRH(base)	(*(volatile unsigned char *)((base) + 0x2c))
#define AMBA_UART_CR(base)	(*(volatile unsigned char *)((base) + 0x30))
#define AMBA_UART_FR(base)	(*(volatile unsigned char *)((base) + 0x18))

/*
 * Return the UART base address
 */
static inline unsigned long get_uart_base(void)
{
	if (machine_is_realview_eb())
		return REALVIEW_EB_UART0_BASE;
	else if (machine_is_realview_pb11mp())
		return REALVIEW_PB11MP_UART0_BASE;
	else if (machine_is_realview_pb1176())
		return REALVIEW_PB1176_UART0_BASE;
	else if (machine_is_realview_pba8())
		return REALVIEW_PBA8_UART0_BASE;
	else if (machine_is_realview_pbx())
		return REALVIEW_PBX_UART0_BASE;
	else
		return 0;
}

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	unsigned long base = get_uart_base();

	while (AMBA_UART_FR(base) & (1 << 5))
		barrier();

	AMBA_UART_DR(base) = c;
}

static inline void flush(void)
{
	unsigned long base = get_uart_base();

	while (AMBA_UART_FR(base) & (1 << 3))
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
