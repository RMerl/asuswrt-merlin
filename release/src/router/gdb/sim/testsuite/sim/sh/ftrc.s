# sh testcase for ftrc 
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	start
ftrc_single:
	set_grs_a5a5
	set_fprs_a5a5
	# ftrc(0.0) = 0.
	fldi0	fr0
	ftrc	fr0, fpul
	# check results.
	mov	#0, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	.L0
	fail
.L0:
	# ftrc(1.5) = 1.
	fldi1	fr0
	fldi1	fr1
	fldi1	fr2
	# double it.
	fadd	fr2, fr2
	# form the fraction.
	fdiv	fr2, fr1
	fadd	fr1, fr0
	# now we've got 1.5 in fr0.
	ftrc	fr0, fpul
	# check results.
	mov	#1, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	.L1
	fail
.L1:
	# ftrc(-1.5) = -1.
	fldi1	fr0
	fneg	fr0
	fldi1	fr1
	fldi1	fr2
	# double it.
	fadd	fr2, fr2
	# form the fraction.
	fdiv	fr2, fr1
	fneg	fr1
	# -1 + -0.5 = -1.5.
	fadd	fr1, fr0
	# now we've got 1.5 in fr0.
	ftrc	fr0, fpul
	# check results.
	mov	#1, r0
	neg	r0, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	ftrc_double
	fail

ftrc_double:
	double_prec
	# ftrc(0.0) = 0.
	fldi0	fr0
	_s2d	fr0, dr0
	ftrc	dr0, fpul
	# check results.
	mov	#0, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	.L10
	fail
.L10:
	# ftrc(1.5) = 1.
	fldi1	fr0
	fldi1	fr2
	fldi1	fr4
	# double it.
	single_prec
	fadd	fr4, fr4
	# form 0.5.
	fdiv	fr4, fr2
	fadd	fr2, fr0
	double_prec
	# now we've got 1.5 in fr0, so do some single->double
	# conversions and perform the ftrc.
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	_s2d	fr4, dr4
	ftrc	dr0, fpul
	
	# check results.
	mov	#1, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	.L11
	fail
.L11:
	# ftrc(-1.5) = -1.
	fldi1	fr0
	fneg	fr0
	fldi1	fr2
	fldi1	fr4
	single_prec
	# double it.
	fadd	fr4, fr4
	# form the fraction.
	fdiv	fr4, fr2
	fneg	fr2
	# -1 + -0.5 = -1.5.
	fadd	fr2, fr0
	double_prec
	# now we've got 1.5 in fr0, so do some single->double
	# conversions and perform the ftrc.
	_s2d	fr0, dr0
	_s2d	fr2, dr2
	_s2d	fr4, dr4
	ftrc	dr0, fpul
	
	# check results.
	mov	#1, r0
	neg	r0, r0
	sts	fpul, r1
	cmp/eq	r0, r1
	bt	.L12
	fail
.L12:
	assertreg0	-1
	assertreg	-1, r1
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

	assert_dpreg_i	2, dr4
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
