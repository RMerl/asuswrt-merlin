# sh testcase for all fmov instructions
# mach: sh
# as(sh):	-defsym sim_cpu=0

	.include "testutils.inc"

	.macro init
	fldi0	fr0
	fldi1	fr1
	fldi1	fr2
	fldi1	fr3
	.endm

	start

fmov1:	# Test fr -> fr.
	set_grs_a5a5
	set_fprs_a5a5
	init
	single_prec
	sz_32
	fmov	fr0, fr1
	# Ensure fr0 and fr1 are now equal.
	fcmp/eq	fr0, fr1
	bt	fmov2
	fail

fmov2:	# Test dr -> dr.
	init
	double_prec
	sz_64
	fmov	dr0, dr2
	# Ensure dr0 and dr2 are now equal.
	fcmp/eq	dr0, dr2
	bt	fmov3
	fail

fmov3:	# Test dr -> xd and xd -> dr.
	init
	sz_64
	fmov	dr0, xd0
	# Ensure dr0 and xd0 are now equal.
	fmov	xd0, dr2
	fcmp/eq	dr0, dr2
	bt	fmov4
	fail

fmov4:	# Test xd -> xd.
	init
	sz_64
	double_prec
	fmov	dr0, xd0
	fmov	xd0, xd2
	fmov	xd2, dr2
	# Ensure dr0 and dr2 are now equal.
	fcmp/eq	dr0, dr2
	bt	.L0
	fail

	# FIXME: test fmov.s fr -> @gr,      fmov dr -> @gr
	# FIXME: test fmov.s @gr -> fr,      fmov @gr -> dr
	# FIXME: test fmov.s @gr+ -> fr,     fmov @gr+ -> dr
	# FIXME: test fmov.s fr -> @-gr,     fmov dr -> @-gr
	# FIXME: test fmov.s @(r0,gr) -> fr, fmov @(r0,gr) -> dr
	# FIXME: test fmov.s fr -> @(r0,gr), fmov dr -> @(r0,gr)
	
.L0:
	test_grs_a5a5
	sz_32
	single_prec
	assert_fpreg_i	0, fr0
	assert_fpreg_i	1, fr1
	assert_fpreg_i	0, fr2
	assert_fpreg_i	1, fr3
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

fmov5:	# Test fr -> @rn and @rn -> fr.
	init
	sz_32
	single_prec
	# FIXME!  Use a reserved memory location!
	mov	#40, r0
	shll8	r0
	fmov	fr0, @r0
	fmov	@r0, fr1
	fcmp/eq	fr0, fr1
	bt	fmov6
	fail

fmov6:	# Test dr -> @rn and @rn -> dr.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	fmov	dr0, @r0
	fmov	@r0, dr2
	fcmp/eq	dr0, dr2
	bt	fmov7
	fail

fmov7:	# Test xd -> @rn and @rn -> xd.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	fmov	dr0, xd0
	fmov	xd0, @r0
	fmov	@r0, xd2
	fmov	xd2, dr2
	fcmp/eq	dr0, dr2
	bt	fmov8
	fail

fmov8:	# Test fr -> @-rn.
	init
	sz_32
	single_prec
	mov	#40, r0
	shll8	r0
	# Preserve.
	mov	r0, r1
	fmov	fr0, @-r0
	fmov	@r0, fr2
	fcmp/eq	fr0, fr2
	bt	f8b
	fail
f8b:	# check pre-dec.
	add	#4, r0
	cmp/eq	r0, r1
	bt	fmov9
	fail

fmov9:	# Test dr -> @-rn.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	# Preserve r0.
	mov	r0, r1
	fmov	dr0, @-r0
	fmov	@r0, dr2
	fcmp/eq	dr0, dr2
	bt	f9b
	fail
f9b:	# check pre-dec.
	add	#8, r0
	cmp/eq	r0, r1
	bt	fmov10
	fail

fmov10:	# Test xd -> @-rn.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	# Preserve r0.
	mov	r0, r1
	fmov	dr0, xd0
	fmov	xd0, @-r0
	fmov	@r0, xd2
	fmov	xd2, dr2
	fcmp/eq	dr0, dr2
	bt	f10b
	fail
f10b:	# check pre-dec.
	add	#8, r0
	cmp/eq	r0, r1
	bt	fmov11
	fail

fmov11:	# Test @rn+ -> fr.
	init
	sz_32
	single_prec
	mov	#40, r0
	shll8	r0
	# Preserve r0.
	mov	r0, r1
	fmov	fr0, @r0
	fmov	@r0+, fr2
	fcmp/eq	fr0, fr2
	bt	f11b
	fail
f11b:	# check post-inc.
	add	#4, r1
	cmp/eq	r0, r1
	bt	fmov12
	fail

fmov12:	# Test @rn+ -> dr.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	# preserve r0.
	mov	r0, r1
	fmov	dr0, @r0
	fmov	@r0+, dr2
	fcmp/eq	dr0, dr2
	bt	f12b
	fail
f12b:	# check post-inc.
	add	#8, r1
	cmp/eq	r0, r1
	bt	fmov13
	fail

fmov13:	# Test @rn -> xd.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	# Preserve r0.
	mov	r0, r1
	fmov	dr0, xd0
	fmov	xd0, @r0
	fmov	@r0+, xd2
	fmov	xd2, dr2
	fcmp/eq	dr0, dr2
	bt	f13b
	fail
f13b:
	add	#8, r1
	cmp/eq	r0, r1
	bt	fmov14
	fail

fmov14:	# Test fr -> @(r0,rn), @(r0, rn) -> fr.
	init
	sz_32
	single_prec
	mov	#40, r0
	shll8	r0
	mov	#0, r1
	fmov	fr0, @(r0, r1)
	fmov	@(r0, r1), fr1
	fcmp/eq	fr0, fr1
	bt	fmov15
	fail

fmov15:	# Test dr -> @(r0, rn), @(r0, rn) -> dr.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	mov	#0, r1
	fmov	dr0, @(r0, r1)
	fmov	@(r0, r1), dr2
	fcmp/eq	dr0, dr2
	bt	fmov16
	fail

fmov16:	# Test xd -> @(r0, rn), @(r0, rn) -> xd.
	init
	sz_64
	double_prec
	mov	#40, r0
	shll8	r0
	mov	#0, r1
	fmov	dr0, xd0
	fmov	xd0, @(r0, r1)
	fmov	@(r0, r1), xd2
	fmov	xd2, dr2
	fcmp/eq	dr0, dr2
	bt	.L1
	fail
.L1:
	assertreg0	0x2800
	assertreg	0, r1
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

	sz_32
	single_prec
	assert_fpreg_i	0, fr0
	assert_fpreg_i	1, fr1
	assert_fpreg_i	0, fr2
	assert_fpreg_i	1, fr3
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
