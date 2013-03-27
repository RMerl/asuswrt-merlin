# sh testcase for pdec
# mach: shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

pdecx:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	pdec	x0, y0
	assert_sreg	0xa5a40000, y0

	test_grs_a5a5 
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pdecy:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	pdec	y0, x0
	assert_sreg	0xa5a40000, x0

	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, y0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

dct_pdecx:
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
	dct	pdec	x0, y0
	assert_sreg	0xa5a5a5a5, y0
	set_dctrue
	dct	pdec	x0, y0
	assert_sreg	0xa5a40000, y0

	test_grs_a5a5 
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

dcf_pdecy:
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
	dcf	pdec	y0, x0
	assert_sreg	0xa5a5a5a5, x0
	set_dcfalse
	dcf	pdec	y0, x0
	assert_sreg	0xa5a40000, x0

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
