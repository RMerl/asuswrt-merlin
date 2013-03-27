# mips test sanity, expected to pass.
# mach:	 mips64 sb1
# as:		-mabi=eabi
# ld:		-N -Ttext=0x80010000
# output:	*\\npass\\n

	.include "testutils.inc"

        .macro check_ps psval, upperval, lowerval
	.set push
	.set noreorder
	cvt.s.pu	$f0, \psval		# upper
	cvt.s.pl	$f2, \psval		# lower
	li.s		$f4, \upperval
	li.s		$f6, \lowerval
	c.eq.s		$fcc0, $f0, $f4
	bc1f		$fcc0, _fail
	 c.eq.s		$fcc0, $f2, $f6
	bc1f		$fcc0, _fail
	 nop
	.set pop
        .endm

	setup

	.set noreorder

	.ent DIAG
DIAG:

	# make sure that Status.FR and .CU1 are set.
	mfc0	$2, $12
	or	$2, $2, (1 << 26) | (1 << 29)
	mtc0	$2, $12


	writemsg "ldc1"

	.data
1:	.dword	0xc1a8000042200000		# -21.0, 40.0
	.text
	la	$2, 1b
	ldc1	$f8, 0($2)
	check_ps $f8, -21.0, 40.0


	writemsg "cvt.ps.s"

	li.s	$f10, 1.0
	li.s	$f12, 3.0
	cvt.ps.s $f8, $f10, $f12		# upper, lower
	check_ps $f8, 1.0, 3.0


	writemsg "cvt.ps.s, sdc1, copy, ldc1"

	.data
1:	.dword	0
	.dword	0
	.text
	la	$2, 1b
	li.s	$f12, -4.0
	li.s	$f14, 32.0
	cvt.ps.s $f10, $f12, $f14		# upper, lower
	sdc1	$f10, 8($2)
	lw	$3, 8($2)
	lw	$4, 12($2)
	sw	$3, 0($2)
	sw	$4, 4($2)
	ldc1	$f8, 0($2)
	check_ps $f8, -4.0, 32.0


	# Load some constants for later use

	li.s	$f10, 4.0
	li.s	$f12, 16.0
	cvt.ps.s $f20, $f10, $f12		# $f20: u=4.0, l=16.0

	li.s	$f10, -1.0
	li.s	$f12, 2.0
	cvt.ps.s $f22, $f10, $f12		# $f22: u=-1.0, l=2.0

	li.s	$f10, 17.0
	li.s	$f12, -8.0
	cvt.ps.s $f24, $f10, $f12		# $f24: u=17.0, l=-8.0


	writemsg "pll.ps"

	pll.ps	$f8, $f20, $f22
	check_ps $f8, 16.0, 2.0


	writemsg "plu.ps"

	plu.ps	$f8, $f20, $f22
	check_ps $f8, 16.0, -1.0


	writemsg "pul.ps"

	pul.ps	$f8, $f20, $f22
	check_ps $f8, 4.0, 2.0


	writemsg "puu.ps"

	puu.ps	$f8, $f20, $f22
	check_ps $f8, 4.0, -1.0


	writemsg "abs.ps"

	abs.ps	$f8, $f22
	check_ps $f8, 1.0, 2.0


	writemsg "mov.ps"

	mov.ps	$f8, $f22
	check_ps $f8, -1.0, 2.0


	writemsg "neg.ps"

	neg.ps	$f8, $f22
	check_ps $f8, 1.0, -2.0


	writemsg "add.ps"

	add.ps	$f8, $f20, $f22
	check_ps $f8, 3.0, 18.0


	writemsg "mul.ps"

	mul.ps	$f8, $f20, $f22
	check_ps $f8, -4.0, 32.0


	writemsg "sub.ps"

	sub.ps	$f8, $f20, $f22
	check_ps $f8, 5.0, 14.0


	writemsg "madd.ps"

	madd.ps	$f8, $f24, $f20, $f22
	check_ps $f8, 13.0, 24.0


	writemsg "msub.ps"

	msub.ps	$f8, $f24, $f20, $f22
	check_ps $f8, -21.0, 40.0


	writemsg "nmadd.ps"

	nmadd.ps $f8, $f24, $f20, $f22
	check_ps $f8, -13.0, -24.0


	writemsg "nmsub.ps"

	nmsub.ps $f8, $f24, $f20, $f22
	check_ps $f8, 21.0, -40.0


	writemsg "movn.ps (n)"

	li	$2, 0
	mov.ps	$f8, $f20
	movn.ps	$f8, $f22, $2		# doesn't move
	check_ps $f8, 4.0, 16.0


	writemsg "movn.ps (y)"

	li	$2, 1
	mov.ps	$f8, $f20
	movn.ps	$f8, $f22, $2		# does move
	check_ps $f8, -1.0, 2.0


	writemsg "movz.ps (y)"

	li	$2, 0
	mov.ps	$f8, $f20
	movz.ps	$f8, $f22, $2		# does move
	check_ps $f8, -1.0, 2.0


	writemsg "movz.ps (n)"

	li	$2, 1
	mov.ps	$f8, $f20
	movz.ps	$f8, $f22, $2		# doesn't move
	check_ps $f8, 4.0, 16.0


	writemsg "movf.ps (y,y)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (1 << 23) | (1 << 25)
	ctc1	$2, $31			# clear fcc0, clear fcc1
	mov.ps	$f8, $f20
	movf.ps	$f8, $f22, $fcc0	# moves both halves
	check_ps $f8, -1.0, 2.0


	writemsg "movf.ps (y,n)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (0 << 23) | (1 << 25)
	ctc1	$2, $31			# set fcc0, clear fcc1
	mov.ps	$f8, $f20
	movf.ps	$f8, $f22, $fcc0	# moves upper half only
	check_ps $f8, -1.0, 16.0


	writemsg "movf.ps (n,y)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (1 << 23) | (0 << 25)
	ctc1	$2, $31			# clear fcc0, set fcc1
	mov.ps	$f8, $f20
	movf.ps	$f8, $f22, $fcc0	# moves lower half only
	check_ps $f8, 4.0, 2.0


	writemsg "movf.ps (n,n)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (0 << 23) | (0 << 25)
	ctc1	$2, $31			# set fcc0, set fcc1
	mov.ps	$f8, $f20
	movf.ps	$f8, $f22, $fcc0	# doesn't move either half
	check_ps $f8, 4.0, 16.0


	writemsg "movt.ps (n,n)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (1 << 23) | (1 << 25)
	ctc1	$2, $31			# clear fcc0, clear fcc1
	mov.ps	$f8, $f20
	movt.ps	$f8, $f22, $fcc0	# doesn't move either half
	check_ps $f8, 4.0, 16.0


	writemsg "movt.ps (n,y)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (0 << 23) | (1 << 25)
	ctc1	$2, $31			# set fcc0, clear fcc1
	mov.ps	$f8, $f20
	movt.ps	$f8, $f22, $fcc0	# moves lower half only
	check_ps $f8, 4.0, 2.0


	writemsg "movt.ps (y,n)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (1 << 23) | (0 << 25)
	ctc1	$2, $31			# clear fcc0, set fcc1
	mov.ps	$f8, $f20
	movt.ps	$f8, $f22, $fcc0	# moves upper half only
	check_ps $f8, -1.0, 16.0


	writemsg "movt.ps (y,y)"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (1 << 25)
	xor	$2, $2, (0 << 23) | (0 << 25)
	ctc1	$2, $31			# set fcc0, set fcc1
	mov.ps	$f8, $f20
	movt.ps	$f8, $f22, $fcc0	# moves both halves
	check_ps $f8, -1.0, 2.0


	writemsg "alnv.ps (aligned)"

	.data
