# sh testcase for psub
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

psub_sx_sy:
	# 0xa5a5a5a5 minus 0xa5a5a5a5 equals zero
	psub	x0, y0, a0
	assert_sreg	0, a0

psub_sy_sx:
	# 100 - 25 = 75
	mov	#100, r0
	mov	#25, r1
	lds	r0, y1
	lds	r1, x1
	psub	y1, x1, a0
	assert_sreg	75, a0

dct_psub:
	# 100 - 25 = 75
	set_dcfalse
	dct	psub	y1, x1, a1
	assert_sreg2	0xa5a5a5a5, a1
	set_dctrue
	dct	psub	y1, x1, a1
	assert_sreg2	75, a1

dcf_psub:
	# 25 - 100 = -75
	set_dctrue
	dcf	psub	x1, y1, m1
	assert_sreg2	0xa5a5a5a5, m1
	set_dcfalse
	dcf	psub	x1, y1, m1
	assert_sreg2	-75, m1

psub_pmuls:
	# 25 - 100 = -75, and 2 x 2 = 8 (yes, eight, not four)
	mov	#2, r0
	shll16	r0
	lds	r0, x0
	lds	r0, y0
	psub	x1, y1, a1	pmuls	x0, y0, a0
	assert_sreg	8, a0
	assert_sreg2	-75, a1

	set_greg	0xa5a5a5a5, r0
	set_greg	0xa5a5a5a5, r1
	test_grs_a5a5
	pass
	exit 0
