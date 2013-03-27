# MIPS DSP ASE simulator testsuite utility functions.
# Copyright (C) 2005, 2007 Free Software Foundation, Inc.
# Contributed by MIPS Technologies, Inc.  Written by Chao-ying Fu.
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

# $4, $5, $6, $7, $ac0, $ac1, $ac2, $ac3 are used as temps by the macros
# defined here.

	# If a != b, jump to _fail.
	# Otherwise, fall through.
	.macro dsp_assert a, b
	beq	\a, \b, 1f
	nop
	j	_fail
	nop
1: 
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, s, t)
	# Check if crout == dsp control register
	.macro dspck_dstio inst, d, s, t, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \s
	li	$5, \t
	\inst	$6, $4, $5
	li	$7, \d
	dsp_assert	$6, $7
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# (inst s, t)
	# Check if crout == dsp control register
	.macro dspck_stio inst, s, t, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \s
	li	$5, \t
	\inst	$4, $5
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, s, t)
	.macro dspck_dsti inst, d, s, t, crin
	li	$4, \crin
	wrdsp	$4
	li	$4, \s
	li	$5, \t
	\inst	$6, $4, $5
	li	$7, \d
	dsp_assert	$6, $7
	.endm

	# Set dsp control register <= crin
	# Check if tou == (inst tin, s)
	.macro dspck_tsi inst, tou, tin, s, crin
	li	$4, \crin
	wrdsp	$4
	li	$4, \s
	li	$5, \tin
	\inst	$5, $4
	li	$6, \tou
	dsp_assert	$5, $6
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, imm)
	# Check if crout == dsp control register
	.macro dspck_dIio inst, d, imm, crin, crout
	li	$4, \crin
	wrdsp	$4
	\inst	$5, \imm
	li	$6, \d
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, s)
	# Check if crout == dsp control register
	.macro dspck_dsio inst, d, s, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \s
	\inst	$6, $4
	li	$7, \d
	dsp_assert	$6, $7
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, t, sa)
	# Check if crout == dsp control register
	.macro dspck_dtsaio inst, d, t, sa, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \t
	\inst	$6, $4, \sa
	li	$7, \d
	dsp_assert	$6, $7
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Check if d == (inst ?, t, sa)
	.macro dspck_dtsai inst, d, t, sa, crin
	li	$4, \crin
	wrdsp	$4
	li	$4, \t
	\inst	$6, $4, \sa
	li	$7, \d
	dsp_assert	$6, $7
	.endm

	# Set dsp control register <= crin
	# Set $ac3 <= {hiin, loin}
	# (inst $ac3, s, t)
	# Check if {hiou, loou} == $ac3
	# Check if (crout & 0x80000) == (dsp control register & 0x80000)
	.macro dspck_astio inst, hiin, loin, hiou, loou, s, t, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hiin
	mthi	$4, $ac3
	li	$4, \loin
	mtlo	$4, $ac3
	li	$4, \s
	li	$5, \t
	\inst	$ac3, $4, $5
	li	$4, \hiou
	mfhi	$5, $ac3
	dsp_assert	$4, $5
	li	$4, \loou
	mflo	$5, $ac3
	dsp_assert	$4, $5
	li	$4, \crout
	and	$4, $4, 0x80000
	rddsp	$5
	and	$5, $5, 0x80000
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Set $ac1 <= {hi, lo}
	# Check if t == (inst ? $ac1, sa)
	# Check if crout == dsp control register
	.macro dspck_atsaio inst, hi, lo, t, sa, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac1
	li	$4, \lo
	mtlo	$4, $ac1
	\inst	$5, $ac1, \sa
	li	$6, \t
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Set acc <= {hiin, loin}
	# (inst acc, s, t)
	# Check if {hiou, loou} == acc
	# Check if (crout & 0x80000) == (dsp control register & 0x80000)
	.macro dspckacc_astio inst, acc, hiin, loin, hiou, loou, s, t, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hiin
	mthi	$4, \acc
	li	$4, \loin
	mtlo	$4, \acc
	li	$4, \s
	li	$5, \t
	\inst	\acc, $4, $5
	li	$4, \hiou
	mfhi	$5, \acc
	dsp_assert	$4, $5
	li	$4, \loou
	mflo	$5, \acc
	dsp_assert	$4, $5
	li	$4, \crout
	and	$4, $4, 0x80000
	rddsp	$5
	and	$5, $5, 0x80000
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Set $ac1 <= {hi, lo}
	# Check if t == (inst ? $ac1, s)
	# Check if crout == dsp control register
	.macro dspck_atsio inst, hi, lo, t, s, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac1
	li	$4, \lo
	mtlo	$4, $ac1
	li	$4, \s
	\inst	$5, $ac1, $4
	li	$6, \t
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= (crin & crinmask)
	# Set $ac2 <= {hi, lo}
	# Check if t == (inst ? $ac2, size)
	# Check if (crout & croutmask) == (dsp control register & croutmask)
	.macro dspck_tasiimom inst, hi, lo, t, size, crin, crinmask, crout, croutmask
	li	$4, \crin
	and	$4, \crinmask
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac2
	li	$4, \lo
	mtlo	$4, $ac2
	\inst	$5, $ac2, \size
	li	$6, \t
	dsp_assert	$5, $6
	li	$4, \crout
	and	$4, \croutmask
	rddsp	$5
	and	$5, \croutmask
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= (crin & crinmask)
	# Set $ac2 <= {hi, lo}
	# Check if t == (inst ? $ac2, size)
	.macro dspck_tasiim inst, hi, lo, t, size, crin, crinmask
	li	$4, \crin
	and	$4, \crinmask
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac2
	li	$4, \lo
	mtlo	$4, $ac2
	\inst	$5, $ac2, \size
	li	$6, \t
	dsp_assert	$5, $6
	.endm

	# Set dsp control register <= (crin & crinmask)
	# Set $ac2 <= {hi, lo}
	# Check if t == (inst ? $ac2, s)
	# Check if (crout & croutmask) == (dsp control register & croutmask)
	.macro dspck_tasimom inst, hi, lo, t, s, crin, crinmask, crout, croutmask
	li	$4, \crin
	and	$4, \crinmask
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac2
	li	$4, \lo
	mtlo	$4, $ac2
	li	$4, \s
	\inst	$5, $ac2, $4
	li	$6, \t
	dsp_assert	$5, $6
	li	$4, \crout
	and	$4, \croutmask
	rddsp	$5
	and	$5, \croutmask
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= (crin & crinmask)
	# Set $ac2 <= {hi, lo}
	# Check if t == (inst ? $ac2, s)
	.macro dspck_tasim inst, hi, lo, t, s, crin, crinmask
	li	$4, \crin
	and	$4, \crinmask
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac2
	li	$4, \lo
	mtlo	$4, $ac2
	li	$4, \s
	\inst	$5, $ac2, $4
	li	$6, \t
	dsp_assert	$5, $6
	.endm

	# Set dsp control register <= crin
	# Set $ac0 <= {hi, lo}
	# (inst $ac0, shift)
	# Check if $ac0 == {hio, loo}
	# Check if crout == dsp control register
	.macro dspck_asaio inst, hi, lo, hio, loo, shift, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac0
	li	$4, \lo
	mtlo	$4, $ac0
	\inst	$ac0, \shift
	mfhi	$5, $ac0
	li	$6, \hio
	dsp_assert	$5, $6
	mflo	$5, $ac0
	li	$6, \loo
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Set $ac0 <= {hi, lo}
	# (inst $ac0, s)
	# Check if $ac0 == {hio, loo}
	# Check if crout == dsp control register
	.macro dspck_asio inst, hi, lo, hio, loo, s, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac0
	li	$4, \lo
	mtlo	$4, $ac0
	li	$4, \s
	\inst	$ac0, $4
	mfhi	$5, $ac0
	li	$6, \hio
	dsp_assert	$5, $6
	mflo	$5, $ac0
	li	$6, \loo
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# Set $ac3 <= {hi, lo}
	# Check if s == (inst ? $ac3)
	# Check if $ac3 == {hio, loo}
	# Check if crout == dsp control register
	.macro dspck_saio inst, hi, lo, hio, loo, s, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$4, \hi
	mthi	$4, $ac3
	li	$4, \lo
	mtlo	$4, $ac3
	li	$5, \s
	\inst	$5, $ac3
	mfhi	$5, $ac3
	li	$6, \hio
	dsp_assert	$5, $6
	mflo	$5, $ac3
	li	$6, \loo
	dsp_assert	$5, $6
	li	$4, \crout
	rddsp	$5
	dsp_assert	$4, $5
	.endm

	# Set dsp control register <= crin
	# (wrdsp s, m)
	# Check if crout == dsp control register
	.macro dspck_wrdsp s, m, crin, crout
	li	$4, \crin
	wrdsp	$4
	li	$5, \s
	wrdsp	$5, \m
	li	$6, \crout
	rddsp	$7
	dsp_assert	$6, $7
	.endm

	# Set dsp control register <= crin
	# Check if d == (rddsp ?, m)
	.macro dspck_rddsp d, m, crin
	li	$4, \crin
	wrdsp	$4
	rddsp	$5, \m
	li	$6, \d
	dsp_assert	$5, $6
	.endm

	# Check if d == (inst i(b))
	.macro dspck_load inst, d, i, b
	li	$4, \i
	la	$5, \b
	\inst	$6, $4($5)
	li	$7, \d
	dsp_assert	$6, $7
	.endm

	# Set dsp control register <= crin
	# Check if bposge32 is taken or not as expected in r
	# (1 => taken, 0 => not taken)
	.macro dspck_bposge32 crin, r
	li	$4, \crin
	wrdsp	$4
	li	$5, 1
	bposge32	1f
	nop
	li	$5, 0
1:
	li	$6, \r
	dsp_assert	$5, $6
	.endm

	# Check if tou == (inst tin, s)
	.macro dspck_tsimm inst, tou, tin, s, sa
	li	$4, \s
	li	$5, \tin
	\inst	$5, $4, \sa
	li	$6, \tou
	dsp_assert	$5, $6
	.endm
