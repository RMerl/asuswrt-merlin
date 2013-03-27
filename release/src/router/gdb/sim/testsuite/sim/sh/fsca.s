# sh testcase for fsca
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fsca:
	set_grs_a5a5
	set_fprs_a5a5
	# Start with angle zero
	mov.l	zero, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 0, fr2
	assert_fpreg_i 1, fr3
	
	mov.l	plus_90, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 1, fr2
	assert_fpreg_i 0, fr3
	
	mov.l	plus_180, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 0, fr2
	assert_fpreg_i -1, fr3
	
	mov.l	plus_270, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i -1, fr2
	assert_fpreg_i 0, fr3
	
	mov.l	plus_360, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 0, fr2
	assert_fpreg_i 1, fr3

	mov.l	minus_90, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i -1, fr2
	assert_fpreg_i 0, fr3
	
	mov.l	minus_180, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 0, fr2
	assert_fpreg_i -1, fr3
	
	mov.l	minus_270, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 1, fr2
	assert_fpreg_i 0, fr3
	
	mov.l	minus_360, r0
	lds	r0, fpul
	fsca	fpul, dr2
	assert_fpreg_i 0, fr2
	assert_fpreg_i 1, fr3

	assertreg0      0xffff0000
	set_greg        0xa5a5a5a5, r0
	test_grs_a5a5
	test_fpr_a5a5	fr0
	test_fpr_a5a5	fr1
	test_fpr_a5a5	fr4
	test_fpr_a5a5	fr5
	test_fpr_a5a5	fr6
	test_fpr_a5a5	fr7
	test_fpr_a5a5	fr8
	test_fpr_a5a5	fr9
	test_fpr_a5a5	fr10
	test_fpr_a5a5	fr11
	test_fpr_a5a5	fr12
	test_fpr_a5a5	fr13
	test_fpr_a5a5	fr14
	test_fpr_a5a5	fr15
	pass
	exit 0

		.align 2
zero:		.long	0
one_bitty:	.long	1
plus_90:	.long	0x04000
plus_180:	.long	0x08000
plus_270:	.long	0x0c000
plus_360:	.long	0x10000
minus_90:	.long	0xffffc000
minus_180:	.long	0xffff8000
minus_270:	.long	0xffff4000
minus_360:	.long	0xffff0000
minus_1_bitty:	.long	0xffffffff