1:	.dword	0xc1a8000042200000		# -21.0, 40.0
	.dword	0xc228000041a00000		# -42.0, 20.0
	.text
	la	$2, 1b
	li	$3, 0
	addu	$4, $3, 8
	luxc1	$f10, $3($2)
	luxc1	$f12, $4($2)
	alnv.ps	$f8, $f10, $f12, $3
	check_ps $f8, -21.0, 40.0


	writemsg "alnv.ps (unaligned)"

	.data
1:	.dword	0xc1a8000042200000		# -21.0, 40.0
	.dword	0xc228000041a00000		# -42.0, 20.0
	.hword	0x0001
	.text
	la	$2, 1b
	li	$3, 4
	addu	$4, $3, 8
	luxc1	$f10, $3($2)
	luxc1	$f12, $4($2)
	alnv.ps	$f8, $f10, $f12, $3

	lb	$5, 16($2)
	bnez	$5, 2f				# little endian
	 nop

	# big endian
	check_ps $f8, 40.0, -42.0
	b	3f
	 nop
2:
	# little endian
	check_ps $f8, 20.0, -21.0
3:


	# We test c.cond.ps only lightly, just to make sure it modifies
	# two bits and compares the halves separately.  Perhaps it should
	# be tested more thoroughly.

	writemsg "c.f.ps"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (0x7f << 25)
	ctc1	$2, $31			# set all fcc bits
	c.f.ps	$fcc0, $f8, $f8		# -> f, f
	bc1t	$fcc0, _fail
	 nop
	bc1t	$fcc1, _fail
	 nop

	
	writemsg "c.olt.ps"

	cfc1	$2, $31	
	or	$2, $2, (1 << 23) | (0x7f << 25)
	xor	$2, $2, (1 << 23) | (0x7f << 25)
	ctc1	$2, $31			# clear all fcc bits
	c.lt.ps $fcc0, $f22, $f24	# -> f, t
	bc1t	$fcc0, _fail
	 nop
	bc1f	$fcc1, _fail
	 nop
	

	pass

	.end DIAG
