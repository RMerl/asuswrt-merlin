/*
 * include/mach//at572d940hf_matrix.h
 *
 * Antonio R. Costa <costa.antonior@gmail.com>
 * Copyright (C) 2008 Atmel
 *
 * Copyright (C) 2005 SAN People
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

#ifndef AT572D940HF_MATRIX_H
#define AT572D940HF_MATRIX_H

#define AT91_MATRIX_MCFG0	(AT91_MATRIX + 0x00)	/* Master Configuration Register 0 */
#define AT91_MATRIX_MCFG1	(AT91_MATRIX + 0x04)	/* Master Configuration Register 1 */
#define AT91_MATRIX_MCFG2	(AT91_MATRIX + 0x08)	/* Master Configuration Register 2 */
#define AT91_MATRIX_MCFG3	(AT91_MATRIX + 0x0C)	/* Master Configuration Register 3 */
#define AT91_MATRIX_MCFG4	(AT91_MATRIX + 0x10)	/* Master Configuration Register 4 */
#define AT91_MATRIX_MCFG5	(AT91_MATRIX + 0x14)	/* Master Configuration Register 5 */

#define		AT91_MATRIX_ULBT	(7 << 0)	/* Undefined Length Burst Type */
#define			AT91_MATRIX_ULBT_INFINITE	(0 << 0)
#define			AT91_MATRIX_ULBT_SINGLE		(1 << 0)
#define			AT91_MATRIX_ULBT_FOUR		(2 << 0)
#define			AT91_MATRIX_ULBT_EIGHT		(3 << 0)
#define			AT91_MATRIX_ULBT_SIXTEEN	(4 << 0)

#define AT91_MATRIX_SCFG0	(AT91_MATRIX + 0x40)	/* Slave Configuration Register 0 */
#define AT91_MATRIX_SCFG1	(AT91_MATRIX + 0x44)	/* Slave Configuration Register 1 */
#define AT91_MATRIX_SCFG2	(AT91_MATRIX + 0x48)	/* Slave Configuration Register 2 */
#define AT91_MATRIX_SCFG3	(AT91_MATRIX + 0x4C)	/* Slave Configuration Register 3 */
#define AT91_MATRIX_SCFG4	(AT91_MATRIX + 0x50)	/* Slave Configuration Register 4 */
#define		AT91_MATRIX_SLOT_CYCLE		(0xff << 0)	/* Maximum Number of Allowed Cycles for a Burst */
#define		AT91_MATRIX_DEFMSTR_TYPE	(3    << 16)	/* Default Master Type */
#define			AT91_MATRIX_DEFMSTR_TYPE_NONE	(0 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_LAST	(1 << 16)
#define			AT91_MATRIX_DEFMSTR_TYPE_FIXED	(2 << 16)
#define		AT91_MATRIX_FIXED_DEFMSTR	(0x7  << 18)	/* Fixed Index of Default Master */
#define		AT91_MATRIX_ARBT		(3    << 24)	/* Arbitration Type */
#define			AT91_MATRIX_ARBT_ROUND_ROBIN	(0 << 24)
#define			AT91_MATRIX_ARBT_FIXED_PRIORITY	(1 << 24)

#define AT91_MATRIX_PRAS0	(AT91_MATRIX + 0x80)	/* Priority Register A for Slave 0 */
#define AT91_MATRIX_PRAS1	(AT91_MATRIX + 0x88)	/* Priority Register A for Slave 1 */
#define AT91_MATRIX_PRAS2	(AT91_MATRIX + 0x90)	/* Priority Register A for Slave 2 */
#define AT91_MATRIX_PRAS3	(AT91_MATRIX + 0x98)	/* Priority Register A for Slave 3 */
#define AT91_MATRIX_PRAS4	(AT91_MATRIX + 0xA0)	/* Priority Register A for Slave 4 */

#define		AT91_MATRIX_M0PR		(3 << 0)	/* Master 0 Priority */
#define		AT91_MATRIX_M1PR		(3 << 4)	/* Master 1 Priority */
#define		AT91_MATRIX_M2PR		(3 << 8)	/* Master 2 Priority */
#define		AT91_MATRIX_M3PR		(3 << 12)	/* Master 3 Priority */
#define		AT91_MATRIX_M4PR		(3 << 16)	/* Master 4 Priority */
#define		AT91_MATRIX_M5PR		(3 << 20)	/* Master 5 Priority */
#define		AT91_MATRIX_M6PR		(3 << 24)	/* Master 6 Priority */

