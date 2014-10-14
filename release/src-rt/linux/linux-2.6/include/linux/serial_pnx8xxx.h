/*
 * Embedded Alley Solutions, source@embeddedalley.com.
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

#ifndef _LINUX_SERIAL_PNX8XXX_H
#define _LINUX_SERIAL_PNX8XXX_H

#include <linux/serial_core.h>
#include <linux/device.h>

#define PNX8XXX_NR_PORTS	2

struct pnx8xxx_port {
	struct uart_port	port;
	struct timer_list	timer;
	unsigned int		old_status;
};

/* register offsets */
#define PNX8XXX_LCR		0
#define PNX8XXX_MCR		0x004
#define PNX8XXX_BAUD		0x008
#define PNX8XXX_CFG		0x00c
#define PNX8XXX_FIFO		0x028
#define PNX8XXX_ISTAT		0xfe0
#define PNX8XXX_IEN		0xfe4
#define PNX8XXX_ICLR		0xfe8
#define PNX8XXX_ISET		0xfec
#define PNX8XXX_PD		0xff4
#define PNX8XXX_MID		0xffc

#define PNX8XXX_UART_LCR_TXBREAK	(1<<30)
#define PNX8XXX_UART_LCR_PAREVN		0x10000000
#define PNX8XXX_UART_LCR_PAREN		0x08000000
#define PNX8XXX_UART_LCR_2STOPB		0x04000000
#define PNX8XXX_UART_LCR_8BIT		0x01000000
#define PNX8XXX_UART_LCR_TX_RST		0x00040000
#define PNX8XXX_UART_LCR_RX_RST		0x00020000
#define PNX8XXX_UART_LCR_RX_NEXT	0x00010000

#define PNX8XXX_UART_MCR_SCR		0xFF000000
#define PNX8XXX_UART_MCR_DCD		0x00800000
#define PNX8XXX_UART_MCR_CTS		0x00100000
#define PNX8XXX_UART_MCR_LOOP		0x00000010
#define PNX8XXX_UART_MCR_RTS		0x00000002
#define PNX8XXX_UART_MCR_DTR		0x00000001

#define PNX8XXX_UART_INT_TX		0x00000080
#define PNX8XXX_UART_INT_EMPTY		0x00000040
#define PNX8XXX_UART_INT_RCVTO		0x00000020
#define PNX8XXX_UART_INT_RX		0x00000010
#define PNX8XXX_UART_INT_RXOVRN		0x00000008
#define PNX8XXX_UART_INT_FRERR		0x00000004
#define PNX8XXX_UART_INT_BREAK		0x00000002
#define PNX8XXX_UART_INT_PARITY		0x00000001
#define PNX8XXX_UART_INT_ALLRX		0x0000003F
#define PNX8XXX_UART_INT_ALLTX		0x000000C0

#define PNX8XXX_UART_FIFO_TXFIFO	0x001F0000
#define PNX8XXX_UART_FIFO_TXFIFO_STA	(0x1f<<16)
#define PNX8XXX_UART_FIFO_RXBRK		0x00008000
#define PNX8XXX_UART_FIFO_RXFE		0x00004000
#define PNX8XXX_UART_FIFO_RXPAR		0x00002000
#define PNX8XXX_UART_FIFO_RXFIFO	0x00001F00
#define PNX8XXX_UART_FIFO_RBRTHR	0x000000FF

#endif
