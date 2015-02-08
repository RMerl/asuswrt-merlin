/*
 * Douglas Leung, douglas@mips.com
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
 * Defines for the SEAD3 interrupt controller.
 *
 */
#ifndef _MIPS_SEAD3INT_H
#define _MIPS_SEAD3INT_H

/*
 * SEAD3 GIC's address space definitions
 */
#define GIC_BASE_ADDR                   0x1b1c0000
#define GIC_ADDRSPACE_SZ                (128 * 1024)

/* GIC's Nomenclature for Core Interrupt Pins on the SEAD3 */
#define GIC_CPU_INT0		0 /* Core Interrupt 2 	*/
#define GIC_CPU_INT1		1 /* .			*/
#define GIC_CPU_INT2		2 /* .			*/
#define GIC_CPU_INT3		3 /* .			*/
#define GIC_CPU_INT4		4 /* .			*/
#define GIC_CPU_INT5		5 /* Core Interrupt 7   */

/* SEAD3 GIC local interrupts */
#define GIC_INT_TMR             (GIC_CPU_INT5)
#define GIC_INT_PERFCTR         (GIC_CPU_INT5)

/* SEAD3 GIC constants */
/* Add 2 to convert non-eic hw int # to eic vector # */
#define GIC_CPU_TO_VEC_OFFSET   (2)

/* GIC constants */
/* If we map an intr to pin X, GIC will actually generate vector X+1 */
#define GIC_PIN_TO_VEC_OFFSET   (1)

#define GIC_EXT_INTR(x)		x

/* Dummy data */
#define X			0xdead

/* External Interrupts used for IPI */
/* Currently linux don't know about GIC => GIC base must be same as what Linux is using */
#define MIPS_GIC_IRQ_BASE       (MIPS_CPU_IRQ_BASE + 0)

#ifndef __ASSEMBLY__
extern void sead3int_init(void);
#endif

#endif /* !(_MIPS_SEAD3INT_H) */
