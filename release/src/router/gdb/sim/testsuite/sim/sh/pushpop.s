# sh testcase for push/pop (mov,movml,movmu...) insns.
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start
movml_1:
	set_greg 0, r0
	set_greg 1, r1
	set_greg 2, r2
	set_greg 3, r3
	set_greg 4, r4
	set_greg 5, r5
	set_greg 6, r6
	set_greg 7, r7
	set_greg 8, r8
	set_greg 9, r9
	set_greg 10, r10
	set_greg 11, r11
	set_greg 12, r12
	set_greg 13, r13
	set_greg 14, r14
	set_sreg 15, pr

	movml.l		r15,@-r15

	assertmem	stackt-4,  15
	assertmem	stackt-8,  14
	assertmem	stackt-12, 13
	assertmem	stackt-16, 12
	assertmem	stackt-20, 11
	assertmem	stackt-24, 10
	assertmem	stackt-28, 9
	assertmem	stackt-32, 8
	assertmem	stackt-36, 7
	assertmem	stackt-40, 6
	assertmem	stackt-44, 5
	assertmem	stackt-48, 4
	assertmem	stackt-52, 3
	assertmem	stackt-56, 2
	assertmem	stackt-60, 1
	assertmem	stackt-64, 0

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
	mov		r15, r0
	assertreg0	stackt-64

movml_2:
	set_grs_a5a5
	movml.l		@r15+, r15
	assert_sreg	15, pr
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
	mov		r15, r0
	assertreg0	stackt

movmu_1:
	set_grs_a5a5
	add	#1,r14
	add	#2,r13
	add	#3,r12
	set_sreg 0xa5a5,pr

	movmu.l	r12,@-r15

	assert_sreg	0xa5a5,pr
	assertreg	0xa5a5a5a6, r14
	assertreg	0xa5a5a5a7, r13
	assertreg	0xa5a5a5a8, r12
	test_gr_a5a5	r11
	test_gr_a5a5	r10
	test_gr_a5a5	r9
	test_gr_a5a5	r8
	test_gr_a5a5	r7
	test_gr_a5a5	r6
	test_gr_a5a5	r5
	test_gr_a5a5	r4
	test_gr_a5a5	r3
	test_gr_a5a5	r2
	test_gr_a5a5	r1
	test_gr_a5a5	r0
	mov	r15, r0
	assertreg	stackt-16, r0

	assertmem	stackt-4, 0xa5a5
	assertmem	stackt-8, 0xa5a5a5a6
	assertmem	stackt-12, 0xa5a5a5a7
	assertmem	stackt-16, 0xa5a5a5a8

movmu_2:
	set_grs_a5a5
	movmu.l		@r15+,r12

	assert_sreg	0xa5a5, pr
	assertreg	0xa5a5a5a6, r14
	assertreg	0xa5a5a5a7, r13
	assertreg	0xa5a5a5a8, r12
	test_gr_a5a5	r11
	test_gr_a5a5	r10
	test_gr_a5a5	r9
	test_gr_a5a5	r8
	test_gr_a5a5	r7
	test_gr_a5a5	r6
	test_gr_a5a5	r5
	test_gr_a5a5	r4
	test_gr_a5a5	r3
	test_gr_a5a5	r2
	test_gr_a5a5	r1
	test_gr_a5a5	r0
	mov	r15, r0
	assertreg	stackt, r0

	pass

	exit 0

	