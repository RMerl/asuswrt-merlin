# sh testcase for paddc
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

	# 2 + 2 = 4
	set_dcfalse
	mov	#2, r0
	lds	r0, x0
	lds	r0, y0
	paddc	x0, y0, a0
	assert_sreg	4, a0

	# 2 + 2 + carry = 5
	set_dctrue
	paddc	x0, y0, a1
	assert_sreg2	5, a1

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1

	pass
	exit 0
