# sh testcase for fmul 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	.macro init
	fldi0 fr0
	fldi1 fr1
	fldi1 fr2
	fadd fr2, fr2
	.endm

	start
fmul_single:
	set_grs_a5a5
	set_fprs_a5a5
	# 0.0 * 0.0 = 0.0.
	init
	fmul	fr0, fr0
	assert_fpreg_i	0, fr0

	# 0.0 * 1.0 = 0.0.
	init
	fmul	fr1, fr0
	assert_fpreg_i	0, fr0

	# 1.0 * 0.0 = 0.0.
	init
	fmul	fr0, fr1
	assert_fpreg_i	0, fr1

	# 1.0 * 1.0 = 1.0.
	init
	fmul	fr1, fr1
	assert_fpreg_i	1, fr1

	# 2.0 * 1.0 = 2.0.
	init
	fmul	fr2, fr1
	assert_fpreg_i	2, fr1

	test_grs_a5a5
	assert_fpreg_i	0, fr0
	assert_fpreg_i	2, fr1
	assert_fpreg_i	2, fr2
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

	.macro dinit
	fldi0 fr0
	fldi1 fr2
	fldi1 fr4
	single_prec
	fadd fr4, fr4
	double_prec
	_s2d fr0, dr0
	_s2d fr2, dr2
	_s2d fr4, dr4
	.endm
	
fmul_double:
	double_prec
	# 0.0 * 0.0 = 0.0.
	dinit
	fmul	dr0, dr0
	assert_dpreg_i	0, dr0

	# 0.0 * 1.0 = 0.0.
	dinit
	fmul	dr2, dr0
	assert_dpreg_i	0, dr0

	# 1.0 * 0.0 = 0.0.
	dinit
	fmul	dr0, dr2
	assert_dpreg_i	0, dr2

	# 1.0 * 1.0 = 1.0.
	dinit
	fmul	dr2, dr2
	assert_dpreg_i	1, dr2

	# 2.0 * 1.0 = 2.0.
	dinit
	fmul	dr4, dr2
	assert_dpreg_i	2, dr2

	test_grs_a5a5
	assert_dpreg_i	0, dr0
	assert_dpreg_i	2, dr2
	assert_dpreg_i	2, dr4
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
