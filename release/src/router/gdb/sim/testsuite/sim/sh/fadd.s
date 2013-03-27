# sh testcase for fadd
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fadd_freg_freg_b0:
	set_grs_a5a5
	set_fprs_a5a5
	bank0

	fldi1 fr0
	fldi1 fr1
	fadd fr0, fr1
	assert_fpreg_i 2 fr1

	fldi0 fr0
	fldi1 fr1
	fadd fr0, fr1
	assert_fpreg_i 1 fr1

	fldi1 fr0
	fldi0 fr1
	fadd fr0, fr1
	assert_fpreg_i 1 fr1
	test_grs_a5a5
	assert_fpreg_i 1 fr0
	test_fpr_a5a5  fr2
	test_fpr_a5a5  fr3
	test_fpr_a5a5  fr4
	test_fpr_a5a5  fr5
	test_fpr_a5a5  fr6
	test_fpr_a5a5  fr7
	test_fpr_a5a5  fr8
	test_fpr_a5a5  fr9
	test_fpr_a5a5  fr10
	test_fpr_a5a5  fr11
	test_fpr_a5a5  fr12
	test_fpr_a5a5  fr13
	test_fpr_a5a5  fr14
	test_fpr_a5a5  fr15

fadd_dreg_dreg_b0:
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	fldi1	fr0
	fldi1	fr2
	flds	fr0, fpul
	fcnvsd	fpul, dr0
	flds	fr2, fpul
	fcnvsd	fpul, dr2
	fadd dr0, dr2
	fcnvds	dr2, fpul
	fsts	fpul, fr0

	test_grs_a5a5
	assert_fpreg_i	2, fr0
	assert_dpreg_i	2, dr2
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
