
/*
 *  include/asm-arm/arch-pnx4008/io.h
 *
 * Author: Dmitry Chigirev <chigirev@ru.mvista.com>
 *
 * 2005 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io(a)			((void __iomem *)(a))
#define __mem_pci(a)		(a)

#endif
