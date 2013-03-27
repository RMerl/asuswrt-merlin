# sh testcase for shll 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shll:
	set_grs_a5a5
	mov #1, r1
	shll r1
	assertreg 2, r1
	shll r1
	assertreg 4, r1
	shll r1
	assertreg 8, r1
	shll r1
	assertreg 16, r1
	shll r1
	assertreg 32, r1
	shll r1
	assertreg 64, r1
	shll r1
	assertreg 0x80, r1
	shll r1
	assertreg 0x100, r1
	shll r1
	assertreg 0x200, r1
	shll r1
	assertreg 0x400, r1
	shll r1
	assertreg 0x800, r1
	shll r1
	assertreg 0x1000, r1
	shll r1
	assertreg 0x2000, r1
	shll r1
	assertreg 0x4000, r1
	shll r1
	assertreg 0x8000, r1
	shll r1
	assertreg 0x10000, r1
	shll r1
	assertreg 0x20000, r1
	shll r1
	assertreg 0x40000, r1
	shll r1
	assertreg 0x80000, r1
	shll r1
	assertreg 0x100000, r1
	shll r1
	assertreg 0x200000, r1
	shll r1
	assertreg 0x400000, r1
	shll r1
	assertreg 0x800000, r1
	shll r1
	assertreg 0x1000000, r1
	shll r1
	assertreg 0x2000000, r1
	shll r1
	assertreg 0x4000000, r1
	shll r1
	assertreg 0x8000000, r1
	shll r1
	assertreg 0x10000000, r1
	shll r1
	assertreg 0x20000000, r1
	shll r1
	assertreg 0x40000000, r1
	shll r1
	assertreg 0x80000000, r1
	shll r1
	assertreg 0, r1
	shll r1
	assertreg 0, r1

	# another:
	mov #1, r1
	shll r1
	shll r1
	shll r1
	assertreg 8, r1

	set_greg     0xa5a5a5a5, r1
	test_grs_a5a5

	pass
	exit 0
