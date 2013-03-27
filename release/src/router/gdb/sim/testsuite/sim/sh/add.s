# sh testcase for add
# mach:	 all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	.align 2
_x:	.long	1
_y:	.long	1
	
	start

add_reg_reg_direct:
	set_grs_a5a5
	mov.l	i, r1
	mov.l	j, r2
	add	r1, r2
	test_gr0_a5a5
	assertreg 2 r1
	assertreg 4 r2
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

add_reg_reg_indirect:
	set_grs_a5a5
	mov.l	x, r1
	mov.l	y, r2
	mov.l	@r1, r1
	mov.l	@r2, r2
	add	r1, r2
	test_gr0_a5a5
	assertreg 1 r1
	assertreg 2 r2
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
	
add_imm_reg:
	set_grs_a5a5
	add	#0x16, r1
	test_gr0_a5a5 
	assertreg 0xa5a5a5bb r1
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
i:	.long	2
j:	.long	2
	
