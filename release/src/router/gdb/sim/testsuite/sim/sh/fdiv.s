# sh testcase for fdiv 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fdiv_single:
	# Single test
	set_grs_a5a5
	set_fprs_a5a5
	single_prec
	# 1.0 / 0.0 should be INF
	# (and not crash the sim).
	fldi0	fr0
	fldi1	fr1
	fdiv	fr0, fr1
	assert_fpreg_x	0x7f800000, fr1

	# 0.0 / 1.0 == 0.0.
	fldi0	fr0
	fldi1	fr1
	fdiv	fr1, fr0
	assert_fpreg_x	0, fr0

	# 2.0 / 1.0 == 2.0.
	fldi1	fr1
	fldi1	fr2
	fadd	fr2, fr2
	fdiv	fr1, fr2
	assert_fpreg_i	2, fr2

	# (1.0 / 2.0) + (1.0 / 2.0) == 1.0.
	fldi1	fr1
	fldi1	fr2
	fadd	fr2, fr2
	fdiv	fr2, fr1
	# fr1 should contain 0.5.
	fadd	fr1, fr1
	assert_fpreg_i	1, fr1
	test_grs_a5a5
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

fdiv_double:	
	# Double test
	set_grs_a5a5
	set_fprs_a5a5
	# (1.0 / 2.0) + (1.0 / 2.0) == 1.0.
	fldi1	fr1
	fldi1	fr2
	# This add must be in single precision.  The rest must be in double.
	fadd	fr2, fr2
	double_prec
	_s2d	fr1, dr0
	_s2d	fr2, dr2
	fdiv	dr2, dr0
	# dr0 should contain 0.5.
	# double it, expect 1.0.
	fadd	dr0, dr0
	assert_dpreg_i	1, dr0
	assert_dpreg_i	2, dr2
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

