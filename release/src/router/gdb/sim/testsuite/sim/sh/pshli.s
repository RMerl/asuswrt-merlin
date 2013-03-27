# sh testcase for pshl <imm>
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

pshl_imm:			! shift logical, immediate operand
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_sreg 0x10000, a0
	pshl	#0, a0
	assert_sreg	0x10000, a0
	pshl	#-0, a0
	assert_sreg	0x10000, a0

	pshl	#1, a0
	assert_sreg	0x20000, a0
	pshl	#-1, a0
	assert_sreg	0x10000, a0

	pshl	#2, a0
	assert_sreg	0x40000, a0
	pshl	#-2, a0
	assert_sreg	0x10000, a0

	pshl	#3, a0
	assert_sreg	0x80000, a0
	pshl	#-3, a0
	assert_sreg	0x10000, a0

	pshl	#4, a0
	assert_sreg	0x100000, a0
	pshl	#-4, a0
	assert_sreg	0x10000, a0

	pshl	#5, a0
	assert_sreg	0x200000, a0
	pshl	#-5, a0
	assert_sreg	0x10000, a0

	pshl	#6, a0
	assert_sreg	0x400000, a0
	pshl	#-6, a0
	assert_sreg	0x10000, a0

	pshl	#7, a0
	assert_sreg	0x800000, a0
	pshl	#-7, a0
	assert_sreg	0x10000, a0

	pshl	#8, a0
	assert_sreg	0x1000000, a0
	pshl	#-8, a0
	assert_sreg	0x10000, a0

	pshl	#9, a0
	assert_sreg	0x2000000, a0
	pshl	#-9, a0
	assert_sreg	0x10000, a0

	pshl	#10, a0
	assert_sreg	0x4000000, a0
	pshl	#-10, a0
	assert_sreg	0x10000, a0

	pshl	#11, a0
	assert_sreg	0x8000000, a0
	pshl	#-11, a0
	assert_sreg	0x10000, a0

	pshl	#12, a0
	assert_sreg	0x10000000, a0
	pshl	#-12, a0
	assert_sreg	0x10000, a0

	pshl	#13, a0
	assert_sreg	0x20000000, a0
	pshl	#-13, a0
	assert_sreg	0x10000, a0

	pshl	#14, a0
	assert_sreg	0x40000000, a0
	pshl	#-14, a0
	assert_sreg	0x10000, a0

	pshl	#15, a0
	assert_sreg	0x80000000, a0
	pshl	#-15, a0
	assert_sreg	0x10000, a0

	pshl	#16, a0
	assert_sreg	0x00000000, a0
	pshl	#-16, a0
	assert_sreg	0x0, a0

	test_grs_a5a5
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg	0xa5a5a5a5, x0
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y0
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1


	pass
	exit 0

