# sh testcase for pabs
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

	pabs	x0, x1
	pabs	y0, y1
	assert_sreg	0x5a5a5a5b, x1
	assert_sreg	0x5a5a5a5b, y1
	pabs	x1, x0
	pabs	y1, y0
	assert_sreg	0x5a5a5a5b, x0
	assert_sreg	0x5a5a5a5b, y0

	set_dcfalse
	dct pabs a0, a0
	dct pabs m0, m0
	assert_sreg	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, m0
	set_dctrue
	dct pabs a0, a0
	dct pabs m0, m0
	assert_sreg	0x5a5a5a5b, a0
	assert_sreg2	0x5a5a5a5b, m0

	set_dctrue
	dcf pabs a1, a1
	dcf pabs m1, m1
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg2	0xa5a5a5a5, m1
	set_dcfalse
	dcf pabs a1, a1
	dcf pabs m1, m1
	assert_sreg2	0x5a5a5a5b, a1
	assert_sreg2	0x5a5a5a5b, m1

	test_grs_a5a5

	pass
	exit 0
