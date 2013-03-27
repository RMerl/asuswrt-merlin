# sh testcase for fldi0 $frn 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fldi0_single:
	set_grs_a5a5
	set_fprs_a5a5
	fldi0	fr0
	fldi0	fr2
	fldi0	fr4
	fldi0	fr6
	fldi0	fr8
	fldi0	fr10
	fldi0	fr12
	fldi0	fr14
	test_grs_a5a5
	assert_fpreg_i 0 fr0
	assert_fpreg_i 0 fr2
	assert_fpreg_i 0 fr4
	assert_fpreg_i 0 fr6
	assert_fpreg_i 0 fr8
	assert_fpreg_i 0 fr10
	assert_fpreg_i 0 fr12
	assert_fpreg_i 0 fr14
	assert_fpreg_x 0xa5a5a5a5 fr1
	assert_fpreg_x 0xa5a5a5a5 fr3
	assert_fpreg_x 0xa5a5a5a5 fr5
	assert_fpreg_x 0xa5a5a5a5 fr7
	assert_fpreg_x 0xa5a5a5a5 fr9
	assert_fpreg_x 0xa5a5a5a5 fr11
	assert_fpreg_x 0xa5a5a5a5 fr13
	assert_fpreg_x 0xa5a5a5a5 fr15
	pass
	exit 0
