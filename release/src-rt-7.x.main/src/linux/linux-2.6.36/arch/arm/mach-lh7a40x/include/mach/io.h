/* arch/arm/mach-lh7a40x/include/mach/io.h
 *
 *  Copyright (C) 2004 Coastal Environmental Systems
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */

#ifndef __ASM_ARCH_IO_H
#define __ASM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

/* No ISA or PCI bus on this machine. */
#define __io(a)		__typesafe_io(a)
#define __mem_pci(a)	(a)

#endif /* __ASM_ARCH_IO_H */
