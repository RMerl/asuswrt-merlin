# sh testcase for prnd
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp

	# FIXME: opcode table ambiguity in ignored bits 4-7.

	.include "testutils.inc"

	start
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	# prnd(0xa5a5a5a5) = 0xa5a60000
	prnd	x0, x0
	prnd	y0, y0
	assert_sreg	0xa5a60000, x0
	assert_sreg	0xa5a60000, y0

	# prnd(1) = 1
	mov	#1, r0
	shll16	r0
	lds	r0, x0
	pcopy	x0, y0
	prnd	x0, x0
	prnd	y0, y0
	assert_sreg	0x10000, x0
	assert_sreg	0x10000, y0

	# prnd(1.4999999) = 1
	mov	#1, r0
	shll8	r0
	or	#0x7f, r0
	shll8	r0
	or	#0xff, r0
	lds	r0, x0
	pcopy	x0, y0
	prnd	x0, x0
	prnd	y0, y0
	assert_sreg	0x10000, x0
	assert_sreg	0x10000, y0

	# prnd(1.5) = 2
	mov	#1, r0
	shll8	r0
	or	#0x80, r0
	shll8	r0
	lds	r0, x0
	pcopy	x0, y0
	prnd	x0, x0
	prnd	y0, y0
	assert_sreg	0x20000, x0
	assert_sreg	0x20000, y0

	# dct prnd
	set_dcfalse
	dct	prnd	x0, x1
	dct	prnd	y0, y1
	assert_sreg2	0xa5a5a5a5, x1
	assert_sreg2	0xa5a5a5a5, y1
	set_dctrue
	dct	prnd	x0, x1
	dct	prnd	y0, y1
	assert_sreg2	0x20000, x1
	assert_sreg2	0x20000, y1

	# dcf prnd
	set_dctrue
	dcf	prnd	x0, m0
	dcf	prnd	y0, m1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1
	set_dcfalse
	dcf	prnd	x0, m0
	dcf	prnd	y0, m1
	assert_sreg2	0x20000, m0
	assert_sreg2	0x20000, m1

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	pass
	exit 0
