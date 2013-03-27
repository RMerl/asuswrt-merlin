# sh testcase for fcmpgt 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fcmpgt_single:
	set_grs_a5a5
	set_fprs_a5a5
	# 1.0 !> 1.0.
	fldi1	fr0
	fldi1	fr1
	fcmp/gt	fr0, fr1
	bf	.L0
	fail
.L0:
	# 0.0 !> 1.0.
	fldi0	fr0
	fldi1	fr1
	fcmp/gt	fr0, fr1
	bt	.L1
	fail
.L1:
	# 1.0 > 0.0.
	fldi1	fr0
	fldi0	fr1
	fcmp/gt	fr0, fr1
	bf	.L2
	fail
.L2:
	# 2.0 > 1.0
	fldi1	fr0
	fadd	fr0, fr0
	fldi1	fr1
	fcmp/gt	fr0, fr1
	bf	.L3
	fail
.L3:
	test_grs_a5a5
	assert_fpreg_i	2, fr0
	assert_fpreg_i	1, fr1
	test_fpr_a5a5	fr2
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

fcmpgt_double:
	# double precision tests.
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	# 1.0 !> 1.0.
	fldi1	fr0
	fldi1	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fcmp/gt	dr0, dr2
	bf	.L10
	fail
.L10:
	# 0.0 !> 1.0.
	fldi0	fr0
	fldi1	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fcmp/gt	dr0, dr2
	bt	.L11
	fail
.L11:
	# 1.0 > 0.0.
	fldi1	fr0
	fldi0	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fcmp/gt	dr0, dr2
	bf	.L12
	fail
.L12:
	# 2.0 > 1.0.
	fldi1	fr0
	single_prec
	fadd	fr0, fr0
	double_prec
	fldi1	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fcmp/gt	dr0, dr2
	bf	.L13
	fail
.L13:
	test_grs_a5a5
	assert_dpreg_i	2, dr0
	assert_dpreg_i	1, dr2
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
