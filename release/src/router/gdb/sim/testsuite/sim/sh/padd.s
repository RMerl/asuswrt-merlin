# sh testcase for padd
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

	padd	x0, y0, a0
	assert_sreg	0x4b4b4b4a, a0

	# 2 + 2 = 4
	mov	#2, r0
	lds	r0, x0
	lds	r0, y0
	padd	x0, y0, a0
	assert_sreg	4, a0
		
	set_dcfalse
	dct padd x0, y0, a1
	assert_sreg2	0xa5a5a5a5, a1
	set_dctrue
	dct padd x0, y0, a1
	assert_sreg2	4, a1

	set_dctrue
	dcf padd x0, y0, m1
	assert_sreg2	0xa5a5a5a5, m1
	set_dcfalse
	dcf padd x0, y0, m1
	assert_sreg2	4, m1

	# padd / pmuls

	padd	x0, y0, y0	pmuls	x1, y1, m1
	assert_sreg	4, y0
	assert_sreg2	0x3fc838b2, m1	! (int) 0xa5a5 x (int) 0xa5a5 x 2

	set_greg	0xa5a5a5a5, r0
	test_grs_a5a5
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1

	pass
	exit 0
