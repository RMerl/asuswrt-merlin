# sh testcase for fabs 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fabs_freg_b0:
	single_prec
	bank0
	set_grs_a5a5
	set_fprs_a5a5
	# fabs(0.0) = 0.0.
	fldi0	fr0
	fabs	fr0
	fldi0	fr1
	fcmp/eq	fr0, fr1
	bt	.L1
	fail
.L1:
	# fabs(1.0) = 1.0.
	fldi1	fr0
	fabs	fr0
	fldi1	fr1
	fcmp/eq	fr0, fr1
	bt	.L2
	fail
.L2:
	# fabs(-1.0) = 1.0.
	fldi1	fr0
	fneg	fr0
	fabs	fr0
	fldi1	fr1
	fcmp/eq	fr0, fr1
	bt	.L3
	fail
.L3:
	test_grs_a5a5
	test_fpr_a5a5 fr2
	test_fpr_a5a5 fr3
	test_fpr_a5a5 fr4
	test_fpr_a5a5 fr5
	test_fpr_a5a5 fr6
	test_fpr_a5a5 fr7
	test_fpr_a5a5 fr8
	test_fpr_a5a5 fr9
	test_fpr_a5a5 fr10
	test_fpr_a5a5 fr11
	test_fpr_a5a5 fr12
	test_fpr_a5a5 fr13
	test_fpr_a5a5 fr14
	test_fpr_a5a5 fr15

fabs_dreg_b0:
	# double precision tests.
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	# fabs(0.0) = 0.0.
	fldi0	fr0
	flds	fr0, fpul
	fcnvsd	fpul, dr0
	fabs dr0
	assert_dpreg_i 0 dr0

	# fabs(1.0) = 1.0.
	fldi1	fr0
	flds	fr0, fpul
	fcnvsd	fpul, dr0
	fabs dr0
	assert_dpreg_i 1 dr0

	# check.
	fldi1 fr2
	flds	fr2, fpul
	fcnvsd	fpul, dr2
	fcmp/eq dr0, dr2
	bt	.L4
	fail

.L4:
	# fabs(-1.0) = 1.0.
	fldi1 fr0
	fneg fr0
	flds	fr0, fpul
	fcnvsd	fpul, dr0
	fabs dr0
	assert_dpreg_i 1 dr0

	# check.
	fldi1 fr2
	flds	fr2, fpul
	fcnvsd	fpul, dr2
	fcmp/eq dr0, dr2
	bt	.L5
	fail
.L5:
	test_grs_a5a5
	assert_dpreg_i 1 dr0
	assert_dpreg_i 1 dr2
	test_fpr_a5a5 fr4
	test_fpr_a5a5 fr5
	test_fpr_a5a5 fr6
	test_fpr_a5a5 fr7
	test_fpr_a5a5 fr8
	test_fpr_a5a5 fr9
	test_fpr_a5a5 fr10
	test_fpr_a5a5 fr11
	test_fpr_a5a5 fr12
	test_fpr_a5a5 fr13
	test_fpr_a5a5 fr14
	test_fpr_a5a5 fr15
	
	pass
	exit 0
