# sh testcase for fipr $fvm, $fvn
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
initv0:
	set_grs_a5a5
	set_fprs_a5a5
	# Load 1 into fr0.
	fldi1	fr0
	# Load 2 into fr1.
	fldi1	fr1
	fadd	fr1, fr1
	# Load 4 into fr2.
	fldi1	fr2
	fadd	fr2, fr2
	fadd	fr2, fr2
	# Load 8 into fr3.
	fmov	fr2, fr3
	fadd	fr2, fr3

initv8:
	fldi1	fr8
	fldi0	fr9
	fldi1	fr10
	fldi0	fr11

	fipr	fv0, fv8
test1:	
	# Result will be in fr11.
	assert_fpreg_i	1, fr0
	assert_fpreg_i	2, fr1
	assert_fpreg_i	4, fr2
	assert_fpreg_i	8, fr3
	assert_fpreg_x	0xa5a5a5a5, fr4
	assert_fpreg_x	0xa5a5a5a5, fr5
	assert_fpreg_x	0xa5a5a5a5, fr6
	assert_fpreg_x	0xa5a5a5a5, fr7
	assert_fpreg_i	1, fr8
	assert_fpreg_i	0, fr9
	assert_fpreg_i	1, fr10
	assert_fpreg_i	5, fr11
	assert_fpreg_x	0xa5a5a5a5, fr12
	assert_fpreg_x	0xa5a5a5a5, fr13
	assert_fpreg_x	0xa5a5a5a5, fr14
	assert_fpreg_x	0xa5a5a5a5, fr15

	test_grs_a5a5
test_infp:
	# Test positive infinity
	fldi0	fr11
	mov.l	infp, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be plus infinity
	assert_fpreg_x	0x7f800000, fr11
test_infm:
	# Test negitive infinity
	fldi0	fr11
	mov.l	infm, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be plus infinity
	assert_fpreg_x	0xff800000, fr11
test_qnanp:
	# Test positive qnan
	fldi0	fr11
	mov.l	qnanp, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be plus qnan (or greater)
	flds	fr11, fpul
	sts	fpul, r1
	cmp/ge	r0, r1
	bt	.L0
	fail
.L0:
test_snanp:
	# Test positive snan
	fldi0	fr11
	mov.l	snanp, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be plus snan (or greater)
	flds	fr11, fpul
	sts	fpul, r1
	cmp/ge	r0, r1
	bt	.L1
	fail
.L1:
.if 0
	# Handling of nan and inf not implemented yet.
test_qnanm:
	# Test negantive qnan
	fldi0	fr11
	mov.l	qnanm, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be minus qnan (or less)
	flds	fr11, fpul
	sts	fpul, r1
	cmp/ge	r1, r0
	bt	.L2
	fail
.L2:
test_snanm:
	# Test negative snan
	fldi0	fr11
	mov.l	snanm, r0
	lds	r0, fpul
	fsts	fpul, fr0
	fipr	fv0, fv8
	# fr11 should be minus snan (or less)
	flds	fr11, fpul
	sts	fpul, r1
	cmp/ge	r1, r0
	bt	.L3
	fail
.L3:	
.endif
	pass
	exit 0

	.align 2
qnanp:	.long	0x7f800001
qnanm:	.long	0xff800001
snanp:	.long	0x7fc00000
snanm:	.long	0xffc00000
infp:	.long	0x7f800000
infm:	.long	0xff800000
