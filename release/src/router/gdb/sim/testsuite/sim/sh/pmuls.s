# sh testcase for pmuls
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

	# 2 x 2 = 8 (?)
	# (I don't understand why the result is x2,
	# but that's what it says in the manual...)
	mov	#2, r0
	shll16	r0
	lds	r0, y0
	lds	r0, y1
	pmuls	y0, y1, a0

	assert_sreg	8, a0

	set_greg 0xa5a5a5a5, r0
	test_grs_a5a5
	pass
	exit 0

