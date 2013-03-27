# sh testcase for movli
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
x:	.long	1
y:	.long	2
z:	.long	3
	
	start
	set_grs_a5a5
	mov.l	xptr, r1
	mov.l	yptr, r2
	# Move linked/conditional, x to y
	movli.l	@r1, r0
	movco.l r0, @r2

	# Check result.
	assertreg0 1
	mov.l	yptr, r1
	mov.l	@r1, r2
	assertreg  1, r2

	# Now attempt an unlinked move of r0 to z
	mov.l	zptr, r1
	movco.l	r0, @r1

	# Check that z is unchanged.
	mov.l	zptr, r1
	mov.l	@r1, r2
	assertreg 3, r2

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

	pass
	exit 0

	.align 2
xptr:	.long	x
yptr:	.long	y
zptr:	.long	z
