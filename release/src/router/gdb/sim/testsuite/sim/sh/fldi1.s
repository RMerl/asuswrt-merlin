# sh testcase for fldi1 $frn 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
fldi1_single:
	set_grs_a5a5
	set_fprs_a5a5
	fldi1	fr1
	fldi1	fr3
	fldi1	fr5
	fldi1	fr7
	fldi1	fr9
	fldi1	fr11
	fldi1	fr13
	fldi1	fr15
	test_grs_a5a5
	assert_fpreg_x 0xa5a5a5a5 fr0
	assert_fpreg_x 0xa5a5a5a5 fr2
	assert_fpreg_x 0xa5a5a5a5 fr4
	assert_fpreg_x 0xa5a5a5a5 fr6
	assert_fpreg_x 0xa5a5a5a5 fr8
	assert_fpreg_x 0xa5a5a5a5 fr10
	assert_fpreg_x 0xa5a5a5a5 fr12
	assert_fpreg_x 0xa5a5a5a5 fr14
	assert_fpreg_i 1 fr1
	assert_fpreg_i 1 fr3
	assert_fpreg_i 1 fr5
	assert_fpreg_i 1 fr7
	assert_fpreg_i 1 fr9
	assert_fpreg_i 1 fr11
	assert_fpreg_i 1 fr13
	assert_fpreg_i 1 fr15

	pass
	exit 0
