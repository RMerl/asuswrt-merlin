# sh testcase for fcnvds 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
	double_prec
	sz_64
	set_grs_a5a5
	set_fprs_a5a5
	mov.l	ax, r0
	fmov	@r0, dr0
	fcnvds dr0, fpul
	fsts	fpul, fr2

	assert_dpreg_i	5, dr0
	single_prec
	assert_fpreg_i	5, fr2
	test_fpr_a5a5	fr3
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

	assertreg0	x
	test_gr_a5a5	r1
	test_gr_a5a5	r2
	test_gr_a5a5	r3
	test_gr_a5a5	r4
	test_gr_a5a5	r5
	test_gr_a5a5	r6
	test_gr_a5a5	r7
	test_gr_a5a5	r8
	test_gr_a5a5	r9
	test_gr_a5a5	r10
	test_gr_a5a5	r11
	test_gr_a5a5	r12
	test_gr_a5a5	r13
	test_gr_a5a5	r14

	pass
	exit 0

	.align 2
x:	.double	5.0
ax:	.long	x

