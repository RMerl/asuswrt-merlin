# sh testcase for fsqrt 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fsqrt_single:
	set_grs_a5a5
	set_fprs_a5a5
	# sqrt(0.0) = 0.0.
	fldi0	fr0
	fsqrt	fr0
	fldi0	fr1
	fcmp/eq	fr0, fr1
	bt	.L0
	fail
.L0:
	# sqrt(1.0) = 1.0.
	fldi1	fr0
	fsqrt	fr0
	fldi1	fr1
	fcmp/eq	fr0, fr1
	bt	.L1
	fail
.L1:
	# sqrt(4.0) = 2.0
	fldi1	fr0
	# Double it.
	fadd	fr0, fr0
	# Double it again.
	fadd	fr0, fr0
	fsqrt	fr0
	fldi1	fr1
	# Double it.
	fadd	fr1, fr1
	fcmp/eq	fr0, fr1
	bt	.L2
	fail
.L2:
	test_grs_a5a5
	assert_fpreg_i	2, fr0
	assert_fpreg_i	2, fr1
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

fsqrt_double:
	double_prec
	set_grs_a5a5
	set_fprs_a5a5
	# sqrt(0.0) = 0.0.
	fldi0	fr0
	_s2d	fr0, dr0
	fsqrt	dr0
	fldi0	fr2
	_s2d	fr2, dr2
	fcmp/eq	dr0, dr2
	bt	.L10
	fail
.L10:
	# sqrt(1.0) = 1.0.
	fldi1	fr0
	_s2d	fr0, dr0
	fsqrt	dr0
	fldi1	fr2
	_s2d	fr2, dr2
	fcmp/eq	dr0, dr2
	bt	.L11
	fail
.L11:
	# sqrt(4.0) = 2.0.
	fldi1	fr0
	# Double it.
	single_prec
	fadd	fr0, fr0
	# Double it again.
	fadd	fr0, fr0
	double_prec
	_s2d	fr0, dr0
	fsqrt	dr0
	fldi1	fr2
	# Double it.
	single_prec
	fadd	fr2, fr2
	double_prec
	_s2d	fr2, dr2
	fcmp/eq	dr0, dr2
	bt	.L12
	fail
.L12:
	test_grs_a5a5
	assert_dpreg_i	2, dr0
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
