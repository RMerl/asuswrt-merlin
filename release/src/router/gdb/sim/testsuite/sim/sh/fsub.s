# sh testcase for fsub
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fsub_single:
	set_grs_a5a5
	set_fprs_a5a5
	# 0.0 - 0.0 = 0.0.
	fldi0	fr0
	fldi0	fr1
	fsub	fr0, fr1
	fldi0	fr2
	fcmp/eq	fr1, fr2
	bt	.L0
	fail
.L0:
	# 1.0 - 0.0 = 1.0.
	fldi0	fr0
	fldi1	fr1
	fsub	fr0, fr1
	fldi1	fr2
	fcmp/eq	fr1, fr2
	bt	.L1
	fail
.L1:
	# 1.0 - 1.0 = 0.0.
	fldi1	fr0
	fldi1	fr1
	fsub	fr0, fr1
	fldi0	fr2
	fcmp/eq	fr1, fr2
	bt	.L2
	fail
.L2:
	# 0.0 - 1.0 = -1.0.
	fldi1	fr0
	fldi0	fr1
	fsub	fr0, fr1
	fldi1	fr2
	fneg	fr2
	fcmp/eq	fr1, fr2
	bt	.L3
	fail
.L3:
	test_grs_a5a5
	assert_fpreg_i	 1, fr0
	assert_fpreg_i	-1, fr1
	assert_fpreg_i	-1, fr2
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

fsub_double:
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	# 0.0 - 0.0 = 0.0.
	fldi0	fr0
	fldi0	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fsub	dr0, dr2
	fldi0	fr4
	_s2d	fr4, dr4
	fcmp/eq	dr2, dr4
	bt	.L10
	fail
.L10:	
	# 1.0 - 0.0 = 1.0.
	fldi0	fr0
	fldi1	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fsub	dr0, dr2
	fldi1	fr4
	_s2d	fr4, dr4
	fcmp/eq	dr2, dr4
	bt	.L11
	fail
.L11:	
	# 1.0 - 1.0 = 0.0.
	fldi1	fr0
	fldi1	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fsub	dr0, dr2
	fldi0	fr4
	_s2d	fr4, dr4
	fcmp/eq	dr2, dr4
	bt	.L12
	fail
.L12:	
	# 0.0 - 1.0 = -1.0.
	fldi1	fr0
	fldi0	fr2
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	fsub	dr0, dr2
	fldi1	fr4
	single_prec
	fneg	fr4
	double_prec
	_s2d	fr4, dr4
	fcmp/eq	dr2, dr4
	bt	.L13
	fail
.L13:
	test_grs_a5a5
	assert_dpreg_i	 1, dr0
	assert_dpreg_i	-1, dr2
	assert_dpreg_i	-1, dr4
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