#define AT91_MATRIX_MRCR	(AT91_MATRIX + 0x100)	/* Master Remap Control Register */
#define		AT91_MATRIX_RCB0		(1 << 0)	/* Remap Command for AHB Master 0 (ARM926EJ-S Instruction Master) */
#define		AT91_MATRIX_RCB1		(1 << 1)	/* Remap Command for AHB Master 1 (ARM926EJ-S Data Master) */

#define AT91_MATRIX_SFR0	(AT91_MATRIX + 0x110)	/* Special Function Register 0 */
#define AT91_MATRIX_SFR1	(AT91_MATRIX + 0x114)	/* Special Function Register 1 */
#define AT91_MATRIX_SFR2	(AT91_MATRIX + 0x118)	/* Special Function Register 2 */
#define AT91_MATRIX_SFR3	(AT91_MATRIX + 0x11C)	/* Special Function Register 3 */
#define AT91_MATRIX_SFR4	(AT91_MATRIX + 0x120)	/* Special Function Register 4 */
#define AT91_MATRIX_SFR5	(AT91_MATRIX + 0x124)	/* Special Function Register 5 */
#define AT91_MATRIX_SFR6	(AT91_MATRIX + 0x128)	/* Special Function Register 6 */
#define AT91_MATRIX_SFR7	(AT91_MATRIX + 0x12C)	/* Special Function Register 7 */
#define AT91_MATRIX_SFR8	(AT91_MATRIX + 0x130)	/* Special Function Register 8 */
#define AT91_MATRIX_SFR9	(AT91_MATRIX + 0x134)	/* Special Function Register 9 */
#define AT91_MATRIX_SFR10	(AT91_MATRIX + 0x138)	/* Special Function Register 10 */
#define AT91_MATRIX_SFR11	(AT91_MATRIX + 0x13C)	/* Special Function Register 11 */
#define AT91_MATRIX_SFR12	(AT91_MATRIX + 0x140)	/* Special Function Register 12 */
#define AT91_MATRIX_SFR13	(AT91_MATRIX + 0x144)	/* Special Function Register 13 */
#define AT91_MATRIX_SFR14	(AT91_MATRIX + 0x148)	/* Special Function Register 14 */
#define AT91_MATRIX_SFR15	(AT91_MATRIX + 0x14C)	/* Special Function Register 15 */


/*
 * The following registers / bits are not defined in the Datasheet (Revision A)
 */

#define AT91_MATRIX_TCR		(AT91_MATRIX + 0x100)	/* TCM Configuration Register */
#define		AT91_MATRIX_ITCM_SIZE		(0xf << 0)	/* Size of ITCM enabled memory block */
#define			AT91_MATRIX_ITCM_0		(0 << 0)
#define			AT91_MATRIX_ITCM_16		(5 << 0)
#define			AT91_MATRIX_ITCM_32		(6 << 0)
#define			AT91_MATRIX_ITCM_64		(7 << 0)
#define		AT91_MATRIX_DTCM_SIZE		(0xf << 4)	/* Size of DTCM enabled memory block */
#define			AT91_MATRIX_DTCM_0		(0 << 4)
#define			AT91_MATRIX_DTCM_16		(5 << 4)
#define			AT91_MATRIX_DTCM_32		(6 << 4)
#define			AT91_MATRIX_DTCM_64		(7 << 4)

#define AT91_MATRIX_EBICSA	(AT91_MATRIX + 0x11C)	/* EBI Chip Select Assignment Register */
#define		AT91_MATRIX_CS1A		(1 << 1)	/* Chip Select 1 Assignment */
#define			AT91_MATRIX_CS1A_SMC		(0 << 1)
#define			AT91_MATRIX_CS1A_SDRAMC		(1 << 1)
#define		AT91_MATRIX_CS3A		(1 << 3)	/* Chip Select 3 Assignment */
#define			AT91_MATRIX_CS3A_SMC		(0 << 3)
#define			AT91_MATRIX_CS3A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91_MATRIX_CS4A		(1 << 4)	/* Chip Select 4 Assignment */
#define			AT91_MATRIX_CS4A_SMC		(0 << 4)
#define			AT91_MATRIX_CS4A_SMC_CF1	(1 << 4)
#define		AT91_MATRIX_CS5A		(1 << 5)	/* Chip Select 5 Assignment */
#define			AT91_MATRIX_CS5A_SMC		(0 << 5)
#define			AT91_MATRIX_CS5A_SMC_CF2	(1 << 5)
#define		AT91_MATRIX_DBPUC		(1 << 8)	/* Data Bus Pull-up Configuration */

#endif
