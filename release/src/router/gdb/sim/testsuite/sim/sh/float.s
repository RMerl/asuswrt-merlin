# sh testcase for float 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start

float_pos:
	set_grs_a5a5
	set_fprs_a5a5
	single_prec
	mov	#3, r0
	lds	r0, fpul
	float	fpul, fr2

	# Check the result.
	fldi1	fr0
	fldi1	fr1
	fadd	fr0, fr1
	fadd	fr0, fr1
	fcmp/eq	fr1, fr2
	bt	float_neg
	fail

float_neg:	
	mov	#3, r0
	neg	r0, r0
	lds	r0, fpul
	float	fpul, fr2

	# Check the result.
	fldi1	fr0
	fldi1	fr1
	fadd	fr0, fr1
	fadd	fr0, fr1
	fneg	fr1
	fcmp/eq	fr1, fr2
	bt	.L0
	fail
.L0:
	assertreg0	-3
	test_gr_a5a5	r1
	test_gr_a5a5	r2
	test_gr_a5a5	r3
	test_gr_a5a5	r4
	test_gr_a5a5	r5
	test_gr_a5a5	r6
	test_gr_a5a5	r7
	test_gr_a5a5	r8
	test_gr_a5a5	r9
	test_gr_a5a5	r10
	test_gr_a5a5	r11
	test_gr_a5a5	r12
	test_gr_a5a5	r13
	test_gr_a5a5	r14

	assert_fpreg_i	 1, fr0
	assert_fpreg_i	-3, fr1
	assert_fpreg_i	-3, fr2	
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

double_pos:
	set_grs_a5a5
	set_fprs_a5a5
	double_prec
	mov	#3, r0
	lds	r0, fpul
	float	fpul, dr4

	# check the result.
	fldi1	fr0
	fldi1	fr1
	single_prec
	fadd	fr0, fr1
	fadd	fr0, fr1
	double_prec
	_s2d	fr1, dr2
	fcmp/eq	dr2, dr4
	bt	double_neg
	fail

double_neg:
	double_prec
	mov	#3, r0
	neg	r0, r0
	lds	r0, fpul
	float	fpul, dr4

	# check the result.
	fldi1	fr0
	fldi1	fr1
	single_prec
	fadd	fr0, fr1
	fadd	fr0, fr1
	fneg	fr1
	double_prec
	_s2d	fr1, dr2
	fcmp/eq	dr2, dr4
	bt	.L2
	fail
.L2:
	assertreg0	-3
	test_gr_a5a5	r1
	test_gr_a5a5	r2
	test_gr_a5a5	r3
	test_gr_a5a5	r4
	test_gr_a5a5	r5
	test_gr_a5a5	r6
	test_gr_a5a5	r7
	test_gr_a5a5	r8
	test_gr_a5a5	r9
	test_gr_a5a5	r10
	test_gr_a5a5	r11
	test_gr_a5a5	r12
	test_gr_a5a5	r13
	test_gr_a5a5	r14

	single_prec
	assert_fpreg_i	 1, fr0
	assert_fpreg_i	-3, fr1
	double_prec
	assert_dpreg_i	-3, dr2
	assert_dpreg_i	-3, dr4
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
