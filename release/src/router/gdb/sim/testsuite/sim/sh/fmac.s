# sh testcase for fmac 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fmac_:
	set_grs_a5a5
	set_fprs_a5a5
	# 0.0 * x + y = y.

	fldi0	fr0
	fldi1	fr1
	fldi1	fr2
	fmac	fr0, fr1, fr2
	# check result.
	fldi1	fr0
	fcmp/eq	fr0, fr2
	bt	.L0
	fail
.L0:
	# x * y + 0.0 = x * y.

	fldi1	fr0
	fldi1	fr1
	fldi0	fr2
	# double it.
	fadd	fr1, fr2
	fmac	fr0, fr1, fr2
	# check result.
	fldi1	fr0
	fadd	fr0, fr0
	fcmp/eq	fr0, fr2
	bt	.L1
	fail
.L1:	
	# x * 0.0 + y = y.

	fldi1	fr0
	fldi0	fr1
	fldi1	fr2
	fadd	fr2, fr2
	fmac	fr0, fr1, fr2
	# check result.
	fldi1	fr0
	# double fr0.
	fadd	fr0, fr0
	fcmp/eq	fr0, fr2
	bt	.L2
	fail
.L2:
	# x * 0.0 + 0.0 = 0.0

	fldi1	fr0
	fadd	fr0, fr0
	fldi0	fr1
	fldi0	fr2
	fmac	fr0, fr1, fr2
	# check result.
	fldi0	fr0
	fcmp/eq	fr0, fr2
	bt	.L3
	fail
.L3:	
	# 0.0 * x + 0.0 = 0.0.

	fldi0	fr0
	fldi1	fr1
	# double it.
	fadd	fr1, fr1
	fldi0	fr2
	fmac	fr0, fr1, fr2
	# check result.
	fldi0	fr0
	fcmp/eq	fr0, fr2
	bt	.L4
	fail
.L4:
	test_grs_a5a5
	assert_fpreg_i	0, fr0
	assert_fpreg_i	2, fr1
	assert_fpreg_i	0, fr2
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
	pass
	exit 0
