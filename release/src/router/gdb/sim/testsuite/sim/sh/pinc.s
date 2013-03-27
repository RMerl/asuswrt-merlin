# sh testcase for pinc
# mach: shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

pincx:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	pinc	x0, y0
	assert_sreg	0xa5a60000, y0

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pincy:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	pinc	y0, x0
	assert_sreg	0xa5a60000, x0

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, y0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

dct_pincx:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_dcfalse
	dct	pinc	x0, y0
	assert_sreg	0xa5a5a5a5, y0
	set_dctrue
	dct	pinc	x0, y0
	assert_sreg	0xa5a60000, y0

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

dcf_pincy:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_dctrue
	dcf	pinc	y0, x0
	assert_sreg	0xa5a5a5a5, x0
	set_dcfalse
	dcf	pinc	y0, x0
	assert_sreg	0xa5a60000, x0

	test_grs_a5a5 
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y0
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

	pass
	exit 0
