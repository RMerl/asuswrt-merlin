# sh testcase for pand
# mach:	 shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp

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

	pand	x0, y0, a0
	assert_sreg	0xa5a50000, a0

	# 0xa5a5a5a5 & 0x5a5a5a5a == 0
	set_greg	0x5a5a5a5a r0
	lds	r0, x0
	pand	x0, y0, a0
	assert_sreg	0, a0
		
	set_dcfalse
	dct pand x0, y0, m0
	assert_sreg2	0xa5a5a5a5, m0
	set_dctrue
	dct pand x0, y0, m0
	assert_sreg2	0, m0

	set_dctrue
	dcf pand x0, y0, m1
	assert_sreg2	0xa5a5a5a5, m1
	set_dcfalse
	dcf pand x0, y0, m1
	assert_sreg2	0, m1

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, a1

	pass
	exit 0
