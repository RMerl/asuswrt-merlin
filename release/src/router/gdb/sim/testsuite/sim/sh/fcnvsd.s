# sh testcase for fcnvsd 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	fldi1	fr0
	flds	fr0, fpul
	fcnvsd	fpul, dr2
	assert_dpreg_i	1, dr2

	# Convert back.
	fcnvds	dr2, fpul
	fsts	fpul, fr1
	single_prec
	assert_fpreg_i	1, fr1
	fcmp/eq	fr0, fr1
	bt	.L0
	fail
.L0:
	test_grs_a5a5
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

