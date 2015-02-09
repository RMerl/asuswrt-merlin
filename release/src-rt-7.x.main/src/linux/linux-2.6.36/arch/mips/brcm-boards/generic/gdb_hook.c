/*
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.      
 *       
 * Permission to use, copy, modify, and/or distribute this software for any      
 * purpose with or without fee is hereby granted, provided that the above      
 * copyright notice and this permission notice appear in all copies.      
 *       
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES      
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF      
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY      
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES      
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION      
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN      
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.      
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * This is the interface to the remote debugger stub.
 *
 */

#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#include <asm/serial.h>
#include <asm/io.h>

static struct async_struct kdb_port_info = {0};

static __inline__ unsigned int serial_in(struct async_struct *info, int offset)
{
	return readb(info->iomem_base + (offset<<info->iomem_reg_shift));
}

static __inline__ void serial_out(struct async_struct *info, int offset,
				  int value)
{
	writeb(value, info->iomem_base + (offset<<info->iomem_reg_shift));
}

void rs_kgdb_hook(struct uart_port *ser) {
	int t;

	kdb_port_info.state = (struct serial_state *)ser;
	kdb_port_info.magic = SERIAL_MAGIC;
	kdb_port_info.port = ser->line;
	kdb_port_info.flags = ser->flags;
	kdb_port_info.iomem_base = ser->membase;
	kdb_port_info.iomem_reg_shift = ser->regshift;
	kdb_port_info.MCR = UART_MCR_DTR | UART_MCR_RTS;

	/*
	 * Clear all interrupts
	 */
	serial_in(&kdb_port_info, UART_LSR);
	serial_in(&kdb_port_info, UART_RX);
	serial_in(&kdb_port_info, UART_IIR);
	serial_in(&kdb_port_info, UART_MSR);

	/*
	 * Now, initialize the UART 
	 */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8);	/* reset DLAB */
	serial_out(&kdb_port_info, UART_MCR, kdb_port_info.MCR);
	
	/*
	 * and set the speed of the serial port
	 * (currently hardwired to 115200 8N1
	 */

	/* baud rate is fixed to 115200 (is this sufficient?)*/
	t = ser->uartclk / 115200;	
	/* set DLAB */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8 | UART_LCR_DLAB);
	serial_out(&kdb_port_info, UART_DLL, t & 0xff);/* LS of divisor */
	serial_out(&kdb_port_info, UART_DLM, t >> 8);  /* MS of divisor */
	/* reset DLAB */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8);
}

int putDebugChar(char c)
{

	if (!kdb_port_info.state) { 	/* need to init device first */
		return 0;
	}

	while ((serial_in(&kdb_port_info, UART_LSR) & UART_LSR_THRE) == 0)
		;

	serial_out(&kdb_port_info, UART_TX, c);

	return 1;
}

char getDebugChar(void) 
{
	if (!kdb_port_info.state) { 	/* need to init device first */
		return 0;
	}

	while (!(serial_in(&kdb_port_info, UART_LSR) & 1))
		;

	return(serial_in(&kdb_port_info, UART_RX));
}
