# mips test sanity, expected to pass.
# mach:	 sb1
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

	# make sure that Status.FR, .CU1, and .SBX are set.
	mfc0	$2, $12
	or	$2, $2, (1 << 26) | (1 << 29) | (1 << 16)
	mtc0	$2, $12


	li.s	$f10, 4.0
	li.s	$f12, 16.0
	cvt.ps.s $f20, $f10, $f12		# $f20: u=4.0, l=16.0

	li.s	$f10, -1.0
	li.s	$f12, 2.0
	cvt.ps.s $f22, $f10, $f12		# $f22: u=-1.0, l=2.0


	writemsg "div.ps"

	div.ps $f8, $f20, $f22
	check_ps $f8, -4.0, 8.0


	writemsg "recip.ps"

	recip.ps $f8, $f20
	check_ps $f8, 0.25, 0.0625


	writemsg "rsqrt.ps"

	rsqrt.ps $f8, $f20
	check_ps $f8, 0.5, 0.25


	writemsg "sqrt.ps"

	sqrt.ps $f8, $f20
	check_ps $f8, 2.0, 4.0


	pass

	.end DIAG
