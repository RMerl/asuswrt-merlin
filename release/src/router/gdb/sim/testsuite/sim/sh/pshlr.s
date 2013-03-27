# sh testcase for pshl <reg>
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

pshl_reg:			! shift arithmetic, register operand
	set_grs_a5a5
	lds	r0, a0
	pcopy	a0, a1
	lds	r0, x0
	lds	r0, x1
	lds	r0, y0
	lds	r0, y1
	pcopy	x0, m0
	pcopy	y1, m1

	set_sreg 0x10000, x0
	set_sreg 0x0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x10000, y0
	pshl	x0, y0, x0
	assert_sreg	0x20000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x20000, y0
	pshl	x0, y0, x0
	assert_sreg	0x40000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x30000, y0
	pshl	x0, y0, x0
	assert_sreg	0x80000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x40000, y0
	pshl	x0, y0, x0
	assert_sreg	0x100000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x50000, y0
	pshl	x0, y0, x0
	assert_sreg	0x200000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x60000, y0
	pshl	x0, y0, x0
	assert_sreg	0x400000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x70000, y0
	pshl	x0, y0, x0
	assert_sreg	0x800000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x80000, y0
	pshl	x0, y0, x0
	assert_sreg	0x1000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x90000, y0
	pshl	x0, y0, x0
	assert_sreg	0x2000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xa0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x4000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xb0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x8000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xc0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xd0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x20000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xe0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x40000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0xf0000, y0
	pshl	x0, y0, x0
	assert_sreg	0x80000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x10000, x0

	set_sreg 0x100000, y0
	pshl	x0, y0, x0
	assert_sreg	0x00000000, x0
	pneg	y0, y0
	pshl	x0, y0, x0
	assert_sreg	0x0, x0

	test_grs_a5a5
	assert_sreg2	0xa5a5a5a5, a0
	assert_sreg2	0xa5a5a5a5, a1
	assert_sreg	0xa5a5a5a5, x1
	assert_sreg	0xa5a5a5a5, y1
	assert_sreg2	0xa5a5a5a5, m0
	assert_sreg2	0xa5a5a5a5, m1


	pass
	exit 0

