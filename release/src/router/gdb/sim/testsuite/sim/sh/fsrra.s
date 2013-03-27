# sh testcase for fsrra
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fsrra_single:
	set_grs_a5a5
	set_fprs_a5a5
	# 1/sqrt(0.0) = +infinity.
	fldi0	fr0
	fsrra	fr0
	assert_fpreg_x	0x7f800000, fr0

	# 1/sqrt(1.0) = 1.0.
	fldi1	fr0
	fsrra	fr0
	assert_fpreg_i	1, fr0

	# 1/sqrt(4.0) = 1/2.0
	fldi1	fr0
	# Double it.
	fadd	fr0, fr0
	# Double it again.
	fadd	fr0, fr0
	fsrra	fr0
	fldi1	fr2
	# Double it.
	fadd	fr2, fr2
	fldi1	fr1
	# Divide
	fdiv	fr2, fr1
	fcmp/eq	fr0, fr1
	bt	.L2
	fail
.L2:
	# Double-check (pun intended)
	fadd	fr0, fr0
	assert_fpreg_i	1, fr0
	fadd	fr1, fr1
	assert_fpreg_i	1, fr1

	# And make sure the rest of the regs are un-affected.
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
	test_grs_a5a5

	pass
	exit 0
