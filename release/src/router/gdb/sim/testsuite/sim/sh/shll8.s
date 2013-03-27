# sh testcase for shll8 
# mach: all
# as(sh):	-defsym sim_cpu=0
# as(shdsp):	-defsym sim_cpu=1 -dsp 

	.include "testutils.inc"

	start

shll8:
	set_grs_a5a5
	mov #1, r1
	shll8 r1
	assertreg 0x100, r1
	shll8 r1
	assertreg 0x10000, r1
	shll8 r1
	assertreg 0x1000000, r1
	shll8 r1
	assertreg 0, r1

	# another:
	mov #1, r1
	shll8 r1
	mov #1, r2
	shll r2
	shll r2
	shll r2
	shll r2
	shll r2
	shll r2
	shll r2
	shll r2
	cmp/eq r1, r2
	bt   okay
	fail
okay:
	set_greg 0xa5a5a5a5, r1
	set_greg 0xa5a5a5a5, r2
	test_grs_a5a5
	pass
	exit 0
