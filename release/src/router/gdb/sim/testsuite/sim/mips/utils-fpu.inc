# MIPS simulator testsuite FPU utility functions.
# Copyright (C) 2004, 2007 Free Software Foundation, Inc.
# Contributed by Chris Demetriou of Broadcom Corporation.
#
# This file is part of the GNU simulators.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

	.macro	enable_fpu fr
        mfc0    $20, $12
        or      $20, $20, (1 << 29) | (\fr << 26)
        mtc0    $20, $20
	.endm

	###
	### Data movement macros
	###

	.macro ld_fp_df r, v
	.data
1:	.double	\v
	.previous
	ldc1	\r, 1b
	.endm

	.macro ld_fp_di r, v
	.data
1:	.dword	\v
	.previous
	ldc1	\r, 1b
	.endm

	.macro ld_fp_sf r, v
	.data
1:	.float	\v
	.previous
	lwc1	\r, 1b
	.endm

	.macro ld_fp_si r, v
	.data
1:	.word	\v
	.previous
	lwc1	\r, 1b
	.endm


	###
	### FP condition code manipulation macros
	###

	.macro clrset_fp_cc clr, set
	cfc1	$20, $31	
	or	$20, $20, (((\clr & 0xfe) << 24) | ((\clr & 0x01) << 23))
	xor	$20, $20, (((\clr & 0xfe) << 24) | ((\clr & 0x01) << 23))
	or	$20, $20, (((\set & 0xfe) << 24) | ((\set & 0x01) << 23))
	ctc1	$20, $31
	.endm

	.macro clr_fp_cc clr
	clrset_fp_cc \clr, 0
	.endm

	.macro set_fp_cc set
	clrset_fp_cc 0, \set
	.endm

	.macro get_fp_cc r
	.set push
	.set noat
	cfc1	$1, $31
	srl	$1, $1, 23
	andi	\r, $1, 0x1fc
	andi	$1, $1, 0x1
	srl	\r, \r, 1
	or	\r, \r, $1
	.set pop
	.endm

	.macro ck_fp_cc v
	get_fp_cc $20
	xori	$20, $20, \v
	bnez	$20, _fail
	 nop
	.endm

	.macro ckm_fp_cc v, mask
	get_fp_cc $20
	xori	$20, $20, \v
	andi	$20, $20, \mask
	bnez	$20, _fail
	 nop
	.endm
