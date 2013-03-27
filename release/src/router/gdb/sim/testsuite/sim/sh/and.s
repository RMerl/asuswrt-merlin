# sh testcase for and
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	0xa5a5a5a5
_y:	.long	0x55555555
	
	start

and_reg_reg_direct:
	set_grs_a5a5
	mov.l	i, r1
	mov.l	j, r2
	and	r1, r2
	test_gr0_a5a5
	assertreg 0xa5a5a5a5 r1
	assertreg 0xa0a0a0a0 r2
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
	bra	and_imm_reg
	nop

	.align	2
i:	.long	0xa5a5a5a5
j:	.long	0xaaaaaaaa

and_imm_reg:
	set_grs_a5a5
	and	#0xff, r0
	assertreg    0xa5, r0
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

and_b_imm_ind:
	set_grs_a5a5
	mov.l	x, r0
	and.b	#0x55, @(r0, GBR)	
	mov.l	@r0, r0

	assertreg 0xa5a5a505, r0
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

	pass

	exit 0

	.align 2
x:	.long	_x
y:	.long	_y
	
