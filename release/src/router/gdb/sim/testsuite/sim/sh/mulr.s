# sh testcase for mulr
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

mulr_1:	! multiply by one
	set_grs_a5a5
	mov	#1, r0
	mulr	r0, r1
	assertreg0   1
	test_gr_a5a5 r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

mulr_2:	! multiply by two
	set_grs_a5a5
	mov	#2, r0
	mov	#12, r1
	mulr	r0, r1
	assertreg0   2
	assertreg 24, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

mulr_3:	! multiply five by five
	set_grs_a5a5
	mov	#5, r0
	mov	#5, r1
	mulr	r0, r1
	assertreg0   5
	assertreg 25, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14


mulr_4:	! multiply 127 by 127
	set_grs_a5a5
	mov	#127, r0
	mov	#127, r1
	mulr	r0, r1
	assertreg0   127
	assertreg 0x3f01, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

mulr_5:	! multiply -1 by -1
	set_grs_a5a5
	mov	#-1, r0
	mov	#-1, r1
	mulr	r0, r1
	assertreg0   -1
	assertreg 1, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

mulr_6:	! multiply 46340 by 46340
	set_grs_a5a5
	movi20	#46340, r0
	movi20	#46340, r1
	mulr	r0, r1
	assertreg0   46340
	assertreg 0x7ffea810, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14

mulr_7:	! multiply 7ffff by 7ffff (overflow)
	set_grs_a5a5
	movi20	#0x7ffff, r0
	movi20	#0x7ffff, r1
	mulr	r0, r1
	assertreg0   0x7ffff
	assertreg 0xfff00001, r1
	test_gr_a5a5 r2
	test_gr_a5a5 r3
	test_gr_a5a5 r4
	test_gr_a5a5 r5
	test_gr_a5a5 r6
	test_gr_a5a5 r7
	test_gr_a5a5 r8
	test_gr_a5a5 r9
	test_gr_a5a5 r10
	test_gr_a5a5 r11
	test_gr_a5a5 r12
	test_gr_a5a5 r13
	test_gr_a5a5 r14


	pass

	exit 0

	