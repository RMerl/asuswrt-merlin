# sh testcase for pswap
# mach: shdsp
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

pswapx:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, x0
	pswap	x0, y0
	assert_sreg	0x7777a5a5, y0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5 
	assert_sreg	0xa5a57777, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pswapy:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, y0
	pswap	y0, x0
	assert_sreg	0x7777a5a5, x0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a57777, y0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pswapa:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, a0
	pcopy	a0, a1
	pswap	a1, y0
	assert_sreg	0x7777a5a5, y0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a57777, a0
	assert_sreg2	0xa5a57777, a1
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

pswapm:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, a0
	pcopy	a0, m1
	pswap	m1, y0
	assert_sreg	0x7777a5a5, y0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a57777, a0
	assert_sreg2	0xa5a57777, m1
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0


dct_pswapx:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, x0
	set_dcfalse
	dct	pswap	x0, y0
	assert_sreg	0xa5a5a5a5, y0
	set_dctrue
	dct	pswap	x0, y0
	assert_sreg	0x7777a5a5, y0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5 
	assert_sreg	0xa5a57777, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

dcf_pswapy:
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_greg	0xa5a57777, r0
	lds	r0, x0
	set_dctrue
	dcf	pswap	x0, y0
	assert_sreg	0xa5a5a5a5, y0
	set_dcfalse
	dcf	pswap	x0, y0
	assert_sreg	0x7777a5a5, y0

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5 
	assert_sreg	0xa5a57777, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

	pass
	exit 0
