# sh testcase for ldbank stbank resbank
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.macro	SEND reg bankno regno
	set_greg ((\bankno << 7) + (\regno << 2)), \reg
	.endm

	start

stbank_1:
	set_grs_a5a5
	mov	#0, r0
	SEND	r1, 0, 0
	stbank	r0, @r1
	mov	#1, r0
	SEND	r1, 0, 1
	stbank	r0, @r1
	mov	#2, r0
	SEND	r1, 0, 2
	stbank	r0, @r1
	mov	#3, r0
	SEND	r1, 0, 3
	stbank	r0, @r1
	mov	#4, r0
	SEND	r1, 0, 4
	stbank	r0, @r1
	mov	#5, r0
	SEND	r1, 0, 5
	stbank	r0, @r1
	mov	#6, r0
	SEND	r1, 0, 6
	stbank	r0, @r1
	mov	#7, r0
	SEND	r1, 0, 7
	stbank	r0, @r1
	mov	#8, r0
	SEND	r1, 0, 8
	stbank	r0, @r1
	mov	#9, r0
	SEND	r1, 0, 9
	stbank	r0, @r1
	mov	#10, r0
	SEND	r1, 0, 10
	stbank	r0, @r1
	mov	#11, r0
	SEND	r1, 0, 11
	stbank	r0, @r1
	mov	#12, r0
	SEND	r1, 0, 12
	stbank	r0, @r1
	mov	#13, r0
	SEND	r1, 0, 13
	stbank	r0, @r1
	mov	#14, r0
	SEND	r1, 0, 14
	stbank	r0, @r1
	mov	#15, r0
	SEND	r1, 0, 15
	stbank	r0, @r1
	mov	#16, r0
	SEND	r1, 0, 16
	stbank	r0, @r1
	mov	#17, r0
	SEND	r1, 0, 17
	stbank	r0, @r1
	mov	#18, r0
	SEND	r1, 0, 18
	stbank	r0, @r1
	mov	#19, r0
	SEND	r1, 0, 19
	stbank	r0, @r1

	assertreg0	19
	assertreg	19 << 2, r1
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

ldbank_1:
	set_grs_a5a5
	SEND	r1, 0, 0
	ldbank	@r1, r0
	assertreg0 0
	SEND	r1, 0, 1
	ldbank	@r1, r0
	assertreg0 1
	SEND	r1, 0, 2
	ldbank	@r1, r0
	assertreg0 2
	SEND	r1, 0, 3
	ldbank	@r1, r0
	assertreg0 3
	SEND	r1, 0, 4
	ldbank	@r1, r0
	assertreg0 4
	SEND	r1, 0, 5
	ldbank	@r1, r0
	assertreg0 5
	SEND	r1, 0, 6
	ldbank	@r1, r0
	assertreg0 6
	SEND	r1, 0, 7
	ldbank	@r1, r0
	assertreg0 7
	SEND	r1, 0, 8
	ldbank	@r1, r0
	assertreg0 8
	SEND	r1, 0, 9
	ldbank	@r1, r0
	assertreg0 9
	SEND	r1, 0, 10
	ldbank	@r1, r0
	assertreg0 10
	SEND	r1, 0, 11
	ldbank	@r1, r0
	assertreg0 11
	SEND	r1, 0, 12
	ldbank	@r1, r0
	assertreg0 12
	SEND	r1, 0, 13
	ldbank	@r1, r0
	assertreg0 13
	SEND	r1, 0, 14
	ldbank	@r1, r0
	assertreg0 14
	SEND	r1, 0, 15
	ldbank	@r1, r0
	assertreg0 15
	SEND	r1, 0, 16
	ldbank	@r1, r0
	assertreg0 16
	SEND	r1, 0, 17
	ldbank	@r1, r0
	assertreg0 17
	SEND	r1, 0, 18
	ldbank	@r1, r0
	assertreg0 18
	SEND	r1, 0, 19
	ldbank	@r1, r0
	assertreg0 19

	assertreg (19 << 2), r1
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

resbank_1:
	set_grs_a5a5
	mov	#1, r0
	trapa	#13	! magic trap, sets ibnr

	resbank

	assertreg0	0
	assertreg	1, r1
	assertreg	2, r2
	assertreg	3, r3
	assertreg	4, r4
	assertreg	5, r5
	assertreg	6, r6
	assertreg	7, r7
	assertreg	8, r8
	assertreg	9, r9
	assertreg	10, r10
	assertreg	11, r11
	assertreg	12, r12
	assertreg	13, r13
	assertreg	14, r14
	assert_sreg	15, mach
	assert_sreg	17, pr
	assert_creg	18, gbr
	assert_sreg	19, macl

resbank_2:
	set_grs_a5a5
	movi20	#555, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15
	add	#-1, r0
	mov.l	r0, @-r15

	set_sr_bit	(1 << 14)	! set BO

	resbank

	assert_sreg	555, macl
	assert_sreg	554, mach
	assert_creg	553, gbr
	assert_sreg	552, pr
	assertreg	551, r14
	assertreg	550, r13
	assertreg	549, r12
	assertreg	548, r11
	assertreg	547, r10
	assertreg	546, r9
	assertreg	545, r8
	assertreg	544, r7
	assertreg	543, r6
	assertreg	542, r5
	assertreg	541, r4
	assertreg	540, r3
	assertreg	539, r2
	assertreg	538, r1
	assertreg0	537

	mov		r15, r0
	assertreg0	stackt

	pass

	exit 0
