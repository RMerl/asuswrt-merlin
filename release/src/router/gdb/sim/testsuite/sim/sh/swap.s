# sh testcase for swap
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

swapb:
	set_grs_a5a5
	mov	#0x5a, r0
	shll8	r0
	or	#0xa5, r0
	assertreg0	0x5aa5
	
	swap.b	r0, r1
	assertreg	0xa55a, r1

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

swapw:	
	set_grs_a5a5
	mov	#0x5a, r0
	shll16	r0
	or	#0xa5, r0
	assertreg0	0x5a00a5

	swap.w	r0, r1
	assertreg	0xa5005a, r1

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
