# sh testcase for psha <imm>
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

psha_imm:			! shift arithmetic, immediate operand
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_sreg 0x1, a0
	psha	#0, a0
	assert_sreg	0x1, a0
	psha	#-0, a0
	assert_sreg	0x1, a0

	psha	#1, a0
	assert_sreg	0x2, a0
	psha	#-1, a0
	assert_sreg	0x1, a0

	psha	#2, a0
	assert_sreg	0x4, a0
	psha	#-2, a0
	assert_sreg	0x1, a0

	psha	#3, a0
	assert_sreg	0x8, a0
	psha	#-3, a0
	assert_sreg	0x1, a0

	psha	#4, a0
	assert_sreg	0x10, a0
	psha	#-4, a0
	assert_sreg	0x1, a0

	psha	#5, a0
	assert_sreg	0x20, a0
	psha	#-5, a0
	assert_sreg	0x1, a0

	psha	#6, a0
	assert_sreg	0x40, a0
	psha	#-6, a0
	assert_sreg	0x1, a0

	psha	#7, a0
	assert_sreg	0x80, a0
	psha	#-7, a0
	assert_sreg	0x1, a0

	psha	#8, a0
	assert_sreg	0x100, a0
	psha	#-8, a0
	assert_sreg	0x1, a0

	psha	#9, a0
	assert_sreg	0x200, a0
	psha	#-9, a0
	assert_sreg	0x1, a0

	psha	#10, a0
	assert_sreg	0x400, a0
	psha	#-10, a0
	assert_sreg	0x1, a0

	psha	#11, a0
	assert_sreg	0x800, a0
	psha	#-11, a0
	assert_sreg	0x1, a0

	psha	#12, a0
	assert_sreg	0x1000, a0
	psha	#-12, a0
	assert_sreg	0x1, a0

	psha	#13, a0
	assert_sreg	0x2000, a0
	psha	#-13, a0
	assert_sreg	0x1, a0

	psha	#14, a0
	assert_sreg	0x4000, a0
	psha	#-14, a0
	assert_sreg	0x1, a0

	psha	#15, a0
	assert_sreg	0x8000, a0
	psha	#-15, a0
	assert_sreg	0x1, a0

	psha	#16, a0
	assert_sreg	0x10000, a0
	psha	#-16, a0
	assert_sreg	0x1, a0

	psha	#17, a0
	assert_sreg	0x20000, a0
	psha	#-17, a0
	assert_sreg	0x1, a0

	psha	#18, a0
	assert_sreg	0x40000, a0
	psha	#-18, a0
	assert_sreg	0x1, a0

	psha	#19, a0
	assert_sreg	0x80000, a0
	psha	#-19, a0
	assert_sreg	0x1, a0

	psha	#20, a0
	assert_sreg	0x100000, a0
	psha	#-20, a0
	assert_sreg	0x1, a0

	psha	#21, a0
	assert_sreg	0x200000, a0
	psha	#-21, a0
	assert_sreg	0x1, a0

	psha	#22, a0
	assert_sreg	0x400000, a0
	psha	#-22, a0
	assert_sreg	0x1, a0

	psha	#23, a0
	assert_sreg	0x800000, a0
	psha	#-23, a0
	assert_sreg	0x1, a0

	psha	#24, a0
	assert_sreg	0x1000000, a0
	psha	#-24, a0
	assert_sreg	0x1, a0

	psha	#25, a0
	assert_sreg	0x2000000, a0
	psha	#-25, a0
	assert_sreg	0x1, a0

	psha	#26, a0
	assert_sreg	0x4000000, a0
	psha	#-26, a0
	assert_sreg	0x1, a0

	psha	#27, a0
	assert_sreg	0x8000000, a0
	psha	#-27, a0
	assert_sreg	0x1, a0

	psha	#28, a0
	assert_sreg	0x10000000, a0
	psha	#-28, a0
	assert_sreg	0x1, a0

	psha	#29, a0
	assert_sreg	0x20000000, a0
	psha	#-29, a0
	assert_sreg	0x1, a0

	psha	#30, a0
	assert_sreg	0x40000000, a0
	psha	#-30, a0
	assert_sreg	0x1, a0

	psha	#31, a0
	assert_sreg	0x80000000, a0
	psha	#-31, a0
	assert_sreg	0xffffffff, a0

	psha	#32, a0
	assert_sreg	0x00000000, a0
#	I don't grok what should happen here...
#	psha	#-32, a0
#	assert_sreg	0x0, a0

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

