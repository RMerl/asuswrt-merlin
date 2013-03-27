# MIPS simulator testsuite MDMX utility functions.
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

	.include "utils-fpu.inc"

	###
	### Shared macros
	###

	# Enable MDMX: enable the FPU w/ FR=1, then set Status.MX
	.macro	enable_mdmx
	enable_fpu 1
	mfc0	$20, $12
	or	$20, $20, (1 << 24)
	mtc0	$20, $12
	.endm


	###
	### .OB-format macros
	###

	.macro ld_ob r, v
	.data
1:	.dword	\v
	.previous
	ldc1	\r, 1b
	.endm

	.macro ck_ob r, v
	.data
1:	.dword	\v
	.previous
	dmfc1	$20, \r
	ld	$21, 1b
	bne	$20, $21, _fail
	 nop
	.endm

	.macro ld_acc_ob h, m, l
	ld_ob	$f20, \m
	ld_ob	$f21, \l
	wacl.ob	$f20, $f21
	ld_ob	$f20, \h
	wach.ob	$f20
	.endm

	.macro ck_acc_ob h, m, l
	rach.ob	$f20
	ck_ob	$f20, \h
	racm.ob	$f20
	ck_ob	$f20, \m
	racl.ob	$f20
	ck_ob	$f20, \l
	.endm
