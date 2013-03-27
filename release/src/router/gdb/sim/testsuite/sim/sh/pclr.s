# sh testcase for pclr
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp

	# FIXME: opcode table ambiguity in ignored bits 4-7.

	.include "testutils.inc"

	start
pclr_cc:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	assert_sreg	0xa5a5a5a5, x0
	pclr	x0
	assert_sreg	0, x0

	set_dcfalse
	dct	pclr	x1
	assert_sreg	0xa5a5a5a5, x1
	set_dctrue
	dct	pclr	x1
	assert_sreg	0, x1
	
	set_dctrue
	dcf	pclr	y0
	assert_sreg	0xa5a5a5a5, y0
	set_dcfalse
	dcf	pclr	y0
	assert_sreg	0, y0

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pclr_pmuls:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	pclr	x0	pmuls	y0, y1, a0

	assert_sreg	0, x0
	assert_sreg	0x3fc838b2, a0	! 0xa5a5 x 0xa5a5

	test_grs_a5a5
	
	pass
	exit 0
