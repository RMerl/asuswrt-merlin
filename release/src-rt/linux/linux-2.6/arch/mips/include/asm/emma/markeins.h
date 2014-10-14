/*
 *  Copyright (C) NEC Electronics Corporation 2005-2006
 *
 *  This file based on include/asm-mips/ddb5xxx/ddb5xxx.h
 *          Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MARKEINS_H
#define MARKEINS_H

#define NUM_EMMA2RH_IRQ_SW	32
#define NUM_EMMA2RH_IRQ_GPIO	32

#define EMMA2RH_SW_CASCADE	(EMMA2RH_IRQ_INT(7) - EMMA2RH_IRQ_INT(0))
#define EMMA2RH_GPIO_CASCADE	(EMMA2RH_IRQ_INT(46) - EMMA2RH_IRQ_INT(0))

#define EMMA2RH_SW_IRQ_BASE	(EMMA2RH_IRQ_BASE + NUM_EMMA2RH_IRQ)
#define EMMA2RH_GPIO_IRQ_BASE	(EMMA2RH_SW_IRQ_BASE + NUM_EMMA2RH_IRQ_SW)

#define EMMA2RH_SW_IRQ_INT(n)	(EMMA2RH_SW_IRQ_BASE + (n))

#define MARKEINS_PCI_IRQ_INTA	EMMA2RH_GPIO_IRQ_BASE+15
#define MARKEINS_PCI_IRQ_INTB	EMMA2RH_GPIO_IRQ_BASE+16
#define MARKEINS_PCI_IRQ_INTC	EMMA2RH_GPIO_IRQ_BASE+17
#define MARKEINS_PCI_IRQ_INTD	EMMA2RH_GPIO_IRQ_BASE+18

#endif /* CONFIG_MARKEINS */
